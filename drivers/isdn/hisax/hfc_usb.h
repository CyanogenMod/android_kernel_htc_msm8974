
#ifndef __HFC_USB_H__
#define __HFC_USB_H__

#define DRIVER_AUTHOR   "Peter Sprenger (sprenger@moving-byters.de)"
#define DRIVER_DESC     "HFC-S USB based HiSAX ISDN driver"


#define HFC_CTRL_TIMEOUT	20	
#define HFC_TIMER_T3		8000	
#define HFC_TIMER_T4		500	

#define HFCUSB_L1_STATECHANGE	0	
#define HFCUSB_L1_DRX		1	
#define HFCUSB_L1_ERX		2	
#define HFCUSB_L1_DTX		4	

#define MAX_BCH_SIZE		2048	

#define HFCUSB_RX_THRESHOLD	64	
#define HFCUSB_TX_THRESHOLD	64	

#define HFCUSB_CHIP_ID		0x16	
#define HFCUSB_CIRM		0x00	
#define HFCUSB_USB_SIZE		0x07	
#define HFCUSB_USB_SIZE_I	0x06	
#define HFCUSB_F_CROSS		0x0b	
#define HFCUSB_CLKDEL		0x37	
#define HFCUSB_CON_HDLC		0xfa	
#define HFCUSB_HDLC_PAR		0xfb
#define HFCUSB_SCTRL		0x31	
#define HFCUSB_SCTRL_E		0x32	
#define HFCUSB_SCTRL_R		0x33	
#define HFCUSB_F_THRES		0x0c	
#define HFCUSB_FIFO		0x0f	
#define HFCUSB_F_USAGE		0x1a	
#define HFCUSB_MST_MODE0	0x14
#define HFCUSB_MST_MODE1	0x15
#define HFCUSB_P_DATA		0x1f
#define HFCUSB_INC_RES_F	0x0e
#define HFCUSB_STATES		0x30

#define HFCUSB_CHIPID		0x40	


#define HFCUSB_NUM_FIFOS	8	
#define HFCUSB_B1_TX		0	
#define HFCUSB_B1_RX		1	
#define HFCUSB_B2_TX		2
#define HFCUSB_B2_RX		3
#define HFCUSB_D_TX		4
#define HFCUSB_D_RX		5
#define HFCUSB_PCM_TX		6
#define HFCUSB_PCM_RX		7

#define USB_INT		0
#define USB_BULK	1
#define USB_ISOC	2

#define ISOC_PACKETS_D	8
#define ISOC_PACKETS_B	8
#define ISO_BUFFER_SIZE	128

#define SINK_MAX	68
#define SINK_MIN	48
#define SINK_DMIN	12
#define SINK_DMAX	18
#define BITLINE_INF	(-64 * 8)

#define write_usb(a, b, c) usb_control_msg((a)->dev, (a)->ctrl_out_pipe, 0, 0x40, (c), (b), NULL, 0, HFC_CTRL_TIMEOUT)
#define read_usb(a, b, c) usb_control_msg((a)->dev, (a)->ctrl_in_pipe, 1, 0xC0, 0, (b), (c), 1, HFC_CTRL_TIMEOUT)
#define HFC_CTRL_BUFSIZE 32

typedef struct {
	__u8 hfc_reg;		
	__u8 reg_val;		/* value to be written (or read) */
	int action;		
} ctrl_buft;

#define HFCUSB_DBG_INIT		0x0001
#define HFCUSB_DBG_STATES	0x0002
#define HFCUSB_DBG_DCHANNEL	0x0080
#define HFCUSB_DBG_FIFO_ERR	0x4000
#define HFCUSB_DBG_VERBOSE_USB	0x8000

struct hfcusb_symbolic_list {
	const int num;
	const char *name;
};

static struct hfcusb_symbolic_list urb_errlist[] = {
	{-ENOMEM, "No memory for allocation of internal structures"},
	{-ENOSPC, "The host controller's bandwidth is already consumed"},
	{-ENOENT, "URB was canceled by unlink_urb"},
	{-EXDEV, "ISO transfer only partially completed"},
	{-EAGAIN, "Too match scheduled for the future"},
	{-ENXIO, "URB already queued"},
	{-EFBIG, "Too much ISO frames requested"},
	{-ENOSR, "Buffer error (overrun)"},
	{-EPIPE, "Specified endpoint is stalled (device not responding)"},
	{-EOVERFLOW, "Babble (bad cable?)"},
	{-EPROTO, "Bit-stuff error (bad cable?)"},
	{-EILSEQ, "CRC/Timeout"},
	{-ETIMEDOUT, "NAK (device does not respond)"},
	{-ESHUTDOWN, "Device unplugged"},
	{-1, NULL}
};



#define CNF_4INT3ISO	1	
#define CNF_3INT3ISO	2	
#define CNF_4ISO3ISO	3	
#define CNF_3ISO3ISO	4	

#define EP_NUL	1	
#define EP_NOP	2	
#define EP_ISO	3	
#define EP_BLK	4	
#define EP_INT	5	

static int validconf[][19] = {
	
	{EP_NUL, EP_INT, EP_NUL, EP_INT, EP_NUL, EP_INT, EP_NOP, EP_INT,
	 EP_ISO, EP_NUL, EP_ISO, EP_NUL, EP_ISO, EP_NUL, EP_NUL, EP_NUL,
	 CNF_4INT3ISO, 2, 1},
	{EP_NUL, EP_INT, EP_NUL, EP_INT, EP_NUL, EP_INT, EP_NUL, EP_NUL,
	 EP_ISO, EP_NUL, EP_ISO, EP_NUL, EP_ISO, EP_NUL, EP_NUL, EP_NUL,
	 CNF_3INT3ISO, 2, 0},
	
	{EP_NUL, EP_NUL, EP_NUL, EP_NUL, EP_NUL, EP_NUL, EP_NUL, EP_NUL,
	 EP_ISO, EP_ISO, EP_ISO, EP_ISO, EP_ISO, EP_ISO, EP_NOP, EP_ISO,
	 CNF_4ISO3ISO, 2, 1},
	{EP_NUL, EP_NUL, EP_NUL, EP_NUL, EP_NUL, EP_NUL, EP_NUL, EP_NUL,
	 EP_ISO, EP_ISO, EP_ISO, EP_ISO, EP_ISO, EP_ISO, EP_NUL, EP_NUL,
	 CNF_3ISO3ISO, 2, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	
};

#ifdef CONFIG_HISAX_DEBUG
static char *conf_str[] = {
	"4 Interrupt IN + 3 Isochron OUT",
	"3 Interrupt IN + 3 Isochron OUT",
	"4 Isochron IN + 3 Isochron OUT",
	"3 Isochron IN + 3 Isochron OUT"
};
#endif

typedef struct {
	int vendor;		
	int prod_id;		
	char *vend_name;	
	__u8 led_scheme;	
	signed short led_bits[8];	
} vendor_data;

#define LED_OFF		0	
#define LED_SCHEME1	1	
#define LED_SCHEME2	2	

#define LED_POWER_ON	1
#define LED_POWER_OFF	2
#define LED_S0_ON	3
#define LED_S0_OFF	4
#define LED_B1_ON	5
#define LED_B1_OFF	6
#define LED_B1_DATA	7
#define LED_B2_ON	8
#define LED_B2_OFF	9
#define LED_B2_DATA	10

#define LED_NORMAL	0	
#define LED_INVERTED	1	


#endif	
