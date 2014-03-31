/*
 * Audio support for PS3
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * Copyright 2006, 2007 Sony Corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */


#define PS3_AUDIO_INTR_0                 (0x00000100)
#define PS3_AUDIO_INTR_EN_0              (0x00000140)
#define PS3_AUDIO_CONFIG                 (0x00000200)

#define PS3_AUDIO_DMAC_REGBASE(x)         (0x0000210 + 0x20 * (x))

#define PS3_AUDIO_KICK(n)                 (PS3_AUDIO_DMAC_REGBASE(n) + 0x00)
#define PS3_AUDIO_SOURCE(n)               (PS3_AUDIO_DMAC_REGBASE(n) + 0x04)
#define PS3_AUDIO_DEST(n)                 (PS3_AUDIO_DMAC_REGBASE(n) + 0x08)
#define PS3_AUDIO_DMASIZE(n)              (PS3_AUDIO_DMAC_REGBASE(n) + 0x0C)

#define PS3_AUDIO_AX_MCTRL                (0x00004000)
#define PS3_AUDIO_AX_ISBP                 (0x00004004)
#define PS3_AUDIO_AX_AOBP                 (0x00004008)
#define PS3_AUDIO_AX_IC                   (0x00004010)
#define PS3_AUDIO_AX_IE                   (0x00004014)
#define PS3_AUDIO_AX_IS                   (0x00004018)

#define PS3_AUDIO_AO_MCTRL                (0x00006000)
#define PS3_AUDIO_AO_3WMCTRL              (0x00006004)

#define PS3_AUDIO_AO_3WCTRL(n)            (0x00006200 + 0x200 * (n))

#define PS3_AUDIO_AO_SPD_REGBASE(n)       (0x00007200 + 0x200 * (n))

#define PS3_AUDIO_AO_SPDCTRL(n) \
	(PS3_AUDIO_AO_SPD_REGBASE(n) + 0x00)
#define PS3_AUDIO_AO_SPDUB(n, x) \
	(PS3_AUDIO_AO_SPD_REGBASE(n) + 0x04 + 0x04 * (x))
#define PS3_AUDIO_AO_SPDCS(n, y) \
	(PS3_AUDIO_AO_SPD_REGBASE(n) + 0x34 + 0x04 * (y))


#define PS3_AUDIO_INTR_0_CHAN(n)	(1 << ((n) * 2))
#define PS3_AUDIO_INTR_0_CHAN9     PS3_AUDIO_INTR_0_CHAN(9)
#define PS3_AUDIO_INTR_0_CHAN8     PS3_AUDIO_INTR_0_CHAN(8)
#define PS3_AUDIO_INTR_0_CHAN7     PS3_AUDIO_INTR_0_CHAN(7)
#define PS3_AUDIO_INTR_0_CHAN6     PS3_AUDIO_INTR_0_CHAN(6)
#define PS3_AUDIO_INTR_0_CHAN5     PS3_AUDIO_INTR_0_CHAN(5)
#define PS3_AUDIO_INTR_0_CHAN4     PS3_AUDIO_INTR_0_CHAN(4)
#define PS3_AUDIO_INTR_0_CHAN3     PS3_AUDIO_INTR_0_CHAN(3)
#define PS3_AUDIO_INTR_0_CHAN2     PS3_AUDIO_INTR_0_CHAN(2)
#define PS3_AUDIO_INTR_0_CHAN1     PS3_AUDIO_INTR_0_CHAN(1)
#define PS3_AUDIO_INTR_0_CHAN0     PS3_AUDIO_INTR_0_CHAN(0)



#define PS3_AUDIO_CONFIG_CLEAR          (1 << 8)  


#define PS3_AUDIO_AX_MCTRL_ASOMT(n)     (1 << (3 - (n)))  
#define PS3_AUDIO_AX_MCTRL_ASO3MT       (1 << 0)          
#define PS3_AUDIO_AX_MCTRL_ASO2MT       (1 << 1)          
#define PS3_AUDIO_AX_MCTRL_ASO1MT       (1 << 2)          
#define PS3_AUDIO_AX_MCTRL_ASO0MT       (1 << 3)          

