/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


struct SYM_FWA_SCR {
	u32 start		[ 11];
	u32 getjob_begin	[  4];
	u32 _sms_a10		[  5];
	u32 getjob_end		[  4];
	u32 _sms_a20		[  4];
#ifdef SYM_CONF_TARGET_ROLE_SUPPORT
	u32 select		[  8];
#else
	u32 select		[  6];
#endif
	u32 _sms_a30		[  5];
	u32 wf_sel_done		[  2];
	u32 send_ident		[  2];
#ifdef SYM_CONF_IARB_SUPPORT
	u32 select2		[  8];
#else
	u32 select2		[  2];
#endif
	u32 command		[  2];
	u32 dispatch		[ 28];
	u32 sel_no_cmd		[ 10];
	u32 init		[  6];
	u32 clrack		[  4];
	u32 datai_done		[ 11];
	u32 datai_done_wsr	[ 20];
	u32 datao_done		[ 11];
	u32 datao_done_wss	[  6];
	u32 datai_phase		[  5];
	u32 datao_phase		[  5];
	u32 msg_in		[  2];
	u32 msg_in2		[ 10];
#ifdef SYM_CONF_IARB_SUPPORT
	u32 status		[ 14];
#else
	u32 status		[ 10];
#endif
	u32 complete		[  6];
	u32 complete2		[  8];
	u32 _sms_a40		[ 12];
	u32 done		[  5];
	u32 _sms_a50		[  5];
	u32 _sms_a60		[  2];
	u32 done_end		[  4];
	u32 complete_error	[  5];
	u32 save_dp		[ 11];
	u32 restore_dp		[  7];
	u32 disconnect		[ 11];
	u32 disconnect2		[  5];
	u32 _sms_a65		[  3];
#ifdef SYM_CONF_IARB_SUPPORT
	u32 idle		[  4];
#else
	u32 idle		[  2];
#endif
#ifdef SYM_CONF_IARB_SUPPORT
	u32 ungetjob		[  7];
#else
	u32 ungetjob		[  5];
#endif
#ifdef SYM_CONF_TARGET_ROLE_SUPPORT
	u32 reselect		[  4];
#else
	u32 reselect		[  2];
#endif
	u32 reselected		[ 19];
	u32 _sms_a70		[  6];
	u32 _sms_a80		[  4];
	u32 reselected1		[ 25];
	u32 _sms_a90		[  4];
	u32 resel_lun0		[  7];
	u32 _sms_a100		[  4];
	u32 resel_tag		[  8];
#if   SYM_CONF_MAX_TASK*4 > 512
	u32 _sms_a110		[ 23];
#elif SYM_CONF_MAX_TASK*4 > 256
	u32 _sms_a110		[ 17];
#else
	u32 _sms_a110		[ 13];
#endif
	u32 _sms_a120		[  2];
	u32 resel_go		[  4];
	u32 _sms_a130		[  7];
	u32 resel_dsa		[  2];
	u32 resel_dsa1		[  4];
	u32 _sms_a140		[  7];
	u32 resel_no_tag	[  4];
	u32 _sms_a145		[  7];
	u32 data_in		[SYM_CONF_MAX_SG * 2];
	u32 data_in2		[  4];
	u32 data_out		[SYM_CONF_MAX_SG * 2];
	u32 data_out2		[  4];
	u32 pm0_data		[ 12];
	u32 pm0_data_out	[  6];
	u32 pm0_data_end	[  7];
	u32 pm_data_end		[  4];
	u32 _sms_a150		[  4];
	u32 pm1_data		[ 12];
	u32 pm1_data_out	[  6];
	u32 pm1_data_end	[  9];
};

