# EIGRP Packetizing Specification

Copyright (C) 2026 Donnie V. Savage

This file is a new project design document. New files created for this EIGRP work use Donnie V. Savage as the copyright owner unless stated otherwise.

Existing source files must preserve all prior copyright notices, SPDX identifiers, and author history. Refactoring an existing file is not permission to remove earlier authorship.

## 1. Purpose

This document defines the first-pass packetizing model for the `eigrpd` project.

It owns the handoff between DUAL/topology work and packet transmission:

```text
DUAL/FSM/topology event
  -> packetizer work queue
  -> packetizer
  -> per-interface packet queue
  -> interface transmit pacing
  -> reliable transport / ACK handling
```

This document extends `specs/design-spec.md`.

## 2. Authority

Packetizing authority follows the project authority order defined in `design-spec.md`:

1. Donnie V. Savage.
2. RFC 7868 for protocol behavior unless intentionally clarified or superseded by Donnie V. Savage.
3. FRR only for build compatibility, daemon integration, library/API compatibility, and event-loop integration.
4. Existing `eigrpd` code only where it does not conflict with this specification.

The Cisco internal term `threading` is not used as a new code module name because it conflicts with OS threads and FRR `struct event *thread` naming.

## 3. RFC Conflict Review

No protocol conflict is intended by this specification.

This document describes implementation machinery that RFC 7868 does not specify directly:

- the DUAL work queue
- NDB/RDB work beads
- packetizer work queue scheduling
- southbound host work queue adaptation
- packet object ownership/refcounting
- internal queue names

These are implementation details as long as the emitted packets and reliable transport behavior follow RFC 7868.

RFC-sensitive rules captured here:

- UPDATE, QUERY, REPLY, SIA-QUERY, and SIA-REPLY carry per-destination routing information and are processed individually by DUAL.
- Multiple destinations may be packed into one or more packets.
- Reliable packets may be multicast on first send, but retransmission is unicast to neighbors that have not acknowledged.
- Reliable transport ACK state is per neighbor.
- Interface pacing limits EIGRP bandwidth use per interface.
- Split horizon, poison reverse, pending-neighbor exclusion, and conditional receive behavior are interface/neighbor sensitive.
- Classic TLV and multiprotocol/wide TLV support may require TLV1, TLV2, or both on the same interface depending on peer capability.

If any future implementation chooses behavior different from these rules, the difference must be documented as an intentional protocol clarification or reviewed as a possible design mistake.

## 4. Terminology

### 4.1 Packetizer Work Item

A packetizer work item is an internal marker that tells the packetizer an EIGRP topology object needs to be advertised, queried, replied, withdrawn, poisoned, or otherwise emitted.

A work item is not a packet.

A work item must not contain prebuilt wire TLVs.

Preferred code name:

```text
eigrp_packetizer_work
```

### 4.2 NDB Work

An NDB work item represents destination-level work.

The interface is not known at enqueue time.

When the packetizer processes NDB work, it walks eligible EIGRP interfaces and builds interface-specific packets.

### 4.3 RDB Work

An RDB work item represents route-descriptor-level work.

The interface or neighbor context is known from the route descriptor.

When the packetizer processes RDB work, it builds only for the relevant interface/neighbor context unless the packetizing rule explicitly requires broader emission.

### 4.4 Packet

An `eigrp_packet` is a built wire packet buffer.

Once built, the packet is immutable and locked to one interface.

A built packet must not be modified after it is queued for transmission.

### 4.5 Interface Packet Queue

An interface packet queue is the per-interface transmit queue.

It holds built packet references waiting for interface pacing and send.

Preferred code name:

```text
eigrp_packet_queue
```

There is one `eigrp_packet_queue` per EIGRP interface.

## 5. Queue Model

The design uses two real queues.

```text
eigrp_packetizer_queue
  global DUAL work queue
  holds NDB/RDB work items
  no wire packet yet
  no TLV1/TLV2 decision yet
  no interface pacing yet

eigrp_packet_queue
  one per EIGRP interface
  holds built packet refs
  packet is already locked to that interface
  enforces transmit pacing
```

