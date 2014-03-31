/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2006 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/


#ifndef _E1000_HW_H_
#define _E1000_HW_H_

#include "e1000_osdep.h"


struct e1000_hw;
struct e1000_hw_stats;

typedef enum {
	e1000_undefined = 0,
	e1000_82542_rev2_0,
	e1000_82542_rev2_1,
	e1000_82543,
	e1000_82544,
	e1000_82540,
	e1000_82545,
	e1000_82545_rev_3,
	e1000_82546,
	e1000_ce4100,
	e1000_82546_rev_3,
	e1000_82541,
	e1000_82541_rev_2,
	e1000_82547,
	e1000_82547_rev_2,
	e1000_num_macs
} e1000_mac_type;

typedef enum {
	e1000_eeprom_uninitialized = 0,
	e1000_eeprom_spi,
	e1000_eeprom_microwire,
	e1000_eeprom_flash,
	e1000_eeprom_none,	
	e1000_num_eeprom_types
} e1000_eeprom_type;

typedef enum {
	e1000_media_type_copper = 0,
	e1000_media_type_fiber = 1,
	e1000_media_type_internal_serdes = 2,
	e1000_num_media_types
} e1000_media_type;

typedef enum {
	e1000_10_half = 0,
	e1000_10_full = 1,
	e1000_100_half = 2,
	e1000_100_full = 3
} e1000_speed_duplex_type;

typedef enum {
	E1000_FC_NONE = 0,
	E1000_FC_RX_PAUSE = 1,
	E1000_FC_TX_PAUSE = 2,
	E1000_FC_FULL = 3,
	E1000_FC_DEFAULT = 0xFF
} e1000_fc_type;

struct e1000_shadow_ram {
	u16 eeprom_word;
	bool modified;
};

typedef enum {
	e1000_bus_type_unknown = 0,
	e1000_bus_type_pci,
	e1000_bus_type_pcix,
	e1000_bus_type_reserved
} e1000_bus_type;

typedef enum {
	e1000_bus_speed_unknown = 0,
	e1000_bus_speed_33,
	e1000_bus_speed_66,
	e1000_bus_speed_100,
	e1000_bus_speed_120,
	e1000_bus_speed_133,
	e1000_bus_speed_reserved
} e1000_bus_speed;

typedef enum {
	e1000_bus_width_unknown = 0,
	e1000_bus_width_32,
	e1000_bus_width_64,
	e1000_bus_width_reserved
} e1000_bus_width;

typedef enum {
	e1000_cable_length_50 = 0,
	e1000_cable_length_50_80,
	e1000_cable_length_80_110,
	e1000_cable_length_110_140,
	e1000_cable_length_140,
	e1000_cable_length_undefined = 0xFF
} e1000_cable_length;

typedef enum {
	e1000_gg_cable_length_60 = 0,
	e1000_gg_cable_length_60_115 = 1,
	e1000_gg_cable_length_115_150 = 2,
	e1000_gg_cable_length_150 = 4
} e1000_gg_cable_length;

typedef enum {
	e1000_igp_cable_length_10 = 10,
	e1000_igp_cable_length_20 = 20,
	e1000_igp_cable_length_30 = 30,
	e1000_igp_cable_length_40 = 40,
	e1000_igp_cable_length_50 = 50,
	e1000_igp_cable_length_60 = 60,
	e1000_igp_cable_length_70 = 70,
	e1000_igp_cable_length_80 = 80,
	e1000_igp_cable_length_90 = 90,
	e1000_igp_cable_length_100 = 100,
	e1000_igp_cable_length_110 = 110,
	e1000_igp_cable_length_115 = 115,
	e1000_igp_cable_length_120 = 120,
	e1000_igp_cable_length_130 = 130,
	e1000_igp_cable_length_140 = 140,
	e1000_igp_cable_length_150 = 150,
	e1000_igp_cable_length_160 = 160,
	e1000_igp_cable_length_170 = 170,
	e1000_igp_cable_length_180 = 180
} e1000_igp_cable_length;

typedef enum {
	e1000_10bt_ext_dist_enable_normal = 0,
	e1000_10bt_ext_dist_enable_lower,
	e1000_10bt_ext_dist_enable_undefined = 0xFF
} e1000_10bt_ext_dist_enable;

typedef enum {
	e1000_rev_polarity_normal = 0,
	e1000_rev_polarity_reversed,
	e1000_rev_polarity_undefined = 0xFF
} e1000_rev_polarity;

typedef enum {
	e1000_downshift_normal = 0,
	e1000_downshift_activated,
	e1000_downshift_undefined = 0xFF
} e1000_downshift;

typedef enum {
	e1000_smart_speed_default = 0,
	e1000_smart_speed_on,
	e1000_smart_speed_off
} e1000_smart_speed;

typedef enum {
	e1000_polarity_reversal_enabled = 0,
	e1000_polarity_reversal_disabled,
	e1000_polarity_reversal_undefined = 0xFF
} e1000_polarity_reversal;

typedef enum {
	e1000_auto_x_mode_manual_mdi = 0,
	e1000_auto_x_mode_manual_mdix,
	e1000_auto_x_mode_auto1,
	e1000_auto_x_mode_auto2,
	e1000_auto_x_mode_undefined = 0xFF
} e1000_auto_x_mode;

typedef enum {
	e1000_1000t_rx_status_not_ok = 0,
	e1000_1000t_rx_status_ok,
	e1000_1000t_rx_status_undefined = 0xFF
} e1000_1000t_rx_status;

typedef enum {
	e1000_phy_m88 = 0,
	e1000_phy_igp,
	e1000_phy_8211,
	e1000_phy_8201,
	e1000_phy_undefined = 0xFF
} e1000_phy_type;

typedef enum {
	e1000_ms_hw_default = 0,
	e1000_ms_force_master,
	e1000_ms_force_slave,
	e1000_ms_auto
} e1000_ms_type;

typedef enum {
	e1000_ffe_config_enabled = 0,
	e1000_ffe_config_active,
	e1000_ffe_config_blocked
} e1000_ffe_config;

typedef enum {
	e1000_dsp_config_disabled = 0,
	e1000_dsp_config_enabled,
	e1000_dsp_config_activated,
	e1000_dsp_config_undefined = 0xFF
} e1000_dsp_config;

struct e1000_phy_info {
	e1000_cable_length cable_length;
	e1000_10bt_ext_dist_enable extended_10bt_distance;
	e1000_rev_polarity cable_polarity;
	e1000_downshift downshift;
	e1000_polarity_reversal polarity_correction;
	e1000_auto_x_mode mdix_mode;
	e1000_1000t_rx_status local_rx;
	e1000_1000t_rx_status remote_rx;
};

struct e1000_phy_stats {
	u32 idle_errors;
	u32 receive_errors;
};

struct e1000_eeprom_info {
	e1000_eeprom_type type;
	u16 word_size;
	u16 opcode_bits;
	u16 address_bits;
	u16 delay_usec;
	u16 page_size;
};

#define E1000_HOST_IF_MAX_SIZE  2048

typedef enum {
	e1000_byte_align = 0,
	e1000_word_align = 1,
	e1000_dword_align = 2
} e1000_align_type;

#define E1000_SUCCESS      0
#define E1000_ERR_EEPROM   1
#define E1000_ERR_PHY      2
#define E1000_ERR_CONFIG   3
#define E1000_ERR_PARAM    4
#define E1000_ERR_MAC_TYPE 5
#define E1000_ERR_PHY_TYPE 6
#define E1000_ERR_RESET   9
#define E1000_ERR_MASTER_REQUESTS_PENDING 10
#define E1000_ERR_HOST_INTERFACE_COMMAND 11
#define E1000_BLK_PHY_RESET   12

#define E1000_BYTE_SWAP_WORD(_value) ((((_value) & 0x00ff) << 8) | \
                                     (((_value) & 0xff00) >> 8))

s32 e1000_reset_hw(struct e1000_hw *hw);
s32 e1000_init_hw(struct e1000_hw *hw);
s32 e1000_set_mac_type(struct e1000_hw *hw);
void e1000_set_media_type(struct e1000_hw *hw);

s32 e1000_setup_link(struct e1000_hw *hw);
s32 e1000_phy_setup_autoneg(struct e1000_hw *hw);
void e1000_config_collision_dist(struct e1000_hw *hw);
s32 e1000_check_for_link(struct e1000_hw *hw);
s32 e1000_get_speed_and_duplex(struct e1000_hw *hw, u16 * speed, u16 * duplex);
s32 e1000_force_mac_fc(struct e1000_hw *hw);

s32 e1000_read_phy_reg(struct e1000_hw *hw, u32 reg_addr, u16 * phy_data);
s32 e1000_write_phy_reg(struct e1000_hw *hw, u32 reg_addr, u16 data);
s32 e1000_phy_hw_reset(struct e1000_hw *hw);
s32 e1000_phy_reset(struct e1000_hw *hw);
s32 e1000_phy_get_info(struct e1000_hw *hw, struct e1000_phy_info *phy_info);
s32 e1000_validate_mdi_setting(struct e1000_hw *hw);

s32 e1000_init_eeprom_params(struct e1000_hw *hw);

u32 e1000_enable_mng_pass_thru(struct e1000_hw *hw);

#define E1000_MNG_DHCP_TX_PAYLOAD_CMD   64
#define E1000_HI_MAX_MNG_DATA_LENGTH    0x6F8	

#define E1000_MNG_DHCP_COMMAND_TIMEOUT  10	
#define E1000_MNG_DHCP_COOKIE_OFFSET    0x6F0	
#define E1000_MNG_DHCP_COOKIE_LENGTH    0x10	
#define E1000_MNG_IAMT_MODE             0x3
#define E1000_MNG_ICH_IAMT_MODE         0x2
#define E1000_IAMT_SIGNATURE            0x544D4149	

#define E1000_MNG_DHCP_COOKIE_STATUS_PARSING_SUPPORT 0x1	
#define E1000_MNG_DHCP_COOKIE_STATUS_VLAN_SUPPORT    0x2	
#define E1000_VFTA_ENTRY_SHIFT                       0x5
#define E1000_VFTA_ENTRY_MASK                        0x7F
#define E1000_VFTA_ENTRY_BIT_SHIFT_MASK              0x1F

struct e1000_host_mng_command_header {
	u8 command_id;
	u8 checksum;
	u16 reserved1;
	u16 reserved2;
	u16 command_length;
};

struct e1000_host_mng_command_info {
	struct e1000_host_mng_command_header command_header;	
	u8 command_data[E1000_HI_MAX_MNG_DATA_LENGTH];	
};
#ifdef __BIG_ENDIAN
struct e1000_host_mng_dhcp_cookie {
	u32 signature;
	u16 vlan_id;
	u8 reserved0;
	u8 status;
	u32 reserved1;
	u8 checksum;
	u8 reserved3;
	u16 reserved2;
};
#else
struct e1000_host_mng_dhcp_cookie {
	u32 signature;
	u8 status;
	u8 reserved0;
	u16 vlan_id;
	u32 reserved1;
	u16 reserved2;
	u8 reserved3;
	u8 checksum;
};
#endif

bool e1000_check_mng_mode(struct e1000_hw *hw);
s32 e1000_read_eeprom(struct e1000_hw *hw, u16 reg, u16 words, u16 * data);
s32 e1000_validate_eeprom_checksum(struct e1000_hw *hw);
s32 e1000_update_eeprom_checksum(struct e1000_hw *hw);
s32 e1000_write_eeprom(struct e1000_hw *hw, u16 reg, u16 words, u16 * data);
s32 e1000_read_mac_addr(struct e1000_hw *hw);

u32 e1000_hash_mc_addr(struct e1000_hw *hw, u8 * mc_addr);
void e1000_mta_set(struct e1000_hw *hw, u32 hash_value);
void e1000_rar_set(struct e1000_hw *hw, u8 * mc_addr, u32 rar_index);
void e1000_write_vfta(struct e1000_hw *hw, u32 offset, u32 value);

s32 e1000_setup_led(struct e1000_hw *hw);
s32 e1000_cleanup_led(struct e1000_hw *hw);
s32 e1000_led_on(struct e1000_hw *hw);
s32 e1000_led_off(struct e1000_hw *hw);
s32 e1000_blink_led_start(struct e1000_hw *hw);


void e1000_reset_adaptive(struct e1000_hw *hw);
void e1000_update_adaptive(struct e1000_hw *hw);
void e1000_tbi_adjust_stats(struct e1000_hw *hw, struct e1000_hw_stats *stats,
			    u32 frame_len, u8 * mac_addr);
void e1000_get_bus_info(struct e1000_hw *hw);
void e1000_pci_set_mwi(struct e1000_hw *hw);
void e1000_pci_clear_mwi(struct e1000_hw *hw);
void e1000_pcix_set_mmrbc(struct e1000_hw *hw, int mmrbc);
int e1000_pcix_get_mmrbc(struct e1000_hw *hw);
void e1000_io_write(struct e1000_hw *hw, unsigned long port, u32 value);

#define E1000_READ_REG_IO(a, reg) \
    e1000_read_reg_io((a), E1000_##reg)
#define E1000_WRITE_REG_IO(a, reg, val) \
    e1000_write_reg_io((a), E1000_##reg, val)

#define E1000_DEV_ID_82542               0x1000
#define E1000_DEV_ID_82543GC_FIBER       0x1001
#define E1000_DEV_ID_82543GC_COPPER      0x1004
#define E1000_DEV_ID_82544EI_COPPER      0x1008
#define E1000_DEV_ID_82544EI_FIBER       0x1009
#define E1000_DEV_ID_82544GC_COPPER      0x100C
#define E1000_DEV_ID_82544GC_LOM         0x100D
#define E1000_DEV_ID_82540EM             0x100E
#define E1000_DEV_ID_82540EM_LOM         0x1015
#define E1000_DEV_ID_82540EP_LOM         0x1016
#define E1000_DEV_ID_82540EP             0x1017
#define E1000_DEV_ID_82540EP_LP          0x101E
#define E1000_DEV_ID_82545EM_COPPER      0x100F
#define E1000_DEV_ID_82545EM_FIBER       0x1011
#define E1000_DEV_ID_82545GM_COPPER      0x1026
#define E1000_DEV_ID_82545GM_FIBER       0x1027
#define E1000_DEV_ID_82545GM_SERDES      0x1028
#define E1000_DEV_ID_82546EB_COPPER      0x1010
#define E1000_DEV_ID_82546EB_FIBER       0x1012
#define E1000_DEV_ID_82546EB_QUAD_COPPER 0x101D
#define E1000_DEV_ID_82541EI             0x1013
#define E1000_DEV_ID_82541EI_MOBILE      0x1018
#define E1000_DEV_ID_82541ER_LOM         0x1014
#define E1000_DEV_ID_82541ER             0x1078
#define E1000_DEV_ID_82547GI             0x1075
#define E1000_DEV_ID_82541GI             0x1076
#define E1000_DEV_ID_82541GI_MOBILE      0x1077
#define E1000_DEV_ID_82541GI_LF          0x107C
#define E1000_DEV_ID_82546GB_COPPER      0x1079
#define E1000_DEV_ID_82546GB_FIBER       0x107A
#define E1000_DEV_ID_82546GB_SERDES      0x107B
#define E1000_DEV_ID_82546GB_PCIE        0x108A
#define E1000_DEV_ID_82546GB_QUAD_COPPER 0x1099
#define E1000_DEV_ID_82547EI             0x1019
#define E1000_DEV_ID_82547EI_MOBILE      0x101A
#define E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3 0x10B5
#define E1000_DEV_ID_INTEL_CE4100_GBE    0x2E6E

#define NODE_ADDRESS_SIZE 6

#define MAC_DECODE_SIZE (128 * 1024)

#define E1000_82542_2_0_REV_ID 2
#define E1000_82542_2_1_REV_ID 3
#define E1000_REVISION_0       0
#define E1000_REVISION_1       1
#define E1000_REVISION_2       2
#define E1000_REVISION_3       3

#define SPEED_10    10
#define SPEED_100   100
#define SPEED_1000  1000
#define HALF_DUPLEX 1
#define FULL_DUPLEX 2

#define ENET_HEADER_SIZE             14
#define MINIMUM_ETHERNET_FRAME_SIZE  64	
#define ETHERNET_FCS_SIZE            4
#define MINIMUM_ETHERNET_PACKET_SIZE \
    (MINIMUM_ETHERNET_FRAME_SIZE - ETHERNET_FCS_SIZE)
#define CRC_LENGTH                   ETHERNET_FCS_SIZE
#define MAX_JUMBO_FRAME_SIZE         0x3F00

#define VLAN_TAG_SIZE  4	

#define ETHERNET_IEEE_VLAN_TYPE 0x8100	
#define ETHERNET_IP_TYPE        0x0800	
#define ETHERNET_ARP_TYPE       0x0806	

#define IP_PROTOCOL_TCP    6
#define IP_PROTOCOL_UDP    0x11

#define POLL_IMS_ENABLE_MASK ( \
    E1000_IMS_RXDMT0 |         \
    E1000_IMS_RXSEQ)

/* This defines the bits that are set in the Interrupt Mask
 * Set/Read Register.  Each bit is documented below:
 *   o RXT0   = Receiver Timer Interrupt (ring 0)
 *   o TXDW   = Transmit Descriptor Written Back
 *   o RXDMT0 = Receive Descriptor Minimum Threshold hit (ring 0)
 *   o RXSEQ  = Receive Sequence Error
 *   o LSC    = Link Status Change
 */
#define IMS_ENABLE_MASK ( \
    E1000_IMS_RXT0   |    \
    E1000_IMS_TXDW   |    \
    E1000_IMS_RXDMT0 |    \
    E1000_IMS_RXSEQ  |    \
    E1000_IMS_LSC)

