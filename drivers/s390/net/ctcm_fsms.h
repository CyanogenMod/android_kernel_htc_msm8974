/*
 * drivers/s390/net/ctcm_fsms.h
 *
 * Copyright IBM Corp. 2001, 2007
 * Authors: 	Fritz Elfert (felfert@millenux.com)
 * 		Peter Tiedemann (ptiedem@de.ibm.com)
 * 	MPC additions :
 *		Belinda Thompson (belindat@us.ibm.com)
 *		Andy Richter (richtera@us.ibm.com)
 */
#ifndef _CTCM_FSMS_H_
#define _CTCM_FSMS_H_

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/bitops.h>

#include <linux/signal.h>
#include <linux/string.h>

#include <linux/ip.h>
#include <linux/if_arp.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <net/dst.h>

#include <linux/io.h>
#include <asm/ccwdev.h>
#include <asm/ccwgroup.h>
#include <linux/uaccess.h>

#include <asm/idals.h>

#include "fsm.h"
#include "ctcm_main.h"

enum ctc_ch_events {
	CTC_EVENT_IO_SUCCESS,
	CTC_EVENT_IO_EBUSY,
	CTC_EVENT_IO_ENODEV,
	CTC_EVENT_IO_UNKNOWN,

	CTC_EVENT_ATTNBUSY,
	CTC_EVENT_ATTN,
	CTC_EVENT_BUSY,
	CTC_EVENT_UC_RCRESET,
	CTC_EVENT_UC_RSRESET,
	CTC_EVENT_UC_TXTIMEOUT,
	CTC_EVENT_UC_TXPARITY,
	CTC_EVENT_UC_HWFAIL,
	CTC_EVENT_UC_RXPARITY,
	CTC_EVENT_UC_ZERO,
	CTC_EVENT_UC_UNKNOWN,
	CTC_EVENT_SC_UNKNOWN,
	CTC_EVENT_MC_FAIL,
	CTC_EVENT_MC_GOOD,
	CTC_EVENT_IRQ,
	CTC_EVENT_FINSTAT,
	CTC_EVENT_TIMER,
	CTC_EVENT_START,
	CTC_EVENT_STOP,
	CTC_NR_EVENTS,
	CTC_EVENT_SEND_XID = CTC_NR_EVENTS,
	CTC_EVENT_RSWEEP_TIMER,
	CTC_MPC_NR_EVENTS,
};

enum ctc_ch_states {
	CTC_STATE_IDLE,
	CTC_STATE_STOPPED,
	CTC_STATE_STARTWAIT,
	CTC_STATE_STARTRETRY,
	CTC_STATE_SETUPWAIT,
	CTC_STATE_RXINIT,
	CTC_STATE_TXINIT,
	CTC_STATE_RX,
	CTC_STATE_TX,
	CTC_STATE_RXIDLE,
	CTC_STATE_TXIDLE,
	CTC_STATE_RXERR,
	CTC_STATE_TXERR,
	CTC_STATE_TERM,
	CTC_STATE_DTERM,
	CTC_STATE_NOTOP,
	CTC_NR_STATES,     
	CH_XID0_PENDING = CTC_NR_STATES,
	CH_XID0_INPROGRESS,
	CH_XID7_PENDING,
	CH_XID7_PENDING1,
	CH_XID7_PENDING2,
	CH_XID7_PENDING3,
	CH_XID7_PENDING4,
	CTC_MPC_NR_STATES, 
};

extern const char *ctc_ch_event_names[];

extern const char *ctc_ch_state_names[];

void ctcm_ccw_check_rc(struct channel *ch, int rc, char *msg);
void ctcm_purge_skb_queue(struct sk_buff_head *q);
void fsm_action_nop(fsm_instance *fi, int event, void *arg);

void ctcm_chx_txidle(fsm_instance *fi, int event, void *arg);

extern const fsm_node ch_fsm[];
extern int ch_fsm_len;


void ctcmpc_chx_rxidle(fsm_instance *fi, int event, void *arg);

extern const fsm_node ctcmpc_ch_fsm[];
extern int mpc_ch_fsm_len;


enum dev_states {
	DEV_STATE_STOPPED,
	DEV_STATE_STARTWAIT_RXTX,
	DEV_STATE_STARTWAIT_RX,
	DEV_STATE_STARTWAIT_TX,
	DEV_STATE_STOPWAIT_RXTX,
	DEV_STATE_STOPWAIT_RX,
	DEV_STATE_STOPWAIT_TX,
	DEV_STATE_RUNNING,
	CTCM_NR_DEV_STATES
};

extern const char *dev_state_names[];

enum dev_events {
	DEV_EVENT_START,
	DEV_EVENT_STOP,
	DEV_EVENT_RXUP,
	DEV_EVENT_TXUP,
	DEV_EVENT_RXDOWN,
	DEV_EVENT_TXDOWN,
	DEV_EVENT_RESTART,
	CTCM_NR_DEV_EVENTS
};

extern const char *dev_event_names[];


extern const fsm_node dev_fsm[];
extern int dev_fsm_len;




enum mpcg_events {
	MPCG_EVENT_INOP,
	MPCG_EVENT_DISCONC,
	MPCG_EVENT_XID0DO,
	MPCG_EVENT_XID2,
	MPCG_EVENT_XID2DONE,
	MPCG_EVENT_XID7DONE,
	MPCG_EVENT_TIMER,
	MPCG_EVENT_DOIO,
	MPCG_NR_EVENTS,
};

enum mpcg_states {
	MPCG_STATE_RESET,
	MPCG_STATE_INOP,
	MPCG_STATE_XID2INITW,
	MPCG_STATE_XID2INITX,
	MPCG_STATE_XID7INITW,
	MPCG_STATE_XID7INITX,
	MPCG_STATE_XID0IOWAIT,
	MPCG_STATE_XID0IOWAIX,
	MPCG_STATE_XID7INITI,
	MPCG_STATE_XID7INITZ,
	MPCG_STATE_XID7INITF,
	MPCG_STATE_FLOWC,
	MPCG_STATE_READY,
	MPCG_NR_STATES,
};

#endif
