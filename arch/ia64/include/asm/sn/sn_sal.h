#ifndef _ASM_IA64_SN_SN_SAL_H
#define _ASM_IA64_SN_SN_SAL_H

/*
 * System Abstraction Layer definitions for IA64
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000-2006 Silicon Graphics, Inc.  All rights reserved.
 */


#include <asm/sal.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/arch.h>
#include <asm/sn/geo.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/shub_mmr.h>

#define  SN_SAL_POD_MODE                           0x02000001
#define  SN_SAL_SYSTEM_RESET                       0x02000002
#define  SN_SAL_PROBE                              0x02000003
#define  SN_SAL_GET_MASTER_NASID                   0x02000004
#define	 SN_SAL_GET_KLCONFIG_ADDR		   0x02000005
#define  SN_SAL_LOG_CE				   0x02000006
#define  SN_SAL_REGISTER_CE			   0x02000007
#define  SN_SAL_GET_PARTITION_ADDR		   0x02000009
#define  SN_SAL_XP_ADDR_REGION			   0x0200000f
#define  SN_SAL_NO_FAULT_ZONE_VIRTUAL		   0x02000010
#define  SN_SAL_NO_FAULT_ZONE_PHYSICAL		   0x02000011
#define  SN_SAL_PRINT_ERROR			   0x02000012
#define  SN_SAL_REGISTER_PMI_HANDLER		   0x02000014
#define  SN_SAL_SET_ERROR_HANDLING_FEATURES	   0x0200001a	
#define  SN_SAL_GET_FIT_COMPT			   0x0200001b	
#define  SN_SAL_GET_SAPIC_INFO                     0x0200001d
#define  SN_SAL_GET_SN_INFO                        0x0200001e
#define  SN_SAL_CONSOLE_PUTC                       0x02000021
#define  SN_SAL_CONSOLE_GETC                       0x02000022
#define  SN_SAL_CONSOLE_PUTS                       0x02000023
#define  SN_SAL_CONSOLE_GETS                       0x02000024
#define  SN_SAL_CONSOLE_GETS_TIMEOUT               0x02000025
#define  SN_SAL_CONSOLE_POLL                       0x02000026
#define  SN_SAL_CONSOLE_INTR                       0x02000027
#define  SN_SAL_CONSOLE_PUTB			   0x02000028
#define  SN_SAL_CONSOLE_XMIT_CHARS		   0x0200002a
#define  SN_SAL_CONSOLE_READC			   0x0200002b
#define  SN_SAL_SYSCTL_OP			   0x02000030
#define  SN_SAL_SYSCTL_MODID_GET	           0x02000031
#define  SN_SAL_SYSCTL_GET                         0x02000032
#define  SN_SAL_SYSCTL_IOBRICK_MODULE_GET          0x02000033
#define  SN_SAL_SYSCTL_IO_PORTSPEED_GET            0x02000035
#define  SN_SAL_SYSCTL_SLAB_GET                    0x02000036
#define  SN_SAL_BUS_CONFIG		   	   0x02000037
#define  SN_SAL_SYS_SERIAL_GET			   0x02000038
#define  SN_SAL_PARTITION_SERIAL_GET		   0x02000039
#define  SN_SAL_SYSCTL_PARTITION_GET               0x0200003a
#define  SN_SAL_SYSTEM_POWER_DOWN		   0x0200003b
#define  SN_SAL_GET_MASTER_BASEIO_NASID		   0x0200003c
#define  SN_SAL_COHERENCE                          0x0200003d
#define  SN_SAL_MEMPROTECT                         0x0200003e
#define  SN_SAL_SYSCTL_FRU_CAPTURE		   0x0200003f

#define  SN_SAL_SYSCTL_IOBRICK_PCI_OP		   0x02000042	
#define	 SN_SAL_IROUTER_OP			   0x02000043
#define  SN_SAL_SYSCTL_EVENT                       0x02000044
#define  SN_SAL_IOIF_INTERRUPT			   0x0200004a
#define  SN_SAL_HWPERF_OP			   0x02000050   
#define  SN_SAL_IOIF_ERROR_INTERRUPT		   0x02000051
#define  SN_SAL_IOIF_PCI_SAFE			   0x02000052
#define  SN_SAL_IOIF_SLOT_ENABLE		   0x02000053
#define  SN_SAL_IOIF_SLOT_DISABLE		   0x02000054
#define  SN_SAL_IOIF_GET_HUBDEV_INFO		   0x02000055
#define  SN_SAL_IOIF_GET_PCIBUS_INFO		   0x02000056
#define  SN_SAL_IOIF_GET_PCIDEV_INFO		   0x02000057
#define  SN_SAL_IOIF_GET_WIDGET_DMAFLUSH_LIST	   0x02000058	
#define  SN_SAL_IOIF_GET_DEVICE_DMAFLUSH_LIST	   0x0200005a

