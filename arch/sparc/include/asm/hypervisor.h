#ifndef _SPARC64_HYPERVISOR_H
#define _SPARC64_HYPERVISOR_H


#define HV_FAST_TRAP		0x80
#define HV_MMU_MAP_ADDR_TRAP	0x83
#define HV_MMU_UNMAP_ADDR_TRAP	0x84
#define HV_TTRACE_ADDENTRY_TRAP	0x85
#define HV_CORE_TRAP		0xff

#define HV_EOK				0  
#define HV_ENOCPU			1  
#define HV_ENORADDR			2  
#define HV_ENOINTR			3  
#define HV_EBADPGSZ			4  
#define HV_EBADTSB			5  
#define HV_EINVAL			6  
#define HV_EBADTRAP			7  
#define HV_EBADALIGN			8  
#define HV_EWOULDBLOCK			9  
#define HV_ENOACCESS			10 
#define HV_EIO				11 
#define HV_ECPUERROR			12 
#define HV_ENOTSUPPORTED		13 
#define HV_ENOMAP			14 
#define HV_ETOOMANY			15 
#define HV_ECHANNEL			16 
#define HV_EBUSY			17 

#define HV_FAST_MACH_EXIT		0x00

#ifndef __ASSEMBLY__
extern void sun4v_mach_exit(unsigned long exit_code);
#endif


#define HV_FAST_MACH_DESC		0x01

#ifndef __ASSEMBLY__
extern unsigned long sun4v_mach_desc(unsigned long buffer_pa,
				     unsigned long buf_len,
				     unsigned long *real_buf_len);
#endif

#define HV_FAST_MACH_SIR		0x02

#ifndef __ASSEMBLY__
extern void sun4v_mach_sir(void);
#endif

#define HV_FAST_MACH_SET_WATCHDOG	0x05

#ifndef __ASSEMBLY__
extern unsigned long sun4v_mach_set_watchdog(unsigned long timeout,
					     unsigned long *orig_timeout);
#endif


#define HV_FAST_CPU_START		0x10

#ifndef __ASSEMBLY__
extern unsigned long sun4v_cpu_start(unsigned long cpuid,
				     unsigned long pc,
				     unsigned long rtba,
				     unsigned long arg0);
#endif

#define HV_FAST_CPU_STOP		0x11

#ifndef __ASSEMBLY__
extern unsigned long sun4v_cpu_stop(unsigned long cpuid);
#endif

#define HV_FAST_CPU_YIELD		0x12

#ifndef __ASSEMBLY__
extern unsigned long sun4v_cpu_yield(void);
#endif

#define HV_FAST_CPU_QCONF		0x14
#define  HV_CPU_QUEUE_CPU_MONDO		 0x3c
#define  HV_CPU_QUEUE_DEVICE_MONDO	 0x3d
#define  HV_CPU_QUEUE_RES_ERROR		 0x3e
#define  HV_CPU_QUEUE_NONRES_ERROR	 0x3f

#ifndef __ASSEMBLY__
extern unsigned long sun4v_cpu_qconf(unsigned long type,
				     unsigned long queue_paddr,
				     unsigned long num_queue_entries);
#endif

#define HV_FAST_CPU_QINFO		0x15

#define HV_FAST_CPU_MONDO_SEND		0x42

#ifndef __ASSEMBLY__
extern unsigned long sun4v_cpu_mondo_send(unsigned long cpu_count, unsigned long cpu_list_pa, unsigned long mondo_block_pa);
#endif

#define HV_FAST_CPU_MYID		0x16

#define HV_FAST_CPU_STATE		0x17
#define  HV_CPU_STATE_STOPPED		 0x01
#define  HV_CPU_STATE_RUNNING		 0x02
#define  HV_CPU_STATE_ERROR		 0x03

#ifndef __ASSEMBLY__
extern long sun4v_cpu_state(unsigned long cpuid);
#endif

#define HV_FAST_CPU_SET_RTBA		0x18

#define HV_FAST_CPU_GET_RTBA		0x19

#ifndef __ASSEMBLY__
struct hv_tsb_descr {
	unsigned short		pgsz_idx;
	unsigned short		assoc;
	unsigned int		num_ttes;	
	unsigned int		ctx_idx;
	unsigned int		pgsz_mask;
	unsigned long		tsb_base;
	unsigned long		resv;
};
#endif
#define HV_TSB_DESCR_PGSZ_IDX_OFFSET	0x00
#define HV_TSB_DESCR_ASSOC_OFFSET	0x02
#define HV_TSB_DESCR_NUM_TTES_OFFSET	0x04
#define HV_TSB_DESCR_CTX_IDX_OFFSET	0x08
#define HV_TSB_DESCR_PGSZ_MASK_OFFSET	0x0c
#define HV_TSB_DESCR_TSB_BASE_OFFSET	0x10
#define HV_TSB_DESCR_RESV_OFFSET	0x18

