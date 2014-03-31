/******************************************************************************
 * This software may be used and distributed according to the terms of
 * the GNU General Public License (GPL), incorporated herein by reference.
 * Drivers based on or derived from this code fall under the GPL and must
 * retain the authorship, copyright and license notice.  This file is not
 * a complete program and may only be used when the entire operating
 * system is licensed under the GPL.
 * See the file COPYING in this distribution for more information.
 *
 * vxge-config.h: Driver for Exar Corp's X3100 Series 10GbE PCIe I/O
 *                Virtualized Server Adapter.
 * Copyright(c) 2002-2010 Exar Corp.
 ******************************************************************************/
#ifndef VXGE_CONFIG_H
#define VXGE_CONFIG_H
#include <linux/hardirq.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/io.h>

#ifndef VXGE_CACHE_LINE_SIZE
#define VXGE_CACHE_LINE_SIZE 128
#endif

#ifndef VXGE_ALIGN
#define VXGE_ALIGN(adrs, size) \
	(((size) - (((u64)adrs) & ((size)-1))) & ((size)-1))
#endif

#define VXGE_HW_MIN_MTU				68
#define VXGE_HW_MAX_MTU				9600
#define VXGE_HW_DEFAULT_MTU			1500

#define VXGE_HW_MAX_ROM_IMAGES			8

struct eprom_image {
	u8 is_valid:1;
	u8 index;
	u8 type;
	u16 version;
};

#ifdef VXGE_DEBUG_ASSERT
#define vxge_assert(test) BUG_ON(!(test))
#else
#define vxge_assert(test)
#endif 

enum vxge_debug_level {
	VXGE_NONE   = 0,
	VXGE_TRACE  = 1,
	VXGE_ERR    = 2
};

#define NULL_VPID					0xFFFFFFFF
#ifdef CONFIG_VXGE_DEBUG_TRACE_ALL
#define VXGE_DEBUG_MODULE_MASK  0xffffffff
#define VXGE_DEBUG_TRACE_MASK   0xffffffff
#define VXGE_DEBUG_ERR_MASK     0xffffffff
#define VXGE_DEBUG_MASK         0x000001ff
#else
#define VXGE_DEBUG_MODULE_MASK  0x20000000
#define VXGE_DEBUG_TRACE_MASK   0x20000000
#define VXGE_DEBUG_ERR_MASK     0x20000000
#define VXGE_DEBUG_MASK         0x00000001
#endif

#define	VXGE_COMPONENT_LL				0x20000000
#define	VXGE_COMPONENT_ALL				0xffffffff

#define VXGE_HW_BASE_INF	100
#define VXGE_HW_BASE_ERR	200
#define VXGE_HW_BASE_BADCFG	300

enum vxge_hw_status {
	VXGE_HW_OK				  = 0,
	VXGE_HW_FAIL				  = 1,
	VXGE_HW_PENDING				  = 2,
	VXGE_HW_COMPLETIONS_REMAIN		  = 3,

	VXGE_HW_INF_NO_MORE_COMPLETED_DESCRIPTORS = VXGE_HW_BASE_INF + 1,
	VXGE_HW_INF_OUT_OF_DESCRIPTORS		  = VXGE_HW_BASE_INF + 2,

	VXGE_HW_ERR_INVALID_HANDLE		  = VXGE_HW_BASE_ERR + 1,
	VXGE_HW_ERR_OUT_OF_MEMORY		  = VXGE_HW_BASE_ERR + 2,
	VXGE_HW_ERR_VPATH_NOT_AVAILABLE	  	  = VXGE_HW_BASE_ERR + 3,
	VXGE_HW_ERR_VPATH_NOT_OPEN		  = VXGE_HW_BASE_ERR + 4,
	VXGE_HW_ERR_WRONG_IRQ			  = VXGE_HW_BASE_ERR + 5,
	VXGE_HW_ERR_SWAPPER_CTRL		  = VXGE_HW_BASE_ERR + 6,
	VXGE_HW_ERR_INVALID_MTU_SIZE		  = VXGE_HW_BASE_ERR + 7,
	VXGE_HW_ERR_INVALID_INDEX		  = VXGE_HW_BASE_ERR + 8,
	VXGE_HW_ERR_INVALID_TYPE		  = VXGE_HW_BASE_ERR + 9,
	VXGE_HW_ERR_INVALID_OFFSET		  = VXGE_HW_BASE_ERR + 10,
	VXGE_HW_ERR_INVALID_DEVICE		  = VXGE_HW_BASE_ERR + 11,
	VXGE_HW_ERR_VERSION_CONFLICT		  = VXGE_HW_BASE_ERR + 12,
	VXGE_HW_ERR_INVALID_PCI_INFO		  = VXGE_HW_BASE_ERR + 13,
	VXGE_HW_ERR_INVALID_TCODE 		  = VXGE_HW_BASE_ERR + 14,
	VXGE_HW_ERR_INVALID_BLOCK_SIZE		  = VXGE_HW_BASE_ERR + 15,
	VXGE_HW_ERR_INVALID_STATE		  = VXGE_HW_BASE_ERR + 16,
	VXGE_HW_ERR_PRIVILAGED_OPEARATION	  = VXGE_HW_BASE_ERR + 17,
	VXGE_HW_ERR_INVALID_PORT 		  = VXGE_HW_BASE_ERR + 18,
	VXGE_HW_ERR_FIFO		 	  = VXGE_HW_BASE_ERR + 19,
	VXGE_HW_ERR_VPATH			  = VXGE_HW_BASE_ERR + 20,
	VXGE_HW_ERR_CRITICAL			  = VXGE_HW_BASE_ERR + 21,
	VXGE_HW_ERR_SLOT_FREEZE 		  = VXGE_HW_BASE_ERR + 22,

	VXGE_HW_BADCFG_RING_INDICATE_MAX_PKTS	  = VXGE_HW_BASE_BADCFG + 1,
	VXGE_HW_BADCFG_FIFO_BLOCKS		  = VXGE_HW_BASE_BADCFG + 2,
	VXGE_HW_BADCFG_VPATH_MTU		  = VXGE_HW_BASE_BADCFG + 3,
	VXGE_HW_BADCFG_VPATH_RPA_STRIP_VLAN_TAG	  = VXGE_HW_BASE_BADCFG + 4,
	VXGE_HW_BADCFG_VPATH_MIN_BANDWIDTH	  = VXGE_HW_BASE_BADCFG + 5,
	VXGE_HW_BADCFG_INTR_MODE		  = VXGE_HW_BASE_BADCFG + 6,
	VXGE_HW_BADCFG_RTS_MAC_EN		  = VXGE_HW_BASE_BADCFG + 7,

	VXGE_HW_EOF_TRACE_BUF			  = -1
};

enum vxge_hw_device_link_state {
	VXGE_HW_LINK_NONE,
	VXGE_HW_LINK_DOWN,
	VXGE_HW_LINK_UP
};

enum vxge_hw_fw_upgrade_code {
	VXGE_HW_FW_UPGRADE_OK		= 0,
	VXGE_HW_FW_UPGRADE_DONE		= 1,
	VXGE_HW_FW_UPGRADE_ERR		= 2,
	VXGE_FW_UPGRADE_BYTES2SKIP	= 3
};

enum vxge_hw_fw_upgrade_err_code {
	VXGE_HW_FW_UPGRADE_ERR_CORRUPT_DATA_1		= 1,
	VXGE_HW_FW_UPGRADE_ERR_BUFFER_OVERFLOW		= 2,
	VXGE_HW_FW_UPGRADE_ERR_INV_NCF_FILE_3		= 3,
	VXGE_HW_FW_UPGRADE_ERR_INV_NCF_FILE_4		= 4,
	VXGE_HW_FW_UPGRADE_ERR_INV_NCF_FILE_5		= 5,
	VXGE_HW_FW_UPGRADE_ERR_INV_NCF_FILE_6		= 6,
	VXGE_HW_FW_UPGRADE_ERR_CORRUPT_DATA_7		= 7,
	VXGE_HW_FW_UPGRADE_ERR_INV_NCF_FILE_8		= 8,
	VXGE_HW_FW_UPGRADE_ERR_GENERIC_ERROR_UNKNOWN	= 9,
	VXGE_HW_FW_UPGRADE_ERR_FAILED_TO_FLASH		= 10
};


