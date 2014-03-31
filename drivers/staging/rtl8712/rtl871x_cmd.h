/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * Modifications for inclusion into the Linux staging tree are
 * Copyright(c) 2010 Larry Finger. All rights reserved.
 *
 * Contact information:
 * WLAN FAE <wlanfae@realtek.com>
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 ******************************************************************************/
#ifndef __RTL871X_CMD_H_
#define __RTL871X_CMD_H_

#include "wlan_bssdef.h"
#include "rtl871x_rf.h"
#define C2H_MEM_SZ (16*1024)

#include "osdep_service.h"
#include "ieee80211.h"

#define FREE_CMDOBJ_SZ	128
#define MAX_CMDSZ	512
#define MAX_RSPSZ	512
#define MAX_EVTSZ	1024
#define CMDBUFF_ALIGN_SZ 512

struct cmd_obj {
	u16	cmdcode;
	u8	res;
	u8	*parmbuf;
	u32	cmdsz;
	u8	*rsp;
	u32	rspsz;
	struct list_head list;
};

struct cmd_priv {
	struct semaphore cmd_queue_sema;
	struct semaphore terminate_cmdthread_sema;
	struct  __queue	cmd_queue;
	u8 cmd_seq;
	u8 *cmd_buf;	
	u8 *cmd_allocated_buf;
	u8 *rsp_buf;	
	u8 *rsp_allocated_buf;
	u32 cmd_issued_cnt;
	u32 cmd_done_cnt;
	u32 rsp_cnt;
	struct _adapter *padapter;
};

struct evt_obj {
	u16 evtcode;
	u8 res;
	u8 *parmbuf;
	u32 evtsz;
	struct list_head list;
};

struct	evt_priv {
	struct  __queue	evt_queue;
	u8	event_seq;
	u8	*evt_buf;	
	u8	*evt_allocated_buf;
	u32	evt_done_cnt;
	struct tasklet_struct event_tasklet;
};

#define init_h2fwcmd_w_parm_no_rsp(pcmd, pparm, code) \
do {\
	_init_listhead(&pcmd->list);\
	pcmd->cmdcode = code;\
	pcmd->parmbuf = (u8 *)(pparm);\
	pcmd->cmdsz = sizeof(*pparm);\
	pcmd->rsp = NULL;\
	pcmd->rspsz = 0;\
} while (0)

u32 r8712_enqueue_cmd(struct cmd_priv *pcmdpriv, struct cmd_obj *obj);
u32 r8712_enqueue_cmd_ex(struct cmd_priv *pcmdpriv, struct cmd_obj *obj);
struct cmd_obj *r8712_dequeue_cmd(struct  __queue *queue);
void r8712_free_cmd_obj(struct cmd_obj *pcmd);
int r8712_cmd_thread(void *context);
u32 r8712_init_cmd_priv(struct cmd_priv *pcmdpriv);
void r8712_free_cmd_priv(struct cmd_priv *pcmdpriv);
u32 r8712_init_evt_priv(struct evt_priv *pevtpriv);
void r8712_free_evt_priv(struct evt_priv *pevtpriv);

enum rtl871x_drvint_cid {
	NONE_WK_CID,
	WDG_WK_CID,
	MAX_WK_CID
};

enum RFINTFS {
	SWSI,
	HWSI,
	HWPI,
};

struct usb_suspend_parm {
	u32 action; 
};

struct joinbss_parm {
	struct ndis_wlan_bssid_ex network;
};

struct disconnect_parm {
	u32 rsvd;
};

struct createbss_parm {
	struct ndis_wlan_bssid_ex network;
};

struct	setopmode_parm {
	u8	mode;
	u8	rsvd[3];
};

struct sitesurvey_parm {
	sint passive_mode;	
	sint bsslimit;	
	sint	ss_ssidlen;
	u8	ss_ssid[IW_ESSID_MAX_SIZE + 1];
};

struct setauth_parm {
	u8 mode;  
	u8 _1x;   
	u8 rsvd[2];
};