Reliable ACK state is per neighbor, but it does not need to be a third general queue.

Neighbor ACK state may hold packet references until the neighbor ACKs, resets, times out, or otherwise releases the packet.

## 6. Packetizer Queue Rules

### 6.1 Ownership

DUAL, topology, and route-management code enqueue work into `eigrp_packetizer_queue`.

Packet send functions should not independently build route TLVs for topology changes.

Preferred model:

```text
DUAL/topology marks work.
Packetizer chooses wire format and interface destination.
```

### 6.2 No Early Interface Decision for NDB

NDB work must not decide TLV1/TLV2/both at enqueue time.

The packetizer owns that decision because peer capability is interface-sensitive.

### 6.3 RDB Interface Context

RDB work may carry known interface or neighbor context.

The packetizer must still validate the context is current before building a packet.

If the interface or neighbor no longer exists, the work item is discarded safely.

### 6.4 Coalescing and Rethreading

A topology object may change while an existing work item for that object is still queued.

If the queued work item has not yet been pulled by the packetizer:

```text
update/coalesce the existing work item
move it to the desired queue position if needed
```

If the packetizer has already pulled the work item:

```text
create a new work item for the newer object state
enqueue it at the head/high-priority side when required
```

The object state at packetization time wins.

Queued work should be treated as a request to inspect current object state, not as a frozen copy of old state.

### 6.5 Southbound Work Queue Scheduling

Adding work to the packetizer queue schedules the packetizer through an EIGRP-owned work queue abstraction.

Core packetizer code must not call FRR `work_queue` APIs directly. It calls:

```c
eigrp_work_queue_enqueue(eigrp_work_queue_t *queue, void *data);
```

The FRR build implements that API in `eigrp_southbound.[c|h]` using FRR `work_queue`. A future BSD build replaces the southbound implementation without changing packetizer, DUAL, topology, or TLV code.

There is no public packetizer `wakeup` API. Scheduling is an internal side effect of enqueueing work.

## 7. Packetizer Processing Rules

### 7.1 NDB Processing

For NDB work:

```text
for each EIGRP interface:
  validate interface is active/eligible
  inspect peer capability on that interface
  decide TLV1, TLV2, TLV1+TLV2, or no packet
  apply split horizon / poison reverse / pending-neighbor rules
  build one packet locked to that interface
  queue packet to that interface packet queue
```

One NDB work item may produce zero, one, or many interface-specific packets.

### 7.2 RDB Processing

For RDB work:

```text
validate route descriptor still exists
validate interface/neighbor context still exists
inspect peer capability for that interface/neighbor
build packet locked to the relevant interface
queue packet to that interface packet queue
```

An RDB work item normally produces packet output for one interface.

### 7.3 Interface Peer Type

Each EIGRP interface needs a packetizing view of its peer capabilities.

Suggested conceptual values:

```text
none
  no eligible peers need this work on this interface

tlv1
  eligible peers need classic TLV encoding

tlv2
  eligible peers can use multiprotocol/wide TLV encoding

tlv1_tlv2
  mixed peer capability exists; packet must carry both encodings
```

The exact enum name can be chosen during code implementation.

The packetizer must calculate this from current neighbor state, not stale queue state.

### 7.4 Packet Immutability

After the packetizer builds an `eigrp_packet`, the packet is immutable.

The packet is locked to one interface.

Do not queue the same built packet object to multiple interfaces unless the implementation explicitly proves the wire image, auth, pacing, MTU, and ACK target set are identical.

Default rule:

```text
one interface packet = one built packet object
```

## 8. Interface Packet Queue Rules

### 8.1 Per-Interface Queue

Each EIGRP interface owns one `eigrp_packet_queue` for outbound packets.

The queue holds packet references.

The queue does not own topology state.

### 8.2 Pacing

