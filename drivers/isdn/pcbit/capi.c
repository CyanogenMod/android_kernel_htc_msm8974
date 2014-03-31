/*
 * CAPI encoder/decoder for
 * Portugal Telecom CAPI 2.0
 *
 * Copyright (C) 1996 Universidade de Lisboa
 *
 * Written by Pedro Roque Marques (roque@di.fc.ul.pt)
 *
 * This software may be used and distributed according to the terms of
 * the GNU General Public License, incorporated herein by reference.
 *
 * Not compatible with the AVM Gmbh. CAPI 2.0
 *
 */



#include <linux/string.h>
#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <linux/skbuff.h>

#include <asm/io.h>
#include <asm/string.h>

#include <linux/isdnif.h>

#include "pcbit.h"
#include "edss1.h"
#include "capi.h"



int capi_conn_req(const char *calledPN, struct sk_buff **skb, int proto)
{
	ushort len;


	len = 18 + strlen(calledPN);

	if (proto == ISDN_PROTO_L2_TRANS)
		len++;

	if ((*skb = dev_alloc_skb(len)) == NULL) {

		printk(KERN_WARNING "capi_conn_req: alloc_skb failed\n");
		return -1;
	}

	
	*((ushort *)skb_put(*skb, 2)) = AppInfoMask;

	if (proto == ISDN_PROTO_L2_TRANS)
	{
		
		*(skb_put(*skb, 1)) = 3;        
		*(skb_put(*skb, 1)) = 0x80;     
		*(skb_put(*skb, 1)) = 0x10;     
		*(skb_put(*skb, 1)) = 0x23;     
	}
	else
	{
		
		*(skb_put(*skb, 1)) = 2;        
		*(skb_put(*skb, 1)) = 0x88;     
		*(skb_put(*skb, 1)) = 0x90;     
	}

	
	*(skb_put(*skb, 1)) = 0;        

	*(skb_put(*skb, 1)) = 1;        
	*(skb_put(*skb, 1)) = 0x83;     

	*(skb_put(*skb, 1)) = 0;        


	*(skb_put(*skb, 1)) = 0;        
	*(skb_put(*skb, 1)) = 0;        

	
	*(skb_put(*skb, 1)) = strlen(calledPN) + 1;
	*(skb_put(*skb, 1)) = 0x81;
	memcpy(skb_put(*skb, strlen(calledPN)), calledPN, strlen(calledPN));

	

	*(skb_put(*skb, 1)) = 0;       

	
	
	
	
	memset(skb_put(*skb, 4), 0, 4);

	return len;
}

int capi_conn_resp(struct pcbit_chan *chan, struct sk_buff **skb)
{

	if ((*skb = dev_alloc_skb(5)) == NULL) {

		printk(KERN_WARNING "capi_conn_resp: alloc_skb failed\n");
		return -1;
	}

	*((ushort *)skb_put(*skb, 2)) = chan->callref;
	*(skb_put(*skb, 1)) = 0x01;  
	*(skb_put(*skb, 1)) = 0;
	*(skb_put(*skb, 1)) = 0;

	return 5;
}

int capi_conn_active_req(struct pcbit_chan *chan, struct sk_buff **skb)
{

	if ((*skb = dev_alloc_skb(8)) == NULL) {

		printk(KERN_WARNING "capi_conn_active_req: alloc_skb failed\n");
		return -1;
	}

	*((ushort *)skb_put(*skb, 2)) = chan->callref;

#ifdef DEBUG
	printk(KERN_DEBUG "Call Reference: %04x\n", chan->callref);
#endif

	*(skb_put(*skb, 1)) = 0;       
	*(skb_put(*skb, 1)) = 0;       
	*(skb_put(*skb, 1)) = 0;       
	*(skb_put(*skb, 1)) = 0;       
	*(skb_put(*skb, 1)) = 0;       
	*(skb_put(*skb, 1)) = 0;       

	return 8;
}

int capi_conn_active_resp(struct pcbit_chan *chan, struct sk_buff **skb)
{

	if ((*skb = dev_alloc_skb(2)) == NULL) {

		printk(KERN_WARNING "capi_conn_active_resp: alloc_skb failed\n");
		return -1;
	}

	*((ushort *)skb_put(*skb, 2)) = chan->callref;

	return 2;
}