struct setkey_parm {
	u8	algorithm;	
	u8	keyid;
	u8	grpkey;		
	u8	key[16];	
};

struct set_stakey_parm {
	u8	addr[ETH_ALEN];
	u8	algorithm;
	u8	key[16];
};

struct set_stakey_rsp {
	u8	addr[ETH_ALEN];
	u8	keyid;
	u8	rsvd;
};

struct SetMacAddr_param {
	u8	MacAddr[ETH_ALEN];
};

struct set_assocsta_parm {
	u8	addr[ETH_ALEN];
};

struct set_assocsta_rsp {
	u8	cam_id;
	u8	rsvd[3];
};

struct del_assocsta_parm {
	u8	addr[ETH_ALEN];
};

struct setstapwrstate_parm {
	u8	staid;
	u8	status;
	u8	hwaddr[6];
};

struct	setbasicrate_parm {
	u8	basicrates[NumRates];
};

struct getbasicrate_parm {
	u32 rsvd;
};

struct getbasicrate_rsp {
	u8 basicrates[NumRates];
};

struct setdatarate_parm {
	u8	mac_id;
	u8	datarates[NumRates];
};

enum _RT_CHANNEL_DOMAIN {
	RT_CHANNEL_DOMAIN_FCC = 0,
	RT_CHANNEL_DOMAIN_IC = 1,
	RT_CHANNEL_DOMAIN_ETSI = 2,
	RT_CHANNEL_DOMAIN_SPAIN = 3,
	RT_CHANNEL_DOMAIN_FRANCE = 4,
	RT_CHANNEL_DOMAIN_MKK = 5,
	RT_CHANNEL_DOMAIN_MKK1 = 6,
	RT_CHANNEL_DOMAIN_ISRAEL = 7,
	RT_CHANNEL_DOMAIN_TELEC = 8,

	
	RT_CHANNEL_DOMAIN_MIC = 9,
	RT_CHANNEL_DOMAIN_GLOBAL_DOAMIN = 10,
	RT_CHANNEL_DOMAIN_WORLD_WIDE_13 = 11,
	RT_CHANNEL_DOMAIN_TELEC_NETGEAR = 12,

	RT_CHANNEL_DOMAIN_NCC = 13,
	RT_CHANNEL_DOMAIN_5G = 14,
	RT_CHANNEL_DOMAIN_5G_40M = 15,
 
	RT_CHANNEL_DOMAIN_MAX,
};


struct SetChannelPlan_param {
	enum _RT_CHANNEL_DOMAIN ChannelPlan;
};

struct getdatarate_parm {
	u32 rsvd;

};
struct getdatarate_rsp {
	u8 datarates[NumRates];
};



struct	getphy_rsp {
	u8	rfchannel;
	u8	modem;
};

struct readBB_parm {
	u8	offset;
};
struct readBB_rsp {
	u8	value;
};

struct readTSSI_parm {
	u8	offset;
};
struct readTSSI_rsp {
	u8	value;
};

struct writeBB_parm {
	u8	offset;
	u8	value;
};

struct writePTM_parm {
	u8	type;
};

struct readRF_parm {
	u8	offset;
};
struct readRF_rsp {
	u32	value;
};

struct writeRF_parm {
	u32	offset;
	u32	value;
};

struct setrfintfs_parm {
	u8	rfintfs;
};

struct getrfintfs_parm {
	u8	rfintfs;
};


struct drvint_cmd_parm {
	int i_cid; 
	int sz; 
	unsigned char *pbuf;
};


struct	setantenna_parm {
	u8	tx_antset;
	u8	rx_antset;
	u8	tx_antenna;
	u8	rx_antenna;
};

struct	enrateadaptive_parm {
	u32	en;
};

struct settxagctbl_parm {
	u32	txagc[MAX_RATES_LENGTH];
};

struct gettxagctbl_parm {
	u32 rsvd;
};
struct gettxagctbl_rsp {
	u32	txagc[MAX_RATES_LENGTH];
};

