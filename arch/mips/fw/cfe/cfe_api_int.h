/*
 * Copyright (C) 2000, 2001, 2002 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef CFE_API_INT_H
#define CFE_API_INT_H

#define CFE_CMD_FW_GETINFO	0
#define CFE_CMD_FW_RESTART	1
#define CFE_CMD_FW_BOOT		2
#define CFE_CMD_FW_CPUCTL	3
#define CFE_CMD_FW_GETTIME      4
#define CFE_CMD_FW_MEMENUM	5
#define CFE_CMD_FW_FLUSHCACHE	6

#define CFE_CMD_DEV_GETHANDLE	9
#define CFE_CMD_DEV_ENUM	10
#define CFE_CMD_DEV_OPEN	11
#define CFE_CMD_DEV_INPSTAT	12
#define CFE_CMD_DEV_READ	13
#define CFE_CMD_DEV_WRITE	14
#define CFE_CMD_DEV_IOCTL	15
#define CFE_CMD_DEV_CLOSE	16
#define CFE_CMD_DEV_GETINFO	17

#define CFE_CMD_ENV_ENUM	20
#define CFE_CMD_ENV_GET		22
#define CFE_CMD_ENV_SET		23
#define CFE_CMD_ENV_DEL		24

#define CFE_CMD_MAX		32

#define CFE_CMD_VENDOR_USE	0x8000	


typedef s64 cfe_xptr_t;

struct xiocb_buffer {
	u64 buf_offset;		
	cfe_xptr_t  buf_ptr;		
	u64 buf_length;		
	u64 buf_retlen;		
	u64 buf_ioctlcmd;	
};

struct xiocb_inpstat {
	u64 inp_status;		
};

struct xiocb_envbuf {
	s64 enum_idx;		
	cfe_xptr_t name_ptr;		
	s64 name_length;		
	cfe_xptr_t val_ptr;		
	s64 val_length;		
};

struct xiocb_cpuctl {
	u64 cpu_number;		
	u64 cpu_command;	
	u64 start_addr;		
	u64 gp_val;		
	u64 sp_val;		
	u64 a1_val;		
};

struct xiocb_time {
	s64 ticks;		
};

struct xiocb_exitstat{
	s64 status;
};

struct xiocb_meminfo {
	s64 mi_idx;		
	s64 mi_type;		
	u64 mi_addr;		
	u64 mi_size;		
};

struct xiocb_fwinfo {
	s64 fwi_version;		
	s64 fwi_totalmem;	
	s64 fwi_flags;		
	s64 fwi_boardid;		
	s64 fwi_bootarea_va;	
	s64 fwi_bootarea_pa;	
	s64 fwi_bootarea_size;	
	s64 fwi_reserved1;
	s64 fwi_reserved2;
	s64 fwi_reserved3;
};

struct cfe_xiocb {
	u64 xiocb_fcode;	
	s64 xiocb_status;	
	s64 xiocb_handle;	
	u64 xiocb_flags;	
	u64 xiocb_psize;	
	union {
		
		struct xiocb_buffer xiocb_buffer;

		
		struct xiocb_inpstat xiocb_inpstat;

		
		struct xiocb_envbuf xiocb_envbuf;

		
		struct xiocb_cpuctl xiocb_cpuctl;

		
		struct xiocb_time xiocb_time;

		
		struct xiocb_meminfo xiocb_meminfo;

		
		struct xiocb_fwinfo xiocb_fwinfo;

		
		struct xiocb_exitstat xiocb_exitstat;
	} plist;
};

#endif 
