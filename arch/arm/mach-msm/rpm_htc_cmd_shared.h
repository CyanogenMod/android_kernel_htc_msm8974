/*===========================================================================

  Copyright (c) 2013 HTC.
  All Rights Reserved.
  HTC Proprietary and Confidential.

  ===========================================================================*/

#ifndef RPM_HTC_CMD_SHARED_H
#define RPM_HTC_CMD_SHARED_H


#define RPM_HTC_CMD_REQ_TYPE 0x63637468 

/*
 * This index indicates what command it runs actually.
 * NOTICE! NEVER INSERT NEW ITEM TO OLD ITEMS,
 * ALWAYS PUT NEW ITEM BEFORE RHCH_NUM!
 */
typedef enum
{
	RHCF_VDD_DIG_HOLD = 0,
	RHCF_RPM_FATAL,
	RHCF_NUM,
} RPM_HTC_CMD_FUNC_T;

/* Command RHCF_VDD_DIG_HOLD parameters*/
#define VDD_DIG_HOLD_PARA_ENABLE	0x62616e65 

#endif