int capi_select_proto_req(struct pcbit_chan *chan, struct sk_buff **skb,
			  int outgoing)
{


	if ((*skb = dev_alloc_skb(18)) == NULL) {

		printk(KERN_WARNING "capi_select_proto_req: alloc_skb failed\n");
		return -1;
	}

	*((ushort *)skb_put(*skb, 2)) = chan->callref;

	

	switch (chan->proto) {
	case ISDN_PROTO_L2_X75I:
		*(skb_put(*skb, 1)) = 0x05;            
		break;
	case ISDN_PROTO_L2_HDLC:
		*(skb_put(*skb, 1)) = 0x02;
		break;
	case ISDN_PROTO_L2_TRANS:
		*(skb_put(*skb, 1)) = 0x06;
		break;
	default:
#ifdef DEBUG
		printk(KERN_DEBUG "Transparent\n");
#endif
		*(skb_put(*skb, 1)) = 0x03;
		break;
	}

	*(skb_put(*skb, 1)) = (outgoing ? 0x02 : 0x42);    
	*(skb_put(*skb, 1)) = 0x00;

	*((ushort *) skb_put(*skb, 2)) = MRU;


	*(skb_put(*skb, 1)) = 0x08;           
	*(skb_put(*skb, 1)) = 0x07;           

	*(skb_put(*skb, 1)) = 0x01;           


	memset(skb_put(*skb, 8), 0, 8);

	return 18;
}


int capi_activate_transp_req(struct pcbit_chan *chan, struct sk_buff **skb)
{

	if ((*skb = dev_alloc_skb(7)) == NULL) {

		printk(KERN_WARNING "capi_activate_transp_req: alloc_skb failed\n");
		return -1;
	}

	*((ushort *)skb_put(*skb, 2)) = chan->callref;


	*(skb_put(*skb, 1)) = chan->layer2link; 
	*(skb_put(*skb, 1)) = 0x00;             

	*((ushort *) skb_put(*skb, 2)) = MRU;

	*(skb_put(*skb, 1)) = 0x01;             

	return 7;
}

int capi_tdata_req(struct pcbit_chan *chan, struct sk_buff *skb)
{
	ushort data_len;



	data_len = skb->len;

	if (skb_headroom(skb) < 10)
	{
		printk(KERN_CRIT "No headspace (%u) on headroom %p for capi header\n", skb_headroom(skb), skb);
	}
	else
	{
		skb_push(skb, 10);
	}

	*((u16 *) (skb->data)) = chan->callref;
	skb->data[2] = chan->layer2link;
	*((u16 *) (skb->data + 3)) = data_len;

	chan->s_refnum = (chan->s_refnum + 1) % 8;
	*((u32 *) (skb->data + 5)) = chan->s_refnum;

	skb->data[9] = 0;                           

	return 10;
}

int capi_tdata_resp(struct pcbit_chan *chan, struct sk_buff **skb)

{
	if ((*skb = dev_alloc_skb(4)) == NULL) {

		printk(KERN_WARNING "capi_tdata_resp: alloc_skb failed\n");
		return -1;
	}

	*((ushort *)skb_put(*skb, 2)) = chan->callref;

	*(skb_put(*skb, 1)) = chan->layer2link;
	*(skb_put(*skb, 1)) = chan->r_refnum;

	return (*skb)->len;
}

int capi_disc_req(ushort callref, struct sk_buff **skb, u_char cause)
{

	if ((*skb = dev_alloc_skb(6)) == NULL) {

		printk(KERN_WARNING "capi_disc_req: alloc_skb failed\n");
		return -1;
	}

	*((ushort *)skb_put(*skb, 2)) = callref;

	*(skb_put(*skb, 1)) = 2;                  
	*(skb_put(*skb, 1)) = 0x80;
	*(skb_put(*skb, 1)) = 0x80 | cause;


	*(skb_put(*skb, 1)) = 0;                   

	return 6;
}

int capi_disc_resp(struct pcbit_chan *chan, struct sk_buff **skb)
{
	if ((*skb = dev_alloc_skb(2)) == NULL) {

		printk(KERN_WARNING "capi_disc_resp: alloc_skb failed\n");
		return -1;
	}

	*((ushort *)skb_put(*skb, 2)) = chan->callref;

	return 2;
}



int capi_decode_conn_ind(struct pcbit_chan *chan,
			 struct sk_buff *skb,
			 struct callb_data *info)
{
	int CIlen, len;

	
	chan->callref = *((ushort *)skb->data);
	skb_pull(skb, 2);

#ifdef DEBUG
	printk(KERN_DEBUG "Call Reference: %04x\n", chan->callref);
#endif

	


	CIlen = skb->data[0];
#ifdef DEBUG
	if (CIlen == 1) {

		if (((skb->data[1]) & 0xFC) == 0x48)
			printk(KERN_DEBUG "decode_conn_ind: chan ok\n");
		printk(KERN_DEBUG "phyChan = %d\n", skb->data[1] & 0x03);
	}
	else
		printk(KERN_DEBUG "conn_ind: CIlen = %d\n", CIlen);
#endif
	skb_pull(skb, CIlen + 1);

	
	

	len = skb->data[0];

