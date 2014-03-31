/*
 * Copyright © 2006 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifndef _I830_BIOS_H_
#define _I830_BIOS_H_

#include "drmP.h"

struct vbt_header {
	u8 signature[20];		
	u16 version;			
	u16 header_size;		
	u16 vbt_size;			
	u8 vbt_checksum;
	u8 reserved0;
	u32 bdb_offset;			
	u32 aim_offset[4];		
} __attribute__((packed));

struct bdb_header {
	u8 signature[16];		
	u16 version;			
	u16 header_size;		
	u16 bdb_size;			
};

struct vbios_data {
	u8 type; 
	u8 relstage;
	u8 chipset;
	u8 lvds_present:1;
	u8 tv_present:1;
	u8 rsvd2:6; 
	u8 rsvd3[4];
	u8 signon[155];
	u8 copyright[61];
	u16 code_segment;
	u8 dos_boot_mode;
	u8 bandwidth_percent;
	u8 rsvd4; 
	u8 resize_pci_bios;
	u8 rsvd5; 
} __attribute__((packed));

#define BDB_GENERAL_FEATURES	  1
#define BDB_GENERAL_DEFINITIONS	  2
#define BDB_OLD_TOGGLE_LIST	  3
#define BDB_MODE_SUPPORT_LIST	  4
#define BDB_GENERIC_MODE_TABLE	  5
#define BDB_EXT_MMIO_REGS	  6
#define BDB_SWF_IO		  7
#define BDB_SWF_MMIO		  8
#define BDB_DOT_CLOCK_TABLE	  9
#define BDB_MODE_REMOVAL_TABLE	 10
#define BDB_CHILD_DEVICE_TABLE	 11
#define BDB_DRIVER_FEATURES	 12
#define BDB_DRIVER_PERSISTENCE	 13
#define BDB_EXT_TABLE_PTRS	 14
#define BDB_DOT_CLOCK_OVERRIDE	 15
#define BDB_DISPLAY_SELECT	 16
#define BDB_DRIVER_ROTATION	 18
#define BDB_DISPLAY_REMOVE	 19
#define BDB_OEM_CUSTOM		 20
#define BDB_EFP_LIST		 21 
#define BDB_SDVO_LVDS_OPTIONS	 22
#define BDB_SDVO_PANEL_DTDS	 23
#define BDB_SDVO_LVDS_PNP_IDS	 24
#define BDB_SDVO_LVDS_POWER_SEQ	 25
#define BDB_TV_OPTIONS		 26
#define BDB_EDP			 27
#define BDB_LVDS_OPTIONS	 40
#define BDB_LVDS_LFP_DATA_PTRS	 41
#define BDB_LVDS_LFP_DATA	 42
#define BDB_LVDS_BACKLIGHT	 43
#define BDB_LVDS_POWER		 44
#define BDB_SKIP		254 

struct bdb_general_features {
        
	u8 panel_fitting:2;
	u8 flexaim:1;
	u8 msg_enable:1;
	u8 clear_screen:3;
	u8 color_flip:1;

        
	u8 download_ext_vbt:1;
	u8 enable_ssc:1;
	u8 ssc_freq:1;
	u8 enable_lfp_on_override:1;
	u8 disable_ssc_ddt:1;
	u8 rsvd7:1;
	u8 display_clock_mode:1;
	u8 rsvd8:1; 

        
	u8 disable_smooth_vision:1;
	u8 single_dvi:1;
	u8 rsvd9:6; 

        
	u8 legacy_monitor_detect;

        
	u8 int_crt_support:1;
	u8 int_tv_support:1;
	u8 int_efp_support:1;
	u8 dp_ssc_enb:1;	
	u8 dp_ssc_freq:1;	
	u8 rsvd11:3; 
} __attribute__((packed));

#define GPIO_PIN_DVI_LVDS	0x03 
#define GPIO_PIN_ADD_I2C	0x05 
#define GPIO_PIN_ADD_DDC	0x04 
#define GPIO_PIN_ADD_DDC_I2C	0x06 

#define DEVICE_TYPE_NONE	0x00
#define DEVICE_TYPE_CRT		0x01
#define DEVICE_TYPE_TV		0x09
#define DEVICE_TYPE_EFP		0x12
#define DEVICE_TYPE_LFP		0x22
#define DEVICE_TYPE_CRT_DPMS		0x6001
#define DEVICE_TYPE_CRT_DPMS_HOTPLUG	0x4001
#define DEVICE_TYPE_TV_COMPOSITE	0x0209
#define DEVICE_TYPE_TV_MACROVISION	0x0289
#define DEVICE_TYPE_TV_RF_COMPOSITE	0x020c
#define DEVICE_TYPE_TV_SVIDEO_COMPOSITE	0x0609
#define DEVICE_TYPE_TV_SCART		0x0209
#define DEVICE_TYPE_TV_CODEC_HOTPLUG_PWR 0x6009
#define DEVICE_TYPE_EFP_HOTPLUG_PWR	0x6012
#define DEVICE_TYPE_EFP_DVI_HOTPLUG_PWR	0x6052
#define DEVICE_TYPE_EFP_DVI_I		0x6053
#define DEVICE_TYPE_EFP_DVI_D_DUAL	0x6152
#define DEVICE_TYPE_EFP_DVI_D_HDCP	0x60d2
#define DEVICE_TYPE_OPENLDI_HOTPLUG_PWR	0x6062
#define DEVICE_TYPE_OPENLDI_DUALPIX	0x6162
#define DEVICE_TYPE_LFP_PANELLINK	0x5012
#define DEVICE_TYPE_LFP_CMOS_PWR	0x5042
#define DEVICE_TYPE_LFP_LVDS_PWR	0x5062
#define DEVICE_TYPE_LFP_LVDS_DUAL	0x5162
#define DEVICE_TYPE_LFP_LVDS_DUAL_HDCP	0x51e2

#define DEVICE_CFG_NONE		0x00
#define DEVICE_CFG_12BIT_DVOB	0x01
#define DEVICE_CFG_12BIT_DVOC	0x02
#define DEVICE_CFG_24BIT_DVOBC	0x09
#define DEVICE_CFG_24BIT_DVOCB	0x0a
#define DEVICE_CFG_DUAL_DVOB	0x11
#define DEVICE_CFG_DUAL_DVOC	0x12
#define DEVICE_CFG_DUAL_DVOBC	0x13
#define DEVICE_CFG_DUAL_LINK_DVOBC	0x19
#define DEVICE_CFG_DUAL_LINK_DVOCB	0x1a

#define DEVICE_WIRE_NONE	0x00
#define DEVICE_WIRE_DVOB	0x01
#define DEVICE_WIRE_DVOC	0x02
#define DEVICE_WIRE_DVOBC	0x03
#define DEVICE_WIRE_DVOBB	0x05
#define DEVICE_WIRE_DVOCC	0x06
#define DEVICE_WIRE_DVOB_MASTER 0x0d
#define DEVICE_WIRE_DVOC_MASTER 0x0e

#define DEVICE_PORT_DVOA	0x00 
#define DEVICE_PORT_DVOB	0x01
#define DEVICE_PORT_DVOC	0x02

struct child_device_config {
	u16 handle;
	u16 device_type;
	u8  device_id[10]; 
	u16 addin_offset;
	u8  dvo_port; 
	u8  i2c_pin;
	u8  slave_addr;
	u8  ddc_pin;
	u16 edid_ptr;
	u8  dvo_cfg; 
	u8  dvo2_port;
	u8  i2c2_pin;
	u8  slave2_addr;
	u8  ddc2_pin;
	u8  capabilities;
	u8  dvo_wiring;
	u8  dvo2_wiring;
	u16 extended_type;
	u8  dvo_function;
} __attribute__((packed));

struct bdb_general_definitions {
	
	u8 crt_ddc_gmbus_pin;

	
	u8 dpms_acpi:1;
	u8 skip_boot_crt_detect:1;
	u8 dpms_aim:1;
	u8 rsvd1:5; 

	
	u8 boot_display[2];
	u8 child_dev_size;

	struct child_device_config devices[0];
} __attribute__((packed));

struct bdb_lvds_options {
	u8 panel_type;
	u8 rsvd1;
	
	u8 pfit_mode:2;
	u8 pfit_text_mode_enhanced:1;
	u8 pfit_gfx_mode_enhanced:1;
	u8 pfit_ratio_auto:1;
	u8 pixel_dither:1;
	u8 lvds_edid:1;
	u8 rsvd2:1;
	u8 rsvd4;
} __attribute__((packed));

struct bdb_lvds_lfp_data_ptr {
	u16 fp_timing_offset; 
	u8 fp_table_size;
	u16 dvo_timing_offset;
	u8 dvo_table_size;
	u16 panel_pnp_id_offset;
	u8 pnp_table_size;
} __attribute__((packed));

struct bdb_lvds_lfp_data_ptrs {
	u8 lvds_entries; 
	struct bdb_lvds_lfp_data_ptr ptr[16];
} __attribute__((packed));

struct lvds_fp_timing {
	u16 x_res;
	u16 y_res;
	u32 lvds_reg;
	u32 lvds_reg_val;
	u32 pp_on_reg;
	u32 pp_on_reg_val;
	u32 pp_off_reg;
	u32 pp_off_reg_val;
	u32 pp_cycle_reg;
	u32 pp_cycle_reg_val;
	u32 pfit_reg;
	u32 pfit_reg_val;
	u16 terminator;
} __attribute__((packed));

struct lvds_dvo_timing {
	u16 clock;		
	u8 hactive_lo;
	u8 hblank_lo;
	u8 hblank_hi:4;
	u8 hactive_hi:4;
	u8 vactive_lo;
	u8 vblank_lo;
	u8 vblank_hi:4;
	u8 vactive_hi:4;
	u8 hsync_off_lo;
	u8 hsync_pulse_width;
	u8 vsync_pulse_width:4;
	u8 vsync_off:4;
	u8 rsvd0:6;
	u8 hsync_off_hi:2;
	u8 h_image;
	u8 v_image;
	u8 max_hv;
	u8 h_border;
	u8 v_border;
	u8 rsvd1:3;
	u8 digital:2;
	u8 vsync_positive:1;
	u8 hsync_positive:1;
	u8 rsvd2:1;
} __attribute__((packed));

struct lvds_pnp_id {
	u16 mfg_name;
	u16 product_code;
	u32 serial;
	u8 mfg_week;
	u8 mfg_year;
} __attribute__((packed));

struct bdb_lvds_lfp_data_entry {
	struct lvds_fp_timing fp_timing;
	struct lvds_dvo_timing dvo_timing;
	struct lvds_pnp_id pnp_id;
} __attribute__((packed));

struct bdb_lvds_lfp_data {
	struct bdb_lvds_lfp_data_entry data[16];
} __attribute__((packed));

struct aimdb_header {
	char signature[16];
	char oem_device[20];
	u16 aimdb_version;
	u16 aimdb_header_size;
	u16 aimdb_size;
} __attribute__((packed));

struct aimdb_block {
	u8 aimdb_id;
	u16 aimdb_size;
} __attribute__((packed));

struct vch_panel_data {
	u16 fp_timing_offset;
	u8 fp_timing_size;
	u16 dvo_timing_offset;
	u8 dvo_timing_size;
	u16 text_fitting_offset;
	u8 text_fitting_size;
	u16 graphics_fitting_offset;
	u8 graphics_fitting_size;
} __attribute__((packed));

struct vch_bdb_22 {
	struct aimdb_block aimdb_block;
	struct vch_panel_data panels[16];
} __attribute__((packed));

struct bdb_sdvo_lvds_options {
	u8 panel_backlight;
	u8 h40_set_panel_type;
	u8 panel_type;
	u8 ssc_clk_freq;
	u16 als_low_trip;
	u16 als_high_trip;
	u8 sclalarcoeff_tab_row_num;
	u8 sclalarcoeff_tab_row_size;
	u8 coefficient[8];
	u8 panel_misc_bits_1;
	u8 panel_misc_bits_2;
	u8 panel_misc_bits_3;
	u8 panel_misc_bits_4;
} __attribute__((packed));


#define BDB_DRIVER_FEATURE_NO_LVDS		0
#define BDB_DRIVER_FEATURE_INT_LVDS		1
#define BDB_DRIVER_FEATURE_SDVO_LVDS		2
#define BDB_DRIVER_FEATURE_EDP			3

struct bdb_driver_features {
	u8 boot_dev_algorithm:1;
	u8 block_display_switch:1;
	u8 allow_display_switch:1;
	u8 hotplug_dvo:1;
	u8 dual_view_zoom:1;
	u8 int15h_hook:1;
	u8 sprite_in_clone:1;
	u8 primary_lfp_id:1;

	u16 boot_mode_x;
	u16 boot_mode_y;
	u8 boot_mode_bpp;
	u8 boot_mode_refresh;

	u16 enable_lfp_primary:1;
	u16 selective_mode_pruning:1;
	u16 dual_frequency:1;
	u16 render_clock_freq:1; 
	u16 nt_clone_support:1;
	u16 power_scheme_ui:1; 
	u16 sprite_display_assign:1; 
	u16 cui_aspect_scaling:1;
	u16 preserve_aspect_ratio:1;
	u16 sdvo_device_power_down:1;
	u16 crt_hotplug:1;
	u16 lvds_config:2;
	u16 tv_hotplug:1;
	u16 hdmi_config:2;

	u8 static_display:1;
	u8 reserved2:7;
	u16 legacy_crt_max_x;
	u16 legacy_crt_max_y;
	u8 legacy_crt_max_refresh;

	u8 hdmi_termination;
	u8 custom_vbt_version;
} __attribute__((packed));

#define EDP_18BPP	0
#define EDP_24BPP	1
#define EDP_30BPP	2
#define EDP_RATE_1_62	0
#define EDP_RATE_2_7	1
#define EDP_LANE_1	0
#define EDP_LANE_2	1
#define EDP_LANE_4	3
#define EDP_PREEMPHASIS_NONE	0
#define EDP_PREEMPHASIS_3_5dB	1
#define EDP_PREEMPHASIS_6dB	2
#define EDP_PREEMPHASIS_9_5dB	3
#define EDP_VSWING_0_4V		0
#define EDP_VSWING_0_6V		1
#define EDP_VSWING_0_8V		2
#define EDP_VSWING_1_2V		3

struct edp_power_seq {
	u16 t1_t3;
	u16 t8;
	u16 t9;
	u16 t10;
	u16 t11_t12;
} __attribute__ ((packed));

struct edp_link_params {
	u8 rate:4;
	u8 lanes:4;
	u8 preemphasis:4;
	u8 vswing:4;
} __attribute__ ((packed));

struct bdb_edp {
	struct edp_power_seq power_seqs[16];
	u32 color_depth;
	struct edp_link_params link_params[16];
	u32 sdrrs_msa_timing_delay;

	
	u16 edp_s3d_feature;
	u16 edp_t3_optimization;
} __attribute__ ((packed));

void intel_setup_bios(struct drm_device *dev);
bool intel_parse_bios(struct drm_device *dev);


#define GR18_DRIVER_SWITCH_EN	(1<<7) 
#define GR18_HOTKEY_MASK	0x78 
#define   GR18_HK_NONE		(0x0<<3)
#define   GR18_HK_LFP_STRETCH	(0x1<<3)
#define   GR18_HK_TOGGLE_DISP	(0x2<<3)
#define   GR18_HK_DISP_SWITCH	(0x4<<3) 
#define   GR18_HK_POPUP_DISABLED (0x6<<3)
#define   GR18_HK_POPUP_ENABLED	(0x7<<3)
#define   GR18_HK_PFIT		(0x8<<3)
#define   GR18_HK_APM_CHANGE	(0xa<<3)
#define   GR18_HK_MULTIPLE	(0xc<<3)
#define GR18_USER_INT_EN	(1<<2)
#define GR18_A0000_FLUSH_EN	(1<<1)
#define GR18_SMM_EN		(1<<0)

#define SWF00_YRES_SHIFT	16
#define SWF00_XRES_SHIFT	0
#define SWF00_RES_MASK		0xffff

#define SWF01_TV2_FORMAT_SHIFT	8
#define SWF01_TV1_FORMAT_SHIFT	0
#define SWF01_TV_FORMAT_MASK	0xffff

#define SWF10_VBIOS_BLC_I2C_EN	(1<<29)
#define SWF10_GTT_OVERRIDE_EN	(1<<28)
#define SWF10_LFP_DPMS_OVR	(1<<27) 
#define SWF10_ACTIVE_TOGGLE_LIST_MASK (7<<24)
#define   SWF10_OLD_TOGGLE	0x0
#define   SWF10_TOGGLE_LIST_1	0x1
#define   SWF10_TOGGLE_LIST_2	0x2
#define   SWF10_TOGGLE_LIST_3	0x3
#define   SWF10_TOGGLE_LIST_4	0x4
#define SWF10_PANNING_EN	(1<<23)
#define SWF10_DRIVER_LOADED	(1<<22)
#define SWF10_EXTENDED_DESKTOP	(1<<21)
#define SWF10_EXCLUSIVE_MODE	(1<<20)
#define SWF10_OVERLAY_EN	(1<<19)
#define SWF10_PLANEB_HOLDOFF	(1<<18)
#define SWF10_PLANEA_HOLDOFF	(1<<17)
#define SWF10_VGA_HOLDOFF	(1<<16)
#define SWF10_ACTIVE_DISP_MASK	0xffff
#define   SWF10_PIPEB_LFP2	(1<<15)
#define   SWF10_PIPEB_EFP2	(1<<14)
#define   SWF10_PIPEB_TV2	(1<<13)
#define   SWF10_PIPEB_CRT2	(1<<12)
#define   SWF10_PIPEB_LFP	(1<<11)
#define   SWF10_PIPEB_EFP	(1<<10)
#define   SWF10_PIPEB_TV	(1<<9)
#define   SWF10_PIPEB_CRT	(1<<8)
#define   SWF10_PIPEA_LFP2	(1<<7)
#define   SWF10_PIPEA_EFP2	(1<<6)
#define   SWF10_PIPEA_TV2	(1<<5)
#define   SWF10_PIPEA_CRT2	(1<<4)
#define   SWF10_PIPEA_LFP	(1<<3)
#define   SWF10_PIPEA_EFP	(1<<2)
#define   SWF10_PIPEA_TV	(1<<1)
#define   SWF10_PIPEA_CRT	(1<<0)

#define SWF11_MEMORY_SIZE_SHIFT	16
#define SWF11_SV_TEST_EN	(1<<15)
#define SWF11_IS_AGP		(1<<14)
#define SWF11_DISPLAY_HOLDOFF	(1<<13)
#define SWF11_DPMS_REDUCED	(1<<12)
#define SWF11_IS_VBE_MODE	(1<<11)
#define SWF11_PIPEB_ACCESS	(1<<10) 
#define SWF11_DPMS_MASK		0x07
#define   SWF11_DPMS_OFF	(1<<2)
#define   SWF11_DPMS_SUSPEND	(1<<1)
#define   SWF11_DPMS_STANDBY	(1<<0)
#define   SWF11_DPMS_ON		0

#define SWF14_GFX_PFIT_EN	(1<<31)
#define SWF14_TEXT_PFIT_EN	(1<<30)
#define SWF14_LID_STATUS_CLOSED	(1<<29) 
#define SWF14_POPUP_EN		(1<<28)
#define SWF14_DISPLAY_HOLDOFF	(1<<27)
#define SWF14_DISP_DETECT_EN	(1<<26)
#define SWF14_DOCKING_STATUS_DOCKED (1<<25) 
#define SWF14_DRIVER_STATUS	(1<<24)
#define SWF14_OS_TYPE_WIN9X	(1<<23)
#define SWF14_OS_TYPE_WINNT	(1<<22)
#define SWF14_PM_TYPE_MASK	0x00070000
#define   SWF14_PM_ACPI_VIDEO	(0x4 << 16)
#define   SWF14_PM_ACPI		(0x3 << 16)
#define   SWF14_PM_APM_12	(0x2 << 16)
#define   SWF14_PM_APM_11	(0x1 << 16)
#define SWF14_HK_REQUEST_MASK	0x0000ffff 
          
#define   SWF14_DS_PIPEB_LFP2_EN (1<<15)
#define   SWF14_DS_PIPEB_EFP2_EN (1<<14)
#define   SWF14_DS_PIPEB_TV2_EN  (1<<13)
#define   SWF14_DS_PIPEB_CRT2_EN (1<<12)
#define   SWF14_DS_PIPEB_LFP_EN  (1<<11)
#define   SWF14_DS_PIPEB_EFP_EN  (1<<10)
#define   SWF14_DS_PIPEB_TV_EN   (1<<9)
#define   SWF14_DS_PIPEB_CRT_EN  (1<<8)
#define   SWF14_DS_PIPEA_LFP2_EN (1<<7)
#define   SWF14_DS_PIPEA_EFP2_EN (1<<6)
#define   SWF14_DS_PIPEA_TV2_EN  (1<<5)
#define   SWF14_DS_PIPEA_CRT2_EN (1<<4)
#define   SWF14_DS_PIPEA_LFP_EN  (1<<3)
#define   SWF14_DS_PIPEA_EFP_EN  (1<<2)
#define   SWF14_DS_PIPEA_TV_EN   (1<<1)
#define   SWF14_DS_PIPEA_CRT_EN  (1<<0)
          
#define   SWF14_PFIT_EN		(1<<0) 
          
#define   SWF14_APM_HIBERNATE	0x4
#define   SWF14_APM_SUSPEND	0x3
#define   SWF14_APM_STANDBY	0x1
#define   SWF14_APM_RESTORE	0x0

#define	 DEVICE_TYPE_INT_LFP	0x1022
#define	 DEVICE_TYPE_INT_TV	0x1009
#define	 DEVICE_TYPE_HDMI	0x60D2
#define	 DEVICE_TYPE_DP		0x68C6
#define	 DEVICE_TYPE_eDP	0x78C6

#define		DVO_B		1
#define		DVO_C		2
#define		DVO_D		3

#define		PORT_IDPB	7
#define		PORT_IDPC	8
#define		PORT_IDPD	9

#endif 
