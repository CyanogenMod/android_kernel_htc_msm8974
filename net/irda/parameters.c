/*********************************************************************
 *
 * Filename:      parameters.c
 * Version:       1.0
 * Description:   A more general way to handle (pi,pl,pv) parameters
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Jun  7 10:25:11 1999
 * Modified at:   Sun Jan 30 14:08:39 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1999-2000 Dag Brattli, All Rights Reserved.
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *     MA 02111-1307 USA
 *
 ********************************************************************/

#include <linux/types.h>
#include <linux/module.h>

#include <asm/unaligned.h>
#include <asm/byteorder.h>

#include <net/irda/irda.h>
#include <net/irda/parameters.h>

static int irda_extract_integer(void *self, __u8 *buf, int len, __u8 pi,
				PV_TYPE type, PI_HANDLER func);
static int irda_extract_string(void *self, __u8 *buf, int len, __u8 pi,
			       PV_TYPE type, PI_HANDLER func);
static int irda_extract_octseq(void *self, __u8 *buf, int len, __u8 pi,
			       PV_TYPE type, PI_HANDLER func);
static int irda_extract_no_value(void *self, __u8 *buf, int len, __u8 pi,
				 PV_TYPE type, PI_HANDLER func);

static int irda_insert_integer(void *self, __u8 *buf, int len, __u8 pi,
			       PV_TYPE type, PI_HANDLER func);
static int irda_insert_no_value(void *self, __u8 *buf, int len, __u8 pi,
				PV_TYPE type, PI_HANDLER func);

static int irda_param_unpack(__u8 *buf, char *fmt, ...);

static PV_HANDLER pv_extract_table[] = {
	irda_extract_integer, 
	irda_extract_integer, 
	irda_extract_integer, 
	irda_extract_string,  
	irda_extract_integer, 
	irda_extract_octseq,  
	irda_extract_no_value 
};

static PV_HANDLER pv_insert_table[] = {
	irda_insert_integer, 
	irda_insert_integer, 
	irda_insert_integer, 
	NULL,                
	irda_insert_integer, 
	NULL,                
	irda_insert_no_value 
};

static int irda_insert_no_value(void *self, __u8 *buf, int len, __u8 pi,
				PV_TYPE type, PI_HANDLER func)
{
	irda_param_t p;
	int ret;

	p.pi = pi;
	p.pl = 0;

	
	ret = (*func)(self, &p, PV_GET);

	
	irda_param_pack(buf, "bb", p.pi, p.pl);

	if (ret < 0)
		return ret;

	return 2; 
}

static int irda_extract_no_value(void *self, __u8 *buf, int len, __u8 pi,
				 PV_TYPE type, PI_HANDLER func)
{
	irda_param_t p;
	int ret;

	
	irda_param_unpack(buf, "bb", &p.pi, &p.pl);

	
	ret = (*func)(self, &p, PV_PUT);

	if (ret < 0)
		return ret;

	return 2; 
}

static int irda_insert_integer(void *self, __u8 *buf, int len, __u8 pi,
			       PV_TYPE type, PI_HANDLER func)
{
	irda_param_t p;
	int n = 0;
	int err;

	p.pi = pi;             
	p.pl = type & PV_MASK; 
	p.pv.i = 0;            

	
	err = (*func)(self, &p, PV_GET);
	if (err < 0)
		return err;

	if (p.pl == 0) {
		if (p.pv.i < 0xff) {
			IRDA_DEBUG(2, "%s(), using 1 byte\n", __func__);
			p.pl = 1;
		} else if (p.pv.i < 0xffff) {
			IRDA_DEBUG(2, "%s(), using 2 bytes\n", __func__);
			p.pl = 2;
		} else {
			IRDA_DEBUG(2, "%s(), using 4 bytes\n", __func__);
			p.pl = 4; 
		}
	}
	
	if (len < (2+p.pl)) {
		IRDA_WARNING("%s: buffer too short for insertion!\n",
			     __func__);
		return -1;
	}
	IRDA_DEBUG(2, "%s(), pi=%#x, pl=%d, pi=%d\n", __func__,
		   p.pi, p.pl, p.pv.i);
	switch (p.pl) {
	case 1:
		n += irda_param_pack(buf, "bbb", p.pi, p.pl, (__u8) p.pv.i);
		break;
	case 2:
		if (type & PV_BIG_ENDIAN)
			p.pv.i = cpu_to_be16((__u16) p.pv.i);
		else
			p.pv.i = cpu_to_le16((__u16) p.pv.i);
		n += irda_param_pack(buf, "bbs", p.pi, p.pl, (__u16) p.pv.i);
		break;
	case 4:
		if (type & PV_BIG_ENDIAN)
			cpu_to_be32s(&p.pv.i);
		else
			cpu_to_le32s(&p.pv.i);
		n += irda_param_pack(buf, "bbi", p.pi, p.pl, p.pv.i);

		break;
	default:
		IRDA_WARNING("%s: length %d not supported\n",
			     __func__, p.pl);
		
		return -1;
	}

	return p.pl+2; 
}

