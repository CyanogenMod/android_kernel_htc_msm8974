/******************************************************************************
 *    ixj.h
 *
 *
 * Device Driver for Quicknet Technologies, Inc.'s Telephony cards
 * including the Internet PhoneJACK, Internet PhoneJACK Lite,
 * Internet PhoneJACK PCI, Internet LineJACK, Internet PhoneCARD and
 * SmartCABLE
 *
 *    (c) Copyright 1999-2001  Quicknet Technologies, Inc.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version
 *    2 of the License, or (at your option) any later version.
 *
 * Author:          Ed Okerson, <eokerson@quicknet.net>
 *    
 * Contributors:    Greg Herlein, <gherlein@quicknet.net>
 *                  David W. Erhart, <derhart@quicknet.net>
 *                  John Sellers, <jsellers@quicknet.net>
 *                  Mike Preston, <mpreston@quicknet.net>
 *
 * More information about the hardware related to this driver can be found
 * at our website:    http://www.quicknet.net
 *
 * Fixes:
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF QUICKNET
 * TECHNOLOGIES, INC.HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * QUICKNET TECHNOLOGIES, INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND QUICKNET TECHNOLOGIES, INC. HAS NO OBLIGATION 
 * TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 *****************************************************************************/
#define IXJ_VERSION 3031

#include <linux/types.h>

#include <linux/ixjuser.h>
#include <linux/phonedev.h>

typedef __u16 WORD;
typedef __u32 DWORD;
typedef __u8 BYTE;

#ifndef IXJMAX
#define IXJMAX 16
#endif

typedef struct {
	unsigned char low;
	unsigned char high;
} BYTES;

typedef union {
        BYTES bytes;
        short word;
} IXJ_WORD;

typedef struct{
	unsigned int b0:1;
	unsigned int b1:1;
	unsigned int b2:1;
	unsigned int b3:1;
	unsigned int b4:1;
	unsigned int b5:1;
	unsigned int b6:1;
	unsigned int b7:1;
} IXJ_CBITS;

typedef union{
	IXJ_CBITS cbits;
	  char  cbyte;
} IXJ_CBYTE;

typedef struct {
	unsigned int rxrdy:1;
	unsigned int txrdy:1;
	unsigned int status:1;
	unsigned int auxstatus:1;
	unsigned int rxdma:1;
	unsigned int txdma:1;
	unsigned int rxburst:1;
	unsigned int txburst:1;
	unsigned int dmadir:1;
	unsigned int cont:1;
	unsigned int irqn:1;
	unsigned int t:5;
} HCRBIT;

typedef union {
	HCRBIT bits;
	BYTES bytes;
} HCR;

typedef struct {
	unsigned int controlrdy:1;
	unsigned int auxctlrdy:1;
	unsigned int statusrdy:1;
	unsigned int auxstatusrdy:1;
	unsigned int rxrdy:1;
	unsigned int txrdy:1;
	unsigned int restart:1;
	unsigned int irqn:1;
	unsigned int rxdma:1;
	unsigned int txdma:1;
	unsigned int cohostshutdown:1;
	unsigned int t:5;
} HSRBIT;

typedef union {
	HSRBIT bits;
	BYTES bytes;
} HSR;

typedef struct {
	unsigned int x:1;
	unsigned int gpio1:1;
	unsigned int gpio2:1;
	unsigned int gpio3:1;
	unsigned int gpio4:1;
	unsigned int gpio5:1;
	unsigned int gpio6:1;
	unsigned int gpio7:1;
	unsigned int xread:1;
	unsigned int gpio1read:1;
	unsigned int gpio2read:1;
	unsigned int gpio3read:1;
	unsigned int gpio4read:1;
	unsigned int gpio5read:1;
	unsigned int gpio6read:1;
	unsigned int gpio7read:1;
} GPIOBIT;

typedef union {
	GPIOBIT bits;
	BYTES bytes;
	unsigned short word;
} GPIO;