#define E1000_RAR_ENTRIES 15

#define MIN_NUMBER_OF_DESCRIPTORS  8
#define MAX_NUMBER_OF_DESCRIPTORS  0xFFF8

struct e1000_rx_desc {
	__le64 buffer_addr;	
	__le16 length;		
	__le16 csum;		
	u8 status;		
	u8 errors;		
	__le16 special;
};

union e1000_rx_desc_extended {
	struct {
		__le64 buffer_addr;
		__le64 reserved;
	} read;
	struct {
		struct {
			__le32 mrq;	
			union {
				__le32 rss;	
				struct {
					__le16 ip_id;	
					__le16 csum;	
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;	
			__le16 length;
			__le16 vlan;	
		} upper;
	} wb;			
};

#define MAX_PS_BUFFERS 4
union e1000_rx_desc_packet_split {
	struct {
		
		__le64 buffer_addr[MAX_PS_BUFFERS];
	} read;
	struct {
		struct {
			__le32 mrq;	
			union {
				__le32 rss;	
				struct {
					__le16 ip_id;	
					__le16 csum;	
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;	
			__le16 length0;	
			__le16 vlan;	
		} middle;
		struct {
			__le16 header_status;
			__le16 length[3];	
		} upper;
		__le64 reserved;
	} wb;			
};

#define E1000_RXD_STAT_DD       0x01	
#define E1000_RXD_STAT_EOP      0x02	
#define E1000_RXD_STAT_IXSM     0x04	
#define E1000_RXD_STAT_VP       0x08	
#define E1000_RXD_STAT_UDPCS    0x10	
#define E1000_RXD_STAT_TCPCS    0x20	
#define E1000_RXD_STAT_IPCS     0x40	
#define E1000_RXD_STAT_PIF      0x80	
#define E1000_RXD_STAT_IPIDV    0x200	
#define E1000_RXD_STAT_UDPV     0x400	
#define E1000_RXD_STAT_ACK      0x8000	
#define E1000_RXD_ERR_CE        0x01	
#define E1000_RXD_ERR_SE        0x02	
#define E1000_RXD_ERR_SEQ       0x04	
#define E1000_RXD_ERR_CXE       0x10	
#define E1000_RXD_ERR_TCPE      0x20	
#define E1000_RXD_ERR_IPE       0x40	
#define E1000_RXD_ERR_RXE       0x80	
#define E1000_RXD_SPC_VLAN_MASK 0x0FFF	
#define E1000_RXD_SPC_PRI_MASK  0xE000	
#define E1000_RXD_SPC_PRI_SHIFT 13
#define E1000_RXD_SPC_CFI_MASK  0x1000	
#define E1000_RXD_SPC_CFI_SHIFT 12

#define E1000_RXDEXT_STATERR_CE    0x01000000
#define E1000_RXDEXT_STATERR_SE    0x02000000
#define E1000_RXDEXT_STATERR_SEQ   0x04000000
#define E1000_RXDEXT_STATERR_CXE   0x10000000
#define E1000_RXDEXT_STATERR_TCPE  0x20000000
#define E1000_RXDEXT_STATERR_IPE   0x40000000
#define E1000_RXDEXT_STATERR_RXE   0x80000000

#define E1000_RXDPS_HDRSTAT_HDRSP        0x00008000
#define E1000_RXDPS_HDRSTAT_HDRLEN_MASK  0x000003FF

#define E1000_RXD_ERR_FRAME_ERR_MASK ( \
    E1000_RXD_ERR_CE  |                \
    E1000_RXD_ERR_SE  |                \
    E1000_RXD_ERR_SEQ |                \
    E1000_RXD_ERR_CXE |                \
    E1000_RXD_ERR_RXE)

#define E1000_RXDEXT_ERR_FRAME_ERR_MASK ( \
    E1000_RXDEXT_STATERR_CE  |            \
    E1000_RXDEXT_STATERR_SE  |            \
    E1000_RXDEXT_STATERR_SEQ |            \
    E1000_RXDEXT_STATERR_CXE |            \
    E1000_RXDEXT_STATERR_RXE)

struct e1000_tx_desc {
	__le64 buffer_addr;	
	union {
		__le32 data;
		struct {
			__le16 length;	
			u8 cso;	
			u8 cmd;	
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;	
			u8 css;	
			__le16 special;
		} fields;
	} upper;
};

#define E1000_TXD_DTYP_D     0x00100000	
#define E1000_TXD_DTYP_C     0x00000000	
#define E1000_TXD_POPTS_IXSM 0x01	
#define E1000_TXD_POPTS_TXSM 0x02	
#define E1000_TXD_CMD_EOP    0x01000000	
#define E1000_TXD_CMD_IFCS   0x02000000	
#define E1000_TXD_CMD_IC     0x04000000	
#define E1000_TXD_CMD_RS     0x08000000	
#define E1000_TXD_CMD_RPS    0x10000000	
#define E1000_TXD_CMD_DEXT   0x20000000	
#define E1000_TXD_CMD_VLE    0x40000000	
#define E1000_TXD_CMD_IDE    0x80000000	
#define E1000_TXD_STAT_DD    0x00000001	
#define E1000_TXD_STAT_EC    0x00000002	
#define E1000_TXD_STAT_LC    0x00000004	
#define E1000_TXD_STAT_TU    0x00000008	
#define E1000_TXD_CMD_TCP    0x01000000	
#define E1000_TXD_CMD_IP     0x02000000	
#define E1000_TXD_CMD_TSE    0x04000000	
#define E1000_TXD_STAT_TC    0x00000004	

struct e1000_context_desc {
	union {
		__le32 ip_config;
		struct {
			u8 ipcss;	
			u8 ipcso;	
			__le16 ipcse;	
		} ip_fields;
	} lower_setup;
	union {
		__le32 tcp_config;
		struct {
			u8 tucss;	
			u8 tucso;	
			__le16 tucse;	
		} tcp_fields;
	} upper_setup;
	__le32 cmd_and_length;	
	union {
		__le32 data;
		struct {
			u8 status;	
			u8 hdr_len;	
			__le16 mss;	
		} fields;
	} tcp_seg_setup;
};

struct e1000_data_desc {
	__le64 buffer_addr;	
	union {
		__le32 data;
		struct {
			__le16 length;	
			u8 typ_len_ext;	
			u8 cmd;	
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;	
			u8 popts;	
			__le16 special;	
		} fields;
	} upper;
};

#define E1000_NUM_UNICAST          16	
#define E1000_MC_TBL_SIZE          128	
#define E1000_VLAN_FILTER_TBL_SIZE 128	

struct e1000_rar {
	volatile __le32 low;	
	volatile __le32 high;	
};

#define E1000_NUM_MTA_REGISTERS 128

struct e1000_ipv4_at_entry {
	volatile u32 ipv4_addr;	
	volatile u32 reserved;
};

#define E1000_WAKEUP_IP_ADDRESS_COUNT_MAX 4
#define E1000_IP4AT_SIZE                  E1000_WAKEUP_IP_ADDRESS_COUNT_MAX
#define E1000_IP6AT_SIZE                  1

struct e1000_ipv6_at_entry {
	volatile u8 ipv6_addr[16];
};

struct e1000_fflt_entry {
	volatile u32 length;	
	volatile u32 reserved;
};

struct e1000_ffmt_entry {
	volatile u32 mask;	
	volatile u32 reserved;
};

struct e1000_ffvt_entry {
	volatile u32 value;	
	volatile u32 reserved;
};

#define E1000_FLEXIBLE_FILTER_COUNT_MAX 4

#define E1000_FLEXIBLE_FILTER_SIZE_MAX  128

#define E1000_FFLT_SIZE E1000_FLEXIBLE_FILTER_COUNT_MAX
#define E1000_FFMT_SIZE E1000_FLEXIBLE_FILTER_SIZE_MAX
#define E1000_FFVT_SIZE E1000_FLEXIBLE_FILTER_SIZE_MAX

#define E1000_DISABLE_SERDES_LOOPBACK   0x0400

#define E1000_CTRL     0x00000	
#define E1000_CTRL_DUP 0x00004	
#define E1000_STATUS   0x00008	
#define E1000_EECD     0x00010	
#define E1000_EERD     0x00014	
#define E1000_CTRL_EXT 0x00018	
#define E1000_FLA      0x0001C	
#define E1000_MDIC     0x00020	

#define INTEL_CE_GBE_MDIO_RCOMP_BASE    (hw->ce4100_gbe_mdio_base_virt)
#define E1000_MDIO_STS  (INTEL_CE_GBE_MDIO_RCOMP_BASE + 0)
#define E1000_MDIO_CMD  (INTEL_CE_GBE_MDIO_RCOMP_BASE + 4)
#define E1000_MDIO_DRV  (INTEL_CE_GBE_MDIO_RCOMP_BASE + 8)
#define E1000_MDC_CMD   (INTEL_CE_GBE_MDIO_RCOMP_BASE + 0xC)
#define E1000_RCOMP_CTL (INTEL_CE_GBE_MDIO_RCOMP_BASE + 0x20)
#define E1000_RCOMP_STS (INTEL_CE_GBE_MDIO_RCOMP_BASE + 0x24)

#define E1000_SCTL     0x00024	
#define E1000_FEXTNVM  0x00028	
#define E1000_FCAL     0x00028	
#define E1000_FCAH     0x0002C	
#define E1000_FCT      0x00030	
#define E1000_VET      0x00038	
#define E1000_ICR      0x000C0	
#define E1000_ITR      0x000C4	
#define E1000_ICS      0x000C8	
#define E1000_IMS      0x000D0	
#define E1000_IMC      0x000D8	
#define E1000_IAM      0x000E0	

#define E1000_CTL_AUX  0x000E0
#define E1000_CTL_AUX_END_SEL_SHIFT     10
#define E1000_CTL_AUX_ENDIANESS_SHIFT   8
#define E1000_CTL_AUX_RGMII_RMII_SHIFT  0

#define E1000_CTL_AUX_DES_PKT   (0x0 << E1000_CTL_AUX_END_SEL_SHIFT)
#define E1000_CTL_AUX_DES       (0x1 << E1000_CTL_AUX_END_SEL_SHIFT)
#define E1000_CTL_AUX_PKT       (0x2 << E1000_CTL_AUX_END_SEL_SHIFT)
#define E1000_CTL_AUX_ALL       (0x3 << E1000_CTL_AUX_END_SEL_SHIFT)

#define E1000_CTL_AUX_RGMII     (0x0 << E1000_CTL_AUX_RGMII_RMII_SHIFT)
#define E1000_CTL_AUX_RMII      (0x1 << E1000_CTL_AUX_RGMII_RMII_SHIFT)

#define E1000_CTL_AUX_LWLE_BBE  (0x0 << E1000_CTL_AUX_ENDIANESS_SHIFT)
#define E1000_CTL_AUX_LWLE_BLE  (0x1 << E1000_CTL_AUX_ENDIANESS_SHIFT)
#define E1000_CTL_AUX_LWBE_BBE  (0x2 << E1000_CTL_AUX_ENDIANESS_SHIFT)
#define E1000_CTL_AUX_LWBE_BLE  (0x3 << E1000_CTL_AUX_ENDIANESS_SHIFT)

#define E1000_RCTL     0x00100	
#define E1000_RDTR1    0x02820	
#define E1000_RDBAL1   0x02900	
#define E1000_RDBAH1   0x02904	
#define E1000_RDLEN1   0x02908	
#define E1000_RDH1     0x02910	
#define E1000_RDT1     0x02918	
#define E1000_FCTTV    0x00170	
#define E1000_TXCW     0x00178	
#define E1000_RXCW     0x00180	
#define E1000_TCTL     0x00400	
#define E1000_TCTL_EXT 0x00404	
#define E1000_TIPG     0x00410	
#define E1000_TBT      0x00448	
#define E1000_AIT      0x00458	
#define E1000_LEDCTL   0x00E00	
#define E1000_EXTCNF_CTRL  0x00F00	
#define E1000_EXTCNF_SIZE  0x00F08	
#define E1000_PHY_CTRL     0x00F10	
#define FEXTNVM_SW_CONFIG  0x0001
#define E1000_PBA      0x01000	
#define E1000_PBS      0x01008	
#define E1000_EEMNGCTL 0x01010	
#define E1000_FLASH_UPDATES 1000
#define E1000_EEARBC   0x01024	
#define E1000_FLASHT   0x01028	
#define E1000_EEWR     0x0102C	
#define E1000_FLSWCTL  0x01030	
#define E1000_FLSWDATA 0x01034	
#define E1000_FLSWCNT  0x01038	
#define E1000_FLOP     0x0103C	
#define E1000_ERT      0x02008	
#define E1000_FCRTL    0x02160	
#define E1000_FCRTH    0x02168	
#define E1000_PSRCTL   0x02170	
#define E1000_RDFH     0x02410  
#define E1000_RDFT     0x02418  
#define E1000_RDFHS    0x02420  
#define E1000_RDFTS    0x02428  
#define E1000_RDFPC    0x02430  
#define E1000_RDBAL    0x02800	
#define E1000_RDBAH    0x02804	
#define E1000_RDLEN    0x02808	
#define E1000_RDH      0x02810	
#define E1000_RDT      0x02818	
#define E1000_RDTR     0x02820	
#define E1000_RDBAL0   E1000_RDBAL	
#define E1000_RDBAH0   E1000_RDBAH	
#define E1000_RDLEN0   E1000_RDLEN	
#define E1000_RDH0     E1000_RDH	
#define E1000_RDT0     E1000_RDT	
#define E1000_RDTR0    E1000_RDTR	
#define E1000_RXDCTL   0x02828	
#define E1000_RXDCTL1  0x02928	
#define E1000_RADV     0x0282C	
#define E1000_RSRPD    0x02C00	
#define E1000_RAID     0x02C08	
#define E1000_TXDMAC   0x03000	
#define E1000_KABGTXD  0x03004	
#define E1000_TDFH     0x03410	
#define E1000_TDFT     0x03418	
#define E1000_TDFHS    0x03420	
#define E1000_TDFTS    0x03428	
#define E1000_TDFPC    0x03430	
#define E1000_TDBAL    0x03800	
#define E1000_TDBAH    0x03804	
#define E1000_TDLEN    0x03808	
#define E1000_TDH      0x03810	
#define E1000_TDT      0x03818	
#define E1000_TIDV     0x03820	
#define E1000_TXDCTL   0x03828	
#define E1000_TADV     0x0382C	
#define E1000_TSPMT    0x03830	
#define E1000_TARC0    0x03840	
#define E1000_TDBAL1   0x03900	
#define E1000_TDBAH1   0x03904	
#define E1000_TDLEN1   0x03908	
#define E1000_TDH1     0x03910	
#define E1000_TDT1     0x03918	
#define E1000_TXDCTL1  0x03928	
#define E1000_TARC1    0x03940	
#define E1000_CRCERRS  0x04000	
#define E1000_ALGNERRC 0x04004	
#define E1000_SYMERRS  0x04008	
#define E1000_RXERRC   0x0400C	
#define E1000_MPC      0x04010	
#define E1000_SCC      0x04014	
#define E1000_ECOL     0x04018	
#define E1000_MCC      0x0401C	
#define E1000_LATECOL  0x04020	
#define E1000_COLC     0x04028	
#define E1000_DC       0x04030	
#define E1000_TNCRS    0x04034	
#define E1000_SEC      0x04038	
#define E1000_CEXTERR  0x0403C	
#define E1000_RLEC     0x04040	
#define E1000_XONRXC   0x04048	
#define E1000_XONTXC   0x0404C	
#define E1000_XOFFRXC  0x04050	
#define E1000_XOFFTXC  0x04054	
#define E1000_FCRUC    0x04058	
#define E1000_PRC64    0x0405C	
#define E1000_PRC127   0x04060	
#define E1000_PRC255   0x04064	
#define E1000_PRC511   0x04068	
#define E1000_PRC1023  0x0406C	
#define E1000_PRC1522  0x04070	
#define E1000_GPRC     0x04074	
#define E1000_BPRC     0x04078	
#define E1000_MPRC     0x0407C	
#define E1000_GPTC     0x04080	
#define E1000_GORCL    0x04088	
#define E1000_GORCH    0x0408C	
#define E1000_GOTCL    0x04090	
#define E1000_GOTCH    0x04094	
#define E1000_RNBC     0x040A0	
#define E1000_RUC      0x040A4	
#define E1000_RFC      0x040A8	
#define E1000_ROC      0x040AC	
#define E1000_RJC      0x040B0	
#define E1000_MGTPRC   0x040B4	
#define E1000_MGTPDC   0x040B8	
#define E1000_MGTPTC   0x040BC	
#define E1000_TORL     0x040C0	
#define E1000_TORH     0x040C4	
#define E1000_TOTL     0x040C8	
#define E1000_TOTH     0x040CC	
#define E1000_TPR      0x040D0	
#define E1000_TPT      0x040D4	
#define E1000_PTC64    0x040D8	
#define E1000_PTC127   0x040DC	
#define E1000_PTC255   0x040E0	
#define E1000_PTC511   0x040E4	
#define E1000_PTC1023  0x040E8	
#define E1000_PTC1522  0x040EC	
#define E1000_MPTC     0x040F0	
#define E1000_BPTC     0x040F4	
#define E1000_TSCTC    0x040F8	
#define E1000_TSCTFC   0x040FC	
#define E1000_IAC      0x04100	
#define E1000_ICRXPTC  0x04104	
#define E1000_ICRXATC  0x04108	
#define E1000_ICTXPTC  0x0410C	
#define E1000_ICTXATC  0x04110	
#define E1000_ICTXQEC  0x04118	
#define E1000_ICTXQMTC 0x0411C	
#define E1000_ICRXDMTC 0x04120	
#define E1000_ICRXOC   0x04124	
#define E1000_RXCSUM   0x05000	
#define E1000_RFCTL    0x05008	
#define E1000_MTA      0x05200	
#define E1000_RA       0x05400	
#define E1000_VFTA     0x05600	
#define E1000_WUC      0x05800	
#define E1000_WUFC     0x05808	
#define E1000_WUS      0x05810	
#define E1000_MANC     0x05820	
#define E1000_IPAV     0x05838	
#define E1000_IP4AT    0x05840	
#define E1000_IP6AT    0x05880	
#define E1000_WUPL     0x05900	
#define E1000_WUPM     0x05A00	
#define E1000_FFLT     0x05F00	
#define E1000_HOST_IF  0x08800	
#define E1000_FFMT     0x09000	
#define E1000_FFVT     0x09800	

#define E1000_KUMCTRLSTA 0x00034	
#define E1000_MDPHYA     0x0003C	
#define E1000_MANC2H     0x05860	
#define E1000_SW_FW_SYNC 0x05B5C	

#define E1000_GCR       0x05B00	
#define E1000_GSCL_1    0x05B10	
#define E1000_GSCL_2    0x05B14	
#define E1000_GSCL_3    0x05B18	
#define E1000_GSCL_4    0x05B1C	
#define E1000_FACTPS    0x05B30	
#define E1000_SWSM      0x05B50	
#define E1000_FWSM      0x05B54	
#define E1000_FFLT_DBG  0x05F04	
#define E1000_HICR      0x08F00	

#define E1000_CPUVEC    0x02C10	
#define E1000_MRQC      0x05818	
#define E1000_RETA      0x05C00	
#define E1000_RSSRK     0x05C80	
#define E1000_RSSIM     0x05864	
#define E1000_RSSIR     0x05868	
#define E1000_82542_CTL_AUX  E1000_CTL_AUX
#define E1000_82542_CTRL     E1000_CTRL
#define E1000_82542_CTRL_DUP E1000_CTRL_DUP
#define E1000_82542_STATUS   E1000_STATUS
#define E1000_82542_EECD     E1000_EECD
#define E1000_82542_EERD     E1000_EERD
#define E1000_82542_CTRL_EXT E1000_CTRL_EXT
#define E1000_82542_FLA      E1000_FLA
#define E1000_82542_MDIC     E1000_MDIC
#define E1000_82542_SCTL     E1000_SCTL
#define E1000_82542_FEXTNVM  E1000_FEXTNVM
#define E1000_82542_FCAL     E1000_FCAL
#define E1000_82542_FCAH     E1000_FCAH
#define E1000_82542_FCT      E1000_FCT
#define E1000_82542_VET      E1000_VET
#define E1000_82542_RA       0x00040
#define E1000_82542_ICR      E1000_ICR
#define E1000_82542_ITR      E1000_ITR
#define E1000_82542_ICS      E1000_ICS
#define E1000_82542_IMS      E1000_IMS
#define E1000_82542_IMC      E1000_IMC
#define E1000_82542_RCTL     E1000_RCTL
#define E1000_82542_RDTR     0x00108
#define E1000_82542_RDFH     E1000_RDFH
#define E1000_82542_RDFT     E1000_RDFT
#define E1000_82542_RDFHS    E1000_RDFHS
#define E1000_82542_RDFTS    E1000_RDFTS
#define E1000_82542_RDFPC    E1000_RDFPC
#define E1000_82542_RDBAL    0x00110
#define E1000_82542_RDBAH    0x00114
#define E1000_82542_RDLEN    0x00118
#define E1000_82542_RDH      0x00120
#define E1000_82542_RDT      0x00128
#define E1000_82542_RDTR0    E1000_82542_RDTR
#define E1000_82542_RDBAL0   E1000_82542_RDBAL
#define E1000_82542_RDBAH0   E1000_82542_RDBAH
#define E1000_82542_RDLEN0   E1000_82542_RDLEN
#define E1000_82542_RDH0     E1000_82542_RDH
#define E1000_82542_RDT0     E1000_82542_RDT
#define E1000_82542_SRRCTL(_n) (0x280C + ((_n) << 8))	
#define E1000_82542_DCA_RXCTRL(_n) (0x02814 + ((_n) << 8))
#define E1000_82542_RDBAH3   0x02B04	
#define E1000_82542_RDBAL3   0x02B00	
#define E1000_82542_RDLEN3   0x02B08	
#define E1000_82542_RDH3     0x02B10	
#define E1000_82542_RDT3     0x02B18	
#define E1000_82542_RDBAL2   0x02A00	
#define E1000_82542_RDBAH2   0x02A04	
#define E1000_82542_RDLEN2   0x02A08	
#define E1000_82542_RDH2     0x02A10	
#define E1000_82542_RDT2     0x02A18	
#define E1000_82542_RDTR1    0x00130
#define E1000_82542_RDBAL1   0x00138
#define E1000_82542_RDBAH1   0x0013C
#define E1000_82542_RDLEN1   0x00140
#define E1000_82542_RDH1     0x00148
#define E1000_82542_RDT1     0x00150
#define E1000_82542_FCRTH    0x00160
#define E1000_82542_FCRTL    0x00168
#define E1000_82542_FCTTV    E1000_FCTTV
#define E1000_82542_TXCW     E1000_TXCW
#define E1000_82542_RXCW     E1000_RXCW
#define E1000_82542_MTA      0x00200
#define E1000_82542_TCTL     E1000_TCTL
#define E1000_82542_TCTL_EXT E1000_TCTL_EXT
#define E1000_82542_TIPG     E1000_TIPG
#define E1000_82542_TDBAL    0x00420
#define E1000_82542_TDBAH    0x00424
#define E1000_82542_TDLEN    0x00428
#define E1000_82542_TDH      0x00430
#define E1000_82542_TDT      0x00438
#define E1000_82542_TIDV     0x00440
#define E1000_82542_TBT      E1000_TBT
#define E1000_82542_AIT      E1000_AIT
#define E1000_82542_VFTA     0x00600
#define E1000_82542_LEDCTL   E1000_LEDCTL
#define E1000_82542_PBA      E1000_PBA
#define E1000_82542_PBS      E1000_PBS
#define E1000_82542_EEMNGCTL E1000_EEMNGCTL
#define E1000_82542_EEARBC   E1000_EEARBC
#define E1000_82542_FLASHT   E1000_FLASHT
#define E1000_82542_EEWR     E1000_EEWR
#define E1000_82542_FLSWCTL  E1000_FLSWCTL
#define E1000_82542_FLSWDATA E1000_FLSWDATA
#define E1000_82542_FLSWCNT  E1000_FLSWCNT
#define E1000_82542_FLOP     E1000_FLOP
#define E1000_82542_EXTCNF_CTRL  E1000_EXTCNF_CTRL
#define E1000_82542_EXTCNF_SIZE  E1000_EXTCNF_SIZE
#define E1000_82542_PHY_CTRL E1000_PHY_CTRL
#define E1000_82542_ERT      E1000_ERT
#define E1000_82542_RXDCTL   E1000_RXDCTL
#define E1000_82542_RXDCTL1  E1000_RXDCTL1
#define E1000_82542_RADV     E1000_RADV
#define E1000_82542_RSRPD    E1000_RSRPD
#define E1000_82542_TXDMAC   E1000_TXDMAC
#define E1000_82542_KABGTXD  E1000_KABGTXD
#define E1000_82542_TDFHS    E1000_TDFHS
#define E1000_82542_TDFTS    E1000_TDFTS
#define E1000_82542_TDFPC    E1000_TDFPC
#define E1000_82542_TXDCTL   E1000_TXDCTL
#define E1000_82542_TADV     E1000_TADV
#define E1000_82542_TSPMT    E1000_TSPMT
#define E1000_82542_CRCERRS  E1000_CRCERRS
#define E1000_82542_ALGNERRC E1000_ALGNERRC
#define E1000_82542_SYMERRS  E1000_SYMERRS
#define E1000_82542_RXERRC   E1000_RXERRC
#define E1000_82542_MPC      E1000_MPC
#define E1000_82542_SCC      E1000_SCC
#define E1000_82542_ECOL     E1000_ECOL
#define E1000_82542_MCC      E1000_MCC
#define E1000_82542_LATECOL  E1000_LATECOL
#define E1000_82542_COLC     E1000_COLC
#define E1000_82542_DC       E1000_DC
#define E1000_82542_TNCRS    E1000_TNCRS
#define E1000_82542_SEC      E1000_SEC
#define E1000_82542_CEXTERR  E1000_CEXTERR
#define E1000_82542_RLEC     E1000_RLEC
#define E1000_82542_XONRXC   E1000_XONRXC
#define E1000_82542_XONTXC   E1000_XONTXC
#define E1000_82542_XOFFRXC  E1000_XOFFRXC
#define E1000_82542_XOFFTXC  E1000_XOFFTXC
#define E1000_82542_FCRUC    E1000_FCRUC
#define E1000_82542_PRC64    E1000_PRC64
#define E1000_82542_PRC127   E1000_PRC127
#define E1000_82542_PRC255   E1000_PRC255
#define E1000_82542_PRC511   E1000_PRC511
#define E1000_82542_PRC1023  E1000_PRC1023
#define E1000_82542_PRC1522  E1000_PRC1522
#define E1000_82542_GPRC     E1000_GPRC
#define E1000_82542_BPRC     E1000_BPRC
#define E1000_82542_MPRC     E1000_MPRC
#define E1000_82542_GPTC     E1000_GPTC
#define E1000_82542_GORCL    E1000_GORCL
#define E1000_82542_GORCH    E1000_GORCH
#define E1000_82542_GOTCL    E1000_GOTCL
#define E1000_82542_GOTCH    E1000_GOTCH
#define E1000_82542_RNBC     E1000_RNBC
#define E1000_82542_RUC      E1000_RUC
#define E1000_82542_RFC      E1000_RFC
#define E1000_82542_ROC      E1000_ROC
#define E1000_82542_RJC      E1000_RJC
#define E1000_82542_MGTPRC   E1000_MGTPRC
#define E1000_82542_MGTPDC   E1000_MGTPDC
#define E1000_82542_MGTPTC   E1000_MGTPTC
#define E1000_82542_TORL     E1000_TORL
#define E1000_82542_TORH     E1000_TORH
#define E1000_82542_TOTL     E1000_TOTL
#define E1000_82542_TOTH     E1000_TOTH
#define E1000_82542_TPR      E1000_TPR
#define E1000_82542_TPT      E1000_TPT
#define E1000_82542_PTC64    E1000_PTC64
#define E1000_82542_PTC127   E1000_PTC127
#define E1000_82542_PTC255   E1000_PTC255
#define E1000_82542_PTC511   E1000_PTC511
#define E1000_82542_PTC1023  E1000_PTC1023
#define E1000_82542_PTC1522  E1000_PTC1522
#define E1000_82542_MPTC     E1000_MPTC
#define E1000_82542_BPTC     E1000_BPTC
#define E1000_82542_TSCTC    E1000_TSCTC
#define E1000_82542_TSCTFC   E1000_TSCTFC
#define E1000_82542_RXCSUM   E1000_RXCSUM
#define E1000_82542_WUC      E1000_WUC
#define E1000_82542_WUFC     E1000_WUFC
#define E1000_82542_WUS      E1000_WUS
#define E1000_82542_MANC     E1000_MANC
#define E1000_82542_IPAV     E1000_IPAV
#define E1000_82542_IP4AT    E1000_IP4AT
#define E1000_82542_IP6AT    E1000_IP6AT
#define E1000_82542_WUPL     E1000_WUPL
#define E1000_82542_WUPM     E1000_WUPM
#define E1000_82542_FFLT     E1000_FFLT
#define E1000_82542_TDFH     0x08010
#define E1000_82542_TDFT     0x08018
#define E1000_82542_FFMT     E1000_FFMT
#define E1000_82542_FFVT     E1000_FFVT
#define E1000_82542_HOST_IF  E1000_HOST_IF
#define E1000_82542_IAM         E1000_IAM
#define E1000_82542_EEMNGCTL    E1000_EEMNGCTL
#define E1000_82542_PSRCTL      E1000_PSRCTL
#define E1000_82542_RAID        E1000_RAID
#define E1000_82542_TARC0       E1000_TARC0
#define E1000_82542_TDBAL1      E1000_TDBAL1
#define E1000_82542_TDBAH1      E1000_TDBAH1
#define E1000_82542_TDLEN1      E1000_TDLEN1
#define E1000_82542_TDH1        E1000_TDH1
#define E1000_82542_TDT1        E1000_TDT1
#define E1000_82542_TXDCTL1     E1000_TXDCTL1
#define E1000_82542_TARC1       E1000_TARC1
#define E1000_82542_RFCTL       E1000_RFCTL
#define E1000_82542_GCR         E1000_GCR
#define E1000_82542_GSCL_1      E1000_GSCL_1
#define E1000_82542_GSCL_2      E1000_GSCL_2
#define E1000_82542_GSCL_3      E1000_GSCL_3
#define E1000_82542_GSCL_4      E1000_GSCL_4
#define E1000_82542_FACTPS      E1000_FACTPS
#define E1000_82542_SWSM        E1000_SWSM
#define E1000_82542_FWSM        E1000_FWSM
#define E1000_82542_FFLT_DBG    E1000_FFLT_DBG
#define E1000_82542_IAC         E1000_IAC
#define E1000_82542_ICRXPTC     E1000_ICRXPTC
#define E1000_82542_ICRXATC     E1000_ICRXATC
#define E1000_82542_ICTXPTC     E1000_ICTXPTC
#define E1000_82542_ICTXATC     E1000_ICTXATC
#define E1000_82542_ICTXQEC     E1000_ICTXQEC
#define E1000_82542_ICTXQMTC    E1000_ICTXQMTC
#define E1000_82542_ICRXDMTC    E1000_ICRXDMTC
#define E1000_82542_ICRXOC      E1000_ICRXOC
#define E1000_82542_HICR        E1000_HICR

#define E1000_82542_CPUVEC      E1000_CPUVEC
#define E1000_82542_MRQC        E1000_MRQC
#define E1000_82542_RETA        E1000_RETA
#define E1000_82542_RSSRK       E1000_RSSRK
#define E1000_82542_RSSIM       E1000_RSSIM
#define E1000_82542_RSSIR       E1000_RSSIR
#define E1000_82542_KUMCTRLSTA E1000_KUMCTRLSTA
#define E1000_82542_SW_FW_SYNC E1000_SW_FW_SYNC

struct e1000_hw_stats {
	u64 crcerrs;
	u64 algnerrc;
	u64 symerrs;
	u64 rxerrc;
	u64 txerrc;
	u64 mpc;
	u64 scc;
	u64 ecol;
	u64 mcc;
	u64 latecol;
	u64 colc;
	u64 dc;
	u64 tncrs;
	u64 sec;
	u64 cexterr;
	u64 rlec;
	u64 xonrxc;
	u64 xontxc;
	u64 xoffrxc;
	u64 xofftxc;
	u64 fcruc;
	u64 prc64;
	u64 prc127;
	u64 prc255;
	u64 prc511;
	u64 prc1023;
	u64 prc1522;
	u64 gprc;
	u64 bprc;
	u64 mprc;
	u64 gptc;
	u64 gorcl;
	u64 gorch;
	u64 gotcl;
	u64 gotch;
	u64 rnbc;
	u64 ruc;
	u64 rfc;
	u64 roc;
	u64 rlerrc;
	u64 rjc;
	u64 mgprc;
	u64 mgpdc;
	u64 mgptc;
	u64 torl;
	u64 torh;
	u64 totl;
	u64 toth;
	u64 tpr;
	u64 tpt;
	u64 ptc64;
	u64 ptc127;
	u64 ptc255;
	u64 ptc511;
	u64 ptc1023;
	u64 ptc1522;
	u64 mptc;
	u64 bptc;
	u64 tsctc;
	u64 tsctfc;
	u64 iac;
	u64 icrxptc;
	u64 icrxatc;
	u64 ictxptc;
	u64 ictxatc;
	u64 ictxqec;
	u64 ictxqmtc;
	u64 icrxdmtc;
	u64 icrxoc;
};

struct e1000_hw {
	u8 __iomem *hw_addr;
	u8 __iomem *flash_address;
	void __iomem *ce4100_gbe_mdio_base_virt;
	e1000_mac_type mac_type;
	e1000_phy_type phy_type;
	u32 phy_init_script;
	e1000_media_type media_type;
	void *back;
	struct e1000_shadow_ram *eeprom_shadow_ram;
	u32 flash_bank_size;
	u32 flash_base_addr;
	e1000_fc_type fc;
	e1000_bus_speed bus_speed;
	e1000_bus_width bus_width;
	e1000_bus_type bus_type;
	struct e1000_eeprom_info eeprom;
	e1000_ms_type master_slave;
	e1000_ms_type original_master_slave;
	e1000_ffe_config ffe_config_state;
	u32 asf_firmware_present;
	u32 eeprom_semaphore_present;
	unsigned long io_base;
	u32 phy_id;
	u32 phy_revision;
	u32 phy_addr;
	u32 original_fc;
	u32 txcw;
	u32 autoneg_failed;
	u32 max_frame_size;
	u32 min_frame_size;
	u32 mc_filter_type;
	u32 num_mc_addrs;
	u32 collision_delta;
	u32 tx_packet_delta;
	u32 ledctl_default;
	u32 ledctl_mode1;
	u32 ledctl_mode2;
	bool tx_pkt_filtering;
	struct e1000_host_mng_dhcp_cookie mng_cookie;
	u16 phy_spd_default;
	u16 autoneg_advertised;
	u16 pci_cmd_word;
	u16 fc_high_water;
	u16 fc_low_water;
	u16 fc_pause_time;
	u16 current_ifs_val;
	u16 ifs_min_val;
	u16 ifs_max_val;
	u16 ifs_step_size;
	u16 ifs_ratio;
	u16 device_id;
	u16 vendor_id;
	u16 subsystem_id;
	u16 subsystem_vendor_id;
	u8 revision_id;
	u8 autoneg;
	u8 mdix;
	u8 forced_speed_duplex;
	u8 wait_autoneg_complete;
	u8 dma_fairness;
	u8 mac_addr[NODE_ADDRESS_SIZE];
	u8 perm_mac_addr[NODE_ADDRESS_SIZE];
	bool disable_polarity_correction;
	bool speed_downgraded;
	e1000_smart_speed smart_speed;
	e1000_dsp_config dsp_config_state;
	bool get_link_status;
	bool serdes_has_link;
	bool tbi_compatibility_en;
	bool tbi_compatibility_on;
	bool laa_is_present;
	bool phy_reset_disable;
	bool initialize_hw_bits_disable;
	bool fc_send_xon;
	bool fc_strict_ieee;
	bool report_tx_early;
	bool adaptive_ifs;
	bool ifs_params_forced;
	bool in_ifs_mode;
	bool mng_reg_access_disabled;
	bool leave_av_bit_off;
	bool bad_tx_carr_stats_fd;
	bool has_smbus;
};

#define E1000_EEPROM_SWDPIN0   0x0001	
#define E1000_EEPROM_LED_LOGIC 0x0020	
#define E1000_EEPROM_RW_REG_DATA   16	
#define E1000_EEPROM_RW_REG_DONE   2	
#define E1000_EEPROM_RW_REG_START  1	
#define E1000_EEPROM_RW_ADDR_SHIFT 2	
#define E1000_EEPROM_POLL_WRITE    1	
#define E1000_EEPROM_POLL_READ     0	
#define E1000_CTRL_FD       0x00000001	
#define E1000_CTRL_BEM      0x00000002	
#define E1000_CTRL_PRIOR    0x00000004	
#define E1000_CTRL_GIO_MASTER_DISABLE 0x00000004	
#define E1000_CTRL_LRST     0x00000008	
#define E1000_CTRL_TME      0x00000010	
#define E1000_CTRL_SLE      0x00000020	
#define E1000_CTRL_ASDE     0x00000020	
#define E1000_CTRL_SLU      0x00000040	
#define E1000_CTRL_ILOS     0x00000080	
#define E1000_CTRL_SPD_SEL  0x00000300	
#define E1000_CTRL_SPD_10   0x00000000	
#define E1000_CTRL_SPD_100  0x00000100	
#define E1000_CTRL_SPD_1000 0x00000200	
#define E1000_CTRL_BEM32    0x00000400	
#define E1000_CTRL_FRCSPD   0x00000800	
#define E1000_CTRL_FRCDPX   0x00001000	
#define E1000_CTRL_D_UD_EN  0x00002000	
#define E1000_CTRL_D_UD_POLARITY 0x00004000	
#define E1000_CTRL_FORCE_PHY_RESET 0x00008000	
#define E1000_CTRL_EXT_LINK_EN 0x00010000	
#define E1000_CTRL_SWDPIN0  0x00040000	
#define E1000_CTRL_SWDPIN1  0x00080000	
#define E1000_CTRL_SWDPIN2  0x00100000	
#define E1000_CTRL_SWDPIN3  0x00200000	
#define E1000_CTRL_SWDPIO0  0x00400000	
#define E1000_CTRL_SWDPIO1  0x00800000	
#define E1000_CTRL_SWDPIO2  0x01000000	
#define E1000_CTRL_SWDPIO3  0x02000000	
#define E1000_CTRL_RST      0x04000000	
#define E1000_CTRL_RFCE     0x08000000	
#define E1000_CTRL_TFCE     0x10000000	
#define E1000_CTRL_RTE      0x20000000	
#define E1000_CTRL_VME      0x40000000	
#define E1000_CTRL_PHY_RST  0x80000000	
#define E1000_CTRL_SW2FW_INT 0x02000000	

#define E1000_STATUS_FD         0x00000001	
#define E1000_STATUS_LU         0x00000002	
#define E1000_STATUS_FUNC_MASK  0x0000000C	
#define E1000_STATUS_FUNC_SHIFT 2
#define E1000_STATUS_FUNC_0     0x00000000	
#define E1000_STATUS_FUNC_1     0x00000004	
#define E1000_STATUS_TXOFF      0x00000010	
#define E1000_STATUS_TBIMODE    0x00000020	
#define E1000_STATUS_SPEED_MASK 0x000000C0
#define E1000_STATUS_SPEED_10   0x00000000	
#define E1000_STATUS_SPEED_100  0x00000040	
#define E1000_STATUS_SPEED_1000 0x00000080	
#define E1000_STATUS_LAN_INIT_DONE 0x00000200	
#define E1000_STATUS_ASDV       0x00000300	
#define E1000_STATUS_DOCK_CI    0x00000800	
#define E1000_STATUS_GIO_MASTER_ENABLE 0x00080000	
#define E1000_STATUS_MTXCKOK    0x00000400	
#define E1000_STATUS_PCI66      0x00000800	
#define E1000_STATUS_BUS64      0x00001000	
#define E1000_STATUS_PCIX_MODE  0x00002000	
#define E1000_STATUS_PCIX_SPEED 0x0000C000	
#define E1000_STATUS_BMC_SKU_0  0x00100000	
#define E1000_STATUS_BMC_SKU_1  0x00200000	
#define E1000_STATUS_BMC_SKU_2  0x00400000	
#define E1000_STATUS_BMC_CRYPTO 0x00800000	
#define E1000_STATUS_BMC_LITE   0x01000000	
#define E1000_STATUS_RGMII_ENABLE 0x02000000	
#define E1000_STATUS_FUSE_8       0x04000000
#define E1000_STATUS_FUSE_9       0x08000000
#define E1000_STATUS_SERDES0_DIS  0x10000000	
#define E1000_STATUS_SERDES1_DIS  0x20000000	

#define E1000_STATUS_PCIX_SPEED_66  0x00000000	
#define E1000_STATUS_PCIX_SPEED_100 0x00004000	
#define E1000_STATUS_PCIX_SPEED_133 0x00008000	

#define E1000_EECD_SK        0x00000001	
#define E1000_EECD_CS        0x00000002	
#define E1000_EECD_DI        0x00000004	
#define E1000_EECD_DO        0x00000008	
#define E1000_EECD_FWE_MASK  0x00000030
#define E1000_EECD_FWE_DIS   0x00000010	
#define E1000_EECD_FWE_EN    0x00000020	
#define E1000_EECD_FWE_SHIFT 4
#define E1000_EECD_REQ       0x00000040	
#define E1000_EECD_GNT       0x00000080	
#define E1000_EECD_PRES      0x00000100	
#define E1000_EECD_SIZE      0x00000200	
#define E1000_EECD_ADDR_BITS 0x00000400	
#define E1000_EECD_TYPE      0x00002000	
#ifndef E1000_EEPROM_GRANT_ATTEMPTS
#define E1000_EEPROM_GRANT_ATTEMPTS 1000	
#endif
#define E1000_EECD_AUTO_RD          0x00000200	
#define E1000_EECD_SIZE_EX_MASK     0x00007800	
#define E1000_EECD_SIZE_EX_SHIFT    11
#define E1000_EECD_NVADDS    0x00018000	
#define E1000_EECD_SELSHAD   0x00020000	
#define E1000_EECD_INITSRAM  0x00040000	
#define E1000_EECD_FLUPD     0x00080000	
#define E1000_EECD_AUPDEN    0x00100000	
#define E1000_EECD_SHADV     0x00200000	
#define E1000_EECD_SEC1VAL   0x00400000	
#define E1000_EECD_SECVAL_SHIFT      22
#define E1000_STM_OPCODE     0xDB00
#define E1000_HICR_FW_RESET  0xC0

#define E1000_SHADOW_RAM_WORDS     2048
#define E1000_ICH_NVM_SIG_WORD     0x13
#define E1000_ICH_NVM_SIG_MASK     0xC0

#define E1000_EERD_START      0x00000001	
#define E1000_EERD_DONE       0x00000010	
#define E1000_EERD_ADDR_SHIFT 8
#define E1000_EERD_ADDR_MASK  0x0000FF00	
#define E1000_EERD_DATA_SHIFT 16
#define E1000_EERD_DATA_MASK  0xFFFF0000	

#define EEPROM_STATUS_RDY_SPI  0x01
#define EEPROM_STATUS_WEN_SPI  0x02
#define EEPROM_STATUS_BP0_SPI  0x04
#define EEPROM_STATUS_BP1_SPI  0x08
#define EEPROM_STATUS_WPEN_SPI 0x80

#define E1000_CTRL_EXT_GPI0_EN   0x00000001	
#define E1000_CTRL_EXT_GPI1_EN   0x00000002	
#define E1000_CTRL_EXT_PHYINT_EN E1000_CTRL_EXT_GPI1_EN
#define E1000_CTRL_EXT_GPI2_EN   0x00000004	
#define E1000_CTRL_EXT_GPI3_EN   0x00000008	
#define E1000_CTRL_EXT_SDP4_DATA 0x00000010	
#define E1000_CTRL_EXT_SDP5_DATA 0x00000020	
#define E1000_CTRL_EXT_PHY_INT   E1000_CTRL_EXT_SDP5_DATA
#define E1000_CTRL_EXT_SDP6_DATA 0x00000040	
#define E1000_CTRL_EXT_SDP7_DATA 0x00000080	
#define E1000_CTRL_EXT_SDP4_DIR  0x00000100	
#define E1000_CTRL_EXT_SDP5_DIR  0x00000200	
#define E1000_CTRL_EXT_SDP6_DIR  0x00000400	
#define E1000_CTRL_EXT_SDP7_DIR  0x00000800	
#define E1000_CTRL_EXT_ASDCHK    0x00001000	
#define E1000_CTRL_EXT_EE_RST    0x00002000	
#define E1000_CTRL_EXT_IPS       0x00004000	
#define E1000_CTRL_EXT_SPD_BYPS  0x00008000	
#define E1000_CTRL_EXT_RO_DIS    0x00020000	
#define E1000_CTRL_EXT_LINK_MODE_MASK 0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_GMII 0x00000000
#define E1000_CTRL_EXT_LINK_MODE_TBI  0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_KMRN 0x00000000
#define E1000_CTRL_EXT_LINK_MODE_SERDES  0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_SGMII   0x00800000
#define E1000_CTRL_EXT_WR_WMARK_MASK  0x03000000
#define E1000_CTRL_EXT_WR_WMARK_256   0x00000000
#define E1000_CTRL_EXT_WR_WMARK_320   0x01000000
#define E1000_CTRL_EXT_WR_WMARK_384   0x02000000
#define E1000_CTRL_EXT_WR_WMARK_448   0x03000000
#define E1000_CTRL_EXT_DRV_LOAD       0x10000000	
#define E1000_CTRL_EXT_IAME           0x08000000	
#define E1000_CTRL_EXT_INT_TIMER_CLR  0x20000000	
#define E1000_CRTL_EXT_PB_PAREN       0x01000000	
#define E1000_CTRL_EXT_DF_PAREN       0x02000000	
#define E1000_CTRL_EXT_GHOST_PAREN    0x40000000

#define E1000_MDIC_DATA_MASK 0x0000FFFF
#define E1000_MDIC_REG_MASK  0x001F0000
#define E1000_MDIC_REG_SHIFT 16
#define E1000_MDIC_PHY_MASK  0x03E00000
#define E1000_MDIC_PHY_SHIFT 21
#define E1000_MDIC_OP_WRITE  0x04000000
#define E1000_MDIC_OP_READ   0x08000000
#define E1000_MDIC_READY     0x10000000
#define E1000_MDIC_INT_EN    0x20000000
#define E1000_MDIC_ERROR     0x40000000

#define INTEL_CE_GBE_MDIC_OP_WRITE      0x04000000
#define INTEL_CE_GBE_MDIC_OP_READ       0x00000000
#define INTEL_CE_GBE_MDIC_GO            0x80000000
#define INTEL_CE_GBE_MDIC_READ_ERROR    0x80000000

#define E1000_KUMCTRLSTA_MASK           0x0000FFFF
#define E1000_KUMCTRLSTA_OFFSET         0x001F0000
#define E1000_KUMCTRLSTA_OFFSET_SHIFT   16
#define E1000_KUMCTRLSTA_REN            0x00200000

#define E1000_KUMCTRLSTA_OFFSET_FIFO_CTRL      0x00000000
#define E1000_KUMCTRLSTA_OFFSET_CTRL           0x00000001
#define E1000_KUMCTRLSTA_OFFSET_INB_CTRL       0x00000002
#define E1000_KUMCTRLSTA_OFFSET_DIAG           0x00000003
#define E1000_KUMCTRLSTA_OFFSET_TIMEOUTS       0x00000004
#define E1000_KUMCTRLSTA_OFFSET_INB_PARAM      0x00000009
#define E1000_KUMCTRLSTA_OFFSET_HD_CTRL        0x00000010
#define E1000_KUMCTRLSTA_OFFSET_M2P_SERDES     0x0000001E
#define E1000_KUMCTRLSTA_OFFSET_M2P_MODES      0x0000001F

#define E1000_KUMCTRLSTA_FIFO_CTRL_RX_BYPASS   0x00000008
#define E1000_KUMCTRLSTA_FIFO_CTRL_TX_BYPASS   0x00000800

#define E1000_KUMCTRLSTA_INB_CTRL_LINK_STATUS_TX_TIMEOUT_DEFAULT    0x00000500
#define E1000_KUMCTRLSTA_INB_CTRL_DIS_PADDING  0x00000010

#define E1000_KUMCTRLSTA_HD_CTRL_10_100_DEFAULT 0x00000004
#define E1000_KUMCTRLSTA_HD_CTRL_1000_DEFAULT  0x00000000

#define E1000_KUMCTRLSTA_OFFSET_K0S_CTRL       0x0000001E

#define E1000_KUMCTRLSTA_DIAG_FELPBK           0x2000
#define E1000_KUMCTRLSTA_DIAG_NELPBK           0x1000

#define E1000_KUMCTRLSTA_K0S_100_EN            0x2000
#define E1000_KUMCTRLSTA_K0S_GBE_EN            0x1000
#define E1000_KUMCTRLSTA_K0S_ENTRY_LATENCY_MASK   0x0003

#define E1000_KABGTXD_BGSQLBIAS                0x00050000

#define E1000_PHY_CTRL_SPD_EN                  0x00000001
#define E1000_PHY_CTRL_D0A_LPLU                0x00000002
#define E1000_PHY_CTRL_NOND0A_LPLU             0x00000004
#define E1000_PHY_CTRL_NOND0A_GBE_DISABLE      0x00000008
#define E1000_PHY_CTRL_GBE_DISABLE             0x00000040
#define E1000_PHY_CTRL_B2B_EN                  0x00000080

#define E1000_LEDCTL_LED0_MODE_MASK       0x0000000F
#define E1000_LEDCTL_LED0_MODE_SHIFT      0
#define E1000_LEDCTL_LED0_BLINK_RATE      0x0000020
#define E1000_LEDCTL_LED0_IVRT            0x00000040
#define E1000_LEDCTL_LED0_BLINK           0x00000080
#define E1000_LEDCTL_LED1_MODE_MASK       0x00000F00
#define E1000_LEDCTL_LED1_MODE_SHIFT      8
#define E1000_LEDCTL_LED1_BLINK_RATE      0x0002000
#define E1000_LEDCTL_LED1_IVRT            0x00004000
#define E1000_LEDCTL_LED1_BLINK           0x00008000
#define E1000_LEDCTL_LED2_MODE_MASK       0x000F0000
#define E1000_LEDCTL_LED2_MODE_SHIFT      16
#define E1000_LEDCTL_LED2_BLINK_RATE      0x00200000
#define E1000_LEDCTL_LED2_IVRT            0x00400000
#define E1000_LEDCTL_LED2_BLINK           0x00800000
#define E1000_LEDCTL_LED3_MODE_MASK       0x0F000000
#define E1000_LEDCTL_LED3_MODE_SHIFT      24
#define E1000_LEDCTL_LED3_BLINK_RATE      0x20000000
#define E1000_LEDCTL_LED3_IVRT            0x40000000
#define E1000_LEDCTL_LED3_BLINK           0x80000000

#define E1000_LEDCTL_MODE_LINK_10_1000  0x0
#define E1000_LEDCTL_MODE_LINK_100_1000 0x1
#define E1000_LEDCTL_MODE_LINK_UP       0x2
#define E1000_LEDCTL_MODE_ACTIVITY      0x3
#define E1000_LEDCTL_MODE_LINK_ACTIVITY 0x4
#define E1000_LEDCTL_MODE_LINK_10       0x5
#define E1000_LEDCTL_MODE_LINK_100      0x6
#define E1000_LEDCTL_MODE_LINK_1000     0x7
#define E1000_LEDCTL_MODE_PCIX_MODE     0x8
#define E1000_LEDCTL_MODE_FULL_DUPLEX   0x9
#define E1000_LEDCTL_MODE_COLLISION     0xA
#define E1000_LEDCTL_MODE_BUS_SPEED     0xB
#define E1000_LEDCTL_MODE_BUS_SIZE      0xC
#define E1000_LEDCTL_MODE_PAUSED        0xD
#define E1000_LEDCTL_MODE_LED_ON        0xE
#define E1000_LEDCTL_MODE_LED_OFF       0xF

#define E1000_RAH_AV  0x80000000	

#define E1000_ICR_TXDW          0x00000001	/* Transmit desc written back */
#define E1000_ICR_TXQE          0x00000002	
#define E1000_ICR_LSC           0x00000004	
#define E1000_ICR_RXSEQ         0x00000008	
#define E1000_ICR_RXDMT0        0x00000010	
#define E1000_ICR_RXO           0x00000040	
#define E1000_ICR_RXT0          0x00000080	
#define E1000_ICR_MDAC          0x00000200	
#define E1000_ICR_RXCFG         0x00000400	
#define E1000_ICR_GPI_EN0       0x00000800	
#define E1000_ICR_GPI_EN1       0x00001000	
#define E1000_ICR_GPI_EN2       0x00002000	
#define E1000_ICR_GPI_EN3       0x00004000	
#define E1000_ICR_TXD_LOW       0x00008000
#define E1000_ICR_SRPD          0x00010000
#define E1000_ICR_ACK           0x00020000	
#define E1000_ICR_MNG           0x00040000	
#define E1000_ICR_DOCK          0x00080000	
#define E1000_ICR_INT_ASSERTED  0x80000000	
#define E1000_ICR_RXD_FIFO_PAR0 0x00100000	
#define E1000_ICR_TXD_FIFO_PAR0 0x00200000	
#define E1000_ICR_HOST_ARB_PAR  0x00400000	
#define E1000_ICR_PB_PAR        0x00800000	
#define E1000_ICR_RXD_FIFO_PAR1 0x01000000	
#define E1000_ICR_TXD_FIFO_PAR1 0x02000000	
#define E1000_ICR_ALL_PARITY    0x03F00000	
#define E1000_ICR_DSW           0x00000020	
#define E1000_ICR_PHYINT        0x00001000	
#define E1000_ICR_EPRST         0x00100000	

#define E1000_ICS_TXDW      E1000_ICR_TXDW	/* Transmit desc written back */
#define E1000_ICS_TXQE      E1000_ICR_TXQE	
#define E1000_ICS_LSC       E1000_ICR_LSC	
#define E1000_ICS_RXSEQ     E1000_ICR_RXSEQ	
#define E1000_ICS_RXDMT0    E1000_ICR_RXDMT0	
#define E1000_ICS_RXO       E1000_ICR_RXO	
#define E1000_ICS_RXT0      E1000_ICR_RXT0	
#define E1000_ICS_MDAC      E1000_ICR_MDAC	
#define E1000_ICS_RXCFG     E1000_ICR_RXCFG	
#define E1000_ICS_GPI_EN0   E1000_ICR_GPI_EN0	
#define E1000_ICS_GPI_EN1   E1000_ICR_GPI_EN1	
#define E1000_ICS_GPI_EN2   E1000_ICR_GPI_EN2	
#define E1000_ICS_GPI_EN3   E1000_ICR_GPI_EN3	
#define E1000_ICS_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_ICS_SRPD      E1000_ICR_SRPD
#define E1000_ICS_ACK       E1000_ICR_ACK	
#define E1000_ICS_MNG       E1000_ICR_MNG	
#define E1000_ICS_DOCK      E1000_ICR_DOCK	
#define E1000_ICS_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0	
#define E1000_ICS_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0	
#define E1000_ICS_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR	
#define E1000_ICS_PB_PAR        E1000_ICR_PB_PAR	
#define E1000_ICS_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1	
#define E1000_ICS_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1	
#define E1000_ICS_DSW       E1000_ICR_DSW
#define E1000_ICS_PHYINT    E1000_ICR_PHYINT
#define E1000_ICS_EPRST     E1000_ICR_EPRST

#define E1000_IMS_TXDW      E1000_ICR_TXDW	/* Transmit desc written back */
#define E1000_IMS_TXQE      E1000_ICR_TXQE	
#define E1000_IMS_LSC       E1000_ICR_LSC	
#define E1000_IMS_RXSEQ     E1000_ICR_RXSEQ	
#define E1000_IMS_RXDMT0    E1000_ICR_RXDMT0	
#define E1000_IMS_RXO       E1000_ICR_RXO	
#define E1000_IMS_RXT0      E1000_ICR_RXT0	
#define E1000_IMS_MDAC      E1000_ICR_MDAC	
#define E1000_IMS_RXCFG     E1000_ICR_RXCFG	
#define E1000_IMS_GPI_EN0   E1000_ICR_GPI_EN0	
#define E1000_IMS_GPI_EN1   E1000_ICR_GPI_EN1	
#define E1000_IMS_GPI_EN2   E1000_ICR_GPI_EN2	
#define E1000_IMS_GPI_EN3   E1000_ICR_GPI_EN3	
#define E1000_IMS_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_IMS_SRPD      E1000_ICR_SRPD
#define E1000_IMS_ACK       E1000_ICR_ACK	
#define E1000_IMS_MNG       E1000_ICR_MNG	
#define E1000_IMS_DOCK      E1000_ICR_DOCK	
#define E1000_IMS_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0	
#define E1000_IMS_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0	
#define E1000_IMS_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR	
#define E1000_IMS_PB_PAR        E1000_ICR_PB_PAR	
#define E1000_IMS_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1	
#define E1000_IMS_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1	
#define E1000_IMS_DSW       E1000_ICR_DSW
#define E1000_IMS_PHYINT    E1000_ICR_PHYINT
#define E1000_IMS_EPRST     E1000_ICR_EPRST

#define E1000_IMC_TXDW      E1000_ICR_TXDW	/* Transmit desc written back */
#define E1000_IMC_TXQE      E1000_ICR_TXQE	
#define E1000_IMC_LSC       E1000_ICR_LSC	
#define E1000_IMC_RXSEQ     E1000_ICR_RXSEQ	
#define E1000_IMC_RXDMT0    E1000_ICR_RXDMT0	
#define E1000_IMC_RXO       E1000_ICR_RXO	
#define E1000_IMC_RXT0      E1000_ICR_RXT0	
#define E1000_IMC_MDAC      E1000_ICR_MDAC	
#define E1000_IMC_RXCFG     E1000_ICR_RXCFG	
#define E1000_IMC_GPI_EN0   E1000_ICR_GPI_EN0	
#define E1000_IMC_GPI_EN1   E1000_ICR_GPI_EN1	
#define E1000_IMC_GPI_EN2   E1000_ICR_GPI_EN2	
#define E1000_IMC_GPI_EN3   E1000_ICR_GPI_EN3	
#define E1000_IMC_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_IMC_SRPD      E1000_ICR_SRPD
#define E1000_IMC_ACK       E1000_ICR_ACK	
#define E1000_IMC_MNG       E1000_ICR_MNG	
#define E1000_IMC_DOCK      E1000_ICR_DOCK	
#define E1000_IMC_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0	
#define E1000_IMC_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0	
#define E1000_IMC_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR	
#define E1000_IMC_PB_PAR        E1000_ICR_PB_PAR	
#define E1000_IMC_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1	
#define E1000_IMC_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1	
#define E1000_IMC_DSW       E1000_ICR_DSW
#define E1000_IMC_PHYINT    E1000_ICR_PHYINT
#define E1000_IMC_EPRST     E1000_ICR_EPRST

#define E1000_RCTL_RST            0x00000001	
#define E1000_RCTL_EN             0x00000002	
#define E1000_RCTL_SBP            0x00000004	
#define E1000_RCTL_UPE            0x00000008	
#define E1000_RCTL_MPE            0x00000010	
#define E1000_RCTL_LPE            0x00000020	
#define E1000_RCTL_LBM_NO         0x00000000	
#define E1000_RCTL_LBM_MAC        0x00000040	
#define E1000_RCTL_LBM_SLP        0x00000080	
#define E1000_RCTL_LBM_TCVR       0x000000C0	
#define E1000_RCTL_DTYP_MASK      0x00000C00	
#define E1000_RCTL_DTYP_PS        0x00000400	
#define E1000_RCTL_RDMTS_HALF     0x00000000	
#define E1000_RCTL_RDMTS_QUAT     0x00000100	
#define E1000_RCTL_RDMTS_EIGTH    0x00000200	
#define E1000_RCTL_MO_SHIFT       12	
#define E1000_RCTL_MO_0           0x00000000	
#define E1000_RCTL_MO_1           0x00001000	
#define E1000_RCTL_MO_2           0x00002000	
#define E1000_RCTL_MO_3           0x00003000	
#define E1000_RCTL_MDR            0x00004000	
#define E1000_RCTL_BAM            0x00008000	
#define E1000_RCTL_SZ_2048        0x00000000	
#define E1000_RCTL_SZ_1024        0x00010000	
#define E1000_RCTL_SZ_512         0x00020000	
#define E1000_RCTL_SZ_256         0x00030000	
#define E1000_RCTL_SZ_16384       0x00010000	
#define E1000_RCTL_SZ_8192        0x00020000	
#define E1000_RCTL_SZ_4096        0x00030000	
#define E1000_RCTL_VFE            0x00040000	
#define E1000_RCTL_CFIEN          0x00080000	
#define E1000_RCTL_CFI            0x00100000	
#define E1000_RCTL_DPF            0x00400000	
#define E1000_RCTL_PMCF           0x00800000	
#define E1000_RCTL_BSEX           0x02000000	
#define E1000_RCTL_SECRC          0x04000000	
#define E1000_RCTL_FLXBUF_MASK    0x78000000	
#define E1000_RCTL_FLXBUF_SHIFT   27	


#define E1000_PSRCTL_BSIZE0_MASK   0x0000007F
#define E1000_PSRCTL_BSIZE1_MASK   0x00003F00
#define E1000_PSRCTL_BSIZE2_MASK   0x003F0000
#define E1000_PSRCTL_BSIZE3_MASK   0x3F000000

#define E1000_PSRCTL_BSIZE0_SHIFT  7	
#define E1000_PSRCTL_BSIZE1_SHIFT  2	
#define E1000_PSRCTL_BSIZE2_SHIFT  6	
#define E1000_PSRCTL_BSIZE3_SHIFT 14	

#define E1000_SWFW_EEP_SM     0x0001
#define E1000_SWFW_PHY0_SM    0x0002
#define E1000_SWFW_PHY1_SM    0x0004
#define E1000_SWFW_MAC_CSR_SM 0x0008

#define E1000_RDT_DELAY 0x0000ffff	
#define E1000_RDT_FPDB  0x80000000	
#define E1000_RDLEN_LEN 0x0007ff80	
#define E1000_RDH_RDH   0x0000ffff	
#define E1000_RDT_RDT   0x0000ffff	

#define E1000_FCRTH_RTH  0x0000FFF8	
#define E1000_FCRTH_XFCE 0x80000000	
#define E1000_FCRTL_RTL  0x0000FFF8	
#define E1000_FCRTL_XONE 0x80000000	

#define E1000_RFCTL_ISCSI_DIS           0x00000001
#define E1000_RFCTL_ISCSI_DWC_MASK      0x0000003E
#define E1000_RFCTL_ISCSI_DWC_SHIFT     1
#define E1000_RFCTL_NFSW_DIS            0x00000040
#define E1000_RFCTL_NFSR_DIS            0x00000080
#define E1000_RFCTL_NFS_VER_MASK        0x00000300
#define E1000_RFCTL_NFS_VER_SHIFT       8
#define E1000_RFCTL_IPV6_DIS            0x00000400
#define E1000_RFCTL_IPV6_XSUM_DIS       0x00000800
#define E1000_RFCTL_ACK_DIS             0x00001000
#define E1000_RFCTL_ACKD_DIS            0x00002000
#define E1000_RFCTL_IPFRSP_DIS          0x00004000
#define E1000_RFCTL_EXTEN               0x00008000
#define E1000_RFCTL_IPV6_EX_DIS         0x00010000
#define E1000_RFCTL_NEW_IPV6_EXT_DIS    0x00020000

#define E1000_RXDCTL_PTHRESH 0x0000003F	
#define E1000_RXDCTL_HTHRESH 0x00003F00	
#define E1000_RXDCTL_WTHRESH 0x003F0000	
#define E1000_RXDCTL_GRAN    0x01000000	

#define E1000_TXDCTL_PTHRESH 0x0000003F	
#define E1000_TXDCTL_HTHRESH 0x00003F00	
#define E1000_TXDCTL_WTHRESH 0x003F0000	
#define E1000_TXDCTL_GRAN    0x01000000	
#define E1000_TXDCTL_LWTHRESH 0xFE000000	
#define E1000_TXDCTL_FULL_TX_DESC_WB 0x01010000	
#define E1000_TXDCTL_COUNT_DESC 0x00400000	
#define E1000_TXCW_FD         0x00000020	
#define E1000_TXCW_HD         0x00000040	
#define E1000_TXCW_PAUSE      0x00000080	
#define E1000_TXCW_ASM_DIR    0x00000100	
#define E1000_TXCW_PAUSE_MASK 0x00000180	
#define E1000_TXCW_RF         0x00003000	
#define E1000_TXCW_NP         0x00008000	
#define E1000_TXCW_CW         0x0000ffff	
#define E1000_TXCW_TXC        0x40000000	
#define E1000_TXCW_ANE        0x80000000	

#define E1000_RXCW_CW    0x0000ffff	
#define E1000_RXCW_NC    0x04000000	
#define E1000_RXCW_IV    0x08000000	
#define E1000_RXCW_CC    0x10000000	
#define E1000_RXCW_C     0x20000000	
#define E1000_RXCW_SYNCH 0x40000000	
#define E1000_RXCW_ANC   0x80000000	

#define E1000_TCTL_RST    0x00000001	
#define E1000_TCTL_EN     0x00000002	
#define E1000_TCTL_BCE    0x00000004	
#define E1000_TCTL_PSP    0x00000008	
#define E1000_TCTL_CT     0x00000ff0	
#define E1000_TCTL_COLD   0x003ff000	
#define E1000_TCTL_SWXOFF 0x00400000	
#define E1000_TCTL_PBE    0x00800000	
#define E1000_TCTL_RTLC   0x01000000	
#define E1000_TCTL_NRTU   0x02000000	
#define E1000_TCTL_MULR   0x10000000	
#define E1000_TCTL_EXT_BST_MASK  0x000003FF	
#define E1000_TCTL_EXT_GCEX_MASK 0x000FFC00	

#define E1000_RXCSUM_PCSS_MASK 0x000000FF	
#define E1000_RXCSUM_IPOFL     0x00000100	
#define E1000_RXCSUM_TUOFL     0x00000200	
#define E1000_RXCSUM_IPV6OFL   0x00000400	
#define E1000_RXCSUM_IPPCSE    0x00001000	
#define E1000_RXCSUM_PCSD      0x00002000	

#define E1000_MRQC_ENABLE_MASK              0x00000003
#define E1000_MRQC_ENABLE_RSS_2Q            0x00000001
#define E1000_MRQC_ENABLE_RSS_INT           0x00000004
#define E1000_MRQC_RSS_FIELD_MASK           0xFFFF0000
#define E1000_MRQC_RSS_FIELD_IPV4_TCP       0x00010000
#define E1000_MRQC_RSS_FIELD_IPV4           0x00020000
#define E1000_MRQC_RSS_FIELD_IPV6_TCP_EX    0x00040000
#define E1000_MRQC_RSS_FIELD_IPV6_EX        0x00080000
#define E1000_MRQC_RSS_FIELD_IPV6           0x00100000
#define E1000_MRQC_RSS_FIELD_IPV6_TCP       0x00200000

#define E1000_WUC_APME       0x00000001	
#define E1000_WUC_PME_EN     0x00000002	
#define E1000_WUC_PME_STATUS 0x00000004	
#define E1000_WUC_APMPME     0x00000008	
#define E1000_WUC_SPM        0x80000000	

#define E1000_WUFC_LNKC 0x00000001	
#define E1000_WUFC_MAG  0x00000002	
#define E1000_WUFC_EX   0x00000004	
#define E1000_WUFC_MC   0x00000008	
#define E1000_WUFC_BC   0x00000010	
#define E1000_WUFC_ARP  0x00000020	
#define E1000_WUFC_IPV4 0x00000040	
#define E1000_WUFC_IPV6 0x00000080	
#define E1000_WUFC_IGNORE_TCO      0x00008000	
#define E1000_WUFC_FLX0 0x00010000	
#define E1000_WUFC_FLX1 0x00020000	
#define E1000_WUFC_FLX2 0x00040000	
#define E1000_WUFC_FLX3 0x00080000	
#define E1000_WUFC_ALL_FILTERS 0x000F00FF	
#define E1000_WUFC_FLX_OFFSET 16	
#define E1000_WUFC_FLX_FILTERS 0x000F0000	

#define E1000_WUS_LNKC 0x00000001	
#define E1000_WUS_MAG  0x00000002	
#define E1000_WUS_EX   0x00000004	
#define E1000_WUS_MC   0x00000008	
#define E1000_WUS_BC   0x00000010	
#define E1000_WUS_ARP  0x00000020	
#define E1000_WUS_IPV4 0x00000040	
#define E1000_WUS_IPV6 0x00000080	
#define E1000_WUS_FLX0 0x00010000	
#define E1000_WUS_FLX1 0x00020000	
#define E1000_WUS_FLX2 0x00040000	
#define E1000_WUS_FLX3 0x00080000	
#define E1000_WUS_FLX_FILTERS 0x000F0000	

#define E1000_MANC_SMBUS_EN      0x00000001	
#define E1000_MANC_ASF_EN        0x00000002	
#define E1000_MANC_R_ON_FORCE    0x00000004	
#define E1000_MANC_RMCP_EN       0x00000100	
#define E1000_MANC_0298_EN       0x00000200	
#define E1000_MANC_IPV4_EN       0x00000400	
#define E1000_MANC_IPV6_EN       0x00000800	
#define E1000_MANC_SNAP_EN       0x00001000	
#define E1000_MANC_ARP_EN        0x00002000	
#define E1000_MANC_NEIGHBOR_EN   0x00004000	
#define E1000_MANC_ARP_RES_EN    0x00008000	
#define E1000_MANC_TCO_RESET     0x00010000	
#define E1000_MANC_RCV_TCO_EN    0x00020000	
#define E1000_MANC_REPORT_STATUS 0x00040000	
#define E1000_MANC_RCV_ALL       0x00080000	
#define E1000_MANC_BLK_PHY_RST_ON_IDE   0x00040000	
#define E1000_MANC_EN_MAC_ADDR_FILTER   0x00100000	
#define E1000_MANC_EN_MNG2HOST   0x00200000	
#define E1000_MANC_EN_IP_ADDR_FILTER    0x00400000	
#define E1000_MANC_EN_XSUM_FILTER   0x00800000	
#define E1000_MANC_BR_EN         0x01000000	
#define E1000_MANC_SMB_REQ       0x01000000	
#define E1000_MANC_SMB_GNT       0x02000000	
#define E1000_MANC_SMB_CLK_IN    0x04000000	
#define E1000_MANC_SMB_DATA_IN   0x08000000	
#define E1000_MANC_SMB_DATA_OUT  0x10000000	
#define E1000_MANC_SMB_CLK_OUT   0x20000000	

#define E1000_MANC_SMB_DATA_OUT_SHIFT  28	
#define E1000_MANC_SMB_CLK_OUT_SHIFT   29	

#define E1000_SWSM_SMBI         0x00000001	
#define E1000_SWSM_SWESMBI      0x00000002	
#define E1000_SWSM_WMNG         0x00000004	
#define E1000_SWSM_DRV_LOAD     0x00000008	

#define E1000_FWSM_MODE_MASK    0x0000000E	
#define E1000_FWSM_MODE_SHIFT            1
#define E1000_FWSM_FW_VALID     0x00008000	

#define E1000_FWSM_RSPCIPHY        0x00000040	
#define E1000_FWSM_DISSW           0x10000000	
#define E1000_FWSM_SKUSEL_MASK     0x60000000	
#define E1000_FWSM_SKUEL_SHIFT     29
#define E1000_FWSM_SKUSEL_EMB      0x0	
#define E1000_FWSM_SKUSEL_CONS     0x1	
#define E1000_FWSM_SKUSEL_PERF_100 0x2	
#define E1000_FWSM_SKUSEL_PERF_GBE 0x3	

#define E1000_FFLT_DBG_INVC     0x00100000	

typedef enum {
	e1000_mng_mode_none = 0,
	e1000_mng_mode_asf,
	e1000_mng_mode_pt,
	e1000_mng_mode_ipmi,
	e1000_mng_mode_host_interface_only
} e1000_mng_mode;

#define E1000_HICR_EN           0x00000001	
#define E1000_HICR_C            0x00000002	
#define E1000_HICR_SV           0x00000004	
#define E1000_HICR_FWR          0x00000080	

#define E1000_HI_MAX_DATA_LENGTH         252	
#define E1000_HI_MAX_BLOCK_BYTE_LENGTH  1792	
#define E1000_HI_MAX_BLOCK_DWORD_LENGTH  448	
#define E1000_HI_COMMAND_TIMEOUT         500	

struct e1000_host_command_header {
	u8 command_id;
	u8 command_length;
	u8 command_options;	
	u8 checksum;
};
struct e1000_host_command_info {
	struct e1000_host_command_header command_header;	
	u8 command_data[E1000_HI_MAX_DATA_LENGTH];	
};

#define E1000_HSMC0R_CLKIN      0x00000001	
#define E1000_HSMC0R_DATAIN     0x00000002	
#define E1000_HSMC0R_DATAOUT    0x00000004	
#define E1000_HSMC0R_CLKOUT     0x00000008	

#define E1000_HSMC1R_CLKIN      E1000_HSMC0R_CLKIN
#define E1000_HSMC1R_DATAIN     E1000_HSMC0R_DATAIN
#define E1000_HSMC1R_DATAOUT    E1000_HSMC0R_DATAOUT
#define E1000_HSMC1R_CLKOUT     E1000_HSMC0R_CLKOUT

#define E1000_FWSTS_FWS_MASK    0x000000FF	

#define E1000_WUPL_LENGTH_MASK 0x0FFF	

#define E1000_MDALIGN          4096


#define E1000_GCR_RXD_NO_SNOOP          0x00000001
#define E1000_GCR_RXDSCW_NO_SNOOP       0x00000002
#define E1000_GCR_RXDSCR_NO_SNOOP       0x00000004
#define E1000_GCR_TXD_NO_SNOOP          0x00000008
#define E1000_GCR_TXDSCW_NO_SNOOP       0x00000010
#define E1000_GCR_TXDSCR_NO_SNOOP       0x00000020

#define PCI_EX_NO_SNOOP_ALL (E1000_GCR_RXD_NO_SNOOP         | \
                             E1000_GCR_RXDSCW_NO_SNOOP      | \
                             E1000_GCR_RXDSCR_NO_SNOOP      | \
                             E1000_GCR_TXD_NO_SNOOP         | \
                             E1000_GCR_TXDSCW_NO_SNOOP      | \
                             E1000_GCR_TXDSCR_NO_SNOOP)

#define PCI_EX_82566_SNOOP_ALL PCI_EX_NO_SNOOP_ALL

#define E1000_GCR_L1_ACT_WITHOUT_L0S_RX 0x08000000
#define E1000_FACTPS_FUNC0_POWER_STATE_MASK         0x00000003
#define E1000_FACTPS_LAN0_VALID                     0x00000004
#define E1000_FACTPS_FUNC0_AUX_EN                   0x00000008
#define E1000_FACTPS_FUNC1_POWER_STATE_MASK         0x000000C0
#define E1000_FACTPS_FUNC1_POWER_STATE_SHIFT        6
#define E1000_FACTPS_LAN1_VALID                     0x00000100
#define E1000_FACTPS_FUNC1_AUX_EN                   0x00000200
#define E1000_FACTPS_FUNC2_POWER_STATE_MASK         0x00003000
#define E1000_FACTPS_FUNC2_POWER_STATE_SHIFT        12
#define E1000_FACTPS_IDE_ENABLE                     0x00004000
#define E1000_FACTPS_FUNC2_AUX_EN                   0x00008000
#define E1000_FACTPS_FUNC3_POWER_STATE_MASK         0x000C0000
#define E1000_FACTPS_FUNC3_POWER_STATE_SHIFT        18
#define E1000_FACTPS_SP_ENABLE                      0x00100000
#define E1000_FACTPS_FUNC3_AUX_EN                   0x00200000
#define E1000_FACTPS_FUNC4_POWER_STATE_MASK         0x03000000
#define E1000_FACTPS_FUNC4_POWER_STATE_SHIFT        24
#define E1000_FACTPS_IPMI_ENABLE                    0x04000000
#define E1000_FACTPS_FUNC4_AUX_EN                   0x08000000
#define E1000_FACTPS_MNGCG                          0x20000000
#define E1000_FACTPS_LAN_FUNC_SEL                   0x40000000
#define E1000_FACTPS_PM_STATE_CHANGED               0x80000000

#define PCI_EX_LINK_STATUS           0x12
#define PCI_EX_LINK_WIDTH_MASK       0x3F0
#define PCI_EX_LINK_WIDTH_SHIFT      4

#define EEPROM_READ_OPCODE_MICROWIRE  0x6	
#define EEPROM_WRITE_OPCODE_MICROWIRE 0x5	
#define EEPROM_ERASE_OPCODE_MICROWIRE 0x7	
#define EEPROM_EWEN_OPCODE_MICROWIRE  0x13	
#define EEPROM_EWDS_OPCODE_MICROWIRE  0x10	

#define EEPROM_MAX_RETRY_SPI        5000	
#define EEPROM_READ_OPCODE_SPI      0x03	
#define EEPROM_WRITE_OPCODE_SPI     0x02	
#define EEPROM_A8_OPCODE_SPI        0x08	
#define EEPROM_WREN_OPCODE_SPI      0x06	
#define EEPROM_WRDI_OPCODE_SPI      0x04	
#define EEPROM_RDSR_OPCODE_SPI      0x05	
#define EEPROM_WRSR_OPCODE_SPI      0x01	
#define EEPROM_ERASE4K_OPCODE_SPI   0x20	
#define EEPROM_ERASE64K_OPCODE_SPI  0xD8	
#define EEPROM_ERASE256_OPCODE_SPI  0xDB	

#define EEPROM_WORD_SIZE_SHIFT  6
#define EEPROM_SIZE_SHIFT       10
#define EEPROM_SIZE_MASK        0x1C00

#define EEPROM_COMPAT                 0x0003
#define EEPROM_ID_LED_SETTINGS        0x0004
#define EEPROM_VERSION                0x0005
#define EEPROM_SERDES_AMPLITUDE       0x0006	
#define EEPROM_PHY_CLASS_WORD         0x0007
#define EEPROM_INIT_CONTROL1_REG      0x000A
#define EEPROM_INIT_CONTROL2_REG      0x000F
#define EEPROM_SWDEF_PINS_CTRL_PORT_1 0x0010
#define EEPROM_INIT_CONTROL3_PORT_B   0x0014
#define EEPROM_INIT_3GIO_3            0x001A
#define EEPROM_SWDEF_PINS_CTRL_PORT_0 0x0020
#define EEPROM_INIT_CONTROL3_PORT_A   0x0024
#define EEPROM_CFG                    0x0012
#define EEPROM_FLASH_VERSION          0x0032
#define EEPROM_CHECKSUM_REG           0x003F

#define E1000_EEPROM_CFG_DONE         0x00040000	
#define E1000_EEPROM_CFG_DONE_PORT_1  0x00080000	

#define ID_LED_RESERVED_0000 0x0000
#define ID_LED_RESERVED_FFFF 0xFFFF
#define ID_LED_DEFAULT       ((ID_LED_OFF1_ON2 << 12) | \
                              (ID_LED_OFF1_OFF2 << 8) | \
                              (ID_LED_DEF1_DEF2 << 4) | \
                              (ID_LED_DEF1_DEF2))
