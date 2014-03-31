/*
 * linux/can/raw.h
 *
 * Definitions for raw CAN sockets
 *
 * Authors: Oliver Hartkopp <oliver.hartkopp@volkswagen.de>
 *          Urs Thuermann   <urs.thuermann@volkswagen.de>
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 */

#ifndef CAN_RAW_H
#define CAN_RAW_H

#include <linux/can.h>

#define SOL_CAN_RAW (SOL_CAN_BASE + CAN_RAW)


enum {
	CAN_RAW_FILTER = 1,	
	CAN_RAW_ERR_FILTER,	
	CAN_RAW_LOOPBACK,	
	CAN_RAW_RECV_OWN_MSGS	
};

#endif
