/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SYM_GLUE_H
#define SYM_GLUE_H

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/types.h>

#include <asm/io.h>
#ifdef __sparc__
#  include <asm/irq.h>
#endif

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_transport_spi.h>
#include <scsi/scsi_host.h>

#include "sym53c8xx.h"
#include "sym_defs.h"
#include "sym_misc.h"

#define	SYM_CONF_TIMER_INTERVAL		((HZ+1)/2)

#undef SYM_OPT_HANDLE_DEVICE_QUEUEING
#define SYM_OPT_LIMIT_COMMAND_REORDERING

#define printf_emerg(args...)	printk(KERN_EMERG args)
#define	printf_alert(args...)	printk(KERN_ALERT args)
#define	printf_crit(args...)	printk(KERN_CRIT args)
#define	printf_err(args...)	printk(KERN_ERR	args)
#define	printf_warning(args...)	printk(KERN_WARNING args)
#define	printf_notice(args...)	printk(KERN_NOTICE args)
#define	printf_info(args...)	printk(KERN_INFO args)
#define	printf_debug(args...)	printk(KERN_DEBUG args)
#define	printf(args...)		printk(args)


#define MEMORY_READ_BARRIER()	rmb()
#define MEMORY_WRITE_BARRIER()	wmb()


#ifdef	__BIG_ENDIAN

#define	readw_l2b	readw
#define	readl_l2b	readl
#define	writew_b2l	writew
#define	writel_b2l	writel

#else	

#define	readw_raw	readw
#define	readl_raw	readl
#define	writew_raw	writew
#define	writel_raw	writel

#endif 

#ifdef	SYM_CONF_CHIP_BIG_ENDIAN
#error	"Chips in BIG ENDIAN addressing mode are not (yet) supported"
#endif


#define cpu_to_scr(dw)	cpu_to_le32(dw)
#define scr_to_cpu(dw)	le32_to_cpu(dw)

#define SCSI_SUCCESS	SUCCESS
#define SCSI_FAILED	FAILED


#define SYM_HAVE_SLCB
struct sym_slcb {
	u_short	reqtags;	
	u_short scdev_depth;	
};


struct sym_shcb {
	int		unit;
	char		inst_name[16];
	char		chip_name[8];

	struct Scsi_Host *host;

	void __iomem *	ioaddr;		
	void __iomem *	ramaddr;	

	struct timer_list timer;	
	u_long		lasttime;
	u_long		settle_time;	
	u_char		settle_time_valid;
};

#define sym_name(np) (np)->s.inst_name

struct sym_nvram;

struct sym_device {
	struct pci_dev *pdev;
	unsigned long mmio_base;
	unsigned long ram_base;
	struct {
		void __iomem *ioaddr;
		void __iomem *ramaddr;
	} s;
	struct sym_chip chip;
	struct sym_nvram *nvram;
	u_char host_id;
};

struct sym_data {
	struct sym_hcb *ncb;
	struct completion *io_reset;		
	struct pci_dev *pdev;
};

static inline struct sym_hcb * sym_get_hcb(struct Scsi_Host *host)
{
	return ((struct sym_data *)host->hostdata)->ncb;
}

#include "sym_fw.h"
#include "sym_hipd.h"

static inline void
sym_set_cam_status(struct scsi_cmnd *cmd, int status)
{
	cmd->result &= ~(0xff  << 16);
	cmd->result |= (status << 16);
}

static inline int
sym_get_cam_status(struct scsi_cmnd *cmd)
{
	return host_byte(cmd->result);
}

static inline void sym_set_cam_result_ok(struct sym_ccb *cp, struct scsi_cmnd *cmd, int resid)
{
	scsi_set_resid(cmd, resid);
	cmd->result = (((DID_OK) << 16) + ((cp->ssss_status) & 0x7f));
}
void sym_set_cam_result_error(struct sym_hcb *np, struct sym_ccb *cp, int resid);

void sym_xpt_done(struct sym_hcb *np, struct scsi_cmnd *ccb);
#define sym_print_addr(cmd, arg...) dev_info(&cmd->device->sdev_gendev , ## arg)
void sym_xpt_async_bus_reset(struct sym_hcb *np);
int  sym_setup_data_and_start (struct sym_hcb *np, struct scsi_cmnd *csio, struct sym_ccb *cp);
void sym_log_bus_error(struct Scsi_Host *);
void sym_dump_registers(struct Scsi_Host *);

#endif 