#define ID_LED_DEF1_DEF2     0x1
#define ID_LED_DEF1_ON2      0x2
#define ID_LED_DEF1_OFF2     0x3
#define ID_LED_ON1_DEF2      0x4
#define ID_LED_ON1_ON2       0x5
#define ID_LED_ON1_OFF2      0x6
#define ID_LED_OFF1_DEF2     0x7
#define ID_LED_OFF1_ON2      0x8
#define ID_LED_OFF1_OFF2     0x9

#define IGP_ACTIVITY_LED_MASK   0xFFFFF0FF
#define IGP_ACTIVITY_LED_ENABLE 0x0300
#define IGP_LED3_MODE           0x07000000

#define EEPROM_SERDES_AMPLITUDE_MASK  0x000F

#define EEPROM_PHY_CLASS_A   0x8000

#define EEPROM_WORD0A_ILOS   0x0010
#define EEPROM_WORD0A_SWDPIO 0x01E0
#define EEPROM_WORD0A_LRST   0x0200
#define EEPROM_WORD0A_FD     0x0400
#define EEPROM_WORD0A_66MHZ  0x0800

#define EEPROM_WORD0F_PAUSE_MASK 0x3000
#define EEPROM_WORD0F_PAUSE      0x1000
#define EEPROM_WORD0F_ASM_DIR    0x2000
#define EEPROM_WORD0F_ANE        0x0800
#define EEPROM_WORD0F_SWPDIO_EXT 0x00F0
#define EEPROM_WORD0F_LPLU       0x0001

