/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004-2007 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#ifndef __DLMCONSTANTS_DOT_H__
#define __DLMCONSTANTS_DOT_H__


#define DLM_LOCKSPACE_LEN       64
#define DLM_RESNAME_MAXLEN      64



#define DLM_LOCK_IV		(-1)	
#define DLM_LOCK_NL		0	
#define DLM_LOCK_CR		1	
#define DLM_LOCK_CW		2	
#define DLM_LOCK_PR		3	
#define DLM_LOCK_PW		4	
#define DLM_LOCK_EX		5	



#define DLM_LKF_NOQUEUE		0x00000001
#define DLM_LKF_CANCEL		0x00000002
#define DLM_LKF_CONVERT		0x00000004
#define DLM_LKF_VALBLK		0x00000008
#define DLM_LKF_QUECVT		0x00000010
#define DLM_LKF_IVVALBLK	0x00000020
#define DLM_LKF_CONVDEADLK	0x00000040
#define DLM_LKF_PERSISTENT	0x00000080
#define DLM_LKF_NODLCKWT	0x00000100
#define DLM_LKF_NODLCKBLK	0x00000200
#define DLM_LKF_EXPEDITE	0x00000400
#define DLM_LKF_NOQUEUEBAST	0x00000800
#define DLM_LKF_HEADQUE		0x00001000
#define DLM_LKF_NOORDER		0x00002000
#define DLM_LKF_ORPHAN		0x00004000
#define DLM_LKF_ALTPR		0x00008000
#define DLM_LKF_ALTCW		0x00010000
#define DLM_LKF_FORCEUNLOCK	0x00020000
#define DLM_LKF_TIMEOUT		0x00040000


#define DLM_ECANCEL		0x10001
#define DLM_EUNLOCK		0x10002

#endif  