#define HV_PGSZ_MASK_8K			(1 << 0)
#define HV_PGSZ_MASK_64K		(1 << 1)
#define HV_PGSZ_MASK_512K		(1 << 2)
#define HV_PGSZ_MASK_4MB		(1 << 3)
#define HV_PGSZ_MASK_32MB		(1 << 4)
#define HV_PGSZ_MASK_256MB		(1 << 5)
#define HV_PGSZ_MASK_2GB		(1 << 6)
#define HV_PGSZ_MASK_16GB		(1 << 7)

#define HV_PGSZ_IDX_8K			0
#define HV_PGSZ_IDX_64K			1
#define HV_PGSZ_IDX_512K		2
#define HV_PGSZ_IDX_4MB			3
#define HV_PGSZ_IDX_32MB		4
#define HV_PGSZ_IDX_256MB		5
#define HV_PGSZ_IDX_2GB			6
#define HV_PGSZ_IDX_16GB		7

#ifndef __ASSEMBLY__
struct hv_fault_status {
	unsigned long		i_fault_type;
	unsigned long		i_fault_addr;
	unsigned long		i_fault_ctx;
	unsigned long		i_reserved[5];
	unsigned long		d_fault_type;
	unsigned long		d_fault_addr;
	unsigned long		d_fault_ctx;
	unsigned long		d_reserved[5];
};
#endif
#define HV_FAULT_I_TYPE_OFFSET	0x00
#define HV_FAULT_I_ADDR_OFFSET	0x08
#define HV_FAULT_I_CTX_OFFSET	0x10
#define HV_FAULT_D_TYPE_OFFSET	0x40
#define HV_FAULT_D_ADDR_OFFSET	0x48
#define HV_FAULT_D_CTX_OFFSET	0x50

#define HV_FAULT_TYPE_FAST_MISS	1
#define HV_FAULT_TYPE_FAST_PROT	2
#define HV_FAULT_TYPE_MMU_MISS	3
#define HV_FAULT_TYPE_INV_RA	4
#define HV_FAULT_TYPE_PRIV_VIOL	5
#define HV_FAULT_TYPE_PROT_VIOL	6
#define HV_FAULT_TYPE_NFO	7
#define HV_FAULT_TYPE_NFO_SEFF	8
#define HV_FAULT_TYPE_INV_VA	9
#define HV_FAULT_TYPE_INV_ASI	10
#define HV_FAULT_TYPE_NC_ATOMIC	11
#define HV_FAULT_TYPE_PRIV_ACT	12
#define HV_FAULT_TYPE_RESV1	13
#define HV_FAULT_TYPE_UNALIGNED	14
#define HV_FAULT_TYPE_INV_PGSZ	15
#define HV_FAULT_TYPE_MULTIPLE	-1

#define HV_MMU_DMMU			0x01
#define HV_MMU_IMMU			0x02
#define HV_MMU_ALL			(HV_MMU_DMMU | HV_MMU_IMMU)



#define HV_FAST_MMU_TSB_CTX0		0x20

#ifndef __ASSEMBLY__
extern unsigned long sun4v_mmu_tsb_ctx0(unsigned long num_descriptions,
					unsigned long tsb_desc_ra);
#endif

#define HV_FAST_MMU_TSB_CTXNON0		0x21

#define HV_FAST_MMU_DEMAP_PAGE		0x22

#define HV_FAST_MMU_DEMAP_CTX		0x23

#define HV_FAST_MMU_DEMAP_ALL		0x24

#ifndef __ASSEMBLY__
extern void sun4v_mmu_demap_all(void);
#endif

#define HV_FAST_MMU_MAP_PERM_ADDR	0x25

#ifndef __ASSEMBLY__
extern unsigned long sun4v_mmu_map_perm_addr(unsigned long vaddr,
					     unsigned long set_to_zero,
					     unsigned long tte,
					     unsigned long flags);
#endif

#define HV_FAST_MMU_FAULT_AREA_CONF	0x26

#define HV_FAST_MMU_ENABLE		0x27

#define HV_FAST_MMU_UNMAP_PERM_ADDR	0x28

#define HV_FAST_MMU_TSB_CTX0_INFO	0x29

#define HV_FAST_MMU_TSB_CTXNON0_INFO	0x2a

#define HV_FAST_MMU_FAULT_AREA_INFO	0x2b


#define HV_FAST_MEM_SCRUB		0x31

#define HV_FAST_MEM_SYNC		0x32


#define HV_FAST_TOD_GET			0x50

#ifndef __ASSEMBLY__
extern unsigned long sun4v_tod_get(unsigned long *time);
#endif

#define HV_FAST_TOD_SET			0x51

#ifndef __ASSEMBLY__
extern unsigned long sun4v_tod_set(unsigned long time);
#endif


#define HV_FAST_CONS_GETCHAR		0x60

#define HV_FAST_CONS_PUTCHAR		0x61

#define HV_FAST_CONS_READ		0x62

