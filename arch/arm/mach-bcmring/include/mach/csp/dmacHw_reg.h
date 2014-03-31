/*****************************************************************************
* Copyright 2004 - 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/


#ifndef _DMACHW_REG_H
#define _DMACHW_REG_H

#include <csp/stdint.h>
#include <mach/csp/mm_io.h>

typedef struct {
	volatile uint32_t lo;	
	volatile uint32_t hi;	
} dmacHw_REG64_t;

typedef struct {
	dmacHw_REG64_t ChannelSar;	
	dmacHw_REG64_t ChannelDar;	
	dmacHw_REG64_t ChannelLlp;	
	dmacHw_REG64_t ChannelCtl;	
	dmacHw_REG64_t ChannelSstat;	
	dmacHw_REG64_t ChannelDstat;	
	dmacHw_REG64_t ChannelSstatAddr;	
	dmacHw_REG64_t ChannelDstatAddr;	
	dmacHw_REG64_t ChannelConfig;	
	dmacHw_REG64_t SrcGather;	
	dmacHw_REG64_t DstScatter;	
} dmacHw_CH_REG_t;

typedef struct {
	dmacHw_REG64_t RawTfr;	
	dmacHw_REG64_t RawBlock;	
	dmacHw_REG64_t RawSrcTran;	
	dmacHw_REG64_t RawDstTran;	
	dmacHw_REG64_t RawErr;	
} dmacHw_INT_RAW_t;

typedef struct {
	dmacHw_REG64_t StatusTfr;	
	dmacHw_REG64_t StatusBlock;	
	dmacHw_REG64_t StatusSrcTran;	
	dmacHw_REG64_t StatusDstTran;	
	dmacHw_REG64_t StatusErr;	
} dmacHw_INT_STATUS_t;

typedef struct {
	dmacHw_REG64_t MaskTfr;	
	dmacHw_REG64_t MaskBlock;	
	dmacHw_REG64_t MaskSrcTran;	
	dmacHw_REG64_t MaskDstTran;	
	dmacHw_REG64_t MaskErr;	
} dmacHw_INT_MASK_t;

typedef struct {
	dmacHw_REG64_t ClearTfr;	
	dmacHw_REG64_t ClearBlock;	
	dmacHw_REG64_t ClearSrcTran;	
	dmacHw_REG64_t ClearDstTran;	
	dmacHw_REG64_t ClearErr;	
	dmacHw_REG64_t StatusInt;	
} dmacHw_INT_CLEAR_t;

typedef struct {
	dmacHw_REG64_t ReqSrcReg;	
	dmacHw_REG64_t ReqDstReg;	
	dmacHw_REG64_t SglReqSrcReg;	
	dmacHw_REG64_t SglReqDstReg;	
	dmacHw_REG64_t LstSrcReg;	
	dmacHw_REG64_t LstDstReg;	
} dmacHw_SW_HANDSHAKE_t;

typedef struct {
	dmacHw_REG64_t DmaCfgReg;	
	dmacHw_REG64_t ChEnReg;	
	dmacHw_REG64_t DmaIdReg;	
	dmacHw_REG64_t DmaTestReg;	
	dmacHw_REG64_t Reserved0;	
	dmacHw_REG64_t Reserved1;	
	dmacHw_REG64_t CompParm6;	
	dmacHw_REG64_t CompParm5;	
	dmacHw_REG64_t CompParm4;	
	dmacHw_REG64_t CompParm3;	
	dmacHw_REG64_t CompParm2;	
	dmacHw_REG64_t CompParm1;	
	dmacHw_REG64_t CompId;	
} dmacHw_MISC_t;

#define dmacHw_0_MODULE_BASE_ADDR        (char *) MM_IO_BASE_DMA0	
#define dmacHw_1_MODULE_BASE_ADDR        (char *) MM_IO_BASE_DMA1	

extern uint32_t dmaChannelCount_0;
extern uint32_t dmaChannelCount_1;

#define dmacHw_CHAN_BASE(module, chan)          ((dmacHw_CH_REG_t *) ((char *)((module) ? dmacHw_1_MODULE_BASE_ADDR : dmacHw_0_MODULE_BASE_ADDR) + ((chan) * sizeof(dmacHw_CH_REG_t))))

#define dmacHw_REG_INT_RAW_BASE(module)         ((char *)dmacHw_CHAN_BASE((module), ((module) ? dmaChannelCount_1 : dmaChannelCount_0)))
#define dmacHw_REG_INT_RAW_TRAN(module)         (((dmacHw_INT_RAW_t *) dmacHw_REG_INT_RAW_BASE((module)))->RawTfr.lo)
#define dmacHw_REG_INT_RAW_BLOCK(module)        (((dmacHw_INT_RAW_t *) dmacHw_REG_INT_RAW_BASE((module)))->RawBlock.lo)
#define dmacHw_REG_INT_RAW_STRAN(module)        (((dmacHw_INT_RAW_t *) dmacHw_REG_INT_RAW_BASE((module)))->RawSrcTran.lo)
#define dmacHw_REG_INT_RAW_DTRAN(module)        (((dmacHw_INT_RAW_t *) dmacHw_REG_INT_RAW_BASE((module)))->RawDstTran.lo)
#define dmacHw_REG_INT_RAW_ERROR(module)        (((dmacHw_INT_RAW_t *) dmacHw_REG_INT_RAW_BASE((module)))->RawErr.lo)

#define dmacHw_REG_INT_STAT_BASE(module)        ((char *)(dmacHw_REG_INT_RAW_BASE((module)) + sizeof(dmacHw_INT_RAW_t)))
#define dmacHw_REG_INT_STAT_TRAN(module)        (((dmacHw_INT_STATUS_t *) dmacHw_REG_INT_STAT_BASE((module)))->StatusTfr.lo)
#define dmacHw_REG_INT_STAT_BLOCK(module)       (((dmacHw_INT_STATUS_t *) dmacHw_REG_INT_STAT_BASE((module)))->StatusBlock.lo)
#define dmacHw_REG_INT_STAT_STRAN(module)       (((dmacHw_INT_STATUS_t *) dmacHw_REG_INT_STAT_BASE((module)))->StatusSrcTran.lo)
#define dmacHw_REG_INT_STAT_DTRAN(module)       (((dmacHw_INT_STATUS_t *) dmacHw_REG_INT_STAT_BASE((module)))->StatusDstTran.lo)
#define dmacHw_REG_INT_STAT_ERROR(module)       (((dmacHw_INT_STATUS_t *) dmacHw_REG_INT_STAT_BASE((module)))->StatusErr.lo)

#define dmacHw_REG_INT_MASK_BASE(module)        ((char *)(dmacHw_REG_INT_STAT_BASE((module)) + sizeof(dmacHw_INT_STATUS_t)))
#define dmacHw_REG_INT_MASK_TRAN(module)        (((dmacHw_INT_MASK_t *) dmacHw_REG_INT_MASK_BASE((module)))->MaskTfr.lo)
#define dmacHw_REG_INT_MASK_BLOCK(module)       (((dmacHw_INT_MASK_t *) dmacHw_REG_INT_MASK_BASE((module)))->MaskBlock.lo)
#define dmacHw_REG_INT_MASK_STRAN(module)       (((dmacHw_INT_MASK_t *) dmacHw_REG_INT_MASK_BASE((module)))->MaskSrcTran.lo)
#define dmacHw_REG_INT_MASK_DTRAN(module)       (((dmacHw_INT_MASK_t *) dmacHw_REG_INT_MASK_BASE((module)))->MaskDstTran.lo)
#define dmacHw_REG_INT_MASK_ERROR(module)       (((dmacHw_INT_MASK_t *) dmacHw_REG_INT_MASK_BASE((module)))->MaskErr.lo)

#define dmacHw_REG_INT_CLEAR_BASE(module)       ((char *)(dmacHw_REG_INT_MASK_BASE((module)) + sizeof(dmacHw_INT_MASK_t)))
#define dmacHw_REG_INT_CLEAR_TRAN(module)       (((dmacHw_INT_CLEAR_t *) dmacHw_REG_INT_CLEAR_BASE((module)))->ClearTfr.lo)
#define dmacHw_REG_INT_CLEAR_BLOCK(module)      (((dmacHw_INT_CLEAR_t *) dmacHw_REG_INT_CLEAR_BASE((module)))->ClearBlock.lo)
#define dmacHw_REG_INT_CLEAR_STRAN(module)      (((dmacHw_INT_CLEAR_t *) dmacHw_REG_INT_CLEAR_BASE((module)))->ClearSrcTran.lo)
#define dmacHw_REG_INT_CLEAR_DTRAN(module)      (((dmacHw_INT_CLEAR_t *) dmacHw_REG_INT_CLEAR_BASE((module)))->ClearDstTran.lo)
#define dmacHw_REG_INT_CLEAR_ERROR(module)      (((dmacHw_INT_CLEAR_t *) dmacHw_REG_INT_CLEAR_BASE((module)))->ClearErr.lo)
#define dmacHw_REG_INT_STATUS(module)           (((dmacHw_INT_CLEAR_t *) dmacHw_REG_INT_CLEAR_BASE((module)))->StatusInt.lo)

#define dmacHw_REG_SW_HS_BASE(module)           ((char *)(dmacHw_REG_INT_CLEAR_BASE((module)) + sizeof(dmacHw_INT_CLEAR_t)))
#define dmacHw_REG_SW_HS_SRC_REQ(module)        (((dmacHw_SW_HANDSHAKE_t *) dmacHw_REG_SW_HS_BASE((module)))->ReqSrcReg.lo)
#define dmacHw_REG_SW_HS_DST_REQ(module)        (((dmacHw_SW_HANDSHAKE_t *) dmacHw_REG_SW_HS_BASE((module)))->ReqDstReg.lo)
#define dmacHw_REG_SW_HS_SRC_SGL_REQ(module)    (((dmacHw_SW_HANDSHAKE_t *) dmacHw_REG_SW_HS_BASE((module)))->SglReqSrcReg.lo)
#define dmacHw_REG_SW_HS_DST_SGL_REQ(module)    (((dmacHw_SW_HANDSHAKE_t *) dmacHw_REG_SW_HS_BASE((module)))->SglReqDstReg.lo)
#define dmacHw_REG_SW_HS_SRC_LST_REQ(module)    (((dmacHw_SW_HANDSHAKE_t *) dmacHw_REG_SW_HS_BASE((module)))->LstSrcReg.lo)
#define dmacHw_REG_SW_HS_DST_LST_REQ(module)    (((dmacHw_SW_HANDSHAKE_t *) dmacHw_REG_SW_HS_BASE((module)))->LstDstReg.lo)

#define dmacHw_REG_MISC_BASE(module)            ((char *)(dmacHw_REG_SW_HS_BASE((module)) + sizeof(dmacHw_SW_HANDSHAKE_t)))
#define dmacHw_REG_MISC_CFG(module)             (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->DmaCfgReg.lo)
#define dmacHw_REG_MISC_CH_ENABLE(module)       (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->ChEnReg.lo)
#define dmacHw_REG_MISC_ID(module)              (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->DmaIdReg.lo)
#define dmacHw_REG_MISC_TEST(module)            (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->DmaTestReg.lo)
#define dmacHw_REG_MISC_COMP_PARAM1_LO(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm1.lo)
#define dmacHw_REG_MISC_COMP_PARAM1_HI(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm1.hi)
#define dmacHw_REG_MISC_COMP_PARAM2_LO(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm2.lo)
#define dmacHw_REG_MISC_COMP_PARAM2_HI(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm2.hi)
#define dmacHw_REG_MISC_COMP_PARAM3_LO(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm3.lo)
#define dmacHw_REG_MISC_COMP_PARAM3_HI(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm3.hi)
#define dmacHw_REG_MISC_COMP_PARAM4_LO(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm4.lo)
#define dmacHw_REG_MISC_COMP_PARAM4_HI(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm4.hi)
#define dmacHw_REG_MISC_COMP_PARAM5_LO(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm5.lo)
#define dmacHw_REG_MISC_COMP_PARAM5_HI(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm5.hi)
#define dmacHw_REG_MISC_COMP_PARAM6_LO(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm6.lo)
#define dmacHw_REG_MISC_COMP_PARAM6_HI(module)  (((dmacHw_MISC_t *) dmacHw_REG_MISC_BASE((module)))->CompParm6.hi)

#define dmacHw_REG_SAR(module, chan)            (dmacHw_CHAN_BASE((module), (chan))->ChannelSar.lo)
#define dmacHw_REG_DAR(module, chan)            (dmacHw_CHAN_BASE((module), (chan))->ChannelDar.lo)
#define dmacHw_REG_LLP(module, chan)            (dmacHw_CHAN_BASE((module), (chan))->ChannelLlp.lo)

#define dmacHw_REG_CTL_LO(module, chan)         (dmacHw_CHAN_BASE((module), (chan))->ChannelCtl.lo)
#define dmacHw_REG_CTL_HI(module, chan)         (dmacHw_CHAN_BASE((module), (chan))->ChannelCtl.hi)

#define dmacHw_REG_SSTAT(module, chan)          (dmacHw_CHAN_BASE((module), (chan))->ChannelSstat.lo)
#define dmacHw_REG_DSTAT(module, chan)          (dmacHw_CHAN_BASE((module), (chan))->ChannelDstat.lo)
#define dmacHw_REG_SSTATAR(module, chan)        (dmacHw_CHAN_BASE((module), (chan))->ChannelSstatAddr.lo)
#define dmacHw_REG_DSTATAR(module, chan)        (dmacHw_CHAN_BASE((module), (chan))->ChannelDstatAddr.lo)

#define dmacHw_REG_CFG_LO(module, chan)         (dmacHw_CHAN_BASE((module), (chan))->ChannelConfig.lo)
#define dmacHw_REG_CFG_HI(module, chan)         (dmacHw_CHAN_BASE((module), (chan))->ChannelConfig.hi)

#define dmacHw_REG_SGR_LO(module, chan)         (dmacHw_CHAN_BASE((module), (chan))->SrcGather.lo)
#define dmacHw_REG_SGR_HI(module, chan)         (dmacHw_CHAN_BASE((module), (chan))->SrcGather.hi)

#define dmacHw_REG_DSR_LO(module, chan)         (dmacHw_CHAN_BASE((module), (chan))->DstScatter.lo)
#define dmacHw_REG_DSR_HI(module, chan)         (dmacHw_CHAN_BASE((module), (chan))->DstScatter.hi)

#define INT_STATUS_MASK(channel)                (0x00000001 << (channel))
#define CHANNEL_BUSY(mod, channel)              (dmacHw_REG_MISC_CH_ENABLE((mod)) & (0x00000001 << (channel)))


#define dmacHw_REG_CTL_INT_EN                       0x00000001	

#define dmacHw_REG_CTL_DST_TR_WIDTH_MASK            0x0000000E	
#define dmacHw_REG_CTL_DST_TR_WIDTH_SHIFT           1
#define dmacHw_REG_CTL_DST_TR_WIDTH_8               0x00000000	
#define dmacHw_REG_CTL_DST_TR_WIDTH_16              0x00000002	
#define dmacHw_REG_CTL_DST_TR_WIDTH_32              0x00000004	
#define dmacHw_REG_CTL_DST_TR_WIDTH_64              0x00000006	

#define dmacHw_REG_CTL_SRC_TR_WIDTH_MASK            0x00000070	
#define dmacHw_REG_CTL_SRC_TR_WIDTH_SHIFT           4
#define dmacHw_REG_CTL_SRC_TR_WIDTH_8               0x00000000	
#define dmacHw_REG_CTL_SRC_TR_WIDTH_16              0x00000010	
#define dmacHw_REG_CTL_SRC_TR_WIDTH_32              0x00000020	
#define dmacHw_REG_CTL_SRC_TR_WIDTH_64              0x00000030	

#define dmacHw_REG_CTL_DS_ENABLE                    0x00040000	
#define dmacHw_REG_CTL_SG_ENABLE                    0x00020000	

#define dmacHw_REG_CTL_DINC_MASK                    0x00000180	
#define dmacHw_REG_CTL_DINC_INC                     0x00000000	
#define dmacHw_REG_CTL_DINC_DEC                     0x00000080	
#define dmacHw_REG_CTL_DINC_NC                      0x00000100	

#define dmacHw_REG_CTL_SINC_MASK                    0x00000600	
#define dmacHw_REG_CTL_SINC_INC                     0x00000000	
#define dmacHw_REG_CTL_SINC_DEC                     0x00000200	
#define dmacHw_REG_CTL_SINC_NC                      0x00000400	

#define dmacHw_REG_CTL_DST_MSIZE_MASK               0x00003800	
#define dmacHw_REG_CTL_DST_MSIZE_0                  0x00000000	
#define dmacHw_REG_CTL_DST_MSIZE_4                  0x00000800	
#define dmacHw_REG_CTL_DST_MSIZE_8                  0x00001000	
#define dmacHw_REG_CTL_DST_MSIZE_16                 0x00001800	

#define dmacHw_REG_CTL_SRC_MSIZE_MASK               0x0001C000	
#define dmacHw_REG_CTL_SRC_MSIZE_0                  0x00000000	
#define dmacHw_REG_CTL_SRC_MSIZE_4                  0x00004000	
#define dmacHw_REG_CTL_SRC_MSIZE_8                  0x00008000	
#define dmacHw_REG_CTL_SRC_MSIZE_16                 0x0000C000	

#define dmacHw_REG_CTL_TTFC_MASK                    0x00700000	
#define dmacHw_REG_CTL_TTFC_MM_DMAC                 0x00000000	
#define dmacHw_REG_CTL_TTFC_MP_DMAC                 0x00100000	
#define dmacHw_REG_CTL_TTFC_PM_DMAC                 0x00200000	
#define dmacHw_REG_CTL_TTFC_PP_DMAC                 0x00300000	
#define dmacHw_REG_CTL_TTFC_PM_PERI                 0x00400000	
#define dmacHw_REG_CTL_TTFC_PP_SPERI                0x00500000	
#define dmacHw_REG_CTL_TTFC_MP_PERI                 0x00600000	
#define dmacHw_REG_CTL_TTFC_PP_DPERI                0x00700000	

#define dmacHw_REG_CTL_DMS_MASK                     0x01800000	
#define dmacHw_REG_CTL_DMS_1                        0x00000000	
#define dmacHw_REG_CTL_DMS_2                        0x00800000	

#define dmacHw_REG_CTL_SMS_MASK                     0x06000000	
#define dmacHw_REG_CTL_SMS_1                        0x00000000	
#define dmacHw_REG_CTL_SMS_2                        0x02000000	

#define dmacHw_REG_CTL_LLP_DST_EN                   0x08000000	
#define dmacHw_REG_CTL_LLP_SRC_EN                   0x10000000	

#define dmacHw_REG_CTL_BLOCK_TS_MASK                0x00000FFF	
#define dmacHw_REG_CTL_DONE                         0x00001000	

#define dmacHw_REG_CFG_LO_CH_PRIORITY_SHIFT                  5	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_MASK          0x000000E0	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_0             0x00000000	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_1             0x00000020	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_2             0x00000040	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_3             0x00000060	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_4             0x00000080	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_5             0x000000A0	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_6             0x000000C0	
#define dmacHw_REG_CFG_LO_CH_PRIORITY_7             0x000000E0	

#define dmacHw_REG_CFG_LO_CH_SUSPEND                0x00000100	
#define dmacHw_REG_CFG_LO_CH_FIFO_EMPTY             0x00000200	
#define dmacHw_REG_CFG_LO_DST_CH_SW_HS              0x00000400	
#define dmacHw_REG_CFG_LO_SRC_CH_SW_HS              0x00000800	

#define dmacHw_REG_CFG_LO_CH_LOCK_MASK              0x00003000	
#define dmacHw_REG_CFG_LO_CH_LOCK_DMA               0x00000000	
#define dmacHw_REG_CFG_LO_CH_LOCK_BLOCK             0x00001000	
#define dmacHw_REG_CFG_LO_CH_LOCK_TRANS             0x00002000	
#define dmacHw_REG_CFG_LO_CH_LOCK_ENABLE            0x00010000	

#define dmacHw_REG_CFG_LO_BUS_LOCK_MASK             0x0000C000	
#define dmacHw_REG_CFG_LO_BUS_LOCK_DMA              0x00000000	
#define dmacHw_REG_CFG_LO_BUS_LOCK_BLOCK            0x00004000	
#define dmacHw_REG_CFG_LO_BUS_LOCK_TRANS            0x00008000	
#define dmacHw_REG_CFG_LO_BUS_LOCK_ENABLE           0x00020000	

#define dmacHw_REG_CFG_LO_DST_HS_POLARITY_LOW       0x00040000	
#define dmacHw_REG_CFG_LO_SRC_HS_POLARITY_LOW       0x00080000	

#define dmacHw_REG_CFG_LO_MAX_AMBA_BURST_LEN_MASK   0x3FF00000	

#define dmacHw_REG_CFG_LO_AUTO_RELOAD_SRC           0x40000000	
#define dmacHw_REG_CFG_LO_AUTO_RELOAD_DST           0x80000000	

#define dmacHw_REG_CFG_HI_FC_DST_READY              0x00000001	
#define dmacHw_REG_CFG_HI_FIFO_ENOUGH               0x00000002	

#define dmacHw_REG_CFG_HI_AHB_HPROT_MASK            0x0000001C	
#define dmacHw_REG_CFG_HI_AHB_HPROT_1               0x00000004	
#define dmacHw_REG_CFG_HI_AHB_HPROT_2               0x00000008	
#define dmacHw_REG_CFG_HI_AHB_HPROT_3               0x00000010	

#define dmacHw_REG_CFG_HI_UPDATE_DST_STAT           0x00000020	
#define dmacHw_REG_CFG_HI_UPDATE_SRC_STAT           0x00000040	

#define dmacHw_REG_CFG_HI_SRC_PERI_INTF_MASK        0x00000780	
#define dmacHw_REG_CFG_HI_DST_PERI_INTF_MASK        0x00007800	

#define dmacHw_REG_COMP_PARAM_NUM_CHANNELS          0x00000700	
#define dmacHw_REG_COMP_PARAM_NUM_INTERFACE         0x00001800	
#define dmacHw_REG_COMP_PARAM_MAX_BLK_SIZE          0x0000000f	
#define dmacHw_REG_COMP_PARAM_DATA_WIDTH            0x00006000	

#define dmacHw_SET_SAR(module, channel, addr)          (dmacHw_REG_SAR((module), (channel)) = (uint32_t) (addr))
#define dmacHw_SET_DAR(module, channel, addr)          (dmacHw_REG_DAR((module), (channel)) = (uint32_t) (addr))
#define dmacHw_SET_LLP(module, channel, ptr)           (dmacHw_REG_LLP((module), (channel)) = (uint32_t) (ptr))

#define dmacHw_GET_SSTAT(module, channel)              (dmacHw_REG_SSTAT((module), (channel)))
#define dmacHw_GET_DSTAT(module, channel)              (dmacHw_REG_DSTAT((module), (channel)))

#define dmacHw_SET_SSTATAR(module, channel, addr)      (dmacHw_REG_SSTATAR((module), (channel)) = (uint32_t) (addr))
#define dmacHw_SET_DSTATAR(module, channel, addr)      (dmacHw_REG_DSTATAR((module), (channel)) = (uint32_t) (addr))

#define dmacHw_SET_CONTROL_LO(module, channel, ctl)    (dmacHw_REG_CTL_LO((module), (channel)) |= (ctl))
#define dmacHw_RESET_CONTROL_LO(module, channel)       (dmacHw_REG_CTL_LO((module), (channel)) = 0)
#define dmacHw_GET_CONTROL_LO(module, channel)         (dmacHw_REG_CTL_LO((module), (channel)))

#define dmacHw_SET_CONTROL_HI(module, channel, ctl)    (dmacHw_REG_CTL_HI((module), (channel)) |= (ctl))
#define dmacHw_RESET_CONTROL_HI(module, channel)       (dmacHw_REG_CTL_HI((module), (channel)) = 0)
#define dmacHw_GET_CONTROL_HI(module, channel)         (dmacHw_REG_CTL_HI((module), (channel)))

#define dmacHw_GET_BLOCK_SIZE(module, channel)         (dmacHw_REG_CTL_HI((module), (channel)) & dmacHw_REG_CTL_BLOCK_TS_MASK)
#define dmacHw_DMA_COMPLETE(module, channel)           (dmacHw_REG_CTL_HI((module), (channel)) & dmacHw_REG_CTL_DONE)

#define dmacHw_SET_CONFIG_LO(module, channel, cfg)     (dmacHw_REG_CFG_LO((module), (channel)) |= (cfg))
#define dmacHw_RESET_CONFIG_LO(module, channel)        (dmacHw_REG_CFG_LO((module), (channel)) = 0)
#define dmacHw_GET_CONFIG_LO(module, channel)          (dmacHw_REG_CFG_LO((module), (channel)))
#define dmacHw_SET_AMBA_BUSRT_LEN(module, channel, len)    (dmacHw_REG_CFG_LO((module), (channel)) = (dmacHw_REG_CFG_LO((module), (channel)) & ~(dmacHw_REG_CFG_LO_MAX_AMBA_BURST_LEN_MASK)) | (((len) << 20) & dmacHw_REG_CFG_LO_MAX_AMBA_BURST_LEN_MASK))
#define dmacHw_SET_CHANNEL_PRIORITY(module, channel, prio) (dmacHw_REG_CFG_LO((module), (channel)) = (dmacHw_REG_CFG_LO((module), (channel)) & ~(dmacHw_REG_CFG_LO_CH_PRIORITY_MASK)) | (prio))
#define dmacHw_SET_AHB_HPROT(module, channel, protect)  (dmacHw_REG_CFG_HI(module, channel) = (dmacHw_REG_CFG_HI((module), (channel)) & ~(dmacHw_REG_CFG_HI_AHB_HPROT_MASK)) | (protect))

#define dmacHw_SET_CONFIG_HI(module, channel, cfg)      (dmacHw_REG_CFG_HI((module), (channel)) |= (cfg))
#define dmacHw_RESET_CONFIG_HI(module, channel)         (dmacHw_REG_CFG_HI((module), (channel)) = 0)
#define dmacHw_GET_CONFIG_HI(module, channel)           (dmacHw_REG_CFG_HI((module), (channel)))
#define dmacHw_SET_SRC_PERI_INTF(module, channel, intf) (dmacHw_REG_CFG_HI((module), (channel)) = (dmacHw_REG_CFG_HI((module), (channel)) & ~(dmacHw_REG_CFG_HI_SRC_PERI_INTF_MASK)) | (((intf) << 7) & dmacHw_REG_CFG_HI_SRC_PERI_INTF_MASK))
#define dmacHw_SRC_PERI_INTF(intf)                      (((intf) << 7) & dmacHw_REG_CFG_HI_SRC_PERI_INTF_MASK)
#define dmacHw_SET_DST_PERI_INTF(module, channel, intf) (dmacHw_REG_CFG_HI((module), (channel)) = (dmacHw_REG_CFG_HI((module), (channel)) & ~(dmacHw_REG_CFG_HI_DST_PERI_INTF_MASK)) | (((intf) << 11) & dmacHw_REG_CFG_HI_DST_PERI_INTF_MASK))
#define dmacHw_DST_PERI_INTF(intf)                      (((intf) << 11) & dmacHw_REG_CFG_HI_DST_PERI_INTF_MASK)

#define dmacHw_DMA_START(module, channel)              (dmacHw_REG_MISC_CH_ENABLE((module)) = (0x00000001 << ((channel) + 8)) | (0x00000001 << (channel)))
#define dmacHw_DMA_STOP(module, channel)               (dmacHw_REG_MISC_CH_ENABLE((module)) = (0x00000001 << ((channel) + 8)))
#define dmacHw_DMA_ENABLE(module)                      (dmacHw_REG_MISC_CFG((module)) = 1)
#define dmacHw_DMA_DISABLE(module)                     (dmacHw_REG_MISC_CFG((module)) = 0)

#define dmacHw_TRAN_INT_ENABLE(module, channel)        (dmacHw_REG_INT_MASK_TRAN((module)) = (0x00000001 << ((channel) + 8)) | (0x00000001 << (channel)))
#define dmacHw_BLOCK_INT_ENABLE(module, channel)       (dmacHw_REG_INT_MASK_BLOCK((module)) = (0x00000001 << ((channel) + 8)) | (0x00000001 << (channel)))
#define dmacHw_ERROR_INT_ENABLE(module, channel)       (dmacHw_REG_INT_MASK_ERROR((module)) = (0x00000001 << ((channel) + 8)) | (0x00000001 << (channel)))

#define dmacHw_TRAN_INT_DISABLE(module, channel)       (dmacHw_REG_INT_MASK_TRAN((module)) = (0x00000001 << ((channel) + 8)))
#define dmacHw_BLOCK_INT_DISABLE(module, channel)      (dmacHw_REG_INT_MASK_BLOCK((module)) = (0x00000001 << ((channel) + 8)))
#define dmacHw_ERROR_INT_DISABLE(module, channel)      (dmacHw_REG_INT_MASK_ERROR((module)) = (0x00000001 << ((channel) + 8)))
#define dmacHw_STRAN_INT_DISABLE(module, channel)      (dmacHw_REG_INT_MASK_STRAN((module)) = (0x00000001 << ((channel) + 8)))
#define dmacHw_DTRAN_INT_DISABLE(module, channel)      (dmacHw_REG_INT_MASK_DTRAN((module)) = (0x00000001 << ((channel) + 8)))

#define dmacHw_TRAN_INT_CLEAR(module, channel)         (dmacHw_REG_INT_CLEAR_TRAN((module)) = (0x00000001 << (channel)))
#define dmacHw_BLOCK_INT_CLEAR(module, channel)        (dmacHw_REG_INT_CLEAR_BLOCK((module)) = (0x00000001 << (channel)))
#define dmacHw_ERROR_INT_CLEAR(module, channel)        (dmacHw_REG_INT_CLEAR_ERROR((module)) = (0x00000001 << (channel)))

#define dmacHw_GET_NUM_CHANNEL(module)                 (((dmacHw_REG_MISC_COMP_PARAM1_HI((module)) & dmacHw_REG_COMP_PARAM_NUM_CHANNELS) >> 8) + 1)
#define dmacHw_GET_NUM_INTERFACE(module)               (((dmacHw_REG_MISC_COMP_PARAM1_HI((module)) & dmacHw_REG_COMP_PARAM_NUM_INTERFACE) >> 11) + 1)
#define dmacHw_GET_MAX_BLOCK_SIZE(module, channel)     ((dmacHw_REG_MISC_COMP_PARAM1_LO((module)) >> (4 * (channel))) & dmacHw_REG_COMP_PARAM_MAX_BLK_SIZE)
#define dmacHw_GET_CHANNEL_DATA_WIDTH(module, channel) ((dmacHw_REG_MISC_COMP_PARAM1_HI((module)) & dmacHw_REG_COMP_PARAM_DATA_WIDTH) >> 13)

#endif 
