/*
 * Copyright (C) 2004 Hollis Blanchard <hollisb@us.ibm.com>, IBM
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */


#undef DEBUG

#include <linux/console.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/sysrq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <asm/hvcall.h>
#include <asm/hvconsole.h>
#include <asm/prom.h>
#include <asm/uaccess.h>
#include <asm/vio.h>
#include <asm/param.h>
#include <asm/hvsi.h>

#define HVSI_MAJOR	229
#define HVSI_MINOR	128
#define MAX_NR_HVSI_CONSOLES 4

#define HVSI_TIMEOUT (5*HZ)
#define HVSI_VERSION 1
#define HVSI_MAX_PACKET 256
#define HVSI_MAX_READ 16
#define HVSI_MAX_OUTGOING_DATA 12
#define N_OUTBUF 12

#define __ALIGNED__	__attribute__((__aligned__(sizeof(long))))

struct hvsi_struct {
	struct delayed_work writer;
	struct work_struct handshaker;
	wait_queue_head_t emptyq; 
	wait_queue_head_t stateq; 
	spinlock_t lock;
	int index;
	struct tty_struct *tty;
	int count;
	uint8_t throttle_buf[128];
	uint8_t outbuf[N_OUTBUF]; 
	
	uint8_t inbuf[HVSI_MAX_PACKET + HVSI_MAX_READ];
	uint8_t *inbuf_end;
	int n_throttle;
	int n_outbuf;
	uint32_t vtermno;
	uint32_t virq;
	atomic_t seqno; 
	uint16_t mctrl;
	uint8_t state;  
	uint8_t flags;
#ifdef CONFIG_MAGIC_SYSRQ
	uint8_t sysrq;
#endif 
};
static struct hvsi_struct hvsi_ports[MAX_NR_HVSI_CONSOLES];

static struct tty_driver *hvsi_driver;
static int hvsi_count;
static int (*hvsi_wait)(struct hvsi_struct *hp, int state);

enum HVSI_PROTOCOL_STATE {
	HVSI_CLOSED,
	HVSI_WAIT_FOR_VER_RESPONSE,
	HVSI_WAIT_FOR_VER_QUERY,
	HVSI_OPEN,
	HVSI_WAIT_FOR_MCTRL_RESPONSE,
	HVSI_FSP_DIED,
};
#define HVSI_CONSOLE 0x1

static inline int is_console(struct hvsi_struct *hp)
{
	return hp->flags & HVSI_CONSOLE;
}

static inline int is_open(struct hvsi_struct *hp)
{
	
	return (hp->state == HVSI_OPEN)
			|| (hp->state == HVSI_WAIT_FOR_MCTRL_RESPONSE);
}

static inline void print_state(struct hvsi_struct *hp)
{
#ifdef DEBUG
	static const char *state_names[] = {
		"HVSI_CLOSED",
		"HVSI_WAIT_FOR_VER_RESPONSE",
		"HVSI_WAIT_FOR_VER_QUERY",
		"HVSI_OPEN",
		"HVSI_WAIT_FOR_MCTRL_RESPONSE",
		"HVSI_FSP_DIED",
	};
	const char *name = (hp->state < ARRAY_SIZE(state_names))
		? state_names[hp->state] : "UNKNOWN";

	pr_debug("hvsi%i: state = %s\n", hp->index, name);
#endif 
}

static inline void __set_state(struct hvsi_struct *hp, int state)
{
	hp->state = state;
	print_state(hp);
	wake_up_all(&hp->stateq);
}

static inline void set_state(struct hvsi_struct *hp, int state)
{
	unsigned long flags;

	spin_lock_irqsave(&hp->lock, flags);
	__set_state(hp, state);
	spin_unlock_irqrestore(&hp->lock, flags);
}

static inline int len_packet(const uint8_t *packet)
{
	return (int)((struct hvsi_header *)packet)->len;
}

static inline int is_header(const uint8_t *packet)
{
	struct hvsi_header *header = (struct hvsi_header *)packet;
	return header->type >= VS_QUERY_RESPONSE_PACKET_HEADER;
}

static inline int got_packet(const struct hvsi_struct *hp, uint8_t *packet)
{
	if (hp->inbuf_end < packet + sizeof(struct hvsi_header))
		return 0; 

	if (hp->inbuf_end < (packet + len_packet(packet)))
		return 0; 

	return 1;
}