#define SN_SAL_IOIF_INIT			   0x0200005f
#define SN_SAL_HUB_ERROR_INTERRUPT		   0x02000060
#define SN_SAL_BTE_RECOVER			   0x02000061
#define SN_SAL_RESERVED_DO_NOT_USE		   0x02000062
#define SN_SAL_IOIF_GET_PCI_TOPOLOGY		   0x02000064

#define  SN_SAL_GET_PROM_FEATURE_SET		   0x02000065
#define  SN_SAL_SET_OS_FEATURE_SET		   0x02000066
#define  SN_SAL_INJECT_ERROR			   0x02000067
#define  SN_SAL_SET_CPU_NUMBER			   0x02000068

#define  SN_SAL_KERNEL_LAUNCH_EVENT		   0x02000069
#define  SN_SAL_WATCHLIST_ALLOC			   0x02000070
#define  SN_SAL_WATCHLIST_FREE			   0x02000071


	
#define SAL_CONSOLE_INTR_OFF    0       
#define SAL_CONSOLE_INTR_ON     1       
#define SAL_CONSOLE_INTR_STATUS 2	
	
#define SAL_CONSOLE_INTR_XMIT	1	
#define SAL_CONSOLE_INTR_RECV	2	

#define SAL_INTR_ALLOC		1
#define SAL_INTR_FREE		2
#define SAL_INTR_REDIRECT	3

#define SAL_SYSCTL_OP_IOBOARD		0x0001  
#define SAL_SYSCTL_OP_TIO_JLCK_RST      0x0002  

#define SAL_IROUTER_OPEN	0	
#define SAL_IROUTER_CLOSE	1	
#define SAL_IROUTER_SEND	2	
#define SAL_IROUTER_RECV	3	
#define SAL_IROUTER_INTR_STATUS	4	
#define SAL_IROUTER_INTR_ON	5	
#define SAL_IROUTER_INTR_OFF	6	
#define SAL_IROUTER_INIT	7	

#define SAL_IROUTER_INTR_XMIT	SAL_CONSOLE_INTR_XMIT
#define SAL_IROUTER_INTR_RECV	SAL_CONSOLE_INTR_RECV

#define SAL_ERR_FEAT_MCA_SLV_TO_OS_INIT_SLV	0x1	
#define SAL_ERR_FEAT_LOG_SBES			0x2	
#define SAL_ERR_FEAT_MFR_OVERRIDE		0x4
#define SAL_ERR_FEAT_SBE_THRESHOLD		0xffff0000

#define SALRET_MORE_PASSES	1
#define SALRET_OK		0
#define SALRET_NOT_IMPLEMENTED	(-1)
#define SALRET_INVALID_ARG	(-2)
#define SALRET_ERROR		(-3)

#define SN_SAL_FAKE_PROM			   0x02009999

static inline u32
sn_sal_rev(void)
{
	struct ia64_sal_systab *systab = __va(efi.sal_systab);

	return (u32)(systab->sal_b_rev_major << 8 | systab->sal_b_rev_minor);
}

static inline u64
ia64_sn_get_console_nasid(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL(ret_stuff, SN_SAL_GET_MASTER_NASID, 0, 0, 0, 0, 0, 0, 0);

	if (ret_stuff.status < 0)
		return ret_stuff.status;

	
	return ret_stuff.v0;
}

static inline u64
ia64_sn_get_master_baseio_nasid(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL(ret_stuff, SN_SAL_GET_MASTER_BASEIO_NASID, 0, 0, 0, 0, 0, 0, 0);

	if (ret_stuff.status < 0)
		return ret_stuff.status;

	
	return ret_stuff.v0;
}

static inline void *
ia64_sn_get_klconfig_addr(nasid_t nasid)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL(ret_stuff, SN_SAL_GET_KLCONFIG_ADDR, (u64)nasid, 0, 0, 0, 0, 0, 0);
	return ret_stuff.v0 ? __va(ret_stuff.v0) : NULL;
}

