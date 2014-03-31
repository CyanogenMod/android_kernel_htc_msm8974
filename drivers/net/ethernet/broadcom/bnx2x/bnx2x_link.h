/* Copyright 2008-2012 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Written by Yaniv Rosner
 *
 */

#ifndef BNX2X_LINK_H
#define BNX2X_LINK_H



#define DEFAULT_PHY_DEV_ADDR	3
#define E2_DEFAULT_PHY_DEV_ADDR	5



#define BNX2X_FLOW_CTRL_AUTO		PORT_FEATURE_FLOW_CONTROL_AUTO
#define BNX2X_FLOW_CTRL_TX		PORT_FEATURE_FLOW_CONTROL_TX
#define BNX2X_FLOW_CTRL_RX		PORT_FEATURE_FLOW_CONTROL_RX
#define BNX2X_FLOW_CTRL_BOTH		PORT_FEATURE_FLOW_CONTROL_BOTH
#define BNX2X_FLOW_CTRL_NONE		PORT_FEATURE_FLOW_CONTROL_NONE

#define NET_SERDES_IF_XFI		1
#define NET_SERDES_IF_SFI		2
#define NET_SERDES_IF_KR		3
#define NET_SERDES_IF_DXGXS	4

#define SPEED_AUTO_NEG		0
#define SPEED_20000		20000

#define SFP_EEPROM_VENDOR_NAME_ADDR		0x14
#define SFP_EEPROM_VENDOR_NAME_SIZE		16
#define SFP_EEPROM_VENDOR_OUI_ADDR		0x25
#define SFP_EEPROM_VENDOR_OUI_SIZE		3
#define SFP_EEPROM_PART_NO_ADDR			0x28
#define SFP_EEPROM_PART_NO_SIZE			16
#define SFP_EEPROM_REVISION_ADDR		0x38
#define SFP_EEPROM_REVISION_SIZE		4
#define SFP_EEPROM_SERIAL_ADDR			0x44
#define SFP_EEPROM_SERIAL_SIZE			16
#define SFP_EEPROM_DATE_ADDR			0x54 
#define SFP_EEPROM_DATE_SIZE			6
#define PWR_FLT_ERR_MSG_LEN			250

#define XGXS_EXT_PHY_TYPE(ext_phy_config) \
		((ext_phy_config) & PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK)
#define XGXS_EXT_PHY_ADDR(ext_phy_config) \
		(((ext_phy_config) & PORT_HW_CFG_XGXS_EXT_PHY_ADDR_MASK) >> \
		 PORT_HW_CFG_XGXS_EXT_PHY_ADDR_SHIFT)
#define SERDES_EXT_PHY_TYPE(ext_phy_config) \
		((ext_phy_config) & PORT_HW_CFG_SERDES_EXT_PHY_TYPE_MASK)

#define SINGLE_MEDIA_DIRECT(params)	(params->num_phys == 1)
#define SINGLE_MEDIA(params)		(params->num_phys == 2)
#define DUAL_MEDIA(params)		(params->num_phys == 3)

#define FW_PARAM_PHY_ADDR_MASK		0x000000FF
#define FW_PARAM_PHY_TYPE_MASK		0x0000FF00
#define FW_PARAM_MDIO_CTRL_MASK		0xFFFF0000
#define FW_PARAM_MDIO_CTRL_OFFSET		16
#define FW_PARAM_PHY_ADDR(fw_param) (fw_param & \
					   FW_PARAM_PHY_ADDR_MASK)
#define FW_PARAM_PHY_TYPE(fw_param) (fw_param & \
					   FW_PARAM_PHY_TYPE_MASK)
#define FW_PARAM_MDIO_CTRL(fw_param) ((fw_param & \
					    FW_PARAM_MDIO_CTRL_MASK) >> \
					    FW_PARAM_MDIO_CTRL_OFFSET)
#define FW_PARAM_SET(phy_addr, phy_type, mdio_access) \
	(phy_addr | phy_type | mdio_access << FW_PARAM_MDIO_CTRL_OFFSET)