static void compact_inbuf(struct hvsi_struct *hp, uint8_t *read_to)
{
	int remaining = (int)(hp->inbuf_end - read_to);

	pr_debug("%s: %i chars remain\n", __func__, remaining);

	if (read_to != hp->inbuf)
		memmove(hp->inbuf, read_to, remaining);

	hp->inbuf_end = hp->inbuf + remaining;
}

#ifdef DEBUG
#define dbg_dump_packet(packet) dump_packet(packet)
#define dbg_dump_hex(data, len) dump_hex(data, len)
#else
#define dbg_dump_packet(packet) do { } while (0)
#define dbg_dump_hex(data, len) do { } while (0)
#endif

static void dump_hex(const uint8_t *data, int len)
{
	int i;

	printk("    ");
	for (i=0; i < len; i++)
		printk("%.2x", data[i]);

	printk("\n    ");
	for (i=0; i < len; i++) {
		if (isprint(data[i]))
			printk("%c", data[i]);
		else
			printk(".");
	}
	printk("\n");
}

static void dump_packet(uint8_t *packet)
{
	struct hvsi_header *header = (struct hvsi_header *)packet;

	printk("type 0x%x, len %i, seqno %i:\n", header->type, header->len,
			header->seqno);

	dump_hex(packet, header->len);
}

static int hvsi_read(struct hvsi_struct *hp, char *buf, int count)
{
	unsigned long got;

	got = hvc_get_chars(hp->vtermno, buf, count);

	return got;
}

static void hvsi_recv_control(struct hvsi_struct *hp, uint8_t *packet,
	struct tty_struct **to_hangup, struct hvsi_struct **to_handshake)
{
	struct hvsi_control *header = (struct hvsi_control *)packet;

	switch (header->verb) {
		case VSV_MODEM_CTL_UPDATE:
			if ((header->word & HVSI_TSCD) == 0) {
				
				pr_debug("hvsi%i: CD dropped\n", hp->index);
				hp->mctrl &= TIOCM_CD;
				
				if (hp->tty && !(hp->tty->flags & CLOCAL))
					*to_hangup = hp->tty;
			}
			break;
		case VSV_CLOSE_PROTOCOL:
			pr_debug("hvsi%i: service processor came back\n", hp->index);
			if (hp->state != HVSI_CLOSED) {
				*to_handshake = hp;
			}
			break;
		default:
			printk(KERN_WARNING "hvsi%i: unknown HVSI control packet: ",
				hp->index);
			dump_packet(packet);
			break;
	}
}

static void hvsi_recv_response(struct hvsi_struct *hp, uint8_t *packet)
{
	struct hvsi_query_response *resp = (struct hvsi_query_response *)packet;

	switch (hp->state) {
		case HVSI_WAIT_FOR_VER_RESPONSE:
			__set_state(hp, HVSI_WAIT_FOR_VER_QUERY);
			break;
		case HVSI_WAIT_FOR_MCTRL_RESPONSE:
			hp->mctrl = 0;
			if (resp->u.mctrl_word & HVSI_TSDTR)
				hp->mctrl |= TIOCM_DTR;
			if (resp->u.mctrl_word & HVSI_TSCD)
				hp->mctrl |= TIOCM_CD;
			__set_state(hp, HVSI_OPEN);
			break;
		default:
			printk(KERN_ERR "hvsi%i: unexpected query response: ", hp->index);
			dump_packet(packet);
			break;
	}
}

static int hvsi_version_respond(struct hvsi_struct *hp, uint16_t query_seqno)
{
	struct hvsi_query_response packet __ALIGNED__;
	int wrote;

	packet.hdr.type = VS_QUERY_RESPONSE_PACKET_HEADER;
	packet.hdr.len = sizeof(struct hvsi_query_response);
	packet.hdr.seqno = atomic_inc_return(&hp->seqno);
	packet.verb = VSV_SEND_VERSION_NUMBER;
	packet.u.version = HVSI_VERSION;
	packet.query_seqno = query_seqno+1;

	pr_debug("%s: sending %i bytes\n", __func__, packet.hdr.len);
	dbg_dump_hex((uint8_t*)&packet, packet.hdr.len);

	wrote = hvc_put_chars(hp->vtermno, (char *)&packet, packet.hdr.len);
	if (wrote != packet.hdr.len) {
		printk(KERN_ERR "hvsi%i: couldn't send query response!\n",
			hp->index);
		return -EIO;
	}

	return 0;
}

