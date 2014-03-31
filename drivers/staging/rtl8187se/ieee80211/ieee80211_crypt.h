/*
 * Original code based on Host AP (software wireless LAN access point) driver
 * for Intersil Prism2/2.5/3.
 *
 * Copyright (c) 2001-2002, SSH Communications Security Corp and Jouni Malinen
 * <jkmaline@cc.hut.fi>
 * Copyright (c) 2002-2003, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * Adaption to a generic IEEE 802.11 stack by James Ketrenos
 * <jketreno@linux.intel.com>
 *
 * Copyright (c) 2004, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

#ifndef IEEE80211_CRYPT_H
#define IEEE80211_CRYPT_H

#include <linux/skbuff.h>

struct ieee80211_crypto_ops {
	const char *name;

	void * (*init)(int keyidx);

	
	void (*deinit)(void *priv);

	int (*encrypt_mpdu)(struct sk_buff *skb, int hdr_len, void *priv);
	int (*decrypt_mpdu)(struct sk_buff *skb, int hdr_len, void *priv);

	int (*encrypt_msdu)(struct sk_buff *skb, int hdr_len, void *priv);
	int (*decrypt_msdu)(struct sk_buff *skb, int keyidx, int hdr_len,
			    void *priv);

	int (*set_key)(void *key, int len, u8 *seq, void *priv);
	int (*get_key)(void *key, int len, u8 *seq, void *priv);

	char * (*print_stats)(char *p, void *priv);

	int extra_prefix_len, extra_postfix_len;

	struct module *owner;
};

struct ieee80211_crypt_data {
	struct list_head list; 
	struct ieee80211_crypto_ops *ops;
	void *priv;
	atomic_t refcnt;
};

int ieee80211_register_crypto_ops(struct ieee80211_crypto_ops *ops);
int ieee80211_unregister_crypto_ops(struct ieee80211_crypto_ops *ops);
struct ieee80211_crypto_ops * ieee80211_get_crypto_ops(const char *name);
void ieee80211_crypt_deinit_entries(struct ieee80211_device *, int);
void ieee80211_crypt_deinit_handler(unsigned long);
void ieee80211_crypt_delayed_deinit(struct ieee80211_device *ieee,
				    struct ieee80211_crypt_data **crypt);

#endif
