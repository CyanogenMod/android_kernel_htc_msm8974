#include "qlge.h"

int ql_unpause_mpi_risc(struct ql_adapter *qdev)
{
	u32 tmp;

	
	tmp = ql_read32(qdev, CSR);
	if (!(tmp & CSR_RP))
		return -EIO;

	ql_write32(qdev, CSR, CSR_CMD_CLR_PAUSE);
	return 0;
}

int ql_pause_mpi_risc(struct ql_adapter *qdev)
{
	u32 tmp;
	int count = UDELAY_COUNT;

	
	ql_write32(qdev, CSR, CSR_CMD_SET_PAUSE);
	do {
		tmp = ql_read32(qdev, CSR);
		if (tmp & CSR_RP)
			break;
		mdelay(UDELAY_DELAY);
		count--;
	} while (count);
	return (count == 0) ? -ETIMEDOUT : 0;
}

int ql_hard_reset_mpi_risc(struct ql_adapter *qdev)
{
	u32 tmp;
	int count = UDELAY_COUNT;

	
	ql_write32(qdev, CSR, CSR_CMD_SET_RST);
	do {
		tmp = ql_read32(qdev, CSR);
		if (tmp & CSR_RR) {
			ql_write32(qdev, CSR, CSR_CMD_CLR_RST);
			break;
		}
		mdelay(UDELAY_DELAY);
		count--;
	} while (count);
	return (count == 0) ? -ETIMEDOUT : 0;
}

int ql_read_mpi_reg(struct ql_adapter *qdev, u32 reg, u32 *data)
{
	int status;
	
	status = ql_wait_reg_rdy(qdev, PROC_ADDR, PROC_ADDR_RDY, PROC_ADDR_ERR);
	if (status)
		goto exit;
	
	ql_write32(qdev, PROC_ADDR, reg | PROC_ADDR_R);
	
	status = ql_wait_reg_rdy(qdev, PROC_ADDR, PROC_ADDR_RDY, PROC_ADDR_ERR);
	if (status)
		goto exit;
	
	*data = ql_read32(qdev, PROC_DATA);
exit:
	return status;
}

int ql_write_mpi_reg(struct ql_adapter *qdev, u32 reg, u32 data)
{
	int status = 0;
	
	status = ql_wait_reg_rdy(qdev, PROC_ADDR, PROC_ADDR_RDY, PROC_ADDR_ERR);
	if (status)
		goto exit;
	
	ql_write32(qdev, PROC_DATA, data);
	
	ql_write32(qdev, PROC_ADDR, reg);
	
	status = ql_wait_reg_rdy(qdev, PROC_ADDR, PROC_ADDR_RDY, PROC_ADDR_ERR);
	if (status)
		goto exit;
exit:
	return status;
}

int ql_soft_reset_mpi_risc(struct ql_adapter *qdev)
{
	int status;
	status = ql_write_mpi_reg(qdev, 0x00001010, 1);
	return status;
}

int ql_own_firmware(struct ql_adapter *qdev)
{
	u32 temp;

	if (qdev->func < qdev->alt_func)
		return 1;

	temp =  ql_read32(qdev, STS);
	if (!(temp & (1 << (8 + qdev->alt_func))))
		return 1;

	return 0;

}

static int ql_get_mb_sts(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int i, status;

	status = ql_sem_spinlock(qdev, SEM_PROC_REG_MASK);
	if (status)
		return -EBUSY;
	for (i = 0; i < mbcp->out_count; i++) {
		status =
		    ql_read_mpi_reg(qdev, qdev->mailbox_out + i,
				     &mbcp->mbox_out[i]);
		if (status) {
			netif_err(qdev, drv, qdev->ndev, "Failed mailbox read.\n");
			break;
		}
	}
	ql_sem_unlock(qdev, SEM_PROC_REG_MASK);	
	return status;
}

static int ql_wait_mbx_cmd_cmplt(struct ql_adapter *qdev)
{
	int count = 100;
	u32 value;

	do {
		value = ql_read32(qdev, STS);
		if (value & STS_PI)
			return 0;
		mdelay(UDELAY_DELAY); 
	} while (--count);
	return -ETIMEDOUT;
}

