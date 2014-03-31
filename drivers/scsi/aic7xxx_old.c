/*+M*************************************************************************
 * Adaptec AIC7xxx device driver for Linux.
 *
 * Copyright (c) 1994 John Aycock
 *   The University of Calgary Department of Computer Science.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Sources include the Adaptec 1740 driver (aha1740.c), the Ultrastor 24F
 * driver (ultrastor.c), various Linux kernel source, the Adaptec EISA
 * config file (!adp7771.cfg), the Adaptec AHA-2740A Series User's Guide,
 * the Linux Kernel Hacker's Guide, Writing a SCSI Device Driver for Linux,
 * the Adaptec 1542 driver (aha1542.c), the Adaptec EISA overlay file
 * (adp7770.ovl), the Adaptec AHA-2740 Series Technical Reference Manual,
 * the Adaptec AIC-7770 Data Book, the ANSI SCSI specification, the
 * ANSI SCSI-2 specification (draft 10c), ...
 *
 * --------------------------------------------------------------------------
 *
 *  Modifications by Daniel M. Eischen (deischen@iworks.InterWorks.org):
 *
 *  Substantially modified to include support for wide and twin bus
 *  adapters, DMAing of SCBs, tagged queueing, IRQ sharing, bug fixes,
 *  SCB paging, and other rework of the code.
 *
 *  Parts of this driver were also based on the FreeBSD driver by
 *  Justin T. Gibbs.  His copyright follows:
 *
 * --------------------------------------------------------------------------  
 * Copyright (c) 1994-1997 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU General Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: aic7xxx.c,v 1.119 1997/06/27 19:39:18 gibbs Exp $
 *---------------------------------------------------------------------------
 *
 *  Thanks also go to (in alphabetical order) the following:
 *
 *    Rory Bolt     - Sequencer bug fixes
 *    Jay Estabrook - Initial DEC Alpha support
 *    Doug Ledford  - Much needed abort/reset bug fixes
 *    Kai Makisara  - DMAing of SCBs
 *
 *  A Boot time option was also added for not resetting the scsi bus.
 *
 *    Form:  aic7xxx=extended
 *           aic7xxx=no_reset
 *           aic7xxx=ultra
 *           aic7xxx=irq_trigger:[0,1]  # 0 edge, 1 level
 *           aic7xxx=verbose
 *
 *  Daniel M. Eischen, deischen@iworks.InterWorks.org, 1/23/97
 *
 *  $Id: aic7xxx.c,v 4.1 1997/06/12 08:23:42 deang Exp $
 *-M*************************************************************************/

/*+M**************************************************************************
 *
 * Further driver modifications made by Doug Ledford <dledford@redhat.com>
 *
 * Copyright (c) 1997-1999 Doug Ledford
 *
 * These changes are released under the same licensing terms as the FreeBSD
 * driver written by Justin Gibbs.  Please see his Copyright notice above
 * for the exact terms and conditions covering my changes as well as the
 * warranty statement.
 *
 * Modifications made to the aic7xxx.c,v 4.1 driver from Dan Eischen include
 * but are not limited to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification of kernel code to accommodate different sequencer semantics
 *  3: Extensive changes throughout kernel portion of driver to improve
 *     abort/reset processing and error hanndling
 *  4: Other work contributed by various people on the Internet
 *  5: Changes to printk information and verbosity selection code
 *  6: General reliability related changes, especially in IRQ management
 *  7: Modifications to the default probe/attach order for supported cards
 *  8: SMP friendliness has been improved
 *
 * Overall, this driver represents a significant departure from the official
 * aic7xxx driver released by Dan Eischen in two ways.  First, in the code
 * itself.  A diff between the two version of the driver is now a several
 * thousand line diff.  Second, in approach to solving the same problem.  The
 * problem is importing the FreeBSD aic7xxx driver code to linux can be a
 * difficult and time consuming process, that also can be error prone.  Dan
 * Eischen's official driver uses the approach that the linux and FreeBSD
 * drivers should be as identical as possible.  To that end, his next version
 * of this driver will be using a mid-layer code library that he is developing
 * to moderate communications between the linux mid-level SCSI code and the
 * low level FreeBSD driver.  He intends to be able to essentially drop the
 * FreeBSD driver into the linux kernel with only a few minor tweaks to some
 * include files and the like and get things working, making for fast easy
 * imports of the FreeBSD code into linux.
 *
 * I disagree with Dan's approach.  Not that I don't think his way of doing
 * things would be nice, easy to maintain, and create a more uniform driver
 * between FreeBSD and Linux.  I have no objection to those issues.  My
 * disagreement is on the needed functionality.  There simply are certain
 * things that are done differently in FreeBSD than linux that will cause
 * problems for this driver regardless of any middle ware Dan implements.
 * The biggest example of this at the moment is interrupt semantics.  Linux
 * doesn't provide the same protection techniques as FreeBSD does, nor can
 * they be easily implemented in any middle ware code since they would truly
 * belong in the kernel proper and would effect all drivers.  For the time
 * being, I see issues such as these as major stumbling blocks to the 
 * reliability of code based upon such middle ware.  Therefore, I choose to
 * use a different approach to importing the FreeBSD code that doesn't
 * involve any middle ware type code.  My approach is to import the sequencer
 * code from FreeBSD wholesale.  Then, to only make changes in the kernel
 * portion of the driver as they are needed for the new sequencer semantics.
 * In this way, the portion of the driver that speaks to the rest of the
 * linux kernel is fairly static and can be changed/modified to solve
 * any problems one might encounter without concern for the FreeBSD driver.
 *
 * Note: If time and experience should prove me wrong that the middle ware
 * code Dan writes is reliable in its operation, then I'll retract my above
 * statements.  But, for those that don't know, I'm from Missouri (in the US)
 * and our state motto is "The Show-Me State".  Well, before I will put
 * faith into it, you'll have to show me that it works :)
 *
 *_M*************************************************************************/



#define AIC7XXX_STRICT_PCI_SETUP

 
 
#include <linux/module.h>
#include <stdarg.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/byteorder.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include "scsi.h"
#include <scsi/scsi_host.h>
#include "aic7xxx_old/aic7xxx.h"

#include "aic7xxx_old/sequencer.h"
#include "aic7xxx_old/scsi_message.h"
#include "aic7xxx_old/aic7xxx_reg.h"
#include <scsi/scsicam.h>

#include <linux/stat.h>
#include <linux/slab.h>        

#define AIC7XXX_C_VERSION  "5.2.6"

#define ALL_TARGETS -1
#define ALL_CHANNELS -1
#define ALL_LUNS -1
#define MAX_TARGETS  16
#define MAX_LUNS     8
#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

#if defined(__powerpc__) || defined(__i386__) || defined(__x86_64__)
#  define MMAPIO
#endif

#define AIC7XXX_CMDS_PER_DEVICE 32

typedef struct
{
  unsigned char tag_commands[16];   
} adapter_tag_info_t;

#define DEFAULT_TAG_COMMANDS {0, 0, 0, 0, 0, 0, 0, 0,\
                              0, 0, 0, 0, 0, 0, 0, 0}



static adapter_tag_info_t aic7xxx_tag_info[] =
{
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS},
  {DEFAULT_TAG_COMMANDS}
};


static const char *board_names[] = {
  "AIC-7xxx Unknown",                                   
  "Adaptec AIC-7810 Hardware RAID Controller",          
  "Adaptec AIC-7770 SCSI host adapter",                 
  "Adaptec AHA-274X SCSI host adapter",                 
  "Adaptec AHA-284X SCSI host adapter",                 
  "Adaptec AIC-7850 SCSI host adapter",                 
  "Adaptec AIC-7855 SCSI host adapter",                 
  "Adaptec AIC-7860 Ultra SCSI host adapter",           
  "Adaptec AHA-2940A Ultra SCSI host adapter",          
  "Adaptec AIC-7870 SCSI host adapter",                 
  "Adaptec AHA-294X SCSI host adapter",                 
  "Adaptec AHA-394X SCSI host adapter",                 
  "Adaptec AHA-398X SCSI host adapter",                 
  "Adaptec AHA-2944 SCSI host adapter",                 
  "Adaptec AIC-7880 Ultra SCSI host adapter",           
  "Adaptec AHA-294X Ultra SCSI host adapter",           
  "Adaptec AHA-394X Ultra SCSI host adapter",           
  "Adaptec AHA-398X Ultra SCSI host adapter",           
  "Adaptec AHA-2944 Ultra SCSI host adapter",           
  "Adaptec AHA-2940UW Pro Ultra SCSI host adapter",     
  "Adaptec AIC-7895 Ultra SCSI host adapter",           
  "Adaptec AIC-7890/1 Ultra2 SCSI host adapter",        
  "Adaptec AHA-293X Ultra2 SCSI host adapter",          
  "Adaptec AHA-294X Ultra2 SCSI host adapter",          
  "Adaptec AIC-7896/7 Ultra2 SCSI host adapter",        
  "Adaptec AHA-394X Ultra2 SCSI host adapter",          
  "Adaptec AHA-395X Ultra2 SCSI host adapter",          
  "Adaptec PCMCIA SCSI controller",                     
  "Adaptec AIC-7892 Ultra 160/m SCSI host adapter",     
  "Adaptec AIC-7899 Ultra 160/m SCSI host adapter",     
};

#define DID_UNDERFLOW   DID_ERROR

#define DID_RETRY_COMMAND DID_ERROR

#define HSCSIID        0x07
#define SCSI_RESET     0x040

#define MINSLOT                1
#define MAXSLOT                15
#define SLOTBASE(x)        ((x) << 12)
#define BASE_TO_SLOT(x) ((x) >> 12)

#define AHC_HID0              0x80   
#define AHC_HID1              0x81   
#define AHC_HID2              0x82   
#define AHC_HID3              0x83   

#define MINREG                0xC00
#define MAXREG                0xCFF

#define INTDEF                0x5C      

#define        CLASS_PROGIF_REVID        0x08
#define                DEVREVID        0x000000FFul
#define                PROGINFC        0x0000FF00ul
#define                SUBCLASS        0x00FF0000ul
#define                BASECLASS        0xFF000000ul

#define        CSIZE_LATTIME                0x0C
#define                CACHESIZE        0x0000003Ful        
#define                LATTIME                0x0000FF00ul

#define        DEVCONFIG                0x40
#define                SCBSIZE32        0x00010000ul        
#define                MPORTMODE        0x00000400ul        
#define                RAMPSM           0x00000200ul        
#define                RAMPSM_ULTRA2    0x00000004
#define                VOLSENSE         0x00000100ul
#define                SCBRAMSEL        0x00000080ul
#define                SCBRAMSEL_ULTRA2 0x00000008
#define                MRDCEN           0x00000040ul
#define                EXTSCBTIME       0x00000020ul        
#define                EXTSCBPEN        0x00000010ul        
#define                BERREN           0x00000008ul
#define                DACEN            0x00000004ul
#define                STPWLEVEL        0x00000002ul
#define                DIFACTNEGEN      0x00000001ul        

#define        SCAMCTL                  0x1a                
#define        CCSCBBADDR               0xf0                

typedef enum {C46 = 6, C56_66 = 8} seeprom_chip_type;

struct seeprom_config {

#define CFXFER                0x0007      
#define CFSYNCH               0x0008      
#define CFDISC                0x0010      
#define CFWIDEB               0x0020      
#define CFSYNCHISULTRA        0x0040      
#define CFNEWULTRAFORMAT      0x0080      
#define CFSTART               0x0100      
#define CFINCBIOS             0x0200      
#define CFRNFOUND             0x0400      
#define CFMULTILUN            0x0800      
#define CFWBCACHEYES          0x4000      
#define CFWBCACHENC           0xc000      
  unsigned short device_flags[16];        

#define CFSUPREM        0x0001  
#define CFSUPREMB       0x0002  
#define CFBIOSEN        0x0004  
#define CFSM2DRV        0x0010  
#define CF284XEXTEND    0x0020  
#define CFEXTEND        0x0080  
  unsigned short bios_control;  

#define CFAUTOTERM      0x0001  
#define CFULTRAEN       0x0002  
#define CF284XSELTO     0x0003  
#define CF284XFIFO      0x000C  
#define CFSTERM         0x0004  
#define CFWSTERM        0x0008  
#define CFSPARITY       0x0010  
#define CF284XSTERM     0x0020  
#define CFRESETB        0x0040  
#define CFBPRIMARY      0x0100  
#define CFSEAUTOTERM    0x0400  
#define CFLVDSTERM      0x0800  
  unsigned short adapter_control;        

#define CFSCSIID        0x000F                
#define CFBRTIME        0xFF00                
  unsigned short brtime_id;                

#define CFMAXTARG        0x00FF        
  unsigned short max_targets;                

  unsigned short res_1[11];                
  unsigned short checksum;                
};

#define SELBUS_MASK                0x0a
#define         SELNARROW        0x00
#define         SELBUSB                0x08
#define SINGLE_BUS                0x00

#define SCB_TARGET(scb)         \
       (((scb)->hscb->target_channel_lun & TID) >> 4)
#define SCB_LUN(scb)            \
       ((scb)->hscb->target_channel_lun & LID)
#define SCB_IS_SCSIBUS_B(scb)   \
       (((scb)->hscb->target_channel_lun & SELBUSB) != 0)

#define aic7xxx_error(cmd)        ((cmd)->SCp.Status)

#define aic7xxx_status(cmd)        ((cmd)->SCp.sent_command)

#define aic7xxx_position(cmd)        ((cmd)->SCp.have_data_in)

#define aic7xxx_mapping(cmd)	     ((cmd)->SCp.phase)

#define AIC_DEV(cmd)	((struct aic_dev_data *)(cmd)->device->hostdata)

static struct aic7xxx_host *first_aic7xxx = NULL;

struct hw_scatterlist {
  unsigned int address;
  unsigned int length;
};

#define        AIC7XXX_MAX_SG 128

#define AIC7XXX_MAXSCB        255


struct aic7xxx_hwscb {
  unsigned char control;
  unsigned char target_channel_lun;       
  unsigned char target_status;
  unsigned char SG_segment_count;
  unsigned int  SG_list_pointer;
  unsigned char residual_SG_segment_count;
  unsigned char residual_data_count[3];
  unsigned int  data_pointer;
  unsigned int  data_count;
  unsigned int  SCSI_cmd_pointer;
  unsigned char SCSI_cmd_length;
  unsigned char tag;          
#define SCB_PIO_TRANSFER_SIZE  26   
  unsigned char next;         
  unsigned char prev;
  unsigned int pad;           
};

typedef enum {
        SCB_FREE                = 0x0000,
        SCB_DTR_SCB             = 0x0001,
        SCB_WAITINGQ            = 0x0002,
        SCB_ACTIVE              = 0x0004,
        SCB_SENSE               = 0x0008,
        SCB_ABORT               = 0x0010,
        SCB_DEVICE_RESET        = 0x0020,
        SCB_RESET               = 0x0040,
        SCB_RECOVERY_SCB        = 0x0080,
        SCB_MSGOUT_PPR          = 0x0100,
        SCB_MSGOUT_SENT         = 0x0200,
        SCB_MSGOUT_SDTR         = 0x0400,
        SCB_MSGOUT_WDTR         = 0x0800,
        SCB_MSGOUT_BITS         = SCB_MSGOUT_PPR |
                                  SCB_MSGOUT_SENT | 
                                  SCB_MSGOUT_SDTR |
                                  SCB_MSGOUT_WDTR,
        SCB_QUEUED_ABORT        = 0x1000,
        SCB_QUEUED_FOR_DONE     = 0x2000,
        SCB_WAS_BUSY            = 0x4000,
	SCB_QUEUE_FULL		= 0x8000
} scb_flag_type;

typedef enum {
        AHC_FNONE                 = 0x00000000,
        AHC_PAGESCBS              = 0x00000001,
        AHC_CHANNEL_B_PRIMARY     = 0x00000002,
        AHC_USEDEFAULTS           = 0x00000004,
        AHC_INDIRECT_PAGING       = 0x00000008,
        AHC_CHNLB                 = 0x00000020,
        AHC_CHNLC                 = 0x00000040,
        AHC_EXTEND_TRANS_A        = 0x00000100,
        AHC_EXTEND_TRANS_B        = 0x00000200,
        AHC_TERM_ENB_A            = 0x00000400,
        AHC_TERM_ENB_SE_LOW       = 0x00000400,
        AHC_TERM_ENB_B            = 0x00000800,
        AHC_TERM_ENB_SE_HIGH      = 0x00000800,
        AHC_HANDLING_REQINITS     = 0x00001000,
        AHC_TARGETMODE            = 0x00002000,
        AHC_NEWEEPROM_FMT         = 0x00004000,
        AHC_MOTHERBOARD           = 0x00020000,
        AHC_NO_STPWEN             = 0x00040000,
        AHC_RESET_DELAY           = 0x00080000,
        AHC_A_SCANNED             = 0x00100000,
        AHC_B_SCANNED             = 0x00200000,
        AHC_MULTI_CHANNEL         = 0x00400000,
        AHC_BIOS_ENABLED          = 0x00800000,
        AHC_SEEPROM_FOUND         = 0x01000000,
        AHC_TERM_ENB_LVD          = 0x02000000,
        AHC_ABORT_PENDING         = 0x04000000,
        AHC_RESET_PENDING         = 0x08000000,
#define AHC_IN_ISR_BIT              28
        AHC_IN_ISR                = 0x10000000,
        AHC_IN_ABORT              = 0x20000000,
        AHC_IN_RESET              = 0x40000000,
        AHC_EXTERNAL_SRAM         = 0x80000000
} ahc_flag_type;

typedef enum {
  AHC_NONE             = 0x0000,
  AHC_CHIPID_MASK      = 0x00ff,
  AHC_AIC7770          = 0x0001,
  AHC_AIC7850          = 0x0002,
  AHC_AIC7860          = 0x0003,
  AHC_AIC7870          = 0x0004,
  AHC_AIC7880          = 0x0005,
  AHC_AIC7890          = 0x0006,
  AHC_AIC7895          = 0x0007,
  AHC_AIC7896          = 0x0008,
  AHC_AIC7892          = 0x0009,
  AHC_AIC7899          = 0x000a,
  AHC_VL               = 0x0100,
  AHC_EISA             = 0x0200,
  AHC_PCI              = 0x0400,
} ahc_chip;

typedef enum {
  AHC_FENONE           = 0x0000,
  AHC_ULTRA            = 0x0001,
  AHC_ULTRA2           = 0x0002,
  AHC_WIDE             = 0x0004,
  AHC_TWIN             = 0x0008,
  AHC_MORE_SRAM        = 0x0010,
  AHC_CMD_CHAN         = 0x0020,
  AHC_QUEUE_REGS       = 0x0040,
  AHC_SG_PRELOAD       = 0x0080,
  AHC_SPIOCAP          = 0x0100,
  AHC_ULTRA3           = 0x0200,
  AHC_NEW_AUTOTERM     = 0x0400,
  AHC_AIC7770_FE       = AHC_FENONE,
  AHC_AIC7850_FE       = AHC_SPIOCAP,
  AHC_AIC7860_FE       = AHC_ULTRA|AHC_SPIOCAP,
  AHC_AIC7870_FE       = AHC_FENONE,
  AHC_AIC7880_FE       = AHC_ULTRA,
  AHC_AIC7890_FE       = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA2|
                         AHC_QUEUE_REGS|AHC_SG_PRELOAD|AHC_NEW_AUTOTERM,
  AHC_AIC7895_FE       = AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA,
  AHC_AIC7896_FE       = AHC_AIC7890_FE,
  AHC_AIC7892_FE       = AHC_AIC7890_FE|AHC_ULTRA3,
  AHC_AIC7899_FE       = AHC_AIC7890_FE|AHC_ULTRA3,
} ahc_feature;

#define SCB_DMA_ADDR(scb, addr) ((unsigned long)(addr) + (scb)->scb_dma->dma_offset)

struct aic7xxx_scb_dma {
	unsigned long	       dma_offset;    
	dma_addr_t	       dma_address;   
	unsigned int	       dma_len;	      
};

typedef enum {
  AHC_BUG_NONE            = 0x0000,
  AHC_BUG_TMODE_WIDEODD   = 0x0001,
  AHC_BUG_AUTOFLUSH       = 0x0002,
  AHC_BUG_CACHETHEN       = 0x0004,
  AHC_BUG_CACHETHEN_DIS   = 0x0008,
  AHC_BUG_PCI_2_1_RETRY   = 0x0010,
  AHC_BUG_PCI_MWI         = 0x0020,
  AHC_BUG_SCBCHAN_UPLOAD  = 0x0040,
} ahc_bugs;

struct aic7xxx_scb {
	struct aic7xxx_hwscb	*hscb;		
	struct scsi_cmnd	*cmd;		
	struct aic7xxx_scb	*q_next;        
	volatile scb_flag_type	flags;		
	struct hw_scatterlist	*sg_list;	
	unsigned char		tag_action;
	unsigned char		sg_count;
	unsigned char		*sense_cmd;	
	unsigned char		*cmnd;
	unsigned int		sg_length;	
	void			*kmalloc_ptr;
	struct aic7xxx_scb_dma	*scb_dma;
};

typedef struct {
  struct aic7xxx_scb *head;
  struct aic7xxx_scb *tail;
} scb_queue_type;

static struct {
  unsigned char errno;
  const char *errmesg;
} hard_error[] = {
  { ILLHADDR,  "Illegal Host Access" },
  { ILLSADDR,  "Illegal Sequencer Address referenced" },
  { ILLOPCODE, "Illegal Opcode in sequencer program" },
  { SQPARERR,  "Sequencer Ram Parity Error" },
  { DPARERR,   "Data-Path Ram Parity Error" },
  { MPARERR,   "Scratch Ram/SCB Array Ram Parity Error" },
  { PCIERRSTAT,"PCI Error detected" },
  { CIOPARERR, "CIOBUS Parity Error" }
};

static unsigned char
generic_sense[] = { REQUEST_SENSE, 0, 0, 0, 255, 0 };

typedef struct {
  scb_queue_type free_scbs;        
  struct aic7xxx_scb   *scb_array[AIC7XXX_MAXSCB];
  struct aic7xxx_hwscb *hscbs;
  unsigned char  numscbs;          
  unsigned char  maxhscbs;         
  unsigned char  maxscbs;          
  dma_addr_t	 hscbs_dma;	   
  unsigned int   hscbs_dma_len;    
  void          *hscb_kmalloc_ptr;
} scb_data_type;

struct target_cmd {
  unsigned char mesg_bytes[4];
  unsigned char command[28];
};

#define AHC_TRANS_CUR    0x0001
#define AHC_TRANS_ACTIVE 0x0002
#define AHC_TRANS_GOAL   0x0004
#define AHC_TRANS_USER   0x0008
#define AHC_TRANS_QUITE  0x0010
typedef struct {
  unsigned char width;
  unsigned char period;
  unsigned char offset;
  unsigned char options;
} transinfo_type;

struct aic_dev_data {
  volatile scb_queue_type  delayed_scbs;
  volatile unsigned short  temp_q_depth;
  unsigned short           max_q_depth;
  volatile unsigned char   active_cmds;
  long w_total;                          
  long r_total;                          
  long barrier_total;			 
  long ordered_total;			 
  long w_bins[6];                       
  long r_bins[6];                       
  transinfo_type	cur;
  transinfo_type	goal;
#define  BUS_DEVICE_RESET_PENDING       0x01
#define  DEVICE_RESET_DELAY             0x02
#define  DEVICE_PRINT_DTR               0x04
#define  DEVICE_WAS_BUSY                0x08
#define  DEVICE_DTR_SCANNED		0x10
#define  DEVICE_SCSI_3			0x20
  volatile unsigned char   flags;
  unsigned needppr:1;
  unsigned needppr_copy:1;
  unsigned needsdtr:1;
  unsigned needsdtr_copy:1;
  unsigned needwdtr:1;
  unsigned needwdtr_copy:1;
  unsigned dtr_pending:1;
  struct scsi_device *SDptr;
  struct list_head list;
};

struct aic7xxx_host {

  /*
   * We are grouping things here....first, items that get either read or
   * written with nearly every interrupt
   */
	volatile long	flags;
	ahc_feature	features;	
	unsigned long	base;		
	volatile unsigned char  __iomem *maddr;	
	unsigned long	isr_count;	
	unsigned long	spurious_int;
	scb_data_type	*scb_data;
	struct aic7xxx_cmd_queue {
		struct scsi_cmnd *head;
		struct scsi_cmnd *tail;
	} completeq;

	/*
	* Things read/written on nearly every entry into aic7xxx_queue()
	*/
	volatile scb_queue_type	waiting_scbs;
	unsigned char	unpause;	
	unsigned char	pause;		
	volatile unsigned char	qoutfifonext;
	volatile unsigned char	activescbs;	
	volatile unsigned char	max_activescbs;
	volatile unsigned char	qinfifonext;
	volatile unsigned char	*untagged_scbs;
	volatile unsigned char	*qoutfifo;
	volatile unsigned char	*qinfifo;

	unsigned char	dev_last_queue_full[MAX_TARGETS];
	unsigned char	dev_last_queue_full_count[MAX_TARGETS];
	unsigned short	ultraenb; 
	unsigned short	discenable; 
	transinfo_type	user[MAX_TARGETS];

	unsigned char	msg_buf[13];	
	unsigned char	msg_type;
#define MSG_TYPE_NONE              0x00
#define MSG_TYPE_INITIATOR_MSGOUT  0x01
#define MSG_TYPE_INITIATOR_MSGIN   0x02
	unsigned char	msg_len;	
	unsigned char	msg_index;	



	unsigned int	irq;		
	int		instance;	
	int		scsi_id;	
	int		scsi_id_b;	
	unsigned int	bios_address;
	int		board_name_index;
	unsigned short	bios_control;		
	unsigned short	adapter_control;	
	struct pci_dev	*pdev;
	unsigned char	pci_bus;
	unsigned char	pci_device_fn;
	struct seeprom_config	sc;
	unsigned short	sc_type;
	unsigned short	sc_size;
	struct aic7xxx_host	*next;	
	struct Scsi_Host	*host;	
	struct list_head	 aic_devs; 
	int		host_no;	
	unsigned long	mbase;		
	ahc_chip	chip;		
	ahc_bugs	bugs;
	dma_addr_t	fifo_dma;	
};

#define AHC_SYNCRATE_ULTRA3 0
#define AHC_SYNCRATE_ULTRA2 1
#define AHC_SYNCRATE_ULTRA  3
#define AHC_SYNCRATE_FAST   6
#define AHC_SYNCRATE_CRC 0x40
#define AHC_SYNCRATE_SE  0x10
static struct aic7xxx_syncrate {
  
#define                ULTRA_SXFR 0x100
  int sxfr_ultra2;
  int sxfr;
  unsigned char period;
  const char *rate[2];
} aic7xxx_syncrates[] = {
  { 0x42,  0x000,   9,  {"80.0", "160.0"} },
  { 0x13,  0x000,  10,  {"40.0", "80.0"} },
  { 0x14,  0x000,  11,  {"33.0", "66.6"} },
  { 0x15,  0x100,  12,  {"20.0", "40.0"} },
  { 0x16,  0x110,  15,  {"16.0", "32.0"} },
  { 0x17,  0x120,  18,  {"13.4", "26.8"} },
  { 0x18,  0x000,  25,  {"10.0", "20.0"} },
  { 0x19,  0x010,  31,  {"8.0",  "16.0"} },
  { 0x1a,  0x020,  37,  {"6.67", "13.3"} },
  { 0x1b,  0x030,  43,  {"5.7",  "11.4"} },
  { 0x10,  0x040,  50,  {"5.0",  "10.0"} },
  { 0x00,  0x050,  56,  {"4.4",  "8.8" } },
  { 0x00,  0x060,  62,  {"4.0",  "8.0" } },
  { 0x00,  0x070,  68,  {"3.6",  "7.2" } },
  { 0x00,  0x000,  0,   {NULL, NULL}   },
};

#define CTL_OF_SCB(scb) (((scb->hscb)->target_channel_lun >> 3) & 0x1),  \
                        (((scb->hscb)->target_channel_lun >> 4) & 0xf), \
                        ((scb->hscb)->target_channel_lun & 0x07)

#define CTL_OF_CMD(cmd) ((cmd->device->channel) & 0x01),  \
                        ((cmd->device->id) & 0x0f), \
                        ((cmd->device->lun) & 0x07)

#define TARGET_INDEX(cmd)  ((cmd)->device->id | ((cmd)->device->channel << 3))


#define WARN_LEAD KERN_WARNING "(scsi%d:%d:%d:%d) "
#define INFO_LEAD KERN_INFO "(scsi%d:%d:%d:%d) "


static unsigned int aic7xxx_default_queue_depth = AIC7XXX_CMDS_PER_DEVICE;

static unsigned int aic7xxx_no_reset = 0;
static int aic7xxx_reverse_scan = 0;
static unsigned int aic7xxx_extended = 0;
static int aic7xxx_irq_trigger = -1;
static int aic7xxx_override_term = -1;
static int aic7xxx_stpwlev = -1;
static int aic7xxx_panic_on_abort = 0;
static int aic7xxx_pci_parity = 0;
static int aic7xxx_dump_card = 0;
static int aic7xxx_dump_sequencer = 0;
static int aic7xxx_no_probe = 0;
static int aic7xxx_scbram = 0;
static int aic7xxx_seltime = 0x10;
#ifdef MODULE
static char * aic7xxx = NULL;
module_param(aic7xxx, charp, 0);
#endif

#define VERBOSE_NORMAL         0x0000
#define VERBOSE_NEGOTIATION    0x0001
#define VERBOSE_SEQINT         0x0002
#define VERBOSE_SCSIINT        0x0004
#define VERBOSE_PROBE          0x0008
#define VERBOSE_PROBE2         0x0010
#define VERBOSE_NEGOTIATION2   0x0020
#define VERBOSE_MINOR_ERROR    0x0040
#define VERBOSE_TRACING        0x0080
#define VERBOSE_ABORT          0x0f00
#define VERBOSE_ABORT_MID      0x0100
#define VERBOSE_ABORT_FIND     0x0200
#define VERBOSE_ABORT_PROCESS  0x0400
#define VERBOSE_ABORT_RETURN   0x0800
#define VERBOSE_RESET          0xf000
#define VERBOSE_RESET_MID      0x1000
#define VERBOSE_RESET_FIND     0x2000
#define VERBOSE_RESET_PROCESS  0x4000
#define VERBOSE_RESET_RETURN   0x8000
static int aic7xxx_verbose = VERBOSE_NORMAL | VERBOSE_NEGOTIATION |
           VERBOSE_PROBE;                     



static int aic7xxx_release(struct Scsi_Host *host);
static void aic7xxx_set_syncrate(struct aic7xxx_host *p, 
		struct aic7xxx_syncrate *syncrate, int target, int channel,
		unsigned int period, unsigned int offset, unsigned char options,
		unsigned int type, struct aic_dev_data *aic_dev);
static void aic7xxx_set_width(struct aic7xxx_host *p, int target, int channel,
		int lun, unsigned int width, unsigned int type,
		struct aic_dev_data *aic_dev);
static void aic7xxx_panic_abort(struct aic7xxx_host *p, struct scsi_cmnd *cmd);
static void aic7xxx_print_card(struct aic7xxx_host *p);
static void aic7xxx_print_scratch_ram(struct aic7xxx_host *p);
static void aic7xxx_print_sequencer(struct aic7xxx_host *p, int downloaded);
#ifdef AIC7XXX_VERBOSE_DEBUGGING
static void aic7xxx_check_scbs(struct aic7xxx_host *p, char *buffer);
#endif


static unsigned char
aic_inb(struct aic7xxx_host *p, long port)
{
#ifdef MMAPIO
  unsigned char x;
  if(p->maddr)
  {
    x = readb(p->maddr + port);
  }
  else
  {
    x = inb(p->base + port);
  }
  return(x);
#else
  return(inb(p->base + port));
#endif
}

static void
aic_outb(struct aic7xxx_host *p, unsigned char val, long port)
{
#ifdef MMAPIO
  if(p->maddr)
  {
    writeb(val, p->maddr + port);
    mb(); 
    readb(p->maddr + HCNTRL); 
  }
  else
  {
    outb(val, p->base + port);
    mb(); 
  }
#else
  outb(val, p->base + port);
  mb(); 
#endif
}

static int
aic7xxx_setup(char *s)
{
  int   i, n;
  char *p;
  char *end;

  static struct {
    const char *name;
    unsigned int *flag;
  } options[] = {
    { "extended",    &aic7xxx_extended },
    { "no_reset",    &aic7xxx_no_reset },
    { "irq_trigger", &aic7xxx_irq_trigger },
    { "verbose",     &aic7xxx_verbose },
    { "reverse_scan",&aic7xxx_reverse_scan },
    { "override_term", &aic7xxx_override_term },
    { "stpwlev", &aic7xxx_stpwlev },
    { "no_probe", &aic7xxx_no_probe },
    { "panic_on_abort", &aic7xxx_panic_on_abort },
    { "pci_parity", &aic7xxx_pci_parity },
    { "dump_card", &aic7xxx_dump_card },
    { "dump_sequencer", &aic7xxx_dump_sequencer },
    { "default_queue_depth", &aic7xxx_default_queue_depth },
    { "scbram", &aic7xxx_scbram },
    { "seltime", &aic7xxx_seltime },
    { "tag_info",    NULL }
  };

  end = strchr(s, '\0');

  while ((p = strsep(&s, ",.")) != NULL)
  {
    for (i = 0; i < ARRAY_SIZE(options); i++)
    {
      n = strlen(options[i].name);
      if (!strncmp(options[i].name, p, n))
      {
        if (!strncmp(p, "tag_info", n))
        {
          if (p[n] == ':')
          {
            char *base;
            char *tok, *tok_end, *tok_end2;
            char tok_list[] = { '.', ',', '{', '}', '\0' };
            int i, instance = -1, device = -1;
            unsigned char done = FALSE;

            base = p;
            tok = base + n + 1;  
            tok_end = strchr(tok, '\0');
            if (tok_end < end)
              *tok_end = ',';
            while(!done)
            {
              switch(*tok)
              {
                case '{':
                  if (instance == -1)
                    instance = 0;
                  else if (device == -1)
                    device = 0;
                  tok++;
                  break;
                case '}':
                  if (device != -1)
                    device = -1;
                  else if (instance != -1)
                    instance = -1;
                  tok++;
                  break;
                case ',':
                case '.':
                  if (instance == -1)
                    done = TRUE;
                  else if (device >= 0)
                    device++;
                  else if (instance >= 0)
                    instance++;
                  if ( (device >= MAX_TARGETS) || 
                       (instance >= ARRAY_SIZE(aic7xxx_tag_info)) )
                    done = TRUE;
                  tok++;
                  if (!done)
                  {
                    base = tok;
                  }
                  break;
                case '\0':
                  done = TRUE;
                  break;
                default:
                  done = TRUE;
                  tok_end = strchr(tok, '\0');
                  for(i=0; tok_list[i]; i++)
                  {
                    tok_end2 = strchr(tok, tok_list[i]);
                    if ( (tok_end2) && (tok_end2 < tok_end) )
                    {
                      tok_end = tok_end2;
                      done = FALSE;
                    }
                  }
                  if ( (instance >= 0) && (device >= 0) &&
                       (instance < ARRAY_SIZE(aic7xxx_tag_info)) &&
                       (device < MAX_TARGETS) )
                    aic7xxx_tag_info[instance].tag_commands[device] =
                      simple_strtoul(tok, NULL, 0) & 0xff;
                  tok = tok_end;
                  break;
              }
            }
            while((p != base) && (p != NULL))
              p = strsep(&s, ",.");
          }
        }
        else if (p[n] == ':')
        {
          *(options[i].flag) = simple_strtoul(p + n + 1, NULL, 0);
          if(!strncmp(p, "seltime", n))
          {
            *(options[i].flag) = (*(options[i].flag) % 4) << 3;
          }
        }
        else if (!strncmp(p, "verbose", n))
        {
          *(options[i].flag) = 0xff29;
        }
        else
        {
          *(options[i].flag) = ~(*(options[i].flag));
          if(!strncmp(p, "seltime", n))
          {
            *(options[i].flag) = (*(options[i].flag) % 4) << 3;
          }
        }
      }
    }
  }
  return 1;
}

__setup("aic7xxx=", aic7xxx_setup);

static void
pause_sequencer(struct aic7xxx_host *p)
{
  aic_outb(p, p->pause, HCNTRL);
  while ((aic_inb(p, HCNTRL) & PAUSE) == 0)
  {
    ;
  }
  if(p->features & AHC_ULTRA2)
  {
    aic_inb(p, CCSCBCTL);
  }
}

static void
unpause_sequencer(struct aic7xxx_host *p, int unpause_always)
{
  if (unpause_always ||
      ( !(aic_inb(p, INTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) &&
        !(p->flags & AHC_HANDLING_REQINITS) ) )
  {
    aic_outb(p, p->unpause, HCNTRL);
  }
}

static void
restart_sequencer(struct aic7xxx_host *p)
{
  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);
  aic_outb(p, FASTMODE, SEQCTL);
}

#include "aic7xxx_old/aic7xxx_seq.c"

static int
aic7xxx_check_patch(struct aic7xxx_host *p,
  struct sequencer_patch **start_patch, int start_instr, int *skip_addr)
{
  struct sequencer_patch *cur_patch;
  struct sequencer_patch *last_patch;
  int num_patches;

