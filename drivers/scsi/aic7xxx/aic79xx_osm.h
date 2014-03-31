/*
 * Adaptec AIC79xx device driver for Linux.
 *
 * Copyright (c) 2000-2001 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/aic7xxx/linux/drivers/scsi/aic7xxx/aic79xx_osm.h#166 $
 *
 */
#ifndef _AIC79XX_LINUX_H_
#define _AIC79XX_LINUX_H_

#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/byteorder.h>
#include <asm/io.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_spi.h>

#define AIC_LIB_PREFIX ahd

#ifdef LIST_HEAD
#undef LIST_HEAD
#endif

#include "cam.h"
#include "queue.h"
#include "scsi_message.h"
#include "scsi_iu.h"
#include "aiclib.h"

#ifdef CONFIG_AIC79XX_DEBUG_ENABLE
#ifdef CONFIG_AIC79XX_DEBUG_MASK
#define AHD_DEBUG 1
#define AHD_DEBUG_OPTS CONFIG_AIC79XX_DEBUG_MASK
#else
#define AHD_DEBUG 1
#define AHD_DEBUG_OPTS 0
#endif
#endif

#define	powerof2(x)	((((x)-1)&(x))==0)

struct ahd_softc;
typedef struct pci_dev *ahd_dev_softc_t;
typedef struct scsi_cmnd      *ahd_io_ctx_t;

#define ahd_htobe16(x)	cpu_to_be16(x)
#define ahd_htobe32(x)	cpu_to_be32(x)
#define ahd_htobe64(x)	cpu_to_be64(x)
#define ahd_htole16(x)	cpu_to_le16(x)
#define ahd_htole32(x)	cpu_to_le32(x)
#define ahd_htole64(x)	cpu_to_le64(x)

#define ahd_be16toh(x)	be16_to_cpu(x)
#define ahd_be32toh(x)	be32_to_cpu(x)
#define ahd_be64toh(x)	be64_to_cpu(x)
#define ahd_le16toh(x)	le16_to_cpu(x)
#define ahd_le32toh(x)	le32_to_cpu(x)
#define ahd_le64toh(x)	le64_to_cpu(x)

extern uint32_t aic79xx_allow_memio;
extern struct scsi_host_template aic79xx_driver_template;


typedef uint32_t bus_size_t;

typedef enum {
	BUS_SPACE_MEMIO,
	BUS_SPACE_PIO
} bus_space_tag_t;

typedef union {
	u_long		  ioport;
	volatile uint8_t __iomem *maddr;
} bus_space_handle_t;

typedef struct bus_dma_segment
{
	dma_addr_t	ds_addr;
	bus_size_t	ds_len;
} bus_dma_segment_t;

struct ahd_linux_dma_tag
{
	bus_size_t	alignment;
	bus_size_t	boundary;
	bus_size_t	maxsize;
};
typedef struct ahd_linux_dma_tag* bus_dma_tag_t;

typedef dma_addr_t bus_dmamap_t;

typedef int bus_dma_filter_t(void*, dma_addr_t);
typedef void bus_dmamap_callback_t(void *, bus_dma_segment_t *, int, int);

#define BUS_DMA_WAITOK		0x0
#define BUS_DMA_NOWAIT		0x1
#define BUS_DMA_ALLOCNOW	0x2
#define BUS_DMA_LOAD_SEGS	0x4	

#define BUS_SPACE_MAXADDR	0xFFFFFFFF
#define BUS_SPACE_MAXADDR_32BIT	0xFFFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT	0xFFFFFFFF

int	ahd_dma_tag_create(struct ahd_softc *, bus_dma_tag_t ,
			   bus_size_t , bus_size_t ,
			   dma_addr_t , dma_addr_t ,
			   bus_dma_filter_t*, void *,
			   bus_size_t , int ,
			   bus_size_t , int ,
			   bus_dma_tag_t *);

void	ahd_dma_tag_destroy(struct ahd_softc *, bus_dma_tag_t );

int	ahd_dmamem_alloc(struct ahd_softc *, bus_dma_tag_t ,
			 void** , int ,
			 bus_dmamap_t* );

void	ahd_dmamem_free(struct ahd_softc *, bus_dma_tag_t ,
			void* , bus_dmamap_t );

void	ahd_dmamap_destroy(struct ahd_softc *, bus_dma_tag_t ,
			   bus_dmamap_t );

int	ahd_dmamap_load(struct ahd_softc *ahd, bus_dma_tag_t ,
			bus_dmamap_t , void * ,
			bus_size_t , bus_dmamap_callback_t *,
			void *, int );