#define EEPROM_WORD1020_GIGA_DISABLE         0x0010
#define EEPROM_WORD1020_GIGA_DISABLE_NON_D0A 0x0008

#define EEPROM_WORD1A_ASPM_MASK  0x000C

#define EEPROM_SUM 0xBABA

#define EEPROM_NODE_ADDRESS_BYTE_0 0
#define EEPROM_PBA_BYTE_1          8

#define EEPROM_RESERVED_WORD          0xFFFF

#define PBA_SIZE 4

#define E1000_COLLISION_THRESHOLD       15
#define E1000_CT_SHIFT                  4
#define E1000_COLLISION_DISTANCE        63
#define E1000_COLLISION_DISTANCE_82542  64
#define E1000_FDX_COLLISION_DISTANCE    E1000_COLLISION_DISTANCE
#define E1000_HDX_COLLISION_DISTANCE    E1000_COLLISION_DISTANCE
#define E1000_COLD_SHIFT                12

#define REQ_TX_DESCRIPTOR_MULTIPLE  8
#define REQ_RX_DESCRIPTOR_MULTIPLE  8

#define DEFAULT_82542_TIPG_IPGT        10
#define DEFAULT_82543_TIPG_IPGT_FIBER  9
#define DEFAULT_82543_TIPG_IPGT_COPPER 8

