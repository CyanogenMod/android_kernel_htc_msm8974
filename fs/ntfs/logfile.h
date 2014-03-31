/*
 * logfile.h - Defines for NTFS kernel journal ($LogFile) handling.  Part of
 *	       the Linux-NTFS project.
 *
 * Copyright (c) 2000-2005 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_NTFS_LOGFILE_H
#define _LINUX_NTFS_LOGFILE_H

#ifdef NTFS_RW

#include <linux/fs.h>

#include "types.h"
#include "endian.h"
#include "layout.h"


#define MaxLogFileSize		0x100000000ULL
#define DefaultLogPageSize	4096
#define MinLogRecordPages	48

typedef struct {
	NTFS_RECORD_TYPE magic;	
	le16 usa_ofs;		
	le16 usa_count;		

	leLSN chkdsk_lsn;	
	le32 system_page_size;	
	le32 log_page_size;	
	le16 restart_area_offset;
	sle16 minor_ver;	
	sle16 major_ver;	
} __attribute__ ((__packed__)) RESTART_PAGE_HEADER;

#define LOGFILE_NO_CLIENT	cpu_to_le16(0xffff)
#define LOGFILE_NO_CLIENT_CPU	0xffff

enum {
	RESTART_VOLUME_IS_CLEAN	= cpu_to_le16(0x0002),
	RESTART_SPACE_FILLER	= cpu_to_le16(0xffff), 
} __attribute__ ((__packed__));

typedef le16 RESTART_AREA_FLAGS;

typedef struct {
	leLSN current_lsn;	/* The current, i.e. last LSN inside the log
				   when the restart area was last written.
				   This happens often but what is the interval?
				   Is it just fixed time or is it every time a
				   check point is written or somethine else?
				   On create set to 0. */
	le16 log_clients;	
	le16 client_free_list;	
	le16 client_in_use_list;
	RESTART_AREA_FLAGS flags;
	le32 seq_number_bits;	
	le16 restart_area_length;
	le16 client_array_offset;
	sle64 file_size;	
	le32 last_lsn_data_length;
	le16 log_record_header_length;
	le16 log_page_data_offset;
	le32 restart_log_open_count;
	le32 reserved;		
} __attribute__ ((__packed__)) RESTART_AREA;

typedef struct {
	leLSN oldest_lsn;	
	leLSN client_restart_lsn;
	le16 prev_client;	
	le16 next_client;	
	le16 seq_number;	
	u8 reserved[6];		
	le32 client_name_length;
	ntfschar client_name[64];
} __attribute__ ((__packed__)) LOG_CLIENT_RECORD;

extern bool ntfs_check_logfile(struct inode *log_vi,
		RESTART_PAGE_HEADER **rp);

extern bool ntfs_is_logfile_clean(struct inode *log_vi,
		const RESTART_PAGE_HEADER *rp);

extern bool ntfs_empty_logfile(struct inode *log_vi);

#endif 

#endif 