#define VXGE_HW_FW_STRLEN	32
struct vxge_hw_device_date {
	u32     day;
	u32     month;
	u32     year;
	char    date[VXGE_HW_FW_STRLEN];
};

struct vxge_hw_device_version {
	u32     major;
	u32     minor;
	u32     build;
	char    version[VXGE_HW_FW_STRLEN];
};

struct vxge_hw_fifo_config {
	u32				enable;
#define VXGE_HW_FIFO_ENABLE				1
#define VXGE_HW_FIFO_DISABLE				0

	u32				fifo_blocks;
#define VXGE_HW_MIN_FIFO_BLOCKS				2
#define VXGE_HW_MAX_FIFO_BLOCKS				128

	u32				max_frags;
#define VXGE_HW_MIN_FIFO_FRAGS				1
#define VXGE_HW_MAX_FIFO_FRAGS				256

	u32				memblock_size;
#define VXGE_HW_MIN_FIFO_MEMBLOCK_SIZE			VXGE_HW_BLOCK_SIZE
#define VXGE_HW_MAX_FIFO_MEMBLOCK_SIZE			131072
#define VXGE_HW_DEF_FIFO_MEMBLOCK_SIZE			8096

	u32		                alignment_size;
#define VXGE_HW_MIN_FIFO_ALIGNMENT_SIZE		0
#define VXGE_HW_MAX_FIFO_ALIGNMENT_SIZE		65536
#define VXGE_HW_DEF_FIFO_ALIGNMENT_SIZE		VXGE_CACHE_LINE_SIZE

	u32		                intr;
#define VXGE_HW_FIFO_QUEUE_INTR_ENABLE			1
#define VXGE_HW_FIFO_QUEUE_INTR_DISABLE			0
#define VXGE_HW_FIFO_QUEUE_INTR_DEFAULT			0

	u32				no_snoop_bits;
#define VXGE_HW_FIFO_NO_SNOOP_DISABLED			0
#define VXGE_HW_FIFO_NO_SNOOP_TXD			1
#define VXGE_HW_FIFO_NO_SNOOP_FRM			2
#define VXGE_HW_FIFO_NO_SNOOP_ALL			3
#define VXGE_HW_FIFO_NO_SNOOP_DEFAULT			0

};
struct vxge_hw_ring_config {
	u32				enable;
#define VXGE_HW_RING_ENABLE					1
#define VXGE_HW_RING_DISABLE					0
#define VXGE_HW_RING_DEFAULT					1

	u32				ring_blocks;
#define VXGE_HW_MIN_RING_BLOCKS					1
#define VXGE_HW_MAX_RING_BLOCKS					128
#define VXGE_HW_DEF_RING_BLOCKS					2

	u32				buffer_mode;
#define VXGE_HW_RING_RXD_BUFFER_MODE_1				1
#define VXGE_HW_RING_RXD_BUFFER_MODE_3				3
#define VXGE_HW_RING_RXD_BUFFER_MODE_5				5
#define VXGE_HW_RING_RXD_BUFFER_MODE_DEFAULT			1

	u32				scatter_mode;
#define VXGE_HW_RING_SCATTER_MODE_A				0
#define VXGE_HW_RING_SCATTER_MODE_B				1
#define VXGE_HW_RING_SCATTER_MODE_C				2
#define VXGE_HW_RING_SCATTER_MODE_USE_FLASH_DEFAULT		0xffffffff

	u64				rxds_limit;
#define VXGE_HW_DEF_RING_RXDS_LIMIT				44
};

struct vxge_hw_vp_config {
	u32				vp_id;

#define	VXGE_HW_VPATH_PRIORITY_MIN			0
#define	VXGE_HW_VPATH_PRIORITY_MAX			16
#define	VXGE_HW_VPATH_PRIORITY_DEFAULT			0

	u32				min_bandwidth;
#define	VXGE_HW_VPATH_BANDWIDTH_MIN			0
#define	VXGE_HW_VPATH_BANDWIDTH_MAX			100
#define	VXGE_HW_VPATH_BANDWIDTH_DEFAULT			0

	struct vxge_hw_ring_config		ring;
	struct vxge_hw_fifo_config		fifo;
	struct vxge_hw_tim_intr_config	tti;
	struct vxge_hw_tim_intr_config	rti;

	u32				mtu;
#define VXGE_HW_VPATH_MIN_INITIAL_MTU			VXGE_HW_MIN_MTU
#define VXGE_HW_VPATH_MAX_INITIAL_MTU			VXGE_HW_MAX_MTU
#define VXGE_HW_VPATH_USE_FLASH_DEFAULT_INITIAL_MTU	0xffffffff

	u32				rpa_strip_vlan_tag;
#define VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_ENABLE			1
#define VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_DISABLE		0
#define VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_USE_FLASH_DEFAULT	0xffffffff

};
struct vxge_hw_device_config {
	u32					device_poll_millis;
#define VXGE_HW_MIN_DEVICE_POLL_MILLIS		1
#define VXGE_HW_MAX_DEVICE_POLL_MILLIS		100000
#define VXGE_HW_DEF_DEVICE_POLL_MILLIS		1000

	u32					dma_blockpool_initial;
	u32					dma_blockpool_max;
#define VXGE_HW_MIN_DMA_BLOCK_POOL_SIZE		0
#define VXGE_HW_INITIAL_DMA_BLOCK_POOL_SIZE	0
#define VXGE_HW_INCR_DMA_BLOCK_POOL_SIZE	4
#define VXGE_HW_MAX_DMA_BLOCK_POOL_SIZE		4096

#define	VXGE_HW_MAX_PAYLOAD_SIZE_512		2

	u32					intr_mode:2,
#define VXGE_HW_INTR_MODE_IRQLINE		0
#define VXGE_HW_INTR_MODE_MSIX			1
#define VXGE_HW_INTR_MODE_MSIX_ONE_SHOT		2

#define VXGE_HW_INTR_MODE_DEF			0

						rth_en:1,
#define VXGE_HW_RTH_DISABLE			0
#define VXGE_HW_RTH_ENABLE			1
#define VXGE_HW_RTH_DEFAULT			0

						rth_it_type:1,
#define VXGE_HW_RTH_IT_TYPE_SOLO_IT		0
#define VXGE_HW_RTH_IT_TYPE_MULTI_IT		1
#define VXGE_HW_RTH_IT_TYPE_DEFAULT		0

						rts_mac_en:1,
#define VXGE_HW_RTS_MAC_DISABLE			0
#define VXGE_HW_RTS_MAC_ENABLE			1
#define VXGE_HW_RTS_MAC_DEFAULT			0

						hwts_en:1;
#define	VXGE_HW_HWTS_DISABLE			0
#define	VXGE_HW_HWTS_ENABLE			1
#define	VXGE_HW_HWTS_DEFAULT			1

	struct vxge_hw_vp_config vp_config[VXGE_HW_MAX_VIRTUAL_PATHS];
};




struct vxge_hw_uld_cbs {
	void (*link_up)(struct __vxge_hw_device *devh);
	void (*link_down)(struct __vxge_hw_device *devh);
	void (*crit_err)(struct __vxge_hw_device *devh,
			enum vxge_hw_event type, u64 ext_data);
};

struct __vxge_hw_blockpool_entry {
	struct list_head	item;
	u32			length;
	void			*memblock;
	dma_addr_t		dma_addr;
	struct pci_dev 		*dma_handle;
	struct pci_dev 		*acc_handle;
};

struct __vxge_hw_blockpool {
	struct __vxge_hw_device *hldev;
	u32				block_size;
	u32				pool_size;
	u32				pool_max;
	u32				req_out;
	struct list_head		free_block_list;
	struct list_head		free_entry_list;
};

