/*
 * zfcp device driver
 *
 * Fibre Channel related definitions and inline functions for the zfcp
 * device driver
 *
 * Copyright IBM Corporation 2009
 */

#ifndef ZFCP_FC_H
#define ZFCP_FC_H

#include <scsi/fc/fc_els.h>
#include <scsi/fc/fc_fcp.h>
#include <scsi/fc/fc_ns.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_tcq.h>
#include "zfcp_fsf.h"

#define ZFCP_FC_CT_SIZE_PAGE	  (PAGE_SIZE - sizeof(struct fc_ct_hdr))
#define ZFCP_FC_GPN_FT_ENT_PAGE	  (ZFCP_FC_CT_SIZE_PAGE \
					/ sizeof(struct fc_gpn_ft_resp))
#define ZFCP_FC_GPN_FT_NUM_BUFS	  4 

#define ZFCP_FC_GPN_FT_MAX_SIZE	  (ZFCP_FC_GPN_FT_NUM_BUFS * PAGE_SIZE \
					- sizeof(struct fc_ct_hdr))
#define ZFCP_FC_GPN_FT_MAX_ENT	  (ZFCP_FC_GPN_FT_NUM_BUFS * \
					(ZFCP_FC_GPN_FT_ENT_PAGE + 1))

#define ZFCP_FC_CTELS_TMO	(2 * FC_DEF_R_A_TOV / 1000)

struct zfcp_fc_event {
	enum fc_host_event_code code;
	u32 data;
	struct list_head list;
};

struct zfcp_fc_events {
	struct list_head list;
	spinlock_t list_lock;
	struct work_struct work;
};

struct zfcp_fc_gid_pn_req {
	struct fc_ct_hdr	ct_hdr;
	struct fc_ns_gid_pn	gid_pn;
} __packed;

struct zfcp_fc_gid_pn_rsp {
	struct fc_ct_hdr	ct_hdr;
	struct fc_gid_pn_resp	gid_pn;
} __packed;

struct zfcp_fc_gpn_ft_req {
	struct fc_ct_hdr	ct_hdr;
	struct fc_ns_gid_ft	gpn_ft;
} __packed;

struct zfcp_fc_gspn_req {
	struct fc_ct_hdr	ct_hdr;
	struct fc_gid_pn_resp	gspn;
} __packed;

struct zfcp_fc_gspn_rsp {
	struct fc_ct_hdr	ct_hdr;
	struct fc_gspn_resp	gspn;
	char			name[FC_SYMBOLIC_NAME_SIZE];
} __packed;

struct zfcp_fc_rspn_req {
	struct fc_ct_hdr	ct_hdr;
	struct fc_ns_rspn	rspn;
	char			name[FC_SYMBOLIC_NAME_SIZE];
} __packed;

struct zfcp_fc_req {
	struct zfcp_fsf_ct_els				ct_els;
	struct scatterlist				sg_req;
	struct scatterlist				sg_rsp;
	union {
		struct {
			struct fc_els_adisc		req;
			struct fc_els_adisc		rsp;
		} adisc;
		struct {
			struct zfcp_fc_gid_pn_req	req;
			struct zfcp_fc_gid_pn_rsp	rsp;
		} gid_pn;
		struct {
			struct scatterlist sg_rsp2[ZFCP_FC_GPN_FT_NUM_BUFS - 1];
			struct zfcp_fc_gpn_ft_req	req;
		} gpn_ft;
		struct {
			struct zfcp_fc_gspn_req		req;
			struct zfcp_fc_gspn_rsp		rsp;
		} gspn;
		struct {
			struct zfcp_fc_rspn_req		req;
			struct fc_ct_hdr		rsp;
		} rspn;
	} u;
};

enum zfcp_fc_wka_status {
	ZFCP_FC_WKA_PORT_OFFLINE,
	ZFCP_FC_WKA_PORT_CLOSING,
	ZFCP_FC_WKA_PORT_OPENING,
	ZFCP_FC_WKA_PORT_ONLINE,
};