typedef struct {
	unsigned int digit:4;
	unsigned int cpf_valid:1;
	unsigned int dtmf_valid:1;
	unsigned int peak:1;
	unsigned int z:1;
	unsigned int f0:1;
	unsigned int f1:1;
	unsigned int f2:1;
	unsigned int f3:1;
	unsigned int frame:4;
} LMON;

typedef union {
	LMON bits;
	BYTES bytes;
} DTMF;

typedef struct {
	unsigned int z:7;
	unsigned int dtmf_en:1;
	unsigned int y:4;
	unsigned int F3:1;
	unsigned int F2:1;
	unsigned int F1:1;
	unsigned int F0:1;
} CP;

typedef union {
	CP bits;
	BYTES bytes;
} CPTF;

typedef struct {
	unsigned int c0:1;
	unsigned int c1:1;
	unsigned int stereo:1;
	unsigned int daafsyncen:1;
	unsigned int led1:1;
	unsigned int led2:1;
	unsigned int led3:1;
	unsigned int led4:1;
} PSCRWI;			

typedef struct {
	unsigned int eidp:1;
	unsigned int eisd:1;
	unsigned int x:6;
} PSCRWP;			

typedef union {
	PSCRWI bits;
	PSCRWP pcib;
	char byte;
} PLD_SCRW;

typedef struct {
	unsigned int c0:1;
	unsigned int c1:1;
	unsigned int x:1;
	unsigned int d0ee:1;
	unsigned int mixerbusy:1;
	unsigned int sci:1;
	unsigned int dspflag:1;
	unsigned int daaflag:1;
} PSCRRI;

typedef struct {
	unsigned int eidp:1;
	unsigned int eisd:1;
	unsigned int x:4;
	unsigned int dspflag:1;
	unsigned int det:1;
} PSCRRP;

typedef union {
	PSCRRI bits;
	PSCRRP pcib;
	char byte;
} PLD_SCRR;

typedef struct {
	unsigned int c1:1;
	unsigned int c2:1;
	unsigned int c3:1;
	unsigned int b2en:1;
	unsigned int spken:1;
	unsigned int rly1:1;
	unsigned int rly2:1;
	unsigned int rly3:1;
} PSLICWRITE;

typedef struct {
	unsigned int state:3;
	unsigned int b2en:1;
	unsigned int spken:1;
	unsigned int c3:1;
	unsigned int potspstn:1;
	unsigned int det:1;
} PSLICREAD;

typedef struct {
	unsigned int c1:1;
	unsigned int c2:1;
	unsigned int c3:1;
	unsigned int b2en:1;
	unsigned int e1:1;
	unsigned int mic:1;
	unsigned int spk:1;
	unsigned int x:1;
} PSLICPCI;

typedef union {
	PSLICPCI pcib;
	PSLICWRITE bits;
	PSLICREAD slic;
	char byte;
} PLD_SLICW;

typedef union {
	PSLICPCI pcib;
	PSLICREAD bits;
	char byte;
} PLD_SLICR;

typedef struct {
	unsigned int clk0:1;
	unsigned int clk1:1;
	unsigned int clk2:1;
	unsigned int x0:1;
	unsigned int slic_e1:1;
	unsigned int x1:1;
	unsigned int x2:1;
	unsigned int x3:1;
} PCLOCK;

typedef union {
	PCLOCK bits;
	char byte;
} PLD_CLOCK;


typedef struct {
	unsigned short vol[10];
	unsigned int recsrc;
	unsigned int modcnt;
	unsigned short micpreamp;
} MIX;

typedef struct {
	unsigned int x0:4;	

	unsigned int ed:1;	

	unsigned int drf:1;	

	unsigned int dspf:1;	

	unsigned int crr:1;	

} COMMAND_REG1;

typedef union {
	COMMAND_REG1 bits;
	unsigned char byte;
} PCMCIA_CR1;

typedef struct {
	unsigned int x0:4;	

	unsigned int rstc:1;	

	unsigned int pwr:1;	

	unsigned int x1:2;	

} COMMAND_REG2;

typedef union {
	COMMAND_REG2 bits;
	unsigned char byte;
} PCMCIA_CR2;

