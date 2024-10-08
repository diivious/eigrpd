// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * EIGRP General Sending and Receiving of EIGRP Packets.
 * Copyright (C) 2013-2014
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
 */
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_auth.h"

#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_errors.h"

DEFINE_MTYPE_STATIC(EIGRPD, EIGRP_PACKET,          "EIGRP Packet");
DEFINE_MTYPE_STATIC(EIGRPD, EIGRP_PACKET_QUEUE,    "EIGRP Packet Queue");
DEFINE_MTYPE_STATIC(EIGRPD, EIGRP_IPV4_INT_TLV,    "EIGRP IPv4 TLV");
DEFINE_MTYPE_STATIC(EIGRPD, EIGRP_SEQ_TLV,         "EIGRP SEQ TLV");

/* Packet Type String. */
const struct message eigrp_packet_type_str[] = {
	{EIGRP_OPC_UPDATE, "Update"},
	{EIGRP_OPC_REQUEST, "Request"},
	{EIGRP_OPC_QUERY, "Query"},
	{EIGRP_OPC_REPLY, "Reply"},
	{EIGRP_OPC_HELLO, "Hello"},
	{EIGRP_OPC_IPXSAP, "IPX-SAP"},
	{EIGRP_OPC_PROBE, "Probe"},
	{EIGRP_OPC_ACK, "Ack"},
	{EIGRP_OPC_SIAQUERY, "SIAQuery"},
	{EIGRP_OPC_SIAREPLY, "SIAReply"},
	{0}};

/* Forward function reference*/
static struct stream *eigrp_packet_recv(eigrp_instance_t *eigrp, int fd,
					struct interface **ifp,
					struct stream *s);
static int eigrp_verify_header(struct stream *s, eigrp_interface_t *ei,
			       struct ip *addr, struct eigrp_header *header);
static int eigrp_check_network_mask(eigrp_interface_t *ei, struct in_addr mask);

static int eigrp_retrans_count_exceeded(eigrp_packet_t *packet,
					eigrp_neighbor_t *nbr)
{
	return 1;
}

/*
 * New testing block of code for handling Acks
 */
static void eigrp_packet_ack(eigrp_instance_t *eigrp, struct eigrp_header *eigrph,
			     eigrp_neighbor_t *nbr)
{
	struct eigrp_packet *packet = NULL;

	packet = eigrp_packet_queue_next(nbr->retrans_queue);
	if ((packet) && (ntohl(eigrph->ack) == packet->sequence_number)) {
		packet = eigrp_packet_dequeue(nbr->retrans_queue);
		eigrp_packet_free(packet);

		if ((nbr->state == EIGRP_NEIGHBOR_PENDING)
		    && (ntohl(eigrph->ack) == nbr->init_sequence_number)) {
			eigrp_nbr_state_set(nbr, EIGRP_NEIGHBOR_UP);
			zlog_info("Neighbor(%s) adjacency became full",
				  eigrp_print_addr(&nbr->src));
			nbr->init_sequence_number = 0;
			nbr->recv_sequence_number = ntohl(eigrph->sequence);
			eigrp_update_send_EOT(nbr);
		} else
			eigrp_packet_send_reliably(eigrp, nbr);
	}

	packet = eigrp_packet_queue_next(nbr->multicast_queue);
	if (packet) {
		if (ntohl(eigrph->ack) == packet->sequence_number) {
			packet = eigrp_packet_dequeue(nbr->multicast_queue);
			eigrp_packet_free(packet);
			if (nbr->multicast_queue->count > 0) {
				eigrp_packet_send_reliably(eigrp, nbr);
			}
		}
	}
}

