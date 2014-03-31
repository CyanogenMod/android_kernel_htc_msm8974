/*
 * Common data handling layer for ser_gigaset and usb_gigaset
 *
 * Copyright (c) 2005 by Tilman Schmidt <tilman@imap.cc>,
 *                       Hansjoerg Lipp <hjlipp@web.de>,
 *                       Stefan Eilers.
 *
 * =====================================================================
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 * =====================================================================
 */

#include "gigaset.h"
#include <linux/crc-ccitt.h>
#include <linux/bitrev.h>
#include <linux/export.h>

static inline int muststuff(unsigned char c)
{
	if (c < PPP_TRANS) return 1;
	if (c == PPP_FLAG) return 1;
	if (c == PPP_ESCAPE) return 1;
	
	
	
	return 0;
}


static unsigned cmd_loop(unsigned numbytes, struct inbuf_t *inbuf)
{
	unsigned char *src = inbuf->data + inbuf->head;
	struct cardstate *cs = inbuf->cs;
	unsigned cbytes = cs->cbytes;
	unsigned procbytes = 0;
	unsigned char c;

	while (procbytes < numbytes) {
		c = *src++;
		procbytes++;

		switch (c) {
		case '\n':
			if (cbytes == 0 && cs->respdata[0] == '\r') {
				
				cs->respdata[0] = 0;
				break;
			}
			
		case '\r':
			
			if (cbytes >= MAX_RESP_SIZE) {
				dev_warn(cs->dev, "response too large (%d)\n",
					 cbytes);
				cbytes = MAX_RESP_SIZE;
			}
			cs->cbytes = cbytes;
			gigaset_dbg_buffer(DEBUG_TRANSCMD, "received response",
					   cbytes, cs->respdata);
			gigaset_handle_modem_response(cs);
			cbytes = 0;

			
			cs->respdata[0] = c;

			
			if (cs->dle && !(inbuf->inputstate & INS_DLE_command))
				inbuf->inputstate &= ~INS_command;

			
			goto exit;

		case DLE_FLAG:
			if (inbuf->inputstate & INS_DLE_char) {
				
				inbuf->inputstate &= ~INS_DLE_char;
			} else if (cs->dle ||
				   (inbuf->inputstate & INS_DLE_command)) {
				
				inbuf->inputstate |= INS_DLE_char;
				goto exit;
			}
			
			
		default:
			
			if (cbytes < MAX_RESP_SIZE)
				cs->respdata[cbytes] = c;
			cbytes++;
		}
	}
exit:
	cs->cbytes = cbytes;
	return procbytes;
}

static unsigned lock_loop(unsigned numbytes, struct inbuf_t *inbuf)
{
	unsigned char *src = inbuf->data + inbuf->head;

	gigaset_dbg_buffer(DEBUG_LOCKCMD, "received response", numbytes, src);
	gigaset_if_receive(inbuf->cs, src, numbytes);
	return numbytes;
}

static unsigned hdlc_loop(unsigned numbytes, struct inbuf_t *inbuf)
{
	struct cardstate *cs = inbuf->cs;
	struct bc_state *bcs = cs->bcs;
	int inputstate = bcs->inputstate;
	__u16 fcs = bcs->rx_fcs;
	struct sk_buff *skb = bcs->rx_skb;
	unsigned char *src = inbuf->data + inbuf->head;
	unsigned procbytes = 0;
	unsigned char c;

	if (inputstate & INS_byte_stuff) {
		if (!numbytes)
			return 0;
		inputstate &= ~INS_byte_stuff;
		goto byte_stuff;
	}

	while (procbytes < numbytes) {
		c = *src++;
		procbytes++;
		if (c == DLE_FLAG) {
			if (inputstate & INS_DLE_char) {
				
				inputstate &= ~INS_DLE_char;
			} else if (cs->dle || (inputstate & INS_DLE_command)) {
				
				inputstate |= INS_DLE_char;
				break;
			}
		}

		if (c == PPP_ESCAPE) {
			
			if (procbytes >= numbytes) {
				
				inputstate |= INS_byte_stuff;
				break;
			}
byte_stuff:
			c = *src++;
			procbytes++;
			if (c == DLE_FLAG) {
				if (inputstate & INS_DLE_char) {
					
					inputstate &= ~INS_DLE_char;
				} else if (cs->dle ||
					   (inputstate & INS_DLE_command)) {
					
					inputstate |=
						INS_DLE_char | INS_byte_stuff;
					break;
				}
			}
			c ^= PPP_TRANS;
#ifdef CONFIG_GIGASET_DEBUG
			if (!muststuff(c))
				gig_dbg(DEBUG_HDLC, "byte stuffed: 0x%02x", c);
#endif
		} else if (c == PPP_FLAG) {
			
			if (inputstate & INS_have_data) {
				gig_dbg(DEBUG_HDLC,
					"7e----------------------------");

				
				if (!skb) {
					
					gigaset_isdn_rcv_err(bcs);
				} else if (skb->len < 2) {
					
					dev_warn(cs->dev,
						 "short frame (%d)\n",
						 skb->len);
					gigaset_isdn_rcv_err(bcs);
					dev_kfree_skb_any(skb);
				} else if (fcs != PPP_GOODFCS) {
					
					dev_err(cs->dev,
						"Checksum failed, %u bytes corrupted!\n",
						skb->len);
					gigaset_isdn_rcv_err(bcs);
					dev_kfree_skb_any(skb);
				} else {
					
					__skb_trim(skb, skb->len - 2);
					gigaset_skb_rcvd(bcs, skb);
				}

				
				inputstate &= ~INS_have_data;
				skb = gigaset_new_rx_skb(bcs);
			} else {
				
#ifdef CONFIG_GIGASET_DEBUG
				++bcs->emptycount;
#endif
				if (!skb) {
					
					gigaset_isdn_rcv_err(bcs);
					skb = gigaset_new_rx_skb(bcs);
				}
			}

			fcs = PPP_INITFCS;
			continue;
#ifdef CONFIG_GIGASET_DEBUG
		} else if (muststuff(c)) {
			
			gig_dbg(DEBUG_HDLC, "not byte stuffed: 0x%02x", c);
#endif
		}

		
#ifdef CONFIG_GIGASET_DEBUG
		if (!(inputstate & INS_have_data)) {
			gig_dbg(DEBUG_HDLC, "7e (%d x) ================",
				bcs->emptycount);
			bcs->emptycount = 0;
		}
#endif
		inputstate |= INS_have_data;
		if (skb) {
			if (skb->len >= bcs->rx_bufsize) {
				dev_warn(cs->dev, "received packet too long\n");
				dev_kfree_skb_any(skb);
				
				bcs->rx_skb = skb = NULL;
			} else {
				*__skb_put(skb, 1) = c;
				fcs = crc_ccitt_byte(fcs, c);
			}
		}
	}

	bcs->inputstate = inputstate;
	bcs->rx_fcs = fcs;
	return procbytes;
}