/* con_write()
 * TRAP:	HV_FAST_TRAP
 * FUNCTION:	HV_FAST_CONS_WRITE
 * ARG0:	buffer real address
 * ARG1:	buffer size in bytes
 * RET0:	status
 * RET1:	bytes written
 * ERRORS:	EWOULDBLOCK	Output buffer currently full, would block
 *
 * Send a characters in buffer to the console device.  Breaks must be
 * sent using con_putchar().
 */
#define HV_FAST_CONS_WRITE		0x63

#ifndef __ASSEMBLY__
extern long sun4v_con_getchar(long *status);
extern long sun4v_con_putchar(long c);
extern long sun4v_con_read(unsigned long buffer,
			   unsigned long size,
			   unsigned long *bytes_read);
extern unsigned long sun4v_con_write(unsigned long buffer,
				     unsigned long size,
				     unsigned long *bytes_written);
#endif

#define HV_FAST_MACH_SET_SOFT_STATE	0x70
#define  HV_SOFT_STATE_NORMAL		 0x01
#define  HV_SOFT_STATE_TRANSITION	 0x02

#ifndef __ASSEMBLY__
extern unsigned long sun4v_mach_set_soft_state(unsigned long soft_state,
					       unsigned long msg_string_ra);
#endif

#define HV_FAST_MACH_GET_SOFT_STATE	0x71

#define HV_FAST_SVC_SEND		0x80

#define HV_FAST_SVC_RECV		0x81

#define HV_FAST_SVC_GETSTATUS		0x82

#define HV_FAST_SVC_SETSTATUS		0x83

#define HV_FAST_SVC_CLRSTATUS		0x84

#ifndef __ASSEMBLY__
extern unsigned long sun4v_svc_send(unsigned long svc_id,
				    unsigned long buffer,
				    unsigned long buffer_size,
				    unsigned long *sent_bytes);
extern unsigned long sun4v_svc_recv(unsigned long svc_id,
				    unsigned long buffer,
				    unsigned long buffer_size,
				    unsigned long *recv_bytes);
extern unsigned long sun4v_svc_getstatus(unsigned long svc_id,
					 unsigned long *status_bits);
extern unsigned long sun4v_svc_setstatus(unsigned long svc_id,
					 unsigned long status_bits);
extern unsigned long sun4v_svc_clrstatus(unsigned long svc_id,
					 unsigned long status_bits);
#endif

#ifndef __ASSEMBLY__
struct hv_trap_trace_control {
	unsigned long		head_offset;
	unsigned long		tail_offset;
	unsigned long		__reserved[0x30 / sizeof(unsigned long)];
};
#endif
#define HV_TRAP_TRACE_CTRL_HEAD_OFFSET	0x00
#define HV_TRAP_TRACE_CTRL_TAIL_OFFSET	0x08

/* The head offset is the offset of the most recently completed entry
 * in the trap-trace buffer.  The tail offset is the offset of the
 * next entry to be written.  The control structure is owned and
 * modified by the hypervisor.  A guest may not modify the control
 * structure contents.  Attempts to do so will result in undefined
 * behavior for the guest.
 *
 * Each trap trace buffer entry is laid out as follows:
 */
#ifndef __ASSEMBLY__
struct hv_trap_trace_entry {
	unsigned char	type;		
	unsigned char	hpstate;	
	unsigned char	tl;		
	unsigned char	gl;		
	unsigned short	tt;		
	unsigned short	tag;		
	unsigned long	tstate;		
	unsigned long	tick;		
	unsigned long	tpc;		
	unsigned long	f1;		
	unsigned long	f2;		
	unsigned long	f3;		
	unsigned long	f4;		
};
#endif
#define HV_TRAP_TRACE_ENTRY_TYPE	0x00
#define HV_TRAP_TRACE_ENTRY_HPSTATE	0x01
#define HV_TRAP_TRACE_ENTRY_TL		0x02
#define HV_TRAP_TRACE_ENTRY_GL		0x03
#define HV_TRAP_TRACE_ENTRY_TT		0x04
#define HV_TRAP_TRACE_ENTRY_TAG		0x06
#define HV_TRAP_TRACE_ENTRY_TSTATE	0x08
#define HV_TRAP_TRACE_ENTRY_TICK	0x10
#define HV_TRAP_TRACE_ENTRY_TPC		0x18
#define HV_TRAP_TRACE_ENTRY_F1		0x20
#define HV_TRAP_TRACE_ENTRY_F2		0x28
#define HV_TRAP_TRACE_ENTRY_F3		0x30
#define HV_TRAP_TRACE_ENTRY_F4		0x38

#define HV_TRAP_TYPE_UNDEF		0x00 
#define HV_TRAP_TYPE_HV			0x01 
#define HV_TRAP_TYPE_GUEST		0xff 

#define HV_FAST_TTRACE_BUF_CONF		0x90

#define HV_FAST_TTRACE_BUF_INFO		0x91