#define PS3_AUDIO_AX_MCTRL_SPOMT(n)     (1 << (5 - (n)))  
#define PS3_AUDIO_AX_MCTRL_SPO1MT       (1 << 4)          
#define PS3_AUDIO_AX_MCTRL_SPO0MT       (1 << 5)          

#define PS3_AUDIO_AX_MCTRL_AASOMT       (1 << 13)         

#define PS3_AUDIO_AX_MCTRL_ASPOMT       (1 << 14)         

#define PS3_AUDIO_AX_MCTRL_AAOMT        (1 << 15)         


#define PS3_AUDIO_AX_ISBP_SPOBRN_MASK(n) (0x7 << 4 * (1 - (n))) 
#define PS3_AUDIO_AX_ISBP_SPO1BRN_MASK		(0x7 << 0) 
#define PS3_AUDIO_AX_ISBP_SPO0BRN_MASK		(0x7 << 4) 

#define PS3_AUDIO_AX_ISBP_SPOBWN_MASK(n) (0x7 <<  4 * (5 - (n))) 
#define PS3_AUDIO_AX_ISBP_SPO1BWN_MASK		(0x7 << 16) 
#define PS3_AUDIO_AX_ISBP_SPO0BWN_MASK		(0x7 << 20) 


#define PS3_AUDIO_AX_AOBP_ASOBRN_MASK(n) (0x7 << 4 * (3 - (n))) 

#define PS3_AUDIO_AX_AOBP_ASO3BRN_MASK	(0x7 << 0) 
#define PS3_AUDIO_AX_AOBP_ASO2BRN_MASK	(0x7 << 4) 
#define PS3_AUDIO_AX_AOBP_ASO1BRN_MASK	(0x7 << 8) 
#define PS3_AUDIO_AX_AOBP_ASO0BRN_MASK	(0x7 << 12) 

#define PS3_AUDIO_AX_AOBP_ASOBWN_MASK(n) (0x7 << 4 * (7 - (n))) 

#define PS3_AUDIO_AX_AOBP_ASO3BWN_MASK        (0x7 << 16) 
#define PS3_AUDIO_AX_AOBP_ASO2BWN_MASK        (0x7 << 20) 
#define PS3_AUDIO_AX_AOBP_ASO1BWN_MASK        (0x7 << 24) 
#define PS3_AUDIO_AX_AOBP_ASO0BWN_MASK        (0x7 << 28) 



#define PS3_AUDIO_AX_IC_AASOIMD_MASK          (0x3 << 12) 
#define PS3_AUDIO_AX_IC_AASOIMD_EVERY1        (0x0 << 12) 
#define PS3_AUDIO_AX_IC_AASOIMD_EVERY2        (0x1 << 12) 
#define PS3_AUDIO_AX_IC_AASOIMD_EVERY4        (0x2 << 12) 

#define PS3_AUDIO_AX_IC_SPO1IMD_MASK          (0x3 << 16) 
#define PS3_AUDIO_AX_IC_SPO1IMD_EVERY1        (0x0 << 16) 
#define PS3_AUDIO_AX_IC_SPO1IMD_EVERY2        (0x1 << 16) 
#define PS3_AUDIO_AX_IC_SPO1IMD_EVERY4        (0x2 << 16) 

#define PS3_AUDIO_AX_IC_SPO0IMD_MASK          (0x3 << 20) 
#define PS3_AUDIO_AX_IC_SPO0IMD_EVERY1        (0x0 << 20) 
#define PS3_AUDIO_AX_IC_SPO0IMD_EVERY2        (0x1 << 20) 
#define PS3_AUDIO_AX_IC_SPO0IMD_EVERY4        (0x2 << 20) 


#define PS3_AUDIO_AX_IE_ASOBUIE(n)      (1 << (3 - (n))) 
#define PS3_AUDIO_AX_IE_ASO3BUIE        (1 << 0) 
#define PS3_AUDIO_AX_IE_ASO2BUIE        (1 << 1) 
#define PS3_AUDIO_AX_IE_ASO1BUIE        (1 << 2) 
#define PS3_AUDIO_AX_IE_ASO0BUIE        (1 << 3) 


