/* include/video/s1d13xxxfb.h
 *
 * (c) 2004 Simtec Electronics
 * (c) 2005 Thibaut VARENE <varenet@parisc-linux.org>
 *
 * Header file for Epson S1D13XXX driver code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef	S1D13XXXFB_H
#define	S1D13XXXFB_H

#define S1D_PALETTE_SIZE		256
#define S1D_FBID			"S1D13xxx"
#define S1D_DEVICENAME			"s1d13xxxfb"

#define S1D13505_PROD_ID		0x3	
#define S1D13506_PROD_ID		0x4	
#define S1D13806_PROD_ID		0x7	

#define S1DREG_REV_CODE			0x0000	
#define S1DREG_MISC			0x0001	
#define S1DREG_GPIO_CNF0		0x0004	
#define S1DREG_GPIO_CNF1		0x0005	
#define S1DREG_GPIO_CTL0		0x0008	
#define S1DREG_GPIO_CTL1		0x0009	
#define S1DREG_CNF_STATUS		0x000C	
#define S1DREG_CLK_CNF			0x0010	
#define S1DREG_LCD_CLK_CNF		0x0014	
#define S1DREG_CRT_CLK_CNF		0x0018	
#define S1DREG_MPLUG_CLK_CNF		0x001C	
#define S1DREG_CPU2MEM_WST_SEL		0x001E	
#define S1DREG_MEM_CNF			0x0020	
#define S1DREG_SDRAM_REF_RATE		0x0021	
#define S1DREG_SDRAM_TC0		0x002A	
#define S1DREG_SDRAM_TC1		0x002B	
#define S1DREG_PANEL_TYPE		0x0030	
#define S1DREG_MOD_RATE			0x0031	
#define S1DREG_LCD_DISP_HWIDTH		0x0032	
#define S1DREG_LCD_NDISP_HPER		0x0034	
#define S1DREG_TFT_FPLINE_START		0x0035	
#define S1DREG_TFT_FPLINE_PWIDTH	0x0036	
#define S1DREG_LCD_DISP_VHEIGHT0	0x0038	
#define S1DREG_LCD_DISP_VHEIGHT1	0x0039	
#define S1DREG_LCD_NDISP_VPER		0x003A	
#define S1DREG_TFT_FPFRAME_START	0x003B	
#define S1DREG_TFT_FPFRAME_PWIDTH	0x003C	
#define S1DREG_LCD_DISP_MODE		0x0040	
#define S1DREG_LCD_MISC			0x0041	
#define S1DREG_LCD_DISP_START0		0x0042	
#define S1DREG_LCD_DISP_START1		0x0043	
#define S1DREG_LCD_DISP_START2		0x0044	
#define S1DREG_LCD_MEM_OFF0		0x0046	
#define S1DREG_LCD_MEM_OFF1		0x0047	
#define S1DREG_LCD_PIX_PAN		0x0048	
#define S1DREG_LCD_DISP_FIFO_HTC	0x004A	
#define S1DREG_LCD_DISP_FIFO_LTC	0x004B	
#define S1DREG_CRT_DISP_HWIDTH		0x0050	
#define S1DREG_CRT_NDISP_HPER		0x0052	
#define S1DREG_CRT_HRTC_START		0x0053	
#define S1DREG_CRT_HRTC_PWIDTH		0x0054	
#define S1DREG_CRT_DISP_VHEIGHT0	0x0056	
#define S1DREG_CRT_DISP_VHEIGHT1	0x0057	
#define S1DREG_CRT_NDISP_VPER		0x0058	
#define S1DREG_CRT_VRTC_START		0x0059	
#define S1DREG_CRT_VRTC_PWIDTH		0x005A	
#define S1DREG_TV_OUT_CTL		0x005B	
#define S1DREG_CRT_DISP_MODE		0x0060	
#define S1DREG_CRT_DISP_START0		0x0062	
#define S1DREG_CRT_DISP_START1		0x0063	
#define S1DREG_CRT_DISP_START2		0x0064	
#define S1DREG_CRT_MEM_OFF0		0x0066	
#define S1DREG_CRT_MEM_OFF1		0x0067	
#define S1DREG_CRT_PIX_PAN		0x0068	
#define S1DREG_CRT_DISP_FIFO_HTC	0x006A	
#define S1DREG_CRT_DISP_FIFO_LTC	0x006B	
#define S1DREG_LCD_CUR_CTL		0x0070	
#define S1DREG_LCD_CUR_START		0x0071	
#define S1DREG_LCD_CUR_XPOS0		0x0072	
#define S1DREG_LCD_CUR_XPOS1		0x0073	
#define S1DREG_LCD_CUR_YPOS0		0x0074	
#define S1DREG_LCD_CUR_YPOS1		0x0075	
#define S1DREG_LCD_CUR_BCTL0		0x0076	
#define S1DREG_LCD_CUR_GCTL0		0x0077	
#define S1DREG_LCD_CUR_RCTL0		0x0078	
#define S1DREG_LCD_CUR_BCTL1		0x007A	
#define S1DREG_LCD_CUR_GCTL1		0x007B	
#define S1DREG_LCD_CUR_RCTL1		0x007C	
#define S1DREG_LCD_CUR_FIFO_HTC		0x007E	
#define S1DREG_CRT_CUR_CTL		0x0080	
#define S1DREG_CRT_CUR_START		0x0081	
#define S1DREG_CRT_CUR_XPOS0		0x0082	
#define S1DREG_CRT_CUR_XPOS1		0x0083	
#define S1DREG_CRT_CUR_YPOS0		0x0084	
#define S1DREG_CRT_CUR_YPOS1		0x0085	
#define S1DREG_CRT_CUR_BCTL0		0x0086	
#define S1DREG_CRT_CUR_GCTL0		0x0087	
#define S1DREG_CRT_CUR_RCTL0		0x0088	
#define S1DREG_CRT_CUR_BCTL1		0x008A	
#define S1DREG_CRT_CUR_GCTL1		0x008B	
#define S1DREG_CRT_CUR_RCTL1		0x008C	
#define S1DREG_CRT_CUR_FIFO_HTC		0x008E	
#define S1DREG_BBLT_CTL0		0x0100	
#define S1DREG_BBLT_CTL1		0x0101	
#define S1DREG_BBLT_CC_EXP		0x0102	
#define S1DREG_BBLT_OP			0x0103	
#define S1DREG_BBLT_SRC_START0		0x0104	
#define S1DREG_BBLT_SRC_START1		0x0105	
#define S1DREG_BBLT_SRC_START2		0x0106	
#define S1DREG_BBLT_DST_START0		0x0108	
#define S1DREG_BBLT_DST_START1		0x0109	
#define S1DREG_BBLT_DST_START2		0x010A	
#define S1DREG_BBLT_MEM_OFF0		0x010C	
#define S1DREG_BBLT_MEM_OFF1		0x010D	
#define S1DREG_BBLT_WIDTH0		0x0110	
#define S1DREG_BBLT_WIDTH1		0x0111	
#define S1DREG_BBLT_HEIGHT0		0x0112	
#define S1DREG_BBLT_HEIGHT1		0x0113	
#define S1DREG_BBLT_BGC0		0x0114	
#define S1DREG_BBLT_BGC1		0x0115	
#define S1DREG_BBLT_FGC0		0x0118	
#define S1DREG_BBLT_FGC1		0x0119	
#define S1DREG_LKUP_MODE		0x01E0	
#define S1DREG_LKUP_ADDR		0x01E2	
#define S1DREG_LKUP_DATA		0x01E4	
#define S1DREG_PS_CNF			0x01F0	
#define S1DREG_PS_STATUS		0x01F1	
#define S1DREG_CPU2MEM_WDOGT		0x01F4	
#define S1DREG_COM_DISP_MODE		0x01FC	

#define S1DREG_DELAYOFF			0xFFFE
#define S1DREG_DELAYON			0xFFFF

#define BBLT_SOLID_FILL			0x0c



struct s1d13xxxfb_regval {
	u16	addr;
	u8	value;
};

struct s1d13xxxfb_par {
	void __iomem	*regs;
	unsigned char	display;
	unsigned char	prod_id;
	unsigned char	revision;

	unsigned int	pseudo_palette[16];
#ifdef CONFIG_PM
	void		*regs_save;	
	void		*disp_save;	
#endif
};

struct s1d13xxxfb_pdata {
	const struct s1d13xxxfb_regval	*initregs;
	const unsigned int		initregssize;
	void				(*platform_init_video)(void);
#ifdef CONFIG_PM
	int				(*platform_suspend_video)(void);
	int				(*platform_resume_video)(void);
#endif
};

#endif

