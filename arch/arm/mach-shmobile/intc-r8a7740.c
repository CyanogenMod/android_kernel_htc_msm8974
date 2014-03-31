/*
 * R8A7740 processor support
 *
 * Copyright (C) 2011  Renesas Solutions Corp.
 * Copyright (C) 2011  Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/sh_intc.h>
#include <mach/intc.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

enum {
	UNUSED_INTCA = 0,

	
	DIRC,
	ATAPI,
	IIC1_ALI, IIC1_TACKI, IIC1_WAITI, IIC1_DTEI,
	AP_ARM_COMMTX, AP_ARM_COMMRX,
	MFI, MFIS,
	BBIF1, BBIF2,
	USBHSDMAC,
	USBF_OUL_SOF, USBF_IXL_INT,
	SGX540,
	CMT1_0, CMT1_1, CMT1_2, CMT1_3,
	CMT2,
	CMT3,
	KEYSC,
	SCIFA0, SCIFA1, SCIFA2, SCIFA3,
	MSIOF2, MSIOF1,
	SCIFA4, SCIFA5, SCIFB,
	FLCTL_FLSTEI, FLCTL_FLTENDI, FLCTL_FLTREQ0I, FLCTL_FLTREQ1I,
	SDHI0_0, SDHI0_1, SDHI0_2, SDHI0_3,
	SDHI1_0, SDHI1_1, SDHI1_2, SDHI1_3,
	AP_ARM_L2CINT,
	IRDA,
	TPU0,
	SCIFA6, SCIFA7,
	GbEther,
	ICBS0,
	DDM,
	SDHI2_0, SDHI2_1, SDHI2_2, SDHI2_3,
	RWDT0,
	DMAC1_1_DEI0, DMAC1_1_DEI1, DMAC1_1_DEI2, DMAC1_1_DEI3,
	DMAC1_2_DEI4, DMAC1_2_DEI5, DMAC1_2_DADERR,
	DMAC2_1_DEI0, DMAC2_1_DEI1, DMAC2_1_DEI2, DMAC2_1_DEI3,
	DMAC2_2_DEI4, DMAC2_2_DEI5, DMAC2_2_DADERR,
	DMAC3_1_DEI0, DMAC3_1_DEI1, DMAC3_1_DEI2, DMAC3_1_DEI3,
	DMAC3_2_DEI4, DMAC3_2_DEI5, DMAC3_2_DADERR,
	SHWYSTAT_RT, SHWYSTAT_HS, SHWYSTAT_COM,
	USBH_INT, USBH_OHCI, USBH_EHCI, USBH_PME, USBH_BIND,
	RSPI_OVRF, RSPI_SPTEF, RSPI_SPRF,
	SPU2_0, SPU2_1,
	FSI, FMSI,
	IPMMU,
	AP_ARM_CTIIRQ, AP_ARM_PMURQ,
	MFIS2,
	CPORTR2S,
	CMT14, CMT15,
	MMCIF_0, MMCIF_1, MMCIF_2,
	SIM_ERI, SIM_RXI, SIM_TXI, SIM_TEI,
	STPRO_0, STPRO_1, STPRO_2, STPRO_3, STPRO_4,

	
	DMAC1_1, DMAC1_2,
	DMAC2_1, DMAC2_2,
	DMAC3_1, DMAC3_2,
	AP_ARM1, AP_ARM2,
	SDHI0, SDHI1, SDHI2,
	SHWYSTAT,
	USBF, USBH1, USBH2,
	RSPI, SPU2, FLCTL, IIC1,
};

static struct intc_vect intca_vectors[] __initdata = {
	INTC_VECT(DIRC,			0x0560),
	INTC_VECT(ATAPI,		0x05E0),
	INTC_VECT(IIC1_ALI,		0x0780),
	INTC_VECT(IIC1_TACKI,		0x07A0),
	INTC_VECT(IIC1_WAITI,		0x07C0),
	INTC_VECT(IIC1_DTEI,		0x07E0),
	INTC_VECT(AP_ARM_COMMTX,	0x0840),
	INTC_VECT(AP_ARM_COMMRX,	0x0860),
	INTC_VECT(MFI,			0x0900),
	INTC_VECT(MFIS,			0x0920),
	INTC_VECT(BBIF1,		0x0940),
	INTC_VECT(BBIF2,		0x0960),
	INTC_VECT(USBHSDMAC,		0x0A00),
	INTC_VECT(USBF_OUL_SOF,		0x0A20),
	INTC_VECT(USBF_IXL_INT,		0x0A40),
	INTC_VECT(SGX540,		0x0A60),
	INTC_VECT(CMT1_0,		0x0B00),
	INTC_VECT(CMT1_1,		0x0B20),
	INTC_VECT(CMT1_2,		0x0B40),
	INTC_VECT(CMT1_3,		0x0B60),
	INTC_VECT(CMT2,			0x0B80),
	INTC_VECT(CMT3,			0x0BA0),
	INTC_VECT(KEYSC,		0x0BE0),
	INTC_VECT(SCIFA0,		0x0C00),
	INTC_VECT(SCIFA1,		0x0C20),
	INTC_VECT(SCIFA2,		0x0C40),
	INTC_VECT(SCIFA3,		0x0C60),
	INTC_VECT(MSIOF2,		0x0C80),
	INTC_VECT(MSIOF1,		0x0D00),
	INTC_VECT(SCIFA4,		0x0D20),
	INTC_VECT(SCIFA5,		0x0D40),
	INTC_VECT(SCIFB,		0x0D60),
	INTC_VECT(FLCTL_FLSTEI,		0x0D80),
	INTC_VECT(FLCTL_FLTENDI,	0x0DA0),
	INTC_VECT(FLCTL_FLTREQ0I,	0x0DC0),
	INTC_VECT(FLCTL_FLTREQ1I,	0x0DE0),
	INTC_VECT(SDHI0_0,		0x0E00),
	INTC_VECT(SDHI0_1,		0x0E20),
	INTC_VECT(SDHI0_2,		0x0E40),
	INTC_VECT(SDHI0_3,		0x0E60),
	INTC_VECT(SDHI1_0,		0x0E80),
	INTC_VECT(SDHI1_1,		0x0EA0),
	INTC_VECT(SDHI1_2,		0x0EC0),
	INTC_VECT(SDHI1_3,		0x0EE0),
	INTC_VECT(AP_ARM_L2CINT,	0x0FA0),
	INTC_VECT(IRDA,			0x0480),
	INTC_VECT(TPU0,			0x04A0),
	INTC_VECT(SCIFA6,		0x04C0),
	INTC_VECT(SCIFA7,		0x04E0),
	INTC_VECT(GbEther,		0x0500),
	INTC_VECT(ICBS0,		0x0540),
	INTC_VECT(DDM,			0x1140),
	INTC_VECT(SDHI2_0,		0x1200),
	INTC_VECT(SDHI2_1,		0x1220),
	INTC_VECT(SDHI2_2,		0x1240),
	INTC_VECT(SDHI2_3,		0x1260),
	INTC_VECT(RWDT0,		0x1280),
	INTC_VECT(DMAC1_1_DEI0,		0x2000),
	INTC_VECT(DMAC1_1_DEI1,		0x2020),
	INTC_VECT(DMAC1_1_DEI2,		0x2040),
	INTC_VECT(DMAC1_1_DEI3,		0x2060),
	INTC_VECT(DMAC1_2_DEI4,		0x2080),
	INTC_VECT(DMAC1_2_DEI5,		0x20A0),
	INTC_VECT(DMAC1_2_DADERR,	0x20C0),
	INTC_VECT(DMAC2_1_DEI0,		0x2100),
	INTC_VECT(DMAC2_1_DEI1,		0x2120),
	INTC_VECT(DMAC2_1_DEI2,		0x2140),
	INTC_VECT(DMAC2_1_DEI3,		0x2160),
	INTC_VECT(DMAC2_2_DEI4,		0x2180),
	INTC_VECT(DMAC2_2_DEI5,		0x21A0),
	INTC_VECT(DMAC2_2_DADERR,	0x21C0),
	INTC_VECT(DMAC3_1_DEI0,		0x2200),
	INTC_VECT(DMAC3_1_DEI1,		0x2220),
	INTC_VECT(DMAC3_1_DEI2,		0x2240),
	INTC_VECT(DMAC3_1_DEI3,		0x2260),
	INTC_VECT(DMAC3_2_DEI4,		0x2280),
	INTC_VECT(DMAC3_2_DEI5,		0x22A0),
	INTC_VECT(DMAC3_2_DADERR,	0x22C0),
	INTC_VECT(SHWYSTAT_RT,		0x1300),
	INTC_VECT(SHWYSTAT_HS,		0x1320),
	INTC_VECT(SHWYSTAT_COM,		0x1340),
	INTC_VECT(USBH_INT,		0x1540),
	INTC_VECT(USBH_OHCI,		0x1560),
	INTC_VECT(USBH_EHCI,		0x1580),
	INTC_VECT(USBH_PME,		0x15A0),
	INTC_VECT(USBH_BIND,		0x15C0),
	INTC_VECT(RSPI_OVRF,		0x1780),
	INTC_VECT(RSPI_SPTEF,		0x17A0),
	INTC_VECT(RSPI_SPRF,		0x17C0),
	INTC_VECT(SPU2_0,		0x1800),
	INTC_VECT(SPU2_1,		0x1820),
	INTC_VECT(FSI,			0x1840),
	INTC_VECT(FMSI,			0x1860),
	INTC_VECT(IPMMU,		0x1920),
	INTC_VECT(AP_ARM_CTIIRQ,	0x1980),
	INTC_VECT(AP_ARM_PMURQ,		0x19A0),
	INTC_VECT(MFIS2,		0x1A00),
	INTC_VECT(CPORTR2S,		0x1A20),
	INTC_VECT(CMT14,		0x1A40),
	INTC_VECT(CMT15,		0x1A60),
	INTC_VECT(MMCIF_0,		0x1AA0),
	INTC_VECT(MMCIF_1,		0x1AC0),
	INTC_VECT(MMCIF_2,		0x1AE0),
	INTC_VECT(SIM_ERI,		0x1C00),
	INTC_VECT(SIM_RXI,		0x1C20),
	INTC_VECT(SIM_TXI,		0x1C40),
	INTC_VECT(SIM_TEI,		0x1C60),
	INTC_VECT(STPRO_0,		0x1C80),
	INTC_VECT(STPRO_1,		0x1CA0),
	INTC_VECT(STPRO_2,		0x1CC0),
	INTC_VECT(STPRO_3,		0x1CE0),
	INTC_VECT(STPRO_4,		0x1D00),
};

static struct intc_group intca_groups[] __initdata = {
	INTC_GROUP(DMAC1_1,
		   DMAC1_1_DEI0, DMAC1_1_DEI1, DMAC1_1_DEI2, DMAC1_1_DEI3),
	INTC_GROUP(DMAC1_2,
		   DMAC1_2_DEI4, DMAC1_2_DEI5, DMAC1_2_DADERR),
	INTC_GROUP(DMAC2_1,
		   DMAC2_1_DEI0, DMAC2_1_DEI1, DMAC2_1_DEI2, DMAC2_1_DEI3),
	INTC_GROUP(DMAC2_2,
		   DMAC2_2_DEI4, DMAC2_2_DEI5, DMAC2_2_DADERR),
	INTC_GROUP(DMAC3_1,
		   DMAC3_1_DEI0, DMAC3_1_DEI1, DMAC3_1_DEI2, DMAC3_1_DEI3),
	INTC_GROUP(DMAC3_2,
		   DMAC3_2_DEI4, DMAC3_2_DEI5, DMAC3_2_DADERR),
	INTC_GROUP(AP_ARM1,
		   AP_ARM_COMMTX, AP_ARM_COMMRX),
	INTC_GROUP(AP_ARM2,
		   AP_ARM_CTIIRQ, AP_ARM_PMURQ),
	INTC_GROUP(USBF,
		   USBF_OUL_SOF, USBF_IXL_INT),
	INTC_GROUP(SDHI0,
		   SDHI0_0, SDHI0_1, SDHI0_2, SDHI0_3),
	INTC_GROUP(SDHI1,
		   SDHI1_0, SDHI1_1, SDHI1_2, SDHI1_3),
	INTC_GROUP(SDHI2,
		   SDHI2_0, SDHI2_1, SDHI2_2, SDHI2_3),
	INTC_GROUP(SHWYSTAT,
		   SHWYSTAT_RT, SHWYSTAT_HS, SHWYSTAT_COM),
	INTC_GROUP(USBH1, 
		   USBH_INT, USBH_OHCI),
	INTC_GROUP(USBH2, 
		   USBH_EHCI,
		   USBH_PME, USBH_BIND),
	INTC_GROUP(RSPI,
		   RSPI_OVRF, RSPI_SPTEF, RSPI_SPRF),
	INTC_GROUP(SPU2,
		   SPU2_0, SPU2_1),
	INTC_GROUP(FLCTL,
		   FLCTL_FLSTEI, FLCTL_FLTENDI, FLCTL_FLTREQ0I, FLCTL_FLTREQ1I),
	INTC_GROUP(IIC1,
		   IIC1_ALI, IIC1_TACKI, IIC1_WAITI, IIC1_DTEI),
};

static struct intc_mask_reg intca_mask_registers[] __initdata = {
	{  0xe6940080, 0xe69400c0, 8,
	  { DMAC2_1_DEI3, DMAC2_1_DEI2, DMAC2_1_DEI1, DMAC2_1_DEI0,
	    0, 0, AP_ARM_COMMTX, AP_ARM_COMMRX } },
	{  0xe6940084, 0xe69400c4, 8,
	  { ATAPI, 0, DIRC, 0,
	    DMAC1_1_DEI3, DMAC1_1_DEI2, DMAC1_1_DEI1, DMAC1_1_DEI0 } },
	{  0xe6940088, 0xe69400c8, 8,
	  { 0, 0, 0, 0,
	    BBIF1, BBIF2, MFIS, MFI } },
	{  0xe694008c, 0xe69400cc, 8,
	  { DMAC3_1_DEI3, DMAC3_1_DEI2, DMAC3_1_DEI1, DMAC3_1_DEI0,
	    DMAC3_2_DADERR, DMAC3_2_DEI5, DMAC3_2_DEI4, IRDA } },
	{  0xe6940090, 0xe69400d0, 8,
	  { DDM, 0, 0, 0,
	    0, 0, 0, 0 } },
	{  0xe6940094, 0xe69400d4, 8,
	  { KEYSC, DMAC1_2_DADERR, DMAC1_2_DEI5, DMAC1_2_DEI4,
	    SCIFA3, SCIFA2, SCIFA1, SCIFA0 } },
	{  0xe6940098, 0xe69400d8, 8,
	  { SCIFB, SCIFA5, SCIFA4, MSIOF1,
	    0, 0, MSIOF2, 0 } },
	{  0xe694009c, 0xe69400dc, 8,
	  { SDHI0_3, SDHI0_2, SDHI0_1, SDHI0_0,
	    FLCTL_FLTREQ1I, FLCTL_FLTREQ0I, FLCTL_FLTENDI, FLCTL_FLSTEI } },
	{  0xe69400a0, 0xe69400e0, 8,
	  { SDHI1_3, SDHI1_2, SDHI1_1, SDHI1_0,
	    0, USBHSDMAC, 0, AP_ARM_L2CINT } },
	{  0xe69400a4, 0xe69400e4, 8,
	  { CMT1_3, CMT1_2, CMT1_1, CMT1_0,
	    CMT2, USBF_IXL_INT, USBF_OUL_SOF, SGX540 } },
	{  0xe69400a8, 0xe69400e8, 8,
	  { 0, DMAC2_2_DADERR, DMAC2_2_DEI5, DMAC2_2_DEI4,
	    0, 0, 0, 0 } },
	{  0xe69400ac, 0xe69400ec, 8,
	  { IIC1_DTEI, IIC1_WAITI, IIC1_TACKI, IIC1_ALI,
	    ICBS0, 0, 0, 0 } },
	{  0xe69400b0, 0xe69400f0, 8,
	  { 0, 0, TPU0, SCIFA6,
	    SCIFA7, GbEther, 0, 0 } },
	{  0xe69400b4, 0xe69400f4, 8,
	  { SDHI2_3, SDHI2_2, SDHI2_1, SDHI2_0,
	    0, CMT3, 0, RWDT0 } },
	{  0xe6950080, 0xe69500c0, 8,
	  { SHWYSTAT_RT, SHWYSTAT_HS, SHWYSTAT_COM, 0,
	    0, 0, 0, 0 } },
	  
	{  0xe6950088, 0xe69500c8, 8,
	  { 0, 0, USBH_INT, USBH_OHCI,
	    USBH_EHCI, USBH_PME, USBH_BIND, 0 } },
	  
	{  0xe6950090, 0xe69500d0, 8,
	  { 0, 0, 0, 0,
	    RSPI_OVRF, RSPI_SPTEF, RSPI_SPRF, 0 } },
	{  0xe6950094, 0xe69500d4, 8,
	  { SPU2_0, SPU2_1, FSI, FMSI,
	    0, 0, 0, 0 } },
	{  0xe6950098, 0xe69500d8, 8,
	  { 0, IPMMU, 0, 0,
	    AP_ARM_CTIIRQ, AP_ARM_PMURQ, 0, 0 } },
	{  0xe695009c, 0xe69500dc, 8,
	  { MFIS2, CPORTR2S, CMT14, CMT15,
	    0, MMCIF_0, MMCIF_1, MMCIF_2 } },
	  
	{  0xe69500a4, 0xe69500e4, 8,
	  { SIM_ERI, SIM_RXI, SIM_TXI, SIM_TEI,
	    STPRO_0, STPRO_1, STPRO_2, STPRO_3 } },
	{  0xe69500a8, 0xe69500e8, 8,
	  { STPRO_4, 0, 0, 0,
	    0, 0, 0, 0 } },
};

static struct intc_prio_reg intca_prio_registers[] __initdata = {
	{ 0xe6940000, 0, 16, 4,  { DMAC3_1, DMAC3_2, CMT2, ICBS0 } },
	{ 0xe6940004, 0, 16, 4,  { IRDA, 0, BBIF1, BBIF2 } },
	{ 0xe6940008, 0, 16, 4,  { ATAPI, 0, CMT1_1, AP_ARM1 } },
	{ 0xe694000c, 0, 16, 4,  { 0, 0, CMT1_2, 0 } },
	{ 0xe6940010, 0, 16, 4,  { DMAC1_1, MFIS, MFI, USBF } },
	{ 0xe6940014, 0, 16, 4,  { KEYSC, DMAC1_2,
					      SGX540, CMT1_0 } },
	{ 0xe6940018, 0, 16, 4,  { SCIFA0, SCIFA1,
					      SCIFA2, SCIFA3 } },
	{ 0xe694001c, 0, 16, 4,  { MSIOF2, USBHSDMAC,
					      FLCTL, SDHI0 } },
	{ 0xe6940020, 0, 16, 4,  { MSIOF1, SCIFA4, 0, IIC1 } },
	{ 0xe6940024, 0, 16, 4,  { DMAC2_1, DMAC2_2,
					      AP_ARM_L2CINT, 0 } },
	{ 0xe6940028, 0, 16, 4,  { 0, CMT1_3, 0, SDHI1 } },
	{ 0xe694002c, 0, 16, 4,  { TPU0, SCIFA6,
					      SCIFA7, GbEther } },
	{ 0xe6940030, 0, 16, 4,  { 0, CMT3, 0, RWDT0 } },
	{ 0xe6940034, 0, 16, 4,  { SCIFB, SCIFA5, 0, DDM } },
	{ 0xe6940038, 0, 16, 4,  { 0, 0, DIRC, SDHI2 } },
	{ 0xe6950000, 0, 16, 4,  { SHWYSTAT, 0, 0, 0 } },
				
				
				
	{ 0xe6950010, 0, 16, 4,  { USBH1, 0, 0, 0 } },
	{ 0xe6950014, 0, 16, 4,  { USBH2, 0, 0, 0 } },
				
				
				
	{ 0xe6950024, 0, 16, 4,  { RSPI, 0, 0, 0 } },
	{ 0xe6950028, 0, 16, 4,  { SPU2, 0, FSI, FMSI } },
				
	{ 0xe6950030, 0, 16, 4,  { IPMMU, 0, 0, 0 } },
	{ 0xe6950034, 0, 16, 4,  { AP_ARM2, 0, 0, 0 } },
	{ 0xe6950038, 0, 16, 4,  { MFIS2, CPORTR2S,
					       CMT14, CMT15 } },
	{ 0xe695003c, 0, 16, 4,  { 0, MMCIF_0, MMCIF_1, MMCIF_2 } },
				
				
	{ 0xe6950048, 0, 16, 4,  { SIM_ERI, SIM_RXI,
					       SIM_TXI, SIM_TEI } },
	{ 0xe695004c, 0, 16, 4,  { STPRO_0, STPRO_1,
					       STPRO_2, STPRO_3 } },
	{ 0xe6950050, 0, 16, 4,  { STPRO_4, 0, 0, 0 } },
};

static DECLARE_INTC_DESC(intca_desc, "r8a7740-intca",
			 intca_vectors, intca_groups,
			 intca_mask_registers, intca_prio_registers,
			 NULL);

INTC_IRQ_PINS_32(intca_irq_pins, 0xe6900000,
		 INTC_VECT, "r8a7740-intca-irq-pins");


enum {
	UNUSED_INTCS = 0,

	INTCS,

	

	
	
	
	VPU5HA2,
	_2DG_TRAP, _2DG_GPM_INT, _2DG_CER_INT,
	
	
	VPU5F,
	_2DG_BRK_INT,
	
	
	
	
	
	
	IIC0_ALI, IIC0_TACKI, IIC0_WAITI, IIC0_DTEI,
	TMU0_0, TMU0_1, TMU0_2,
	CMT0,
	
	LMB,
	CTI,
	VOU,
	
	ICB,
	VIO6C,
	CEU20, CEU21,
	JPU,
	LCDC0,
	LCRC,
	
	
	LCDC1,
	
	
	
	TMU1_0, TMU1_1, TMU1_2,
	CMT4,
	DISP,
	DSRV,
	
	CPORTS2R,

	
	_2DG1,
	IIC0, TMU1,
};

static struct intc_vect intcs_vectors[] = {
	
	
	
	INTCS_VECT(VPU5HA2,		0x0880),
	INTCS_VECT(_2DG_TRAP,		0x08A0),
	INTCS_VECT(_2DG_GPM_INT,	0x08C0),
	INTCS_VECT(_2DG_CER_INT,	0x08E0),
	
	
	INTCS_VECT(VPU5F,		0x0980),
	INTCS_VECT(_2DG_BRK_INT,	0x09A0),
	
	
	
	
	
	
	INTCS_VECT(IIC0_ALI,		0x0E00),
	INTCS_VECT(IIC0_TACKI,		0x0E20),
	INTCS_VECT(IIC0_WAITI,		0x0E40),
	INTCS_VECT(IIC0_DTEI,		0x0E60),
	INTCS_VECT(TMU0_0,		0x0E80),
	INTCS_VECT(TMU0_1,		0x0EA0),
	INTCS_VECT(TMU0_2,		0x0EC0),
	INTCS_VECT(CMT0,		0x0F00),
	
	INTCS_VECT(LMB,			0x0F60),
	INTCS_VECT(CTI,			0x0400),
	INTCS_VECT(VOU,			0x0420),
	
	INTCS_VECT(ICB,			0x0480),
	INTCS_VECT(VIO6C,		0x04E0),
	INTCS_VECT(CEU20,		0x0500),
	INTCS_VECT(CEU21,		0x0520),
	INTCS_VECT(JPU,			0x0560),
	INTCS_VECT(LCDC0,		0x0580),
	INTCS_VECT(LCRC,		0x05A0),
	
	
	INTCS_VECT(LCDC1,		0x1780),
	
	
	
	INTCS_VECT(TMU1_0,		0x1900),
	INTCS_VECT(TMU1_1,		0x1920),
	INTCS_VECT(TMU1_2,		0x1940),
	INTCS_VECT(CMT4,		0x1980),
	INTCS_VECT(DISP,		0x19A0),
	INTCS_VECT(DSRV,		0x19C0),
	
	INTCS_VECT(CPORTS2R,		0x1A20),

	INTC_VECT(INTCS,		0xf80),
};

static struct intc_group intcs_groups[] __initdata = {
	INTC_GROUP(_2DG1, 
		   _2DG_CER_INT, _2DG_GPM_INT, _2DG_TRAP),
	INTC_GROUP(IIC0,
		   IIC0_DTEI, IIC0_WAITI, IIC0_TACKI, IIC0_ALI),
	INTC_GROUP(TMU1,
		   TMU1_0, TMU1_1, TMU1_2),
};

static struct intc_mask_reg intcs_mask_registers[] = {
	   
	{  0xffd20184, 0xffd201c4, 8,
	  { _2DG_CER_INT, _2DG_GPM_INT, _2DG_TRAP, VPU5HA2,
	    0, 0, 0, 0  } },
	{  0xffd20188, 0xffd201c8, 8,
	  { 0, 0, CEU21, VPU5F,
	    0, 0, 0, 0 } },
	{  0xffd2018c, 0xffd201cc, 8,
	  { 0, 0, 0, 0, 
	    VIO6C, 0, 0, ICB } },
	{  0xffd20190, 0xffd201d0, 8,
	  { 0, 0, VOU, CTI,
	    JPU, 0, LCRC, LCDC0 } },
	   
	   
	{  0xffd2019c, 0xffd201dc, 8,
	  { 0, TMU0_2, TMU0_1, TMU0_0,
	    0, 0, 0, 0 } },
	{  0xffd201a0, 0xffd201e0, 8,
	  { 0, 0, 0, 0,
	    CEU20, 0, 0, 0 } },
	{  0xffd201a4, 0xffd201e4, 8,
	  { 0, 0, 0, CMT0,
	    0, 0, 0, 0 } },
	   
	{  0xffd201ac, 0xffd201ec, 8,
	  { IIC0_DTEI, IIC0_WAITI, IIC0_TACKI, IIC0_ALI,
	    0, _2DG_BRK_INT, LMB, 0 } },
	  
	  
	   
	  
	  
	  
	{  0xffd50190, 0xffd501d0, 8,
	  { 0, 0, 0, 0,
	    LCDC1, 0, 0, 0 } },
	   
	{  0xffd50198, 0xffd501d8, 8,
	  { TMU1_0, TMU1_1, TMU1_2, 0,
	    CMT4, DISP, DSRV, 0 } },
	{  0xffd5019c, 0xffd501dc, 8,
	  { 0, CPORTS2R, 0, 0,
	    0, 0, 0, 0 } },
	{  0xffd20104, 0, 16,
	  { 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, INTCS } },
};

static struct intc_prio_reg intcs_prio_registers[] = {
	{ 0xffd20000, 0, 16, 4,  { CTI, VOU, 0, ICB } },
	{ 0xffd20004, 0, 16, 4,  { JPU, LCDC0, 0, LCRC } },
				 
				
	{ 0xffd20010, 0, 16, 4,  { 0, VPU5HA2,
					      0, VPU5F } },
	{ 0xffd20014, 0, 16, 4,  { 0, 0,
					      0, CMT0 } },
	{ 0xffd20018, 0, 16, 4,  { TMU0_0, TMU0_1,
					      TMU0_2, _2DG1 } },
	{ 0xffd2001c, 0, 16, 4,  { 0, 0, 0,
					      _2DG_BRK_INT } },
	{ 0xffd20020, 0, 16, 4,  { 0, 0, 0, IIC0 } },
	{ 0xffd20024, 0, 16, 4,  { CEU20, 0, 0, 0 } },
	{ 0xffd20028, 0, 16, 4,  { VIO6C, 0, LMB, 0 } },
	{ 0xffd2002c, 0, 16, 4,  { 0, 0, CEU21, 0 } },
				 
				 
				 
				
				
				
				
				
				
				
	{ 0xffd50024, 0, 16, 4,  { LCDC1, 0, 0, 0 } },
				 
				
	{ 0xffd50030, 0, 16, 4,  { TMU1, 0, 0, 0 } },
	{ 0xffd50034, 0, 16, 4,  { CMT4, DISP, DSRV, 0 } },
	{ 0xffd50038, 0, 16, 4,  { 0, CPORTS2R, 0, 0 } },
				
};

static struct resource intcs_resources[] __initdata = {
	[0] = {
		.start	= 0xffd20000,
		.end	= 0xffd201ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 0xffd50000,
		.end	= 0xffd501ff,
		.flags	= IORESOURCE_MEM,
	}
};

static struct intc_desc intcs_desc __initdata = {
	.name = "r8a7740-intcs",
	.resource = intcs_resources,
	.num_resources = ARRAY_SIZE(intcs_resources),
	.hw = INTC_HW_DESC(intcs_vectors, intcs_groups, intcs_mask_registers,
			   intcs_prio_registers, NULL, NULL),
};

static void intcs_demux(unsigned int irq, struct irq_desc *desc)
{
	void __iomem *reg = (void *)irq_get_handler_data(irq);
	unsigned int evtcodeas = ioread32(reg);

	generic_handle_irq(intcs_evt2irq(evtcodeas));
}

void __init r8a7740_init_irq(void)
{
	void __iomem *intevtsa = ioremap_nocache(0xffd20100, PAGE_SIZE);

	register_intc_controller(&intca_desc);
	register_intc_controller(&intca_irq_pins_desc);
	register_intc_controller(&intcs_desc);

	
	irq_set_handler_data(evt2irq(0xf80), (void *)intevtsa);
	irq_set_chained_handler(evt2irq(0xf80), intcs_demux);
}
