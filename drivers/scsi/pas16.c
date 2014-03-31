#define AUTOSENSE
#define PSEUDO_DMA
#define FOO
#define UNSAFE  
#define PDEBUG 0

/*
 * This driver adapted from Drew Eckhardt's Trantor T128 driver
 *
 * Copyright 1993, Drew Eckhardt
 *	Visionary Computing
 *	(Unix and Linux consulting and custom programming)
 *	drew@colorado.edu
 *      +1 (303) 666-5836
 *
 *  ( Based on T128 - DISTRIBUTION RELEASE 3. ) 
 *
 * Modified to work with the Pro Audio Spectrum/Studio 16
 * by John Weidman.
 *
 *
 * For more information, please consult 
 *
 * Media Vision
 * (510) 770-8600
 * (800) 348-7116
 * 
 * and 
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
 *
 * NCR Microelectronics
 * 1635 Aeroplaza Drive
 * Colorado Springs, CO 80916
 * 1+ (719) 578-3400
 * 1+ (800) 334-5454
 */

 
#include <linux/module.h>

#include <linux/signal.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/stat.h>
#include <linux/init.h>

#include "scsi.h"
#include <scsi/scsi_host.h>
#include "pas16.h"
#define AUTOPROBE_IRQ
#include "NCR5380.h"


static int pas_maxi = 0;
static int pas_wmaxi = 0;
static unsigned short pas16_addr = 0;
static int pas16_irq = 0;
 

static const int scsi_irq_translate[] =
	{ 0,  0,  1,  2,  3,  4,  5,  6, 0,  0,  7,  8,  9,  0, 10, 11 };

static int default_irqs[] __initdata =
	{  PAS16_DEFAULT_BOARD_1_IRQ,
	   PAS16_DEFAULT_BOARD_2_IRQ,
	   PAS16_DEFAULT_BOARD_3_IRQ,
	   PAS16_DEFAULT_BOARD_4_IRQ
	};

static struct override {
    unsigned short io_port;
    int  irq;
} overrides
#ifdef PAS16_OVERRIDE
    [] __initdata = PAS16_OVERRIDE;
#else
    [4] __initdata = {{0,IRQ_AUTO}, {0,IRQ_AUTO}, {0,IRQ_AUTO},
	{0,IRQ_AUTO}};
#endif

#define NO_OVERRIDES ARRAY_SIZE(overrides)

static struct base {
    unsigned short io_port;
    int noauto;
} bases[] __initdata =
	{ {PAS16_DEFAULT_BASE_1, 0},
	  {PAS16_DEFAULT_BASE_2, 0},
	  {PAS16_DEFAULT_BASE_3, 0},
	  {PAS16_DEFAULT_BASE_4, 0}
	};

#define NO_BASES ARRAY_SIZE(bases)

static const unsigned short  pas16_offset[ 8 ] =
    {
	0x1c00,    
	0x1c01,    
	0x1c02,    
	0x1c03,    
	0x3c00,    
	0x3c01,    
	0x3c02,    
	0x3c03,    
    };
#if 1
#define rtrc(i) {inb(0x3da); outb(0x31, 0x3c0); outb((i), 0x3c0);}
#else
#define rtrc(i) {}
#endif



static void __init
	enable_board( int  board_num,  unsigned short port )
{
    outb( 0xbc + board_num, MASTER_ADDRESS_PTR );
    outb( port >> 2, MASTER_ADDRESS_PTR );
}




static void __init 
	init_board( unsigned short io_port, int irq, int force_irq )
{
	unsigned int	tmp;
	unsigned int	pas_irq_code;

	

	outb( 0x30, io_port + P_TIMEOUT_COUNTER_REG );  
	outb( 0x01, io_port + P_TIMEOUT_STATUS_REG_OFFSET );   
	outb( 0x01, io_port + WAIT_STATE );   

	NCR5380_read( RESET_PARITY_INTERRUPT_REG );

	pas_irq_code = ( irq < 16 ) ? scsi_irq_translate[irq] : 0;
	tmp = inb( io_port + IO_CONFIG_3 );

	if( (( tmp & 0x0f ) == pas_irq_code) && pas_irq_code > 0 
	    && !force_irq )
	{
	    printk( "pas16: WARNING: Can't use same irq as sound "
		    "driver -- interrupts disabled\n" );
	    
	    outb( 0x4d, io_port + SYS_CONFIG_4 );
	}
	else
	{
	    tmp = (  tmp & 0x0f ) | ( pas_irq_code << 4 );
	    outb( tmp, io_port + IO_CONFIG_3 );

	    
	    outb( 0x6d, io_port + SYS_CONFIG_4 );
	}
}