void eigrp_packet_write(struct event *event)
{
	eigrp_instance_t *eigrp = EVENT_ARG(event);
	struct eigrp_header *eigrph;
	eigrp_interface_t *ei;
	eigrp_packet_t *packet;
	struct sockaddr_in sa_dst;
	struct ip iph;
	struct msghdr msg;
	struct iovec iov[2];
	uint32_t seqno, ack;

	int ret;
	int flags = 0;
	struct listnode *node;
#ifdef WANT_EIGRP_PACKET_WRITE_FRAGMENT
	static uint16_t ipid = 0;
#endif /* WANT_EIGRP_PACKET_WRITE_FRAGMENT */
#define EIGRP_PACKET_WRITE_IPHL_SHIFT 2

	node = listhead(eigrp->oi_write_q);
	assert(node);
	ei = listgetdata(node);
	assert(ei);

#ifdef WANT_EIGRP_PACKET_WRITE_FRAGMENT
	/* seed ipid static with low order bits of time */
	if (ipid == 0)
		ipid = (time(NULL) & 0xffff);
#endif /* WANT_EIGRP_PACKET_WRITE_FRAGMENT */

	/* Get one packet from queue. */
	packet = eigrp_packet_queue_next(ei->obuf);
	if (!packet) {
		flog_err(EC_LIB_DEVELOPMENT,
			 "%s: Interface %s no packet on queue?", __func__,
			 ei->ifp->name);
		goto out;
	}
	if (packet->length < EIGRP_HEADER_LEN) {
		flog_err(EC_EIGRP_PACKET, "%s: Packet just has a header?",
			 __func__);
		eigrp_header_dump((struct eigrp_header *)packet->s->data);
		eigrp_packet_delete(ei);
		goto out;
	}
	// DVS: ipv6 issue
	if (packet->dst.ip.v4.s_addr == htonl(EIGRP_MULTICAST_ADDRESS))
		eigrp_intf_ipmulticast(eigrp, &ei->address, ei->ifp->ifindex);

	memset(&iph, 0, sizeof(struct ip));
	memset(&sa_dst, 0, sizeof(sa_dst));

	/*
	 * We build and schedule packets to go out
	 * in the future.  In the mean time we may
	 * process some update packets from the
	 * neighbor, thus making it necessary
	 * to update the ack we are using for
	 * this outgoing packet.
	 */
	eigrph = (struct eigrp_header *)STREAM_DATA(packet->s);
	seqno = ntohl(eigrph->sequence);
	ack = ntohl(eigrph->ack);
	if (packet->nbr && (ack != packet->nbr->recv_sequence_number)) {
		eigrph->ack = htonl(packet->nbr->recv_sequence_number);
		ack = packet->nbr->recv_sequence_number;
		eigrph->checksum = 0;
		eigrp_packet_checksum(ei, packet->s, packet->length);
	}

	sa_dst.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
	sa_dst.sin_len = sizeof(sa_dst);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */

	// DVS: ipv6 issue
	sa_dst.sin_addr = packet->dst.ip.v4;
	sa_dst.sin_port = htons(0);

	/* Set DONTROUTE flag if dst is unicast. */
	if (!IN_MULTICAST(htonl(packet->dst.ip.v4.s_addr)))
		flags = MSG_DONTROUTE;

	iph.ip_hl = sizeof(struct ip) >> EIGRP_PACKET_WRITE_IPHL_SHIFT;
	/* it'd be very strange for header to not be 4byte-word aligned but.. */
	if (sizeof(struct ip)
	    > (unsigned int)(iph.ip_hl << EIGRP_PACKET_WRITE_IPHL_SHIFT))
		iph.ip_hl++; /* we presume sizeof struct ip cant overflow
				ip_hl.. */

	iph.ip_v = IPVERSION;
	iph.ip_tos = IPTOS_PREC_INTERNETCONTROL;
	iph.ip_len = (iph.ip_hl << EIGRP_PACKET_WRITE_IPHL_SHIFT) + packet->length;

#if defined(__DragonFly__)
	/*
	 * DragonFly's raw socket expects ip_len/ip_off in network byte order.
	 */
	iph.ip_len = htons(iph.ip_len);
#endif

	iph.ip_off = 0;
	iph.ip_ttl = EIGRP_IP_TTL;
	iph.ip_p = IPPROTO_EIGRPIGP;
	iph.ip_sum = 0;
	// DVS: ipv6 issue
	iph.ip_src.s_addr = ei->address.u.prefix4.s_addr;
	iph.ip_dst.s_addr = packet->dst.ip.v4.s_addr;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (caddr_t)&sa_dst;
	msg.msg_namelen = sizeof(sa_dst);
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;

	iov[0].iov_base = (char *)&iph;
	iov[0].iov_len = iph.ip_hl << EIGRP_PACKET_WRITE_IPHL_SHIFT;
	iov[1].iov_base = stream_pnt(packet->s);
	iov[1].iov_len = packet->length;

	/* send final fragment (could be first) */
	sockopt_iphdrincl_swab_htosys(&iph);
	ret = sendmsg(eigrp->fd, &msg, flags);
	sockopt_iphdrincl_swab_systoh(&iph);

	if (IS_DEBUG_EIGRP_TRANSMIT(0, SEND)) {
		eigrph = (struct eigrp_header *)STREAM_DATA(packet->s);
		zlog_debug(
			"Sending [%s][%d/%d] to [%s] via [%s] ret [%d].",
			lookup_msg(eigrp_packet_type_str, eigrph->opcode, NULL),
			seqno, ack, eigrp_print_addr(&packet->dst),
			EIGRP_INTF_NAME(ei), ret);
	}

	if (ret < 0) //DVS: IPV6 issue
		zlog_warn("*** sendmsg in eigrp_packet_write failed to %pI4, "
			  "id %d, off %d, len %d, interface %s, mtu %u: %s",
			  &iph.ip_dst, iph.ip_id, iph.ip_off,
			  iph.ip_len, ei->ifp->name, ei->ifp->mtu,
			  safe_strerror(errno));

	/* Now delete packet from queue. */
	eigrp_packet_delete(ei);

out:
	if (eigrp_packet_queue_next(ei->obuf) == NULL) {
		ei->on_write_q = 0;
		list_delete_node(eigrp->oi_write_q, node);
	}

	/* If packets still remain in queue, call write event. */
	if (!list_isempty(eigrp->oi_write_q)) {
	    EIGRP_EVENT_ADD_WRITE(eigrp);
	}

	return;
}

