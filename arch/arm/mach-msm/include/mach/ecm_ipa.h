/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ECM_IPA_H_
#define _ECM_IPA_H_

#include <mach/ipa.h>

typedef void (*ecm_ipa_callback)(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data);

struct ecm_ipa_params {
	ecm_ipa_callback ecm_ipa_rx_dp_notify;
	ecm_ipa_callback ecm_ipa_tx_dp_notify;
	u8 host_ethaddr[ETH_ALEN];
	u8 device_ethaddr[ETH_ALEN];
	void *private;
};


#ifdef CONFIG_ECM_IPA

int ecm_ipa_init(struct ecm_ipa_params *params);

int ecm_ipa_connect(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl,
		void *priv);

int ecm_ipa_disconnect(void *priv);

void ecm_ipa_cleanup(void *priv);

#else 

int ecm_ipa_init(struct ecm_ipa_params *params)
{
	return 0;
}

static inline int ecm_ipa_connect(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl,
		void *priv)
{
	return 0;
}

static inline int ecm_ipa_disconnect(void *priv)
{
	return 0;
}

static inline void ecm_ipa_cleanup(void *priv)
{

}
#endif 

#endif 
