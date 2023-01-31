/*
 * MIT License
 *
 * Copyright (c) 2019-2021 Ecole Polytechnique Federale Lausanne (EPFL)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <iostream>
#include <stdint.h>
#include <assert.h>
#include <string>

// Must be before all DPDK includes
#include <rte_config.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_byteorder.h>
#include <rte_ring.h>
#include <rte_flow.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#define MEMPOOL_CACHE_SIZE 64
#define NB_MBUF ((1 << 17) - 1)
#define ETH_DEV_RX_QUEUE_SZ 4096 //1024
#define ETH_DEV_TX_QUEUE_SZ 512
#define BATCH_SIZE 256 

#define ETH_MTU 1500
#define UDP_MAX_LEN (ETH_MTU - sizeof(struct ipv4_hdr) - sizeof(struct udp_hdr))
#define L2_HDR_LEN sizeof(struct ether_hdr)
#define L3_HDR_LEN (L2_HDR_LEN + sizeof(struct ipv4_hdr))
#define UDP_HDRS_LEN (L3_HDR_LEN + sizeof(struct udp_hdr))

#define MAX_PATTERN_NUM     4
#define MAX_ACTION_NUM      2

#define NUM_PRIORITIES 1

using namespace std;

RTE_DEFINE_PER_LCORE(struct rte_eth_dev_tx_buffer *, tx_buf);//This is needed for batch transmission
RTE_DEFINE_PER_LCORE(int, queue_id);

struct rte_mempool *pktmbuf_pool;
static uint8_t nb_ports;
struct rte_ring *input_stream_ring;
struct rte_ring *output_stream_ring;
struct rte_ring *task_list_ring[NUM_PRIORITIES];

struct rte_eth_conf port_conf ;

typedef void *generic_buffer;

struct ip_tuple {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
} __packed;

struct net_sge {
	void *payload;
	uint32_t len;
	void *handle; /* Not to be used by applications */
};

/* Initialization */
int rss_reta_setup(uint8_t port_id, unsigned int lcore_num);
struct rte_flow * generate_udp_flow(uint16_t port_id, uint16_t rx_q, uint16_t dst_port, uint16_t dst_port_mask);
void dpdk_init(int *argc, char ***argv);
struct net_sge *alloc_net_sge(void);

/* Util */
static inline void pkt_dump(struct rte_mbuf *pkt);
static inline void get_local_mac(struct ether_addr *mac);
static inline void ip_addr_to_str(uint32_t addr, char *str);
static inline int str_to_eth_addr(const char *src, unsigned char *dst);

/* packet processing */
int dpdk_eth_send(struct rte_mbuf *pkt_buf, uint16_t len);
static RetrieveTaskPacket udp_recv(struct net_sge *entry, struct ip_tuple *id);

// net_sge * udp_in(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph, struct udp_hdr *udph);
// net_sge * ip_in(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph);
// net_sge * eth_in(struct rte_mbuf *pkt_buf);
net_sge * udp_in(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph, struct udp_hdr *udph, struct ip_tuple *id);
net_sge * ip_in(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph, struct ip_tuple *id);
net_sge * eth_in(struct rte_mbuf *pkt_buf, struct ip_tuple *id);
int eth_out(struct rte_mbuf *pkt_buf, uint16_t h_proto,	struct ether_addr *dst_haddr, uint16_t iplen);
void ip_out(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph, uint32_t src_ip, uint32_t dst_ip, uint8_t ttl, uint8_t tos, uint8_t proto, uint16_t l4len, struct ether_addr *dst_haddr);
int udp_out(struct rte_mbuf *pkt_buf, struct ip_tuple *id, int len);

/* Sending and recieving main functions */
static inline int udp_send(struct net_sge *entry, struct ip_tuple *id);
inline net_sge ** dpdk_net_poll(void);

/* Implementations */