static unsigned iraw_loop(unsigned numbytes, struct inbuf_t *inbuf)
{
	struct cardstate *cs = inbuf->cs;
	struct bc_state *bcs = cs->bcs;
	int inputstate = bcs->inputstate;
	struct sk_buff *skb = bcs->rx_skb;
	unsigned char *src = inbuf->data + inbuf->head;
	unsigned procbytes = 0;
	unsigned char c;

	if (!skb) {
		
		gigaset_new_rx_skb(bcs);
		return numbytes;
	}

	while (procbytes < numbytes && skb->len < bcs->rx_bufsize) {
		c = *src++;
		procbytes++;

		if (c == DLE_FLAG) {
			if (inputstate & INS_DLE_char) {
				
				inputstate &= ~INS_DLE_char;
			} else if (cs->dle || (inputstate & INS_DLE_command)) {
				
				inputstate |= INS_DLE_char;
				break;
			}
		}

		
		inputstate |= INS_have_data;
		*__skb_put(skb, 1) = bitrev8(c);
	}

	
	if (inputstate & INS_have_data) {
		gigaset_skb_rcvd(bcs, skb);
		inputstate &= ~INS_have_data;
		gigaset_new_rx_skb(bcs);
	}

	bcs->inputstate = inputstate;
	return procbytes;
}

static void handle_dle(struct inbuf_t *inbuf)
{
	struct cardstate *cs = inbuf->cs;

	if (cs->mstate == MS_LOCKED)
		return;		

	if (!(inbuf->inputstate & INS_DLE_char)) {
		
		if (inbuf->data[inbuf->head] == DLE_FLAG &&
		    (cs->dle || inbuf->inputstate & INS_DLE_command)) {
			
			inbuf->head++;
			if (inbuf->head == inbuf->tail ||
			    inbuf->head == RBUFSIZE) {
				
				inbuf->inputstate |= INS_DLE_char;
				return;
			}
		} else {
			
			return;
		}
	}

	
	inbuf->inputstate &= ~INS_DLE_char;

	switch (inbuf->data[inbuf->head]) {
	case 'X':	
		if (inbuf->inputstate & INS_command)
			dev_notice(cs->dev,
				   "received <DLE>X in command mode\n");
		inbuf->inputstate |= INS_command | INS_DLE_command;
		inbuf->head++;	
		break;
	case '.':	
		if (!(inbuf->inputstate & INS_DLE_command))
			dev_notice(cs->dev,
				   "received <DLE>. without <DLE>X\n");
		inbuf->inputstate &= ~INS_DLE_command;
		
		if (cs->dle)
			inbuf->inputstate &= ~INS_command;
		inbuf->head++;	
		break;
	case DLE_FLAG:	
		
		inbuf->inputstate |= INS_DLE_char;
		if (!(cs->dle || inbuf->inputstate & INS_DLE_command))
			dev_notice(cs->dev,
				   "received <DLE><DLE> not in DLE mode\n");
		break;	
	default:
		dev_notice(cs->dev, "received <DLE><%02x>\n",
			   inbuf->data[inbuf->head]);
		
	}
}

