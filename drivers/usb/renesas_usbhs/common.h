/*
 * Renesas USB driver
 *
 * Copyright (C) 2011 Renesas Solutions Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef RENESAS_USB_DRIVER_H
#define RENESAS_USB_DRIVER_H

#include <linux/platform_device.h>
#include <linux/usb/renesas_usbhs.h>

struct usbhs_priv;

#include "./mod.h"
#include "./pipe.h"

#define SYSCFG		0x0000
#define BUSWAIT		0x0002
#define DVSTCTR		0x0008
#define TESTMODE	0x000C
#define CFIFO		0x0014
#define CFIFOSEL	0x0020
#define CFIFOCTR	0x0022
#define D0FIFO		0x0100
#define D0FIFOSEL	0x0028
#define D0FIFOCTR	0x002A
#define D1FIFO		0x0120
#define D1FIFOSEL	0x002C
#define D1FIFOCTR	0x002E
#define INTENB0		0x0030
#define INTENB1		0x0032
#define BRDYENB		0x0036
#define NRDYENB		0x0038
#define BEMPENB		0x003A
#define INTSTS0		0x0040
#define INTSTS1		0x0042
#define BRDYSTS		0x0046
#define NRDYSTS		0x0048
#define BEMPSTS		0x004A
#define FRMNUM		0x004C
#define USBREQ		0x0054	
#define USBVAL		0x0056	
#define USBINDX		0x0058	
#define USBLENG		0x005A	
#define DCPCFG		0x005C
#define DCPMAXP		0x005E
#define DCPCTR		0x0060
#define PIPESEL		0x0064
#define PIPECFG		0x0068
#define PIPEBUF		0x006A
#define PIPEMAXP	0x006C
#define PIPEPERI	0x006E
#define PIPEnCTR	0x0070
#define PIPE1TRE	0x0090
#define PIPE1TRN	0x0092
#define PIPE2TRE	0x0094
#define PIPE2TRN	0x0096
#define PIPE3TRE	0x0098
#define PIPE3TRN	0x009A
#define PIPE4TRE	0x009C
#define PIPE4TRN	0x009E
#define PIPE5TRE	0x00A0
#define PIPE5TRN	0x00A2
#define PIPEBTRE	0x00A4
#define PIPEBTRN	0x00A6
#define PIPECTRE	0x00A8
#define PIPECTRN	0x00AA
#define PIPEDTRE	0x00AC
#define PIPEDTRN	0x00AE
#define PIPEETRE	0x00B0
#define PIPEETRN	0x00B2
#define PIPEFTRE	0x00B4
#define PIPEFTRN	0x00B6
#define PIPE9TRE	0x00B8
#define PIPE9TRN	0x00BA
#define PIPEATRE	0x00BC
#define PIPEATRN	0x00BE
#define DEVADD0		0x00D0 
#define DEVADD1		0x00D2
#define DEVADD2		0x00D4
#define DEVADD3		0x00D6
#define DEVADD4		0x00D8
#define DEVADD5		0x00DA
#define DEVADD6		0x00DC
#define DEVADD7		0x00DE
#define DEVADD8		0x00E0
#define DEVADD9		0x00E2
#define DEVADDA		0x00E4

#define SCKE	(1 << 10)	
#define HSE	(1 << 7)	
#define DCFM	(1 << 6)	
#define DRPD	(1 << 5)	
#define DPRPU	(1 << 4)	
#define USBE	(1 << 0)	

#define EXTLP	(1 << 10)	
#define PWEN	(1 << 9)	
#define USBRST	(1 << 6)	
#define UACT	(1 << 4)	
#define RHST	(0x7)		
#define  RHST_LOW_SPEED  1	
#define  RHST_FULL_SPEED 2	
#define  RHST_HIGH_SPEED 3	

#define DREQE	(1 << 12)	
#define MBW_32	(0x2 << 10)	

#define BVAL	(1 << 15)	
#define BCLR	(1 << 14)	
#define FRDY	(1 << 13)	
#define DTLN_MASK (0x0FFF)	

#define VBSE	(1 << 15)	
#define RSME	(1 << 14)	
#define SOFE	(1 << 13)	
#define DVSE	(1 << 12)	
#define CTRE	(1 << 11)	
#define BEMPE	(1 << 10)	
#define NRDYE	(1 << 9)	
#define BRDYE	(1 << 8)	

#define BCHGE	(1 << 14)	
#define DTCHE	(1 << 12)	
#define ATTCHE	(1 << 11)	
#define EOFERRE	(1 << 6)	
#define SIGNE	(1 << 5)	
#define SACKE	(1 << 4)	

#define VBINT	(1 << 15)	
#define DVST	(1 << 12)	
#define CTRT	(1 << 11)	
#define BEMP	(1 << 10)	
#define BRDY	(1 << 8)	
#define VBSTS	(1 << 7)	
#define VALID	(1 << 3)	

#define DVSQ_MASK		(0x3 << 4)	
#define  POWER_STATE		(0 << 4)
#define  DEFAULT_STATE		(1 << 4)
#define  ADDRESS_STATE		(2 << 4)
#define  CONFIGURATION_STATE	(3 << 4)

#define CTSQ_MASK		(0x7)	
#define  IDLE_SETUP_STAGE	0	
#define  READ_DATA_STAGE	1	
#define  READ_STATUS_STAGE	2	
#define  WRITE_DATA_STAGE	3	
#define  WRITE_STATUS_STAGE	4	
#define  NODATA_STATUS_STAGE	5	
#define  SEQUENCE_ERROR		6	

#define OVRCR	(1 << 15) 
#define BCHG	(1 << 14) 
#define DTCH	(1 << 12) 
#define ATTCH	(1 << 11) 
#define EOFERR	(1 << 6)  
#define SIGN	(1 << 5)  
#define SACK	(1 << 4)  

#define TYPE_NONE	(0 << 14)	
#define TYPE_BULK	(1 << 14)
#define TYPE_INT	(2 << 14)
#define TYPE_ISO	(3 << 14)
#define DBLB		(1 << 9)	
#define SHTNAK		(1 << 7)	
#define DIR_OUT		(1 << 4)	

#define DEVSEL_MASK	(0xF << 12)	
#define DCP_MAXP_MASK	(0x7F)
#define PIPE_MAXP_MASK	(0x7FF)

#define BUFSIZE_SHIFT	10
#define BUFSIZE_MASK	(0x1F << BUFSIZE_SHIFT)
#define BUFNMB_MASK	(0xFF)

#define BSTS		(1 << 15)	
#define SUREQ		(1 << 14)	
#define CSSTS		(1 << 12)	
#define	ACLRM		(1 << 9)	
#define SQCLR		(1 << 8)	
#define SQSET		(1 << 7)	
#define PBUSY		(1 << 5)	
#define PID_MASK	(0x3)		
#define  PID_NAK	0
#define  PID_BUF	1
#define  PID_STALL10	2
#define  PID_STALL11	3

#define CCPL		(1 << 2)	

#define TRENB		(1 << 9)	
#define TRCLR		(1 << 8)	

#define FRNM_MASK	(0x7FF)

#define UPPHUB(x)	(((x) & 0xF) << 11)	
#define HUBPORT(x)	(((x) & 0x7) << 8)	
#define USBSPD(x)	(((x) & 0x3) << 6)	
#define USBSPD_SPEED_LOW	0x1
#define USBSPD_SPEED_FULL	0x2
#define USBSPD_SPEED_HIGH	0x3

struct usbhs_priv {

	void __iomem *base;
	unsigned int irq;
	unsigned long irqflags;

	struct renesas_usbhs_platform_callback	pfunc;
	struct renesas_usbhs_driver_param	dparam;

	struct delayed_work notify_hotplug_work;
	struct platform_device *pdev;

	spinlock_t		lock;

	u32 flags;

	struct usbhs_mod_info mod_info;

	struct usbhs_pipe_info pipe_info;

	struct usbhs_fifo_info fifo_info;
};

u16 usbhs_read(struct usbhs_priv *priv, u32 reg);
void usbhs_write(struct usbhs_priv *priv, u32 reg, u16 data);
void usbhs_bset(struct usbhs_priv *priv, u32 reg, u16 mask, u16 data);

#define usbhs_lock(p, f) spin_lock_irqsave(usbhs_priv_to_lock(p), f)
#define usbhs_unlock(p, f) spin_unlock_irqrestore(usbhs_priv_to_lock(p), f)

void usbhs_sys_host_ctrl(struct usbhs_priv *priv, int enable);
void usbhs_sys_function_ctrl(struct usbhs_priv *priv, int enable);
void usbhs_sys_set_test_mode(struct usbhs_priv *priv, u16 mode);

void usbhs_usbreq_get_val(struct usbhs_priv *priv, struct usb_ctrlrequest *req);
void usbhs_usbreq_set_val(struct usbhs_priv *priv, struct usb_ctrlrequest *req);

void usbhs_bus_send_sof_enable(struct usbhs_priv *priv);
void usbhs_bus_send_reset(struct usbhs_priv *priv);
int usbhs_bus_get_speed(struct usbhs_priv *priv);
int usbhs_vbus_ctrl(struct usbhs_priv *priv, int enable);

int usbhs_frame_get_num(struct usbhs_priv *priv);

int usbhs_set_device_config(struct usbhs_priv *priv, int devnum, u16 upphub,
			   u16 hubport, u16 speed);

struct usbhs_priv *usbhs_pdev_to_priv(struct platform_device *pdev);
#define usbhs_get_dparam(priv, param)	(priv->dparam.param)
#define usbhs_priv_to_pdev(priv)	(priv->pdev)
#define usbhs_priv_to_dev(priv)		(&priv->pdev->dev)
#define usbhs_priv_to_lock(priv)	(&priv->lock)

#endif 
