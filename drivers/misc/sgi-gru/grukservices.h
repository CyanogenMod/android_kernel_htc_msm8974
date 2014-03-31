
/*
 *  Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#ifndef __GRU_KSERVICES_H_
#define __GRU_KSERVICES_H_



struct gru_message_queue_desc {
	void		*mq;			
	unsigned long	mq_gpa;			
	int		qlines;			
	int		interrupt_vector;	
	int		interrupt_pnode;	
	int		interrupt_apicid;	
};

extern int gru_create_message_queue(struct gru_message_queue_desc *mqd,
		void *p, unsigned int bytes, int nasid, int vector, int apicid);

extern int gru_send_message_gpa(struct gru_message_queue_desc *mqd,
			void *mesg, unsigned int bytes);

#define MQE_OK			0	
#define MQE_CONGESTION		1	
#define MQE_QUEUE_FULL		2	
#define MQE_UNEXPECTED_CB_ERR	3	
#define MQE_PAGE_OVERFLOW	10	
#define MQE_BUG_NO_RESOURCES	11	

extern void gru_free_message(struct gru_message_queue_desc *mqd,
			     void *mesq);

extern void *gru_get_next_message(struct gru_message_queue_desc *mqd);


int gru_read_gpa(unsigned long *value, unsigned long gpa);


extern int gru_copy_gpa(unsigned long dest_gpa, unsigned long src_gpa,
							unsigned int bytes);

extern unsigned long gru_reserve_async_resources(int blade_id, int cbrs, int dsr_bytes,
				struct completion *cmp);

extern void gru_release_async_resources(unsigned long han);

extern void gru_wait_async_cbr(unsigned long han);

extern void gru_lock_async_resource(unsigned long han,  void **cb, void **dsr);

extern void gru_unlock_async_resource(unsigned long han);

#endif 		
