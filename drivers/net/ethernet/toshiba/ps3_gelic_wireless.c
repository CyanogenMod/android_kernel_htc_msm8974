/*
 *  PS3 gelic network driver.
 *
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * Copyright 2007 Sony Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>

#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <linux/if_arp.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <net/iw_handler.h>

#include <linux/dma-mapping.h>
#include <net/checksum.h>
#include <asm/firmware.h>
#include <asm/ps3.h>
#include <asm/lv1call.h>

#include "ps3_gelic_net.h"
#include "ps3_gelic_wireless.h"


static int gelic_wl_start_scan(struct gelic_wl_info *wl, int always_scan,
			       u8 *essid, size_t essid_len);
static int gelic_wl_try_associate(struct net_device *netdev);


static const int channel_freq[] = {
	2412, 2417, 2422, 2427, 2432,
	2437, 2442, 2447, 2452, 2457,
	2462, 2467, 2472, 2484
};
#define NUM_CHANNELS ARRAY_SIZE(channel_freq)

static const int bitrate_list[] = {
	  1000000,
	  2000000,
	  5500000,
	 11000000,
	  6000000,
	  9000000,
	 12000000,
	 18000000,
	 24000000,
	 36000000,
	 48000000,
	 54000000
};
#define NUM_BITRATES ARRAY_SIZE(bitrate_list)

static inline int wpa2_capable(void)
{
	return 0 <= ps3_compare_firmware_version(2, 0, 0);
}

static inline int precise_ie(void)
{
	return 0 <= ps3_compare_firmware_version(2, 2, 0);
}
struct eurus_cmd_arg_info {
	int pre_arg; 
	int post_arg; 
};

static const struct eurus_cmd_arg_info cmd_info[GELIC_EURUS_CMD_MAX_INDEX] = {
	[GELIC_EURUS_CMD_SET_COMMON_CFG] = { .pre_arg = 1},
	[GELIC_EURUS_CMD_SET_WEP_CFG]    = { .pre_arg = 1},
	[GELIC_EURUS_CMD_SET_WPA_CFG]    = { .pre_arg = 1},
	[GELIC_EURUS_CMD_GET_COMMON_CFG] = { .post_arg = 1},
	[GELIC_EURUS_CMD_GET_WEP_CFG]    = { .post_arg = 1},
	[GELIC_EURUS_CMD_GET_WPA_CFG]    = { .post_arg = 1},
	[GELIC_EURUS_CMD_GET_RSSI_CFG]   = { .post_arg = 1},
	[GELIC_EURUS_CMD_START_SCAN]     = { .pre_arg = 1},
	[GELIC_EURUS_CMD_GET_SCAN]       = { .post_arg = 1},
};

#ifdef DEBUG
static const char *cmdstr(enum gelic_eurus_command ix)
{
	switch (ix) {
	case GELIC_EURUS_CMD_ASSOC:
		return "ASSOC";
	case GELIC_EURUS_CMD_DISASSOC:
		return "DISASSOC";
	case GELIC_EURUS_CMD_START_SCAN:
		return "SCAN";
	case GELIC_EURUS_CMD_GET_SCAN:
		return "GET SCAN";
	case GELIC_EURUS_CMD_SET_COMMON_CFG:
		return "SET_COMMON_CFG";
	case GELIC_EURUS_CMD_GET_COMMON_CFG:
		return "GET_COMMON_CFG";
	case GELIC_EURUS_CMD_SET_WEP_CFG:
		return "SET_WEP_CFG";
	case GELIC_EURUS_CMD_GET_WEP_CFG:
		return "GET_WEP_CFG";
	case GELIC_EURUS_CMD_SET_WPA_CFG:
		return "SET_WPA_CFG";
	case GELIC_EURUS_CMD_GET_WPA_CFG:
		return "GET_WPA_CFG";
	case GELIC_EURUS_CMD_GET_RSSI_CFG:
		return "GET_RSSI";
	default:
		break;
	}
	return "";
};
#else
static inline const char *cmdstr(enum gelic_eurus_command ix)
{
	return "";
}
#endif

static void gelic_eurus_sync_cmd_worker(struct work_struct *work)
{
	struct gelic_eurus_cmd *cmd;
	struct gelic_card *card;
	struct gelic_wl_info *wl;

	u64 arg1, arg2;

	pr_debug("%s: <-\n", __func__);
	cmd = container_of(work, struct gelic_eurus_cmd, work);
	BUG_ON(cmd_info[cmd->cmd].pre_arg &&
	       cmd_info[cmd->cmd].post_arg);
	wl = cmd->wl;
	card = port_to_card(wl_port(wl));

	if (cmd_info[cmd->cmd].pre_arg) {
		arg1 = (cmd->buffer) ?
			ps3_mm_phys_to_lpar(__pa(cmd->buffer)) :
			0;
		arg2 = cmd->buf_size;
	} else {
		arg1 = 0;
		arg2 = 0;
	}
	init_completion(&wl->cmd_done_intr);
	pr_debug("%s: cmd='%s' start\n", __func__, cmdstr(cmd->cmd));
	cmd->status = lv1_net_control(bus_id(card), dev_id(card),
				      GELIC_LV1_POST_WLAN_CMD,
				      cmd->cmd, arg1, arg2,
				      &cmd->tag, &cmd->size);
	if (cmd->status) {
		complete(&cmd->done);
		pr_info("%s: cmd issue failed\n", __func__);
		return;
	}

	wait_for_completion(&wl->cmd_done_intr);

	if (cmd_info[cmd->cmd].post_arg) {
		arg1 = ps3_mm_phys_to_lpar(__pa(cmd->buffer));
		arg2 = cmd->buf_size;
	} else {
		arg1 = 0;
		arg2 = 0;
	}

	cmd->status = lv1_net_control(bus_id(card), dev_id(card),
				      GELIC_LV1_GET_WLAN_CMD_RESULT,
				      cmd->tag, arg1, arg2,
				      &cmd->cmd_status, &cmd->size);
#ifdef DEBUG
	if (cmd->status || cmd->cmd_status) {
	pr_debug("%s: cmd done tag=%#lx arg1=%#lx, arg2=%#lx\n", __func__,
		 cmd->tag, arg1, arg2);
	pr_debug("%s: cmd done status=%#x cmd_status=%#lx size=%#lx\n",
		 __func__, cmd->status, cmd->cmd_status, cmd->size);
	}
#endif
	complete(&cmd->done);
	pr_debug("%s: cmd='%s' done\n", __func__, cmdstr(cmd->cmd));
}

static struct gelic_eurus_cmd *gelic_eurus_sync_cmd(struct gelic_wl_info *wl,
						    unsigned int eurus_cmd,
						    void *buffer,
						    unsigned int buf_size)
{
	struct gelic_eurus_cmd *cmd;

	
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return NULL;

	
	cmd->cmd = eurus_cmd;
	cmd->buffer = buffer;
	cmd->buf_size = buf_size;
	cmd->wl = wl;
	INIT_WORK(&cmd->work, gelic_eurus_sync_cmd_worker);
	init_completion(&cmd->done);
	queue_work(wl->eurus_cmd_queue, &cmd->work);

	
	wait_for_completion(&cmd->done);

	return cmd;
}

static u32 gelic_wl_get_link(struct net_device *netdev)
{
	struct gelic_wl_info *wl = port_wl(netdev_port(netdev));
	u32 ret;

	pr_debug("%s: <-\n", __func__);
	mutex_lock(&wl->assoc_stat_lock);
	if (wl->assoc_stat == GELIC_WL_ASSOC_STAT_ASSOCIATED)
		ret = 1;
	else
		ret = 0;
	mutex_unlock(&wl->assoc_stat_lock);
	pr_debug("%s: ->\n", __func__);
	return ret;
}

static void gelic_wl_send_iwap_event(struct gelic_wl_info *wl, u8 *bssid)
{
	union iwreq_data data;

	memset(&data, 0, sizeof(data));
	if (bssid)
		memcpy(data.ap_addr.sa_data, bssid, ETH_ALEN);
	data.ap_addr.sa_family = ARPHRD_ETHER;
	wireless_send_event(port_to_netdev(wl_port(wl)), SIOCGIWAP,
			    &data, NULL);
}


static int gelic_wl_get_name(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *iwreq, char *extra)
{
	strcpy(iwreq->name, "IEEE 802.11bg");
	return 0;
}

static void gelic_wl_get_ch_info(struct gelic_wl_info *wl)
{
	struct gelic_card *card = port_to_card(wl_port(wl));
	u64 ch_info_raw, tmp;
	int status;

	if (!test_and_set_bit(GELIC_WL_STAT_CH_INFO, &wl->stat)) {
		status = lv1_net_control(bus_id(card), dev_id(card),
					 GELIC_LV1_GET_CHANNEL, 0, 0, 0,
					 &ch_info_raw,
					 &tmp);
		
		if (status) {
			if (status != LV1_NO_ENTRY)
				pr_info("%s: available ch unknown\n", __func__);
			wl->ch_info = 0x07ff;
		} else
			
			wl->ch_info = ch_info_raw >> 48;
	}
}

static int gelic_wl_get_range(struct net_device *netdev,
			      struct iw_request_info *info,
			      union iwreq_data *iwreq, char *extra)
{
	struct iw_point *point = &iwreq->data;
	struct iw_range *range = (struct iw_range *)extra;
	struct gelic_wl_info *wl = port_wl(netdev_port(netdev));
	unsigned int i, chs;

	pr_debug("%s: <-\n", __func__);
	point->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 22;

	
	gelic_wl_get_ch_info(wl);

	for (i = 0, chs = 0;
	     i < NUM_CHANNELS && chs < IW_MAX_FREQUENCIES; i++)
		if (wl->ch_info & (1 << i)) {
			range->freq[chs].i = i + 1;
			range->freq[chs].m = channel_freq[i];
			range->freq[chs].e = 6;
			chs++;
		}
	range->num_frequency = chs;
	range->old_num_frequency = chs;
	range->num_channels = chs;
	range->old_num_channels = chs;

	
	for (i = 0; i < NUM_BITRATES; i++)
		range->bitrate[i] = bitrate_list[i];
	range->num_bitrates = i;

	
	range->max_qual.qual = 100; 
	range->max_qual.level = 100;
	range->avg_qual.qual = 50;
	range->avg_qual.level = 50;
	range->sensitivity = 0;

	
	IW_EVENT_CAPA_SET_KERNEL(range->event_capa);
	IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWAP);
	IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWSCAN);

	
	range->enc_capa = IW_ENC_CAPA_WPA |
		IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP |
		IW_ENC_CAPA_4WAY_HANDSHAKE;
	if (wpa2_capable())
		range->enc_capa |= IW_ENC_CAPA_WPA2;
	range->encoding_size[0] = 5;	
	range->encoding_size[1] = 13;	
	range->encoding_size[2] = 32;	
	range->num_encoding_sizes = 3;
	range->max_encoding_tokens = GELIC_WEP_KEYS;

	
	range->scan_capa = IW_SCAN_CAPA_ESSID;

	pr_debug("%s: ->\n", __func__);
	return 0;

}

static int gelic_wl_set_scan(struct net_device *netdev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	struct iw_scan_req *req;
	u8 *essid = NULL;
	size_t essid_len = 0;

	if (wrqu->data.length == sizeof(struct iw_scan_req) &&
	    wrqu->data.flags & IW_SCAN_THIS_ESSID) {
		req = (struct iw_scan_req*)extra;
		essid = req->essid;
		essid_len = req->essid_len;
		pr_debug("%s: ESSID scan =%s\n", __func__, essid);
	}
	return gelic_wl_start_scan(wl, 1, essid, essid_len);
}

#define OUI_LEN 3
static const u8 rsn_oui[OUI_LEN] = { 0x00, 0x0f, 0xac };
static const u8 wpa_oui[OUI_LEN] = { 0x00, 0x50, 0xf2 };

static size_t gelic_wl_synthesize_ie(u8 *buf,
				     struct gelic_eurus_scan_info *scan)
{

	const u8 *oui_header;
	u8 *start = buf;
	int rsn;
	int ccmp;

	pr_debug("%s: <- sec=%16x\n", __func__, scan->security);
	switch (be16_to_cpu(scan->security) & GELIC_EURUS_SCAN_SEC_MASK) {
	case GELIC_EURUS_SCAN_SEC_WPA:
		rsn = 0;
		break;
	case GELIC_EURUS_SCAN_SEC_WPA2:
		rsn = 1;
		break;
	default:
		
		return 0;
	}

	switch (be16_to_cpu(scan->security) & GELIC_EURUS_SCAN_SEC_WPA_MASK) {
	case GELIC_EURUS_SCAN_SEC_WPA_TKIP:
		ccmp = 0;
		break;
	case GELIC_EURUS_SCAN_SEC_WPA_AES:
		ccmp = 1;
		break;
	default:
		if (rsn) {
			ccmp = 1;
			pr_info("%s: no cipher info. defaulted to CCMP\n",
				__func__);
		} else {
			ccmp = 0;
			pr_info("%s: no cipher info. defaulted to TKIP\n",
				__func__);
		}
	}

	if (rsn)
		oui_header = rsn_oui;
	else
		oui_header = wpa_oui;

	
	if (rsn)
		*buf++ = WLAN_EID_RSN;
	else
		*buf++ = WLAN_EID_GENERIC;

	
	buf++;

	
	if (!rsn) {
		memcpy(buf, wpa_oui, OUI_LEN);
		buf += OUI_LEN;
		*buf++ = 0x01;
	}

	
	*buf++ = 0x01; 
	*buf++ = 0x00;

	
	memcpy(buf, oui_header, OUI_LEN);
	buf += OUI_LEN;

	if (ccmp)
		*buf++ = 0x04; 
	else
		*buf++ = 0x02; 

	
	*buf++ = 0x01;
	*buf++ = 0x00;

	
	memcpy(buf, oui_header, OUI_LEN);
	buf += OUI_LEN;
	if (ccmp)
		*buf++ = 0x04; 
	else
		*buf++ = 0x02; 

	
	*buf++ = 0x01;
	*buf++ = 0x00;

	
	memcpy(buf, oui_header, OUI_LEN);
	buf += OUI_LEN;
	*buf++ = 0x02; 

	
	*buf++ = 0x00;
	*buf++ = 0x00;

	
	start[1] = (buf - start - 2);

	pr_debug("%s: ->\n", __func__);
	return buf - start;
}

struct ie_item {
	u8 *data;
	u8 len;
};

struct ie_info {
	struct ie_item wpa;
	struct ie_item rsn;
};

static void gelic_wl_parse_ie(u8 *data, size_t len,
			      struct ie_info *ie_info)
{
	size_t data_left = len;
	u8 *pos = data;
	u8 item_len;
	u8 item_id;

	pr_debug("%s: data=%p len=%ld\n", __func__,
		 data, len);
	memset(ie_info, 0, sizeof(struct ie_info));

	while (2 <= data_left) {
		item_id = *pos++;
		item_len = *pos++;
		data_left -= 2;

		if (data_left < item_len)
			break;

		switch (item_id) {
		case WLAN_EID_GENERIC:
			if ((OUI_LEN + 1 <= item_len) &&
			    !memcmp(pos, wpa_oui, OUI_LEN) &&
			    pos[OUI_LEN] == 0x01) {
				ie_info->wpa.data = pos - 2;
				ie_info->wpa.len = item_len + 2;
			}
			break;
		case WLAN_EID_RSN:
			ie_info->rsn.data = pos - 2;
			
			ie_info->rsn.len = item_len + 2;
			break;
		default:
			pr_debug("%s: ignore %#x,%d\n", __func__,
				 item_id, item_len);
			break;
		}
		pos += item_len;
		data_left -= item_len;
	}
	pr_debug("%s: wpa=%p,%d wpa2=%p,%d\n", __func__,
		 ie_info->wpa.data, ie_info->wpa.len,
		 ie_info->rsn.data, ie_info->rsn.len);
}


static char *gelic_wl_translate_scan(struct net_device *netdev,
				     struct iw_request_info *info,
				     char *ev,
				     char *stop,
				     struct gelic_wl_scan_info *network)
{
	struct iw_event iwe;
	struct gelic_eurus_scan_info *scan = network->hwinfo;
	char *tmp;
	u8 rate;
	unsigned int i, j, len;
	u8 buf[64]; 

	pr_debug("%s: <-\n", __func__);

	
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
	memcpy(iwe.u.ap_addr.sa_data, &scan->bssid[2], ETH_ALEN);
	ev = iwe_stream_add_event(info, ev, stop, &iwe, IW_EV_ADDR_LEN);

	
	iwe.cmd = SIOCGIWESSID;
	iwe.u.data.flags = 1;
	iwe.u.data.length = strnlen(scan->essid, 32);
	ev = iwe_stream_add_point(info, ev, stop, &iwe, scan->essid);

	
	iwe.cmd = SIOCGIWFREQ;
	iwe.u.freq.m = be16_to_cpu(scan->channel);
	iwe.u.freq.e = 0; 
	iwe.u.freq.i = 0;
	ev = iwe_stream_add_event(info, ev, stop, &iwe, IW_EV_FREQ_LEN);

	
	iwe.cmd = SIOCGIWRATE;
	iwe.u.bitrate.fixed = iwe.u.bitrate.disabled = 0;
	
	tmp = ev + iwe_stream_lcp_len(info);
	
	i = 0;
	j = 0;
	pr_debug("%s: rates=%d rate=%d\n", __func__,
		 network->rate_len, network->rate_ext_len);
	while (i < network->rate_len) {
		if (j < network->rate_ext_len &&
		    ((scan->ext_rate[j] & 0x7f) < (scan->rate[i] & 0x7f)))
		    rate = scan->ext_rate[j++] & 0x7f;
		else
		    rate = scan->rate[i++] & 0x7f;
		iwe.u.bitrate.value = rate * 500000; 
		tmp = iwe_stream_add_value(info, ev, tmp, stop, &iwe,
					   IW_EV_PARAM_LEN);
	}
	while (j < network->rate_ext_len) {
		iwe.u.bitrate.value = (scan->ext_rate[j++] & 0x7f) * 500000;
		tmp = iwe_stream_add_value(info, ev, tmp, stop, &iwe,
					   IW_EV_PARAM_LEN);
	}
	
	if (iwe_stream_lcp_len(info) < (tmp - ev))
		ev = tmp;

	
	iwe.cmd = SIOCGIWENCODE;
	if (be16_to_cpu(scan->capability) & WLAN_CAPABILITY_PRIVACY)
		iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	else
		iwe.u.data.flags = IW_ENCODE_DISABLED;
	iwe.u.data.length = 0;
	ev = iwe_stream_add_point(info, ev, stop, &iwe, scan->essid);

	
	iwe.cmd = SIOCGIWMODE;
	if (be16_to_cpu(scan->capability) &
	    (WLAN_CAPABILITY_ESS | WLAN_CAPABILITY_IBSS)) {
		if (be16_to_cpu(scan->capability) & WLAN_CAPABILITY_ESS)
			iwe.u.mode = IW_MODE_MASTER;
		else
			iwe.u.mode = IW_MODE_ADHOC;
		ev = iwe_stream_add_event(info, ev, stop, &iwe, IW_EV_UINT_LEN);
	}

	
	iwe.cmd = IWEVQUAL;
	iwe.u.qual.updated  = IW_QUAL_ALL_UPDATED |
			IW_QUAL_QUAL_INVALID | IW_QUAL_NOISE_INVALID;
	iwe.u.qual.level = be16_to_cpu(scan->rssi);
	iwe.u.qual.qual = be16_to_cpu(scan->rssi);
	iwe.u.qual.noise = 0;
	ev  = iwe_stream_add_event(info, ev, stop, &iwe, IW_EV_QUAL_LEN);

	
	memset(&iwe, 0, sizeof(iwe));
	if (be16_to_cpu(scan->size) <= sizeof(*scan)) {
		
		len = gelic_wl_synthesize_ie(buf, scan);
		if (len) {
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = len;
			ev = iwe_stream_add_point(info, ev, stop, &iwe, buf);
		}
	} else {
		
		struct ie_info ie_info;
		size_t data_len;

		data_len = be16_to_cpu(scan->size) - sizeof(*scan);

		gelic_wl_parse_ie(scan->elements, data_len, &ie_info);

		if (ie_info.wpa.len && (ie_info.wpa.len <= sizeof(buf))) {
			memcpy(buf, ie_info.wpa.data, ie_info.wpa.len);
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = ie_info.wpa.len;
			ev = iwe_stream_add_point(info, ev, stop, &iwe, buf);
		}

		if (ie_info.rsn.len && (ie_info.rsn.len <= sizeof(buf))) {
			memset(&iwe, 0, sizeof(iwe));
			memcpy(buf, ie_info.rsn.data, ie_info.rsn.len);
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = ie_info.rsn.len;
			ev = iwe_stream_add_point(info, ev, stop, &iwe, buf);
		}
	}

	pr_debug("%s: ->\n", __func__);
	return ev;
}


static int gelic_wl_get_scan(struct net_device *netdev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	struct gelic_wl_scan_info *scan_info;
	char *ev = extra;
	char *stop = ev + wrqu->data.length;
	int ret = 0;
	unsigned long this_time = jiffies;

	pr_debug("%s: <-\n", __func__);
	if (mutex_lock_interruptible(&wl->scan_lock))
		return -EAGAIN;

	switch (wl->scan_stat) {
	case GELIC_WL_SCAN_STAT_SCANNING:
		
		ret = -EAGAIN;
		goto out;
		break;

	case GELIC_WL_SCAN_STAT_INIT:
		
		ret = -ENODEV;
		goto out;
		break;
	case GELIC_WL_SCAN_STAT_GOT_LIST:
		
		break;
	}

	list_for_each_entry(scan_info, &wl->network_list, list) {
		if (wl->scan_age == 0 ||
		    time_after(scan_info->last_scanned + wl->scan_age,
			       this_time))
			ev = gelic_wl_translate_scan(netdev, info,
						     ev, stop,
						     scan_info);
		else
			pr_debug("%s:entry too old\n", __func__);

		if (stop - ev <= IW_EV_ADDR_LEN) {
			ret = -E2BIG;
			goto out;
		}
	}

	wrqu->data.length = ev - extra;
	wrqu->data.flags = 0;
out:
	mutex_unlock(&wl->scan_lock);
	pr_debug("%s: -> %d %d\n", __func__, ret, wrqu->data.length);
	return ret;
}

#ifdef DEBUG
static void scan_list_dump(struct gelic_wl_info *wl)
{
	struct gelic_wl_scan_info *scan_info;
	int i;

	i = 0;
	list_for_each_entry(scan_info, &wl->network_list, list) {
		pr_debug("%s: item %d\n", __func__, i++);
		pr_debug("valid=%d eurusindex=%d last=%lx\n",
			 scan_info->valid, scan_info->eurus_index,
			 scan_info->last_scanned);
		pr_debug("r_len=%d r_ext_len=%d essid_len=%d\n",
			 scan_info->rate_len, scan_info->rate_ext_len,
			 scan_info->essid_len);
		
		pr_debug("bssid=%pM\n", &scan_info->hwinfo->bssid[2]);
		pr_debug("essid=%s\n", scan_info->hwinfo->essid);
	}
}
#endif

static int gelic_wl_set_auth(struct net_device *netdev,
			     struct iw_request_info *info,
			     union iwreq_data *data, char *extra)
{
	struct iw_param *param = &data->param;
	struct gelic_wl_info *wl = port_wl(netdev_port(netdev));
	unsigned long irqflag;
	int ret = 0;

	pr_debug("%s: <- %d\n", __func__, param->flags & IW_AUTH_INDEX);
	spin_lock_irqsave(&wl->lock, irqflag);
	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
		if (param->value & IW_AUTH_WPA_VERSION_DISABLED) {
			pr_debug("%s: NO WPA selected\n", __func__);
			wl->wpa_level = GELIC_WL_WPA_LEVEL_NONE;
			wl->group_cipher_method = GELIC_WL_CIPHER_WEP;
			wl->pairwise_cipher_method = GELIC_WL_CIPHER_WEP;
		}
		if (param->value & IW_AUTH_WPA_VERSION_WPA) {
			pr_debug("%s: WPA version 1 selected\n", __func__);
			wl->wpa_level = GELIC_WL_WPA_LEVEL_WPA;
			wl->group_cipher_method = GELIC_WL_CIPHER_TKIP;
			wl->pairwise_cipher_method = GELIC_WL_CIPHER_TKIP;
			wl->auth_method = GELIC_EURUS_AUTH_OPEN;
		}
		if (param->value & IW_AUTH_WPA_VERSION_WPA2) {
			if (!precise_ie())
				pr_info("%s: WPA2 may not work\n", __func__);
			if (wpa2_capable()) {
				wl->wpa_level = GELIC_WL_WPA_LEVEL_WPA2;
				wl->group_cipher_method = GELIC_WL_CIPHER_AES;
				wl->pairwise_cipher_method =
					GELIC_WL_CIPHER_AES;
				wl->auth_method = GELIC_EURUS_AUTH_OPEN;
			} else
				ret = -EINVAL;
		}
		break;

	case IW_AUTH_CIPHER_PAIRWISE:
		if (param->value &
		    (IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40)) {
			pr_debug("%s: WEP selected\n", __func__);
			wl->pairwise_cipher_method = GELIC_WL_CIPHER_WEP;
		}
		if (param->value & IW_AUTH_CIPHER_TKIP) {
			pr_debug("%s: TKIP selected\n", __func__);
			wl->pairwise_cipher_method = GELIC_WL_CIPHER_TKIP;
		}
		if (param->value & IW_AUTH_CIPHER_CCMP) {
			pr_debug("%s: CCMP selected\n", __func__);
			wl->pairwise_cipher_method = GELIC_WL_CIPHER_AES;
		}
		if (param->value & IW_AUTH_CIPHER_NONE) {
			pr_debug("%s: no auth selected\n", __func__);
			wl->pairwise_cipher_method = GELIC_WL_CIPHER_NONE;
		}
		break;
	case IW_AUTH_CIPHER_GROUP:
		if (param->value &
		    (IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40)) {
			pr_debug("%s: WEP selected\n", __func__);
			wl->group_cipher_method = GELIC_WL_CIPHER_WEP;
		}
		if (param->value & IW_AUTH_CIPHER_TKIP) {
			pr_debug("%s: TKIP selected\n", __func__);
			wl->group_cipher_method = GELIC_WL_CIPHER_TKIP;
		}
		if (param->value & IW_AUTH_CIPHER_CCMP) {
			pr_debug("%s: CCMP selected\n", __func__);
			wl->group_cipher_method = GELIC_WL_CIPHER_AES;
		}
		if (param->value & IW_AUTH_CIPHER_NONE) {
			pr_debug("%s: no auth selected\n", __func__);
			wl->group_cipher_method = GELIC_WL_CIPHER_NONE;
		}
		break;
	case IW_AUTH_80211_AUTH_ALG:
		if (param->value & IW_AUTH_ALG_SHARED_KEY) {
			pr_debug("%s: shared key specified\n", __func__);
			wl->auth_method = GELIC_EURUS_AUTH_SHARED;
		} else if (param->value & IW_AUTH_ALG_OPEN_SYSTEM) {
			pr_debug("%s: open system specified\n", __func__);
			wl->auth_method = GELIC_EURUS_AUTH_OPEN;
		} else
			ret = -EINVAL;
		break;

	case IW_AUTH_WPA_ENABLED:
		if (param->value) {
			pr_debug("%s: WPA enabled\n", __func__);
			wl->wpa_level = GELIC_WL_WPA_LEVEL_WPA;
		} else {
			pr_debug("%s: WPA disabled\n", __func__);
			wl->wpa_level = GELIC_WL_WPA_LEVEL_NONE;
		}
		break;

	case IW_AUTH_KEY_MGMT:
		if (param->value & IW_AUTH_KEY_MGMT_PSK)
			break;
		
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	if (!ret)
		set_bit(GELIC_WL_STAT_CONFIGURED, &wl->stat);

	spin_unlock_irqrestore(&wl->lock, irqflag);
	pr_debug("%s: -> %d\n", __func__, ret);
	return ret;
}

static int gelic_wl_get_auth(struct net_device *netdev,
			     struct iw_request_info *info,
			     union iwreq_data *iwreq, char *extra)
{
	struct iw_param *param = &iwreq->param;
	struct gelic_wl_info *wl = port_wl(netdev_port(netdev));
	unsigned long irqflag;
	int ret = 0;

	pr_debug("%s: <- %d\n", __func__, param->flags & IW_AUTH_INDEX);
	spin_lock_irqsave(&wl->lock, irqflag);
	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
		switch (wl->wpa_level) {
		case GELIC_WL_WPA_LEVEL_WPA:
			param->value |= IW_AUTH_WPA_VERSION_WPA;
			break;
		case GELIC_WL_WPA_LEVEL_WPA2:
			param->value |= IW_AUTH_WPA_VERSION_WPA2;
			break;
		default:
			param->value |= IW_AUTH_WPA_VERSION_DISABLED;
		}
		break;

	case IW_AUTH_80211_AUTH_ALG:
		if (wl->auth_method == GELIC_EURUS_AUTH_SHARED)
			param->value = IW_AUTH_ALG_SHARED_KEY;
		else if (wl->auth_method == GELIC_EURUS_AUTH_OPEN)
			param->value = IW_AUTH_ALG_OPEN_SYSTEM;
		break;

	case IW_AUTH_WPA_ENABLED:
		switch (wl->wpa_level) {
		case GELIC_WL_WPA_LEVEL_WPA:
		case GELIC_WL_WPA_LEVEL_WPA2:
			param->value = 1;
			break;
		default:
			param->value = 0;
			break;
		}
		break;
	default:
		ret = -EOPNOTSUPP;
	}

	spin_unlock_irqrestore(&wl->lock, irqflag);
	pr_debug("%s: -> %d\n", __func__, ret);
	return ret;
}

static int gelic_wl_set_essid(struct net_device *netdev,
			      struct iw_request_info *info,
			      union iwreq_data *data, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	unsigned long irqflag;

	pr_debug("%s: <- l=%d f=%d\n", __func__,
		 data->essid.length, data->essid.flags);
	if (IW_ESSID_MAX_SIZE < data->essid.length)
		return -EINVAL;

	spin_lock_irqsave(&wl->lock, irqflag);
	if (data->essid.flags) {
		wl->essid_len = data->essid.length;
		memcpy(wl->essid, extra, wl->essid_len);
		pr_debug("%s: essid = '%s'\n", __func__, extra);
		set_bit(GELIC_WL_STAT_ESSID_SET, &wl->stat);
	} else {
		pr_debug("%s: ESSID any\n", __func__);
		clear_bit(GELIC_WL_STAT_ESSID_SET, &wl->stat);
	}
	set_bit(GELIC_WL_STAT_CONFIGURED, &wl->stat);
	spin_unlock_irqrestore(&wl->lock, irqflag);


	gelic_wl_try_associate(netdev); 
	pr_debug("%s: ->\n", __func__);
	return 0;
}

static int gelic_wl_get_essid(struct net_device *netdev,
			      struct iw_request_info *info,
			      union iwreq_data *data, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	unsigned long irqflag;

	pr_debug("%s: <-\n", __func__);
	mutex_lock(&wl->assoc_stat_lock);
	spin_lock_irqsave(&wl->lock, irqflag);
	if (test_bit(GELIC_WL_STAT_ESSID_SET, &wl->stat) ||
	    wl->assoc_stat == GELIC_WL_ASSOC_STAT_ASSOCIATED) {
		memcpy(extra, wl->essid, wl->essid_len);
		data->essid.length = wl->essid_len;
		data->essid.flags = 1;
	} else
		data->essid.flags = 0;

	mutex_unlock(&wl->assoc_stat_lock);
	spin_unlock_irqrestore(&wl->lock, irqflag);
	pr_debug("%s: -> len=%d\n", __func__, data->essid.length);

	return 0;
}

static int gelic_wl_set_encode(struct net_device *netdev,
			       struct iw_request_info *info,
			       union iwreq_data *data, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	struct iw_point *enc = &data->encoding;
	__u16 flags;
	unsigned long irqflag;
	int key_index, index_specified;
	int ret = 0;

	pr_debug("%s: <-\n", __func__);
	flags = enc->flags & IW_ENCODE_FLAGS;
	key_index = enc->flags & IW_ENCODE_INDEX;

	pr_debug("%s: key_index = %d\n", __func__, key_index);
	pr_debug("%s: key_len = %d\n", __func__, enc->length);
	pr_debug("%s: flag=%x\n", __func__, enc->flags & IW_ENCODE_FLAGS);

	if (GELIC_WEP_KEYS < key_index)
		return -EINVAL;

	spin_lock_irqsave(&wl->lock, irqflag);
	if (key_index) {
		index_specified = 1;
		key_index--;
	} else {
		index_specified = 0;
		key_index = wl->current_key;
	}

	if (flags & IW_ENCODE_NOKEY) {
		
		if (!flags && index_specified) {
			wl->current_key = key_index;
			goto done;
		}

		if (flags & IW_ENCODE_DISABLED) {
			if (!index_specified) {
				
				wl->group_cipher_method = GELIC_WL_CIPHER_NONE;
				wl->pairwise_cipher_method =
					GELIC_WL_CIPHER_NONE;
				
				wl->key_enabled = 0;
			} else
				clear_bit(key_index, &wl->key_enabled);
		}

		if (flags & IW_ENCODE_OPEN)
			wl->auth_method = GELIC_EURUS_AUTH_OPEN;
		if (flags & IW_ENCODE_RESTRICTED) {
			pr_info("%s: shared key mode enabled\n", __func__);
			wl->auth_method = GELIC_EURUS_AUTH_SHARED;
		}
	} else {
		if (IW_ENCODING_TOKEN_MAX < enc->length) {
			ret = -EINVAL;
			goto done;
		}
		wl->key_len[key_index] = enc->length;
		memcpy(wl->key[key_index], extra, enc->length);
		set_bit(key_index, &wl->key_enabled);
		wl->pairwise_cipher_method = GELIC_WL_CIPHER_WEP;
		wl->group_cipher_method = GELIC_WL_CIPHER_WEP;
	}
	set_bit(GELIC_WL_STAT_CONFIGURED, &wl->stat);
done:
	spin_unlock_irqrestore(&wl->lock, irqflag);
	pr_debug("%s: ->\n", __func__);
	return ret;
}

static int gelic_wl_get_encode(struct net_device *netdev,
			       struct iw_request_info *info,
			       union iwreq_data *data, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	struct iw_point *enc = &data->encoding;
	unsigned long irqflag;
	unsigned int key_index, index_specified;
	int ret = 0;

	pr_debug("%s: <-\n", __func__);
	key_index = enc->flags & IW_ENCODE_INDEX;
	pr_debug("%s: flag=%#x point=%p len=%d extra=%p\n", __func__,
		 enc->flags, enc->pointer, enc->length, extra);
	if (GELIC_WEP_KEYS < key_index)
		return -EINVAL;

	spin_lock_irqsave(&wl->lock, irqflag);
	if (key_index) {
		index_specified = 1;
		key_index--;
	} else {
		index_specified = 0;
		key_index = wl->current_key;
	}

	if (wl->group_cipher_method == GELIC_WL_CIPHER_WEP) {
		switch (wl->auth_method) {
		case GELIC_EURUS_AUTH_OPEN:
			enc->flags = IW_ENCODE_OPEN;
			break;
		case GELIC_EURUS_AUTH_SHARED:
			enc->flags = IW_ENCODE_RESTRICTED;
			break;
		}
	} else
		enc->flags = IW_ENCODE_DISABLED;

	if (test_bit(key_index, &wl->key_enabled)) {
		if (enc->length < wl->key_len[key_index]) {
			ret = -EINVAL;
			goto done;
		}
		enc->length = wl->key_len[key_index];
		memcpy(extra, wl->key[key_index], wl->key_len[key_index]);
	} else {
		enc->length = 0;
		enc->flags |= IW_ENCODE_NOKEY;
	}
	enc->flags |= key_index + 1;
	pr_debug("%s: -> flag=%x len=%d\n", __func__,
		 enc->flags, enc->length);

done:
	spin_unlock_irqrestore(&wl->lock, irqflag);
	return ret;
}

static int gelic_wl_set_ap(struct net_device *netdev,
			   struct iw_request_info *info,
			   union iwreq_data *data, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	unsigned long irqflag;

	pr_debug("%s: <-\n", __func__);
	if (data->ap_addr.sa_family != ARPHRD_ETHER)
		return -EINVAL;

	spin_lock_irqsave(&wl->lock, irqflag);
	if (is_valid_ether_addr(data->ap_addr.sa_data)) {
		memcpy(wl->bssid, data->ap_addr.sa_data,
		       ETH_ALEN);
		set_bit(GELIC_WL_STAT_BSSID_SET, &wl->stat);
		set_bit(GELIC_WL_STAT_CONFIGURED, &wl->stat);
		pr_debug("%s: bss=%pM\n", __func__, wl->bssid);
	} else {
		pr_debug("%s: clear bssid\n", __func__);
		clear_bit(GELIC_WL_STAT_BSSID_SET, &wl->stat);
		memset(wl->bssid, 0, ETH_ALEN);
	}
	spin_unlock_irqrestore(&wl->lock, irqflag);
	pr_debug("%s: ->\n", __func__);
	return 0;
}

static int gelic_wl_get_ap(struct net_device *netdev,
			   struct iw_request_info *info,
			   union iwreq_data *data, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	unsigned long irqflag;

	pr_debug("%s: <-\n", __func__);
	mutex_lock(&wl->assoc_stat_lock);
	spin_lock_irqsave(&wl->lock, irqflag);
	if (wl->assoc_stat == GELIC_WL_ASSOC_STAT_ASSOCIATED) {
		data->ap_addr.sa_family = ARPHRD_ETHER;
		memcpy(data->ap_addr.sa_data, wl->active_bssid,
		       ETH_ALEN);
	} else
		memset(data->ap_addr.sa_data, 0, ETH_ALEN);

	spin_unlock_irqrestore(&wl->lock, irqflag);
	mutex_unlock(&wl->assoc_stat_lock);
	pr_debug("%s: ->\n", __func__);
	return 0;
}

static int gelic_wl_set_encodeext(struct net_device *netdev,
				  struct iw_request_info *info,
				  union iwreq_data *data, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	struct iw_point *enc = &data->encoding;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	__u16 alg;
	__u16 flags;
	unsigned long irqflag;
	int key_index;
	int ret = 0;

	pr_debug("%s: <-\n", __func__);
	flags = enc->flags & IW_ENCODE_FLAGS;
	alg = ext->alg;
	key_index = enc->flags & IW_ENCODE_INDEX;

	pr_debug("%s: key_index = %d\n", __func__, key_index);
	pr_debug("%s: key_len = %d\n", __func__, enc->length);
	pr_debug("%s: flag=%x\n", __func__, enc->flags & IW_ENCODE_FLAGS);
	pr_debug("%s: ext_flag=%x\n", __func__, ext->ext_flags);
	pr_debug("%s: ext_key_len=%x\n", __func__, ext->key_len);

	if (GELIC_WEP_KEYS < key_index)
		return -EINVAL;

	spin_lock_irqsave(&wl->lock, irqflag);
	if (key_index)
		key_index--;
	else
		key_index = wl->current_key;

	if (!enc->length && (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)) {
		
		pr_debug("%s: request to change default key to %d\n",
			 __func__, key_index);
		wl->current_key = key_index;
		goto done;
	}

	if (alg == IW_ENCODE_ALG_NONE || (flags & IW_ENCODE_DISABLED)) {
		pr_debug("%s: alg disabled\n", __func__);
		wl->wpa_level = GELIC_WL_WPA_LEVEL_NONE;
		wl->group_cipher_method = GELIC_WL_CIPHER_NONE;
		wl->pairwise_cipher_method = GELIC_WL_CIPHER_NONE;
		wl->auth_method = GELIC_EURUS_AUTH_OPEN; 
	} else if (alg == IW_ENCODE_ALG_WEP) {
		pr_debug("%s: WEP requested\n", __func__);
		if (flags & IW_ENCODE_OPEN) {
			pr_debug("%s: open key mode\n", __func__);
			wl->auth_method = GELIC_EURUS_AUTH_OPEN;
		}
		if (flags & IW_ENCODE_RESTRICTED) {
			pr_debug("%s: shared key mode\n", __func__);
			wl->auth_method = GELIC_EURUS_AUTH_SHARED;
		}
		if (IW_ENCODING_TOKEN_MAX < ext->key_len) {
			pr_info("%s: key is too long %d\n", __func__,
				ext->key_len);
			ret = -EINVAL;
			goto done;
		}
		
		wl->key_len[key_index] = ext->key_len;
		memset(wl->key[key_index], 0, IW_ENCODING_TOKEN_MAX);
		memcpy(wl->key[key_index], ext->key, ext->key_len);
		set_bit(key_index, &wl->key_enabled);
		
		set_bit(GELIC_WL_STAT_CONFIGURED, &wl->stat);
	} else if (alg == IW_ENCODE_ALG_PMK) {
		if (ext->key_len != WPA_PSK_LEN) {
			pr_err("%s: PSK length wrong %d\n", __func__,
			       ext->key_len);
			ret = -EINVAL;
			goto done;
		}
		memset(wl->psk, 0, sizeof(wl->psk));
		memcpy(wl->psk, ext->key, ext->key_len);
		wl->psk_len = ext->key_len;
		wl->psk_type = GELIC_EURUS_WPA_PSK_BIN;
		
		set_bit(GELIC_WL_STAT_WPA_PSK_SET, &wl->stat);
	}
done:
	spin_unlock_irqrestore(&wl->lock, irqflag);
	pr_debug("%s: ->\n", __func__);
	return ret;
}

static int gelic_wl_get_encodeext(struct net_device *netdev,
				  struct iw_request_info *info,
				  union iwreq_data *data, char *extra)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	struct iw_point *enc = &data->encoding;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	unsigned long irqflag;
	int key_index;
	int ret = 0;
	int max_key_len;

	pr_debug("%s: <-\n", __func__);

	max_key_len = enc->length - sizeof(struct iw_encode_ext);
	if (max_key_len < 0)
		return -EINVAL;
	key_index = enc->flags & IW_ENCODE_INDEX;

	pr_debug("%s: key_index = %d\n", __func__, key_index);
	pr_debug("%s: key_len = %d\n", __func__, enc->length);
	pr_debug("%s: flag=%x\n", __func__, enc->flags & IW_ENCODE_FLAGS);

	if (GELIC_WEP_KEYS < key_index)
		return -EINVAL;

	spin_lock_irqsave(&wl->lock, irqflag);
	if (key_index)
		key_index--;
	else
		key_index = wl->current_key;

	memset(ext, 0, sizeof(struct iw_encode_ext));
	switch (wl->group_cipher_method) {
	case GELIC_WL_CIPHER_WEP:
		ext->alg = IW_ENCODE_ALG_WEP;
		enc->flags |= IW_ENCODE_ENABLED;
		break;
	case GELIC_WL_CIPHER_TKIP:
		ext->alg = IW_ENCODE_ALG_TKIP;
		enc->flags |= IW_ENCODE_ENABLED;
		break;
	case GELIC_WL_CIPHER_AES:
		ext->alg = IW_ENCODE_ALG_CCMP;
		enc->flags |= IW_ENCODE_ENABLED;
		break;
	case GELIC_WL_CIPHER_NONE:
	default:
		ext->alg = IW_ENCODE_ALG_NONE;
		enc->flags |= IW_ENCODE_NOKEY;
		break;
	}

	if (!(enc->flags & IW_ENCODE_NOKEY)) {
		if (max_key_len < wl->key_len[key_index]) {
			ret = -E2BIG;
			goto out;
		}
		if (test_bit(key_index, &wl->key_enabled))
			memcpy(ext->key, wl->key[key_index],
			       wl->key_len[key_index]);
		else
			pr_debug("%s: disabled key requested ix=%d\n",
				 __func__, key_index);
	}
out:
	spin_unlock_irqrestore(&wl->lock, irqflag);
	pr_debug("%s: ->\n", __func__);
	return ret;
}
static int gelic_wl_set_mode(struct net_device *netdev,
			     struct iw_request_info *info,
			     union iwreq_data *data, char *extra)
{
	__u32 mode = data->mode;
	int ret;

	pr_debug("%s: <-\n", __func__);
	if (mode == IW_MODE_INFRA)
		ret = 0;
	else
		ret = -EOPNOTSUPP;
	pr_debug("%s: -> %d\n", __func__, ret);
	return ret;
}

static int gelic_wl_get_mode(struct net_device *netdev,
			     struct iw_request_info *info,
			     union iwreq_data *data, char *extra)
{
	__u32 *mode = &data->mode;
	pr_debug("%s: <-\n", __func__);
	*mode = IW_MODE_INFRA;
	pr_debug("%s: ->\n", __func__);
	return 0;
}

static int gelic_wl_get_nick(struct net_device *net_dev,
				  struct iw_request_info *info,
				  union iwreq_data *data, char *extra)
{
	strcpy(extra, "gelic_wl");
	data->data.length = strlen(extra);
	data->data.flags = 1;
	return 0;
}



static struct iw_statistics *gelic_wl_get_wireless_stats(
	struct net_device *netdev)
{

	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	struct gelic_eurus_cmd *cmd;
	struct iw_statistics *is;
	struct gelic_eurus_rssi_info *rssi;
	void *buf;

	pr_debug("%s: <-\n", __func__);

	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return NULL;

	is = &wl->iwstat;
	memset(is, 0, sizeof(*is));
	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_GET_RSSI_CFG,
				   buf, sizeof(*rssi));
	if (cmd && !cmd->status && !cmd->cmd_status) {
		rssi = buf;
		is->qual.level = be16_to_cpu(rssi->rssi);
		is->qual.updated = IW_QUAL_LEVEL_UPDATED |
			IW_QUAL_QUAL_INVALID | IW_QUAL_NOISE_INVALID;
	} else
		
		is->qual.updated = IW_QUAL_ALL_INVALID;

	kfree(cmd);
	free_page((unsigned long)buf);
	pr_debug("%s: ->\n", __func__);
	return is;
}

static int gelic_wl_start_scan(struct gelic_wl_info *wl, int always_scan,
			       u8 *essid, size_t essid_len)
{
	struct gelic_eurus_cmd *cmd;
	int ret = 0;
	void *buf = NULL;
	size_t len;

	pr_debug("%s: <- always=%d\n", __func__, always_scan);
	if (mutex_lock_interruptible(&wl->scan_lock))
		return -ERESTARTSYS;

	if (wl->scan_stat == GELIC_WL_SCAN_STAT_SCANNING) {
		pr_debug("%s: scanning now\n", __func__);
		goto out;
	}

	init_completion(&wl->scan_done);
	if ((!essid_len && !always_scan)
	    && wl->scan_stat == GELIC_WL_SCAN_STAT_GOT_LIST) {
		pr_debug("%s: already has the list\n", __func__);
		complete(&wl->scan_done);
		goto out;
	}

	
	if (essid_len && essid) {
		buf = (void *)__get_free_page(GFP_KERNEL);
		if (!buf) {
			ret = -ENOMEM;
			goto out;
		}
		len = IW_ESSID_MAX_SIZE; 
		memset(buf, 0, len);
		memcpy(buf, essid, essid_len);
		pr_debug("%s: essid scan='%s'\n", __func__, (char *)buf);
	} else
		len = 0;

	wl->scan_stat = GELIC_WL_SCAN_STAT_SCANNING;
	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_START_SCAN,
				   buf, len);
	if (!cmd || cmd->status || cmd->cmd_status) {
		wl->scan_stat = GELIC_WL_SCAN_STAT_INIT;
		complete(&wl->scan_done);
		ret = -ENOMEM;
		goto out;
	}
	kfree(cmd);
out:
	free_page((unsigned long)buf);
	mutex_unlock(&wl->scan_lock);
	pr_debug("%s: ->\n", __func__);
	return ret;
}

static void gelic_wl_scan_complete_event(struct gelic_wl_info *wl)
{
	struct gelic_eurus_cmd *cmd = NULL;
	struct gelic_wl_scan_info *target, *tmp;
	struct gelic_wl_scan_info *oldest = NULL;
	struct gelic_eurus_scan_info *scan_info;
	unsigned int scan_info_size;
	union iwreq_data data;
	unsigned long this_time = jiffies;
	unsigned int data_len, i, found, r;
	void *buf;

	pr_debug("%s:start\n", __func__);
	mutex_lock(&wl->scan_lock);

	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		pr_info("%s: scan buffer alloc failed\n", __func__);
		goto out;
	}

	if (wl->scan_stat != GELIC_WL_SCAN_STAT_SCANNING) {
		pr_debug("%s: scan complete when stat != scanning(%d)\n",
			 __func__, wl->scan_stat);
		goto out;
	}

	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_GET_SCAN,
				   buf, PAGE_SIZE);
	if (!cmd || cmd->status || cmd->cmd_status) {
		wl->scan_stat = GELIC_WL_SCAN_STAT_INIT;
		pr_info("%s:cmd failed\n", __func__);
		kfree(cmd);
		goto out;
	}
	data_len = cmd->size;
	pr_debug("%s: data_len = %d\n", __func__, data_len);
	kfree(cmd);

	
	wl->scan_stat = GELIC_WL_SCAN_STAT_GOT_LIST;

	
	list_for_each_entry_safe(target, tmp, &wl->network_list, list) {
		target->valid = 0;
		
		if (time_before(target->last_scanned + wl->scan_age,
				this_time)) {
			kfree(target->hwinfo);
			target->hwinfo = NULL;
			list_move_tail(&target->list, &wl->network_free_list);
		}
	}

	
	for (i = 0, scan_info_size = 0, scan_info = buf;
	     scan_info_size < data_len;
	     i++, scan_info_size += be16_to_cpu(scan_info->size),
	     scan_info = (void *)scan_info + be16_to_cpu(scan_info->size)) {
		pr_debug("%s:size=%d bssid=%pM scan_info=%p\n", __func__,
			 be16_to_cpu(scan_info->size),
			 &scan_info->bssid[2], scan_info);

		if (!be16_to_cpu(scan_info->channel) || !scan_info->rate[0]) {
			pr_debug("%s: invalid scan info\n", __func__);
			continue;
		}

		found = 0;
		oldest = NULL;
		list_for_each_entry(target, &wl->network_list, list) {
			if (!compare_ether_addr(&target->hwinfo->bssid[2],
						&scan_info->bssid[2])) {
				found = 1;
				pr_debug("%s: same BBS found scanned list\n",
					 __func__);
				break;
			}
			if (!oldest ||
			    (target->last_scanned < oldest->last_scanned))
				oldest = target;
		}

		if (!found) {
			
			if (list_empty(&wl->network_free_list)) {
				
				target = oldest;
			} else {
				target = list_entry(wl->network_free_list.next,
						    struct gelic_wl_scan_info,
						    list);
			}
		}

		
		target->last_scanned = this_time;
		target->valid = 1;
		target->eurus_index = i;
		kfree(target->hwinfo);
		target->hwinfo = kzalloc(be16_to_cpu(scan_info->size),
					 GFP_KERNEL);
		if (!target->hwinfo)
			continue;

		
		memcpy(target->hwinfo, scan_info, scan_info->size);
		target->essid_len = strnlen(scan_info->essid,
					    sizeof(scan_info->essid));
		target->rate_len = 0;
		for (r = 0; r < 12; r++)
			if (scan_info->rate[r])
				target->rate_len++;
		if (8 < target->rate_len)
			pr_info("%s: AP returns %d rates\n", __func__,
				target->rate_len);
		target->rate_ext_len = 0;
		for (r = 0; r < 16; r++)
			if (scan_info->ext_rate[r])
				target->rate_ext_len++;
		list_move_tail(&target->list, &wl->network_list);
	}
	memset(&data, 0, sizeof(data));
	wireless_send_event(port_to_netdev(wl_port(wl)), SIOCGIWSCAN, &data,
			    NULL);
out:
	free_page((unsigned long)buf);
	complete(&wl->scan_done);
	mutex_unlock(&wl->scan_lock);
	pr_debug("%s:end\n", __func__);
}

static void update_best(struct gelic_wl_scan_info **best,
			struct gelic_wl_scan_info *candid,
			int *best_weight,
			int *weight)
{
	if (*best_weight < ++(*weight)) {
		*best_weight = *weight;
		*best = candid;
	}
}

static
struct gelic_wl_scan_info *gelic_wl_find_best_bss(struct gelic_wl_info *wl)
{
	struct gelic_wl_scan_info *scan_info;
	struct gelic_wl_scan_info *best_bss;
	int weight, best_weight;
	u16 security;

	pr_debug("%s: <-\n", __func__);

	best_bss = NULL;
	best_weight = 0;

	list_for_each_entry(scan_info, &wl->network_list, list) {
		pr_debug("%s: station %p\n", __func__, scan_info);

		if (!scan_info->valid) {
			pr_debug("%s: station invalid\n", __func__);
			continue;
		}

		
		if (test_bit(GELIC_WL_STAT_BSSID_SET, &wl->stat)) {
			if (!compare_ether_addr(&scan_info->hwinfo->bssid[2],
						wl->bssid)) {
				best_bss = scan_info;
				pr_debug("%s: bssid matched\n", __func__);
				break;
			} else {
				pr_debug("%s: bssid unmached\n", __func__);
				continue;
			}
		}

		weight = 0;

		
		security = be16_to_cpu(scan_info->hwinfo->security) &
			GELIC_EURUS_SCAN_SEC_MASK;
		if (wl->wpa_level == GELIC_WL_WPA_LEVEL_WPA2) {
			if (security == GELIC_EURUS_SCAN_SEC_WPA2)
				update_best(&best_bss, scan_info,
					    &best_weight, &weight);
			else
				continue;
		} else if (wl->wpa_level == GELIC_WL_WPA_LEVEL_WPA) {
			if (security == GELIC_EURUS_SCAN_SEC_WPA)
				update_best(&best_bss, scan_info,
					    &best_weight, &weight);
			else
				continue;
		} else if (wl->wpa_level == GELIC_WL_WPA_LEVEL_NONE &&
			   wl->group_cipher_method == GELIC_WL_CIPHER_WEP) {
			if (security == GELIC_EURUS_SCAN_SEC_WEP)
				update_best(&best_bss, scan_info,
					    &best_weight, &weight);
			else
				continue;
		}

		
		if (test_bit(GELIC_WL_STAT_ESSID_SET, &wl->stat)) {
			if ((scan_info->essid_len == wl->essid_len) &&
			    !strncmp(wl->essid,
				     scan_info->hwinfo->essid,
				     scan_info->essid_len))
				update_best(&best_bss, scan_info,
					    &best_weight, &weight);
			else
				continue;
		}
	}

#ifdef DEBUG
	pr_debug("%s: -> bss=%p\n", __func__, best_bss);
	if (best_bss) {
		pr_debug("%s:addr=%pM\n", __func__,
			 &best_bss->hwinfo->bssid[2]);
	}
#endif
	return best_bss;
}

static int gelic_wl_do_wep_setup(struct gelic_wl_info *wl)
{
	unsigned int i;
	struct gelic_eurus_wep_cfg *wep;
	struct gelic_eurus_cmd *cmd;
	int wep104 = 0;
	int have_key = 0;
	int ret = 0;

	pr_debug("%s: <-\n", __func__);
	
	wep = (struct gelic_eurus_wep_cfg *)__get_free_page(GFP_KERNEL);
	if (!wep)
		return -ENOMEM;

	memset(wep, 0, sizeof(*wep));

	if (wl->group_cipher_method == GELIC_WL_CIPHER_WEP) {
		pr_debug("%s: WEP mode\n", __func__);
		for (i = 0; i < GELIC_WEP_KEYS; i++) {
			if (!test_bit(i, &wl->key_enabled))
				continue;

			pr_debug("%s: key#%d enabled\n", __func__, i);
			have_key = 1;
			if (wl->key_len[i] == 13)
				wep104 = 1;
			else if (wl->key_len[i] != 5) {
				pr_info("%s: wrong wep key[%d]=%d\n",
					__func__, i, wl->key_len[i]);
				ret = -EINVAL;
				goto out;
			}
			memcpy(wep->key[i], wl->key[i], wl->key_len[i]);
		}

		if (!have_key) {
			pr_info("%s: all wep key disabled\n", __func__);
			ret = -EINVAL;
			goto out;
		}

		if (wep104) {
			pr_debug("%s: 104bit key\n", __func__);
			wep->security = cpu_to_be16(GELIC_EURUS_WEP_SEC_104BIT);
		} else {
			pr_debug("%s: 40bit key\n", __func__);
			wep->security = cpu_to_be16(GELIC_EURUS_WEP_SEC_40BIT);
		}
	} else {
		pr_debug("%s: NO encryption\n", __func__);
		wep->security = cpu_to_be16(GELIC_EURUS_WEP_SEC_NONE);
	}

	
	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_SET_WEP_CFG,
				   wep, sizeof(*wep));
	if (!cmd)
		ret = -ENOMEM;
	else if (cmd->status || cmd->cmd_status)
		ret = -ENXIO;

	kfree(cmd);
out:
	free_page((unsigned long)wep);
	pr_debug("%s: ->\n", __func__);
	return ret;
}

#ifdef DEBUG
static const char *wpasecstr(enum gelic_eurus_wpa_security sec)
{
	switch (sec) {
	case GELIC_EURUS_WPA_SEC_NONE:
		return "NONE";
		break;
	case GELIC_EURUS_WPA_SEC_WPA_TKIP_TKIP:
		return "WPA_TKIP_TKIP";
		break;
	case GELIC_EURUS_WPA_SEC_WPA_TKIP_AES:
		return "WPA_TKIP_AES";
		break;
	case GELIC_EURUS_WPA_SEC_WPA_AES_AES:
		return "WPA_AES_AES";
		break;
	case GELIC_EURUS_WPA_SEC_WPA2_TKIP_TKIP:
		return "WPA2_TKIP_TKIP";
		break;
	case GELIC_EURUS_WPA_SEC_WPA2_TKIP_AES:
		return "WPA2_TKIP_AES";
		break;
	case GELIC_EURUS_WPA_SEC_WPA2_AES_AES:
		return "WPA2_AES_AES";
		break;
	}
	return "";
};
#endif

static int gelic_wl_do_wpa_setup(struct gelic_wl_info *wl)
{
	struct gelic_eurus_wpa_cfg *wpa;
	struct gelic_eurus_cmd *cmd;
	u16 security;
	int ret = 0;

	pr_debug("%s: <-\n", __func__);
	
	wpa = (struct gelic_eurus_wpa_cfg *)__get_free_page(GFP_KERNEL);
	if (!wpa)
		return -ENOMEM;

	memset(wpa, 0, sizeof(*wpa));

	if (!test_bit(GELIC_WL_STAT_WPA_PSK_SET, &wl->stat))
		pr_info("%s: PSK not configured yet\n", __func__);

	
	memcpy(wpa->psk, wl->psk, wl->psk_len);

	
	if (wl->wpa_level == GELIC_WL_WPA_LEVEL_WPA2) {
		if (wl->group_cipher_method == GELIC_WL_CIPHER_AES) {
			security = GELIC_EURUS_WPA_SEC_WPA2_AES_AES;
		} else {
			if (wl->pairwise_cipher_method == GELIC_WL_CIPHER_AES &&
			    precise_ie())
				security = GELIC_EURUS_WPA_SEC_WPA2_TKIP_AES;
			else
				security = GELIC_EURUS_WPA_SEC_WPA2_TKIP_TKIP;
		}
	} else {
		if (wl->group_cipher_method == GELIC_WL_CIPHER_AES) {
			security = GELIC_EURUS_WPA_SEC_WPA_AES_AES;
		} else {
			if (wl->pairwise_cipher_method == GELIC_WL_CIPHER_AES &&
			    precise_ie())
				security = GELIC_EURUS_WPA_SEC_WPA_TKIP_AES;
			else
				security = GELIC_EURUS_WPA_SEC_WPA_TKIP_TKIP;
		}
	}
	wpa->security = cpu_to_be16(security);

	
	wpa->psk_type = cpu_to_be16(wl->psk_type);
#ifdef DEBUG
	pr_debug("%s: sec=%s psktype=%s\n", __func__,
		 wpasecstr(wpa->security),
		 (wpa->psk_type == GELIC_EURUS_WPA_PSK_BIN) ?
		 "BIN" : "passphrase");
#if 0
	pr_debug("%s: psk=%s\n", __func__,
		 (wpa->psk_type == GELIC_EURUS_WPA_PSK_BIN) ?
		 "N/A" : wpa->psk);
#endif
#endif
	
	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_SET_WPA_CFG,
				   wpa, sizeof(*wpa));
	if (!cmd)
		ret = -ENOMEM;
	else if (cmd->status || cmd->cmd_status)
		ret = -ENXIO;
	kfree(cmd);
	free_page((unsigned long)wpa);
	pr_debug("%s: --> %d\n", __func__, ret);
	return ret;
}

static int gelic_wl_associate_bss(struct gelic_wl_info *wl,
				  struct gelic_wl_scan_info *bss)
{
	struct gelic_eurus_cmd *cmd;
	struct gelic_eurus_common_cfg *common;
	int ret = 0;
	unsigned long rc;

	pr_debug("%s: <-\n", __func__);

	
	common = (struct gelic_eurus_common_cfg *)__get_free_page(GFP_KERNEL);
	if (!common)
		return -ENOMEM;

	memset(common, 0, sizeof(*common));
	common->bss_type = cpu_to_be16(GELIC_EURUS_BSS_INFRA);
	common->op_mode = cpu_to_be16(GELIC_EURUS_OPMODE_11BG);

	common->scan_index = cpu_to_be16(bss->eurus_index);
	switch (wl->auth_method) {
	case GELIC_EURUS_AUTH_OPEN:
		common->auth_method = cpu_to_be16(GELIC_EURUS_AUTH_OPEN);
		break;
	case GELIC_EURUS_AUTH_SHARED:
		common->auth_method = cpu_to_be16(GELIC_EURUS_AUTH_SHARED);
		break;
	}

#ifdef DEBUG
	scan_list_dump(wl);
#endif
	pr_debug("%s: common cfg index=%d bsstype=%d auth=%d\n", __func__,
		 be16_to_cpu(common->scan_index),
		 be16_to_cpu(common->bss_type),
		 be16_to_cpu(common->auth_method));

	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_SET_COMMON_CFG,
				   common, sizeof(*common));
	if (!cmd || cmd->status || cmd->cmd_status) {
		ret = -ENOMEM;
		kfree(cmd);
		goto out;
	}
	kfree(cmd);

	
	switch (wl->wpa_level) {
	case GELIC_WL_WPA_LEVEL_NONE:
		
		ret = gelic_wl_do_wep_setup(wl);
		break;
	case GELIC_WL_WPA_LEVEL_WPA:
	case GELIC_WL_WPA_LEVEL_WPA2:
		ret = gelic_wl_do_wpa_setup(wl);
		break;
	}

	if (ret) {
		pr_debug("%s: WEP/WPA setup failed %d\n", __func__,
			 ret);
		ret = -EPERM;
		gelic_wl_send_iwap_event(wl, NULL);
		goto out;
	}

	
	init_completion(&wl->assoc_done);
	wl->assoc_stat = GELIC_WL_ASSOC_STAT_ASSOCIATING;
	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_ASSOC,
				   NULL, 0);
	if (!cmd || cmd->status || cmd->cmd_status) {
		pr_debug("%s: assoc request failed\n", __func__);
		wl->assoc_stat = GELIC_WL_ASSOC_STAT_DISCONN;
		kfree(cmd);
		ret = -ENOMEM;
		gelic_wl_send_iwap_event(wl, NULL);
		goto out;
	}
	kfree(cmd);

	
	rc = wait_for_completion_timeout(&wl->assoc_done, HZ * 4);

	if (!rc) {
		
		pr_info("%s: connect timeout\n", __func__);
		cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_DISASSOC,
					   NULL, 0);
		kfree(cmd);
		wl->assoc_stat = GELIC_WL_ASSOC_STAT_DISCONN;
		gelic_wl_send_iwap_event(wl, NULL);
		ret = -ENXIO;
	} else {
		wl->assoc_stat = GELIC_WL_ASSOC_STAT_ASSOCIATED;
		
		memcpy(wl->active_bssid, &bss->hwinfo->bssid[2], ETH_ALEN);

		
		gelic_wl_send_iwap_event(wl, wl->active_bssid);
		pr_info("%s: connected\n", __func__);
	}
out:
	free_page((unsigned long)common);
	pr_debug("%s: ->\n", __func__);
	return ret;
}

static void gelic_wl_connected_event(struct gelic_wl_info *wl,
				     u64 event)
{
	u64 desired_event = 0;

	switch (wl->wpa_level) {
	case GELIC_WL_WPA_LEVEL_NONE:
		desired_event = GELIC_LV1_WL_EVENT_CONNECTED;
		break;
	case GELIC_WL_WPA_LEVEL_WPA:
	case GELIC_WL_WPA_LEVEL_WPA2:
		desired_event = GELIC_LV1_WL_EVENT_WPA_CONNECTED;
		break;
	}

	if (desired_event == event) {
		pr_debug("%s: completed\n", __func__);
		complete(&wl->assoc_done);
		netif_carrier_on(port_to_netdev(wl_port(wl)));
	} else
		pr_debug("%s: event %#llx under wpa\n",
				 __func__, event);
}

static void gelic_wl_disconnect_event(struct gelic_wl_info *wl,
				      u64 event)
{
	struct gelic_eurus_cmd *cmd;
	int lock;

	if (!mutex_trylock(&wl->assoc_stat_lock)) {
		pr_debug("%s: already locked\n", __func__);
		lock = 0;
	} else {
		pr_debug("%s: obtain lock\n", __func__);
		lock = 1;
	}

	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_DISASSOC, NULL, 0);
	kfree(cmd);

	
	if (wl->assoc_stat == GELIC_WL_ASSOC_STAT_ASSOCIATED)
		gelic_wl_send_iwap_event(wl, NULL);

	wl->assoc_stat = GELIC_WL_ASSOC_STAT_DISCONN;
	netif_carrier_off(port_to_netdev(wl_port(wl)));

	if (lock)
		mutex_unlock(&wl->assoc_stat_lock);
}
#ifdef DEBUG
static const char *eventstr(enum gelic_lv1_wl_event event)
{
	static char buf[32];
	char *ret;
	if (event & GELIC_LV1_WL_EVENT_DEVICE_READY)
		ret = "EURUS_READY";
	else if (event & GELIC_LV1_WL_EVENT_SCAN_COMPLETED)
		ret = "SCAN_COMPLETED";
	else if (event & GELIC_LV1_WL_EVENT_DEAUTH)
		ret = "DEAUTH";
	else if (event & GELIC_LV1_WL_EVENT_BEACON_LOST)
		ret = "BEACON_LOST";
	else if (event & GELIC_LV1_WL_EVENT_CONNECTED)
		ret = "CONNECTED";
	else if (event & GELIC_LV1_WL_EVENT_WPA_CONNECTED)
		ret = "WPA_CONNECTED";
	else if (event & GELIC_LV1_WL_EVENT_WPA_ERROR)
		ret = "WPA_ERROR";
	else {
		sprintf(buf, "Unknown(%#x)", event);
		ret = buf;
	}
	return ret;
}
#else
static const char *eventstr(enum gelic_lv1_wl_event event)
{
	return NULL;
}
#endif
static void gelic_wl_event_worker(struct work_struct *work)
{
	struct gelic_wl_info *wl;
	struct gelic_port *port;
	u64 event, tmp;
	int status;

	pr_debug("%s:start\n", __func__);
	wl = container_of(work, struct gelic_wl_info, event_work.work);
	port = wl_port(wl);
	while (1) {
		status = lv1_net_control(bus_id(port->card), dev_id(port->card),
					 GELIC_LV1_GET_WLAN_EVENT, 0, 0, 0,
					 &event, &tmp);
		if (status) {
			if (status != LV1_NO_ENTRY)
				pr_debug("%s:wlan event failed %d\n",
					 __func__, status);
			
			pr_debug("%s:end\n", __func__);
			return;
		}
		pr_debug("%s: event=%s\n", __func__, eventstr(event));
		switch (event) {
		case GELIC_LV1_WL_EVENT_SCAN_COMPLETED:
			gelic_wl_scan_complete_event(wl);
			break;
		case GELIC_LV1_WL_EVENT_BEACON_LOST:
		case GELIC_LV1_WL_EVENT_DEAUTH:
			gelic_wl_disconnect_event(wl, event);
			break;
		case GELIC_LV1_WL_EVENT_CONNECTED:
		case GELIC_LV1_WL_EVENT_WPA_CONNECTED:
			gelic_wl_connected_event(wl, event);
			break;
		default:
			break;
		}
	} 
}
static void gelic_wl_assoc_worker(struct work_struct *work)
{
	struct gelic_wl_info *wl;

	struct gelic_wl_scan_info *best_bss;
	int ret;
	unsigned long irqflag;
	u8 *essid;
	size_t essid_len;

	wl = container_of(work, struct gelic_wl_info, assoc_work.work);

	mutex_lock(&wl->assoc_stat_lock);

	if (wl->assoc_stat != GELIC_WL_ASSOC_STAT_DISCONN)
		goto out;

	spin_lock_irqsave(&wl->lock, irqflag);
	if (test_bit(GELIC_WL_STAT_ESSID_SET, &wl->stat)) {
		pr_debug("%s: assoc ESSID configured %s\n", __func__,
			 wl->essid);
		essid = wl->essid;
		essid_len = wl->essid_len;
	} else {
		essid = NULL;
		essid_len = 0;
	}
	spin_unlock_irqrestore(&wl->lock, irqflag);

	ret = gelic_wl_start_scan(wl, 0, essid, essid_len);
	if (ret == -ERESTARTSYS) {
		pr_debug("%s: scan start failed association\n", __func__);
		schedule_delayed_work(&wl->assoc_work, HZ/10); 
		goto out;
	} else if (ret) {
		pr_info("%s: scan prerequisite failed\n", __func__);
		goto out;
	}

	wait_for_completion(&wl->scan_done);

	pr_debug("%s: scan done\n", __func__);
	mutex_lock(&wl->scan_lock);
	if (wl->scan_stat != GELIC_WL_SCAN_STAT_GOT_LIST) {
		gelic_wl_send_iwap_event(wl, NULL);
		pr_info("%s: no scan list. association failed\n", __func__);
		goto scan_lock_out;
	}

	
	best_bss = gelic_wl_find_best_bss(wl);
	if (!best_bss) {
		gelic_wl_send_iwap_event(wl, NULL);
		pr_info("%s: no bss matched. association failed\n", __func__);
		goto scan_lock_out;
	}

	
	ret = gelic_wl_associate_bss(wl, best_bss);
	if (ret)
		pr_info("%s: association failed %d\n", __func__, ret);
scan_lock_out:
	mutex_unlock(&wl->scan_lock);
out:
	mutex_unlock(&wl->assoc_stat_lock);
}
void gelic_wl_interrupt(struct net_device *netdev, u64 status)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));

	if (status & GELIC_CARD_WLAN_COMMAND_COMPLETED) {
		pr_debug("%s:cmd complete\n", __func__);
		complete(&wl->cmd_done_intr);
	}

	if (status & GELIC_CARD_WLAN_EVENT_RECEIVED) {
		pr_debug("%s:event received\n", __func__);
		queue_delayed_work(wl->event_queue, &wl->event_work, 0);
	}
}

static const iw_handler gelic_wl_wext_handler[] =
{
	IW_HANDLER(SIOCGIWNAME, gelic_wl_get_name),
	IW_HANDLER(SIOCGIWRANGE, gelic_wl_get_range),
	IW_HANDLER(SIOCSIWSCAN, gelic_wl_set_scan),
	IW_HANDLER(SIOCGIWSCAN, gelic_wl_get_scan),
	IW_HANDLER(SIOCSIWAUTH, gelic_wl_set_auth),
	IW_HANDLER(SIOCGIWAUTH, gelic_wl_get_auth),
	IW_HANDLER(SIOCSIWESSID, gelic_wl_set_essid),
	IW_HANDLER(SIOCGIWESSID, gelic_wl_get_essid),
	IW_HANDLER(SIOCSIWENCODE, gelic_wl_set_encode),
	IW_HANDLER(SIOCGIWENCODE, gelic_wl_get_encode),
	IW_HANDLER(SIOCSIWAP, gelic_wl_set_ap),
	IW_HANDLER(SIOCGIWAP, gelic_wl_get_ap),
	IW_HANDLER(SIOCSIWENCODEEXT, gelic_wl_set_encodeext),
	IW_HANDLER(SIOCGIWENCODEEXT, gelic_wl_get_encodeext),
	IW_HANDLER(SIOCSIWMODE, gelic_wl_set_mode),
	IW_HANDLER(SIOCGIWMODE, gelic_wl_get_mode),
	IW_HANDLER(SIOCGIWNICKN, gelic_wl_get_nick),
};

static const struct iw_handler_def gelic_wl_wext_handler_def = {
	.num_standard		= ARRAY_SIZE(gelic_wl_wext_handler),
	.standard		= gelic_wl_wext_handler,
	.get_wireless_stats	= gelic_wl_get_wireless_stats,
};

static struct net_device * __devinit gelic_wl_alloc(struct gelic_card *card)
{
	struct net_device *netdev;
	struct gelic_port *port;
	struct gelic_wl_info *wl;
	unsigned int i;

	pr_debug("%s:start\n", __func__);
	netdev = alloc_etherdev(sizeof(struct gelic_port) +
				sizeof(struct gelic_wl_info));
	pr_debug("%s: netdev =%p card=%p\n", __func__, netdev, card);
	if (!netdev)
		return NULL;

	strcpy(netdev->name, "wlan%d");

	port = netdev_priv(netdev);
	port->netdev = netdev;
	port->card = card;
	port->type = GELIC_PORT_WIRELESS;

	wl = port_wl(port);
	pr_debug("%s: wl=%p port=%p\n", __func__, wl, port);

	
	wl->networks = kzalloc(sizeof(struct gelic_wl_scan_info) *
			       GELIC_WL_BSS_MAX_ENT, GFP_KERNEL);

	if (!wl->networks)
		goto fail_bss;

	wl->eurus_cmd_queue = create_singlethread_workqueue("gelic_cmd");
	if (!wl->eurus_cmd_queue)
		goto fail_cmd_workqueue;

	wl->event_queue = create_singlethread_workqueue("gelic_event");
	if (!wl->event_queue)
		goto fail_event_workqueue;

	INIT_LIST_HEAD(&wl->network_free_list);
	INIT_LIST_HEAD(&wl->network_list);
	for (i = 0; i < GELIC_WL_BSS_MAX_ENT; i++)
		list_add_tail(&wl->networks[i].list,
			      &wl->network_free_list);
	init_completion(&wl->cmd_done_intr);

	INIT_DELAYED_WORK(&wl->event_work, gelic_wl_event_worker);
	INIT_DELAYED_WORK(&wl->assoc_work, gelic_wl_assoc_worker);
	mutex_init(&wl->scan_lock);
	mutex_init(&wl->assoc_stat_lock);

	init_completion(&wl->scan_done);
	
	complete(&wl->scan_done);

	spin_lock_init(&wl->lock);

	wl->scan_age = 5*HZ; 

	
	BUILD_BUG_ON(PAGE_SIZE <
		     sizeof(struct gelic_eurus_scan_info) *
		     GELIC_EURUS_MAX_SCAN);
	pr_debug("%s:end\n", __func__);
	return netdev;

fail_event_workqueue:
	destroy_workqueue(wl->eurus_cmd_queue);
fail_cmd_workqueue:
	kfree(wl->networks);
fail_bss:
	free_netdev(netdev);
	pr_debug("%s:end error\n", __func__);
	return NULL;

}

static void gelic_wl_free(struct gelic_wl_info *wl)
{
	struct gelic_wl_scan_info *scan_info;
	unsigned int i;

	pr_debug("%s: <-\n", __func__);

	pr_debug("%s: destroy queues\n", __func__);
	destroy_workqueue(wl->eurus_cmd_queue);
	destroy_workqueue(wl->event_queue);

	scan_info = wl->networks;
	for (i = 0; i < GELIC_WL_BSS_MAX_ENT; i++, scan_info++)
		kfree(scan_info->hwinfo);
	kfree(wl->networks);

	free_netdev(port_to_netdev(wl_port(wl)));

	pr_debug("%s: ->\n", __func__);
}

static int gelic_wl_try_associate(struct net_device *netdev)
{
	struct gelic_wl_info *wl = port_wl(netdev_priv(netdev));
	int ret = -1;
	unsigned int i;

	pr_debug("%s: <-\n", __func__);

	
	
	if (wl->group_cipher_method == GELIC_WL_CIPHER_NONE) {
		if (test_bit(GELIC_WL_STAT_CONFIGURED,
			     &wl->stat))
			goto do_associate;
		else {
			pr_debug("%s: no wep, not configured\n", __func__);
			return ret;
		}
	}

	
	if (wl->group_cipher_method == GELIC_WL_CIPHER_WEP) {
		
		for (i = 0; i < GELIC_WEP_KEYS; i++) {
			if (test_bit(i, &wl->key_enabled))
			    goto do_associate;
		}
		pr_debug("%s: WEP, but no key specified\n", __func__);
		return ret;
	}

	
	if ((wl->group_cipher_method == GELIC_WL_CIPHER_TKIP) ||
	    (wl->group_cipher_method == GELIC_WL_CIPHER_AES)) {
		if (test_bit(GELIC_WL_STAT_WPA_PSK_SET,
			     &wl->stat))
			goto do_associate;
		else {
			pr_debug("%s: AES/TKIP, but PSK not configured\n",
				 __func__);
			return ret;
		}
	}

do_associate:
	ret = schedule_delayed_work(&wl->assoc_work, 0);
	pr_debug("%s: start association work %d\n", __func__, ret);
	return ret;
}

static int gelic_wl_open(struct net_device *netdev)
{
	struct gelic_card *card = netdev_card(netdev);

	pr_debug("%s:->%p\n", __func__, netdev);

	gelic_card_up(card);

	
	gelic_wl_try_associate(netdev);

	netif_start_queue(netdev);

	pr_debug("%s:<-\n", __func__);
	return 0;
}

static int gelic_wl_reset_state(struct gelic_wl_info *wl)
{
	struct gelic_wl_scan_info *target;
	struct gelic_wl_scan_info *tmp;

	
	list_for_each_entry_safe(target, tmp, &wl->network_list, list) {
		list_move_tail(&target->list, &wl->network_free_list);
	}
	wl->scan_stat = GELIC_WL_SCAN_STAT_INIT;

	
	wl->auth_method = GELIC_EURUS_AUTH_OPEN;
	wl->group_cipher_method = GELIC_WL_CIPHER_NONE;
	wl->pairwise_cipher_method = GELIC_WL_CIPHER_NONE;
	wl->wpa_level = GELIC_WL_WPA_LEVEL_NONE;

	wl->key_enabled = 0;
	wl->current_key = 0;

	wl->psk_type = GELIC_EURUS_WPA_PSK_PASSPHRASE;
	wl->psk_len = 0;

	wl->essid_len = 0;
	memset(wl->essid, 0, sizeof(wl->essid));
	memset(wl->bssid, 0, sizeof(wl->bssid));
	memset(wl->active_bssid, 0, sizeof(wl->active_bssid));

	wl->assoc_stat = GELIC_WL_ASSOC_STAT_DISCONN;

	memset(&wl->iwstat, 0, sizeof(wl->iwstat));
	
	wl->stat = 0;
	return 0;
}

static void gelic_wl_disconnect(struct net_device *netdev)
{
	struct gelic_port *port = netdev_priv(netdev);
	struct gelic_wl_info *wl = port_wl(port);
	struct gelic_eurus_cmd *cmd;

	if (wl->scan_stat == GELIC_WL_SCAN_STAT_SCANNING)
		wait_for_completion_timeout(&wl->scan_done, HZ);

	cmd = gelic_eurus_sync_cmd(wl, GELIC_EURUS_CMD_DISASSOC, NULL, 0);
	kfree(cmd);
	gelic_wl_send_iwap_event(wl, NULL);
};

static int gelic_wl_stop(struct net_device *netdev)
{
	struct gelic_port *port = netdev_priv(netdev);
	struct gelic_wl_info *wl = port_wl(port);
	struct gelic_card *card = netdev_card(netdev);

	pr_debug("%s:<-\n", __func__);

	cancel_delayed_work(&wl->assoc_work);

	if (wl->assoc_stat == GELIC_WL_ASSOC_STAT_ASSOCIATED)
		gelic_wl_disconnect(netdev);

	
	gelic_wl_reset_state(wl);

	netif_stop_queue(netdev);

	gelic_card_down(card);

	pr_debug("%s:->\n", __func__);
	return 0;
}


static const struct net_device_ops gelic_wl_netdevice_ops = {
	.ndo_open = gelic_wl_open,
	.ndo_stop = gelic_wl_stop,
	.ndo_start_xmit = gelic_net_xmit,
	.ndo_set_rx_mode = gelic_net_set_multi,
	.ndo_change_mtu = gelic_net_change_mtu,
	.ndo_tx_timeout = gelic_net_tx_timeout,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = gelic_net_poll_controller,
#endif
};

static const struct ethtool_ops gelic_wl_ethtool_ops = {
	.get_drvinfo	= gelic_net_get_drvinfo,
	.get_link	= gelic_wl_get_link,
};

static void __devinit gelic_wl_setup_netdev_ops(struct net_device *netdev)
{
	struct gelic_wl_info *wl;
	wl = port_wl(netdev_priv(netdev));
	BUG_ON(!wl);
	netdev->watchdog_timeo = GELIC_NET_WATCHDOG_TIMEOUT;

	netdev->ethtool_ops = &gelic_wl_ethtool_ops;
	netdev->netdev_ops = &gelic_wl_netdevice_ops;
	netdev->wireless_data = &wl->wireless_data;
	netdev->wireless_handlers = &gelic_wl_wext_handler_def;
}

int __devinit gelic_wl_driver_probe(struct gelic_card *card)
{
	int ret;
	struct net_device *netdev;

	pr_debug("%s:start\n", __func__);

	if (ps3_compare_firmware_version(1, 6, 0) < 0)
		return 0;
	if (!card->vlan[GELIC_PORT_WIRELESS].tx)
		return 0;

	
	netdev = gelic_wl_alloc(card);
	if (!netdev)
		return -ENOMEM;

	
	SET_NETDEV_DEV(netdev, &card->dev->core);
	gelic_wl_setup_netdev_ops(netdev);

	
	ret = gelic_net_setup_netdev(netdev, card);
	if (ret)
		goto fail_setup;
	card->netdev[GELIC_PORT_WIRELESS] = netdev;

	
	card->irq_mask |= GELIC_CARD_WLAN_EVENT_RECEIVED |
		GELIC_CARD_WLAN_COMMAND_COMPLETED;
	
	gelic_card_set_irq_mask(card, GELIC_CARD_WLAN_EVENT_RECEIVED |
				GELIC_CARD_WLAN_COMMAND_COMPLETED);
	pr_debug("%s:end\n", __func__);
	return 0;

fail_setup:
	gelic_wl_free(port_wl(netdev_port(netdev)));

	return ret;
}

int gelic_wl_driver_remove(struct gelic_card *card)
{
	struct gelic_wl_info *wl;
	struct net_device *netdev;

	pr_debug("%s:start\n", __func__);

	if (ps3_compare_firmware_version(1, 6, 0) < 0)
		return 0;
	if (!card->vlan[GELIC_PORT_WIRELESS].tx)
		return 0;

	netdev = card->netdev[GELIC_PORT_WIRELESS];
	wl = port_wl(netdev_priv(netdev));

	
	if (wl->assoc_stat == GELIC_WL_ASSOC_STAT_ASSOCIATED)
		gelic_wl_disconnect(netdev);

	complete(&wl->cmd_done_intr);

	
	cancel_delayed_work(&wl->assoc_work);
	cancel_delayed_work(&wl->event_work);
	flush_workqueue(wl->eurus_cmd_queue);
	flush_workqueue(wl->event_queue);

	unregister_netdev(netdev);

	
	pr_debug("%s: disable intr\n", __func__);
	card->irq_mask &= ~(GELIC_CARD_WLAN_EVENT_RECEIVED |
			    GELIC_CARD_WLAN_COMMAND_COMPLETED);
	
	gelic_wl_free(wl);
	pr_debug("%s:end\n", __func__);
	return 0;
}