struct setagcctrl_parm {
	u32	agcctrl;	
};

struct setssup_parm	{
	u32	ss_ForceUp[MAX_RATES_LENGTH];
};

struct getssup_parm	{
	u32 rsvd;
};
struct getssup_rsp	{
	u8	ss_ForceUp[MAX_RATES_LENGTH];
};

struct setssdlevel_parm	{
	u8	ss_DLevel[MAX_RATES_LENGTH];
};

struct getssdlevel_parm	{
	u32 rsvd;
};
struct getssdlevel_rsp	{
	u8	ss_DLevel[MAX_RATES_LENGTH];
};

struct setssulevel_parm	{
	u8	ss_ULevel[MAX_RATES_LENGTH];
};

struct getssulevel_parm	{
	u32 rsvd;
};
struct getssulevel_rsp	{
	u8	ss_ULevel[MAX_RATES_LENGTH];
};

struct	setcountjudge_parm {
	u8	count_judge[MAX_RATES_LENGTH];
};

struct	getcountjudge_parm {
	u32 rsvd;
};

struct	getcountjudge_rsp {
	u8	count_judge[MAX_RATES_LENGTH];
};

struct setpwrmode_parm  {
	u8	mode;
	u8	flag_low_traffic_en;
	u8	flag_lpnav_en;
	u8	flag_rf_low_snr_en;
	u8	flag_dps_en; 
	u8	bcn_rx_en;
	u8	bcn_pass_cnt;	  
	u8	bcn_to;		  
	u16	bcn_itv;
	u8	app_itv; 
	u8	awake_bcn_itv;
	u8	smart_ps;
	u8	bcn_pass_time;	
};

struct setatim_parm {
	u8 op;   
	u8 txid; 
};

struct setratable_parm {
	u8 ss_ForceUp[NumRates];
	u8 ss_ULevel[NumRates];
	u8 ss_DLevel[NumRates];
	u8 count_judge[NumRates];
};

struct getratable_parm {
	uint rsvd;
};
struct getratable_rsp {
	u8 ss_ForceUp[NumRates];
	u8 ss_ULevel[NumRates];
	u8 ss_DLevel[NumRates];
	u8 count_judge[NumRates];
};

struct gettxretrycnt_parm {
	unsigned int rsvd;
};

struct gettxretrycnt_rsp {
	unsigned long tx_retrycnt;
};

struct getrxretrycnt_parm {
	unsigned int rsvd;
};

struct getrxretrycnt_rsp {
	unsigned long rx_retrycnt;
};

struct getbcnokcnt_parm {
	unsigned int rsvd;
};

struct getbcnokcnt_rsp {
	unsigned long bcnokcnt;
};

struct getbcnerrcnt_parm {
	unsigned int rsvd;
};
struct getbcnerrcnt_rsp {
	unsigned long bcnerrcnt;
};

struct getcurtxpwrlevel_parm {
	unsigned int rsvd;
};

struct getcurtxpwrlevel_rsp {
	unsigned short tx_power;
};

struct setdig_parm {
	unsigned char dig_on;	
};

struct setra_parm {
	unsigned char ra_on;	
};

struct setprobereqextraie_parm {
	unsigned char e_id;
	unsigned char ie_len;
	unsigned char ie[0];
};

struct setassocreqextraie_parm {
	unsigned char e_id;
	unsigned char ie_len;
	unsigned char ie[0];
};

struct setproberspextraie_parm {
	unsigned char e_id;
	unsigned char ie_len;
	unsigned char ie[0];
};

struct setassocrspextraie_parm {
	unsigned char e_id;
	unsigned char ie_len;
	unsigned char ie[0];
};

struct addBaReq_parm {
	unsigned int tid;
};

struct SetChannel_parm {
	u32 curr_ch;
};

struct DisconnectCtrlEx_param {
	
	unsigned char EnableDrvCtrl;
	unsigned char TryPktCnt;
	unsigned char TryPktInterval; 
	unsigned char rsvd;
	unsigned int  FirstStageTO; 
};