static void hvsi_recv_query(struct hvsi_struct *hp, uint8_t *packet)
{
	struct hvsi_query *query = (struct hvsi_query *)packet;

	switch (hp->state) {
		case HVSI_WAIT_FOR_VER_QUERY:
			hvsi_version_respond(hp, query->hdr.seqno);
			__set_state(hp, HVSI_OPEN);
			break;
		default:
			printk(KERN_ERR "hvsi%i: unexpected query: ", hp->index);
			dump_packet(packet);
			break;
	}
}

static void hvsi_insert_chars(struct hvsi_struct *hp, const char *buf, int len)
{
	int i;

	for (i=0; i < len; i++) {
		char c = buf[i];
#ifdef CONFIG_MAGIC_SYSRQ
		if (c == '\0') {
			hp->sysrq = 1;
			continue;
		} else if (hp->sysrq) {
			handle_sysrq(c);
			hp->sysrq = 0;
			continue;
		}
#endif 
		tty_insert_flip_char(hp->tty, c, 0);
	}
}

#define TTY_THRESHOLD_THROTTLE 128
static struct tty_struct *hvsi_recv_data(struct hvsi_struct *hp,
		const uint8_t *packet)
{
	const struct hvsi_header *header = (const struct hvsi_header *)packet;
	const uint8_t *data = packet + sizeof(struct hvsi_header);
	int datalen = header->len - sizeof(struct hvsi_header);
	int overflow = datalen - TTY_THRESHOLD_THROTTLE;

	pr_debug("queueing %i chars '%.*s'\n", datalen, datalen, data);

	if (datalen == 0)
		return NULL;

	if (overflow > 0) {
		pr_debug("%s: got >TTY_THRESHOLD_THROTTLE bytes\n", __func__);
		datalen = TTY_THRESHOLD_THROTTLE;
	}

	hvsi_insert_chars(hp, data, datalen);

	if (overflow > 0) {
		pr_debug("%s: deferring overflow\n", __func__);
		memcpy(hp->throttle_buf, data + TTY_THRESHOLD_THROTTLE, overflow);
		hp->n_throttle = overflow;
	}

	return hp->tty;
}

static int hvsi_load_chunk(struct hvsi_struct *hp, struct tty_struct **flip,
		struct tty_struct **hangup, struct hvsi_struct **handshake)
{
	uint8_t *packet = hp->inbuf;
	int chunklen;

	*flip = NULL;
	*hangup = NULL;
	*handshake = NULL;

	chunklen = hvsi_read(hp, hp->inbuf_end, HVSI_MAX_READ);
	if (chunklen == 0) {
		pr_debug("%s: 0-length read\n", __func__);
		return 0;
	}

	pr_debug("%s: got %i bytes\n", __func__, chunklen);
	dbg_dump_hex(hp->inbuf_end, chunklen);

	hp->inbuf_end += chunklen;

	
	while ((packet < hp->inbuf_end) && got_packet(hp, packet)) {
		struct hvsi_header *header = (struct hvsi_header *)packet;

		if (!is_header(packet)) {
			printk(KERN_ERR "hvsi%i: got malformed packet\n", hp->index);
			
			while ((packet < hp->inbuf_end) && (!is_header(packet)))
				packet++;
			continue;
		}

		pr_debug("%s: handling %i-byte packet\n", __func__,
				len_packet(packet));
		dbg_dump_packet(packet);

		switch (header->type) {
			case VS_DATA_PACKET_HEADER:
				if (!is_open(hp))
					break;
				if (hp->tty == NULL)
					break; 
				*flip = hvsi_recv_data(hp, packet);
				break;
			case VS_CONTROL_PACKET_HEADER:
				hvsi_recv_control(hp, packet, hangup, handshake);
				break;
			case VS_QUERY_RESPONSE_PACKET_HEADER:
				hvsi_recv_response(hp, packet);
				break;
			case VS_QUERY_PACKET_HEADER:
				hvsi_recv_query(hp, packet);
				break;
			default:
				printk(KERN_ERR "hvsi%i: unknown HVSI packet type 0x%x\n",
						hp->index, header->type);
				dump_packet(packet);
				break;
		}

		packet += len_packet(packet);

		if (*hangup || *handshake) {
			pr_debug("%s: hangup or handshake\n", __func__);
			break;
		}
	}

