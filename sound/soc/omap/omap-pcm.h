/*
 * omap-pcm.h
 *
 * Copyright (C) 2008 Nokia Corporation
 *
 * Contact: Jarkko Nikula <jarkko.nikula@bitmer.com>
 *          Peter Ujfalusi <peter.ujfalusi@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __OMAP_PCM_H__
#define __OMAP_PCM_H__

struct snd_pcm_substream;

struct omap_pcm_dma_data {
	char		*name;		
	int		dma_req;	
	unsigned long	port_addr;	
	void (*set_threshold)(struct snd_pcm_substream *substream);
	int		data_type;	
	int		sync_mode;	
	int		packet_size;	
};

#endif