#define E1000_TIPG_IPGT_MASK  0x000003FF
#define E1000_TIPG_IPGR1_MASK 0x000FFC00
#define E1000_TIPG_IPGR2_MASK 0x3FF00000

#define DEFAULT_82542_TIPG_IPGR1 2
#define DEFAULT_82543_TIPG_IPGR1 8
#define E1000_TIPG_IPGR1_SHIFT  10

#define DEFAULT_82542_TIPG_IPGR2 10
#define DEFAULT_82543_TIPG_IPGR2 6
#define E1000_TIPG_IPGR2_SHIFT  20

#define E1000_TXDMAC_DPP 0x00000001

#define TX_THRESHOLD_START     8
#define TX_THRESHOLD_INCREMENT 10
#define TX_THRESHOLD_DECREMENT 1
#define TX_THRESHOLD_STOP      190
#define TX_THRESHOLD_DISABLE   0
#define TX_THRESHOLD_TIMER_MS  10000
#define MIN_NUM_XMITS          1000
#define IFS_MAX                80
#define IFS_STEP               10
#define IFS_MIN                40
#define IFS_RATIO              4

#define E1000_EXTCNF_CTRL_PCIE_WRITE_ENABLE 0x00000001
#define E1000_EXTCNF_CTRL_PHY_WRITE_ENABLE  0x00000002
#define E1000_EXTCNF_CTRL_D_UD_ENABLE       0x00000004
#define E1000_EXTCNF_CTRL_D_UD_LATENCY      0x00000008
#define E1000_EXTCNF_CTRL_D_UD_OWNER        0x00000010
#define E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP 0x00000020
#define E1000_EXTCNF_CTRL_MDIO_HW_OWNERSHIP 0x00000040
#define E1000_EXTCNF_CTRL_EXT_CNF_POINTER   0x0FFF0000