#define PFC_BRB_FULL_LB_XOFF_THRESHOLD				170
#define PFC_BRB_FULL_LB_XON_THRESHOLD				250

#define MAXVAL(a, b) (((a) > (b)) ? (a) : (b))

#define BMAC_CONTROL_RX_ENABLE		2
#define INT_PHY		0
#define EXT_PHY1	1
#define EXT_PHY2	2
#define MAX_PHYS	3

#define LINK_CONFIG_SIZE (MAX_PHYS - 1)
#define LINK_CONFIG_IDX(_phy_idx) ((_phy_idx == INT_PHY) ? \
					 0 : (_phy_idx - 1))
struct link_vars;
struct link_params;
struct bnx2x_phy;

typedef u8 (*config_init_t)(struct bnx2x_phy *phy, struct link_params *params,
			    struct link_vars *vars);
typedef u8 (*read_status_t)(struct bnx2x_phy *phy, struct link_params *params,
			    struct link_vars *vars);
typedef void (*link_reset_t)(struct bnx2x_phy *phy,
			     struct link_params *params);
typedef void (*config_loopback_t)(struct bnx2x_phy *phy,
				  struct link_params *params);
typedef u8 (*format_fw_ver_t)(u32 raw, u8 *str, u16 *len);
typedef void (*hw_reset_t)(struct bnx2x_phy *phy, struct link_params *params);
typedef void (*set_link_led_t)(struct bnx2x_phy *phy,
			       struct link_params *params, u8 mode);
typedef void (*phy_specific_func_t)(struct bnx2x_phy *phy,
				    struct link_params *params, u32 action);

struct bnx2x_phy {
	u32 type;

	
	u8 addr;
	u8 def_md_devad;
	u16 flags;
	
#define FLAGS_HW_LOCK_REQUIRED		(1<<0)
	
#define FLAGS_NOC			(1<<1)
	
#define FLAGS_FAN_FAILURE_DET_REQ	(1<<2)
	
#define FLAGS_INIT_XGXS_FIRST		(1<<3)
#define FLAGS_WC_DUAL_MODE		(1<<4)
#define FLAGS_4_PORT_MODE		(1<<5)
#define FLAGS_REARM_LATCH_SIGNAL	(1<<6)
#define FLAGS_SFP_NOT_APPROVED		(1<<7)
#define FLAGS_MDC_MDIO_WA		(1<<8)
#define FLAGS_DUMMY_READ		(1<<9)
#define FLAGS_MDC_MDIO_WA_B0		(1<<10)
#define FLAGS_TX_ERROR_CHECK		(1<<12)

	
	u16 rx_preemphasis[4];

	
	u16 tx_preemphasis[4];

	
	u32 mdio_ctrl;

	u32 supported;

	u32 media_type;
#define	ETH_PHY_UNSPECIFIED 0x0
#define	ETH_PHY_SFP_FIBER   0x1
#define	ETH_PHY_XFP_FIBER   0x2
#define	ETH_PHY_DA_TWINAX   0x3
#define	ETH_PHY_BASE_T      0x4
#define	ETH_PHY_KR          0xf0
#define	ETH_PHY_CX4         0xf1
#define	ETH_PHY_NOT_PRESENT 0xff

	
	u32 ver_addr;

	u16 req_flow_ctrl;

	u16 req_line_speed;

	u32 speed_cap_mask;

	u16 req_duplex;
	u16 rsrv;
	config_init_t config_init;

	
	read_status_t read_status;

	
	link_reset_t link_reset;

	
	config_loopback_t config_loopback;

	
	format_fw_ver_t format_fw_ver;

	
	hw_reset_t hw_reset;

	
	set_link_led_t set_link_led;

	
	phy_specific_func_t phy_specific_func;
#define DISABLE_TX	1
#define ENABLE_TX	2
};

struct link_params {

	u8 port;

	
	u8 loopback_mode;
#define LOOPBACK_NONE		0
#define LOOPBACK_EMAC		1
#define LOOPBACK_BMAC		2
#define LOOPBACK_XGXS		3
#define LOOPBACK_EXT_PHY	4
#define LOOPBACK_EXT		5
#define LOOPBACK_UMAC		6
#define LOOPBACK_XMAC		7

	
	u8 mac_addr[6];