enum __vxge_hw_channel_type {
	VXGE_HW_CHANNEL_TYPE_UNKNOWN			= 0,
	VXGE_HW_CHANNEL_TYPE_FIFO			= 1,
	VXGE_HW_CHANNEL_TYPE_RING			= 2,
	VXGE_HW_CHANNEL_TYPE_MAX			= 3
};

struct __vxge_hw_channel {
	struct list_head		item;
	enum __vxge_hw_channel_type	type;
	struct __vxge_hw_device 	*devh;
	struct __vxge_hw_vpath_handle 	*vph;
	u32			length;
	u32			vp_id;
	void		**reserve_arr;
	u32			reserve_ptr;
	u32			reserve_top;
	void		**work_arr;
	u32			post_index ____cacheline_aligned;
	u32			compl_index ____cacheline_aligned;
	void		**free_arr;
	u32			free_ptr;
	void		**orig_arr;
	u32			per_dtr_space;
	void		*userdata;
	struct vxge_hw_common_reg	__iomem *common_reg;
	u32			first_vp_id;
	struct vxge_hw_vpath_stats_sw_common_info *stats;

} ____cacheline_aligned;

struct __vxge_hw_virtualpath {
	u32				vp_id;

	u32				vp_open;
#define VXGE_HW_VP_NOT_OPEN	0
#define	VXGE_HW_VP_OPEN		1

	struct __vxge_hw_device		*hldev;
	struct vxge_hw_vp_config	*vp_config;
	struct vxge_hw_vpath_reg	__iomem *vp_reg;
	struct vxge_hw_vpmgmt_reg	__iomem *vpmgmt_reg;
	struct __vxge_hw_non_offload_db_wrapper	__iomem *nofl_db;

	u32				max_mtu;
	u32				vsport_number;
	u32				max_kdfc_db;
	u32				max_nofl_db;
	u64				tim_tti_cfg1_saved;
	u64				tim_tti_cfg3_saved;
	u64				tim_rti_cfg1_saved;
	u64				tim_rti_cfg3_saved;

	struct __vxge_hw_ring *____cacheline_aligned ringh;
	struct __vxge_hw_fifo *____cacheline_aligned fifoh;
	struct list_head		vpath_handles;
	struct __vxge_hw_blockpool_entry		*stats_block;
	struct vxge_hw_vpath_stats_hw_info	*hw_stats;
	struct vxge_hw_vpath_stats_hw_info	*hw_stats_sav;
	struct vxge_hw_vpath_stats_sw_info	*sw_stats;
	spinlock_t lock;
};

struct __vxge_hw_vpath_handle {
	struct list_head	item;
	struct __vxge_hw_virtualpath	*vpath;
};

struct __vxge_hw_device {
	u32				magic;
#define VXGE_HW_DEVICE_MAGIC		0x12345678
#define VXGE_HW_DEVICE_DEAD		0xDEADDEAD
	void __iomem			*bar0;
	struct pci_dev			*pdev;
	struct net_device		*ndev;
	struct vxge_hw_device_config	config;
	enum vxge_hw_device_link_state	link_state;

	const struct vxge_hw_uld_cbs	*uld_callbacks;

	u32				host_type;
	u32				func_id;
	u32				access_rights;
#define VXGE_HW_DEVICE_ACCESS_RIGHT_VPATH      0x1
#define VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM     0x2
#define VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM     0x4
	struct vxge_hw_legacy_reg	__iomem *legacy_reg;
	struct vxge_hw_toc_reg		__iomem *toc_reg;
	struct vxge_hw_common_reg	__iomem *common_reg;
	struct vxge_hw_mrpcim_reg	__iomem *mrpcim_reg;
	struct vxge_hw_srpcim_reg	__iomem *srpcim_reg \
					[VXGE_HW_TITAN_SRPCIM_REG_SPACES];
	struct vxge_hw_vpmgmt_reg	__iomem *vpmgmt_reg \
					[VXGE_HW_TITAN_VPMGMT_REG_SPACES];
	struct vxge_hw_vpath_reg	__iomem *vpath_reg \
					[VXGE_HW_TITAN_VPATH_REG_SPACES];
	u8				__iomem *kdfc;
	u8				__iomem *usdc;
	struct __vxge_hw_virtualpath	virtual_paths \
					[VXGE_HW_MAX_VIRTUAL_PATHS];
	u64				vpath_assignments;
	u64				vpaths_deployed;
	u32				first_vp_id;
	u64				tim_int_mask0[4];
	u32				tim_int_mask1[4];

	struct __vxge_hw_blockpool	block_pool;
	struct vxge_hw_device_stats	stats;
	u32				debug_module_mask;
	u32				debug_level;
	u32				level_err;
	u32				level_trace;
	u16 eprom_versions[VXGE_HW_MAX_ROM_IMAGES];
};

#define VXGE_HW_INFO_LEN	64
struct vxge_hw_device_hw_info {
	u32		host_type;
#define VXGE_HW_NO_MR_NO_SR_NORMAL_FUNCTION			0
#define VXGE_HW_MR_NO_SR_VH0_BASE_FUNCTION			1
#define VXGE_HW_NO_MR_SR_VH0_FUNCTION0				2
#define VXGE_HW_NO_MR_SR_VH0_VIRTUAL_FUNCTION			3
#define VXGE_HW_MR_SR_VH0_INVALID_CONFIG			4
#define VXGE_HW_SR_VH_FUNCTION0					5
#define VXGE_HW_SR_VH_VIRTUAL_FUNCTION				6
#define VXGE_HW_VH_NORMAL_FUNCTION				7
	u64		function_mode;
#define VXGE_HW_FUNCTION_MODE_SINGLE_FUNCTION			0
#define VXGE_HW_FUNCTION_MODE_MULTI_FUNCTION			1
#define VXGE_HW_FUNCTION_MODE_SRIOV				2
#define VXGE_HW_FUNCTION_MODE_MRIOV				3
#define VXGE_HW_FUNCTION_MODE_MRIOV_8				4
#define VXGE_HW_FUNCTION_MODE_MULTI_FUNCTION_17			5
#define VXGE_HW_FUNCTION_MODE_SRIOV_8				6
#define VXGE_HW_FUNCTION_MODE_SRIOV_4				7
#define VXGE_HW_FUNCTION_MODE_MULTI_FUNCTION_2			8
#define VXGE_HW_FUNCTION_MODE_MULTI_FUNCTION_4			9
#define VXGE_HW_FUNCTION_MODE_MRIOV_4				10

	u32		func_id;
	u64		vpath_mask;
	struct vxge_hw_device_version fw_version;
	struct vxge_hw_device_date    fw_date;
	struct vxge_hw_device_version flash_version;
	struct vxge_hw_device_date    flash_date;
	u8		serial_number[VXGE_HW_INFO_LEN];
	u8		part_number[VXGE_HW_INFO_LEN];
	u8		product_desc[VXGE_HW_INFO_LEN];
	u8 mac_addrs[VXGE_HW_MAX_VIRTUAL_PATHS][ETH_ALEN];
	u8 mac_addr_masks[VXGE_HW_MAX_VIRTUAL_PATHS][ETH_ALEN];
};

struct vxge_hw_device_attr {
	void __iomem		*bar0;
	struct pci_dev 		*pdev;
	const struct vxge_hw_uld_cbs *uld_callbacks;
};

#define VXGE_HW_DEVICE_LINK_STATE_SET(hldev, ls)	(hldev->link_state = ls)

#define VXGE_HW_DEVICE_TIM_INT_MASK_SET(m0, m1, i) {	\
	if (i < 16) {				\
		m0[0] |= vxge_vBIT(0x8, (i*4), 4);	\
		m0[1] |= vxge_vBIT(0x4, (i*4), 4);	\
	}			       		\
	else {					\
		m1[0] = 0x80000000;		\
		m1[1] = 0x40000000;		\
	}					\
}

#define VXGE_HW_DEVICE_TIM_INT_MASK_RESET(m0, m1, i) {	\
	if (i < 16) {					\
		m0[0] &= ~vxge_vBIT(0x8, (i*4), 4);		\
		m0[1] &= ~vxge_vBIT(0x4, (i*4), 4);		\
	}						\
	else {						\
		m1[0] = 0;				\
		m1[1] = 0;				\
	}						\
}