#define HV_FAST_TTRACE_ENABLE		0x92

#define HV_FAST_TTRACE_FREEZE		0x93



#define HV_FAST_DUMP_BUF_UPDATE		0x94

#define HV_FAST_DUMP_BUF_INFO		0x95


#define HV_INTR_STATE_IDLE		0 
#define HV_INTR_STATE_RECEIVED		1 
#define HV_INTR_STATE_DELIVERED		2 

#define HV_INTR_DISABLED		0 
#define HV_INTR_ENABLED			1 

#define HV_FAST_INTR_DEVINO2SYSINO	0xa0

#ifndef __ASSEMBLY__
extern unsigned long sun4v_devino_to_sysino(unsigned long devhandle,
					    unsigned long devino);
#endif

#define HV_FAST_INTR_GETENABLED		0xa1

#ifndef __ASSEMBLY__
extern unsigned long sun4v_intr_getenabled(unsigned long sysino);
#endif

#define HV_FAST_INTR_SETENABLED		0xa2

#ifndef __ASSEMBLY__
extern unsigned long sun4v_intr_setenabled(unsigned long sysino, unsigned long intr_enabled);
#endif

#define HV_FAST_INTR_GETSTATE		0xa3

#ifndef __ASSEMBLY__
extern unsigned long sun4v_intr_getstate(unsigned long sysino);
#endif

#define HV_FAST_INTR_SETSTATE		0xa4

#ifndef __ASSEMBLY__
extern unsigned long sun4v_intr_setstate(unsigned long sysino, unsigned long intr_state);
#endif

#define HV_FAST_INTR_GETTARGET		0xa5

#ifndef __ASSEMBLY__
extern unsigned long sun4v_intr_gettarget(unsigned long sysino);
#endif

#define HV_FAST_INTR_SETTARGET		0xa6

#ifndef __ASSEMBLY__
extern unsigned long sun4v_intr_settarget(unsigned long sysino, unsigned long cpuid);
#endif

#define HV_FAST_VINTR_GET_COOKIE	0xa7

#define HV_FAST_VINTR_SET_COOKIE	0xa8

#define HV_FAST_VINTR_GET_VALID		0xa9

#define HV_FAST_VINTR_SET_VALID		0xaa

#define HV_FAST_VINTR_GET_STATE		0xab

#define HV_FAST_VINTR_SET_STATE		0xac

#define HV_FAST_VINTR_GET_TARGET	0xad

#define HV_FAST_VINTR_SET_TARGET	0xae

#ifndef __ASSEMBLY__
extern unsigned long sun4v_vintr_get_cookie(unsigned long dev_handle,
					    unsigned long dev_ino,
					    unsigned long *cookie);
extern unsigned long sun4v_vintr_set_cookie(unsigned long dev_handle,
					    unsigned long dev_ino,
					    unsigned long cookie);
extern unsigned long sun4v_vintr_get_valid(unsigned long dev_handle,
					   unsigned long dev_ino,
					   unsigned long *valid);
extern unsigned long sun4v_vintr_set_valid(unsigned long dev_handle,
					   unsigned long dev_ino,
					   unsigned long valid);
extern unsigned long sun4v_vintr_get_state(unsigned long dev_handle,
					   unsigned long dev_ino,
					   unsigned long *state);
extern unsigned long sun4v_vintr_set_state(unsigned long dev_handle,
					   unsigned long dev_ino,
					   unsigned long state);
extern unsigned long sun4v_vintr_get_target(unsigned long dev_handle,
					    unsigned long dev_ino,
					    unsigned long *cpuid);
extern unsigned long sun4v_vintr_set_target(unsigned long dev_handle,
					    unsigned long dev_ino,
					    unsigned long cpuid);
#endif


#define HV_PCI_MAP_ATTR_READ		0x01
#define HV_PCI_MAP_ATTR_WRITE		0x02

#define HV_PCI_DEVICE_BUILD(b,d,f)	\
	((((b) & 0xff) << 16) | \
	 (((d) & 0x1f) << 11) | \
	 (((f) & 0x07) <<  8))

#define HV_PCI_TSBID(__tsb_num, __tsb_index) \
	((((u64)(__tsb_num)) << 32UL) | ((u64)(__tsb_index)))

#define HV_PCI_SYNC_FOR_DEVICE		0x01
#define HV_PCI_SYNC_FOR_CPU		0x02

#define HV_FAST_PCI_IOMMU_MAP		0xb0

#define HV_FAST_PCI_IOMMU_DEMAP		0xb1

#define HV_FAST_PCI_IOMMU_GETMAP	0xb2

#define HV_FAST_PCI_IOMMU_GETBYPASS	0xb3

#define HV_FAST_PCI_CONFIG_GET		0xb4