static int ql_exec_mb_cmd(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int i, status;

	if (ql_read32(qdev, CSR) & CSR_HRI)
		return -EIO;

	status = ql_sem_spinlock(qdev, SEM_PROC_REG_MASK);
	if (status)
		return status;

	for (i = 0; i < mbcp->in_count; i++) {
		status = ql_write_mpi_reg(qdev, qdev->mailbox_in + i,
						mbcp->mbox_in[i]);
		if (status)
			goto end;
	}
	ql_write32(qdev, CSR, CSR_CMD_SET_H2R_INT);
end:
	ql_sem_unlock(qdev, SEM_PROC_REG_MASK);
	return status;
}

static int ql_idc_req_aen(struct ql_adapter *qdev)
{
	int status;
	struct mbox_params *mbcp = &qdev->idc_mbc;

	netif_err(qdev, drv, qdev->ndev, "Enter!\n");
	mbcp = &qdev->idc_mbc;
	mbcp->out_count = 4;
	status = ql_get_mb_sts(qdev, mbcp);
	if (status) {
		netif_err(qdev, drv, qdev->ndev,
			  "Could not read MPI, resetting ASIC!\n");
		ql_queue_asic_error(qdev);
	} else	{
		ql_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16));
		queue_delayed_work(qdev->workqueue, &qdev->mpi_idc_work, 0);
	}
	return status;
}

static int ql_idc_cmplt_aen(struct ql_adapter *qdev)
{
	int status;
	struct mbox_params *mbcp = &qdev->idc_mbc;
	mbcp->out_count = 4;
	status = ql_get_mb_sts(qdev, mbcp);
	if (status) {
		netif_err(qdev, drv, qdev->ndev,
			  "Could not read MPI, resetting RISC!\n");
		ql_queue_fw_error(qdev);
	} else
		complete(&qdev->ide_completion);

	return status;
}

static void ql_link_up(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int status;
	mbcp->out_count = 2;

	status = ql_get_mb_sts(qdev, mbcp);
	if (status) {
		netif_err(qdev, drv, qdev->ndev,
			  "%s: Could not get mailbox status.\n", __func__);
		return;
	}

	qdev->link_status = mbcp->mbox_out[1];
	netif_err(qdev, drv, qdev->ndev, "Link Up.\n");

	if (test_bit(QL_CAM_RT_SET, &qdev->flags)) {
		status = ql_cam_route_initialize(qdev);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to init CAM/Routing tables.\n");
			return;
		} else
			clear_bit(QL_CAM_RT_SET, &qdev->flags);
	}

	if (!test_bit(QL_PORT_CFG, &qdev->flags)) {
		netif_err(qdev, drv, qdev->ndev, "Queue Port Config Worker!\n");
		set_bit(QL_PORT_CFG, &qdev->flags);
		ql_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16));
		queue_delayed_work(qdev->workqueue,
				&qdev->mpi_port_cfg_work, 0);
	}

	ql_link_on(qdev);
}

static void ql_link_down(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int status;

	mbcp->out_count = 3;

	status = ql_get_mb_sts(qdev, mbcp);
	if (status)
		netif_err(qdev, drv, qdev->ndev, "Link down AEN broken!\n");

	ql_link_off(qdev);
}

static int ql_sfp_in(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int status;

	mbcp->out_count = 5;

	status = ql_get_mb_sts(qdev, mbcp);
	if (status)
		netif_err(qdev, drv, qdev->ndev, "SFP in AEN broken!\n");
	else
		netif_err(qdev, drv, qdev->ndev, "SFP insertion detected.\n");

	return status;
}

static int ql_sfp_out(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int status;

	mbcp->out_count = 1;

	status = ql_get_mb_sts(qdev, mbcp);
	if (status)
		netif_err(qdev, drv, qdev->ndev, "SFP out AEN broken!\n");
	else
		netif_err(qdev, drv, qdev->ndev, "SFP removal detected.\n");

	return status;
}

static int ql_aen_lost(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int status;

	mbcp->out_count = 6;

	status = ql_get_mb_sts(qdev, mbcp);
	if (status)
		netif_err(qdev, drv, qdev->ndev, "Lost AEN broken!\n");
	else {
		int i;
		netif_err(qdev, drv, qdev->ndev, "Lost AEN detected.\n");
		for (i = 0; i < mbcp->out_count; i++)
			netif_err(qdev, drv, qdev->ndev, "mbox_out[%d] = 0x%.08x.\n",
				  i, mbcp->mbox_out[i]);

	}

	return status;
}