struct SYM_FWB_SCR {
	u32 no_data		[  2];
#ifdef SYM_CONF_TARGET_ROLE_SUPPORT
	u32 sel_for_abort	[ 18];
#else
	u32 sel_for_abort	[ 16];
#endif
	u32 sel_for_abort_1	[  2];
	u32 msg_in_etc		[ 12];
	u32 msg_received	[  5];
	u32 msg_weird_seen	[  5];
	u32 msg_extended	[ 17];
	u32 _sms_b10		[  4];
	u32 msg_bad		[  6];
	u32 msg_weird		[  4];
	u32 msg_weird1		[  8];
	u32 wdtr_resp		[  6];
	u32 send_wdtr		[  4];
	u32 sdtr_resp		[  6];
	u32 send_sdtr		[  4];
	u32 ppr_resp		[  6];
	u32 send_ppr		[  4];
	u32 nego_bad_phase	[  4];
	u32 msg_out		[  4];
	u32 msg_out_done	[  4];
	u32 data_ovrun		[  3];
	u32 data_ovrun1		[ 22];
	u32 data_ovrun2		[  8];
	u32 abort_resel		[ 16];
	u32 resend_ident	[  4];
	u32 ident_break		[  4];
	u32 ident_break_atn	[  4];
	u32 sdata_in		[  6];
	u32 resel_bad_lun	[  4];
	u32 bad_i_t_l		[  4];
	u32 bad_i_t_l_q		[  4];
	u32 bad_status		[  7];
	u32 wsr_ma_helper	[  4];

	
	u32 zero		[  1];
	u32 scratch		[  1];
	u32 scratch1		[  1];
	u32 prev_done		[  1];
	u32 done_pos		[  1];
	u32 nextjob		[  1];
	u32 startpos		[  1];
	u32 targtbl		[  1];
};

struct SYM_FWZ_SCR {
	u32 snooptest		[  9];
	u32 snoopend		[  2];
};

static struct SYM_FWA_SCR SYM_FWA_SCR = {
 {
	SCR_REG_REG (gpreg, SCR_AND, 0xfe),
		0,
	SCR_FROM_REG (ctest2),
		0,
	SCR_FROM_REG (istat),
		0,
	SCR_COPY (4),
		PADDR_B (startpos),
		RADDR_1 (scratcha),
	SCR_INT ^ IFTRUE (MASK (SEM, SEM)),
		SIR_SCRIPT_STOPPED,
},{
	SCR_COPY (4),
		RADDR_1 (scratcha),
		PADDR_A (_sms_a10),
	SCR_COPY (8),
},{
		0,
		PADDR_B (nextjob),
	SCR_COPY (4),
		PADDR_B (nextjob),
		RADDR_1 (dsa),
},{
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a20),
	SCR_COPY (4),
},{
		0,
		RADDR_1 (temp),
	SCR_RETURN,
		0,
},{
#ifdef SYM_CONF_TARGET_ROLE_SUPPORT
	SCR_CLR (SCR_TRG),
		0,
#endif
	SCR_SEL_TBL_ATN ^ offsetof (struct sym_dsb, select),
		PADDR_A (ungetjob),

	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a30),
	SCR_COPY (sizeof(struct sym_ccbh)),
},{
		0,
		HADDR_1 (ccb_head),
	SCR_COPY (4),
		HADDR_1 (ccb_head.status),
		RADDR_1 (scr0),
},{
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		SIR_SEL_ATN_NO_MSG_OUT,
},{
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct sym_dsb, smsg),
},{
#ifdef SYM_CONF_IARB_SUPPORT
	SCR_FROM_REG (HF_REG),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (HF_HINT_IARB, HF_HINT_IARB)),
		8,
	SCR_REG_REG (scntl1, SCR_OR, IARB),
		0,