/* pci_config_put()
 * TRAP:	HV_FAST_TRAP
 * FUNCTION:	HV_FAST_PCI_CONFIG_PUT
 * ARG0:	devhandle
 * ARG1:	pci_device
 * ARG2:	pci_config_offset
 * ARG3:	size
 * ARG4:	data
 * RET0:	status
 * RET1:	error_flag
 * ERRORS:	EINVAL		Invalid devhandle/pci_device/offset/size
 *		EBADALIGN	pci_config_offset not size aligned
 *		ENOACCESS	Access to this offset is not permitted
 *
 * Write PCI configuration space for the adapter described by the given
 * devhandle.  Write size (1, 2, or 4) bytes of data in a single operation,
 * at pci_config_offset from the beginning of the device's configuration
 * space.  The data argument contains the data to be written to configuration
 * space.  Prior to writing, the data is size based byte swapped.
 *
 * If an error occurs during the write access, do not generate an error
 * report, do set RET1 to a non-zero value.  Otherwise RET1 is zero.
 * The given pci_config_offset must be 'size' aligned.
 *
 * This function is permitted to read from offset zero in the configuration
 * space described by the given pci_device if necessary to ensure that the
 * write access to config space completes.
 */
#define HV_FAST_PCI_CONFIG_PUT		0xb5

#define HV_FAST_PCI_PEEK		0xb6

/* pci_poke()
 * TRAP:	HV_FAST_TRAP
 * FUNCTION:	HV_FAST_PCI_POKE
 * ARG0:	devhandle
 * ARG1:	real address
 * ARG2:	size
 * ARG3:	data
 * ARG4:	pci_device
 * RET0:	status
 * RET1:	error_flag
 * ERRORS:	EINVAL		Invalid devhandle, size, or pci_device
 *		EBADALIGN	Improperly aligned real address
 *		ENORADDR	Bad real address
 *		ENOACCESS	Guest access prohibited
 *		ENOTSUPPORTED	Function is not supported by implementation
 *
 * Attempt to write data to the IO address given by the given devhandle,
 * real address, and size.  Size must be 1, 2, 4, or 8.  The write is
 * performed as a single access operation using the given size. Prior to
 * writing the data is size based swapped.
 *
 * If an error occurs when writing to the given location, do not generate an
 * error report, but return a non-zero value in RET1.  If the write was
 * successful, return zero in RET1.
 *
 * pci_device describes the configuration address of the device being
 * written to.  The implementation may safely read from offset 0 with
 * the configuration space of the device described by devhandle and
 * pci_device in order to guarantee that the write portion of the operation
 * completes
 *
 * Any error that occurs due to the read shall be reported using the normal
 * error reporting mechanisms .. the read error is not suppressed.
 *
 * The caller must have permission to write to the given devhandle, real
 * address, which must be an IO address.  The argument real address must be a
 * size aligned address.  The caller must have permission to read from
 * the given devhandle, pci_device cofiguration space offset 0.
 *
 * The hypervisor implementation of this function must block access to any
 * IO address that the guest does not have explicit permission to access.
 */
#define HV_FAST_PCI_POKE		0xb7

#define HV_FAST_PCI_DMA_SYNC		0xb8


#define HV_MSITYPE_MSI32		0x00
#define HV_MSITYPE_MSI64		0x01

#define HV_MSIQSTATE_IDLE		0x00
#define HV_MSIQSTATE_ERROR		0x01

#define HV_MSIQ_INVALID			0x00
#define HV_MSIQ_VALID			0x01

#define HV_MSISTATE_IDLE		0x00
#define HV_MSISTATE_DELIVERED		0x01

#define HV_MSIVALID_INVALID		0x00
#define HV_MSIVALID_VALID		0x01

#define HV_PCIE_MSGTYPE_PME_MSG		0x18
#define HV_PCIE_MSGTYPE_PME_ACK_MSG	0x1b
#define HV_PCIE_MSGTYPE_CORR_MSG	0x30
#define HV_PCIE_MSGTYPE_NONFATAL_MSG	0x31
#define HV_PCIE_MSGTYPE_FATAL_MSG	0x33

#define HV_MSG_INVALID			0x00
#define HV_MSG_VALID			0x01

#define HV_FAST_PCI_MSIQ_CONF		0xc0

#define HV_FAST_PCI_MSIQ_INFO		0xc1

#define HV_FAST_PCI_MSIQ_GETVALID	0xc2

#define HV_FAST_PCI_MSIQ_SETVALID	0xc3

#define HV_FAST_PCI_MSIQ_GETSTATE	0xc4

#define HV_FAST_PCI_MSIQ_SETSTATE	0xc5

#define HV_FAST_PCI_MSIQ_GETHEAD	0xc6

#define HV_FAST_PCI_MSIQ_SETHEAD	0xc7

#define HV_FAST_PCI_MSIQ_GETTAIL	0xc8

#define HV_FAST_PCI_MSI_GETVALID	0xc9

#define HV_FAST_PCI_MSI_SETVALID	0xca

#define HV_FAST_PCI_MSI_GETMSIQ		0xcb