static void ql_init_fw_done(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int status;

	mbcp->out_count = 2;

	status = ql_get_mb_sts(qdev, mbcp);
	if (status) {
		netif_err(qdev, drv, qdev->ndev, "Firmware did not initialize!\n");
	} else {
		netif_err(qdev, drv, qdev->ndev, "Firmware Revision  = 0x%.08x.\n",
			  mbcp->mbox_out[1]);
		qdev->fw_rev_id = mbcp->mbox_out[1];
		status = ql_cam_route_initialize(qdev);
		if (status)
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to init CAM/Routing tables.\n");
	}
}

static int ql_mpi_handler(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int status;
	int orig_count = mbcp->out_count;

	
	mbcp->out_count = 1;
	status = ql_get_mb_sts(qdev, mbcp);
	if (status) {
		netif_err(qdev, drv, qdev->ndev,
			  "Could not read MPI, resetting ASIC!\n");
		ql_queue_asic_error(qdev);
		goto end;
	}

	switch (mbcp->mbox_out[0]) {

	case MB_CMD_STS_INTRMDT:
	case MB_CMD_STS_GOOD:
	case MB_CMD_STS_INVLD_CMD:
	case MB_CMD_STS_XFC_ERR:
	case MB_CMD_STS_CSUM_ERR:
	case MB_CMD_STS_ERR:
	case MB_CMD_STS_PARAM_ERR:
		mbcp->out_count = orig_count;
		status = ql_get_mb_sts(qdev, mbcp);
		return status;

	case AEN_IDC_REQ:
		status = ql_idc_req_aen(qdev);
		break;

	case AEN_IDC_CMPLT:
	case AEN_IDC_EXT:
		status = ql_idc_cmplt_aen(qdev);
		break;

	case AEN_LINK_UP:
		ql_link_up(qdev, mbcp);
		break;

	case AEN_LINK_DOWN:
		ql_link_down(qdev, mbcp);
		break;

	case AEN_FW_INIT_DONE:
		if (mbcp->mbox_in[0] == MB_CMD_EX_FW) {
			mbcp->out_count = orig_count;
			status = ql_get_mb_sts(qdev, mbcp);
			mbcp->mbox_out[0] = MB_CMD_STS_GOOD;
			return status;
		}
		ql_init_fw_done(qdev, mbcp);
		break;

	case AEN_AEN_SFP_IN:
		ql_sfp_in(qdev, mbcp);
		break;

	case AEN_AEN_SFP_OUT:
		ql_sfp_out(qdev, mbcp);
		break;

	case AEN_FW_INIT_FAIL:
		if (mbcp->mbox_in[0] == MB_CMD_EX_FW) {
			mbcp->out_count = orig_count;
			status = ql_get_mb_sts(qdev, mbcp);
			mbcp->mbox_out[0] = MB_CMD_STS_ERR;
			return status;
		}
		netif_err(qdev, drv, qdev->ndev,
			  "Firmware initialization failed.\n");
		status = -EIO;
		ql_queue_fw_error(qdev);
		break;

	case AEN_SYS_ERR:
		netif_err(qdev, drv, qdev->ndev, "System Error.\n");
		ql_queue_fw_error(qdev);
		status = -EIO;
		break;

	case AEN_AEN_LOST:
		ql_aen_lost(qdev, mbcp);
		break;

	case AEN_DCBX_CHG:
		
		break;
	default:
		netif_err(qdev, drv, qdev->ndev,
			  "Unsupported AE %.08x.\n", mbcp->mbox_out[0]);
		
	}
end:
	ql_write32(qdev, CSR, CSR_CMD_CLR_R2PCI_INT);
	mbcp->out_count = orig_count;
	return status;
}