	if (len > 0) {
		int count = 1;

#ifdef DEBUG
		printk(KERN_DEBUG "CPN: Octect 3 %02x\n", skb->data[1]);
#endif
		if ((skb->data[1] & 0x80) == 0)
			count = 2;

		if (!(info->data.setup.CallingPN = kmalloc(len - count + 1, GFP_ATOMIC)))
			return -1;

		skb_copy_from_linear_data_offset(skb, count + 1,
						 info->data.setup.CallingPN,
						 len - count);
		info->data.setup.CallingPN[len - count] = 0;

	}
	else {
		info->data.setup.CallingPN = NULL;
		printk(KERN_DEBUG "NULL CallingPN\n");
	}

	skb_pull(skb, len + 1);

	
	skb_pull(skb, skb->data[0] + 1);

	

	len = skb->data[0];

	if (len > 0) {
		int count = 1;

		if ((skb->data[1] & 0x80) == 0)
			count = 2;

		if (!(info->data.setup.CalledPN = kmalloc(len - count + 1, GFP_ATOMIC)))
			return -1;

		skb_copy_from_linear_data_offset(skb, count + 1,
						 info->data.setup.CalledPN,
						 len - count);
		info->data.setup.CalledPN[len - count] = 0;

	}
	else {
		info->data.setup.CalledPN = NULL;
		printk(KERN_DEBUG "NULL CalledPN\n");
	}

	skb_pull(skb, len + 1);

	
	skb_pull(skb, skb->data[0] + 1);

	
	skb_pull(skb, skb->data[0] + 1);

	
	skb_pull(skb, skb->data[0] + 1);

	
	skb_pull(skb, skb->data[0] + 1);

	return 0;
}


int capi_decode_conn_conf(struct pcbit_chan *chan, struct sk_buff *skb,
			  int *complete)
{
	int errcode;

	chan->callref = *((ushort *)skb->data);     
	skb_pull(skb, 2);

	errcode = *((ushort *) skb->data);   
	skb_pull(skb, 2);

	*complete = *(skb->data);
	skb_pull(skb, 1);

	
	
	if (!*complete)
	{
		printk(KERN_DEBUG "complete=%02x\n", *complete);
		*complete = 1;
	}


	
	skb_pull(skb, *(skb->data) + 1);

	
	skb_pull(skb, *(skb->data) + 1);

	
	skb_pull(skb, *(skb->data) + 1);

	return errcode;
}

int capi_decode_conn_actv_ind(struct pcbit_chan *chan, struct sk_buff *skb)
{
	ushort len;
#ifdef DEBUG
	char str[32];
#endif

	
	skb_pull(skb, *(skb->data) + 1);


	
	len = *(skb->data);

#ifdef DEBUG
	if (len > 1 && len < 31) {
		skb_copy_from_linear_data_offset(skb, 2, str, len - 1);
		str[len] = 0;
		printk(KERN_DEBUG "Connected Party Number: %s\n", str);
	}
	else
		printk(KERN_DEBUG "actv_ind CPN len = %d\n", len);
#endif

	skb_pull(skb, len + 1);

	
	skb_pull(skb, *(skb->data) + 1);

	
	skb_pull(skb, *(skb->data) + 1);

	
	skb_pull(skb, *(skb->data) + 1);

	return 0;
}

int capi_decode_conn_actv_conf(struct pcbit_chan *chan, struct sk_buff *skb)
{
	ushort errcode;

	errcode = *((ushort *)skb->data);
	skb_pull(skb, 2);

	return errcode;
}


int capi_decode_sel_proto_conf(struct pcbit_chan *chan, struct sk_buff *skb)
{
	ushort errcode;

	chan->layer2link = *(skb->data);
	skb_pull(skb, 1);

	errcode = *((ushort *)skb->data);
	skb_pull(skb, 2);

	return errcode;
}

int capi_decode_actv_trans_conf(struct pcbit_chan *chan, struct sk_buff *skb)
{
	ushort errcode;

	if (chan->layer2link != *(skb->data))
		printk("capi_decode_actv_trans_conf: layer2link doesn't match\n");

	skb_pull(skb, 1);

	errcode = *((ushort *)skb->data);
	skb_pull(skb, 2);

	return errcode;
}

int capi_decode_disc_ind(struct pcbit_chan *chan, struct sk_buff *skb)
{
	ushort len;
#ifdef DEBUG
	int i;
#endif
	

	len = *(skb->data);
	skb_pull(skb, 1);

#ifdef DEBUG

	for (i = 0; i < len; i++)
		printk(KERN_DEBUG "Cause Octect %d: %02x\n", i + 3,
		       *(skb->data + i));
#endif

	skb_pull(skb, len);

	return 0;
}

#ifdef DEBUG
int capi_decode_debug_188(u_char *hdr, ushort hdrlen)
{
	char str[64];
	int len;

	len = hdr[0];

	if (len < 64 && len == hdrlen - 1) {
		memcpy(str, hdr + 1, hdrlen - 1);
		str[hdrlen - 1] = 0;
		printk("%s\n", str);
	}
	else
		printk("debug message incorrect\n");

	return 0;
}
#endif
