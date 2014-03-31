
#define CMDR	0
#define DATR	2
#define CTLR	4
#define	STATR	5
#define	IRQSTAT	12

#define	CMD_WE	0x80
#define	CMD_RF	0x40
#define	DAT_WE	0x20
#define	DAT_RF	0x10

#define	IRQ_REQ	0x08
#define	DMA1	0x04
#define	DMA0	0x02

#define	CMD_WEIE	0x80
#define	CMD_RFIE	0x40
#define	DAT_WEIE	0x20
#define	DAT_RFIE	0x10

#define	RESET	0x08
#define	DMA1_IE	0x04
#define	DMA0_IE	0x02
#define	IRQ_ACK	0x01


#define	WACMD_SYSTEMID		0x00
#define WACMD_GETREV		0x00
#define	WACMD_INPUTFORMAT	0x10	
#define	WACMD_INPUTCHANNELS	0x11	
#define	WACMD_INPUTSPEED	0x12	
#define	WACMD_INPUTDMA		0x13	
#define	WACMD_INPUTSIZE		0x14	
#define	WACMD_INPUTSTART	0x15	
#define	WACMD_INPUTPAUSE	0x16	
#define	WACMD_INPUTSTOP		0x17	
#define	WACMD_INPUTRESUME	0x18	
#define	WACMD_INPUTPIO		0x19	

#define	WACMD_OUTPUTFORMAT	0x20	
#define	WACMD_OUTPUTCHANNELS	0x21	
#define	WACMD_OUTPUTSPEED	0x22	
#define	WACMD_OUTPUTDMA		0x23	
#define	WACMD_OUTPUTSIZE	0x24	
#define	WACMD_OUTPUTSTART	0x25	
#define	WACMD_OUTPUTPAUSE	0x26	
#define	WACMD_OUTPUTSTOP	0x27	
#define	WACMD_OUTPUTRESUME	0x28	
#define	WACMD_OUTPUTPIO		0x29	

#define	WACMD_GET_LEVEL		0x30
#define	WACMD_SET_LEVEL		0x31
#define	WACMD_SET_MIXER		0x32
#define	WACMD_RST_MIXER		0x33
#define	WACMD_SET_MONO		0x34

#define ADC_MUX_NONE	0
#define ADC_MUX_MIXER	1
#define ADC_MUX_LINE	2
#define ADC_MUX_AUX2	3
#define ADC_MUX_AUX1	4
#define ADC_MUX_MIC	5

#define MIX_GAIN_LINE	0	
#define MIX_GAIN_AUX1	1	
#define MIX_GAIN_AUX2	2	
#define MIX_GAIN_XMIC	3	
#define MIX_GAIN_MIC	4	
#define MIX_GAIN_PREMIC	5	
#define MIX_GAIN_OUT	6	
#define MIX_GAIN_MONO	7	

int wa_sendcmd(unsigned int cmd);
int wa_writecmd(unsigned int cmd, unsigned int arg);
