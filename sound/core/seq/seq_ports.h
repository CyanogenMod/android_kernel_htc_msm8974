/*
 *   ALSA sequencer Ports 
 *   Copyright (c) 1998 by Frank van de Pol <fvdpol@coil.demon.nl>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#ifndef __SND_SEQ_PORTS_H
#define __SND_SEQ_PORTS_H

#include <sound/seq_kernel.h>
#include "seq_lock.h"



struct snd_seq_subscribers {
	struct snd_seq_port_subscribe info;	
	struct list_head src_list;	
	struct list_head dest_list;	
	atomic_t ref_count;
};

struct snd_seq_port_subs_info {
	struct list_head list_head;	
	unsigned int count;		
	unsigned int exclusive: 1;	
	struct rw_semaphore list_mutex;
	rwlock_t list_lock;
	int (*open)(void *private_data, struct snd_seq_port_subscribe *info);
	int (*close)(void *private_data, struct snd_seq_port_subscribe *info);
};

struct snd_seq_client_port {

	struct snd_seq_addr addr;	
	struct module *owner;		
	char name[64];				
	struct list_head list;		
	snd_use_lock_t use_lock;

	
	struct snd_seq_port_subs_info c_src;	
	struct snd_seq_port_subs_info c_dest;	

	int (*event_input)(struct snd_seq_event *ev, int direct, void *private_data,
			   int atomic, int hop);
	void (*private_free)(void *private_data);
	void *private_data;
	unsigned int callback_all : 1;
	unsigned int closing : 1;
	unsigned int timestamping: 1;
	unsigned int time_real: 1;
	int time_queue;
	
	
	unsigned int capability;	
	unsigned int type;		

	
	int midi_channels;
	int midi_voices;
	int synth_voices;
		
};

struct snd_seq_client;

struct snd_seq_client_port *snd_seq_port_use_ptr(struct snd_seq_client *client, int num);

struct snd_seq_client_port *snd_seq_port_query_nearest(struct snd_seq_client *client,
						       struct snd_seq_port_info *pinfo);

#define snd_seq_port_unlock(port) snd_use_lock_free(&(port)->use_lock)

struct snd_seq_client_port *snd_seq_create_port(struct snd_seq_client *client, int port_index);

int snd_seq_delete_port(struct snd_seq_client *client, int port);

int snd_seq_delete_all_ports(struct snd_seq_client *client);

int snd_seq_set_port_info(struct snd_seq_client_port *port,
			  struct snd_seq_port_info *info);

int snd_seq_get_port_info(struct snd_seq_client_port *port,
			  struct snd_seq_port_info *info);

int snd_seq_port_connect(struct snd_seq_client *caller,
			 struct snd_seq_client *s, struct snd_seq_client_port *sp,
			 struct snd_seq_client *d, struct snd_seq_client_port *dp,
			 struct snd_seq_port_subscribe *info);

 
int snd_seq_port_disconnect(struct snd_seq_client *caller,
			    struct snd_seq_client *s, struct snd_seq_client_port *sp,
			    struct snd_seq_client *d, struct snd_seq_client_port *dp,
			    struct snd_seq_port_subscribe *info);

int snd_seq_port_subscribe(struct snd_seq_client_port *port,
			   struct snd_seq_port_subscribe *info);

struct snd_seq_subscribers *snd_seq_port_get_subscription(struct snd_seq_port_subs_info *src_grp,
							  struct snd_seq_addr *dest_addr);

#endif