static inline void pkt_dump(struct rte_mbuf *pkt)
{
	struct ether_hdr *ethh = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	struct ipv4_hdr *iph = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, sizeof(struct ether_hdr));
	struct udp_hdr *udph = rte_pktmbuf_mtod_offset(pkt, struct udp_hdr *, sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
	
	printf("DST MAC: ");
	for (int i = 0; i < ETHER_ADDR_LEN; i++)
		printf("%hhx ", (char)ethh->d_addr.addr_bytes[i]);
	printf("\nSRC MAC: ");
	for (int i = 0; i < ETHER_ADDR_LEN; i++)
		printf("%hhx ", (char)ethh->s_addr.addr_bytes[i]);
	char ipaddr[64];
	ip_addr_to_str(iph->src_addr, ipaddr);
	printf("\nSRC IP: %s\n", ipaddr);
	ip_addr_to_str(iph->dst_addr, ipaddr);
	printf("DST IP: %s\n", ipaddr);

	printf("SRC PORT: %i\n", udph->src_port);
	printf("DST PORT: %i\n", udph->dst_port);

}

int rss_reta_setup(uint8_t port_id, unsigned int lcore_num){
	
	struct rte_eth_rss_reta_entry64 reta_conf;
	int status;
	reta_conf.mask	= UINT64_MAX;

	// uint16_t curr_mask = 0;

	for (int i = 0 ; i < RTE_RETA_GROUP_SIZE ; i++ ){
		// reta_conf.reta[i] = curr_mask;
		// if(i > (RTE_RETA_GROUP_SIZE / lcore_num))
		// 	curr_mask++;
		reta_conf.reta[i] = i % lcore_num;

	}

	status = rte_eth_dev_rss_reta_update(port_id, &reta_conf , lcore_num); 

	return status;
}

struct rte_flow * generate_udp_flow(uint16_t port_id, uint16_t rx_q, uint16_t dst_port, uint16_t dst_port_mask){

	struct rte_flow_attr attr;
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action action[MAX_ACTION_NUM];
    struct rte_flow *flow = NULL;
    struct rte_flow_action_queue queue = { .index = rx_q };
    struct rte_flow_item_udp udp_spec;
	struct rte_flow_item_udp udp_mask;
    int res;

    memset(pattern, 0, sizeof(pattern));
    memset(action, 0, sizeof(action));

	/* Create attribute, only incoming packets concern us here */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
    attr.ingress = 1;

	/* Create action move packet to the specified queue */
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    action[0].conf = &queue;
    action[1].type = RTE_FLOW_ACTION_TYPE_END;

	/* Create patterns, our only concern is udp pattern */
    pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	
	memset(&udp_spec, 0, sizeof(struct rte_flow_item_udp));
    memset(&udp_mask, 0, sizeof(struct rte_flow_item_udp));
	udp_spec.hdr.dst_port = dst_port;
	udp_spec.hdr.dgram_len = 0;
	udp_spec.hdr.dgram_cksum = 0;

	udp_mask.hdr.dst_port = dst_port_mask;
	udp_mask.hdr.dgram_len = 0;
	udp_mask.hdr.dgram_cksum = 0;

	pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
    pattern[2].spec = &udp_spec;
    pattern[2].mask = &udp_mask;

    pattern[3].type = RTE_FLOW_ITEM_TYPE_END;

	res = rte_flow_validate(port_id, &attr, pattern, action, NULL);//last argument is for errors (struct rte_flow_error *)
    if (!res)
        flow = rte_flow_create(port_id, &attr, pattern, action, NULL);

    return flow;

}

