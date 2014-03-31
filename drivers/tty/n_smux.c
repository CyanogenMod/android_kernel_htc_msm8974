/* drivers/tty/n_smux.c
 *
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/tty_driver.h>
#include <linux/smux.h>
#include <linux/list.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>
#include <mach/msm_serial_hs.h>
#include <mach/msm_ipc_logging.h>
#include "smux_private.h"
#include "smux_loopback.h"

#define SMUX_NOTIFY_FIFO_SIZE	128
#define SMUX_TX_QUEUE_SIZE	256
#define SMUX_PKT_LOG_SIZE 128

#define TTY_RECEIVE_ROOM 65536
#define TTY_BUFFER_FULL_WAIT_MS 50

#define SMUX_WAKEUP_DELAY_MAX (1 << 20)

#define SMUX_WAKEUP_DELAY_MIN (1 << 15)

#define SMUX_INACTIVITY_TIMEOUT_MS 1000000

#define SMUX_RX_RETRY_MIN_MS (1 << 0)  
#define SMUX_RX_RETRY_MAX_MS (1 << 10) 

enum {
	MSM_SMUX_DEBUG = 1U << 0,
	MSM_SMUX_INFO = 1U << 1,
	MSM_SMUX_POWER_INFO = 1U << 2,
	MSM_SMUX_PKT = 1U << 3,
};

static int smux_debug_mask = MSM_SMUX_DEBUG | MSM_SMUX_POWER_INFO;
module_param_named(debug_mask, smux_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

static int disable_ipc_logging;

int smux_byte_loopback;
module_param_named(byte_loopback, smux_byte_loopback,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);
int smux_simulate_wakeup_delay = 1;
module_param_named(simulate_wakeup_delay, smux_simulate_wakeup_delay,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#define IPC_LOG_STR(x...) do { \
	if (!disable_ipc_logging && log_ctx) \
		ipc_log_string(log_ctx, x); \
} while (0)

#define SMUX_DBG(x...) do {                              \
	if (smux_debug_mask & MSM_SMUX_DEBUG) \
			IPC_LOG_STR(x);  \
} while (0)

#define SMUX_ERR(x...) do {                              \
	pr_err(x); \
	IPC_LOG_STR(x);  \
} while (0)

#define SMUX_PWR(x...) do {                              \
	if (smux_debug_mask & MSM_SMUX_POWER_INFO) \
			IPC_LOG_STR(x);  \
} while (0)

#define SMUX_PWR_PKT_RX(pkt) do { \
	if (smux_debug_mask & MSM_SMUX_POWER_INFO) \
			smux_log_pkt(pkt, 1); \
} while (0)

#define SMUX_PWR_PKT_TX(pkt) do { \
	if (smux_debug_mask & MSM_SMUX_POWER_INFO) { \
			if (pkt->hdr.cmd == SMUX_CMD_BYTE && \
					pkt->hdr.flags == SMUX_WAKEUP_ACK) \
				IPC_LOG_STR("smux: TX Wakeup ACK\n"); \
			else if (pkt->hdr.cmd == SMUX_CMD_BYTE && \
					pkt->hdr.flags == SMUX_WAKEUP_REQ) \
				IPC_LOG_STR("smux: TX Wakeup REQ\n"); \
			else \
				smux_log_pkt(pkt, 0); \
	} \
} while (0)

#define SMUX_PWR_BYTE_TX(pkt) do { \
	if (smux_debug_mask & MSM_SMUX_POWER_INFO) { \
			smux_log_pkt(pkt, 0); \
	} \
} while (0)

#define SMUX_LOG_PKT_RX(pkt) do { \
	if (smux_debug_mask & MSM_SMUX_PKT) \
			smux_log_pkt(pkt, 1); \
} while (0)

#define SMUX_LOG_PKT_TX(pkt) do { \
	if (smux_debug_mask & MSM_SMUX_PKT) \
			smux_log_pkt(pkt, 0); \
} while (0)

#define IS_FULLY_OPENED(ch) \
	(ch && (ch)->local_state == SMUX_LCH_LOCAL_OPENED \
	 && (ch)->remote_state == SMUX_LCH_REMOTE_OPENED)

static struct platform_device smux_devs[] = {
	{.name = "SMUX_CTL", .id = -1},
	{.name = "SMUX_RMNET", .id = -1},
	{.name = "SMUX_DUN_DATA_HSUART", .id = 0},
	{.name = "SMUX_RMNET_DATA_HSUART", .id = 1},
	{.name = "SMUX_RMNET_CTL_HSUART", .id = 0},
	{.name = "SMUX_DIAG", .id = -1},
};

enum {
	SMUX_CMD_STATUS_RTC = 1 << 0,
	SMUX_CMD_STATUS_RTR = 1 << 1,
	SMUX_CMD_STATUS_RI = 1 << 2,
	SMUX_CMD_STATUS_DCD = 1 << 3,
	SMUX_CMD_STATUS_FLOW_CNTL = 1 << 4,
};

enum {
	SMUX_LCH_MODE_NORMAL,
	SMUX_LCH_MODE_LOCAL_LOOPBACK,
	SMUX_LCH_MODE_REMOTE_LOOPBACK,
};

enum {
	SMUX_RX_IDLE,
	SMUX_RX_MAGIC,
	SMUX_RX_HDR,
	SMUX_RX_PAYLOAD,
	SMUX_RX_FAILURE,
};

enum {
	SMUX_PWR_OFF,
	SMUX_PWR_TURNING_ON,
	SMUX_PWR_ON,
	SMUX_PWR_TURNING_OFF_FLUSH, 
	SMUX_PWR_TURNING_OFF,
	SMUX_PWR_OFF_FLUSH,
};

union notifier_metadata {
	struct smux_meta_disconnected disconnected;
	struct smux_meta_read read;
	struct smux_meta_write write;
	struct smux_meta_tiocm tiocm;
};

struct smux_notify_handle {
	void (*notify)(void *priv, int event_type, const void *metadata);
	void *priv;
	int event_type;
	union notifier_metadata *metadata;
};

struct smux_rx_pkt_retry {
	struct smux_pkt_t *pkt;
	struct list_head rx_retry_list;
	unsigned timeout_in_ms;
};

struct smux_rx_worker_data {
	const unsigned char *data;
	int len;
	int flag;

	struct work_struct work;
	struct completion work_complete;
};

struct smux_ldisc_t {
	struct mutex mutex_lha0;

	int is_initialized;
	int platform_devs_registered;
	int in_reset;
	int remote_is_alive;
	int ld_open_count;
	struct tty_struct *tty;

	
	unsigned char recv_buf[SMUX_MAX_PKT_SIZE];
	unsigned int recv_len;
	unsigned int pkt_remain;
	unsigned rx_state;

	
	spinlock_t rx_lock_lha1;
	unsigned rx_activity_flag;

	
	spinlock_t tx_lock_lha2;
	struct list_head lch_tx_ready_list;
	unsigned power_state;
	unsigned pwr_wakeup_delay_us;
	unsigned tx_activity_flag;
	unsigned powerdown_enabled;
	unsigned power_ctl_remote_req_received;
	struct list_head power_queue;
	unsigned remote_initiated_wakeup_count;
	unsigned local_initiated_wakeup_count;
};


struct smux_lch_t smux_lch[SMUX_NUM_LOGICAL_CHANNELS];
static struct smux_ldisc_t smux;
static const char *tty_error_type[] = {
	[TTY_NORMAL] = "normal",
	[TTY_OVERRUN] = "overrun",
	[TTY_BREAK] = "break",
	[TTY_PARITY] = "parity",
	[TTY_FRAME] = "framing",
};

static const char * const smux_cmds[] = {
	[SMUX_CMD_DATA] = "DATA",
	[SMUX_CMD_OPEN_LCH] = "OPEN",
	[SMUX_CMD_CLOSE_LCH] = "CLOSE",
	[SMUX_CMD_STATUS] = "STATUS",
	[SMUX_CMD_PWR_CTL] = "PWR",
	[SMUX_CMD_DELAY] = "DELAY",
	[SMUX_CMD_BYTE] = "Raw Byte",
};

static const char * const smux_events[] = {
	[SMUX_CONNECTED] = "CONNECTED" ,
	[SMUX_DISCONNECTED] = "DISCONNECTED",
	[SMUX_READ_DONE] = "READ_DONE",
	[SMUX_READ_FAIL] = "READ_FAIL",
	[SMUX_WRITE_DONE] = "WRITE_DONE",
	[SMUX_WRITE_FAIL] = "WRITE_FAIL",
	[SMUX_TIOCM_UPDATE] = "TIOCM_UPDATE",
	[SMUX_LOW_WM_HIT] = "LOW_WM_HIT",
	[SMUX_HIGH_WM_HIT] = "HIGH_WM_HIT",
	[SMUX_RX_RETRY_HIGH_WM_HIT] = "RX_RETRY_HIGH_WM_HIT",
	[SMUX_RX_RETRY_LOW_WM_HIT] = "RX_RETRY_LOW_WM_HIT",
	[SMUX_LOCAL_CLOSED] = "LOCAL_CLOSED",
	[SMUX_REMOTE_CLOSED] = "REMOTE_CLOSED",
};

static const char * const smux_local_state[] = {
	[SMUX_LCH_LOCAL_CLOSED] = "CLOSED",
	[SMUX_LCH_LOCAL_OPENING] = "OPENING",
	[SMUX_LCH_LOCAL_OPENED] = "OPENED",
	[SMUX_LCH_LOCAL_CLOSING] = "CLOSING",
};

static const char * const smux_remote_state[] = {
	[SMUX_LCH_REMOTE_CLOSED] = "CLOSED",
	[SMUX_LCH_REMOTE_OPENED] = "OPENED",
};

static const char * const smux_mode[] = {
	[SMUX_LCH_MODE_NORMAL] = "N",
	[SMUX_LCH_MODE_LOCAL_LOOPBACK] = "L",
	[SMUX_LCH_MODE_REMOTE_LOOPBACK] = "R",
};

static const char * const smux_undef[] = {
	[SMUX_UNDEF_LONG] = "UNDEF",
	[SMUX_UNDEF_SHORT] = "U",
};

static void *log_ctx;
static void smux_notify_local_fn(struct work_struct *work);
static DECLARE_WORK(smux_notify_local, smux_notify_local_fn);

static struct workqueue_struct *smux_notify_wq;
static size_t handle_size;
static struct kfifo smux_notify_fifo;
static int queued_fifo_notifications;
static DEFINE_SPINLOCK(notify_lock_lhc1);

static struct workqueue_struct *smux_tx_wq;
static struct workqueue_struct *smux_rx_wq;
static void smux_tx_worker(struct work_struct *work);
static DECLARE_WORK(smux_tx_work, smux_tx_worker);

static void smux_wakeup_worker(struct work_struct *work);
static void smux_rx_retry_worker(struct work_struct *work);
static void smux_rx_worker(struct work_struct *work);
static DECLARE_WORK(smux_wakeup_work, smux_wakeup_worker);
static DECLARE_DELAYED_WORK(smux_wakeup_delayed_work, smux_wakeup_worker);

static void smux_inactivity_worker(struct work_struct *work);
static DECLARE_WORK(smux_inactivity_work, smux_inactivity_worker);
static DECLARE_DELAYED_WORK(smux_delayed_inactivity_work,
		smux_inactivity_worker);

static void list_channel(struct smux_lch_t *ch);
static int smux_send_status_cmd(struct smux_lch_t *ch);
static int smux_dispatch_rx_pkt(struct smux_pkt_t *pkt);
static void smux_flush_tty(void);
static void smux_purge_ch_tx_queue(struct smux_lch_t *ch, int is_ssr);
static int schedule_notify(uint8_t lcid, int event,
			const union notifier_metadata *metadata);
static int ssr_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data);
static void smux_uart_power_on_atomic(void);
static int smux_rx_flow_control_updated(struct smux_lch_t *ch);
static void smux_flush_workqueues(void);
static void smux_pdev_release(struct device *dev);

const char *local_lch_state(unsigned state)
{
	if (state < ARRAY_SIZE(smux_local_state))
		return smux_local_state[state];
	else
		return smux_undef[SMUX_UNDEF_LONG];
}

const char *remote_lch_state(unsigned state)
{
	if (state < ARRAY_SIZE(smux_remote_state))
		return smux_remote_state[state];
	else
		return smux_undef[SMUX_UNDEF_LONG];
}

const char *lch_mode(unsigned mode)
{
	if (mode < ARRAY_SIZE(smux_mode))
		return smux_mode[mode];
	else
		return smux_undef[SMUX_UNDEF_SHORT];
}

static const char *tty_flag_to_str(unsigned flag)
{
	if (flag < ARRAY_SIZE(tty_error_type))
		return tty_error_type[flag];
	return NULL;
}

static const char *cmd_to_str(unsigned cmd)
{
	if (cmd < ARRAY_SIZE(smux_cmds))
		return smux_cmds[cmd];
	return NULL;
}

static const char *event_to_str(unsigned cmd)
{
	if (cmd < ARRAY_SIZE(smux_events))
		return smux_events[cmd];
	return NULL;
}

static void smux_enter_reset(void)
{
	SMUX_ERR("%s: unrecoverable failure, waiting for ssr\n", __func__);
	smux.in_reset = 1;
	smux.remote_is_alive = 0;
}

static int lch_init(void)
{
	unsigned int id;
	struct smux_lch_t *ch;
	int i = 0;

	handle_size = sizeof(struct smux_notify_handle *);

	smux_notify_wq = create_singlethread_workqueue("smux_notify_wq");
	smux_tx_wq = create_singlethread_workqueue("smux_tx_wq");
	smux_rx_wq = create_singlethread_workqueue("smux_rx_wq");

	if (IS_ERR(smux_notify_wq) || IS_ERR(smux_tx_wq)) {
		SMUX_DBG("smux: %s: create_singlethread_workqueue ENOMEM\n",
							__func__);
		return -ENOMEM;
	}

	i |= kfifo_alloc(&smux_notify_fifo,
			SMUX_NOTIFY_FIFO_SIZE * handle_size,
			GFP_KERNEL);
	i |= smux_loopback_init();

	if (i) {
		SMUX_ERR("%s: out of memory error\n", __func__);
		return -ENOMEM;
	}

	for (id = 0 ; id < SMUX_NUM_LOGICAL_CHANNELS; id++) {
		ch = &smux_lch[id];

		spin_lock_init(&ch->state_lock_lhb1);
		ch->lcid = id;
		ch->local_state = SMUX_LCH_LOCAL_CLOSED;
		ch->local_mode = SMUX_LCH_MODE_NORMAL;
		ch->local_tiocm = 0x0;
		ch->options = SMUX_CH_OPTION_AUTO_REMOTE_TX_STOP;
		ch->remote_state = SMUX_LCH_REMOTE_CLOSED;
		ch->remote_mode = SMUX_LCH_MODE_NORMAL;
		ch->remote_tiocm = 0x0;
		ch->tx_flow_control = 0;
		ch->rx_flow_control_auto = 0;
		ch->rx_flow_control_client = 0;
		ch->priv = 0;
		ch->notify = 0;
		ch->get_rx_buffer = 0;

		INIT_LIST_HEAD(&ch->rx_retry_queue);
		ch->rx_retry_queue_cnt = 0;
		INIT_DELAYED_WORK(&ch->rx_retry_work, smux_rx_retry_worker);

		spin_lock_init(&ch->tx_lock_lhb2);
		INIT_LIST_HEAD(&ch->tx_queue);
		INIT_LIST_HEAD(&ch->tx_ready_list);
		ch->tx_pending_data_cnt = 0;
		ch->notify_lwm = 0;
	}

	return 0;
}

static void smux_lch_purge(void)
{
	struct smux_lch_t *ch;
	unsigned long flags;
	int i;

	
	spin_lock_irqsave(&smux.tx_lock_lha2, flags);
	while (!list_empty(&smux.lch_tx_ready_list)) {
		SMUX_DBG("smux: %s: emptying ready list %p\n",
				__func__, smux.lch_tx_ready_list.next);
		ch = list_first_entry(&smux.lch_tx_ready_list,
						struct smux_lch_t,
						tx_ready_list);
		list_del(&ch->tx_ready_list);
		INIT_LIST_HEAD(&ch->tx_ready_list);
	}

	
	while (!list_empty(&smux.power_queue)) {
		struct smux_pkt_t *pkt;

		pkt =  list_first_entry(&smux.power_queue,
						struct smux_pkt_t,
						list);
		list_del(&pkt->list);
		SMUX_DBG("smux: %s: emptying power queue pkt=%p\n",
				__func__, pkt);
		smux_free_pkt(pkt);
	}
	spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);

	
	for (i = 0 ; i < SMUX_NUM_LOGICAL_CHANNELS; i++) {
		union notifier_metadata meta;
		int send_disconnect = 0;
		ch = &smux_lch[i];
		SMUX_DBG("smux: %s: cleaning up lcid %d\n", __func__, i);

		spin_lock_irqsave(&ch->state_lock_lhb1, flags);

		
		spin_lock(&ch->tx_lock_lhb2);
		smux_purge_ch_tx_queue(ch, 1);
		spin_unlock(&ch->tx_lock_lhb2);

		meta.disconnected.is_ssr = smux.in_reset;
		
		if (ch->local_state == SMUX_LCH_LOCAL_OPENED ||
			ch->local_state == SMUX_LCH_LOCAL_CLOSING) {
			schedule_notify(ch->lcid, SMUX_LOCAL_CLOSED, &meta);
			send_disconnect = 1;
		}
		if (ch->remote_state != SMUX_LCH_REMOTE_CLOSED) {
			schedule_notify(ch->lcid, SMUX_REMOTE_CLOSED, &meta);
			send_disconnect = 1;
		}
		if (send_disconnect)
			schedule_notify(ch->lcid, SMUX_DISCONNECTED, &meta);

		ch->local_state = SMUX_LCH_LOCAL_CLOSED;
		ch->remote_state = SMUX_LCH_REMOTE_CLOSED;
		ch->remote_mode = SMUX_LCH_MODE_NORMAL;
		ch->tx_flow_control = 0;
		ch->rx_flow_control_auto = 0;
		ch->rx_flow_control_client = 0;

		
		if (ch->rx_retry_queue_cnt)
			queue_delayed_work(smux_rx_wq, &ch->rx_retry_work, 0);

		spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
	}
}

int smux_assert_lch_id(uint32_t lcid)
{
	if (lcid >= SMUX_NUM_LOGICAL_CHANNELS)
		return -ENXIO;
	else
		return 0;
}

static void smux_log_pkt(struct smux_pkt_t *pkt, int is_recv)
{
	char logbuf[SMUX_PKT_LOG_SIZE];
	char cmd_extra[16];
	int i = 0;
	int count;
	int len;
	char local_state;
	char local_mode;
	char remote_state;
	char remote_mode;
	struct smux_lch_t *ch = NULL;
	unsigned char *data;

	if (!smux_assert_lch_id(pkt->hdr.lcid))
		ch = &smux_lch[pkt->hdr.lcid];

	if (ch) {
		switch (ch->local_state) {
		case SMUX_LCH_LOCAL_CLOSED:
			local_state = 'C';
			break;
		case SMUX_LCH_LOCAL_OPENING:
			local_state = 'o';
			break;
		case SMUX_LCH_LOCAL_OPENED:
			local_state = 'O';
			break;
		case SMUX_LCH_LOCAL_CLOSING:
			local_state = 'c';
			break;
		default:
			local_state = 'U';
			break;
		}

		switch (ch->local_mode) {
		case SMUX_LCH_MODE_LOCAL_LOOPBACK:
			local_mode = 'L';
			break;
		case SMUX_LCH_MODE_REMOTE_LOOPBACK:
			local_mode = 'R';
			break;
		case SMUX_LCH_MODE_NORMAL:
			local_mode = 'N';
			break;
		default:
			local_mode = 'U';
			break;
		}

		switch (ch->remote_state) {
		case SMUX_LCH_REMOTE_CLOSED:
			remote_state = 'C';
			break;
		case SMUX_LCH_REMOTE_OPENED:
			remote_state = 'O';
			break;

		default:
			remote_state = 'U';
			break;
		}

		switch (ch->remote_mode) {
		case SMUX_LCH_MODE_REMOTE_LOOPBACK:
			remote_mode = 'R';
			break;
		case SMUX_LCH_MODE_NORMAL:
			remote_mode = 'N';
			break;
		default:
			remote_mode = 'U';
			break;
		}
	} else {
		
		local_state = '-';
		local_mode = '-';
		remote_state = '-';
		remote_mode = '-';
	}

	
	cmd_extra[0] = '\0';
	switch (pkt->hdr.cmd) {
	case SMUX_CMD_OPEN_LCH:
		if (pkt->hdr.flags & SMUX_CMD_OPEN_ACK)
			snprintf(cmd_extra, sizeof(cmd_extra), " ACK");
		break;
	case SMUX_CMD_CLOSE_LCH:
		if (pkt->hdr.flags & SMUX_CMD_CLOSE_ACK)
			snprintf(cmd_extra, sizeof(cmd_extra), " ACK");
		break;

	case SMUX_CMD_PWR_CTL:
	   if (pkt->hdr.flags & SMUX_CMD_PWR_CTL_ACK)
			snprintf(cmd_extra, sizeof(cmd_extra), " ACK");
	   break;
	};

	i += snprintf(logbuf + i, SMUX_PKT_LOG_SIZE - i,
			"smux: %c%d %c%c:%c%c %s%s flags %x len %d:%d ",
			is_recv ? 'R' : 'S', pkt->hdr.lcid,
			local_state, local_mode,
			remote_state, remote_mode,
			cmd_to_str(pkt->hdr.cmd), cmd_extra, pkt->hdr.flags,
			pkt->hdr.payload_len, pkt->hdr.pad_len);

	len = (pkt->hdr.payload_len > 16) ? 16 : pkt->hdr.payload_len;
	data = (unsigned char *)pkt->payload;
	for (count = 0; count < len; count++)
		i += snprintf(logbuf + i, SMUX_PKT_LOG_SIZE - i,
				"%02x ", (unsigned)data[count]);

	IPC_LOG_STR(logbuf);
}

static void smux_notify_local_fn(struct work_struct *work)
{
	struct smux_notify_handle *notify_handle = NULL;
	union notifier_metadata *metadata = NULL;
	unsigned long flags;
	int i;

	for (;;) {
		
		spin_lock_irqsave(&notify_lock_lhc1, flags);
		if (kfifo_len(&smux_notify_fifo) >= handle_size) {
			i = kfifo_out(&smux_notify_fifo,
				&notify_handle,
				handle_size);
		if (i != handle_size) {
			SMUX_ERR(
				"%s: unable to retrieve handle %d expected %d\n",
				__func__, i, handle_size);
			spin_unlock_irqrestore(&notify_lock_lhc1, flags);
			break;
			}
		} else {
			spin_unlock_irqrestore(&notify_lock_lhc1, flags);
			break;
		}
		--queued_fifo_notifications;
		spin_unlock_irqrestore(&notify_lock_lhc1, flags);

		
		metadata = notify_handle->metadata;
		notify_handle->notify(notify_handle->priv,
			notify_handle->event_type,
			metadata);

		kfree(metadata);
		kfree(notify_handle);
	}
}

void smux_init_pkt(struct smux_pkt_t *pkt)
{
	memset(pkt, 0x0, sizeof(*pkt));
	pkt->hdr.magic = SMUX_MAGIC;
	INIT_LIST_HEAD(&pkt->list);
}

struct smux_pkt_t *smux_alloc_pkt(void)
{
	struct smux_pkt_t *pkt;

	
	pkt = kmalloc(sizeof(struct smux_pkt_t), GFP_ATOMIC);
	if (!pkt) {
		SMUX_ERR("%s: out of memory\n", __func__);
		return NULL;
	}
	smux_init_pkt(pkt);
	pkt->allocated = 1;

	return pkt;
}

void smux_free_pkt(struct smux_pkt_t *pkt)
{
	if (pkt) {
		if (pkt->free_payload)
			kfree(pkt->payload);
		if (pkt->allocated)
			kfree(pkt);
	}
}

int smux_alloc_pkt_payload(struct smux_pkt_t *pkt)
{
	if (!pkt)
		return -EINVAL;

	pkt->payload = kmalloc(pkt->hdr.payload_len, GFP_ATOMIC);
	pkt->free_payload = 1;
	if (!pkt->payload) {
		SMUX_ERR("%s: unable to malloc %d bytes for payload\n",
				__func__, pkt->hdr.payload_len);
		return -ENOMEM;
	}

	return 0;
}

static int schedule_notify(uint8_t lcid, int event,
			const union notifier_metadata *metadata)
{
	struct smux_notify_handle *notify_handle = 0;
	union notifier_metadata *meta_copy = 0;
	struct smux_lch_t *ch;
	int i;
	unsigned long flags;
	int ret = 0;

	IPC_LOG_STR("smux: %s ch:%d\n", event_to_str(event), lcid);
	ch = &smux_lch[lcid];
	if (!ch->notify) {
		SMUX_DBG("%s: [%d]lcid notify fn is NULL\n", __func__, lcid);
		return ret;
	}
	notify_handle = kzalloc(sizeof(struct smux_notify_handle),
						GFP_ATOMIC);
	if (!notify_handle) {
		SMUX_ERR("%s: out of memory\n", __func__);
		ret = -ENOMEM;
		goto free_out;
	}

	notify_handle->notify = ch->notify;
	notify_handle->priv = ch->priv;
	notify_handle->event_type = event;
	if (metadata) {
		meta_copy = kzalloc(sizeof(union notifier_metadata),
							GFP_ATOMIC);
		if (!meta_copy) {
			SMUX_ERR("%s: out of memory\n", __func__);
			ret = -ENOMEM;
			goto free_out;
		}
		*meta_copy = *metadata;
		notify_handle->metadata = meta_copy;
	} else {
		notify_handle->metadata = NULL;
	}

	spin_lock_irqsave(&notify_lock_lhc1, flags);
	i = kfifo_avail(&smux_notify_fifo);
	if (i < handle_size) {
		SMUX_ERR("%s: fifo full error %d expected %d\n",
					__func__, i, handle_size);
		ret = -ENOMEM;
		goto unlock_out;
	}

	i = kfifo_in(&smux_notify_fifo, &notify_handle, handle_size);
	if (i < 0 || i != handle_size) {
		SMUX_ERR("%s: fifo not available error %d (expected %d)\n",
				__func__, i, handle_size);
		ret = -ENOSPC;
		goto unlock_out;
	}
	++queued_fifo_notifications;

unlock_out:
	spin_unlock_irqrestore(&notify_lock_lhc1, flags);

free_out:
	queue_work(smux_notify_wq, &smux_notify_local);
	if (ret < 0 && notify_handle) {
		kfree(notify_handle->metadata);
		kfree(notify_handle);
	}
	return ret;
}

static unsigned int smux_serialize_size(struct smux_pkt_t *pkt)
{
	unsigned int size;

	size = sizeof(struct smux_hdr_t);
	size += pkt->hdr.payload_len;
	size += pkt->hdr.pad_len;

	return size;
}

int smux_serialize(struct smux_pkt_t *pkt, char *out,
					unsigned int *out_len)
{
	char *data_start = out;

	if (smux_serialize_size(pkt) > SMUX_MAX_PKT_SIZE) {
		SMUX_ERR("%s: packet size %d too big\n",
				__func__, smux_serialize_size(pkt));
		return -E2BIG;
	}

	memcpy(out, &pkt->hdr, sizeof(struct smux_hdr_t));
	out += sizeof(struct smux_hdr_t);
	if (pkt->payload) {
		memcpy(out, pkt->payload, pkt->hdr.payload_len);
		out += pkt->hdr.payload_len;
	}
	if (pkt->hdr.pad_len) {
		memset(out, 0x0,  pkt->hdr.pad_len);
		out += pkt->hdr.pad_len;
	}
	*out_len = out - data_start;
	return 0;
}

static void smux_serialize_hdr(struct smux_pkt_t *pkt, char **out,
					unsigned int *out_len)
{
	*out = (char *)&pkt->hdr;
	*out_len = sizeof(struct smux_hdr_t);
}

static void smux_serialize_payload(struct smux_pkt_t *pkt, char **out,
					unsigned int *out_len)
{
	*out = pkt->payload;
	*out_len = pkt->hdr.payload_len;
}

static void smux_serialize_padding(struct smux_pkt_t *pkt, char **out,
					unsigned int *out_len)
{
	*out = NULL;
	*out_len = pkt->hdr.pad_len;
}

static int write_to_tty(char *data, unsigned len)
{
	int data_written;

	if (!data)
		return 0;

	while (len > 0 && !smux.in_reset) {
		data_written = smux.tty->ops->write(smux.tty, data, len);
		if (data_written >= 0) {
			len -= data_written;
			data += data_written;
		} else {
			SMUX_ERR("%s: TTY write returned error %d\n",
					__func__, data_written);
			return data_written;
		}

		if (len)
			tty_wait_until_sent(smux.tty,
				msecs_to_jiffies(TTY_BUFFER_FULL_WAIT_MS));
	}
	return 0;
}

static int smux_tx_tty(struct smux_pkt_t *pkt)
{
	char *data;
	unsigned int len;
	int ret;

	if (!smux.tty) {
		SMUX_ERR("%s: TTY not initialized", __func__);
		return -ENOTTY;
	}

	if (pkt->hdr.cmd == SMUX_CMD_BYTE) {
		SMUX_DBG("smux: %s: tty send single byte\n", __func__);
		ret = write_to_tty(&pkt->hdr.flags, 1);
		return ret;
	}

	smux_serialize_hdr(pkt, &data, &len);
	ret = write_to_tty(data, len);
	if (ret) {
		SMUX_ERR("%s: failed %d to write header %d\n",
				__func__, ret, len);
		return ret;
	}

	smux_serialize_payload(pkt, &data, &len);
	ret = write_to_tty(data, len);
	if (ret) {
		SMUX_ERR("%s: failed %d to write payload %d\n",
				__func__, ret, len);
		return ret;
	}

	smux_serialize_padding(pkt, &data, &len);
	while (len > 0) {
		char zero = 0x0;
		ret = write_to_tty(&zero, 1);
		if (ret) {
			SMUX_ERR("%s: failed %d to write padding %d\n",
					__func__, ret, len);
			return ret;
		}
		--len;
	}
	return 0;
}

static void smux_send_byte(char ch)
{
	struct smux_pkt_t *pkt;

	pkt = smux_alloc_pkt();
	if (!pkt) {
		SMUX_ERR("%s: alloc failure for byte %x\n", __func__, ch);
		return;
	}
	pkt->hdr.cmd = SMUX_CMD_BYTE;
	pkt->hdr.flags = ch;
	pkt->hdr.lcid = SMUX_BROADCAST_LCID;

	list_add_tail(&pkt->list, &smux.power_queue);
	queue_work(smux_tx_wq, &smux_tx_work);
}

static int smux_receive_byte(char ch, int lcid)
{
	struct smux_pkt_t pkt;

	smux_init_pkt(&pkt);
	pkt.hdr.lcid = lcid;
	pkt.hdr.cmd = SMUX_CMD_BYTE;
	pkt.hdr.flags = ch;

	return smux_dispatch_rx_pkt(&pkt);
}

static void smux_tx_queue(struct smux_pkt_t *pkt_ptr, struct smux_lch_t *ch,
		int queue)
{
	unsigned long flags;

	SMUX_DBG("smux: %s: queuing pkt %p\n", __func__, pkt_ptr);

	spin_lock_irqsave(&ch->tx_lock_lhb2, flags);
	list_add_tail(&pkt_ptr->list, &ch->tx_queue);
	spin_unlock_irqrestore(&ch->tx_lock_lhb2, flags);

	if (queue)
		list_channel(ch);
}

static int smux_handle_rx_open_ack(struct smux_pkt_t *pkt)
{
	uint8_t lcid;
	int ret;
	struct smux_lch_t *ch;
	int enable_powerdown = 0;
	int tx_ready = 0;

	lcid = pkt->hdr.lcid;
	ch = &smux_lch[lcid];

	spin_lock(&ch->state_lock_lhb1);
	if (ch->local_state == SMUX_LCH_LOCAL_OPENING) {
		SMUX_DBG("smux: lcid %d local state 0x%x -> 0x%x\n", lcid,
				ch->local_state,
				SMUX_LCH_LOCAL_OPENED);

		if (pkt->hdr.flags & SMUX_CMD_OPEN_POWER_COLLAPSE)
			enable_powerdown = 1;

		ch->local_state = SMUX_LCH_LOCAL_OPENED;
		if (ch->remote_state == SMUX_LCH_REMOTE_OPENED) {
			schedule_notify(lcid, SMUX_CONNECTED, NULL);
			if (!(list_empty(&ch->tx_queue)))
				tx_ready = 1;
		}
		ret = 0;
	} else if (ch->remote_mode == SMUX_LCH_MODE_REMOTE_LOOPBACK) {
		SMUX_DBG("smux: Remote loopback OPEN ACK received\n");
		ret = 0;
	} else {
		SMUX_ERR("%s: lcid %d state 0x%x open ack invalid\n",
				__func__, lcid, ch->local_state);
		ret = -EINVAL;
	}
	spin_unlock(&ch->state_lock_lhb1);

	if (enable_powerdown) {
		spin_lock(&smux.tx_lock_lha2);
		if (!smux.powerdown_enabled) {
			smux.powerdown_enabled = 1;
			SMUX_DBG("smux: %s: enabling power-collapse support\n",
					__func__);
		}
		spin_unlock(&smux.tx_lock_lha2);
	}

	if (tx_ready)
		list_channel(ch);

	return ret;
}

static int smux_handle_close_ack(struct smux_pkt_t *pkt)
{
	uint8_t lcid;
	int ret;
	struct smux_lch_t *ch;
	union notifier_metadata meta_disconnected;
	unsigned long flags;

	lcid = pkt->hdr.lcid;
	ch = &smux_lch[lcid];
	meta_disconnected.disconnected.is_ssr = 0;

	spin_lock_irqsave(&ch->state_lock_lhb1, flags);

	if (ch->local_state == SMUX_LCH_LOCAL_CLOSING) {
		SMUX_DBG("smux: lcid %d local state 0x%x -> 0x%x\n", lcid,
				SMUX_LCH_LOCAL_CLOSING,
				SMUX_LCH_LOCAL_CLOSED);
		ch->local_state = SMUX_LCH_LOCAL_CLOSED;
		schedule_notify(lcid, SMUX_LOCAL_CLOSED, &meta_disconnected);
		if (ch->remote_state == SMUX_LCH_REMOTE_CLOSED)
			schedule_notify(lcid, SMUX_DISCONNECTED,
				&meta_disconnected);
		ret = 0;
	} else if (ch->remote_mode == SMUX_LCH_MODE_REMOTE_LOOPBACK) {
		SMUX_DBG("smux: Remote loopback CLOSE ACK received\n");
		ret = 0;
	} else {
		SMUX_ERR("%s: lcid %d state 0x%x close ack invalid\n",
				__func__, lcid,	ch->local_state);
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
	return ret;
}

static int smux_handle_rx_open_cmd(struct smux_pkt_t *pkt)
{
	uint8_t lcid;
	int ret;
	struct smux_lch_t *ch;
	struct smux_pkt_t *ack_pkt;
	unsigned long flags;
	int tx_ready = 0;
	int enable_powerdown = 0;

	if (pkt->hdr.flags & SMUX_CMD_OPEN_ACK)
		return smux_handle_rx_open_ack(pkt);

	lcid = pkt->hdr.lcid;
	ch = &smux_lch[lcid];

	spin_lock_irqsave(&ch->state_lock_lhb1, flags);

	if (ch->remote_state == SMUX_LCH_REMOTE_CLOSED) {
		SMUX_DBG("smux: lcid %d remote state 0x%x -> 0x%x\n", lcid,
				SMUX_LCH_REMOTE_CLOSED,
				SMUX_LCH_REMOTE_OPENED);

		ch->remote_state = SMUX_LCH_REMOTE_OPENED;
		if (pkt->hdr.flags & SMUX_CMD_OPEN_POWER_COLLAPSE)
			enable_powerdown = 1;

		
		ack_pkt = smux_alloc_pkt();
		if (!ack_pkt) {
			
			ret = -ENOMEM;
			goto out;
		}
		ack_pkt->hdr.cmd = SMUX_CMD_OPEN_LCH;
		ack_pkt->hdr.flags = SMUX_CMD_OPEN_ACK;
		if (enable_powerdown)
			ack_pkt->hdr.flags |= SMUX_CMD_OPEN_POWER_COLLAPSE;
		ack_pkt->hdr.lcid = lcid;
		ack_pkt->hdr.payload_len = 0;
		ack_pkt->hdr.pad_len = 0;
		if (pkt->hdr.flags & SMUX_CMD_OPEN_REMOTE_LOOPBACK) {
			ch->remote_mode = SMUX_LCH_MODE_REMOTE_LOOPBACK;
			ack_pkt->hdr.flags |= SMUX_CMD_OPEN_REMOTE_LOOPBACK;
		}
		smux_tx_queue(ack_pkt, ch, 0);
		tx_ready = 1;

		if (ch->remote_mode == SMUX_LCH_MODE_REMOTE_LOOPBACK) {
			ack_pkt = smux_alloc_pkt();
			if (ack_pkt) {
				ack_pkt->hdr.lcid = lcid;
				ack_pkt->hdr.cmd = SMUX_CMD_OPEN_LCH;
				if (enable_powerdown)
					ack_pkt->hdr.flags |=
						SMUX_CMD_OPEN_POWER_COLLAPSE;
				ack_pkt->hdr.payload_len = 0;
				ack_pkt->hdr.pad_len = 0;
				smux_tx_queue(ack_pkt, ch, 0);
				tx_ready = 1;
			} else {
				SMUX_ERR(
					"%s: Remote loopack allocation failure\n",
					__func__);
			}
		} else if (ch->local_state == SMUX_LCH_LOCAL_OPENED) {
			schedule_notify(lcid, SMUX_CONNECTED, NULL);
		}
		ret = 0;
	} else {
		SMUX_ERR("%s: lcid %d remote state 0x%x open invalid\n",
			   __func__, lcid, ch->remote_state);
		ret = -EINVAL;
	}

out:
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);

	if (enable_powerdown) {
		spin_lock_irqsave(&smux.tx_lock_lha2, flags);
		if (!smux.powerdown_enabled) {
			smux.powerdown_enabled = 1;
			SMUX_DBG("smux: %s: enabling power-collapse support\n",
					__func__);
		}
		spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
	}

	if (tx_ready)
		list_channel(ch);

	return ret;
}

static int smux_handle_rx_close_cmd(struct smux_pkt_t *pkt)
{
	uint8_t lcid;
	int ret;
	struct smux_lch_t *ch;
	struct smux_pkt_t *ack_pkt;
	union notifier_metadata meta_disconnected;
	unsigned long flags;
	int tx_ready = 0;

	if (pkt->hdr.flags & SMUX_CMD_CLOSE_ACK)
		return smux_handle_close_ack(pkt);

	lcid = pkt->hdr.lcid;
	ch = &smux_lch[lcid];
	meta_disconnected.disconnected.is_ssr = 0;

	spin_lock_irqsave(&ch->state_lock_lhb1, flags);
	if (ch->remote_state == SMUX_LCH_REMOTE_OPENED) {
		SMUX_DBG("smux: lcid %d remote state 0x%x -> 0x%x\n", lcid,
				SMUX_LCH_REMOTE_OPENED,
				SMUX_LCH_REMOTE_CLOSED);

		ack_pkt = smux_alloc_pkt();
		if (!ack_pkt) {
			
			ret = -ENOMEM;
			goto out;
		}
		ch->remote_state = SMUX_LCH_REMOTE_CLOSED;
		ack_pkt->hdr.cmd = SMUX_CMD_CLOSE_LCH;
		ack_pkt->hdr.flags = SMUX_CMD_CLOSE_ACK;
		ack_pkt->hdr.lcid = lcid;
		ack_pkt->hdr.payload_len = 0;
		ack_pkt->hdr.pad_len = 0;
		smux_tx_queue(ack_pkt, ch, 0);
		tx_ready = 1;

		if (ch->remote_mode == SMUX_LCH_MODE_REMOTE_LOOPBACK) {
			ack_pkt = smux_alloc_pkt();
			if (ack_pkt) {
				ack_pkt->hdr.lcid = lcid;
				ack_pkt->hdr.cmd = SMUX_CMD_CLOSE_LCH;
				ack_pkt->hdr.flags = 0;
				ack_pkt->hdr.payload_len = 0;
				ack_pkt->hdr.pad_len = 0;
				smux_tx_queue(ack_pkt, ch, 0);
				tx_ready = 1;
			} else {
				SMUX_ERR(
					"%s: Remote loopack allocation failure\n",
					__func__);
			}
		}

		schedule_notify(lcid, SMUX_REMOTE_CLOSED, &meta_disconnected);
		if (ch->local_state == SMUX_LCH_LOCAL_CLOSED)
			schedule_notify(lcid, SMUX_DISCONNECTED,
				&meta_disconnected);
		ret = 0;
	} else {
		SMUX_ERR("%s: lcid %d remote state 0x%x close invalid\n",
				__func__, lcid, ch->remote_state);
		ret = -EINVAL;
	}
out:
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
	if (tx_ready)
		list_channel(ch);

	return ret;
}

static int smux_handle_rx_data_cmd(struct smux_pkt_t *pkt)
{
	uint8_t lcid;
	int ret = 0;
	int do_retry = 0;
	int tx_ready = 0;
	int tmp;
	int rx_len;
	struct smux_lch_t *ch;
	union notifier_metadata metadata;
	int remote_loopback;
	struct smux_pkt_t *ack_pkt;
	unsigned long flags;

	if (!pkt || smux_assert_lch_id(pkt->hdr.lcid)) {
		ret = -ENXIO;
		goto out;
	}

	rx_len = pkt->hdr.payload_len;
	if (rx_len == 0) {
		ret = -EINVAL;
		goto out;
	}

	lcid = pkt->hdr.lcid;
	ch = &smux_lch[lcid];
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);
	remote_loopback = ch->remote_mode == SMUX_LCH_MODE_REMOTE_LOOPBACK;

	if (ch->local_state != SMUX_LCH_LOCAL_OPENED
		&& !remote_loopback) {
		SMUX_ERR("smux: ch %d error data on local state 0x%x",
					lcid, ch->local_state);
		ret = -EIO;
		spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
		goto out;
	}

	if (ch->remote_state != SMUX_LCH_REMOTE_OPENED) {
		SMUX_ERR("smux: ch %d error data on remote state 0x%x",
					lcid, ch->remote_state);
		ret = -EIO;
		spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
		goto out;
	}

	if (!list_empty(&ch->rx_retry_queue)) {
		do_retry = 1;

		if ((ch->options & SMUX_CH_OPTION_AUTO_REMOTE_TX_STOP) &&
			!ch->rx_flow_control_auto &&
			((ch->rx_retry_queue_cnt + 1) >= SMUX_RX_WM_HIGH)) {
			
			ch->rx_flow_control_auto = 1;
			tx_ready |= smux_rx_flow_control_updated(ch);
			schedule_notify(ch->lcid, SMUX_RX_RETRY_HIGH_WM_HIT,
					NULL);
		}
		if ((ch->rx_retry_queue_cnt + 1) > SMUX_RX_RETRY_MAX_PKTS) {
			
			SMUX_ERR(
				"%s: ch %d RX retry queue full; rx flow=%d\n",
				__func__, lcid, ch->rx_flow_control_auto);
			schedule_notify(lcid, SMUX_READ_FAIL, NULL);
			ret = -ENOMEM;
			spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
			goto out;
		}
	}
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);

	if (remote_loopback) {
		
		ack_pkt = smux_alloc_pkt();
		if (ack_pkt) {
			ack_pkt->hdr.lcid = lcid;
			ack_pkt->hdr.cmd = SMUX_CMD_DATA;
			ack_pkt->hdr.flags = 0;
			ack_pkt->hdr.payload_len = pkt->hdr.payload_len;
			if (ack_pkt->hdr.payload_len) {
				smux_alloc_pkt_payload(ack_pkt);
				memcpy(ack_pkt->payload, pkt->payload,
						ack_pkt->hdr.payload_len);
			}
			ack_pkt->hdr.pad_len = pkt->hdr.pad_len;
			smux_tx_queue(ack_pkt, ch, 0);
			tx_ready = 1;
		} else {
			SMUX_ERR("%s: Remote loopack allocation failure\n",
					__func__);
		}
	} else if (!do_retry) {
		
		metadata.read.pkt_priv = 0;
		metadata.read.buffer = 0;
		tmp = ch->get_rx_buffer(ch->priv,
				(void **)&metadata.read.pkt_priv,
				(void **)&metadata.read.buffer,
				rx_len);

		if (tmp == 0 && metadata.read.buffer) {
			
			memcpy(metadata.read.buffer, pkt->payload,
					rx_len);
			metadata.read.len = rx_len;
			schedule_notify(lcid, SMUX_READ_DONE,
							&metadata);
		} else if (tmp == -EAGAIN ||
				(tmp == 0 && !metadata.read.buffer)) {
			
			do_retry = 1;
		} else if (tmp < 0) {
			SMUX_ERR("%s: ch %d Client RX buffer alloc failed %d\n",
					__func__, lcid, tmp);
			schedule_notify(lcid, SMUX_READ_FAIL, NULL);
			ret = -ENOMEM;
		}
	}

	if (do_retry) {
		struct smux_rx_pkt_retry *retry;

		retry = kmalloc(sizeof(struct smux_rx_pkt_retry), GFP_KERNEL);
		if (!retry) {
			SMUX_ERR("%s: retry alloc failure\n", __func__);
			ret = -ENOMEM;
			schedule_notify(lcid, SMUX_READ_FAIL, NULL);
			goto out;
		}
		INIT_LIST_HEAD(&retry->rx_retry_list);
		retry->timeout_in_ms = SMUX_RX_RETRY_MIN_MS;

		
		retry->pkt = smux_alloc_pkt();
		if (!retry->pkt) {
			kfree(retry);
			SMUX_ERR("%s: pkt alloc failure\n", __func__);
			ret = -ENOMEM;
			schedule_notify(lcid, SMUX_READ_FAIL, NULL);
			goto out;
		}
		retry->pkt->hdr.lcid = lcid;
		retry->pkt->hdr.payload_len = pkt->hdr.payload_len;
		retry->pkt->hdr.pad_len = pkt->hdr.pad_len;
		if (retry->pkt->hdr.payload_len) {
			smux_alloc_pkt_payload(retry->pkt);
			memcpy(retry->pkt->payload, pkt->payload,
					retry->pkt->hdr.payload_len);
		}

		
		spin_lock_irqsave(&ch->state_lock_lhb1, flags);
		list_add_tail(&retry->rx_retry_list, &ch->rx_retry_queue);
		++ch->rx_retry_queue_cnt;
		if (ch->rx_retry_queue_cnt == 1)
			queue_delayed_work(smux_rx_wq, &ch->rx_retry_work,
				msecs_to_jiffies(retry->timeout_in_ms));
		spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
	}

	if (tx_ready)
		list_channel(ch);
out:
	return ret;
}

static int smux_handle_rx_byte_cmd(struct smux_pkt_t *pkt)
{
	uint8_t lcid;
	int ret;
	struct smux_lch_t *ch;
	union notifier_metadata metadata;
	unsigned long flags;

	if (!pkt || smux_assert_lch_id(pkt->hdr.lcid)) {
		SMUX_ERR("%s: invalid packet or channel id\n", __func__);
		return -ENXIO;
	}

	lcid = pkt->hdr.lcid;
	ch = &smux_lch[lcid];
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);

	if (ch->local_state != SMUX_LCH_LOCAL_OPENED) {
		SMUX_ERR("smux: ch %d error data on local state 0x%x",
					lcid, ch->local_state);
		ret = -EIO;
		goto out;
	}

	if (ch->remote_state != SMUX_LCH_REMOTE_OPENED) {
		SMUX_ERR("smux: ch %d error data on remote state 0x%x",
					lcid, ch->remote_state);
		ret = -EIO;
		goto out;
	}

	metadata.read.pkt_priv = (void *)(int)pkt->hdr.flags;
	metadata.read.buffer = 0;
	schedule_notify(lcid, SMUX_READ_DONE, &metadata);
	ret = 0;

out:
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
	return ret;
}

static int smux_handle_rx_status_cmd(struct smux_pkt_t *pkt)
{
	uint8_t lcid;
	int ret;
	struct smux_lch_t *ch;
	union notifier_metadata meta;
	unsigned long flags;
	int tx_ready = 0;

	lcid = pkt->hdr.lcid;
	ch = &smux_lch[lcid];

	spin_lock_irqsave(&ch->state_lock_lhb1, flags);
	meta.tiocm.tiocm_old = ch->remote_tiocm;
	meta.tiocm.tiocm_new = pkt->hdr.flags;

	
	if ((meta.tiocm.tiocm_old & SMUX_CMD_STATUS_FLOW_CNTL) ^
		(meta.tiocm.tiocm_new & SMUX_CMD_STATUS_FLOW_CNTL)) {
		
		if (pkt->hdr.flags & SMUX_CMD_STATUS_FLOW_CNTL) {
			
			SMUX_DBG("smux: TX Flow control enabled\n");
			ch->tx_flow_control = 1;
		} else {
			
			SMUX_DBG("smux: TX Flow control disabled\n");
			ch->tx_flow_control = 0;
			tx_ready = 1;
		}
	}
	meta.tiocm.tiocm_old = msm_smux_tiocm_get_atomic(ch);
	ch->remote_tiocm = pkt->hdr.flags;
	meta.tiocm.tiocm_new = msm_smux_tiocm_get_atomic(ch);

	
	if (IS_FULLY_OPENED(ch)) {
		if (meta.tiocm.tiocm_old != meta.tiocm.tiocm_new)
			schedule_notify(lcid, SMUX_TIOCM_UPDATE, &meta);
		ret = 0;
	}
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
	if (tx_ready)
		list_channel(ch);

	return ret;
}

static int smux_handle_rx_power_cmd(struct smux_pkt_t *pkt)
{
	struct smux_pkt_t *ack_pkt;
	int power_down = 0;
	unsigned long flags;

	SMUX_PWR_PKT_RX(pkt);

	spin_lock_irqsave(&smux.tx_lock_lha2, flags);
	if (pkt->hdr.flags & SMUX_CMD_PWR_CTL_ACK) {
		
		if (smux.power_state == SMUX_PWR_TURNING_OFF)
			
			power_down = 1;
		else
			SMUX_ERR("%s: sleep request ack invalid in state %d\n",
					__func__, smux.power_state);
	} else {
		if (smux.power_state == SMUX_PWR_ON) {
			ack_pkt = smux_alloc_pkt();
			if (ack_pkt) {
				SMUX_PWR("smux: %s: Power %d->%d\n", __func__,
						smux.power_state,
						SMUX_PWR_TURNING_OFF_FLUSH);

				smux.power_state = SMUX_PWR_TURNING_OFF_FLUSH;

				
				ack_pkt->hdr.cmd = SMUX_CMD_PWR_CTL;
				ack_pkt->hdr.flags = SMUX_CMD_PWR_CTL_ACK;
				ack_pkt->hdr.lcid = SMUX_BROADCAST_LCID;
				list_add_tail(&ack_pkt->list,
						&smux.power_queue);
				queue_work(smux_tx_wq, &smux_tx_work);
			}
		} else if (smux.power_state == SMUX_PWR_TURNING_OFF_FLUSH) {
			
			SMUX_PWR("smux: %s: Power-down shortcut - no ack\n",
					__func__);
			smux.power_ctl_remote_req_received = 1;
		} else if (smux.power_state == SMUX_PWR_TURNING_OFF) {
			SMUX_PWR("smux: %s: Power-down shortcut - no ack\n",
					__func__);
			power_down = 1;
		} else {
			SMUX_ERR("%s: sleep request invalid in state %d\n",
					__func__, smux.power_state);
		}
	}

	if (power_down) {
		SMUX_PWR("smux: %s: Power %d->%d\n", __func__,
				smux.power_state, SMUX_PWR_OFF_FLUSH);
		smux.power_state = SMUX_PWR_OFF_FLUSH;
		queue_work(smux_tx_wq, &smux_inactivity_work);
	}
	spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);

	return 0;
}

static int smux_dispatch_rx_pkt(struct smux_pkt_t *pkt)
{
	int ret = -ENXIO;

	switch (pkt->hdr.cmd) {
	case SMUX_CMD_OPEN_LCH:
		SMUX_LOG_PKT_RX(pkt);
		if (smux_assert_lch_id(pkt->hdr.lcid)) {
			SMUX_ERR("%s: invalid channel id %d\n",
					__func__, pkt->hdr.lcid);
			break;
		}
		ret = smux_handle_rx_open_cmd(pkt);
		break;

	case SMUX_CMD_DATA:
		SMUX_LOG_PKT_RX(pkt);
		if (smux_assert_lch_id(pkt->hdr.lcid)) {
			SMUX_ERR("%s: invalid channel id %d\n",
					__func__, pkt->hdr.lcid);
			break;
		}
		ret = smux_handle_rx_data_cmd(pkt);
		break;

	case SMUX_CMD_CLOSE_LCH:
		SMUX_LOG_PKT_RX(pkt);
		if (smux_assert_lch_id(pkt->hdr.lcid)) {
			SMUX_ERR("%s: invalid channel id %d\n",
					__func__, pkt->hdr.lcid);
			break;
		}
		ret = smux_handle_rx_close_cmd(pkt);
		break;

	case SMUX_CMD_STATUS:
		SMUX_LOG_PKT_RX(pkt);
		if (smux_assert_lch_id(pkt->hdr.lcid)) {
			SMUX_ERR("%s: invalid channel id %d\n",
					__func__, pkt->hdr.lcid);
			break;
		}
		ret = smux_handle_rx_status_cmd(pkt);
		break;

	case SMUX_CMD_PWR_CTL:
		ret = smux_handle_rx_power_cmd(pkt);
		break;

	case SMUX_CMD_BYTE:
		SMUX_LOG_PKT_RX(pkt);
		ret = smux_handle_rx_byte_cmd(pkt);
		break;

	default:
		SMUX_LOG_PKT_RX(pkt);
		SMUX_ERR("%s: command %d unknown\n", __func__, pkt->hdr.cmd);
		ret = -EINVAL;
	}
	return ret;
}

static int smux_deserialize(unsigned char *data, int len)
{
	struct smux_pkt_t recv;

	smux_init_pkt(&recv);

	memcpy(&recv.hdr, data, sizeof(struct smux_hdr_t));

	if (recv.hdr.magic != SMUX_MAGIC) {
		SMUX_ERR("%s: invalid header magic\n", __func__);
		return -EINVAL;
	}

	if (recv.hdr.payload_len)
		recv.payload = data + sizeof(struct smux_hdr_t);

	return smux_dispatch_rx_pkt(&recv);
}

static void smux_handle_wakeup_req(void)
{
	unsigned long flags;

	spin_lock_irqsave(&smux.tx_lock_lha2, flags);
	if (smux.power_state == SMUX_PWR_OFF
		|| smux.power_state == SMUX_PWR_TURNING_ON) {
		
		SMUX_PWR("smux: %s: Power %d->%d\n", __func__,
				smux.power_state, SMUX_PWR_ON);
		smux.remote_initiated_wakeup_count++;
		smux.power_state = SMUX_PWR_ON;
		queue_work(smux_tx_wq, &smux_wakeup_work);
		queue_work(smux_tx_wq, &smux_tx_work);
		queue_delayed_work(smux_tx_wq, &smux_delayed_inactivity_work,
			msecs_to_jiffies(SMUX_INACTIVITY_TIMEOUT_MS));
		smux_send_byte(SMUX_WAKEUP_ACK);
	} else if (smux.power_state == SMUX_PWR_ON) {
		smux_send_byte(SMUX_WAKEUP_ACK);
	} else {
		
		SMUX_PWR("smux: %s: stale Wakeup REQ in state %d\n",
				__func__, smux.power_state);
	}
	spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
}

static void smux_handle_wakeup_ack(void)
{
	unsigned long flags;

	spin_lock_irqsave(&smux.tx_lock_lha2, flags);
	if (smux.power_state == SMUX_PWR_TURNING_ON) {
		
		SMUX_PWR("smux: %s: Power %d->%d\n", __func__,
				smux.power_state, SMUX_PWR_ON);
		smux.power_state = SMUX_PWR_ON;
		queue_work(smux_tx_wq, &smux_tx_work);
		queue_delayed_work(smux_tx_wq, &smux_delayed_inactivity_work,
			msecs_to_jiffies(SMUX_INACTIVITY_TIMEOUT_MS));

	} else if (smux.power_state != SMUX_PWR_ON) {
		
		SMUX_PWR("smux: %s: stale Wakeup REQ ACK in state %d\n",
				__func__, smux.power_state);
	}
	spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
}

static void smux_rx_handle_idle(const unsigned char *data,
		int len, int *used, int flag)
{
	int i;

	if (flag) {
		if (smux_byte_loopback)
			smux_receive_byte(SMUX_UT_ECHO_ACK_FAIL,
					smux_byte_loopback);
		SMUX_ERR("%s: TTY error 0x%x - ignoring\n", __func__, flag);
		++*used;
		return;
	}

	for (i = *used; i < len && smux.rx_state == SMUX_RX_IDLE; i++) {
		switch (data[i]) {
		case SMUX_MAGIC_WORD1:
			smux.rx_state = SMUX_RX_MAGIC;
			break;
		case SMUX_WAKEUP_REQ:
			SMUX_PWR("smux: smux: RX Wakeup REQ\n");
			if (unlikely(!smux.remote_is_alive)) {
				mutex_lock(&smux.mutex_lha0);
				smux.remote_is_alive = 1;
				mutex_unlock(&smux.mutex_lha0);
			}
			smux_handle_wakeup_req();
			break;
		case SMUX_WAKEUP_ACK:
			SMUX_PWR("smux: smux: RX Wakeup ACK\n");
			if (unlikely(!smux.remote_is_alive)) {
				mutex_lock(&smux.mutex_lha0);
				smux.remote_is_alive = 1;
				mutex_unlock(&smux.mutex_lha0);
			}
			smux_handle_wakeup_ack();
			break;
		default:
			
			if (smux_byte_loopback && data[i] == SMUX_UT_ECHO_REQ)
				smux_receive_byte(SMUX_UT_ECHO_ACK_OK,
						smux_byte_loopback);
			SMUX_ERR("%s: parse error 0x%02x - ignoring\n",
				__func__, (unsigned)data[i]);
			break;
		}
	}

	*used = i;
}

static void smux_rx_handle_magic(const unsigned char *data,
		int len, int *used, int flag)
{
	int i;

	if (flag) {
		SMUX_ERR("%s: TTY RX error %d\n", __func__, flag);
		smux_enter_reset();
		smux.rx_state = SMUX_RX_FAILURE;
		++*used;
		return;
	}

	for (i = *used; i < len && smux.rx_state == SMUX_RX_MAGIC; i++) {
		
		if (data[i] == SMUX_MAGIC_WORD2) {
			smux.recv_len = 0;
			smux.recv_buf[smux.recv_len++] = SMUX_MAGIC_WORD1;
			smux.recv_buf[smux.recv_len++] = SMUX_MAGIC_WORD2;
			smux.rx_state = SMUX_RX_HDR;
		} else {
			
			SMUX_ERR(
				"%s: rx parse error for char %c; *used=%d, len=%d\n",
				__func__, data[i], *used, len);
			smux.rx_state = SMUX_RX_IDLE;
		}
	}

	*used = i;
}

static void smux_rx_handle_hdr(const unsigned char *data,
		int len, int *used, int flag)
{
	int i;
	struct smux_hdr_t *hdr;

	if (flag) {
		SMUX_ERR("%s: TTY RX error %d\n", __func__, flag);
		smux_enter_reset();
		smux.rx_state = SMUX_RX_FAILURE;
		++*used;
		return;
	}

	for (i = *used; i < len && smux.rx_state == SMUX_RX_HDR; i++) {
		smux.recv_buf[smux.recv_len++] = data[i];

		if (smux.recv_len == sizeof(struct smux_hdr_t)) {
			
			hdr = (struct smux_hdr_t *)smux.recv_buf;
			smux.pkt_remain = hdr->payload_len + hdr->pad_len;
			smux.rx_state = SMUX_RX_PAYLOAD;
		}
	}
	*used = i;
}

static void smux_rx_handle_pkt_payload(const unsigned char *data,
		int len, int *used, int flag)
{
	int remaining;

	if (flag) {
		SMUX_ERR("%s: TTY RX error %d\n", __func__, flag);
		smux_enter_reset();
		smux.rx_state = SMUX_RX_FAILURE;
		++*used;
		return;
	}

	
	if (smux.pkt_remain < (len - *used))
		remaining = smux.pkt_remain;
	else
		remaining = len - *used;

	memcpy(&smux.recv_buf[smux.recv_len], &data[*used], remaining);
	smux.recv_len += remaining;
	smux.pkt_remain -= remaining;
	*used += remaining;

	if (smux.pkt_remain == 0) {
		
		smux_deserialize(smux.recv_buf, smux.recv_len);
		smux.rx_state = SMUX_RX_IDLE;
	}
}

void smux_rx_state_machine(const unsigned char *data,
						int len, int flag)
{
	struct smux_rx_worker_data work;

	work.data = data;
	work.len = len;
	work.flag = flag;
	INIT_WORK_ONSTACK(&work.work, smux_rx_worker);
	work.work_complete = COMPLETION_INITIALIZER_ONSTACK(work.work_complete);

	queue_work(smux_rx_wq, &work.work);
	wait_for_completion(&work.work_complete);
}

bool smux_remote_is_active(void)
{
	bool is_active = false;

	mutex_lock(&smux.mutex_lha0);
	if (smux.remote_is_alive)
		is_active = true;
	mutex_unlock(&smux.mutex_lha0);

	return is_active;
}

void smux_set_loopback_data_reply_delay(uint32_t ms)
{
	struct smux_lch_t *ch = &smux_lch[SMUX_TEST_LCID];
	struct smux_pkt_t *pkt;

	pkt = smux_alloc_pkt();
	if (!pkt) {
		pr_err("%s: unable to allocate packet\n", __func__);
		return;
	}

	pkt->hdr.lcid = ch->lcid;
	pkt->hdr.cmd = SMUX_CMD_DELAY;
	pkt->hdr.flags = 0;
	pkt->hdr.payload_len = sizeof(uint32_t);
	pkt->hdr.pad_len = 0;

	if (smux_alloc_pkt_payload(pkt)) {
		pr_err("%s: unable to allocate payload\n", __func__);
		smux_free_pkt(pkt);
		return;
	}
	memcpy(pkt->payload, &ms, sizeof(uint32_t));

	smux_tx_queue(pkt, ch, 1);
}

void smux_get_wakeup_counts(int *local_cnt, int *remote_cnt)
{
	unsigned long flags;

	spin_lock_irqsave(&smux.tx_lock_lha2, flags);

	if (local_cnt)
		*local_cnt = smux.local_initiated_wakeup_count;

	if (remote_cnt)
		*remote_cnt = smux.remote_initiated_wakeup_count;

	spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
}

static void list_channel(struct smux_lch_t *ch)
{
	unsigned long flags;

	SMUX_DBG("smux: %s: listing channel %d\n",
			__func__, ch->lcid);

	spin_lock_irqsave(&smux.tx_lock_lha2, flags);
	spin_lock(&ch->tx_lock_lhb2);
	smux.tx_activity_flag = 1;
	if (list_empty(&ch->tx_ready_list))
		list_add_tail(&ch->tx_ready_list, &smux.lch_tx_ready_list);
	spin_unlock(&ch->tx_lock_lhb2);
	spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);

	queue_work(smux_tx_wq, &smux_tx_work);
}

static void smux_tx_pkt(struct smux_lch_t *ch, struct smux_pkt_t *pkt)
{
	union notifier_metadata meta_write;
	int ret;

	if (ch && pkt) {
		SMUX_LOG_PKT_TX(pkt);
		if (ch->local_mode == SMUX_LCH_MODE_LOCAL_LOOPBACK)
			ret = smux_tx_loopback(pkt);
		else
			ret = smux_tx_tty(pkt);

		if (pkt->hdr.cmd == SMUX_CMD_DATA) {
			
			meta_write.write.pkt_priv = pkt->priv;
			meta_write.write.buffer = pkt->payload;
			meta_write.write.len = pkt->hdr.payload_len;
			if (ret >= 0) {
				SMUX_DBG("smux: %s: PKT write done", __func__);
				schedule_notify(ch->lcid, SMUX_WRITE_DONE,
						&meta_write);
			} else {
				SMUX_ERR("%s: failed to write pkt %d\n",
						__func__, ret);
				schedule_notify(ch->lcid, SMUX_WRITE_FAIL,
						&meta_write);
			}
		}
	}
}

static void smux_flush_tty(void)
{
	mutex_lock(&smux.mutex_lha0);
	if (!smux.tty) {
		SMUX_ERR("%s: ldisc not loaded\n", __func__);
		mutex_unlock(&smux.mutex_lha0);
		return;
	}

	tty_wait_until_sent(smux.tty,
			msecs_to_jiffies(TTY_BUFFER_FULL_WAIT_MS));

	if (tty_chars_in_buffer(smux.tty) > 0)
		SMUX_ERR("%s: unable to flush UART queue\n", __func__);

	mutex_unlock(&smux.mutex_lha0);
}

static void smux_purge_ch_tx_queue(struct smux_lch_t *ch, int is_ssr)
{
	struct smux_pkt_t *pkt;
	int send_disconnect = 0;
	struct smux_pkt_t *pkt_tmp;
	int is_state_pkt;

	list_for_each_entry_safe(pkt, pkt_tmp, &ch->tx_queue, list) {
		is_state_pkt = 0;
		if (pkt->hdr.cmd == SMUX_CMD_OPEN_LCH) {
			if (pkt->hdr.flags & SMUX_CMD_OPEN_ACK) {
				
				is_state_pkt = 1;
			} else {
				
				ch->local_state = SMUX_LCH_LOCAL_CLOSED;
				send_disconnect = 1;
			}
		} else if (pkt->hdr.cmd == SMUX_CMD_CLOSE_LCH) {
			if (pkt->hdr.flags & SMUX_CMD_CLOSE_ACK)
				is_state_pkt = 1;
			if (!send_disconnect)
				is_state_pkt = 1;
		} else if (pkt->hdr.cmd == SMUX_CMD_DATA) {
			
			union notifier_metadata meta_write;

			meta_write.write.pkt_priv = pkt->priv;
			meta_write.write.buffer = pkt->payload;
			meta_write.write.len = pkt->hdr.payload_len;
			schedule_notify(ch->lcid, SMUX_WRITE_FAIL, &meta_write);
		}

		if (!is_state_pkt || is_ssr) {
			list_del(&pkt->list);
			smux_free_pkt(pkt);
		}
	}

	if (send_disconnect) {
		union notifier_metadata meta_disconnected;

		meta_disconnected.disconnected.is_ssr = smux.in_reset;
		schedule_notify(ch->lcid, SMUX_LOCAL_CLOSED,
			&meta_disconnected);
		if (ch->remote_state == SMUX_LCH_REMOTE_CLOSED)
			schedule_notify(ch->lcid, SMUX_DISCONNECTED,
				&meta_disconnected);
	}
}

static void smux_uart_power_on_atomic(void)
{
	struct uart_state *state;

	if (!smux.tty || !smux.tty->driver_data) {
		SMUX_ERR("%s: unable to find UART port for tty %p\n",
				__func__, smux.tty);
		return;
	}
	state = smux.tty->driver_data;
	msm_hs_request_clock_on(state->uart_port);
}

static void smux_uart_power_on(void)
{
	mutex_lock(&smux.mutex_lha0);
	smux_uart_power_on_atomic();
	mutex_unlock(&smux.mutex_lha0);
}

static void smux_uart_power_off_atomic(void)
{
	struct uart_state *state;

	if (!smux.tty || !smux.tty->driver_data) {
		SMUX_ERR("%s: unable to find UART port for tty %p\n",
				__func__, smux.tty);
		mutex_unlock(&smux.mutex_lha0);
		return;
	}
	state = smux.tty->driver_data;
	msm_hs_request_clock_off(state->uart_port);
}

static void smux_uart_power_off(void)
{
	mutex_lock(&smux.mutex_lha0);
	smux_uart_power_off_atomic();
	mutex_unlock(&smux.mutex_lha0);
}

static void smux_wakeup_worker(struct work_struct *work)
{
	unsigned long flags;
	unsigned wakeup_delay;

	if (smux.in_reset)
		return;

	spin_lock_irqsave(&smux.tx_lock_lha2, flags);
	if (smux.power_state == SMUX_PWR_ON) {
		
		smux.pwr_wakeup_delay_us = 1;
		spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
		SMUX_DBG("smux: %s: wakeup complete\n", __func__);

		cancel_delayed_work(&smux_wakeup_delayed_work);
		queue_work(smux_tx_wq, &smux_tx_work);
	} else if (smux.power_state == SMUX_PWR_TURNING_ON) {
		
		wakeup_delay = smux.pwr_wakeup_delay_us;
		smux.pwr_wakeup_delay_us <<= 1;
		if (smux.pwr_wakeup_delay_us > SMUX_WAKEUP_DELAY_MAX)
			smux.pwr_wakeup_delay_us =
				SMUX_WAKEUP_DELAY_MAX;

		spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
		SMUX_PWR("smux: %s: triggering wakeup\n", __func__);
		smux_send_byte(SMUX_WAKEUP_REQ);

		if (wakeup_delay < SMUX_WAKEUP_DELAY_MIN) {
			SMUX_DBG("smux: %s: sleeping for %u us\n", __func__,
					wakeup_delay);
			usleep_range(wakeup_delay, 2*wakeup_delay);
			queue_work(smux_tx_wq, &smux_wakeup_work);
		} else {
			
			SMUX_DBG(
			"smux: %s: scheduling delayed wakeup in %u ms\n",
					__func__, wakeup_delay / 1000);
			queue_delayed_work(smux_tx_wq,
					&smux_wakeup_delayed_work,
					msecs_to_jiffies(wakeup_delay / 1000));
		}
	} else {
		
		smux.pwr_wakeup_delay_us = 1;
		spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
		SMUX_PWR("smux: %s: wakeup aborted\n", __func__);
		cancel_delayed_work(&smux_wakeup_delayed_work);
	}
}


static void smux_inactivity_worker(struct work_struct *work)
{
	struct smux_pkt_t *pkt;
	unsigned long flags;

	if (smux.in_reset)
		return;

	spin_lock_irqsave(&smux.rx_lock_lha1, flags);
	spin_lock(&smux.tx_lock_lha2);

	if (!smux.tx_activity_flag && !smux.rx_activity_flag) {
		
		if (smux.powerdown_enabled) {
			if (smux.power_state == SMUX_PWR_ON) {
				
				pkt = smux_alloc_pkt();
				if (pkt) {
					SMUX_PWR(
					"smux: %s: Power %d->%d\n", __func__,
						smux.power_state,
						SMUX_PWR_TURNING_OFF_FLUSH);
					smux.power_state =
						SMUX_PWR_TURNING_OFF_FLUSH;

					
					pkt->hdr.cmd = SMUX_CMD_PWR_CTL;
					pkt->hdr.flags = 0;
					pkt->hdr.lcid = SMUX_BROADCAST_LCID;
					list_add_tail(&pkt->list,
							&smux.power_queue);
					queue_work(smux_tx_wq, &smux_tx_work);
				} else {
					SMUX_ERR("%s: packet alloc failed\n",
							__func__);
				}
			}
		}
	}
	smux.tx_activity_flag = 0;
	smux.rx_activity_flag = 0;

	if (smux.power_state == SMUX_PWR_OFF_FLUSH) {
		
		SMUX_PWR("smux: %s: Power %d->%d\n", __func__,
				smux.power_state, SMUX_PWR_OFF);
		smux.power_state = SMUX_PWR_OFF;

		
		if (!list_empty(&smux.lch_tx_ready_list) ||
		   !list_empty(&smux.power_queue))
			queue_work(smux_tx_wq, &smux_tx_work);

		spin_unlock(&smux.tx_lock_lha2);
		spin_unlock_irqrestore(&smux.rx_lock_lha1, flags);

		
		smux_flush_tty();
		smux_uart_power_off();
	} else {
		spin_unlock(&smux.tx_lock_lha2);
		spin_unlock_irqrestore(&smux.rx_lock_lha1, flags);
	}

	
	if (smux.power_state != SMUX_PWR_OFF)
		queue_delayed_work(smux_tx_wq, &smux_delayed_inactivity_work,
			msecs_to_jiffies(SMUX_INACTIVITY_TIMEOUT_MS));
}

int smux_remove_rx_retry(struct smux_lch_t *ch,
		struct smux_rx_pkt_retry *retry)
{
	int tx_ready = 0;

	list_del(&retry->rx_retry_list);
	--ch->rx_retry_queue_cnt;
	smux_free_pkt(retry->pkt);
	kfree(retry);

	if ((ch->options & SMUX_CH_OPTION_AUTO_REMOTE_TX_STOP) &&
			(ch->rx_retry_queue_cnt <= SMUX_RX_WM_LOW) &&
			ch->rx_flow_control_auto) {
		ch->rx_flow_control_auto = 0;
		smux_rx_flow_control_updated(ch);
		schedule_notify(ch->lcid, SMUX_RX_RETRY_LOW_WM_HIT, NULL);
		tx_ready = 1;
	}
	return tx_ready;
}

static void smux_rx_worker(struct work_struct *work)
{
	unsigned long flags;
	int used;
	int initial_rx_state;
	struct smux_rx_worker_data *w;
	const unsigned char *data;
	int len;
	int flag;

	w =  container_of(work, struct smux_rx_worker_data, work);
	data = w->data;
	len = w->len;
	flag = w->flag;

	spin_lock_irqsave(&smux.rx_lock_lha1, flags);
	smux.rx_activity_flag = 1;
	spin_unlock_irqrestore(&smux.rx_lock_lha1, flags);

	SMUX_DBG("smux: %s: %p, len=%d, flag=%d\n", __func__, data, len, flag);
	used = 0;
	do {
		if (smux.in_reset) {
			SMUX_DBG("smux: %s: abort RX due to reset\n", __func__);
			smux.rx_state = SMUX_RX_IDLE;
			break;
		}

		SMUX_DBG("smux: %s: state %d; %d of %d\n",
				__func__, smux.rx_state, used, len);
		initial_rx_state = smux.rx_state;

		switch (smux.rx_state) {
		case SMUX_RX_IDLE:
			smux_rx_handle_idle(data, len, &used, flag);
			break;
		case SMUX_RX_MAGIC:
			smux_rx_handle_magic(data, len, &used, flag);
			break;
		case SMUX_RX_HDR:
			smux_rx_handle_hdr(data, len, &used, flag);
			break;
		case SMUX_RX_PAYLOAD:
			smux_rx_handle_pkt_payload(data, len, &used, flag);
			break;
		default:
			SMUX_DBG("smux: %s: invalid state %d\n",
					__func__, smux.rx_state);
			smux.rx_state = SMUX_RX_IDLE;
			break;
		}
	} while (used < len || smux.rx_state != initial_rx_state);

	complete(&w->work_complete);
}

static void smux_rx_retry_worker(struct work_struct *work)
{
	struct smux_lch_t *ch;
	struct smux_rx_pkt_retry *retry;
	union notifier_metadata metadata;
	int tmp;
	unsigned long flags;
	int immediate_retry = 0;
	int tx_ready = 0;

	ch = container_of(work, struct smux_lch_t, rx_retry_work.work);

	
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);
	if ((ch->local_state != SMUX_LCH_LOCAL_OPENED) || smux.in_reset) {
		
		while (!list_empty(&ch->rx_retry_queue)) {
			retry = list_first_entry(&ch->rx_retry_queue,
						struct smux_rx_pkt_retry,
						rx_retry_list);
			(void)smux_remove_rx_retry(ch, retry);
		}
	}

	if (list_empty(&ch->rx_retry_queue)) {
		SMUX_DBG("smux: %s: retry list empty for channel %d\n",
				__func__, ch->lcid);
		spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
		return;
	}
	retry = list_first_entry(&ch->rx_retry_queue,
					struct smux_rx_pkt_retry,
					rx_retry_list);
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);

	SMUX_DBG("smux: %s: ch %d retrying rx pkt %p\n",
			__func__, ch->lcid, retry);
	metadata.read.pkt_priv = 0;
	metadata.read.buffer = 0;
	tmp = ch->get_rx_buffer(ch->priv,
			(void **)&metadata.read.pkt_priv,
			(void **)&metadata.read.buffer,
			retry->pkt->hdr.payload_len);
	if (tmp == 0 && metadata.read.buffer) {
		

		memcpy(metadata.read.buffer, retry->pkt->payload,
						retry->pkt->hdr.payload_len);
		metadata.read.len = retry->pkt->hdr.payload_len;

		spin_lock_irqsave(&ch->state_lock_lhb1, flags);
		tx_ready = smux_remove_rx_retry(ch, retry);
		spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
		schedule_notify(ch->lcid, SMUX_READ_DONE, &metadata);
		if (tx_ready)
			list_channel(ch);

		immediate_retry = 1;
	} else if (tmp == -EAGAIN ||
			(tmp == 0 && !metadata.read.buffer)) {
		
		retry->timeout_in_ms <<= 1;
		if (retry->timeout_in_ms > SMUX_RX_RETRY_MAX_MS) {
			
			SMUX_ERR("%s: ch %d RX retry client timeout\n",
					__func__, ch->lcid);
			spin_lock_irqsave(&ch->state_lock_lhb1, flags);
			tx_ready = smux_remove_rx_retry(ch, retry);
			spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
			schedule_notify(ch->lcid, SMUX_READ_FAIL, NULL);
			if (tx_ready)
				list_channel(ch);
		}
	} else {
		
		SMUX_ERR("%s: ch %d RX retry client failed (%d)\n",
				__func__, ch->lcid, tmp);
		spin_lock_irqsave(&ch->state_lock_lhb1, flags);
		tx_ready = smux_remove_rx_retry(ch, retry);
		spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
		schedule_notify(ch->lcid, SMUX_READ_FAIL, NULL);
		if (tx_ready)
			list_channel(ch);
	}

	
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);
	if (!list_empty(&ch->rx_retry_queue)) {
		retry = list_first_entry(&ch->rx_retry_queue,
						struct smux_rx_pkt_retry,
						rx_retry_list);

		if (immediate_retry)
			queue_delayed_work(smux_rx_wq, &ch->rx_retry_work, 0);
		else
			queue_delayed_work(smux_rx_wq, &ch->rx_retry_work,
					msecs_to_jiffies(retry->timeout_in_ms));
	}
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
}

static void smux_tx_worker(struct work_struct *work)
{
	struct smux_pkt_t *pkt;
	struct smux_lch_t *ch;
	unsigned low_wm_notif;
	unsigned lcid;
	unsigned long flags;


	while (!smux.in_reset) {
		pkt = NULL;
		low_wm_notif = 0;

		spin_lock_irqsave(&smux.tx_lock_lha2, flags);

		
		if (smux.power_state == SMUX_PWR_OFF) {
			if (!list_empty(&smux.lch_tx_ready_list) ||
			   !list_empty(&smux.power_queue)) {
				
				SMUX_PWR("smux: %s: Power %d->%d\n", __func__,
						smux.power_state,
						SMUX_PWR_TURNING_ON);
				smux.local_initiated_wakeup_count++;
				smux.power_state = SMUX_PWR_TURNING_ON;
				spin_unlock_irqrestore(&smux.tx_lock_lha2,
						flags);
				queue_work(smux_tx_wq, &smux_wakeup_work);
			} else {
				
				spin_unlock_irqrestore(&smux.tx_lock_lha2,
						flags);
			}
			break;
		}

		
		if (!list_empty(&smux.power_queue)) {
			pkt = list_first_entry(&smux.power_queue,
					struct smux_pkt_t, list);
			list_del(&pkt->list);
			spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);

			
			spin_lock_irqsave(&smux.tx_lock_lha2, flags);
			if (smux.power_state == SMUX_PWR_TURNING_OFF_FLUSH &&
				pkt->hdr.cmd == SMUX_CMD_PWR_CTL) {
				if (pkt->hdr.flags & SMUX_CMD_PWR_CTL_ACK ||
					smux.power_ctl_remote_req_received) {
					SMUX_PWR(
					"smux: %s: Power %d->%d\n", __func__,
							smux.power_state,
							SMUX_PWR_OFF_FLUSH);
					smux.power_state = SMUX_PWR_OFF_FLUSH;
					smux.power_ctl_remote_req_received = 0;
					queue_work(smux_tx_wq,
							&smux_inactivity_work);
				} else {
					
					SMUX_PWR(
					"smux: %s: Power %d->%d\n", __func__,
							smux.power_state,
							SMUX_PWR_TURNING_OFF);
					smux.power_state = SMUX_PWR_TURNING_OFF;
				}
			}
			spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);

			
			smux_uart_power_on();
			smux.tx_activity_flag = 1;
			SMUX_PWR_PKT_TX(pkt);
			if (!smux_byte_loopback) {
				smux_tx_tty(pkt);
				smux_flush_tty();
			} else {
				smux_tx_loopback(pkt);
			}

			smux_free_pkt(pkt);
			continue;
		}

		
		if (list_empty(&smux.lch_tx_ready_list)) {
			
			SMUX_DBG("smux: %s: no more ready channels, exiting\n",
					__func__);
			spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
			break;
		}
		smux.tx_activity_flag = 1;

		if (smux.power_state != SMUX_PWR_ON) {
			
			SMUX_DBG("smux: %s: waiting for link up (state %d)\n",
					__func__,
					smux.power_state);
			spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
			break;
		}

		
		ch = list_first_entry(&smux.lch_tx_ready_list,
						struct smux_lch_t,
						tx_ready_list);

		spin_lock(&ch->state_lock_lhb1);
		spin_lock(&ch->tx_lock_lhb2);
		if (!list_empty(&ch->tx_queue)) {
			if (ch->tx_flow_control || !IS_FULLY_OPENED(ch)) {
				struct smux_pkt_t *curr;
				list_for_each_entry(curr, &ch->tx_queue, list) {
					if (curr->hdr.cmd != SMUX_CMD_DATA) {
						pkt = curr;
						break;
					}
				}
			} else {
				
				pkt = list_first_entry(&ch->tx_queue,
						struct smux_pkt_t, list);
			}
		}

		if (pkt) {
			list_del(&pkt->list);

			
			if (pkt->hdr.cmd == SMUX_CMD_DATA) {
				--ch->tx_pending_data_cnt;
				if (ch->notify_lwm &&
					ch->tx_pending_data_cnt
						<= SMUX_TX_WM_LOW) {
					ch->notify_lwm = 0;
					low_wm_notif = 1;
				}
			}

			
			list_rotate_left(&smux.lch_tx_ready_list);
		} else {
			
			list_del(&ch->tx_ready_list);
			INIT_LIST_HEAD(&ch->tx_ready_list);
		}
		lcid = ch->lcid;
		spin_unlock(&ch->tx_lock_lhb2);
		spin_unlock(&ch->state_lock_lhb1);
		spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);

		if (low_wm_notif)
			schedule_notify(lcid, SMUX_LOW_WM_HIT, NULL);

		
		smux_tx_pkt(ch, pkt);
		smux_free_pkt(pkt);
	}
}

static int smux_rx_flow_control_updated(struct smux_lch_t *ch)
{
	int updated = 0;
	int prev_state;

	prev_state = ch->local_tiocm & SMUX_CMD_STATUS_FLOW_CNTL;

	if (ch->rx_flow_control_client || ch->rx_flow_control_auto)
		ch->local_tiocm |= SMUX_CMD_STATUS_FLOW_CNTL;
	else
		ch->local_tiocm &= ~SMUX_CMD_STATUS_FLOW_CNTL;

	if (prev_state != (ch->local_tiocm & SMUX_CMD_STATUS_FLOW_CNTL)) {
		smux_send_status_cmd(ch);
		updated = 1;
	}

	return updated;
}

static void smux_flush_workqueues(void)
{
	smux.in_reset = 1;

	SMUX_DBG("smux: %s: flushing tx wq\n", __func__);
	flush_workqueue(smux_tx_wq);
	SMUX_DBG("smux: %s: flushing rx wq\n", __func__);
	flush_workqueue(smux_rx_wq);
	SMUX_DBG("smux: %s: flushing notify wq\n", __func__);
	flush_workqueue(smux_notify_wq);
}


int msm_smux_set_ch_option(uint8_t lcid, uint32_t set, uint32_t clear)
{
	unsigned long flags;
	struct smux_lch_t *ch;
	int tx_ready = 0;
	int ret = 0;

	if (smux_assert_lch_id(lcid))
		return -ENXIO;

	ch = &smux_lch[lcid];
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);

	
	if (set & SMUX_CH_OPTION_LOCAL_LOOPBACK)
		ch->local_mode = SMUX_LCH_MODE_LOCAL_LOOPBACK;

	if (clear & SMUX_CH_OPTION_LOCAL_LOOPBACK)
		ch->local_mode = SMUX_LCH_MODE_NORMAL;

	
	if (set & SMUX_CH_OPTION_REMOTE_LOOPBACK)
		ch->local_mode = SMUX_LCH_MODE_REMOTE_LOOPBACK;

	if (clear & SMUX_CH_OPTION_REMOTE_LOOPBACK)
		ch->local_mode = SMUX_LCH_MODE_NORMAL;

	
	if (set & SMUX_CH_OPTION_REMOTE_TX_STOP) {
		ch->rx_flow_control_client = 1;
		tx_ready |= smux_rx_flow_control_updated(ch);
	}

	if (clear & SMUX_CH_OPTION_REMOTE_TX_STOP) {
		ch->rx_flow_control_client = 0;
		tx_ready |= smux_rx_flow_control_updated(ch);
	}

	
	if (set & SMUX_CH_OPTION_AUTO_REMOTE_TX_STOP) {
		SMUX_DBG("smux: %s: auto rx flow control option enabled\n",
			__func__);
		ch->options |= SMUX_CH_OPTION_AUTO_REMOTE_TX_STOP;
	}

	if (clear & SMUX_CH_OPTION_AUTO_REMOTE_TX_STOP) {
		SMUX_DBG("smux: %s: auto rx flow control option disabled\n",
			__func__);
		ch->options &= ~SMUX_CH_OPTION_AUTO_REMOTE_TX_STOP;
		ch->rx_flow_control_auto = 0;
		tx_ready |= smux_rx_flow_control_updated(ch);
	}

	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);

	if (tx_ready)
		list_channel(ch);

	return ret;
}

int msm_smux_open(uint8_t lcid, void *priv,
	void (*notify)(void *priv, int event_type, const void *metadata),
	int (*get_rx_buffer)(void *priv, void **pkt_priv, void **buffer,
								int size))
{
	int ret;
	struct smux_lch_t *ch;
	struct smux_pkt_t *pkt;
	int tx_ready = 0;
	unsigned long flags;

	if (smux_assert_lch_id(lcid))
		return -ENXIO;

	ch = &smux_lch[lcid];
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);

	if (ch->local_state == SMUX_LCH_LOCAL_CLOSING) {
		ret = -EAGAIN;
		goto out;
	}

	if (ch->local_state != SMUX_LCH_LOCAL_CLOSED) {
		SMUX_ERR("%s: open lcid %d local state %x invalid\n",
				__func__, lcid, ch->local_state);
		ret = -EINVAL;
		goto out;
	}

	SMUX_DBG("smux: lcid %d local state 0x%x -> 0x%x\n", lcid,
			ch->local_state,
			SMUX_LCH_LOCAL_OPENING);

	ch->rx_flow_control_auto = 0;
	ch->local_state = SMUX_LCH_LOCAL_OPENING;

	ch->priv = priv;
	ch->notify = notify;
	ch->get_rx_buffer = get_rx_buffer;
	ret = 0;

	
	pkt = smux_alloc_pkt();
	if (!pkt) {
		ret = -ENOMEM;
		goto out;
	}
	pkt->hdr.magic = SMUX_MAGIC;
	pkt->hdr.cmd = SMUX_CMD_OPEN_LCH;
	pkt->hdr.flags = SMUX_CMD_OPEN_POWER_COLLAPSE;
	if (ch->local_mode == SMUX_LCH_MODE_REMOTE_LOOPBACK)
		pkt->hdr.flags |= SMUX_CMD_OPEN_REMOTE_LOOPBACK;
	pkt->hdr.lcid = lcid;
	pkt->hdr.payload_len = 0;
	pkt->hdr.pad_len = 0;
	smux_tx_queue(pkt, ch, 0);
	tx_ready = 1;

out:
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);
	smux_rx_flow_control_updated(ch);
	if (tx_ready)
		list_channel(ch);
	return ret;
}

int msm_smux_close(uint8_t lcid)
{
	int ret = 0;
	struct smux_lch_t *ch;
	struct smux_pkt_t *pkt;
	int tx_ready = 0;
	unsigned long flags;

	if (smux_assert_lch_id(lcid))
		return -ENXIO;

	ch = &smux_lch[lcid];
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);
	ch->local_tiocm = 0x0;
	ch->remote_tiocm = 0x0;
	ch->tx_pending_data_cnt = 0;
	ch->notify_lwm = 0;
	ch->tx_flow_control = 0;

	
	spin_lock(&ch->tx_lock_lhb2);
	smux_purge_ch_tx_queue(ch, 0);
	spin_unlock(&ch->tx_lock_lhb2);

	
	if (ch->local_state == SMUX_LCH_LOCAL_OPENED ||
		ch->local_state == SMUX_LCH_LOCAL_OPENING) {
		SMUX_DBG("smux: lcid %d local state 0x%x -> 0x%x\n", lcid,
				ch->local_state,
				SMUX_LCH_LOCAL_CLOSING);

		ch->local_state = SMUX_LCH_LOCAL_CLOSING;
		pkt = smux_alloc_pkt();
		if (pkt) {
			pkt->hdr.cmd = SMUX_CMD_CLOSE_LCH;
			pkt->hdr.flags = 0;
			pkt->hdr.lcid = lcid;
			pkt->hdr.payload_len = 0;
			pkt->hdr.pad_len = 0;
			smux_tx_queue(pkt, ch, 0);
			tx_ready = 1;
		} else {
			SMUX_ERR("%s: pkt allocation failed\n", __func__);
			ret = -ENOMEM;
		}

		
		if (ch->rx_retry_queue_cnt)
			queue_delayed_work(smux_rx_wq, &ch->rx_retry_work, 0);
	}
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);

	if (tx_ready)
		list_channel(ch);

	return ret;
}

/**
 * Write data to a logical channel.
 *
 * @lcid      Logical channel ID
 * @pkt_priv  Client data that will be returned with the SMUX_WRITE_DONE or
 *            SMUX_WRITE_FAIL notification.
 * @data      Data to write
 * @len       Length of @data
 *
 * @returns   0 for success, <0 otherwise
 *
 * Data may be written immediately after msm_smux_open() is called,
 * but the data will wait in the transmit queue until the channel has
 * been fully opened.
 *
 * Once the data has been written, the client will receive either a completion
 * (SMUX_WRITE_DONE) or a failure notice (SMUX_WRITE_FAIL).
 */