	u16 req_duplex[LINK_CONFIG_SIZE];
	u16 req_flow_ctrl[LINK_CONFIG_SIZE];

	u16 req_line_speed[LINK_CONFIG_SIZE]; 

	
	u32 shmem_base;
	u32 shmem2_base;
	u32 speed_cap_mask[LINK_CONFIG_SIZE];
	u32 switch_cfg;
#define SWITCH_CFG_1G		PORT_FEATURE_CON_SWITCH_1G_SWITCH
#define SWITCH_CFG_10G		PORT_FEATURE_CON_SWITCH_10G_SWITCH
#define SWITCH_CFG_AUTO_DETECT	PORT_FEATURE_CON_SWITCH_AUTO_DETECT

	u32 lane_config;

	
	u32 chip_id;

	
	u32 feature_config_flags;
#define FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED	(1<<0)
#define FEATURE_CONFIG_PFC_ENABLED			(1<<1)
#define FEATURE_CONFIG_BC_SUPPORTS_OPT_MDL_VRFY		(1<<2)
#define FEATURE_CONFIG_BC_SUPPORTS_DUAL_PHY_OPT_MDL_VRFY	(1<<3)
#define FEATURE_CONFIG_AUTOGREEEN_ENABLED			(1<<9)
#define FEATURE_CONFIG_BC_SUPPORTS_SFP_TX_DISABLED		(1<<10)
	
	struct bnx2x_phy phy[MAX_PHYS];

	
	u8 num_phys;

	u8 rsrv;
	u16 hw_led_mode; 
	u32 multi_phy_config;

	
	struct bnx2x *bp;
	u16 req_fc_auto_adv; 
};

struct link_vars {
	u8 phy_flags;
#define PHY_XGXS_FLAG			(1<<0)
#define PHY_SGMII_FLAG			(1<<1)
#define PHY_PHYSICAL_LINK_FLAG		(1<<2)
#define PHY_HALF_OPEN_CONN_FLAG		(1<<3)
#define PHY_OVER_CURRENT_FLAG		(1<<4)

	u8 mac_type;
#define MAC_TYPE_NONE		0
#define MAC_TYPE_EMAC		1
#define MAC_TYPE_BMAC		2
#define MAC_TYPE_UMAC		3
#define MAC_TYPE_XMAC		4

	u8 phy_link_up; 
	u8 link_up;

	u16 line_speed;
	u16 duplex;

	u16 flow_ctrl;
	u16 ieee_fc;

	
	u32 link_status;
	u8 fault_detected;
	u8 rsrv1;
	u16 periodic_flags;
#define PERIODIC_FLAGS_LINK_EVENT	0x0001

	u32 aeu_int_mask;
	u8 rx_tx_asic_rst;
	u8 turn_to_run_wc_rt;
	u16 rsrv2;
};

int bnx2x_phy_init(struct link_params *params, struct link_vars *vars);

int bnx2x_link_reset(struct link_params *params, struct link_vars *vars,
		     u8 reset_ext_phy);

int bnx2x_link_update(struct link_params *params, struct link_vars *vars);

int bnx2x_phy_read(struct link_params *params, u8 phy_addr,
		   u8 devad, u16 reg, u16 *ret_val);

int bnx2x_phy_write(struct link_params *params, u8 phy_addr,
		    u8 devad, u16 reg, u16 val);

void bnx2x_link_status_update(struct link_params *input,
			    struct link_vars *output);
int bnx2x_get_ext_phy_fw_version(struct link_params *params, u8 *version,
				 u16 len);

int bnx2x_set_led(struct link_params *params,
		  struct link_vars *vars, u8 mode, u32 speed);
#define LED_MODE_OFF			0
#define LED_MODE_ON			1
#define LED_MODE_OPER			2
#define LED_MODE_FRONT_PANEL_OFF	3

void bnx2x_handle_module_detect_int(struct link_params *params);

