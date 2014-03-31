/*
    comedi/drivers/rtd520.h
    Comedi driver defines for Real Time Devices (RTD) PCI4520/DM7520

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2001 David A. Schleef <ds@schleef.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#define LAS0_SPARE_00    0x0000	
#define LAS0_SPARE_04    0x0004	
#define LAS0_USER_IO     0x0008	
#define LAS0_SPARE_0C    0x000C	
#define LAS0_ADC         0x0010	
#define LAS0_DAC1        0x0014	
#define LAS0_DAC2        0x0018	
#define LAS0_SPARE_1C    0x001C	
#define LAS0_SPARE_20    0x0020	
#define LAS0_DAC         0x0024	
#define LAS0_PACER       0x0028	
#define LAS0_TIMER       0x002C	
#define LAS0_IT          0x0030	
#define LAS0_CLEAR       0x0034	
#define LAS0_OVERRUN     0x0038	
#define LAS0_SPARE_3C    0x003C	

#define LAS0_PCLK        0x0040	
#define LAS0_BCLK        0x0044	
#define LAS0_ADC_SCNT    0x0048	
#define LAS0_DAC1_UCNT   0x004C	
#define LAS0_DAC2_UCNT   0x0050	
#define LAS0_DCNT        0x0054	
#define LAS0_ACNT        0x0058	
#define LAS0_DAC_CLK     0x005C	
#define LAS0_UTC0        0x0060	
#define LAS0_UTC1        0x0064	
#define LAS0_UTC2        0x0068	
#define LAS0_UTC_CTRL    0x006C	
#define LAS0_DIO0        0x0070	
#define LAS0_DIO1        0x0074	
#define LAS0_DIO0_CTRL   0x0078	
#define LAS0_DIO_STATUS  0x007C	

#define LAS0_BOARD_RESET        0x0100	
#define LAS0_DMA0_SRC           0x0104	
#define LAS0_DMA1_SRC           0x0108	
#define LAS0_ADC_CONVERSION     0x010C	
#define LAS0_BURST_START        0x0110	
#define LAS0_PACER_START        0x0114	
#define LAS0_PACER_STOP         0x0118	
#define LAS0_ACNT_STOP_ENABLE   0x011C	
#define LAS0_PACER_REPEAT       0x0120	
#define LAS0_DIN_START          0x0124	
#define LAS0_DIN_FIFO_CLEAR     0x0128	
#define LAS0_ADC_FIFO_CLEAR     0x012C	
#define LAS0_CGT_WRITE          0x0130	
#define LAS0_CGL_WRITE          0x0134	
#define LAS0_CG_DATA            0x0138	
#define LAS0_CGT_ENABLE		0x013C	
#define LAS0_CG_ENABLE          0x0140	
#define LAS0_CGT_PAUSE          0x0144	
#define LAS0_CGT_RESET          0x0148	
#define LAS0_CGT_CLEAR          0x014C	
#define LAS0_DAC1_CTRL          0x0150	
#define LAS0_DAC1_SRC           0x0154	
#define LAS0_DAC1_CYCLE         0x0158	
#define LAS0_DAC1_RESET         0x015C	
#define LAS0_DAC1_FIFO_CLEAR    0x0160	
#define LAS0_DAC2_CTRL          0x0164	
#define LAS0_DAC2_SRC           0x0168	
#define LAS0_DAC2_CYCLE         0x016C	
#define LAS0_DAC2_RESET         0x0170	
#define LAS0_DAC2_FIFO_CLEAR    0x0174	
#define LAS0_ADC_SCNT_SRC       0x0178	
#define LAS0_PACER_SELECT       0x0180	
#define LAS0_SBUS0_SRC          0x0184	
#define LAS0_SBUS0_ENABLE       0x0188	
#define LAS0_SBUS1_SRC          0x018C	
#define LAS0_SBUS1_ENABLE       0x0190	
#define LAS0_SBUS2_SRC          0x0198	
#define LAS0_SBUS2_ENABLE       0x019C	
#define LAS0_ETRG_POLARITY      0x01A4	
#define LAS0_EINT_POLARITY      0x01A8	
#define LAS0_UTC0_CLOCK         0x01AC	
#define LAS0_UTC0_GATE          0x01B0	
#define LAS0_UTC1_CLOCK         0x01B4	
#define LAS0_UTC1_GATE          0x01B8	
#define LAS0_UTC2_CLOCK         0x01BC	
#define LAS0_UTC2_GATE          0x01C0	
#define LAS0_UOUT0_SELECT       0x01C4	
#define LAS0_UOUT1_SELECT       0x01C8	
#define LAS0_DMA0_RESET         0x01CC	
#define LAS0_DMA1_RESET         0x01D0	

#define LAS1_ADC_FIFO            0x0000	
#define LAS1_HDIO_FIFO           0x0004	
#define LAS1_DAC1_FIFO           0x0008	
#define LAS1_DAC2_FIFO           0x000C	

#define LCFG_ITCSR              0x0068	
#define LCFG_DMAMODE0           0x0080	
#define LCFG_DMAPADR0           0x0084	
#define LCFG_DMALADR0           0x0088	
#define LCFG_DMASIZ0            0x008C	
#define LCFG_DMADPR0            0x0090	
#define LCFG_DMAMODE1           0x0094	
#define LCFG_DMAPADR1           0x0098	
#define LCFG_DMALADR1           0x009C	
#define LCFG_DMASIZ1            0x00A0	
#define LCFG_DMADPR1            0x00A4	
#define LCFG_DMACSR0            0x00A8	
#define LCFG_DMACSR1            0x00A9	
#define LCFG_DMAARB             0x00AC	
#define LCFG_DMATHR             0x00B0	


#define FS_DAC1_NOT_EMPTY    0x0001	
#define FS_DAC1_HEMPTY   0x0002	
#define FS_DAC1_NOT_FULL     0x0004	
#define FS_DAC2_NOT_EMPTY    0x0010	
#define FS_DAC2_HEMPTY   0x0020	
#define FS_DAC2_NOT_FULL     0x0040	
#define FS_ADC_NOT_EMPTY     0x0100	
#define FS_ADC_HEMPTY    0x0200	
#define FS_ADC_NOT_FULL      0x0400	
#define FS_DIN_NOT_EMPTY     0x1000	
#define FS_DIN_HEMPTY    0x2000	
#define FS_DIN_NOT_FULL      0x4000	

#define TS_PCLK_GATE   0x0001
#define TS_BCLK_GATE   0x0002
#define TS_DCNT_GATE   0x0004
#define TS_ACNT_GATE   0x0008
#define TS_PCLK_RUN    0x0010

#define POL_POSITIVE         0x0	
#define POL_NEGATIVE         0x1	

#define UOUT_ADC                0x0	
#define UOUT_DAC1               0x1	
#define UOUT_DAC2               0x2	
#define UOUT_SOFTWARE           0x3	

#define PCLK_INTERNAL           1	
#define PCLK_EXTERNAL           0	

#define ADC_SCNT_CGT_RESET         0x0	
#define ADC_SCNT_FIFO_WRITE        0x1

#define ADC_START_SOFTWARE         0x0	
#define ADC_START_PCLK             0x1	
#define ADC_START_BCLK             0x2	
#define ADC_START_DIGITAL_IT       0x3	
#define ADC_START_DAC1_MARKER1     0x4	
#define ADC_START_DAC2_MARKER1     0x5	
#define ADC_START_SBUS0            0x6	
#define ADC_START_SBUS1            0x7	
#define ADC_START_SBUS2            0x8	

#define BCLK_START_SOFTWARE        0x0	
#define BCLK_START_PCLK            0x1	
#define BCLK_START_ETRIG           0x2	
#define BCLK_START_DIGITAL_IT      0x3	
#define BCLK_START_SBUS0           0x4	
#define BCLK_START_SBUS1           0x5	
#define BCLK_START_SBUS2           0x6	

#define PCLK_START_SOFTWARE        0x0	
#define PCLK_START_ETRIG           0x1	
#define PCLK_START_DIGITAL_IT      0x2	
#define PCLK_START_UTC2            0x3	
#define PCLK_START_SBUS0           0x4	
#define PCLK_START_SBUS1           0x5	
#define PCLK_START_SBUS2           0x6	
#define PCLK_START_D_SOFTWARE      0x8	
#define PCLK_START_D_ETRIG         0x9	
#define PCLK_START_D_DIGITAL_IT    0xA	
#define PCLK_START_D_UTC2          0xB	
#define PCLK_START_D_SBUS0         0xC	
#define PCLK_START_D_SBUS1         0xD	
#define PCLK_START_D_SBUS2         0xE	
#define PCLK_START_ETRIG_GATED     0xF	

#define PCLK_STOP_SOFTWARE         0x0	
#define PCLK_STOP_ETRIG            0x1	
#define PCLK_STOP_DIGITAL_IT       0x2	
#define PCLK_STOP_ACNT             0x3	
#define PCLK_STOP_UTC2             0x4	
#define PCLK_STOP_SBUS0            0x5	
#define PCLK_STOP_SBUS1            0x6	
#define PCLK_STOP_SBUS2            0x7	
#define PCLK_STOP_A_SOFTWARE       0x8	
#define PCLK_STOP_A_ETRIG          0x9	
#define PCLK_STOP_A_DIGITAL_IT     0xA	
#define PCLK_STOP_A_UTC2           0xC	
#define PCLK_STOP_A_SBUS0          0xD	
#define PCLK_STOP_A_SBUS1          0xE	
#define PCLK_STOP_A_SBUS2          0xF	

#define ACNT_STOP                  0x0	
#define ACNT_NO_STOP               0x1	

#define DAC_START_SOFTWARE         0x0	
#define DAC_START_CGT              0x1	
#define DAC_START_DAC_CLK          0x2	
#define DAC_START_EPCLK            0x3	
#define DAC_START_SBUS0            0x4	
#define DAC_START_SBUS1            0x5	
#define DAC_START_SBUS2            0x6	

#define DAC_CYCLE_SINGLE           0x0	
#define DAC_CYCLE_MULTI            0x1	

#define M8254_EVENT_COUNTER        0	
#define M8254_HW_ONE_SHOT          1	
#define M8254_RATE_GENERATOR       2	
#define M8254_SQUARE_WAVE          3	
#define M8254_SW_STROBE            4	
#define M8254_HW_STROBE            5	

#define CUTC0_8MHZ                 0x0	
#define CUTC0_EXT_TC_CLOCK1        0x1	
#define CUTC0_EXT_TC_CLOCK2        0x2	
#define CUTC0_EXT_PCLK             0x3	

#define CUTC1_8MHZ                 0x0	
#define CUTC1_EXT_TC_CLOCK1        0x1	
#define CUTC1_EXT_TC_CLOCK2        0x2	
#define CUTC1_EXT_PCLK             0x3	
#define CUTC1_UTC0_OUT             0x4	
#define CUTC1_DIN_SIGNAL           0x5	

#define CUTC2_8MHZ                 0x0	
#define CUTC2_EXT_TC_CLOCK1        0x1	
#define CUTC2_EXT_TC_CLOCK2        0x2	
#define CUTC2_EXT_PCLK             0x3	
#define CUTC2_UTC1_OUT             0x4	

#define GUTC0_NOT_GATED            0x0	
#define GUTC0_GATED                0x1	
#define GUTC0_EXT_TC_GATE1         0x2	
#define GUTC0_EXT_TC_GATE2         0x3	

#define GUTC1_NOT_GATED            0x0	
#define GUTC1_GATED                0x1	
#define GUTC1_EXT_TC_GATE1         0x2	
#define GUTC1_EXT_TC_GATE2         0x3	
#define GUTC1_UTC0_OUT             0x4	

#define GUTC2_NOT_GATED            0x0	
#define GUTC2_GATED                0x1	
#define GUTC2_EXT_TC_GATE1         0x2	
#define GUTC2_EXT_TC_GATE2         0x3	
#define GUTC2_UTC1_OUT             0x4	

#define IRQM_ADC_FIFO_WRITE        0x0001	
#define IRQM_CGT_RESET             0x0002	
#define IRQM_CGT_PAUSE             0x0008	
#define IRQM_ADC_ABOUT_CNT         0x0010	
#define IRQM_ADC_DELAY_CNT         0x0020	
#define IRQM_ADC_SAMPLE_CNT	   0x0040	
#define IRQM_DAC1_UCNT             0x0080	
#define IRQM_DAC2_UCNT             0x0100	
#define IRQM_UTC1                  0x0200	
#define IRQM_UTC1_INV              0x0400	
#define IRQM_UTC2                  0x0800	
#define IRQM_DIGITAL_IT            0x1000	
#define IRQM_EXTERNAL_IT           0x2000	
#define IRQM_ETRIG_RISING          0x4000	
#define IRQM_ETRIG_FALLING         0x8000	

#define DMAS_DISABLED              0x0	
#define DMAS_ADC_SCNT              0x1	
#define DMAS_DAC1_UCNT             0x2	
#define DMAS_DAC2_UCNT             0x3	
#define DMAS_UTC1                  0x4	
#define DMAS_ADFIFO_HALF_FULL      0x8	
#define DMAS_DAC1_FIFO_HALF_EMPTY  0x9	
#define DMAS_DAC2_FIFO_HALF_EMPTY  0xA	

#define DMALADDR_ADC       0x40000000	
#define DMALADDR_HDIN      0x40000004	
#define DMALADDR_DAC1      0x40000008	
#define DMALADDR_DAC2      0x4000000C	

#define DIO_MODE_EVENT     0	
#define DIO_MODE_MATCH     1	

#define DTBL_DISABLE       0	
#define DTBL_ENABLE        1	

#define HDIN_SOFTWARE      0x0	
#define HDIN_ADC           0x1	
#define HDIN_UTC0          0x2	
#define HDIN_UTC1          0x3	
#define HDIN_UTC2          0x4	
#define HDIN_EPCLK         0x5	
#define HDIN_ETRG          0x6	

#define CSC_LATCH          0	
#define CSC_CGT            1	

#define CGT_PAUSE_DISABLE  0	
#define CGT_PAUSE_ENABLE   1	

#define AOUT_UNIP5         0	
#define AOUT_UNIP10        1	
#define AOUT_BIP5          2	
#define AOUT_BIP10         3	

#define GAIN1              0
#define GAIN2              1
#define GAIN4              2
#define GAIN8              3
#define GAIN16             4
#define GAIN32             5
#define GAIN64             6
#define GAIN128            7

#define AIN_BIP5           0	
#define AIN_BIP10          1	
#define AIN_UNIP10         2	

#define NRSE_AGND          0	
#define NRSE_AINS          1	

#define GND_SE		0	
#define GND_DIFF	1	