int msm_smux_write(uint8_t lcid, void *pkt_priv, const void *data, int len)
{
	struct smux_lch_t *ch;
	struct smux_pkt_t *pkt;
	int tx_ready = 0;
	unsigned long flags;
	int ret;

	if (smux_assert_lch_id(lcid))
		return -ENXIO;

	ch = &smux_lch[lcid];
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);

	if (ch->local_state != SMUX_LCH_LOCAL_OPENED &&
		ch->local_state != SMUX_LCH_LOCAL_OPENING) {
		SMUX_ERR("%s: hdr.invalid local state %d channel %d\n",
					__func__, ch->local_state, lcid);
		ret = -EINVAL;
		goto out;
	}

	if (len > SMUX_MAX_PKT_SIZE - sizeof(struct smux_hdr_t)) {
		SMUX_ERR("%s: payload %d too large\n",
				__func__, len);
		ret = -E2BIG;
		goto out;
	}

	pkt = smux_alloc_pkt();
	if (!pkt) {
		ret = -ENOMEM;
		goto out;
	}

	pkt->hdr.cmd = SMUX_CMD_DATA;
	pkt->hdr.lcid = lcid;
	pkt->hdr.flags = 0;
	pkt->hdr.payload_len = len;
	pkt->payload = (void *)data;
	pkt->priv = pkt_priv;
	pkt->hdr.pad_len = 0;

	spin_lock(&ch->tx_lock_lhb2);
	
	SMUX_DBG("smux: %s: pending %d", __func__, ch->tx_pending_data_cnt);

	if (ch->tx_pending_data_cnt >= SMUX_TX_WM_HIGH) {
		SMUX_ERR("%s: ch %d high watermark %d exceeded %d\n",
				__func__, lcid, SMUX_TX_WM_HIGH,
				ch->tx_pending_data_cnt);
		ret = -EAGAIN;
		goto out_inner;
	}

	
	if (++ch->tx_pending_data_cnt == SMUX_TX_WM_HIGH) {
		ch->notify_lwm = 1;
		SMUX_ERR("%s: high watermark hit\n", __func__);
		schedule_notify(lcid, SMUX_HIGH_WM_HIT, NULL);
	}
	list_add_tail(&pkt->list, &ch->tx_queue);

	
	if (IS_FULLY_OPENED(ch))
		tx_ready = 1;

	ret = 0;