static int __init 
     pas16_hw_detect( unsigned short  board_num )
{
    unsigned char	board_rev, tmp;
    unsigned short	io_port = bases[ board_num ].io_port;


    enable_board( board_num, io_port );

    
    board_rev = inb( io_port + PCB_CONFIG );

    if( board_rev == 0xff )
	return 0;

    tmp = board_rev ^ 0xe0;

    outb( tmp, io_port + PCB_CONFIG );
    tmp = inb( io_port + PCB_CONFIG );
    outb( board_rev, io_port + PCB_CONFIG );

    if( board_rev != tmp ) 	
	return 0;

    if( ( inb( io_port + OPERATION_MODE_1 ) & 0x03 ) != 0x03 ) 
	return 0;  	


    outb( 0x01, io_port + WAIT_STATE ); 	
    NCR5380_write( MODE_REG, 0x20 );		
    if( NCR5380_read( MODE_REG ) != 0x20 )	
	return 0;				
    NCR5380_write( MODE_REG, 0x00 );		
    if( NCR5380_read( MODE_REG ) != 0x00 )
	return 0;

    return 1;
}



void __init pas16_setup(char *str, int *ints)
{
    static int commandline_current = 0;
    int i;
    if (ints[0] != 2) 
	printk("pas16_setup : usage pas16=io_port,irq\n");
    else 
	if (commandline_current < NO_OVERRIDES) {
	    overrides[commandline_current].io_port = (unsigned short) ints[1];
	    overrides[commandline_current].irq = ints[2];
	    for (i = 0; i < NO_BASES; ++i)
		if (bases[i].io_port == (unsigned short) ints[1]) {
 		    bases[i].noauto = 1;
		    break;
		}
	    ++commandline_current;
	}
}


int __init pas16_detect(struct scsi_host_template * tpnt)
{
    static int current_override = 0;
    static unsigned short current_base = 0;
    struct Scsi_Host *instance;
    unsigned short io_port;
    int  count;

    tpnt->proc_name = "pas16";
    tpnt->proc_info = &pas16_proc_info;

    if (pas16_addr != 0) {
	overrides[0].io_port = pas16_addr;
	for (count = 0; count < NO_BASES; ++count)
	    if (bases[count].io_port == pas16_addr) {
 		    bases[count].noauto = 1;
		    break;
	}
    }
    if (pas16_irq != 0)
	overrides[0].irq = pas16_irq;

    for (count = 0; current_override < NO_OVERRIDES; ++current_override) {
	io_port = 0;

	if (overrides[current_override].io_port)
	{
	    io_port = overrides[current_override].io_port;
	    enable_board( current_override, io_port );
	    init_board( io_port, overrides[current_override].irq, 1 );
	}
	else
	    for (; !io_port && (current_base < NO_BASES); ++current_base) {
#if (PDEBUG & PDEBUG_INIT)
    printk("scsi-pas16 : probing io_port %04x\n", (unsigned int) bases[current_base].io_port);
#endif
		if ( !bases[current_base].noauto &&
		     pas16_hw_detect( current_base ) ){
			io_port = bases[current_base].io_port;
			init_board( io_port, default_irqs[ current_base ], 0 ); 
#if (PDEBUG & PDEBUG_INIT)
			printk("scsi-pas16 : detected board.\n");
#endif
		}
    }


#if defined(PDEBUG) && (PDEBUG & PDEBUG_INIT)
	printk("scsi-pas16 : io_port = %04x\n", (unsigned int) io_port);
#endif

	if (!io_port)
	    break;

	instance = scsi_register (tpnt, sizeof(struct NCR5380_hostdata));
	if(instance == NULL)
		break;
		
	instance->io_port = io_port;

	NCR5380_init(instance, 0);

	if (overrides[current_override].irq != IRQ_AUTO)
	    instance->irq = overrides[current_override].irq;
	else 
	    instance->irq = NCR5380_probe_irq(instance, PAS16_IRQS);

	if (instance->irq != SCSI_IRQ_NONE) 
	    if (request_irq(instance->irq, pas16_intr, IRQF_DISABLED,
			    "pas16", instance)) {
		printk("scsi%d : IRQ%d not free, interrupts disabled\n", 
		    instance->host_no, instance->irq);
		instance->irq = SCSI_IRQ_NONE;
	    } 

	if (instance->irq == SCSI_IRQ_NONE) {
	    printk("scsi%d : interrupts not enabled. for better interactive performance,\n", instance->host_no);
	    printk("scsi%d : please jumper the board for a free IRQ.\n", instance->host_no);
	    
	    outb( 0x4d, io_port + SYS_CONFIG_4 );
	    outb( (inb(io_port + IO_CONFIG_3) & 0x0f), io_port + IO_CONFIG_3 );
	}

#if defined(PDEBUG) && (PDEBUG & PDEBUG_INIT)
	printk("scsi%d : irq = %d\n", instance->host_no, instance->irq);
#endif

	printk("scsi%d : at 0x%04x", instance->host_no, (int) 
	    instance->io_port);
	if (instance->irq == SCSI_IRQ_NONE)
	    printk (" interrupts disabled");
	else 
	    printk (" irq %d", instance->irq);
	printk(" options CAN_QUEUE=%d  CMD_PER_LUN=%d release=%d",
	    CAN_QUEUE, CMD_PER_LUN, PAS16_PUBLIC_RELEASE);
	NCR5380_print_options(instance);
	printk("\n");

	++current_override;
	++count;
    }
    return count;
}