typedef struct {
	unsigned int addr:5;	

	unsigned int rw:1;	

	unsigned int dev:2;	

} CONTROL_REG;

typedef union {
	CONTROL_REG bits;
	unsigned char byte;
} PCMCIA_SCCR;

typedef struct {
	unsigned int hsw:1;
	unsigned int det:1;
	unsigned int led2:1;
	unsigned int led1:1;
	unsigned int ring1:1;
	unsigned int ring0:1;
	unsigned int x:1;
	unsigned int powerdown:1;
} PCMCIA_SLIC_REG;

typedef union {
	PCMCIA_SLIC_REG bits;
	unsigned char byte;
} PCMCIA_SLIC;

typedef struct {
	unsigned int cpd:1;	

	unsigned int mpd:1;	

	unsigned int hpd:1;	

	unsigned int lpd:1;	

	unsigned int spd:1;	

	unsigned int x:2;	

	unsigned int sr:1;	

} Si3CONTROL1;

typedef union {
	Si3CONTROL1 bits;
	unsigned char byte;
} Si3C1;

typedef struct {
	unsigned int al:1;	

	unsigned int dl2:1;	

	unsigned int dl1:1;	

	unsigned int pll:1;	

	unsigned int hpd:1;	

	unsigned int x:3;	

} Si3CONTROL2;

typedef union {
	Si3CONTROL2 bits;
	unsigned char byte;
} Si3C2;

typedef struct {
	unsigned int iir:1;	

	unsigned int him:1;	

	unsigned int mcm:1;	

	unsigned int mcg:2;	

	unsigned int lim:1;	

	unsigned int lig:2;	

} Si3RXGAIN;

typedef union {
	Si3RXGAIN bits;
	unsigned char byte;
} Si3RXG;

typedef struct {
	unsigned int hom:1;	

	unsigned int lom:1;	

	unsigned int rxg:5;	

	unsigned int x:1;	

} Si3ADCVOLUME;

typedef union {
	Si3ADCVOLUME bits;
	unsigned char byte;
} Si3ADC;

typedef struct {
	unsigned int srm:1;	

	unsigned int slm:1;	

	unsigned int txg:5;	

	unsigned int x:1;	

} Si3DACVOLUME;

typedef union {
	Si3DACVOLUME bits;
	unsigned char byte;
} Si3DAC;

typedef struct {
	unsigned int x:5;	

	unsigned int losc:1;	

	unsigned int srsc:1;	

	unsigned int slsc:1;	

} Si3STATUSREPORT;

typedef union {
	Si3STATUSREPORT bits;
	unsigned char byte;
} Si3STAT;

typedef struct {
	unsigned int sot:2;	

	unsigned int lot:2;	

	unsigned int x:4;	

} Si3ANALOGATTN;

typedef union {
	Si3ANALOGATTN bits;
	unsigned char byte;
} Si3AATT;