static inline u64
ia64_sn_console_getc(int *ch)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_GETC, 0, 0, 0, 0, 0, 0, 0);

	
	*ch = (int)ret_stuff.v0;

	return ret_stuff.status;
}

static inline u64
ia64_sn_console_readc(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_READC, 0, 0, 0, 0, 0, 0, 0);

	
	return ret_stuff.v0;
}

static inline u64
ia64_sn_console_putc(char ch)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_PUTC, (u64)ch, 0, 0, 0, 0, 0, 0);

	return ret_stuff.status;
}

static inline u64
ia64_sn_console_putb(const char *buf, int len)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0; 
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_PUTB, (u64)buf, (u64)len, 0, 0, 0, 0, 0);

	if ( ret_stuff.status == 0 ) {
		return ret_stuff.v0;
	}
	return (u64)0;
}

static inline u64
ia64_sn_plat_specific_err_print(int (*hook)(const char*, ...), char *rec)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_REENTRANT(ret_stuff, SN_SAL_PRINT_ERROR, (u64)hook, (u64)rec, 0, 0, 0, 0, 0);

	return ret_stuff.status;
}

static inline u64
ia64_sn_plat_cpei_handler(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_LOG_CE, 0, 0, 0, 0, 0, 0, 0);

	return ret_stuff.status;
}

static inline u64
ia64_sn_plat_set_error_handling_features(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_REENTRANT(ret_stuff, SN_SAL_SET_ERROR_HANDLING_FEATURES,
		SAL_ERR_FEAT_LOG_SBES,
		0, 0, 0, 0, 0, 0);

	return ret_stuff.status;
}

static inline u64
ia64_sn_console_check(int *result)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_POLL, 0, 0, 0, 0, 0, 0, 0);

	
	*result = (int)ret_stuff.v0;

	return ret_stuff.status;
}

static inline u64
ia64_sn_console_intr_status(void)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_INTR, 
		 0, SAL_CONSOLE_INTR_STATUS,
		 0, 0, 0, 0, 0);

	if (ret_stuff.status == 0) {
	    return ret_stuff.v0;
	}
	
	return 0;
}

static inline void
ia64_sn_console_intr_enable(u64 intr)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_INTR, 
		 intr, SAL_CONSOLE_INTR_ON,
		 0, 0, 0, 0, 0);
}

static inline void
ia64_sn_console_intr_disable(u64 intr)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_INTR, 
		 intr, SAL_CONSOLE_INTR_OFF,
		 0, 0, 0, 0, 0);
}

static inline u64
ia64_sn_console_xmit_chars(char *buf, int len)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_CONSOLE_XMIT_CHARS,
		 (u64)buf, (u64)len,
		 0, 0, 0, 0, 0);

	if (ret_stuff.status == 0) {
	    return ret_stuff.v0;
	}

	return 0;
}

static inline u64
ia64_sn_sysctl_iobrick_module_get(nasid_t nasid, int *result)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_SYSCTL_IOBRICK_MODULE_GET, nasid, 0, 0, 0, 0, 0, 0);

	
	*result = (int)ret_stuff.v0;

	return ret_stuff.status;
}

static inline u64
ia64_sn_pod_mode(void)
{
	struct ia64_sal_retval isrv;
	SAL_CALL_REENTRANT(isrv, SN_SAL_POD_MODE, 0, 0, 0, 0, 0, 0, 0);
	if (isrv.status)
		return 0;
	return isrv.v0;
}

static inline u64
ia64_sn_probe_mem(long addr, long size, void *data_ptr)
{
	struct ia64_sal_retval isrv;

	SAL_CALL(isrv, SN_SAL_PROBE, addr, size, 0, 0, 0, 0, 0);

	if (data_ptr) {
		switch (size) {
		case 1:
			*((u8*)data_ptr) = (u8)isrv.v0;
			break;
		case 2:
			*((u16*)data_ptr) = (u16)isrv.v0;
			break;
		case 4:
			*((u32*)data_ptr) = (u32)isrv.v0;
			break;
		case 8:
			*((u64*)data_ptr) = (u64)isrv.v0;
			break;
		default:
			isrv.status = 2;
		}
	}
	return isrv.status;
}

static inline u64
ia64_sn_sys_serial_get(char *buf)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_SYS_SERIAL_GET, buf, 0, 0, 0, 0, 0, 0);
	return ret_stuff.status;
}

