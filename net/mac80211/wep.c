/*
 * Software WEP encryption implementation
 * Copyright 2002, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright 2003, Instant802 Networks, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/random.h>
#include <linux/compiler.h>
#include <linux/crc32.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include <net/mac80211.h>
#include "ieee80211_i.h"
#include "wep.h"


int ieee80211_wep_init(struct ieee80211_local *local)
{
	
	get_random_bytes(&local->wep_iv, WEP_IV_LEN);

	local->wep_tx_tfm = crypto_alloc_cipher("arc4", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(local->wep_tx_tfm)) {
		local->wep_rx_tfm = ERR_PTR(-EINVAL);
		return PTR_ERR(local->wep_tx_tfm);
	}

	local->wep_rx_tfm = crypto_alloc_cipher("arc4", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(local->wep_rx_tfm)) {
		crypto_free_cipher(local->wep_tx_tfm);
		local->wep_tx_tfm = ERR_PTR(-EINVAL);
		return PTR_ERR(local->wep_rx_tfm);
	}

	return 0;
}

void ieee80211_wep_free(struct ieee80211_local *local)
{
	if (!IS_ERR(local->wep_tx_tfm))
		crypto_free_cipher(local->wep_tx_tfm);
	if (!IS_ERR(local->wep_rx_tfm))
		crypto_free_cipher(local->wep_rx_tfm);
}

static inline bool ieee80211_wep_weak_iv(u32 iv, int keylen)
{
	if ((iv & 0xff00) == 0xff00) {
		u8 B = (iv >> 16) & 0xff;
		if (B >= 3 && B < 3 + keylen)
			return true;
	}
	return false;
}


static void ieee80211_wep_get_iv(struct ieee80211_local *local,
				 int keylen, int keyidx, u8 *iv)
{
	local->wep_iv++;
	if (ieee80211_wep_weak_iv(local->wep_iv, keylen))
		local->wep_iv += 0x0100;

	if (!iv)
		return;

	*iv++ = (local->wep_iv >> 16) & 0xff;
	*iv++ = (local->wep_iv >> 8) & 0xff;
	*iv++ = local->wep_iv & 0xff;
	*iv++ = keyidx << 6;
}


static u8 *ieee80211_wep_add_iv(struct ieee80211_local *local,
				struct sk_buff *skb,
				int keylen, int keyidx)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	unsigned int hdrlen;
	u8 *newhdr;

	hdr->frame_control |= cpu_to_le16(IEEE80211_FCTL_PROTECTED);

	if (WARN_ON(skb_tailroom(skb) < WEP_ICV_LEN ||
		    skb_headroom(skb) < WEP_IV_LEN))
		return NULL;

	hdrlen = ieee80211_hdrlen(hdr->frame_control);
	newhdr = skb_push(skb, WEP_IV_LEN);
	memmove(newhdr, newhdr + WEP_IV_LEN, hdrlen);
	ieee80211_wep_get_iv(local, keylen, keyidx, newhdr + hdrlen);
	return newhdr + hdrlen;
}


static void ieee80211_wep_remove_iv(struct ieee80211_local *local,
				    struct sk_buff *skb,
				    struct ieee80211_key *key)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	unsigned int hdrlen;

	hdrlen = ieee80211_hdrlen(hdr->frame_control);
	memmove(skb->data + WEP_IV_LEN, skb->data, hdrlen);
	skb_pull(skb, WEP_IV_LEN);
}


int ieee80211_wep_encrypt_data(struct crypto_cipher *tfm, u8 *rc4key,
			       size_t klen, u8 *data, size_t data_len)
{
	__le32 icv;
	int i;

	if (IS_ERR(tfm))
		return -1;

	icv = cpu_to_le32(~crc32_le(~0, data, data_len));
	put_unaligned(icv, (__le32 *)(data + data_len));

	crypto_cipher_setkey(tfm, rc4key, klen);
	for (i = 0; i < data_len + WEP_ICV_LEN; i++)
		crypto_cipher_encrypt_one(tfm, data + i, data + i);

	return 0;
}


int ieee80211_wep_encrypt(struct ieee80211_local *local,
			  struct sk_buff *skb,
			  const u8 *key, int keylen, int keyidx)
{
	u8 *iv;
	size_t len;
	u8 rc4key[3 + WLAN_KEY_LEN_WEP104];

	iv = ieee80211_wep_add_iv(local, skb, keylen, keyidx);
	if (!iv)
		return -1;

	len = skb->len - (iv + WEP_IV_LEN - skb->data);

	
	memcpy(rc4key, iv, 3);

	
	memcpy(rc4key + 3, key, keylen);

	
	skb_put(skb, WEP_ICV_LEN);

	return ieee80211_wep_encrypt_data(local->wep_tx_tfm, rc4key, keylen + 3,
					  iv + WEP_IV_LEN, len);
}


int ieee80211_wep_decrypt_data(struct crypto_cipher *tfm, u8 *rc4key,
			       size_t klen, u8 *data, size_t data_len)
{
	__le32 crc;
	int i;

	if (IS_ERR(tfm))
		return -1;

	crypto_cipher_setkey(tfm, rc4key, klen);
	for (i = 0; i < data_len + WEP_ICV_LEN; i++)
		crypto_cipher_decrypt_one(tfm, data + i, data + i);

	crc = cpu_to_le32(~crc32_le(~0, data, data_len));
	if (memcmp(&crc, data + data_len, WEP_ICV_LEN) != 0)
		
		return -1;

	return 0;
}


static int ieee80211_wep_decrypt(struct ieee80211_local *local,
				 struct sk_buff *skb,
				 struct ieee80211_key *key)
{
	u32 klen;
	u8 rc4key[3 + WLAN_KEY_LEN_WEP104];
	u8 keyidx;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	unsigned int hdrlen;
	size_t len;
	int ret = 0;

	if (!ieee80211_has_protected(hdr->frame_control))
		return -1;

	hdrlen = ieee80211_hdrlen(hdr->frame_control);
	if (skb->len < hdrlen + WEP_IV_LEN + WEP_ICV_LEN)
		return -1;

	len = skb->len - hdrlen - WEP_IV_LEN - WEP_ICV_LEN;

	keyidx = skb->data[hdrlen + 3] >> 6;

	if (!key || keyidx != key->conf.keyidx)
		return -1;

	klen = 3 + key->conf.keylen;

	
	memcpy(rc4key, skb->data + hdrlen, 3);

	
	memcpy(rc4key + 3, key->conf.key, key->conf.keylen);

	if (ieee80211_wep_decrypt_data(local->wep_rx_tfm, rc4key, klen,
				       skb->data + hdrlen + WEP_IV_LEN,
				       len))
		ret = -1;

	
	skb_trim(skb, skb->len - WEP_ICV_LEN);

	
	memmove(skb->data + WEP_IV_LEN, skb->data, hdrlen);
	skb_pull(skb, WEP_IV_LEN);

	return ret;
}


static bool ieee80211_wep_is_weak_iv(struct sk_buff *skb,
				     struct ieee80211_key *key)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	unsigned int hdrlen;
	u8 *ivpos;
	u32 iv;

	hdrlen = ieee80211_hdrlen(hdr->frame_control);
	ivpos = skb->data + hdrlen;
	iv = (ivpos[0] << 16) | (ivpos[1] << 8) | ivpos[2];

	return ieee80211_wep_weak_iv(iv, key->conf.keylen);
}

ieee80211_rx_result
ieee80211_crypto_wep_decrypt(struct ieee80211_rx_data *rx)
{
	struct sk_buff *skb = rx->skb;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	__le16 fc = hdr->frame_control;

	if (!ieee80211_is_data(fc) && !ieee80211_is_auth(fc))
		return RX_CONTINUE;

	if (!(status->flag & RX_FLAG_DECRYPTED)) {
		if (skb_linearize(rx->skb))
			return RX_DROP_UNUSABLE;
		if (rx->sta && ieee80211_wep_is_weak_iv(rx->skb, rx->key))
			rx->sta->wep_weak_iv_count++;
		if (ieee80211_wep_decrypt(rx->local, rx->skb, rx->key))
			return RX_DROP_UNUSABLE;
	} else if (!(status->flag & RX_FLAG_IV_STRIPPED)) {
		if (!pskb_may_pull(rx->skb, ieee80211_hdrlen(fc) + WEP_IV_LEN))
			return RX_DROP_UNUSABLE;
		if (rx->sta && ieee80211_wep_is_weak_iv(rx->skb, rx->key))
			rx->sta->wep_weak_iv_count++;
		ieee80211_wep_remove_iv(rx->local, rx->skb, rx->key);
		
		if (pskb_trim(rx->skb, rx->skb->len - WEP_ICV_LEN))
			return RX_DROP_UNUSABLE;
	}

	return RX_CONTINUE;
}

static int wep_encrypt_skb(struct ieee80211_tx_data *tx, struct sk_buff *skb)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);

	if (!info->control.hw_key) {
		if (ieee80211_wep_encrypt(tx->local, skb, tx->key->conf.key,
					  tx->key->conf.keylen,
					  tx->key->conf.keyidx))
			return -1;
	} else if (info->control.hw_key->flags &
			IEEE80211_KEY_FLAG_GENERATE_IV) {
		if (!ieee80211_wep_add_iv(tx->local, skb,
					  tx->key->conf.keylen,
					  tx->key->conf.keyidx))
			return -1;
	}

	return 0;
}

ieee80211_tx_result
ieee80211_crypto_wep_encrypt(struct ieee80211_tx_data *tx)
{
	struct sk_buff *skb;

	ieee80211_tx_set_protected(tx);

	skb_queue_walk(&tx->skbs, skb) {
		if (wep_encrypt_skb(tx, skb) < 0) {
			I802_DEBUG_INC(tx->local->tx_handlers_drop_wep);
			return TX_DROP;
		}
	}

	return TX_CONTINUE;
}