#endif
	SCR_JUMP ^ IFFALSE (WHEN (SCR_COMMAND)),
		PADDR_A (sel_no_cmd),
},{
	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct sym_dsb, cmd),
},{
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR_A (msg_in),
	SCR_JUMP ^ IFTRUE (IF (SCR_DATA_OUT)),
		PADDR_A (datao_phase),
	SCR_JUMP ^ IFTRUE (IF (SCR_DATA_IN)),
		PADDR_A (datai_phase),
	SCR_JUMP ^ IFTRUE (IF (SCR_STATUS)),
		PADDR_A (status),
	SCR_JUMP ^ IFTRUE (IF (SCR_COMMAND)),
		PADDR_A (command),
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_OUT)),
		PADDR_B (msg_out),
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_ILG_OUT)),
		16,
	SCR_MOVE_ABS (1) ^ SCR_ILG_OUT,
		HADDR_1 (scratch),
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_ILG_OUT)),
		-16,
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_ILG_IN)),
		16,
	SCR_MOVE_ABS (1) ^ SCR_ILG_IN,
		HADDR_1 (scratch),
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_ILG_IN)),
		-16,
	SCR_INT,
		SIR_BAD_PHASE,
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDR_B (resend_ident),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR_A (dispatch),
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_FROM_REG (sstat0),
		0,
	SCR_JUMPR ^ IFTRUE (MASK (IRST, IRST)),
		-16,
	SCR_JUMP,
		PADDR_A (start),
},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_COPY (4),
		RADDR_1 (temp),
		HADDR_1 (ccb_head.lastp),
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMP ^ IFTRUE (MASK (WSR, WSR)),
		PADDR_A (datai_done_wsr),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_STATUS)),
		PADDR_A (status),
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_SWIDE_OVERRUN,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR_A (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		HADDR_1 (msgin[0]),
	SCR_INT ^ IFFALSE (DATA (M_IGN_RESIDUE)),
		SIR_SWIDE_OVERRUN,
	SCR_JUMP ^ IFFALSE (DATA (M_IGN_RESIDUE)),
		PADDR_A (msg_in2),
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		HADDR_1 (msgin[1]),
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_COPY (4),
		RADDR_1 (temp),
		HADDR_1 (ccb_head.lastp),
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMP ^ IFTRUE (MASK (WSS, WSS)),
		PADDR_A (datao_done_wss),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_STATUS)),
		PADDR_A (status),
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_REG_REG (scntl2, SCR_OR, WSS),
		0,
	SCR_INT,
		SIR_SODL_UNDERRUN,
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_COPY (4),
		HADDR_1 (ccb_head.lastp),
		RADDR_1 (temp),
	SCR_RETURN,
		0,
},{
	SCR_COPY (4),
		HADDR_1 (ccb_head.lastp),
		RADDR_1 (temp),
	SCR_RETURN,
		0,
},{
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		HADDR_1 (msgin[0]),
},{
	SCR_JUMP ^ IFTRUE (DATA (M_COMPLETE)),
		PADDR_A (complete),
	SCR_JUMP ^ IFTRUE (DATA (M_DISCONNECT)),
		PADDR_A (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (M_SAVE_DP)),
		PADDR_A (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_RESTORE_DP)),
		PADDR_A (restore_dp),
	SCR_JUMP,
		PADDR_B (msg_in_etc),
},{
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		HADDR_1 (scratch),
#ifdef SYM_CONF_IARB_SUPPORT
	SCR_JUMPR ^ IFTRUE (DATA (S_GOOD)),
		8,
	SCR_REG_REG (scntl1, SCR_AND, ~IARB),
		0,
#endif
	SCR_TO_REG (SS_REG),
		0,
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR_A (msg_in),
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
},{
	SCR_COPY (4),
		RADDR_1 (scr0),
		HADDR_1 (ccb_head.status),
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a40),
	SCR_COPY (sizeof(struct sym_ccbh)),
		HADDR_1 (ccb_head),
},{
		0,
	SCR_COPY (4),			
		HADDR_1 (ccb_head.status),
		RADDR_1 (scr0),
	SCR_FROM_REG (SS_REG),
		0,
	SCR_CALL ^ IFFALSE (DATA (S_GOOD)),
		PADDR_B (bad_status),
	SCR_FROM_REG (HF_REG),
		0,
	SCR_JUMP ^ IFFALSE (MASK (0 ,(HF_SENSE|HF_EXT_ERR))),
		PADDR_A (complete_error),
},{
	SCR_COPY (4),
		PADDR_B (done_pos),
		PADDR_A (_sms_a50),
	SCR_COPY (4),
		RADDR_1 (dsa),
},{
		0,
	SCR_COPY (4),
		PADDR_B (done_pos),
		PADDR_A (_sms_a60),
	SCR_COPY (8),
},{
		0,
		PADDR_B (prev_done),
},{
	SCR_INT_FLY,
		0,
	SCR_JUMP,
		PADDR_A (start),
},{
	SCR_COPY (4),
		PADDR_B (startpos),
		RADDR_1 (scratcha),
	SCR_INT,
		SIR_COMPLETE_ERROR,
},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_REG_REG (HF_REG, SCR_OR, HF_DP_SAVED),
		0,
	SCR_COPY (4),
		HADDR_1 (ccb_head.lastp),
		HADDR_1 (ccb_head.savep),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR_A (msg_in),
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_COPY (4),
		HADDR_1 (ccb_head.savep),
		HADDR_1 (ccb_head.lastp),
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	SCR_COPY (4),
		RADDR_1 (scr0),
		HADDR_1 (ccb_head.status),
},{
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a65),
	SCR_COPY (sizeof(struct sym_ccbh)),
		HADDR_1 (ccb_head),
},{
		0,
	SCR_JUMP,
		PADDR_A (start),
},{
	SCR_REG_REG (gpreg, SCR_OR, 0x01),
		0,
#ifdef SYM_CONF_IARB_SUPPORT
	SCR_JUMPR,
		8,
#endif
},{
#ifdef SYM_CONF_IARB_SUPPORT
	SCR_REG_REG (scntl1, SCR_OR, IARB),
		0,
#endif
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_COPY (4),
		RADDR_1 (scratcha),
		PADDR_B (startpos),
},{
#ifdef SYM_CONF_TARGET_ROLE_SUPPORT
	SCR_CLR (SCR_TRG),
		0,
#endif
	SCR_WAIT_RESEL,
		PADDR_A(start),
},{
	SCR_REG_REG (gpreg, SCR_AND, 0xfe),
		0,
	SCR_REG_SFBR (ssid, SCR_AND, 0x8F),
		0,
	SCR_TO_REG (sdid),
		0,
	SCR_COPY (4),
		PADDR_B (targtbl),
		RADDR_1 (dsa),
	SCR_SFBR_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_AND, 0x3c),
		0,
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a70),
	SCR_COPY (4),
},{
		0,
		RADDR_1 (dsa),
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a80),
	SCR_COPY (sizeof(struct sym_tcbh)),
},{
		0,
		HADDR_1 (tcb_head),
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_RESEL_NO_MSG_IN,
},{
	SCR_COPY (1),
		HADDR_1 (tcb_head.wval),
		RADDR_1 (scntl3),
	SCR_COPY (1),
		HADDR_1 (tcb_head.sval),
		RADDR_1 (sxfer),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		HADDR_1 (msgin),
	SCR_JUMP ^ IFTRUE (MASK (0x80, 0xbf)),
		PADDR_A (resel_lun0),
	SCR_INT ^ IFFALSE (MASK (0x80, 0x80)),
		SIR_RESEL_NO_IDENTIFY,
	SCR_COPY (4),
		HADDR_1 (tcb_head.luntbl_sa),
		RADDR_1 (dsa),
	SCR_SFBR_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_AND, 0xfc),
		0,
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a90),
	SCR_COPY (4),
},{
		0,
		RADDR_1 (dsa),
	SCR_JUMPR,
		12,
},{
	SCR_COPY (4),
		HADDR_1 (tcb_head.lun0_sa),
		RADDR_1 (dsa),
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a100),
	SCR_COPY (4),
},{
		0,
		RADDR_1 (temp),
	SCR_RETURN,
		0,
	
},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (2) ^ SCR_MSG_IN,
		HADDR_1 (msgin),
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a110),
	SCR_COPY (sizeof(struct sym_lcbh)),
},{
		0,
		HADDR_1 (lcb_head),
	SCR_COPY (4),
		HADDR_1 (lcb_head.itlq_tbl_sa),
		RADDR_1 (dsa),
	SCR_REG_SFBR (sidl, SCR_SHL, 0),
		0,
#if SYM_CONF_MAX_TASK*4 > 512
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 2),
		0,
	SCR_REG_REG (sfbr, SCR_SHL, 0),
		0,
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 1),
		0,