extern char sn_system_serial_number_string[];
extern u64 sn_partition_serial_number;

static inline char *
sn_system_serial_number(void) {
	if (sn_system_serial_number_string[0]) {
		return(sn_system_serial_number_string);
	} else {
		ia64_sn_sys_serial_get(sn_system_serial_number_string);
		return(sn_system_serial_number_string);
	}
}
	

/*
 * Returns a unique id number for this system and partition (suitable for
 * use with license managers), based in part on the system serial number.
 */
static inline u64
ia64_sn_partition_serial_get(void)
{
	struct ia64_sal_retval ret_stuff;
	ia64_sal_oemcall_reentrant(&ret_stuff, SN_SAL_PARTITION_SERIAL_GET, 0,
				   0, 0, 0, 0, 0, 0);
	if (ret_stuff.status != 0)
	    return 0;
	return ret_stuff.v0;
}

static inline u64
sn_partition_serial_number_val(void) {
	if (unlikely(sn_partition_serial_number == 0)) {
		sn_partition_serial_number = ia64_sn_partition_serial_get();
	}
	return sn_partition_serial_number;
}

static inline partid_t
ia64_sn_sysctl_partition_get(nasid_t nasid)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL(ret_stuff, SN_SAL_SYSCTL_PARTITION_GET, nasid,
		0, 0, 0, 0, 0, 0);
	if (ret_stuff.status != 0)
	    return -1;
	return ((partid_t)ret_stuff.v0);
}

static inline s64
sn_partition_reserved_page_pa(u64 buf, u64 *cookie, u64 *addr, u64 *len)
{
	struct ia64_sal_retval rv;
	ia64_sal_oemcall_reentrant(&rv, SN_SAL_GET_PARTITION_ADDR, *cookie,
				   *addr, buf, *len, 0, 0, 0);
	*cookie = rv.v0;
	*addr = rv.v1;
	*len = rv.v2;
	return rv.status;
}

static inline int
sn_register_xp_addr_region(u64 paddr, u64 len, int operation)
{
	struct ia64_sal_retval ret_stuff;
	ia64_sal_oemcall(&ret_stuff, SN_SAL_XP_ADDR_REGION, paddr, len,
			 (u64)operation, 0, 0, 0, 0);
	return ret_stuff.status;
}

static inline int
sn_register_nofault_code(u64 start_addr, u64 end_addr, u64 return_addr,
			 int virtual, int operation)
{
	struct ia64_sal_retval ret_stuff;
	u64 call;
	if (virtual) {
		call = SN_SAL_NO_FAULT_ZONE_VIRTUAL;
	} else {
		call = SN_SAL_NO_FAULT_ZONE_PHYSICAL;
	}
	ia64_sal_oemcall(&ret_stuff, call, start_addr, end_addr, return_addr,
			 (u64)1, 0, 0, 0);
	return ret_stuff.status;
}

static inline int
sn_register_pmi_handler(u64 handler, u64 global_pointer)
{
	struct ia64_sal_retval ret_stuff;
	ia64_sal_oemcall(&ret_stuff, SN_SAL_REGISTER_PMI_HANDLER, handler,
			 global_pointer, 0, 0, 0, 0, 0);
	return ret_stuff.status;
}

static inline int
sn_change_coherence(u64 *new_domain, u64 *old_domain)
{
	struct ia64_sal_retval ret_stuff;
	ia64_sal_oemcall_nolock(&ret_stuff, SN_SAL_COHERENCE, (u64)new_domain,
				(u64)old_domain, 0, 0, 0, 0, 0);
	return ret_stuff.status;
}

static inline int
sn_change_memprotect(u64 paddr, u64 len, u64 perms, u64 *nasid_array)
{
	struct ia64_sal_retval ret_stuff;

	ia64_sal_oemcall_nolock(&ret_stuff, SN_SAL_MEMPROTECT, paddr, len,
				(u64)nasid_array, perms, 0, 0, 0);
	return ret_stuff.status;
}
#define SN_MEMPROT_ACCESS_CLASS_0		0x14a080
#define SN_MEMPROT_ACCESS_CLASS_1		0x2520c2
#define SN_MEMPROT_ACCESS_CLASS_2		0x14a1ca
#define SN_MEMPROT_ACCESS_CLASS_3		0x14a290
#define SN_MEMPROT_ACCESS_CLASS_6		0x084080
#define SN_MEMPROT_ACCESS_CLASS_7		0x021080