typedef struct _DAA_REGS {
	
	
	
	BYTE bySOP;

	union _SOP_REGS {
		struct _SOP {
			union	
			 {
				BYTE reg;
				struct _CR0_BITREGS {
					BYTE CLK_EXT:1;		

					BYTE RIP:1;	

					BYTE AR:1;	

					BYTE AX:1;	

					BYTE FRR:1;	

					BYTE FRX:1;	

					BYTE IM:1;	

					BYTE TH:1;	

				} bitreg;
			} cr0;

			union	
			 {
				BYTE reg;
				struct _CR1_REGS {
					BYTE RM:1;	

					BYTE RMR:1;	

					BYTE No_auto:1;		

					BYTE Pulse:1;	

					BYTE P_Tone1:1;		

					BYTE P_Tone2:1;		

					BYTE E_Tone1:1;		

					BYTE E_Tone2:1;		

				} bitreg;
			} cr1;

			union	
			 {
				BYTE reg;
				struct _CR2_REGS {
					BYTE Call_II:1;		

					BYTE Call_I:1;	

					BYTE Call_en:1;		

					BYTE Call_pon:1;	

					BYTE IDR:1;	

					BYTE COT_R:3;	

				} bitreg;
			} cr2;

			union	
			 {
				BYTE reg;
				struct _CR3_REGS {
					BYTE DHP_X:1;	

					BYTE DHP_R:1;	

					BYTE Cal_pctl:1;	

					BYTE SEL:1;	

					BYTE TestLoops:4;	

				} bitreg;
			} cr3;

			union	
			 {
				BYTE reg;
				struct _CR4_REGS {
					BYTE Fsc_en:1;	

					BYTE Int_en:1;	

					BYTE AGX:2;	

					BYTE AGR_R:2;	

					BYTE AGR_Z:2;	

				} bitreg;
			} cr4;

			union	
			 {
				BYTE reg;
				struct _CR5_REGS {
					BYTE V_0:1;	

					BYTE V_1:1;	

					BYTE V_2:1;	

					BYTE V_3:1;	

					BYTE V_4:1;	

					BYTE V_5:1;	

					BYTE V_6:1;	

					BYTE V_7:1;	

				} bitreg;
			} cr5;

			union	
			 {
				BYTE reg;
				struct _CR6_REGS {
					BYTE reserved:8;	

				} bitreg;
			} cr6;

			union	
			 {
				BYTE reg;
				struct _CR7_REGS {
					BYTE reserved:8;	

				} bitreg;
			} cr7;
		} SOP;

		BYTE ByteRegs[sizeof(struct _SOP)];

	} SOP_REGS;

	
	
	
	

	
	
	
	BYTE byXOP;

	union _XOP_REGS {
		struct _XOP {
			union	XOPXR0
			 {
				BYTE reg;
				struct _XR0_BITREGS {
					BYTE SI_0:1;	

					BYTE SI_1:1;	

					BYTE VDD_OK:1;	

					BYTE Caller_ID:1;	

					BYTE RING:1;	

					BYTE Cadence:1;		

					BYTE Wake_up:1;		

					BYTE RMR:1;	

				} bitreg;
			} xr0;

			union	
			 {
				BYTE reg;
				struct _XR1_BITREGS {
					BYTE M_SI_0:1;	

					BYTE M_SI_1:1;	

					BYTE M_VDD_OK:1;	

					BYTE M_Caller_ID:1;	

					BYTE M_RING:1;	

					BYTE M_Cadence:1;	

					BYTE M_Wake_up:1;	

					BYTE unused:1;	

				} bitreg;
			} xr1;

			union	
			 {
				BYTE reg;
				struct _XR2_BITREGS {
					BYTE CTO0:1;	

					BYTE CTO1:1;	

					BYTE CTO2:1;	

					BYTE CTO3:1;	

					BYTE CTO4:1;	

					BYTE CTO5:1;	

					BYTE CTO6:1;	

					BYTE CTO7:1;	

				} bitreg;
			} xr2;

			union	
			 {
				BYTE reg;
				struct _XR3_BITREGS {
					BYTE DCR0:1;	

					BYTE DCR1:1;	

					BYTE DCI:1;	

					BYTE DCU0:1;	

					BYTE DCU1:1;	

					BYTE B_off:1;	

					BYTE AGB0:1;	

					BYTE AGB1:1;	

				} bitreg;
			} xr3;

			union	
			 {
				BYTE reg;
				struct _XR4_BITREGS {
					BYTE C_0:1;	

					BYTE C_1:1;	

					BYTE C_2:1;	

					BYTE C_3:1;	

					BYTE C_4:1;	

					BYTE C_5:1;	

					BYTE C_6:1;	

					BYTE C_7:1;	

				} bitreg;
			} xr4;

			union	
			 {
				BYTE reg;
				struct _XR5_BITREGS {
					BYTE T_0:1;	

					BYTE T_1:1;	

					BYTE T_2:1;	

					BYTE T_3:1;	

					BYTE T_4:1;	

					BYTE T_5:1;	

					BYTE T_6:1;	

					BYTE T_7:1;	

				} bitreg;
			} xr5;

			union	
			 {
				BYTE reg;
				struct _XR6_BITREGS {
					BYTE CPS0:1;	

					BYTE CPS1:1;	

					BYTE unused1:2;		

					BYTE CLK_OFF:1;		

					BYTE unused2:3;		

				} bitreg;
			} xr6;

			union	
			 {
				BYTE reg;
				struct _XR7_BITREGS {
					BYTE unused1:1;		

					BYTE Vdd0:1;	

					BYTE Vdd1:1;	

					BYTE unused2:5;		

				} bitreg;
			} xr7;
		} XOP;

		BYTE ByteRegs[sizeof(struct _XOP)];

	} XOP_REGS;

	
	
	
	

	
	
	
	BYTE byCOP;

	union _COP_REGS {
		struct _COP {
			BYTE THFilterCoeff_1[8];	

			BYTE THFilterCoeff_2[8];	

			BYTE THFilterCoeff_3[8];	

			BYTE RingerImpendance_1[8];	

			BYTE IMFilterCoeff_1[8];	

			BYTE IMFilterCoeff_2[8];	

			BYTE RingerImpendance_2[8];	

			BYTE FRRFilterCoeff[8];		

			BYTE FRXFilterCoeff[8];		

			BYTE ARFilterCoeff[4];	

			BYTE AXFilterCoeff[4];	

			BYTE Tone1Coeff[4];	

			BYTE Tone2Coeff[4];	

			BYTE LevelmeteringRinging[4];	

			BYTE CallerID1stTone[8];	

			BYTE CallerID2ndTone[8];	

		} COP;

		BYTE ByteRegs[sizeof(struct _COP)];

	} COP_REGS;

	
	
	
	

	
	
	
	BYTE byCAO;

	union _CAO_REGS {
		struct _CAO {
			BYTE CallerID[512];	

		} CAO;

		BYTE ByteRegs[sizeof(struct _CAO)];
	} CAO_REGS;

	union			
	 {
		BYTE reg;
		struct _XR0_BITREGSW {
			BYTE SO_0:1;	

			BYTE SO_1:1;	

			BYTE SO_2:1;	

			BYTE unused:5;	

		} bitreg;
	} XOP_xr0_W;

	union			
	 {
		BYTE reg;
		struct _XR6_BITREGSW {
			BYTE unused1:4;		

			BYTE CLK_OFF:1;		

			BYTE unused2:3;		

		} bitreg;
	} XOP_xr6_W;

} DAA_REGS;