static int ql_mailbox_command(struct ql_adapter *qdev, struct mbox_params *mbcp)
{
	int status;
	unsigned long count;

	mutex_lock(&qdev->mpi_mutex);

	
	ql_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16));

	
	status = ql_exec_mb_cmd(qdev, mbcp);
	if (status)
		goto end;


	if (mbcp->mbox_in[0] == MB_CMD_MAKE_SYS_ERR)
		goto end;

	count = jiffies + HZ * MAILBOX_TIMEOUT;
	do {
		
		status = ql_wait_mbx_cmd_cmplt(qdev);
		if (status)
			continue;

		status = ql_mpi_handler(qdev, mbcp);
		if (status)
			goto end;

		if (((mbcp->mbox_out[0] & 0x0000f000) ==
					MB_CMD_STS_GOOD) ||
			((mbcp->mbox_out[0] & 0x0000f000) ==
					MB_CMD_STS_INTRMDT))
			goto done;
	} while (time_before(jiffies, count));

	netif_err(qdev, drv, qdev->ndev,
		  "Timed out waiting for mailbox complete.\n");
	status = -ETIMEDOUT;
	goto end;

done:

	ql_write32(qdev, CSR, CSR_CMD_CLR_R2PCI_INT);

	if (((mbcp->mbox_out[0] & 0x0000f000) !=
					MB_CMD_STS_GOOD) &&
		((mbcp->mbox_out[0] & 0x0000f000) !=
					MB_CMD_STS_INTRMDT)) {
		status = -EIO;
	}
end:
	
	ql_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16) | INTR_MASK_PI);
	mutex_unlock(&qdev->mpi_mutex);
	return status;
}

int ql_mb_about_fw(struct ql_adapter *qdev)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status = 0;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 1;
	mbcp->out_count = 3;

	mbcp->mbox_in[0] = MB_CMD_ABOUT_FW;

	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev,
			  "Failed about firmware command\n");
		status = -EIO;
	}

	
	qdev->fw_rev_id = mbcp->mbox_out[1];

	return status;
}

int ql_mb_get_fw_state(struct ql_adapter *qdev)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status = 0;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 1;
	mbcp->out_count = 2;

	mbcp->mbox_in[0] = MB_CMD_GET_FW_STATE;

	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev,
			  "Failed Get Firmware State.\n");
		status = -EIO;
	}

	if (mbcp->mbox_out[1] & 1) {
		netif_err(qdev, drv, qdev->ndev,
			  "Firmware waiting for initialization.\n");
		status = -EIO;
	}

	return status;
}

static int ql_mb_idc_ack(struct ql_adapter *qdev)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status = 0;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 5;
	mbcp->out_count = 1;

	mbcp->mbox_in[0] = MB_CMD_IDC_ACK;
	mbcp->mbox_in[1] = qdev->idc_mbc.mbox_out[1];
	mbcp->mbox_in[2] = qdev->idc_mbc.mbox_out[2];
	mbcp->mbox_in[3] = qdev->idc_mbc.mbox_out[3];
	mbcp->mbox_in[4] = qdev->idc_mbc.mbox_out[4];

	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev, "Failed IDC ACK send.\n");
		status = -EIO;
	}
	return status;
}

int ql_mb_set_port_cfg(struct ql_adapter *qdev)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status = 0;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 3;
	mbcp->out_count = 1;

	mbcp->mbox_in[0] = MB_CMD_SET_PORT_CFG;
	mbcp->mbox_in[1] = qdev->link_config;
	mbcp->mbox_in[2] = qdev->max_frame_size;


	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] == MB_CMD_STS_INTRMDT) {
		netif_err(qdev, drv, qdev->ndev,
			  "Port Config sent, wait for IDC.\n");
	} else	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev,
			  "Failed Set Port Configuration.\n");
		status = -EIO;
	}
	return status;
}

static int ql_mb_dump_ram(struct ql_adapter *qdev, u64 req_dma, u32 addr,
	u32 size)
{
	int status = 0;
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 9;
	mbcp->out_count = 1;

	mbcp->mbox_in[0] = MB_CMD_DUMP_RISC_RAM;
	mbcp->mbox_in[1] = LSW(addr);
	mbcp->mbox_in[2] = MSW(req_dma);
	mbcp->mbox_in[3] = LSW(req_dma);
	mbcp->mbox_in[4] = MSW(size);
	mbcp->mbox_in[5] = LSW(size);
	mbcp->mbox_in[6] = MSW(MSD(req_dma));
	mbcp->mbox_in[7] = LSW(MSD(req_dma));
	mbcp->mbox_in[8] = MSW(addr);


	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev, "Failed to dump risc RAM.\n");
		status = -EIO;
	}
	return status;
}

