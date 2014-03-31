/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) Jonathan Naylor G4KLX (g4klx@g4klx.demon.co.uk)
 */
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/gfp.h>
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <net/rose.h>

static void rose_send_iframe(struct sock *sk, struct sk_buff *skb)
{
	struct rose_sock *rose = rose_sk(sk);

	if (skb == NULL)
		return;

	skb->data[2] |= (rose->vr << 5) & 0xE0;
	skb->data[2] |= (rose->vs << 1) & 0x0E;

	rose_start_idletimer(sk);

	rose_transmit_link(skb, rose->neighbour);
}

void rose_kick(struct sock *sk)
{
	struct rose_sock *rose = rose_sk(sk);
	struct sk_buff *skb, *skbn;
	unsigned short start, end;

	if (rose->state != ROSE_STATE_3)
		return;

	if (rose->condition & ROSE_COND_PEER_RX_BUSY)
		return;

	if (!skb_peek(&sk->sk_write_queue))
		return;

	start = (skb_peek(&rose->ack_queue) == NULL) ? rose->va : rose->vs;
	end   = (rose->va + sysctl_rose_window_size) % ROSE_MODULUS;

	if (start == end)
		return;

	rose->vs = start;


	skb  = skb_dequeue(&sk->sk_write_queue);

	do {
		if ((skbn = skb_clone(skb, GFP_ATOMIC)) == NULL) {
			skb_queue_head(&sk->sk_write_queue, skb);
			break;
		}

		skb_set_owner_w(skbn, sk);

		rose_send_iframe(sk, skbn);

		rose->vs = (rose->vs + 1) % ROSE_MODULUS;

		skb_queue_tail(&rose->ack_queue, skb);

	} while (rose->vs != end &&
		 (skb = skb_dequeue(&sk->sk_write_queue)) != NULL);

	rose->vl         = rose->vr;
	rose->condition &= ~ROSE_COND_ACK_PENDING;

	rose_stop_timer(sk);
}


void rose_enquiry_response(struct sock *sk)
{
	struct rose_sock *rose = rose_sk(sk);

	if (rose->condition & ROSE_COND_OWN_RX_BUSY)
		rose_write_internal(sk, ROSE_RNR);
	else
		rose_write_internal(sk, ROSE_RR);

	rose->vl         = rose->vr;
	rose->condition &= ~ROSE_COND_ACK_PENDING;

	rose_stop_timer(sk);
}
