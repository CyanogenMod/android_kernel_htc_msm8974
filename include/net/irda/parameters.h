/*********************************************************************
 *                
 * Filename:      parameters.h
 * Version:       1.0
 * Description:   A more general way to handle (pi,pl,pv) parameters
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Jun  7 08:47:28 1999
 * Modified at:   Sun Jan 30 14:05:14 2000
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
 *     Michel Dänzer <daenzer@debian.org>, 10/2001
 *     - simplify irda_pv_t to avoid endianness issues
 *     
 ********************************************************************/

#ifndef IRDA_PARAMS_H
#define IRDA_PARAMS_H

typedef enum {
	PV_INTEGER,      
	PV_INT_8_BITS,   
	PV_INT_16_BITS,  
	PV_STRING,       
	PV_INT_32_BITS,  
	PV_OCT_SEQ,      
	PV_NO_VALUE      
} PV_TYPE;

#define PV_BIG_ENDIAN    0x80 
#define PV_LITTLE_ENDIAN 0x00
#define PV_MASK          0x7f   

#define PV_PUT 0
#define PV_GET 1

typedef union {
	char   *c;
	__u32   i;
	__u32 *ip;
} irda_pv_t;

typedef struct {
	__u8 pi;
	__u8 pl;
	irda_pv_t pv;
} irda_param_t;

typedef int (*PI_HANDLER)(void *self, irda_param_t *param, int get);
typedef int (*PV_HANDLER)(void *self, __u8 *buf, int len, __u8 pi,
			  PV_TYPE type, PI_HANDLER func);

typedef struct {
	PI_HANDLER func;  
	PV_TYPE    type;  
} pi_minor_info_t;

typedef struct {
	pi_minor_info_t *pi_minor_call_table;
	int len;
} pi_major_info_t;

typedef struct {
	pi_major_info_t *tables;
	int              len;
	__u8             pi_mask;
	int              pi_major_offset;
} pi_param_info_t;

int irda_param_pack(__u8 *buf, char *fmt, ...);

int irda_param_insert(void *self, __u8 pi, __u8 *buf, int len, 
		      pi_param_info_t *info);
int irda_param_extract_all(void *self, __u8 *buf, int len, 
			   pi_param_info_t *info);

#define irda_param_insert_byte(buf,pi,pv) irda_param_pack(buf,"bbb",pi,1,pv)

#endif 

