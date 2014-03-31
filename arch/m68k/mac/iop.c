/*
 * I/O Processor (IOP) management
 * Written and (C) 1999 by Joshua M. Thompson (funaho@jurai.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice and this list of conditions.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice and this list of conditions in the documentation and/or other
 *    materials provided with the distribution.
 */

/*
 * The IOP chips are used in the IIfx and some Quadras (900, 950) to manage
 * serial and ADB. They are actually a 6502 processor and some glue logic.
 *
 * 990429 (jmt) - Initial implementation, just enough to knock the SCC IOP
 *		  into compatible mode so nobody has to fiddle with the
 *		  Serial Switch control panel anymore.
 * 990603 (jmt) - Added code to grab the correct ISM IOP interrupt for OSS
 *		  and non-OSS machines (at least I hope it's correct on a
 *		  non-OSS machine -- someone with a Q900 or Q950 needs to
 *		  check this.)
 * 990605 (jmt) - Rearranged things a bit wrt IOP detection; iop_present is
 *		  gone, IOP base addresses are now in an array and the
 *		  globally-visible functions take an IOP number instead of an
 *		  an actual base address.
 * 990610 (jmt) - Finished the message passing framework and it seems to work.
 *		  Sending _definitely_ works; my adb-bus.c mods can send
 *		  messages and receive the MSG_COMPLETED status back from the
 *		  IOP. The trick now is figuring out the message formats.
 * 990611 (jmt) - More cleanups. Fixed problem where unclaimed messages on a
 *		  receive channel were never properly acknowledged. Bracketed
 *		  the remaining debug printk's with #ifdef's and disabled
 *		  debugging. I can now type on the console.
 * 990612 (jmt) - Copyright notice added. Reworked the way replies are handled.
 *		  It turns out that replies are placed back in the send buffer
 *		  for that channel; messages on the receive channels are always
 *		  unsolicited messages from the IOP (and our replies to them
 *		  should go back in the receive channel.) Also added tracking
 *		  of device names to the listener functions ala the interrupt
 *		  handlers.
 * 990729 (jmt) - Added passing of pt_regs structure to IOP handlers. This is
 *		  used by the new unified ADB driver.
 *
 * TODO:
 *
 * o Something should be periodically checking iop_alive() to make sure the
 *   IOP hasn't died.
 * o Some of the IOP manager routines need better error checking and
 *   return codes. Nothing major, just prettying up.
 */


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <asm/bootinfo.h>
#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/mac_iop.h>



int iop_scc_present,iop_ism_present;


struct listener {
	const char *devname;
	void (*handler)(struct iop_msg *);
};


static volatile struct mac_iop *iop_base[NUM_IOPS];


static struct iop_msg iop_msg_pool[NUM_IOP_MSGS];
static struct iop_msg *iop_send_queue[NUM_IOPS][NUM_IOP_CHAN];
static struct listener iop_listeners[NUM_IOPS][NUM_IOP_CHAN];

irqreturn_t iop_ism_irq(int, void *);


static __inline__ void iop_loadaddr(volatile struct mac_iop *iop, __u16 addr)
{
	iop->ram_addr_lo = addr;
	iop->ram_addr_hi = addr >> 8;
}

static __inline__ __u8 iop_readb(volatile struct mac_iop *iop, __u16 addr)
{
	iop->ram_addr_lo = addr;
	iop->ram_addr_hi = addr >> 8;
	return iop->ram_data;
}

static __inline__ void iop_writeb(volatile struct mac_iop *iop, __u16 addr, __u8 data)
{
	iop->ram_addr_lo = addr;
	iop->ram_addr_hi = addr >> 8;
	iop->ram_data = data;
}

static __inline__ void iop_stop(volatile struct mac_iop *iop)
{
	iop->status_ctrl &= ~IOP_RUN;
}

static __inline__ void iop_start(volatile struct mac_iop *iop)
{
	iop->status_ctrl = IOP_RUN | IOP_AUTOINC;
}

static __inline__ void iop_bypass(volatile struct mac_iop *iop)
{
	iop->status_ctrl |= IOP_BYPASS;
}

static __inline__ void iop_interrupt(volatile struct mac_iop *iop)
{
	iop->status_ctrl |= IOP_IRQ;
}