static inline void
ia64_sn_power_down(void)
{
	struct ia64_sal_retval ret_stuff;
	SAL_CALL(ret_stuff, SN_SAL_SYSTEM_POWER_DOWN, 0, 0, 0, 0, 0, 0, 0);
	while(1)
		cpu_relax();
	
}

static inline u64
ia64_sn_fru_capture(void)
{
        struct ia64_sal_retval isrv;
        SAL_CALL(isrv, SN_SAL_SYSCTL_FRU_CAPTURE, 0, 0, 0, 0, 0, 0, 0);
        if (isrv.status)
                return 0;
        return isrv.v0;
}

static inline u64
ia64_sn_sysctl_iobrick_pci_op(nasid_t n, u64 connection_type, 
			      u64 bus, char slot, 
			      u64 action)
{
	struct ia64_sal_retval rv = {0, 0, 0, 0};

	SAL_CALL_NOLOCK(rv, SN_SAL_SYSCTL_IOBRICK_PCI_OP, connection_type, n, action,
		 bus, (u64) slot, 0, 0);
	if (rv.status)
	    	return rv.v0;
	return 0;
}


static inline int
ia64_sn_irtr_open(nasid_t nasid)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_IROUTER_OP, SAL_IROUTER_OPEN, nasid,
			   0, 0, 0, 0, 0);
	return (int) rv.v0;
}

static inline int
ia64_sn_irtr_close(nasid_t nasid, int subch)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_IROUTER_OP, SAL_IROUTER_CLOSE,
			   (u64) nasid, (u64) subch, 0, 0, 0, 0);
	return (int) rv.status;
}

static inline int
ia64_sn_irtr_recv(nasid_t nasid, int subch, char *buf, int *len)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_IROUTER_OP, SAL_IROUTER_RECV,
			   (u64) nasid, (u64) subch, (u64) buf, (u64) len,
			   0, 0);
	return (int) rv.status;
}

/*
 * Write data to the system controller network via the system
 * controller associated with 'nasid' on suchannel 'subch'.  The
 * buffer to be written out is pointed to by 'buf', and 'len' is the
 * number of bytes to be written.  The return value is either the
 * number of bytes written (which could be zero) or a negative error
 * code.
 */
static inline int
ia64_sn_irtr_send(nasid_t nasid, int subch, char *buf, int len)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_IROUTER_OP, SAL_IROUTER_SEND,
			   (u64) nasid, (u64) subch, (u64) buf, (u64) len,
			   0, 0);
	return (int) rv.v0;
}

static inline int
ia64_sn_irtr_intr(nasid_t nasid, int subch)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_IROUTER_OP, SAL_IROUTER_INTR_STATUS,
			   (u64) nasid, (u64) subch, 0, 0, 0, 0);
	return (int) rv.v0;
}

static inline int
ia64_sn_irtr_intr_enable(nasid_t nasid, int subch, u64 intr)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_IROUTER_OP, SAL_IROUTER_INTR_ON,
			   (u64) nasid, (u64) subch, intr, 0, 0, 0);
	return (int) rv.v0;
}

static inline int
ia64_sn_irtr_intr_disable(nasid_t nasid, int subch, u64 intr)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_IROUTER_OP, SAL_IROUTER_INTR_OFF,
			   (u64) nasid, (u64) subch, intr, 0, 0, 0);
	return (int) rv.v0;
}

static inline int
ia64_sn_sysctl_event_init(nasid_t nasid)
{
        struct ia64_sal_retval rv;
        SAL_CALL_REENTRANT(rv, SN_SAL_SYSCTL_EVENT, (u64) nasid,
			   0, 0, 0, 0, 0, 0);
        return (int) rv.v0;
}

static inline int
ia64_sn_sysctl_tio_clock_reset(nasid_t nasid)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_SYSCTL_OP, SAL_SYSCTL_OP_TIO_JLCK_RST,
			nasid, 0, 0, 0, 0, 0);
	if (rv.status != 0)
		return (int)rv.status;
	if (rv.v0 != 0)
		return (int)rv.v0;

	return 0;
}