out_inner:
	spin_unlock(&ch->tx_lock_lhb2);

out:
	if (ret)
		smux_free_pkt(pkt);
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);

	if (tx_ready)
		list_channel(ch);

	return ret;
}

int msm_smux_is_ch_full(uint8_t lcid)
{
	struct smux_lch_t *ch;
	unsigned long flags;
	int is_full = 0;

	if (smux_assert_lch_id(lcid))
		return -ENXIO;

	ch = &smux_lch[lcid];

	spin_lock_irqsave(&ch->tx_lock_lhb2, flags);
	if (ch->tx_pending_data_cnt >= SMUX_TX_WM_HIGH)
		is_full = 1;
	spin_unlock_irqrestore(&ch->tx_lock_lhb2, flags);

	return is_full;
}

int msm_smux_is_ch_low(uint8_t lcid)
{
	struct smux_lch_t *ch;
	unsigned long flags;
	int is_low = 0;

	if (smux_assert_lch_id(lcid))
		return -ENXIO;

	ch = &smux_lch[lcid];

	spin_lock_irqsave(&ch->tx_lock_lhb2, flags);
	if (ch->tx_pending_data_cnt <= SMUX_TX_WM_LOW)
		is_low = 1;
	spin_unlock_irqrestore(&ch->tx_lock_lhb2, flags);

	return is_low;
}