int bnx2x_test_link(struct link_params *params, struct link_vars *vars,
		    u8 is_serdes);

int bnx2x_common_init_phy(struct bnx2x *bp, u32 shmem_base_path[],
			  u32 shmem2_base_path[], u32 chip_id);

void bnx2x_ext_phy_hw_reset(struct bnx2x *bp, u8 port);

void bnx2x_sfx7101_sp_sw_reset(struct bnx2x *bp, struct bnx2x_phy *phy);

int bnx2x_read_sfp_module_eeprom(struct bnx2x_phy *phy,
				 struct link_params *params, u16 addr,
				 u8 byte_cnt, u8 *o_buf);

void bnx2x_hw_reset_phy(struct link_params *params);

u8 bnx2x_hw_lock_required(struct bnx2x *bp, u32 shmem_base,
			  u32 shmem2_base);

u32 bnx2x_phy_selection(struct link_params *params);

int bnx2x_phy_probe(struct link_params *params);

u8 bnx2x_fan_failure_det_req(struct bnx2x *bp, u32 shmem_base,
			     u32 shmem2_base, u8 port);




#define DCBX_E2E3_MAX_NUM_COS		(2)
#define DCBX_E3B0_MAX_NUM_COS_PORT0	(6)
#define DCBX_E3B0_MAX_NUM_COS_PORT1	(3)
#define DCBX_E3B0_MAX_NUM_COS		( \
			MAXVAL(DCBX_E3B0_MAX_NUM_COS_PORT0, \
			    DCBX_E3B0_MAX_NUM_COS_PORT1))

#define DCBX_MAX_NUM_COS			( \
			MAXVAL(DCBX_E3B0_MAX_NUM_COS, \
			    DCBX_E2E3_MAX_NUM_COS))

struct bnx2x_nig_brb_pfc_port_params {
	
	u32 pause_enable;
	u32 llfc_out_en;
	u32 llfc_enable;
	u32 pkt_priority_to_cos;
	u8 num_of_rx_cos_priority_mask;
	u32 rx_cos_priority_mask[DCBX_MAX_NUM_COS];
	u32 llfc_high_priority_classes;
	u32 llfc_low_priority_classes;
	
	u32 cos0_pauseable;
	u32 cos1_pauseable;
};


struct bnx2x_ets_bw_params {
	u8 bw;
};

struct bnx2x_ets_sp_params {
	u8 pri;
};

enum bnx2x_cos_state {
	bnx2x_cos_state_strict = 0,
	bnx2x_cos_state_bw = 1,
};

struct bnx2x_ets_cos_params {
	enum bnx2x_cos_state state ;
	union {
		struct bnx2x_ets_bw_params bw_params;
		struct bnx2x_ets_sp_params sp_params;
	} params;
};

struct bnx2x_ets_params {
	u8 num_of_cos; 
	struct bnx2x_ets_cos_params cos[DCBX_MAX_NUM_COS];
};

int bnx2x_update_pfc(struct link_params *params,
		      struct link_vars *vars,
		      struct bnx2x_nig_brb_pfc_port_params *pfc_params);


int bnx2x_ets_disabled(struct link_params *params,
		       struct link_vars *vars);

void bnx2x_ets_bw_limit(const struct link_params *params, const u32 cos0_bw,
			const u32 cos1_bw);

int bnx2x_ets_strict(const struct link_params *params, const u8 strict_cos);


int bnx2x_ets_e3b0_config(const struct link_params *params,
			 const struct link_vars *vars,
			 struct bnx2x_ets_params *ets_params);
void bnx2x_pfc_statistic(struct link_params *params, struct link_vars *vars,
						 u32 pfc_frames_sent[2],
						 u32 pfc_frames_received[2]);
void bnx2x_init_mod_abs_int(struct bnx2x *bp, struct link_vars *vars,
			    u32 chip_id, u32 shmem_base, u32 shmem2_base,
			    u8 port);

int bnx2x_sfp_module_detection(struct bnx2x_phy *phy,
			       struct link_params *params);

void bnx2x_period_func(struct link_params *params, struct link_vars *vars);

#endif 