struct zfcp_fc_wka_port {
	struct zfcp_adapter	*adapter;
	wait_queue_head_t	completion_wq;
	enum zfcp_fc_wka_status	status;
	atomic_t		refcount;
	u32			d_id;
	u32			handle;
	struct mutex		mutex;
	struct delayed_work	work;
};

struct zfcp_fc_wka_ports {
	struct zfcp_fc_wka_port ms;
	struct zfcp_fc_wka_port ts;
	struct zfcp_fc_wka_port ds;
	struct zfcp_fc_wka_port as;
};

static inline
void zfcp_fc_scsi_to_fcp(struct fcp_cmnd *fcp, struct scsi_cmnd *scsi,
			 u8 tm_flags)
{
	char tag[2];

	int_to_scsilun(scsi->device->lun, (struct scsi_lun *) &fcp->fc_lun);

	if (unlikely(tm_flags)) {
		fcp->fc_tm_flags = tm_flags;
		return;
	}

	if (scsi_populate_tag_msg(scsi, tag)) {
		switch (tag[0]) {
		case MSG_ORDERED_TAG:
			fcp->fc_pri_ta |= FCP_PTA_ORDERED;
			break;
		case MSG_SIMPLE_TAG:
			fcp->fc_pri_ta |= FCP_PTA_SIMPLE;
			break;
		};
	} else
		fcp->fc_pri_ta = FCP_PTA_SIMPLE;

	if (scsi->sc_data_direction == DMA_FROM_DEVICE)
		fcp->fc_flags |= FCP_CFL_RDDATA;
	if (scsi->sc_data_direction == DMA_TO_DEVICE)
		fcp->fc_flags |= FCP_CFL_WRDATA;

	memcpy(fcp->fc_cdb, scsi->cmnd, scsi->cmd_len);

	fcp->fc_dl = scsi_bufflen(scsi);

	if (scsi_get_prot_type(scsi) == SCSI_PROT_DIF_TYPE1)
		fcp->fc_dl += fcp->fc_dl / scsi->device->sector_size * 8;
}

static inline
void zfcp_fc_eval_fcp_rsp(struct fcp_resp_with_ext *fcp_rsp,
			  struct scsi_cmnd *scsi)
{
	struct fcp_resp_rsp_info *rsp_info;
	char *sense;
	u32 sense_len, resid;
	u8 rsp_flags;

	set_msg_byte(scsi, COMMAND_COMPLETE);
	scsi->result |= fcp_rsp->resp.fr_status;

	rsp_flags = fcp_rsp->resp.fr_flags;

	if (unlikely(rsp_flags & FCP_RSP_LEN_VAL)) {
		rsp_info = (struct fcp_resp_rsp_info *) &fcp_rsp[1];
		if (rsp_info->rsp_code == FCP_TMF_CMPL)
			set_host_byte(scsi, DID_OK);
		else {
			set_host_byte(scsi, DID_ERROR);
			return;
		}
	}

	if (unlikely(rsp_flags & FCP_SNS_LEN_VAL)) {
		sense = (char *) &fcp_rsp[1];
		if (rsp_flags & FCP_RSP_LEN_VAL)
			sense += fcp_rsp->ext.fr_rsp_len;
		sense_len = min(fcp_rsp->ext.fr_sns_len,
				(u32) SCSI_SENSE_BUFFERSIZE);
		memcpy(scsi->sense_buffer, sense, sense_len);
	}

	if (unlikely(rsp_flags & FCP_RESID_UNDER)) {
		resid = fcp_rsp->ext.fr_resid;
		scsi_set_resid(scsi, resid);
		if (scsi_bufflen(scsi) - resid < scsi->underflow &&
		     !(rsp_flags & FCP_SNS_LEN_VAL) &&
		     fcp_rsp->resp.fr_status == SAM_STAT_GOOD)
			set_host_byte(scsi, DID_ERROR);
	}
}

#endif