static int irda_extract_integer(void *self, __u8 *buf, int len, __u8 pi,
				PV_TYPE type, PI_HANDLER func)
{
	irda_param_t p;
	int n = 0;
	int extract_len;	
	int err;

	p.pi = pi;     
	p.pl = buf[1]; 
	p.pv.i = 0;    
	extract_len = p.pl;	

	
	if (len < (2+p.pl)) {
		IRDA_WARNING("%s: buffer too short for parsing! "
			     "Need %d bytes, but len is only %d\n",
			     __func__, p.pl, len);
		return -1;
	}

	if (((type & PV_MASK) != PV_INTEGER) && ((type & PV_MASK) != p.pl)) {
		IRDA_ERROR("%s: invalid parameter length! "
			   "Expected %d bytes, but value had %d bytes!\n",
			   __func__, type & PV_MASK, p.pl);

		if((p.pl < (type & PV_MASK)) || (type & PV_BIG_ENDIAN)) {
			
			return p.pl+2;
		} else {
			
			extract_len = type & PV_MASK;
		}
	}


	switch (extract_len) {
	case 1:
		n += irda_param_unpack(buf+2, "b", &p.pv.i);
		break;
	case 2:
		n += irda_param_unpack(buf+2, "s", &p.pv.i);
		if (type & PV_BIG_ENDIAN)
			p.pv.i = be16_to_cpu((__u16) p.pv.i);
		else
			p.pv.i = le16_to_cpu((__u16) p.pv.i);
		break;
	case 4:
		n += irda_param_unpack(buf+2, "i", &p.pv.i);
		if (type & PV_BIG_ENDIAN)
			be32_to_cpus(&p.pv.i);
		else
			le32_to_cpus(&p.pv.i);
		break;
	default:
		IRDA_WARNING("%s: length %d not supported\n",
			     __func__, p.pl);

		
		return p.pl+2;
	}

	IRDA_DEBUG(2, "%s(), pi=%#x, pl=%d, pi=%d\n", __func__,
		   p.pi, p.pl, p.pv.i);
	
	err = (*func)(self, &p, PV_PUT);
	if (err < 0)
		return err;

	return p.pl+2; 
}

static int irda_extract_string(void *self, __u8 *buf, int len, __u8 pi,
			       PV_TYPE type, PI_HANDLER func)
{
	char str[33];
	irda_param_t p;
	int err;

	IRDA_DEBUG(2, "%s()\n", __func__);

	p.pi = pi;     
	p.pl = buf[1]; 
	if (p.pl > 32)
		p.pl = 32;

	IRDA_DEBUG(2, "%s(), pi=%#x, pl=%d\n", __func__,
		   p.pi, p.pl);

	
	if (len < (2+p.pl)) {
		IRDA_WARNING("%s: buffer too short for parsing! "
			     "Need %d bytes, but len is only %d\n",
			     __func__, p.pl, len);
		return -1;
	}

	strncpy(str, buf+2, p.pl);

	IRDA_DEBUG(2, "%s(), str=0x%02x 0x%02x\n", __func__,
		   (__u8) str[0], (__u8) str[1]);

	
	str[p.pl] = '\0';

	p.pv.c = str; 

	
	err = (*func)(self, &p, PV_PUT);
	if (err < 0)
		return err;

	return p.pl+2; 
}

static int irda_extract_octseq(void *self, __u8 *buf, int len, __u8 pi,
			       PV_TYPE type, PI_HANDLER func)
{
	irda_param_t p;

	p.pi = pi;     
	p.pl = buf[1]; 

	
	if (len < (2+p.pl)) {
		IRDA_WARNING("%s: buffer too short for parsing! "
			     "Need %d bytes, but len is only %d\n",
			     __func__, p.pl, len);
		return -1;
	}

	IRDA_DEBUG(0, "%s(), not impl\n", __func__);

	return p.pl+2; 
}

int irda_param_pack(__u8 *buf, char *fmt, ...)
{
	irda_pv_t arg;
	va_list args;
	char *p;
	int n = 0;

	va_start(args, fmt);

	for (p = fmt; *p != '\0'; p++) {
		switch (*p) {
		case 'b':  
			buf[n++] = (__u8)va_arg(args, int);
			break;
		case 's':  
			arg.i = (__u16)va_arg(args, int);
			put_unaligned((__u16)arg.i, (__u16 *)(buf+n)); n+=2;
			break;
		case 'i':  
			arg.i = va_arg(args, __u32);
			put_unaligned(arg.i, (__u32 *)(buf+n)); n+=4;
			break;
#if 0
		case 'c': 
			arg.c = va_arg(args, char *);
			strcpy(buf+n, arg.c);
			n += strlen(arg.c) + 1;
			break;
#endif
		default:
			va_end(args);
			return -1;
		}
	}
	va_end(args);

	return 0;
}
EXPORT_SYMBOL(irda_param_pack);