  num_patches = ARRAY_SIZE(sequencer_patches);
  last_patch = &sequencer_patches[num_patches];
  cur_patch = *start_patch;

  while ((cur_patch < last_patch) && (start_instr == cur_patch->begin))
  {
    if (cur_patch->patch_func(p) == 0)
    {
      *skip_addr = start_instr + cur_patch->skip_instr;
      cur_patch += cur_patch->skip_patch;
    }
    else
    {
      cur_patch++;
    }
  }

  *start_patch = cur_patch;
  if (start_instr < *skip_addr)
    return (0);
  return(1);
}


static void
aic7xxx_download_instr(struct aic7xxx_host *p, int instrptr,
  unsigned char *dconsts)
{
  union ins_formats instr;
  struct ins_format1 *fmt1_ins;
  struct ins_format3 *fmt3_ins;
  unsigned char opcode;

  instr = *(union ins_formats*) &seqprog[instrptr * 4];

  instr.integer = le32_to_cpu(instr.integer);
  
  fmt1_ins = &instr.format1;
  fmt3_ins = NULL;

  
  opcode = instr.format1.opcode;
  switch (opcode)
  {
    case AIC_OP_JMP:
    case AIC_OP_JC:
    case AIC_OP_JNC:
    case AIC_OP_CALL:
    case AIC_OP_JNE:
    case AIC_OP_JNZ:
    case AIC_OP_JE:
    case AIC_OP_JZ:
    {
      struct sequencer_patch *cur_patch;
      int address_offset;
      unsigned int address;
      int skip_addr;
      int i;

      fmt3_ins = &instr.format3;
      address_offset = 0;
      address = fmt3_ins->address;
      cur_patch = sequencer_patches;
      skip_addr = 0;

      for (i = 0; i < address;)
      {
        aic7xxx_check_patch(p, &cur_patch, i, &skip_addr);
        if (skip_addr > i)
        {
          int end_addr;

          end_addr = min_t(int, address, skip_addr);
          address_offset += end_addr - i;
          i = skip_addr;
        }
        else
        {
          i++;
        }
      }
      address -= address_offset;
      fmt3_ins->address = address;
      
    }
    case AIC_OP_OR:
    case AIC_OP_AND:
    case AIC_OP_XOR:
    case AIC_OP_ADD:
    case AIC_OP_ADC:
    case AIC_OP_BMOV:
      if (fmt1_ins->parity != 0)
      {
        fmt1_ins->immediate = dconsts[fmt1_ins->immediate];
      }
      fmt1_ins->parity = 0;
      
    case AIC_OP_ROL:
      if ((p->features & AHC_ULTRA2) != 0)
      {
        int i, count;

        
        for ( i=0, count=0; i < 31; i++)
        {
          unsigned int mask;

          mask = 0x01 << i;
          if ((instr.integer & mask) != 0)
            count++;
        }
        if (!(count & 0x01))
          instr.format1.parity = 1;
      }
      else
      {
        if (fmt3_ins != NULL)
        {
          instr.integer =  fmt3_ins->immediate |
                          (fmt3_ins->source << 8) |
                          (fmt3_ins->address << 16) |
                          (fmt3_ins->opcode << 25);
        }
        else
        {
          instr.integer =  fmt1_ins->immediate |
                          (fmt1_ins->source << 8) |
                          (fmt1_ins->destination << 16) |
                          (fmt1_ins->ret << 24) |
                          (fmt1_ins->opcode << 25);
        }
      }
      aic_outb(p, (instr.integer & 0xff), SEQRAM);
      aic_outb(p, ((instr.integer >> 8) & 0xff), SEQRAM);
      aic_outb(p, ((instr.integer >> 16) & 0xff), SEQRAM);
      aic_outb(p, ((instr.integer >> 24) & 0xff), SEQRAM);
      udelay(10);
      break;

    default:
      panic("aic7xxx: Unknown opcode encountered in sequencer program.");
      break;
  }
}


static void
aic7xxx_loadseq(struct aic7xxx_host *p)
{
  struct sequencer_patch *cur_patch;
  int i;
  int downloaded;
  int skip_addr;
  unsigned char download_consts[4] = {0, 0, 0, 0};

  if (aic7xxx_verbose & VERBOSE_PROBE)
  {
    printk(KERN_INFO "(scsi%d) Downloading sequencer code...", p->host_no);
  }
#if 0
  download_consts[TMODE_NUMCMDS] = p->num_targetcmds;
#endif
  download_consts[TMODE_NUMCMDS] = 0;
  cur_patch = &sequencer_patches[0];
  downloaded = 0;
  skip_addr = 0;

  aic_outb(p, PERRORDIS|LOADRAM|FAILDIS|FASTMODE, SEQCTL);
  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);

  for (i = 0; i < sizeof(seqprog) / 4;  i++)
  {
    if (aic7xxx_check_patch(p, &cur_patch, i, &skip_addr) == 0)
    {
      
      continue;
    }
    aic7xxx_download_instr(p, i, &download_consts[0]);
    downloaded++;
  }

  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);
  aic_outb(p, FASTMODE | FAILDIS, SEQCTL);
  unpause_sequencer(p, TRUE);
  mdelay(1);
  pause_sequencer(p);
  aic_outb(p, FASTMODE, SEQCTL);
  if (aic7xxx_verbose & VERBOSE_PROBE)
  {
    printk(" %d instructions downloaded\n", downloaded);
  }
  if (aic7xxx_dump_sequencer)
    aic7xxx_print_sequencer(p, downloaded);
}

static void
aic7xxx_print_sequencer(struct aic7xxx_host *p, int downloaded)
{
  int i, k, temp;
  
  aic_outb(p, PERRORDIS|LOADRAM|FAILDIS|FASTMODE, SEQCTL);
  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);

  k = 0;
  for (i=0; i < downloaded; i++)
  {
    if ( k == 0 )
      printk("%03x: ", i);
    temp = aic_inb(p, SEQRAM);
    temp |= (aic_inb(p, SEQRAM) << 8);
    temp |= (aic_inb(p, SEQRAM) << 16);
    temp |= (aic_inb(p, SEQRAM) << 24);
    printk("%08x", temp);
    if ( ++k == 8 )
    {
      printk("\n");
      k = 0;
    }
    else
      printk(" ");
  }
  aic_outb(p, 0, SEQADDR0);
  aic_outb(p, 0, SEQADDR1);
  aic_outb(p, FASTMODE | FAILDIS, SEQCTL);
  unpause_sequencer(p, TRUE);
  mdelay(1);
  pause_sequencer(p);
  aic_outb(p, FASTMODE, SEQCTL);
  printk("\n");
}

static const char *
aic7xxx_info(struct Scsi_Host *dooh)
{
  static char buffer[256];
  char *bp;
  struct aic7xxx_host *p;

  bp = &buffer[0];
  p = (struct aic7xxx_host *)dooh->hostdata;
  memset(bp, 0, sizeof(buffer));
  strcpy(bp, "Adaptec AHA274x/284x/294x (EISA/VLB/PCI-Fast SCSI) ");
  strcat(bp, AIC7XXX_C_VERSION);
  strcat(bp, "/");
  strcat(bp, AIC7XXX_H_VERSION);
  strcat(bp, "\n");
  strcat(bp, "       <");
  strcat(bp, board_names[p->board_name_index]);
  strcat(bp, ">");

  return(bp);
}

static struct aic7xxx_syncrate *
aic7xxx_find_syncrate(struct aic7xxx_host *p, unsigned int *period,
  unsigned int maxsync, unsigned char *options)
{
  struct aic7xxx_syncrate *syncrate;
  int done = FALSE;

  switch(*options)
  {
    case MSG_EXT_PPR_OPTION_DT_CRC:
    case MSG_EXT_PPR_OPTION_DT_UNITS:
      if(!(p->features & AHC_ULTRA3))
      {
        *options = 0;
        maxsync = max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      }
      break;
    case MSG_EXT_PPR_OPTION_DT_CRC_QUICK:
    case MSG_EXT_PPR_OPTION_DT_UNITS_QUICK:
      if(!(p->features & AHC_ULTRA3))
      {
        *options = 0;
        maxsync = max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      }
      else
      {
        switch(*options)
        {
          case MSG_EXT_PPR_OPTION_DT_CRC_QUICK:
            *options = MSG_EXT_PPR_OPTION_DT_CRC;
            break;
          case MSG_EXT_PPR_OPTION_DT_UNITS_QUICK:
            *options = MSG_EXT_PPR_OPTION_DT_UNITS;
            break;
        }
      }
      break;
    default:
      *options = 0;
      maxsync = max_t(unsigned int, maxsync, AHC_SYNCRATE_ULTRA2);
      break;
  }
  syncrate = &aic7xxx_syncrates[maxsync];
  while ( (syncrate->rate[0] != NULL) &&
         (!(p->features & AHC_ULTRA2) || syncrate->sxfr_ultra2) )
  {
    if (*period <= syncrate->period) 
    {
      switch(*options)
      {
        case MSG_EXT_PPR_OPTION_DT_CRC:
        case MSG_EXT_PPR_OPTION_DT_UNITS:
          if(!(syncrate->sxfr_ultra2 & AHC_SYNCRATE_CRC))
          {
            done = TRUE;
            *options = 0;
            *period = syncrate->period;
          }
          else
          {
            done = TRUE;
            if(syncrate == &aic7xxx_syncrates[maxsync])
            {
              *period = syncrate->period;
            }
          }
          break;
        default:
          if(!(syncrate->sxfr_ultra2 & AHC_SYNCRATE_CRC))
          {
            done = TRUE;
            if(syncrate == &aic7xxx_syncrates[maxsync])
            {
              *period = syncrate->period;
            }
          }
          break;
      }
      if(done)
      {
        break;
      }
    }
    syncrate++;
  }
  if ( (*period == 0) || (syncrate->rate[0] == NULL) ||
       ((p->features & AHC_ULTRA2) && (syncrate->sxfr_ultra2 == 0)) )
  {
    *options = 0;
    *period = 255;
    syncrate = NULL;
  }
  return (syncrate);
}


static unsigned int
aic7xxx_find_period(struct aic7xxx_host *p, unsigned int scsirate,
  unsigned int maxsync)
{
  struct aic7xxx_syncrate *syncrate;

  if (p->features & AHC_ULTRA2)
  {
    scsirate &= SXFR_ULTRA2;
  }
  else
  {
    scsirate &= SXFR;
  }

  syncrate = &aic7xxx_syncrates[maxsync];
  while (syncrate->rate[0] != NULL)
  {
    if (p->features & AHC_ULTRA2)
    {
      if (syncrate->sxfr_ultra2 == 0)
        break;
      else if (scsirate == syncrate->sxfr_ultra2)
        return (syncrate->period);
      else if (scsirate == (syncrate->sxfr_ultra2 & ~AHC_SYNCRATE_CRC))
        return (syncrate->period);
    }
    else if (scsirate == (syncrate->sxfr & ~ULTRA_SXFR))
    {
      return (syncrate->period);
    }
    syncrate++;
  }
  return (0); 
}

static void
aic7xxx_validate_offset(struct aic7xxx_host *p,
  struct aic7xxx_syncrate *syncrate, unsigned int *offset, int wide)
{
  unsigned int maxoffset;

  
  if (syncrate == NULL)
  {
    maxoffset = 0;
  }
  else if (p->features & AHC_ULTRA2)
  {
    maxoffset = MAX_OFFSET_ULTRA2;
  }
  else
  {
    if (wide)
      maxoffset = MAX_OFFSET_16BIT;
    else
      maxoffset = MAX_OFFSET_8BIT;
  }
  *offset = min(*offset, maxoffset);
}

static void
aic7xxx_set_syncrate(struct aic7xxx_host *p, struct aic7xxx_syncrate *syncrate,
    int target, int channel, unsigned int period, unsigned int offset,
    unsigned char options, unsigned int type, struct aic_dev_data *aic_dev)
{
  unsigned char tindex;
  unsigned short target_mask;
  unsigned char lun, old_options;
  unsigned int old_period, old_offset;

  tindex = target | (channel << 3);
  target_mask = 0x01 << tindex;
  lun = aic_inb(p, SCB_TCL) & 0x07;

  if (syncrate == NULL)
  {
    period = 0;
    offset = 0;
  }

  old_period = aic_dev->cur.period;
  old_offset = aic_dev->cur.offset;
  old_options = aic_dev->cur.options;

  
  if (type & AHC_TRANS_CUR)
  {
    unsigned int scsirate;

    scsirate = aic_inb(p, TARG_SCSIRATE + tindex);
    if (p->features & AHC_ULTRA2)
    {
      scsirate &= ~SXFR_ULTRA2;
      if (syncrate != NULL)
      {
        switch(options)
        {
          case MSG_EXT_PPR_OPTION_DT_UNITS:
            scsirate |= (syncrate->sxfr_ultra2 & ~AHC_SYNCRATE_CRC);
            break;
          default:
            scsirate |= syncrate->sxfr_ultra2;
            break;
        }
      }
      if (type & AHC_TRANS_ACTIVE)
      {
        aic_outb(p, offset, SCSIOFFSET);
      }
      aic_outb(p, offset, TARG_OFFSET + tindex);
    }
    else 
    {
      scsirate &= ~(SXFR|SOFS);
      p->ultraenb &= ~target_mask;
      if (syncrate != NULL)
      {
        if (syncrate->sxfr & ULTRA_SXFR)
        {
          p->ultraenb |= target_mask;
        }
        scsirate |= (syncrate->sxfr & SXFR);
        scsirate |= (offset & SOFS);
      }
      if (type & AHC_TRANS_ACTIVE)
      {
        unsigned char sxfrctl0;

        sxfrctl0 = aic_inb(p, SXFRCTL0);
        sxfrctl0 &= ~FAST20;
        if (p->ultraenb & target_mask)
          sxfrctl0 |= FAST20;
        aic_outb(p, sxfrctl0, SXFRCTL0);
      }
      aic_outb(p, p->ultraenb & 0xff, ULTRA_ENB);
      aic_outb(p, (p->ultraenb >> 8) & 0xff, ULTRA_ENB + 1 );
    }
    if (type & AHC_TRANS_ACTIVE)
    {
      aic_outb(p, scsirate, SCSIRATE);
    }
    aic_outb(p, scsirate, TARG_SCSIRATE + tindex);
    aic_dev->cur.period = period;
    aic_dev->cur.offset = offset;
    aic_dev->cur.options = options;
    if ( !(type & AHC_TRANS_QUITE) &&
         (aic7xxx_verbose & VERBOSE_NEGOTIATION) &&
         (aic_dev->flags & DEVICE_PRINT_DTR) )
    {
      if (offset)
      {
        int rate_mod = (scsirate & WIDEXFER) ? 1 : 0;
      
        printk(INFO_LEAD "Synchronous at %s Mbyte/sec, "
               "offset %d.\n", p->host_no, channel, target, lun,
               syncrate->rate[rate_mod], offset);
      }
      else
      {
        printk(INFO_LEAD "Using asynchronous transfers.\n",
               p->host_no, channel, target, lun);
      }
      aic_dev->flags &= ~DEVICE_PRINT_DTR;
    }
  }

  if (type & AHC_TRANS_GOAL)
  {
    aic_dev->goal.period = period;
    aic_dev->goal.offset = offset;
    aic_dev->goal.options = options;
  }

  if (type & AHC_TRANS_USER)
  {
    p->user[tindex].period = period;
    p->user[tindex].offset = offset;
    p->user[tindex].options = options;
  }
}

static void
aic7xxx_set_width(struct aic7xxx_host *p, int target, int channel, int lun,
    unsigned int width, unsigned int type, struct aic_dev_data *aic_dev)
{
  unsigned char tindex;
  unsigned short target_mask;
  unsigned int old_width;

  tindex = target | (channel << 3);
  target_mask = 1 << tindex;
  
  old_width = aic_dev->cur.width;

  if (type & AHC_TRANS_CUR) 
  {
    unsigned char scsirate;

    scsirate = aic_inb(p, TARG_SCSIRATE + tindex);

    scsirate &= ~WIDEXFER;
    if (width == MSG_EXT_WDTR_BUS_16_BIT)
      scsirate |= WIDEXFER;

    aic_outb(p, scsirate, TARG_SCSIRATE + tindex);

    if (type & AHC_TRANS_ACTIVE)
      aic_outb(p, scsirate, SCSIRATE);

    aic_dev->cur.width = width;

    if ( !(type & AHC_TRANS_QUITE) &&
          (aic7xxx_verbose & VERBOSE_NEGOTIATION2) && 
          (aic_dev->flags & DEVICE_PRINT_DTR) )
    {
      printk(INFO_LEAD "Using %s transfers\n", p->host_no, channel, target,
        lun, (scsirate & WIDEXFER) ? "Wide(16bit)" : "Narrow(8bit)" );
    }
  }

  if (type & AHC_TRANS_GOAL)
    aic_dev->goal.width = width;
  if (type & AHC_TRANS_USER)
    p->user[tindex].width = width;

  if (aic_dev->goal.offset)
  {
    if (p->features & AHC_ULTRA2)
    {
      aic_dev->goal.offset = MAX_OFFSET_ULTRA2;
    }
    else if (width == MSG_EXT_WDTR_BUS_16_BIT)
    {
      aic_dev->goal.offset = MAX_OFFSET_16BIT;
    }
    else
    {
      aic_dev->goal.offset = MAX_OFFSET_8BIT;
    }
  }
}
      
static void
scbq_init(volatile scb_queue_type *queue)
{
  queue->head = NULL;
  queue->tail = NULL;
}

static inline void
scbq_insert_head(volatile scb_queue_type *queue, struct aic7xxx_scb *scb)
{
  scb->q_next = queue->head;
  queue->head = scb;
  if (queue->tail == NULL)       
    queue->tail = queue->head;
}

static inline struct aic7xxx_scb *
scbq_remove_head(volatile scb_queue_type *queue)
{
  struct aic7xxx_scb * scbp;

  scbp = queue->head;
  if (queue->head != NULL)
    queue->head = queue->head->q_next;
  if (queue->head == NULL)       
    queue->tail = NULL;
  return(scbp);
}

static inline void
scbq_remove(volatile scb_queue_type *queue, struct aic7xxx_scb *scb)
{
  if (queue->head == scb)
  {
    
    scbq_remove_head(queue);
  }
  else
  {
    struct aic7xxx_scb *curscb = queue->head;

    while ((curscb != NULL) && (curscb->q_next != scb))
    {
      curscb = curscb->q_next;
    }
    if (curscb != NULL)
    {
      
      curscb->q_next = scb->q_next;
      if (scb->q_next == NULL)
      {
        
        queue->tail = curscb;
      }
    }
  }
}

static inline void
scbq_insert_tail(volatile scb_queue_type *queue, struct aic7xxx_scb *scb)
{
  scb->q_next = NULL;
  if (queue->tail != NULL)       
    queue->tail->q_next = scb;
  queue->tail = scb;             
  if (queue->head == NULL)       
    queue->head = queue->tail;
}

static int
aic7xxx_match_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb,
    int target, int channel, int lun, unsigned char tag)
{
  int targ = (scb->hscb->target_channel_lun >> 4) & 0x0F;
  int chan = (scb->hscb->target_channel_lun >> 3) & 0x01;
  int slun = scb->hscb->target_channel_lun & 0x07;
  int match;

  match = ((chan == channel) || (channel == ALL_CHANNELS));
  if (match != 0)
    match = ((targ == target) || (target == ALL_TARGETS));
  if (match != 0)
    match = ((lun == slun) || (lun == ALL_LUNS));
  if (match != 0)
    match = ((tag == scb->hscb->tag) || (tag == SCB_LIST_NULL));

  return (match);
}

static void
aic7xxx_add_curscb_to_free_list(struct aic7xxx_host *p)
{
  aic_outb(p, SCB_LIST_NULL, SCB_TAG);
  aic_outb(p, 0, SCB_CONTROL);

  aic_outb(p, aic_inb(p, FREE_SCBH), SCB_NEXT);
  aic_outb(p, aic_inb(p, SCBPTR), FREE_SCBH);
}

static unsigned char
aic7xxx_rem_scb_from_disc_list(struct aic7xxx_host *p, unsigned char scbptr,
                               unsigned char prev)
{
  unsigned char next;

  aic_outb(p, scbptr, SCBPTR);
  next = aic_inb(p, SCB_NEXT);
  aic7xxx_add_curscb_to_free_list(p);

  if (prev != SCB_LIST_NULL)
  {
    aic_outb(p, prev, SCBPTR);
    aic_outb(p, next, SCB_NEXT);
  }
  else
  {
    aic_outb(p, next, DISCONNECTED_SCBH);
  }

  return next;
}

static inline void
aic7xxx_busy_target(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
  p->untagged_scbs[scb->hscb->target_channel_lun] = scb->hscb->tag;
}

static inline unsigned char
aic7xxx_index_busy_target(struct aic7xxx_host *p, unsigned char tcl,
    int unbusy)
{
  unsigned char busy_scbid;

  busy_scbid = p->untagged_scbs[tcl];
  if (unbusy)
  {
    p->untagged_scbs[tcl] = SCB_LIST_NULL;
  }
  return (busy_scbid);
}

static unsigned char
aic7xxx_find_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
  unsigned char saved_scbptr;
  unsigned char curindex;

  saved_scbptr = aic_inb(p, SCBPTR);
  curindex = 0;
  for (curindex = 0; curindex < p->scb_data->maxhscbs; curindex++)
  {
    aic_outb(p, curindex, SCBPTR);
    if (aic_inb(p, SCB_TAG) == scb->hscb->tag)
    {
      break;
    }
  }
  aic_outb(p, saved_scbptr, SCBPTR);
  if (curindex >= p->scb_data->maxhscbs)
  {
    curindex = SCB_LIST_NULL;
  }

  return (curindex);
}

static int
aic7xxx_allocate_scb(struct aic7xxx_host *p)
{
  struct aic7xxx_scb   *scbp = NULL;
  int scb_size = (sizeof (struct hw_scatterlist) * AIC7XXX_MAX_SG) + 12 + 6;
  int i;
  int step = PAGE_SIZE / 1024;
  unsigned long scb_count = 0;
  struct hw_scatterlist *hsgp;
  struct aic7xxx_scb *scb_ap;
  struct aic7xxx_scb_dma *scb_dma;
  unsigned char *bufs;

  if (p->scb_data->numscbs < p->scb_data->maxscbs)
  {
    for ( i=step;; i *= 2 )
    {
      if ( (scb_size * (i-1)) >= ( (PAGE_SIZE * (i/step)) - 64 ) )
      {
        i /= 2;
        break;
      }
    }
    scb_count = min( (i-1), p->scb_data->maxscbs - p->scb_data->numscbs);
    scb_ap = kmalloc(sizeof (struct aic7xxx_scb) * scb_count
					   + sizeof(struct aic7xxx_scb_dma), GFP_ATOMIC);
    if (scb_ap == NULL)
      return(0);
    scb_dma = (struct aic7xxx_scb_dma *)&scb_ap[scb_count];
    hsgp = (struct hw_scatterlist *)
      pci_alloc_consistent(p->pdev, scb_size * scb_count,
			   &scb_dma->dma_address);
    if (hsgp == NULL)
    {
      kfree(scb_ap);
      return(0);
    }
    bufs = (unsigned char *)&hsgp[scb_count * AIC7XXX_MAX_SG];
#ifdef AIC7XXX_VERBOSE_DEBUGGING
    if (aic7xxx_verbose > 0xffff)
    {
      if (p->scb_data->numscbs == 0)
	printk(INFO_LEAD "Allocating initial %ld SCB structures.\n",
	  p->host_no, -1, -1, -1, scb_count);
      else
	printk(INFO_LEAD "Allocating %ld additional SCB structures.\n",
	  p->host_no, -1, -1, -1, scb_count);
    }
#endif
    memset(scb_ap, 0, sizeof (struct aic7xxx_scb) * scb_count);
    scb_dma->dma_offset = (unsigned long)scb_dma->dma_address
			  - (unsigned long)hsgp;
    scb_dma->dma_len = scb_size * scb_count;
    for (i=0; i < scb_count; i++)
    {
      scbp = &scb_ap[i];
      scbp->hscb = &p->scb_data->hscbs[p->scb_data->numscbs];
      scbp->sg_list = &hsgp[i * AIC7XXX_MAX_SG];
      scbp->sense_cmd = bufs;
      scbp->cmnd = bufs + 6;
      bufs += 12 + 6;
      scbp->scb_dma = scb_dma;
      memset(scbp->hscb, 0, sizeof(struct aic7xxx_hwscb));
      scbp->hscb->tag = p->scb_data->numscbs;
      p->scb_data->scb_array[p->scb_data->numscbs++] = scbp;
      scbq_insert_tail(&p->scb_data->free_scbs, scbp);
    }
    scbp->kmalloc_ptr = scb_ap;
  }
  return(scb_count);
}

static void
aic7xxx_queue_cmd_complete(struct aic7xxx_host *p, struct scsi_cmnd *cmd)
{
  aic7xxx_position(cmd) = SCB_LIST_NULL;
  cmd->host_scribble = (char *)p->completeq.head;
  p->completeq.head = cmd;
}

static void aic7xxx_done_cmds_complete(struct aic7xxx_host *p)
{
	struct scsi_cmnd *cmd;

	while (p->completeq.head != NULL) {
		cmd = p->completeq.head;
		p->completeq.head = (struct scsi_cmnd *) cmd->host_scribble;
		cmd->host_scribble = NULL;
		cmd->scsi_done(cmd);
	}
}

static void
aic7xxx_free_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{

  scb->flags = SCB_FREE;
  scb->cmd = NULL;
  scb->sg_count = 0;
  scb->sg_length = 0;
  scb->tag_action = 0;
  scb->hscb->control = 0;
  scb->hscb->target_status = 0;
  scb->hscb->target_channel_lun = SCB_LIST_NULL;

  scbq_insert_head(&p->scb_data->free_scbs, scb);
}

static void
aic7xxx_done(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
	struct scsi_cmnd *cmd = scb->cmd;
	struct aic_dev_data *aic_dev = cmd->device->hostdata;
	int tindex = TARGET_INDEX(cmd);
	struct aic7xxx_scb *scbp;
	unsigned char queue_depth;

        scsi_dma_unmap(cmd);

  if (scb->flags & SCB_SENSE)
  {
    pci_unmap_single(p->pdev,
                     le32_to_cpu(scb->sg_list[0].address),
                     SCSI_SENSE_BUFFERSIZE,
                     PCI_DMA_FROMDEVICE);
  }
  if (scb->flags & SCB_RECOVERY_SCB)
  {
    p->flags &= ~AHC_ABORT_PENDING;
  }
  if (scb->flags & (SCB_RESET|SCB_ABORT))
  {
    cmd->result |= (DID_RESET << 16);
  }

  if ((scb->flags & SCB_MSGOUT_BITS) != 0)
  {
    unsigned short mask;
    int message_error = FALSE;

    mask = 0x01 << tindex;
 
    if ((scb->flags & SCB_SENSE) && 
          ((scb->cmd->sense_buffer[12] == 0x43) ||  
          (scb->cmd->sense_buffer[12] == 0x49))) 
    {
      message_error = TRUE;
    }

    if (scb->flags & SCB_MSGOUT_WDTR)
    {
      if (message_error)
      {
        if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
             (aic_dev->flags & DEVICE_PRINT_DTR) )
        {
          printk(INFO_LEAD "Device failed to complete Wide Negotiation "
            "processing and\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "returned a sense error code for invalid message, "
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "Wide negotiation to this device.\n", p->host_no,
            CTL_OF_SCB(scb));
        }
        aic_dev->needwdtr = aic_dev->needwdtr_copy = 0;
      }
    }
    if (scb->flags & SCB_MSGOUT_SDTR)
    {
      if (message_error)
      {
        if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
             (aic_dev->flags & DEVICE_PRINT_DTR) )
        {
          printk(INFO_LEAD "Device failed to complete Sync Negotiation "
            "processing and\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "returned a sense error code for invalid message, "
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "Sync negotiation to this device.\n", p->host_no,
            CTL_OF_SCB(scb));
          aic_dev->flags &= ~DEVICE_PRINT_DTR;
        }
        aic_dev->needsdtr = aic_dev->needsdtr_copy = 0;
      }
    }
    if (scb->flags & SCB_MSGOUT_PPR)
    {
      if(message_error)
      {
        if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
             (aic_dev->flags & DEVICE_PRINT_DTR) )
        {
          printk(INFO_LEAD "Device failed to complete Parallel Protocol "
            "Request processing and\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "returned a sense error code for invalid message, "
            "disabling future\n", p->host_no, CTL_OF_SCB(scb));
          printk(INFO_LEAD "Parallel Protocol Request negotiation to this "
            "device.\n", p->host_no, CTL_OF_SCB(scb));
        }
        aic_dev->needppr = aic_dev->needppr_copy = 0;
        aic_dev->needsdtr = aic_dev->needsdtr_copy = 1;
        aic_dev->needwdtr = aic_dev->needwdtr_copy = 1;
      }
    }
  }

  queue_depth = aic_dev->temp_q_depth;
  if (queue_depth >= aic_dev->active_cmds)
  {
    scbp = scbq_remove_head(&aic_dev->delayed_scbs);
    if (scbp)
    {
      if (queue_depth == 1)
      {
        scbq_insert_head(&p->waiting_scbs, scbp);
      }
      else
      {
        scbq_insert_tail(&p->waiting_scbs, scbp);
      }
#ifdef AIC7XXX_VERBOSE_DEBUGGING
      if (aic7xxx_verbose > 0xffff)
        printk(INFO_LEAD "Moving SCB from delayed to waiting queue.\n",
               p->host_no, CTL_OF_SCB(scbp));
#endif
      if (queue_depth > aic_dev->active_cmds)
      {
        scbp = scbq_remove_head(&aic_dev->delayed_scbs);
        if (scbp)
          scbq_insert_tail(&p->waiting_scbs, scbp);
      }
    }
  }
  if (!(scb->tag_action))
  {
    aic7xxx_index_busy_target(p, scb->hscb->target_channel_lun,
                               TRUE);
    if (cmd->device->simple_tags)
    {
      aic_dev->temp_q_depth = aic_dev->max_q_depth;
    }
  }
  if(scb->flags & SCB_DTR_SCB)
  {
    aic_dev->dtr_pending = 0;
  }
  aic_dev->active_cmds--;
  p->activescbs--;

  if ((scb->sg_length >= 512) && (((cmd->result >> 16) & 0xf) == DID_OK))
  {
    long *ptr;
    int x, i;


    if (rq_data_dir(cmd->request) == WRITE)
    {
      aic_dev->w_total++;
      ptr = aic_dev->w_bins;
    }
    else
    {
      aic_dev->r_total++;
      ptr = aic_dev->r_bins;
    }
    x = scb->sg_length;
    x >>= 10;
    for(i=0; i<6; i++)
    {
      x >>= 2;
      if(!x) {
        ptr[i]++;
	break;
      }
    }
    if(i == 6 && x)
      ptr[5]++;
  }
  aic7xxx_free_scb(p, scb);
  aic7xxx_queue_cmd_complete(p, cmd);

}

static void
aic7xxx_run_done_queue(struct aic7xxx_host *p,  int complete)
{
  struct aic7xxx_scb *scb;
  int i, found = 0;

  for (i = 0; i < p->scb_data->numscbs; i++)
  {
    scb = p->scb_data->scb_array[i];
    if (scb->flags & SCB_QUEUED_FOR_DONE)
    {
      if (scb->flags & SCB_QUEUE_FULL)
      {
	scb->cmd->result = QUEUE_FULL << 1;
      }
      else
      {
        if (aic7xxx_verbose & (VERBOSE_ABORT_PROCESS | VERBOSE_RESET_PROCESS))
          printk(INFO_LEAD "Aborting scb %d\n",
               p->host_no, CTL_OF_SCB(scb), scb->hscb->tag);
        scb->hscb->residual_SG_segment_count = 0;
        scb->hscb->residual_data_count[0] = 0;
        scb->hscb->residual_data_count[1] = 0;
        scb->hscb->residual_data_count[2] = 0;
      }
      found++;
      aic7xxx_done(p, scb);
    }
  }
  if (aic7xxx_verbose & (VERBOSE_ABORT_RETURN | VERBOSE_RESET_RETURN))
  {
    printk(INFO_LEAD "%d commands found and queued for "
        "completion.\n", p->host_no, -1, -1, -1, found);
  }
  if (complete)
  {
    aic7xxx_done_cmds_complete(p);
  }
}

static unsigned char
aic7xxx_abort_waiting_scb(struct aic7xxx_host *p, struct aic7xxx_scb *scb,
    unsigned char scbpos, unsigned char prev)
{
  unsigned char curscb, next;

  curscb = aic_inb(p, SCBPTR);
  aic_outb(p, scbpos, SCBPTR);
  next = aic_inb(p, SCB_NEXT);

  aic7xxx_add_curscb_to_free_list(p);

  if (prev == SCB_LIST_NULL)
  {
    aic_outb(p, next, WAITING_SCBH);
  }
  else
  {
    aic_outb(p, prev, SCBPTR);
    aic_outb(p, next, SCB_NEXT);
  }
  aic_outb(p, curscb, SCBPTR);
  return (next);
}

static int
aic7xxx_search_qinfifo(struct aic7xxx_host *p, int target, int channel,
    int lun, unsigned char tag, int flags, int requeue,
    volatile scb_queue_type *queue)
{
  int      found;
  unsigned char qinpos, qintail;
  struct aic7xxx_scb *scbp;

  found = 0;
  qinpos = aic_inb(p, QINPOS);
  qintail = p->qinfifonext;

  p->qinfifonext = qinpos;

  while (qinpos != qintail)
  {
    scbp = p->scb_data->scb_array[p->qinfifo[qinpos++]];
    if (aic7xxx_match_scb(p, scbp, target, channel, lun, tag))
    {
       if (requeue && (queue != NULL))
       {
         if (scbp->flags & SCB_WAITINGQ)
         {
           scbq_remove(queue, scbp);
           scbq_remove(&p->waiting_scbs, scbp);
           scbq_remove(&AIC_DEV(scbp->cmd)->delayed_scbs, scbp);
           AIC_DEV(scbp->cmd)->active_cmds++;
           p->activescbs++;
         }
         scbq_insert_tail(queue, scbp);
         AIC_DEV(scbp->cmd)->active_cmds--;
         p->activescbs--;
         scbp->flags |= SCB_WAITINGQ;
         if ( !(scbp->tag_action & TAG_ENB) )
         {
           aic7xxx_index_busy_target(p, scbp->hscb->target_channel_lun,
             TRUE);
         }
       }
       else if (requeue)
       {
         p->qinfifo[p->qinfifonext++] = scbp->hscb->tag;
       }
       else
       {
         scbp->flags = flags | (scbp->flags & SCB_RECOVERY_SCB);
         if (aic7xxx_index_busy_target(p, scbp->hscb->target_channel_lun,
                                       FALSE) == scbp->hscb->tag)
         {
           aic7xxx_index_busy_target(p, scbp->hscb->target_channel_lun,
             TRUE);
         }
       }
       found++;
    }
    else
    {
      p->qinfifo[p->qinfifonext++] = scbp->hscb->tag;
    }
  }
  qinpos = p->qinfifonext;
  while(qinpos != qintail)
  {
    p->qinfifo[qinpos++] = SCB_LIST_NULL;
  }
  if (p->features & AHC_QUEUE_REGS)
    aic_outb(p, p->qinfifonext, HNSCB_QOFF);
  else
    aic_outb(p, p->qinfifonext, KERNEL_QINPOS);

  return (found);
}

static int
aic7xxx_scb_on_qoutfifo(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
  int i=0;

  while(p->qoutfifo[(p->qoutfifonext + i) & 0xff ] != SCB_LIST_NULL)
  {
    if(p->qoutfifo[(p->qoutfifonext + i) & 0xff ] == scb->hscb->tag)
      return TRUE;
    else
      i++;
  }
  return FALSE;
}