#elif SYM_CONF_MAX_TASK*4 > 256
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 1),
		0,
#endif
	SCR_SFBR_REG (dsa, SCR_AND, 0xfc),
		0,
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a120),
	SCR_COPY (4),
},{
		0,
		RADDR_1 (dsa),
},{
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a130),
	SCR_COPY (8),
},{
		0,
		PADDR_B (scratch),
	SCR_COPY (4),
		PADDR_B (scratch1), 
		RADDR_1 (temp),
	SCR_RETURN,
		0,
	
},{
	SCR_CLR (SCR_ACK),
		0,
},{
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a140),
	SCR_COPY (sizeof(struct sym_ccbh)),
},{
		0,
		HADDR_1 (ccb_head),
	SCR_COPY (4),
		HADDR_1 (ccb_head.status),
		RADDR_1 (scr0),
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_COPY (4),
		RADDR_1 (dsa),
		PADDR_A (_sms_a145),
	SCR_COPY (sizeof(struct sym_lcbh)),
},{
		0,
		HADDR_1 (lcb_head),
	SCR_COPY (4),
		HADDR_1 (lcb_head.itl_task_sa),
		RADDR_1 (dsa),
	SCR_JUMP,
		PADDR_A (resel_go),
},{
0
},{
	SCR_CALL,
		PADDR_A (datai_done),
	SCR_JUMP,
		PADDR_B (data_ovrun),
},{
0
},{
	SCR_CALL,
		PADDR_A (datao_done),
	SCR_JUMP,
		PADDR_B (data_ovrun),
},{
	SCR_FROM_REG (HF_REG),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDR_A (pm0_data_out),
	SCR_JUMP ^ IFFALSE (MASK (HF_DATA_IN, HF_DATA_IN)),
		PADDR_B (data_ovrun),
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM0),
		0,
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct sym_ccb, phys.pm0.sg),
	SCR_JUMP,
		PADDR_A (pm0_data_end),
},{
	SCR_JUMP ^ IFTRUE (MASK (HF_DATA_IN, HF_DATA_IN)),
		PADDR_B (data_ovrun),
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM0),
		0,
	SCR_CHMOV_TBL ^ SCR_DATA_OUT,
		offsetof (struct sym_ccb, phys.pm0.sg),
},{
	SCR_REG_REG (HF_REG, SCR_AND, (~HF_IN_PM0)),
		0,
	SCR_COPY (4),
		RADDR_1 (dsa),
		RADDR_1 (scratcha),
	SCR_REG_REG (scratcha, SCR_ADD, offsetof (struct sym_ccb,phys.pm0.ret)),
		0,
},{
	SCR_COPY (4),
		RADDR_1 (scratcha),
		PADDR_A (_sms_a150),
	SCR_COPY (4),
},{
		0,
		RADDR_1 (temp),
	SCR_RETURN,
		0,
},{
	SCR_FROM_REG (HF_REG),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDR_A (pm1_data_out),
	SCR_JUMP ^ IFFALSE (MASK (HF_DATA_IN, HF_DATA_IN)),
		PADDR_B (data_ovrun),
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM1),
		0,
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct sym_ccb, phys.pm1.sg),
	SCR_JUMP,
		PADDR_A (pm1_data_end),
},{
	SCR_JUMP ^ IFTRUE (MASK (HF_DATA_IN, HF_DATA_IN)),
		PADDR_B (data_ovrun),
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM1),
		0,
	SCR_CHMOV_TBL ^ SCR_DATA_OUT,
		offsetof (struct sym_ccb, phys.pm1.sg),
},{
	SCR_REG_REG (HF_REG, SCR_AND, (~HF_IN_PM1)),
		0,
	SCR_COPY (4),
		RADDR_1 (dsa),
		RADDR_1 (scratcha),
	SCR_REG_REG (scratcha, SCR_ADD, offsetof (struct sym_ccb,phys.pm1.ret)),
		0,
	SCR_JUMP,
		PADDR_A (pm_data_end),
}
};