/* Starting point of packet process function. */
void eigrp_packet_read(struct event *event)
{
	int ret;
	struct stream *ibuf;
	eigrp_instance_t *eigrp;
	eigrp_interface_t *ei;
	struct ip *iph;
	struct eigrp_header *eigrph;
	struct interface *ifp;
	eigrp_addr_t src;
	eigrp_neighbor_t *nbr;
	struct in_addr srcaddr;
	uint16_t opcode = 0;
	uint16_t length = 0;

	/* first of all get interface pointer. */
	eigrp = EVENT_ARG(event);

	/* prepare for next packet. */
	event_add_read(eigrpd_event, eigrp_packet_read, eigrp, eigrp->fd, &eigrp->t_read);

	stream_reset(eigrp->ibuf);
	if (!(ibuf = eigrp_packet_recv(eigrp, eigrp->fd, &ifp, eigrp->ibuf))) {
		/* This raw packet is known to be at least as big as its IP
		 * header. */
		return;
	}

	/* Note that there should not be alignment problems with this assignment
	   because this is at the beginning of the stream data buffer. */
	iph = (struct ip *)STREAM_DATA(ibuf);

	// Substract IPv4 header size from EIGRP Packet itself
	if (iph->ip_v == 4) {
		length = (iph->ip_len) - 20U;
		// DVS: get it into a workable form, but this is an ugly hack
		//      cleaning these up as I get ipv6 fixed
		src.afi = AF_INET;
		src.ip.v4 = iph->ip_src;
	} else {
	    // DVS: your now broken...
	    src.afi = AF_INET6;
	    zlog_debug("IPv6: Neighbor not supported");
	    return;
	}

	srcaddr = iph->ip_src;

	/* IP Header dump. */
	if (IS_DEBUG_EIGRP_TRANSMIT(0, RECV)
	    && IS_DEBUG_EIGRP_TRANSMIT(0, PACKET_DETAIL))
		eigrp_ip_header_dump(iph);

	/* Note that sockopt_iphdrincl_swab_systoh was called in
	 * eigrp_packet_recv. */
	if (ifp == NULL) {
		struct connected *c;
		/* Handle cases where the platform does not support retrieving
		   the ifindex,
		   and also platforms (such as Solaris 8) that claim to support
		   ifindex
		   retrieval but do not. */
		c = if_lookup_address((void *)&srcaddr, AF_INET, eigrp->vrf_id);

		if (c == NULL)
			return;

		ifp = c->ifp;
	}

	/* associate packet with eigrp interface */
	ei = ifp->info;

	/* eigrp_verify_header() relies on a valid "ei" and thus can be called
	   only
	   after the checks below are passed. These checks in turn access the
	   fields of unverified "eigrph" structure for their own purposes and
	   must remain very accurate in doing this.
	*/
	if (!ei)
		return;

	/* Self-originated packet should be discarded silently. */
	if (eigrp_intf_lookup_by_local_addr(eigrp, NULL, iph->ip_src)
	    || (IPV4_ADDR_SAME(&srcaddr, &ei->address.u.prefix4))) {
		if (IS_DEBUG_EIGRP_TRANSMIT(0, RECV))
			zlog_debug("eigrp_packet_read[%pI4]: Dropping self-originated packet",
				   &srcaddr);
		return;
	}

	/* Advance from IP header to EIGRP header (iph->ip_hl has been verified
	   by eigrp_packet_recv() to be correct). */

	stream_forward_getp(ibuf, (iph->ip_hl * 4));
	eigrph = (struct eigrp_header *)stream_pnt(ibuf);

	if (IS_DEBUG_EIGRP_TRANSMIT(0, RECV) && IS_DEBUG_EIGRP_TRANSMIT(0, PACKET_DETAIL))
		eigrp_header_dump(eigrph);

	//  if (MSG_OK != eigrp_packet_examin(eigrph, stream_get_endp(ibuf) -
	//  stream_get_getp(ibuf)))
	//    return;

	/* If incoming interface is passive one, ignore it. */
	if (eigrp_intf_is_passive(ei)) {
		if (IS_DEBUG_EIGRP_TRANSMIT(0, RECV))
			zlog_debug("ignoring packet from router %u sent to %pI4, received on a passive interface, %pI4",
				ntohs(eigrph->vrid), &iph->ip_dst,
				&ei->address.u.prefix4);

		if (iph->ip_dst.s_addr == htonl(EIGRP_MULTICAST_ADDRESS)) {
			eigrp_intf_set_multicast(ei);
		}
		return;
	}

	/* else it must be a local eigrp interface, check it was received on
	 * correct link
	 */
	else if (ei->ifp != ifp) {
		if (IS_DEBUG_EIGRP_TRANSMIT(0, RECV))
			zlog_warn(
				"Packet from [%pI4] received on wrong link %s",
				&iph->ip_src, ifp->name);
		return;
	}

	/* Verify more EIGRP header fields. */
	ret = eigrp_verify_header(ibuf, ei, iph, eigrph);
	if (ret < 0) {
		if (IS_DEBUG_EIGRP_TRANSMIT(0, RECV))
			zlog_debug(
				"eigrp_packet_read[%pI4]: Header check failed, dropping.",
				&iph->ip_src);
		return;
	}

	/* calcualte the eigrp packet length, and move the pounter to the
	   start of the eigrp TLVs */
	opcode = eigrph->opcode;

	if (IS_DEBUG_EIGRP_TRANSMIT(0, RECV))
		zlog_debug(
			"Received [%s][%d/%d] length [%u] via [%s] src [%pI4] dst [%pI4]",
			lookup_msg(eigrp_packet_type_str, opcode, NULL),
			ntohl(eigrph->sequence), ntohl(eigrph->ack), length,
			EIGRP_INTF_NAME(ei), &iph->ip_src, &iph->ip_dst);

	/* Read rest of the packet and call each sort of packet routine. */
	stream_forward_getp(ibuf, EIGRP_HEADER_LEN);

	 /*
	 * handle case where neigbor is sending hello, could be first time we
	 * have seen this neghbor, if so create the need data structures
	 */
	if (opcode == EIGRP_OPC_HELLO) {
	    eigrp_hello_receive(eigrp, eigrph, &src, ei, ibuf, length);
	}

	/* neighbor must be valid, if not ignore this packet until/unless
	 * neigbor says hello frist.  mannors you know...
	 */
	nbr = eigrp_nbr_lookup(ei, eigrph, &src);
	if (!nbr) {
		return;
	}

	if (ntohl(eigrph->ack)) {
		eigrp_packet_ack(eigrp, eigrph, nbr);
	}

	/* process all known opcodes */
	switch (opcode) {
	case EIGRP_OPC_PROBE:
		// eigrp_probe_receive(eigrp, nbr, eigrph, ibuf, ei, length);
		break;
	case EIGRP_OPC_QUERY:
		eigrp_query_receive(eigrp, nbr, eigrph, ibuf, ei, length);
		break;
	case EIGRP_OPC_REPLY:
		eigrp_reply_receive(eigrp, nbr, eigrph, ibuf, ei, length);
		break;
	case EIGRP_OPC_REQUEST:
		// eigrp_request_receive(eigrp, nbr, eigrph, ibuf, ei, length);
		break;
	case EIGRP_OPC_SIAQUERY:
		eigrp_query_receive(eigrp, nbr, eigrph, ibuf, ei, length);
		break;
	case EIGRP_OPC_SIAREPLY:
		eigrp_reply_receive(eigrp, nbr, eigrph, ibuf, ei, length);
		break;
	case EIGRP_OPC_UPDATE:
		eigrp_update_receive(eigrp, nbr, eigrph, ibuf, ei, length);
		break;
	default:
		zlog_warn(
			"interface %s: EIGRP packet header type %d unsupported",
			EIGRP_INTF_NAME(ei), opcode);
		break;
	}

	return;
}