static void
aic7xxx_reset_device(struct aic7xxx_host *p, int target, int channel,
                     int lun, unsigned char tag)
{
  struct aic7xxx_scb *scbp, *prev_scbp;
  struct scsi_device *sd;
  unsigned char active_scb, tcl, scb_tag;
  int i = 0, init_lists = FALSE;
  struct aic_dev_data *aic_dev;

  active_scb = aic_inb(p, SCBPTR);
  scb_tag = aic_inb(p, SCB_TAG);

  if (aic7xxx_verbose & (VERBOSE_RESET_PROCESS | VERBOSE_ABORT_PROCESS))
  {
    printk(INFO_LEAD "Reset device, hardware_scb %d,\n",
         p->host_no, channel, target, lun, active_scb);
    printk(INFO_LEAD "Current scb %d, SEQADDR 0x%x, LASTPHASE "
           "0x%x\n",
         p->host_no, channel, target, lun, scb_tag,
         aic_inb(p, SEQADDR0) | (aic_inb(p, SEQADDR1) << 8),
         aic_inb(p, LASTPHASE));
    printk(INFO_LEAD "SG_CACHEPTR 0x%x, SG_COUNT %d, SCSISIGI 0x%x\n",
         p->host_no, channel, target, lun,
         (p->features & AHC_ULTRA2) ?  aic_inb(p, SG_CACHEPTR) : 0,
         aic_inb(p, SG_COUNT), aic_inb(p, SCSISIGI));
    printk(INFO_LEAD "SSTAT0 0x%x, SSTAT1 0x%x, SSTAT2 0x%x\n",
         p->host_no, channel, target, lun, aic_inb(p, SSTAT0),
         aic_inb(p, SSTAT1), aic_inb(p, SSTAT2));
  }

  list_for_each_entry(aic_dev, &p->aic_devs, list)
  {
    if (aic7xxx_verbose & (VERBOSE_RESET_PROCESS | VERBOSE_ABORT_PROCESS))
      printk(INFO_LEAD "processing aic_dev %p\n", p->host_no, channel, target,
		    lun, aic_dev);
    sd = aic_dev->SDptr;

    if((target != ALL_TARGETS && target != sd->id) ||
       (channel != ALL_CHANNELS && channel != sd->channel))
      continue;
    if (aic7xxx_verbose & (VERBOSE_ABORT_PROCESS | VERBOSE_RESET_PROCESS))
        printk(INFO_LEAD "Cleaning up status information "
          "and delayed_scbs.\n", p->host_no, sd->channel, sd->id, sd->lun);
    aic_dev->flags &= ~BUS_DEVICE_RESET_PENDING;
    if ( tag == SCB_LIST_NULL )
    {
      aic_dev->dtr_pending = 0;
      aic_dev->needppr = aic_dev->needppr_copy;
      aic_dev->needsdtr = aic_dev->needsdtr_copy;
      aic_dev->needwdtr = aic_dev->needwdtr_copy;
      aic_dev->flags = DEVICE_PRINT_DTR;
      aic_dev->temp_q_depth = aic_dev->max_q_depth;
    }
    tcl = (sd->id << 4) | (sd->channel << 3) | sd->lun;
    if ( (aic7xxx_index_busy_target(p, tcl, FALSE) == tag) ||
         (tag == SCB_LIST_NULL) )
      aic7xxx_index_busy_target(p, tcl,  TRUE);
    prev_scbp = NULL; 
    scbp = aic_dev->delayed_scbs.head;
    while (scbp != NULL)
    {
      prev_scbp = scbp;
      scbp = scbp->q_next;
      if (aic7xxx_match_scb(p, prev_scbp, target, channel, lun, tag))
      {
        scbq_remove(&aic_dev->delayed_scbs, prev_scbp);
        if (prev_scbp->flags & SCB_WAITINGQ)
        {
          aic_dev->active_cmds++;
          p->activescbs++;
        }
        prev_scbp->flags &= ~(SCB_ACTIVE | SCB_WAITINGQ);
        prev_scbp->flags |= SCB_RESET | SCB_QUEUED_FOR_DONE;
      }
    }
  }

  if (aic7xxx_verbose & (VERBOSE_ABORT_PROCESS | VERBOSE_RESET_PROCESS))
    printk(INFO_LEAD "Cleaning QINFIFO.\n", p->host_no, channel, target, lun );
  aic7xxx_search_qinfifo(p, target, channel, lun, tag,
      SCB_RESET | SCB_QUEUED_FOR_DONE,  FALSE, NULL);

  if (aic7xxx_verbose & (VERBOSE_ABORT_PROCESS | VERBOSE_RESET_PROCESS))
    printk(INFO_LEAD "Cleaning waiting_scbs.\n", p->host_no, channel,
      target, lun );
  {
    struct aic7xxx_scb *scbp, *prev_scbp;

    prev_scbp = NULL; 
    scbp = p->waiting_scbs.head;
    while (scbp != NULL)
    {
      prev_scbp = scbp;
      scbp = scbp->q_next;
      if (aic7xxx_match_scb(p, prev_scbp, target, channel, lun, tag))
      {
        scbq_remove(&p->waiting_scbs, prev_scbp);
        if (prev_scbp->flags & SCB_WAITINGQ)
        {
          AIC_DEV(prev_scbp->cmd)->active_cmds++;
          p->activescbs++;
        }
        prev_scbp->flags &= ~(SCB_ACTIVE | SCB_WAITINGQ);
        prev_scbp->flags |= SCB_RESET | SCB_QUEUED_FOR_DONE;
      }
    }
  }


  if (aic7xxx_verbose & (VERBOSE_ABORT_PROCESS | VERBOSE_RESET_PROCESS))
    printk(INFO_LEAD "Cleaning waiting for selection "
      "list.\n", p->host_no, channel, target, lun);
  {
    unsigned char next, prev, scb_index;

    next = aic_inb(p, WAITING_SCBH);  
    prev = SCB_LIST_NULL;
    while (next != SCB_LIST_NULL)
    {
      aic_outb(p, next, SCBPTR);
      scb_index = aic_inb(p, SCB_TAG);
      if (scb_index >= p->scb_data->numscbs)
      {
        printk(WARN_LEAD "Waiting List inconsistency; SCB index=%d, "
          "numscbs=%d\n", p->host_no, channel, target, lun, scb_index,
          p->scb_data->numscbs);
        next = aic_inb(p, SCB_NEXT);
        aic7xxx_add_curscb_to_free_list(p);
      }
      else
      {
        scbp = p->scb_data->scb_array[scb_index];
        if (aic7xxx_match_scb(p, scbp, target, channel, lun, tag))
        {
          next = aic7xxx_abort_waiting_scb(p, scbp, next, prev);
          if (scbp->flags & SCB_WAITINGQ)
          {
            AIC_DEV(scbp->cmd)->active_cmds++;
            p->activescbs++;
          }
          scbp->flags &= ~(SCB_ACTIVE | SCB_WAITINGQ);
          scbp->flags |= SCB_RESET | SCB_QUEUED_FOR_DONE;
          if (prev == SCB_LIST_NULL)
          {
            aic_outb(p, aic_inb(p, SCSISEQ) & ~ENSELO, SCSISEQ);
            aic_outb(p, CLRSELTIMEO, CLRSINT1);
          }
        }
        else
        {
          prev = next;
          next = aic_inb(p, SCB_NEXT);
        }
      }
    }
  }

  if (aic7xxx_verbose & (VERBOSE_ABORT_PROCESS | VERBOSE_RESET_PROCESS))
    printk(INFO_LEAD "Cleaning disconnected scbs "
      "list.\n", p->host_no, channel, target, lun);
  if (p->flags & AHC_PAGESCBS)
  {
    unsigned char next, prev, scb_index;

    next = aic_inb(p, DISCONNECTED_SCBH);
    prev = SCB_LIST_NULL;
    while (next != SCB_LIST_NULL)
    {
      aic_outb(p, next, SCBPTR);
      scb_index = aic_inb(p, SCB_TAG);
      if (scb_index > p->scb_data->numscbs)
      {
        printk(WARN_LEAD "Disconnected List inconsistency; SCB index=%d, "
          "numscbs=%d\n", p->host_no, channel, target, lun, scb_index,
          p->scb_data->numscbs);
        next = aic7xxx_rem_scb_from_disc_list(p, next, prev);
      }
      else
      {
        scbp = p->scb_data->scb_array[scb_index];
        if (aic7xxx_match_scb(p, scbp, target, channel, lun, tag))
        {
          next = aic7xxx_rem_scb_from_disc_list(p, next, prev);
          if (scbp->flags & SCB_WAITINGQ)
          {
            AIC_DEV(scbp->cmd)->active_cmds++;
            p->activescbs++;
          }
          scbp->flags &= ~(SCB_ACTIVE | SCB_WAITINGQ);
          scbp->flags |= SCB_RESET | SCB_QUEUED_FOR_DONE;
          scbp->hscb->control = 0;
        }
        else
        {
          prev = next;
          next = aic_inb(p, SCB_NEXT);
        }
      }
    }
  }

  if (p->flags & AHC_PAGESCBS)
  {
    unsigned char next;

    next = aic_inb(p, FREE_SCBH);
    while (next != SCB_LIST_NULL)
    {
      aic_outb(p, next, SCBPTR);
      if (aic_inb(p, SCB_TAG) < p->scb_data->numscbs)
      {
        printk(WARN_LEAD "Free list inconsistency!.\n", p->host_no, channel,
          target, lun);
        init_lists = TRUE;
        next = SCB_LIST_NULL;
      }
      else
      {
        aic_outb(p, SCB_LIST_NULL, SCB_TAG);
        aic_outb(p, 0, SCB_CONTROL);
        next = aic_inb(p, SCB_NEXT);
      }
    }
  }

  if (init_lists)
  {
    aic_outb(p, SCB_LIST_NULL, FREE_SCBH);
    aic_outb(p, SCB_LIST_NULL, WAITING_SCBH);
    aic_outb(p, SCB_LIST_NULL, DISCONNECTED_SCBH);
  }
  for (i = p->scb_data->maxhscbs - 1; i >= 0; i--)
  {
    unsigned char scbid;

    aic_outb(p, i, SCBPTR);
    if (init_lists)
    {
      aic_outb(p, SCB_LIST_NULL, SCB_TAG);
      aic_outb(p, SCB_LIST_NULL, SCB_NEXT);
      aic_outb(p, 0, SCB_CONTROL);
      aic7xxx_add_curscb_to_free_list(p);
    }
    else
    {
      scbid = aic_inb(p, SCB_TAG);
      if (scbid < p->scb_data->numscbs)
      {
        scbp = p->scb_data->scb_array[scbid];
        if (aic7xxx_match_scb(p, scbp, target, channel, lun, tag))
        {
          aic_outb(p, 0, SCB_CONTROL);
          aic_outb(p, SCB_LIST_NULL, SCB_TAG);
          aic7xxx_add_curscb_to_free_list(p);
        }
      }
    }
  }

  for (i = 0; i < p->scb_data->numscbs; i++)
  {
    scbp = p->scb_data->scb_array[i];
    if ((scbp->flags & SCB_ACTIVE) &&
        aic7xxx_match_scb(p, scbp, target, channel, lun, tag) &&
        !aic7xxx_scb_on_qoutfifo(p, scbp))
    {
      if (scbp->flags & SCB_WAITINGQ)
      {
        scbq_remove(&p->waiting_scbs, scbp);
        scbq_remove(&AIC_DEV(scbp->cmd)->delayed_scbs, scbp);
        AIC_DEV(scbp->cmd)->active_cmds++;
        p->activescbs++;
      }
      scbp->flags |= SCB_RESET | SCB_QUEUED_FOR_DONE;
      scbp->flags &= ~(SCB_ACTIVE | SCB_WAITINGQ);
    }
  }

  aic_outb(p, active_scb, SCBPTR);
}


static void
aic7xxx_clear_intstat(struct aic7xxx_host *p)
{
  
  aic_outb(p, CLRSELDO | CLRSELDI | CLRSELINGO, CLRSINT0);
  aic_outb(p, CLRSELTIMEO | CLRATNO | CLRSCSIRSTI | CLRBUSFREE | CLRSCSIPERR |
       CLRPHASECHG | CLRREQINIT, CLRSINT1);
  aic_outb(p, CLRSCSIINT | CLRSEQINT | CLRBRKADRINT | CLRPARERR, CLRINT);
}

static void
aic7xxx_reset_current_bus(struct aic7xxx_host *p)
{

  
  aic_outb(p, aic_inb(p, SIMODE1) & ~ENSCSIRST, SIMODE1);


  
  aic_outb(p, aic_inb(p, SCSISEQ) | SCSIRSTO, SCSISEQ);
  while ( (aic_inb(p, SCSISEQ) & SCSIRSTO) == 0)
    mdelay(5);

  if (p->features & AHC_ULTRA2)
    mdelay(250);
  else
    mdelay(50);

  
  aic_outb(p, 0, SCSISEQ);
  mdelay(10);

  aic7xxx_clear_intstat(p);
  
  aic_outb(p, aic_inb(p, SIMODE1) | ENSCSIRST, SIMODE1);

}

static void
aic7xxx_reset_channel(struct aic7xxx_host *p, int channel, int initiate_reset)
{
  unsigned long offset_min, offset_max;
  unsigned char sblkctl;
  int cur_channel;

  if (aic7xxx_verbose & VERBOSE_RESET_PROCESS)
    printk(INFO_LEAD "Reset channel called, %s initiate reset.\n",
      p->host_no, channel, -1, -1, (initiate_reset==TRUE) ? "will" : "won't" );


  if (channel == 1)
  {
    offset_min = 8;
    offset_max = 16;
  }
  else
  {
    if (p->features & AHC_TWIN)
    {
      
      offset_min = 0;
      offset_max = 8;
    }
    else
    {
      offset_min = 0;
      if (p->features & AHC_WIDE)
      {
        offset_max = 16;
      }
      else
      {
        offset_max = 8;
      }
    }
  }

  while (offset_min < offset_max)
  {
    aic_outb(p, 0, TARG_SCSIRATE + offset_min);
    if (p->features & AHC_ULTRA2)
    {
      aic_outb(p, 0, TARG_OFFSET + offset_min);
    }
    offset_min++;
  }

  sblkctl = aic_inb(p, SBLKCTL);
  if ( (p->chip & AHC_CHIPID_MASK) == AHC_AIC7770 )
    cur_channel = (sblkctl & SELBUSB) >> 3;
  else
    cur_channel = 0;
  if ( (cur_channel != channel) && (p->features & AHC_TWIN) )
  {
    if (aic7xxx_verbose & VERBOSE_RESET_PROCESS)
      printk(INFO_LEAD "Stealthily resetting idle channel.\n", p->host_no,
        channel, -1, -1);
    aic_outb(p, sblkctl ^ SELBUSB, SBLKCTL);
    aic_outb(p, aic_inb(p, SIMODE1) & ~ENBUSFREE, SIMODE1);
    if (initiate_reset)
    {
      aic7xxx_reset_current_bus(p);
    }
    aic_outb(p, aic_inb(p, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP), SCSISEQ);
    aic7xxx_clear_intstat(p);
    aic_outb(p, sblkctl, SBLKCTL);
  }
  else
  {
    if (aic7xxx_verbose & VERBOSE_RESET_PROCESS)
      printk(INFO_LEAD "Resetting currently active channel.\n", p->host_no,
        channel, -1, -1);
    aic_outb(p, aic_inb(p, SIMODE1) & ~(ENBUSFREE|ENREQINIT),
      SIMODE1);
    p->flags &= ~AHC_HANDLING_REQINITS;
    p->msg_type = MSG_TYPE_NONE;
    p->msg_len = 0;
    if (initiate_reset)
    {
      aic7xxx_reset_current_bus(p);
    }
    aic_outb(p, aic_inb(p, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP), SCSISEQ);
    aic7xxx_clear_intstat(p);
  }
  if (aic7xxx_verbose & VERBOSE_RESET_RETURN)
    printk(INFO_LEAD "Channel reset\n", p->host_no, channel, -1, -1);
  aic7xxx_reset_device(p, ALL_TARGETS, channel, ALL_LUNS, SCB_LIST_NULL);

  if ( !(p->features & AHC_TWIN) )
  {
    restart_sequencer(p);
  }

  return;
}

static void
aic7xxx_run_waiting_queues(struct aic7xxx_host *p)
{
  struct aic7xxx_scb *scb;
  struct aic_dev_data *aic_dev;
  int sent;


  if (p->waiting_scbs.head == NULL)
    return;

  sent = 0;

  while ((scb = scbq_remove_head(&p->waiting_scbs)) != NULL)
  {
    aic_dev = scb->cmd->device->hostdata;
    if ( !scb->tag_action )
    {
      aic_dev->temp_q_depth = 1;
    }
    if ( aic_dev->active_cmds >= aic_dev->temp_q_depth)
    {
      scbq_insert_tail(&aic_dev->delayed_scbs, scb);
    }
    else
    {
        scb->flags &= ~SCB_WAITINGQ;
        aic_dev->active_cmds++;
        p->activescbs++;
        if ( !(scb->tag_action) )
        {
          aic7xxx_busy_target(p, scb);
        }
        p->qinfifo[p->qinfifonext++] = scb->hscb->tag;
        sent++;
    }
  }
  if (sent)
  {
    if (p->features & AHC_QUEUE_REGS)
      aic_outb(p, p->qinfifonext, HNSCB_QOFF);
    else
    {
      pause_sequencer(p);
      aic_outb(p, p->qinfifonext, KERNEL_QINPOS);
      unpause_sequencer(p, FALSE);
    }
    if (p->activescbs > p->max_activescbs)
      p->max_activescbs = p->activescbs;
  }
}

#ifdef CONFIG_PCI

#define  DPE 0x80
#define  SSE 0x40
#define  RMA 0x20
#define  RTA 0x10
#define  STA 0x08
#define  DPR 0x01

static void
aic7xxx_pci_intr(struct aic7xxx_host *p)
{
  unsigned char status1;

  pci_read_config_byte(p->pdev, PCI_STATUS + 1, &status1);

  if ( (status1 & DPE) && (aic7xxx_verbose & VERBOSE_MINOR_ERROR) )
    printk(WARN_LEAD "Data Parity Error during PCI address or PCI write"
      "phase.\n", p->host_no, -1, -1, -1);
  if ( (status1 & SSE) && (aic7xxx_verbose & VERBOSE_MINOR_ERROR) )
    printk(WARN_LEAD "Signal System Error Detected\n", p->host_no,
      -1, -1, -1);
  if ( (status1 & RMA) && (aic7xxx_verbose & VERBOSE_MINOR_ERROR) )
    printk(WARN_LEAD "Received a PCI Master Abort\n", p->host_no,
      -1, -1, -1);
  if ( (status1 & RTA) && (aic7xxx_verbose & VERBOSE_MINOR_ERROR) )
    printk(WARN_LEAD "Received a PCI Target Abort\n", p->host_no,
      -1, -1, -1);
  if ( (status1 & STA) && (aic7xxx_verbose & VERBOSE_MINOR_ERROR) )
    printk(WARN_LEAD "Signaled a PCI Target Abort\n", p->host_no,
      -1, -1, -1);
  if ( (status1 & DPR) && (aic7xxx_verbose & VERBOSE_MINOR_ERROR) )
    printk(WARN_LEAD "Data Parity Error has been reported via PCI pin "
      "PERR#\n", p->host_no, -1, -1, -1);
  
  pci_write_config_byte(p->pdev, PCI_STATUS + 1, status1);
  if (status1 & (DPR|RMA|RTA))
    aic_outb(p,  CLRPARERR, CLRINT);

  if ( (aic7xxx_panic_on_abort) && (p->spurious_int > 500) )
    aic7xxx_panic_abort(p, NULL);

}
#endif 

static void
aic7xxx_construct_ppr(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
  p->msg_buf[p->msg_index++] = MSG_EXTENDED;
  p->msg_buf[p->msg_index++] = MSG_EXT_PPR_LEN;
  p->msg_buf[p->msg_index++] = MSG_EXT_PPR;
  p->msg_buf[p->msg_index++] = AIC_DEV(scb->cmd)->goal.period;
  p->msg_buf[p->msg_index++] = 0;
  p->msg_buf[p->msg_index++] = AIC_DEV(scb->cmd)->goal.offset;
  p->msg_buf[p->msg_index++] = AIC_DEV(scb->cmd)->goal.width;
  p->msg_buf[p->msg_index++] = AIC_DEV(scb->cmd)->goal.options;
  p->msg_len += 8;
}

static void
aic7xxx_construct_sdtr(struct aic7xxx_host *p, unsigned char period,
        unsigned char offset)
{
  p->msg_buf[p->msg_index++] = MSG_EXTENDED;
  p->msg_buf[p->msg_index++] = MSG_EXT_SDTR_LEN;
  p->msg_buf[p->msg_index++] = MSG_EXT_SDTR;
  p->msg_buf[p->msg_index++] = period;
  p->msg_buf[p->msg_index++] = offset;
  p->msg_len += 5;
}

static void
aic7xxx_construct_wdtr(struct aic7xxx_host *p, unsigned char bus_width)
{
  p->msg_buf[p->msg_index++] = MSG_EXTENDED;
  p->msg_buf[p->msg_index++] = MSG_EXT_WDTR_LEN;
  p->msg_buf[p->msg_index++] = MSG_EXT_WDTR;
  p->msg_buf[p->msg_index++] = bus_width;
  p->msg_len += 4;
}

static void
aic7xxx_calculate_residual (struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
	struct aic7xxx_hwscb *hscb;
	struct scsi_cmnd *cmd;
	int actual, i;

  cmd = scb->cmd;
  hscb = scb->hscb;

  if (((scb->hscb->control & DISCONNECTED) == 0) &&
      (scb->flags & SCB_SENSE) == 0)
  {
    actual = scb->sg_length;
    for (i=1; i < hscb->residual_SG_segment_count; i++)
    {
      actual -= scb->sg_list[scb->sg_count - i].length;
    }
    actual -= (hscb->residual_data_count[2] << 16) |
              (hscb->residual_data_count[1] <<  8) |
              hscb->residual_data_count[0];

    if (actual < cmd->underflow)
    {
      if (aic7xxx_verbose & VERBOSE_MINOR_ERROR)
      {
        printk(INFO_LEAD "Underflow - Wanted %u, %s %u, residual SG "
          "count %d.\n", p->host_no, CTL_OF_SCB(scb), cmd->underflow,
          (rq_data_dir(cmd->request) == WRITE) ? "wrote" : "read", actual,
          hscb->residual_SG_segment_count);
        printk(INFO_LEAD "status 0x%x.\n", p->host_no, CTL_OF_SCB(scb),
          hscb->target_status);
      }
      scsi_set_resid(cmd, scb->sg_length - actual);
      aic7xxx_status(cmd) = hscb->target_status;
    }
  }

  hscb->residual_data_count[2] = 0;
  hscb->residual_data_count[1] = 0;
  hscb->residual_data_count[0] = 0;
  hscb->residual_SG_segment_count = 0;
}

static void
aic7xxx_handle_device_reset(struct aic7xxx_host *p, int target, int channel)
{
  unsigned char tindex = target;

  tindex |= ((channel & 0x01) << 3);

  aic_outb(p, 0, TARG_SCSIRATE + tindex);
  if (p->features & AHC_ULTRA2)
    aic_outb(p, 0, TARG_OFFSET + tindex);
  aic7xxx_reset_device(p, target, channel, ALL_LUNS, SCB_LIST_NULL);
  if (aic7xxx_verbose & VERBOSE_RESET_PROCESS)
    printk(INFO_LEAD "Bus Device Reset delivered.\n", p->host_no, channel,
      target, -1);
  aic7xxx_run_done_queue(p,  TRUE);
}