int	ahd_dmamap_unload(struct ahd_softc *, bus_dma_tag_t, bus_dmamap_t);

#define BUS_DMASYNC_PREREAD	0x01	
#define BUS_DMASYNC_POSTREAD	0x02	
#define BUS_DMASYNC_PREWRITE	0x04	
#define BUS_DMASYNC_POSTWRITE	0x08	

#define ahd_dmamap_sync(ahd, dma_tag, dmamap, offset, len, op)

typedef struct timer_list ahd_timer_t;

#ifdef CONFIG_AIC79XX_REG_PRETTY_PRINT
#define AIC_DEBUG_REGISTERS 1
#else
#define AIC_DEBUG_REGISTERS 0
#endif
#include "aic79xx.h"

#define ahd_timer_init init_timer
#define ahd_timer_stop del_timer_sync

#include <linux/spinlock.h>

#define AIC79XX_DRIVER_VERSION "3.0"


typedef enum {
	AHD_DEV_FREEZE_TIL_EMPTY = 0x02, 
	AHD_DEV_Q_BASIC		 = 0x10, 
	AHD_DEV_Q_TAGGED	 = 0x20, 
	AHD_DEV_PERIODIC_OTAG	 = 0x40, 
} ahd_linux_dev_flags;

struct ahd_linux_device {
	TAILQ_ENTRY(ahd_linux_device) links;

	int			active;

	int			openings;

	u_int			qfrozen;
	
	u_long			commands_issued;

	u_int			tag_success_count;
#define AHD_TAG_SUCCESS_INTERVAL 50

	ahd_linux_dev_flags	flags;

	struct timer_list	timer;

	u_int			maxtags;

	u_int			tags_on_last_queuefull;

	u_int			last_queuefull_same_count;
#define AHD_LOCK_TAGS_COUNT 50

	u_int			commands_since_idle_or_otag;
#define AHD_OTAG_THRESH	500
};

#define	AHD_NSEG 128

struct scb_platform_data {
	struct ahd_linux_device	*dev;
	dma_addr_t		 buf_busaddr;
	uint32_t		 xfer_len;
	uint32_t		 sense_resid;	
};

struct ahd_platform_data {
	struct scsi_target *starget[AHD_NUM_TARGETS]; 

	spinlock_t		 spin_lock;
	struct completion	*eh_done;
	struct Scsi_Host        *host;		
#define AHD_LINUX_NOIRQ	((uint32_t)~0)
	uint32_t		 irq;		
	uint32_t		 bios_address;
	resource_size_t		 mem_busaddr;	
};

void ahd_delay(long);

uint8_t ahd_inb(struct ahd_softc * ahd, long port);
void ahd_outb(struct ahd_softc * ahd, long port, uint8_t val);
void ahd_outw_atomic(struct ahd_softc * ahd,
				     long port, uint16_t val);
void ahd_outsb(struct ahd_softc * ahd, long port,
			       uint8_t *, int count);
void ahd_insb(struct ahd_softc * ahd, long port,
			       uint8_t *, int count);

int		ahd_linux_register_host(struct ahd_softc *,
					struct scsi_host_template *);

struct info_str {
	char *buffer;
	int length;
	off_t offset;
	int pos;
};

static inline void
ahd_lockinit(struct ahd_softc *ahd)
{
	spin_lock_init(&ahd->platform_data->spin_lock);
}

static inline void
ahd_lock(struct ahd_softc *ahd, unsigned long *flags)
{
	spin_lock_irqsave(&ahd->platform_data->spin_lock, *flags);
}

static inline void
ahd_unlock(struct ahd_softc *ahd, unsigned long *flags)
{
	spin_unlock_irqrestore(&ahd->platform_data->spin_lock, *flags);
}

#define PCIR_DEVVENDOR		0x00
#define PCIR_VENDOR		0x00
#define PCIR_DEVICE		0x02
#define PCIR_COMMAND		0x04
#define PCIM_CMD_PORTEN		0x0001
#define PCIM_CMD_MEMEN		0x0002
#define PCIM_CMD_BUSMASTEREN	0x0004
#define PCIM_CMD_MWRICEN	0x0010
#define PCIM_CMD_PERRESPEN	0x0040
#define	PCIM_CMD_SERRESPEN	0x0100
#define PCIR_STATUS		0x06
#define PCIR_REVID		0x08
#define PCIR_PROGIF		0x09
#define PCIR_SUBCLASS		0x0a
#define PCIR_CLASS		0x0b
#define PCIR_CACHELNSZ		0x0c
#define PCIR_LATTIMER		0x0d
#define PCIR_HEADERTYPE		0x0e
#define PCIM_MFDEV		0x80
#define PCIR_BIST		0x0f
#define PCIR_CAP_PTR		0x34

