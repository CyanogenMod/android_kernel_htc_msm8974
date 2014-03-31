/*
*
* 3780i.h -- declarations for 3780i.c
*
*
* Written By: Mike Sullivan IBM Corporation
*
* Copyright (C) 1999 IBM Corporation
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* NO WARRANTY
* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
* CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
* LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
* solely responsible for determining the appropriateness of using and
* distributing the Program and assumes all risks associated with its
* exercise of rights under this Agreement, including but not limited to
* the risks and costs of program errors, damage to or loss of data,
* programs or equipment, and unavailability or interruption of operations.
*
* DISCLAIMER OF LIABILITY
* NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
* HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*
* 10/23/2000 - Alpha Release
*	First release to the public
*/

#ifndef _LINUX_3780I_H
#define _LINUX_3780I_H

#include <asm/io.h>

#define DSP_IsaSlaveControl        0x0000	
#define DSP_IsaSlaveStatus         0x0001	
#define DSP_ConfigAddress          0x0002	
#define DSP_ConfigData             0x0003	
#define DSP_HBridgeControl         0x0002	
#define DSP_MsaAddrLow             0x0004	
#define DSP_MsaAddrHigh            0x0006	
#define DSP_MsaDataDSISHigh        0x0008	
#define DSP_MsaDataISLow           0x000A	
#define DSP_ReadAndClear           0x000C	
#define DSP_Interrupt              0x000E	

typedef struct {
	unsigned char ClockControl:1;	
	unsigned char SoftReset:1;	
	unsigned char ConfigMode:1;	
	unsigned char Reserved:5;	
} DSP_ISA_SLAVE_CONTROL;


typedef struct {
	unsigned short EnableDspInt:1;	
	unsigned short MemAutoInc:1;	
	unsigned short IoAutoInc:1;	
	unsigned short DiagnosticMode:1;	
	unsigned short IsaPacingTimer:12;	
} DSP_HBRIDGE_CONTROL;


#define DSP_UartCfg1Index          0x0003	
#define DSP_UartCfg2Index          0x0004	
#define DSP_HBridgeCfg1Index       0x0007	
#define DSP_HBridgeCfg2Index       0x0008	
#define DSP_BusMasterCfg1Index     0x0009	
#define DSP_BusMasterCfg2Index     0x000A	
#define DSP_IsaProtCfgIndex        0x000F	
#define DSP_PowerMgCfgIndex        0x0010	
#define DSP_HBusTimerCfgIndex      0x0011	

typedef struct {
	unsigned char IrqActiveLow:1;	
	unsigned char IrqPulse:1;	
	unsigned char Irq:3;	
	unsigned char BaseIO:2;	
	unsigned char Reserved:1;	
} DSP_UART_CFG_1;

typedef struct {
	unsigned char Enable:1;	
	unsigned char Reserved:7;	
} DSP_UART_CFG_2;

typedef struct {
	unsigned char IrqActiveLow:1;	
	unsigned char IrqPulse:1;	
	unsigned char Irq:3;	
	unsigned char AccessMode:1;	
	unsigned char Reserved:2;	
} DSP_HBRIDGE_CFG_1;

typedef struct {
	unsigned char Enable:1;	
	unsigned char Reserved:7;	
} DSP_HBRIDGE_CFG_2;


typedef struct {
	unsigned char Dma:3;	
	unsigned char NumTransfers:2;	
	unsigned char ReRequest:2;	
	unsigned char MEMCS16:1;	
} DSP_BUSMASTER_CFG_1;

typedef struct {
	unsigned char IsaMemCmdWidth:2;	
	unsigned char Reserved:6;	
} DSP_BUSMASTER_CFG_2;


typedef struct {
	unsigned char GateIOCHRDY:1;	
	unsigned char Reserved:7;	
} DSP_ISA_PROT_CFG;

typedef struct {
	unsigned char Enable:1;	
	unsigned char Reserved:7;	
} DSP_POWER_MGMT_CFG;