static int irda_param_unpack(__u8 *buf, char *fmt, ...)
{
	irda_pv_t arg;
	va_list args;
	char *p;
	int n = 0;

	va_start(args, fmt);

	for (p = fmt; *p != '\0'; p++) {
		switch (*p) {
		case 'b':  
			arg.ip = va_arg(args, __u32 *);
			*arg.ip = buf[n++];
			break;
		case 's':  
			arg.ip = va_arg(args, __u32 *);
			*arg.ip = get_unaligned((__u16 *)(buf+n)); n+=2;
			break;
		case 'i':  
			arg.ip = va_arg(args, __u32 *);
			*arg.ip = get_unaligned((__u32 *)(buf+n)); n+=4;
			break;
#if 0
		case 'c':   
			arg.c = va_arg(args, char *);
			strcpy(arg.c, buf+n);
			n += strlen(arg.c) + 1;
			break;
#endif
		default:
			va_end(args);
			return -1;
		}

	}
	va_end(args);

	return 0;
}

int irda_param_insert(void *self, __u8 pi, __u8 *buf, int len,
		      pi_param_info_t *info)
{
	pi_minor_info_t *pi_minor_info;
	__u8 pi_minor;
	__u8 pi_major;
	int type;
	int ret = -1;
	int n = 0;

	IRDA_ASSERT(buf != NULL, return ret;);
	IRDA_ASSERT(info != NULL, return ret;);

	pi_minor = pi & info->pi_mask;
	pi_major = pi >> info->pi_major_offset;

	
	if ((pi_major > info->len-1) ||
	    (pi_minor > info->tables[pi_major].len-1))
	{
		IRDA_DEBUG(0, "%s(), no handler for parameter=0x%02x\n",
			   __func__, pi);

		
		return -1;
	}

	
	pi_minor_info = &info->tables[pi_major].pi_minor_call_table[pi_minor];

	
	type = pi_minor_info->type;

	
	if (!pi_minor_info->func) {
		IRDA_MESSAGE("%s: no handler for pi=%#x\n", __func__, pi);
		
		return -1;
	}

	
	ret = (*pv_insert_table[type & PV_MASK])(self, buf+n, len, pi, type,
						 pi_minor_info->func);
	return ret;
}
EXPORT_SYMBOL(irda_param_insert);

static int irda_param_extract(void *self, __u8 *buf, int len,
			      pi_param_info_t *info)
{
	pi_minor_info_t *pi_minor_info;
	__u8 pi_minor;
	__u8 pi_major;
	int type;
	int ret = -1;
	int n = 0;

	IRDA_ASSERT(buf != NULL, return ret;);
	IRDA_ASSERT(info != NULL, return ret;);

	pi_minor = buf[n] & info->pi_mask;
	pi_major = buf[n] >> info->pi_major_offset;

	
	if ((pi_major > info->len-1) ||
	    (pi_minor > info->tables[pi_major].len-1))
	{
		IRDA_DEBUG(0, "%s(), no handler for parameter=0x%02x\n",
			   __func__, buf[0]);

		
		return 2 + buf[n + 1];  
	}

	
	pi_minor_info = &info->tables[pi_major].pi_minor_call_table[pi_minor];

	
	type = pi_minor_info->type;

	IRDA_DEBUG(3, "%s(), pi=[%d,%d], type=%d\n", __func__,
		   pi_major, pi_minor, type);

	
	if (!pi_minor_info->func) {
		IRDA_MESSAGE("%s: no handler for pi=%#x\n",
			     __func__, buf[n]);
		
		return 2 + buf[n + 1]; 
	}

	
	ret = (*pv_extract_table[type & PV_MASK])(self, buf+n, len, buf[n],
						  type, pi_minor_info->func);
	return ret;
}

int irda_param_extract_all(void *self, __u8 *buf, int len,
			   pi_param_info_t *info)
{
	int ret = -1;
	int n = 0;

	IRDA_ASSERT(buf != NULL, return ret;);
	IRDA_ASSERT(info != NULL, return ret;);

	while (len > 2) {
		ret = irda_param_extract(self, buf+n, len, info);
		if (ret < 0)
			return ret;

		n += ret;
		len -= ret;
	}
	return n;
}
EXPORT_SYMBOL(irda_param_extract_all);