	compact_inbuf(hp, packet);

	return 1;
}

static void hvsi_send_overflow(struct hvsi_struct *hp)
{
	pr_debug("%s: delivering %i bytes overflow\n", __func__,
			hp->n_throttle);

	hvsi_insert_chars(hp, hp->throttle_buf, hp->n_throttle);
	hp->n_throttle = 0;
}

static irqreturn_t hvsi_interrupt(int irq, void *arg)
{
	struct hvsi_struct *hp = (struct hvsi_struct *)arg;
	struct tty_struct *flip;
	struct tty_struct *hangup;
	struct hvsi_struct *handshake;
	unsigned long flags;
	int again = 1;

	pr_debug("%s\n", __func__);

	while (again) {
		spin_lock_irqsave(&hp->lock, flags);
		again = hvsi_load_chunk(hp, &flip, &hangup, &handshake);
		spin_unlock_irqrestore(&hp->lock, flags);


		if (flip) {
			
			tty_flip_buffer_push(flip);
			flip = NULL;
		}

		if (hangup) {
			tty_hangup(hangup);
		}

		if (handshake) {
			pr_debug("hvsi%i: attempting re-handshake\n", handshake->index);
			schedule_work(&handshake->handshaker);
		}
	}

	spin_lock_irqsave(&hp->lock, flags);
	if (hp->tty && hp->n_throttle
			&& (!test_bit(TTY_THROTTLED, &hp->tty->flags))) {
		flip = hp->tty;
		hvsi_send_overflow(hp);
	}
	spin_unlock_irqrestore(&hp->lock, flags);

	if (flip) {
		tty_flip_buffer_push(flip);
	}

	return IRQ_HANDLED;
}

static int __init poll_for_state(struct hvsi_struct *hp, int state)
{
	unsigned long end_jiffies = jiffies + HVSI_TIMEOUT;

	for (;;) {
		hvsi_interrupt(hp->virq, (void *)hp); 

		if (hp->state == state)
			return 0;

		mdelay(5);
		if (time_after(jiffies, end_jiffies))
			return -EIO;
	}
}

static int wait_for_state(struct hvsi_struct *hp, int state)
{
	int ret = 0;

	if (!wait_event_timeout(hp->stateq, (hp->state == state), HVSI_TIMEOUT))
		ret = -EIO;

	return ret;
}

static int hvsi_query(struct hvsi_struct *hp, uint16_t verb)
{
	struct hvsi_query packet __ALIGNED__;
	int wrote;

	packet.hdr.type = VS_QUERY_PACKET_HEADER;
	packet.hdr.len = sizeof(struct hvsi_query);
	packet.hdr.seqno = atomic_inc_return(&hp->seqno);
	packet.verb = verb;

	pr_debug("%s: sending %i bytes\n", __func__, packet.hdr.len);
	dbg_dump_hex((uint8_t*)&packet, packet.hdr.len);

	wrote = hvc_put_chars(hp->vtermno, (char *)&packet, packet.hdr.len);
	if (wrote != packet.hdr.len) {
		printk(KERN_ERR "hvsi%i: couldn't send query (%i)!\n", hp->index,
			wrote);
		return -EIO;
	}

	return 0;
}

static int hvsi_get_mctrl(struct hvsi_struct *hp)
{
	int ret;

	set_state(hp, HVSI_WAIT_FOR_MCTRL_RESPONSE);
	hvsi_query(hp, VSV_SEND_MODEM_CTL_STATUS);

	ret = hvsi_wait(hp, HVSI_OPEN);
	if (ret < 0) {
		printk(KERN_ERR "hvsi%i: didn't get modem flags\n", hp->index);
		set_state(hp, HVSI_OPEN);
		return ret;
	}

	pr_debug("%s: mctrl 0x%x\n", __func__, hp->mctrl);

	return 0;
}