#define VXGE_HW_DEVICE_STATS_PIO_READ(loc, offset) {		\
	status = vxge_hw_mrpcim_stats_access(hldev, \
				VXGE_HW_STATS_OP_READ, \
				loc, \
				offset, \
				&val64);			\
	if (status != VXGE_HW_OK)				\
		return status;						\
}

struct __vxge_hw_ring {
	struct __vxge_hw_channel		channel;
	struct vxge_hw_mempool			*mempool;
	struct vxge_hw_vpath_reg		__iomem	*vp_reg;
	struct vxge_hw_common_reg		__iomem	*common_reg;
	u32					ring_length;
	u32					buffer_mode;
	u32					rxd_size;
	u32					rxd_priv_size;
	u32					per_rxd_space;
	u32					rxds_per_block;
	u32					rxdblock_priv_size;
	u32					cmpl_cnt;
	u32					vp_id;
	u32					doorbell_cnt;
	u32					total_db_cnt;
	u64					rxds_limit;
	u32					rtimer;
	u64					tim_rti_cfg1_saved;
	u64					tim_rti_cfg3_saved;

	enum vxge_hw_status (*callback)(
			struct __vxge_hw_ring *ringh,
			void *rxdh,
			u8 t_code,
			void *userdata);

	enum vxge_hw_status (*rxd_init)(
			void *rxdh,
			void *userdata);

	void (*rxd_term)(
			void *rxdh,
			enum vxge_hw_rxd_state state,
			void *userdata);

	struct vxge_hw_vpath_stats_sw_ring_info *stats	____cacheline_aligned;
	struct vxge_hw_ring_config		*config;
} ____cacheline_aligned;

enum vxge_hw_txdl_state {
	VXGE_HW_TXDL_STATE_NONE	= 0,
	VXGE_HW_TXDL_STATE_AVAIL	= 1,
	VXGE_HW_TXDL_STATE_POSTED	= 2,
	VXGE_HW_TXDL_STATE_FREED	= 3
};
struct __vxge_hw_fifo {
	struct __vxge_hw_channel		channel;
	struct vxge_hw_mempool			*mempool;
	struct vxge_hw_fifo_config		*config;
	struct vxge_hw_vpath_reg		__iomem *vp_reg;
	struct __vxge_hw_non_offload_db_wrapper	__iomem *nofl_db;
	u64					interrupt_type;
	u32					no_snoop_bits;
	u32					txdl_per_memblock;
	u32					txdl_size;
	u32					priv_size;
	u32					per_txdl_space;
	u32					vp_id;
	u32					tx_intr_num;
	u32					rtimer;
	u64					tim_tti_cfg1_saved;
	u64					tim_tti_cfg3_saved;

	enum vxge_hw_status (*callback)(
			struct __vxge_hw_fifo *fifo_handle,
			void *txdlh,
			enum vxge_hw_fifo_tcode t_code,
			void *userdata,
			struct sk_buff ***skb_ptr,
			int nr_skb,
			int *more);

	void (*txdl_term)(
			void *txdlh,
			enum vxge_hw_txdl_state state,
			void *userdata);

	struct vxge_hw_vpath_stats_sw_fifo_info *stats ____cacheline_aligned;
} ____cacheline_aligned;

struct __vxge_hw_fifo_txdl_priv {
	dma_addr_t		dma_addr;
	struct pci_dev	*dma_handle;
	ptrdiff_t		dma_offset;
	u32				frags;
	u8				*align_vaddr_start;
	u8				*align_vaddr;
	dma_addr_t		align_dma_addr;
	struct pci_dev 	*align_dma_handle;
	struct pci_dev	*align_dma_acch;
	ptrdiff_t		align_dma_offset;
	u32				align_used_frags;
	u32				alloc_frags;
	u32				unused;
	struct __vxge_hw_fifo_txdl_priv	*next_txdl_priv;
	struct vxge_hw_fifo_txd		*first_txdp;
	void			*memblock;
};

/*
 * struct __vxge_hw_non_offload_db_wrapper - Non-offload Doorbell Wrapper
 * @control_0: Bits 0 to 7 - Doorbell type.
 *             Bits 8 to 31 - Reserved.
 *             Bits 32 to 39 - The highest TxD in this TxDL.
 *             Bits 40 to 47 - Reserved.
	*	       Bits 48 to 55 - Reserved.
 *             Bits 56 to 63 - No snoop flags.
 * @txdl_ptr:  The starting location of the TxDL in host memory.
 *
 * Created by the host and written to the adapter via PIO to a Kernel Doorbell
 * FIFO. All non-offload doorbell wrapper fields must be written by the host as
 * part of a doorbell write. Consumed by the adapter but is not written by the
 * adapter.
 */
struct __vxge_hw_non_offload_db_wrapper {
	u64		control_0;
#define	VXGE_HW_NODBW_GET_TYPE(ctrl0)			vxge_bVALn(ctrl0, 0, 8)
#define VXGE_HW_NODBW_TYPE(val) vxge_vBIT(val, 0, 8)
#define	VXGE_HW_NODBW_TYPE_NODBW				0

#define	VXGE_HW_NODBW_GET_LAST_TXD_NUMBER(ctrl0)	vxge_bVALn(ctrl0, 32, 8)
#define VXGE_HW_NODBW_LAST_TXD_NUMBER(val) vxge_vBIT(val, 32, 8)

#define	VXGE_HW_NODBW_GET_NO_SNOOP(ctrl0)		vxge_bVALn(ctrl0, 56, 8)
#define VXGE_HW_NODBW_LIST_NO_SNOOP(val) vxge_vBIT(val, 56, 8)
#define	VXGE_HW_NODBW_LIST_NO_SNOOP_TXD_READ_TXD0_WRITE		0x2
#define	VXGE_HW_NODBW_LIST_NO_SNOOP_TX_FRAME_DATA_READ		0x1

	u64		txdl_ptr;
};