static inline long
ia64_sn_sysctl_ioboard_get(nasid_t nasid, u16 *ioboard)
{
	struct ia64_sal_retval isrv;
	SAL_CALL_REENTRANT(isrv, SN_SAL_SYSCTL_OP, SAL_SYSCTL_OP_IOBOARD,
			   nasid, 0, 0, 0, 0, 0);
	if (isrv.v0 != 0) {
		*ioboard = isrv.v0;
		return isrv.status;
	}
	if (isrv.v1 != 0) {
		*ioboard = isrv.v1;
		return isrv.status;
	}

	return isrv.status;
}

static inline int
ia64_sn_get_fit_compt(u64 nasid, u64 index, void *fitentry, void *banbuf,
		      u64 banlen)
{
	struct ia64_sal_retval rv;
	SAL_CALL_NOLOCK(rv, SN_SAL_GET_FIT_COMPT, nasid, index, fitentry,
			banbuf, banlen, 0, 0);
	return (int) rv.status;
}

static inline int
ia64_sn_irtr_init(nasid_t nasid, void *buf, int len)
{
	struct ia64_sal_retval rv;
	SAL_CALL_REENTRANT(rv, SN_SAL_IROUTER_OP, SAL_IROUTER_INIT,
			   (u64) nasid, (u64) buf, (u64) len, 0, 0, 0);
	return (int) rv.status;
}

static inline u64
ia64_sn_get_sapic_info(int sapicid, int *nasid, int *subnode, int *slice)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_GET_SAPIC_INFO, sapicid, 0, 0, 0, 0, 0, 0);

	if (ret_stuff.status == SALRET_NOT_IMPLEMENTED) {
		if (nasid) *nasid = sapicid & 0xfff;
		if (subnode) *subnode = (sapicid >> 13) & 1;
		if (slice) *slice = (sapicid >> 12) & 3;
		return 0;
	}

	if (ret_stuff.status < 0)
		return ret_stuff.status;

	if (nasid) *nasid = (int) ret_stuff.v0;
	if (subnode) *subnode = (int) ret_stuff.v1;
	if (slice) *slice = (int) ret_stuff.v2;
	return 0;
}
 
static inline u64
ia64_sn_get_sn_info(int fc, u8 *shubtype, u16 *nasid_bitmask, u8 *nasid_shift, 
		u8 *systemsize, u8 *sharing_domain_size, u8 *partid, u8 *coher, u8 *reg)
{
	struct ia64_sal_retval ret_stuff;

	ret_stuff.status = 0;
	ret_stuff.v0 = 0;
	ret_stuff.v1 = 0;
	ret_stuff.v2 = 0;
	SAL_CALL_NOLOCK(ret_stuff, SN_SAL_GET_SN_INFO, fc, 0, 0, 0, 0, 0, 0);

	if (ret_stuff.status == SALRET_NOT_IMPLEMENTED) {
		int nasid = get_sapicid() & 0xfff;
#define SH_SHUB_ID_NODES_PER_BIT_MASK 0x001f000000000000UL
#define SH_SHUB_ID_NODES_PER_BIT_SHFT 48
		if (shubtype) *shubtype = 0;
		if (nasid_bitmask) *nasid_bitmask = 0x7ff;
		if (nasid_shift) *nasid_shift = 38;
		if (systemsize) *systemsize = 10;
		if (sharing_domain_size) *sharing_domain_size = 8;
		if (partid) *partid = ia64_sn_sysctl_partition_get(nasid);
		if (coher) *coher = nasid >> 9;
		if (reg) *reg = (HUB_L((u64 *) LOCAL_MMR_ADDR(SH1_SHUB_ID)) & SH_SHUB_ID_NODES_PER_BIT_MASK) >>
			SH_SHUB_ID_NODES_PER_BIT_SHFT;
		return 0;
	}

	if (ret_stuff.status < 0)
		return ret_stuff.status;

	if (shubtype) *shubtype = ret_stuff.v0 & 0xff;
	if (systemsize) *systemsize = (ret_stuff.v0 >> 8) & 0xff;
	if (sharing_domain_size) *sharing_domain_size = (ret_stuff.v0 >> 16) & 0xff;
	if (partid) *partid = (ret_stuff.v0 >> 24) & 0xff;
	if (coher) *coher = (ret_stuff.v0 >> 32) & 0xff;
	if (reg) *reg = (ret_stuff.v0 >> 40) & 0xff;
	if (nasid_bitmask) *nasid_bitmask = (ret_stuff.v1 & 0xffff);
	if (nasid_shift) *nasid_shift = (ret_stuff.v1 >> 16) & 0xff;
	return 0;
}
 