static struct stream *eigrp_packet_recv(eigrp_instance_t *eigrp, int fd,
					struct interface **ifp,
					struct stream *ibuf)
{
	int ret;
	struct ip *iph;
	uint16_t ip_len;
	unsigned int ifindex = 0;
	struct iovec iov;
	/* Header and data both require alignment. */
	char buff[CMSG_SPACE(SOPT_SIZE_CMSG_IFINDEX_IPV4())];
	struct msghdr msgh;

	memset(&msgh, 0, sizeof(struct msghdr));
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = (caddr_t)buff;
	msgh.msg_controllen = sizeof(buff);

	ret = stream_recvmsg(ibuf, fd, &msgh, 0, (EIGRP_PACKET_MAX_LEN + 1));
	if (ret < 0) {
		zlog_warn("stream_recvmsg failed: %s", safe_strerror(errno));
		return NULL;
	}
	if ((unsigned int)ret < sizeof(*iph)) /* ret must be > 0 now */
	{
		zlog_warn(
			"eigrp_packet_recv: discarding runt packet of length %d "
			"(ip header size is %u)",
			ret, (unsigned int)sizeof(*iph));
		return NULL;
	}

	/* Note that there should not be alignment problems with this assignment
	   because this is at the beginning of the stream data buffer. */
	iph = (struct ip *)STREAM_DATA(ibuf);
	sockopt_iphdrincl_swab_systoh(iph);