typedef struct {
	unsigned char LoadValue:8;	
} DSP_HBUS_TIMER_CFG;



#define DSP_ChipID                 0x80000000
#define DSP_MspBootDomain          0x80000580
#define DSP_LBusTimeoutDisable     0x80000580
#define DSP_ClockControl_1         0x8000058A
#define DSP_ClockControl_2         0x8000058C
#define DSP_ChipReset              0x80000588
#define DSP_GpioModeControl_15_8   0x80000082
#define DSP_GpioDriverEnable_15_8  0x80000076
#define DSP_GpioOutputData_15_8    0x80000072

typedef struct {
	unsigned short NMI:1;	
	unsigned short Halt:1;	
	unsigned short ResetCore:1;	
	unsigned short Reserved:13;	
} DSP_BOOT_DOMAIN;

typedef struct {
	unsigned short DisableTimeout:1;	
	unsigned short Reserved:15;	
} DSP_LBUS_TIMEOUT_DISABLE;

typedef struct {
	unsigned short Memory:1;	
	unsigned short SerialPort1:1;	
	unsigned short SerialPort2:1;	
	unsigned short SerialPort3:1;	
	unsigned short Gpio:1;	
	unsigned short Dma:1;	
	unsigned short SoundBlaster:1;	
	unsigned short Uart:1;	
	unsigned short Midi:1;	
	unsigned short IsaMaster:1;	
	unsigned short Reserved:6;	
} DSP_CHIP_RESET;

typedef struct {
	unsigned short N_Divisor:6;	
	unsigned short Reserved1:2;	
	unsigned short M_Multiplier:6;	
	unsigned short Reserved2:2;	
} DSP_CLOCK_CONTROL_1;

typedef struct {
	unsigned short PllBypass:1;	
	unsigned short Reserved:15;	
} DSP_CLOCK_CONTROL_2;

typedef struct {
	unsigned short Latch8:1;
	unsigned short Latch9:1;
	unsigned short Latch10:1;
	unsigned short Latch11:1;
	unsigned short Latch12:1;
	unsigned short Latch13:1;
	unsigned short Latch14:1;
	unsigned short Latch15:1;
	unsigned short Mask8:1;
	unsigned short Mask9:1;
	unsigned short Mask10:1;
	unsigned short Mask11:1;
	unsigned short Mask12:1;
	unsigned short Mask13:1;
	unsigned short Mask14:1;
	unsigned short Mask15:1;
} DSP_GPIO_OUTPUT_DATA_15_8;

typedef struct {
	unsigned short Enable8:1;
	unsigned short Enable9:1;
	unsigned short Enable10:1;
	unsigned short Enable11:1;
	unsigned short Enable12:1;
	unsigned short Enable13:1;
	unsigned short Enable14:1;
	unsigned short Enable15:1;
	unsigned short Mask8:1;
	unsigned short Mask9:1;
	unsigned short Mask10:1;
	unsigned short Mask11:1;
	unsigned short Mask12:1;
	unsigned short Mask13:1;
	unsigned short Mask14:1;
	unsigned short Mask15:1;
} DSP_GPIO_DRIVER_ENABLE_15_8;

typedef struct {
	unsigned short GpioMode8:2;
	unsigned short GpioMode9:2;
	unsigned short GpioMode10:2;
	unsigned short GpioMode11:2;
	unsigned short GpioMode12:2;
	unsigned short GpioMode13:2;
	unsigned short GpioMode14:2;
	unsigned short GpioMode15:2;
} DSP_GPIO_MODE_15_8;

#define MW_ADC_MASK    0x0001
#define MW_AIC2_MASK   0x0006
#define MW_MIDI_MASK   0x0008
#define MW_CDDAC_MASK  0x8001
#define MW_AIC1_MASK   0xE006
#define MW_UART_MASK   0xE00A
#define MW_ACI_MASK    0xE00B

