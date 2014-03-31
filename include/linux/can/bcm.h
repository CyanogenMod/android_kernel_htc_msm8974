/*
 * linux/can/bcm.h
 *
 * Definitions for CAN Broadcast Manager (BCM)
 *
 * Author: Oliver Hartkopp <oliver.hartkopp@volkswagen.de>
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 */

#ifndef CAN_BCM_H
#define CAN_BCM_H

#include <linux/types.h>
#include <linux/can.h>

struct bcm_msg_head {
	__u32 opcode;
	__u32 flags;
	__u32 count;
	struct timeval ival1, ival2;
	canid_t can_id;
	__u32 nframes;
	struct can_frame frames[0];
};

enum {
	TX_SETUP = 1,	
	TX_DELETE,	
	TX_READ,	
	TX_SEND,	
	RX_SETUP,	
	RX_DELETE,	
	RX_READ,	
	TX_STATUS,	
	TX_EXPIRED,	
	RX_STATUS,	
	RX_TIMEOUT,	
	RX_CHANGED	
};

#define SETTIMER            0x0001
#define STARTTIMER          0x0002
#define TX_COUNTEVT         0x0004
#define TX_ANNOUNCE         0x0008
#define TX_CP_CAN_ID        0x0010
#define RX_FILTER_ID        0x0020
#define RX_CHECK_DLC        0x0040
#define RX_NO_AUTOTIMER     0x0080
#define RX_ANNOUNCE_RESUME  0x0100
#define TX_RESET_MULTI_IDX  0x0200
#define RX_RTR_FRAME        0x0400

#endif 