	ip_len = iph->ip_len;

#if defined(__FreeBSD__) && (__FreeBSD_version < 1000000)
	/*
	 * Kernel network code touches incoming IP header parameters,
	 * before protocol specific processing.
	 *
	 *   1) Convert byteorder to host representation.
	 *      --> ip_len, ip_id, ip_off
	 *
	 *   2) Adjust ip_len to strip IP header size!
	 *      --> If user process receives entire IP packet via RAW
	 *          socket, it must consider adding IP header size to
	 *          the "ip_len" field of "ip" structure.
	 *
	 * For more details, see <netinet/ip_input.c>.
	 */
	ip_len = ip_len + (iph->ip_hl << 2);
#endif

#if defined(__DragonFly__)
	/*
	 * in DragonFly's raw socket, ip_len/ip_off are read
	 * in network byte order.
	 * As OpenBSD < 200311 adjust ip_len to strip IP header size!
	 */
	ip_len = ntohs(iph->ip_len) + (iph->ip_hl << 2);
#endif

	ifindex = getsockopt_ifindex(AF_INET, &msgh);

	*ifp = if_lookup_by_index(ifindex, eigrp->vrf_id);

	if (ret != ip_len) {
		zlog_warn(
			"eigrp_packet_recv read length mismatch: ip_len is %d, "
			"but recvmsg returned %d",
			ip_len, ret);
		return NULL;
	}

