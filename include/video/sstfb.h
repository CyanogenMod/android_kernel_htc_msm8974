/*
 * linux/drivers/video/sstfb.h -- voodoo graphics frame buffer
 *
 *     Copyright (c) 2000,2001 Ghozlane Toumi <gtoumi@messel.emse.fr>
 *
 *     Created 28 Aug 2001 by Ghozlane Toumi
 */


#ifndef _SSTFB_H_
#define _SSTFB_H_


#ifdef SST_DEBUG
#  define dprintk(X...)		printk("sstfb: " X)
#  define SST_DEBUG_REG  1
#  define SST_DEBUG_FUNC 1
#  define SST_DEBUG_VAR  1
#else
#  define dprintk(X...)
#  define SST_DEBUG_REG  0
#  define SST_DEBUG_FUNC 0
#  define SST_DEBUG_VAR  0
#endif

#if (SST_DEBUG_REG > 0)
#  define r_dprintk(X...)	dprintk(X)
#else
#  define r_dprintk(X...)
#endif
#if (SST_DEBUG_REG > 1)
#  define r_ddprintk(X...)	dprintk(" " X)
#else
#  define r_ddprintk(X...)
#endif

#if (SST_DEBUG_FUNC > 0)
#  define f_dprintk(X...)	dprintk(X)
#else
#  define f_dprintk(X...)
#endif
#if (SST_DEBUG_FUNC > 1)
#  define f_ddprintk(X...)	dprintk(" " X)
#else
#  define f_ddprintk(X...)
#endif
#if (SST_DEBUG_FUNC > 2)
#  define f_dddprintk(X...)	dprintk(" " X)
#else
#  define f_dddprintk(X...)
#endif

#if (SST_DEBUG_VAR > 0)
#  define v_dprintk(X...)	dprintk(X)
#  define print_var(V, X...)	\
   {				\
     dprintk(X);		\
     printk(" :\n");		\
     sst_dbg_print_var(V);	\
   }
#else
#  define v_dprintk(X...)
#  define print_var(X,Y...)
#endif

#define POW2(x)		(1ul<<(x))


#define PCI_INIT_ENABLE		0x40
#  define PCI_EN_INIT_WR	  BIT(0)
#  define PCI_EN_FIFO_WR	  BIT(1)
#  define PCI_REMAP_DAC		  BIT(2)
#define PCI_VCLK_ENABLE		0xc0	
#define PCI_VCLK_DISABLE	0xe0