static int iop_alive(volatile struct mac_iop *iop)
{
	int retval;

	retval = (iop_readb(iop, IOP_ADDR_ALIVE) == 0xFF);
	iop_writeb(iop, IOP_ADDR_ALIVE, 0);
	return retval;
}

static struct iop_msg *iop_alloc_msg(void)
{
	int i;
	unsigned long flags;

	local_irq_save(flags);

	for (i = 0 ; i < NUM_IOP_MSGS ; i++) {
		if (iop_msg_pool[i].status == IOP_MSGSTATUS_UNUSED) {
			iop_msg_pool[i].status = IOP_MSGSTATUS_WAITING;
			local_irq_restore(flags);
			return &iop_msg_pool[i];
		}
	}

	local_irq_restore(flags);
	return NULL;
}

static void iop_free_msg(struct iop_msg *msg)
{
	msg->status = IOP_MSGSTATUS_UNUSED;
}


void __init iop_preinit(void)
{
	if (macintosh_config->scc_type == MAC_SCC_IOP) {
		if (macintosh_config->ident == MAC_MODEL_IIFX) {
			iop_base[IOP_NUM_SCC] = (struct mac_iop *) SCC_IOP_BASE_IIFX;
		} else {
			iop_base[IOP_NUM_SCC] = (struct mac_iop *) SCC_IOP_BASE_QUADRA;
		}
		iop_base[IOP_NUM_SCC]->status_ctrl = 0x87;
		iop_scc_present = 1;
	} else {
		iop_base[IOP_NUM_SCC] = NULL;
		iop_scc_present = 0;
	}
	if (macintosh_config->adb_type == MAC_ADB_IOP) {
		if (macintosh_config->ident == MAC_MODEL_IIFX) {
			iop_base[IOP_NUM_ISM] = (struct mac_iop *) ISM_IOP_BASE_IIFX;
		} else {
			iop_base[IOP_NUM_ISM] = (struct mac_iop *) ISM_IOP_BASE_QUADRA;
		}
		iop_base[IOP_NUM_ISM]->status_ctrl = 0;
		iop_ism_present = 1;
	} else {
		iop_base[IOP_NUM_ISM] = NULL;
		iop_ism_present = 0;
	}
}


void __init iop_init(void)
{
	int i;

	if (iop_scc_present) {
		printk("IOP: detected SCC IOP at %p\n", iop_base[IOP_NUM_SCC]);
	}
	if (iop_ism_present) {
		printk("IOP: detected ISM IOP at %p\n", iop_base[IOP_NUM_ISM]);
		iop_start(iop_base[IOP_NUM_ISM]);
		iop_alive(iop_base[IOP_NUM_ISM]); 
	}

	

	for (i = 0 ; i < NUM_IOP_MSGS ; i++) {
		iop_msg_pool[i].status = IOP_MSGSTATUS_UNUSED;
	}

	for (i = 0 ; i < NUM_IOP_CHAN ; i++) {
		iop_send_queue[IOP_NUM_SCC][i] = NULL;
		iop_send_queue[IOP_NUM_ISM][i] = NULL;
		iop_listeners[IOP_NUM_SCC][i].devname = NULL;
		iop_listeners[IOP_NUM_SCC][i].handler = NULL;
		iop_listeners[IOP_NUM_ISM][i].devname = NULL;
		iop_listeners[IOP_NUM_ISM][i].handler = NULL;
	}
}


void __init iop_register_interrupts(void)
{
	if (iop_ism_present) {
		if (macintosh_config->ident == MAC_MODEL_IIFX) {
			if (request_irq(IRQ_MAC_ADB, iop_ism_irq, 0,
					"ISM IOP", (void *)IOP_NUM_ISM))
				pr_err("Couldn't register ISM IOP interrupt\n");
		} else {
			if (request_irq(IRQ_VIA2_0, iop_ism_irq, 0, "ISM IOP",
					(void *)IOP_NUM_ISM))
				pr_err("Couldn't register ISM IOP interrupt\n");
		}
		if (!iop_alive(iop_base[IOP_NUM_ISM])) {
			printk("IOP: oh my god, they killed the ISM IOP!\n");
		} else {
			printk("IOP: the ISM IOP seems to be alive.\n");
		}
	}
}