	return ibuf;
}

eigrp_packet_queue_t *eigrp_packet_queue_new(void)
{
	eigrp_packet_queue_t *new;

	new = XCALLOC(MTYPE_EIGRP_PACKET_QUEUE, sizeof(eigrp_packet_queue_t));
	return new;
}

/* Free eigrp packet queue. */
void eigrp_packet_queue_free(eigrp_packet_queue_t *queue)
{
	eigrp_packet_t *packet;
	eigrp_packet_t *next;

	for (packet = queue->head; packet; packet = next) {
		next = packet->next;
		eigrp_packet_free(packet);
	}
	queue->head = queue->tail = NULL;
	queue->count = 0;

	XFREE(MTYPE_EIGRP_PACKET_QUEUE, queue);
}

/* Free eigrp queue entries without destroying queue itself*/
void eigrp_packet_queue_reset(eigrp_packet_queue_t *queue)
{
	eigrp_packet_t *packet;
	eigrp_packet_t *next;

	for (packet = queue->head; packet; packet = next) {
		next = packet->next;
		eigrp_packet_free(packet);
	}
	queue->head = queue->tail = NULL;
	queue->count = 0;
}

eigrp_packet_t *eigrp_packet_new(size_t size, eigrp_neighbor_t *nbr)
{
	eigrp_packet_t *new;

	new = XCALLOC(MTYPE_EIGRP_PACKET, sizeof(eigrp_packet_t));
	new->s = stream_new(size);
	new->retrans_counter = 0;
	new->nbr = nbr;

	return new;
}

void eigrp_packet_send_reliably(eigrp_instance_t *eigrp, eigrp_neighbor_t *nbr)
{
	eigrp_packet_t *packet;

	packet = eigrp_packet_queue_next(nbr->retrans_queue);

	if (packet) {
		eigrp_packet_t *duplicate;
		duplicate = eigrp_packet_duplicate(packet, nbr);
		/* Add packet to the top of the interface output queue*/
		eigrp_packet_enqueue(nbr->ei->obuf, duplicate);

		/*Start retransmission timer*/
		event_add_timer(eigrpd_event, eigrp_packet_unack_retrans, nbr,
				 EIGRP_PACKET_RETRANS_TIME,
				 &packet->t_retrans_timer);

		/*Increment sequence number counter*/
		nbr->ei->eigrp->sequence_number++;

		/* Hook event to write packet. */
		if (nbr->ei->on_write_q == 0) {
			listnode_add(nbr->ei->eigrp->oi_write_q, nbr->ei);
			nbr->ei->on_write_q = 1;
		}
		EIGRP_EVENT_ADD_WRITE(nbr->ei->eigrp);
	}
}

/* Calculate EIGRP checksum */
void eigrp_packet_checksum(eigrp_interface_t *ei, struct stream *s,
			   uint16_t length)
{
	struct eigrp_header *eigrph;

	eigrph = (struct eigrp_header *)STREAM_DATA(s);

	/* Calculate checksum. */
	eigrph->checksum = in_cksum(eigrph, length);
}

