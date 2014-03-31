/*
 *
 * envctrl.h: Definitions for access to the i2c environment
 *            monitoring on Ultrasparc systems.
 *
 * Copyright (C) 1998  Eddie C. Dost  (ecd@skynet.be)
 * Copyright (C) 2000  Vinh Truong  (vinh.truong@eng.sun.com)
 * VT - Add all ioctl commands and environment status definitions
 * VT - Add application note
 */
#ifndef _SPARC64_ENVCTRL_H
#define _SPARC64_ENVCTRL_H 1

#include <linux/ioctl.h>



#define ENVCTRL_RD_CPU_TEMPERATURE	_IOR('p', 0x40, int)
#define ENVCTRL_RD_CPU_VOLTAGE		_IOR('p', 0x41, int)
#define ENVCTRL_RD_FAN_STATUS		_IOR('p', 0x42, int)
#define ENVCTRL_RD_WARNING_TEMPERATURE	_IOR('p', 0x43, int)
#define ENVCTRL_RD_SHUTDOWN_TEMPERATURE	_IOR('p', 0x44, int)
#define ENVCTRL_RD_VOLTAGE_STATUS	_IOR('p', 0x45, int)
#define ENVCTRL_RD_SCSI_TEMPERATURE	_IOR('p', 0x46, int)
#define ENVCTRL_RD_ETHERNET_TEMPERATURE	_IOR('p', 0x47, int)
#define ENVCTRL_RD_MTHRBD_TEMPERATURE	_IOR('p', 0x48, int)

#define ENVCTRL_RD_GLOBALADDRESS	_IOR('p', 0x49, int)

#define ENVCTRL_VOLTAGE_POWERSUPPLY_GOOD	0x01
#define ENVCTRL_VOLTAGE_BAD			0x02
#define ENVCTRL_POWERSUPPLY_BAD			0x03
#define ENVCTRL_VOLTAGE_POWERSUPPLY_BAD		0x04


#define ENVCTRL_ALL_FANS_GOOD			0x00
#define ENVCTRL_FAN0_FAILURE_MASK		0x01
#define ENVCTRL_FAN1_FAILURE_MASK		0x02
#define ENVCTRL_FAN2_FAILURE_MASK		0x04
#define ENVCTRL_FAN3_FAILURE_MASK		0x08
#define ENVCTRL_FAN4_FAILURE_MASK		0x10
#define ENVCTRL_FAN5_FAILURE_MASK		0x20
#define ENVCTRL_FAN6_FAILURE_MASK		0x40
#define ENVCTRL_FAN7_FAILURE_MASK		0x80
#define ENVCTRL_ALL_FANS_BAD 			0xFF

#endif 