static int hvsi_set_mctrl(struct hvsi_struct *hp, uint16_t mctrl)
{
	struct hvsi_control packet __ALIGNED__;
	int wrote;

	packet.hdr.type = VS_CONTROL_PACKET_HEADER,
	packet.hdr.seqno = atomic_inc_return(&hp->seqno);
	packet.hdr.len = sizeof(struct hvsi_control);
	packet.verb = VSV_SET_MODEM_CTL;
	packet.mask = HVSI_TSDTR;

	if (mctrl & TIOCM_DTR)
		packet.word = HVSI_TSDTR;

	pr_debug("%s: sending %i bytes\n", __func__, packet.hdr.len);
	dbg_dump_hex((uint8_t*)&packet, packet.hdr.len);

	wrote = hvc_put_chars(hp->vtermno, (char *)&packet, packet.hdr.len);
	if (wrote != packet.hdr.len) {
		printk(KERN_ERR "hvsi%i: couldn't set DTR!\n", hp->index);
		return -EIO;
	}

	return 0;
}

static void hvsi_drain_input(struct hvsi_struct *hp)
{
	uint8_t buf[HVSI_MAX_READ] __ALIGNED__;
	unsigned long end_jiffies = jiffies + HVSI_TIMEOUT;

	while (time_before(end_jiffies, jiffies))
		if (0 == hvsi_read(hp, buf, HVSI_MAX_READ))
			break;
}

static int hvsi_handshake(struct hvsi_struct *hp)
{
	int ret;

	hvsi_drain_input(hp);

	set_state(hp, HVSI_WAIT_FOR_VER_RESPONSE);
	ret = hvsi_query(hp, VSV_SEND_VERSION_NUMBER);
	if (ret < 0) {
		printk(KERN_ERR "hvsi%i: couldn't send version query\n", hp->index);
		return ret;
	}

	ret = hvsi_wait(hp, HVSI_OPEN);
	if (ret < 0)
		return ret;

	return 0;
}

static void hvsi_handshaker(struct work_struct *work)
{
	struct hvsi_struct *hp =
		container_of(work, struct hvsi_struct, handshaker);

	if (hvsi_handshake(hp) >= 0)
		return;

	printk(KERN_ERR "hvsi%i: re-handshaking failed\n", hp->index);
	if (is_console(hp)) {
		printk(KERN_ERR "hvsi%i: lost console!\n", hp->index);
	}
}

static int hvsi_put_chars(struct hvsi_struct *hp, const char *buf, int count)
{
	struct hvsi_data packet __ALIGNED__;
	int ret;

	BUG_ON(count > HVSI_MAX_OUTGOING_DATA);

	packet.hdr.type = VS_DATA_PACKET_HEADER;
	packet.hdr.seqno = atomic_inc_return(&hp->seqno);
	packet.hdr.len = count + sizeof(struct hvsi_header);
	memcpy(&packet.data, buf, count);

	ret = hvc_put_chars(hp->vtermno, (char *)&packet, packet.hdr.len);
	if (ret == packet.hdr.len) {
		/* return the number of chars written, not the packet length */
		return count;
	}
	return ret; 
}

static void hvsi_close_protocol(struct hvsi_struct *hp)
{
	struct hvsi_control packet __ALIGNED__;

	packet.hdr.type = VS_CONTROL_PACKET_HEADER;
	packet.hdr.seqno = atomic_inc_return(&hp->seqno);
	packet.hdr.len = 6;
	packet.verb = VSV_CLOSE_PROTOCOL;

	pr_debug("%s: sending %i bytes\n", __func__, packet.hdr.len);
	dbg_dump_hex((uint8_t*)&packet, packet.hdr.len);

	hvc_put_chars(hp->vtermno, (char *)&packet, packet.hdr.len);
}

static int hvsi_open(struct tty_struct *tty, struct file *filp)
{
	struct hvsi_struct *hp;
	unsigned long flags;
	int ret;

	pr_debug("%s\n", __func__);

	hp = &hvsi_ports[tty->index];

	tty->driver_data = hp;

	mb();
	if (hp->state == HVSI_FSP_DIED)
		return -EIO;

	spin_lock_irqsave(&hp->lock, flags);
	hp->tty = tty;
	hp->count++;
	atomic_set(&hp->seqno, 0);
	h_vio_signal(hp->vtermno, VIO_IRQ_ENABLE);
	spin_unlock_irqrestore(&hp->lock, flags);

	if (is_console(hp))
		return 0; 

	ret = hvsi_handshake(hp);
	if (ret < 0) {
		printk(KERN_ERR "%s: HVSI handshaking failed\n", tty->name);
		return ret;
	}

	ret = hvsi_get_mctrl(hp);
	if (ret < 0) {
		printk(KERN_ERR "%s: couldn't get initial modem flags\n", tty->name);
		return ret;
	}

	ret = hvsi_set_mctrl(hp, hp->mctrl | TIOCM_DTR);
	if (ret < 0) {
		printk(KERN_ERR "%s: couldn't set DTR\n", tty->name);
		return ret;
	}

	return 0;
}