typedef struct _DSP_3780I_CONFIG_SETTINGS {

	
	unsigned short usBaseConfigIO;

	
	int bDSPEnabled;
	int bModemEnabled;
	int bInterruptClaimed;

	
	unsigned short usDspIrq;
	unsigned short usDspDma;
	unsigned short usDspBaseIO;
	unsigned short usUartIrq;
	unsigned short usUartBaseIO;

	
	int bDspIrqActiveLow;
	int bUartIrqActiveLow;
	int bDspIrqPulse;
	int bUartIrqPulse;

	
	unsigned uIps;
	unsigned uDStoreSize;
	unsigned uIStoreSize;
	unsigned uDmaBandwidth;

	
	unsigned short usNumTransfers;
	unsigned short usReRequest;
	int bEnableMEMCS16;
	unsigned short usIsaMemCmdWidth;
	int bGateIOCHRDY;
	int bEnablePwrMgmt;
	unsigned short usHBusTimerLoadValue;
	int bDisableLBusTimeout;
	unsigned short usN_Divisor;
	unsigned short usM_Multiplier;
	int bPllBypass;
	unsigned short usChipletEnable;	

	
	int bUartSaved;		
	unsigned char ucIER;	
	unsigned char ucFCR;	
	unsigned char ucLCR;	
	unsigned char ucMCR;	
	unsigned char ucSCR;	
	unsigned char ucDLL;	
	unsigned char ucDLM;	
} DSP_3780I_CONFIG_SETTINGS;


int dsp3780I_EnableDSP(DSP_3780I_CONFIG_SETTINGS * pSettings,
                       unsigned short *pIrqMap,
                       unsigned short *pDmaMap);
int dsp3780I_DisableDSP(DSP_3780I_CONFIG_SETTINGS * pSettings);
int dsp3780I_Reset(DSP_3780I_CONFIG_SETTINGS * pSettings);
int dsp3780I_Run(DSP_3780I_CONFIG_SETTINGS * pSettings);
int dsp3780I_ReadDStore(unsigned short usDspBaseIO, void __user *pvBuffer,
                        unsigned uCount, unsigned long ulDSPAddr);
int dsp3780I_ReadAndClearDStore(unsigned short usDspBaseIO,
                                void __user *pvBuffer, unsigned uCount,
                                unsigned long ulDSPAddr);
int dsp3780I_WriteDStore(unsigned short usDspBaseIO, void __user *pvBuffer,
                         unsigned uCount, unsigned long ulDSPAddr);
int dsp3780I_ReadIStore(unsigned short usDspBaseIO, void __user *pvBuffer,
                        unsigned uCount, unsigned long ulDSPAddr);
int dsp3780I_WriteIStore(unsigned short usDspBaseIO, void __user *pvBuffer,
                         unsigned uCount, unsigned long ulDSPAddr);
unsigned short dsp3780I_ReadMsaCfg(unsigned short usDspBaseIO,
                                   unsigned long ulMsaAddr);
void dsp3780I_WriteMsaCfg(unsigned short usDspBaseIO,
                          unsigned long ulMsaAddr, unsigned short usValue);
int dsp3780I_GetIPCSource(unsigned short usDspBaseIO,
                          unsigned short *pusIPCSource);

#define MKWORD(var) (*((unsigned short *)(&var)))
#define MKBYTE(var) (*((unsigned char *)(&var)))

#define WriteMsaCfg(addr,value) dsp3780I_WriteMsaCfg(usDspBaseIO,addr,value)
#define ReadMsaCfg(addr) dsp3780I_ReadMsaCfg(usDspBaseIO,addr)
#define WriteGenCfg(index,value) dsp3780I_WriteGenCfg(usDspBaseIO,index,value)
#define ReadGenCfg(index) dsp3780I_ReadGenCfg(usDspBaseIO,index)

#define InWordDsp(index)          inw(usDspBaseIO+index)
#define InByteDsp(index)          inb(usDspBaseIO+index)
#define OutWordDsp(index,value)   outw(value,usDspBaseIO+index)
#define OutByteDsp(index,value)   outb(value,usDspBaseIO+index)

#endif
