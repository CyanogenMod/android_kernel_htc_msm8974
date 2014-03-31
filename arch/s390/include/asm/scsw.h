/*
 *  Helper functions for scsw access.
 *
 *    Copyright IBM Corp. 2008,2009
 *    Author(s): Peter Oberparleiter <peter.oberparleiter@de.ibm.com>
 */

#ifndef _ASM_S390_SCSW_H_
#define _ASM_S390_SCSW_H_

#include <linux/types.h>
#include <asm/chsc.h>
#include <asm/cio.h>

struct cmd_scsw {
	__u32 key  : 4;
	__u32 sctl : 1;
	__u32 eswf : 1;
	__u32 cc   : 2;
	__u32 fmt  : 1;
	__u32 pfch : 1;
	__u32 isic : 1;
	__u32 alcc : 1;
	__u32 ssi  : 1;
	__u32 zcc  : 1;
	__u32 ectl : 1;
	__u32 pno  : 1;
	__u32 res  : 1;
	__u32 fctl : 3;
	__u32 actl : 7;
	__u32 stctl : 5;
	__u32 cpa;
	__u32 dstat : 8;
	__u32 cstat : 8;
	__u32 count : 16;
} __attribute__ ((packed));

struct tm_scsw {
	u32 key:4;
	u32 :1;
	u32 eswf:1;
	u32 cc:2;
	u32 fmt:3;
	u32 x:1;
	u32 q:1;
	u32 :1;
	u32 ectl:1;
	u32 pno:1;
	u32 :1;
	u32 fctl:3;
	u32 actl:7;
	u32 stctl:5;
	u32 tcw;
	u32 dstat:8;
	u32 cstat:8;
	u32 fcxs:8;
	u32 schxs:8;
} __attribute__ ((packed));

union scsw {
	struct cmd_scsw cmd;
	struct tm_scsw tm;
} __attribute__ ((packed));

#define SCSW_FCTL_CLEAR_FUNC	 0x1
#define SCSW_FCTL_HALT_FUNC	 0x2
#define SCSW_FCTL_START_FUNC	 0x4

#define SCSW_ACTL_SUSPENDED	 0x1
#define SCSW_ACTL_DEVACT	 0x2
#define SCSW_ACTL_SCHACT	 0x4
#define SCSW_ACTL_CLEAR_PEND	 0x8
#define SCSW_ACTL_HALT_PEND	 0x10
#define SCSW_ACTL_START_PEND	 0x20
#define SCSW_ACTL_RESUME_PEND	 0x40

#define SCSW_STCTL_STATUS_PEND	 0x1
#define SCSW_STCTL_SEC_STATUS	 0x2
#define SCSW_STCTL_PRIM_STATUS	 0x4
#define SCSW_STCTL_INTER_STATUS	 0x8
#define SCSW_STCTL_ALERT_STATUS	 0x10

#define DEV_STAT_ATTENTION	 0x80
#define DEV_STAT_STAT_MOD	 0x40
#define DEV_STAT_CU_END		 0x20
#define DEV_STAT_BUSY		 0x10
#define DEV_STAT_CHN_END	 0x08
#define DEV_STAT_DEV_END	 0x04
#define DEV_STAT_UNIT_CHECK	 0x02
#define DEV_STAT_UNIT_EXCEP	 0x01

#define SCHN_STAT_PCI		 0x80
#define SCHN_STAT_INCORR_LEN	 0x40
#define SCHN_STAT_PROG_CHECK	 0x20
#define SCHN_STAT_PROT_CHECK	 0x10
#define SCHN_STAT_CHN_DATA_CHK	 0x08
#define SCHN_STAT_CHN_CTRL_CHK	 0x04
#define SCHN_STAT_INTF_CTRL_CHK	 0x02
#define SCHN_STAT_CHAIN_CHECK	 0x01

#define SNS0_CMD_REJECT		0x80
#define SNS_CMD_REJECT		SNS0_CMD_REJEC
#define SNS0_INTERVENTION_REQ	0x40
#define SNS0_BUS_OUT_CHECK	0x20
#define SNS0_EQUIPMENT_CHECK	0x10
#define SNS0_DATA_CHECK		0x08
#define SNS0_OVERRUN		0x04
#define SNS0_INCOMPL_DOMAIN	0x01

#define SNS1_PERM_ERR		0x80
#define SNS1_INV_TRACK_FORMAT	0x40
#define SNS1_EOC		0x20
#define SNS1_MESSAGE_TO_OPER	0x10
#define SNS1_NO_REC_FOUND	0x08
#define SNS1_FILE_PROTECTED	0x04
#define SNS1_WRITE_INHIBITED	0x02
#define SNS1_INPRECISE_END	0x01