static struct SYM_FWB_SCR SYM_FWB_SCR = {
 {
	SCR_JUMP,
		PADDR_B (data_ovrun),
},{

#ifdef SYM_CONF_TARGET_ROLE_SUPPORT
	SCR_CLR (SCR_TRG),
		0,
#endif
	SCR_SEL_TBL_ATN ^ offsetof (struct sym_hcb, abrt_sel),
		PADDR_A (reselect),
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		-8,
	SCR_INT,
		SIR_TARGET_SELECTED,
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct sym_hcb, abrt_tbl),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_INT,
		SIR_ABORT_SENT,
},{
	SCR_JUMP,
		PADDR_A (start),
},{
	SCR_JUMP ^ IFTRUE (DATA (M_EXTENDED)),
		PADDR_B (msg_extended),
	SCR_JUMP ^ IFTRUE (MASK (0x00, 0xf0)),
		PADDR_B (msg_received),
	SCR_JUMP ^ IFTRUE (MASK (0x10, 0xf0)),
		PADDR_B (msg_received),
	SCR_JUMP ^ IFFALSE (MASK (0x20, 0xf0)),
		PADDR_B (msg_weird_seen),
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		HADDR_1 (msgin[1]),
},{
	SCR_COPY (4),			
		HADDR_1 (scratch),
		RADDR_1 (scratcha),
	SCR_INT,
		SIR_MSG_RECEIVED,
},{
	SCR_COPY (4),			
		HADDR_1 (scratch),
		RADDR_1 (scratcha),
	SCR_INT,
		SIR_MSG_WEIRD,
},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		HADDR_1 (msgin[1]),
	SCR_JUMP ^ IFTRUE (DATA (0)),
		PADDR_B (msg_weird_seen),
	SCR_TO_REG (scratcha),
		0,
	SCR_REG_REG (sfbr, SCR_ADD, (256-8)),
		0,
	SCR_JUMP ^ IFTRUE (CARRYSET),
		PADDR_B (msg_weird_seen),
	SCR_COPY (1),
		RADDR_1 (scratcha),
		PADDR_B (_sms_b10),
	SCR_CLR (SCR_ACK),
		0,
},{
	SCR_MOVE_ABS (0) ^ SCR_MSG_IN,
		HADDR_1 (msgin[2]),
	SCR_JUMP,
		PADDR_B (msg_received),
},{
	SCR_INT,
		SIR_REJECT_TO_SEND,
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR_A (clrack),
},{
	SCR_INT,
		SIR_REJECT_TO_SEND,
	SCR_SET (SCR_ATN),
		0,
},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR_A (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		HADDR_1 (scratch),
	SCR_JUMP,
		PADDR_B (msg_weird1),
},{
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDR_B (nego_bad_phase),
},{
	SCR_MOVE_ABS (4) ^ SCR_MSG_OUT,
		HADDR_1 (msgout),
	SCR_JUMP,
		PADDR_B (msg_out_done),
},{
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDR_B (nego_bad_phase),
},{
	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		HADDR_1 (msgout),
	SCR_JUMP,
		PADDR_B (msg_out_done),
},{
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDR_B (nego_bad_phase),
},{
	SCR_MOVE_ABS (8) ^ SCR_MSG_OUT,
		HADDR_1 (msgout),
	SCR_JUMP,
		PADDR_B (msg_out_done),
},{
	SCR_INT,
		SIR_NEGO_PROTO,
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		HADDR_1 (msgout),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDR_B (msg_out),
},{
	SCR_INT,
		SIR_MSG_OUT_DONE,
	SCR_JUMP,
		PADDR_A (dispatch),
},{
	SCR_COPY (4),
		PADDR_B (zero),
		RADDR_1 (scratcha),
},{
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_OUT)),
		16,
	SCR_CHMOV_ABS (1) ^ SCR_DATA_OUT,
		HADDR_1 (scratch),
	SCR_JUMP,
		PADDR_B (data_ovrun2),
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (WSR, WSR)),
		16,
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	SCR_JUMP,
		PADDR_B (data_ovrun2),
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_DATA_IN)),
		16,
	SCR_INT,
		SIR_DATA_OVERRUN,
	SCR_JUMP,
		PADDR_A (dispatch),
	SCR_CHMOV_ABS (1) ^ SCR_DATA_IN,
		HADDR_1 (scratch),
},{
	SCR_REG_REG (scratcha,  SCR_ADD,  0x01),
		0,
	SCR_REG_REG (scratcha1, SCR_ADDC, 0),
		0,
	SCR_REG_REG (scratcha2, SCR_ADDC, 0),
		0,
	SCR_JUMP,
		PADDR_B (data_ovrun1),
},{
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		HADDR_1 (msgout),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_INT,
		SIR_RESEL_ABORTED,
	SCR_JUMP,
		PADDR_A (start),
},{
	SCR_SET (SCR_ATN), 
		0,         
	SCR_JUMP,
		PADDR_A (send_ident),
},{
	SCR_CLR (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR_A (select2),
},{
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR_A (select2),
},{
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct sym_dsb, sense),
	SCR_CALL,
		PADDR_A (datai_done),
	SCR_JUMP,
		PADDR_B (data_ovrun),
},{
	SCR_INT,
		SIR_RESEL_BAD_LUN,
	SCR_JUMP,
		PADDR_B (abort_resel),
},{
	SCR_INT,
		SIR_RESEL_BAD_I_T_L,
	SCR_JUMP,
		PADDR_B (abort_resel),
},{
	SCR_INT,
		SIR_RESEL_BAD_I_T_L_Q,
	SCR_JUMP,
		PADDR_B (abort_resel),
},{
	SCR_COPY (4),
		PADDR_B (startpos),
		RADDR_1 (scratcha),
	SCR_INT ^ IFFALSE (DATA (S_COND_MET)),
		SIR_BAD_SCSI_STATUS,
	SCR_RETURN,
		0,
},{
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct sym_ccb, phys.wresid),
	SCR_JUMP,
		PADDR_A (dispatch),

},{
	SCR_DATA_ZERO,
},{
	SCR_DATA_ZERO, 
},{
	SCR_DATA_ZERO,
},{
	SCR_DATA_ZERO, 
},{
	SCR_DATA_ZERO,
},{
	SCR_DATA_ZERO, 
},{
	SCR_DATA_ZERO,
},{
	SCR_DATA_ZERO,
}
};

static struct SYM_FWZ_SCR SYM_FWZ_SCR = {
 {
	SCR_COPY (4),
		HADDR_1 (scratch),
		RADDR_1 (scratcha),
	SCR_COPY (4),
		RADDR_1 (temp),
		HADDR_1 (scratch),
	SCR_COPY (4),
		HADDR_1 (scratch),
		RADDR_1 (temp),
},{
	SCR_INT,
		99,
}
};