#define PS3_AUDIO_AX_IE_SPOBUIE(n)      (1 << (7 - (n))) 
#define PS3_AUDIO_AX_IE_SPO1BUIE        (1 << 6) 
#define PS3_AUDIO_AX_IE_SPO0BUIE        (1 << 7) 


#define PS3_AUDIO_AX_IE_SPOBTCIE(n)     (1 << (11 - (n))) 
#define PS3_AUDIO_AX_IE_SPO1BTCIE       (1 << 10) 
#define PS3_AUDIO_AX_IE_SPO0BTCIE       (1 << 11) 


#define PS3_AUDIO_AX_IE_ASOBEIE(n)      (1 << (19 - (n))) 
#define PS3_AUDIO_AX_IE_ASO3BEIE        (1 << 16) 
#define PS3_AUDIO_AX_IE_ASO2BEIE        (1 << 17) 
#define PS3_AUDIO_AX_IE_ASO1BEIE        (1 << 18) 
#define PS3_AUDIO_AX_IE_ASO0BEIE        (1 << 19) 


#define PS3_AUDIO_AX_IE_SPOBEIE(n)      (1 << (23 - (n))) 
#define PS3_AUDIO_AX_IE_SPO1BEIE        (1 << 22) 
#define PS3_AUDIO_AX_IE_SPO0BEIE        (1 << 23) 




#define PS3_AUDIO_AO_MCTRL_MCLKC1_MASK		(0x3 << 12) 
#define PS3_AUDIO_AO_MCTRL_MCLKC1_DISABLED	(0x0 << 12) 
#define PS3_AUDIO_AO_MCTRL_MCLKC1_ENABLED	(0x1 << 12) 
#define PS3_AUDIO_AO_MCTRL_MCLKC1_RESVD2	(0x2 << 12) 
#define PS3_AUDIO_AO_MCTRL_MCLKC1_RESVD3	(0x3 << 12) 

#define PS3_AUDIO_AO_MCTRL_MCLKC0_MASK		(0x3 << 14) 
#define PS3_AUDIO_AO_MCTRL_MCLKC0_DISABLED	(0x0 << 14) 
#define PS3_AUDIO_AO_MCTRL_MCLKC0_ENABLED	(0x1 << 14) 
#define PS3_AUDIO_AO_MCTRL_MCLKC0_RESVD2	(0x2 << 14) 
#define PS3_AUDIO_AO_MCTRL_MCLKC0_RESVD3	(0x3 << 14) 
#define PS3_AUDIO_AO_MCTRL_MR1_MASK	(0xf << 16)
#define PS3_AUDIO_AO_MCTRL_MR1_DEFAULT	(0x0 << 16) 
#define PS3_AUDIO_AO_MCTRL_MR0_MASK	(0xf << 20) 
#define PS3_AUDIO_AO_MCTRL_MR0_DEFAULT	(0x0 << 20) 
#define PS3_AUDIO_AO_MCTRL_SCKSEL1_MASK		(0x7 << 24) 
#define PS3_AUDIO_AO_MCTRL_SCKSEL1_DEFAULT	(0x2 << 24) 

#define PS3_AUDIO_AO_MCTRL_SCKSEL0_MASK		(0x7 << 28) 
#define PS3_AUDIO_AO_MCTRL_SCKSEL0_DEFAULT	(0x2 << 28) 




#define PS3_AUDIO_AO_3WMCTRL_ASOPLRCK 		(1 << 8) 
#define PS3_AUDIO_AO_3WMCTRL_ASOPLRCK_DEFAULT	(1 << 8) 


#define PS3_AUDIO_AO_3WMCTRL_ASOLRCKD		(1 << 10) 
#define PS3_AUDIO_AO_3WMCTRL_ASOLRCKD_ENABLED	(0 << 10) 
#define PS3_AUDIO_AO_3WMCTRL_ASOLRCKD_DISABLED	(1 << 10) 