static void
aic7xxx_handle_seqint(struct aic7xxx_host *p, unsigned char intstat)
{
  struct aic7xxx_scb *scb;
  struct aic_dev_data *aic_dev;
  unsigned short target_mask;
  unsigned char target, lun, tindex;
  unsigned char queue_flag = FALSE;
  char channel;
  int result;

  target = ((aic_inb(p, SAVED_TCL) >> 4) & 0x0f);
  if ( (p->chip & AHC_CHIPID_MASK) == AHC_AIC7770 )
    channel = (aic_inb(p, SBLKCTL) & SELBUSB) >> 3;
  else
    channel = 0;
  tindex = target + (channel << 3);
  lun = aic_inb(p, SAVED_TCL) & 0x07;
  target_mask = (0x01 << tindex);

  aic_outb(p, CLRSEQINT, CLRINT);
  switch (intstat & SEQINT_MASK)
  {
    case NO_MATCH:
      {
        aic_outb(p, aic_inb(p, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP),
                 SCSISEQ);
        printk(WARN_LEAD "No active SCB for reconnecting target - Issuing "
               "BUS DEVICE RESET.\n", p->host_no, channel, target, lun);
        printk(WARN_LEAD "      SAVED_TCL=0x%x, ARG_1=0x%x, SEQADDR=0x%x\n",
               p->host_no, channel, target, lun,
               aic_inb(p, SAVED_TCL), aic_inb(p, ARG_1),
               (aic_inb(p, SEQADDR1) << 8) | aic_inb(p, SEQADDR0));
        if (aic7xxx_panic_on_abort)
          aic7xxx_panic_abort(p, NULL);
      }
      break;

    case SEND_REJECT:
      {
        if (aic7xxx_verbose & VERBOSE_MINOR_ERROR)
          printk(INFO_LEAD "Rejecting unknown message (0x%x) received from "
            "target, SEQ_FLAGS=0x%x\n", p->host_no, channel, target, lun,
            aic_inb(p, ACCUM), aic_inb(p, SEQ_FLAGS));
      }
      break;

    case NO_IDENT:
      {
        if (aic7xxx_verbose & (VERBOSE_SEQINT | VERBOSE_RESET_MID))
          printk(INFO_LEAD "Target did not send an IDENTIFY message; "
            "LASTPHASE 0x%x, SAVED_TCL 0x%x\n", p->host_no, channel, target,
            lun, aic_inb(p, LASTPHASE), aic_inb(p, SAVED_TCL));

        aic7xxx_reset_channel(p, channel,  TRUE);
        aic7xxx_run_done_queue(p, TRUE);

      }
      break;

    case BAD_PHASE:
      if (aic_inb(p, LASTPHASE) == P_BUSFREE)
      {
        if (aic7xxx_verbose & VERBOSE_SEQINT)
          printk(INFO_LEAD "Missed busfree.\n", p->host_no, channel,
            target, lun);
        restart_sequencer(p);
      }
      else
      {
        if (aic7xxx_verbose & VERBOSE_SEQINT)
          printk(INFO_LEAD "Unknown scsi bus phase, continuing\n", p->host_no,
            channel, target, lun);
      }
      break;

    case EXTENDED_MSG:
      {
        p->msg_type = MSG_TYPE_INITIATOR_MSGIN;
        p->msg_len = 0;
        p->msg_index = 0;

#ifdef AIC7XXX_VERBOSE_DEBUGGING
        if (aic7xxx_verbose > 0xffff)
          printk(INFO_LEAD "Enabling REQINITs for MSG_IN\n", p->host_no,
                 channel, target, lun);
#endif

        p->flags |= AHC_HANDLING_REQINITS;
        aic_outb(p, aic_inb(p, SIMODE1) | ENREQINIT, SIMODE1);

        return;
      }

    case REJECT_MSG:
      {
        unsigned char scb_index;
        unsigned char last_msg;

        scb_index = aic_inb(p, SCB_TAG);
        scb = p->scb_data->scb_array[scb_index];
	aic_dev = AIC_DEV(scb->cmd);
        last_msg = aic_inb(p, LAST_MSG);

        if ( (last_msg == MSG_IDENTIFYFLAG) &&
             (scb->tag_action) &&
            !(scb->flags & SCB_MSGOUT_BITS) )
        {
          if (scb->tag_action == MSG_ORDERED_Q_TAG)
          {
	    scsi_adjust_queue_depth(scb->cmd->device, MSG_SIMPLE_TAG,
			    scb->cmd->device->queue_depth);
            scb->tag_action = MSG_SIMPLE_Q_TAG;
            scb->hscb->control &= ~SCB_TAG_TYPE;
            scb->hscb->control |= MSG_SIMPLE_Q_TAG;
            aic_outb(p, scb->hscb->control, SCB_CONTROL);
            aic_outb(p, MSG_IDENTIFYFLAG, MSG_OUT);
            aic_outb(p, aic_inb(p, SCSISIGI) | ATNO, SCSISIGO);
          }
          else if (scb->tag_action == MSG_SIMPLE_Q_TAG)
          {
            unsigned char i;
            struct aic7xxx_scb *scbp;
            int old_verbose;
	    scsi_adjust_queue_depth(scb->cmd->device, 0 ,
			    p->host->cmd_per_lun);
            aic_dev->max_q_depth = aic_dev->temp_q_depth = 1;
            scb->tag_action = 0;
            scb->hscb->control &= ~(TAG_ENB | SCB_TAG_TYPE);
            aic_outb(p,  scb->hscb->control, SCB_CONTROL);

            old_verbose = aic7xxx_verbose;
            aic7xxx_verbose &= ~(VERBOSE_RESET|VERBOSE_ABORT);
            for (i=0; i < p->scb_data->numscbs; i++)
            {
              scbp = p->scb_data->scb_array[i];
              if ((scbp->flags & SCB_ACTIVE) && (scbp != scb))
              {
                if (aic7xxx_match_scb(p, scbp, target, channel, lun, i))
                {
                  aic7xxx_reset_device(p, target, channel, lun, i);
                }
              }
            }
            aic7xxx_run_done_queue(p, TRUE);
            aic7xxx_verbose = old_verbose;
            aic7xxx_busy_target(p, scb);
            printk(INFO_LEAD "Device is refusing tagged commands, using "
              "untagged I/O.\n", p->host_no, channel, target, lun);
            aic_outb(p, MSG_IDENTIFYFLAG, MSG_OUT);
            aic_outb(p, aic_inb(p, SCSISIGI) | ATNO, SCSISIGO);
          }
        }
        else if (scb->flags & SCB_MSGOUT_PPR)
        {
          aic_dev->needppr = aic_dev->needppr_copy = 0;
          aic7xxx_set_width(p, target, channel, lun, MSG_EXT_WDTR_BUS_8_BIT,
            (AHC_TRANS_ACTIVE|AHC_TRANS_CUR|AHC_TRANS_QUITE), aic_dev);
          aic7xxx_set_syncrate(p, NULL, target, channel, 0, 0, 0,
                               AHC_TRANS_ACTIVE|AHC_TRANS_CUR|AHC_TRANS_QUITE,
			       aic_dev);
          aic_dev->goal.options = aic_dev->dtr_pending = 0;
          scb->flags &= ~SCB_MSGOUT_BITS;
          if(aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Device is rejecting PPR messages, falling "
              "back.\n", p->host_no, channel, target, lun);
          }
          if ( aic_dev->goal.width )
          {
            aic_dev->needwdtr = aic_dev->needwdtr_copy = 1;
            aic_dev->dtr_pending = 1;
            scb->flags |= SCB_MSGOUT_WDTR;
          }
          if ( aic_dev->goal.offset )
          {
            aic_dev->needsdtr = aic_dev->needsdtr_copy = 1;
            if( !aic_dev->dtr_pending )
            {
              aic_dev->dtr_pending = 1;
              scb->flags |= SCB_MSGOUT_SDTR;
            }
          }
          if ( aic_dev->dtr_pending )
          {
            aic_outb(p, HOST_MSG, MSG_OUT);
            aic_outb(p, aic_inb(p, SCSISIGI) | ATNO, SCSISIGO);
          }
        }
        else if (scb->flags & SCB_MSGOUT_WDTR)
        {
          aic_dev->needwdtr = aic_dev->needwdtr_copy = 0;
          scb->flags &= ~SCB_MSGOUT_BITS;
          aic7xxx_set_width(p, target, channel, lun, MSG_EXT_WDTR_BUS_8_BIT,
            (AHC_TRANS_ACTIVE|AHC_TRANS_GOAL|AHC_TRANS_CUR), aic_dev);
          aic7xxx_set_syncrate(p, NULL, target, channel, 0, 0, 0,
                               AHC_TRANS_ACTIVE|AHC_TRANS_CUR|AHC_TRANS_QUITE,
			       aic_dev);
          if(aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Device is rejecting WDTR messages, using "
              "narrow transfers.\n", p->host_no, channel, target, lun);
          }
          aic_dev->needsdtr = aic_dev->needsdtr_copy;
        }
        else if (scb->flags & SCB_MSGOUT_SDTR)
        {
          aic_dev->needsdtr = aic_dev->needsdtr_copy = 0;
          scb->flags &= ~SCB_MSGOUT_BITS;
          aic7xxx_set_syncrate(p, NULL, target, channel, 0, 0, 0,
            (AHC_TRANS_CUR|AHC_TRANS_ACTIVE|AHC_TRANS_GOAL), aic_dev);
          if(aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Device is rejecting SDTR messages, using "
              "async transfers.\n", p->host_no, channel, target, lun);
          }
        }
        else if (aic7xxx_verbose & VERBOSE_SEQINT)
        {
          printk(INFO_LEAD "Received MESSAGE_REJECT for unknown cause.  "
            "Ignoring.\n", p->host_no, channel, target, lun);
        }
      }
      break;

    case BAD_STATUS:
      {
	unsigned char scb_index;
	struct aic7xxx_hwscb *hscb;
	struct scsi_cmnd *cmd;

        aic_outb(p, 0, RETURN_1);
        scb_index = aic_inb(p, SCB_TAG);
        if (scb_index > p->scb_data->numscbs)
        {
          printk(WARN_LEAD "Invalid SCB during SEQINT 0x%02x, SCB_TAG %d.\n",
            p->host_no, channel, target, lun, intstat, scb_index);
          break;
        }
        scb = p->scb_data->scb_array[scb_index];
        hscb = scb->hscb;

        if (!(scb->flags & SCB_ACTIVE) || (scb->cmd == NULL))
        {
          printk(WARN_LEAD "Invalid SCB during SEQINT 0x%x, scb %d, flags 0x%x,"
            " cmd 0x%lx.\n", p->host_no, channel, target, lun, intstat,
            scb_index, scb->flags, (unsigned long) scb->cmd);
        }
        else
        {
          cmd = scb->cmd;
	  aic_dev = AIC_DEV(scb->cmd);
          hscb->target_status = aic_inb(p, SCB_TARGET_STATUS);
          aic7xxx_status(cmd) = hscb->target_status;

          cmd->result = hscb->target_status;

          switch (status_byte(hscb->target_status))
          {
            case GOOD:
              if (aic7xxx_verbose & VERBOSE_SEQINT)
                printk(INFO_LEAD "Interrupted for status of GOOD???\n",
                  p->host_no, CTL_OF_SCB(scb));
              break;

            case COMMAND_TERMINATED:
            case CHECK_CONDITION:
              if ( !(scb->flags & SCB_SENSE) )
              {
                memcpy(scb->sense_cmd, &generic_sense[0],
                       sizeof(generic_sense));

                scb->sense_cmd[1] = (cmd->device->lun << 5);
                scb->sense_cmd[4] = SCSI_SENSE_BUFFERSIZE;

                scb->sg_list[0].length = 
                  cpu_to_le32(SCSI_SENSE_BUFFERSIZE);
		scb->sg_list[0].address =
                        cpu_to_le32(pci_map_single(p->pdev, cmd->sense_buffer,
                                                   SCSI_SENSE_BUFFERSIZE,
                                                   PCI_DMA_FROMDEVICE));

                
                hscb->control = 0;
                hscb->target_status = 0;
                hscb->SG_list_pointer = 
		  cpu_to_le32(SCB_DMA_ADDR(scb, scb->sg_list));
                hscb->SCSI_cmd_pointer = 
                  cpu_to_le32(SCB_DMA_ADDR(scb, scb->sense_cmd));
                hscb->data_count = scb->sg_list[0].length;
                hscb->data_pointer = scb->sg_list[0].address;
                hscb->SCSI_cmd_length = COMMAND_SIZE(scb->sense_cmd[0]);
                hscb->residual_SG_segment_count = 0;
                hscb->residual_data_count[0] = 0;
                hscb->residual_data_count[1] = 0;
                hscb->residual_data_count[2] = 0;

                scb->sg_count = hscb->SG_segment_count = 1;
                scb->sg_length = SCSI_SENSE_BUFFERSIZE;
                scb->tag_action = 0;
                scb->flags |= SCB_SENSE;
#ifdef AIC7XXX_VERBOSE_DEBUGGING
                if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
                {
                  if (scb->flags & SCB_MSGOUT_BITS)
                    printk(INFO_LEAD "Requesting SENSE with %s\n", p->host_no,
                           CTL_OF_SCB(scb), (scb->flags & SCB_MSGOUT_SDTR) ?
                           "SDTR" : "WDTR");
                  else
                    printk(INFO_LEAD "Requesting SENSE, no MSG\n", p->host_no,
                           CTL_OF_SCB(scb));
                }
#endif
                aic7xxx_busy_target(p, scb);
                aic_outb(p, SEND_SENSE, RETURN_1);
                aic7xxx_error(cmd) = DID_OK;
                break;
              }  
              printk(INFO_LEAD "CHECK_CONDITION on REQUEST_SENSE, returning "
                     "an error.\n", p->host_no, CTL_OF_SCB(scb));
              aic7xxx_error(cmd) = DID_ERROR;
              scb->flags &= ~SCB_SENSE;
              break;

            case QUEUE_FULL:
              queue_flag = TRUE;    
            case BUSY:              
            {
              struct aic7xxx_scb *next_scbp, *prev_scbp;
              unsigned char active_hscb, next_hscb, prev_hscb, scb_index;
              next_scbp = p->waiting_scbs.head;
              while ( next_scbp != NULL )
              {
                prev_scbp = next_scbp;
                next_scbp = next_scbp->q_next;
                if ( aic7xxx_match_scb(p, prev_scbp, target, channel, lun,
                     SCB_LIST_NULL) )
                {
                  scbq_remove(&p->waiting_scbs, prev_scbp);
		  scb->flags = SCB_QUEUED_FOR_DONE | SCB_QUEUE_FULL;
		  p->activescbs++;
		  aic_dev->active_cmds++;
                }
              }
              aic7xxx_search_qinfifo(p, target, channel, lun,
                SCB_LIST_NULL, SCB_QUEUED_FOR_DONE | SCB_QUEUE_FULL,
	       	FALSE, NULL);
              next_scbp = NULL;
              active_hscb = aic_inb(p, SCBPTR);
              prev_hscb = next_hscb = scb_index = SCB_LIST_NULL;
              next_hscb = aic_inb(p, WAITING_SCBH);
              while (next_hscb != SCB_LIST_NULL)
              {
                aic_outb(p, next_hscb, SCBPTR);
                scb_index = aic_inb(p, SCB_TAG);
                if (scb_index < p->scb_data->numscbs)
                {
                  next_scbp = p->scb_data->scb_array[scb_index];
                  if (aic7xxx_match_scb(p, next_scbp, target, channel, lun,
                      SCB_LIST_NULL) )
                  {
		    next_scbp->flags = SCB_QUEUED_FOR_DONE | SCB_QUEUE_FULL;
                    next_hscb = aic_inb(p, SCB_NEXT);
                    aic_outb(p, 0, SCB_CONTROL);
                    aic_outb(p, SCB_LIST_NULL, SCB_TAG);
                    aic7xxx_add_curscb_to_free_list(p);
                    if (prev_hscb == SCB_LIST_NULL)
                    {
                      aic_outb(p, aic_inb(p, SCSISEQ) & ~ENSELO, SCSISEQ);
                      aic_outb(p, CLRSELTIMEO, CLRSINT1);
                      aic_outb(p, next_hscb, WAITING_SCBH);
                    }
                    else
                    {
                      aic_outb(p, prev_hscb, SCBPTR);
                      aic_outb(p, next_hscb, SCB_NEXT);
                    }
                  }
                  else
                  {
                    prev_hscb = next_hscb;
                    next_hscb = aic_inb(p, SCB_NEXT);
                  }
                } 
              }
              aic_outb(p, active_hscb, SCBPTR);
	      aic7xxx_run_done_queue(p, FALSE);
                  
#ifdef AIC7XXX_VERBOSE_DEBUGGING
              if( (aic7xxx_verbose & VERBOSE_MINOR_ERROR) ||
                  (aic7xxx_verbose > 0xffff) )
              {
                if (queue_flag)
                  printk(INFO_LEAD "Queue full received; queue depth %d, "
                    "active %d\n", p->host_no, CTL_OF_SCB(scb),
                    aic_dev->max_q_depth, aic_dev->active_cmds);
                else
                  printk(INFO_LEAD "Target busy\n", p->host_no, CTL_OF_SCB(scb));
              }
#endif
              if (queue_flag)
              {
		int diff;
		result = scsi_track_queue_full(cmd->device,
			       	aic_dev->active_cmds);
		if ( result < 0 )
		{
                  if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
                    printk(INFO_LEAD "Tagged Command Queueing disabled.\n",
			p->host_no, CTL_OF_SCB(scb));
		  diff = aic_dev->max_q_depth - p->host->cmd_per_lun;
		  aic_dev->temp_q_depth = 1;
		  aic_dev->max_q_depth = 1;
		}
		else if ( result > 0 )
		{
                  if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
                    printk(INFO_LEAD "Queue depth reduced to %d\n", p->host_no,
                      CTL_OF_SCB(scb), result);
		  diff = aic_dev->max_q_depth - result;
		  aic_dev->max_q_depth = result;
		  if(aic_dev->temp_q_depth > result)
		    aic_dev->temp_q_depth = result;
		}
	      }
              break;
            }
            
            default:
              if (aic7xxx_verbose & VERBOSE_SEQINT)
                printk(INFO_LEAD "Unexpected target status 0x%x.\n", p->host_no,
                     CTL_OF_SCB(scb), scb->hscb->target_status);
              if (!aic7xxx_error(cmd))
              {
                aic7xxx_error(cmd) = DID_RETRY_COMMAND;
              }
              break;
          }  
        }  
      }
      break;

    case AWAITING_MSG:
      {
        unsigned char scb_index, msg_out;

        scb_index = aic_inb(p, SCB_TAG);
        msg_out = aic_inb(p, MSG_OUT);
        scb = p->scb_data->scb_array[scb_index];
	aic_dev = AIC_DEV(scb->cmd);
        p->msg_index = p->msg_len = 0;

        if ( !(scb->flags & SCB_DEVICE_RESET) &&
              (msg_out == MSG_IDENTIFYFLAG) &&
              (scb->hscb->control & TAG_ENB) )
        {
          p->msg_buf[p->msg_index++] = scb->tag_action;
          p->msg_buf[p->msg_index++] = scb->hscb->tag;
          p->msg_len += 2;
        }

        if (scb->flags & SCB_DEVICE_RESET)
        {
          p->msg_buf[p->msg_index++] = MSG_BUS_DEV_RESET;
          p->msg_len++;
          if (aic7xxx_verbose & VERBOSE_RESET_PROCESS)
            printk(INFO_LEAD "Bus device reset mailed.\n",
                 p->host_no, CTL_OF_SCB(scb));
        }
        else if (scb->flags & SCB_ABORT)
        {
          if (scb->tag_action)
          {
            p->msg_buf[p->msg_index++] = MSG_ABORT_TAG;
          }
          else
          {
            p->msg_buf[p->msg_index++] = MSG_ABORT;
          }
          p->msg_len++;
          if (aic7xxx_verbose & VERBOSE_ABORT_PROCESS)
            printk(INFO_LEAD "Abort message mailed.\n", p->host_no,
              CTL_OF_SCB(scb));
        }
        else if (scb->flags & SCB_MSGOUT_PPR)
        {
          if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Sending PPR (%d/%d/%d/%d) message.\n",
                   p->host_no, CTL_OF_SCB(scb),
                   aic_dev->goal.period,
                   aic_dev->goal.offset,
                   aic_dev->goal.width,
                   aic_dev->goal.options);
          }
          aic7xxx_construct_ppr(p, scb);
        }
        else if (scb->flags & SCB_MSGOUT_WDTR)
        {
          if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Sending WDTR message.\n", p->host_no,
                   CTL_OF_SCB(scb));
          }
          aic7xxx_construct_wdtr(p, aic_dev->goal.width);
        }
        else if (scb->flags & SCB_MSGOUT_SDTR)
        {
          unsigned int max_sync, period;
          unsigned char options = 0;
          if (p->features & AHC_ULTRA2)
          {
            if ( (aic_inb(p, SBLKCTL) & ENAB40) &&
                !(aic_inb(p, SSTAT2) & EXP_ACTIVE) )
            {
              max_sync = AHC_SYNCRATE_ULTRA2;
            }
            else
            {
              max_sync = AHC_SYNCRATE_ULTRA;
            }
          }
          else if (p->features & AHC_ULTRA)
          {
            max_sync = AHC_SYNCRATE_ULTRA;
          }
          else
          {
            max_sync = AHC_SYNCRATE_FAST;
          }
          period = aic_dev->goal.period;
          aic7xxx_find_syncrate(p, &period, max_sync, &options);
          if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Sending SDTR %d/%d message.\n", p->host_no,
                   CTL_OF_SCB(scb), period,
                   aic_dev->goal.offset);
          }
          aic7xxx_construct_sdtr(p, period, aic_dev->goal.offset);
        }
        else 
        {
          panic("aic7xxx: AWAITING_MSG for an SCB that does "
                "not have a waiting message.\n");
        }
        scb->flags |= SCB_MSGOUT_SENT;
        p->msg_index = 0;
        p->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
        p->flags |= AHC_HANDLING_REQINITS;
        aic_outb(p, aic_inb(p, SIMODE1) | ENREQINIT, SIMODE1);
        return;
      }
      break;

    case DATA_OVERRUN:
      {
        unsigned char scb_index = aic_inb(p, SCB_TAG);
        unsigned char lastphase = aic_inb(p, LASTPHASE);
        unsigned int i;

        scb = (p->scb_data->scb_array[scb_index]);
        if ( !(scb->flags & SCB_SENSE) )
        {
          printk(WARN_LEAD "Data overrun detected in %s phase, tag %d;\n",
            p->host_no, CTL_OF_SCB(scb), 
            (lastphase == P_DATAIN) ? "Data-In" : "Data-Out", scb->hscb->tag);
          printk(KERN_WARNING "  %s seen Data Phase. Length=%d, NumSGs=%d.\n",
            (aic_inb(p, SEQ_FLAGS) & DPHASE) ? "Have" : "Haven't",
            scb->sg_length, scb->sg_count);
          printk(KERN_WARNING "  Raw SCSI Command: 0x");
          for (i = 0; i < scb->hscb->SCSI_cmd_length; i++)
          {
            printk("%02x ", scb->cmd->cmnd[i]);
          }
          printk("\n");
          if(aic7xxx_verbose > 0xffff)
          {
            for (i = 0; i < scb->sg_count; i++)
            {
              printk(KERN_WARNING "     sg[%d] - Addr 0x%x : Length %d\n",
                 i, 
                 le32_to_cpu(scb->sg_list[i].address),
                 le32_to_cpu(scb->sg_list[i].length) );
            }
          }
          aic7xxx_error(scb->cmd) = DID_ERROR;
        }
        else
          printk(INFO_LEAD "Data Overrun during SEND_SENSE operation.\n",
            p->host_no, CTL_OF_SCB(scb));
      }
      break;

    case WIDE_RESIDUE:
      {
        unsigned char resid_sgcnt, index;
        unsigned char scb_index = aic_inb(p, SCB_TAG);
        unsigned int cur_addr, resid_dcnt;
        unsigned int native_addr, native_length, sg_addr;
        int i;

        if(scb_index > p->scb_data->numscbs)
        {
          printk(WARN_LEAD "invalid scb_index during WIDE_RESIDUE.\n",
            p->host_no, -1, -1, -1);
          break;
        }
        scb = p->scb_data->scb_array[scb_index];
        if(!(scb->flags & SCB_ACTIVE) || (scb->cmd == NULL))
        {
          printk(WARN_LEAD "invalid scb during WIDE_RESIDUE flags:0x%x "
                 "scb->cmd:0x%lx\n", p->host_no, CTL_OF_SCB(scb),
                 scb->flags, (unsigned long)scb->cmd);
          break;
        }
        if(aic7xxx_verbose & VERBOSE_MINOR_ERROR)
          printk(INFO_LEAD "Got WIDE_RESIDUE message, patching up data "
                 "pointer.\n", p->host_no, CTL_OF_SCB(scb));

        cur_addr = aic_inb(p, SHADDR) | (aic_inb(p, SHADDR + 1) << 8) |
          (aic_inb(p, SHADDR + 2) << 16) | (aic_inb(p, SHADDR + 3) << 24);
        sg_addr = aic_inb(p, SG_COUNT + 1) | (aic_inb(p, SG_COUNT + 2) << 8) |
          (aic_inb(p, SG_COUNT + 3) << 16) | (aic_inb(p, SG_COUNT + 4) << 24);
        resid_sgcnt = aic_inb(p, SCB_RESID_SGCNT);
        resid_dcnt = aic_inb(p, SCB_RESID_DCNT) |
          (aic_inb(p, SCB_RESID_DCNT + 1) << 8) |
          (aic_inb(p, SCB_RESID_DCNT + 2) << 16);
        index = scb->sg_count - ((resid_sgcnt) ? resid_sgcnt : 1);
        native_addr = le32_to_cpu(scb->sg_list[index].address);
        native_length = le32_to_cpu(scb->sg_list[index].length);
        if(resid_dcnt == native_length)
        {
          if(index == 0)
          {
            break;
          }
          resid_dcnt = 1;
          resid_sgcnt += 1;
          native_addr = le32_to_cpu(scb->sg_list[index - 1].address);
          native_length = le32_to_cpu(scb->sg_list[index - 1].length);
          cur_addr = native_addr + (native_length - 1);
          sg_addr -= sizeof(struct hw_scatterlist);
        }
        else
        {
          resid_dcnt += 1;
          cur_addr -= 1;
        }
        
        aic_outb(p, resid_sgcnt, SG_COUNT);
        aic_outb(p, resid_sgcnt, SCB_RESID_SGCNT);
        aic_outb(p, sg_addr & 0xff, SG_COUNT + 1);
        aic_outb(p, (sg_addr >> 8) & 0xff, SG_COUNT + 2);
        aic_outb(p, (sg_addr >> 16) & 0xff, SG_COUNT + 3);
        aic_outb(p, (sg_addr >> 24) & 0xff, SG_COUNT + 4);
        aic_outb(p, resid_dcnt & 0xff, SCB_RESID_DCNT);
        aic_outb(p, (resid_dcnt >> 8) & 0xff, SCB_RESID_DCNT + 1);
        aic_outb(p, (resid_dcnt >> 16) & 0xff, SCB_RESID_DCNT + 2);

        if(p->features & AHC_ULTRA2)
        {
          aic_outb(p, resid_dcnt & 0xff, HCNT);
          aic_outb(p, (resid_dcnt >> 8) & 0xff, HCNT + 1);
          aic_outb(p, (resid_dcnt >> 16) & 0xff, HCNT + 2);
          aic_outb(p, cur_addr & 0xff, HADDR);
          aic_outb(p, (cur_addr >> 8) & 0xff, HADDR + 1);
          aic_outb(p, (cur_addr >> 16) & 0xff, HADDR + 2);
          aic_outb(p, (cur_addr >> 24) & 0xff, HADDR + 3);
          aic_outb(p, aic_inb(p, DMAPARAMS) | PRELOADEN, DFCNTRL);
          udelay(1);
          aic_outb(p, aic_inb(p, DMAPARAMS) & ~(SCSIEN|HDMAEN), DFCNTRL);
          i=0;
          while(((aic_inb(p, DFCNTRL) & (SCSIEN|HDMAEN)) != 0) && (i++ < 1000))
          {
            udelay(1);
          }
        }
        else
        {
          aic_outb(p, cur_addr & 0xff, SHADDR);
          aic_outb(p, (cur_addr >> 8) & 0xff, SHADDR + 1);
          aic_outb(p, (cur_addr >> 16) & 0xff, SHADDR + 2);
          aic_outb(p, (cur_addr >> 24) & 0xff, SHADDR + 3);
        }
      }
      break;

    case SEQ_SG_FIXUP:
    {
      unsigned char scb_index, tmp;
      int sg_addr, sg_length;

      scb_index = aic_inb(p, SCB_TAG);

      if(scb_index > p->scb_data->numscbs)
      {
        printk(WARN_LEAD "invalid scb_index during SEQ_SG_FIXUP.\n",
          p->host_no, -1, -1, -1);
        printk(INFO_LEAD "SCSISIGI 0x%x, SEQADDR 0x%x, SSTAT0 0x%x, SSTAT1 "
           "0x%x\n", p->host_no, -1, -1, -1,
           aic_inb(p, SCSISIGI),
           aic_inb(p, SEQADDR0) | (aic_inb(p, SEQADDR1) << 8),
           aic_inb(p, SSTAT0), aic_inb(p, SSTAT1));
        printk(INFO_LEAD "SG_CACHEPTR 0x%x, SSTAT2 0x%x, STCNT 0x%x\n",
           p->host_no, -1, -1, -1, aic_inb(p, SG_CACHEPTR),
           aic_inb(p, SSTAT2), aic_inb(p, STCNT + 2) << 16 |
           aic_inb(p, STCNT + 1) << 8 | aic_inb(p, STCNT));
        break;
      }
      scb = p->scb_data->scb_array[scb_index];
      if(!(scb->flags & SCB_ACTIVE) || (scb->cmd == NULL))
      {
        printk(WARN_LEAD "invalid scb during SEQ_SG_FIXUP flags:0x%x "
               "scb->cmd:0x%p\n", p->host_no, CTL_OF_SCB(scb),
               scb->flags, scb->cmd);
        printk(INFO_LEAD "SCSISIGI 0x%x, SEQADDR 0x%x, SSTAT0 0x%x, SSTAT1 "
           "0x%x\n", p->host_no, CTL_OF_SCB(scb),
           aic_inb(p, SCSISIGI),
           aic_inb(p, SEQADDR0) | (aic_inb(p, SEQADDR1) << 8),
           aic_inb(p, SSTAT0), aic_inb(p, SSTAT1));
        printk(INFO_LEAD "SG_CACHEPTR 0x%x, SSTAT2 0x%x, STCNT 0x%x\n",
           p->host_no, CTL_OF_SCB(scb), aic_inb(p, SG_CACHEPTR),
           aic_inb(p, SSTAT2), aic_inb(p, STCNT + 2) << 16 |
           aic_inb(p, STCNT + 1) << 8 | aic_inb(p, STCNT));
        break;
      }
      if(aic7xxx_verbose & VERBOSE_MINOR_ERROR)
        printk(INFO_LEAD "Fixing up SG address for sequencer.\n", p->host_no,
               CTL_OF_SCB(scb));
      tmp = aic_inb(p, SG_NEXT);
      tmp += SG_SIZEOF;
      aic_outb(p, tmp, SG_NEXT);
      if( tmp < SG_SIZEOF )
        aic_outb(p, aic_inb(p, SG_NEXT + 1) + 1, SG_NEXT + 1);
      tmp = aic_inb(p, SG_COUNT) - 1;
      aic_outb(p, tmp, SG_COUNT);
      sg_addr = le32_to_cpu(scb->sg_list[scb->sg_count - tmp].address);
      sg_length = le32_to_cpu(scb->sg_list[scb->sg_count - tmp].length);
      aic_outb(p, sg_addr & 0xff, HADDR);
      aic_outb(p, (sg_addr >> 8) & 0xff, HADDR + 1);
      aic_outb(p, (sg_addr >> 16) & 0xff, HADDR + 2);
      aic_outb(p, (sg_addr >> 24) & 0xff, HADDR + 3);
      aic_outb(p, sg_length & 0xff, HCNT);
      aic_outb(p, (sg_length >> 8) & 0xff, HCNT + 1);
      aic_outb(p, (sg_length >> 16) & 0xff, HCNT + 2);
      aic_outb(p, (tmp << 2) | ((tmp == 1) ? LAST_SEG : 0), SG_CACHEPTR);
      aic_outb(p, aic_inb(p, DMAPARAMS), DFCNTRL);
      while(aic_inb(p, SSTAT0) & SDONE) udelay(1);
      while(aic_inb(p, DFCNTRL) & (HDMAEN|SCSIEN)) aic_outb(p, 0, DFCNTRL);
    }
    break;

#ifdef AIC7XXX_NOT_YET 
    case TRACEPOINT2:
      {
        printk(INFO_LEAD "Tracepoint #2 reached.\n", p->host_no,
               channel, target, lun);
      }
      break;

    
    case MSG_BUFFER_BUSY:
      printk("aic7xxx: Message buffer busy.\n");
      break;
    case MSGIN_PHASEMIS:
      printk("aic7xxx: Message-in phasemis.\n");
      break;
#endif

    default:                   
      printk(WARN_LEAD "Unknown SEQINT, INTSTAT 0x%x, SCSISIGI 0x%x.\n",
             p->host_no, channel, target, lun, intstat,
             aic_inb(p, SCSISIGI));
      break;
  }

  unpause_sequencer(p,  TRUE);
}

static int
aic7xxx_parse_msg(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
  int reject, reply, done;
  unsigned char target_scsirate, tindex;
  unsigned short target_mask;
  unsigned char target, channel, lun;
  unsigned char bus_width, new_bus_width;
  unsigned char trans_options, new_trans_options;
  unsigned int period, new_period, offset, new_offset, maxsync;
  struct aic7xxx_syncrate *syncrate;
  struct aic_dev_data *aic_dev;

  target = scb->cmd->device->id;
  channel = scb->cmd->device->channel;
  lun = scb->cmd->device->lun;
  reply = reject = done = FALSE;
  tindex = TARGET_INDEX(scb->cmd);
  aic_dev = AIC_DEV(scb->cmd);
  target_scsirate = aic_inb(p, TARG_SCSIRATE + tindex);
  target_mask = (0x01 << tindex);


  if (p->msg_buf[0] != MSG_EXTENDED)
  {
    reject = TRUE;
  }

  if (p->features & AHC_ULTRA2)
  {
    if ( (aic_inb(p, SBLKCTL) & ENAB40) &&
         !(aic_inb(p, SSTAT2) & EXP_ACTIVE) )
    {
      if (p->features & AHC_ULTRA3)
        maxsync = AHC_SYNCRATE_ULTRA3;
      else
        maxsync = AHC_SYNCRATE_ULTRA2;
    }
    else
    {
      maxsync = AHC_SYNCRATE_ULTRA;
    }
  }
  else if (p->features & AHC_ULTRA)
  {
    maxsync = AHC_SYNCRATE_ULTRA;
  }
  else
  {
    maxsync = AHC_SYNCRATE_FAST;
  }


  if ( !reject && (p->msg_len > 2) )
  {
    switch(p->msg_buf[2])
    {
      case MSG_EXT_SDTR:
      {
        
        if (p->msg_buf[1] != MSG_EXT_SDTR_LEN)
        {
          reject = TRUE;
          break;
        }

        if (p->msg_len < (MSG_EXT_SDTR_LEN + 2))
        {
          break;
        }

        period = new_period = p->msg_buf[3];
        offset = new_offset = p->msg_buf[4];
        trans_options = new_trans_options = 0;
        bus_width = new_bus_width = target_scsirate & WIDEXFER;

        if(maxsync == AHC_SYNCRATE_ULTRA3)
          maxsync = AHC_SYNCRATE_ULTRA2;

        if ( (scb->flags & (SCB_MSGOUT_SENT|SCB_MSGOUT_SDTR)) !=
             (SCB_MSGOUT_SENT|SCB_MSGOUT_SDTR) )
        {
          if (!(aic_dev->flags & DEVICE_DTR_SCANNED))
          {
            aic_dev->goal.width = MSG_EXT_WDTR_BUS_8_BIT;
            aic_dev->goal.options = 0;
            if(p->user[tindex].offset)
            {
              aic_dev->needsdtr_copy = 1;
              aic_dev->goal.period = max_t(unsigned char, 10,p->user[tindex].period);
              if(p->features & AHC_ULTRA2)
              {
                aic_dev->goal.offset = MAX_OFFSET_ULTRA2;
              }
              else
              {
                aic_dev->goal.offset = MAX_OFFSET_8BIT;
              }
            }
            else
            {
              aic_dev->needsdtr_copy = 0;
              aic_dev->goal.period = 255;
              aic_dev->goal.offset = 0;
            }
            aic_dev->flags |= DEVICE_DTR_SCANNED | DEVICE_PRINT_DTR;
          }
          else if (aic_dev->needsdtr_copy == 0)
          {
            reject = TRUE;
            break;
          }

          
          reply = TRUE;
          
          if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Received pre-emptive SDTR message from "
                   "target.\n", p->host_no, CTL_OF_SCB(scb));
          }
          new_period = max_t(unsigned int, period, aic_dev->goal.period);
          new_offset = min_t(unsigned int, offset, aic_dev->goal.offset);
        }
 
        syncrate = aic7xxx_find_syncrate(p, &new_period, maxsync,
                                         &trans_options);
        aic7xxx_validate_offset(p, syncrate, &new_offset, bus_width);

        if ((new_offset == 0) && (new_offset != offset))
        {
          aic_dev->needsdtr_copy = 0;
          reply = TRUE;
        }
        
        if(reply)
        {
          aic7xxx_set_syncrate(p, syncrate, target, channel, new_period,
                               new_offset, trans_options,
                               AHC_TRANS_GOAL|AHC_TRANS_ACTIVE|AHC_TRANS_CUR,
			       aic_dev);
          scb->flags &= ~SCB_MSGOUT_BITS;
          scb->flags |= SCB_MSGOUT_SDTR;
          aic_outb(p, HOST_MSG, MSG_OUT);
          aic_outb(p, aic_inb(p, SCSISIGO) | ATNO, SCSISIGO);
        }
        else
        {
          aic7xxx_set_syncrate(p, syncrate, target, channel, new_period,
                               new_offset, trans_options,
                               AHC_TRANS_ACTIVE|AHC_TRANS_CUR, aic_dev);
          aic_dev->needsdtr = 0;
        }
        done = TRUE;
        break;
      }
      case MSG_EXT_WDTR:
      {
          
        if (p->msg_buf[1] != MSG_EXT_WDTR_LEN)
        {
          reject = TRUE;
          break;
        }

        if (p->msg_len < (MSG_EXT_WDTR_LEN + 2))
        {
          break;
        }

        bus_width = new_bus_width = p->msg_buf[3];

        if ( (scb->flags & (SCB_MSGOUT_SENT|SCB_MSGOUT_WDTR)) ==
             (SCB_MSGOUT_SENT|SCB_MSGOUT_WDTR) )
        {
          switch(bus_width)
          {
            default:
            {
              reject = TRUE;
              if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
                   ((aic_dev->flags & DEVICE_PRINT_DTR) ||
                    (aic7xxx_verbose > 0xffff)) )
              {
                printk(INFO_LEAD "Requesting %d bit transfers, rejecting.\n",
                  p->host_no, CTL_OF_SCB(scb), 8 * (0x01 << bus_width));
              }
            } 
            case MSG_EXT_WDTR_BUS_8_BIT:
            {
              aic_dev->goal.width = MSG_EXT_WDTR_BUS_8_BIT;
              aic_dev->needwdtr_copy &= ~target_mask;
              break;
            }
            case MSG_EXT_WDTR_BUS_16_BIT:
            {
              break;
            }
          }
          aic_dev->needwdtr = 0;
          aic7xxx_set_width(p, target, channel, lun, new_bus_width,
                            AHC_TRANS_ACTIVE|AHC_TRANS_CUR, aic_dev);
        }
        else
        {
          if ( !(aic_dev->flags & DEVICE_DTR_SCANNED) )
          {
            if( (p->features & AHC_WIDE) && p->user[tindex].width )
            {
              aic_dev->goal.width = MSG_EXT_WDTR_BUS_16_BIT;
              aic_dev->needwdtr_copy = 1;
            }
            
            aic_dev->goal.options = 0;

            if(p->user[tindex].offset)
            {
              aic_dev->needsdtr_copy = 1;
              aic_dev->goal.period = max_t(unsigned char, 10, p->user[tindex].period);
              if(p->features & AHC_ULTRA2)
              {
                aic_dev->goal.offset = MAX_OFFSET_ULTRA2;
              }
              else if( aic_dev->goal.width )
              {
                aic_dev->goal.offset = MAX_OFFSET_16BIT;
              }
              else
              {
                aic_dev->goal.offset = MAX_OFFSET_8BIT;
              }
            } else {
              aic_dev->needsdtr_copy = 0;
              aic_dev->goal.period = 255;
              aic_dev->goal.offset = 0;
            }
            
            aic_dev->flags |= DEVICE_DTR_SCANNED | DEVICE_PRINT_DTR;
          }
          else if (aic_dev->needwdtr_copy == 0)
          {
            reject = TRUE;
            break;
          }

          
          reply = TRUE;

          if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Received pre-emptive WDTR message from "
                   "target.\n", p->host_no, CTL_OF_SCB(scb));
          }
          switch(bus_width)
          {
            case MSG_EXT_WDTR_BUS_16_BIT:
            {
              if ( (p->features & AHC_WIDE) &&
                   (aic_dev->goal.width == MSG_EXT_WDTR_BUS_16_BIT) )
              {
                new_bus_width = MSG_EXT_WDTR_BUS_16_BIT;
                break;
              }
            } 
            default:
            case MSG_EXT_WDTR_BUS_8_BIT:
            {
              aic_dev->needwdtr_copy = 0;
              new_bus_width = MSG_EXT_WDTR_BUS_8_BIT;
              break;
            }
          }
          scb->flags &= ~SCB_MSGOUT_BITS;
          scb->flags |= SCB_MSGOUT_WDTR;
          aic_dev->needwdtr = 0;
          if(aic_dev->dtr_pending == 0)
          {
            aic_dev->dtr_pending = 1;
            scb->flags |= SCB_DTR_SCB;
          }
          aic_outb(p, HOST_MSG, MSG_OUT);
          aic_outb(p, aic_inb(p, SCSISIGO) | ATNO, SCSISIGO);
          aic7xxx_set_width(p, target, channel, lun, new_bus_width,
                          AHC_TRANS_GOAL|AHC_TRANS_ACTIVE|AHC_TRANS_CUR,
			  aic_dev);
        }
        
        aic7xxx_set_syncrate(p, NULL, target, channel, 0, 0, 0,
                             AHC_TRANS_ACTIVE|AHC_TRANS_CUR|AHC_TRANS_QUITE,
			     aic_dev);
        aic_dev->needsdtr = aic_dev->needsdtr_copy;
        done = TRUE;
        break;
      }
      case MSG_EXT_PPR:
      {
        
        if (p->msg_buf[1] != MSG_EXT_PPR_LEN)
        {
          reject = TRUE;
          break;
        }

        if (p->msg_len < (MSG_EXT_PPR_LEN + 2))
        {
          break;
        }

        period = new_period = p->msg_buf[3];
        offset = new_offset = p->msg_buf[5];
        bus_width = new_bus_width = p->msg_buf[6];
        trans_options = new_trans_options = p->msg_buf[7] & 0xf;

        if(aic7xxx_verbose & VERBOSE_NEGOTIATION2)
        {
          printk(INFO_LEAD "Parsing PPR message (%d/%d/%d/%d)\n",
                 p->host_no, CTL_OF_SCB(scb), period, offset, bus_width,
                 trans_options);
        }

        if ( (scb->flags & (SCB_MSGOUT_SENT|SCB_MSGOUT_PPR)) !=
             (SCB_MSGOUT_SENT|SCB_MSGOUT_PPR) )
        { 
          
          if (!(aic_dev->flags & DEVICE_DTR_SCANNED))
          {
            aic_dev->needppr = aic_dev->needppr_copy = 1;
            aic_dev->needsdtr = aic_dev->needsdtr_copy = 0;
            aic_dev->needwdtr = aic_dev->needwdtr_copy = 0;
          
            
            aic_dev->flags |= DEVICE_SCSI_3;
          
            aic_dev->goal.width = p->user[tindex].width;
            if(p->user[tindex].offset)
            {
              aic_dev->goal.period = p->user[tindex].period;
              aic_dev->goal.options = p->user[tindex].options;
              if(p->features & AHC_ULTRA2)
              {
                aic_dev->goal.offset = MAX_OFFSET_ULTRA2;
              }
              else if( aic_dev->goal.width &&
                       (bus_width == MSG_EXT_WDTR_BUS_16_BIT) &&
                       p->features & AHC_WIDE )
              {
                aic_dev->goal.offset = MAX_OFFSET_16BIT;
              }
              else
              {
                aic_dev->goal.offset = MAX_OFFSET_8BIT;
              }
            }
            else
            {
              aic_dev->goal.period = 255;
              aic_dev->goal.offset = 0;
              aic_dev->goal.options = 0;
            }
            aic_dev->flags |= DEVICE_DTR_SCANNED | DEVICE_PRINT_DTR;
          }
          else if (aic_dev->needppr_copy == 0)
          {
            reject = TRUE;
            break;
          }

          
          reply = TRUE;
          
          if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
          {
            printk(INFO_LEAD "Received pre-emptive PPR message from "
                   "target.\n", p->host_no, CTL_OF_SCB(scb));
          }

        }

        switch(bus_width)
        {
          case MSG_EXT_WDTR_BUS_16_BIT:
          {
            if ( (aic_dev->goal.width == MSG_EXT_WDTR_BUS_16_BIT) &&
			    p->features & AHC_WIDE)
            {
              break;
            }
          }
          default:
          {
            if ( (aic7xxx_verbose & VERBOSE_NEGOTIATION2) &&
                 ((aic_dev->flags & DEVICE_PRINT_DTR) ||
                  (aic7xxx_verbose > 0xffff)) )
            {
              reply = TRUE;
              printk(INFO_LEAD "Requesting %d bit transfers, rejecting.\n",
                p->host_no, CTL_OF_SCB(scb), 8 * (0x01 << bus_width));
            }
          } 
          case MSG_EXT_WDTR_BUS_8_BIT:
          {
            new_trans_options = 0;
            new_bus_width = MSG_EXT_WDTR_BUS_8_BIT;
            break;
          }
        }

        if(reply)
        {
          aic7xxx_set_width(p, target, channel, lun, new_bus_width,
                            AHC_TRANS_GOAL|AHC_TRANS_ACTIVE|AHC_TRANS_CUR,
			    aic_dev);
          syncrate = aic7xxx_find_syncrate(p, &new_period, maxsync,
                                           &new_trans_options);
          aic7xxx_validate_offset(p, syncrate, &new_offset, new_bus_width);
          aic7xxx_set_syncrate(p, syncrate, target, channel, new_period,
                               new_offset, new_trans_options,
                               AHC_TRANS_GOAL|AHC_TRANS_ACTIVE|AHC_TRANS_CUR,
			       aic_dev);
        }
        else
        {
          aic7xxx_set_width(p, target, channel, lun, new_bus_width,
                            AHC_TRANS_ACTIVE|AHC_TRANS_CUR, aic_dev);
          syncrate = aic7xxx_find_syncrate(p, &new_period, maxsync,
                                           &new_trans_options);
          aic7xxx_validate_offset(p, syncrate, &new_offset, new_bus_width);
          aic7xxx_set_syncrate(p, syncrate, target, channel, new_period,
                               new_offset, new_trans_options,
                               AHC_TRANS_ACTIVE|AHC_TRANS_CUR, aic_dev);
        }

        if(new_trans_options == 0)
        {
          aic_dev->needppr = aic_dev->needppr_copy = 0;
          if(new_offset)
          {
            aic_dev->needsdtr = aic_dev->needsdtr_copy = 1;
          }
          if (new_bus_width)
          {
            aic_dev->needwdtr = aic_dev->needwdtr_copy = 1;
          }
        }

        if((new_offset == 0) && (offset != 0))
        {
          reply = TRUE;
        }

        if(reply)
        {
          scb->flags &= ~SCB_MSGOUT_BITS;
          scb->flags |= SCB_MSGOUT_PPR;
          aic_outb(p, HOST_MSG, MSG_OUT);
          aic_outb(p, aic_inb(p, SCSISIGO) | ATNO, SCSISIGO);
        }
        else
        {
          aic_dev->needppr = 0;
        }
        done = TRUE;
        break;
      }
      default:
      {
        reject = TRUE;
        break;
      }
    } 
  } 

  if (!reply && reject)
  {
    aic_outb(p, MSG_MESSAGE_REJECT, MSG_OUT);
    aic_outb(p, aic_inb(p, SCSISIGO) | ATNO, SCSISIGO);
    done = TRUE;
  }
  return(done);
}


static void
aic7xxx_handle_reqinit(struct aic7xxx_host *p, struct aic7xxx_scb *scb)
{
  unsigned char lastbyte;
  unsigned char phasemis;
  int done = FALSE;

  switch(p->msg_type)
  {
    case MSG_TYPE_INITIATOR_MSGOUT:
      {
        if (p->msg_len == 0)
          panic("aic7xxx: REQINIT with no active message!\n");

        lastbyte = (p->msg_index == (p->msg_len - 1));
        phasemis = ( aic_inb(p, SCSISIGI) & PHASE_MASK) != P_MESGOUT;

        if (lastbyte || phasemis)
        {
          
          p->msg_len = 0;
          p->msg_type = MSG_TYPE_NONE;
          aic_outb(p, aic_inb(p, SIMODE1) & ~ENREQINIT, SIMODE1);
          aic_outb(p, CLRSCSIINT, CLRINT);
          p->flags &= ~AHC_HANDLING_REQINITS;

          if (phasemis == 0)
          {
            aic_outb(p, p->msg_buf[p->msg_index], SINDEX);
            aic_outb(p, 0, RETURN_1);
#ifdef AIC7XXX_VERBOSE_DEBUGGING
            if (aic7xxx_verbose > 0xffff)
              printk(INFO_LEAD "Completed sending of REQINIT message.\n",
                     p->host_no, CTL_OF_SCB(scb));
#endif
          }
          else
          {
            aic_outb(p, MSGOUT_PHASEMIS, RETURN_1);
#ifdef AIC7XXX_VERBOSE_DEBUGGING
            if (aic7xxx_verbose > 0xffff)
              printk(INFO_LEAD "PHASEMIS while sending REQINIT message.\n",
                     p->host_no, CTL_OF_SCB(scb));
#endif
          }
          unpause_sequencer(p, TRUE);
        }
        else
        {
          aic_outb(p, CLRREQINIT, CLRSINT1);
          aic_outb(p, CLRSCSIINT, CLRINT);
          aic_outb(p,  p->msg_buf[p->msg_index++], SCSIDATL);
        }
        break;
      }
    case MSG_TYPE_INITIATOR_MSGIN:
      {
        phasemis = ( aic_inb(p, SCSISIGI) & PHASE_MASK ) != P_MESGIN;

        if (phasemis == 0)
        {
          p->msg_len++;
          
          p->msg_buf[p->msg_index] = aic_inb(p, SCSIBUSL);
          done = aic7xxx_parse_msg(p, scb);
          
          aic_outb(p, CLRREQINIT, CLRSINT1);
          aic_outb(p, CLRSCSIINT, CLRINT);
          aic_inb(p, SCSIDATL);
          p->msg_index++;
        }
        if (phasemis || done)
        {
#ifdef AIC7XXX_VERBOSE_DEBUGGING
          if (aic7xxx_verbose > 0xffff)
          {
            if (phasemis)
              printk(INFO_LEAD "PHASEMIS while receiving REQINIT message.\n",
                     p->host_no, CTL_OF_SCB(scb));
            else
              printk(INFO_LEAD "Completed receipt of REQINIT message.\n",
                     p->host_no, CTL_OF_SCB(scb));
          }
#endif
          
          p->msg_len = 0;
          p->msg_type = MSG_TYPE_NONE;
          aic_outb(p, aic_inb(p, SIMODE1) & ~ENREQINIT, SIMODE1);
          aic_outb(p, CLRSCSIINT, CLRINT);
          p->flags &= ~AHC_HANDLING_REQINITS;
          unpause_sequencer(p, TRUE);
        }
        break;
      }
    default:
      {
        panic("aic7xxx: Unknown REQINIT message type.\n");
        break;
      }
  } 
}

