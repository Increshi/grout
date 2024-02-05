// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 Robin Jarry

#include <br_datapath.h>
#include <br_graph.h>
#include <br_log.h>
#include <br_tx.h>
#include <br_worker.h>

#include <rte_build_config.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_malloc.h>

#define DROP 0

struct tx_ctx {
	uint16_t *txq_ids; // RTE_MAX_ETHPORTS
};

static uint16_t
tx_process(struct rte_graph *graph, struct rte_node *node, void **objs, uint16_t nb_objs) {
	NODE_CTX_PTR(const struct tx_ctx *, ctx, node);
	uint16_t c, count = 0;

	for (uint16_t i = 0; i < nb_objs; i++) {
		struct rte_mbuf *mbuf = objs[i];
		struct tx_mbuf_priv *priv;
		uint16_t port_id, txq_id;

		if ((priv = tx_mbuf_priv(mbuf)) == NULL)
			goto drop;

		port_id = priv->port_id;
		txq_id = ctx->txq_ids[port_id];

		c = rte_eth_tx_burst(port_id, txq_id, &mbuf, 1);
		count += c;
		if (c == 1)
			continue;
drop:
		rte_node_enqueue(graph, node, DROP, (void **)&mbuf, 1);
	}

	return count;
}

static const struct rte_mbuf_dynfield tx_mbuf_priv_desc = {
	.name = "tx",
	.size = sizeof(struct tx_mbuf_priv),
	.align = __alignof__(struct tx_mbuf_priv),
};

int tx_mbuf_priv_offset = -1;

static int tx_init(const struct rte_graph *graph, struct rte_node *node) {
	NODE_CTX_PTR(struct tx_ctx *, ctx, node);
	const struct tx_node_queues *data;
	static bool once;

	(void)graph;

	if (!once) {
		once = true;
		tx_mbuf_priv_offset = rte_mbuf_dynfield_register(&tx_mbuf_priv_desc);
	}
	if (tx_mbuf_priv_offset < 0) {
		LOG(ERR, "rte_mbuf_dynfield_register(): %s", rte_strerror(rte_errno));
		return -1;
	}

	if (br_node_data_get(graph->name, node->name, (void **)&data) < 0)
		return -1;

	ctx->txq_ids = rte_malloc(__func__, sizeof(data->txq_ids), RTE_CACHE_LINE_SIZE);
	if (ctx->txq_ids == NULL) {
		LOG(ERR, "rte_malloc(): %s", rte_strerror(rte_errno));
		return -1;
	}
	memcpy(ctx->txq_ids, data->txq_ids, sizeof(data->txq_ids));

	return 0;
}

static void tx_fini(const struct rte_graph *graph, struct rte_node *node) {
	NODE_CTX_PTR(struct tx_ctx *, ctx, node);
	(void)graph;
	rte_free(ctx->txq_ids);
	ctx->txq_ids = NULL;
}

static struct rte_node_register tx_node_base = {
	.name = "tx",

	.process = tx_process,
	.init = tx_init,
	.fini = tx_fini,

	.nb_edges = 1,
	.next_nodes = {
		[DROP] = "drop",
	},
};

static struct br_node_info info = {
	.node = &tx_node_base,
};

BR_NODE_REGISTER(info)
