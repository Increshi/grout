// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2023 Robin Jarry

#ifndef _BR_INFRA_TYPES
#define _BR_INFRA_TYPES

#include <sched.h>
#include <stdint.h>

struct br_ether_addr {
	uint8_t bytes[6];
};

struct br_infra_port {
	uint16_t index;
	char name[64];
	char device[128];
	uint16_t mtu;
	struct br_ether_addr mac;
	uint16_t n_rxq;
	uint16_t n_txq;
};

#endif