static int smux_send_status_cmd(struct smux_lch_t *ch)
{
	struct smux_pkt_t *pkt;

	if (!ch)
		return -EINVAL;

	pkt = smux_alloc_pkt();
	if (!pkt)
		return -ENOMEM;

	pkt->hdr.lcid = ch->lcid;
	pkt->hdr.cmd = SMUX_CMD_STATUS;
	pkt->hdr.flags = ch->local_tiocm;
	pkt->hdr.payload_len = 0;
	pkt->hdr.pad_len = 0;
	smux_tx_queue(pkt, ch, 0);

	return 0;
}

long msm_smux_tiocm_get_atomic(struct smux_lch_t *ch)
{
	long status = 0x0;

	status |= (ch->remote_tiocm & SMUX_CMD_STATUS_RTC) ? TIOCM_DSR : 0;
	status |= (ch->remote_tiocm & SMUX_CMD_STATUS_RTR) ? TIOCM_CTS : 0;
	status |= (ch->remote_tiocm & SMUX_CMD_STATUS_RI) ? TIOCM_RI : 0;
	status |= (ch->remote_tiocm & SMUX_CMD_STATUS_DCD) ? TIOCM_CD : 0;

	status |= (ch->local_tiocm & SMUX_CMD_STATUS_RTC) ? TIOCM_DTR : 0;
	status |= (ch->local_tiocm & SMUX_CMD_STATUS_RTR) ? TIOCM_RTS : 0;

	return status;
}