int iop_listen(uint iop_num, uint chan,
		void (*handler)(struct iop_msg *),
		const char *devname)
{
	if ((iop_num >= NUM_IOPS) || !iop_base[iop_num]) return -EINVAL;
	if (chan >= NUM_IOP_CHAN) return -EINVAL;
	if (iop_listeners[iop_num][chan].handler && handler) return -EINVAL;
	iop_listeners[iop_num][chan].devname = devname;
	iop_listeners[iop_num][chan].handler = handler;
	return 0;
}


void iop_complete_message(struct iop_msg *msg)
{
	int iop_num = msg->iop_num;
	int chan = msg->channel;
	int i,offset;

#ifdef DEBUG_IOP
	printk("iop_complete(%p): iop %d chan %d\n", msg, msg->iop_num, msg->channel);
#endif

	offset = IOP_ADDR_RECV_MSG + (msg->channel * IOP_MSG_LEN);

	for (i = 0 ; i < IOP_MSG_LEN ; i++, offset++) {
		iop_writeb(iop_base[iop_num], offset, msg->reply[i]);
	}

	iop_writeb(iop_base[iop_num],
		   IOP_ADDR_RECV_STATE + chan, IOP_MSG_COMPLETE);
	iop_interrupt(iop_base[msg->iop_num]);

	iop_free_msg(msg);
}


static void iop_do_send(struct iop_msg *msg)
{
	volatile struct mac_iop *iop = iop_base[msg->iop_num];
	int i,offset;

	offset = IOP_ADDR_SEND_MSG + (msg->channel * IOP_MSG_LEN);

	for (i = 0 ; i < IOP_MSG_LEN ; i++, offset++) {
		iop_writeb(iop, offset, msg->message[i]);
	}

	iop_writeb(iop, IOP_ADDR_SEND_STATE + msg->channel, IOP_MSG_NEW);

	iop_interrupt(iop);
}


static void iop_handle_send(uint iop_num, uint chan)
{
	volatile struct mac_iop *iop = iop_base[iop_num];
	struct iop_msg *msg,*msg2;
	int i,offset;

#ifdef DEBUG_IOP
	printk("iop_handle_send: iop %d channel %d\n", iop_num, chan);
#endif

	iop_writeb(iop, IOP_ADDR_SEND_STATE + chan, IOP_MSG_IDLE);

	if (!(msg = iop_send_queue[iop_num][chan])) return;

	msg->status = IOP_MSGSTATUS_COMPLETE;
	offset = IOP_ADDR_SEND_MSG + (chan * IOP_MSG_LEN);
	for (i = 0 ; i < IOP_MSG_LEN ; i++, offset++) {
		msg->reply[i] = iop_readb(iop, offset);
	}
	if (msg->handler) (*msg->handler)(msg);
	msg2 = msg;
	msg = msg->next;
	iop_free_msg(msg2);

	iop_send_queue[iop_num][chan] = msg;
	if (msg) iop_do_send(msg);
}


static void iop_handle_recv(uint iop_num, uint chan)
{
	volatile struct mac_iop *iop = iop_base[iop_num];
	int i,offset;
	struct iop_msg *msg;

#ifdef DEBUG_IOP
	printk("iop_handle_recv: iop %d channel %d\n", iop_num, chan);
#endif

	msg = iop_alloc_msg();
	msg->iop_num = iop_num;
	msg->channel = chan;
	msg->status = IOP_MSGSTATUS_UNSOL;
	msg->handler = iop_listeners[iop_num][chan].handler;

	offset = IOP_ADDR_RECV_MSG + (chan * IOP_MSG_LEN);

	for (i = 0 ; i < IOP_MSG_LEN ; i++, offset++) {
		msg->message[i] = iop_readb(iop, offset);
	}

	iop_writeb(iop, IOP_ADDR_RECV_STATE + chan, IOP_MSG_RCVD);

	
	

	if (msg->handler) {
		(*msg->handler)(msg);
	} else {
#ifdef DEBUG_IOP
		printk("iop_handle_recv: unclaimed message on iop %d channel %d\n", iop_num, chan);
		printk("iop_handle_recv:");
		for (i = 0 ; i < IOP_MSG_LEN ; i++) {
			printk(" %02X", (uint) msg->message[i]);
		}
		printk("\n");
#endif
		iop_complete_message(msg);
	}
}