Transmit pacing is an interface packet queue responsibility.

The packetizer work queue must not perform global bandwidth gating.

Reason:

- RFC pacing is interface-sensitive.
- NDB work may build packets for multiple interfaces with different bandwidth and pacing budgets.
- RDB work may target a specific interface that has independent pacing state.

### 8.3 Pacing Result

If an interface is over its allowed transmit rate, the packet remains queued for that interface.

Other interfaces may still transmit their own packets if their pacing budget allows.

A blocked interface queue must reschedule transmission when budget is expected to be available.

## 9. Reliable Transport Rules

### 9.1 First Send

A reliable packet may be sent multicast on first transmission when appropriate for the interface and packet type.

The sender records the neighbor ACK set for all peers that must acknowledge the packet.

### 9.2 ACK Tracking

ACK tracking is per neighbor.

When a neighbor ACKs the packet sequence number, that neighbor is removed from the packet's pending ACK set.

When all required neighbors have ACKed or been removed due to reset/drop policy, the reliable hold on the packet is released.

### 9.3 Retransmission

A reliable multicast packet must not be multicast again for retransmission.

Retransmission is unicast only to neighbors that have not ACKed.

Reason:

- peers that already processed the reliable packet must not be forced to process it again
- duplicate QUERY/UPDATE delivery can cause unnecessary DUAL churn
- RFC reliable transport describes multicast first send and unicast retransmission to missing peers

### 9.4 Packet References

A packet may be held by:

```text
interface packet queue
send-in-progress state
neighbor ACK state
retransmit timer/state
```

The packet is freed only after the last holder releases it.

A future implementation may use refcounting or an equivalent explicit ownership model.

## 10. Packet Type Behavior

### 10.1 Hello and ACK

Hello and ACK packet generation are not DUAL work items.

They may continue to use packet send paths directly as long as packet safety, checksum/auth, and queue ownership rules are honored.

### 10.2 Update

UPDATE packet generation for topology changes should flow through the packetizer queue.

Startup full-table UPDATEs may use the same packetizer machinery or a dedicated startup walker, but the final packets must still be interface-specific and pacing-aware.

### 10.3 Query

QUERY packet generation should flow through the packetizer queue.

DUAL marks the destination as needing QUERY work.

The packetizer decides which interfaces and neighbors receive the QUERY and applies split horizon and pending-neighbor rules.

### 10.4 Reply and SIA-Reply

REPLY and SIA-REPLY use the same packetizer path. The work item carries the desired opcode.

The inbound QUERY/SIA-QUERY identifies the neighbor/interface context, so these are neighbor-specific route work items. The packetizer owns final TLV encoding and packet construction.

### 10.5 Query and SIA-Query

QUERY and SIA-QUERY use the same packetizer path where the packet body is concerned. The work item carries the desired opcode.

Standard QUERY is normally destination/interface scoped and may multicast on first send. SIA-QUERY is neighbor/destination specific. The packetizer uses the opcode and neighbor/interface context to choose the final transmission behavior.

## 11. Split Horizon, Poison Reverse, and Pending Neighbors

Packetizer owns outbound suppression/poison decisions because they are interface/neighbor sensitive.

Rules:

- do not send QUERY/UPDATE to pending neighbors that are not eligible for convergence
- apply split horizon based on the inbound/successor interface context
- use poison reverse where required instead of suppression
- re-evaluate these rules at packetization time

## 12. Packet Sizing

Packetizer owns packet size management.

Rules:

- do not exceed interface MTU
- split multiple destinations across multiple packets when needed
- do not split one TLV across packets
- preserve reliable transport sequencing rules
- packet fragments are EIGRP packets, not IP fragments

## 13. Proposed Modules

### 13.1 `eigrp_packetizer.[c|h]`

Owns packetizer work items and work-to-packet orchestration.

Public API:

```c
void eigrp_packetizer_init(eigrp_instance_t *eigrp);
void eigrp_packetizer_finish(eigrp_instance_t *eigrp);
eigrp_packetizer_work_t *eigrp_packetizer_work_new(uint8_t opcode);
void eigrp_packetizer_work_free(eigrp_packetizer_work_t *work);
void eigrp_packetizer_enqueue(eigrp_instance_t *eigrp,
                              eigrp_packetizer_work_t *work);
```

The packetizer work item is intentionally opcode-driven instead of subtype-function-driven:

```c
typedef struct eigrp_packetizer_work {
    uint8_t opcode;
    eigrp_prefix_descriptor_t *prefix;
    eigrp_route_descriptor_t *route;
    eigrp_interface_t *exception;
    eigrp_neighbor_t *nbr;
    void *owner;
    uint32_t flags;
} eigrp_packetizer_work_t;
```

`QUERY` and `SIA-QUERY` share packetizing behavior and differ by opcode/context. `REPLY` and `SIA-REPLY` share packetizing behavior and differ by opcode/context.

### 13.2 `eigrp_southbound.[c|h]`

Owns host runtime adaptation.

Initial FRR-backed work queue API:

```c
eigrp_work_queue_t *eigrp_work_queue_new(eigrp_instance_t *eigrp,
                                         const char *name,
                                         eigrp_work_queue_func_t workfunc,
                                         eigrp_work_queue_delete_func_t deletefunc);
void eigrp_work_queue_free(eigrp_work_queue_t *queue);
void eigrp_work_queue_reset(eigrp_work_queue_t *queue);
void eigrp_work_queue_enqueue(eigrp_work_queue_t *queue, void *data);
eigrp_instance_t *eigrp_work_queue_eigrp(eigrp_work_queue_t *queue);
```

FRR-specific details such as `struct work_queue`, `work_queue_add()`, `work_queue_new()`, and event-loop scheduling live here only.

### 13.3 `eigrp_packet_queue.[c|h]`

Owns built packet queues.

The current built packet queue remains in `eigrp_packet.[c|h]` for this pass. It can be moved into `eigrp_packet_queue.[c|h]` later when that refactor can be done without mixing it into the packetizer work queue change.

Candidate future API:

```c
eigrp_packet_queue_t *eigrp_packet_queue_create(void);
void eigrp_packet_queue_delete(eigrp_packet_queue_t *queue);
void eigrp_packet_queue_enqueue(eigrp_packet_queue_t *queue, eigrp_packet_t *packet);
eigrp_packet_t *eigrp_packet_queue_peek(eigrp_packet_queue_t *queue);
eigrp_packet_t *eigrp_packet_queue_dequeue(eigrp_packet_queue_t *queue);
bool eigrp_packet_queue_empty(eigrp_packet_queue_t *queue);
```

## 14. Implementation Notes

Initial implementation should favor correctness over optimization.

Acceptable first pass:

```text
one work item processed at a time
one built packet per interface
FRR watched/work queue hidden behind `eigrp_southbound`
clear debug dumps
hard validation before queueing packet refs
```

Optimizations can come later:

```text
coalescing many NDBs into fewer packets
interface peer type caching
shared immutable packet buffers where provably safe
reduced wakeups
packet packing by opcode/type/capability
```

Do not add legacy aliases or parallel old/new packet paths.

## 15. Open Questions

These should be answered before deep code work:

1. Current code still uses `eigrp_prefix_descriptor_t` and `eigrp_route_descriptor_t` for topology prefix/route records. Their lifecycle APIs are owned by topology and named `eigrp_topology_prefix_create/free()` and `eigrp_topology_route_create/free()`. A later typedef rename to NDB/RDB terminology can be reviewed separately.
2. Should interface peer type be cached on the interface or calculated during each packetizer pass?
3. Where should packet refcount ownership live: inside `eigrp_packet_t`, or in a small wrapper object owned by reliable transport?
4. Should startup full-table UPDATEs use the same packetizer queue from day one, or be migrated after topology-change packetizing is stable?