void dpdk_init(int *argc, char ***argv)
{
	int ret;
	unsigned int i;
	uint8_t port_id = 0;
	uint16_t nb_rx_q;
	uint16_t nb_tx_q;
	uint16_t nb_tx_desc = ETH_DEV_TX_QUEUE_SZ; // 4096
	uint16_t nb_rx_desc = ETH_DEV_RX_QUEUE_SZ; // 512
	struct rte_eth_link link;

	// port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS; 
	port_conf.rxmode.mq_mode = ETH_MQ_RX_NONE; 
	port_conf.rxmode.split_hdr_size = 0;

	port_conf.txmode.mq_mode = ETH_MQ_TX_NONE;

	port_conf.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_NONFRAG_IPV4_UDP;

	/* init EAL */
	ret = rte_eal_init(*argc, *argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	*argc -= ret;
	*argv += ret;

	nb_rx_q = rte_lcore_count();
	nb_tx_q = rte_lcore_count();

	/* create the mbuf pool */
	pktmbuf_pool =
		rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF, MEMPOOL_CACHE_SIZE, 0,
				RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	printf("I found %" PRIu8 " ports\n", nb_ports);


	/* Port config */
	printf("Configuring port...\n");
	ret = rte_eth_dev_configure(port_id, nb_rx_q, nb_tx_q, &port_conf);

	if (ret < 0) {
		rte_exit(EXIT_FAILURE, "rte_eth_dev_configure:err=%d, port=%u\n",
				ret, (unsigned)port_id);
	}

	/* enable multicast */
	rte_eth_allmulticast_enable(port_id);

	/* initialize one queue per cpu */
	for (i = 0; i < rte_lcore_count(); i++) {
		printf("setting up TX and RX queues...\n");
		ret = rte_eth_tx_queue_setup(port_id, i, nb_tx_desc,
				rte_eth_dev_socket_id(port_id), NULL);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE,
					"rte_eth_tx_queue_setup:err=%d, port=%u\n", ret,
					(unsigned)port_id);
		}

		ret = rte_eth_rx_queue_setup(port_id, i, nb_rx_desc,
				rte_eth_dev_socket_id(port_id), NULL,
				pktmbuf_pool);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE,
					"rte_eth_rx_queue_setup:err=%d, port=%u\n", ret,
					(unsigned)port_id);
		}
	}

	// /* RSS RETA setup */
	// int status = rss_reta_setup(port_id, rte_lcore_count());
	// if (status != 0)
	// 	cout << "RSS setup failed! " << endl;

	/* initialize shared ring  */
	input_stream_ring = rte_ring_create("input_stream_ring", rte_align32pow2(NB_MBUF), rte_socket_id(),0);
	output_stream_ring = rte_ring_create("output_stream_ring", rte_align32pow2(NB_MBUF), rte_socket_id(),0);
	for (int i =0 ; i < NUM_PRIORITIES ; i++)
		task_list_ring[i] = rte_ring_create("task_list_ring", rte_align32pow2(NB_MBUF), rte_socket_id(),0);//TODO: support multiple priority rings(have different names)

	/* start the device */
	ret = rte_eth_dev_start(port_id);
	if (ret < 0) {
		printf("ERROR starting device at port %d\n", port_id);
	} else {
		printf("started device at port %d\n", port_id);
	}

	/* check the link */
	rte_eth_link_get(port_id, &link);

	if (!link.link_status) {
		printf("eth:\tlink appears to be down, check connection.\n");
	} else {
		printf("eth:\tlink up - speed %u Mbps, %s\n",
				(uint32_t)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX)
				? ("full-duplex")
				: ("half-duplex\n"));
	}


}

struct net_sge *alloc_net_sge(void)
{
	struct net_sge *e;
	struct rte_mbuf *pkt_buf = rte_pktmbuf_alloc(pktmbuf_pool);
	assert(pkt_buf != NULL);
	pkt_buf->userdata = NULL;
	e = rte_pktmbuf_mtod(pkt_buf, struct net_sge *);
	e->len = 0;

	e->payload = rte_pktmbuf_mtod_offset(pkt_buf, void *, UDP_HDRS_LEN);
	e->handle = pkt_buf;
	return e;
}