int iop_send_message(uint iop_num, uint chan, void *privdata,
		      uint msg_len, __u8 *msg_data,
		      void (*handler)(struct iop_msg *))
{
	struct iop_msg *msg, *q;

	if ((iop_num >= NUM_IOPS) || !iop_base[iop_num]) return -EINVAL;
	if (chan >= NUM_IOP_CHAN) return -EINVAL;
	if (msg_len > IOP_MSG_LEN) return -EINVAL;

	msg = iop_alloc_msg();
	if (!msg) return -ENOMEM;

	msg->next = NULL;
	msg->status = IOP_MSGSTATUS_WAITING;
	msg->iop_num = iop_num;
	msg->channel = chan;
	msg->caller_priv = privdata;
	memcpy(msg->message, msg_data, msg_len);
	msg->handler = handler;

	if (!(q = iop_send_queue[iop_num][chan])) {
		iop_send_queue[iop_num][chan] = msg;
	} else {
		while (q->next) q = q->next;
		q->next = msg;
	}

	if (iop_readb(iop_base[iop_num],
	    IOP_ADDR_SEND_STATE + chan) == IOP_MSG_IDLE) {
		iop_do_send(msg);
	}

	return 0;
}


void iop_upload_code(uint iop_num, __u8 *code_start,
		     uint code_len, __u16 shared_ram_start)
{
	if ((iop_num >= NUM_IOPS) || !iop_base[iop_num]) return;

	iop_loadaddr(iop_base[iop_num], shared_ram_start);

	while (code_len--) {
		iop_base[iop_num]->ram_data = *code_start++;
	}
}


void iop_download_code(uint iop_num, __u8 *code_start,
		       uint code_len, __u16 shared_ram_start)
{
	if ((iop_num >= NUM_IOPS) || !iop_base[iop_num]) return;

	iop_loadaddr(iop_base[iop_num], shared_ram_start);

	while (code_len--) {
		*code_start++ = iop_base[iop_num]->ram_data;
	}
}


__u8 *iop_compare_code(uint iop_num, __u8 *code_start,
		       uint code_len, __u16 shared_ram_start)
{
	if ((iop_num >= NUM_IOPS) || !iop_base[iop_num]) return code_start;

	iop_loadaddr(iop_base[iop_num], shared_ram_start);

	while (code_len--) {
		if (*code_start != iop_base[iop_num]->ram_data) {
			return code_start;
		}
		code_start++;
	}
	return (__u8 *) 0;
}


irqreturn_t iop_ism_irq(int irq, void *dev_id)
{
	uint iop_num = (uint) dev_id;
	volatile struct mac_iop *iop = iop_base[iop_num];
	int i,state;

#ifdef DEBUG_IOP
	printk("iop_ism_irq: status = %02X\n", (uint) iop->status_ctrl);
#endif

	

	if (iop->status_ctrl & IOP_INT0) {
		iop->status_ctrl = IOP_INT0 | IOP_RUN | IOP_AUTOINC;
#ifdef DEBUG_IOP
		printk("iop_ism_irq: new status = %02X, send states",
			(uint) iop->status_ctrl);
#endif
		for (i = 0 ; i < NUM_IOP_CHAN  ; i++) {
			state = iop_readb(iop, IOP_ADDR_SEND_STATE + i);
#ifdef DEBUG_IOP
			printk(" %02X", state);
#endif
			if (state == IOP_MSG_COMPLETE) {
				iop_handle_send(iop_num, i);
			}
		}
#ifdef DEBUG_IOP
		printk("\n");
#endif
	}

	if (iop->status_ctrl & IOP_INT1) {	
		iop->status_ctrl = IOP_INT1 | IOP_RUN | IOP_AUTOINC;
#ifdef DEBUG_IOP
		printk("iop_ism_irq: new status = %02X, recv states",
			(uint) iop->status_ctrl);
#endif
		for (i = 0 ; i < NUM_IOP_CHAN ; i++) {
			state = iop_readb(iop, IOP_ADDR_RECV_STATE + i);
#ifdef DEBUG_IOP
			printk(" %02X", state);
#endif
			if (state == IOP_MSG_NEW) {
				iop_handle_recv(iop_num, i);
			}
		}
#ifdef DEBUG_IOP
		printk("\n");
#endif
	}
	return IRQ_HANDLED;
}
