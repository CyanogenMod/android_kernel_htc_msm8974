/* Copyright (c) 2009-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_PCOM_H
#define __ARCH_ARM_MACH_MSM_CLOCK_PCOM_H

#include <mach/clk-provider.h>


#define P_ACPU_CLK	0   
#define P_ADM_CLK	1   
#define P_ADSP_CLK	2   
#define P_EBI1_CLK	3   
#define P_EBI2_CLK	4   
#define P_ECODEC_CLK	5   
#define P_EMDH_CLK	6   
#define P_GP_CLK	7   
#define P_GRP_3D_CLK	8   
#define P_I2C_CLK	9   
#define P_ICODEC_RX_CLK	10  
#define P_ICODEC_TX_CLK	11  
#define P_IMEM_CLK	12  
#define P_MDC_CLK	13  
#define P_MDP_CLK	14  
#define P_PBUS_CLK	15  
#define P_PCM_CLK	16  
#define P_PMDH_CLK	17  
#define P_SDAC_CLK	18  
#define P_SDC1_CLK	19  
#define P_SDC1_P_CLK	20
#define P_SDC2_CLK	21
#define P_SDC2_P_CLK	22
#define P_SDC3_CLK	23
#define P_SDC3_P_CLK	24
#define P_SDC4_CLK	25
#define P_SDC4_P_CLK	26
#define P_TSIF_CLK	27  
#define P_TSIF_REF_CLK	28
#define P_TV_DAC_CLK	29  
#define P_TV_ENC_CLK	30
#define P_UART1_CLK	31  
#define P_UART2_CLK	32
#define P_UART3_CLK	33
#define P_UART1DM_CLK	34
#define P_UART2DM_CLK	35
#define P_USB_HS_CLK	36  
#define P_USB_HS_P_CLK	37  
#define P_USB_OTG_CLK	38  
#define P_VDC_CLK	39  
#define P_VFE_MDC_CLK	40  
#define P_VFE_CLK	41  
#define P_MDP_LCDC_PCLK_CLK	42
#define P_MDP_LCDC_PAD_PCLK_CLK 43
#define P_MDP_VSYNC_CLK	44
#define P_SPI_CLK	45
#define P_VFE_AXI_CLK	46
#define P_USB_HS2_CLK	47  
#define P_USB_HS2_P_CLK	48  
#define P_USB_HS3_CLK	49  
#define P_USB_HS3_P_CLK	50  
#define P_GRP_3D_P_CLK	51  
#define P_USB_PHY_CLK	52  
#define P_USB_HS_CORE_CLK	53  
#define P_USB_HS2_CORE_CLK	54  
#define P_USB_HS3_CORE_CLK	55  
#define P_CAM_M_CLK		56
#define P_CAMIF_PAD_P_CLK	57
#define P_GRP_2D_CLK		58
#define P_GRP_2D_P_CLK		59
#define P_I2S_CLK		60
#define P_JPEG_CLK		61
#define P_JPEG_P_CLK		62
#define P_LPA_CODEC_CLK		63
#define P_LPA_CORE_CLK		64
#define P_LPA_P_CLK		65
#define P_MDC_IO_CLK		66
#define P_MDC_P_CLK		67
#define P_MFC_CLK		68
#define P_MFC_DIV2_CLK		69
#define P_MFC_P_CLK		70
#define P_QUP_I2C_CLK		71
#define P_ROTATOR_IMEM_CLK	72
#define P_ROTATOR_P_CLK		73
#define P_VFE_CAMIF_CLK		74
#define P_VFE_P_CLK		75
#define P_VPE_CLK		76
#define P_I2C_2_CLK		77
#define P_MI2S_CODEC_RX_S_CLK	78
#define P_MI2S_CODEC_RX_M_CLK	79
#define P_MI2S_CODEC_TX_S_CLK	80
#define P_MI2S_CODEC_TX_M_CLK	81
#define P_PMDH_P_CLK		82
#define P_EMDH_P_CLK		83
#define P_SPI_P_CLK		84
#define P_TSIF_P_CLK		85
#define P_MDP_P_CLK		86
#define P_SDAC_M_CLK		87
#define P_MI2S_S_CLK		88
#define P_MI2S_M_CLK		89
#define P_AXI_ROTATOR_CLK	90
#define P_HDMI_CLK		91
#define P_CSI0_CLK		92
#define P_CSI0_VFE_CLK		93
#define P_CSI0_P_CLK		94
#define P_CSI1_CLK		95
#define P_CSI1_VFE_CLK		96
#define P_CSI1_P_CLK		97
#define P_GSBI_CLK		98
#define P_GSBI_P_CLK		99
#define P_CE_CLK		100 
#define P_CODEC_SSBI_CLK	101
#define P_TCXO_DIV4_CLK		102
#define P_GSBI1_QUP_CLK		103
#define P_GSBI2_QUP_CLK		104
#define P_GSBI1_QUP_P_CLK	105
#define P_GSBI2_QUP_P_CLK	106
#define P_DSI_CLK		107
#define P_DSI_ESC_CLK		108
#define P_DSI_PIXEL_CLK		109
#define P_DSI_BYTE_CLK		110
#define P_EBI1_FIXED_CLK	111 
#define P_DSI_REF_CLK		112
#define P_MDP_DSI_P_CLK		113
#define P_AHB_M_CLK		114
#define P_AHB_S_CLK		115

#define P_NR_CLKS		116

extern int pc_clk_reset(unsigned id, enum clk_reset_action action);

struct clk_ops;
extern struct clk_ops clk_ops_pcom;
extern struct clk_ops clk_ops_pcom_div2;
extern struct clk_ops clk_ops_pcom_ext_config;

struct pcom_clk {
	unsigned id;
	struct clk c;
};

static inline struct pcom_clk *to_pcom_clk(struct clk *clk)
{
	return container_of(clk, struct pcom_clk, c);
}

#define DEFINE_CLK_PCOM(clk_name, clk_id, clk_flags) \
	struct pcom_clk clk_name = { \
		.id = P_##clk_id, \
		.c = { \
			.ops = &clk_ops_pcom, \
			.flags = clk_flags, \
			.dbg_name = #clk_id, \
			CLK_INIT(clk_name.c), \
		}, \
	}

#endif