static void
aic7xxx_handle_scsiint(struct aic7xxx_host *p, unsigned char intstat)
{
  unsigned char scb_index;
  unsigned char status;
  struct aic7xxx_scb *scb;
  struct aic_dev_data *aic_dev;

  scb_index = aic_inb(p, SCB_TAG);
  status = aic_inb(p, SSTAT1);

  if (scb_index < p->scb_data->numscbs)
  {
    scb = p->scb_data->scb_array[scb_index];
    if ((scb->flags & SCB_ACTIVE) == 0)
    {
      scb = NULL;
    }
  }
  else
  {
    scb = NULL;
  }


  if ((status & SCSIRSTI) != 0)
  {
    int channel;

    if ( (p->chip & AHC_CHIPID_MASK) == AHC_AIC7770 )
      channel = (aic_inb(p, SBLKCTL) & SELBUSB) >> 3;
    else
      channel = 0;

    if (aic7xxx_verbose & VERBOSE_RESET)
      printk(WARN_LEAD "Someone else reset the channel!!\n",
           p->host_no, channel, -1, -1);
    if (aic7xxx_panic_on_abort)
      aic7xxx_panic_abort(p, NULL);
    aic7xxx_reset_channel(p, channel,  FALSE);
    aic7xxx_run_done_queue(p, TRUE);
    scb = NULL;
  }
  else if ( ((status & BUSFREE) != 0) && ((status & SELTO) == 0) )
  {
    unsigned char lastphase = aic_inb(p, LASTPHASE);
    unsigned char saved_tcl = aic_inb(p, SAVED_TCL);
    unsigned char target = (saved_tcl >> 4) & 0x0F;
    int channel;
    int printerror = TRUE;

    if ( (p->chip & AHC_CHIPID_MASK) == AHC_AIC7770 )
      channel = (aic_inb(p, SBLKCTL) & SELBUSB) >> 3;
    else
      channel = 0;

    aic_outb(p, aic_inb(p, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP),
             SCSISEQ);
    if (lastphase == P_MESGOUT)
    {
      unsigned char message;

      message = aic_inb(p, SINDEX);

      if ((message == MSG_ABORT) || (message == MSG_ABORT_TAG))
      {
        if (aic7xxx_verbose & VERBOSE_ABORT_PROCESS)
          printk(INFO_LEAD "SCB %d abort delivered.\n", p->host_no,
            CTL_OF_SCB(scb), scb->hscb->tag);
        aic7xxx_reset_device(p, target, channel, ALL_LUNS,
                (message == MSG_ABORT) ? SCB_LIST_NULL : scb->hscb->tag );
        aic7xxx_run_done_queue(p, TRUE);
        scb = NULL;
        printerror = 0;
      }
      else if (message == MSG_BUS_DEV_RESET)
      {
        aic7xxx_handle_device_reset(p, target, channel);
        scb = NULL;
        printerror = 0;
      }
    }
    if ( (scb != NULL) && (scb->flags & SCB_DTR_SCB) ) 
    {
      printerror = 0;
      aic7xxx_reset_device(p, target, channel, ALL_LUNS, scb->hscb->tag);
      aic7xxx_run_done_queue(p, TRUE);
      scb = NULL;
    }
    if (printerror != 0)
    {
      if (scb != NULL)
      {
        unsigned char tag;

        if ((scb->hscb->control & TAG_ENB) != 0)
        {
          tag = scb->hscb->tag;
        }
        else
        {
          tag = SCB_LIST_NULL;
        }
        aic7xxx_reset_device(p, target, channel, ALL_LUNS, tag);
        aic7xxx_run_done_queue(p, TRUE);
      }
      else
      {
        aic7xxx_reset_device(p, target, channel, ALL_LUNS, SCB_LIST_NULL);
        aic7xxx_run_done_queue(p, TRUE);
      }
      printk(INFO_LEAD "Unexpected busfree, LASTPHASE = 0x%x, "
             "SEQADDR = 0x%x\n", p->host_no, channel, target, -1, lastphase,
             (aic_inb(p, SEQADDR1) << 8) | aic_inb(p, SEQADDR0));
      scb = NULL;
    }
    aic_outb(p, MSG_NOOP, MSG_OUT);
    aic_outb(p, aic_inb(p, SIMODE1) & ~(ENBUSFREE|ENREQINIT),
      SIMODE1);
    p->flags &= ~AHC_HANDLING_REQINITS;
    aic_outb(p, CLRBUSFREE, CLRSINT1);
    aic_outb(p, CLRSCSIINT, CLRINT);
    restart_sequencer(p);
    unpause_sequencer(p, TRUE);
  }
  else if ((status & SELTO) != 0)
  {
	unsigned char scbptr;
	unsigned char nextscb;
	struct scsi_cmnd *cmd;

    scbptr = aic_inb(p, WAITING_SCBH);
    if (scbptr > p->scb_data->maxhscbs)
    {
      printk(INFO_LEAD "Invalid WAITING_SCBH value %d, improvising.\n",
             p->host_no, -1, -1, -1, scbptr);
      if (p->scb_data->maxhscbs > 4)
        scbptr &= (p->scb_data->maxhscbs - 1);
      else
        scbptr &= 0x03;
    }
    aic_outb(p, scbptr, SCBPTR);
    scb_index = aic_inb(p, SCB_TAG);

    scb = NULL;
    if (scb_index < p->scb_data->numscbs)
    {
      scb = p->scb_data->scb_array[scb_index];
      if ((scb->flags & SCB_ACTIVE) == 0)
      {
        scb = NULL;
      }
    }
    if (scb == NULL)
    {
      printk(WARN_LEAD "Referenced SCB %d not valid during SELTO.\n",
             p->host_no, -1, -1, -1, scb_index);
      printk(KERN_WARNING "        SCSISEQ = 0x%x SEQADDR = 0x%x SSTAT0 = 0x%x "
             "SSTAT1 = 0x%x\n", aic_inb(p, SCSISEQ),
             aic_inb(p, SEQADDR0) | (aic_inb(p, SEQADDR1) << 8),
             aic_inb(p, SSTAT0), aic_inb(p, SSTAT1));
      if (aic7xxx_panic_on_abort)
        aic7xxx_panic_abort(p, NULL);
    }
    else
    {
      cmd = scb->cmd;
      cmd->result = (DID_TIME_OUT << 16);

      aic_outb(p, 0, SCB_CONTROL);

      aic_outb(p, MSG_NOOP, MSG_OUT);

      nextscb = aic_inb(p, SCB_NEXT);
      aic_outb(p, nextscb, WAITING_SCBH);

      aic7xxx_add_curscb_to_free_list(p);
#ifdef AIC7XXX_VERBOSE_DEBUGGING
      if (aic7xxx_verbose > 0xffff)
        printk(INFO_LEAD "Selection Timeout.\n", p->host_no, CTL_OF_SCB(scb));
#endif
      if (scb->flags & SCB_QUEUED_ABORT)
      {
        cmd->result = 0;
        scb = NULL;
      }
    }
    aic_outb(p, aic_inb(p, SCSISEQ) & ~ENSELO, SCSISEQ);
    if( (p->chip & ~AHC_CHIPID_MASK) == AHC_PCI )
      aic_outb(p, 0, SCSIBUSL);

    udelay(301);
    aic_outb(p, CLRSELINGO, CLRSINT0);
    aic_outb(p, aic_inb(p, SIMODE1) & ~(ENREQINIT|ENBUSFREE), SIMODE1);
    p->flags &= ~AHC_HANDLING_REQINITS;
    aic_outb(p, CLRSELTIMEO | CLRBUSFREE, CLRSINT1);
    aic_outb(p, CLRSCSIINT, CLRINT);
    restart_sequencer(p);
    unpause_sequencer(p, TRUE);
  }
  else if (scb == NULL)
  {
    printk(WARN_LEAD "aic7xxx_isr - referenced scb not valid "
           "during scsiint 0x%x scb(%d)\n"
           "      SIMODE0 0x%x, SIMODE1 0x%x, SSTAT0 0x%x, SEQADDR 0x%x\n",
           p->host_no, -1, -1, -1, status, scb_index, aic_inb(p, SIMODE0),
           aic_inb(p, SIMODE1), aic_inb(p, SSTAT0),
           (aic_inb(p, SEQADDR1) << 8) | aic_inb(p, SEQADDR0));
    aic_outb(p, status, CLRSINT1);
    aic_outb(p, CLRSCSIINT, CLRINT);
    unpause_sequencer(p,  TRUE);
    scb = NULL;
  }
  else if (status & SCSIPERR)
  {
	char  *phase;
	struct scsi_cmnd *cmd;
	unsigned char mesg_out = MSG_NOOP;
	unsigned char lastphase = aic_inb(p, LASTPHASE);
	unsigned char sstat2 = aic_inb(p, SSTAT2);

    cmd = scb->cmd;
    switch (lastphase)
    {
      case P_DATAOUT:
        phase = "Data-Out";
        break;
      case P_DATAIN:
        phase = "Data-In";
        mesg_out = MSG_INITIATOR_DET_ERR;
        break;
      case P_COMMAND:
        phase = "Command";
        break;
      case P_MESGOUT:
        phase = "Message-Out";
        break;
      case P_STATUS:
        phase = "Status";
        mesg_out = MSG_INITIATOR_DET_ERR;
        break;
      case P_MESGIN:
        phase = "Message-In";
        mesg_out = MSG_PARITY_ERROR;
        break;
      default:
        phase = "unknown";
        break;
    }

    if( (p->features & AHC_ULTRA3) && 
        (aic_inb(p, SCSIRATE) & AHC_SYNCRATE_CRC) &&
        (lastphase == P_DATAIN) )
    {
      printk(WARN_LEAD "CRC error during %s phase.\n",
             p->host_no, CTL_OF_SCB(scb), phase);
      if(sstat2 & CRCVALERR)
      {
        printk(WARN_LEAD "  CRC error in intermediate CRC packet.\n",
               p->host_no, CTL_OF_SCB(scb));
      }
      if(sstat2 & CRCENDERR)
      {
        printk(WARN_LEAD "  CRC error in ending CRC packet.\n",
               p->host_no, CTL_OF_SCB(scb));
      }
      if(sstat2 & CRCREQERR)
      {
        printk(WARN_LEAD "  Target incorrectly requested a CRC packet.\n",
               p->host_no, CTL_OF_SCB(scb));
      }
      if(sstat2 & DUAL_EDGE_ERROR)
      {
        printk(WARN_LEAD "  Dual Edge transmission error.\n",
               p->host_no, CTL_OF_SCB(scb));
      }
    }
    else if( (lastphase == P_MESGOUT) &&
             (scb->flags & SCB_MSGOUT_PPR) )
    {
      aic_dev = AIC_DEV(scb->cmd);
      aic_dev->needppr = aic_dev->needppr_copy = 0;
      aic7xxx_set_width(p, scb->cmd->device->id, scb->cmd->device->channel, scb->cmd->device->lun,
                        MSG_EXT_WDTR_BUS_8_BIT,
                        (AHC_TRANS_ACTIVE|AHC_TRANS_CUR|AHC_TRANS_QUITE),
			aic_dev);
      aic7xxx_set_syncrate(p, NULL, scb->cmd->device->id, scb->cmd->device->channel, 0, 0,
                           0, AHC_TRANS_ACTIVE|AHC_TRANS_CUR|AHC_TRANS_QUITE,
			   aic_dev);
      aic_dev->goal.options = 0;
      scb->flags &= ~SCB_MSGOUT_BITS;
      if(aic7xxx_verbose & VERBOSE_NEGOTIATION2)
      {
        printk(INFO_LEAD "parity error during PPR message, reverting "
               "to WDTR/SDTR\n", p->host_no, CTL_OF_SCB(scb));
      }
      if ( aic_dev->goal.width )
      {
        aic_dev->needwdtr = aic_dev->needwdtr_copy = 1;
      }
      if ( aic_dev->goal.offset )
      {
        if( aic_dev->goal.period <= 9 )
        {
          aic_dev->goal.period = 10;
        }
        aic_dev->needsdtr = aic_dev->needsdtr_copy = 1;
      }
      scb = NULL;
    }

    if (mesg_out != MSG_NOOP)
    {
      aic_outb(p, mesg_out, MSG_OUT);
      aic_outb(p, aic_inb(p, SCSISIGI) | ATNO, SCSISIGO);
      scb = NULL;
    }
    aic_outb(p, CLRSCSIPERR, CLRSINT1);
    aic_outb(p, CLRSCSIINT, CLRINT);
    unpause_sequencer(p,  TRUE);
  }
  else if ( (status & REQINIT) &&
            (p->flags & AHC_HANDLING_REQINITS) )
  {
#ifdef AIC7XXX_VERBOSE_DEBUGGING
    if (aic7xxx_verbose > 0xffff)
      printk(INFO_LEAD "Handling REQINIT, SSTAT1=0x%x.\n", p->host_no,
             CTL_OF_SCB(scb), aic_inb(p, SSTAT1));
#endif
    aic7xxx_handle_reqinit(p, scb);
    return;
  }
  else
  {
    if (aic7xxx_verbose & VERBOSE_SCSIINT)
      printk(INFO_LEAD "Unknown SCSIINT status, SSTAT1(0x%x).\n",
        p->host_no, -1, -1, -1, status);
    aic_outb(p, status, CLRSINT1);
    aic_outb(p, CLRSCSIINT, CLRINT);
    unpause_sequencer(p,  TRUE);
    scb = NULL;
  }
  if (scb != NULL)
  {
    aic7xxx_done(p, scb);
  }
}

#ifdef AIC7XXX_VERBOSE_DEBUGGING
static void
aic7xxx_check_scbs(struct aic7xxx_host *p, char *buffer)
{
  unsigned char saved_scbptr, free_scbh, dis_scbh, wait_scbh, temp;
  int i, bogus, lost;
  static unsigned char scb_status[AIC7XXX_MAXSCB];

#define SCB_NO_LIST 0
#define SCB_FREE_LIST 1
#define SCB_WAITING_LIST 2
#define SCB_DISCONNECTED_LIST 4
#define SCB_CURRENTLY_ACTIVE 8

  bogus = FALSE;
  memset(&scb_status[0], 0, sizeof(scb_status));
  pause_sequencer(p);
  saved_scbptr = aic_inb(p, SCBPTR);
  if (saved_scbptr >= p->scb_data->maxhscbs)
  {
    printk("Bogus SCBPTR %d\n", saved_scbptr);
    bogus = TRUE;
  }
  scb_status[saved_scbptr] = SCB_CURRENTLY_ACTIVE;
  free_scbh = aic_inb(p, FREE_SCBH);
  if ( (free_scbh != SCB_LIST_NULL) &&
       (free_scbh >= p->scb_data->maxhscbs) )
  {
    printk("Bogus FREE_SCBH %d\n", free_scbh);
    bogus = TRUE;
  }
  else
  {
    temp = free_scbh;
    while( (temp != SCB_LIST_NULL) && (temp < p->scb_data->maxhscbs) )
    {
      if(scb_status[temp] & 0x07)
      {
        printk("HSCB %d on multiple lists, status 0x%02x", temp,
               scb_status[temp] | SCB_FREE_LIST);
        bogus = TRUE;
      }
      scb_status[temp] |= SCB_FREE_LIST;
      aic_outb(p, temp, SCBPTR);
      temp = aic_inb(p, SCB_NEXT);
    }
  }

  dis_scbh = aic_inb(p, DISCONNECTED_SCBH);
  if ( (dis_scbh != SCB_LIST_NULL) &&
       (dis_scbh >= p->scb_data->maxhscbs) )
  {
    printk("Bogus DISCONNECTED_SCBH %d\n", dis_scbh);
    bogus = TRUE;
  }
  else
  {
    temp = dis_scbh;
    while( (temp != SCB_LIST_NULL) && (temp < p->scb_data->maxhscbs) )
    {
      if(scb_status[temp] & 0x07)
      {
        printk("HSCB %d on multiple lists, status 0x%02x", temp,
               scb_status[temp] | SCB_DISCONNECTED_LIST);
        bogus = TRUE;
      }
      scb_status[temp] |= SCB_DISCONNECTED_LIST;
      aic_outb(p, temp, SCBPTR);
      temp = aic_inb(p, SCB_NEXT);
    }
  }
  
  wait_scbh = aic_inb(p, WAITING_SCBH);
  if ( (wait_scbh != SCB_LIST_NULL) &&
       (wait_scbh >= p->scb_data->maxhscbs) )
  {
    printk("Bogus WAITING_SCBH %d\n", wait_scbh);
    bogus = TRUE;
  }
  else
  {
    temp = wait_scbh;
    while( (temp != SCB_LIST_NULL) && (temp < p->scb_data->maxhscbs) )
    {
      if(scb_status[temp] & 0x07)
      {
        printk("HSCB %d on multiple lists, status 0x%02x", temp,
               scb_status[temp] | SCB_WAITING_LIST);
        bogus = TRUE;
      }
      scb_status[temp] |= SCB_WAITING_LIST;
      aic_outb(p, temp, SCBPTR);
      temp = aic_inb(p, SCB_NEXT);
    }
  }

  lost=0;
  for(i=0; i < p->scb_data->maxhscbs; i++)
  {
    aic_outb(p, i, SCBPTR);
    temp = aic_inb(p, SCB_NEXT);
    if ( ((temp != SCB_LIST_NULL) &&
          (temp >= p->scb_data->maxhscbs)) )
    {
      printk("HSCB %d bad, SCB_NEXT invalid(%d).\n", i, temp);
      bogus = TRUE;
    }
    if ( temp == i )
    {
      printk("HSCB %d bad, SCB_NEXT points to self.\n", i);
      bogus = TRUE;
    }
    if (scb_status[i] == 0)
      lost++;
    if (lost > 1)
    {
      printk("Too many lost scbs.\n");
      bogus=TRUE;
    }
  }
  aic_outb(p, saved_scbptr, SCBPTR);
  unpause_sequencer(p, FALSE);
  if (bogus)
  {
    printk("Bogus parameters found in card SCB array structures.\n");
    printk("%s\n", buffer);
    aic7xxx_panic_abort(p, NULL);
  }
  return;
}
#endif


static void
aic7xxx_handle_command_completion_intr(struct aic7xxx_host *p)
{
	struct aic7xxx_scb *scb = NULL;
	struct aic_dev_data *aic_dev;
	struct scsi_cmnd *cmd;
	unsigned char scb_index, tindex;

#ifdef AIC7XXX_VERBOSE_DEBUGGING
  if( (p->isr_count < 16) && (aic7xxx_verbose > 0xffff) )
    printk(INFO_LEAD "Command Complete Int.\n", p->host_no, -1, -1, -1);
#endif
    
  aic_outb(p, CLRCMDINT, CLRINT);
  aic_inb(p, INTSTAT);

  while (p->qoutfifo[p->qoutfifonext] != SCB_LIST_NULL)
  {
    scb_index = p->qoutfifo[p->qoutfifonext];
    p->qoutfifo[p->qoutfifonext++] = SCB_LIST_NULL;
    if ( scb_index >= p->scb_data->numscbs )
    {
      printk(WARN_LEAD "CMDCMPLT with invalid SCB index %d\n", p->host_no,
        -1, -1, -1, scb_index);
      continue;
    }
    scb = p->scb_data->scb_array[scb_index];
    if (!(scb->flags & SCB_ACTIVE) || (scb->cmd == NULL))
    {
      printk(WARN_LEAD "CMDCMPLT without command for SCB %d, SCB flags "
        "0x%x, cmd 0x%lx\n", p->host_no, -1, -1, -1, scb_index, scb->flags,
        (unsigned long) scb->cmd);
      continue;
    }
    tindex = TARGET_INDEX(scb->cmd);
    aic_dev = AIC_DEV(scb->cmd);
    if (scb->flags & SCB_QUEUED_ABORT)
    {
      pause_sequencer(p);
      if ( ((aic_inb(p, LASTPHASE) & PHASE_MASK) != P_BUSFREE) &&
           (aic_inb(p, SCB_TAG) == scb->hscb->tag) )
      {
        unpause_sequencer(p, FALSE);
        continue;
      }
      aic7xxx_reset_device(p, scb->cmd->device->id, scb->cmd->device->channel,
        scb->cmd->device->lun, scb->hscb->tag);
      scb->flags &= ~(SCB_QUEUED_FOR_DONE | SCB_RESET | SCB_ABORT |
        SCB_QUEUED_ABORT);
      unpause_sequencer(p, FALSE);
    }
    else if (scb->flags & SCB_ABORT)
    {
      scb->flags &= ~(SCB_ABORT|SCB_RESET);
    }
    else if (scb->flags & SCB_SENSE)
    {
      char *buffer = &scb->cmd->sense_buffer[0];

      if (buffer[12] == 0x47 || buffer[12] == 0x54)
      {
        aic_dev->needppr = aic_dev->needppr_copy;
        aic_dev->needsdtr = aic_dev->needsdtr_copy;
        aic_dev->needwdtr = aic_dev->needwdtr_copy;
      }
    }
    cmd = scb->cmd;
    if (scb->hscb->residual_SG_segment_count != 0)
    {
      aic7xxx_calculate_residual(p, scb);
    }
    cmd->result |= (aic7xxx_error(cmd) << 16);
    aic7xxx_done(p, scb);
  }
}

static void
aic7xxx_isr(void *dev_id)
{
  struct aic7xxx_host *p;
  unsigned char intstat;

  p = dev_id;

  if (!((intstat = aic_inb(p, INTSTAT)) & INT_PEND))
  {
#ifdef CONFIG_PCI
    if ( (p->chip & AHC_PCI) && (p->spurious_int > 500) &&
        !(p->flags & AHC_HANDLING_REQINITS) )
    {
      if ( aic_inb(p, ERROR) & PCIERRSTAT )
      {
        aic7xxx_pci_intr(p);
      }
      p->spurious_int = 0;
    }
    else if ( !(p->flags & AHC_HANDLING_REQINITS) )
    {
      p->spurious_int++;
    }
#endif
    return;
  }

  p->spurious_int = 0;

  p->isr_count++;

#ifdef AIC7XXX_VERBOSE_DEBUGGING
  if ( (p->isr_count < 16) && (aic7xxx_verbose > 0xffff) &&
       (aic7xxx_panic_on_abort) && (p->flags & AHC_PAGESCBS) )
    aic7xxx_check_scbs(p, "Bogus settings at start of interrupt.");
#endif

  if (intstat & CMDCMPLT)
  {
    aic7xxx_handle_command_completion_intr(p);
  }

  if (intstat & BRKADRINT)
  {
    int i;
    unsigned char errno = aic_inb(p, ERROR);

    printk(KERN_ERR "(scsi%d) BRKADRINT error(0x%x):\n", p->host_no, errno);
    for (i = 0; i < ARRAY_SIZE(hard_error); i++)
    {
      if (errno & hard_error[i].errno)
      {
        printk(KERN_ERR "  %s\n", hard_error[i].errmesg);
      }
    }
    printk(KERN_ERR "(scsi%d)   SEQADDR=0x%x\n", p->host_no,
      (((aic_inb(p, SEQADDR1) << 8) & 0x100) | aic_inb(p, SEQADDR0)));
    if (aic7xxx_panic_on_abort)
      aic7xxx_panic_abort(p, NULL);
#ifdef CONFIG_PCI
    if (errno & PCIERRSTAT)
      aic7xxx_pci_intr(p);
#endif
    if (errno & (SQPARERR | ILLOPCODE | ILLSADDR))
    {
      panic("aic7xxx: unrecoverable BRKADRINT.\n");
    }
    if (errno & ILLHADDR)
    {
      printk(KERN_ERR "(scsi%d) BUG! Driver accessed chip without first "
             "pausing controller!\n", p->host_no);
    }
#ifdef AIC7XXX_VERBOSE_DEBUGGING
    if (errno & DPARERR)
    {
      if (aic_inb(p, DMAPARAMS) & DIRECTION)
        printk("(scsi%d) while DMAing SCB from host to card.\n", p->host_no);
      else
        printk("(scsi%d) while DMAing SCB from card to host.\n", p->host_no);
    }
#endif
    aic_outb(p, CLRPARERR | CLRBRKADRINT, CLRINT);
    unpause_sequencer(p, FALSE);
  }

  if (intstat & SEQINT)
  {
    if(p->features & AHC_ULTRA2)
    {
      aic_inb(p, CCSCBCTL);
    }
    aic7xxx_handle_seqint(p, intstat);
  }

  if (intstat & SCSIINT)
  {
    aic7xxx_handle_scsiint(p, intstat);
  }

#ifdef AIC7XXX_VERBOSE_DEBUGGING
  if ( (p->isr_count < 16) && (aic7xxx_verbose > 0xffff) &&
       (aic7xxx_panic_on_abort) && (p->flags & AHC_PAGESCBS) )
    aic7xxx_check_scbs(p, "Bogus settings at end of interrupt.");
#endif

}

static irqreturn_t
do_aic7xxx_isr(int irq, void *dev_id)
{
  unsigned long cpu_flags;
  struct aic7xxx_host *p;
  
  p = dev_id;
  if(!p)
    return IRQ_NONE;
  spin_lock_irqsave(p->host->host_lock, cpu_flags);
  p->flags |= AHC_IN_ISR;
  do
  {
    aic7xxx_isr(dev_id);
  } while ( (aic_inb(p, INTSTAT) & INT_PEND) );
  aic7xxx_done_cmds_complete(p);
  aic7xxx_run_waiting_queues(p);
  p->flags &= ~AHC_IN_ISR;
  spin_unlock_irqrestore(p->host->host_lock, cpu_flags);

  return IRQ_HANDLED;
}

static void
aic7xxx_init_transinfo(struct aic7xxx_host *p, struct aic_dev_data *aic_dev)
{
  struct scsi_device *sdpnt = aic_dev->SDptr;
  unsigned char tindex;

  tindex = sdpnt->id | (sdpnt->channel << 3);
  if (!(aic_dev->flags & DEVICE_DTR_SCANNED))
  {
    aic_dev->flags |= DEVICE_DTR_SCANNED;

    if ( sdpnt->wdtr && (p->features & AHC_WIDE) )
    {
      aic_dev->needwdtr = aic_dev->needwdtr_copy = 1;
      aic_dev->goal.width = p->user[tindex].width;
    }
    else
    {
      aic_dev->needwdtr = aic_dev->needwdtr_copy = 0;
      pause_sequencer(p);
      aic7xxx_set_width(p, sdpnt->id, sdpnt->channel, sdpnt->lun,
                        MSG_EXT_WDTR_BUS_8_BIT, (AHC_TRANS_ACTIVE |
                                                 AHC_TRANS_GOAL |
                                                 AHC_TRANS_CUR), aic_dev );
      unpause_sequencer(p, FALSE);
    }
    if ( sdpnt->sdtr && p->user[tindex].offset )
    {
      aic_dev->goal.period = p->user[tindex].period;
      aic_dev->goal.options = p->user[tindex].options;
      if (p->features & AHC_ULTRA2)
        aic_dev->goal.offset = MAX_OFFSET_ULTRA2;
      else if (aic_dev->goal.width == MSG_EXT_WDTR_BUS_16_BIT)
        aic_dev->goal.offset = MAX_OFFSET_16BIT;
      else
        aic_dev->goal.offset = MAX_OFFSET_8BIT;
      if ( sdpnt->ppr && p->user[tindex].period <= 9 &&
             p->user[tindex].options )
      {
        aic_dev->needppr = aic_dev->needppr_copy = 1;
        aic_dev->needsdtr = aic_dev->needsdtr_copy = 0;
        aic_dev->needwdtr = aic_dev->needwdtr_copy = 0;
        aic_dev->flags |= DEVICE_SCSI_3;
      }
      else
      {
        aic_dev->needsdtr = aic_dev->needsdtr_copy = 1;
        aic_dev->goal.period = max_t(unsigned char, 10, aic_dev->goal.period);
        aic_dev->goal.options = 0;
      }
    }
    else
    {
      aic_dev->needsdtr = aic_dev->needsdtr_copy = 0;
      aic_dev->goal.period = 255;
      aic_dev->goal.offset = 0;
      aic_dev->goal.options = 0;
    }
    aic_dev->flags |= DEVICE_PRINT_DTR;
  }
}

static int
aic7xxx_slave_alloc(struct scsi_device *SDptr)
{
  struct aic7xxx_host *p = (struct aic7xxx_host *)SDptr->host->hostdata;
  struct aic_dev_data *aic_dev;

  aic_dev = kmalloc(sizeof(struct aic_dev_data), GFP_KERNEL);
  if(!aic_dev)
    return 1;
  
  if (!(p->flags & AHC_A_SCANNED) && (SDptr->channel == 0))
  {
    if (aic7xxx_verbose & VERBOSE_PROBE2)
      printk(INFO_LEAD "Scanning channel for devices.\n",
        p->host_no, 0, -1, -1);
    p->flags |= AHC_A_SCANNED;
  }
  else
  {
    if (!(p->flags & AHC_B_SCANNED) && (SDptr->channel == 1))
    {
      if (aic7xxx_verbose & VERBOSE_PROBE2)
        printk(INFO_LEAD "Scanning channel for devices.\n",
          p->host_no, 1, -1, -1);
      p->flags |= AHC_B_SCANNED;
    }
  }

  memset(aic_dev, 0, sizeof(struct aic_dev_data));
  SDptr->hostdata = aic_dev;
  aic_dev->SDptr = SDptr;
  aic_dev->max_q_depth = 1;
  aic_dev->temp_q_depth = 1;
  scbq_init(&aic_dev->delayed_scbs);
  INIT_LIST_HEAD(&aic_dev->list);
  list_add_tail(&aic_dev->list, &p->aic_devs);
  return 0;
}

static void
aic7xxx_device_queue_depth(struct aic7xxx_host *p, struct scsi_device *device)
{
  int tag_enabled = FALSE;
  struct aic_dev_data *aic_dev = device->hostdata;
  unsigned char tindex;

  tindex = device->id | (device->channel << 3);

  if (device->simple_tags)
    return; 

  if (device->tagged_supported)
  {
    tag_enabled = TRUE;

    if (!(p->discenable & (1 << tindex)))
    {
      if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
        printk(INFO_LEAD "Disconnection disabled, unable to "
             "enable tagged queueing.\n",
             p->host_no, device->channel, device->id, device->lun);
      tag_enabled = FALSE;
    }
    else
    {
      if (p->instance >= ARRAY_SIZE(aic7xxx_tag_info))
      {
        static int print_warning = TRUE;
        if(print_warning)
        {
          printk(KERN_INFO "aic7xxx: WARNING, insufficient tag_info instances for"
                           " installed controllers.\n");
          printk(KERN_INFO "aic7xxx: Please update the aic7xxx_tag_info array in"
                           " the aic7xxx.c source file.\n");
          print_warning = FALSE;
        }
        aic_dev->max_q_depth = aic_dev->temp_q_depth =
		aic7xxx_default_queue_depth;
      }
      else
      {

        if (aic7xxx_tag_info[p->instance].tag_commands[tindex] == 255)
        {
          tag_enabled = FALSE;
        }
        else if (aic7xxx_tag_info[p->instance].tag_commands[tindex] == 0)
        {
          aic_dev->max_q_depth = aic_dev->temp_q_depth =
		  aic7xxx_default_queue_depth;
        }
        else
        {
          aic_dev->max_q_depth = aic_dev->temp_q_depth = 
            aic7xxx_tag_info[p->instance].tag_commands[tindex];
        }
      }
    }
  }
  if (tag_enabled)
  {
    if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
    {
          printk(INFO_LEAD "Tagged queuing enabled, queue depth %d.\n",
            p->host_no, device->channel, device->id,
            device->lun, aic_dev->max_q_depth);
    }
    scsi_adjust_queue_depth(device, MSG_ORDERED_TAG, aic_dev->max_q_depth);
  }
  else
  {
    if (aic7xxx_verbose & VERBOSE_NEGOTIATION2)
    {
          printk(INFO_LEAD "Tagged queuing disabled, queue depth %d.\n",
            p->host_no, device->channel, device->id,
            device->lun, device->host->cmd_per_lun);
    }
    scsi_adjust_queue_depth(device, 0, device->host->cmd_per_lun);
  }
  return;
}

static void
aic7xxx_slave_destroy(struct scsi_device *SDptr)
{
  struct aic_dev_data *aic_dev = SDptr->hostdata;

  list_del(&aic_dev->list);
  SDptr->hostdata = NULL;
  kfree(aic_dev);
  return;
}

static int
aic7xxx_slave_configure(struct scsi_device *SDptr)
{
  struct aic7xxx_host *p = (struct aic7xxx_host *) SDptr->host->hostdata;
  struct aic_dev_data *aic_dev;
  int scbnum;

  aic_dev = (struct aic_dev_data *)SDptr->hostdata;

  aic7xxx_init_transinfo(p, aic_dev);
  aic7xxx_device_queue_depth(p, SDptr);
  if(list_empty(&aic_dev->list))
    list_add_tail(&aic_dev->list, &p->aic_devs);

  scbnum = 0;
  list_for_each_entry(aic_dev, &p->aic_devs, list) {
    scbnum += aic_dev->max_q_depth;
  }
  while (scbnum > p->scb_data->numscbs)
  {
    if ( aic7xxx_allocate_scb(p) == 0 )
      break;
  }


  return(0);
}

#if defined(__i386__) || defined(__alpha__)
static int
aic7xxx_probe(int slot, int base, ahc_flag_type *flags)
{
  int i;
  unsigned char buf[4];

  static struct {
    int n;
    unsigned char signature[sizeof(buf)];
    ahc_chip type;
    int bios_disabled;
  } AIC7xxx[] = {
    { 4, { 0x04, 0x90, 0x77, 0x70 },
      AHC_AIC7770|AHC_EISA, FALSE },  
    { 4, { 0x04, 0x90, 0x77, 0x71 },
      AHC_AIC7770|AHC_EISA, FALSE }, 
    { 4, { 0x04, 0x90, 0x77, 0x56 },
      AHC_AIC7770|AHC_VL, FALSE }, 
    { 4, { 0x04, 0x90, 0x77, 0x57 },
      AHC_AIC7770|AHC_VL, TRUE }   
  };

  for (i = 0; i < sizeof(buf); i++)
  {
    outb(0x80 + i, base);
    buf[i] = inb(base + i);
  }

  for (i = 0; i < ARRAY_SIZE(AIC7xxx); i++)
  {
    if (!memcmp(buf, AIC7xxx[i].signature, AIC7xxx[i].n))
    {
      if (inb(base + 4) & 1)
      {
        if (AIC7xxx[i].bios_disabled)
        {
          *flags |= AHC_USEDEFAULTS;
        }
        else
        {
          *flags |= AHC_BIOS_ENABLED;
        }
        return (i);
      }

      printk("aic7xxx: <Adaptec 7770 SCSI Host Adapter> "
             "disabled at slot %d, ignored.\n", slot);
    }
  }

  return (-1);
}
#endif 


static int
read_284x_seeprom(struct aic7xxx_host *p, struct seeprom_config *sc)
{
  int i = 0, k = 0;
  unsigned char temp;
  unsigned short checksum = 0;
  unsigned short *seeprom = (unsigned short *) sc;
  struct seeprom_cmd {
    unsigned char len;
    unsigned char bits[3];
  };
  struct seeprom_cmd seeprom_read = {3, {1, 1, 0}};

#define CLOCK_PULSE(p) \
  while ((aic_inb(p, STATUS_2840) & EEPROM_TF) == 0)        \
  {                                                \
    ;                                  \
  }                                                \
  (void) aic_inb(p, SEECTL_2840);

  for (k = 0; k < (sizeof(*sc) / 2); k++)
  {
    aic_outb(p, CK_2840 | CS_2840, SEECTL_2840);
    CLOCK_PULSE(p);

    for (i = 0; i < seeprom_read.len; i++)
    {
      temp = CS_2840 | seeprom_read.bits[i];
      aic_outb(p, temp, SEECTL_2840);
      CLOCK_PULSE(p);
      temp = temp ^ CK_2840;
      aic_outb(p, temp, SEECTL_2840);
      CLOCK_PULSE(p);
    }
    for (i = 5; i >= 0; i--)
    {
      temp = k;
      temp = (temp >> i) & 1;  
      temp = CS_2840 | temp;
      aic_outb(p, temp, SEECTL_2840);
      CLOCK_PULSE(p);
      temp = temp ^ CK_2840;
      aic_outb(p, temp, SEECTL_2840);
      CLOCK_PULSE(p);
    }

    for (i = 0; i <= 16; i++)
    {
      temp = CS_2840;
      aic_outb(p, temp, SEECTL_2840);
      CLOCK_PULSE(p);
      temp = temp ^ CK_2840;
      seeprom[k] = (seeprom[k] << 1) | (aic_inb(p, STATUS_2840) & DI_2840);
      aic_outb(p, temp, SEECTL_2840);
      CLOCK_PULSE(p);
    }
    if (k < (sizeof(*sc) / 2) - 1)
    {
      checksum = checksum + seeprom[k];
    }

    aic_outb(p, 0, SEECTL_2840);
    CLOCK_PULSE(p);
    aic_outb(p, CK_2840, SEECTL_2840);
    CLOCK_PULSE(p);
    aic_outb(p, 0, SEECTL_2840);
    CLOCK_PULSE(p);
  }

#if 0
  printk("Computed checksum 0x%x, checksum read 0x%x\n", checksum, sc->checksum);
  printk("Serial EEPROM:");
  for (k = 0; k < (sizeof(*sc) / 2); k++)
  {
    if (((k % 8) == 0) && (k != 0))
    {
      printk("\n              ");
    }
    printk(" 0x%x", seeprom[k]);
  }
  printk("\n");
#endif

  if (checksum != sc->checksum)
  {
    printk("aic7xxx: SEEPROM checksum error, ignoring SEEPROM settings.\n");
    return (0);
  }

  return (1);
#undef CLOCK_PULSE
}