#define HV_FAST_PCI_MSI_SETMSIQ		0xcc

#define HV_FAST_PCI_MSI_GETSTATE	0xcd

#define HV_FAST_PCI_MSI_SETSTATE	0xce

#define HV_FAST_PCI_MSG_GETMSIQ		0xd0

#define HV_FAST_PCI_MSG_SETMSIQ		0xd1

#define HV_FAST_PCI_MSG_GETVALID	0xd2

#define HV_FAST_PCI_MSG_SETVALID	0xd3


#define LDC_CHANNEL_DOWN		0
#define LDC_CHANNEL_UP			1
#define LDC_CHANNEL_RESETTING		2

#define HV_FAST_LDC_TX_QCONF		0xe0

#define HV_FAST_LDC_TX_QINFO		0xe1

#define HV_FAST_LDC_TX_GET_STATE	0xe2

#define HV_FAST_LDC_TX_SET_QTAIL	0xe3

#define HV_FAST_LDC_RX_QCONF		0xe4

#define HV_FAST_LDC_RX_QINFO		0xe5

#define HV_FAST_LDC_RX_GET_STATE	0xe6

#define HV_FAST_LDC_RX_SET_QHEAD	0xe7

#define LDC_MTE_PADDR	0x0fffffffffffe000 
#define LDC_MTE_COPY_W	0x0000000000000400 
#define LDC_MTE_COPY_R	0x0000000000000200 
#define LDC_MTE_IOMMU_W	0x0000000000000100 
#define LDC_MTE_IOMMU_R	0x0000000000000080 
#define LDC_MTE_EXEC	0x0000000000000040 
#define LDC_MTE_WRITE	0x0000000000000020 
#define LDC_MTE_READ	0x0000000000000010 
#define LDC_MTE_SZALL	0x000000000000000f 
#define LDC_MTE_SZ16GB	0x0000000000000007 
#define LDC_MTE_SZ2GB	0x0000000000000006 
#define LDC_MTE_SZ256MB	0x0000000000000005 
#define LDC_MTE_SZ32MB	0x0000000000000004 
#define LDC_MTE_SZ4MB	0x0000000000000003 
#define LDC_MTE_SZ512K	0x0000000000000002 
#define LDC_MTE_SZ64K	0x0000000000000001 
#define LDC_MTE_SZ8K	0x0000000000000000 

#ifndef __ASSEMBLY__
struct ldc_mtable_entry {
	unsigned long	mte;
	unsigned long	cookie;
};
#endif

#define HV_FAST_LDC_SET_MAP_TABLE	0xea

#define HV_FAST_LDC_GET_MAP_TABLE	0xeb

#define LDC_COPY_IN	0
#define LDC_COPY_OUT	1

#define HV_FAST_LDC_COPY		0xec

#define LDC_MEM_READ	1
#define LDC_MEM_WRITE	2
#define LDC_MEM_EXEC	4

#define HV_FAST_LDC_MAPIN		0xed

#define HV_FAST_LDC_UNMAP		0xee

#define HV_FAST_LDC_REVOKE		0xef

#ifndef __ASSEMBLY__
extern unsigned long sun4v_ldc_tx_qconf(unsigned long channel,
					unsigned long ra,
					unsigned long num_entries);
extern unsigned long sun4v_ldc_tx_qinfo(unsigned long channel,
					unsigned long *ra,
					unsigned long *num_entries);
extern unsigned long sun4v_ldc_tx_get_state(unsigned long channel,
					    unsigned long *head_off,
					    unsigned long *tail_off,
					    unsigned long *chan_state);
extern unsigned long sun4v_ldc_tx_set_qtail(unsigned long channel,
					    unsigned long tail_off);
extern unsigned long sun4v_ldc_rx_qconf(unsigned long channel,
					unsigned long ra,
					unsigned long num_entries);
extern unsigned long sun4v_ldc_rx_qinfo(unsigned long channel,
					unsigned long *ra,
					unsigned long *num_entries);
extern unsigned long sun4v_ldc_rx_get_state(unsigned long channel,
					    unsigned long *head_off,
					    unsigned long *tail_off,
					    unsigned long *chan_state);
extern unsigned long sun4v_ldc_rx_set_qhead(unsigned long channel,
					    unsigned long head_off);
extern unsigned long sun4v_ldc_set_map_table(unsigned long channel,
					     unsigned long ra,
					     unsigned long num_entries);
extern unsigned long sun4v_ldc_get_map_table(unsigned long channel,
					     unsigned long *ra,
					     unsigned long *num_entries);
extern unsigned long sun4v_ldc_copy(unsigned long channel,
				    unsigned long dir_code,
				    unsigned long tgt_raddr,
				    unsigned long lcl_raddr,
				    unsigned long len,
				    unsigned long *actual_len);
extern unsigned long sun4v_ldc_mapin(unsigned long channel,
				     unsigned long cookie,
				     unsigned long *ra,
				     unsigned long *perm);