#define PS3_AUDIO_AO_3WMCTRL_ASOBCLKD		(1 << 11) 
#define PS3_AUDIO_AO_3WMCTRL_ASOBCLKD_ENABLED	(0 << 11) 
#define PS3_AUDIO_AO_3WMCTRL_ASOBCLKD_DISABLED	(1 << 11) 

#define PS3_AUDIO_AO_3WMCTRL_ASORUN(n)		(1 << (15 - (n))) 
#define PS3_AUDIO_AO_3WMCTRL_ASORUN_STOPPED(n)	(0 << (15 - (n))) 
#define PS3_AUDIO_AO_3WMCTRL_ASORUN_RUNNING(n)	(1 << (15 - (n))) 
#define PS3_AUDIO_AO_3WMCTRL_ASORUN0		\
	PS3_AUDIO_AO_3WMCTRL_ASORUN(0)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN0_STOPPED	\
	PS3_AUDIO_AO_3WMCTRL_ASORUN_STOPPED(0)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN0_RUNNING	\
	PS3_AUDIO_AO_3WMCTRL_ASORUN_RUNNING(0)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN1		\
	PS3_AUDIO_AO_3WMCTRL_ASORUN(1)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN1_STOPPED	\
	PS3_AUDIO_AO_3WMCTRL_ASORUN_STOPPED(1)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN1_RUNNING	\
	PS3_AUDIO_AO_3WMCTRL_ASORUN_RUNNING(1)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN2		\
	PS3_AUDIO_AO_3WMCTRL_ASORUN(2)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN2_STOPPED	\
	PS3_AUDIO_AO_3WMCTRL_ASORUN_STOPPED(2)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN2_RUNNING	\
	PS3_AUDIO_AO_3WMCTRL_ASORUN_RUNNING(2)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN3		\
	PS3_AUDIO_AO_3WMCTRL_ASORUN(3)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN3_STOPPED	\
	PS3_AUDIO_AO_3WMCTRL_ASORUN_STOPPED(3)
#define PS3_AUDIO_AO_3WMCTRL_ASORUN3_RUNNING	\
	PS3_AUDIO_AO_3WMCTRL_ASORUN_RUNNING(3)

#define PS3_AUDIO_AO_3WMCTRL_ASOSR_MASK		(0xf << 20) 
#define PS3_AUDIO_AO_3WMCTRL_ASOSR_DIV2		(0x1 << 20) 
#define PS3_AUDIO_AO_3WMCTRL_ASOSR_DIV4		(0x2 << 20) 
#define PS3_AUDIO_AO_3WMCTRL_ASOSR_DIV8		(0x4 << 20) 
#define PS3_AUDIO_AO_3WMCTRL_ASOSR_DIV12	(0x6 << 20) 

#define PS3_AUDIO_AO_3WMCTRL_ASOMCKSEL		(1 << 24) 
#define PS3_AUDIO_AO_3WMCTRL_ASOMCKSEL_CLK0	(0 << 24) 
#define PS3_AUDIO_AO_3WMCTRL_ASOMCKSEL_CLK1	(1 << 24) 

#define PS3_AUDIO_AO_3WMCTRL_ASOEN(n)		(1 << (31 - (n))) 
#define PS3_AUDIO_AO_3WMCTRL_ASOEN_DISABLED(n)	(0 << (31 - (n))) 
#define PS3_AUDIO_AO_3WMCTRL_ASOEN_ENABLED(n)	(1 << (31 - (n))) 

#define PS3_AUDIO_AO_3WMCTRL_ASOEN0 \
	PS3_AUDIO_AO_3WMCTRL_ASOEN(0) 
#define PS3_AUDIO_AO_3WMCTRL_ASOEN0_DISABLED \
	PS3_AUDIO_AO_3WMCTRL_ASOEN_DISABLED(0) 
#define PS3_AUDIO_AO_3WMCTRL_ASOEN0_ENABLED \
	PS3_AUDIO_AO_3WMCTRL_ASOEN_ENABLED(0) 
