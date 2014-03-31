/*
 * tboot.h: shared data structure with tboot and kernel and functions
 *          used by kernel for runtime support of Intel(R) Trusted
 *          Execution Technology
 *
 * Copyright (c) 2006-2009, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _LINUX_TBOOT_H
#define _LINUX_TBOOT_H

enum {
	TB_SHUTDOWN_REBOOT = 0,
	TB_SHUTDOWN_S5,
	TB_SHUTDOWN_S4,
	TB_SHUTDOWN_S3,
	TB_SHUTDOWN_HALT,
	TB_SHUTDOWN_WFS
};

#ifdef CONFIG_INTEL_TXT
#include <acpi/acpi.h>

#define TB_KEY_SIZE             64   

#define MAX_TB_MAC_REGIONS      32

struct tboot_mac_region {
	u64  start;         
	u32  size;          
} __packed;

struct tboot_acpi_generic_address {
	u8  space_id;
	u8  bit_width;
	u8  bit_offset;
	u8  access_width;
	u64 address;
} __packed;

struct tboot_acpi_sleep_info {
	struct tboot_acpi_generic_address pm1a_cnt_blk;
	struct tboot_acpi_generic_address pm1b_cnt_blk;
	struct tboot_acpi_generic_address pm1a_evt_blk;
	struct tboot_acpi_generic_address pm1b_evt_blk;
	u16 pm1a_cnt_val;
	u16 pm1b_cnt_val;
	u64 wakeup_vector;
	u32 vector_width;
	u64 kernel_s3_resume_vector;
} __packed;

struct tboot {

	
	u8 uuid[16];

	
	u32 version;

	
	u32 log_addr;

	u32 shutdown_entry;
	u32 shutdown_type;

	
	struct tboot_acpi_sleep_info acpi_sinfo;

	
	u32 tboot_base;
	u32 tboot_size;

	
	u8 num_mac_regions;
	struct tboot_mac_region mac_regions[MAX_TB_MAC_REGIONS];



	
	u8 s3_key[TB_KEY_SIZE];



	
	u8 reserved_align[3];

	
	u32 num_in_wfs;
} __packed;

#define TBOOT_UUID	{0xff, 0x8d, 0x3c, 0x66, 0xb3, 0xe8, 0x82, 0x4b, 0xbf,\
			 0xaa, 0x19, 0xea, 0x4d, 0x5, 0x7a, 0x8}

extern struct tboot *tboot;

static inline int tboot_enabled(void)
{
	return tboot != NULL;
}

extern void tboot_probe(void);
extern void tboot_shutdown(u32 shutdown_type);
extern struct acpi_table_header *tboot_get_dmar_table(
				      struct acpi_table_header *dmar_tbl);
extern int tboot_force_iommu(void);

#else

#define tboot_enabled()			0
#define tboot_probe()			do { } while (0)
#define tboot_shutdown(shutdown_type)	do { } while (0)
#define tboot_sleep(sleep_state, pm1a_control, pm1b_control)	\
					do { } while (0)
#define tboot_get_dmar_table(dmar_tbl)	(dmar_tbl)
#define tboot_force_iommu()		0

#endif 

#endif 
