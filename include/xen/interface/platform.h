/******************************************************************************
 * platform.h
 *
 * Hardware platform operations. Intended for use by domain-0 kernel.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2002-2006, K Fraser
 */

#ifndef __XEN_PUBLIC_PLATFORM_H__
#define __XEN_PUBLIC_PLATFORM_H__

#include "xen.h"

#define XENPF_INTERFACE_VERSION 0x03000001

#define XENPF_settime             17
struct xenpf_settime {
	
	uint32_t secs;
	uint32_t nsecs;
	uint64_t system_time;
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_settime_t);

#define XENPF_add_memtype         31
struct xenpf_add_memtype {
	
	unsigned long mfn;
	uint64_t nr_mfns;
	uint32_t type;
	
	uint32_t handle;
	uint32_t reg;
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_add_memtype_t);

#define XENPF_del_memtype         32
struct xenpf_del_memtype {
	
	uint32_t handle;
	uint32_t reg;
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_del_memtype_t);

#define XENPF_read_memtype        33
struct xenpf_read_memtype {
	
	uint32_t reg;
	
	unsigned long mfn;
	uint64_t nr_mfns;
	uint32_t type;
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_read_memtype_t);

#define XENPF_microcode_update    35
struct xenpf_microcode_update {
	
	GUEST_HANDLE(void) data;          
	uint32_t length;                  
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_microcode_update_t);

#define XENPF_platform_quirk      39
#define QUIRK_NOIRQBALANCING      1 
#define QUIRK_IOAPIC_BAD_REGSEL   2 
#define QUIRK_IOAPIC_GOOD_REGSEL  3 
struct xenpf_platform_quirk {
	
	uint32_t quirk_id;
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_platform_quirk_t);

#define XENPF_firmware_info       50
#define XEN_FW_DISK_INFO          1 
#define XEN_FW_DISK_MBR_SIGNATURE 2 
#define XEN_FW_VBEDDC_INFO        3 
struct xenpf_firmware_info {
	
	uint32_t type;
	uint32_t index;
	
	union {
		struct {
			
			uint8_t device;                   
			uint8_t version;                  
			uint16_t interface_support;       
			
			uint16_t legacy_max_cylinder;     
			uint8_t legacy_max_head;          
			uint8_t legacy_sectors_per_track; 
			
			
			GUEST_HANDLE(void) edd_params;
		} disk_info; 
		struct {
			uint8_t device;                   
			uint32_t mbr_signature;           
		} disk_mbr_signature; 
		struct {
			
			uint8_t capabilities;
			uint8_t edid_transfer_time;
			
			GUEST_HANDLE(uchar) edid;
		} vbeddc_info; 
	} u;
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_firmware_info_t);

#define XENPF_enter_acpi_sleep    51
struct xenpf_enter_acpi_sleep {
	
	uint16_t pm1a_cnt_val;      
	uint16_t pm1b_cnt_val;      
	uint32_t sleep_state;       
	uint32_t flags;             
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_enter_acpi_sleep_t);

#define XENPF_change_freq         52
struct xenpf_change_freq {
	
	uint32_t flags; 
	uint32_t cpu;   
	uint64_t freq;  
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_change_freq_t);

/*
 * Get idle times (nanoseconds since boot) for physical CPUs specified in the
 * @cpumap_bitmap with range [0..@cpumap_nr_cpus-1]. The @idletime array is
 * indexed by CPU number; only entries with the corresponding @cpumap_bitmap
 * bit set are written to. On return, @cpumap_bitmap is modified so that any
 * non-existent CPUs are cleared. Such CPUs have their @idletime array entry
 * cleared.
 */
#define XENPF_getidletime         53
struct xenpf_getidletime {
	
	
	GUEST_HANDLE(uchar) cpumap_bitmap;
	
	
	uint32_t cpumap_nr_cpus;
	
	GUEST_HANDLE(uint64_t) idletime;
	
	
	uint64_t now;
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_getidletime_t);

#define XENPF_set_processor_pminfo      54

#define XEN_PROCESSOR_PM_CX	1
#define XEN_PROCESSOR_PM_PX	2
#define XEN_PROCESSOR_PM_TX	4