static void hvsi_flush_output(struct hvsi_struct *hp)
{
	wait_event_timeout(hp->emptyq, (hp->n_outbuf <= 0), HVSI_TIMEOUT);

	
	cancel_delayed_work_sync(&hp->writer);
	flush_work_sync(&hp->handshaker);

	hp->n_outbuf = 0;
}

static void hvsi_close(struct tty_struct *tty, struct file *filp)
{
	struct hvsi_struct *hp = tty->driver_data;
	unsigned long flags;

	pr_debug("%s\n", __func__);

	if (tty_hung_up_p(filp))
		return;

	spin_lock_irqsave(&hp->lock, flags);

	if (--hp->count == 0) {
		hp->tty = NULL;
		hp->inbuf_end = hp->inbuf; 

		
		if (!is_console(hp)) {
			h_vio_signal(hp->vtermno, VIO_IRQ_DISABLE); 
			__set_state(hp, HVSI_CLOSED);
			tty->closing = 1;

			spin_unlock_irqrestore(&hp->lock, flags);

			
			synchronize_irq(hp->virq);

			
			hvsi_flush_output(hp);

			
			hvsi_close_protocol(hp);

			hvsi_drain_input(hp);

			spin_lock_irqsave(&hp->lock, flags);
		}
	} else if (hp->count < 0)
		printk(KERN_ERR "hvsi_close %lu: oops, count is %d\n",
		       hp - hvsi_ports, hp->count);

	spin_unlock_irqrestore(&hp->lock, flags);
}

static void hvsi_hangup(struct tty_struct *tty)
{
	struct hvsi_struct *hp = tty->driver_data;
	unsigned long flags;

	pr_debug("%s\n", __func__);

	spin_lock_irqsave(&hp->lock, flags);

	hp->count = 0;
	hp->n_outbuf = 0;
	hp->tty = NULL;

	spin_unlock_irqrestore(&hp->lock, flags);
}

static void hvsi_push(struct hvsi_struct *hp)
{
	int n;

	if (hp->n_outbuf <= 0)
		return;

	n = hvsi_put_chars(hp, hp->outbuf, hp->n_outbuf);
	if (n > 0) {
		
		pr_debug("%s: wrote %i chars\n", __func__, n);
		hp->n_outbuf = 0;
	} else if (n == -EIO) {
		__set_state(hp, HVSI_FSP_DIED);
		printk(KERN_ERR "hvsi%i: service processor died\n", hp->index);
	}
}

static void hvsi_write_worker(struct work_struct *work)
{
	struct hvsi_struct *hp =
		container_of(work, struct hvsi_struct, writer.work);
	unsigned long flags;
#ifdef DEBUG
	static long start_j = 0;

	if (start_j == 0)
		start_j = jiffies;
#endif 

	spin_lock_irqsave(&hp->lock, flags);

	pr_debug("%s: %i chars in buffer\n", __func__, hp->n_outbuf);

	if (!is_open(hp)) {
		schedule_delayed_work(&hp->writer, HZ);
		goto out;
	}

	hvsi_push(hp);
	if (hp->n_outbuf > 0)
		schedule_delayed_work(&hp->writer, 10);
	else {
#ifdef DEBUG
		pr_debug("%s: outbuf emptied after %li jiffies\n", __func__,
				jiffies - start_j);
		start_j = 0;
#endif 
		wake_up_all(&hp->emptyq);
		tty_wakeup(hp->tty);
	}

out:
	spin_unlock_irqrestore(&hp->lock, flags);
}

static int hvsi_write_room(struct tty_struct *tty)
{
	struct hvsi_struct *hp = tty->driver_data;

	return N_OUTBUF - hp->n_outbuf;
}