#define PCIR_MAPS	0x10
#define PCIR_SUBVEND_0	0x2c
#define PCIR_SUBDEV_0	0x2e

#define PCIXR_COMMAND	0x96
#define PCIXR_DEVADDR	0x98
#define PCIXM_DEVADDR_FNUM	0x0003	
#define PCIXM_DEVADDR_DNUM	0x00F8	
#define PCIXM_DEVADDR_BNUM	0xFF00	
#define PCIXR_STATUS	0x9A
#define PCIXM_STATUS_64BIT	0x0001	
#define PCIXM_STATUS_133CAP	0x0002	
#define PCIXM_STATUS_SCDISC	0x0004	
#define PCIXM_STATUS_UNEXPSC	0x0008	
#define PCIXM_STATUS_CMPLEXDEV	0x0010	
#define PCIXM_STATUS_MAXMRDBC	0x0060	
#define PCIXM_STATUS_MAXSPLITS	0x0380	
#define PCIXM_STATUS_MAXCRDS	0x1C00	
#define PCIXM_STATUS_RCVDSCEM	0x2000	

typedef enum
{
	AHD_POWER_STATE_D0,
	AHD_POWER_STATE_D1,
	AHD_POWER_STATE_D2,
	AHD_POWER_STATE_D3
} ahd_power_state;

void ahd_power_state_change(struct ahd_softc *ahd,
			    ahd_power_state new_state);

int			 ahd_linux_pci_init(void);
void			 ahd_linux_pci_exit(void);
int			 ahd_pci_map_registers(struct ahd_softc *ahd);
int			 ahd_pci_map_int(struct ahd_softc *ahd);

uint32_t		 ahd_pci_read_config(ahd_dev_softc_t pci,
					     int reg, int width);
void			 ahd_pci_write_config(ahd_dev_softc_t pci,
					  int reg, uint32_t value,
					  int width);

static inline int ahd_get_pci_function(ahd_dev_softc_t);
static inline int
ahd_get_pci_function(ahd_dev_softc_t pci)
{
	return (PCI_FUNC(pci->devfn));
}

static inline int ahd_get_pci_slot(ahd_dev_softc_t);
static inline int
ahd_get_pci_slot(ahd_dev_softc_t pci)
{
	return (PCI_SLOT(pci->devfn));
}

static inline int ahd_get_pci_bus(ahd_dev_softc_t);
static inline int
ahd_get_pci_bus(ahd_dev_softc_t pci)
{
	return (pci->bus->number);
}

static inline void ahd_flush_device_writes(struct ahd_softc *);
static inline void
ahd_flush_device_writes(struct ahd_softc *ahd)
{
	
	ahd_inb(ahd, INTSTAT);
}

int	ahd_linux_proc_info(struct Scsi_Host *, char *, char **,
			    off_t, int, int);

static inline void ahd_cmd_set_transaction_status(struct scsi_cmnd *, uint32_t);
static inline void ahd_set_transaction_status(struct scb *, uint32_t);
static inline void ahd_cmd_set_scsi_status(struct scsi_cmnd *, uint32_t);
static inline void ahd_set_scsi_status(struct scb *, uint32_t);
static inline uint32_t ahd_cmd_get_transaction_status(struct scsi_cmnd *cmd);
static inline uint32_t ahd_get_transaction_status(struct scb *);
static inline uint32_t ahd_cmd_get_scsi_status(struct scsi_cmnd *cmd);
static inline uint32_t ahd_get_scsi_status(struct scb *);
static inline void ahd_set_transaction_tag(struct scb *, int, u_int);
static inline u_long ahd_get_transfer_length(struct scb *);
static inline int ahd_get_transfer_dir(struct scb *);
static inline void ahd_set_residual(struct scb *, u_long);
static inline void ahd_set_sense_residual(struct scb *scb, u_long resid);
static inline u_long ahd_get_residual(struct scb *);
static inline u_long ahd_get_sense_residual(struct scb *);
static inline int ahd_perform_autosense(struct scb *);
static inline uint32_t ahd_get_sense_bufsize(struct ahd_softc *,
					       struct scb *);
static inline void ahd_notify_xfer_settings_change(struct ahd_softc *,
						     struct ahd_devinfo *);
