/*
 * 802.11e protocol header file
 *
 * Copyright (C) 1999-2012, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: 802.11e.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _802_11e_H_
#define _802_11e_H_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#include <packed_section_start.h>


#define WME_TSPEC_HDR_LEN           2           
#define WME_TSPEC_BODY_OFF          2           

#define WME_CATEGORY_CODE_OFFSET	0		
#define WME_ACTION_CODE_OFFSET		1		
#define WME_TOKEN_CODE_OFFSET		2		
#define WME_STATUS_CODE_OFFSET		3		

BWL_PRE_PACKED_STRUCT struct tsinfo {
	uint8 octets[3];
} BWL_POST_PACKED_STRUCT;

typedef struct tsinfo tsinfo_t;

typedef BWL_PRE_PACKED_STRUCT struct tspec {
	uint8 oui[DOT11_OUI_LEN];	
	uint8 type;					
	uint8 subtype;				
	uint8 version;				
	tsinfo_t tsinfo;			
	uint16 nom_msdu_size;		
	uint16 max_msdu_size;		
	uint32 min_srv_interval;	
	uint32 max_srv_interval;	
	uint32 inactivity_interval;	
	uint32 suspension_interval; 
	uint32 srv_start_time;		
	uint32 min_data_rate;		
	uint32 mean_data_rate;		
	uint32 peak_data_rate;		
	uint32 max_burst_size;		
	uint32 delay_bound;			
	uint32 min_phy_rate;		
	uint16 surplus_bw;			
	uint16 medium_time;			
} BWL_POST_PACKED_STRUCT tspec_t;

#define WME_TSPEC_LEN	(sizeof(tspec_t))		

#define TS_INFO_TID_SHIFT		1	
#define TS_INFO_TID_MASK		(0xf << TS_INFO_TID_SHIFT)	
#define TS_INFO_CONTENTION_SHIFT	7	
#define TS_INFO_CONTENTION_MASK	(0x1 << TS_INFO_CONTENTION_SHIFT) 
#define TS_INFO_DIRECTION_SHIFT	5	
#define TS_INFO_DIRECTION_MASK	(0x3 << TS_INFO_DIRECTION_SHIFT) 
#define TS_INFO_PSB_SHIFT		2		
#define TS_INFO_PSB_MASK		(1 << TS_INFO_PSB_SHIFT)	
#define TS_INFO_UPLINK			(0 << TS_INFO_DIRECTION_SHIFT)	
#define TS_INFO_DOWNLINK		(1 << TS_INFO_DIRECTION_SHIFT)	
#define TS_INFO_BIDIRECTIONAL	(3 << TS_INFO_DIRECTION_SHIFT)	
#define TS_INFO_USER_PRIO_SHIFT	3	
#define TS_INFO_USER_PRIO_MASK	(0x7 << TS_INFO_USER_PRIO_SHIFT)

#define WLC_CAC_GET_TID(pt)	((((pt).octets[0]) & TS_INFO_TID_MASK) >> TS_INFO_TID_SHIFT)
#define WLC_CAC_GET_DIR(pt)	((((pt).octets[0]) & \
	TS_INFO_DIRECTION_MASK) >> TS_INFO_DIRECTION_SHIFT)
#define WLC_CAC_GET_PSB(pt)	((((pt).octets[1]) & TS_INFO_PSB_MASK) >> TS_INFO_PSB_SHIFT)
#define WLC_CAC_GET_USER_PRIO(pt)	((((pt).octets[1]) & \
	TS_INFO_USER_PRIO_MASK) >> TS_INFO_USER_PRIO_SHIFT)

#define WLC_CAC_SET_TID(pt, id)	((((pt).octets[0]) & (~TS_INFO_TID_MASK)) | \
	((id) << TS_INFO_TID_SHIFT))
#define WLC_CAC_SET_USER_PRIO(pt, prio)	((((pt).octets[0]) & (~TS_INFO_USER_PRIO_MASK)) | \
	((prio) << TS_INFO_USER_PRIO_SHIFT))

#define QBSS_LOAD_IE_LEN		5	
#define QBSS_LOAD_AAC_OFF		3	

#define CAC_ADDTS_RESP_TIMEOUT		300	

#define DOT11E_STATUS_ADMISSION_ACCEPTED	0	
#define DOT11E_STATUS_ADDTS_INVALID_PARAM	1	
#define DOT11E_STATUS_ADDTS_REFUSED_NSBW	3	
#define DOT11E_STATUS_ADDTS_REFUSED_AWHILE	47	

#define DOT11E_STATUS_QSTA_LEAVE_QBSS		36	
#define DOT11E_STATUS_END_TS				37	
#define DOT11E_STATUS_UNKNOWN_TS			38	
#define DOT11E_STATUS_QSTA_REQ_TIMEOUT		39	


#include <packed_section_end.h>

#endif 