#define STATUS			0x0000
#  define STATUS_FBI_BUSY	  BIT(7)
#define FBZMODE			0x0110
#  define EN_CLIPPING		  BIT(0)	
#  define EN_RGB_WRITE		  BIT(9)	
#  define EN_ALPHA_WRITE	  BIT(10)
#  define ENGINE_INVERT_Y	  BIT(17)	
#define LFBMODE			0x0114
#  define LFB_565		  0		
#  define LFB_888		  4		
#  define LFB_8888		  5		
#  define WR_BUFF_FRONT		  0		
#  define WR_BUFF_BACK		  (1 << 4)	
#  define RD_BUFF_FRONT		  0		
#  define RD_BUFF_BACK		  (1 << 6)	
#  define EN_PXL_PIPELINE	  BIT(8)	
#  define LFB_WORD_SWIZZLE_WR	  BIT(11)	
#  define LFB_BYTE_SWIZZLE_WR	  BIT(12)	
#  define LFB_INVERT_Y		  BIT(13)	
#  define LFB_WORD_SWIZZLE_RD	  BIT(15)	
#  define LFB_BYTE_SWIZZLE_RD	  BIT(16)	
#define CLIP_LEFT_RIGHT		0x0118
#define CLIP_LOWY_HIGHY		0x011c
#define NOPCMD			0x0120
#define FASTFILLCMD		0x0124
#define SWAPBUFFCMD		0x0128
#define FBIINIT4		0x0200		
#  define FAST_PCI_READS	  0		
#  define SLOW_PCI_READS	  BIT(0)	
#  define LFB_READ_AHEAD	  BIT(1)
#define BACKPORCH		0x0208
#define VIDEODIMENSIONS		0x020c
#define FBIINIT0		0x0210		
#  define DIS_VGA_PASSTHROUGH	  BIT(0)
#  define FBI_RESET		  BIT(1)
#  define FIFO_RESET		  BIT(2)
#define FBIINIT1		0x0214		
#  define VIDEO_MASK		  0x8080010f	
#  define FAST_PCI_WRITES	  0		
#  define SLOW_PCI_WRITES	  BIT(1)	
#  define EN_LFB_READ		  BIT(3)
#  define TILES_IN_X_SHIFT	  4
#  define VIDEO_RESET		  BIT(8)
#  define EN_BLANKING		  BIT(12)
#  define EN_DATA_OE		  BIT(13)
#  define EN_BLANK_OE		  BIT(14)
#  define EN_HVSYNC_OE		  BIT(15)
#  define EN_DCLK_OE		  BIT(16)
#  define SEL_INPUT_VCLK_2X	  0		
#  define SEL_INPUT_VCLK_SLAVE	  BIT(17)
#  define SEL_SOURCE_VCLK_SLAVE	  0		
#  define SEL_SOURCE_VCLK_2X_DIV2 (0x01 << 20)
#  define SEL_SOURCE_VCLK_2X_SEL  (0x02 << 20)
#  define EN_24BPP		  BIT(22)
#  define TILES_IN_X_MSB_SHIFT	  24		
#  define VCLK_2X_SEL_DEL_SHIFT	  27		
#  define VCLK_DEL_SHIFT	  29		
#define FBIINIT2		0x0218		
#  define EN_FAST_RAS_READ	  BIT(5)
#  define EN_DRAM_OE		  BIT(6)
#  define EN_FAST_RD_AHEAD_WR	  BIT(7)
#  define VIDEO_OFFSET_SHIFT	  11		
#  define SWAP_DACVSYNC		  0
#  define SWAP_DACDATA0		  (1 << 9)
#  define SWAP_FIFO_STALL	  (2 << 9)
#  define EN_RD_AHEAD_FIFO	  BIT(21)
#  define EN_DRAM_REFRESH	  BIT(22)
#  define DRAM_REFRESH_16	  (0x30 << 23)	
#define DAC_READ		FBIINIT2	
#define FBIINIT3		0x021c		
#  define DISABLE_TEXTURE	  BIT(6)
#  define Y_SWAP_ORIGIN_SHIFT	  22		
#define HSYNC			0x0220
#define VSYNC			0x0224
#define DAC_DATA		0x022c
#  define DAC_READ_CMD		  BIT(11)	
#define FBIINIT5		0x0244		
#  define FBIINIT5_MASK		  0xfa40ffff    
#  define HDOUBLESCAN		  BIT(20)
#  define VDOUBLESCAN		  BIT(21)
#  define HSYNC_HIGH 		  BIT(23)
#  define VSYNC_HIGH 		  BIT(24)
#  define INTERLACE		  BIT(26)
#define FBIINIT6		0x0248		
#  define TILES_IN_X_LSB_SHIFT	  30		
#define FBIINIT7		0x024c		

#define BLTSRCBASEADDR		0x02c0	
#define BLTDSTBASEADDR		0x02c4	
#define BLTXYSTRIDES		0x02c8	
#define BLTSRCCHROMARANGE	0x02cc	
#define BLTDSTCHROMARANGE	0x02d0	
#define BLTCLIPX		0x02d4	
#define BLTCLIPY		0x02d8	
#define BLTSRCXY		0x02e0	
#define BLTDSTXY		0x02e4	
#define BLTSIZE			0x02e8	
#define BLTROP			0x02ec	
#  define BLTROP_COPY		  0x0cccc
#  define BLTROP_INVERT		  0x05555
#  define BLTROP_XOR		  0x06666
#define BLTCOLOR		0x02f0	
#define BLTCOMMAND		0x02f8	
# define BLT_SCR2SCR_BITBLT	  0	  
# define BLT_CPU2SCR_BITBLT	  1	  
# define BLT_RECFILL_BITBLT	  2	  
# define BLT_16BPP_FMT		  2	  
#define BLTDATA			0x02fc	
#  define LAUNCH_BITBLT		  BIT(31) 