static inline void get_local_mac(struct ether_addr *mac)
{
	rte_eth_macaddr_get(0, mac); // Assume only one NIC
}

static inline void ip_addr_to_str(uint32_t addr, char *str)
{
	snprintf(str, 15, "%d.%d.%d.%d", ((addr >> 24) & 0xff),
			 ((addr >> 16) & 0xff), ((addr >> 8) & 0xff), (addr & 0xff));
}

static inline int str_to_eth_addr(const char *src, unsigned char *dst)
{
	struct ether_addr tmp;

	if (sscanf(src, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &tmp.addr_bytes[0],
			   &tmp.addr_bytes[1], &tmp.addr_bytes[2], &tmp.addr_bytes[3],
			   &tmp.addr_bytes[4], &tmp.addr_bytes[5]) != 6)
		return -EINVAL;
	// memcpy(dst, &tmp, sizeof(tmp));
	rte_memcpy(dst, &tmp, sizeof(tmp));
	return 0;
}

int dpdk_eth_send(struct rte_mbuf *pkt_buf, uint16_t len)
{
	int ret = 0;

	/* get mbuf from user data */
	pkt_buf->pkt_len = len; 
	pkt_buf->data_len = len;

	// RetrieveTaskPacket *temp = rte_pktmbuf_mtod_offset(pkt_buf, RetrieveTaskPacket *, UDP_HDRS_LEN);
	// cout << "Sending Packet in dpdk_eth_send: " << temp->toString() << endl;

	while (1) {
		ret = rte_eth_tx_burst(0, RTE_PER_LCORE(queue_id), &pkt_buf, 1);
		if (ret == 1)
			break;
	}

	return 1;
}

static RetrieveTaskPacket udp_recv(struct net_sge *entry, struct ip_tuple *id)
{
	struct rte_mbuf *pkt_buf;
	RetrieveTaskPacket *buf;
	buf = (RetrieveTaskPacket *)entry->payload;

	pkt_buf = (struct rte_mbuf *)entry->handle;
	pkt_buf->userdata = NULL;

    // cout << "Recieved Packet in udp_recv: " << buf->toString() << endl;

	return *buf; 
	// handle_incoming_pck((generic_buffer)entry, entry->len, &source,	&local_host);// This should call packet processing
}

// net_sge * udp_in(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph, struct udp_hdr *udph)
net_sge * udp_in(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph, struct udp_hdr *udph, struct ip_tuple * id)
{
	// struct ip_tuple *id;
	struct net_sge *e;

//	id = rte_pktmbuf_mtod(pkt_buf, struct ip_tuple *);

//	puts("AYAYA");
//	cout << *((uint32_t*) ((char*) pkt_buf + sizeof(ip_tuple))) << "\n";
//	puts("AYAYA2");

	id->src_ip = rte_be_to_cpu_32(iph->src_addr);
	id->dst_ip = rte_be_to_cpu_32(iph->dst_addr);
	id->src_port = rte_be_to_cpu_16(udph->src_port);
	id->dst_port = rte_be_to_cpu_16(udph->dst_port);

	e = rte_pktmbuf_mtod_offset(pkt_buf, struct net_sge *,
								sizeof(struct ip_tuple));
	e->len = rte_be_to_cpu_16(udph->dgram_len) - sizeof(struct udp_hdr);
	e->payload = (void *)((unsigned char *)udph + sizeof(struct udp_hdr));
	e->handle = pkt_buf;

    // return udp_recv(e, id);// return Entry here, dont call udp recv
	return e;
}

