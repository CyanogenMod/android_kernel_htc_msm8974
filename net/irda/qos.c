/*********************************************************************
 *
 * Filename:      qos.c
 * Version:       1.0
 * Description:   IrLAP QoS parameter negotiation
 * Status:        Stable
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Sep  9 00:00:26 1997
 * Modified at:   Sun Jan 30 14:29:16 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1998-2000 Dag Brattli <dagb@cs.uit.no>,
 *     All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
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

#include <linux/export.h>

#include <asm/byteorder.h>

#include <net/irda/irda.h>
#include <net/irda/parameters.h>
#include <net/irda/qos.h>
#include <net/irda/irlap.h>
#include <net/irda/irlap_frame.h>

int sysctl_max_baud_rate = 16000000;
int sysctl_max_noreply_time = 12;
unsigned int sysctl_min_tx_turn_time = 10;
unsigned int sysctl_max_tx_data_size = 2042;
unsigned int sysctl_max_tx_window = 7;

static int irlap_param_baud_rate(void *instance, irda_param_t *param, int get);
static int irlap_param_link_disconnect(void *instance, irda_param_t *parm,
				       int get);
static int irlap_param_max_turn_time(void *instance, irda_param_t *param,
				     int get);
static int irlap_param_data_size(void *instance, irda_param_t *param, int get);
static int irlap_param_window_size(void *instance, irda_param_t *param,
				   int get);
static int irlap_param_additional_bofs(void *instance, irda_param_t *parm,
				       int get);
static int irlap_param_min_turn_time(void *instance, irda_param_t *param,
				     int get);

#ifndef CONFIG_IRDA_DYNAMIC_WINDOW
static __u32 irlap_requested_line_capacity(struct qos_info *qos);
#endif

static __u32 min_turn_times[]  = { 10000, 5000, 1000, 500, 100, 50, 10, 0 }; 
static __u32 baud_rates[]      = { 2400, 9600, 19200, 38400, 57600, 115200, 576000,
				   1152000, 4000000, 16000000 };           
static __u32 data_sizes[]      = { 64, 128, 256, 512, 1024, 2048 };        
static __u32 add_bofs[]        = { 48, 24, 12, 5, 3, 2, 1, 0 };            
static __u32 max_turn_times[]  = { 500, 250, 100, 50 };                    
static __u32 link_disc_times[] = { 3, 8, 12, 16, 20, 25, 30, 40 };         

static __u32 max_line_capacities[10][4] = {
       
	{    100,      0,      0,     0 }, 
	{    400,      0,      0,     0 }, 
	{    800,      0,      0,     0 }, 
	{   1600,      0,      0,     0 }, 
	{   2360,      0,      0,     0 }, 
	{   4800,   2400,    960,   480 }, 
	{  28800,  11520,   5760,  2880 }, 
	{  57600,  28800,  11520,  5760 }, 
	{ 200000, 100000,  40000, 20000 }, 
	{ 800000, 400000, 160000, 80000 }, 
};

static pi_minor_info_t pi_minor_call_table_type_0[] = {
	{ NULL, 0 },
{ irlap_param_baud_rate,       PV_INTEGER | PV_LITTLE_ENDIAN },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
{ irlap_param_link_disconnect, PV_INT_8_BITS }
};

static pi_minor_info_t pi_minor_call_table_type_1[] = {
	{ NULL, 0 },
	{ NULL, 0 },
{ irlap_param_max_turn_time,   PV_INT_8_BITS },
{ irlap_param_data_size,       PV_INT_8_BITS },
{ irlap_param_window_size,     PV_INT_8_BITS },
{ irlap_param_additional_bofs, PV_INT_8_BITS },
{ irlap_param_min_turn_time,   PV_INT_8_BITS },
};

static pi_major_info_t pi_major_call_table[] = {
	{ pi_minor_call_table_type_0, 9 },
	{ pi_minor_call_table_type_1, 7 },
};

static pi_param_info_t irlap_param_info = { pi_major_call_table, 2, 0x7f, 7 };

static inline int value_index(__u32 value, __u32 *array, int size)
{
	int i;

	for (i=0; i < size; i++)
		if (array[i] == value)
			break;
	return i;
}

static inline __u32 index_value(int index, __u32 *array)
{
	return array[index];
}

static int msb_index (__u16 word)
{
	__u16 msb = 0x8000;
	int index = 15;   

	if (word == 0) {
		IRDA_WARNING("%s(), Detected buggy peer, adjust null PV to 0x1!\n",
			 __func__);
		
		word = 0x1;
	}

	while (msb) {
		if (word & msb)
			break;   
		msb >>=1;
		index--;
	}
	return index;
}

static inline int value_lower_bits(__u32 value, __u32 *array, int size, __u16 *field)
{
	int	i;
	__u16	mask = 0x1;
	__u16	result = 0x0;

	for (i=0; i < size; i++) {
		
		result |= mask;
		mask <<= 1;
		
		if (array[i] >= value)
			break;
	}
	
	if(i >= size)
	  i = size - 1;	
	*field = result;
	return i;
}

static inline int value_highest_bit(__u32 value, __u32 *array, int size, __u16 *field)
{
	int	i;
	__u16	mask = 0x1;
	__u16	result = 0x0;

	for (i=0; i < size; i++) {
		
		if (array[i] <= value)
			break;
		
		mask <<= 1;
	}
	
	result |= mask;
	
	if(i >= size)
	  i = size - 1;	
	*field = result;
	return i;
}


void irda_qos_compute_intersection(struct qos_info *qos, struct qos_info *new)
{
	IRDA_ASSERT(qos != NULL, return;);
	IRDA_ASSERT(new != NULL, return;);

	
	qos->baud_rate.bits       &= new->baud_rate.bits;
	qos->window_size.bits     &= new->window_size.bits;
	qos->min_turn_time.bits   &= new->min_turn_time.bits;
	qos->max_turn_time.bits   &= new->max_turn_time.bits;
	qos->data_size.bits       &= new->data_size.bits;
	qos->link_disc_time.bits  &= new->link_disc_time.bits;
	qos->additional_bofs.bits &= new->additional_bofs.bits;

	irda_qos_bits_to_value(qos);
}

void irda_init_max_qos_capabilies(struct qos_info *qos)
{
	int i;

	
	
	i = value_lower_bits(sysctl_max_baud_rate, baud_rates, 10,
			     &qos->baud_rate.bits);
	sysctl_max_baud_rate = index_value(i, baud_rates);

	
	i = value_lower_bits(sysctl_max_noreply_time, link_disc_times, 8,
			     &qos->link_disc_time.bits);
	sysctl_max_noreply_time = index_value(i, link_disc_times);

	
	qos->baud_rate.bits    &= 0x03ff;

	qos->window_size.bits     = 0x7f;
	qos->min_turn_time.bits   = 0xff;
	qos->max_turn_time.bits   = 0x0f;
	qos->data_size.bits       = 0x3f;
	qos->link_disc_time.bits &= 0xff;
	qos->additional_bofs.bits = 0xff;
}
EXPORT_SYMBOL(irda_init_max_qos_capabilies);

static void irlap_adjust_qos_settings(struct qos_info *qos)
{
	__u32 line_capacity;
	int index;

	IRDA_DEBUG(2, "%s()\n", __func__);

	if (sysctl_min_tx_turn_time > qos->min_turn_time.value) {
		int i;

		IRDA_WARNING("%s(), Detected buggy peer, adjust mtt to %dus!\n",
			 __func__, sysctl_min_tx_turn_time);

		
		i = value_highest_bit(sysctl_min_tx_turn_time, min_turn_times,
				      8, &qos->min_turn_time.bits);
		sysctl_min_tx_turn_time = index_value(i, min_turn_times);
		qos->min_turn_time.value = sysctl_min_tx_turn_time;
	}

	if ((qos->baud_rate.value < 115200) &&
	    (qos->max_turn_time.value < 500))
	{
		IRDA_DEBUG(0,
			   "%s(), adjusting max turn time from %d to 500 ms\n",
			   __func__, qos->max_turn_time.value);
		qos->max_turn_time.value = 500;
	}

	index = value_index(qos->data_size.value, data_sizes, 6);
	line_capacity = irlap_max_line_capacity(qos->baud_rate.value,
						qos->max_turn_time.value);

#ifdef CONFIG_IRDA_DYNAMIC_WINDOW
	while ((qos->data_size.value > line_capacity) && (index > 0)) {
		qos->data_size.value = data_sizes[index--];
		IRDA_DEBUG(2, "%s(), reducing data size to %d\n",
			   __func__, qos->data_size.value);
	}
#else 
	while (irlap_requested_line_capacity(qos) > line_capacity) {
		IRDA_ASSERT(index != 0, return;);

		
		if (qos->window_size.value > 1) {
			qos->window_size.value--;
			IRDA_DEBUG(2, "%s(), reducing window size to %d\n",
				   __func__, qos->window_size.value);
		} else if (index > 1) {
			qos->data_size.value = data_sizes[index--];
			IRDA_DEBUG(2, "%s(), reducing data size to %d\n",
				   __func__, qos->data_size.value);
		} else {
			IRDA_WARNING("%s(), nothing more we can do!\n",
				     __func__);
		}
	}
#endif 
	if (qos->data_size.value > sysctl_max_tx_data_size)
		
		qos->data_size.value = sysctl_max_tx_data_size;
	if (qos->window_size.value > sysctl_max_tx_window)
		qos->window_size.value = sysctl_max_tx_window;
}

int irlap_qos_negotiate(struct irlap_cb *self, struct sk_buff *skb)
{
	int ret;

	ret = irda_param_extract_all(self, skb->data, skb->len,
				     &irlap_param_info);

	
	irda_qos_bits_to_value(&self->qos_tx);
	irda_qos_bits_to_value(&self->qos_rx);

	irlap_adjust_qos_settings(&self->qos_tx);

	IRDA_DEBUG(2, "Setting BAUD_RATE to %d bps.\n",
		   self->qos_tx.baud_rate.value);
	IRDA_DEBUG(2, "Setting DATA_SIZE to %d bytes\n",
		   self->qos_tx.data_size.value);
	IRDA_DEBUG(2, "Setting WINDOW_SIZE to %d\n",
		   self->qos_tx.window_size.value);
	IRDA_DEBUG(2, "Setting XBOFS to %d\n",
		   self->qos_tx.additional_bofs.value);
	IRDA_DEBUG(2, "Setting MAX_TURN_TIME to %d ms.\n",
		   self->qos_tx.max_turn_time.value);
	IRDA_DEBUG(2, "Setting MIN_TURN_TIME to %d usecs.\n",
		   self->qos_tx.min_turn_time.value);
	IRDA_DEBUG(2, "Setting LINK_DISC to %d secs.\n",
		   self->qos_tx.link_disc_time.value);
	return ret;
}

int irlap_insert_qos_negotiation_params(struct irlap_cb *self,
					struct sk_buff *skb)
{
	int ret;

	
	ret = irda_param_insert(self, PI_BAUD_RATE, skb_tail_pointer(skb),
				skb_tailroom(skb), &irlap_param_info);
	if (ret < 0)
		return ret;
	skb_put(skb, ret);

	
	ret = irda_param_insert(self, PI_MAX_TURN_TIME, skb_tail_pointer(skb),
				skb_tailroom(skb), &irlap_param_info);
	if (ret < 0)
		return ret;
	skb_put(skb, ret);

	
	ret = irda_param_insert(self, PI_DATA_SIZE, skb_tail_pointer(skb),
				skb_tailroom(skb), &irlap_param_info);
	if (ret < 0)
		return ret;
	skb_put(skb, ret);

	
	ret = irda_param_insert(self, PI_WINDOW_SIZE, skb_tail_pointer(skb),
				skb_tailroom(skb), &irlap_param_info);
	if (ret < 0)
		return ret;
	skb_put(skb, ret);

	
	ret = irda_param_insert(self, PI_ADD_BOFS, skb_tail_pointer(skb),
				skb_tailroom(skb), &irlap_param_info);
	if (ret < 0)
		return ret;
	skb_put(skb, ret);

	
	ret = irda_param_insert(self, PI_MIN_TURN_TIME, skb_tail_pointer(skb),
				skb_tailroom(skb), &irlap_param_info);
	if (ret < 0)
		return ret;
	skb_put(skb, ret);

	
	ret = irda_param_insert(self, PI_LINK_DISC, skb_tail_pointer(skb),
				skb_tailroom(skb), &irlap_param_info);
	if (ret < 0)
		return ret;
	skb_put(skb, ret);

	return 0;
}

static int irlap_param_baud_rate(void *instance, irda_param_t *param, int get)
{
	__u16 final;

	struct irlap_cb *self = (struct irlap_cb *) instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	if (get) {
		param->pv.i = self->qos_rx.baud_rate.bits;
		IRDA_DEBUG(2, "%s(), baud rate = 0x%02x\n",
			   __func__, param->pv.i);
	} else {
		IRDA_DEBUG(2, "Requested BAUD_RATE: 0x%04x\n", (__u16) param->pv.i);
		final = (__u16) param->pv.i & self->qos_rx.baud_rate.bits;

		IRDA_DEBUG(2, "Final BAUD_RATE: 0x%04x\n", final);
		self->qos_tx.baud_rate.bits = final;
		self->qos_rx.baud_rate.bits = final;
	}

	return 0;
}

static int irlap_param_link_disconnect(void *instance, irda_param_t *param,
				       int get)
{
	__u16 final;

	struct irlap_cb *self = (struct irlap_cb *) instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	if (get)
		param->pv.i = self->qos_rx.link_disc_time.bits;
	else {
		IRDA_DEBUG(2, "LINK_DISC: %02x\n", (__u8) param->pv.i);
		final = (__u8) param->pv.i & self->qos_rx.link_disc_time.bits;

		IRDA_DEBUG(2, "Final LINK_DISC: %02x\n", final);
		self->qos_tx.link_disc_time.bits = final;
		self->qos_rx.link_disc_time.bits = final;
	}
	return 0;
}

static int irlap_param_max_turn_time(void *instance, irda_param_t *param,
				     int get)
{
	struct irlap_cb *self = (struct irlap_cb *) instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	if (get)
		param->pv.i = self->qos_rx.max_turn_time.bits;
	else
		self->qos_tx.max_turn_time.bits = (__u8) param->pv.i;

	return 0;
}

static int irlap_param_data_size(void *instance, irda_param_t *param, int get)
{
	struct irlap_cb *self = (struct irlap_cb *) instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	if (get)
		param->pv.i = self->qos_rx.data_size.bits;
	else
		self->qos_tx.data_size.bits = (__u8) param->pv.i;

	return 0;
}

static int irlap_param_window_size(void *instance, irda_param_t *param,
				   int get)
{
	struct irlap_cb *self = (struct irlap_cb *) instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	if (get)
		param->pv.i = self->qos_rx.window_size.bits;
	else
		self->qos_tx.window_size.bits = (__u8) param->pv.i;

	return 0;
}

static int irlap_param_additional_bofs(void *instance, irda_param_t *param, int get)
{
	struct irlap_cb *self = (struct irlap_cb *) instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	if (get)
		param->pv.i = self->qos_rx.additional_bofs.bits;
	else
		self->qos_tx.additional_bofs.bits = (__u8) param->pv.i;

	return 0;
}

static int irlap_param_min_turn_time(void *instance, irda_param_t *param,
				     int get)
{
	struct irlap_cb *self = (struct irlap_cb *) instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	if (get)
		param->pv.i = self->qos_rx.min_turn_time.bits;
	else
		self->qos_tx.min_turn_time.bits = (__u8) param->pv.i;

	return 0;
}

__u32 irlap_max_line_capacity(__u32 speed, __u32 max_turn_time)
{
	__u32 line_capacity;
	int i,j;

	IRDA_DEBUG(2, "%s(), speed=%d, max_turn_time=%d\n",
		   __func__, speed, max_turn_time);

	i = value_index(speed, baud_rates, 10);
	j = value_index(max_turn_time, max_turn_times, 4);

	IRDA_ASSERT(((i >=0) && (i <10)), return 0;);
	IRDA_ASSERT(((j >=0) && (j <4)), return 0;);

	line_capacity = max_line_capacities[i][j];

	IRDA_DEBUG(2, "%s(), line capacity=%d bytes\n",
		   __func__, line_capacity);

	return line_capacity;
}

#ifndef CONFIG_IRDA_DYNAMIC_WINDOW
static __u32 irlap_requested_line_capacity(struct qos_info *qos)
{
	__u32 line_capacity;

	line_capacity = qos->window_size.value *
		(qos->data_size.value + 6 + qos->additional_bofs.value) +
		irlap_min_turn_time_in_bytes(qos->baud_rate.value,
					     qos->min_turn_time.value);

	IRDA_DEBUG(2, "%s(), requested line capacity=%d\n",
		   __func__, line_capacity);

	return line_capacity;
}
#endif

void irda_qos_bits_to_value(struct qos_info *qos)
{
	int index;

	IRDA_ASSERT(qos != NULL, return;);

	index = msb_index(qos->baud_rate.bits);
	qos->baud_rate.value = baud_rates[index];

	index = msb_index(qos->data_size.bits);
	qos->data_size.value = data_sizes[index];

	index = msb_index(qos->window_size.bits);
	qos->window_size.value = index+1;

	index = msb_index(qos->min_turn_time.bits);
	qos->min_turn_time.value = min_turn_times[index];

	index = msb_index(qos->max_turn_time.bits);
	qos->max_turn_time.value = max_turn_times[index];

	index = msb_index(qos->link_disc_time.bits);
	qos->link_disc_time.value = link_disc_times[index];

	index = msb_index(qos->additional_bofs.bits);
	qos->additional_bofs.value = add_bofs[index];
}
EXPORT_SYMBOL(irda_qos_bits_to_value);