#define CLOCK_PULSE(p)                                               \
  do {                                                               \
    int limit = 0;                                                   \
    do {                                                             \
      mb();                                                          \
      pause_sequencer(p);     \
                             \
                               \
                              \
                            \
                                                        \
      udelay(1);                                     \
    } while (((aic_inb(p, SEECTL) & SEERDY) == 0) && (++limit < 1000)); \
  } while(0)

static int
acquire_seeprom(struct aic7xxx_host *p)
{

  aic_outb(p, SEEMS, SEECTL);
  CLOCK_PULSE(p);
  if ((aic_inb(p, SEECTL) & SEERDY) == 0)
  {
    aic_outb(p, 0, SEECTL);
    return (0);
  }
  return (1);
}

static void
release_seeprom(struct aic7xxx_host *p)
{
  CLOCK_PULSE(p);
  aic_outb(p, 0, SEECTL);
}

static int
read_seeprom(struct aic7xxx_host *p, int offset, 
    unsigned short *scarray, unsigned int len, seeprom_chip_type chip)
{
  int i = 0, k;
  unsigned char temp;
  unsigned short checksum = 0;
  struct seeprom_cmd {
    unsigned char len;
    unsigned char bits[3];
  };
  struct seeprom_cmd seeprom_read = {3, {1, 1, 0}};

  if (acquire_seeprom(p) == 0)
  {
    return (0);
  }

  for (k = 0; k < len; k++)
  {
    aic_outb(p, SEEMS | SEECK | SEECS, SEECTL);
    CLOCK_PULSE(p);

    for (i = 0; i < seeprom_read.len; i++)
    {
      temp = SEEMS | SEECS | (seeprom_read.bits[i] << 1);
      aic_outb(p, temp, SEECTL);
      CLOCK_PULSE(p);
      temp = temp ^ SEECK;
      aic_outb(p, temp, SEECTL);
      CLOCK_PULSE(p);
    }
    for (i = ((int) chip - 1); i >= 0; i--)
    {
      temp = k + offset;
      temp = (temp >> i) & 1;  
      temp = SEEMS | SEECS | (temp << 1);
      aic_outb(p, temp, SEECTL);
      CLOCK_PULSE(p);
      temp = temp ^ SEECK;
      aic_outb(p, temp, SEECTL);
      CLOCK_PULSE(p);
    }

    for (i = 0; i <= 16; i++)
    {
      temp = SEEMS | SEECS;
      aic_outb(p, temp, SEECTL);
      CLOCK_PULSE(p);
      temp = temp ^ SEECK;
      scarray[k] = (scarray[k] << 1) | (aic_inb(p, SEECTL) & SEEDI);
      aic_outb(p, temp, SEECTL);
      CLOCK_PULSE(p);
    }

    if (k < (len - 1))
    {
      checksum = checksum + scarray[k];
    }

    aic_outb(p, SEEMS, SEECTL);
    CLOCK_PULSE(p);
    aic_outb(p, SEEMS | SEECK, SEECTL);
    CLOCK_PULSE(p);
    aic_outb(p, SEEMS, SEECTL);
    CLOCK_PULSE(p);
  }

  release_seeprom(p);

#if 0
  printk("Computed checksum 0x%x, checksum read 0x%x\n",
         checksum, scarray[len - 1]);
  printk("Serial EEPROM:");
  for (k = 0; k < len; k++)
  {
    if (((k % 8) == 0) && (k != 0))
    {
      printk("\n              ");
    }
    printk(" 0x%x", scarray[k]);
  }
  printk("\n");
#endif
  if ( (checksum != scarray[len - 1]) || (checksum == 0) )
  {
    return (0);
  }

  return (1);
}

static unsigned char
read_brdctl(struct aic7xxx_host *p)
{
  unsigned char brdctl, value;

  CLOCK_PULSE(p);
  if (p->features & AHC_ULTRA2)
  {
    brdctl = BRDRW_ULTRA2;
    aic_outb(p, brdctl, BRDCTL);
    CLOCK_PULSE(p);
    value = aic_inb(p, BRDCTL);
    CLOCK_PULSE(p);
    return(value);
  }
  brdctl = BRDRW;
  if ( !((p->chip & AHC_CHIPID_MASK) == AHC_AIC7895) ||
        (p->flags & AHC_CHNLB) )
  {
    brdctl |= BRDCS;
  }
  aic_outb(p, brdctl, BRDCTL);
  CLOCK_PULSE(p);
  value = aic_inb(p, BRDCTL);
  CLOCK_PULSE(p);
  aic_outb(p, 0, BRDCTL);
  CLOCK_PULSE(p);
  return (value);
}

static void
write_brdctl(struct aic7xxx_host *p, unsigned char value)
{
  unsigned char brdctl;

  CLOCK_PULSE(p);
  if (p->features & AHC_ULTRA2)
  {
    brdctl = value;
    aic_outb(p, brdctl, BRDCTL);
    CLOCK_PULSE(p);
    brdctl |= BRDSTB_ULTRA2;
    aic_outb(p, brdctl, BRDCTL);
    CLOCK_PULSE(p);
    brdctl &= ~BRDSTB_ULTRA2;
    aic_outb(p, brdctl, BRDCTL);
    CLOCK_PULSE(p);
    read_brdctl(p);
    CLOCK_PULSE(p);
  }
  else
  {
    brdctl = BRDSTB;
    if ( !((p->chip & AHC_CHIPID_MASK) == AHC_AIC7895) ||
          (p->flags & AHC_CHNLB) )
    {
      brdctl |= BRDCS;
    }
    brdctl = BRDSTB | BRDCS;
    aic_outb(p, brdctl, BRDCTL);
    CLOCK_PULSE(p);
    brdctl |= value;
    aic_outb(p, brdctl, BRDCTL);
    CLOCK_PULSE(p);
    brdctl &= ~BRDSTB;
    aic_outb(p, brdctl, BRDCTL);
    CLOCK_PULSE(p);
    brdctl &= ~BRDCS;
    aic_outb(p, brdctl, BRDCTL);
    CLOCK_PULSE(p);
  }
}

static void
aic785x_cable_detect(struct aic7xxx_host *p, int *int_50,
    int *ext_present, int *eeprom)
{
  unsigned char brdctl;

  aic_outb(p, BRDRW | BRDCS, BRDCTL);
  CLOCK_PULSE(p);
  aic_outb(p, 0, BRDCTL);
  CLOCK_PULSE(p);
  brdctl = aic_inb(p, BRDCTL);
  CLOCK_PULSE(p);
  *int_50 = !(brdctl & BRDDAT5);
  *ext_present = !(brdctl & BRDDAT6);
  *eeprom = (aic_inb(p, SPIOCAP) & EEPROM);
}

#undef CLOCK_PULSE

static void
aic2940_uwpro_wide_cable_detect(struct aic7xxx_host *p, int *int_68,
    int *ext_68, int *eeprom)
{
  unsigned char brdctl;

  write_brdctl(p, 0);

  brdctl = read_brdctl(p);
  *int_68 = !(brdctl & BRDDAT7);

  write_brdctl(p, BRDDAT5);
  brdctl = read_brdctl(p);

  *ext_68 = !(brdctl & BRDDAT6);
  *eeprom = !(brdctl & BRDDAT7);

}

static void
aic787x_cable_detect(struct aic7xxx_host *p, int *int_50, int *int_68,
    int *ext_present, int *eeprom)
{
  unsigned char brdctl;

  write_brdctl(p, 0);

  brdctl = read_brdctl(p);
  *int_50 = !(brdctl & BRDDAT6);
  *int_68 = !(brdctl & BRDDAT7);

  write_brdctl(p, BRDDAT5);
  brdctl = read_brdctl(p);

  *ext_present = !(brdctl & BRDDAT6);
  *eeprom = !(brdctl & BRDDAT7);

}

static void
aic7xxx_ultra2_term_detect(struct aic7xxx_host *p, int *enableSE_low,
                           int *enableSE_high, int *enableLVD_low,
                           int *enableLVD_high, int *eprom_present)
{
  unsigned char brdctl;

  brdctl = read_brdctl(p);

  *eprom_present  = (brdctl & BRDDAT7);
  *enableSE_high  = (brdctl & BRDDAT6);
  *enableSE_low   = (brdctl & BRDDAT5);
  *enableLVD_high = (brdctl & BRDDAT4);
  *enableLVD_low  = (brdctl & BRDDAT3);
}

static void
configure_termination(struct aic7xxx_host *p)
{
  int internal50_present = 0;
  int internal68_present = 0;
  int external_present = 0;
  int eprom_present = 0;
  int enableSE_low = 0;
  int enableSE_high = 0;
  int enableLVD_low = 0;
  int enableLVD_high = 0;
  unsigned char brddat = 0;
  unsigned char max_target = 0;
  unsigned char sxfrctl1 = aic_inb(p, SXFRCTL1);

  if (acquire_seeprom(p))
  {
    if (p->features & (AHC_WIDE|AHC_TWIN))
      max_target = 16;
    else
      max_target = 8;
    aic_outb(p, SEEMS | SEECS, SEECTL);
    sxfrctl1 &= ~STPWEN;
    if (p->features & AHC_ULTRA2)
    {
      if (aic7xxx_override_term == -1)
      {
        aic7xxx_ultra2_term_detect(p, &enableSE_low, &enableSE_high,
                                   &enableLVD_low, &enableLVD_high,
                                   &eprom_present);
      }
      
      if (!(p->adapter_control & CFSEAUTOTERM))
      {
        enableSE_low = (p->adapter_control & CFSTERM);
        enableSE_high = (p->adapter_control & CFWSTERM);
      }
      if (!(p->adapter_control & CFAUTOTERM))
      {
        enableLVD_low = enableLVD_high = (p->adapter_control & CFLVDSTERM);
      }

      /*
       * Now take those settings that we have and translate them into the
       * values that must be written into the registers.
       *
       * Flash Enable = BRDDAT7
       * Secondary High Term Enable = BRDDAT6
       * Secondary Low Term Enable = BRDDAT5
       * LVD/Primary High Term Enable = BRDDAT4
       * LVD/Primary Low Term Enable = STPWEN bit in SXFRCTL1
       */
      if (enableLVD_low != 0)
      {
        sxfrctl1 |= STPWEN;
        p->flags |= AHC_TERM_ENB_LVD;
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk(KERN_INFO "(scsi%d) LVD/Primary Low byte termination "
                 "Enabled\n", p->host_no);
      }
          
      if (enableLVD_high != 0)
      {
        brddat |= BRDDAT4;
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk(KERN_INFO "(scsi%d) LVD/Primary High byte termination "
                 "Enabled\n", p->host_no);
      }

      if (enableSE_low != 0)
      {
        brddat |= BRDDAT5;
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk(KERN_INFO "(scsi%d) Secondary Low byte termination "
                 "Enabled\n", p->host_no);
      }

      if (enableSE_high != 0)
      {
        brddat |= BRDDAT6;
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk(KERN_INFO "(scsi%d) Secondary High byte termination "
                 "Enabled\n", p->host_no);
      }
    }
    else if (p->features & AHC_NEW_AUTOTERM)
    {
      sxfrctl1 |= STPWEN;
      if (aic7xxx_verbose & VERBOSE_PROBE2)
        printk(KERN_INFO "(scsi%d) Narrow channel termination Enabled\n",
               p->host_no);

      if (p->adapter_control & CFAUTOTERM)
      {
        aic2940_uwpro_wide_cable_detect(p, &internal68_present,
                                        &external_present,
                                        &eprom_present);
        printk(KERN_INFO "(scsi%d) Cables present (Int-50 %s, Int-68 %s, "
               "Ext-68 %s)\n", p->host_no,
               "Don't Care",
               internal68_present ? "YES" : "NO",
               external_present ? "YES" : "NO");
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk(KERN_INFO "(scsi%d) EEPROM %s present.\n", p->host_no,
               eprom_present ? "is" : "is not");
        if (internal68_present && external_present)
        {
          brddat = 0;
          p->flags &= ~AHC_TERM_ENB_SE_HIGH;
          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk(KERN_INFO "(scsi%d) Wide channel termination Disabled\n",
                   p->host_no);
        }
        else
        {
          brddat = BRDDAT6;
          p->flags |= AHC_TERM_ENB_SE_HIGH;
          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk(KERN_INFO "(scsi%d) Wide channel termination Enabled\n",
                   p->host_no);
        }
      }
      else
      {
        if (p->adapter_control & CFWSTERM)
        {
          brddat = BRDDAT6;
          p->flags |= AHC_TERM_ENB_SE_HIGH;
          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk(KERN_INFO "(scsi%d) Wide channel termination Enabled\n",
                   p->host_no);
        }
        else
        {
          brddat = 0;
        }
      }
    }
    else
    {
      if (p->adapter_control & CFAUTOTERM)
      {
        if (p->flags & AHC_MOTHERBOARD)
        {
          printk(KERN_INFO "(scsi%d) Warning - detected auto-termination\n",
                 p->host_no);
          printk(KERN_INFO "(scsi%d) Please verify driver detected settings "
            "are correct.\n", p->host_no);
          printk(KERN_INFO "(scsi%d) If not, then please properly set the "
            "device termination\n", p->host_no);
          printk(KERN_INFO "(scsi%d) in the Adaptec SCSI BIOS by hitting "
            "CTRL-A when prompted\n", p->host_no);
          printk(KERN_INFO "(scsi%d) during machine bootup.\n", p->host_no);
        }
        

        if ( (p->chip & AHC_CHIPID_MASK) >= AHC_AIC7870 )
        {
          aic787x_cable_detect(p, &internal50_present, &internal68_present,
            &external_present, &eprom_present);
        }
        else
        {
          aic785x_cable_detect(p, &internal50_present, &external_present,
            &eprom_present);
        }

        if (max_target <= 8)
          internal68_present = 0;

        if (max_target > 8)
        {
          printk(KERN_INFO "(scsi%d) Cables present (Int-50 %s, Int-68 %s, "
                 "Ext-68 %s)\n", p->host_no,
                 internal50_present ? "YES" : "NO",
                 internal68_present ? "YES" : "NO",
                 external_present ? "YES" : "NO");
        }
        else
        {
          printk(KERN_INFO "(scsi%d) Cables present (Int-50 %s, Ext-50 %s)\n",
                 p->host_no,
                 internal50_present ? "YES" : "NO",
                 external_present ? "YES" : "NO");
        }
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk(KERN_INFO "(scsi%d) EEPROM %s present.\n", p->host_no,
               eprom_present ? "is" : "is not");

        if (internal50_present && internal68_present && external_present)
        {
          printk(KERN_INFO "(scsi%d) Illegal cable configuration!!  Only two\n",
                 p->host_no);
          printk(KERN_INFO "(scsi%d) connectors on the SCSI controller may be "
                 "in use at a time!\n", p->host_no);
          internal50_present = external_present = 0;
          enableSE_high = enableSE_low = 1;
        }

        if ((max_target > 8) &&
            ((external_present == 0) || (internal68_present == 0)) )
        {
          brddat |= BRDDAT6;
          p->flags |= AHC_TERM_ENB_SE_HIGH;
          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk(KERN_INFO "(scsi%d) SE High byte termination Enabled\n",
                   p->host_no);
        }

        if ( ((internal50_present ? 1 : 0) +
              (internal68_present ? 1 : 0) +
              (external_present   ? 1 : 0)) <= 1 )
        {
          sxfrctl1 |= STPWEN;
          p->flags |= AHC_TERM_ENB_SE_LOW;
          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk(KERN_INFO "(scsi%d) SE Low byte termination Enabled\n",
                   p->host_no);
        }
      }
      else 
      {
        if (p->adapter_control & CFSTERM)
        {
          sxfrctl1 |= STPWEN;
          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk(KERN_INFO "(scsi%d) SE Low byte termination Enabled\n",
                   p->host_no);
        }

        if (p->adapter_control & CFWSTERM)
        {
          brddat |= BRDDAT6;
          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk(KERN_INFO "(scsi%d) SE High byte termination Enabled\n",
                   p->host_no);
        }
      }
    }

    aic_outb(p, sxfrctl1, SXFRCTL1);
    write_brdctl(p, brddat);
    release_seeprom(p);
  }
}

static void
detect_maxscb(struct aic7xxx_host *p)
{
  int i;

  if (p->scb_data->maxhscbs == 0)
  {
    aic_outb(p, 0, FREE_SCBH);

    for (i = 0; i < AIC7XXX_MAXSCB; i++)
    {
      aic_outb(p, i, SCBPTR);
      aic_outb(p, i, SCB_CONTROL);
      if (aic_inb(p, SCB_CONTROL) != i)
        break;
      aic_outb(p, 0, SCBPTR);
      if (aic_inb(p, SCB_CONTROL) != 0)
        break;

      aic_outb(p, i, SCBPTR);
      aic_outb(p, 0, SCB_CONTROL);   
      aic_outb(p, i + 1, SCB_NEXT);  
      aic_outb(p, SCB_LIST_NULL, SCB_TAG);  
      aic_outb(p, SCB_LIST_NULL, SCB_BUSYTARGETS);  
      aic_outb(p, SCB_LIST_NULL, SCB_BUSYTARGETS+1);
      aic_outb(p, SCB_LIST_NULL, SCB_BUSYTARGETS+2);
      aic_outb(p, SCB_LIST_NULL, SCB_BUSYTARGETS+3);
    }

    
    aic_outb(p, i - 1, SCBPTR);
    aic_outb(p, SCB_LIST_NULL, SCB_NEXT);

    
    aic_outb(p, 0, SCBPTR);
    aic_outb(p, 0, SCB_CONTROL);

    p->scb_data->maxhscbs = i;
    if ( i == AIC7XXX_MAXSCB )
      p->flags &= ~AHC_PAGESCBS;
  }

}

static int
aic7xxx_register(struct scsi_host_template *template, struct aic7xxx_host *p,
  int reset_delay)
{
  int i, result;
  int max_targets;
  int found = 1;
  unsigned char term, scsi_conf;
  struct Scsi_Host *host;

  host = p->host;

  p->scb_data->maxscbs = AIC7XXX_MAXSCB;
  host->can_queue = AIC7XXX_MAXSCB;
  host->cmd_per_lun = 3;
  host->sg_tablesize = AIC7XXX_MAX_SG;
  host->this_id = p->scsi_id;
  host->io_port = p->base;
  host->n_io_port = 0xFF;
  host->base = p->mbase;
  host->irq = p->irq;
  if (p->features & AHC_WIDE)
  {
    host->max_id = 16;
  }
  if (p->features & AHC_TWIN)
  {
    host->max_channel = 1;
  }

  p->host = host;
  p->host_no = host->host_no;
  host->unique_id = p->instance;
  p->isr_count = 0;
  p->next = NULL;
  p->completeq.head = NULL;
  p->completeq.tail = NULL;
  scbq_init(&p->scb_data->free_scbs);
  scbq_init(&p->waiting_scbs);
  INIT_LIST_HEAD(&p->aic_devs);

  p->qinfifonext = 0;
  p->qoutfifonext = 0;

  printk(KERN_INFO "(scsi%d) <%s> found at ", p->host_no,
    board_names[p->board_name_index]);
  switch(p->chip)
  {
    case (AHC_AIC7770|AHC_EISA):
      printk("EISA slot %d\n", p->pci_device_fn);
      break;
    case (AHC_AIC7770|AHC_VL):
      printk("VLB slot %d\n", p->pci_device_fn);
      break;
    default:
      printk("PCI %d/%d/%d\n", p->pci_bus, PCI_SLOT(p->pci_device_fn),
        PCI_FUNC(p->pci_device_fn));
      break;
  }
  if (p->features & AHC_TWIN)
  {
    printk(KERN_INFO "(scsi%d) Twin Channel, A SCSI ID %d, B SCSI ID %d, ",
           p->host_no, p->scsi_id, p->scsi_id_b);
  }
  else
  {
    char *channel;

    channel = "";

    if ((p->flags & AHC_MULTI_CHANNEL) != 0)
    {
      channel = " A";

      if ( (p->flags & (AHC_CHNLB|AHC_CHNLC)) != 0 )
      {
        channel = (p->flags & AHC_CHNLB) ? " B" : " C";
      }
    }
    if (p->features & AHC_WIDE)
    {
      printk(KERN_INFO "(scsi%d) Wide ", p->host_no);
    }
    else
    {
      printk(KERN_INFO "(scsi%d) Narrow ", p->host_no);
    }
    printk("Channel%s, SCSI ID=%d, ", channel, p->scsi_id);
  }
  aic_outb(p, 0, SEQ_FLAGS);

  detect_maxscb(p);

  printk("%d/%d SCBs\n", p->scb_data->maxhscbs, p->scb_data->maxscbs);
  if (aic7xxx_verbose & VERBOSE_PROBE2)
  {
    printk(KERN_INFO "(scsi%d) BIOS %sabled, IO Port 0x%lx, IRQ %d\n",
      p->host_no, (p->flags & AHC_BIOS_ENABLED) ? "en" : "dis",
      p->base, p->irq);
    printk(KERN_INFO "(scsi%d) IO Memory at 0x%lx, MMAP Memory at %p\n",
      p->host_no, p->mbase, p->maddr);
  }

#ifdef CONFIG_PCI
  if (aic7xxx_stpwlev != -1)
  {
    if ( (p->chip & ~AHC_CHIPID_MASK) == AHC_PCI)
    {
      unsigned char devconfig;

      pci_read_config_byte(p->pdev, DEVCONFIG, &devconfig);
      if ( (aic7xxx_stpwlev >> p->instance) & 0x01 )
      {
        devconfig |= STPWLEVEL;
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk("(scsi%d) Force setting STPWLEVEL bit\n", p->host_no);
      }
      else
      {
        devconfig &= ~STPWLEVEL;
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk("(scsi%d) Force clearing STPWLEVEL bit\n", p->host_no);
      }
      pci_write_config_byte(p->pdev, DEVCONFIG, devconfig);
    }
  }
#endif

  if (aic7xxx_override_term != -1)
  {
    if ( (p->chip & ~AHC_CHIPID_MASK) == AHC_PCI)
    {
      unsigned char term_override;

      term_override = ( (aic7xxx_override_term >> (p->instance * 4)) & 0x0f);
      p->adapter_control &= 
        ~(CFSTERM|CFWSTERM|CFLVDSTERM|CFAUTOTERM|CFSEAUTOTERM);
      if ( (p->features & AHC_ULTRA2) && (term_override & 0x0c) )
      {
        p->adapter_control |= CFLVDSTERM;
      }
      if (term_override & 0x02)
      {
        p->adapter_control |= CFWSTERM;
      }
      if (term_override & 0x01)
      {
        p->adapter_control |= CFSTERM;
      }
    }
  }

  if ( (p->flags & AHC_SEEPROM_FOUND) || (aic7xxx_override_term != -1) )
  {
    if (p->features & AHC_SPIOCAP)
    {
      if ( aic_inb(p, SPIOCAP) & SSPIOCPS )
        configure_termination(p);
    }
    else if ((p->chip & AHC_CHIPID_MASK) >= AHC_AIC7870)
    {
      configure_termination(p);
    }
  }

  if (p->features & AHC_TWIN)
  {
    
    aic_outb(p, aic_inb(p, SBLKCTL) | SELBUSB, SBLKCTL);

    if ((p->flags & AHC_SEEPROM_FOUND) || (aic7xxx_override_term != -1))
      term = (aic_inb(p, SXFRCTL1) & STPWEN);
    else
      term = ((p->flags & AHC_TERM_ENB_B) ? STPWEN : 0);

    aic_outb(p, p->scsi_id_b, SCSIID);
    scsi_conf = aic_inb(p, SCSICONF + 1);
    aic_outb(p, DFON | SPIOEN, SXFRCTL0);
    aic_outb(p, (scsi_conf & ENSPCHK) | aic7xxx_seltime | term | 
         ENSTIMER | ACTNEGEN, SXFRCTL1);
    aic_outb(p, 0, SIMODE0);
    aic_outb(p, ENSELTIMO | ENSCSIRST | ENSCSIPERR, SIMODE1);
    aic_outb(p, 0, SCSIRATE);

    
    aic_outb(p, aic_inb(p, SBLKCTL) & ~SELBUSB, SBLKCTL);
  }

  if (p->features & AHC_ULTRA2)
  {
    aic_outb(p, p->scsi_id, SCSIID_ULTRA2);
  }
  else
  {
    aic_outb(p, p->scsi_id, SCSIID);
  }
  if ((p->flags & AHC_SEEPROM_FOUND) || (aic7xxx_override_term != -1))
    term = (aic_inb(p, SXFRCTL1) & STPWEN);
  else
    term = ((p->flags & (AHC_TERM_ENB_A|AHC_TERM_ENB_LVD)) ? STPWEN : 0);
  scsi_conf = aic_inb(p, SCSICONF);
  aic_outb(p, DFON | SPIOEN, SXFRCTL0);
  aic_outb(p, (scsi_conf & ENSPCHK) | aic7xxx_seltime | term | 
       ENSTIMER | ACTNEGEN, SXFRCTL1);
  aic_outb(p, 0, SIMODE0);
  if(p->flags & AHC_NO_STPWEN)
    aic_outb(p, ENSELTIMO | ENSCSIPERR, SIMODE1);
  else
    aic_outb(p, ENSELTIMO | ENSCSIRST | ENSCSIPERR, SIMODE1);
  aic_outb(p, 0, SCSIRATE);
  if ( p->features & AHC_ULTRA2)
    aic_outb(p, 0, SCSIOFFSET);

  if ((p->features & (AHC_TWIN|AHC_WIDE)) == 0)
  {
    max_targets = 8;
  }
  else
  {
    max_targets = 16;
  }

  if (!(aic7xxx_no_reset))
  {
    aic_outb(p, 0, ULTRA_ENB);
    aic_outb(p, 0, ULTRA_ENB + 1);
    p->ultraenb = 0;
  }

  {
    size_t array_size;
    unsigned int hscb_physaddr;

    array_size = p->scb_data->maxscbs * sizeof(struct aic7xxx_hwscb);
    if (p->scb_data->hscbs == NULL)
    {
      p->scb_data->hscbs = pci_alloc_consistent(p->pdev, array_size,
						&p->scb_data->hscbs_dma);
      
      p->scb_data->hscb_kmalloc_ptr = NULL;
      p->scb_data->hscbs_dma_len = array_size;
    }
    if (p->scb_data->hscbs == NULL)
    {
      printk("(scsi%d) Unable to allocate hardware SCB array; "
             "failing detection.\n", p->host_no);
      aic_outb(p, 0, SIMODE1);
      p->irq = 0;
      return(0);
    }

    hscb_physaddr = p->scb_data->hscbs_dma;
    aic_outb(p, hscb_physaddr & 0xFF, HSCB_ADDR);
    aic_outb(p, (hscb_physaddr >> 8) & 0xFF, HSCB_ADDR + 1);
    aic_outb(p, (hscb_physaddr >> 16) & 0xFF, HSCB_ADDR + 2);
    aic_outb(p, (hscb_physaddr >> 24) & 0xFF, HSCB_ADDR + 3);

    
    p->untagged_scbs = pci_alloc_consistent(p->pdev, 3*256, &p->fifo_dma);
    if (p->untagged_scbs == NULL)
    {
      printk("(scsi%d) Unable to allocate hardware FIFO arrays; "
             "failing detection.\n", p->host_no);
      p->irq = 0;
      return(0);
    }

    p->qoutfifo = p->untagged_scbs + 256;
    p->qinfifo = p->qoutfifo + 256;
    for (i = 0; i < 256; i++)
    {
      p->untagged_scbs[i] = SCB_LIST_NULL;
      p->qinfifo[i] = SCB_LIST_NULL;
      p->qoutfifo[i] = SCB_LIST_NULL;
    }

    hscb_physaddr = p->fifo_dma;
    aic_outb(p, hscb_physaddr & 0xFF, SCBID_ADDR);
    aic_outb(p, (hscb_physaddr >> 8) & 0xFF, SCBID_ADDR + 1);
    aic_outb(p, (hscb_physaddr >> 16) & 0xFF, SCBID_ADDR + 2);
    aic_outb(p, (hscb_physaddr >> 24) & 0xFF, SCBID_ADDR + 3);
  }

  
  aic_outb(p, 0, QINPOS);
  aic_outb(p, 0, KERNEL_QINPOS);
  aic_outb(p, 0, QOUTPOS);

  if(p->features & AHC_QUEUE_REGS)
  {
    aic_outb(p, SCB_QSIZE_256, QOFF_CTLSTA);
    aic_outb(p, 0, SDSCB_QOFF);
    aic_outb(p, 0, SNSCB_QOFF);
    aic_outb(p, 0, HNSCB_QOFF);
  }

  aic_outb(p, SCB_LIST_NULL, WAITING_SCBH);
  aic_outb(p, SCB_LIST_NULL, DISCONNECTED_SCBH);

  aic_outb(p, MSG_NOOP, MSG_OUT);
  aic_outb(p, MSG_NOOP, LAST_MSG);

  aic_outb(p, 0, TMODE_CMDADDR);
  aic_outb(p, 0, TMODE_CMDADDR + 1);
  aic_outb(p, 0, TMODE_CMDADDR + 2);
  aic_outb(p, 0, TMODE_CMDADDR + 3);
  aic_outb(p, 0, TMODE_CMDADDR_NEXT);

  p->next = first_aic7xxx;
  first_aic7xxx = p;

  aic7xxx_allocate_scb(p);

  aic7xxx_loadseq(p);

  aic_outb(p, aic_inb(p, SBLKCTL) & ~AUTOFLUSHDIS, SBLKCTL);

  if ( (p->chip & AHC_CHIPID_MASK) == AHC_AIC7770 )
  {
    aic_outb(p, ENABLE, BCTL);  
  }

  if ( !(aic7xxx_no_reset) )
  {
    if (p->features & AHC_TWIN)
    {
      if (aic7xxx_verbose & VERBOSE_PROBE2)
        printk(KERN_INFO "(scsi%d) Resetting channel B\n", p->host_no);
      aic_outb(p, aic_inb(p, SBLKCTL) | SELBUSB, SBLKCTL);
      aic7xxx_reset_current_bus(p);
      aic_outb(p, aic_inb(p, SBLKCTL) & ~SELBUSB, SBLKCTL);
    }
    
    if (aic7xxx_verbose & VERBOSE_PROBE2)
    {  
      char *channel = "";
      if (p->flags & AHC_MULTI_CHANNEL)
      {
        channel = " A";
        if (p->flags & (AHC_CHNLB|AHC_CHNLC))
          channel = (p->flags & AHC_CHNLB) ? " B" : " C";
      }
      printk(KERN_INFO "(scsi%d) Resetting channel%s\n", p->host_no, channel);
    }
    
    aic7xxx_reset_current_bus(p);

  }
  else
  {
    if (!reset_delay)
    {
      printk(KERN_INFO "(scsi%d) Not resetting SCSI bus.  Note: Don't use "
             "the no_reset\n", p->host_no);
      printk(KERN_INFO "(scsi%d) option unless you have a verifiable need "
             "for it.\n", p->host_no);
    }
  }
  
  if (!(p->chip & AHC_PCI))
  {
    result = (request_irq(p->irq, do_aic7xxx_isr, 0, "aic7xxx", p));
  }
  else
  {
    result = (request_irq(p->irq, do_aic7xxx_isr, IRQF_SHARED,
              "aic7xxx", p));
    if (result < 0)
    {
      result = (request_irq(p->irq, do_aic7xxx_isr, IRQF_DISABLED | IRQF_SHARED,
              "aic7xxx", p));
    }
  }
  if (result < 0)
  {
    printk(KERN_WARNING "(scsi%d) Couldn't register IRQ %d, ignoring "
           "controller.\n", p->host_no, p->irq);
    aic_outb(p, 0, SIMODE1);
    p->irq = 0;
    return (0);
  }

  if(aic_inb(p, INTSTAT) & INT_PEND)
    printk(INFO_LEAD "spurious interrupt during configuration, cleared.\n",
      p->host_no, -1, -1 , -1);
  aic7xxx_clear_intstat(p);

  unpause_sequencer(p,  TRUE);

  return (found);
}

static int
aic7xxx_chip_reset(struct aic7xxx_host *p)
{
  unsigned char sblkctl;
  int wait;

  aic_outb(p, PAUSE | CHIPRST, HCNTRL);

  wait = 1000;  
  while (--wait && !(aic_inb(p, HCNTRL) & CHIPRSTACK))
  {
    udelay(1);  
  }

  pause_sequencer(p);

  sblkctl = aic_inb(p, SBLKCTL) & (SELBUSB|SELWIDE);
  if (p->chip & AHC_PCI)
    sblkctl &= ~SELBUSB;
  switch( sblkctl )
  {
    case 0:  
      break;
    case 2:  
      p->features |= AHC_WIDE;
      break;
    case 8:  
      p->features |= AHC_TWIN;
      p->flags |= AHC_MULTI_CHANNEL;
      break;
    default: 
      printk(KERN_WARNING "aic7xxx: Unsupported adapter type %d, ignoring.\n",
        aic_inb(p, SBLKCTL) & 0x0a);
      return(-1);
  }
  return(0);
}

static struct aic7xxx_host *
aic7xxx_alloc(struct scsi_host_template *sht, struct aic7xxx_host *temp)
{
  struct aic7xxx_host *p = NULL;
  struct Scsi_Host *host;

  host = scsi_register(sht, sizeof(struct aic7xxx_host));

  if (host != NULL)
  {
    p = (struct aic7xxx_host *) host->hostdata;
    memset(p, 0, sizeof(struct aic7xxx_host));
    *p = *temp;
    p->host = host;

    p->scb_data = kzalloc(sizeof(scb_data_type), GFP_ATOMIC);
    if (p->scb_data)
    {
      scbq_init (&p->scb_data->free_scbs);
    }
    else
    {
      release_region(p->base, MAXREG - MINREG);
      scsi_unregister(host);
      return(NULL);
    }
    p->host_no = host->host_no;
  }
  return (p);
}

static void
aic7xxx_free(struct aic7xxx_host *p)
{
  int i;

  if (p->scb_data != NULL)
  {
    struct aic7xxx_scb_dma *scb_dma = NULL;
    if (p->scb_data->hscbs != NULL)
    {
      pci_free_consistent(p->pdev, p->scb_data->hscbs_dma_len,
			  p->scb_data->hscbs, p->scb_data->hscbs_dma);
      p->scb_data->hscbs = p->scb_data->hscb_kmalloc_ptr = NULL;
    }
    for (i = 0; i < p->scb_data->numscbs; i++)
    {
      if (p->scb_data->scb_array[i]->scb_dma != scb_dma)
      {
	scb_dma = p->scb_data->scb_array[i]->scb_dma;
	pci_free_consistent(p->pdev, scb_dma->dma_len,
			    (void *)((unsigned long)scb_dma->dma_address
                                     - scb_dma->dma_offset),
			    scb_dma->dma_address);
      }
      kfree(p->scb_data->scb_array[i]->kmalloc_ptr);
      p->scb_data->scb_array[i] = NULL;
    }
  
    kfree(p->scb_data);
  }

  pci_free_consistent(p->pdev, 3*256, (void *)p->untagged_scbs, p->fifo_dma);
}

