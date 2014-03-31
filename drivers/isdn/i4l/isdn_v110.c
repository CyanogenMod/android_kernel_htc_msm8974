/* $Id: isdn_v110.c,v 1.1.2.2 2004/01/12 22:37:19 keil Exp $
 *
 * Linux ISDN subsystem, V.110 related functions (linklevel).
 *
 * Copyright by Thomas Pfeiffer (pfeiffer@pds.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/delay.h>

#include <linux/isdn.h>
#include "isdn_v110.h"

#undef ISDN_V110_DEBUG

char *isdn_v110_revision = "$Revision: 1.1.2.2 $";

#define V110_38400 255
#define V110_19200  15
#define V110_9600    3

static unsigned char V110_OnMatrix_9600[] =
{0xfc, 0xfc, 0xfc, 0xfc, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff,
 0xff, 0xfd, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xfd,
 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff,
 0xff, 0xfd, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xfd};

static unsigned char V110_OffMatrix_9600[] =
{0xfc, 0xfc, 0xfc, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static unsigned char V110_OnMatrix_19200[] =
{0xf0, 0xf0, 0xff, 0xf7, 0xff, 0xf7, 0xff, 0xf7, 0xff, 0xf7,
 0xfd, 0xff, 0xff, 0xf7, 0xff, 0xf7, 0xff, 0xf7, 0xff, 0xf7};

static unsigned char V110_OffMatrix_19200[] =
{0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static unsigned char V110_OnMatrix_38400[] =
{0x00, 0x7f, 0x7f, 0x7f, 0x7f, 0xfd, 0x7f, 0x7f, 0x7f, 0x7f};

static unsigned char V110_OffMatrix_38400[] =
{0x00, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff};

static inline unsigned char
FlipBits(unsigned char c, int keylen)
{
	unsigned char b = c;
	unsigned char bit = 128;
	int i;
	int j;
	int hunks = (8 / keylen);

	c = 0;
	for (i = 0; i < hunks; i++) {
		for (j = 0; j < keylen; j++) {
			if (b & (bit >> j))
				c |= bit >> (keylen - j - 1);
		}
		bit >>= keylen;
	}
	return c;
}


static isdn_v110_stream *
isdn_v110_open(unsigned char key, int hdrlen, int maxsize)
{
	int i;
	isdn_v110_stream *v;

	if ((v = kzalloc(sizeof(isdn_v110_stream), GFP_ATOMIC)) == NULL)
		return NULL;
	v->key = key;
	v->nbits = 0;
	for (i = 0; key & (1 << i); i++)
		v->nbits++;

	v->nbytes = 8 / v->nbits;
	v->decodelen = 0;

	switch (key) {
	case V110_38400:
		v->OnlineFrame = V110_OnMatrix_38400;
		v->OfflineFrame = V110_OffMatrix_38400;
		break;
	case V110_19200:
		v->OnlineFrame = V110_OnMatrix_19200;
		v->OfflineFrame = V110_OffMatrix_19200;
		break;
	default:
		v->OnlineFrame = V110_OnMatrix_9600;
		v->OfflineFrame = V110_OffMatrix_9600;
		break;
	}
	v->framelen = v->nbytes * 10;
	v->SyncInit = 5;
	v->introducer = 0;
	v->dbit = 1;
	v->b = 0;
	v->skbres = hdrlen;
	v->maxsize = maxsize - hdrlen;
	if ((v->encodebuf = kmalloc(maxsize, GFP_ATOMIC)) == NULL) {
		kfree(v);
		return NULL;
	}
	return v;
}

void
isdn_v110_close(isdn_v110_stream *v)
{
	if (v == NULL)
		return;
#ifdef ISDN_V110_DEBUG
	printk(KERN_DEBUG "v110 close\n");
#endif
	kfree(v->encodebuf);
	kfree(v);
}


static int
ValidHeaderBytes(isdn_v110_stream *v)
{
	int i;
	for (i = 0; (i < v->decodelen) && (i < v->nbytes); i++)
		if ((v->decodebuf[i] & v->key) != 0)
			break;
	return i;
}

static void
SyncHeader(isdn_v110_stream *v)
{
	unsigned char *rbuf = v->decodebuf;
	int len = v->decodelen;

	if (len == 0)
		return;
	for (rbuf++, len--; len > 0; len--, rbuf++)	
		if ((*rbuf & v->key) == 0)	
			break;  
	if (len)
		memcpy(v->decodebuf, rbuf, len);

	v->decodelen = len;
#ifdef ISDN_V110_DEBUG
	printk(KERN_DEBUG "isdn_v110: Header resync\n");
#endif
}

static int
DecodeMatrix(isdn_v110_stream *v, unsigned char *m, int len, unsigned char *buf)
{
	int line = 0;
	int buflen = 0;
	int mbit = 64;
	int introducer = v->introducer;
	int dbit = v->dbit;
	unsigned char b = v->b;

	while (line < len) {    
		if ((line % 10) == 0) {	
			if (m[line] != 0x00) {	
#ifdef ISDN_V110_DEBUG
				printk(KERN_DEBUG "isdn_v110: DecodeMatrix, V110 Bad Header\n");
				
#endif
			}
			line++; 
			continue;
		} else if ((line % 10) == 5) {	
			if ((m[line] & 0x70) != 0x30) {	
#ifdef ISDN_V110_DEBUG
				printk(KERN_DEBUG "isdn_v110: DecodeMatrix, V110 Bad 5th line\n");
				
#endif
			}
			line++; 
			continue;
		} else if (!introducer) {	
			introducer = (m[line] & mbit) ? 0 : 1;	
		next_byte:
			if (mbit > 2) {	
				mbit >>= 1;	
				continue;
			}       
			mbit = 64;
			line++;
			continue;
		} else {        
			if (m[line] & mbit)	
				b |= dbit;	
			else
				b &= dbit - 1;	
			if (dbit < 128)	
				dbit <<= 1;	
			else {  
				buf[buflen++] = b;	
				introducer = b = 0;	
				dbit = 1;	
			}
			goto next_byte;	
		}
	}
	v->introducer = introducer;
	v->dbit = dbit;
	v->b = b;
	return buflen;          
}

struct sk_buff *
isdn_v110_decode(isdn_v110_stream *v, struct sk_buff *skb)
{
	int i;
	int j;
	int len;
	unsigned char *v110_buf;
	unsigned char *rbuf;

	if (!skb) {
		printk(KERN_WARNING "isdn_v110_decode called with NULL skb!\n");
		return NULL;
	}
	rbuf = skb->data;
	len = skb->len;
	if (v == NULL) {
		
		printk(KERN_WARNING "isdn_v110_decode called with NULL stream!\n");
		dev_kfree_skb(skb);
		return NULL;
	}
	if (v->decodelen == 0)  
		for (; len > 0; len--, rbuf++)	
			if ((*rbuf & v->key) == 0)
				break;	
	if (len == 0) {
		dev_kfree_skb(skb);
		return NULL;
	}
	
	memcpy(&(v->decodebuf[v->decodelen]), rbuf, len);
	v->decodelen += len;
ReSync:
	if (v->decodelen < v->nbytes) {	
		dev_kfree_skb(skb);
		return NULL;    
	}
	if (ValidHeaderBytes(v) != v->nbytes) {	
		SyncHeader(v);  
		goto ReSync;
	}
	len = (v->decodelen - (v->decodelen % (10 * v->nbytes))) / v->nbytes;
	if ((v110_buf = kmalloc(len, GFP_ATOMIC)) == NULL) {
		printk(KERN_WARNING "isdn_v110_decode: Couldn't allocate v110_buf\n");
		dev_kfree_skb(skb);
		return NULL;
	}
	for (i = 0; i < len; i++) {
		v110_buf[i] = 0;
		for (j = 0; j < v->nbytes; j++)
			v110_buf[i] |= (v->decodebuf[(i * v->nbytes) + j] & v->key) << (8 - ((j + 1) * v->nbits));
		v110_buf[i] = FlipBits(v110_buf[i], v->nbits);
	}
	v->decodelen = (v->decodelen % (10 * v->nbytes));
	memcpy(v->decodebuf, &(v->decodebuf[len * v->nbytes]), v->decodelen);

	skb_trim(skb, DecodeMatrix(v, v110_buf, len, skb->data));
	kfree(v110_buf);
	if (skb->len)
		return skb;
	else {
		kfree_skb(skb);
		return NULL;
	}
}

static int
EncodeMatrix(unsigned char *buf, int len, unsigned char *m, int mlen)
{
	int line = 0;
	int i = 0;
	int mbit = 128;
	int dbit = 1;
	int introducer = 3;
	int ibit[] = {0, 1, 1};

	while ((i < len) && (line < mlen)) {	
		switch (line % 10) {	
		case 0:
			m[line++] = 0x00;	
			mbit = 128;	
			break;
		case 5:
			m[line++] = 0xbf;	
			mbit = 128;	
			break;
		}
		if (line >= mlen) {
			printk(KERN_WARNING "isdn_v110 (EncodeMatrix): buffer full!\n");
			return line;
		}
	next_bit:
		switch (mbit) { 
		case 1:
			line++;	
			if (line >= mlen) {
				printk(KERN_WARNING "isdn_v110 (EncodeMatrix): buffer full!\n");
				return line;
			}
		case 128:
			m[line] = 128;	
			mbit = 64;	
			continue;
		}
		if (introducer) {	
			introducer--;	
			m[line] |= ibit[introducer] ? mbit : 0;	
			mbit >>= 1;	
			goto next_bit;	
		}               
		m[line] |= (buf[i] & dbit) ? mbit : 0;	
		if (dbit == 128) {	
			dbit = 1;	
			i++;            
			if (i < len)	
				introducer = 3;	
			else {  
				m[line] |= (mbit - 1) & 0xfe;	
				break;
			}
		} else          
			dbit <<= 1;	
		mbit >>= 1;     
		goto next_bit;

	}
	
	if ((line) && ((line + 10) < mlen))
		switch (++line % 10) {
		case 1:
			m[line++] = 0xfe;
		case 2:
			m[line++] = 0xfe;
		case 3:
			m[line++] = 0xfe;
		case 4:
			m[line++] = 0xfe;
		case 5:
			m[line++] = 0xbf;
		case 6:
			m[line++] = 0xfe;
		case 7:
			m[line++] = 0xfe;
		case 8:
			m[line++] = 0xfe;
		case 9:
			m[line++] = 0xfe;
		}
	return line;            
}

static struct sk_buff *
isdn_v110_sync(isdn_v110_stream *v)
{
	struct sk_buff *skb;

	if (v == NULL) {
		
		printk(KERN_WARNING "isdn_v110_sync called with NULL stream!\n");
		return NULL;
	}
	if ((skb = dev_alloc_skb(v->framelen + v->skbres))) {
		skb_reserve(skb, v->skbres);
		memcpy(skb_put(skb, v->framelen), v->OfflineFrame, v->framelen);
	}
	return skb;
}

static struct sk_buff *
isdn_v110_idle(isdn_v110_stream *v)
{
	struct sk_buff *skb;

	if (v == NULL) {
		
		printk(KERN_WARNING "isdn_v110_sync called with NULL stream!\n");
		return NULL;
	}
	if ((skb = dev_alloc_skb(v->framelen + v->skbres))) {
		skb_reserve(skb, v->skbres);
		memcpy(skb_put(skb, v->framelen), v->OnlineFrame, v->framelen);
	}
	return skb;
}

struct sk_buff *
isdn_v110_encode(isdn_v110_stream *v, struct sk_buff *skb)
{
	int i;
	int j;
	int rlen;
	int mlen;
	int olen;
	int size;
	int sval1;
	int sval2;
	int nframes;
	unsigned char *v110buf;
	unsigned char *rbuf;
	struct sk_buff *nskb;

	if (v == NULL) {
		
		printk(KERN_WARNING "isdn_v110_encode called with NULL stream!\n");
		return NULL;
	}
	if (!skb) {
		
		printk(KERN_WARNING "isdn_v110_encode called with NULL skb!\n");
		return NULL;
	}
	rlen = skb->len;
	nframes = (rlen + 3) / 4;
	v110buf = v->encodebuf;
	if ((nframes * 40) > v->maxsize) {
		size = v->maxsize;
		rlen = v->maxsize / 40;
	} else
		size = nframes * 40;
	if (!(nskb = dev_alloc_skb(size + v->skbres + sizeof(int)))) {
		printk(KERN_WARNING "isdn_v110_encode: Couldn't alloc skb\n");
		return NULL;
	}
	skb_reserve(nskb, v->skbres + sizeof(int));
	if (skb->len == 0) {
		memcpy(skb_put(nskb, v->framelen), v->OnlineFrame, v->framelen);
		*((int *)skb_push(nskb, sizeof(int))) = 0;
		return nskb;
	}
	mlen = EncodeMatrix(skb->data, rlen, v110buf, size);
	
	rbuf = skb_put(nskb, size);
	olen = 0;
	sval1 = 8 - v->nbits;
	sval2 = v->key << sval1;
	for (i = 0; i < mlen; i++) {
		v110buf[i] = FlipBits(v110buf[i], v->nbits);
		for (j = 0; j < v->nbytes; j++) {
			if (size--)
				*rbuf++ = ~v->key | (((v110buf[i] << (j * v->nbits)) & sval2) >> sval1);
			else {
				printk(KERN_WARNING "isdn_v110_encode: buffers full!\n");
				goto buffer_full;
			}
			olen++;
		}
	}
buffer_full:
	skb_trim(nskb, olen);
	*((int *)skb_push(nskb, sizeof(int))) = rlen;
	return nskb;
}

int
isdn_v110_stat_callback(int idx, isdn_ctrl *c)
{
	isdn_v110_stream *v = NULL;
	int i;
	int ret = 0;

	if (idx < 0)
		return 0;
	switch (c->command) {
	case ISDN_STAT_BSENT:
		if (!(v = dev->v110[idx]))
			return 0;
		atomic_inc(&dev->v110use[idx]);
		for (i = 0; i * v->framelen < c->parm.length; i++) {
			if (v->skbidle > 0) {
				v->skbidle--;
				ret = 1;
			} else {
				if (v->skbuser > 0)
					v->skbuser--;
				ret = 0;
			}
		}
		for (i = v->skbuser + v->skbidle; i < 2; i++) {
			struct sk_buff *skb;
			if (v->SyncInit > 0)
				skb = isdn_v110_sync(v);
			else
				skb = isdn_v110_idle(v);
			if (skb) {
				if (dev->drv[c->driver]->interface->writebuf_skb(c->driver, c->arg, 1, skb) <= 0) {
					dev_kfree_skb(skb);
					break;
				} else {
					if (v->SyncInit)
						v->SyncInit--;
					v->skbidle++;
				}
			} else
				break;
		}
		atomic_dec(&dev->v110use[idx]);
		return ret;
	case ISDN_STAT_DHUP:
	case ISDN_STAT_BHUP:
		while (1) {
			atomic_inc(&dev->v110use[idx]);
			if (atomic_dec_and_test(&dev->v110use[idx])) {
				isdn_v110_close(dev->v110[idx]);
				dev->v110[idx] = NULL;
				break;
			}
			mdelay(1);
		}
		break;
	case ISDN_STAT_BCONN:
		if (dev->v110emu[idx] && (dev->v110[idx] == NULL)) {
			int hdrlen = dev->drv[c->driver]->interface->hl_hdrlen;
			int maxsize = dev->drv[c->driver]->interface->maxbufsize;
			atomic_inc(&dev->v110use[idx]);
			switch (dev->v110emu[idx]) {
			case ISDN_PROTO_L2_V11096:
				dev->v110[idx] = isdn_v110_open(V110_9600, hdrlen, maxsize);
				break;
			case ISDN_PROTO_L2_V11019:
				dev->v110[idx] = isdn_v110_open(V110_19200, hdrlen, maxsize);
				break;
			case ISDN_PROTO_L2_V11038:
				dev->v110[idx] = isdn_v110_open(V110_38400, hdrlen, maxsize);
				break;
			default:;
			}
			if ((v = dev->v110[idx])) {
				while (v->SyncInit) {
					struct sk_buff *skb = isdn_v110_sync(v);
					if (dev->drv[c->driver]->interface->writebuf_skb(c->driver, c->arg, 1, skb) <= 0) {
						dev_kfree_skb(skb);
						
						break;
					}
					v->SyncInit--;
					v->skbidle++;
				}
			} else
				printk(KERN_WARNING "isdn_v110: Couldn't open stream for chan %d\n", idx);
			atomic_dec(&dev->v110use[idx]);
		}
		break;
	default:
		return 0;
	}
	return 0;
}