static inline int
ia64_sn_hwperf_op(nasid_t nasid, u64 opcode, u64 a0, u64 a1, u64 a2,
                  u64 a3, u64 a4, int *v0)
{
	struct ia64_sal_retval rv;
	SAL_CALL_NOLOCK(rv, SN_SAL_HWPERF_OP, (u64)nasid,
		opcode, a0, a1, a2, a3, a4);
	if (v0)
		*v0 = (int) rv.v0;
	return (int) rv.status;
}

static inline int
ia64_sn_ioif_get_pci_topology(u64 buf, u64 len)
{
	struct ia64_sal_retval rv;
	SAL_CALL_NOLOCK(rv, SN_SAL_IOIF_GET_PCI_TOPOLOGY, buf, len, 0, 0, 0, 0, 0);
	return (int) rv.status;
}

static inline int
ia64_sn_bte_recovery(nasid_t nasid)
{
	struct ia64_sal_retval rv;

	rv.status = 0;
	SAL_CALL_NOLOCK(rv, SN_SAL_BTE_RECOVER, (u64)nasid, 0, 0, 0, 0, 0, 0);
	if (rv.status == SALRET_NOT_IMPLEMENTED)
		return 0;
	return (int) rv.status;
}

static inline int
ia64_sn_is_fake_prom(void)
{
	struct ia64_sal_retval rv;
	SAL_CALL_NOLOCK(rv, SN_SAL_FAKE_PROM, 0, 0, 0, 0, 0, 0, 0);
	return (rv.status == 0);
}

static inline int
ia64_sn_get_prom_feature_set(int set, unsigned long *feature_set)
{
	struct ia64_sal_retval rv;

	SAL_CALL_NOLOCK(rv, SN_SAL_GET_PROM_FEATURE_SET, set, 0, 0, 0, 0, 0, 0);
	if (rv.status != 0)
		return rv.status;
	*feature_set = rv.v0;
	return 0;
}

static inline int
ia64_sn_set_os_feature(int feature)
{
	struct ia64_sal_retval rv;

	SAL_CALL_NOLOCK(rv, SN_SAL_SET_OS_FEATURE_SET, feature, 0, 0, 0, 0, 0, 0);
	return rv.status;
}

static inline int
sn_inject_error(u64 paddr, u64 *data, u64 *ecc)
{
	struct ia64_sal_retval ret_stuff;

	ia64_sal_oemcall_nolock(&ret_stuff, SN_SAL_INJECT_ERROR, paddr, (u64)data,
				(u64)ecc, 0, 0, 0, 0);
	return ret_stuff.status;
}

static inline int
ia64_sn_set_cpu_number(int cpu)
{
	struct ia64_sal_retval rv;

	SAL_CALL_NOLOCK(rv, SN_SAL_SET_CPU_NUMBER, cpu, 0, 0, 0, 0, 0, 0);
	return rv.status;
}
static inline int
ia64_sn_kernel_launch_event(void)
{
 	struct ia64_sal_retval rv;
	SAL_CALL_NOLOCK(rv, SN_SAL_KERNEL_LAUNCH_EVENT, 0, 0, 0, 0, 0, 0, 0);
	return rv.status;
}

union sn_watchlist_u {
	u64     val;
	struct {
		u64	blade	: 16,
			size	: 32,
			filler	: 16;
	};
};

static inline int
sn_mq_watchlist_alloc(int blade, void *mq, unsigned int mq_size,
				unsigned long *intr_mmr_offset)
{
	struct ia64_sal_retval rv;
	unsigned long addr;
	union sn_watchlist_u size_blade;
	int watchlist;

	addr = (unsigned long)mq;
	size_blade.size = mq_size;
	size_blade.blade = blade;

	ia64_sal_oemcall_nolock(&rv, SN_SAL_WATCHLIST_ALLOC, addr,
			size_blade.val, (u64)intr_mmr_offset,
			(u64)&watchlist, 0, 0, 0);
	if (rv.status < 0)
		return rv.status;

	return watchlist;
}

static inline int
sn_mq_watchlist_free(int blade, int watchlist_num)
{
	struct ia64_sal_retval rv;
	ia64_sal_oemcall_nolock(&rv, SN_SAL_WATCHLIST_FREE, blade,
			watchlist_num, 0, 0, 0, 0, 0);
	return rv.status;
}
#endif 