#define XEN_PM_CX   0
#define XEN_PM_PX   1
#define XEN_PM_TX   2
#define XEN_PM_PDC  3
#define XEN_PX_PCT   1
#define XEN_PX_PSS   2
#define XEN_PX_PPC   4
#define XEN_PX_PSD   8

struct xen_power_register {
	uint32_t     space_id;
	uint32_t     bit_width;
	uint32_t     bit_offset;
	uint32_t     access_size;
	uint64_t     address;
};

struct xen_processor_csd {
	uint32_t    domain;      
	uint32_t    coord_type;  
	uint32_t    num;         
};
DEFINE_GUEST_HANDLE_STRUCT(xen_processor_csd);

struct xen_processor_cx {
	struct xen_power_register  reg; 
	uint8_t     type;     
	uint32_t    latency;  
	uint32_t    power;    
	uint32_t    dpcnt;    
	GUEST_HANDLE(xen_processor_csd) dp; 
};
DEFINE_GUEST_HANDLE_STRUCT(xen_processor_cx);

struct xen_processor_flags {
	uint32_t bm_control:1;
	uint32_t bm_check:1;
	uint32_t has_cst:1;
	uint32_t power_setup_done:1;
	uint32_t bm_rld_set:1;
};

struct xen_processor_power {
	uint32_t count;  
	struct xen_processor_flags flags;  
	GUEST_HANDLE(xen_processor_cx) states; 
};

struct xen_pct_register {
	uint8_t  descriptor;
	uint16_t length;
	uint8_t  space_id;
	uint8_t  bit_width;
	uint8_t  bit_offset;
	uint8_t  reserved;
	uint64_t address;
};

struct xen_processor_px {
	uint64_t core_frequency; 
	uint64_t power;      
	uint64_t transition_latency; 
	uint64_t bus_master_latency; 
	uint64_t control;        
	uint64_t status;     
};
DEFINE_GUEST_HANDLE_STRUCT(xen_processor_px);

struct xen_psd_package {
	uint64_t num_entries;
	uint64_t revision;
	uint64_t domain;
	uint64_t coord_type;
	uint64_t num_processors;
};

struct xen_processor_performance {
	uint32_t flags;     
	uint32_t platform_limit;  
	struct xen_pct_register control_register;
	struct xen_pct_register status_register;
	uint32_t state_count;     
	GUEST_HANDLE(xen_processor_px) states;
	struct xen_psd_package domain_info;
	uint32_t shared_type;     
};
DEFINE_GUEST_HANDLE_STRUCT(xen_processor_performance);

struct xenpf_set_processor_pminfo {
	
	uint32_t id;    
	uint32_t type;  
	union {
		struct xen_processor_power          power;
		struct xen_processor_performance    perf; 
		GUEST_HANDLE(uint32_t)              pdc;
	};
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_set_processor_pminfo);

#define XENPF_get_cpuinfo 55
struct xenpf_pcpuinfo {
	
	uint32_t xen_cpuid;
	
	
	uint32_t max_present;
#define XEN_PCPU_FLAGS_ONLINE   1
	
#define XEN_PCPU_FLAGS_INVALID  2
	uint32_t flags;
	uint32_t apic_id;
	uint32_t acpi_id;
};
DEFINE_GUEST_HANDLE_STRUCT(xenpf_pcpuinfo);

struct xen_platform_op {
	uint32_t cmd;
	uint32_t interface_version; 
	union {
		struct xenpf_settime           settime;
		struct xenpf_add_memtype       add_memtype;
		struct xenpf_del_memtype       del_memtype;
		struct xenpf_read_memtype      read_memtype;
		struct xenpf_microcode_update  microcode;
		struct xenpf_platform_quirk    platform_quirk;
		struct xenpf_firmware_info     firmware_info;
		struct xenpf_enter_acpi_sleep  enter_acpi_sleep;
		struct xenpf_change_freq       change_freq;
		struct xenpf_getidletime       getidletime;
		struct xenpf_set_processor_pminfo set_pminfo;
		struct xenpf_pcpuinfo          pcpu_info;
		uint8_t                        pad[128];
	} u;
};
DEFINE_GUEST_HANDLE_STRUCT(xen_platform_op_t);

#endif 