#define SNS2_REQ_INH_WRITE	0x80
#define SNS2_CORRECTABLE	0x40
#define SNS2_FIRST_LOG_ERR	0x20
#define SNS2_ENV_DATA_PRESENT	0x10
#define SNS2_INPRECISE_END	0x04

static inline int scsw_is_tm(union scsw *scsw)
{
	return css_general_characteristics.fcx && (scsw->tm.x == 1);
}

static inline u32 scsw_key(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.key;
	else
		return scsw->cmd.key;
}

static inline u32 scsw_eswf(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.eswf;
	else
		return scsw->cmd.eswf;
}

static inline u32 scsw_cc(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.cc;
	else
		return scsw->cmd.cc;
}

static inline u32 scsw_ectl(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.ectl;
	else
		return scsw->cmd.ectl;
}

static inline u32 scsw_pno(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.pno;
	else
		return scsw->cmd.pno;
}

static inline u32 scsw_fctl(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.fctl;
	else
		return scsw->cmd.fctl;
}

static inline u32 scsw_actl(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.actl;
	else
		return scsw->cmd.actl;
}

static inline u32 scsw_stctl(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.stctl;
	else
		return scsw->cmd.stctl;
}

static inline u32 scsw_dstat(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.dstat;
	else
		return scsw->cmd.dstat;
}

static inline u32 scsw_cstat(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw->tm.cstat;
	else
		return scsw->cmd.cstat;
}

static inline int scsw_cmd_is_valid_key(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC);
}

static inline int scsw_cmd_is_valid_sctl(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC);
}

static inline int scsw_cmd_is_valid_eswf(union scsw *scsw)
{
	return (scsw->cmd.stctl & SCSW_STCTL_STATUS_PEND);
}

static inline int scsw_cmd_is_valid_cc(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC) &&
	       (scsw->cmd.stctl & SCSW_STCTL_STATUS_PEND);
}

static inline int scsw_cmd_is_valid_fmt(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC);
}

static inline int scsw_cmd_is_valid_pfch(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC);
}

static inline int scsw_cmd_is_valid_isic(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC);
}

static inline int scsw_cmd_is_valid_alcc(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC);
}

static inline int scsw_cmd_is_valid_ssi(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC);
}

static inline int scsw_cmd_is_valid_zcc(union scsw *scsw)
{
	return (scsw->cmd.fctl & SCSW_FCTL_START_FUNC) &&
	       (scsw->cmd.stctl & SCSW_STCTL_INTER_STATUS);
}

static inline int scsw_cmd_is_valid_ectl(union scsw *scsw)
{
	return (scsw->cmd.stctl & SCSW_STCTL_STATUS_PEND) &&
	       !(scsw->cmd.stctl & SCSW_STCTL_INTER_STATUS) &&
	       (scsw->cmd.stctl & SCSW_STCTL_ALERT_STATUS);
}

static inline int scsw_cmd_is_valid_pno(union scsw *scsw)
{
	return (scsw->cmd.fctl != 0) &&
	       (scsw->cmd.stctl & SCSW_STCTL_STATUS_PEND) &&
	       (!(scsw->cmd.stctl & SCSW_STCTL_INTER_STATUS) ||
		 ((scsw->cmd.stctl & SCSW_STCTL_INTER_STATUS) &&
		  (scsw->cmd.actl & SCSW_ACTL_SUSPENDED)));
}

static inline int scsw_cmd_is_valid_fctl(union scsw *scsw)
{
	
	return 1;
}

static inline int scsw_cmd_is_valid_actl(union scsw *scsw)
{
	
	return 1;
}

static inline int scsw_cmd_is_valid_stctl(union scsw *scsw)
{
	
	return 1;
}

static inline int scsw_cmd_is_valid_dstat(union scsw *scsw)
{
	return (scsw->cmd.stctl & SCSW_STCTL_STATUS_PEND) &&
	       (scsw->cmd.cc != 3);
}

static inline int scsw_cmd_is_valid_cstat(union scsw *scsw)
{
	return (scsw->cmd.stctl & SCSW_STCTL_STATUS_PEND) &&
	       (scsw->cmd.cc != 3);
}

static inline int scsw_tm_is_valid_key(union scsw *scsw)
{
	return (scsw->tm.fctl & SCSW_FCTL_START_FUNC);
}

static inline int scsw_tm_is_valid_eswf(union scsw *scsw)
{
	return (scsw->tm.stctl & SCSW_STCTL_STATUS_PEND);
}

