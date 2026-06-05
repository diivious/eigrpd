## Application Process and Worker Model

Named EIGRP is modeled as a parent process with one or more address-family workers.

`router eigrp <name>` creates the parent named process container. Removing that command destroys the parent process and all child address-family state.

Each configured `address-family <afi> [vrf <vrf>] autonomous-system <as>` creates a runnable EIGRP protocol context. When enabled, that address-family context owns its packet-processing worker/thread, topology table, neighbor table, packetizer queue, interface packet queues, timers, and reliable-transport state.

Inbound packets are received by the parent EIGRP receive path and demultiplexed to the correct address-family worker using:

- receiving VRF / socket context
- packet address family, IPv4 or IPv6
- receiving interface
- EIGRP header AS number

The AS number is mandatory for protocol acceptance. A packet whose AS does not match a configured enabled address-family context is discarded.

The local named process name is not carried on the wire. Therefore packet demux must not depend on the configured EIGRP name except as local ownership of the matching address-family context.

If multiple local named processes would create an ambiguous `{vrf, afi, as, interface}` receive context, configuration must reject it or the receive path must treat it as invalid.