long msm_smux_tiocm_get(uint8_t lcid)
{
	struct smux_lch_t *ch;
	unsigned long flags;
	long status = 0x0;

	if (smux_assert_lch_id(lcid))
		return -ENXIO;

	ch = &smux_lch[lcid];
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);
	status = msm_smux_tiocm_get_atomic(ch);
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);

	return status;
}

int msm_smux_tiocm_set(uint8_t lcid, uint32_t set, uint32_t clear)
{
	struct smux_lch_t *ch;
	unsigned long flags;
	uint8_t old_status;
	uint8_t status_set = 0x0;
	uint8_t status_clear = 0x0;
	int tx_ready = 0;
	int ret = 0;

	if (smux_assert_lch_id(lcid))
		return -ENXIO;

	ch = &smux_lch[lcid];
	spin_lock_irqsave(&ch->state_lock_lhb1, flags);

	status_set |= (set & TIOCM_DTR) ? SMUX_CMD_STATUS_RTC : 0;
	status_set |= (set & TIOCM_RTS) ? SMUX_CMD_STATUS_RTR : 0;
	status_set |= (set & TIOCM_RI) ? SMUX_CMD_STATUS_RI : 0;
	status_set |= (set & TIOCM_CD) ? SMUX_CMD_STATUS_DCD : 0;

	status_clear |= (clear & TIOCM_DTR) ? SMUX_CMD_STATUS_RTC : 0;
	status_clear |= (clear & TIOCM_RTS) ? SMUX_CMD_STATUS_RTR : 0;
	status_clear |= (clear & TIOCM_RI) ? SMUX_CMD_STATUS_RI : 0;
	status_clear |= (clear & TIOCM_CD) ? SMUX_CMD_STATUS_DCD : 0;

	old_status = ch->local_tiocm;
	ch->local_tiocm |= status_set;
	ch->local_tiocm &= ~status_clear;

	if (ch->local_tiocm != old_status) {
		ret = smux_send_status_cmd(ch);
		tx_ready = 1;
	}
	spin_unlock_irqrestore(&ch->state_lock_lhb1, flags);

	if (tx_ready)
		list_channel(ch);

	return ret;
}