/* Make EIGRP header. */
void eigrp_packet_header_init(int type, eigrp_instance_t *eigrp, struct stream *s,
			      uint32_t flags, uint32_t sequence, uint32_t ack)
{
	struct eigrp_header *eigrph;

	stream_reset(s);
	eigrph = (struct eigrp_header *)STREAM_DATA(s);

	eigrph->version = (uint8_t)EIGRP_HEADER_VERSION;
	eigrph->opcode = (uint8_t)type;
	eigrph->checksum = 0;

	eigrph->vrid = htons(eigrp->vrid);
	eigrph->ASNumber = htons(eigrp->AS);
	eigrph->ack = htonl(ack);
	eigrph->sequence = htonl(sequence);
	//  if(flags == EIGRP_INIT_FLAG)
	//    eigrph->sequence = htonl(3);
	eigrph->flags = htonl(flags);

	if (IS_DEBUG_EIGRP_TRANSMIT(0, PACKET_DETAIL))
		zlog_debug("Packet Header Init Seq [%u] Ack [%u]",
			   htonl(eigrph->sequence), htonl(eigrph->ack));

	stream_forward_endp(s, EIGRP_HEADER_LEN);
}

/* Add new packet to head of queue. */
void eigrp_packet_enqueue(eigrp_packet_queue_t *queue, eigrp_packet_t *packet)
{
	packet->next = queue->head;
	packet->previous = NULL;

	if (queue->tail == NULL)
		queue->tail = packet;

	if (queue->count != 0)
		queue->head->previous = packet;

	queue->head = packet;

	queue->count++;
}

/* Return last queue entry. */
eigrp_packet_t *eigrp_packet_queue_next(eigrp_packet_queue_t *queue)
{
	return queue->tail;
}

void eigrp_packet_delete(eigrp_interface_t *ei)
{
	eigrp_packet_t *packet;

	packet = eigrp_packet_dequeue(ei->obuf);

	if (packet)
		eigrp_packet_free(packet);
}

void eigrp_packet_free(eigrp_packet_t *packet)
{
	if (packet->s)
		stream_free(packet->s);

	EVENT_OFF(packet->t_retrans_timer);

	XFREE(MTYPE_EIGRP_PACKET, packet);
}

/* EIGRP Header verification. */
static int eigrp_verify_header(struct stream *ibuf, eigrp_interface_t *ei,
			       struct ip *iph, struct eigrp_header *eigrph)
{
	/* Check network mask, Silently discarded. */
	if (!eigrp_check_network_mask(ei, iph->ip_src)) {
		zlog_warn(
			"interface %s: eigrp_packet_read network address is not same [%pI4]",
			EIGRP_INTF_NAME(ei), &iph->ip_src);
		return -1;
	}
	//
	//  /* Check authentication. The function handles logging actions, where
	//  required. */
	//  if (! eigrp_check_auth(ei, eigrph))
	//    return -1;

	return 0;
}

/* Unbound socket will accept any Raw IP packets if proto is matched.
   To prevent it, compare src IP address and i/f address with masking
   i/f network mask. */
static int eigrp_check_network_mask(eigrp_interface_t *ei,
				    struct in_addr ip_src)
{
	struct in_addr mask, me, him;

	if (ei->type == EIGRP_IFTYPE_POINTOPOINT)
		return 1;

	masklen2ip(ei->address.prefixlen, &mask);

	me.s_addr = ei->address.u.prefix4.s_addr & mask.s_addr;
	him.s_addr = ip_src.s_addr & mask.s_addr;

	if (IPV4_ADDR_SAME(&me, &him))
		return 1;

	return 0;
}