#define E1000_EXTCNF_SIZE_EXT_PHY_LENGTH    0x000000FF
#define E1000_EXTCNF_SIZE_EXT_DOCK_LENGTH   0x0000FF00
#define E1000_EXTCNF_SIZE_EXT_PCIE_LENGTH   0x00FF0000
#define E1000_EXTCNF_CTRL_LCD_WRITE_ENABLE  0x00000001
#define E1000_EXTCNF_CTRL_SWFLAG            0x00000020

#define E1000_PBA_8K 0x0008	
#define E1000_PBA_12K 0x000C	
#define E1000_PBA_16K 0x0010	
#define E1000_PBA_20K 0x0014
#define E1000_PBA_22K 0x0016
#define E1000_PBA_24K 0x0018
#define E1000_PBA_30K 0x001E
#define E1000_PBA_32K 0x0020
#define E1000_PBA_34K 0x0022
#define E1000_PBA_38K 0x0026
#define E1000_PBA_40K 0x0028
#define E1000_PBA_48K 0x0030	

#define E1000_PBS_16K E1000_PBA_16K

#define FLOW_CONTROL_ADDRESS_LOW  0x00C28001
#define FLOW_CONTROL_ADDRESS_HIGH 0x00000100
#define FLOW_CONTROL_TYPE         0x8808