#define DACREG_WMA		0x0	
#define DACREG_LUT		0x01	
#define DACREG_RMR		0x02	
#define DACREG_RMA		0x03	
#define DACREG_ADDR_I		DACREG_WMA
#define DACREG_DATA_I		DACREG_RMR
#define DACREG_RMR_I		0x00
#define DACREG_CR0_I		0x01
#  define DACREG_CR0_EN_INDEXED	  BIT(0)	
#  define DACREG_CR0_8BIT	  BIT(1)	
#  define DACREG_CR0_PWDOWN	  BIT(3)	
#  define DACREG_CR0_16BPP	  0x30		
#  define DACREG_CR0_24BPP	  0x50		
#define	DACREG_CR1_I		0x05
#define DACREG_CC_I		0x06
#  define DACREG_CC_CLKA	  BIT(7)	
#  define DACREG_CC_CLKA_C	  (2<<4)	
#  define DACREG_CC_CLKB	  BIT(3)	
#  define DACREG_CC_CLKB_D	  3		
#define DACREG_AC0_I		0x48		
#define DACREG_AC1_I		0x49
#define DACREG_BD0_I		0x6c		
#define DACREG_BD1_I		0x6d

#define DACREG_MIR_TI		0x97
#define DACREG_DIR_TI		0x09
#define DACREG_MIR_ATT		0x84
#define DACREG_DIR_ATT		0x09
#define DACREG_ICS_PLLWMA	0x04	
#define DACREG_ICS_PLLDATA	0x05	
#define DACREG_ICS_CMD		0x06	
#  define DACREG_ICS_CMD_16BPP	  0x50	
#  define DACREG_ICS_CMD_24BPP	  0x70	
#  define DACREG_ICS_CMD_PWDOWN BIT(0)	
#define DACREG_ICS_PLLRMA	0x07	
#define DACREG_ICS_PLL_CLK0_1_INI 0x55	
#define DACREG_ICS_PLL_CLK0_7_INI 0x71	
#define DACREG_ICS_PLL_CLK1_B_INI 0x79	
#define DACREG_ICS_PLL_CTRL	0x0e
#  define DACREG_ICS_CLK0	  BIT(5)
#  define DACREG_ICS_CLK0_0	  0
#  define DACREG_ICS_CLK1_A	  0	

#define FBIINIT0_DEFAULT DIS_VGA_PASSTHROUGH

#define FBIINIT1_DEFAULT 	\
	(			\
	  FAST_PCI_WRITES	\
	\
	| VIDEO_RESET		\
	| 10 << TILES_IN_X_SHIFT\
	| SEL_SOURCE_VCLK_2X_SEL\
	| EN_LFB_READ		\
	)

#define FBIINIT2_DEFAULT	\
	(			\
	 SWAP_DACVSYNC		\
	| EN_DRAM_OE		\
	| DRAM_REFRESH_16	\
	| EN_DRAM_REFRESH	\
	| EN_FAST_RAS_READ	\
	| EN_RD_AHEAD_FIFO	\
	| EN_FAST_RD_AHEAD_WR	\
	)

#define FBIINIT3_DEFAULT 	\
	( DISABLE_TEXTURE )

#define FBIINIT4_DEFAULT	\
	(			\
	  FAST_PCI_READS	\
	\
	| LFB_READ_AHEAD	\
	)
#define FBIINIT6_DEFAULT	(0x0)


#define SSTFB_SET_VGAPASS	_IOW('F', 0xdd, __u32)
#define SSTFB_GET_VGAPASS	_IOR('F', 0xdd, __u32)


enum {
	VID_CLOCK=0,
	GFX_CLOCK=1,
};

#define DAC_FREF	14318	
#define VCO_MAX		260000


struct pll_timing {
	unsigned int m;
	unsigned int n;
	unsigned int p;
};

struct dac_switch {
	const char *name;
	int (*detect) (struct fb_info *info);
	int (*set_pll) (struct fb_info *info, const struct pll_timing *t, const int clock);
	void (*set_vidmod) (struct fb_info *info, const int bpp);
};

struct sst_spec {
	char * name;
	int default_gfx_clock;	
	int max_gfxclk; 	
};

struct sstfb_par {
	u32 palette[16];
	unsigned int yDim;
	unsigned int hSyncOn;	
	unsigned int hSyncOff;	
	unsigned int hBackPorch;
	unsigned int vSyncOn;
	unsigned int vSyncOff;
	unsigned int vBackPorch;
	struct pll_timing pll;
	unsigned int tiles_in_X;
	u8 __iomem *mmio_vbase;
	struct dac_switch 	dac_sw;	
	struct pci_dev		*dev;
	int	type;
	u8	revision;
	u8	vgapass;	
};

#endif 