static void
aic7xxx_load_seeprom(struct aic7xxx_host *p, unsigned char *sxfrctl1)
{
  int have_seeprom = 0;
  int i, max_targets, mask;
  unsigned char scsirate, scsi_conf;
  unsigned short scarray[128];
  struct seeprom_config *sc = (struct seeprom_config *) scarray;

  if (aic7xxx_verbose & VERBOSE_PROBE2)
  {
    printk(KERN_INFO "aic7xxx: Loading serial EEPROM...");
  }
  switch (p->chip)
  {
    case (AHC_AIC7770|AHC_EISA):  
      if (aic_inb(p, SCSICONF) & TERM_ENB)
        p->flags |= AHC_TERM_ENB_A;
      if ( (p->features & AHC_TWIN) && (aic_inb(p, SCSICONF + 1) & TERM_ENB) )
        p->flags |= AHC_TERM_ENB_B;
      break;

    case (AHC_AIC7770|AHC_VL):
      have_seeprom = read_284x_seeprom(p, (struct seeprom_config *) scarray);
      break;

    default:
      have_seeprom = read_seeprom(p, (p->flags & (AHC_CHNLB|AHC_CHNLC)),
                                  scarray, p->sc_size, p->sc_type);
      if (!have_seeprom)
      {
        if(p->sc_type == C46)
          have_seeprom = read_seeprom(p, (p->flags & (AHC_CHNLB|AHC_CHNLC)),
                                      scarray, p->sc_size, C56_66);
        else
          have_seeprom = read_seeprom(p, (p->flags & (AHC_CHNLB|AHC_CHNLC)),
                                      scarray, p->sc_size, C46);
      }
      if (!have_seeprom)
      {
        p->sc_size = 128;
        have_seeprom = read_seeprom(p, 4*(p->flags & (AHC_CHNLB|AHC_CHNLC)),
                                    scarray, p->sc_size, p->sc_type);
        if (!have_seeprom)
        {
          if(p->sc_type == C46)
            have_seeprom = read_seeprom(p, 4*(p->flags & (AHC_CHNLB|AHC_CHNLC)),
                                        scarray, p->sc_size, C56_66);
          else
            have_seeprom = read_seeprom(p, 4*(p->flags & (AHC_CHNLB|AHC_CHNLC)),
                                        scarray, p->sc_size, C46);
        }
      }
      break;
  }

  if (!have_seeprom)
  {
    if (aic7xxx_verbose & VERBOSE_PROBE2)
    {
      printk("\naic7xxx: No SEEPROM available.\n");
    }
    p->flags |= AHC_NEWEEPROM_FMT;
    if (aic_inb(p, SCSISEQ) == 0)
    {
      p->flags |= AHC_USEDEFAULTS;
      p->flags &= ~AHC_BIOS_ENABLED;
      p->scsi_id = p->scsi_id_b = 7;
      *sxfrctl1 |= STPWEN;
      if (aic7xxx_verbose & VERBOSE_PROBE2)
      {
        printk("aic7xxx: Using default values.\n");
      }
    }
    else if (aic7xxx_verbose & VERBOSE_PROBE2)
    {
      printk("aic7xxx: Using leftover BIOS values.\n");
    }
    if ( ((p->chip & ~AHC_CHIPID_MASK) == AHC_PCI) && (*sxfrctl1 & STPWEN) )
    {
      p->flags |= AHC_TERM_ENB_SE_LOW | AHC_TERM_ENB_SE_HIGH;
      sc->adapter_control &= ~CFAUTOTERM;
      sc->adapter_control |= CFSTERM | CFWSTERM | CFLVDSTERM;
    }
    if (aic7xxx_extended)
      p->flags |= (AHC_EXTEND_TRANS_A | AHC_EXTEND_TRANS_B);
    else
      p->flags &= ~(AHC_EXTEND_TRANS_A | AHC_EXTEND_TRANS_B);
  }
  else
  {
    if (aic7xxx_verbose & VERBOSE_PROBE2)
    {
      printk("done\n");
    }

    p->flags |= AHC_SEEPROM_FOUND;

    *sxfrctl1 = 0;

    p->scsi_id = (sc->brtime_id & CFSCSIID);

    if ((p->chip & AHC_CHIPID_MASK) == AHC_AIC7770)
    {
      
      if (sc->bios_control & CF284XEXTEND)
        p->flags |= AHC_EXTEND_TRANS_A;

      if (sc->adapter_control & CF284XSTERM)
      {
        *sxfrctl1 |= STPWEN;
        p->flags |= AHC_TERM_ENB_SE_LOW | AHC_TERM_ENB_SE_HIGH;
      }
    }
    else
    {
      
      if (sc->bios_control & CFEXTEND)
        p->flags |= AHC_EXTEND_TRANS_A;
      if (sc->bios_control & CFBIOSEN)
        p->flags |= AHC_BIOS_ENABLED;
      else
        p->flags &= ~AHC_BIOS_ENABLED;

      if (sc->adapter_control & CFSTERM)
      {
        *sxfrctl1 |= STPWEN;
        p->flags |= AHC_TERM_ENB_SE_LOW | AHC_TERM_ENB_SE_HIGH;
      }
    }
    memcpy(&p->sc, sc, sizeof(struct seeprom_config));
  }

  p->discenable = 0;

  max_targets = ((p->features & (AHC_TWIN | AHC_WIDE)) ? 16 : 8);

  if (have_seeprom)
  {
    for (i = 0; i < max_targets; i++)
    {
      if( ((p->features & AHC_ULTRA) &&
          !(sc->adapter_control & CFULTRAEN) &&
           (sc->device_flags[i] & CFSYNCHISULTRA)) ||
          (sc->device_flags[i] & CFNEWULTRAFORMAT) )
      {
        p->flags |= AHC_NEWEEPROM_FMT;
        break;
      }
    }
  }

  for (i = 0; i < max_targets; i++)
  {
    mask = (0x01 << i);
    if (!have_seeprom)
    {
      if (aic_inb(p, SCSISEQ) != 0)
      {
	p->discenable =
	  ~(aic_inb(p, DISC_DSB) | (aic_inb(p, DISC_DSB + 1) << 8) );
        p->ultraenb =
          (aic_inb(p, ULTRA_ENB) | (aic_inb(p, ULTRA_ENB + 1) << 8) );
	sc->device_flags[i] = (p->discenable & mask) ? CFDISC : 0;
        if (aic_inb(p, TARG_SCSIRATE + i) & WIDEXFER)
          sc->device_flags[i] |= CFWIDEB;
        if (p->features & AHC_ULTRA2)
        {
          if (aic_inb(p, TARG_OFFSET + i))
          {
            sc->device_flags[i] |= CFSYNCH;
            sc->device_flags[i] |= (aic_inb(p, TARG_SCSIRATE + i) & 0x07);
            if ( (aic_inb(p, TARG_SCSIRATE + i) & 0x18) == 0x18 )
              sc->device_flags[i] |= CFSYNCHISULTRA;
          }
        }
        else
        {
          if (aic_inb(p, TARG_SCSIRATE + i) & ~WIDEXFER)
          {
            sc->device_flags[i] |= CFSYNCH;
            if (p->features & AHC_ULTRA)
              sc->device_flags[i] |= ((p->ultraenb & mask) ?
                                      CFSYNCHISULTRA : 0);
          }
        }
      }
      else
      {
        sc->device_flags[i] = CFDISC;
        if (p->features & AHC_WIDE)
          sc->device_flags[i] |= CFWIDEB;
        if (p->features & AHC_ULTRA3)
          sc->device_flags[i] |= 2;
        else if (p->features & AHC_ULTRA2)
          sc->device_flags[i] |= 3;
        else if (p->features & AHC_ULTRA)
          sc->device_flags[i] |= CFSYNCHISULTRA;
        sc->device_flags[i] |= CFSYNCH;
        aic_outb(p, 0, TARG_SCSIRATE + i);
        if (p->features & AHC_ULTRA2)
          aic_outb(p, 0, TARG_OFFSET + i);
      }
    }
    if (sc->device_flags[i] & CFDISC)
    {
      p->discenable |= mask;
    }
    if (p->flags & AHC_NEWEEPROM_FMT)
    {
      if ( !(p->features & AHC_ULTRA2) )
      {
        if ( (sc->device_flags[i] & CFNEWULTRAFORMAT) &&
            ((sc->device_flags[i] & CFXFER) == 0x03) )
        {
          sc->device_flags[i] &= ~CFXFER;
          sc->device_flags[i] |= CFSYNCHISULTRA;
        }
        if (sc->device_flags[i] & CFSYNCHISULTRA)
        {
          p->ultraenb |= mask;
        }
      }
      else if ( !(sc->device_flags[i] & CFNEWULTRAFORMAT) &&
                 (p->features & AHC_ULTRA2) &&
		 (sc->device_flags[i] & CFSYNCHISULTRA) )
      {
        p->ultraenb |= mask;
      }
    }
    else if (sc->adapter_control & CFULTRAEN)
    {
      p->ultraenb |= mask;
    }
    if ( (sc->device_flags[i] & CFSYNCH) == 0)
    {
      sc->device_flags[i] &= ~CFXFER;
      p->ultraenb &= ~mask;
      p->user[i].offset = 0;
      p->user[i].period = 0;
      p->user[i].options = 0;
    }
    else
    {
      if (p->features & AHC_ULTRA3)
      {
        p->user[i].offset = MAX_OFFSET_ULTRA2;
        if( (sc->device_flags[i] & CFXFER) < 0x03 )
        {
          scsirate = (sc->device_flags[i] & CFXFER);
          p->user[i].options = MSG_EXT_PPR_OPTION_DT_CRC;
        }
        else
        {
          scsirate = (sc->device_flags[i] & CFXFER) |
                     ((p->ultraenb & mask) ? 0x18 : 0x10);
          p->user[i].options = 0;
        }
        p->user[i].period = aic7xxx_find_period(p, scsirate,
                                       AHC_SYNCRATE_ULTRA3);
      }
      else if (p->features & AHC_ULTRA2)
      {
        p->user[i].offset = MAX_OFFSET_ULTRA2;
        scsirate = (sc->device_flags[i] & CFXFER) |
                   ((p->ultraenb & mask) ? 0x18 : 0x10);
        p->user[i].options = 0;
        p->user[i].period = aic7xxx_find_period(p, scsirate,
                                       AHC_SYNCRATE_ULTRA2);
      }
      else
      {
        scsirate = (sc->device_flags[i] & CFXFER) << 4;
        p->user[i].options = 0;
        p->user[i].offset = MAX_OFFSET_8BIT;
        if (p->features & AHC_ULTRA)
        {
          short ultraenb;
          ultraenb = aic_inb(p, ULTRA_ENB) |
            (aic_inb(p, ULTRA_ENB + 1) << 8);
          p->user[i].period = aic7xxx_find_period(p, scsirate,
                                          (p->ultraenb & mask) ?
                                          AHC_SYNCRATE_ULTRA :
                                          AHC_SYNCRATE_FAST);
        }
        else
          p->user[i].period = aic7xxx_find_period(p, scsirate,
			  		  AHC_SYNCRATE_FAST);
      }
    }
    if ( (sc->device_flags[i] & CFWIDEB) && (p->features & AHC_WIDE) )
    {
      p->user[i].width = MSG_EXT_WDTR_BUS_16_BIT;
    }
    else
    {
      p->user[i].width = MSG_EXT_WDTR_BUS_8_BIT;
    }
  }
  aic_outb(p, ~(p->discenable & 0xFF), DISC_DSB);
  aic_outb(p, ~((p->discenable >> 8) & 0xFF), DISC_DSB + 1);

  if (p->features & AHC_ULTRA)
    p->ultraenb = aic_inb(p, ULTRA_ENB) | (aic_inb(p, ULTRA_ENB + 1) << 8);

  
  scsi_conf = (p->scsi_id & HSCSIID);

  if(have_seeprom)
  {
    p->adapter_control = sc->adapter_control;
    p->bios_control = sc->bios_control;

    switch (p->chip & AHC_CHIPID_MASK)
    {
      case AHC_AIC7895:
      case AHC_AIC7896:
      case AHC_AIC7899:
        if (p->adapter_control & CFBPRIMARY)
          p->flags |= AHC_CHANNEL_B_PRIMARY;
      default:
        break;
    }

    if (sc->adapter_control & CFSPARITY)
      scsi_conf |= ENSPCHK;
  }
  else
  {
    scsi_conf |= ENSPCHK | RESET_SCSI;
  }

  if ( (p->chip & ~AHC_CHIPID_MASK) == AHC_PCI )
  {
    
    aic_outb(p, scsi_conf, SCSICONF);
    
    aic_outb(p, p->scsi_id, SCSICONF + 1);
  }
}

static void
aic7xxx_configure_bugs(struct aic7xxx_host *p)
{
  unsigned short tmp_word;
 
  switch(p->chip & AHC_CHIPID_MASK)
  {
    case AHC_AIC7860:
      p->bugs |= AHC_BUG_PCI_2_1_RETRY;
      
    case AHC_AIC7850:
    case AHC_AIC7870:
      p->bugs |= AHC_BUG_TMODE_WIDEODD | AHC_BUG_CACHETHEN | AHC_BUG_PCI_MWI;
      break;
    case AHC_AIC7880:
      p->bugs |= AHC_BUG_TMODE_WIDEODD | AHC_BUG_PCI_2_1_RETRY |
                 AHC_BUG_CACHETHEN | AHC_BUG_PCI_MWI;
      break;
    case AHC_AIC7890:
      p->bugs |= AHC_BUG_AUTOFLUSH | AHC_BUG_CACHETHEN;
      break;
    case AHC_AIC7892:
      p->bugs |= AHC_BUG_SCBCHAN_UPLOAD;
      break;
    case AHC_AIC7895:
      p->bugs |= AHC_BUG_TMODE_WIDEODD | AHC_BUG_PCI_2_1_RETRY |
                 AHC_BUG_CACHETHEN | AHC_BUG_PCI_MWI;
      break;
    case AHC_AIC7896:
      p->bugs |= AHC_BUG_CACHETHEN_DIS;
      break;
    case AHC_AIC7899:
      p->bugs |= AHC_BUG_SCBCHAN_UPLOAD;
      break;
    default:
      
      break;
  }

  pci_read_config_word(p->pdev, PCI_COMMAND, &tmp_word);
  if(p->bugs & AHC_BUG_PCI_MWI)
  {
    tmp_word &= ~PCI_COMMAND_INVALIDATE;
  }
  else
  {
    tmp_word |= PCI_COMMAND_INVALIDATE;
  }
  pci_write_config_word(p->pdev, PCI_COMMAND, tmp_word);

  if(p->bugs & AHC_BUG_CACHETHEN)
  {
    aic_outb(p, aic_inb(p, DSCOMMAND0) & ~CACHETHEN, DSCOMMAND0);
  }
  else if (p->bugs & AHC_BUG_CACHETHEN_DIS)
  {
    aic_outb(p, aic_inb(p, DSCOMMAND0) | CACHETHEN, DSCOMMAND0);
  }

  return;
}


static int
aic7xxx_detect(struct scsi_host_template *template)
{
  struct aic7xxx_host *temp_p = NULL;
  struct aic7xxx_host *current_p = NULL;
  struct aic7xxx_host *list_p = NULL;
  int found = 0;
#if defined(__i386__) || defined(__alpha__)
  ahc_flag_type flags = 0;
  int type;
#endif
  unsigned char sxfrctl1;
#if defined(__i386__) || defined(__alpha__)
  unsigned char hcntrl, hostconf;
  unsigned int slot, base;
#endif

#ifdef MODULE
  if(aic7xxx)
    aic7xxx_setup(aic7xxx);
#endif

  template->proc_name = "aic7xxx";
  template->sg_tablesize = AIC7XXX_MAX_SG;


#ifdef CONFIG_PCI
  {
    static struct
    {
      unsigned short      vendor_id;
      unsigned short      device_id;
      ahc_chip            chip;
      ahc_flag_type       flags;
      ahc_feature         features;
      int                 board_name_index;
      unsigned short      seeprom_size;
      unsigned short      seeprom_type;
    } const aic_pdevs[] = {
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7810, AHC_NONE,
       AHC_FNONE, AHC_FENONE,                                1,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7850, AHC_AIC7850,
       AHC_PAGESCBS, AHC_AIC7850_FE,                         5,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7855, AHC_AIC7850,
       AHC_PAGESCBS, AHC_AIC7850_FE,                         6,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7821, AHC_AIC7860,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7860_FE,                                       7,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_3860, AHC_AIC7860,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7860_FE,                                       7,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_38602, AHC_AIC7860,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7860_FE,                                       7,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_38602, AHC_AIC7860,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7860_FE,                                       7,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7860, AHC_AIC7860,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MOTHERBOARD,
       AHC_AIC7860_FE,                                       7,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7861, AHC_AIC7860,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7860_FE,                                       8,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7870, AHC_AIC7870,
       AHC_PAGESCBS | AHC_BIOS_ENABLED | AHC_MOTHERBOARD,
       AHC_AIC7870_FE,                                       9,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7871, AHC_AIC7870,
       AHC_PAGESCBS | AHC_BIOS_ENABLED, AHC_AIC7870_FE,     10,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7872, AHC_AIC7870,
       AHC_PAGESCBS | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7870_FE,                                      11,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7873, AHC_AIC7870,
       AHC_PAGESCBS | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7870_FE,                                      12,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7874, AHC_AIC7870,
       AHC_PAGESCBS | AHC_BIOS_ENABLED, AHC_AIC7870_FE,     13,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7880, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED | AHC_MOTHERBOARD,
       AHC_AIC7880_FE,                                      14,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7881, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED, AHC_AIC7880_FE,     15,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7882, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7880_FE,                                      16,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7883, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7880_FE,                                      17,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7884, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED, AHC_AIC7880_FE,     18,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7885, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED, AHC_AIC7880_FE,     18,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7886, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED, AHC_AIC7880_FE,     18,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7887, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED, AHC_AIC7880_FE | AHC_NEW_AUTOTERM, 19,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7888, AHC_AIC7880,
       AHC_PAGESCBS | AHC_BIOS_ENABLED, AHC_AIC7880_FE,     18,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_7895, AHC_AIC7895,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7895_FE,                                      20,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7890, AHC_AIC7890,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7890_FE,                                      21,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7890B, AHC_AIC7890,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7890_FE,                                      21,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_2930U2, AHC_AIC7890,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7890_FE,                                      22,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_2940U2, AHC_AIC7890,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7890_FE,                                      23,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7896, AHC_AIC7896,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7896_FE,                                      24,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_3940U2, AHC_AIC7896,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7896_FE,                                      25,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_3950U2D, AHC_AIC7896,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7896_FE,                                      26,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC, PCI_DEVICE_ID_ADAPTEC_1480A, AHC_AIC7860,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_NO_STPWEN,
       AHC_AIC7860_FE,                                      27,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7892A, AHC_AIC7892,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7892_FE,                                      28,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7892B, AHC_AIC7892,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7892_FE,                                      28,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7892D, AHC_AIC7892,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7892_FE,                                      28,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7892P, AHC_AIC7892,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED,
       AHC_AIC7892_FE,                                      28,
       32, C46 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7899A, AHC_AIC7899,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7899_FE,                                      29,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7899B, AHC_AIC7899,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7899_FE,                                      29,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7899D, AHC_AIC7899,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7899_FE,                                      29,
       32, C56_66 },
      {PCI_VENDOR_ID_ADAPTEC2, PCI_DEVICE_ID_ADAPTEC2_7899P, AHC_AIC7899,
       AHC_PAGESCBS | AHC_NEWEEPROM_FMT | AHC_BIOS_ENABLED | AHC_MULTI_CHANNEL,
       AHC_AIC7899_FE,                                      29,
       32, C56_66 },
    };

    unsigned short command;
    unsigned int  devconfig, i, oldverbose;
    struct pci_dev *pdev = NULL;

    for (i = 0; i < ARRAY_SIZE(aic_pdevs); i++)
    {
      pdev = NULL;
      while ((pdev = pci_get_device(aic_pdevs[i].vendor_id,
                                     aic_pdevs[i].device_id,
                                     pdev))) {
	if (pci_enable_device(pdev))
		continue;
        if ( i == 0 ) 
        {
          if (aic7xxx_verbose & (VERBOSE_PROBE|VERBOSE_PROBE2))
          {
            printk(KERN_INFO "aic7xxx: The 7810 RAID controller is not "
              "supported by\n");
            printk(KERN_INFO "         this driver, we are ignoring it.\n");
          }
        }
        else if ( (temp_p = kzalloc(sizeof(struct aic7xxx_host),
                                    GFP_ATOMIC)) != NULL )
        {
          temp_p->chip = aic_pdevs[i].chip | AHC_PCI;
          temp_p->flags = aic_pdevs[i].flags;
          temp_p->features = aic_pdevs[i].features;
          temp_p->board_name_index = aic_pdevs[i].board_name_index;
          temp_p->sc_size = aic_pdevs[i].seeprom_size;
          temp_p->sc_type = aic_pdevs[i].seeprom_type;

          temp_p->irq = pdev->irq;
          temp_p->pdev = pdev;
          temp_p->pci_bus = pdev->bus->number;
          temp_p->pci_device_fn = pdev->devfn;
          temp_p->base = pci_resource_start(pdev, 0);
          temp_p->mbase = pci_resource_start(pdev, 1);
          current_p = list_p;
	  while(current_p && temp_p)
	  {
	    if ( ((current_p->pci_bus == temp_p->pci_bus) &&
	          (current_p->pci_device_fn == temp_p->pci_device_fn)) ||
                 (temp_p->base && (current_p->base == temp_p->base)) ||
                 (temp_p->mbase && (current_p->mbase == temp_p->mbase)) )
	    {
              
	      kfree(temp_p);
	      temp_p = NULL;
              continue;
	    }
	    current_p = current_p->next;
	  }
          if(pci_request_regions(temp_p->pdev, "aic7xxx"))
          {
            printk("aic7xxx: <%s> at PCI %d/%d/%d\n", 
              board_names[aic_pdevs[i].board_name_index],
              temp_p->pci_bus,
              PCI_SLOT(temp_p->pci_device_fn),
              PCI_FUNC(temp_p->pci_device_fn));
            printk("aic7xxx: I/O ports already in use, ignoring.\n");
            kfree(temp_p);
            continue;
          }

          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk("aic7xxx: <%s> at PCI %d/%d\n", 
              board_names[aic_pdevs[i].board_name_index],
              PCI_SLOT(pdev->devfn),
              PCI_FUNC(pdev->devfn));
          pci_read_config_word(pdev, PCI_COMMAND, &command);
          if (aic7xxx_verbose & VERBOSE_PROBE2)
          {
            printk("aic7xxx: Initial PCI_COMMAND value was 0x%x\n",
              (int)command);
          }
#ifdef AIC7XXX_STRICT_PCI_SETUP
          command |= PCI_COMMAND_SERR | PCI_COMMAND_PARITY |
            PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_IO;
#else
          command |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_IO;
#endif
          command &= ~PCI_COMMAND_INVALIDATE;
          if (aic7xxx_pci_parity == 0)
            command &= ~(PCI_COMMAND_SERR | PCI_COMMAND_PARITY);
          pci_write_config_word(pdev, PCI_COMMAND, command);
#ifdef AIC7XXX_STRICT_PCI_SETUP
          pci_read_config_dword(pdev, DEVCONFIG, &devconfig);
          if (aic7xxx_verbose & VERBOSE_PROBE2)
          {
            printk("aic7xxx: Initial DEVCONFIG value was 0x%x\n", devconfig);
          }
          devconfig |= 0x80000040;
          pci_write_config_dword(pdev, DEVCONFIG, devconfig);
#endif 

          temp_p->unpause = INTEN;
          temp_p->pause = temp_p->unpause | PAUSE;
          if ( ((temp_p->base == 0) &&
                (temp_p->mbase == 0)) ||
               (temp_p->irq == 0) )
          {
            printk("aic7xxx: <%s> at PCI %d/%d/%d\n", 
              board_names[aic_pdevs[i].board_name_index],
              temp_p->pci_bus,
              PCI_SLOT(temp_p->pci_device_fn),
              PCI_FUNC(temp_p->pci_device_fn));
            printk("aic7xxx: Controller disabled by BIOS, ignoring.\n");
            goto skip_pci_controller;
          }

#ifdef MMAPIO
          if ( !(temp_p->base) || !(temp_p->flags & AHC_MULTI_CHANNEL) ||
               ((temp_p->chip != (AHC_AIC7870 | AHC_PCI)) &&
                (temp_p->chip != (AHC_AIC7880 | AHC_PCI))) )
          {
            temp_p->maddr = ioremap_nocache(temp_p->mbase, 256);
            if(temp_p->maddr)
            {
              if(aic_inb(temp_p, HCNTRL) == 0xff)
              {
                printk(KERN_INFO "aic7xxx: <%s> at PCI %d/%d/%d\n", 
                  board_names[aic_pdevs[i].board_name_index],
                  temp_p->pci_bus,
                  PCI_SLOT(temp_p->pci_device_fn),
                  PCI_FUNC(temp_p->pci_device_fn));
                printk(KERN_INFO "aic7xxx: MMAPed I/O failed, reverting to "
                                 "Programmed I/O.\n");
                iounmap(temp_p->maddr);
                temp_p->maddr = NULL;
                if(temp_p->base == 0)
                {
                  printk("aic7xxx: <%s> at PCI %d/%d/%d\n", 
                    board_names[aic_pdevs[i].board_name_index],
                    temp_p->pci_bus,
                    PCI_SLOT(temp_p->pci_device_fn),
                    PCI_FUNC(temp_p->pci_device_fn));
                  printk("aic7xxx: Controller disabled by BIOS, ignoring.\n");
                  goto skip_pci_controller;
                }
              }
            }
          }
#endif

          pause_sequencer(temp_p);

          oldverbose = aic7xxx_verbose;
          aic7xxx_verbose = 0;
          aic7xxx_pci_intr(temp_p);
          aic7xxx_verbose = oldverbose;

          temp_p->bios_address = 0;

          if (temp_p->features & AHC_ULTRA2)
            temp_p->scsi_id = aic_inb(temp_p, SCSIID_ULTRA2) & OID;
          else
            temp_p->scsi_id = aic_inb(temp_p, SCSIID) & OID;
          sxfrctl1 = aic_inb(temp_p, SXFRCTL1);

          if (aic7xxx_chip_reset(temp_p) == -1)
          {
            goto skip_pci_controller;
          }
          aic_outb(temp_p, sxfrctl1, SXFRCTL1);
          pci_write_config_dword(temp_p->pdev, DEVCONFIG, devconfig);
          sxfrctl1 &= STPWEN;

          switch (temp_p->chip & AHC_CHIPID_MASK)
          {
            case AHC_AIC7870: 
            case AHC_AIC7880: 
              if(temp_p->flags & AHC_MULTI_CHANNEL)
              {
                switch(PCI_SLOT(temp_p->pci_device_fn))
                {
                  case 5:
                    temp_p->flags |= AHC_CHNLB;
                    break;
                  case 8:
                    temp_p->flags |= AHC_CHNLB;
                    break;
                  case 12:
                    temp_p->flags |= AHC_CHNLC;
                    break;
                  default:
                    break;
                }
              }
              break;

            case AHC_AIC7895: 
            case AHC_AIC7896: 
            case AHC_AIC7899: 
              if (PCI_FUNC(pdev->devfn) != 0)
              {
                temp_p->flags |= AHC_CHNLB;
              }
              if ((temp_p->chip & AHC_CHIPID_MASK) == AHC_AIC7895)
              {
                pci_read_config_dword(pdev, DEVCONFIG, &devconfig);
                devconfig |= SCBSIZE32;
                pci_write_config_dword(pdev, DEVCONFIG, devconfig);
              }
              break;
            default:
              break;
          }

          switch (temp_p->chip & AHC_CHIPID_MASK)
          {
            case AHC_AIC7892:
            case AHC_AIC7899:
              aic_outb(temp_p, 0, SCAMCTL);
              aic_outb(temp_p, aic_inb(temp_p, SFUNCT) | ALT_MODE, SFUNCT);
              aic_outb(temp_p, AUTO_MSGOUT_DE | DIS_MSGIN_DUALEDGE, OPTIONMODE);
	      aic_outb(temp_p, 0x00, 0x0b);
	      aic_outb(temp_p, 0x10, 0x0a);
              aic_outb(temp_p, aic_inb(temp_p, SFUNCT) & ~ALT_MODE, SFUNCT);
              aic_outb(temp_p, CRCVALCHKEN | CRCENDCHKEN | CRCREQCHKEN |
			       TARGCRCENDEN | TARGCRCCNTEN,
                       CRCCONTROL1);
              aic_outb(temp_p, ((aic_inb(temp_p, DSCOMMAND0) | USCBSIZE32 |
                                 MPARCKEN | CIOPARCKEN | CACHETHEN) & 
                               ~DPARCKEN), DSCOMMAND0);
              aic7xxx_load_seeprom(temp_p, &sxfrctl1);
              break;
            case AHC_AIC7890:
            case AHC_AIC7896:
              aic_outb(temp_p, 0, SCAMCTL);
              aic_outb(temp_p, (aic_inb(temp_p, DSCOMMAND0) |
                                CACHETHEN | MPARCKEN | USCBSIZE32 |
                                CIOPARCKEN) & ~DPARCKEN, DSCOMMAND0);
              aic7xxx_load_seeprom(temp_p, &sxfrctl1);
              break;
            case AHC_AIC7850:
            case AHC_AIC7860:
              aic_outb(temp_p, (aic_inb(temp_p, DSCOMMAND0) |
                                CACHETHEN | MPARCKEN) & ~DPARCKEN,
                       DSCOMMAND0);
              
            default:
              aic7xxx_load_seeprom(temp_p, &sxfrctl1);
              break;
            case AHC_AIC7880:
              pci_read_config_dword(pdev, DEVCONFIG, &devconfig);
              if ((devconfig & 0xff) >= 1)
              {
                aic_outb(temp_p, (aic_inb(temp_p, DSCOMMAND0) |
                                  CACHETHEN | MPARCKEN) & ~DPARCKEN,
                         DSCOMMAND0);
              }
              aic7xxx_load_seeprom(temp_p, &sxfrctl1);
              break;
          }
          

          switch(temp_p->chip & AHC_CHIPID_MASK)
          {
            case AHC_AIC7895:
            case AHC_AIC7896:
            case AHC_AIC7899:
              current_p = list_p;
              while(current_p != NULL)
              {
                if ( (current_p->pci_bus == temp_p->pci_bus) &&
                     (PCI_SLOT(current_p->pci_device_fn) ==
                      PCI_SLOT(temp_p->pci_device_fn)) )
                {
                  if ( PCI_FUNC(current_p->pci_device_fn) == 0 )
                  {
                    temp_p->flags |= 
                      (current_p->flags & AHC_CHANNEL_B_PRIMARY);
                    temp_p->flags &= ~(AHC_BIOS_ENABLED|AHC_USEDEFAULTS);
                    temp_p->flags |=
                      (current_p->flags & (AHC_BIOS_ENABLED|AHC_USEDEFAULTS));
                  }
                  else
                  {
                    current_p->flags |=
                      (temp_p->flags & AHC_CHANNEL_B_PRIMARY);
                    current_p->flags &= ~(AHC_BIOS_ENABLED|AHC_USEDEFAULTS);
                    current_p->flags |=
                      (temp_p->flags & (AHC_BIOS_ENABLED|AHC_USEDEFAULTS));
                  }
                }
                current_p = current_p->next;
              }
              break;
            default:
              break;
          }

          switch(temp_p->chip & AHC_CHIPID_MASK)
          {
            default:
              break;
            case AHC_AIC7895:
            case AHC_AIC7896:
            case AHC_AIC7899:
              pci_read_config_dword(pdev, DEVCONFIG, &devconfig);
              if (temp_p->features & AHC_ULTRA2)
              {
                if ( (aic_inb(temp_p, DSCOMMAND0) & RAMPSM_ULTRA2) &&
                     (aic7xxx_scbram) )
                {
                  aic_outb(temp_p,
                           aic_inb(temp_p, DSCOMMAND0) & ~SCBRAMSEL_ULTRA2,
                           DSCOMMAND0);
                  temp_p->flags |= AHC_EXTERNAL_SRAM;
                  devconfig |= EXTSCBPEN;
                }
                else if (aic_inb(temp_p, DSCOMMAND0) & RAMPSM_ULTRA2)
                {
                  printk(KERN_INFO "aic7xxx: <%s> at PCI %d/%d/%d\n", 
                    board_names[aic_pdevs[i].board_name_index],
                    temp_p->pci_bus,
                    PCI_SLOT(temp_p->pci_device_fn),
                    PCI_FUNC(temp_p->pci_device_fn));
                  printk("aic7xxx: external SCB RAM detected, "
                         "but not enabled\n");
                }
              }
              else
              {
                if ((devconfig & RAMPSM) && (aic7xxx_scbram))
                {
                  devconfig &= ~SCBRAMSEL;
                  devconfig |= EXTSCBPEN;
                  temp_p->flags |= AHC_EXTERNAL_SRAM;
                }
                else if (devconfig & RAMPSM)
                {
                  printk(KERN_INFO "aic7xxx: <%s> at PCI %d/%d/%d\n", 
                    board_names[aic_pdevs[i].board_name_index],
                    temp_p->pci_bus,
                    PCI_SLOT(temp_p->pci_device_fn),
                    PCI_FUNC(temp_p->pci_device_fn));
                  printk("aic7xxx: external SCB RAM detected, "
                         "but not enabled\n");
                }
              }
              pci_write_config_dword(pdev, DEVCONFIG, devconfig);
              if ( (temp_p->flags & AHC_EXTERNAL_SRAM) &&
                   (temp_p->flags & AHC_CHNLB) )
                aic_outb(temp_p, 1, CCSCBBADDR);
              break;
          }

          aic_outb(temp_p, 
            (aic_inb(temp_p, SBLKCTL) & ~(DIAGLEDEN | DIAGLEDON)),
            SBLKCTL);

          if (temp_p->features & AHC_ULTRA2)
          {
            aic_outb(temp_p, RD_DFTHRSH_MAX | WR_DFTHRSH_MAX, DFF_THRSH);
          }
          else
          {
            aic_outb(temp_p, DFTHRSH_100, DSPCISTATUS);
          }

          aic7xxx_configure_bugs(temp_p);

          
          pci_dev_get(temp_p->pdev);

          if ( list_p == NULL )
          {
            list_p = current_p = temp_p;
          }
          else
          {
            current_p = list_p;
            while(current_p->next != NULL)
              current_p = current_p->next;
            current_p->next = temp_p;
          }
          temp_p->next = NULL;
          found++;
	  continue;
skip_pci_controller:
#ifdef CONFIG_PCI
	  pci_release_regions(temp_p->pdev);
#endif
	  kfree(temp_p);
        }  
        else 
        {
          printk("aic7xxx: Found <%s>\n", 
            board_names[aic_pdevs[i].board_name_index]);
          printk(KERN_INFO "aic7xxx: Unable to allocate device memory, "
            "skipping.\n");
        }
      } 
    } 
  }
#endif 

#if defined(__i386__) || defined(__alpha__)
  slot = MINSLOT;
  while ( (slot <= MAXSLOT) &&
         !(aic7xxx_no_probe) )
  {
    base = SLOTBASE(slot) + MINREG;

    if (!request_region(base, MAXREG - MINREG, "aic7xxx"))
    {
      slot++;
      continue; 
    }
    flags = 0;
    type = aic7xxx_probe(slot, base + AHC_HID0, &flags);
    if (type == -1)
    {
      release_region(base, MAXREG - MINREG);
      slot++;
      continue;
    }
    temp_p = kmalloc(sizeof(struct aic7xxx_host), GFP_ATOMIC);
    if (temp_p == NULL)
    {
      printk(KERN_WARNING "aic7xxx: Unable to allocate device space.\n");
      release_region(base, MAXREG - MINREG);
      slot++;
      continue; 
    }

    if (aic7xxx_irq_trigger == 1)
      hcntrl = IRQMS;  
    else if (aic7xxx_irq_trigger == 0)
      hcntrl = 0;  
    else
      hcntrl = inb(base + HCNTRL) & IRQMS;  
    memset(temp_p, 0, sizeof(struct aic7xxx_host));
    temp_p->unpause = hcntrl | INTEN;
    temp_p->pause = hcntrl | PAUSE | INTEN;
    temp_p->base = base;
    temp_p->mbase = 0;
    temp_p->maddr = NULL;
    temp_p->pci_bus = 0;
    temp_p->pci_device_fn = slot;
    aic_outb(temp_p, hcntrl | PAUSE, HCNTRL);
    while( (aic_inb(temp_p, HCNTRL) & PAUSE) == 0 ) ;
    if (aic7xxx_chip_reset(temp_p) == -1)
      temp_p->irq = 0;
    else
      temp_p->irq = aic_inb(temp_p, INTDEF) & 0x0F;
    temp_p->flags |= AHC_PAGESCBS;

    switch (temp_p->irq)
    {
      case 9:
      case 10:
      case 11:
      case 12:
      case 14:
      case 15:
        break;

      default:
        printk(KERN_WARNING "aic7xxx: Host adapter uses unsupported IRQ "
          "level %d, ignoring.\n", temp_p->irq);
        kfree(temp_p);
        release_region(base, MAXREG - MINREG);
        slot++;
        continue; 
    }


    if (list_p == NULL)
    {
      list_p = current_p = temp_p;
    }
    else
    {
      current_p = list_p;
      while (current_p->next != NULL)
        current_p = current_p->next;
      current_p->next = temp_p;
    }

    switch (type)
    {
      case 0:
        temp_p->board_name_index = 2;
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk("aic7xxx: <%s> at EISA %d\n",
               board_names[2], slot);
        
      case 1:
      {
        temp_p->chip = AHC_AIC7770 | AHC_EISA;
        temp_p->features |= AHC_AIC7770_FE;
        temp_p->bios_control = aic_inb(temp_p, HA_274_BIOSCTRL);

        if (temp_p->board_name_index == 0)
        {
          temp_p->board_name_index = 3;
          if (aic7xxx_verbose & VERBOSE_PROBE2)
            printk("aic7xxx: <%s> at EISA %d\n",
                 board_names[3], slot);
        }
        if (temp_p->bios_control & CHANNEL_B_PRIMARY)
        {
          temp_p->flags |= AHC_CHANNEL_B_PRIMARY;
        }

        if ((temp_p->bios_control & BIOSMODE) == BIOSDISABLED)
        {
          temp_p->flags &= ~AHC_BIOS_ENABLED;
        }
        else
        {
          temp_p->flags &= ~AHC_USEDEFAULTS;
          temp_p->flags |= AHC_BIOS_ENABLED;
          if ( (temp_p->bios_control & 0x20) == 0 )
          {
            temp_p->bios_address = 0xcc000;
            temp_p->bios_address += (0x4000 * (temp_p->bios_control & 0x07));
          }
          else
          {
            temp_p->bios_address = 0xd0000;
            temp_p->bios_address += (0x8000 * (temp_p->bios_control & 0x06));
          }
        }
        temp_p->adapter_control = aic_inb(temp_p, SCSICONF) << 8;
        temp_p->adapter_control |= aic_inb(temp_p, SCSICONF + 1);
        if (temp_p->features & AHC_WIDE)
        {
          temp_p->scsi_id = temp_p->adapter_control & HWSCSIID;
          temp_p->scsi_id_b = temp_p->scsi_id;
        }
        else
        {
          temp_p->scsi_id = (temp_p->adapter_control >> 8) & HSCSIID;
          temp_p->scsi_id_b = temp_p->adapter_control & HSCSIID;
        }
        aic7xxx_load_seeprom(temp_p, &sxfrctl1);
        break;
      }

      case 2:
      case 3:
        temp_p->chip = AHC_AIC7770 | AHC_VL;
        temp_p->features |= AHC_AIC7770_FE;
        if (type == 2)
          temp_p->flags |= AHC_BIOS_ENABLED;
        else
          temp_p->flags &= ~AHC_BIOS_ENABLED;
        if (aic_inb(temp_p, SCSICONF) & TERM_ENB)
          sxfrctl1 = STPWEN;
        aic7xxx_load_seeprom(temp_p, &sxfrctl1);
        temp_p->board_name_index = 4;
        if (aic7xxx_verbose & VERBOSE_PROBE2)
          printk("aic7xxx: <%s> at VLB %d\n",
               board_names[2], slot);
        switch( aic_inb(temp_p, STATUS_2840) & BIOS_SEL )
        {
          case 0x00:
            temp_p->bios_address = 0xe0000;
            break;
          case 0x20:
            temp_p->bios_address = 0xc8000;
            break;
          case 0x40:
            temp_p->bios_address = 0xd0000;
            break;
          case 0x60:
            temp_p->bios_address = 0xd8000;
            break;
          default:
            break; 
        }
        break;

      default:  
        break;
    }
    if (aic7xxx_verbose & VERBOSE_PROBE2)
    {
      printk(KERN_INFO "aic7xxx: BIOS %sabled, IO Port 0x%lx, IRQ %d (%s)\n",
        (temp_p->flags & AHC_USEDEFAULTS) ? "dis" : "en", temp_p->base,
        temp_p->irq,
        (temp_p->pause & IRQMS) ? "level sensitive" : "edge triggered");
      printk(KERN_INFO "aic7xxx: Extended translation %sabled.\n",
             (temp_p->flags & AHC_EXTEND_TRANS_A) ? "en" : "dis");
    }

    temp_p->bugs |= AHC_BUG_TMODE_WIDEODD;

    hostconf = aic_inb(temp_p, HOSTCONF);
    aic_outb(temp_p, hostconf & DFTHRSH, BUSSPD);
    aic_outb(temp_p, (hostconf << 2) & BOFF, BUSTIME);
    slot++;
    found++;
  }

