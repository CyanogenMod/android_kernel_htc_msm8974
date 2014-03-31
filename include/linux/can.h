/*
 * linux/can.h
 *
 * Definitions for CAN network layer (socket addr / CAN frame / CAN filter)
 *
 * Authors: Oliver Hartkopp <oliver.hartkopp@volkswagen.de>
 *          Urs Thuermann   <urs.thuermann@volkswagen.de>
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 */

#ifndef CAN_H
#define CAN_H

#include <linux/types.h>
#include <linux/socket.h>


#define CAN_EFF_FLAG 0x80000000U 
#define CAN_RTR_FLAG 0x40000000U 
#define CAN_ERR_FLAG 0x20000000U 

#define CAN_SFF_MASK 0x000007FFU 
#define CAN_EFF_MASK 0x1FFFFFFFU 
#define CAN_ERR_MASK 0x1FFFFFFFU 

typedef __u32 canid_t;

typedef __u32 can_err_mask_t;

struct can_frame {
	canid_t can_id;  
	__u8    can_dlc; 
	__u8    data[8] __attribute__((aligned(8)));
};

#define CAN_RAW		1 
#define CAN_BCM		2 
#define CAN_TP16	3 
#define CAN_TP20	4 
#define CAN_MCNET	5 
#define CAN_ISOTP	6 
#define CAN_NPROTO	7

#define SOL_CAN_BASE 100

struct sockaddr_can {
	__kernel_sa_family_t can_family;
	int         can_ifindex;
	union {
		
		struct { canid_t rx_id, tx_id; } tp;

		
	} can_addr;
};

struct can_filter {
	canid_t can_id;
	canid_t can_mask;
};

#define CAN_INV_FILTER 0x20000000U 

#endif 