static int hvsi_chars_in_buffer(struct tty_struct *tty)
{
	struct hvsi_struct *hp = tty->driver_data;

	return hp->n_outbuf;
}

static int hvsi_write(struct tty_struct *tty,
		     const unsigned char *buf, int count)
{
	struct hvsi_struct *hp = tty->driver_data;
	const char *source = buf;
	unsigned long flags;
	int total = 0;
	int origcount = count;

	spin_lock_irqsave(&hp->lock, flags);

	pr_debug("%s: %i chars in buffer\n", __func__, hp->n_outbuf);

	if (!is_open(hp)) {
		
		pr_debug("%s: not open\n", __func__);
		goto out;
	}

	while ((count > 0) && (hvsi_write_room(hp->tty) > 0)) {
		int chunksize = min(count, hvsi_write_room(hp->tty));

		BUG_ON(hp->n_outbuf < 0);
		memcpy(hp->outbuf + hp->n_outbuf, source, chunksize);
		hp->n_outbuf += chunksize;

		total += chunksize;
		source += chunksize;
		count -= chunksize;
		hvsi_push(hp);
	}

	if (hp->n_outbuf > 0) {
		schedule_delayed_work(&hp->writer, 10);
	}

out:
	spin_unlock_irqrestore(&hp->lock, flags);

	if (total != origcount)
		pr_debug("%s: wanted %i, only wrote %i\n", __func__, origcount,
			total);

	return total;
}

static void hvsi_throttle(struct tty_struct *tty)
{
	struct hvsi_struct *hp = tty->driver_data;

	pr_debug("%s\n", __func__);

	h_vio_signal(hp->vtermno, VIO_IRQ_DISABLE);
}

static void hvsi_unthrottle(struct tty_struct *tty)
{
	struct hvsi_struct *hp = tty->driver_data;
	unsigned long flags;
	int shouldflip = 0;

	pr_debug("%s\n", __func__);

	spin_lock_irqsave(&hp->lock, flags);
	if (hp->n_throttle) {
		hvsi_send_overflow(hp);
		shouldflip = 1;
	}
	spin_unlock_irqrestore(&hp->lock, flags);

	if (shouldflip)
		tty_flip_buffer_push(hp->tty);

	h_vio_signal(hp->vtermno, VIO_IRQ_ENABLE);
}

static int hvsi_tiocmget(struct tty_struct *tty)
{
	struct hvsi_struct *hp = tty->driver_data;

	hvsi_get_mctrl(hp);
	return hp->mctrl;
}

static int hvsi_tiocmset(struct tty_struct *tty,
				unsigned int set, unsigned int clear)
{
	struct hvsi_struct *hp = tty->driver_data;
	unsigned long flags;
	uint16_t new_mctrl;

	
	clear &= TIOCM_DTR;
	set &= TIOCM_DTR;

	spin_lock_irqsave(&hp->lock, flags);

	new_mctrl = (hp->mctrl & ~clear) | set;

	if (hp->mctrl != new_mctrl) {
		hvsi_set_mctrl(hp, new_mctrl);
		hp->mctrl = new_mctrl;
	}
	spin_unlock_irqrestore(&hp->lock, flags);

	return 0;
}


static const struct tty_operations hvsi_ops = {
	.open = hvsi_open,
	.close = hvsi_close,
	.write = hvsi_write,
	.hangup = hvsi_hangup,
	.write_room = hvsi_write_room,
	.chars_in_buffer = hvsi_chars_in_buffer,
	.throttle = hvsi_throttle,
	.unthrottle = hvsi_unthrottle,
	.tiocmget = hvsi_tiocmget,
	.tiocmset = hvsi_tiocmset,
};