int ql_dump_risc_ram_area(struct ql_adapter *qdev, void *buf,
		u32 ram_addr, int word_count)
{
	int status;
	char *my_buf;
	dma_addr_t buf_dma;

	my_buf = pci_alloc_consistent(qdev->pdev, word_count * sizeof(u32),
					&buf_dma);
	if (!my_buf)
		return -EIO;

	status = ql_mb_dump_ram(qdev, buf_dma, ram_addr, word_count);
	if (!status)
		memcpy(buf, my_buf, word_count * sizeof(u32));

	pci_free_consistent(qdev->pdev, word_count * sizeof(u32), my_buf,
				buf_dma);
	return status;
}

int ql_mb_get_port_cfg(struct ql_adapter *qdev)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status = 0;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 1;
	mbcp->out_count = 3;

	mbcp->mbox_in[0] = MB_CMD_GET_PORT_CFG;

	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev,
			  "Failed Get Port Configuration.\n");
		status = -EIO;
	} else	{
		netif_printk(qdev, drv, KERN_DEBUG, qdev->ndev,
			     "Passed Get Port Configuration.\n");
		qdev->link_config = mbcp->mbox_out[1];
		qdev->max_frame_size = mbcp->mbox_out[2];
	}
	return status;
}

int ql_mb_wol_mode(struct ql_adapter *qdev, u32 wol)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 2;
	mbcp->out_count = 1;

	mbcp->mbox_in[0] = MB_CMD_SET_WOL_MODE;
	mbcp->mbox_in[1] = wol;


	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev, "Failed to set WOL mode.\n");
		status = -EIO;
	}
	return status;
}

int ql_mb_wol_set_magic(struct ql_adapter *qdev, u32 enable_wol)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status;
	u8 *addr = qdev->ndev->dev_addr;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 8;
	mbcp->out_count = 1;

	mbcp->mbox_in[0] = MB_CMD_SET_WOL_MAGIC;
	if (enable_wol) {
		mbcp->mbox_in[1] = (u32)addr[0];
		mbcp->mbox_in[2] = (u32)addr[1];
		mbcp->mbox_in[3] = (u32)addr[2];
		mbcp->mbox_in[4] = (u32)addr[3];
		mbcp->mbox_in[5] = (u32)addr[4];
		mbcp->mbox_in[6] = (u32)addr[5];
		mbcp->mbox_in[7] = 0;
	} else {
		mbcp->mbox_in[1] = 0;
		mbcp->mbox_in[2] = 1;
		mbcp->mbox_in[3] = 1;
		mbcp->mbox_in[4] = 1;
		mbcp->mbox_in[5] = 1;
		mbcp->mbox_in[6] = 1;
		mbcp->mbox_in[7] = 0;
	}

	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev, "Failed to set WOL mode.\n");
		status = -EIO;
	}
	return status;
}

static int ql_idc_wait(struct ql_adapter *qdev)
{
	int status = -ETIMEDOUT;
	long wait_time = 1 * HZ;
	struct mbox_params *mbcp = &qdev->idc_mbc;
	do {
		wait_time =
			wait_for_completion_timeout(&qdev->ide_completion,
							wait_time);
		if (!wait_time) {
			netif_err(qdev, drv, qdev->ndev, "IDC Timeout.\n");
			break;
		}
		if (mbcp->mbox_out[0] == AEN_IDC_EXT) {
			netif_err(qdev, drv, qdev->ndev,
				  "IDC Time Extension from function.\n");
			wait_time += (mbcp->mbox_out[1] >> 8) & 0x0000000f;
		} else if (mbcp->mbox_out[0] == AEN_IDC_CMPLT) {
			netif_err(qdev, drv, qdev->ndev, "IDC Success.\n");
			status = 0;
			break;
		} else {
			netif_err(qdev, drv, qdev->ndev,
				  "IDC: Invalid State 0x%.04x.\n",
				  mbcp->mbox_out[0]);
			status = -EIO;
			break;
		}
	} while (wait_time);

	return status;
}