#define FC_DEFAULT_HI_THRESH        (0x8000)	
#define FC_DEFAULT_LO_THRESH        (0x4000)	
#define FC_DEFAULT_TX_TIMER         (0x100)	

#define PCIX_COMMAND_REGISTER    0xE6
#define PCIX_STATUS_REGISTER_LO  0xE8
#define PCIX_STATUS_REGISTER_HI  0xEA

#define PCIX_COMMAND_MMRBC_MASK      0x000C
#define PCIX_COMMAND_MMRBC_SHIFT     0x2
#define PCIX_STATUS_HI_MMRBC_MASK    0x0060
#define PCIX_STATUS_HI_MMRBC_SHIFT   0x5
#define PCIX_STATUS_HI_MMRBC_4K      0x3
#define PCIX_STATUS_HI_MMRBC_2K      0x2

#define PAUSE_SHIFT 5

#define SWDPIO_SHIFT 17

#define SWDPIO__EXT_SHIFT 4

#define ILOS_SHIFT  3

#define RECEIVE_BUFFER_ALIGN_SIZE  (256)

#define LINK_UP_TIMEOUT             500

#define AUTO_READ_DONE_TIMEOUT      10
#define PHY_CFG_TIMEOUT             100

#define E1000_TX_BUFFER_SIZE ((u32)1514)

#define CARRIER_EXTENSION   0x0F


#define TBI_ACCEPT(adapter, status, errors, length, last_byte) \
    ((adapter)->tbi_compatibility_on && \
     (((errors) & E1000_RXD_ERR_FRAME_ERR_MASK) == E1000_RXD_ERR_CE) && \
     ((last_byte) == CARRIER_EXTENSION) && \
     (((status) & E1000_RXD_STAT_VP) ? \
          (((length) > ((adapter)->min_frame_size - VLAN_TAG_SIZE)) && \
           ((length) <= ((adapter)->max_frame_size + 1))) : \
          (((length) > (adapter)->min_frame_size) && \
           ((length) <= ((adapter)->max_frame_size + VLAN_TAG_SIZE + 1)))))


#define E1000_CTRL_PHY_RESET_DIR  E1000_CTRL_SWDPIO0
#define E1000_CTRL_PHY_RESET      E1000_CTRL_SWDPIN0
#define E1000_CTRL_MDIO_DIR       E1000_CTRL_SWDPIO2
#define E1000_CTRL_MDIO           E1000_CTRL_SWDPIN2
#define E1000_CTRL_MDC_DIR        E1000_CTRL_SWDPIO3
#define E1000_CTRL_MDC            E1000_CTRL_SWDPIN3
#define E1000_CTRL_PHY_RESET_DIR4 E1000_CTRL_EXT_SDP4_DIR
#define E1000_CTRL_PHY_RESET4     E1000_CTRL_EXT_SDP4_DATA

#define PHY_CTRL         0x00	
#define PHY_STATUS       0x01	
#define PHY_ID1          0x02	
#define PHY_ID2          0x03	
#define PHY_AUTONEG_ADV  0x04	
#define PHY_LP_ABILITY   0x05	
#define PHY_AUTONEG_EXP  0x06	
#define PHY_NEXT_PAGE_TX 0x07	
#define PHY_LP_NEXT_PAGE 0x08	
#define PHY_1000T_CTRL   0x09	
#define PHY_1000T_STATUS 0x0A	
#define PHY_EXT_STATUS   0x0F	

#define MAX_PHY_REG_ADDRESS        0x1F	
#define MAX_PHY_MULTI_PAGE_REG     0xF	

#define M88E1000_PHY_SPEC_CTRL     0x10	
#define M88E1000_PHY_SPEC_STATUS   0x11	
#define M88E1000_INT_ENABLE        0x12	
#define M88E1000_INT_STATUS        0x13	
#define M88E1000_EXT_PHY_SPEC_CTRL 0x14	
#define M88E1000_RX_ERR_CNTR       0x15	

#define M88E1000_PHY_EXT_CTRL      0x1A	
#define M88E1000_PHY_PAGE_SELECT   0x1D	
#define M88E1000_PHY_GEN_CONTROL   0x1E	
#define M88E1000_PHY_VCO_REG_BIT8  0x100	
#define M88E1000_PHY_VCO_REG_BIT11 0x800	

#define IGP01E1000_IEEE_REGS_PAGE  0x0000
#define IGP01E1000_IEEE_RESTART_AUTONEG 0x3300
#define IGP01E1000_IEEE_FORCE_GIGA      0x0140

#define IGP01E1000_PHY_PORT_CONFIG 0x10	
#define IGP01E1000_PHY_PORT_STATUS 0x11	
#define IGP01E1000_PHY_PORT_CTRL   0x12	
#define IGP01E1000_PHY_LINK_HEALTH 0x13	
#define IGP01E1000_GMII_FIFO       0x14	
#define IGP01E1000_PHY_CHANNEL_QUALITY 0x15	
#define IGP02E1000_PHY_POWER_MGMT      0x19
#define IGP01E1000_PHY_PAGE_SELECT     0x1F	

#define IGP01E1000_PHY_AGC_A        0x1172
#define IGP01E1000_PHY_AGC_B        0x1272
#define IGP01E1000_PHY_AGC_C        0x1472
#define IGP01E1000_PHY_AGC_D        0x1872

#define IGP02E1000_PHY_AGC_A        0x11B1
#define IGP02E1000_PHY_AGC_B        0x12B1
#define IGP02E1000_PHY_AGC_C        0x14B1
#define IGP02E1000_PHY_AGC_D        0x18B1

#define IGP01E1000_PHY_DSP_RESET   0x1F33
#define IGP01E1000_PHY_DSP_SET     0x1F71
#define IGP01E1000_PHY_DSP_FFE     0x1F35

#define IGP01E1000_PHY_CHANNEL_NUM    4
#define IGP02E1000_PHY_CHANNEL_NUM    4

#define IGP01E1000_PHY_AGC_PARAM_A    0x1171
#define IGP01E1000_PHY_AGC_PARAM_B    0x1271
#define IGP01E1000_PHY_AGC_PARAM_C    0x1471
#define IGP01E1000_PHY_AGC_PARAM_D    0x1871

#define IGP01E1000_PHY_EDAC_MU_INDEX        0xC000
#define IGP01E1000_PHY_EDAC_SIGN_EXT_9_BITS 0x8000

#define IGP01E1000_PHY_ANALOG_TX_STATE      0x2890
#define IGP01E1000_PHY_ANALOG_CLASS_A       0x2000
#define IGP01E1000_PHY_FORCE_ANALOG_ENABLE  0x0004
#define IGP01E1000_PHY_DSP_FFE_CM_CP        0x0069

#define IGP01E1000_PHY_DSP_FFE_DEFAULT      0x002A
#define IGP01E1000_PHY_PCS_INIT_REG  0x00B4
#define IGP01E1000_PHY_PCS_CTRL_REG  0x00B5

#define IGP01E1000_ANALOG_REGS_PAGE  0x20C0

#define MII_CR_SPEED_SELECT_MSB 0x0040	
#define MII_CR_COLL_TEST_ENABLE 0x0080	
#define MII_CR_FULL_DUPLEX      0x0100	
#define MII_CR_RESTART_AUTO_NEG 0x0200	
#define MII_CR_ISOLATE          0x0400	
#define MII_CR_POWER_DOWN       0x0800	
#define MII_CR_AUTO_NEG_EN      0x1000	
#define MII_CR_SPEED_SELECT_LSB 0x2000	
#define MII_CR_LOOPBACK         0x4000	
#define MII_CR_RESET            0x8000	

#define MII_SR_EXTENDED_CAPS     0x0001	
#define MII_SR_JABBER_DETECT     0x0002	
#define MII_SR_LINK_STATUS       0x0004	
#define MII_SR_AUTONEG_CAPS      0x0008	
#define MII_SR_REMOTE_FAULT      0x0010	
#define MII_SR_AUTONEG_COMPLETE  0x0020	
#define MII_SR_PREAMBLE_SUPPRESS 0x0040	
#define MII_SR_EXTENDED_STATUS   0x0100	
#define MII_SR_100T2_HD_CAPS     0x0200	
#define MII_SR_100T2_FD_CAPS     0x0400	
#define MII_SR_10T_HD_CAPS       0x0800	
#define MII_SR_10T_FD_CAPS       0x1000	
#define MII_SR_100X_HD_CAPS      0x2000	
#define MII_SR_100X_FD_CAPS      0x4000	
#define MII_SR_100T4_CAPS        0x8000	

#define NWAY_AR_SELECTOR_FIELD 0x0001	
#define NWAY_AR_10T_HD_CAPS    0x0020	
#define NWAY_AR_10T_FD_CAPS    0x0040	
#define NWAY_AR_100TX_HD_CAPS  0x0080	
#define NWAY_AR_100TX_FD_CAPS  0x0100	
#define NWAY_AR_100T4_CAPS     0x0200	
#define NWAY_AR_PAUSE          0x0400	
#define NWAY_AR_ASM_DIR        0x0800	
#define NWAY_AR_REMOTE_FAULT   0x2000	
#define NWAY_AR_NEXT_PAGE      0x8000	

#define NWAY_LPAR_SELECTOR_FIELD 0x0000	
#define NWAY_LPAR_10T_HD_CAPS    0x0020	
#define NWAY_LPAR_10T_FD_CAPS    0x0040	
#define NWAY_LPAR_100TX_HD_CAPS  0x0080	
#define NWAY_LPAR_100TX_FD_CAPS  0x0100	
#define NWAY_LPAR_100T4_CAPS     0x0200	
#define NWAY_LPAR_PAUSE          0x0400	
#define NWAY_LPAR_ASM_DIR        0x0800	
#define NWAY_LPAR_REMOTE_FAULT   0x2000	
#define NWAY_LPAR_ACKNOWLEDGE    0x4000	
#define NWAY_LPAR_NEXT_PAGE      0x8000	

#define NWAY_ER_LP_NWAY_CAPS      0x0001	
#define NWAY_ER_PAGE_RXD          0x0002	
#define NWAY_ER_NEXT_PAGE_CAPS    0x0004	
#define NWAY_ER_LP_NEXT_PAGE_CAPS 0x0008	
#define NWAY_ER_PAR_DETECT_FAULT  0x0010	

#define NPTX_MSG_CODE_FIELD 0x0001	
#define NPTX_TOGGLE         0x0800	
#define NPTX_ACKNOWLDGE2    0x1000	
#define NPTX_MSG_PAGE       0x2000	
#define NPTX_NEXT_PAGE      0x8000	

#define LP_RNPR_MSG_CODE_FIELD 0x0001	
#define LP_RNPR_TOGGLE         0x0800	
#define LP_RNPR_ACKNOWLDGE2    0x1000	
#define LP_RNPR_MSG_PAGE       0x2000	
#define LP_RNPR_ACKNOWLDGE     0x4000	
#define LP_RNPR_NEXT_PAGE      0x8000	

#define CR_1000T_ASYM_PAUSE      0x0080	
#define CR_1000T_HD_CAPS         0x0100	
#define CR_1000T_FD_CAPS         0x0200	
#define CR_1000T_REPEATER_DTE    0x0400	
					
#define CR_1000T_MS_VALUE        0x0800	
					
#define CR_1000T_MS_ENABLE       0x1000	
					
#define CR_1000T_TEST_MODE_NORMAL 0x0000	
#define CR_1000T_TEST_MODE_1     0x2000	
#define CR_1000T_TEST_MODE_2     0x4000	
#define CR_1000T_TEST_MODE_3     0x6000	
#define CR_1000T_TEST_MODE_4     0x8000	

#define SR_1000T_IDLE_ERROR_CNT   0x00FF	
#define SR_1000T_ASYM_PAUSE_DIR   0x0100	
#define SR_1000T_LP_HD_CAPS       0x0400	
#define SR_1000T_LP_FD_CAPS       0x0800	
#define SR_1000T_REMOTE_RX_STATUS 0x1000	
#define SR_1000T_LOCAL_RX_STATUS  0x2000	
#define SR_1000T_MS_CONFIG_RES    0x4000	
#define SR_1000T_MS_CONFIG_FAULT  0x8000	
#define SR_1000T_REMOTE_RX_STATUS_SHIFT          12
#define SR_1000T_LOCAL_RX_STATUS_SHIFT           13
#define SR_1000T_PHY_EXCESSIVE_IDLE_ERR_COUNT    5
#define FFE_IDLE_ERR_COUNT_TIMEOUT_20            20
#define FFE_IDLE_ERR_COUNT_TIMEOUT_100           100

#define IEEE_ESR_1000T_HD_CAPS 0x1000	
#define IEEE_ESR_1000T_FD_CAPS 0x2000	
#define IEEE_ESR_1000X_HD_CAPS 0x4000	
#define IEEE_ESR_1000X_FD_CAPS 0x8000	

#define PHY_TX_POLARITY_MASK   0x0100	
#define PHY_TX_NORMAL_POLARITY 0	

#define AUTO_POLARITY_DISABLE  0x0010	
				      

#define M88E1000_PSCR_JABBER_DISABLE    0x0001	
#define M88E1000_PSCR_POLARITY_REVERSAL 0x0002	
#define M88E1000_PSCR_SQE_TEST          0x0004	
#define M88E1000_PSCR_CLK125_DISABLE    0x0010	
#define M88E1000_PSCR_MDI_MANUAL_MODE  0x0000	
					       
#define M88E1000_PSCR_MDIX_MANUAL_MODE 0x0020	
#define M88E1000_PSCR_AUTO_X_1000T     0x0040	
#define M88E1000_PSCR_AUTO_X_MODE      0x0060	
#define M88E1000_PSCR_10BT_EXT_DIST_ENABLE 0x0080
#define M88E1000_PSCR_MII_5BIT_ENABLE      0x0100
#define M88E1000_PSCR_SCRAMBLER_DISABLE    0x0200	
#define M88E1000_PSCR_FORCE_LINK_GOOD      0x0400	
#define M88E1000_PSCR_ASSERT_CRS_ON_TX     0x0800	

#define M88E1000_PSCR_POLARITY_REVERSAL_SHIFT    1
#define M88E1000_PSCR_AUTO_X_MODE_SHIFT          5
#define M88E1000_PSCR_10BT_EXT_DIST_ENABLE_SHIFT 7

#define M88E1000_PSSR_JABBER             0x0001	
#define M88E1000_PSSR_REV_POLARITY       0x0002	
#define M88E1000_PSSR_DOWNSHIFT          0x0020	
#define M88E1000_PSSR_MDIX               0x0040	
#define M88E1000_PSSR_CABLE_LENGTH       0x0380	
#define M88E1000_PSSR_LINK               0x0400	
#define M88E1000_PSSR_SPD_DPLX_RESOLVED  0x0800	
#define M88E1000_PSSR_PAGE_RCVD          0x1000	
#define M88E1000_PSSR_DPLX               0x2000	
#define M88E1000_PSSR_SPEED              0xC000	
#define M88E1000_PSSR_10MBS              0x0000	
#define M88E1000_PSSR_100MBS             0x4000	
#define M88E1000_PSSR_1000MBS            0x8000	