#define PS3_AUDIO_A1_3WMCTRL_ASOEN0 \
	PS3_AUDIO_AO_3WMCTRL_ASOEN(1) 
#define PS3_AUDIO_A1_3WMCTRL_ASOEN0_DISABLED \
	PS3_AUDIO_AO_3WMCTRL_ASOEN_DISABLED(1) 
#define PS3_AUDIO_A1_3WMCTRL_ASOEN0_ENABLED \
	PS3_AUDIO_AO_3WMCTRL_ASOEN_ENABLED(1) 
#define PS3_AUDIO_A2_3WMCTRL_ASOEN0 \
	PS3_AUDIO_AO_3WMCTRL_ASOEN(2) 
#define PS3_AUDIO_A2_3WMCTRL_ASOEN0_DISABLED \
	PS3_AUDIO_AO_3WMCTRL_ASOEN_DISABLED(2) 
#define PS3_AUDIO_A2_3WMCTRL_ASOEN0_ENABLED \
	PS3_AUDIO_AO_3WMCTRL_ASOEN_ENABLED(2) 
#define PS3_AUDIO_A3_3WMCTRL_ASOEN0 \
	PS3_AUDIO_AO_3WMCTRL_ASOEN(3) 
#define PS3_AUDIO_A3_3WMCTRL_ASOEN0_DISABLED \
	PS3_AUDIO_AO_3WMCTRL_ASOEN_DISABLED(3) 
#define PS3_AUDIO_A3_3WMCTRL_ASOEN0_ENABLED \
	PS3_AUDIO_AO_3WMCTRL_ASOEN_ENABLED(3) 

#define PS3_AUDIO_AO_3WCTRL_ASODB_MASK	(0x3 << 8) 
#define PS3_AUDIO_AO_3WCTRL_ASODB_16BIT	(0x0 << 8) 
#define PS3_AUDIO_AO_3WCTRL_ASODB_RESVD	(0x1 << 8) 
#define PS3_AUDIO_AO_3WCTRL_ASODB_20BIT	(0x2 << 8) 
#define PS3_AUDIO_AO_3WCTRL_ASODB_24BIT	(0x3 << 8) 
#define PS3_AUDIO_AO_3WCTRL_ASODF 	(1 << 11) 
#define PS3_AUDIO_AO_3WCTRL_ASODF_LSB	(0 << 11) 
#define PS3_AUDIO_AO_3WCTRL_ASODF_MSB	(1 << 11) 
#define PS3_AUDIO_AO_3WCTRL_ASOBRST 		(1 << 16) 
#define PS3_AUDIO_AO_3WCTRL_ASOBRST_IDLE	(0 << 16) 
#define PS3_AUDIO_AO_3WCTRL_ASOBRST_RESET	(1 << 16) 

#define PS3_AUDIO_AO_SPDCTRL_SPOBRST		(1 << 0) 
#define PS3_AUDIO_AO_SPDCTRL_SPOBRST_IDLE	(0 << 0) 
#define PS3_AUDIO_AO_SPDCTRL_SPOBRST_RESET	(1 << 0) 

#define PS3_AUDIO_AO_SPDCTRL_SPODB_MASK		(0x3 << 8) 
#define PS3_AUDIO_AO_SPDCTRL_SPODB_16BIT	(0x0 << 8) 
#define PS3_AUDIO_AO_SPDCTRL_SPODB_RESVD	(0x1 << 8) 
#define PS3_AUDIO_AO_SPDCTRL_SPODB_20BIT	(0x2 << 8) 
#define PS3_AUDIO_AO_SPDCTRL_SPODB_24BIT	(0x3 << 8) 
#define PS3_AUDIO_AO_SPDCTRL_SPODF	(1 << 11) 
#define PS3_AUDIO_AO_SPDCTRL_SPODF_LSB	(0 << 11) 
#define PS3_AUDIO_AO_SPDCTRL_SPODF_MSB	(1 << 11) 
#define PS3_AUDIO_AO_SPDCTRL_SPOSS_MASK		(0x3 << 16) 
#define PS3_AUDIO_AO_SPDCTRL_SPOSS_3WEN		(0x0 << 16) 
#define PS3_AUDIO_AO_SPDCTRL_SPOSS_SPDIF	(0x1 << 16) 
#define PS3_AUDIO_AO_SPDCTRL_SPOSR		(0xf << 20) 
#define PS3_AUDIO_AO_SPDCTRL_SPOSR_DIV2		(0x1 << 20) 
#define PS3_AUDIO_AO_SPDCTRL_SPOSR_DIV4		(0x2 << 20) 
#define PS3_AUDIO_AO_SPDCTRL_SPOSR_DIV8		(0x4 << 20) 
#define PS3_AUDIO_AO_SPDCTRL_SPOSR_DIV12	(0x6 << 20) 
#define PS3_AUDIO_AO_SPDCTRL_SPOMCKSEL		(1 << 24) 
#define PS3_AUDIO_AO_SPDCTRL_SPOMCKSEL_CLK0	(0 << 24) 
#define PS3_AUDIO_AO_SPDCTRL_SPOMCKSEL_CLK1	(1 << 24) 