// net_sge * ip_in(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph)
net_sge * ip_in(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph, struct ip_tuple * id)
{
	struct icmp_hdr *icmph;
	struct udp_hdr *udph;
	struct igmpv2_hdr *igmph;
	int hdrlen;

	/* perform necessary checks */
	hdrlen = (iph->version_ihl & IPV4_HDR_IHL_MASK) * IPV4_IHL_MULTIPLIER;

	switch (iph->next_proto_id) {
	case IPPROTO_TCP:
		printf("TCP not supported\n");
		break;
	case IPPROTO_UDP:
		udph = (struct udp_hdr *)((unsigned char *)iph + hdrlen);
		// return udp_in(pkt_buf, iph, udph);
		return udp_in(pkt_buf, iph, udph, id);
		break;
	case IPPROTO_ICMP:
		// icmph = (struct icmp_hdr *)((unsigned char *)iph + hdrlen);
		// icmp_in(pkt_buf, iph, icmph);
		printf("ICMP not supported\n");
		break;
	case IPPROTO_IGMP:
		// igmph = (struct igmpv2_hdr *)((unsigned char *)iph + hdrlen);
		// igmp_in(pkt_buf, iph, igmph);
		printf("IGMP not supported\n");
        break;
	default:
		goto out;
	}

	//return sth if it's not a udp packet

out:
	printf("UNKNOWN L3 PROTOCOL OR WRONG DST IP\n");
	rte_pktmbuf_free(pkt_buf);
}

// net_sge * eth_in(struct rte_mbuf *pkt_buf)
net_sge * eth_in(struct rte_mbuf *pkt_buf, struct ip_tuple * id)
{
	unsigned char *payload = rte_pktmbuf_mtod(pkt_buf, unsigned char *);
	struct ether_hdr *hdr = (struct ether_hdr *)payload;
	// struct arp_hdr *arph;
	struct ipv4_hdr *iph;

	if (hdr->ether_type == rte_cpu_to_be_16(ETHER_TYPE_ARP)) {
		// arph = (struct arp_hdr *)(payload + (sizeof(struct ether_hdr)));
		// arp_in(pkt_buf, arph);
		printf("Arp ether type: \n");
		rte_pktmbuf_free(pkt_buf);
	} else if (hdr->ether_type == rte_be_to_cpu_16(ETHER_TYPE_IPv4)) {
		iph = (struct ipv4_hdr *)(payload + (sizeof(struct ether_hdr)));
		// return ip_in(pkt_buf, iph);
		return ip_in(pkt_buf, iph, id);
	} else {
		printf("Unknown ether type: %" PRIu16 "\n",
			   rte_be_to_cpu_16(hdr->ether_type));
		rte_pktmbuf_free(pkt_buf);
	}
}

int eth_out(struct rte_mbuf *pkt_buf, uint16_t h_proto, struct ether_addr *dst_haddr, uint16_t iplen)
{
	/* fill the ethernet header */
	struct ether_hdr *hdr = rte_pktmbuf_mtod(pkt_buf, struct ether_hdr *);

	hdr->d_addr = *dst_haddr;

	// cout << "dst_haddr : " ;
	// for (int i =0 ;  i < sizeof(dst_haddr) ; i ++ )
	// 	printf("%hhu ", dst_haddr->addr_bytes[i]) ;
	// cout << endl;

	// cout << "hdr->d_addr  : " ;
	// for (int i =0 ;  i < sizeof(hdr->d_addr) ; i ++ )
	// 	printf("%hhu ", hdr->d_addr.addr_bytes[i]) ;
	// cout << endl;

	get_local_mac(&hdr->s_addr);
	hdr->ether_type = rte_cpu_to_be_16(h_proto);

	/* enqueue the packet */
	return dpdk_eth_send(pkt_buf, iplen + sizeof(struct ether_hdr));
}