static struct notifier_block ssr_notifier = {
	.notifier_call = ssr_notifier_cb,
};

static int ssr_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data)
{
	unsigned long flags;
	int i;
	int tmp;
	int power_off_uart = 0;

	if (code == SUBSYS_BEFORE_SHUTDOWN) {
		SMUX_DBG("smux: %s: ssr - before shutdown\n", __func__);
		mutex_lock(&smux.mutex_lha0);
		smux.in_reset = 1;
		smux.remote_is_alive = 0;
		mutex_unlock(&smux.mutex_lha0);
		return NOTIFY_DONE;
	} else if (code == SUBSYS_AFTER_POWERUP) {
		
		SMUX_DBG("smux: %s: ssr - after power-up\n", __func__);
		mutex_lock(&smux.mutex_lha0);
		if (smux.ld_open_count > 0
				&& !smux.platform_devs_registered) {
			for (i = 0; i < ARRAY_SIZE(smux_devs); ++i) {
				SMUX_DBG("smux: %s: register pdev '%s'\n",
					__func__, smux_devs[i].name);
				smux_devs[i].dev.release = smux_pdev_release;
				tmp = platform_device_register(&smux_devs[i]);
				if (tmp)
					SMUX_ERR(
						"%s: error %d registering device %s\n",
					   __func__, tmp, smux_devs[i].name);
			}
			smux.platform_devs_registered = 1;
		}
		mutex_unlock(&smux.mutex_lha0);
		return NOTIFY_DONE;
	} else if (code != SUBSYS_AFTER_SHUTDOWN) {
		return NOTIFY_DONE;
	}
	SMUX_DBG("smux: %s: ssr - after shutdown\n", __func__);

	
	smux_flush_workqueues();
	mutex_lock(&smux.mutex_lha0);
	if (smux.ld_open_count > 0) {
		smux_lch_purge();
		if (smux.tty)
			tty_driver_flush_buffer(smux.tty);

		
		if (smux.platform_devs_registered) {
			for (i = 0; i < ARRAY_SIZE(smux_devs); ++i) {
				SMUX_DBG("smux: %s: unregister pdev '%s'\n",
						__func__, smux_devs[i].name);
				platform_device_unregister(&smux_devs[i]);
			}
			smux.platform_devs_registered = 0;
		}

		
		spin_lock_irqsave(&smux.tx_lock_lha2, flags);
		if (smux.power_state != SMUX_PWR_OFF) {
			SMUX_PWR("smux: %s: SSR - turning off UART\n",
							__func__);
			smux.power_state = SMUX_PWR_OFF;
			power_off_uart = 1;
		}
		smux.powerdown_enabled = 0;
		spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);

		if (power_off_uart)
			smux_uart_power_off_atomic();
	}
	smux.tx_activity_flag = 0;
	smux.rx_activity_flag = 0;
	smux.rx_state = SMUX_RX_IDLE;
	smux.in_reset = 0;
	smux.remote_is_alive = 0;
	mutex_unlock(&smux.mutex_lha0);

	return NOTIFY_DONE;
}