#define PS3_AUDIO_AO_SPDCTRL_SPORUN		(1 << 27) 
#define PS3_AUDIO_AO_SPDCTRL_SPORUN_STOPPED	(0 << 27) 
#define PS3_AUDIO_AO_SPDCTRL_SPORUN_RUNNING	(1 << 27) 

#define PS3_AUDIO_AO_SPDCTRL_SPOEN		(1 << 31) 
#define PS3_AUDIO_AO_SPDCTRL_SPOEN_DISABLED	(0 << 31) 
#define PS3_AUDIO_AO_SPDCTRL_SPOEN_ENABLED	(1 << 31) 

/*
The REQUEST field is written to ACTIVE to initiate a DMA request when EVENT
occurs.
It will return to the DONE state when the request is completed.
The registers for a DMA channel should only be written if REQUEST is IDLE.
*/

#define PS3_AUDIO_KICK_REQUEST                (1 << 0) 
#define PS3_AUDIO_KICK_REQUEST_IDLE           (0 << 0) 
#define PS3_AUDIO_KICK_REQUEST_ACTIVE         (1 << 0) 

#define PS3_AUDIO_KICK_EVENT_MASK             (0x1f << 16) 
#define PS3_AUDIO_KICK_EVENT_ALWAYS           (0x00 << 16) 
#define PS3_AUDIO_KICK_EVENT_SERIALOUT0_EMPTY (0x01 << 16) 
#define PS3_AUDIO_KICK_EVENT_SERIALOUT0_UNDERFLOW	(0x02 << 16) 
#define PS3_AUDIO_KICK_EVENT_SERIALOUT1_EMPTY		(0x03 << 16) 
#define PS3_AUDIO_KICK_EVENT_SERIALOUT1_UNDERFLOW	(0x04 << 16) 
#define PS3_AUDIO_KICK_EVENT_SERIALOUT2_EMPTY		(0x05 << 16) 
#define PS3_AUDIO_KICK_EVENT_SERIALOUT2_UNDERFLOW	(0x06 << 16) 
#define PS3_AUDIO_KICK_EVENT_SERIALOUT3_EMPTY		(0x07 << 16) 
#define PS3_AUDIO_KICK_EVENT_SERIALOUT3_UNDERFLOW	(0x08 << 16) 
#define PS3_AUDIO_KICK_EVENT_SPDIF0_BLOCKTRANSFERCOMPLETE \
	(0x09 << 16) 
#define PS3_AUDIO_KICK_EVENT_SPDIF0_UNDERFLOW		(0x0A << 16) 
#define PS3_AUDIO_KICK_EVENT_SPDIF0_EMPTY		(0x0B << 16) 
#define PS3_AUDIO_KICK_EVENT_SPDIF1_BLOCKTRANSFERCOMPLETE \
	(0x0C << 16) 
#define PS3_AUDIO_KICK_EVENT_SPDIF1_UNDERFLOW		(0x0D << 16) 
#define PS3_AUDIO_KICK_EVENT_SPDIF1_EMPTY		(0x0E << 16) 