#define GEN_CMD_CODE(cmd)	cmd ## _CMD_


#define H2C_RSP_OFFSET			512
#define H2C_SUCCESS			0x00
#define H2C_SUCCESS_RSP			0x01
#define H2C_DUPLICATED			0x02
#define H2C_DROPPED			0x03
#define H2C_PARAMETERS_ERROR		0x04
#define H2C_REJECTED			0x05
#define H2C_CMD_OVERFLOW		0x06
#define H2C_RESERVED			0x07

u8 r8712_setMacAddr_cmd(struct _adapter *padapter, u8 *mac_addr);
u8 r8712_setassocsta_cmd(struct _adapter *padapter, u8 *mac_addr);
u8 r8712_sitesurvey_cmd(struct _adapter *padapter,
			struct ndis_802_11_ssid *pssid);
u8 r8712_createbss_cmd(struct _adapter *padapter);
u8 r8712_setstakey_cmd(struct _adapter *padapter, u8 *psta, u8 unicast_key);
u8 r8712_joinbss_cmd(struct _adapter *padapter,
		     struct wlan_network *pnetwork);
u8 r8712_disassoc_cmd(struct _adapter *padapter);
u8 r8712_setopmode_cmd(struct _adapter *padapter,
		 enum NDIS_802_11_NETWORK_INFRASTRUCTURE networktype);
u8 r8712_setdatarate_cmd(struct _adapter *padapter, u8 *rateset);
u8 r8712_set_chplan_cmd(struct _adapter  *padapter, int chplan);
u8 r8712_setbasicrate_cmd(struct _adapter *padapter, u8 *rateset);
u8 r8712_getrfreg_cmd(struct _adapter *padapter, u8 offset, u8 * pval);
u8 r8712_setrfintfs_cmd(struct _adapter *padapter, u8 mode);
u8 r8712_setrfreg_cmd(struct _adapter  *padapter, u8 offset, u32 val);
u8 r8712_setrttbl_cmd(struct _adapter  *padapter,
		      struct setratable_parm *prate_table);
u8 r8712_gettssi_cmd(struct _adapter  *padapter, u8 offset, u8 *pval);
u8 r8712_setptm_cmd(struct _adapter *padapter, u8 type);
u8 r8712_setfwdig_cmd(struct _adapter *padapter, u8 type);
u8 r8712_setfwra_cmd(struct _adapter *padapter, u8 type);
u8 r8712_addbareq_cmd(struct _adapter *padapter, u8 tid);
u8 r8712_wdg_wk_cmd(struct _adapter *padapter);
void r8712_survey_cmd_callback(struct _adapter  *padapter,
			       struct cmd_obj *pcmd);
void r8712_disassoc_cmd_callback(struct _adapter  *padapter,
				 struct cmd_obj *pcmd);
void r8712_joinbss_cmd_callback(struct _adapter  *padapter,
				struct cmd_obj *pcmd);
void r8712_createbss_cmd_callback(struct _adapter *padapter,
				  struct cmd_obj *pcmd);
void r8712_getbbrfreg_cmdrsp_callback(struct _adapter *padapter,
				      struct cmd_obj *pcmd);
void r8712_readtssi_cmdrsp_callback(struct _adapter *padapter,
				struct cmd_obj *pcmd);
void r8712_setstaKey_cmdrsp_callback(struct _adapter  *padapter,
				     struct cmd_obj *pcmd);
void r8712_setassocsta_cmdrsp_callback(struct _adapter  *padapter,
				       struct cmd_obj *pcmd);
u8 r8712_disconnectCtrlEx_cmd(struct _adapter *adapter, u32 enableDrvCtrl,
			u32 tryPktCnt, u32 tryPktInterval, u32 firstStageTO);

struct _cmd_callback {
	u32	cmd_code;
	void (*callback)(struct _adapter  *padapter, struct cmd_obj *cmd);
};

#include "rtl8712_cmd.h"

#endif 