void eigrp_packet_unack_retrans(struct event *event)
{
	eigrp_neighbor_t *nbr;
	nbr = (eigrp_neighbor_t *)EVENT_ARG(event);

	eigrp_packet_t *packet;
	packet = eigrp_packet_queue_next(nbr->retrans_queue);

	if (packet) {
		eigrp_packet_t *duplicate;
		duplicate = eigrp_packet_duplicate(packet, nbr);

		/* Add packet to the top of the interface output queue*/
		eigrp_packet_enqueue(nbr->ei->obuf, duplicate);

		packet->retrans_counter++;
		if (packet->retrans_counter == EIGRP_PACKET_RETRANS_MAX) {
			eigrp_retrans_count_exceeded(packet, nbr);
			return;
		}

		/*Start retransmission timer*/
		event_add_timer(eigrpd_event, eigrp_packet_unack_retrans, nbr,
				 EIGRP_PACKET_RETRANS_TIME,
				 &packet->t_retrans_timer);

		/* Hook event to write packet. */
		if (nbr->ei->on_write_q == 0) {
			listnode_add(nbr->ei->eigrp->oi_write_q, nbr->ei);
			nbr->ei->on_write_q = 1;
		}
		EIGRP_EVENT_ADD_WRITE(nbr->ei->eigrp);
	}

	return;
}

void eigrp_packet_unack_multicast_retrans(struct event *event)
{
	eigrp_neighbor_t *nbr;
	nbr = (eigrp_neighbor_t *)EVENT_ARG(event);

	eigrp_packet_t *packet;
	packet = eigrp_packet_queue_next(nbr->multicast_queue);

	if (packet) {
		eigrp_packet_t *duplicate;
		duplicate = eigrp_packet_duplicate(packet, nbr);
		/* Add packet to the top of the interface output queue*/
		eigrp_packet_enqueue(nbr->ei->obuf, duplicate);

		packet->retrans_counter++;
		if (packet->retrans_counter == EIGRP_PACKET_RETRANS_MAX) {
		    eigrp_retrans_count_exceeded(packet, nbr);
		    return;
		}

		/*Start retransmission timer*/
		event_add_timer(eigrpd_event, eigrp_packet_unack_multicast_retrans,
				 nbr, EIGRP_PACKET_RETRANS_TIME,
				 &packet->t_retrans_timer);

		/* Hook event to write packet. */
		if (nbr->ei->on_write_q == 0) {
			listnode_add(nbr->ei->eigrp->oi_write_q, nbr->ei);
			nbr->ei->on_write_q = 1;
		}
		EIGRP_EVENT_ADD_WRITE(nbr->ei->eigrp);
	}

	return;
}

/* Get packet from tail of queue. */
eigrp_packet_t *eigrp_packet_dequeue(eigrp_packet_queue_t *queue)
{
	eigrp_packet_t *packet = NULL;

	packet = queue->tail;

	if (packet) {
		queue->tail = packet->previous;

		if (queue->tail == NULL)
			queue->head = NULL;
		else
			queue->tail->next = NULL;

		queue->count--;
	}

	return packet;
}

eigrp_packet_t *eigrp_packet_duplicate(eigrp_packet_t *old,
				       eigrp_neighbor_t *nbr)
{
	eigrp_packet_t *new;

	new = eigrp_packet_new(EIGRP_PACKET_MTU(nbr->ei->ifp->mtu), nbr);
	new->length = old->length;
	new->retrans_counter = old->retrans_counter;
	new->dst = old->dst;
	new->sequence_number = old->sequence_number;
	stream_copy(new->s, old->s);

	return new;
}

void eigrp_IPv4_InternalTLV_free(
	struct TLV_IPv4_Internal_type *IPv4_InternalTLV)
{
	XFREE(MTYPE_EIGRP_IPV4_INT_TLV, IPv4_InternalTLV);
}

struct TLV_Sequence_Type *eigrp_SequenceTLV_new(void)
{
	struct TLV_Sequence_Type *new;

	new = XCALLOC(MTYPE_EIGRP_SEQ_TLV, sizeof(struct TLV_Sequence_Type));

	return new;
}