static inline int scsw_tm_is_valid_cc(union scsw *scsw)
{
	return (scsw->tm.fctl & SCSW_FCTL_START_FUNC) &&
	       (scsw->tm.stctl & SCSW_STCTL_STATUS_PEND);
}

static inline int scsw_tm_is_valid_fmt(union scsw *scsw)
{
	return 1;
}

static inline int scsw_tm_is_valid_x(union scsw *scsw)
{
	return 1;
}

static inline int scsw_tm_is_valid_q(union scsw *scsw)
{
	return 1;
}

static inline int scsw_tm_is_valid_ectl(union scsw *scsw)
{
	return (scsw->tm.stctl & SCSW_STCTL_STATUS_PEND) &&
	       !(scsw->tm.stctl & SCSW_STCTL_INTER_STATUS) &&
	       (scsw->tm.stctl & SCSW_STCTL_ALERT_STATUS);
}

static inline int scsw_tm_is_valid_pno(union scsw *scsw)
{
	return (scsw->tm.fctl != 0) &&
	       (scsw->tm.stctl & SCSW_STCTL_STATUS_PEND) &&
	       (!(scsw->tm.stctl & SCSW_STCTL_INTER_STATUS) ||
		 ((scsw->tm.stctl & SCSW_STCTL_INTER_STATUS) &&
		  (scsw->tm.actl & SCSW_ACTL_SUSPENDED)));
}

static inline int scsw_tm_is_valid_fctl(union scsw *scsw)
{
	
	return 1;
}

static inline int scsw_tm_is_valid_actl(union scsw *scsw)
{
	
	return 1;
}

static inline int scsw_tm_is_valid_stctl(union scsw *scsw)
{
	
	return 1;
}

static inline int scsw_tm_is_valid_dstat(union scsw *scsw)
{
	return (scsw->tm.stctl & SCSW_STCTL_STATUS_PEND) &&
	       (scsw->tm.cc != 3);
}

static inline int scsw_tm_is_valid_cstat(union scsw *scsw)
{
	return (scsw->tm.stctl & SCSW_STCTL_STATUS_PEND) &&
	       (scsw->tm.cc != 3);
}

static inline int scsw_tm_is_valid_fcxs(union scsw *scsw)
{
	return 1;
}

static inline int scsw_tm_is_valid_schxs(union scsw *scsw)
{
	return (scsw->tm.cstat & (SCHN_STAT_PROG_CHECK |
				  SCHN_STAT_INTF_CTRL_CHK |
				  SCHN_STAT_PROT_CHECK |
				  SCHN_STAT_CHN_DATA_CHK));
}

static inline int scsw_is_valid_actl(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_actl(scsw);
	else
		return scsw_cmd_is_valid_actl(scsw);
}

static inline int scsw_is_valid_cc(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_cc(scsw);
	else
		return scsw_cmd_is_valid_cc(scsw);
}

static inline int scsw_is_valid_cstat(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_cstat(scsw);
	else
		return scsw_cmd_is_valid_cstat(scsw);
}

static inline int scsw_is_valid_dstat(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_dstat(scsw);
	else
		return scsw_cmd_is_valid_dstat(scsw);
}

static inline int scsw_is_valid_ectl(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_ectl(scsw);
	else
		return scsw_cmd_is_valid_ectl(scsw);
}

static inline int scsw_is_valid_eswf(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_eswf(scsw);
	else
		return scsw_cmd_is_valid_eswf(scsw);
}

static inline int scsw_is_valid_fctl(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_fctl(scsw);
	else
		return scsw_cmd_is_valid_fctl(scsw);
}

static inline int scsw_is_valid_key(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_key(scsw);
	else
		return scsw_cmd_is_valid_key(scsw);
}

static inline int scsw_is_valid_pno(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_pno(scsw);
	else
		return scsw_cmd_is_valid_pno(scsw);
}

static inline int scsw_is_valid_stctl(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_valid_stctl(scsw);
	else
		return scsw_cmd_is_valid_stctl(scsw);
}

static inline int scsw_cmd_is_solicited(union scsw *scsw)
{
	return (scsw->cmd.cc != 0) || (scsw->cmd.stctl !=
		(SCSW_STCTL_STATUS_PEND | SCSW_STCTL_ALERT_STATUS));
}

static inline int scsw_tm_is_solicited(union scsw *scsw)
{
	return (scsw->tm.cc != 0) || (scsw->tm.stctl !=
		(SCSW_STCTL_STATUS_PEND | SCSW_STCTL_ALERT_STATUS));
}

static inline int scsw_is_solicited(union scsw *scsw)
{
	if (scsw_is_tm(scsw))
		return scsw_tm_is_solicited(scsw);
	else
		return scsw_cmd_is_solicited(scsw);
}

#endif 