int pas16_biosparam(struct scsi_device *sdev, struct block_device *dev,
		sector_t capacity, int * ip)
{
  int size = capacity;
  ip[0] = 64;
  ip[1] = 32;
  ip[2] = size >> 11;		
  if( ip[2] > 1024 ) {		
	ip[0]=255;
	ip[1]=63;
	ip[2]=size/(63*255);
	if( ip[2] > 1023 )	
		ip[2] = 1023;
  }

  return 0;
}


static inline int NCR5380_pread (struct Scsi_Host *instance, unsigned char *dst,
    int len) {
    register unsigned char  *d = dst;
    register unsigned short reg = (unsigned short) (instance->io_port + 
	P_DATA_REG_OFFSET);
    register int i = len;
    int ii = 0;

    while ( !(inb(instance->io_port + P_STATUS_REG_OFFSET) & P_ST_RDY) )
	 ++ii;

    insb( reg, d, i );

    if ( inb(instance->io_port + P_TIMEOUT_STATUS_REG_OFFSET) & P_TS_TIM) {
	outb( P_TS_CT, instance->io_port + P_TIMEOUT_STATUS_REG_OFFSET);
	printk("scsi%d : watchdog timer fired in NCR5380_pread()\n",
	    instance->host_no);
	return -1;
    }
   if (ii > pas_maxi)
      pas_maxi = ii;
    return 0;
}


static inline int NCR5380_pwrite (struct Scsi_Host *instance, unsigned char *src,
    int len) {
    register unsigned char *s = src;
    register unsigned short reg = (instance->io_port + P_DATA_REG_OFFSET);
    register int i = len;
    int ii = 0;

    while ( !((inb(instance->io_port + P_STATUS_REG_OFFSET)) & P_ST_RDY) )
	 ++ii;
 
    outsb( reg, s, i );

    if (inb(instance->io_port + P_TIMEOUT_STATUS_REG_OFFSET) & P_TS_TIM) {
	outb( P_TS_CT, instance->io_port + P_TIMEOUT_STATUS_REG_OFFSET);
	printk("scsi%d : watchdog timer fired in NCR5380_pwrite()\n",
	    instance->host_no);
	return -1;
    }
    if (ii > pas_maxi)
	 pas_wmaxi = ii;
    return 0;
}

#include "NCR5380.c"

static int pas16_release(struct Scsi_Host *shost)
{
	if (shost->irq)
		free_irq(shost->irq, shost);
	NCR5380_exit(shost);
	if (shost->dma_channel != 0xff)
		free_dma(shost->dma_channel);
	if (shost->io_port && shost->n_io_port)
		release_region(shost->io_port, shost->n_io_port);
	scsi_unregister(shost);
	return 0;
}

static struct scsi_host_template driver_template = {
	.name           = "Pro Audio Spectrum-16 SCSI",
	.detect         = pas16_detect,
	.release        = pas16_release,
	.queuecommand   = pas16_queue_command,
	.eh_abort_handler = pas16_abort,
	.eh_bus_reset_handler = pas16_bus_reset,
	.bios_param     = pas16_biosparam, 
	.can_queue      = CAN_QUEUE,
	.this_id        = 7,
	.sg_tablesize   = SG_ALL,
	.cmd_per_lun    = CMD_PER_LUN,
	.use_clustering = DISABLE_CLUSTERING,
};
#include "scsi_module.c"

#ifdef MODULE
module_param(pas16_addr, ushort, 0);
module_param(pas16_irq, int, 0);
#endif
MODULE_LICENSE("GPL");