int ql_mb_set_led_cfg(struct ql_adapter *qdev, u32 led_config)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 2;
	mbcp->out_count = 1;

	mbcp->mbox_in[0] = MB_CMD_SET_LED_CFG;
	mbcp->mbox_in[1] = led_config;


	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev,
			  "Failed to set LED Configuration.\n");
		status = -EIO;
	}

	return status;
}

int ql_mb_get_led_cfg(struct ql_adapter *qdev)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 1;
	mbcp->out_count = 2;

	mbcp->mbox_in[0] = MB_CMD_GET_LED_CFG;

	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] != MB_CMD_STS_GOOD) {
		netif_err(qdev, drv, qdev->ndev,
			  "Failed to get LED Configuration.\n");
		status = -EIO;
	} else
		qdev->led_config = mbcp->mbox_out[1];

	return status;
}

int ql_mb_set_mgmnt_traffic_ctl(struct ql_adapter *qdev, u32 control)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status;

	memset(mbcp, 0, sizeof(struct mbox_params));

	mbcp->in_count = 1;
	mbcp->out_count = 2;

	mbcp->mbox_in[0] = MB_CMD_SET_MGMNT_TFK_CTL;
	mbcp->mbox_in[1] = control;

	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] == MB_CMD_STS_GOOD)
		return status;

	if (mbcp->mbox_out[0] == MB_CMD_STS_INVLD_CMD) {
		netif_err(qdev, drv, qdev->ndev,
			  "Command not supported by firmware.\n");
		status = -EINVAL;
	} else if (mbcp->mbox_out[0] == MB_CMD_STS_ERR) {
		netif_err(qdev, drv, qdev->ndev,
			  "Command parameters make no change.\n");
	}
	return status;
}

static int ql_mb_get_mgmnt_traffic_ctl(struct ql_adapter *qdev, u32 *control)
{
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int status;

	memset(mbcp, 0, sizeof(struct mbox_params));
	*control = 0;

	mbcp->in_count = 1;
	mbcp->out_count = 1;

	mbcp->mbox_in[0] = MB_CMD_GET_MGMNT_TFK_CTL;

	status = ql_mailbox_command(qdev, mbcp);
	if (status)
		return status;

	if (mbcp->mbox_out[0] == MB_CMD_STS_GOOD) {
		*control = mbcp->mbox_in[1];
		return status;
	}

	if (mbcp->mbox_out[0] == MB_CMD_STS_INVLD_CMD) {
		netif_err(qdev, drv, qdev->ndev,
			  "Command not supported by firmware.\n");
		status = -EINVAL;
	} else if (mbcp->mbox_out[0] == MB_CMD_STS_ERR) {
		netif_err(qdev, drv, qdev->ndev,
			  "Failed to get MPI traffic control.\n");
		status = -EIO;
	}
	return status;
}

int ql_wait_fifo_empty(struct ql_adapter *qdev)
{
	int count = 5;
	u32 mgmnt_fifo_empty;
	u32 nic_fifo_empty;

	do {
		nic_fifo_empty = ql_read32(qdev, STS) & STS_NFE;
		ql_mb_get_mgmnt_traffic_ctl(qdev, &mgmnt_fifo_empty);
		mgmnt_fifo_empty &= MB_GET_MPI_TFK_FIFO_EMPTY;
		if (nic_fifo_empty && mgmnt_fifo_empty)
			return 0;
		msleep(100);
	} while (count-- > 0);
	return -ETIMEDOUT;
}

static int ql_set_port_cfg(struct ql_adapter *qdev)
{
	int status;
	status = ql_mb_set_port_cfg(qdev);
	if (status)
		return status;
	status = ql_idc_wait(qdev);
	return status;
}


void ql_mpi_port_cfg_work(struct work_struct *work)
{
	struct ql_adapter *qdev =
	    container_of(work, struct ql_adapter, mpi_port_cfg_work.work);
	int status;

	status = ql_mb_get_port_cfg(qdev);
	if (status) {
		netif_err(qdev, drv, qdev->ndev,
			  "Bug: Failed to get port config data.\n");
		goto err;
	}

	if (qdev->link_config & CFG_JUMBO_FRAME_SIZE &&
			qdev->max_frame_size ==
			CFG_DEFAULT_MAX_FRAME_SIZE)
		goto end;

	qdev->link_config |=	CFG_JUMBO_FRAME_SIZE;
	qdev->max_frame_size = CFG_DEFAULT_MAX_FRAME_SIZE;
	status = ql_set_port_cfg(qdev);
	if (status) {
		netif_err(qdev, drv, qdev->ndev,
			  "Bug: Failed to set port config data.\n");
		goto err;
	}
end:
	clear_bit(QL_PORT_CFG, &qdev->flags);
	return;
err:
	ql_queue_fw_error(qdev);
	goto end;
}