static int __init hvsi_init(void)
{
	int i;

	hvsi_driver = alloc_tty_driver(hvsi_count);
	if (!hvsi_driver)
		return -ENOMEM;

	hvsi_driver->driver_name = "hvsi";
	hvsi_driver->name = "hvsi";
	hvsi_driver->major = HVSI_MAJOR;
	hvsi_driver->minor_start = HVSI_MINOR;
	hvsi_driver->type = TTY_DRIVER_TYPE_SYSTEM;
	hvsi_driver->init_termios = tty_std_termios;
	hvsi_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL;
	hvsi_driver->init_termios.c_ispeed = 9600;
	hvsi_driver->init_termios.c_ospeed = 9600;
	hvsi_driver->flags = TTY_DRIVER_REAL_RAW;
	tty_set_operations(hvsi_driver, &hvsi_ops);

	for (i=0; i < hvsi_count; i++) {
		struct hvsi_struct *hp = &hvsi_ports[i];
		int ret = 1;

		ret = request_irq(hp->virq, hvsi_interrupt, 0, "hvsi", hp);
		if (ret)
			printk(KERN_ERR "HVSI: couldn't reserve irq 0x%x (error %i)\n",
				hp->virq, ret);
	}
	hvsi_wait = wait_for_state; 

	if (tty_register_driver(hvsi_driver))
		panic("Couldn't register hvsi console driver\n");

	printk(KERN_DEBUG "HVSI: registered %i devices\n", hvsi_count);

	return 0;
}
device_initcall(hvsi_init);


static void hvsi_console_print(struct console *console, const char *buf,
		unsigned int count)
{
	struct hvsi_struct *hp = &hvsi_ports[console->index];
	char c[HVSI_MAX_OUTGOING_DATA] __ALIGNED__;
	unsigned int i = 0, n = 0;
	int ret, donecr = 0;

	mb();
	if (!is_open(hp))
		return;

	while (count > 0 || i > 0) {
		if (count > 0 && i < sizeof(c)) {
			if (buf[n] == '\n' && !donecr) {
				c[i++] = '\r';
				donecr = 1;
			} else {
				c[i++] = buf[n++];
				donecr = 0;
				--count;
			}
		} else {
			ret = hvsi_put_chars(hp, c, i);
			if (ret < 0)
				i = 0;
			i -= ret;
		}
	}
}

static struct tty_driver *hvsi_console_device(struct console *console,
	int *index)
{
	*index = console->index;
	return hvsi_driver;
}

static int __init hvsi_console_setup(struct console *console, char *options)
{
	struct hvsi_struct *hp;
	int ret;

	if (console->index < 0 || console->index >= hvsi_count)
		return -1;
	hp = &hvsi_ports[console->index];

	
	hvsi_close_protocol(hp);

	ret = hvsi_handshake(hp);
	if (ret < 0)
		return ret;

	ret = hvsi_get_mctrl(hp);
	if (ret < 0)
		return ret;

	ret = hvsi_set_mctrl(hp, hp->mctrl | TIOCM_DTR);
	if (ret < 0)
		return ret;

	hp->flags |= HVSI_CONSOLE;

	return 0;
}

static struct console hvsi_console = {
	.name		= "hvsi",
	.write		= hvsi_console_print,
	.device		= hvsi_console_device,
	.setup		= hvsi_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

static int __init hvsi_console_init(void)
{
	struct device_node *vty;

	hvsi_wait = poll_for_state; 

	
	for (vty = of_find_compatible_node(NULL, "serial", "hvterm-protocol");
			vty != NULL;
			vty = of_find_compatible_node(vty, "serial", "hvterm-protocol")) {
		struct hvsi_struct *hp;
		const uint32_t *vtermno, *irq;

		vtermno = of_get_property(vty, "reg", NULL);
		irq = of_get_property(vty, "interrupts", NULL);
		if (!vtermno || !irq)
			continue;

		if (hvsi_count >= MAX_NR_HVSI_CONSOLES) {
			of_node_put(vty);
			break;
		}

		hp = &hvsi_ports[hvsi_count];
		INIT_DELAYED_WORK(&hp->writer, hvsi_write_worker);
		INIT_WORK(&hp->handshaker, hvsi_handshaker);
		init_waitqueue_head(&hp->emptyq);
		init_waitqueue_head(&hp->stateq);
		spin_lock_init(&hp->lock);
		hp->index = hvsi_count;
		hp->inbuf_end = hp->inbuf;
		hp->state = HVSI_CLOSED;
		hp->vtermno = *vtermno;
		hp->virq = irq_create_mapping(NULL, irq[0]);
		if (hp->virq == 0) {
			printk(KERN_ERR "%s: couldn't create irq mapping for 0x%x\n",
				__func__, irq[0]);
			continue;
		}

		hvsi_count++;
	}

	if (hvsi_count)
		register_console(&hvsi_console);
	return 0;
}
console_initcall(hvsi_console_init);