/**
 * struct vxge_hw_fifo_txd - Transmit Descriptor
 * @control_0: Bits 0 to 6 - Reserved.
 *             Bit 7 - List Ownership. This field should be initialized
 *             to '1' by the driver before the transmit list pointer is
 *             written to the adapter. This field will be set to '0' by the
 *             adapter once it has completed transmitting the frame or frames in
 *             the list. Note - This field is only valid in TxD0. Additionally,
 *             for multi-list sequences, the driver should not release any
 *             buffers until the ownership of the last list in the multi-list
 *             sequence has been returned to the host.
 *             Bits 8 to 11 - Reserved
 *             Bits 12 to 15 - Transfer_Code. This field is only valid in
 *             TxD0. It is used to describe the status of the transmit data
 *             buffer transfer. This field is always overwritten by the
 *             adapter, so this field may be initialized to any value.
 *             Bits 16 to 17 - Host steering. This field allows the host to
 *             override the selection of the physical transmit port.
 *             Attention:
 *             Normal sounds as if learned from the switch rather than from
 *             the aggregation algorythms.
 *             00: Normal. Use Destination/MAC Address
 *             lookup to determine the transmit port.
 *             01: Send on physical Port1.
 *             10: Send on physical Port0.
 *	       11: Send on both ports.
 *             Bits 18 to 21 - Reserved
 *             Bits 22 to 23 - Gather_Code. This field is set by the host and
 *             is used to describe how individual buffers comprise a frame.
 *             10: First descriptor of a frame.
 *             00: Middle of a multi-descriptor frame.
 *             01: Last descriptor of a frame.
 *             11: First and last descriptor of a frame (the entire frame
 *             resides in a single buffer).
 *             For multi-descriptor frames, the only valid gather code sequence
 *             is {10, [00], 01}. In other words, the descriptors must be placed
 *             in the list in the correct order.
 *             Bits 24 to 27 - Reserved
 *             Bits 28 to 29 - LSO_Frm_Encap. LSO Frame Encapsulation
 *             definition. Only valid in TxD0. This field allows the host to
 *             indicate the Ethernet encapsulation of an outbound LSO packet.
 *             00 - classic mode (best guess)
 *             01 - LLC
 *             10 - SNAP
 *             11 - DIX
 *             If "classic mode" is selected, the adapter will attempt to
 *             decode the frame's Ethernet encapsulation by examining the L/T
 *             field as follows:
 *             <= 0x05DC LLC/SNAP encoding; must examine DSAP/SSAP to determine
 *             if packet is IPv4 or IPv6.
 *             0x8870 Jumbo-SNAP encoding.
 *             0x0800 IPv4 DIX encoding
 *             0x86DD IPv6 DIX encoding
 *             others illegal encapsulation
 *             Bits 30 - LSO_ Flag. Large Send Offload (LSO) flag.
 *             Set to 1 to perform segmentation offload for TCP/UDP.
 *             This field is valid only in TxD0.
 *             Bits 31 to 33 - Reserved.
 *             Bits 34 to 47 - LSO_MSS. TCP/UDP LSO Maximum Segment Size
 *             This field is meaningful only when LSO_Control is non-zero.
 *             When LSO_Control is set to TCP_LSO, the single (possibly large)
 *             TCP segment described by this TxDL will be sent as a series of
 *             TCP segments each of which contains no more than LSO_MSS
 *             payload bytes.
 *             When LSO_Control is set to UDP_LSO, the single (possibly large)
 *             UDP datagram described by this TxDL will be sent as a series of
 *             UDP datagrams each of which contains no more than LSO_MSS
 *             payload bytes.
 *             All outgoing frames from this TxDL will have LSO_MSS bytes of UDP
 *             or TCP payload, with the exception of the last, which will have
 *             <= LSO_MSS bytes of payload.
 *             Bits 48 to 63 - Buffer_Size. Number of valid bytes in the
 *             buffer to be read by the adapter. This field is written by the
 *             host. A value of 0 is illegal.
 *	       Bits 32 to 63 - This value is written by the adapter upon
 *	       completion of a UDP or TCP LSO operation and indicates the number
 *             of UDP or TCP payload bytes that were transmitted. 0x0000 will be
 *             returned for any non-LSO operation.
 * @control_1: Bits 0 to 4 - Reserved.
 *             Bit 5 - Tx_CKO_IPv4 Set to a '1' to enable IPv4 header checksum
 *             offload. This field is only valid in the first TxD of a frame.
 *             Bit 6 - Tx_CKO_TCP Set to a '1' to enable TCP checksum offload.
 *             This field is only valid in the first TxD of a frame (the TxD's
 *             gather code must be 10 or 11). The driver should only set this
 *             bit if it can guarantee that TCP is present.
 *             Bit 7 - Tx_CKO_UDP Set to a '1' to enable UDP checksum offload.
 *             This field is only valid in the first TxD of a frame (the TxD's
 *             gather code must be 10 or 11). The driver should only set this
 *             bit if it can guarantee that UDP is present.
 *             Bits 8 to 14 - Reserved.
 *             Bit 15 - Tx_VLAN_Enable VLAN tag insertion flag. Set to a '1' to
 *             instruct the adapter to insert the VLAN tag specified by the
 *             Tx_VLAN_Tag field. This field is only valid in the first TxD of
 *             a frame.
 *             Bits 16 to 31 - Tx_VLAN_Tag. Variable portion of the VLAN tag
 *             to be inserted into the frame by the adapter (the first two bytes
 *             of a VLAN tag are always 0x8100). This field is only valid if the
 *             Tx_VLAN_Enable field is set to '1'.
 *             Bits 32 to 33 - Reserved.
 *             Bits 34 to 39 - Tx_Int_Number. Indicates which Tx interrupt
 *             number the frame associated with. This field is written by the
 *             host. It is only valid in the first TxD of a frame.
 *             Bits 40 to 42 - Reserved.
 *             Bit 43 - Set to 1 to exclude the frame from bandwidth metering
 *             functions. This field is valid only in the first TxD
 *             of a frame.
 *             Bits 44 to 45 - Reserved.
 *             Bit 46 - Tx_Int_Per_List Set to a '1' to instruct the adapter to
 *             generate an interrupt as soon as all of the frames in the list
 *             have been transmitted. In order to have per-frame interrupts,
 *             the driver should place a maximum of one frame per list. This
 *             field is only valid in the first TxD of a frame.
 *             Bit 47 - Tx_Int_Utilization Set to a '1' to instruct the adapter
 *             to count the frame toward the utilization interrupt specified in
 *             the Tx_Int_Number field. This field is only valid in the first
 *             TxD of a frame.
 *             Bits 48 to 63 - Reserved.
 * @buffer_pointer: Buffer start address.
 * @host_control: Host_Control.Opaque 64bit data stored by driver inside the
 *            Titan descriptor prior to posting the latter on the fifo
 *            via vxge_hw_fifo_txdl_post().The %host_control is returned as is
 *            to the driver with each completed descriptor.
 *
 * Transmit descriptor (TxD).Fifo descriptor contains configured number
 * (list) of TxDs. * For more details please refer to Titan User Guide,
 * Section 5.4.2 "Transmit Descriptor (TxD) Format".
 */
struct vxge_hw_fifo_txd {
	u64 control_0;
#define VXGE_HW_FIFO_TXD_LIST_OWN_ADAPTER		vxge_mBIT(7)

#define VXGE_HW_FIFO_TXD_T_CODE_GET(ctrl0)		vxge_bVALn(ctrl0, 12, 4)
#define VXGE_HW_FIFO_TXD_T_CODE(val) 			vxge_vBIT(val, 12, 4)
#define VXGE_HW_FIFO_TXD_T_CODE_UNUSED		VXGE_HW_FIFO_T_CODE_UNUSED


#define VXGE_HW_FIFO_TXD_GATHER_CODE(val) 		vxge_vBIT(val, 22, 2)
#define VXGE_HW_FIFO_TXD_GATHER_CODE_FIRST	VXGE_HW_FIFO_GATHER_CODE_FIRST
#define VXGE_HW_FIFO_TXD_GATHER_CODE_LAST	VXGE_HW_FIFO_GATHER_CODE_LAST


#define VXGE_HW_FIFO_TXD_LSO_EN				vxge_mBIT(30)

#define VXGE_HW_FIFO_TXD_LSO_MSS(val) 			vxge_vBIT(val, 34, 14)

#define VXGE_HW_FIFO_TXD_BUFFER_SIZE(val) 		vxge_vBIT(val, 48, 16)

	u64 control_1;
#define VXGE_HW_FIFO_TXD_TX_CKO_IPV4_EN			vxge_mBIT(5)
#define VXGE_HW_FIFO_TXD_TX_CKO_TCP_EN			vxge_mBIT(6)
#define VXGE_HW_FIFO_TXD_TX_CKO_UDP_EN			vxge_mBIT(7)
#define VXGE_HW_FIFO_TXD_VLAN_ENABLE			vxge_mBIT(15)

#define VXGE_HW_FIFO_TXD_VLAN_TAG(val) 			vxge_vBIT(val, 16, 16)

#define VXGE_HW_FIFO_TXD_INT_NUMBER(val) 		vxge_vBIT(val, 34, 6)

#define VXGE_HW_FIFO_TXD_INT_TYPE_PER_LIST		vxge_mBIT(46)
#define VXGE_HW_FIFO_TXD_INT_TYPE_UTILZ			vxge_mBIT(47)

	u64 buffer_pointer;

	u64 host_control;
};