void ql_mpi_idc_work(struct work_struct *work)
{
	struct ql_adapter *qdev =
	    container_of(work, struct ql_adapter, mpi_idc_work.work);
	int status;
	struct mbox_params *mbcp = &qdev->idc_mbc;
	u32 aen;
	int timeout;

	aen = mbcp->mbox_out[1] >> 16;
	timeout = (mbcp->mbox_out[1] >> 8) & 0xf;

	switch (aen) {
	default:
		netif_err(qdev, drv, qdev->ndev,
			  "Bug: Unhandled IDC action.\n");
		break;
	case MB_CMD_PORT_RESET:
	case MB_CMD_STOP_FW:
		ql_link_off(qdev);
	case MB_CMD_SET_PORT_CFG:
		set_bit(QL_CAM_RT_SET, &qdev->flags);
		
		if (timeout) {
			status = ql_mb_idc_ack(qdev);
			if (status)
				netif_err(qdev, drv, qdev->ndev,
					  "Bug: No pending IDC!\n");
		} else {
			netif_printk(qdev, drv, KERN_DEBUG, qdev->ndev,
				     "IDC ACK not required\n");
			status = 0; 
		}
		break;

	case MB_CMD_IOP_RESTART_MPI:
	case MB_CMD_IOP_PREP_LINK_DOWN:
		ql_link_off(qdev);
		set_bit(QL_CAM_RT_SET, &qdev->flags);
		
	case MB_CMD_IOP_DVR_START:
	case MB_CMD_IOP_FLASH_ACC:
	case MB_CMD_IOP_CORE_DUMP_MPI:
	case MB_CMD_IOP_PREP_UPDATE_MPI:
	case MB_CMD_IOP_COMP_UPDATE_MPI:
	case MB_CMD_IOP_NONE:	
		
		if (timeout) {
			status = ql_mb_idc_ack(qdev);
			if (status)
				netif_err(qdev, drv, qdev->ndev,
					  "Bug: No pending IDC!\n");
		} else {
			netif_printk(qdev, drv, KERN_DEBUG, qdev->ndev,
				     "IDC ACK not required\n");
			status = 0; 
		}
		break;
	}
}

void ql_mpi_work(struct work_struct *work)
{
	struct ql_adapter *qdev =
	    container_of(work, struct ql_adapter, mpi_work.work);
	struct mbox_params mbc;
	struct mbox_params *mbcp = &mbc;
	int err = 0;

	mutex_lock(&qdev->mpi_mutex);
	
	ql_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16));

	while (ql_read32(qdev, STS) & STS_PI) {
		memset(mbcp, 0, sizeof(struct mbox_params));
		mbcp->out_count = 1;
		err = ql_mpi_handler(qdev, mbcp);
		if (err)
			break;
	}

	
	ql_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16) | INTR_MASK_PI);
	mutex_unlock(&qdev->mpi_mutex);
	ql_enable_completion_interrupt(qdev, 0);
}

void ql_mpi_reset_work(struct work_struct *work)
{
	struct ql_adapter *qdev =
	    container_of(work, struct ql_adapter, mpi_reset_work.work);
	cancel_delayed_work_sync(&qdev->mpi_work);
	cancel_delayed_work_sync(&qdev->mpi_port_cfg_work);
	cancel_delayed_work_sync(&qdev->mpi_idc_work);
	if (!ql_own_firmware(qdev)) {
		netif_err(qdev, drv, qdev->ndev, "Don't own firmware!\n");
		return;
	}

	if (!ql_core_dump(qdev, qdev->mpi_coredump)) {
		netif_err(qdev, drv, qdev->ndev, "Core is dumped!\n");
		qdev->core_is_dumped = 1;
		queue_delayed_work(qdev->workqueue,
			&qdev->mpi_core_to_log, 5 * HZ);
	}
	ql_soft_reset_mpi_risc(qdev);
}