static void smux_pdev_release(struct device *dev)
{
	struct platform_device *pdev;

	pdev = container_of(dev, struct platform_device, dev);
	SMUX_DBG("smux: %s: releasing pdev %p '%s'\n",
			__func__, pdev, pdev->name);
	memset(&pdev->dev, 0x0, sizeof(pdev->dev));
}

static int smuxld_open(struct tty_struct *tty)
{
	int i;
	int tmp;
	unsigned long flags;

	if (!smux.is_initialized)
		return -ENODEV;

	mutex_lock(&smux.mutex_lha0);
	if (smux.ld_open_count) {
		SMUX_ERR("%s: %p multiple instances not supported\n",
			__func__, tty);
		mutex_unlock(&smux.mutex_lha0);
		return -EEXIST;
	}

	if (tty->ops->write == NULL) {
		SMUX_ERR("%s: tty->ops->write already NULL\n", __func__);
		mutex_unlock(&smux.mutex_lha0);
		return -EINVAL;
	}

	
	++smux.ld_open_count;
	smux.in_reset = 0;
	smux.tty = tty;
	tty->disc_data = &smux;
	tty->receive_room = TTY_RECEIVE_ROOM;
	tty_driver_flush_buffer(tty);

	
	spin_lock_irqsave(&smux.tx_lock_lha2, flags);
	if (smux.power_state == SMUX_PWR_OFF) {
		SMUX_PWR("smux: %s: powering off uart\n", __func__);
		smux.power_state = SMUX_PWR_OFF_FLUSH;
		spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
		queue_work(smux_tx_wq, &smux_inactivity_work);
	} else {
		spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);
	}

	
	for (i = 0; i < ARRAY_SIZE(smux_devs); ++i) {
		SMUX_DBG("smux: %s: register pdev '%s'\n",
				__func__, smux_devs[i].name);
		smux_devs[i].dev.release = smux_pdev_release;
		tmp = platform_device_register(&smux_devs[i]);
		if (tmp)
			SMUX_ERR("%s: error %d registering device %s\n",
				   __func__, tmp, smux_devs[i].name);
	}
	smux.platform_devs_registered = 1;
	mutex_unlock(&smux.mutex_lha0);
	return 0;
}