extern unsigned long sun4v_ldc_unmap(unsigned long ra);
extern unsigned long sun4v_ldc_revoke(unsigned long channel,
				      unsigned long cookie,
				      unsigned long mte_cookie);
#endif


#define HV_PERF_JBUS_PERF_CTRL_REG	0x00
#define HV_PERF_JBUS_PERF_CNT_REG	0x01
#define HV_PERF_DRAM_PERF_CTRL_REG_0	0x02
#define HV_PERF_DRAM_PERF_CNT_REG_0	0x03
#define HV_PERF_DRAM_PERF_CTRL_REG_1	0x04
#define HV_PERF_DRAM_PERF_CNT_REG_1	0x05
#define HV_PERF_DRAM_PERF_CTRL_REG_2	0x06
#define HV_PERF_DRAM_PERF_CNT_REG_2	0x07
#define HV_PERF_DRAM_PERF_CTRL_REG_3	0x08
#define HV_PERF_DRAM_PERF_CNT_REG_3	0x09

#define HV_FAST_GET_PERFREG		0x100

#define HV_FAST_SET_PERFREG		0x101

#define HV_N2_PERF_SPARC_CTL		0x0
#define HV_N2_PERF_DRAM_CTL0		0x1
#define HV_N2_PERF_DRAM_CNT0		0x2
#define HV_N2_PERF_DRAM_CTL1		0x3
#define HV_N2_PERF_DRAM_CNT1		0x4
#define HV_N2_PERF_DRAM_CTL2		0x5
#define HV_N2_PERF_DRAM_CNT2		0x6
#define HV_N2_PERF_DRAM_CTL3		0x7
#define HV_N2_PERF_DRAM_CNT3		0x8

#define HV_FAST_N2_GET_PERFREG		0x104
#define HV_FAST_N2_SET_PERFREG		0x105

#ifndef __ASSEMBLY__
extern unsigned long sun4v_niagara_getperf(unsigned long reg,
					   unsigned long *val);
extern unsigned long sun4v_niagara_setperf(unsigned long reg,
					   unsigned long val);
extern unsigned long sun4v_niagara2_getperf(unsigned long reg,
					    unsigned long *val);
extern unsigned long sun4v_niagara2_setperf(unsigned long reg,
					    unsigned long val);
#endif

#ifndef __ASSEMBLY__
struct hv_mmu_statistics {
	unsigned long immu_tsb_hits_ctx0_8k_tte;
	unsigned long immu_tsb_ticks_ctx0_8k_tte;
	unsigned long immu_tsb_hits_ctx0_64k_tte;
	unsigned long immu_tsb_ticks_ctx0_64k_tte;
	unsigned long __reserved1[2];
	unsigned long immu_tsb_hits_ctx0_4mb_tte;
	unsigned long immu_tsb_ticks_ctx0_4mb_tte;
	unsigned long __reserved2[2];
	unsigned long immu_tsb_hits_ctx0_256mb_tte;
	unsigned long immu_tsb_ticks_ctx0_256mb_tte;
	unsigned long __reserved3[4];
	unsigned long immu_tsb_hits_ctxnon0_8k_tte;
	unsigned long immu_tsb_ticks_ctxnon0_8k_tte;
	unsigned long immu_tsb_hits_ctxnon0_64k_tte;
	unsigned long immu_tsb_ticks_ctxnon0_64k_tte;
	unsigned long __reserved4[2];
	unsigned long immu_tsb_hits_ctxnon0_4mb_tte;
	unsigned long immu_tsb_ticks_ctxnon0_4mb_tte;
	unsigned long __reserved5[2];
	unsigned long immu_tsb_hits_ctxnon0_256mb_tte;
	unsigned long immu_tsb_ticks_ctxnon0_256mb_tte;
	unsigned long __reserved6[4];
	unsigned long dmmu_tsb_hits_ctx0_8k_tte;
	unsigned long dmmu_tsb_ticks_ctx0_8k_tte;
	unsigned long dmmu_tsb_hits_ctx0_64k_tte;
	unsigned long dmmu_tsb_ticks_ctx0_64k_tte;
	unsigned long __reserved7[2];
	unsigned long dmmu_tsb_hits_ctx0_4mb_tte;
	unsigned long dmmu_tsb_ticks_ctx0_4mb_tte;
	unsigned long __reserved8[2];
	unsigned long dmmu_tsb_hits_ctx0_256mb_tte;
	unsigned long dmmu_tsb_ticks_ctx0_256mb_tte;
	unsigned long __reserved9[4];
	unsigned long dmmu_tsb_hits_ctxnon0_8k_tte;
	unsigned long dmmu_tsb_ticks_ctxnon0_8k_tte;
	unsigned long dmmu_tsb_hits_ctxnon0_64k_tte;
	unsigned long dmmu_tsb_ticks_ctxnon0_64k_tte;
	unsigned long __reserved10[2];
	unsigned long dmmu_tsb_hits_ctxnon0_4mb_tte;
	unsigned long dmmu_tsb_ticks_ctxnon0_4mb_tte;
	unsigned long __reserved11[2];
	unsigned long dmmu_tsb_hits_ctxnon0_256mb_tte;
	unsigned long dmmu_tsb_ticks_ctxnon0_256mb_tte;
	unsigned long __reserved12[4];
};
#endif

