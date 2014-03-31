/*
 * mac80211_hwsim - software simulator of 802.11 radio(s) for mac80211
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2011, Javier Lopez <jlopex@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAC80211_HWSIM_H
#define __MAC80211_HWSIM_H

enum hwsim_tx_control_flags {
	HWSIM_TX_CTL_REQ_TX_STATUS		= BIT(0),
	HWSIM_TX_CTL_NO_ACK			= BIT(1),
	HWSIM_TX_STAT_ACK			= BIT(2),
};


enum {
	HWSIM_CMD_UNSPEC,
	HWSIM_CMD_REGISTER,
	HWSIM_CMD_FRAME,
	HWSIM_CMD_TX_INFO_FRAME,
	__HWSIM_CMD_MAX,
};
#define HWSIM_CMD_MAX (_HWSIM_CMD_MAX - 1)



enum {
	HWSIM_ATTR_UNSPEC,
	HWSIM_ATTR_ADDR_RECEIVER,
	HWSIM_ATTR_ADDR_TRANSMITTER,
	HWSIM_ATTR_FRAME,
	HWSIM_ATTR_FLAGS,
	HWSIM_ATTR_RX_RATE,
	HWSIM_ATTR_SIGNAL,
	HWSIM_ATTR_TX_INFO,
	HWSIM_ATTR_COOKIE,
	__HWSIM_ATTR_MAX,
};
#define HWSIM_ATTR_MAX (__HWSIM_ATTR_MAX - 1)

struct hwsim_tx_rate {
	s8 idx;
	u8 count;
} __packed;

#endif 
