/*
 * Implement cfg80211 ("iw") support.
 *
 * Copyright (C) 2009 M&N Solutions GmbH, 61191 Rosbach, Germany
 * Holger Schurig <hs4233@mail.mn-solutions.de>
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <asm/unaligned.h>

#include "decl.h"
#include "cfg.h"
#include "cmd.h"
#include "mesh.h"


#define CHAN2G(_channel, _freq, _flags) {        \
	.band             = IEEE80211_BAND_2GHZ, \
	.center_freq      = (_freq),             \
	.hw_value         = (_channel),          \
	.flags            = (_flags),            \
	.max_antenna_gain = 0,                   \
	.max_power        = 30,                  \
}

static struct ieee80211_channel lbs_2ghz_channels[] = {
	CHAN2G(1,  2412, 0),
	CHAN2G(2,  2417, 0),
	CHAN2G(3,  2422, 0),
	CHAN2G(4,  2427, 0),
	CHAN2G(5,  2432, 0),
	CHAN2G(6,  2437, 0),
	CHAN2G(7,  2442, 0),
	CHAN2G(8,  2447, 0),
	CHAN2G(9,  2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0),
};

#define RATETAB_ENT(_rate, _hw_value, _flags) { \
	.bitrate  = (_rate),                    \
	.hw_value = (_hw_value),                \
	.flags    = (_flags),                   \
}


static struct ieee80211_rate lbs_rates[] = {
	RATETAB_ENT(10,  0,  0),
	RATETAB_ENT(20,  1,  0),
	RATETAB_ENT(55,  2,  0),
	RATETAB_ENT(110, 3,  0),
	RATETAB_ENT(60,  9,  0),
	RATETAB_ENT(90,  6,  0),
	RATETAB_ENT(120, 7,  0),
	RATETAB_ENT(180, 8,  0),
	RATETAB_ENT(240, 9,  0),
	RATETAB_ENT(360, 10, 0),
	RATETAB_ENT(480, 11, 0),
	RATETAB_ENT(540, 12, 0),
};

static struct ieee80211_supported_band lbs_band_2ghz = {
	.channels = lbs_2ghz_channels,
	.n_channels = ARRAY_SIZE(lbs_2ghz_channels),
	.bitrates = lbs_rates,
	.n_bitrates = ARRAY_SIZE(lbs_rates),
};


static const u32 cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
};

#define LBS_DWELL_PASSIVE 100
#define LBS_DWELL_ACTIVE  40



static int lbs_auth_to_authtype(enum nl80211_auth_type auth_type)
{
	int ret = -ENOTSUPP;

	switch (auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
	case NL80211_AUTHTYPE_SHARED_KEY:
		ret = auth_type;
		break;
	case NL80211_AUTHTYPE_AUTOMATIC:
		ret = NL80211_AUTHTYPE_OPEN_SYSTEM;
		break;
	case NL80211_AUTHTYPE_NETWORK_EAP:
		ret = 0x80;
		break;
	default:
		
		break;
	}
	return ret;
}


static int lbs_add_rates(u8 *rates)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(lbs_rates); i++) {
		u8 rate = lbs_rates[i].bitrate / 5;
		if (rate == 0x02 || rate == 0x04 ||
		    rate == 0x0b || rate == 0x16)
			rate |= 0x80;
		rates[i] = rate;
	}
	return ARRAY_SIZE(lbs_rates);
}




#define LBS_MAX_SSID_TLV_SIZE			\
	(sizeof(struct mrvl_ie_header)		\
	 + IEEE80211_MAX_SSID_LEN)

static int lbs_add_ssid_tlv(u8 *tlv, const u8 *ssid, int ssid_len)
{
	struct mrvl_ie_ssid_param_set *ssid_tlv = (void *)tlv;

	ssid_tlv->header.type = cpu_to_le16(TLV_TYPE_SSID);
	ssid_tlv->header.len = cpu_to_le16(ssid_len);
	memcpy(ssid_tlv->ssid, ssid, ssid_len);
	return sizeof(ssid_tlv->header) + ssid_len;
}


#define LBS_MAX_CHANNEL_LIST_TLV_SIZE					\
	(sizeof(struct mrvl_ie_header)					\
	 + (LBS_SCAN_BEFORE_NAP * sizeof(struct chanscanparamset)))

static int lbs_add_channel_list_tlv(struct lbs_private *priv, u8 *tlv,
				    int last_channel, int active_scan)
{
	int chanscanparamsize = sizeof(struct chanscanparamset) *
		(last_channel - priv->scan_channel);

	struct mrvl_ie_header *header = (void *) tlv;


	header->type = cpu_to_le16(TLV_TYPE_CHANLIST);
	header->len  = cpu_to_le16(chanscanparamsize);
	tlv += sizeof(struct mrvl_ie_header);

	memset(tlv, 0, chanscanparamsize);

	while (priv->scan_channel < last_channel) {
		struct chanscanparamset *param = (void *) tlv;

		param->radiotype = CMD_SCAN_RADIO_TYPE_BG;
		param->channumber =
			priv->scan_req->channels[priv->scan_channel]->hw_value;
		if (active_scan) {
			param->maxscantime = cpu_to_le16(LBS_DWELL_ACTIVE);
		} else {
			param->chanscanmode.passivescan = 1;
			param->maxscantime = cpu_to_le16(LBS_DWELL_PASSIVE);
		}
		tlv += sizeof(struct chanscanparamset);
		priv->scan_channel++;
	}
	return sizeof(struct mrvl_ie_header) + chanscanparamsize;
}


#define LBS_MAX_RATES_TLV_SIZE			\
	(sizeof(struct mrvl_ie_header)		\
	 + (ARRAY_SIZE(lbs_rates)))

static int lbs_add_supported_rates_tlv(u8 *tlv)
{
	size_t i;
	struct mrvl_ie_rates_param_set *rate_tlv = (void *)tlv;

	rate_tlv->header.type = cpu_to_le16(TLV_TYPE_RATES);
	tlv += sizeof(rate_tlv->header);
	i = lbs_add_rates(tlv);
	tlv += i;
	rate_tlv->header.len = cpu_to_le16(i);
	return sizeof(rate_tlv->header) + i;
}

static u8 *
add_ie_rates(u8 *tlv, const u8 *ie, int *nrates)
{
	int hw, ap, ap_max = ie[1];
	u8 hw_rate;

	
	ie += 2;

	lbs_deb_hex(LBS_DEB_ASSOC, "AP IE Rates", (u8 *) ie, ap_max);

	for (hw = 0; hw < ARRAY_SIZE(lbs_rates); hw++) {
		hw_rate = lbs_rates[hw].bitrate / 5;
		for (ap = 0; ap < ap_max; ap++) {
			if (hw_rate == (ie[ap] & 0x7f)) {
				*tlv++ = ie[ap];
				*nrates = *nrates + 1;
			}
		}
	}
	return tlv;
}

static int lbs_add_common_rates_tlv(u8 *tlv, struct cfg80211_bss *bss)
{
	struct mrvl_ie_rates_param_set *rate_tlv = (void *)tlv;
	const u8 *rates_eid, *ext_rates_eid;
	int n = 0;

	rates_eid = ieee80211_bss_get_ie(bss, WLAN_EID_SUPP_RATES);
	ext_rates_eid = ieee80211_bss_get_ie(bss, WLAN_EID_EXT_SUPP_RATES);

	rate_tlv->header.type = cpu_to_le16(TLV_TYPE_RATES);
	tlv += sizeof(rate_tlv->header);

	
	if (rates_eid) {
		tlv = add_ie_rates(tlv, rates_eid, &n);

		
		if (ext_rates_eid)
			tlv = add_ie_rates(tlv, ext_rates_eid, &n);
	} else {
		lbs_deb_assoc("assoc: bss had no basic rate IE\n");
		
		*tlv++ = 0x82;
		*tlv++ = 0x84;
		*tlv++ = 0x8b;
		*tlv++ = 0x96;
		n = 4;
	}

	rate_tlv->header.len = cpu_to_le16(n);
	return sizeof(rate_tlv->header) + n;
}


#define LBS_MAX_AUTH_TYPE_TLV_SIZE \
	sizeof(struct mrvl_ie_auth_type)

static int lbs_add_auth_type_tlv(u8 *tlv, enum nl80211_auth_type auth_type)
{
	struct mrvl_ie_auth_type *auth = (void *) tlv;

	auth->header.type = cpu_to_le16(TLV_TYPE_AUTH_TYPE);
	auth->header.len = cpu_to_le16(sizeof(*auth)-sizeof(auth->header));
	auth->auth = cpu_to_le16(lbs_auth_to_authtype(auth_type));
	return sizeof(*auth);
}


#define LBS_MAX_CHANNEL_TLV_SIZE \
	sizeof(struct mrvl_ie_header)

static int lbs_add_channel_tlv(u8 *tlv, u8 channel)
{
	struct mrvl_ie_ds_param_set *ds = (void *) tlv;

	ds->header.type = cpu_to_le16(TLV_TYPE_PHY_DS);
	ds->header.len = cpu_to_le16(sizeof(*ds)-sizeof(ds->header));
	ds->channel = channel;
	return sizeof(*ds);
}


#define LBS_MAX_CF_PARAM_TLV_SIZE		\
	sizeof(struct mrvl_ie_header)

static int lbs_add_cf_param_tlv(u8 *tlv)
{
	struct mrvl_ie_cf_param_set *cf = (void *)tlv;

	cf->header.type = cpu_to_le16(TLV_TYPE_CF);
	cf->header.len = cpu_to_le16(sizeof(*cf)-sizeof(cf->header));
	return sizeof(*cf);
}

#define LBS_MAX_WPA_TLV_SIZE			\
	(sizeof(struct mrvl_ie_header)		\
	 + 128 )

static int lbs_add_wpa_tlv(u8 *tlv, const u8 *ie, u8 ie_len)
{
	size_t tlv_len;

	*tlv++ = *ie++;
	*tlv++ = 0;
	tlv_len = *tlv++ = *ie++;
	*tlv++ = 0;
	while (tlv_len--)
		*tlv++ = *ie++;
	
	return ie_len + 2;
}


static int lbs_cfg_set_channel(struct wiphy *wiphy,
	struct net_device *netdev,
	struct ieee80211_channel *channel,
	enum nl80211_channel_type channel_type)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	int ret = -ENOTSUPP;

	lbs_deb_enter_args(LBS_DEB_CFG80211, "iface %s freq %d, type %d",
			   netdev_name(netdev), channel->center_freq, channel_type);

	if (channel_type != NL80211_CHAN_NO_HT)
		goto out;

	if (netdev == priv->mesh_dev)
		ret = lbs_mesh_set_channel(priv, channel->hw_value);
	else
		ret = lbs_set_channel(priv, channel->hw_value);

 out:
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}




#define LBS_SCAN_BEFORE_NAP 4


#define LBS_SCAN_RSSI_TO_MBM(rssi) \
	((-(int)rssi + 3)*100)

static int lbs_ret_scan(struct lbs_private *priv, unsigned long dummy,
	struct cmd_header *resp)
{
	struct cfg80211_bss *bss;
	struct cmd_ds_802_11_scan_rsp *scanresp = (void *)resp;
	int bsssize;
	const u8 *pos;
	const u8 *tsfdesc;
	int tsfsize;
	int i;
	int ret = -EILSEQ;

	lbs_deb_enter(LBS_DEB_CFG80211);

	bsssize = get_unaligned_le16(&scanresp->bssdescriptsize);

	lbs_deb_scan("scan response: %d BSSs (%d bytes); resp size %d bytes\n",
			scanresp->nr_sets, bsssize, le16_to_cpu(resp->size));

	if (scanresp->nr_sets == 0) {
		ret = 0;
		goto done;
	}


	pos = scanresp->bssdesc_and_tlvbuffer;

	lbs_deb_hex(LBS_DEB_SCAN, "SCAN_RSP", scanresp->bssdesc_and_tlvbuffer,
			scanresp->bssdescriptsize);

	tsfdesc = pos + bsssize;
	tsfsize = 4 + 8 * scanresp->nr_sets;
	lbs_deb_hex(LBS_DEB_SCAN, "SCAN_TSF", (u8 *) tsfdesc, tsfsize);

	
	i = get_unaligned_le16(tsfdesc);
	tsfdesc += 2;
	if (i != TLV_TYPE_TSFTIMESTAMP) {
		lbs_deb_scan("scan response: invalid TSF Timestamp %d\n", i);
		goto done;
	}

	i = get_unaligned_le16(tsfdesc);
	tsfdesc += 2;
	if (i / 8 != scanresp->nr_sets) {
		lbs_deb_scan("scan response: invalid number of TSF timestamp "
			     "sets (expected %d got %d)\n", scanresp->nr_sets,
			     i / 8);
		goto done;
	}

	for (i = 0; i < scanresp->nr_sets; i++) {
		const u8 *bssid;
		const u8 *ie;
		int left;
		int ielen;
		int rssi;
		u16 intvl;
		u16 capa;
		int chan_no = -1;
		const u8 *ssid = NULL;
		u8 ssid_len = 0;
		DECLARE_SSID_BUF(ssid_buf);

		int len = get_unaligned_le16(pos);
		pos += 2;

		
		bssid = pos;
		pos += ETH_ALEN;
		
		rssi = *pos++;
		
		pos += 8;
		
		intvl = get_unaligned_le16(pos);
		pos += 2;
		
		capa = get_unaligned_le16(pos);
		pos += 2;

		
		ie = pos;
		ielen = left = len - (6 + 1 + 8 + 2 + 2);
		while (left >= 2) {
			u8 id, elen;
			id = *pos++;
			elen = *pos++;
			left -= 2;
			if (elen > left || elen == 0) {
				lbs_deb_scan("scan response: invalid IE fmt\n");
				goto done;
			}

			if (id == WLAN_EID_DS_PARAMS)
				chan_no = *pos;
			if (id == WLAN_EID_SSID) {
				ssid = pos;
				ssid_len = elen;
			}
			left -= elen;
			pos += elen;
		}

		
		if (chan_no != -1) {
			struct wiphy *wiphy = priv->wdev->wiphy;
			int freq = ieee80211_channel_to_frequency(chan_no,
							IEEE80211_BAND_2GHZ);
			struct ieee80211_channel *channel =
				ieee80211_get_channel(wiphy, freq);

			lbs_deb_scan("scan: %pM, capa %04x, chan %2d, %s, "
				     "%d dBm\n",
				     bssid, capa, chan_no,
				     print_ssid(ssid_buf, ssid, ssid_len),
				     LBS_SCAN_RSSI_TO_MBM(rssi)/100);

			if (channel &&
			    !(channel->flags & IEEE80211_CHAN_DISABLED)) {
				bss = cfg80211_inform_bss(wiphy, channel,
					bssid, get_unaligned_le64(tsfdesc),
					capa, intvl, ie, ielen,
					LBS_SCAN_RSSI_TO_MBM(rssi),
					GFP_KERNEL);
				cfg80211_put_bss(bss);
			}
		} else
			lbs_deb_scan("scan response: missing BSS channel IE\n");

		tsfdesc += 8;
	}
	ret = 0;

 done:
	lbs_deb_leave_args(LBS_DEB_SCAN, "ret %d", ret);
	return ret;
}


#define LBS_SCAN_MAX_CMD_SIZE			\
	(sizeof(struct cmd_ds_802_11_scan)	\
	 + LBS_MAX_SSID_TLV_SIZE		\
	 + LBS_MAX_CHANNEL_LIST_TLV_SIZE	\
	 + LBS_MAX_RATES_TLV_SIZE)

static void lbs_scan_worker(struct work_struct *work)
{
	struct lbs_private *priv =
		container_of(work, struct lbs_private, scan_work.work);
	struct cmd_ds_802_11_scan *scan_cmd;
	u8 *tlv; 
	int last_channel;
	int running, carrier;

	lbs_deb_enter(LBS_DEB_SCAN);

	scan_cmd = kzalloc(LBS_SCAN_MAX_CMD_SIZE, GFP_KERNEL);
	if (scan_cmd == NULL)
		goto out_no_scan_cmd;

	
	scan_cmd->bsstype = CMD_BSS_TYPE_ANY;

	
	running = !netif_queue_stopped(priv->dev);
	carrier = netif_carrier_ok(priv->dev);
	if (running)
		netif_stop_queue(priv->dev);
	if (carrier)
		netif_carrier_off(priv->dev);

	
	tlv = scan_cmd->tlvbuffer;

	
	if (priv->scan_req->n_ssids && priv->scan_req->ssids[0].ssid_len > 0)
		tlv += lbs_add_ssid_tlv(tlv,
					priv->scan_req->ssids[0].ssid,
					priv->scan_req->ssids[0].ssid_len);

	
	last_channel = priv->scan_channel + LBS_SCAN_BEFORE_NAP;
	if (last_channel > priv->scan_req->n_channels)
		last_channel = priv->scan_req->n_channels;
	tlv += lbs_add_channel_list_tlv(priv, tlv, last_channel,
		priv->scan_req->n_ssids);

	
	tlv += lbs_add_supported_rates_tlv(tlv);

	if (priv->scan_channel < priv->scan_req->n_channels) {
		cancel_delayed_work(&priv->scan_work);
		if (netif_running(priv->dev))
			queue_delayed_work(priv->work_thread, &priv->scan_work,
				msecs_to_jiffies(300));
	}

	
	scan_cmd->hdr.size = cpu_to_le16(tlv - (u8 *)scan_cmd);
	lbs_deb_hex(LBS_DEB_SCAN, "SCAN_CMD", (void *)scan_cmd,
		    sizeof(*scan_cmd));
	lbs_deb_hex(LBS_DEB_SCAN, "SCAN_TLV", scan_cmd->tlvbuffer,
		    tlv - scan_cmd->tlvbuffer);

	__lbs_cmd(priv, CMD_802_11_SCAN, &scan_cmd->hdr,
		le16_to_cpu(scan_cmd->hdr.size),
		lbs_ret_scan, 0);

	if (priv->scan_channel >= priv->scan_req->n_channels) {
		
		cancel_delayed_work(&priv->scan_work);
		lbs_scan_done(priv);
	}

	
	if (carrier)
		netif_carrier_on(priv->dev);
	if (running && !priv->tx_pending_len)
		netif_wake_queue(priv->dev);

	kfree(scan_cmd);

	
	if (priv->scan_req == NULL) {
		lbs_deb_scan("scan: waking up waiters\n");
		wake_up_all(&priv->scan_q);
	}

 out_no_scan_cmd:
	lbs_deb_leave(LBS_DEB_SCAN);
}

static void _internal_start_scan(struct lbs_private *priv, bool internal,
	struct cfg80211_scan_request *request)
{
	lbs_deb_enter(LBS_DEB_CFG80211);

	lbs_deb_scan("scan: ssids %d, channels %d, ie_len %zd\n",
		request->n_ssids, request->n_channels, request->ie_len);

	priv->scan_channel = 0;
	priv->scan_req = request;
	priv->internal_scan = internal;

	queue_delayed_work(priv->work_thread, &priv->scan_work,
		msecs_to_jiffies(50));

	lbs_deb_leave(LBS_DEB_CFG80211);
}

void lbs_scan_done(struct lbs_private *priv)
{
	WARN_ON(!priv->scan_req);

	if (priv->internal_scan)
		kfree(priv->scan_req);
	else
		cfg80211_scan_done(priv->scan_req, false);

	priv->scan_req = NULL;
}

static int lbs_cfg_scan(struct wiphy *wiphy,
	struct net_device *dev,
	struct cfg80211_scan_request *request)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	int ret = 0;

	lbs_deb_enter(LBS_DEB_CFG80211);

	if (priv->scan_req || delayed_work_pending(&priv->scan_work)) {
		
		ret = -EAGAIN;
		goto out;
	}

	_internal_start_scan(priv, false, request);

	if (priv->surpriseremoved)
		ret = -EIO;

 out:
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}





void lbs_send_disconnect_notification(struct lbs_private *priv)
{
	lbs_deb_enter(LBS_DEB_CFG80211);

	cfg80211_disconnected(priv->dev,
		0,
		NULL, 0,
		GFP_KERNEL);

	lbs_deb_leave(LBS_DEB_CFG80211);
}

void lbs_send_mic_failureevent(struct lbs_private *priv, u32 event)
{
	lbs_deb_enter(LBS_DEB_CFG80211);

	cfg80211_michael_mic_failure(priv->dev,
		priv->assoc_bss,
		event == MACREG_INT_CODE_MIC_ERR_MULTICAST ?
			NL80211_KEYTYPE_GROUP :
			NL80211_KEYTYPE_PAIRWISE,
		-1,
		NULL,
		GFP_KERNEL);

	lbs_deb_leave(LBS_DEB_CFG80211);
}






static int lbs_remove_wep_keys(struct lbs_private *priv)
{
	struct cmd_ds_802_11_set_wep cmd;
	int ret;

	lbs_deb_enter(LBS_DEB_CFG80211);

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));
	cmd.keyindex = cpu_to_le16(priv->wep_tx_key);
	cmd.action = cpu_to_le16(CMD_ACT_REMOVE);

	ret = lbs_cmd_with_response(priv, CMD_802_11_SET_WEP, &cmd);

	lbs_deb_leave(LBS_DEB_CFG80211);
	return ret;
}

static int lbs_set_wep_keys(struct lbs_private *priv)
{
	struct cmd_ds_802_11_set_wep cmd;
	int i;
	int ret;

	lbs_deb_enter(LBS_DEB_CFG80211);

	if (priv->wep_key_len[0] || priv->wep_key_len[1] ||
	    priv->wep_key_len[2] || priv->wep_key_len[3]) {
		
		memset(&cmd, 0, sizeof(cmd));
		cmd.hdr.size = cpu_to_le16(sizeof(cmd));
		cmd.keyindex = cpu_to_le16(priv->wep_tx_key);
		cmd.action = cpu_to_le16(CMD_ACT_ADD);

		for (i = 0; i < 4; i++) {
			switch (priv->wep_key_len[i]) {
			case WLAN_KEY_LEN_WEP40:
				cmd.keytype[i] = CMD_TYPE_WEP_40_BIT;
				break;
			case WLAN_KEY_LEN_WEP104:
				cmd.keytype[i] = CMD_TYPE_WEP_104_BIT;
				break;
			default:
				cmd.keytype[i] = 0;
				break;
			}
			memcpy(cmd.keymaterial[i], priv->wep_key[i],
			       priv->wep_key_len[i]);
		}

		ret = lbs_cmd_with_response(priv, CMD_802_11_SET_WEP, &cmd);
	} else {
		
		ret = lbs_remove_wep_keys(priv);
	}

	lbs_deb_leave(LBS_DEB_CFG80211);
	return ret;
}


static int lbs_enable_rsn(struct lbs_private *priv, int enable)
{
	struct cmd_ds_802_11_enable_rsn cmd;
	int ret;

	lbs_deb_enter_args(LBS_DEB_CFG80211, "%d", enable);

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));
	cmd.action = cpu_to_le16(CMD_ACT_SET);
	cmd.enable = cpu_to_le16(enable);

	ret = lbs_cmd_with_response(priv, CMD_802_11_ENABLE_RSN, &cmd);

	lbs_deb_leave(LBS_DEB_CFG80211);
	return ret;
}




struct cmd_key_material {
	struct cmd_header hdr;

	__le16 action;
	struct MrvlIEtype_keyParamSet param;
} __packed;

static int lbs_set_key_material(struct lbs_private *priv,
				int key_type,
				int key_info,
				u8 *key, u16 key_len)
{
	struct cmd_key_material cmd;
	int ret;

	lbs_deb_enter(LBS_DEB_CFG80211);

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));
	cmd.action = cpu_to_le16(CMD_ACT_SET);
	cmd.param.type = cpu_to_le16(TLV_TYPE_KEY_MATERIAL);
	cmd.param.length = cpu_to_le16(sizeof(cmd.param) - 4);
	cmd.param.keytypeid = cpu_to_le16(key_type);
	cmd.param.keyinfo = cpu_to_le16(key_info);
	cmd.param.keylen = cpu_to_le16(key_len);
	if (key && key_len)
		memcpy(cmd.param.key, key, key_len);

	ret = lbs_cmd_with_response(priv, CMD_802_11_KEY_MATERIAL, &cmd);

	lbs_deb_leave(LBS_DEB_CFG80211);
	return ret;
}


static int lbs_set_authtype(struct lbs_private *priv,
			    struct cfg80211_connect_params *sme)
{
	struct cmd_ds_802_11_authenticate cmd;
	int ret;

	lbs_deb_enter_args(LBS_DEB_CFG80211, "%d", sme->auth_type);

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));
	if (sme->bssid)
		memcpy(cmd.bssid, sme->bssid, ETH_ALEN);
	
	ret = lbs_auth_to_authtype(sme->auth_type);
	if (ret < 0)
		goto done;

	cmd.authtype = ret;
	ret = lbs_cmd_with_response(priv, CMD_802_11_AUTHENTICATE, &cmd);

 done:
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}


#define LBS_ASSOC_MAX_CMD_SIZE                     \
	(sizeof(struct cmd_ds_802_11_associate)    \
	 - 512  \
	 + LBS_MAX_SSID_TLV_SIZE                   \
	 + LBS_MAX_CHANNEL_TLV_SIZE                \
	 + LBS_MAX_CF_PARAM_TLV_SIZE               \
	 + LBS_MAX_AUTH_TYPE_TLV_SIZE              \
	 + LBS_MAX_WPA_TLV_SIZE)

static int lbs_associate(struct lbs_private *priv,
		struct cfg80211_bss *bss,
		struct cfg80211_connect_params *sme)
{
	struct cmd_ds_802_11_associate_response *resp;
	struct cmd_ds_802_11_associate *cmd = kzalloc(LBS_ASSOC_MAX_CMD_SIZE,
						      GFP_KERNEL);
	const u8 *ssid_eid;
	size_t len, resp_ie_len;
	int status;
	int ret;
	u8 *pos = &(cmd->iebuf[0]);
	u8 *tmp;

	lbs_deb_enter(LBS_DEB_CFG80211);

	if (!cmd) {
		ret = -ENOMEM;
		goto done;
	}

	cmd->hdr.command = cpu_to_le16(CMD_802_11_ASSOCIATE);

	
	memcpy(cmd->bssid, bss->bssid, ETH_ALEN);
	cmd->listeninterval = cpu_to_le16(MRVDRV_DEFAULT_LISTEN_INTERVAL);
	cmd->capability = cpu_to_le16(bss->capability);

	
	ssid_eid = ieee80211_bss_get_ie(bss, WLAN_EID_SSID);
	if (ssid_eid)
		pos += lbs_add_ssid_tlv(pos, ssid_eid + 2, ssid_eid[1]);
	else
		lbs_deb_assoc("no SSID\n");

	
	if (bss->channel)
		pos += lbs_add_channel_tlv(pos, bss->channel->hw_value);
	else
		lbs_deb_assoc("no channel\n");

	
	pos += lbs_add_cf_param_tlv(pos);

	
	tmp = pos + 4; 
	pos += lbs_add_common_rates_tlv(pos, bss);
	lbs_deb_hex(LBS_DEB_ASSOC, "Common Rates", tmp, pos - tmp);

	
	if (MRVL_FW_MAJOR_REV(priv->fwrelease) >= 9)
		pos += lbs_add_auth_type_tlv(pos, sme->auth_type);

	
	if (sme->ie && sme->ie_len)
		pos += lbs_add_wpa_tlv(pos, sme->ie, sme->ie_len);

	len = (sizeof(*cmd) - sizeof(cmd->iebuf)) +
		(u16)(pos - (u8 *) &cmd->iebuf);
	cmd->hdr.size = cpu_to_le16(len);

	lbs_deb_hex(LBS_DEB_ASSOC, "ASSOC_CMD", (u8 *) cmd,
			le16_to_cpu(cmd->hdr.size));

	
	memcpy(priv->assoc_bss, bss->bssid, ETH_ALEN);

	ret = lbs_cmd_with_response(priv, CMD_802_11_ASSOCIATE, cmd);
	if (ret)
		goto done;

	

	resp = (void *) cmd; 
	status = le16_to_cpu(resp->statuscode);

	if (MRVL_FW_MAJOR_REV(priv->fwrelease) <= 8) {
		switch (status) {
		case 0:
			break;
		case 1:
			lbs_deb_assoc("invalid association parameters\n");
			status = WLAN_STATUS_CAPS_UNSUPPORTED;
			break;
		case 2:
			lbs_deb_assoc("timer expired while waiting for AP\n");
			status = WLAN_STATUS_AUTH_TIMEOUT;
			break;
		case 3:
			lbs_deb_assoc("association refused by AP\n");
			status = WLAN_STATUS_ASSOC_DENIED_UNSPEC;
			break;
		case 4:
			lbs_deb_assoc("authentication refused by AP\n");
			status = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
			break;
		default:
			lbs_deb_assoc("association failure %d\n", status);
			break;
		}
	}

	lbs_deb_assoc("status %d, statuscode 0x%04x, capability 0x%04x, "
		      "aid 0x%04x\n", status, le16_to_cpu(resp->statuscode),
		      le16_to_cpu(resp->capability), le16_to_cpu(resp->aid));

	resp_ie_len = le16_to_cpu(resp->hdr.size)
		- sizeof(resp->hdr)
		- 6;
	cfg80211_connect_result(priv->dev,
				priv->assoc_bss,
				sme->ie, sme->ie_len,
				resp->iebuf, resp_ie_len,
				status,
				GFP_KERNEL);

	if (status == 0) {
		
		priv->connect_status = LBS_CONNECTED;
		netif_carrier_on(priv->dev);
		if (!priv->tx_pending_len)
			netif_tx_wake_all_queues(priv->dev);
	}

done:
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}

static struct cfg80211_scan_request *
_new_connect_scan_req(struct wiphy *wiphy, struct cfg80211_connect_params *sme)
{
	struct cfg80211_scan_request *creq = NULL;
	int i, n_channels = 0;
	enum ieee80211_band band;

	for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
		if (wiphy->bands[band])
			n_channels += wiphy->bands[band]->n_channels;
	}

	creq = kzalloc(sizeof(*creq) + sizeof(struct cfg80211_ssid) +
		       n_channels * sizeof(void *),
		       GFP_ATOMIC);
	if (!creq)
		return NULL;

	
	creq->ssids = (void *)&creq->channels[n_channels];
	creq->n_channels = n_channels;
	creq->n_ssids = 1;

	
	i = 0;
	for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
		int j;

		if (!wiphy->bands[band])
			continue;

		for (j = 0; j < wiphy->bands[band]->n_channels; j++) {
			
			if (wiphy->bands[band]->channels[j].flags &
						IEEE80211_CHAN_DISABLED)
				continue;

			creq->channels[i] = &wiphy->bands[band]->channels[j];
			i++;
		}
	}
	if (i) {
		
		creq->n_channels = i;

		
		memcpy(creq->ssids[0].ssid, sme->ssid, sme->ssid_len);
		creq->ssids[0].ssid_len = sme->ssid_len;
	} else {
		
		kfree(creq);
		creq = NULL;
	}

	return creq;
}

static int lbs_cfg_connect(struct wiphy *wiphy, struct net_device *dev,
			   struct cfg80211_connect_params *sme)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	struct cfg80211_bss *bss = NULL;
	int ret = 0;
	u8 preamble = RADIO_PREAMBLE_SHORT;

	if (dev == priv->mesh_dev)
		return -EOPNOTSUPP;

	lbs_deb_enter(LBS_DEB_CFG80211);

	if (!sme->bssid) {
		struct cfg80211_scan_request *creq;

		lbs_deb_assoc("assoc: waiting for existing scans\n");
		wait_event_interruptible_timeout(priv->scan_q,
						 (priv->scan_req == NULL),
						 (15 * HZ));

		creq = _new_connect_scan_req(wiphy, sme);
		if (!creq) {
			ret = -EINVAL;
			goto done;
		}

		lbs_deb_assoc("assoc: scanning for compatible AP\n");
		_internal_start_scan(priv, true, creq);

		lbs_deb_assoc("assoc: waiting for scan to complete\n");
		wait_event_interruptible_timeout(priv->scan_q,
						 (priv->scan_req == NULL),
						 (15 * HZ));
		lbs_deb_assoc("assoc: scanning competed\n");
	}

	
	bss = cfg80211_get_bss(wiphy, sme->channel, sme->bssid,
		sme->ssid, sme->ssid_len,
		WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);
	if (!bss) {
		wiphy_err(wiphy, "assoc: bss %pM not in scan results\n",
			  sme->bssid);
		ret = -ENOENT;
		goto done;
	}
	lbs_deb_assoc("trying %pM\n", bss->bssid);
	lbs_deb_assoc("cipher 0x%x, key index %d, key len %d\n",
		      sme->crypto.cipher_group,
		      sme->key_idx, sme->key_len);

	
	priv->wep_tx_key = 0;
	memset(priv->wep_key, 0, sizeof(priv->wep_key));
	memset(priv->wep_key_len, 0, sizeof(priv->wep_key_len));

	
	switch (sme->crypto.cipher_group) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		
		priv->wep_tx_key = sme->key_idx;
		priv->wep_key_len[sme->key_idx] = sme->key_len;
		memcpy(priv->wep_key[sme->key_idx], sme->key, sme->key_len);
		
		lbs_set_wep_keys(priv);
		priv->mac_control |= CMD_ACT_MAC_WEP_ENABLE;
		lbs_set_mac_control(priv);
		
		lbs_enable_rsn(priv, 0);
		break;
	case 0: 
	case WLAN_CIPHER_SUITE_TKIP:
	case WLAN_CIPHER_SUITE_CCMP:
		
		lbs_remove_wep_keys(priv);
		priv->mac_control &= ~CMD_ACT_MAC_WEP_ENABLE;
		lbs_set_mac_control(priv);

		
		lbs_set_key_material(priv,
			KEY_TYPE_ID_WEP, 
			KEY_INFO_WPA_UNICAST,
			NULL, 0);
		lbs_set_key_material(priv,
			KEY_TYPE_ID_WEP, 
			KEY_INFO_WPA_MCAST,
			NULL, 0);
		
		lbs_enable_rsn(priv, sme->crypto.cipher_group != 0);
		break;
	default:
		wiphy_err(wiphy, "unsupported cipher group 0x%x\n",
			  sme->crypto.cipher_group);
		ret = -ENOTSUPP;
		goto done;
	}

	ret = lbs_set_authtype(priv, sme);
	if (ret == -ENOTSUPP) {
		wiphy_err(wiphy, "unsupported authtype 0x%x\n", sme->auth_type);
		goto done;
	}

	lbs_set_radio(priv, preamble, 1);

	
	ret = lbs_associate(priv, bss, sme);

 done:
	if (bss)
		cfg80211_put_bss(bss);
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}

int lbs_disconnect(struct lbs_private *priv, u16 reason)
{
	struct cmd_ds_802_11_deauthenticate cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));
	
	memcpy(cmd.macaddr, &priv->assoc_bss, ETH_ALEN);
	cmd.reasoncode = cpu_to_le16(reason);

	ret = lbs_cmd_with_response(priv, CMD_802_11_DEAUTHENTICATE, &cmd);
	if (ret)
		return ret;

	cfg80211_disconnected(priv->dev,
			reason,
			NULL, 0,
			GFP_KERNEL);
	priv->connect_status = LBS_DISCONNECTED;

	return 0;
}

static int lbs_cfg_disconnect(struct wiphy *wiphy, struct net_device *dev,
	u16 reason_code)
{
	struct lbs_private *priv = wiphy_priv(wiphy);

	if (dev == priv->mesh_dev)
		return -EOPNOTSUPP;

	lbs_deb_enter_args(LBS_DEB_CFG80211, "reason_code %d", reason_code);

	
	priv->disassoc_reason = reason_code;

	return lbs_disconnect(priv, reason_code);
}

static int lbs_cfg_set_default_key(struct wiphy *wiphy,
				   struct net_device *netdev,
				   u8 key_index, bool unicast,
				   bool multicast)
{
	struct lbs_private *priv = wiphy_priv(wiphy);

	if (netdev == priv->mesh_dev)
		return -EOPNOTSUPP;

	lbs_deb_enter(LBS_DEB_CFG80211);

	if (key_index != priv->wep_tx_key) {
		lbs_deb_assoc("set_default_key: to %d\n", key_index);
		priv->wep_tx_key = key_index;
		lbs_set_wep_keys(priv);
	}

	return 0;
}


static int lbs_cfg_add_key(struct wiphy *wiphy, struct net_device *netdev,
			   u8 idx, bool pairwise, const u8 *mac_addr,
			   struct key_params *params)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	u16 key_info;
	u16 key_type;
	int ret = 0;

	if (netdev == priv->mesh_dev)
		return -EOPNOTSUPP;

	lbs_deb_enter(LBS_DEB_CFG80211);

	lbs_deb_assoc("add_key: cipher 0x%x, mac_addr %pM\n",
		      params->cipher, mac_addr);
	lbs_deb_assoc("add_key: key index %d, key len %d\n",
		      idx, params->key_len);
	if (params->key_len)
		lbs_deb_hex(LBS_DEB_CFG80211, "KEY",
			    params->key, params->key_len);

	lbs_deb_assoc("add_key: seq len %d\n", params->seq_len);
	if (params->seq_len)
		lbs_deb_hex(LBS_DEB_CFG80211, "SEQ",
			    params->seq, params->seq_len);

	switch (params->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		
		if ((priv->wep_key_len[idx] != params->key_len) ||
			memcmp(priv->wep_key[idx],
			       params->key, params->key_len) != 0) {
			priv->wep_key_len[idx] = params->key_len;
			memcpy(priv->wep_key[idx],
			       params->key, params->key_len);
			lbs_set_wep_keys(priv);
		}
		break;
	case WLAN_CIPHER_SUITE_TKIP:
	case WLAN_CIPHER_SUITE_CCMP:
		key_info = KEY_INFO_WPA_ENABLED | ((idx == 0)
						   ? KEY_INFO_WPA_UNICAST
						   : KEY_INFO_WPA_MCAST);
		key_type = (params->cipher == WLAN_CIPHER_SUITE_TKIP)
			? KEY_TYPE_ID_TKIP
			: KEY_TYPE_ID_AES;
		lbs_set_key_material(priv,
				     key_type,
				     key_info,
				     params->key, params->key_len);
		break;
	default:
		wiphy_err(wiphy, "unhandled cipher 0x%x\n", params->cipher);
		ret = -ENOTSUPP;
		break;
	}

	return ret;
}


static int lbs_cfg_del_key(struct wiphy *wiphy, struct net_device *netdev,
			   u8 key_index, bool pairwise, const u8 *mac_addr)
{

	lbs_deb_enter(LBS_DEB_CFG80211);

	lbs_deb_assoc("del_key: key_idx %d, mac_addr %pM\n",
		      key_index, mac_addr);

#ifdef TODO
	struct lbs_private *priv = wiphy_priv(wiphy);
	if (key_index < 3 && priv->wep_key_len[key_index]) {
		priv->wep_key_len[key_index] = 0;
		lbs_set_wep_keys(priv);
	}
#endif

	return 0;
}



static int lbs_cfg_get_station(struct wiphy *wiphy, struct net_device *dev,
			      u8 *mac, struct station_info *sinfo)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	s8 signal, noise;
	int ret;
	size_t i;

	lbs_deb_enter(LBS_DEB_CFG80211);

	sinfo->filled |= STATION_INFO_TX_BYTES |
			 STATION_INFO_TX_PACKETS |
			 STATION_INFO_RX_BYTES |
			 STATION_INFO_RX_PACKETS;
	sinfo->tx_bytes = priv->dev->stats.tx_bytes;
	sinfo->tx_packets = priv->dev->stats.tx_packets;
	sinfo->rx_bytes = priv->dev->stats.rx_bytes;
	sinfo->rx_packets = priv->dev->stats.rx_packets;

	
	ret = lbs_get_rssi(priv, &signal, &noise);
	if (ret == 0) {
		sinfo->signal = signal;
		sinfo->filled |= STATION_INFO_SIGNAL;
	}

	
	for (i = 0; i < ARRAY_SIZE(lbs_rates); i++) {
		if (priv->cur_rate == lbs_rates[i].hw_value) {
			sinfo->txrate.legacy = lbs_rates[i].bitrate;
			sinfo->filled |= STATION_INFO_TX_BITRATE;
			break;
		}
	}

	return 0;
}





static int lbs_change_intf(struct wiphy *wiphy, struct net_device *dev,
	enum nl80211_iftype type, u32 *flags,
	       struct vif_params *params)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	int ret = 0;

	if (dev == priv->mesh_dev)
		return -EOPNOTSUPP;

	switch (type) {
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_ADHOC:
		break;
	default:
		return -EOPNOTSUPP;
	}

	lbs_deb_enter(LBS_DEB_CFG80211);

	if (priv->iface_running)
		ret = lbs_set_iface_type(priv, type);

	if (!ret)
		priv->wdev->iftype = type;

	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}




#define CAPINFO_MASK (~(0xda00))


static void lbs_join_post(struct lbs_private *priv,
			  struct cfg80211_ibss_params *params,
			  u8 *bssid, u16 capability)
{
	u8 fake_ie[2 + IEEE80211_MAX_SSID_LEN + 
		   2 + 4 +                      
		   2 + 1 +                      
		   2 + 2 +                      
		   2 + 8];                      
	u8 *fake = fake_ie;
	struct cfg80211_bss *bss;

	lbs_deb_enter(LBS_DEB_CFG80211);

	
	*fake++ = WLAN_EID_SSID;
	*fake++ = params->ssid_len;
	memcpy(fake, params->ssid, params->ssid_len);
	fake += params->ssid_len;
	
	*fake++ = WLAN_EID_SUPP_RATES;
	*fake++ = 4;
	*fake++ = 0x82;
	*fake++ = 0x84;
	*fake++ = 0x8b;
	*fake++ = 0x96;
	
	*fake++ = WLAN_EID_DS_PARAMS;
	*fake++ = 1;
	*fake++ = params->channel->hw_value;
	
	*fake++ = WLAN_EID_IBSS_PARAMS;
	*fake++ = 2;
	*fake++ = 0; 
	*fake++ = 0;
	*fake++ = WLAN_EID_EXT_SUPP_RATES;
	*fake++ = 8;
	*fake++ = 0x0c;
	*fake++ = 0x12;
	*fake++ = 0x18;
	*fake++ = 0x24;
	*fake++ = 0x30;
	*fake++ = 0x48;
	*fake++ = 0x60;
	*fake++ = 0x6c;
	lbs_deb_hex(LBS_DEB_CFG80211, "IE", fake_ie, fake - fake_ie);

	bss = cfg80211_inform_bss(priv->wdev->wiphy,
				  params->channel,
				  bssid,
				  0,
				  capability,
				  params->beacon_interval,
				  fake_ie, fake - fake_ie,
				  0, GFP_KERNEL);
	cfg80211_put_bss(bss);

	memcpy(priv->wdev->ssid, params->ssid, params->ssid_len);
	priv->wdev->ssid_len = params->ssid_len;

	cfg80211_ibss_joined(priv->dev, bssid, GFP_KERNEL);

	
	priv->connect_status = LBS_CONNECTED;
	netif_carrier_on(priv->dev);
	if (!priv->tx_pending_len)
		netif_wake_queue(priv->dev);

	lbs_deb_leave(LBS_DEB_CFG80211);
}

static int lbs_ibss_join_existing(struct lbs_private *priv,
	struct cfg80211_ibss_params *params,
	struct cfg80211_bss *bss)
{
	const u8 *rates_eid = ieee80211_bss_get_ie(bss, WLAN_EID_SUPP_RATES);
	struct cmd_ds_802_11_ad_hoc_join cmd;
	u8 preamble = RADIO_PREAMBLE_SHORT;
	int ret = 0;

	lbs_deb_enter(LBS_DEB_CFG80211);

	
	ret = lbs_set_radio(priv, preamble, 1);
	if (ret)
		goto out;

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));

	memcpy(cmd.bss.bssid, bss->bssid, ETH_ALEN);
	memcpy(cmd.bss.ssid, params->ssid, params->ssid_len);
	cmd.bss.type = CMD_BSS_TYPE_IBSS;
	cmd.bss.beaconperiod = cpu_to_le16(params->beacon_interval);
	cmd.bss.ds.header.id = WLAN_EID_DS_PARAMS;
	cmd.bss.ds.header.len = 1;
	cmd.bss.ds.channel = params->channel->hw_value;
	cmd.bss.ibss.header.id = WLAN_EID_IBSS_PARAMS;
	cmd.bss.ibss.header.len = 2;
	cmd.bss.ibss.atimwindow = 0;
	cmd.bss.capability = cpu_to_le16(bss->capability & CAPINFO_MASK);

	if (!rates_eid) {
		lbs_add_rates(cmd.bss.rates);
	} else {
		int hw, i;
		u8 rates_max = rates_eid[1];
		u8 *rates = cmd.bss.rates;
		for (hw = 0; hw < ARRAY_SIZE(lbs_rates); hw++) {
			u8 hw_rate = lbs_rates[hw].bitrate / 5;
			for (i = 0; i < rates_max; i++) {
				if (hw_rate == (rates_eid[i+2] & 0x7f)) {
					u8 rate = rates_eid[i+2];
					if (rate == 0x02 || rate == 0x04 ||
					    rate == 0x0b || rate == 0x16)
						rate |= 0x80;
					*rates++ = rate;
				}
			}
		}
	}

	
	if (MRVL_FW_MAJOR_REV(priv->fwrelease) <= 8) {
		cmd.failtimeout = cpu_to_le16(MRVDRV_ASSOCIATION_TIME_OUT);
		cmd.probedelay = cpu_to_le16(CMD_SCAN_PROBE_DELAY_TIME);
	}
	ret = lbs_cmd_with_response(priv, CMD_802_11_AD_HOC_JOIN, &cmd);
	if (ret)
		goto out;

	lbs_join_post(priv, params, bss->bssid, bss->capability);

 out:
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}



static int lbs_ibss_start_new(struct lbs_private *priv,
	struct cfg80211_ibss_params *params)
{
	struct cmd_ds_802_11_ad_hoc_start cmd;
	struct cmd_ds_802_11_ad_hoc_result *resp =
		(struct cmd_ds_802_11_ad_hoc_result *) &cmd;
	u8 preamble = RADIO_PREAMBLE_SHORT;
	int ret = 0;
	u16 capability;

	lbs_deb_enter(LBS_DEB_CFG80211);

	ret = lbs_set_radio(priv, preamble, 1);
	if (ret)
		goto out;

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));
	memcpy(cmd.ssid, params->ssid, params->ssid_len);
	cmd.bsstype = CMD_BSS_TYPE_IBSS;
	cmd.beaconperiod = cpu_to_le16(params->beacon_interval);
	cmd.ibss.header.id = WLAN_EID_IBSS_PARAMS;
	cmd.ibss.header.len = 2;
	cmd.ibss.atimwindow = 0;
	cmd.ds.header.id = WLAN_EID_DS_PARAMS;
	cmd.ds.header.len = 1;
	cmd.ds.channel = params->channel->hw_value;
	
	if (MRVL_FW_MAJOR_REV(priv->fwrelease) <= 8)
		cmd.probedelay = cpu_to_le16(CMD_SCAN_PROBE_DELAY_TIME);
	
	capability = WLAN_CAPABILITY_IBSS;
	cmd.capability = cpu_to_le16(capability);
	lbs_add_rates(cmd.rates);


	ret = lbs_cmd_with_response(priv, CMD_802_11_AD_HOC_START, &cmd);
	if (ret)
		goto out;

	lbs_join_post(priv, params, resp->bssid, capability);

 out:
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}


static int lbs_join_ibss(struct wiphy *wiphy, struct net_device *dev,
		struct cfg80211_ibss_params *params)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	int ret = 0;
	struct cfg80211_bss *bss;
	DECLARE_SSID_BUF(ssid_buf);

	if (dev == priv->mesh_dev)
		return -EOPNOTSUPP;

	lbs_deb_enter(LBS_DEB_CFG80211);

	if (!params->channel) {
		ret = -ENOTSUPP;
		goto out;
	}

	ret = lbs_set_channel(priv, params->channel->hw_value);
	if (ret)
		goto out;

	bss = cfg80211_get_bss(wiphy, params->channel, params->bssid,
		params->ssid, params->ssid_len,
		WLAN_CAPABILITY_IBSS, WLAN_CAPABILITY_IBSS);

	if (bss) {
		ret = lbs_ibss_join_existing(priv, params, bss);
		cfg80211_put_bss(bss);
	} else
		ret = lbs_ibss_start_new(priv, params);


 out:
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}


static int lbs_leave_ibss(struct wiphy *wiphy, struct net_device *dev)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	struct cmd_ds_802_11_ad_hoc_stop cmd;
	int ret = 0;

	if (dev == priv->mesh_dev)
		return -EOPNOTSUPP;

	lbs_deb_enter(LBS_DEB_CFG80211);

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));
	ret = lbs_cmd_with_response(priv, CMD_802_11_AD_HOC_STOP, &cmd);

	
	lbs_mac_event_disconnected(priv);

	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}





static struct cfg80211_ops lbs_cfg80211_ops = {
	.set_channel = lbs_cfg_set_channel,
	.scan = lbs_cfg_scan,
	.connect = lbs_cfg_connect,
	.disconnect = lbs_cfg_disconnect,
	.add_key = lbs_cfg_add_key,
	.del_key = lbs_cfg_del_key,
	.set_default_key = lbs_cfg_set_default_key,
	.get_station = lbs_cfg_get_station,
	.change_virtual_intf = lbs_change_intf,
	.join_ibss = lbs_join_ibss,
	.leave_ibss = lbs_leave_ibss,
};


struct wireless_dev *lbs_cfg_alloc(struct device *dev)
{
	int ret = 0;
	struct wireless_dev *wdev;

	lbs_deb_enter(LBS_DEB_CFG80211);

	wdev = kzalloc(sizeof(struct wireless_dev), GFP_KERNEL);
	if (!wdev) {
		dev_err(dev, "cannot allocate wireless device\n");
		return ERR_PTR(-ENOMEM);
	}

	wdev->wiphy = wiphy_new(&lbs_cfg80211_ops, sizeof(struct lbs_private));
	if (!wdev->wiphy) {
		dev_err(dev, "cannot allocate wiphy\n");
		ret = -ENOMEM;
		goto err_wiphy_new;
	}

	lbs_deb_leave(LBS_DEB_CFG80211);
	return wdev;

 err_wiphy_new:
	kfree(wdev);
	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ERR_PTR(ret);
}


static void lbs_cfg_set_regulatory_hint(struct lbs_private *priv)
{
	struct region_code_mapping {
		const char *cn;
		int code;
	};

	
	static const struct region_code_mapping regmap[] = {
		{"US ", 0x10}, 
		{"CA ", 0x20}, 
		{"EU ", 0x30}, 
		{"ES ", 0x31}, 
		{"FR ", 0x32}, 
		{"JP ", 0x40}, 
	};
	size_t i;

	lbs_deb_enter(LBS_DEB_CFG80211);

	for (i = 0; i < ARRAY_SIZE(regmap); i++)
		if (regmap[i].code == priv->regioncode) {
			regulatory_hint(priv->wdev->wiphy, regmap[i].cn);
			break;
		}

	lbs_deb_leave(LBS_DEB_CFG80211);
}


int lbs_cfg_register(struct lbs_private *priv)
{
	struct wireless_dev *wdev = priv->wdev;
	int ret;

	lbs_deb_enter(LBS_DEB_CFG80211);

	wdev->wiphy->max_scan_ssids = 1;
	wdev->wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

	wdev->wiphy->interface_modes =
			BIT(NL80211_IFTYPE_STATION) |
			BIT(NL80211_IFTYPE_ADHOC);
	if (lbs_rtap_supported(priv))
		wdev->wiphy->interface_modes |= BIT(NL80211_IFTYPE_MONITOR);
	if (lbs_mesh_activated(priv))
		wdev->wiphy->interface_modes |= BIT(NL80211_IFTYPE_MESH_POINT);

	wdev->wiphy->bands[IEEE80211_BAND_2GHZ] = &lbs_band_2ghz;

	wdev->wiphy->cipher_suites = cipher_suites;
	wdev->wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites);
	wdev->wiphy->reg_notifier = lbs_reg_notifier;

	ret = wiphy_register(wdev->wiphy);
	if (ret < 0)
		pr_err("cannot register wiphy device\n");

	priv->wiphy_registered = true;

	ret = register_netdev(priv->dev);
	if (ret)
		pr_err("cannot register network device\n");

	INIT_DELAYED_WORK(&priv->scan_work, lbs_scan_worker);

	lbs_cfg_set_regulatory_hint(priv);

	lbs_deb_leave_args(LBS_DEB_CFG80211, "ret %d", ret);
	return ret;
}

int lbs_reg_notifier(struct wiphy *wiphy,
		struct regulatory_request *request)
{
	struct lbs_private *priv = wiphy_priv(wiphy);
	int ret;

	lbs_deb_enter_args(LBS_DEB_CFG80211, "cfg80211 regulatory domain "
			"callback for domain %c%c\n", request->alpha2[0],
			request->alpha2[1]);

	ret = lbs_set_11d_domain_info(priv, request, wiphy->bands);

	lbs_deb_leave(LBS_DEB_CFG80211);
	return ret;
}

void lbs_scan_deinit(struct lbs_private *priv)
{
	lbs_deb_enter(LBS_DEB_CFG80211);
	cancel_delayed_work_sync(&priv->scan_work);
}


void lbs_cfg_free(struct lbs_private *priv)
{
	struct wireless_dev *wdev = priv->wdev;

	lbs_deb_enter(LBS_DEB_CFG80211);

	if (!wdev)
		return;

	if (priv->wiphy_registered)
		wiphy_unregister(wdev->wiphy);

	if (wdev->wiphy)
		wiphy_free(wdev->wiphy);

	kfree(wdev);
}