static void smuxld_close(struct tty_struct *tty)
{
	unsigned long flags;
	int power_up_uart = 0;
	int i;

	SMUX_DBG("smux: %s: ldisc unload\n", __func__);
	smux_flush_workqueues();

	mutex_lock(&smux.mutex_lha0);
	if (smux.ld_open_count <= 0) {
		SMUX_ERR("%s: invalid ld count %d\n", __func__,
			smux.ld_open_count);
		mutex_unlock(&smux.mutex_lha0);
		return;
	}
	--smux.ld_open_count;

	
	smux_lch_purge();

	
	if (smux.platform_devs_registered) {
		for (i = 0; i < ARRAY_SIZE(smux_devs); ++i) {
			SMUX_DBG("smux: %s: unregister pdev '%s'\n",
					__func__, smux_devs[i].name);
			platform_device_unregister(&smux_devs[i]);
		}
		smux.platform_devs_registered = 0;
	}

	
	spin_lock_irqsave(&smux.tx_lock_lha2, flags);
	if (smux.power_state == SMUX_PWR_OFF)
		power_up_uart = 1;
	smux.power_state = SMUX_PWR_OFF;
	smux.powerdown_enabled = 0;
	smux.tx_activity_flag = 0;
	smux.rx_activity_flag = 0;
	spin_unlock_irqrestore(&smux.tx_lock_lha2, flags);

	if (power_up_uart)
		smux_uart_power_on_atomic();

	smux.rx_state = SMUX_RX_IDLE;

	
	smux.tty = NULL;
	smux.remote_is_alive = 0;
	mutex_unlock(&smux.mutex_lha0);
	SMUX_DBG("smux: %s: ldisc complete\n", __func__);
}

void smuxld_receive_buf(struct tty_struct *tty, const unsigned char *cp,
			   char *fp, int count)
{
	int i;
	int last_idx = 0;
	const char *tty_name = NULL;
	char *f;

	
	for (i = 0, f = fp; i < count; ++i, ++f) {
		if (*f != TTY_NORMAL) {
			if (tty)
				tty_name = tty->name;
			SMUX_ERR("%s: TTY %s Error %d (%s)\n", __func__,
				   tty_name, *f, tty_flag_to_str(*f));

			
			smux_rx_state_machine(cp + last_idx, i - last_idx,
					TTY_NORMAL);

			
			smux_rx_state_machine(cp + i, 1, *f);
			last_idx = i + 1;
		}
	}

	
	smux_rx_state_machine(cp + last_idx, count - last_idx, TTY_NORMAL);
}

static void smuxld_flush_buffer(struct tty_struct *tty)
{
	SMUX_ERR("%s: not supported\n", __func__);
}

static ssize_t	smuxld_chars_in_buffer(struct tty_struct *tty)
{
	SMUX_ERR("%s: not supported\n", __func__);
	return -ENODEV;
}

static ssize_t	smuxld_read(struct tty_struct *tty, struct file *file,
		unsigned char __user *buf, size_t nr)
{
	SMUX_ERR("%s: not supported\n", __func__);
	return -ENODEV;
}

static ssize_t	smuxld_write(struct tty_struct *tty, struct file *file,
		 const unsigned char *buf, size_t nr)
{
	SMUX_ERR("%s: not supported\n", __func__);
	return -ENODEV;
}

static int	smuxld_ioctl(struct tty_struct *tty, struct file *file,
		 unsigned int cmd, unsigned long arg)
{
	SMUX_ERR("%s: not supported\n", __func__);
	return -ENODEV;
}

static unsigned int smuxld_poll(struct tty_struct *tty, struct file *file,
			 struct poll_table_struct *tbl)
{
	SMUX_ERR("%s: not supported\n", __func__);
	return -ENODEV;
}

static void smuxld_write_wakeup(struct tty_struct *tty)
{
	SMUX_ERR("%s: not supported\n", __func__);
}

static struct tty_ldisc_ops smux_ldisc_ops = {
	.owner           = THIS_MODULE,
	.magic           = TTY_LDISC_MAGIC,
	.name            = "n_smux",
	.open            = smuxld_open,
	.close           = smuxld_close,
	.flush_buffer    = smuxld_flush_buffer,
	.chars_in_buffer = smuxld_chars_in_buffer,
	.read            = smuxld_read,
	.write           = smuxld_write,
	.ioctl           = smuxld_ioctl,
	.poll            = smuxld_poll,
	.receive_buf     = smuxld_receive_buf,
	.write_wakeup    = smuxld_write_wakeup
};

static int __init smux_init(void)
{
	int ret;

	mutex_init(&smux.mutex_lha0);

	spin_lock_init(&smux.rx_lock_lha1);
	smux.rx_state = SMUX_RX_IDLE;
	smux.power_state = SMUX_PWR_OFF;
	smux.pwr_wakeup_delay_us = 1;
	smux.powerdown_enabled = 0;
	smux.power_ctl_remote_req_received = 0;
	INIT_LIST_HEAD(&smux.power_queue);
	smux.rx_activity_flag = 0;
	smux.tx_activity_flag = 0;
	smux.recv_len = 0;
	smux.tty = NULL;
	smux.ld_open_count = 0;
	smux.in_reset = 0;
	smux.remote_is_alive = 0;
	smux.is_initialized = 1;
	smux.platform_devs_registered = 0;
	smux_byte_loopback = 0;

	spin_lock_init(&smux.tx_lock_lha2);
	INIT_LIST_HEAD(&smux.lch_tx_ready_list);

	ret	= tty_register_ldisc(N_SMUX, &smux_ldisc_ops);
	if (ret != 0) {
		SMUX_ERR("%s: error %d registering line discipline\n",
				__func__, ret);
		return ret;
	}

	subsys_notif_register_notifier("external_modem", &ssr_notifier);

	ret = lch_init();
	if (ret != 0) {
		SMUX_ERR("%s: lch_init failed\n", __func__);
		return ret;
	}

	log_ctx = ipc_log_context_create(1, "smux");
	if (!log_ctx) {
		SMUX_ERR("%s: unable to create log context\n", __func__);
		disable_ipc_logging = 1;
	}

	return 0;
}

static void __exit smux_exit(void)
{
	int ret;

	ret	= tty_unregister_ldisc(N_SMUX);
	if (ret != 0) {
		SMUX_ERR("%s error %d unregistering line discipline\n",
				__func__, ret);
		return;
	}
}

module_init(smux_init);
module_exit(smux_exit);

MODULE_DESCRIPTION("Serial Mux TTY Line Discipline");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS_LDISC(N_SMUX);