void ip_out(struct rte_mbuf *pkt_buf, struct ipv4_hdr *iph, uint32_t src_ip, uint32_t dst_ip, uint8_t ttl, uint8_t tos, uint8_t proto, uint16_t l4len, struct ether_addr *dst_haddr)
{
	int sent, hdrlen;
	char *options;

	hdrlen = sizeof(struct ipv4_hdr);
	if (proto == IPPROTO_IGMP)
		hdrlen += 4; // 4 bytes for options

	/* setup ip hdr */
	iph->version_ihl =
		(4 << 4) | (hdrlen / IPV4_IHL_MULTIPLIER);
	iph->type_of_service = tos;
	iph->total_length = rte_cpu_to_be_16(hdrlen + l4len);
	iph->packet_id = 0;
	iph->fragment_offset = rte_cpu_to_be_16(0x4000); // Don't fragment
	iph->time_to_live = ttl;
	iph->next_proto_id = proto;
	iph->hdr_checksum = 0;
	iph->src_addr = rte_cpu_to_be_32(src_ip);
	iph->dst_addr = rte_cpu_to_be_32(dst_ip);

	// if (!dst_haddr)
		// dst_haddr = arp_lookup_mac(dst_ip); 
	
	char tmp[64];
	if (!dst_haddr) {
		ip_addr_to_str(dst_ip, tmp);
		printf("Unknown mac for %s\n", tmp);
	}
	assert(dst_haddr != NULL);

	/* Add options if IGMP */
	if (proto == IPPROTO_IGMP) {
		options = (char *)(iph+1);
		*options = 0x94;
		options++;
		*options = 0x4;
		options++;
		*options = 0x0;
		options++;
		*options = 0x0;
	}

	///* compute checksum */
	iph->hdr_checksum = rte_raw_cksum(iph, hdrlen);
	iph->hdr_checksum = (iph->hdr_checksum == 0xffff) ? iph->hdr_checksum : (uint16_t)~(iph->hdr_checksum);

	if (proto == IPPROTO_TCP) {
		assert(0);
	}

	sent = eth_out(pkt_buf, ETHER_TYPE_IPv4, dst_haddr,
				   rte_be_to_cpu_16(iph->total_length));
	assert(sent == 1);
}

int udp_out(struct rte_mbuf *pkt_buf, struct ip_tuple *id, int len)
{
	struct ipv4_hdr *iph = rte_pktmbuf_mtod_offset(pkt_buf, struct ipv4_hdr *,
												   sizeof(struct ether_hdr));
	struct udp_hdr *udph = rte_pktmbuf_mtod_offset(pkt_buf, struct udp_hdr *,
												   sizeof(struct ether_hdr) +
													   sizeof(struct ipv4_hdr));

	udph->dgram_cksum = 0;
	udph->dgram_len = rte_cpu_to_be_16(len + sizeof(struct udp_hdr));
	udph->src_port = rte_cpu_to_be_16(id->src_port);
	udph->dst_port = rte_cpu_to_be_16(id->dst_port);

	struct ether_addr dst_haddr;
	str_to_eth_addr("ff:ff:ff:ff:ff:ff",(unsigned char *)&dst_haddr);

	ip_out(pkt_buf, iph, id->src_ip, id->dst_ip, 64, 0, IPPROTO_UDP,
		   len + sizeof(struct udp_hdr), &dst_haddr);
	return 0;
}

static inline int udp_send(struct net_sge *entry, struct ip_tuple *id)
{
	if (entry->len > UDP_MAX_LEN)
		return -1;
	return udp_out((rte_mbuf*)entry->handle, id, entry->len);
}

// inline RetrieveTaskPacket dpdk_net_poll(void)
// inline net_sge ** dpdk_net_poll(void) // or we can just call rte_eth_rx_burst and eth_in so we don't deal with the arrays here
// {
// 	int ret, i;
// 	struct rte_mbuf *rx_pkts[BATCH_SIZE];

// 		ret = rte_eth_rx_burst(0, RTE_PER_LCORE(queue_id), rx_pkts, BATCH_SIZE);
        
// 		//cout << "rte burst ret : " << ret << endl;
// 		// start = get_time_now();
// 		for (i = 0; i < ret; i++)
// 			return eth_in(rx_pkts[i]);
// 		// end = get_time_now();
// }