/**
 * struct vxge_hw_ring_rxd_1 - One buffer mode RxD for ring
 * @host_control: This field is exclusively for host use and is "readonly"
 *             from the adapter's perspective.
 * @control_0:Bits 0 to 6 - RTH_Bucket get
 *	      Bit 7 - Own Descriptor ownership bit. This bit is set to 1
 *            by the host, and is set to 0 by the adapter.
 *	      0 - Host owns RxD and buffer.
 *	      1 - The adapter owns RxD and buffer.
 *	      Bit 8 - Fast_Path_Eligible When set, indicates that the
 *            received frame meets all of the criteria for fast path processing.
 *            The required criteria are as follows:
 *            !SYN &
 *            (Transfer_Code == "Transfer OK") &
 *            (!Is_IP_Fragment) &
 *            ((Is_IPv4 & computed_L3_checksum == 0xFFFF) |
 *            (Is_IPv6)) &
 *            ((Is_TCP & computed_L4_checksum == 0xFFFF) |
 *            (Is_UDP & (computed_L4_checksum == 0xFFFF |
 *            computed _L4_checksum == 0x0000)))
 *            (same meaning for all RxD buffer modes)
 *	      Bit 9 - L3 Checksum Correct
 *	      Bit 10 - L4 Checksum Correct
 *	      Bit 11 - Reserved
 *	      Bit 12 to 15 - This field is written by the adapter. It is
 *            used to report the status of the frame transfer to the host.
 *	      0x0 - Transfer OK
 *	      0x4 - RDA Failure During Transfer
 *	      0x5 - Unparseable Packet, such as unknown IPv6 header.
 *	      0x6 - Frame integrity error (FCS or ECC).
 *	      0x7 - Buffer Size Error. The provided buffer(s) were not
 *                  appropriately sized and data loss occurred.
 *	      0x8 - Internal ECC Error. RxD corrupted.
 *	      0x9 - IPv4 Checksum error
 *	      0xA - TCP/UDP Checksum error
 *	      0xF - Unknown Error or Multiple Error. Indicates an
 *               unknown problem or that more than one of transfer codes is set.
 *	      Bit 16 - SYN The adapter sets this field to indicate that
 *                the incoming frame contained a TCP segment with its SYN bit
 *	          set and its ACK bit NOT set. (same meaning for all RxD buffer
 *                modes)
 *	      Bit 17 - Is ICMP
 *	      Bit 18 - RTH_SPDM_HIT Set to 1 if there was a match in the
 *                Socket Pair Direct Match Table and the frame was steered based
 *                on SPDM.
 *	      Bit 19 - RTH_IT_HIT Set to 1 if there was a match in the
 *            Indirection Table and the frame was steered based on hash
 *            indirection.
 *	      Bit 20 to 23 - RTH_HASH_TYPE Indicates the function (hash
 *	          type) that was used to calculate the hash.
 *	      Bit 19 - IS_VLAN Set to '1' if the frame was/is VLAN
 *	          tagged.
 *	      Bit 25 to 26 - ETHER_ENCAP Reflects the Ethernet encapsulation
 *                of the received frame.
 *	      0x0 - Ethernet DIX
 *	      0x1 - LLC
 *	      0x2 - SNAP (includes Jumbo-SNAP)
 *	      0x3 - IPX
 *	      Bit 27 - IS_IPV4 Set to '1' if the frame contains an IPv4	packet.
 *	      Bit 28 - IS_IPV6 Set to '1' if the frame contains an IPv6 packet.
 *	      Bit 29 - IS_IP_FRAG Set to '1' if the frame contains a fragmented
 *            IP packet.
 *	      Bit 30 - IS_TCP Set to '1' if the frame contains a TCP segment.
 *	      Bit 31 - IS_UDP Set to '1' if the frame contains a UDP message.
 *	      Bit 32 to 47 - L3_Checksum[0:15] The IPv4 checksum value 	that
 *            arrived with the frame. If the resulting computed IPv4 header
 *            checksum for the frame did not produce the expected 0xFFFF value,
 *            then the transfer code would be set to 0x9.
 *	      Bit 48 to 63 - L4_Checksum[0:15] The TCP/UDP checksum value that
 *            arrived with the frame. If the resulting computed TCP/UDP checksum
 *            for the frame did not produce the expected 0xFFFF value, then the
 *            transfer code would be set to 0xA.
 * @control_1:Bits 0 to 1 - Reserved
 *            Bits 2 to 15 - Buffer0_Size.This field is set by the host and
 *            eventually overwritten by the adapter. The host writes the
 *            available buffer size in bytes when it passes the descriptor to
 *            the adapter. When a frame is delivered the host, the adapter
 *            populates this field with the number of bytes written into the
 *            buffer. The largest supported buffer is 16, 383 bytes.
 *	      Bit 16 to 47 - RTH Hash Value 32-bit RTH hash value. Only valid if
 *	      RTH_HASH_TYPE (Control_0, bits 20:23) is nonzero.
 *	      Bit 48 to 63 - VLAN_Tag[0:15] The contents of the variable portion
 *            of the VLAN tag, if one was detected by the adapter. This field is
 *            populated even if VLAN-tag stripping is enabled.
 * @buffer0_ptr: Pointer to buffer. This field is populated by the driver.
 *
 * One buffer mode RxD for ring structure
 */
struct vxge_hw_ring_rxd_1 {
	u64 host_control;
	u64 control_0;
#define VXGE_HW_RING_RXD_RTH_BUCKET_GET(ctrl0)		vxge_bVALn(ctrl0, 0, 7)

#define VXGE_HW_RING_RXD_LIST_OWN_ADAPTER		vxge_mBIT(7)

#define VXGE_HW_RING_RXD_FAST_PATH_ELIGIBLE_GET(ctrl0)	vxge_bVALn(ctrl0, 8, 1)

#define VXGE_HW_RING_RXD_L3_CKSUM_CORRECT_GET(ctrl0)	vxge_bVALn(ctrl0, 9, 1)

#define VXGE_HW_RING_RXD_L4_CKSUM_CORRECT_GET(ctrl0)	vxge_bVALn(ctrl0, 10, 1)

#define VXGE_HW_RING_RXD_T_CODE_GET(ctrl0)		vxge_bVALn(ctrl0, 12, 4)
#define VXGE_HW_RING_RXD_T_CODE(val) 			vxge_vBIT(val, 12, 4)

#define VXGE_HW_RING_RXD_T_CODE_UNUSED		VXGE_HW_RING_T_CODE_UNUSED

#define VXGE_HW_RING_RXD_SYN_GET(ctrl0)		vxge_bVALn(ctrl0, 16, 1)

#define VXGE_HW_RING_RXD_IS_ICMP_GET(ctrl0)		vxge_bVALn(ctrl0, 17, 1)

#define VXGE_HW_RING_RXD_RTH_SPDM_HIT_GET(ctrl0)	vxge_bVALn(ctrl0, 18, 1)

#define VXGE_HW_RING_RXD_RTH_IT_HIT_GET(ctrl0)		vxge_bVALn(ctrl0, 19, 1)

#define VXGE_HW_RING_RXD_RTH_HASH_TYPE_GET(ctrl0)	vxge_bVALn(ctrl0, 20, 4)

#define VXGE_HW_RING_RXD_IS_VLAN_GET(ctrl0)		vxge_bVALn(ctrl0, 24, 1)

#define VXGE_HW_RING_RXD_ETHER_ENCAP_GET(ctrl0)		vxge_bVALn(ctrl0, 25, 2)

#define VXGE_HW_RING_RXD_FRAME_PROTO_GET(ctrl0)		vxge_bVALn(ctrl0, 27, 5)

#define VXGE_HW_RING_RXD_L3_CKSUM_GET(ctrl0)	vxge_bVALn(ctrl0, 32, 16)

#define VXGE_HW_RING_RXD_L4_CKSUM_GET(ctrl0)	vxge_bVALn(ctrl0, 48, 16)

	u64 control_1;

#define VXGE_HW_RING_RXD_1_BUFFER0_SIZE_GET(ctrl1)	vxge_bVALn(ctrl1, 2, 14)
#define VXGE_HW_RING_RXD_1_BUFFER0_SIZE(val) vxge_vBIT(val, 2, 14)
#define VXGE_HW_RING_RXD_1_BUFFER0_SIZE_MASK		vxge_vBIT(0x3FFF, 2, 14)

#define VXGE_HW_RING_RXD_1_RTH_HASH_VAL_GET(ctrl1)    vxge_bVALn(ctrl1, 16, 32)

#define VXGE_HW_RING_RXD_VLAN_TAG_GET(ctrl1)	vxge_bVALn(ctrl1, 48, 16)