static inline void ahd_platform_scb_free(struct ahd_softc *ahd,
					   struct scb *scb);
static inline void ahd_freeze_scb(struct scb *scb);

static inline
void ahd_cmd_set_transaction_status(struct scsi_cmnd *cmd, uint32_t status)
{
	cmd->result &= ~(CAM_STATUS_MASK << 16);
	cmd->result |= status << 16;
}

static inline
void ahd_set_transaction_status(struct scb *scb, uint32_t status)
{
	ahd_cmd_set_transaction_status(scb->io_ctx,status);
}

static inline
void ahd_cmd_set_scsi_status(struct scsi_cmnd *cmd, uint32_t status)
{
	cmd->result &= ~0xFFFF;
	cmd->result |= status;
}

static inline
void ahd_set_scsi_status(struct scb *scb, uint32_t status)
{
	ahd_cmd_set_scsi_status(scb->io_ctx, status);
}

static inline
uint32_t ahd_cmd_get_transaction_status(struct scsi_cmnd *cmd)
{
	return ((cmd->result >> 16) & CAM_STATUS_MASK);
}

static inline
uint32_t ahd_get_transaction_status(struct scb *scb)
{
	return (ahd_cmd_get_transaction_status(scb->io_ctx));
}

static inline
uint32_t ahd_cmd_get_scsi_status(struct scsi_cmnd *cmd)
{
	return (cmd->result & 0xFFFF);
}

static inline
uint32_t ahd_get_scsi_status(struct scb *scb)
{
	return (ahd_cmd_get_scsi_status(scb->io_ctx));
}

static inline
void ahd_set_transaction_tag(struct scb *scb, int enabled, u_int type)
{
}

static inline
u_long ahd_get_transfer_length(struct scb *scb)
{
	return (scb->platform_data->xfer_len);
}

static inline
int ahd_get_transfer_dir(struct scb *scb)
{
	return (scb->io_ctx->sc_data_direction);
}

static inline
void ahd_set_residual(struct scb *scb, u_long resid)
{
	scsi_set_resid(scb->io_ctx, resid);
}

static inline
void ahd_set_sense_residual(struct scb *scb, u_long resid)
{
	scb->platform_data->sense_resid = resid;
}

static inline
u_long ahd_get_residual(struct scb *scb)
{
	return scsi_get_resid(scb->io_ctx);
}

static inline
u_long ahd_get_sense_residual(struct scb *scb)
{
	return (scb->platform_data->sense_resid);
}

static inline
int ahd_perform_autosense(struct scb *scb)
{
	return (1);
}

static inline uint32_t
ahd_get_sense_bufsize(struct ahd_softc *ahd, struct scb *scb)
{
	return (sizeof(struct scsi_sense_data));
}

static inline void
ahd_notify_xfer_settings_change(struct ahd_softc *ahd,
				struct ahd_devinfo *devinfo)
{
	
}

static inline void
ahd_platform_scb_free(struct ahd_softc *ahd, struct scb *scb)
{
	ahd->flags &= ~AHD_RESOURCE_SHORTAGE;
}

int	ahd_platform_alloc(struct ahd_softc *ahd, void *platform_arg);
void	ahd_platform_free(struct ahd_softc *ahd);
void	ahd_platform_init(struct ahd_softc *ahd);
void	ahd_platform_freeze_devq(struct ahd_softc *ahd, struct scb *scb);

static inline void
ahd_freeze_scb(struct scb *scb)
{
	if ((scb->io_ctx->result & (CAM_DEV_QFRZN << 16)) == 0) {
                scb->io_ctx->result |= CAM_DEV_QFRZN << 16;
                scb->platform_data->dev->qfrozen++;
        }
}

void	ahd_platform_set_tags(struct ahd_softc *ahd, struct scsi_device *sdev,
			      struct ahd_devinfo *devinfo, ahd_queue_alg);
int	ahd_platform_abort_scbs(struct ahd_softc *ahd, int target,
				char channel, int lun, u_int tag,
				role_t role, uint32_t status);
irqreturn_t
	ahd_linux_isr(int irq, void *dev_id);
void	ahd_done(struct ahd_softc*, struct scb*);
void	ahd_send_async(struct ahd_softc *, char channel,
		       u_int target, u_int lun, ac_code);
void	ahd_print_path(struct ahd_softc *, struct scb *);

#ifdef CONFIG_PCI
#define AHD_PCI_CONFIG 1
#else
#define AHD_PCI_CONFIG 0
#endif
#define bootverbose aic79xx_verbose
extern uint32_t aic79xx_verbose;

#endif 