#define ALISDAA_ID_BYTE      0x81
#define ALISDAA_CALLERID_SIZE  512


#define SOP_PU_SLEEP    0
#define SOP_PU_RINGING    1
#define SOP_PU_CONVERSATION  2
#define SOP_PU_PULSEDIALING  3
#define SOP_PU_RESET    4

#define ALISDAA_CALLERID_SIZE 512

#define PLAYBACK_MODE_COMPRESSED	0	
#define PLAYBACK_MODE_TRUESPEECH_V40	0	
#define PLAYBACK_MODE_TRUESPEECH	8	
#define PLAYBACK_MODE_ULAW		2	
#define PLAYBACK_MODE_ALAW		10	
#define PLAYBACK_MODE_16LINEAR		6	
#define PLAYBACK_MODE_8LINEAR		4	
#define PLAYBACK_MODE_8LINEAR_WSS	5	

#define RECORD_MODE_COMPRESSED		0	
#define RECORD_MODE_TRUESPEECH		0	
#define RECORD_MODE_ULAW		4	
#define RECORD_MODE_ALAW		12	
#define RECORD_MODE_16LINEAR		5	
#define RECORD_MODE_8LINEAR		6	
#define RECORD_MODE_8LINEAR_WSS		7	

enum SLIC_STATES {
	PLD_SLIC_STATE_OC = 0,
	PLD_SLIC_STATE_RINGING,
	PLD_SLIC_STATE_ACTIVE,
	PLD_SLIC_STATE_OHT,
	PLD_SLIC_STATE_TIPOPEN,
	PLD_SLIC_STATE_STANDBY,
	PLD_SLIC_STATE_APR,
	PLD_SLIC_STATE_OHTPR
};

enum SCI_CONTROL {
	SCI_End = 0,
	SCI_Enable_DAA,
	SCI_Enable_Mixer,
	SCI_Enable_EEPROM
};