	u64 buffer0_ptr;
};

enum vxge_hw_rth_algoritms {
	RTH_ALG_JENKINS = 0,
	RTH_ALG_MS_RSS	= 1,
	RTH_ALG_CRC32C	= 2
};

struct vxge_hw_rth_hash_types {
	u8 hash_type_tcpipv4_en:1,
	   hash_type_ipv4_en:1,
	   hash_type_tcpipv6_en:1,
	   hash_type_ipv6_en:1,
	   hash_type_tcpipv6ex_en:1,
	   hash_type_ipv6ex_en:1;
};

void vxge_hw_device_debug_set(
	struct __vxge_hw_device *devh,
	enum vxge_debug_level level,
	u32 mask);

u32
vxge_hw_device_error_level_get(struct __vxge_hw_device *devh);

u32
vxge_hw_device_trace_level_get(struct __vxge_hw_device *devh);

static inline u32 vxge_hw_ring_rxd_size_get(u32 buf_mode)
{
	return sizeof(struct vxge_hw_ring_rxd_1);
}

static inline u32 vxge_hw_ring_rxds_per_block_get(u32 buf_mode)
{
	return (u32)((VXGE_HW_BLOCK_SIZE-16) /
		sizeof(struct vxge_hw_ring_rxd_1));
}

static inline
void vxge_hw_ring_rxd_1b_set(
	void *rxdh,
	dma_addr_t dma_pointer,
	u32 size)
{
	struct vxge_hw_ring_rxd_1 *rxdp = (struct vxge_hw_ring_rxd_1 *)rxdh;
	rxdp->buffer0_ptr = dma_pointer;
	rxdp->control_1	&= ~VXGE_HW_RING_RXD_1_BUFFER0_SIZE_MASK;
	rxdp->control_1	|= VXGE_HW_RING_RXD_1_BUFFER0_SIZE(size);
}

static inline
void vxge_hw_ring_rxd_1b_get(
	struct __vxge_hw_ring *ring_handle,
	void *rxdh,
	u32 *pkt_length)
{
	struct vxge_hw_ring_rxd_1 *rxdp = (struct vxge_hw_ring_rxd_1 *)rxdh;

	*pkt_length =
		(u32)VXGE_HW_RING_RXD_1_BUFFER0_SIZE_GET(rxdp->control_1);
}

static inline
void vxge_hw_ring_rxd_1b_info_get(
	struct __vxge_hw_ring *ring_handle,
	void *rxdh,
	struct vxge_hw_ring_rxd_info *rxd_info)
{

	struct vxge_hw_ring_rxd_1 *rxdp = (struct vxge_hw_ring_rxd_1 *)rxdh;
	rxd_info->syn_flag =
		(u32)VXGE_HW_RING_RXD_SYN_GET(rxdp->control_0);
	rxd_info->is_icmp =
		(u32)VXGE_HW_RING_RXD_IS_ICMP_GET(rxdp->control_0);
	rxd_info->fast_path_eligible =
		(u32)VXGE_HW_RING_RXD_FAST_PATH_ELIGIBLE_GET(rxdp->control_0);
	rxd_info->l3_cksum_valid =
		(u32)VXGE_HW_RING_RXD_L3_CKSUM_CORRECT_GET(rxdp->control_0);
	rxd_info->l3_cksum =
		(u32)VXGE_HW_RING_RXD_L3_CKSUM_GET(rxdp->control_0);
	rxd_info->l4_cksum_valid =
		(u32)VXGE_HW_RING_RXD_L4_CKSUM_CORRECT_GET(rxdp->control_0);
	rxd_info->l4_cksum =
		(u32)VXGE_HW_RING_RXD_L4_CKSUM_GET(rxdp->control_0);
	rxd_info->frame =
		(u32)VXGE_HW_RING_RXD_ETHER_ENCAP_GET(rxdp->control_0);
	rxd_info->proto =
		(u32)VXGE_HW_RING_RXD_FRAME_PROTO_GET(rxdp->control_0);
	rxd_info->is_vlan =
		(u32)VXGE_HW_RING_RXD_IS_VLAN_GET(rxdp->control_0);
	rxd_info->vlan =
		(u32)VXGE_HW_RING_RXD_VLAN_TAG_GET(rxdp->control_1);
	rxd_info->rth_bucket =
		(u32)VXGE_HW_RING_RXD_RTH_BUCKET_GET(rxdp->control_0);
	rxd_info->rth_it_hit =
		(u32)VXGE_HW_RING_RXD_RTH_IT_HIT_GET(rxdp->control_0);
	rxd_info->rth_spdm_hit =
		(u32)VXGE_HW_RING_RXD_RTH_SPDM_HIT_GET(rxdp->control_0);
	rxd_info->rth_hash_type =
		(u32)VXGE_HW_RING_RXD_RTH_HASH_TYPE_GET(rxdp->control_0);
	rxd_info->rth_value =
		(u32)VXGE_HW_RING_RXD_1_RTH_HASH_VAL_GET(rxdp->control_1);
}

static inline void *vxge_hw_ring_rxd_private_get(void *rxdh)
{
	struct vxge_hw_ring_rxd_1 *rxdp = (struct vxge_hw_ring_rxd_1 *)rxdh;
	return (void *)(size_t)rxdp->host_control;
}

static inline void vxge_hw_fifo_txdl_cksum_set_bits(void *txdlh, u64 cksum_bits)
{
	struct vxge_hw_fifo_txd *txdp = (struct vxge_hw_fifo_txd *)txdlh;
	txdp->control_1 |= cksum_bits;
}

static inline void vxge_hw_fifo_txdl_mss_set(void *txdlh, int mss)
{
	struct vxge_hw_fifo_txd *txdp = (struct vxge_hw_fifo_txd *)txdlh;

	txdp->control_0 |= VXGE_HW_FIFO_TXD_LSO_EN;
	txdp->control_0 |= VXGE_HW_FIFO_TXD_LSO_MSS(mss);
}

static inline void vxge_hw_fifo_txdl_vlan_set(void *txdlh, u16 vlan_tag)
{
	struct vxge_hw_fifo_txd *txdp = (struct vxge_hw_fifo_txd *)txdlh;

	txdp->control_1 |= VXGE_HW_FIFO_TXD_VLAN_ENABLE;
	txdp->control_1 |= VXGE_HW_FIFO_TXD_VLAN_TAG(vlan_tag);
}

static inline void *vxge_hw_fifo_txdl_private_get(void *txdlh)
{
	struct vxge_hw_fifo_txd *txdp  = (struct vxge_hw_fifo_txd *)txdlh;

	return (void *)(size_t)txdp->host_control;
}

struct vxge_hw_ring_attr {
	enum vxge_hw_status (*callback)(
			struct __vxge_hw_ring *ringh,
			void *rxdh,
			u8 t_code,
			void *userdata);

	enum vxge_hw_status (*rxd_init)(
			void *rxdh,
			void *userdata);

	void (*rxd_term)(
			void *rxdh,
			enum vxge_hw_rxd_state state,
			void *userdata);

	void		*userdata;
	u32		per_rxd_space;
};

struct vxge_hw_fifo_attr {

	enum vxge_hw_status (*callback)(
			struct __vxge_hw_fifo *fifo_handle,
			void *txdlh,
			enum vxge_hw_fifo_tcode t_code,
			void *userdata,
			struct sk_buff ***skb_ptr,
			int nr_skb, int *more);

	void (*txdl_term)(
			void *txdlh,
			enum vxge_hw_txdl_state state,
			void *userdata);

	void		*userdata;
	u32		per_txdl_space;
};

struct vxge_hw_vpath_attr {
	u32				vp_id;
	struct vxge_hw_ring_attr	ring_attr;
	struct vxge_hw_fifo_attr	fifo_attr;
};

enum vxge_hw_status __devinit vxge_hw_device_hw_info_get(
	void __iomem *bar0,
	struct vxge_hw_device_hw_info *hw_info);

