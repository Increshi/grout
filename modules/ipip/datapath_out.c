// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 Robin Jarry

#include "ipip_priv.h"

#include <br_datapath.h>
#include <br_eth_output.h>
#include <br_graph.h>
#include <br_ip4_control.h>
#include <br_ip4_datapath.h>
#include <br_ipip.h>
#include <br_log.h>
#include <br_mbuf.h>

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>

#include <netinet/in.h>

enum {
	IP_OUTPUT = 0,
	NO_TUNNEL,
	EDGE_COUNT,
};

static uint16_t
ipip_output_process(struct rte_graph *graph, struct rte_node *node, void **objs, uint16_t nb_objs) {
	struct ip_output_mbuf_data *ip_data;
	const struct iface_info_ipip *ipip;
	struct ip_local_mbuf_data tunnel;
	const struct rte_ipv4_hdr *inner;
	struct rte_ipv4_hdr *outer;
	const struct iface *iface;
	struct rte_mbuf *mbuf;
	rte_edge_t next;

	for (uint16_t i = 0; i < nb_objs; i++) {
		mbuf = objs[i];

		// Resolve the IPIP interface from the nexthop provided by ip_output.
		ip_data = ip_output_mbuf_data(mbuf);
		iface = iface_from_id(ip_data->nh->iface_id);
		if (iface == NULL || iface->type_id != BR_IFACE_TYPE_IPIP) {
			next = NO_TUNNEL;
			goto next;
		}
		ipip = (const struct iface_info_ipip *)iface->info;

		// Encapsulate with another IPv4 header.
		inner = rte_pktmbuf_mtod(mbuf, const struct rte_ipv4_hdr *);
		tunnel.src = ipip->local;
		tunnel.dst = ipip->remote;
		tunnel.len = rte_be_to_cpu_16(inner->total_length);
		tunnel.vrf_id = iface->vrf_id;
		tunnel.proto = IPPROTO_IPIP;
		outer = (struct rte_ipv4_hdr *)rte_pktmbuf_prepend(mbuf, sizeof(*outer));
		ip_set_fields(outer, &tunnel);

		// Resolve nexthop for the encapsulated packet.
		ip_data->nh = ip4_route_lookup(iface->vrf_id, ipip->remote);
		next = IP_OUTPUT;
next:
		rte_node_enqueue_x1(graph, node, next, mbuf);
	}

	return nb_objs;
}

static void ipip_output_register(void) {
	rte_edge_t edge = br_node_attach_parent("ip_output", "ipip_output");
	if (edge == RTE_EDGE_ID_INVALID)
		ABORT("br_node_attach_parent(ip_output, ipip_output) failed");
	ip_output_add_tunnel(BR_IFACE_TYPE_IPIP, edge);
}

static struct rte_node_register ipip_output_node = {
	.name = "ipip_output",

	.process = ipip_output_process,

	.nb_edges = EDGE_COUNT,
	.next_nodes = {
		[IP_OUTPUT] = "ip_output",
		[NO_TUNNEL] = "ipip_output_no_tunnel",
	},
};

static struct br_node_info ipip_output_info = {
	.node = &ipip_output_node,
	.register_callback = ipip_output_register,
};

BR_NODE_REGISTER(ipip_output_info);

BR_DROP_REGISTER(ipip_output_no_tunnel);