enum Mode {
	T63, T53, T48, T40
};
enum Dir {
	V3_TO_V4, V4_TO_V3, V4_TO_V5, V5_TO_V4
};

typedef struct Proc_Info_Tag {
	enum Mode convert_mode;
	enum Dir convert_dir;
	int Prev_Frame_Type;
	int Current_Frame_Type;
} Proc_Info_Type;

enum PREVAL {
	NORMAL = 0,
	NOPOST,
	POSTONLY,
	PREERROR
};

enum IXJ_EXTENSIONS {
	G729LOADER = 0,
	TS85LOADER,
	PRE_READ,
	POST_READ,
	PRE_WRITE,
	POST_WRITE,
	PRE_IOCTL,
	POST_IOCTL
};

typedef struct {
	char enable;
	char en_filter;
	unsigned int filter;
	unsigned int state;	

	unsigned int on1;	

	unsigned long on1min;	
 	unsigned long on1dot;	

	unsigned long on1max;	

	unsigned int off1;	

	unsigned long off1min;
 	unsigned long off1dot;	
	unsigned long off1max;
	unsigned int on2;	

	unsigned long on2min;
	unsigned long on2dot;
	unsigned long on2max;
	unsigned int off2;	

	unsigned long off2min;
 	unsigned long off2dot;	
	unsigned long off2max;
	unsigned int on3;	

	unsigned long on3min;
	unsigned long on3dot;
	unsigned long on3max;
	unsigned int off3;	

	unsigned long off3min;
 	unsigned long off3dot;	
	unsigned long off3max;
} IXJ_CADENCE_F;

typedef struct {
	unsigned int busytone:1;
	unsigned int dialtone:1;
	unsigned int ringback:1;
	unsigned int ringing:1;
	unsigned int playing:1;
	unsigned int recording:1;
	unsigned int cringing:1;
	unsigned int play_first_frame:1;
	unsigned int pstn_present:1;
	unsigned int pstn_ringing:1;
	unsigned int pots_correct:1;
	unsigned int pots_pstn:1;
	unsigned int g729_loaded:1;
	unsigned int ts85_loaded:1;
	unsigned int dtmf_oob:1;	

	unsigned int pcmciascp:1;	

	unsigned int pcmciasct:2;	

	unsigned int pcmciastate:3;	

	unsigned int inwrite:1;	

	unsigned int inread:1;	

	unsigned int incheck:1;	

	unsigned int cidplay:1; 

	unsigned int cidring:1; 

	unsigned int cidsent:1; 

	unsigned int cidcw_ack:1; 
	unsigned int firstring:1; 
	unsigned int pstncheck:1;	
	unsigned int pstn_rmr:1;
	unsigned int x:3;	

} IXJ_FLAGS;


typedef struct {
	int elements_used;
	IXJ_CADENCE_TERM termination;
	IXJ_CADENCE_ELEMENT *ce;
} ixj_cadence;