#define M88E1000_PSSR_REV_POLARITY_SHIFT 1
#define M88E1000_PSSR_DOWNSHIFT_SHIFT    5
#define M88E1000_PSSR_MDIX_SHIFT         6
#define M88E1000_PSSR_CABLE_LENGTH_SHIFT 7

#define M88E1000_EPSCR_FIBER_LOOPBACK 0x4000	
#define M88E1000_EPSCR_DOWN_NO_IDLE   0x8000	
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK 0x0C00
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_1X   0x0000
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_2X   0x0400
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_3X   0x0800
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_4X   0x0C00
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK  0x0300
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_DIS   0x0000
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X    0x0100
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_2X    0x0200
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_3X    0x0300
#define M88E1000_EPSCR_TX_CLK_2_5     0x0060	
#define M88E1000_EPSCR_TX_CLK_25      0x0070	
#define M88E1000_EPSCR_TX_CLK_0       0x0000	

#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_MASK  0x0E00
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_1X    0x0000
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_2X    0x0200
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_3X    0x0400
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_4X    0x0600
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_5X    0x0800
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_6X    0x0A00
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_7X    0x0C00
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_8X    0x0E00

#define IGP01E1000_PSCFR_AUTO_MDIX_PAR_DETECT  0x0010
#define IGP01E1000_PSCFR_PRE_EN                0x0020
#define IGP01E1000_PSCFR_SMART_SPEED           0x0080
#define IGP01E1000_PSCFR_DISABLE_TPLOOPBACK    0x0100
#define IGP01E1000_PSCFR_DISABLE_JABBER        0x0400
#define IGP01E1000_PSCFR_DISABLE_TRANSMIT      0x2000

#define IGP01E1000_PSSR_AUTONEG_FAILED         0x0001	
#define IGP01E1000_PSSR_POLARITY_REVERSED      0x0002
#define IGP01E1000_PSSR_CABLE_LENGTH           0x007C
#define IGP01E1000_PSSR_FULL_DUPLEX            0x0200
#define IGP01E1000_PSSR_LINK_UP                0x0400
#define IGP01E1000_PSSR_MDIX                   0x0800
#define IGP01E1000_PSSR_SPEED_MASK             0xC000	
#define IGP01E1000_PSSR_SPEED_10MBPS           0x4000
#define IGP01E1000_PSSR_SPEED_100MBPS          0x8000
#define IGP01E1000_PSSR_SPEED_1000MBPS         0xC000
#define IGP01E1000_PSSR_CABLE_LENGTH_SHIFT     0x0002	
#define IGP01E1000_PSSR_MDIX_SHIFT             0x000B	

#define IGP01E1000_PSCR_TP_LOOPBACK            0x0010
#define IGP01E1000_PSCR_CORRECT_NC_SCMBLR      0x0200
#define IGP01E1000_PSCR_TEN_CRS_SELECT         0x0400
#define IGP01E1000_PSCR_FLIP_CHIP              0x0800
#define IGP01E1000_PSCR_AUTO_MDIX              0x1000
#define IGP01E1000_PSCR_FORCE_MDI_MDIX         0x2000	

#define IGP01E1000_PLHR_SS_DOWNGRADE           0x8000
#define IGP01E1000_PLHR_GIG_SCRAMBLER_ERROR    0x4000
#define IGP01E1000_PLHR_MASTER_FAULT           0x2000
#define IGP01E1000_PLHR_MASTER_RESOLUTION      0x1000
#define IGP01E1000_PLHR_GIG_REM_RCVR_NOK       0x0800	
#define IGP01E1000_PLHR_IDLE_ERROR_CNT_OFLOW   0x0400	
#define IGP01E1000_PLHR_DATA_ERR_1             0x0200	
#define IGP01E1000_PLHR_DATA_ERR_0             0x0100
#define IGP01E1000_PLHR_AUTONEG_FAULT          0x0040
#define IGP01E1000_PLHR_AUTONEG_ACTIVE         0x0010
#define IGP01E1000_PLHR_VALID_CHANNEL_D        0x0008
#define IGP01E1000_PLHR_VALID_CHANNEL_C        0x0004
#define IGP01E1000_PLHR_VALID_CHANNEL_B        0x0002
#define IGP01E1000_PLHR_VALID_CHANNEL_A        0x0001

#define IGP01E1000_MSE_CHANNEL_D        0x000F
#define IGP01E1000_MSE_CHANNEL_C        0x00F0
#define IGP01E1000_MSE_CHANNEL_B        0x0F00
#define IGP01E1000_MSE_CHANNEL_A        0xF000

#define IGP02E1000_PM_SPD                         0x0001	
#define IGP02E1000_PM_D3_LPLU                     0x0004	
#define IGP02E1000_PM_D0_LPLU                     0x0002	

#define DSP_RESET_ENABLE     0x0
#define DSP_RESET_DISABLE    0x2
#define E1000_MAX_DSP_RESETS 10


#define IGP01E1000_AGC_LENGTH_SHIFT 7	
#define IGP02E1000_AGC_LENGTH_SHIFT 9	

#define IGP02E1000_AGC_LENGTH_MASK  0x7F

#define IGP01E1000_AGC_LENGTH_TABLE_SIZE 128
#define IGP02E1000_AGC_LENGTH_TABLE_SIZE 113

#define IGP01E1000_AGC_RANGE    10
#define IGP02E1000_AGC_RANGE    15

#define IGP01E1000_PHY_POLARITY_MASK    0x0078

#define IGP01E1000_GMII_FLEX_SPD               0x10	
#define IGP01E1000_GMII_SPD                    0x20	

#define IGP01E1000_ANALOG_SPARE_FUSE_STATUS       0x20D1
#define IGP01E1000_ANALOG_FUSE_STATUS             0x20D0
#define IGP01E1000_ANALOG_FUSE_CONTROL            0x20DC
#define IGP01E1000_ANALOG_FUSE_BYPASS             0x20DE

#define IGP01E1000_ANALOG_FUSE_POLY_MASK            0xF000
#define IGP01E1000_ANALOG_FUSE_FINE_MASK            0x0F80
#define IGP01E1000_ANALOG_FUSE_COARSE_MASK          0x0070
#define IGP01E1000_ANALOG_SPARE_FUSE_ENABLED        0x0100
#define IGP01E1000_ANALOG_FUSE_ENABLE_SW_CONTROL    0x0002

#define IGP01E1000_ANALOG_FUSE_COARSE_THRESH        0x0040
#define IGP01E1000_ANALOG_FUSE_COARSE_10            0x0010
#define IGP01E1000_ANALOG_FUSE_FINE_1               0x0080
#define IGP01E1000_ANALOG_FUSE_FINE_10              0x0500

#define M88_VENDOR         0x0141
#define M88E1000_E_PHY_ID  0x01410C50
#define M88E1000_I_PHY_ID  0x01410C30
#define M88E1011_I_PHY_ID  0x01410C20
#define IGP01E1000_I_PHY_ID  0x02A80380
#define M88E1000_12_PHY_ID M88E1000_E_PHY_ID
#define M88E1000_14_PHY_ID M88E1000_E_PHY_ID
#define M88E1011_I_REV_4   0x04
#define M88E1111_I_PHY_ID  0x01410CC0
#define M88E1118_E_PHY_ID  0x01410E40
#define L1LXT971A_PHY_ID   0x001378E0

#define RTL8211B_PHY_ID    0x001CC910
#define RTL8201N_PHY_ID    0x8200
#define RTL_PHY_CTRL_FD    0x0100 
#define RTL_PHY_CTRL_SPD_100    0x200000 

#define PHY_PAGE_SHIFT        5
#define PHY_REG(page, reg)    \
        (((page) << PHY_PAGE_SHIFT) | ((reg) & MAX_PHY_REG_ADDRESS))

#define IGP3_PHY_PORT_CTRL           \
        PHY_REG(769, 17)	
#define IGP3_PHY_RATE_ADAPT_CTRL \
        PHY_REG(769, 25)	

#define IGP3_KMRN_FIFO_CTRL_STATS \
        PHY_REG(770, 16)	
#define IGP3_KMRN_POWER_MNG_CTRL \
        PHY_REG(770, 17)	
#define IGP3_KMRN_INBAND_CTRL \
        PHY_REG(770, 18)	
#define IGP3_KMRN_DIAG \
        PHY_REG(770, 19)	
#define IGP3_KMRN_DIAG_PCS_LOCK_LOSS 0x0002	
#define IGP3_KMRN_ACK_TIMEOUT \
        PHY_REG(770, 20)	

#define IGP3_VR_CTRL \
        PHY_REG(776, 18)	
#define IGP3_VR_CTRL_MODE_SHUT       0x0200	
#define IGP3_VR_CTRL_MODE_MASK       0x0300	

#define IGP3_CAPABILITY \
        PHY_REG(776, 19)	

#define IGP3_CAP_INITIATE_TEAM       0x0001	
#define IGP3_CAP_WFM                 0x0002	
#define IGP3_CAP_ASF                 0x0004	
#define IGP3_CAP_LPLU                0x0008	
#define IGP3_CAP_DC_AUTO_SPEED       0x0010	
#define IGP3_CAP_SPD                 0x0020	
#define IGP3_CAP_MULT_QUEUE          0x0040	
#define IGP3_CAP_RSS                 0x0080	
#define IGP3_CAP_8021PQ              0x0100	
#define IGP3_CAP_AMT_CB              0x0200	

#define IGP3_PPC_JORDAN_EN           0x0001
#define IGP3_PPC_JORDAN_GIGA_SPEED   0x0002

#define IGP3_KMRN_PMC_EE_IDLE_LINK_DIS         0x0001
#define IGP3_KMRN_PMC_K0S_ENTRY_LATENCY_MASK   0x001E
#define IGP3_KMRN_PMC_K0S_MODE1_EN_GIGA        0x0020
#define IGP3_KMRN_PMC_K0S_MODE1_EN_100         0x0040

#define IGP3E1000_PHY_MISC_CTRL                0x1B	
#define IGP3_PHY_MISC_DUPLEX_MANUAL_SET        0x1000	

#define IGP3_KMRN_EXT_CTRL  PHY_REG(770, 18)
#define IGP3_KMRN_EC_DIS_INBAND    0x0080

#define IGP03E1000_E_PHY_ID  0x02A80390
#define IFE_E_PHY_ID         0x02A80330	
#define IFE_PLUS_E_PHY_ID    0x02A80320
#define IFE_C_E_PHY_ID       0x02A80310

#define IFE_PHY_EXTENDED_STATUS_CONTROL   0x10	
#define IFE_PHY_SPECIAL_CONTROL           0x11	
#define IFE_PHY_RCV_FALSE_CARRIER         0x13	
#define IFE_PHY_RCV_DISCONNECT            0x14	
#define IFE_PHY_RCV_ERROT_FRAME           0x15	
#define IFE_PHY_RCV_SYMBOL_ERR            0x16	
#define IFE_PHY_PREM_EOF_ERR              0x17	
#define IFE_PHY_RCV_EOF_ERR               0x18	
#define IFE_PHY_TX_JABBER_DETECT          0x19	
#define IFE_PHY_EQUALIZER                 0x1A	
#define IFE_PHY_SPECIAL_CONTROL_LED       0x1B	
#define IFE_PHY_MDIX_CONTROL              0x1C	
#define IFE_PHY_HWI_CONTROL               0x1D	

#define IFE_PESC_REDUCED_POWER_DOWN_DISABLE  0x2000	
#define IFE_PESC_100BTX_POWER_DOWN           0x0400	
#define IFE_PESC_10BTX_POWER_DOWN            0x0200	
#define IFE_PESC_POLARITY_REVERSED           0x0100	
#define IFE_PESC_PHY_ADDR_MASK               0x007C	
#define IFE_PESC_SPEED                       0x0002	
#define IFE_PESC_DUPLEX                      0x0001	
#define IFE_PESC_POLARITY_REVERSED_SHIFT     8

#define IFE_PSC_DISABLE_DYNAMIC_POWER_DOWN   0x0100	
#define IFE_PSC_FORCE_POLARITY               0x0020	
#define IFE_PSC_AUTO_POLARITY_DISABLE        0x0010	
#define IFE_PSC_JABBER_FUNC_DISABLE          0x0001	
#define IFE_PSC_FORCE_POLARITY_SHIFT         5
#define IFE_PSC_AUTO_POLARITY_DISABLE_SHIFT  4

#define IFE_PMC_AUTO_MDIX                    0x0080	
#define IFE_PMC_FORCE_MDIX                   0x0040	
#define IFE_PMC_MDIX_STATUS                  0x0020	
#define IFE_PMC_AUTO_MDIX_COMPLETE           0x0010	
#define IFE_PMC_MDIX_MODE_SHIFT              6
#define IFE_PHC_MDIX_RESET_ALL_MASK          0x0000	

#define IFE_PHC_HWI_ENABLE                   0x8000	
#define IFE_PHC_ABILITY_CHECK                0x4000	
#define IFE_PHC_TEST_EXEC                    0x2000	
#define IFE_PHC_HIGHZ                        0x0200	
#define IFE_PHC_LOWZ                         0x0400	
#define IFE_PHC_LOW_HIGH_Z_MASK              0x0600	
#define IFE_PHC_DISTANCE_MASK                0x01FF	
#define IFE_PHC_RESET_ALL_MASK               0x0000	
#define IFE_PSCL_PROBE_MODE                  0x0020	
#define IFE_PSCL_PROBE_LEDS_OFF              0x0006	
#define IFE_PSCL_PROBE_LEDS_ON               0x0007	

#define ICH_FLASH_COMMAND_TIMEOUT            5000	
#define ICH_FLASH_ERASE_TIMEOUT              3000000	
#define ICH_FLASH_CYCLE_REPEAT_COUNT         10	
#define ICH_FLASH_SEG_SIZE_256               256
#define ICH_FLASH_SEG_SIZE_4K                4096
#define ICH_FLASH_SEG_SIZE_64K               65536

#define ICH_CYCLE_READ                       0x0
#define ICH_CYCLE_RESERVED                   0x1
#define ICH_CYCLE_WRITE                      0x2
#define ICH_CYCLE_ERASE                      0x3

#define ICH_FLASH_GFPREG   0x0000
#define ICH_FLASH_HSFSTS   0x0004
#define ICH_FLASH_HSFCTL   0x0006
#define ICH_FLASH_FADDR    0x0008
#define ICH_FLASH_FDATA0   0x0010
#define ICH_FLASH_FRACC    0x0050
#define ICH_FLASH_FREG0    0x0054
#define ICH_FLASH_FREG1    0x0058
#define ICH_FLASH_FREG2    0x005C
#define ICH_FLASH_FREG3    0x0060
#define ICH_FLASH_FPR0     0x0074
#define ICH_FLASH_FPR1     0x0078
#define ICH_FLASH_SSFSTS   0x0090
#define ICH_FLASH_SSFCTL   0x0092
#define ICH_FLASH_PREOP    0x0094
#define ICH_FLASH_OPTYPE   0x0096
#define ICH_FLASH_OPMENU   0x0098

#define ICH_FLASH_REG_MAPSIZE      0x00A0
#define ICH_FLASH_SECTOR_SIZE      4096
#define ICH_GFPREG_BASE_MASK       0x1FFF
#define ICH_FLASH_LINEAR_ADDR_MASK 0x00FFFFFF

#define PHY_PREAMBLE        0xFFFFFFFF
#define PHY_SOF             0x01
#define PHY_OP_READ         0x02
#define PHY_OP_WRITE        0x01
#define PHY_TURNAROUND      0x02
#define PHY_PREAMBLE_SIZE   32
#define MII_CR_SPEED_1000   0x0040
#define MII_CR_SPEED_100    0x2000
#define MII_CR_SPEED_10     0x0000
#define E1000_PHY_ADDRESS   0x01
#define PHY_AUTO_NEG_TIME   45	
#define PHY_FORCE_TIME      20	
#define PHY_REVISION_MASK   0xFFFFFFF0
#define DEVICE_SPEED_MASK   0x00000300	
#define REG4_SPEED_MASK     0x01E0
#define REG9_SPEED_MASK     0x0300
#define ADVERTISE_10_HALF   0x0001
#define ADVERTISE_10_FULL   0x0002
#define ADVERTISE_100_HALF  0x0004
#define ADVERTISE_100_FULL  0x0008
#define ADVERTISE_1000_HALF 0x0010
#define ADVERTISE_1000_FULL 0x0020
#define AUTONEG_ADVERTISE_SPEED_DEFAULT 0x002F	
#define AUTONEG_ADVERTISE_10_100_ALL    0x000F	
#define AUTONEG_ADVERTISE_10_ALL        0x0003	

#endif 