void gigaset_m10x_input(struct inbuf_t *inbuf)
{
	struct cardstate *cs = inbuf->cs;
	unsigned numbytes, procbytes;

	gig_dbg(DEBUG_INTR, "buffer state: %u -> %u", inbuf->head, inbuf->tail);

	while (inbuf->head != inbuf->tail) {
		
		handle_dle(inbuf);

		
		numbytes = (inbuf->head > inbuf->tail ?
			    RBUFSIZE : inbuf->tail) - inbuf->head;
		gig_dbg(DEBUG_INTR, "processing %u bytes", numbytes);

		if (cs->mstate == MS_LOCKED)
			procbytes = lock_loop(numbytes, inbuf);
		else if (inbuf->inputstate & INS_command)
			procbytes = cmd_loop(numbytes, inbuf);
		else if (cs->bcs->proto2 == L2_HDLC)
			procbytes = hdlc_loop(numbytes, inbuf);
		else
			procbytes = iraw_loop(numbytes, inbuf);
		inbuf->head += procbytes;

		
		if (inbuf->head >= RBUFSIZE)
			inbuf->head = 0;

		gig_dbg(DEBUG_INTR, "head set to %u", inbuf->head);
	}
}
EXPORT_SYMBOL_GPL(gigaset_m10x_input);



static struct sk_buff *HDLC_Encode(struct sk_buff *skb)
{
	struct sk_buff *hdlc_skb;
	__u16 fcs;
	unsigned char c;
	unsigned char *cp;
	int len;
	unsigned int stuf_cnt;

	stuf_cnt = 0;
	fcs = PPP_INITFCS;
	cp = skb->data;
	len = skb->len;
	while (len--) {
		if (muststuff(*cp))
			stuf_cnt++;
		fcs = crc_ccitt_byte(fcs, *cp++);
	}
	fcs ^= 0xffff;			

	hdlc_skb = dev_alloc_skb(skb->len + stuf_cnt + 6 + skb->mac_len);
	if (!hdlc_skb) {
		dev_kfree_skb_any(skb);
		return NULL;
	}

	
	skb_reset_mac_header(hdlc_skb);
	skb_reserve(hdlc_skb, skb->mac_len);
	memcpy(skb_mac_header(hdlc_skb), skb_mac_header(skb), skb->mac_len);
	hdlc_skb->mac_len = skb->mac_len;

	
	*(skb_put(hdlc_skb, 1)) = PPP_FLAG;

	
	while (skb->len--) {
		if (muststuff(*skb->data)) {
			*(skb_put(hdlc_skb, 1)) = PPP_ESCAPE;
			*(skb_put(hdlc_skb, 1)) = (*skb->data++) ^ PPP_TRANS;
		} else
			*(skb_put(hdlc_skb, 1)) = *skb->data++;
	}

	
	c = (fcs & 0x00ff);	
	if (muststuff(c)) {
		*(skb_put(hdlc_skb, 1)) = PPP_ESCAPE;
		c ^= PPP_TRANS;
	}
	*(skb_put(hdlc_skb, 1)) = c;

	c = ((fcs >> 8) & 0x00ff);
	if (muststuff(c)) {
		*(skb_put(hdlc_skb, 1)) = PPP_ESCAPE;
		c ^= PPP_TRANS;
	}
	*(skb_put(hdlc_skb, 1)) = c;

	*(skb_put(hdlc_skb, 1)) = PPP_FLAG;

	dev_kfree_skb_any(skb);
	return hdlc_skb;
}

static struct sk_buff *iraw_encode(struct sk_buff *skb)
{
	struct sk_buff *iraw_skb;
	unsigned char c;
	unsigned char *cp;
	int len;

	iraw_skb = dev_alloc_skb(2 * skb->len + skb->mac_len);
	if (!iraw_skb) {
		dev_kfree_skb_any(skb);
		return NULL;
	}

	
	skb_reset_mac_header(iraw_skb);
	skb_reserve(iraw_skb, skb->mac_len);
	memcpy(skb_mac_header(iraw_skb), skb_mac_header(skb), skb->mac_len);
	iraw_skb->mac_len = skb->mac_len;

	
	cp = skb->data;
	len = skb->len;
	while (len--) {
		c = bitrev8(*cp++);
		if (c == DLE_FLAG)
			*(skb_put(iraw_skb, 1)) = c;
		*(skb_put(iraw_skb, 1)) = c;
	}
	dev_kfree_skb_any(skb);
	return iraw_skb;
}

int gigaset_m10x_send_skb(struct bc_state *bcs, struct sk_buff *skb)
{
	struct cardstate *cs = bcs->cs;
	unsigned len = skb->len;
	unsigned long flags;

	if (bcs->proto2 == L2_HDLC)
		skb = HDLC_Encode(skb);
	else
		skb = iraw_encode(skb);
	if (!skb) {
		dev_err(cs->dev,
			"unable to allocate memory for encoding!\n");
		return -ENOMEM;
	}

	skb_queue_tail(&bcs->squeue, skb);
	spin_lock_irqsave(&cs->lock, flags);
	if (cs->connected)
		tasklet_schedule(&cs->write_tasklet);
	spin_unlock_irqrestore(&cs->lock, flags);

	return len;	
}
EXPORT_SYMBOL_GPL(gigaset_m10x_send_skb);