typedef struct {
	struct phone_device p;
	struct timer_list timer;
	unsigned int board;
	unsigned int DSPbase;
	unsigned int XILINXbase;
	unsigned int serial;
	atomic_t DSPWrite;
	struct phone_capability caplist[30];
	unsigned int caps;
	struct pnp_dev *dev;
	unsigned int cardtype;
	unsigned int rec_codec;
	unsigned int cid_rec_codec;
	unsigned int cid_rec_volume;
	unsigned char cid_rec_flag;
	signed char rec_mode;
	unsigned int play_codec;
	unsigned int cid_play_codec;
	unsigned int cid_play_volume;
	unsigned char cid_play_flag;
	signed char play_mode;
	IXJ_FLAGS flags;
	unsigned long busyflags;
	unsigned int rec_frame_size;
	unsigned int play_frame_size;
	unsigned int cid_play_frame_size;
	unsigned int cid_base_frame_size;
	unsigned long cidcw_wait;
	int aec_level;
	int cid_play_aec_level;
	int readers, writers;
        wait_queue_head_t poll_q;
        wait_queue_head_t read_q;
	char *read_buffer, *read_buffer_end;
	char *read_convert_buffer;
	size_t read_buffer_size;
	unsigned int read_buffer_ready;
        wait_queue_head_t write_q;
	char *write_buffer, *write_buffer_end;
	char *write_convert_buffer;
	size_t write_buffer_size;
	unsigned int write_buffers_empty;
	unsigned long drybuffer;
	char *write_buffer_rp, *write_buffer_wp;
	char dtmfbuffer[80];
	char dtmf_current;
	int dtmf_wp, dtmf_rp, dtmf_state, dtmf_proc;
	int tone_off_time, tone_on_time;
	struct fasync_struct *async_queue;
	unsigned long tone_start_jif;
	char tone_index;
	char tone_state;
	char maxrings;
	ixj_cadence *cadence_t;
	ixj_cadence *cadence_r;
	int tone_cadence_state;
	IXJ_CADENCE_F cadence_f[6];
	DTMF dtmf;
	CPTF cptf;
	BYTES dsp;
	BYTES ver;
	BYTES scr;
	BYTES ssr;
	BYTES baseframe;
	HSR hsr;
	GPIO gpio;
	PLD_SCRR pld_scrr;
	PLD_SCRW pld_scrw;
	PLD_SLICW pld_slicw;
	PLD_SLICR pld_slicr;
	PLD_CLOCK pld_clock;
	PCMCIA_CR1 pccr1;
	PCMCIA_CR2 pccr2;
	PCMCIA_SCCR psccr;
	PCMCIA_SLIC pslic;
	char pscdd;
	Si3C1 sic1;
	Si3C2 sic2;
	Si3RXG sirxg;
	Si3ADC siadc;
	Si3DAC sidac;
	Si3STAT sistat;
	Si3AATT siaatt;
	MIX mix;
	unsigned short ring_cadence;
	int ring_cadence_t;
	unsigned long ring_cadence_jif;
	unsigned long checkwait;
	int intercom;
	int m_hook;
	int r_hook;
	int p_hook;
	char pstn_envelope;
	char pstn_cid_intr;
	unsigned char fskz;
	unsigned char fskphase;
	unsigned char fskcnt;
        unsigned int cidsize;
	unsigned int cidcnt;
	unsigned long pstn_cid_received;
	PHONE_CID cid;
	PHONE_CID cid_send;
	unsigned long pstn_ring_int;
	unsigned long pstn_ring_start;
	unsigned long pstn_ring_stop;
	unsigned long pstn_winkstart;
	unsigned long pstn_last_rmr;
	unsigned long pstn_prev_rmr;
	unsigned long pots_winkstart;
	unsigned int winktime;
	unsigned long flash_end;
	char port;
	char hookstate;
	union telephony_exception ex;
	union telephony_exception ex_sig;
	int ixj_signals[35];
	IXJ_SIGDEF sigdef;
	char daa_mode;
	char daa_country;
	unsigned long pstn_sleeptil;
	DAA_REGS m_DAAShadowRegs;
	Proc_Info_Type Info_read;
	Proc_Info_Type Info_write;
	unsigned short frame_count;
	unsigned int filter_hist[4];
	unsigned char filter_en[6];
	unsigned short proc_load;
	unsigned long framesread;
	unsigned long frameswritten;
	unsigned long read_wait;
	unsigned long write_wait;
	unsigned long timerchecks;
	unsigned long txreadycheck;
	unsigned long rxreadycheck;
	unsigned long statuswait;
	unsigned long statuswaitfail;
	unsigned long pcontrolwait;
	unsigned long pcontrolwaitfail;
	unsigned long iscontrolready;
	unsigned long iscontrolreadyfail;
	unsigned long pstnstatecheck;
#ifdef IXJ_DYN_ALLOC
	short *fskdata;
#else
	short fskdata[8000];
#endif
	int fsksize;
	int fskdcnt;
} IXJ;

typedef int (*IXJ_REGFUNC) (IXJ * j, unsigned long arg);

extern IXJ *ixj_pcmcia_probe(unsigned long, unsigned long);