enum vxge_hw_status __devinit vxge_hw_device_config_default_get(
	struct vxge_hw_device_config *device_config);

static inline
enum vxge_hw_device_link_state vxge_hw_device_link_state_get(
	struct __vxge_hw_device *devh)
{
	return devh->link_state;
}

void vxge_hw_device_terminate(struct __vxge_hw_device *devh);

const u8 *
vxge_hw_device_serial_number_get(struct __vxge_hw_device *devh);

u16 vxge_hw_device_link_width_get(struct __vxge_hw_device *devh);

const u8 *
vxge_hw_device_product_name_get(struct __vxge_hw_device *devh);

enum vxge_hw_status __devinit vxge_hw_device_initialize(
	struct __vxge_hw_device **devh,
	struct vxge_hw_device_attr *attr,
	struct vxge_hw_device_config *device_config);

enum vxge_hw_status vxge_hw_device_getpause_data(
	 struct __vxge_hw_device *devh,
	 u32 port,
	 u32 *tx,
	 u32 *rx);

enum vxge_hw_status vxge_hw_device_setpause_data(
	struct __vxge_hw_device *devh,
	u32 port,
	u32 tx,
	u32 rx);

static inline void *vxge_os_dma_malloc(struct pci_dev *pdev,
			unsigned long size,
			struct pci_dev **p_dmah,
			struct pci_dev **p_dma_acch)
{
	gfp_t flags;
	void *vaddr;
	unsigned long misaligned = 0;
	int realloc_flag = 0;
	*p_dma_acch = *p_dmah = NULL;

	if (in_interrupt())
		flags = GFP_ATOMIC | GFP_DMA;
	else
		flags = GFP_KERNEL | GFP_DMA;
realloc:
	vaddr = kmalloc((size), flags);
	if (vaddr == NULL)
		return vaddr;
	misaligned = (unsigned long)VXGE_ALIGN((unsigned long)vaddr,
				VXGE_CACHE_LINE_SIZE);
	if (realloc_flag)
		goto out;

	if (misaligned) {
		kfree((void *) vaddr);
		size += VXGE_CACHE_LINE_SIZE;
		realloc_flag = 1;
		goto realloc;
	}
out:
	*(unsigned long *)p_dma_acch = misaligned;
	vaddr = (void *)((u8 *)vaddr + misaligned);
	return vaddr;
}

static inline void vxge_os_dma_free(struct pci_dev *pdev, const void *vaddr,
			struct pci_dev **p_dma_acch)
{
	unsigned long misaligned = *(unsigned long *)p_dma_acch;
	u8 *tmp = (u8 *)vaddr;
	tmp -= misaligned;
	kfree((void *)tmp);
}

static inline void*
__vxge_hw_mempool_item_priv(
	struct vxge_hw_mempool *mempool,
	u32 memblock_idx,
	void *item,
	u32 *memblock_item_idx)
{
	ptrdiff_t offset;
	void *memblock = mempool->memblocks_arr[memblock_idx];


	offset = (u32)((u8 *)item - (u8 *)memblock);
	vxge_assert(offset >= 0 && (u32)offset < mempool->memblock_size);

	(*memblock_item_idx) = (u32) offset / mempool->item_size;
	vxge_assert((*memblock_item_idx) < mempool->items_per_memblock);

	return (u8 *)mempool->memblocks_priv_arr[memblock_idx] +
			    (*memblock_item_idx) * mempool->items_priv_size;
}

static inline struct __vxge_hw_fifo_txdl_priv *
__vxge_hw_fifo_txdl_priv(
	struct __vxge_hw_fifo *fifo,
	struct vxge_hw_fifo_txd *txdp)
{
	return (struct __vxge_hw_fifo_txdl_priv *)
			(((char *)((ulong)txdp->host_control)) +
				fifo->per_txdl_space);
}

enum vxge_hw_status vxge_hw_vpath_open(
	struct __vxge_hw_device *devh,
	struct vxge_hw_vpath_attr *attr,
	struct __vxge_hw_vpath_handle **vpath_handle);

enum vxge_hw_status vxge_hw_vpath_close(
	struct __vxge_hw_vpath_handle *vpath_handle);

enum vxge_hw_status
vxge_hw_vpath_reset(
	struct __vxge_hw_vpath_handle *vpath_handle);

enum vxge_hw_status
vxge_hw_vpath_recover_from_reset(
	struct __vxge_hw_vpath_handle *vpath_handle);

void
vxge_hw_vpath_enable(struct __vxge_hw_vpath_handle *vp);

enum vxge_hw_status
vxge_hw_vpath_check_leak(struct __vxge_hw_ring *ringh);

enum vxge_hw_status vxge_hw_vpath_mtu_set(
	struct __vxge_hw_vpath_handle *vpath_handle,
	u32 new_mtu);

void
vxge_hw_vpath_rx_doorbell_init(struct __vxge_hw_vpath_handle *vp);

#ifndef readq
static inline u64 readq(void __iomem *addr)
{
	u64 ret = 0;
	ret = readl(addr + 4);
	ret <<= 32;
	ret |= readl(addr);

	return ret;
}
#endif

#ifndef writeq
static inline void writeq(u64 val, void __iomem *addr)
{
	writel((u32) (val), addr);
	writel((u32) (val >> 32), (addr + 4));
}
#endif

static inline void __vxge_hw_pio_mem_write32_upper(u32 val, void __iomem *addr)
{
	writel(val, addr + 4);
}

static inline void __vxge_hw_pio_mem_write32_lower(u32 val, void __iomem *addr)
{
	writel(val, addr);
}

enum vxge_hw_status
vxge_hw_device_flick_link_led(struct __vxge_hw_device *devh, u64 on_off);

enum vxge_hw_status
vxge_hw_vpath_strip_fcs_check(struct __vxge_hw_device *hldev, u64 vpath_mask);

#if (VXGE_COMPONENT_LL & VXGE_DEBUG_MODULE_MASK)
#define vxge_debug_ll(level, mask, fmt, ...) do {			       \
	if ((level >= VXGE_ERR && VXGE_COMPONENT_LL & VXGE_DEBUG_ERR_MASK) ||  \
	    (level >= VXGE_TRACE && VXGE_COMPONENT_LL & VXGE_DEBUG_TRACE_MASK))\
		if ((mask & VXGE_DEBUG_MASK) == mask)			       \
			printk(fmt "\n", __VA_ARGS__);			       \
} while (0)
#else
#define vxge_debug_ll(level, mask, fmt, ...)
#endif

enum vxge_hw_status vxge_hw_vpath_rts_rth_itable_set(
			struct __vxge_hw_vpath_handle **vpath_handles,
			u32 vpath_count,
			u8 *mtable,
			u8 *itable,
			u32 itable_size);

enum vxge_hw_status vxge_hw_vpath_rts_rth_set(
	struct __vxge_hw_vpath_handle *vpath_handle,
	enum vxge_hw_rth_algoritms algorithm,
	struct vxge_hw_rth_hash_types *hash_type,
	u16 bucket_size);

enum vxge_hw_status
__vxge_hw_device_is_privilaged(u32 host_type, u32 func_id);

#define VXGE_HW_MIN_SUCCESSIVE_IDLE_COUNT 5
#define VXGE_HW_MAX_POLLING_COUNT 100

void
vxge_hw_device_wait_receive_idle(struct __vxge_hw_device *hldev);

enum vxge_hw_status
vxge_hw_upgrade_read_version(struct __vxge_hw_device *hldev, u32 *major,
			     u32 *minor, u32 *build);

enum vxge_hw_status vxge_hw_flash_fw(struct __vxge_hw_device *hldev);

enum vxge_hw_status
vxge_update_fw_image(struct __vxge_hw_device *hldev, const u8 *filebuf,
		     int size);

enum vxge_hw_status
vxge_hw_vpath_eprom_img_ver_get(struct __vxge_hw_device *hldev,
				struct eprom_image *eprom_image_data);

int vxge_hw_vpath_wait_receive_idle(struct __vxge_hw_device *hldev, u32 vp_id);
#endif