#endif 


  {
    struct aic7xxx_host *sort_list[4] = { NULL, NULL, NULL, NULL };
    struct aic7xxx_host *vlb, *pci;
    struct aic7xxx_host *prev_p;
    struct aic7xxx_host *p;
    unsigned char left;

    prev_p = vlb = pci = NULL;

    temp_p = list_p;
    while (temp_p != NULL)
    {
      switch(temp_p->chip & ~AHC_CHIPID_MASK)
      {
        case AHC_EISA:
        case AHC_VL:
        {
          p = temp_p;
          if (p->flags & AHC_BIOS_ENABLED)
            vlb = sort_list[0];
          else
            vlb = sort_list[2];

          if (vlb == NULL)
          {
            vlb = temp_p;
            temp_p = temp_p->next;
            vlb->next = NULL;
          }
          else
          {
            current_p = vlb;
            prev_p = NULL;
            while ( (current_p != NULL) &&
                    (current_p->bios_address < temp_p->bios_address))
            {
              prev_p = current_p;
              current_p = current_p->next;
            }
            if (prev_p != NULL)
            {
              prev_p->next = temp_p;
              temp_p = temp_p->next;
              prev_p->next->next = current_p;
            }
            else
            {
              vlb = temp_p;
              temp_p = temp_p->next;
              vlb->next = current_p;
            }
          }
          
          if (p->flags & AHC_BIOS_ENABLED)
            sort_list[0] = vlb;
          else
            sort_list[2] = vlb;
          
          break;
        }
        default:  
        {

          p = temp_p;
          if (p->flags & AHC_BIOS_ENABLED) 
            pci = sort_list[1];
          else
            pci = sort_list[3];

          if (pci == NULL)
          {
            pci = temp_p;
            temp_p = temp_p->next;
            pci->next = NULL;
          }
          else
          {
            current_p = pci;
            prev_p = NULL;
            if (!aic7xxx_reverse_scan)
            {
              while ( (current_p != NULL) &&
                      ( (PCI_SLOT(current_p->pci_device_fn) |
                        (current_p->pci_bus << 8)) < 
                        (PCI_SLOT(temp_p->pci_device_fn) |
                        (temp_p->pci_bus << 8)) ) )
              {
                prev_p = current_p;
                current_p = current_p->next;
              }
            }
            else
            {
              while ( (current_p != NULL) &&
                      ( (PCI_SLOT(current_p->pci_device_fn) |
                        (current_p->pci_bus << 8)) > 
                        (PCI_SLOT(temp_p->pci_device_fn) |
                        (temp_p->pci_bus << 8)) ) )
              {
                prev_p = current_p;
                current_p = current_p->next;
              }
            }
            if ( (current_p) && (temp_p->flags & AHC_MULTI_CHANNEL) &&
                 (temp_p->pci_bus == current_p->pci_bus) &&
                 (PCI_SLOT(temp_p->pci_device_fn) ==
                  PCI_SLOT(current_p->pci_device_fn)) )
            {
              if (temp_p->flags & AHC_CHNLB)
              {
                if ( !(temp_p->flags & AHC_CHANNEL_B_PRIMARY) )
                {
                  prev_p = current_p;
                  current_p = current_p->next;
                }
              }
              else
              {
                if (temp_p->flags & AHC_CHANNEL_B_PRIMARY)
                {
                  prev_p = current_p;
                  current_p = current_p->next;
                }
              }
            }
            if (prev_p != NULL)
            {
              prev_p->next = temp_p;
              temp_p = temp_p->next;
              prev_p->next->next = current_p;
            }
            else
            {
              pci = temp_p;
              temp_p = temp_p->next;
              pci->next = current_p;
            }
          }

          if (p->flags & AHC_BIOS_ENABLED)
            sort_list[1] = pci;
          else
            sort_list[3] = pci;

          break;
        }
      }  
    } 
    {
      int i;
      
      left = found;
      for (i=0; i<ARRAY_SIZE(sort_list); i++)
      {
        temp_p = sort_list[i];
        while(temp_p != NULL)
        {
          template->name = board_names[temp_p->board_name_index];
          p = aic7xxx_alloc(template, temp_p);
          if (p != NULL)
          {
            p->instance = found - left;
            if (aic7xxx_register(template, p, (--left)) == 0)
            {
              found--;
              aic7xxx_release(p->host);
              scsi_unregister(p->host);
            }
            else if (aic7xxx_dump_card)
            {
              pause_sequencer(p);
              aic7xxx_print_card(p);
              aic7xxx_print_scratch_ram(p);
              unpause_sequencer(p, TRUE);
            }
          }
          current_p = temp_p;
          temp_p = (struct aic7xxx_host *)temp_p->next;
          kfree(current_p);
        }
      }
    }
  }
  return (found);
}

static void aic7xxx_buildscb(struct aic7xxx_host *p, struct scsi_cmnd *cmd,
			     struct aic7xxx_scb *scb)
{
  unsigned short mask;
  struct aic7xxx_hwscb *hscb;
  struct aic_dev_data *aic_dev = cmd->device->hostdata;
  struct scsi_device *sdptr = cmd->device;
  unsigned char tindex = TARGET_INDEX(cmd);
  int use_sg;

  mask = (0x01 << tindex);
  hscb = scb->hscb;

  hscb->control = 0;
  scb->tag_action = 0;

  if (p->discenable & mask)
  {
    hscb->control |= DISCENB;
    
    if (cmd->cmnd[0] != TEST_UNIT_READY && sdptr->simple_tags)
    {
      hscb->control |= MSG_SIMPLE_Q_TAG;
      scb->tag_action = MSG_SIMPLE_Q_TAG;
    }
  }
  if ( !(aic_dev->dtr_pending) &&
        (aic_dev->needppr || aic_dev->needwdtr || aic_dev->needsdtr) &&
        (aic_dev->flags & DEVICE_DTR_SCANNED) )
  {
    aic_dev->dtr_pending = 1;
    scb->tag_action = 0;
    hscb->control &= DISCENB;
    hscb->control |= MK_MESSAGE;
    if(aic_dev->needppr)
    {
      scb->flags |= SCB_MSGOUT_PPR;
    }
    else if(aic_dev->needwdtr)
    {
      scb->flags |= SCB_MSGOUT_WDTR;
    }
    else if(aic_dev->needsdtr)
    {
      scb->flags |= SCB_MSGOUT_SDTR;
    }
    scb->flags |= SCB_DTR_SCB;
  }
  hscb->target_channel_lun = ((cmd->device->id << 4) & 0xF0) |
        ((cmd->device->channel & 0x01) << 3) | (cmd->device->lun & 0x07);


  hscb->SCSI_cmd_length = cmd->cmd_len;
  memcpy(scb->cmnd, cmd->cmnd, cmd->cmd_len);
  hscb->SCSI_cmd_pointer = cpu_to_le32(SCB_DMA_ADDR(scb, scb->cmnd));

  use_sg = scsi_dma_map(cmd);
  BUG_ON(use_sg < 0);

  if (use_sg) {
    struct scatterlist *sg;  

    int i;

    scb->sg_length = 0;


    scsi_for_each_sg(cmd, sg, use_sg, i) {
      unsigned int len = sg_dma_len(sg);
      scb->sg_list[i].address = cpu_to_le32(sg_dma_address(sg));
      scb->sg_list[i].length = cpu_to_le32(len);
      scb->sg_length += len;
    }
    
    hscb->data_pointer = scb->sg_list[0].address;
    hscb->data_count = scb->sg_list[0].length;
    scb->sg_count = i;
    hscb->SG_segment_count = i;
    hscb->SG_list_pointer = cpu_to_le32(SCB_DMA_ADDR(scb, &scb->sg_list[1]));
  } else {
      scb->sg_count = 0;
      scb->sg_length = 0;
      hscb->SG_segment_count = 0;
      hscb->SG_list_pointer = 0;
      hscb->data_count = 0;
      hscb->data_pointer = 0;
  }
}

static int aic7xxx_queue_lck(struct scsi_cmnd *cmd, void (*fn)(struct scsi_cmnd *))
{
  struct aic7xxx_host *p;
  struct aic7xxx_scb *scb;
  struct aic_dev_data *aic_dev;

  p = (struct aic7xxx_host *) cmd->device->host->hostdata;

  aic_dev = cmd->device->hostdata;  
#ifdef AIC7XXX_VERBOSE_DEBUGGING
  if (aic_dev->active_cmds > aic_dev->max_q_depth)
  {
    printk(WARN_LEAD "Commands queued exceeds queue "
           "depth, active=%d\n",
           p->host_no, CTL_OF_CMD(cmd), 
           aic_dev->active_cmds);
  }
#endif

  scb = scbq_remove_head(&p->scb_data->free_scbs);
  if (scb == NULL)
  {
    aic7xxx_allocate_scb(p);
    scb = scbq_remove_head(&p->scb_data->free_scbs);
    if(scb == NULL)
    {
      printk(WARN_LEAD "Couldn't get a free SCB.\n", p->host_no,
             CTL_OF_CMD(cmd));
      return 1;
    }
  }
  scb->cmd = cmd;

  aic7xxx_position(cmd) = scb->hscb->tag;
  cmd->scsi_done = fn;
  cmd->result = DID_OK;
  aic7xxx_error(cmd) = DID_OK;
  aic7xxx_status(cmd) = 0;
  cmd->host_scribble = NULL;

  aic7xxx_buildscb(p, cmd, scb);

  scb->flags |= SCB_ACTIVE | SCB_WAITINGQ;

  scbq_insert_tail(&p->waiting_scbs, scb);
  aic7xxx_run_waiting_queues(p);
  return (0);
}

static DEF_SCSI_QCMD(aic7xxx_queue)

static int __aic7xxx_bus_device_reset(struct scsi_cmnd *cmd)
{
  struct aic7xxx_host  *p;
  struct aic7xxx_scb   *scb;
  struct aic7xxx_hwscb *hscb;
  int channel;
  unsigned char saved_scbptr, lastphase;
  unsigned char hscb_index;
  int disconnected;
  struct aic_dev_data *aic_dev;

  if(cmd == NULL)
  {
    printk(KERN_ERR "aic7xxx_bus_device_reset: called with NULL cmd!\n");
    return FAILED;
  }
  p = (struct aic7xxx_host *)cmd->device->host->hostdata;
  aic_dev = AIC_DEV(cmd);
  if(aic7xxx_position(cmd) < p->scb_data->numscbs)
    scb = (p->scb_data->scb_array[aic7xxx_position(cmd)]);
  else
    return FAILED;

  hscb = scb->hscb;

  aic7xxx_isr(p);
  aic7xxx_done_cmds_complete(p);
  if(!(scb->flags & SCB_ACTIVE))
    return FAILED;

  pause_sequencer(p);
  lastphase = aic_inb(p, LASTPHASE);
  if (aic7xxx_verbose & VERBOSE_RESET_PROCESS)
  {
    printk(INFO_LEAD "Bus Device reset, scb flags 0x%x, ",
         p->host_no, CTL_OF_SCB(scb), scb->flags);
    switch (lastphase)
    {
      case P_DATAOUT:
        printk("Data-Out phase\n");
        break;
      case P_DATAIN:
        printk("Data-In phase\n");
        break;
      case P_COMMAND:
        printk("Command phase\n");
        break;
      case P_MESGOUT:
        printk("Message-Out phase\n");
        break;
      case P_STATUS:
        printk("Status phase\n");
        break;
      case P_MESGIN:
        printk("Message-In phase\n");
        break;
      default:
        printk("while idle, LASTPHASE = 0x%x\n", lastphase);
        break;
    }
    printk(INFO_LEAD "SCSISIGI 0x%x, SEQADDR 0x%x, SSTAT0 0x%x, SSTAT1 "
         "0x%x\n", p->host_no, CTL_OF_SCB(scb),
         aic_inb(p, SCSISIGI),
         aic_inb(p, SEQADDR0) | (aic_inb(p, SEQADDR1) << 8),
         aic_inb(p, SSTAT0), aic_inb(p, SSTAT1));
    printk(INFO_LEAD "SG_CACHEPTR 0x%x, SSTAT2 0x%x, STCNT 0x%x\n", p->host_no,
         CTL_OF_SCB(scb),
         (p->features & AHC_ULTRA2) ? aic_inb(p, SG_CACHEPTR) : 0,
         aic_inb(p, SSTAT2),
         aic_inb(p, STCNT + 2) << 16 | aic_inb(p, STCNT + 1) << 8 |
         aic_inb(p, STCNT));
  }

  channel = cmd->device->channel;

  saved_scbptr = aic_inb(p, SCBPTR);
  disconnected = FALSE;

  if (lastphase != P_BUSFREE)
  {
    if (aic_inb(p, SCB_TAG) >= p->scb_data->numscbs)
    {
      printk(WARN_LEAD "Invalid SCB ID %d is active, "
             "SCB flags = 0x%x.\n", p->host_no,
            CTL_OF_CMD(cmd), scb->hscb->tag, scb->flags);
      unpause_sequencer(p, FALSE);
      return FAILED;
    }
    if (scb->hscb->tag == aic_inb(p, SCB_TAG))
    { 
      if ( (lastphase == P_MESGOUT) || (lastphase == P_MESGIN) )
      {
        printk(WARN_LEAD "Device reset, Message buffer "
                "in use\n", p->host_no, CTL_OF_SCB(scb));
        unpause_sequencer(p, FALSE);
	return FAILED;
      }
	
      if (aic7xxx_verbose & VERBOSE_RESET_PROCESS)
        printk(INFO_LEAD "Device reset message in "
              "message buffer\n", p->host_no, CTL_OF_SCB(scb));
      scb->flags |= SCB_RESET | SCB_DEVICE_RESET;
      aic7xxx_error(cmd) = DID_RESET;
      aic_dev->flags |= BUS_DEVICE_RESET_PENDING;
      
      aic_outb(p, HOST_MSG, MSG_OUT);
      aic_outb(p, lastphase | ATNO, SCSISIGO);
      unpause_sequencer(p, FALSE);
      spin_unlock_irq(p->host->host_lock);
      ssleep(1);
      spin_lock_irq(p->host->host_lock);
      if(aic_dev->flags & BUS_DEVICE_RESET_PENDING)
        return FAILED;
      else
        return SUCCESS;
    }
  } 
  scb->hscb->control |= MK_MESSAGE;
  scb->flags |= SCB_RESET | SCB_DEVICE_RESET;
  aic_dev->flags |= BUS_DEVICE_RESET_PENDING;
  if (aic7xxx_search_qinfifo(p, cmd->device->channel, cmd->device->id, cmd->device->lun, hscb->tag,
			  0, TRUE, NULL) == 0)
  {
    disconnected = TRUE;
    if ((hscb_index = aic7xxx_find_scb(p, scb)) != SCB_LIST_NULL)
    {
      unsigned char scb_control;

      aic_outb(p, hscb_index, SCBPTR);
      scb_control = aic_inb(p, SCB_CONTROL);
      disconnected = (scb_control & DISCONNECTED);
      aic_outb(p, scb_control | MK_MESSAGE, SCB_CONTROL);
    }
    if (disconnected)
    {
      if (aic7xxx_verbose & VERBOSE_RESET_PROCESS)
        printk(INFO_LEAD "Queueing device reset command.\n", p->host_no,
		      CTL_OF_SCB(scb));
      p->qinfifo[p->qinfifonext++] = scb->hscb->tag;
      if (p->features & AHC_QUEUE_REGS)
        aic_outb(p, p->qinfifonext, HNSCB_QOFF);
      else
        aic_outb(p, p->qinfifonext, KERNEL_QINPOS);
      scb->flags |= SCB_QUEUED_ABORT;
    }
  }
  aic_outb(p, saved_scbptr, SCBPTR);
  unpause_sequencer(p, FALSE);
  spin_unlock_irq(p->host->host_lock);
  msleep(1000/4);
  spin_lock_irq(p->host->host_lock);
  if(aic_dev->flags & BUS_DEVICE_RESET_PENDING)
    return FAILED;
  else
    return SUCCESS;
}

static int aic7xxx_bus_device_reset(struct scsi_cmnd *cmd)
{
      int rc;

      spin_lock_irq(cmd->device->host->host_lock);
      rc = __aic7xxx_bus_device_reset(cmd);
      spin_unlock_irq(cmd->device->host->host_lock);

      return rc;
}


static void aic7xxx_panic_abort(struct aic7xxx_host *p, struct scsi_cmnd *cmd)
{

  printk("aic7xxx driver version %s\n", AIC7XXX_C_VERSION);
  printk("Controller type:\n    %s\n", board_names[p->board_name_index]);
  printk("p->flags=0x%lx, p->chip=0x%x, p->features=0x%x, "
         "sequencer %s paused\n",
     p->flags, p->chip, p->features,
    (aic_inb(p, HCNTRL) & PAUSE) ? "is" : "isn't" );
  pause_sequencer(p);
  disable_irq(p->irq);
  aic7xxx_print_card(p);
  aic7xxx_print_scratch_ram(p);
  spin_unlock_irq(p->host->host_lock);
  for(;;) barrier();
}

static int __aic7xxx_abort(struct scsi_cmnd *cmd)
{
  struct aic7xxx_scb  *scb = NULL;
  struct aic7xxx_host *p;
  int    found=0, disconnected;
  unsigned char saved_hscbptr, hscbptr, scb_control;
  struct aic_dev_data *aic_dev;

  if(cmd == NULL)
  {
    printk(KERN_ERR "aic7xxx_abort: called with NULL cmd!\n");
    return FAILED;
  }
  p = (struct aic7xxx_host *)cmd->device->host->hostdata;
  aic_dev = AIC_DEV(cmd);
  if(aic7xxx_position(cmd) < p->scb_data->numscbs)
    scb = (p->scb_data->scb_array[aic7xxx_position(cmd)]);
  else
    return FAILED;

  aic7xxx_isr(p);
  aic7xxx_done_cmds_complete(p);
  if(!(scb->flags & SCB_ACTIVE))
    return FAILED;

  pause_sequencer(p);

  if (aic7xxx_panic_on_abort)
    aic7xxx_panic_abort(p, cmd);

  if (aic7xxx_verbose & VERBOSE_ABORT)
  {
    printk(INFO_LEAD "Aborting scb %d, flags 0x%x, SEQADDR 0x%x, LASTPHASE "
           "0x%x\n",
         p->host_no, CTL_OF_SCB(scb), scb->hscb->tag, scb->flags,
         aic_inb(p, SEQADDR0) | (aic_inb(p, SEQADDR1) << 8),
         aic_inb(p, LASTPHASE));
    printk(INFO_LEAD "SG_CACHEPTR 0x%x, SG_COUNT %d, SCSISIGI 0x%x\n",
         p->host_no, CTL_OF_SCB(scb), (p->features & AHC_ULTRA2) ?
         aic_inb(p, SG_CACHEPTR) : 0, aic_inb(p, SG_COUNT),
         aic_inb(p, SCSISIGI));
    printk(INFO_LEAD "SSTAT0 0x%x, SSTAT1 0x%x, SSTAT2 0x%x\n",
         p->host_no, CTL_OF_SCB(scb), aic_inb(p, SSTAT0),
         aic_inb(p, SSTAT1), aic_inb(p, SSTAT2));
  }

  if (scb->flags & SCB_WAITINGQ)
  {
    if (aic7xxx_verbose & VERBOSE_ABORT_PROCESS) 
      printk(INFO_LEAD "SCB found on waiting list and "
          "aborted.\n", p->host_no, CTL_OF_SCB(scb));
    scbq_remove(&p->waiting_scbs, scb);
    scbq_remove(&aic_dev->delayed_scbs, scb);
    aic_dev->active_cmds++;
    p->activescbs++;
    scb->flags &= ~(SCB_WAITINGQ | SCB_ACTIVE);
    scb->flags |= SCB_ABORT | SCB_QUEUED_FOR_DONE;
    goto success;
  }

  if ( ((found = aic7xxx_search_qinfifo(p, cmd->device->id, cmd->device->channel,
                     cmd->device->lun, scb->hscb->tag, SCB_ABORT | SCB_QUEUED_FOR_DONE,
                     FALSE, NULL)) != 0) &&
                    (aic7xxx_verbose & VERBOSE_ABORT_PROCESS))
  {
    printk(INFO_LEAD "SCB found in QINFIFO and aborted.\n", p->host_no,
		    CTL_OF_SCB(scb));
    goto success;
  }


  saved_hscbptr = aic_inb(p, SCBPTR);
  if ((hscbptr = aic7xxx_find_scb(p, scb)) != SCB_LIST_NULL)
  {
    aic_outb(p, hscbptr, SCBPTR);
    scb_control = aic_inb(p, SCB_CONTROL);
    disconnected = scb_control & DISCONNECTED;
    if(!disconnected && aic_inb(p, LASTPHASE) == P_BUSFREE) {
      if (aic7xxx_verbose & VERBOSE_ABORT_PROCESS)
        printk(INFO_LEAD "SCB found on hardware waiting"
          " list and aborted.\n", p->host_no, CTL_OF_SCB(scb));
      
      if (aic_inb(p, WAITING_SCBH) == hscbptr && aic_inb(p, SCB_NEXT) ==
			SCB_LIST_NULL)
      {
        aic_outb(p, aic_inb(p, SCSISEQ) & ~ENSELO, SCSISEQ);
        aic_outb(p, CLRSELTIMEO, CLRSINT1);
	aic_outb(p, SCB_LIST_NULL, WAITING_SCBH);
      }
      else
      {
	unsigned char prev, next;
	prev = SCB_LIST_NULL;
	next = aic_inb(p, WAITING_SCBH);
	while(next != SCB_LIST_NULL)
	{
	  aic_outb(p, next, SCBPTR);
	  if (next == hscbptr)
	  {
	    next = aic_inb(p, SCB_NEXT);
	    if (prev != SCB_LIST_NULL)
	    {
	      aic_outb(p, prev, SCBPTR);
	      aic_outb(p, next, SCB_NEXT);
	    }
	    else
	      aic_outb(p, next, WAITING_SCBH);
	    aic_outb(p, hscbptr, SCBPTR);
	    next = SCB_LIST_NULL;
	  }
	  else
	  {
	    prev = next;
	    next = aic_inb(p, SCB_NEXT);
	  }
	}
      }
      aic_outb(p, SCB_LIST_NULL, SCB_TAG);
      aic_outb(p, 0, SCB_CONTROL);
      aic7xxx_add_curscb_to_free_list(p);
      scb->flags = SCB_ABORT | SCB_QUEUED_FOR_DONE;
      goto success;
    }
    else if (!disconnected)
    {
      if((aic_inb(p, LASTPHASE) == P_MESGIN) ||
	 (aic_inb(p, LASTPHASE) == P_MESGOUT))
      {
	printk(INFO_LEAD "message buffer busy, unable to abort.\n",
			  p->host_no, CTL_OF_SCB(scb));
	unpause_sequencer(p, FALSE);
	return FAILED;
      }
      
    } 
    aic_outb(p,  scb_control | MK_MESSAGE, SCB_CONTROL);
    if(!disconnected)
    {
      aic_outb(p, HOST_MSG, MSG_OUT);
      aic_outb(p, aic_inb(p, SCSISIGI) | ATNO, SCSISIGO);
    }
    aic_outb(p, saved_hscbptr, SCBPTR);
  } 
  else
  {
    disconnected = 1;
  }
        
  p->flags |= AHC_ABORT_PENDING;
  scb->flags |= SCB_QUEUED_ABORT | SCB_ABORT | SCB_RECOVERY_SCB;
  scb->hscb->control |= MK_MESSAGE;
  if(disconnected)
  {
    if (aic7xxx_verbose & VERBOSE_ABORT_PROCESS)
      printk(INFO_LEAD "SCB disconnected.  Queueing Abort"
        " SCB.\n", p->host_no, CTL_OF_SCB(scb));
    p->qinfifo[p->qinfifonext++] = scb->hscb->tag;
    if (p->features & AHC_QUEUE_REGS)
      aic_outb(p, p->qinfifonext, HNSCB_QOFF);
    else
      aic_outb(p, p->qinfifonext, KERNEL_QINPOS);
  }
  unpause_sequencer(p, FALSE);
  spin_unlock_irq(p->host->host_lock);
  msleep(1000/4);
  spin_lock_irq(p->host->host_lock);
  if (p->flags & AHC_ABORT_PENDING)
  {
    if (aic7xxx_verbose & VERBOSE_ABORT_RETURN)
      printk(INFO_LEAD "Abort never delivered, returning FAILED\n", p->host_no,
		    CTL_OF_CMD(cmd));
    p->flags &= ~AHC_ABORT_PENDING;
    return FAILED;
  }
  if (aic7xxx_verbose & VERBOSE_ABORT_RETURN)
    printk(INFO_LEAD "Abort successful.\n", p->host_no, CTL_OF_CMD(cmd));
  return SUCCESS;

success:
  if (aic7xxx_verbose & VERBOSE_ABORT_RETURN)
    printk(INFO_LEAD "Abort successful.\n", p->host_no, CTL_OF_CMD(cmd));
  aic7xxx_run_done_queue(p, TRUE);
  unpause_sequencer(p, FALSE);
  return SUCCESS;
}

static int aic7xxx_abort(struct scsi_cmnd *cmd)
{
	int rc;

	spin_lock_irq(cmd->device->host->host_lock);
	rc = __aic7xxx_abort(cmd);
	spin_unlock_irq(cmd->device->host->host_lock);

	return rc;
}


static int aic7xxx_reset(struct scsi_cmnd *cmd)
{
  struct aic7xxx_scb *scb;
  struct aic7xxx_host *p;
  struct aic_dev_data *aic_dev;

  p = (struct aic7xxx_host *) cmd->device->host->hostdata;
  spin_lock_irq(p->host->host_lock);

  aic_dev = AIC_DEV(cmd);
  if(aic7xxx_position(cmd) < p->scb_data->numscbs)
  {
    scb = (p->scb_data->scb_array[aic7xxx_position(cmd)]);
    if (scb->cmd != cmd)
      scb = NULL;
  }
  else
  {
    scb = NULL;
  }

  if (aic7xxx_panic_on_abort)
    aic7xxx_panic_abort(p, cmd);

  pause_sequencer(p);

  while((aic_inb(p, INTSTAT) & INT_PEND) && !(p->flags & AHC_IN_ISR))
  {
    aic7xxx_isr(p);
    pause_sequencer(p);
  }
  aic7xxx_done_cmds_complete(p);

  if(scb && (scb->cmd == NULL))
  {
    unpause_sequencer(p, FALSE);
    spin_unlock_irq(p->host->host_lock);
    return SUCCESS;
  }
    
  aic7xxx_reset_channel(p, cmd->device->channel, TRUE);
  if (p->features & AHC_TWIN)
  {
    aic7xxx_reset_channel(p, cmd->device->channel ^ 0x01, TRUE);
    restart_sequencer(p);
  }
  aic_outb(p,  aic_inb(p, SIMODE1) & ~(ENREQINIT|ENBUSFREE), SIMODE1);
  aic7xxx_clear_intstat(p);
  p->flags &= ~AHC_HANDLING_REQINITS;
  p->msg_type = MSG_TYPE_NONE;
  p->msg_index = 0;
  p->msg_len = 0;
  aic7xxx_run_done_queue(p, TRUE);
  unpause_sequencer(p, FALSE);
  spin_unlock_irq(p->host->host_lock);
  ssleep(2);
  return SUCCESS;
}

static int
aic7xxx_biosparam(struct scsi_device *sdev, struct block_device *bdev,
		sector_t capacity, int geom[])
{
  sector_t heads, sectors, cylinders;
  int ret;
  struct aic7xxx_host *p;
  unsigned char *buf;

  p = (struct aic7xxx_host *) sdev->host->hostdata;
  buf = scsi_bios_ptable(bdev);

  if ( buf )
  {
    ret = scsi_partsize(buf, capacity, &geom[2], &geom[0], &geom[1]);
    kfree(buf);
    if ( ret != -1 )
      return(ret);
  }
  
  heads = 64;
  sectors = 32;
  cylinders = capacity >> 11;

  if ((p->flags & AHC_EXTEND_TRANS_A) && (cylinders > 1024))
  {
    heads = 255;
    sectors = 63;
    cylinders = capacity >> 14;
    if(capacity > (65535 * heads * sectors))
      cylinders = 65535;
    else
      cylinders = ((unsigned int)capacity) / (unsigned int)(heads * sectors);
  }

  geom[0] = (int)heads;
  geom[1] = (int)sectors;
  geom[2] = (int)cylinders;

  return (0);
}

static int
aic7xxx_release(struct Scsi_Host *host)
{
  struct aic7xxx_host *p = (struct aic7xxx_host *) host->hostdata;
  struct aic7xxx_host *next, *prev;

  if(p->irq)
    free_irq(p->irq, p);
#ifdef MMAPIO
  if(p->maddr)
  {
    iounmap(p->maddr);
  }
#endif 
  if(!p->pdev)
    release_region(p->base, MAXREG - MINREG);
#ifdef CONFIG_PCI
  else {
    pci_release_regions(p->pdev);
    pci_dev_put(p->pdev);
  }
#endif
  prev = NULL;
  next = first_aic7xxx;
  while(next != NULL)
  {
    if(next == p)
    {
      if(prev == NULL)
        first_aic7xxx = next->next;
      else
        prev->next = next->next;
    }
    else
    {
      prev = next;
    }
    next = next->next;
  }
  aic7xxx_free(p);
  return(0);
}

static void
aic7xxx_print_card(struct aic7xxx_host *p)
{
  int i, j, k, chip;
  static struct register_ranges {
    int num_ranges;
    int range_val[32];
  } cards_ds[] = {
    { 0, {0,} }, 
    {10, {0x00, 0x05, 0x08, 0x11, 0x18, 0x19, 0x1f, 0x1f, 0x60, 0x60, 
          0x62, 0x66, 0x80, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9b, 0x9f} },
    { 9, {0x00, 0x05, 0x08, 0x11, 0x18, 0x1f, 0x60, 0x60, 0x62, 0x66, 
          0x80, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9f} },
    { 9, {0x00, 0x05, 0x08, 0x11, 0x18, 0x1f, 0x60, 0x60, 0x62, 0x66, 
          0x80, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9f} },
    {10, {0x00, 0x05, 0x08, 0x11, 0x18, 0x19, 0x1c, 0x1f, 0x60, 0x60, 
          0x62, 0x66, 0x80, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9f} },
    {10, {0x00, 0x05, 0x08, 0x11, 0x18, 0x1a, 0x1c, 0x1f, 0x60, 0x60, 
          0x62, 0x66, 0x80, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9f} },
    {16, {0x00, 0x05, 0x08, 0x11, 0x18, 0x1f, 0x60, 0x60, 0x62, 0x66, 
          0x84, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9a, 0x9f, 0x9f,
          0xe0, 0xf1, 0xf4, 0xf4, 0xf6, 0xf6, 0xf8, 0xf8, 0xfa, 0xfc,
          0xfe, 0xff} },
    {12, {0x00, 0x05, 0x08, 0x11, 0x18, 0x19, 0x1b, 0x1f, 0x60, 0x60, 
          0x62, 0x66, 0x80, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9a,
          0x9f, 0x9f, 0xe0, 0xf1} },
    {16, {0x00, 0x05, 0x08, 0x11, 0x18, 0x1f, 0x60, 0x60, 0x62, 0x66, 
          0x84, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9a, 0x9f, 0x9f,
          0xe0, 0xf1, 0xf4, 0xf4, 0xf6, 0xf6, 0xf8, 0xf8, 0xfa, 0xfc,
          0xfe, 0xff} },
    {12, {0x00, 0x05, 0x08, 0x11, 0x18, 0x1f, 0x60, 0x60, 0x62, 0x66, 
          0x84, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9a, 0x9c, 0x9f,
          0xe0, 0xf1, 0xf4, 0xfc} },
    {12, {0x00, 0x05, 0x08, 0x11, 0x18, 0x1f, 0x60, 0x60, 0x62, 0x66, 
          0x84, 0x8e, 0x90, 0x95, 0x97, 0x97, 0x9a, 0x9a, 0x9c, 0x9f,
          0xe0, 0xf1, 0xf4, 0xfc} },
  };
  chip = p->chip & AHC_CHIPID_MASK;
  printk("%s at ",
         board_names[p->board_name_index]);
  switch(p->chip & ~AHC_CHIPID_MASK)
  {
    case AHC_VL:
      printk("VLB Slot %d.\n", p->pci_device_fn);
      break;
    case AHC_EISA:
      printk("EISA Slot %d.\n", p->pci_device_fn);
      break;
    case AHC_PCI:
    default:
      printk("PCI %d/%d/%d.\n", p->pci_bus, PCI_SLOT(p->pci_device_fn),
             PCI_FUNC(p->pci_device_fn));
      break;
  }

  printk("Card Dump:\n");
  k = 0;
  for(i=0; i<cards_ds[chip].num_ranges; i++)
  {
    for(j  = cards_ds[chip].range_val[ i * 2 ];
        j <= cards_ds[chip].range_val[ i * 2 + 1 ] ;
        j++)
    {
      printk("%02x:%02x ", j, aic_inb(p, j));
      if(++k == 13)
      {
        printk("\n");
        k=0;
      }
    }
  }
  if(k != 0)
    printk("\n");


  if(p->features & AHC_QUEUE_REGS)
  {
    aic_outb(p, 0, SDSCB_QOFF);
    aic_outb(p, 0, SNSCB_QOFF);
    aic_outb(p, 0, HNSCB_QOFF);
  }

}

static void
aic7xxx_print_scratch_ram(struct aic7xxx_host *p)
{
  int i, k;

  k = 0;
  printk("Scratch RAM:\n");
  for(i = SRAM_BASE; i < SEQCTL; i++)
  {
    printk("%02x:%02x ", i, aic_inb(p, i));
    if(++k == 13)
    {
      printk("\n");
      k=0;
    }
  }
  if (p->features & AHC_MORE_SRAM)
  {
    for(i = TARG_OFFSET; i < 0x80; i++)
    {
      printk("%02x:%02x ", i, aic_inb(p, i));
      if(++k == 13)
      {
        printk("\n");
        k=0;
      }
    }
  }
  printk("\n");
}


#include "aic7xxx_old/aic7xxx_proc.c"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(AIC7XXX_H_VERSION);


static struct scsi_host_template driver_template = {
	.proc_info		= aic7xxx_proc_info,
	.detect			= aic7xxx_detect,
	.release		= aic7xxx_release,
	.info			= aic7xxx_info,	
	.queuecommand		= aic7xxx_queue,
	.slave_alloc		= aic7xxx_slave_alloc,
	.slave_configure	= aic7xxx_slave_configure,
	.slave_destroy		= aic7xxx_slave_destroy,
	.bios_param		= aic7xxx_biosparam,
	.eh_abort_handler	= aic7xxx_abort,
	.eh_device_reset_handler	= aic7xxx_bus_device_reset,
	.eh_host_reset_handler	= aic7xxx_reset,
	.can_queue		= 255,
	.this_id		= -1,
	.max_sectors		= 2048,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
};

#include "scsi_module.c"