#define HV_FAST_MMUSTAT_CONF		0x102

#define HV_FAST_MMUSTAT_INFO		0x103

#ifndef __ASSEMBLY__
extern unsigned long sun4v_mmustat_conf(unsigned long ra, unsigned long *orig_ra);
extern unsigned long sun4v_mmustat_info(unsigned long *ra);
#endif


#define HV_NCS_QCONF			0x01
#define HV_NCS_QTAIL_UPDATE		0x02

#ifndef __ASSEMBLY__
struct hv_ncs_queue_entry {
	
	unsigned long	mau_control;
#define MAU_CONTROL_INV_PARITY	0x0000000000002000
#define MAU_CONTROL_STRAND	0x0000000000001800
#define MAU_CONTROL_BUSY	0x0000000000000400
#define MAU_CONTROL_INT		0x0000000000000200
#define MAU_CONTROL_OP		0x00000000000001c0
#define MAU_CONTROL_OP_SHIFT	6
#define MAU_OP_LOAD_MA_MEMORY	0x0
#define MAU_OP_STORE_MA_MEMORY	0x1
#define MAU_OP_MODULAR_MULT	0x2
#define MAU_OP_MODULAR_REDUCE	0x3
#define MAU_OP_MODULAR_EXP_LOOP	0x4
#define MAU_CONTROL_LEN		0x000000000000003f
#define MAU_CONTROL_LEN_SHIFT	0

	unsigned long	mau_mpa;

	
	unsigned long	mau_ma;

	
	unsigned long	mau_np;
};

struct hv_ncs_qconf_arg {
	unsigned long	mid;      
	unsigned long	base;     
	unsigned long	end;	  
	unsigned long	num_ents; 
};

struct hv_ncs_qtail_update_arg {
	unsigned long	mid;      
	unsigned long	tail;     
	unsigned long	syncflag; 
#define HV_NCS_SYNCFLAG_SYNC	0x00
#define HV_NCS_SYNCFLAG_ASYNC	0x01
};
#endif

#define HV_FAST_NCS_REQUEST		0x110

#ifndef __ASSEMBLY__
extern unsigned long sun4v_ncs_request(unsigned long request,
				       unsigned long arg_ra,
				       unsigned long arg_size);
#endif

#define HV_FAST_FIRE_GET_PERFREG	0x120
#define HV_FAST_FIRE_SET_PERFREG	0x121

#define HV_FAST_REBOOT_DATA_SET		0x172

#ifndef __ASSEMBLY__
extern unsigned long sun4v_reboot_data_set(unsigned long ra,
					   unsigned long len);
#endif

#define HV_CORE_SET_VER			0x00
#define HV_CORE_PUTCHAR			0x01
#define HV_CORE_EXIT			0x02
#define HV_CORE_GET_VER			0x03

#define HV_GRP_SUN4V			0x0000
#define HV_GRP_CORE			0x0001
#define HV_GRP_INTR			0x0002
#define HV_GRP_SOFT_STATE		0x0003
#define HV_GRP_TM			0x0080
#define HV_GRP_PCI			0x0100
#define HV_GRP_LDOM			0x0101
#define HV_GRP_SVC_CHAN			0x0102
#define HV_GRP_NCS			0x0103
#define HV_GRP_RNG			0x0104
#define HV_GRP_PBOOT			0x0105
#define HV_GRP_TPM			0x0107
#define HV_GRP_SDIO			0x0108
#define HV_GRP_SDIO_ERR			0x0109
#define HV_GRP_REBOOT_DATA		0x0110
#define HV_GRP_NIAG_PERF		0x0200
#define HV_GRP_FIRE_PERF		0x0201
#define HV_GRP_N2_CPU			0x0202
#define HV_GRP_NIU			0x0204
#define HV_GRP_VF_CPU			0x0205
#define HV_GRP_KT_CPU			0x0209
#define HV_GRP_DIAG			0x0300

#ifndef __ASSEMBLY__
extern unsigned long sun4v_get_version(unsigned long group,
				       unsigned long *major,
				       unsigned long *minor);
extern unsigned long sun4v_set_version(unsigned long group,
				       unsigned long major,
				       unsigned long minor,
				       unsigned long *actual_minor);

extern int sun4v_hvapi_register(unsigned long group, unsigned long major,
				unsigned long *minor);
extern void sun4v_hvapi_unregister(unsigned long group);
extern int sun4v_hvapi_get(unsigned long group,
			   unsigned long *major,
			   unsigned long *minor);
extern void sun4v_hvapi_init(void);
#endif

#endif 