#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA(n) \
	((0x13 + (n)) << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA0         (0x13 << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA1         (0x14 << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA2         (0x15 << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA3         (0x16 << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA4         (0x17 << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA5         (0x18 << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA6         (0x19 << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA7         (0x1A << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA8         (0x1B << 16) 
#define PS3_AUDIO_KICK_EVENT_AUDIO_DMA9         (0x1C << 16) 

/*
The STATUS field can be used to monitor the progress of a DMA request.
DONE indicates the previous request has completed.
EVENT indicates that the DMA engine is waiting for the EVENT to occur.
PENDING indicates that the DMA engine has not started processing this
request, but the EVENT has occurred.
DMA indicates that the data transfer is in progress.
NOTIFY indicates that the notifier signalling end of transfer is being written.
CLEAR indicated that the previous transfer was cleared.
ERROR indicates the previous transfer requested an unsupported
source/destination combination.
*/

#define PS3_AUDIO_KICK_STATUS_MASK	(0x7 << 24) 
#define PS3_AUDIO_KICK_STATUS_DONE	(0x0 << 24) 
#define PS3_AUDIO_KICK_STATUS_EVENT	(0x1 << 24) 
#define PS3_AUDIO_KICK_STATUS_PENDING	(0x2 << 24) 
#define PS3_AUDIO_KICK_STATUS_DMA	(0x3 << 24) 
#define PS3_AUDIO_KICK_STATUS_NOTIFY	(0x4 << 24) 
#define PS3_AUDIO_KICK_STATUS_CLEAR	(0x5 << 24) 
#define PS3_AUDIO_KICK_STATUS_ERROR	(0x6 << 24) 



#define PS3_AUDIO_SOURCE_START_MASK	(0x01FFFFFF << 7) 


#define PS3_AUDIO_SOURCE_TARGET_MASK 		(3 << 0) 
#define PS3_AUDIO_SOURCE_TARGET_SYSTEM_MEMORY	(2 << 0) 



#define PS3_AUDIO_DEST_START_MASK	(0x01FFFFFF << 7) 


#define PS3_AUDIO_DEST_TARGET_MASK		(3 << 0) 
#define PS3_AUDIO_DEST_TARGET_AUDIOFIFO		(1 << 0) 



#define PS3_AUDIO_DMASIZE_BLOCKS_MASK 	(0x7f << 0) 

#define PS3_AUDIO_AO_3W_LDATA(n)	(0x1000 + (0x100 * (n)))
#define PS3_AUDIO_AO_3W_RDATA(n)	(0x1080 + (0x100 * (n)))

#define PS3_AUDIO_AO_SPD_DATA(n)	(0x2000 + (0x400 * (n)))


/*
 * field attiribute
 *
 *	Read
 *	  ' ' = Other Information
 *	  '-' = Field is part of a write-only register
 *	  'C' = Value read is always the same, constant value line follows (C)
 *	  'R' = Value is read
 *
 *	Write
 *	  ' ' = Other Information
 *	  '-' = Must not be written (D), value ignored when written (R,A,F)
 *	  'W' = Can be written
 *
 *	Internal State
 *	  ' ' = Other Information
 *	  '-' = No internal state
 *	  'X' = Internal state, initial value is unknown
 *	  'I' = Internal state, initial value is known and follows (I)
 *
 *	Declaration/Size
 *	  ' ' = Other Information
 *	  '-' = Does Not Apply
 *	  'V' = Type is void
 *	  'U' = Type is unsigned integer
 *	  'S' = Type is signed integer
 *	  'F' = Type is IEEE floating point
 *	  '1' = Byte size (008)
 *	  '2' = Short size (016)
 *	  '3' = Three byte size (024)
 *	  '4' = Word size (032)
 *	  '8' = Double size (064)
 *
 *	Define Indicator
 *	  ' ' = Other Information
 *	  'D' = Device
 *	  'M' = Memory
 *	  'R' = Register
 *	  'A' = Array of Registers
 *	  'F' = Field
 *	  'V' = Value
 *	  'T' = Task
 */

