/*
 *  UART driver for 68360 CPM SCC or SMC
 *  Copyright (c) 2000 D. Jeff Dionne <jeff@uclinux.org>,
 *  Copyright (c) 2000 Michael Leslie <mleslie@lineo.ca>
 *  Copyright (c) 1997 Dan Malek <dmalek@jlc.net>
 *
 * I used the serial.c driver as the framework for this driver.
 * Give credit to those guys.
 * The original code was written for the MBX860 board.  I tried to make
 * it generic, but there may be some assumptions in the structures that
 * have to be fixed later.
 * To save porting time, I did not bother to change any object names
 * that are not accessed outside of this file.
 * It still needs lots of work........When it was easy, I included code
 * to support the SCCs, but this has never been tested, nor is it complete.
 * Only the SCCs support modem control, so that is not complete either.
 *
 * This module exports the following rs232 io functions:
 *
 *	int rs_360_init(void);
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serialP.h> 
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <asm/m68360.h>
#include <asm/commproc.h>

 
#ifdef CONFIG_KGDB
extern void breakpoint(void);
extern void set_debug_traps(void);
extern int  kgdb_output_string (const char* s, unsigned int count);
#endif


 
#include <linux/console.h>
#include <linux/jiffies.h>

#ifndef CONFIG_SERIAL_CONSOLE_PORT
#define CONFIG_SERIAL_CONSOLE_PORT	1 
#endif

#if 0
#undef CONFIG_SERIAL_CONSOLE_PORT
#define CONFIG_SERIAL_CONSOLE_PORT	2
#endif


#define TX_WAKEUP	ASYNC_SHARE_IRQ

static char *serial_name = "CPM UART driver";
static char *serial_version = "0.03";

static struct tty_driver *serial_driver;
int serial_console_setup(struct console *co, char *options);

#define SERIAL_PARANOIA_CHECK
#define CONFIG_SERIAL_NOPAUSE_IO
#define SERIAL_DO_RESTART


#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT

#define _INLINE_ inline
  
#define DBG_CNT(s)

#define smc_scc_num	hub6
#define NUM_IS_SCC	((int)0x00010000)
#define PORT_NUM(P)	((P) & 0x0000ffff)


#if defined (CONFIG_UCQUICC)

volatile extern void *_periph_base;
#define SIPEX_MODE(n,m) ((m & 0x0f)<<(16+4*(n-1)))

static uint sipex_mode_bits = 0x00000000;

#endif



#if 0
struct async_icount_24 {
	__u32   cts, dsr, rng, dcd, tx, rx;
	__u32   frame, parity, overrun, brk;
	__u32   buf_overrun;
} icount;
#endif

#if 0

struct serial_state {
        int     magic;
        int     baud_base;
        unsigned long   port;
        int     irq;
        int     flags;
        int     hub6;
        int     type;
        int     line;
        int     revision;       
        int     xmit_fifo_size;
        int     custom_divisor;
        int     count;
        u8      *iomem_base;
        u16     iomem_reg_shift;
        unsigned short  close_delay;
        unsigned short  closing_wait; 
        struct async_icount_24     icount; 
        int     io_type;
        struct async_struct *info;
};
#endif

#define SSTATE_MAGIC 0x5302



#define USE_SMC2 1

#if 0
#define SCC_NUM_BASE	(USE_SMC2 + 1)	

#define SCC_IDX_BASE	1	
#endif


static struct serial_state rs_table[] = {
	{  0,     0, PRSLOT_SMC1, CPMVEC_SMC1,   0,    0 }    
#if USE_SMC2
	,{ 0,     0, PRSLOT_SMC2, CPMVEC_SMC2,   0,    1 }     
#endif

#if defined(CONFIG_SERIAL_68360_SCC)
	,{ 0,     0, PRSLOT_SCC2, CPMVEC_SCC2,   0, (NUM_IS_SCC | 1) }    
	,{ 0,     0, PRSLOT_SCC3, CPMVEC_SCC3,   0, (NUM_IS_SCC | 2) }    
	,{ 0,     0, PRSLOT_SCC4, CPMVEC_SCC4,   0, (NUM_IS_SCC | 3) }    
#endif
};

#define NR_PORTS	(sizeof(rs_table)/sizeof(struct serial_state))

#define RX_NUM_FIFO	4
#define RX_BUF_SIZE	32
#define TX_NUM_FIFO	4
#define TX_BUF_SIZE	32

#define CONSOLE_NUM_FIFO 2
#define CONSOLE_BUF_SIZE 4

char *console_fifos[CONSOLE_NUM_FIFO * CONSOLE_BUF_SIZE];

typedef struct serial_info {
	int			magic;
	int			flags;

	struct serial_state	*state;
 	
 	
	
	struct tty_struct 	*tty;
	int			read_status_mask;
	int			ignore_status_mask;
	int			timeout;
	int			line;
	int			x_char;	
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	unsigned long		event;
	unsigned long		last_active;
	int			blocked_open; 
	struct work_struct	tqueue;
	struct work_struct	tqueue_hangup;
 	wait_queue_head_t	open_wait; 
 	wait_queue_head_t	close_wait; 

	
	QUICC_BD			*rx_bd_base;
	QUICC_BD			*rx_cur;
	QUICC_BD			*tx_bd_base;
	QUICC_BD			*tx_cur;
} ser_info_t;


static ser_info_t  quicc_ser_info[NR_PORTS];
static char rx_buf_pool[NR_PORTS * RX_NUM_FIFO * RX_BUF_SIZE];
static char tx_buf_pool[NR_PORTS * TX_NUM_FIFO * TX_BUF_SIZE];

static void change_speed(ser_info_t *info);
static void rs_360_wait_until_sent(struct tty_struct *tty, int timeout);

static inline int serial_paranoia_check(ser_info_t *info,
					char *name, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null async_struct for (%s) in %s\n";

	if (!info) {
		printk(badinfo, name, routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, name, routine);
		return 1;
	}
#endif
	return 0;
}

static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 230400, 460800, 0 };

#if defined(CONFIG_CONSOLE_9600)
  #define CONSOLE_BAUDRATE 9600
#elif defined(CONFIG_CONSOLE_19200)
  #define CONSOLE_BAUDRATE 19200
#elif defined(CONFIG_CONSOLE_115200)
  #define CONSOLE_BAUDRATE 115200
#else
  #warning "console baud rate undefined"
  #define CONSOLE_BAUDRATE 9600
#endif

static void rs_360_stop(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	int	idx;
	unsigned long flags;
 	volatile struct scc_regs *sccp;
 	volatile struct smc_regs *smcp;

	if (serial_paranoia_check(info, tty->name, "rs_stop"))
		return;
	
	local_irq_save(flags);
	idx = PORT_NUM(info->state->smc_scc_num);
	if (info->state->smc_scc_num & NUM_IS_SCC) {
		sccp = &pquicc->scc_regs[idx];
		sccp->scc_sccm &= ~UART_SCCM_TX;
	} else {
		
		smcp = &pquicc->smc_regs[idx];
		smcp->smc_smcm &= ~SMCM_TX;
	}
	local_irq_restore(flags);
}


static void rs_360_start(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	int	idx;
	unsigned long flags;
	volatile struct scc_regs *sccp;
	volatile struct smc_regs *smcp;

	if (serial_paranoia_check(info, tty->name, "rs_stop"))
		return;
	
	local_irq_save(flags);
	idx = PORT_NUM(info->state->smc_scc_num);
	if (info->state->smc_scc_num & NUM_IS_SCC) {
		sccp = &pquicc->scc_regs[idx];
		sccp->scc_sccm |= UART_SCCM_TX;
	} else {
		smcp = &pquicc->smc_regs[idx];
		smcp->smc_smcm |= SMCM_TX;
	}
	local_irq_restore(flags);
}


static _INLINE_ void receive_chars(ser_info_t *info)
{
	struct tty_struct *tty = info->port.tty;
	unsigned char ch, flag, *cp;
	
	int	i;
	ushort	status;
	 struct	async_icount *icount; 
	
	volatile QUICC_BD	*bdp;

	icount = &info->state->icount;

	bdp = info->rx_cur;
	for (;;) {
		if (bdp->status & BD_SC_EMPTY)	
			break;			

		if (!(info->read_status_mask & BD_SC_EMPTY)) {
			bdp->status |= BD_SC_EMPTY;
			bdp->status &=
				~(BD_SC_BR | BD_SC_FR | BD_SC_PR | BD_SC_OV);

			if (bdp->status & BD_SC_WRAP)
				bdp = info->rx_bd_base;
			else
				bdp++;
			continue;
		}

		i = bdp->length;
		
		cp = (char *)bdp->buf;
		status = bdp->status;

		while (i-- > 0) {
			ch = *cp++;
			icount->rx++;

#ifdef SERIAL_DEBUG_INTR
			printk("DR%02x:%02x...", ch, status);
#endif
			flag = TTY_NORMAL;

			if (status & (BD_SC_BR | BD_SC_FR |
				       BD_SC_PR | BD_SC_OV)) {
				if (status & BD_SC_BR)
					icount->brk++;
				else if (status & BD_SC_PR)
					icount->parity++;
				else if (status & BD_SC_FR)
					icount->frame++;
				if (status & BD_SC_OV)
					icount->overrun++;

				status &= info->read_status_mask;
		
				if (status & (BD_SC_BR)) {
#ifdef SERIAL_DEBUG_INTR
					printk("handling break....");
#endif
					*tty->flip.flag_buf_ptr = TTY_BREAK;
					if (info->flags & ASYNC_SAK)
						do_SAK(tty);
				} else if (status & BD_SC_PR)
					flag = TTY_PARITY;
				else if (status & BD_SC_FR)
					flag = TTY_FRAME;
			}
			tty_insert_flip_char(tty, ch, flag);
			if (status & BD_SC_OV)
				tty_insert_flip_char(tty, 0, TTY_OVERRUN);
		}

		bdp->status |= BD_SC_EMPTY;
		bdp->status &= ~(BD_SC_BR | BD_SC_FR | BD_SC_PR | BD_SC_OV);

		if (bdp->status & BD_SC_WRAP)
			bdp = info->rx_bd_base;
		else
			bdp++;
	}

	info->rx_cur = (QUICC_BD *)bdp;

	tty_schedule_flip(tty);
}

static _INLINE_ void receive_break(ser_info_t *info)
{
	struct tty_struct *tty = info->port.tty;

	info->state->icount.brk++;
	tty_insert_flip_char(tty, 0, TTY_BREAK);
	tty_schedule_flip(tty);
}

static _INLINE_ void transmit_chars(ser_info_t *info)
{

	if ((info->flags & TX_WAKEUP) ||
	    (info->port.tty->flags & (1 << TTY_DO_WRITE_WAKEUP))) {
		schedule_work(&info->tqueue);
	}

#ifdef SERIAL_DEBUG_INTR
	printk("THRE...");
#endif
}

#ifdef notdef
static _INLINE_ void check_modem_status(struct async_struct *info)
{
	int	status;
	
	struct	async_icount_24 *icount;
	
	status = serial_in(info, UART_MSR);

	if (status & UART_MSR_ANY_DELTA) {
		icount = &info->state->icount;
		
		if (status & UART_MSR_TERI)
			icount->rng++;
		if (status & UART_MSR_DDSR)
			icount->dsr++;
		if (status & UART_MSR_DDCD) {
			icount->dcd++;
#ifdef CONFIG_HARD_PPS
			if ((info->flags & ASYNC_HARDPPS_CD) &&
			    (status & UART_MSR_DCD))
				hardpps();
#endif
		}
		if (status & UART_MSR_DCTS)
			icount->cts++;
		wake_up_interruptible(&info->delta_msr_wait);
	}

	if ((info->flags & ASYNC_CHECK_CD) && (status & UART_MSR_DDCD)) {
#if (defined(SERIAL_DEBUG_OPEN) || defined(SERIAL_DEBUG_INTR))
		printk("ttys%d CD now %s...", info->line,
		       (status & UART_MSR_DCD) ? "on" : "off");
#endif		
		if (status & UART_MSR_DCD)
			wake_up_interruptible(&info->open_wait);
		else {
#ifdef SERIAL_DEBUG_OPEN
			printk("scheduling hangup...");
#endif
			queue_task(&info->tqueue_hangup,
					   &tq_scheduler);
		}
	}
	if (info->flags & ASYNC_CTS_FLOW) {
		if (info->port.tty->hw_stopped) {
			if (status & UART_MSR_CTS) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx start...");
#endif
				info->port.tty->hw_stopped = 0;
				info->IER |= UART_IER_THRI;
				serial_out(info, UART_IER, info->IER);
				rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
				return;
			}
		} else {
			if (!(status & UART_MSR_CTS)) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx stop...");
#endif
				info->port.tty->hw_stopped = 1;
				info->IER &= ~UART_IER_THRI;
				serial_out(info, UART_IER, info->IER);
			}
		}
	}
}
#endif

 
static void rs_360_interrupt(int vec, void *dev_id)
{
	u_char	events;
	int	idx;
	ser_info_t *info;
	volatile struct smc_regs *smcp;
	volatile struct scc_regs *sccp;
	
	info = dev_id;

	idx = PORT_NUM(info->state->smc_scc_num);
	if (info->state->smc_scc_num & NUM_IS_SCC) {
		sccp = &pquicc->scc_regs[idx];
		events = sccp->scc_scce;
		if (events & SCCM_RX)
			receive_chars(info);
		if (events & SCCM_TX)
			transmit_chars(info);
		sccp->scc_scce = events;
	} else {
		smcp = &pquicc->smc_regs[idx];
		events = smcp->smc_smce;
		if (events & SMCM_BRKE)
			receive_break(info);
		if (events & SMCM_RX)
			receive_chars(info);
		if (events & SMCM_TX)
			transmit_chars(info);
		smcp->smc_smce = events;
	}
	
#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt_single(%d, %x)...",
					info->state->smc_scc_num, events);
#endif
#ifdef modem_control
	check_modem_status(info);
#endif
	info->last_active = jiffies;
#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}




static void do_softint(void *private_)
{
	ser_info_t	*info = (ser_info_t *) private_;
	struct tty_struct	*tty;
	
	tty = info->port.tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event))
		tty_wakeup(tty);
}


static void do_serial_hangup(void *private_)
{
	struct async_struct	*info = (struct async_struct *) private_;
	struct tty_struct	*tty;
	
	tty = info->port.tty;
	if (!tty)
		return;

	tty_hangup(tty);
}


static int startup(ser_info_t *info)
{
	unsigned long flags;
	int	retval=0;
	int	idx;
	
	volatile struct smc_regs *smcp;
	volatile struct scc_regs *sccp;
	volatile struct smc_uart_pram	*up;
	volatile struct uart_pram	    *scup;


	local_irq_save(flags);

	if (info->flags & ASYNC_INITIALIZED) {
		goto errout;
	}

#ifdef maybe
	if (!state->port || !state->type) {
		if (info->port.tty)
			set_bit(TTY_IO_ERROR, &info->port.tty->flags);
		goto errout;
	}
#endif

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttys%d (irq %d)...", info->line, state->irq);
#endif


#ifdef modem_control
	info->MCR = 0;
	if (info->port.tty->termios->c_cflag & CBAUD)
		info->MCR = UART_MCR_DTR | UART_MCR_RTS;
#endif
	
	if (info->port.tty)
		clear_bit(TTY_IO_ERROR, &info->port.tty->flags);

	change_speed(info);

	idx = PORT_NUM(info->state->smc_scc_num);
	if (info->state->smc_scc_num & NUM_IS_SCC) {
		sccp = &pquicc->scc_regs[idx];
		scup = &pquicc->pram[info->state->port].scc.pscc.u;

		scup->mrblr = RX_BUF_SIZE;
		scup->max_idl = RX_BUF_SIZE;

		sccp->scc_sccm |= (UART_SCCM_TX | UART_SCCM_RX);
		sccp->scc_gsmr.w.low |= (SCC_GSMRL_ENR | SCC_GSMRL_ENT);

	} else {
		smcp = &pquicc->smc_regs[idx];

		smcp->smc_smcm |= (SMCM_RX | SMCM_TX);
		smcp->smc_smcmr |= (SMCMR_REN | SMCMR_TEN);

		
		
		up = &pquicc->pram[info->state->port].scc.pothers.idma_smc.psmc.u;

		up->mrblr = RX_BUF_SIZE;
		up->max_idl = RX_BUF_SIZE;

		up->brkcr = 1;	
	}

	info->flags |= ASYNC_INITIALIZED;
	local_irq_restore(flags);
	return 0;
	
errout:
	local_irq_restore(flags);
	return retval;
}

static void shutdown(ser_info_t *info)
{
	unsigned long	flags;
	struct serial_state *state;
	int		idx;
	volatile struct smc_regs	*smcp;
	volatile struct scc_regs	*sccp;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	state = info->state;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....", info->line,
	       state->irq);
#endif
	
	local_irq_save(flags);

	idx = PORT_NUM(state->smc_scc_num);
	if (state->smc_scc_num & NUM_IS_SCC) {
		sccp = &pquicc->scc_regs[idx];
		sccp->scc_gsmr.w.low &= ~(SCC_GSMRL_ENR | SCC_GSMRL_ENT);
#ifdef CONFIG_SERIAL_CONSOLE
		if ((state - rs_table) != CONFIG_SERIAL_CONSOLE_PORT)
#endif
		sccp->scc_sccm &= ~(UART_SCCM_TX | UART_SCCM_RX);
	} else {
		smcp = &pquicc->smc_regs[idx];

		smcp->smc_smcm &= ~(SMCM_RX | SMCM_TX);
#ifdef CONFIG_SERIAL_CONSOLE
		if ((state - rs_table) != CONFIG_SERIAL_CONSOLE_PORT)
#endif
			smcp->smc_smcmr &= ~(SMCMR_REN | SMCMR_TEN);
	}
	
	if (info->port.tty)
		set_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	local_irq_restore(flags);
}

static void change_speed(ser_info_t *info)
{
	int	baud_rate;
	unsigned cflag, cval, scval, prev_mode;
	int	i, bits, sbits, idx;
	unsigned long	flags;
	struct serial_state *state;
	volatile struct smc_regs	*smcp;
	volatile struct scc_regs	*sccp;

	if (!info->port.tty || !info->port.tty->termios)
		return;
	cflag = info->port.tty->termios->c_cflag;

	state = info->state;

	cval = 0;
	scval = 0;

	
	switch (cflag & CSIZE) {
	      case CS5: bits = 5; break;
	      case CS6: bits = 6; break;
	      case CS7: bits = 7; break;
	      case CS8: bits = 8; break;
	      
	      default:  bits = 8; break;
	}
	sbits = bits - 5;

	if (cflag & CSTOPB) {
		cval |= SMCMR_SL;	
		scval |= SCU_PMSR_SL;
		bits++;
	}
	if (cflag & PARENB) {
		cval |= SMCMR_PEN;
		scval |= SCU_PMSR_PEN;
		bits++;
	}
	if (!(cflag & PARODD)) {
		cval |= SMCMR_PM_EVEN;
		scval |= (SCU_PMSR_REVP | SCU_PMSR_TEVP);
	}

	
	i = cflag & CBAUD;
	if (i >= (sizeof(baud_table)/sizeof(int)))
		baud_rate = 9600;
	else
		baud_rate = baud_table[i];

	info->timeout = (TX_BUF_SIZE*HZ*bits);
	info->timeout += HZ/50;		

#ifdef modem_control
	
	info->IER &= ~UART_IER_MSI;
	if (info->flags & ASYNC_HARDPPS_CD)
		info->IER |= UART_IER_MSI;
	if (cflag & CRTSCTS) {
		info->flags |= ASYNC_CTS_FLOW;
		info->IER |= UART_IER_MSI;
	} else
		info->flags &= ~ASYNC_CTS_FLOW;
	if (cflag & CLOCAL)
		info->flags &= ~ASYNC_CHECK_CD;
	else {
		info->flags |= ASYNC_CHECK_CD;
		info->IER |= UART_IER_MSI;
	}
	serial_out(info, UART_IER, info->IER);
#endif

	info->read_status_mask = (BD_SC_EMPTY | BD_SC_OV);
	if (I_INPCK(info->port.tty))
		info->read_status_mask |= BD_SC_FR | BD_SC_PR;
	if (I_BRKINT(info->port.tty) || I_PARMRK(info->port.tty))
		info->read_status_mask |= BD_SC_BR;
	
	info->ignore_status_mask = 0;
	if (I_IGNPAR(info->port.tty))
		info->ignore_status_mask |= BD_SC_PR | BD_SC_FR;
	if (I_IGNBRK(info->port.tty)) {
		info->ignore_status_mask |= BD_SC_BR;
		if (I_IGNPAR(info->port.tty))
			info->ignore_status_mask |= BD_SC_OV;
	}
	if ((cflag & CREAD) == 0)
	 info->read_status_mask &= ~BD_SC_EMPTY;
	 local_irq_save(flags);

	 bits++;
	 idx = PORT_NUM(state->smc_scc_num);
	 if (state->smc_scc_num & NUM_IS_SCC) {
         sccp = &pquicc->scc_regs[idx];
         sccp->scc_psmr = (sbits << 12) | scval;
     } else {
         smcp = &pquicc->smc_regs[idx];

		prev_mode = smcp->smc_smcmr;
		smcp->smc_smcmr = smcr_mk_clen(bits) | cval |  SMCMR_SM_UART;
		smcp->smc_smcmr |= (prev_mode & (SMCMR_REN | SMCMR_TEN));
	}

	m360_cpm_setbrg((state - rs_table), baud_rate);

	local_irq_restore(flags);
}

static void rs_360_put_char(struct tty_struct *tty, unsigned char ch)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	volatile QUICC_BD	*bdp;

	if (serial_paranoia_check(info, tty->name, "rs_put_char"))
		return 0;

	if (!tty)
		return 0;

	bdp = info->tx_cur;
	while (bdp->status & BD_SC_READY);

	
	*((char *)bdp->buf) = ch;
	bdp->length = 1;
	bdp->status |= BD_SC_READY;

	if (bdp->status & BD_SC_WRAP)
		bdp = info->tx_bd_base;
	else
		bdp++;

	info->tx_cur = (QUICC_BD *)bdp;
	return 1;

}

static int rs_360_write(struct tty_struct * tty,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	volatile QUICC_BD *bdp;

#ifdef CONFIG_KGDB
	 
	if (kgdb_output_string(buf, count))
		return ret;
#endif

	if (serial_paranoia_check(info, tty->name, "rs_write"))
		return 0;

	if (!tty) 
		return 0;

	bdp = info->tx_cur;

	while (1) {
		c = min(count, TX_BUF_SIZE);

		if (c <= 0)
			break;

		if (bdp->status & BD_SC_READY) {
			info->flags |= TX_WAKEUP;
			break;
		}

		
		memcpy((void *)bdp->buf, buf, c);

		bdp->length = c;
		bdp->status |= BD_SC_READY;

		buf += c;
		count -= c;
		ret += c;

		if (bdp->status & BD_SC_WRAP)
			bdp = info->tx_bd_base;
		else
			bdp++;
		info->tx_cur = (QUICC_BD *)bdp;
	}
	return ret;
}

static int rs_360_write_room(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	int	ret;

	if (serial_paranoia_check(info, tty->name, "rs_write_room"))
		return 0;

	if ((info->tx_cur->status & BD_SC_READY) == 0) {
		info->flags &= ~TX_WAKEUP;
		ret = TX_BUF_SIZE;
	}
	else {
		info->flags |= TX_WAKEUP;
		ret = 0;
	}
	return ret;
}

static int rs_360_chars_in_buffer(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->name, "rs_chars_in_buffer"))
		return 0;
	return 0;
}

static void rs_360_flush_buffer(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->name, "rs_flush_buffer"))
		return;

	tty_wakeup(tty);
	info->flags &= ~TX_WAKEUP;
}

static void rs_360_send_xchar(struct tty_struct *tty, char ch)
{
	volatile QUICC_BD	*bdp;

	ser_info_t *info = (ser_info_t *)tty->driver_data;

	if (serial_paranoia_check(info, tty->name, "rs_send_char"))
		return;

	bdp = info->tx_cur;
	while (bdp->status & BD_SC_READY);

	
	*((char *)bdp->buf) = ch;
	bdp->length = 1;
	bdp->status |= BD_SC_READY;

	if (bdp->status & BD_SC_WRAP)
		bdp = info->tx_bd_base;
	else
		bdp++;

	info->tx_cur = (QUICC_BD *)bdp;
}

static void rs_360_throttle(struct tty_struct * tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->name, "rs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		rs_360_send_xchar(tty, STOP_CHAR(tty));

#ifdef modem_control
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR &= ~UART_MCR_RTS;

	local_irq_disable();
	serial_out(info, UART_MCR, info->MCR);
	local_irq_enable();
#endif
}

static void rs_360_unthrottle(struct tty_struct * tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->name, "rs_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_360_send_xchar(tty, START_CHAR(tty));
	}
#ifdef modem_control
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR |= UART_MCR_RTS;
	local_irq_disable();
	serial_out(info, UART_MCR, info->MCR);
	local_irq_enable();
#endif
}


#ifdef maybe
/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space. 
 */
static int get_lsr_info(struct async_struct * info, unsigned int *value)
{
	unsigned char status;
	unsigned int result;

	local_irq_disable();
	status = serial_in(info, UART_LSR);
	local_irq_enable();
	result = ((status & UART_LSR_TEMT) ? TIOCSER_TEMT : 0);
	return put_user(result,value);
}
#endif

static int rs_360_tiocmget(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	unsigned int result = 0;
#ifdef modem_control
	unsigned char control, status;

	if (serial_paranoia_check(info, tty->name, __func__))
		return -ENODEV;

	if (tty->flags & (1 << TTY_IO_ERROR))
		return -EIO;

	control = info->MCR;
	local_irq_disable();
	status = serial_in(info, UART_MSR);
	local_irq_enable();
	result =  ((control & UART_MCR_RTS) ? TIOCM_RTS : 0)
		| ((control & UART_MCR_DTR) ? TIOCM_DTR : 0)
#ifdef TIOCM_OUT1
		| ((control & UART_MCR_OUT1) ? TIOCM_OUT1 : 0)
		| ((control & UART_MCR_OUT2) ? TIOCM_OUT2 : 0)
#endif
		| ((status  & UART_MSR_DCD) ? TIOCM_CAR : 0)
		| ((status  & UART_MSR_RI) ? TIOCM_RNG : 0)
		| ((status  & UART_MSR_DSR) ? TIOCM_DSR : 0)
		| ((status  & UART_MSR_CTS) ? TIOCM_CTS : 0);
#endif
	return result;
}

static int rs_360_tiocmset(struct tty_struct *tty,
			   unsigned int set, unsigned int clear)
{
#ifdef modem_control
	ser_info_t *info = (ser_info_t *)tty->driver_data;
 	unsigned int arg;

	if (serial_paranoia_check(info, tty->name, __func__))
		return -ENODEV;

	if (tty->flags & (1 << TTY_IO_ERROR))
		return -EIO;
	
 	if (set & TIOCM_RTS)
 		info->mcr |= UART_MCR_RTS;
 	if (set & TIOCM_DTR)
 		info->mcr |= UART_MCR_DTR;
	if (clear & TIOCM_RTS)
		info->MCR &= ~UART_MCR_RTS;
	if (clear & TIOCM_DTR)
		info->MCR &= ~UART_MCR_DTR;

#ifdef TIOCM_OUT1
	if (set & TIOCM_OUT1)
		info->MCR |= UART_MCR_OUT1;
	if (set & TIOCM_OUT2)
		info->MCR |= UART_MCR_OUT2;
	if (clear & TIOCM_OUT1)
		info->MCR &= ~UART_MCR_OUT1;
	if (clear & TIOCM_OUT2)
		info->MCR &= ~UART_MCR_OUT2;
#endif

	local_irq_disable();
	serial_out(info, UART_MCR, info->MCR);
	local_irq_enable();
#endif
	return 0;
}

static ushort	smc_chan_map[] = {
	CPM_CR_CH_SMC1,
	CPM_CR_CH_SMC2
};

static ushort	scc_chan_map[] = {
	CPM_CR_CH_SCC1,
	CPM_CR_CH_SCC2,
	CPM_CR_CH_SCC3,
	CPM_CR_CH_SCC4
};

static void begin_break(ser_info_t *info)
{
	volatile QUICC *cp;
	ushort	chan;
	int     idx;

	cp = pquicc;

	idx = PORT_NUM(info->state->smc_scc_num);
	if (info->state->smc_scc_num & NUM_IS_SCC)
		chan = scc_chan_map[idx];
	else
		chan = smc_chan_map[idx];

	cp->cp_cr = mk_cr_cmd(chan, CPM_CR_STOP_TX) | CPM_CR_FLG;
	while (cp->cp_cr & CPM_CR_FLG);
}

static void end_break(ser_info_t *info)
{
	volatile QUICC *cp;
	ushort	chan;
	int idx;

	cp = pquicc;

	idx = PORT_NUM(info->state->smc_scc_num);
	if (info->state->smc_scc_num & NUM_IS_SCC)
		chan = scc_chan_map[idx];
	else
		chan = smc_chan_map[idx];

	cp->cp_cr = mk_cr_cmd(chan, CPM_CR_RESTART_TX) | CPM_CR_FLG;
	while (cp->cp_cr & CPM_CR_FLG);
}

static void send_break(ser_info_t *info, unsigned int duration)
{
#ifdef SERIAL_DEBUG_SEND_BREAK
	printk("rs_send_break(%d) jiff=%lu...", duration, jiffies);
#endif
	begin_break(info);
	msleep_interruptible(duration);
	end_break(info);
#ifdef SERIAL_DEBUG_SEND_BREAK
	printk("done jiffies=%lu\n", jiffies);
#endif
}


static int rs_360_get_icount(struct tty_struct *tty,
				struct serial_icounter_struct *icount)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	struct async_icount cnow;

	local_irq_disable();
	cnow = info->state->icount;
	local_irq_enable();

	icount->cts = cnow.cts;
	icount->dsr = cnow.dsr;
	icount->rng = cnow.rng;
	icount->dcd = cnow.dcd;

	return 0;
}

static int rs_360_ioctl(struct tty_struct *tty,
		    unsigned int cmd, unsigned long arg)
{
	int error;
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	int retval;
	struct async_icount cnow; 
	 	
	struct serial_icounter_struct *p_cuser;	

	if (serial_paranoia_check(info, tty->name, "rs_ioctl"))
		return -ENODEV;

	if (cmd != TIOCMIWAIT) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}
	
	switch (cmd) {
		case TCSBRK:	
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
			if (!arg) {
				send_break(info, 250);	
				if (signal_pending(current))
					return -EINTR;
			}
			return 0;
		case TCSBRKP:	
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
			send_break(info, arg ? arg*100 : 250);
			if (signal_pending(current))
				return -EINTR;
			return 0;
		case TIOCSBRK:
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			begin_break(info);
			return 0;
		case TIOCCBRK:
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			end_break(info);
			return 0;
#ifdef maybe
		case TIOCSERGETLSR: 
			return get_lsr_info(info, (unsigned int *) arg);
#endif
		 case TIOCMIWAIT:
#ifdef modem_control
			local_irq_disable();
			
			cprev = info->state->icount;
			local_irq_enable();
			while (1) {
				interruptible_sleep_on(&info->delta_msr_wait);
				
				if (signal_pending(current))
					return -ERESTARTSYS;
				local_irq_disable();
				cnow = info->state->icount; 
				local_irq_enable();
				if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr && 
				    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
					return -EIO; 
				if ( ((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
				     ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
				     ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
				     ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
					return 0;
				}
				cprev = cnow;
			}
			
#else
			return 0;
#endif


		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void rs_360_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;

	change_speed(info);

#ifdef modem_control
	
	if ((old_termios->c_cflag & CBAUD) &&
	    !(tty->termios->c_cflag & CBAUD)) {
		info->MCR &= ~(UART_MCR_DTR|UART_MCR_RTS);
		local_irq_disable();
		serial_out(info, UART_MCR, info->MCR);
		local_irq_enable();
	}
	
	
	if (!(old_termios->c_cflag & CBAUD) &&
	    (tty->termios->c_cflag & CBAUD)) {
		info->MCR |= UART_MCR_DTR;
		if (!tty->hw_stopped ||
		    !(tty->termios->c_cflag & CRTSCTS)) {
			info->MCR |= UART_MCR_RTS;
		}
		local_irq_disable();
		serial_out(info, UART_MCR, info->MCR);
		local_irq_enable();
	}
	
	
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_360_start(tty);
	}
#endif

#if 0
	if (!(old_termios->c_cflag & CLOCAL) &&
	    (tty->termios->c_cflag & CLOCAL))
		wake_up_interruptible(&info->open_wait);
#endif
}

static void rs_360_close(struct tty_struct *tty, struct file * filp)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	
	struct serial_state *state;
	unsigned long	flags;
	int		idx;
	volatile struct smc_regs	*smcp;
	volatile struct scc_regs	*sccp;

	if (!info || serial_paranoia_check(info, tty->name, "rs_close"))
		return;

	state = info->state;
	
	local_irq_save(flags);
	
	if (tty_hung_up_p(filp)) {
		DBG_CNT("before DEC-hung");
		local_irq_restore(flags);
		return;
	}
	
#ifdef SERIAL_DEBUG_OPEN
	printk("rs_close ttys%d, count = %d\n", info->line, state->count);
#endif
	if ((tty->count == 1) && (state->count != 1)) {
		printk("rs_close: bad serial port count; tty->count is 1, "
		       "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		printk("rs_close: bad serial port count for ttys%d: %d\n",
		       info->line, state->count);
		state->count = 0;
	}
	if (state->count) {
		DBG_CNT("before DEC-2");
		local_irq_restore(flags);
		return;
	}
	info->flags |= ASYNC_CLOSING;
	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	info->read_status_mask &= ~BD_SC_EMPTY;
	if (info->flags & ASYNC_INITIALIZED) {

		idx = PORT_NUM(info->state->smc_scc_num);
		if (info->state->smc_scc_num & NUM_IS_SCC) {
			sccp = &pquicc->scc_regs[idx];
			sccp->scc_sccm &= ~UART_SCCM_RX;
			sccp->scc_gsmr.w.low &= ~SCC_GSMRL_ENR;
		} else {
			smcp = &pquicc->smc_regs[idx];
			smcp->smc_smcm &= ~SMCM_RX;
			smcp->smc_smcmr &= ~SMCMR_REN;
		}
		rs_360_wait_until_sent(tty, info->timeout);
	}
	shutdown(info);
	rs_360_flush_buffer(tty);
	tty_ldisc_flush(tty);		
	tty->closing = 0;
	info->event = 0;
	info->port.tty = NULL;
	if (info->blocked_open) {
		if (info->close_delay) {
			msleep_interruptible(jiffies_to_msecs(info->close_delay));
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	local_irq_restore(flags);
}

static void rs_360_wait_until_sent(struct tty_struct *tty, int timeout)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	unsigned long orig_jiffies, char_time;
	
	volatile QUICC_BD *bdp;
	
	if (serial_paranoia_check(info, tty->name, "rs_wait_until_sent"))
		return;

#ifdef maybe
	if (info->state->type == PORT_UNKNOWN)
		return;
#endif

	orig_jiffies = jiffies;
	char_time = 1;
	if (timeout)
		char_time = min(char_time, (unsigned long)timeout);
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("In rs_wait_until_sent(%d) check=%lu...", timeout, char_time);
	printk("jiff=%lu...", jiffies);
#endif

	do {
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
		printk("lsr = %d (jiff=%lu)...", lsr, jiffies);
#endif
		msleep_interruptible(jiffies_to_msecs(char_time));
		if (signal_pending(current))
			break;
		if (timeout && (time_after(jiffies, orig_jiffies + timeout)))
			break;
		bdp = info->tx_cur;
		if (bdp == info->tx_bd_base)
			bdp += (TX_NUM_FIFO-1);
		else
			bdp--;
	} while (bdp->status & BD_SC_READY);
	current->state = TASK_RUNNING;
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("lsr = %d (jiff=%lu)...done\n", lsr, jiffies);
#endif
}

static void rs_360_hangup(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	struct serial_state *state = info->state;
	
	if (serial_paranoia_check(info, tty->name, "rs_hangup"))
		return;

	state = info->state;
	
	rs_360_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	state->count = 0;
	info->flags &= ~ASYNC_NORMAL_ACTIVE;
	info->port.tty = NULL;
	wake_up_interruptible(&info->open_wait);
}

static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   ser_info_t *info)
{
#ifdef DO_THIS_LATER
	DECLARE_WAITQUEUE(wait, current);
#endif
	struct serial_state *state = info->state;
	int		retval;
	int		do_clocal = 0;

	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & ASYNC_HUP_NOTIFY)
			return -EAGAIN;
		else
			return -ERESTARTSYS;
#else
		return -EAGAIN;
#endif
	}

	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR)) ||
	    !(info->state->smc_scc_num & NUM_IS_SCC)) {
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (tty->termios->c_cflag & CLOCAL)
		do_clocal = 1;
	
	retval = 0;
#ifdef DO_THIS_LATER
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttys%d, count = %d\n",
	       state->line, state->count);
#endif
	local_irq_disable();
	if (!tty_hung_up_p(filp)) 
		state->count--;
	local_irq_enable();
	info->blocked_open++;
	while (1) {
		local_irq_disable();
		if (tty->termios->c_cflag & CBAUD)
			serial_out(info, UART_MCR,
				   serial_inp(info, UART_MCR) |
				   (UART_MCR_DTR | UART_MCR_RTS));
		local_irq_enable();
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CLOSING) &&
		    (do_clocal || (serial_in(info, UART_MSR) &
				   UART_MSR_DCD)))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttys%d, count = %d\n",
		       info->line, state->count);
#endif
		tty_unlock();
		schedule();
		tty_lock();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
	       info->line, state->count);
#endif
#endif 
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

static int get_async_struct(int line, ser_info_t **ret_info)
{
	struct serial_state *sstate;

	sstate = rs_table + line;
	if (sstate->info) {
		sstate->count++;
		*ret_info = (ser_info_t *)sstate->info;
		return 0;
	}
	else {
		return -ENOMEM;
	}
}

static int rs_360_open(struct tty_struct *tty, struct file * filp)
{
	ser_info_t	*info;
	int 		retval, line;

	line = tty->index;
	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;
	retval = get_async_struct(line, &info);
	if (retval)
		return retval;
	if (serial_paranoia_check(info, tty->name, "rs_open"))
		return -ENODEV;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s, count = %d\n", tty->name, info->state->count);
#endif
	tty->driver_data = info;
	info->port.tty = tty;

	retval = startup(info);
	if (retval)
		return retval;

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("rs_open returning after block_til_ready with %d\n",
		       retval);
#endif
		return retval;
	}

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s successful...", tty->name);
#endif
	return 0;
}


static inline int line_info(char *buf, struct serial_state *state)
{
#ifdef notdef
	struct async_struct *info = state->info, scr_info;
	char	stat_buf[30], control, status;
#endif
	int	ret;

	ret = sprintf(buf, "%d: uart:%s port:%X irq:%d",
		      state->line,
		      (state->smc_scc_num & NUM_IS_SCC) ? "SCC" : "SMC",
		      (unsigned int)(state->port), state->irq);

	if (!state->port || (state->type == PORT_UNKNOWN)) {
		ret += sprintf(buf+ret, "\n");
		return ret;
	}

#ifdef notdef
	if (!info) {
		info = &scr_info;	

		info->magic = SERIAL_MAGIC;
		info->port = state->port;
		info->flags = state->flags;
		info->quot = 0;
		info->port.tty = NULL;
	}
	local_irq_disable();
	status = serial_in(info, UART_MSR);
	control = info ? info->MCR : serial_in(info, UART_MCR);
	local_irq_enable();
	
	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (control & UART_MCR_RTS)
		strcat(stat_buf, "|RTS");
	if (status & UART_MSR_CTS)
		strcat(stat_buf, "|CTS");
	if (control & UART_MCR_DTR)
		strcat(stat_buf, "|DTR");
	if (status & UART_MSR_DSR)
		strcat(stat_buf, "|DSR");
	if (status & UART_MSR_DCD)
		strcat(stat_buf, "|CD");
	if (status & UART_MSR_RI)
		strcat(stat_buf, "|RI");

	if (info->quot) {
		ret += sprintf(buf+ret, " baud:%d",
			       state->baud_base / info->quot);
	}

	ret += sprintf(buf+ret, " tx:%d rx:%d",
		      state->icount.tx, state->icount.rx);

	if (state->icount.frame)
		ret += sprintf(buf+ret, " fe:%d", state->icount.frame);
	
	if (state->icount.parity)
		ret += sprintf(buf+ret, " pe:%d", state->icount.parity);
	
	if (state->icount.brk)
		ret += sprintf(buf+ret, " brk:%d", state->icount.brk);	

	if (state->icount.overrun)
		ret += sprintf(buf+ret, " oe:%d", state->icount.overrun);

	ret += sprintf(buf+ret, " %s\n", stat_buf+1);
#endif
	return ret;
}

int rs_360_read_proc(char *page, char **start, off_t off, int count,
		 int *eof, void *data)
{
	int i, len = 0;
	off_t	begin = 0;

	len += sprintf(page, "serinfo:1.0 driver:%s\n", serial_version);
	for (i = 0; i < NR_PORTS && len < 4000; i++) {
		len += line_info(page + len, &rs_table[i]);
		if (len+begin > off+count)
			goto done;
		if (len+begin < off) {
			begin += len;
			len = 0;
		}
	}
	*eof = 1;
done:
	if (off >= len+begin)
		return 0;
	*start = page + (begin-off);
	return ((count < begin+len-off) ? count : begin+len-off);
}


static _INLINE_ void show_serial_version(void)
{
 	printk(KERN_INFO "%s version %s\n", serial_name, serial_version);
}



#ifdef CONFIG_SERIAL_CONSOLE

static void my_console_write(int idx, const char *s,
				unsigned count)
{
	struct		serial_state	*ser;
	ser_info_t		*info;
	unsigned		i;
	QUICC_BD		*bdp, *bdbase;
	volatile struct smc_uart_pram	*up;
	volatile	u_char		*cp;

	ser = rs_table + idx;


	if ((info = (ser_info_t *)ser->info) != NULL) {
		bdp = info->tx_cur;
		bdbase = info->tx_bd_base;
	}
	else {
		
		up = &pquicc->pram[ser->port].scc.pothers.idma_smc.psmc.u;

		bdp = bdbase = (QUICC_BD *)((uint)pquicc + (uint)up->tbase);
	}


	for (i = 0; i < count; i++, s++) {
		while (bdp->status & BD_SC_READY);

		cp = bdp->buf;
		*cp = *s;
		
		bdp->length = 1;
		bdp->status |= BD_SC_READY;

		if (bdp->status & BD_SC_WRAP)
			bdp = bdbase;
		else
			bdp++;

		
		if (*s == 10) {
			while (bdp->status & BD_SC_READY);
			
			cp = bdp->buf;
			*cp = 13;
			bdp->length = 1;
			bdp->status |= BD_SC_READY;

			if (bdp->status & BD_SC_WRAP) {
				bdp = bdbase;
			}
			else {
				bdp++;
			}
		}
	}

	while (bdp->status & BD_SC_READY);

	if (info)
		info->tx_cur = (QUICC_BD *)bdp;
}

static void serial_console_write(struct console *c, const char *s,
				unsigned count)
{
#ifdef CONFIG_KGDB
	 
	if (kgdb_output_string(s, count))
		return;
#endif
	my_console_write(c->index, s, count);
}









#ifdef CONFIG_XMON
int
xmon_360_write(const char *s, unsigned count)
{
	my_console_write(0, s, count);
	return(count);
}
#endif

#ifdef CONFIG_KGDB
void
putDebugChar(char ch)
{
	my_console_write(0, &ch, 1);
}
#endif

static int my_console_wait_key(int idx, int xmon, char *obuf)
{
	struct serial_state		*ser;
	u_char			c, *cp;
	ser_info_t		*info;
	QUICC_BD		*bdp;
	volatile struct smc_uart_pram	*up;
	int				i;

	ser = rs_table + idx;

	if ((info = (ser_info_t *)ser->info))
		bdp = info->rx_cur;
	else
		
		bdp = (QUICC_BD *)((uint)pquicc + (uint)up->tbase);

	
	up = &pquicc->pram[info->state->port].scc.pothers.idma_smc.psmc.u;

	if (!xmon) {
		while (bdp->status & BD_SC_EMPTY);
	}
	else {
		if (bdp->status & BD_SC_EMPTY)
			return -1;
	}

	cp = (char *)bdp->buf;

	if (obuf) {
		i = c = bdp->length;
		while (i-- > 0)
			*obuf++ = *cp++;
	}
	else {
		c = *cp;
	}
	bdp->status |= BD_SC_EMPTY;

	if (info) {
		if (bdp->status & BD_SC_WRAP) {
			bdp = info->rx_bd_base;
		}
		else {
			bdp++;
		}
		info->rx_cur = (QUICC_BD *)bdp;
	}

	return((int)c);
}

static int serial_console_wait_key(struct console *co)
{
	return(my_console_wait_key(co->index, 0, NULL));
}

#ifdef CONFIG_XMON
int
xmon_360_read_poll(void)
{
	return(my_console_wait_key(0, 1, NULL));
}

int
xmon_360_read_char(void)
{
	return(my_console_wait_key(0, 0, NULL));
}
#endif

#ifdef CONFIG_KGDB
static char kgdb_buf[RX_BUF_SIZE], *kgdp;
static int kgdb_chars;

unsigned char
getDebugChar(void)
{
	if (kgdb_chars <= 0) {
		kgdb_chars = my_console_wait_key(0, 0, kgdb_buf);
		kgdp = kgdb_buf;
	}
	kgdb_chars--;

	return(*kgdp++);
}

void kgdb_interruptible(int state)
{
}
void kgdb_map_scc(void)
{
	struct		serial_state *ser;
	uint		mem_addr;
	volatile	QUICC_BD		*bdp;
	volatile	smc_uart_t	*up;

	cpmp = (cpm360_t *)&(((immap_t *)IMAP_ADDR)->im_cpm);


	ser = rs_table;

	up = (smc_uart_t *)&cpmp->cp_dparam[ser->port];

	mem_addr = (uint)(&cpmp->cp_dpmem[0x1000]);

	bdp = (QUICC_BD *)&cpmp->cp_dpmem[up->smc_rbase];
	bdp->buf = mem_addr;

	bdp = (QUICC_BD *)&cpmp->cp_dpmem[up->smc_tbase];
	bdp->buf = mem_addr+RX_BUF_SIZE;

	up->smc_mrblr = RX_BUF_SIZE;		
	up->smc_maxidl = RX_BUF_SIZE;
}
#endif

static struct tty_struct *serial_console_device(struct console *c, int *index)
{
	*index = c->index;
	return serial_driver;
}


struct console sercons = {
 	.name		= "ttyS",
 	.write		= serial_console_write,
 	.device		= serial_console_device,
 	.wait_key	= serial_console_wait_key,
 	.setup		= serial_console_setup,
 	.flags		= CON_PRINTBUFFER,
 	.index		= CONFIG_SERIAL_CONSOLE_PORT, 
};



long console_360_init(long kmem_start, long kmem_end)
{
	register_console(&sercons);
	return kmem_start;
}

#endif

static	int	baud_idx;

static const struct tty_operations rs_360_ops = {
	.owner = THIS_MODULE,
	.open = rs_360_open,
	.close = rs_360_close,
	.write = rs_360_write,
	.put_char = rs_360_put_char,
	.write_room = rs_360_write_room,
	.chars_in_buffer = rs_360_chars_in_buffer,
	.flush_buffer = rs_360_flush_buffer,
	.ioctl = rs_360_ioctl,
	.throttle = rs_360_throttle,
	.unthrottle = rs_360_unthrottle,
	
	.set_termios = rs_360_set_termios,
	.stop = rs_360_stop,
	.start = rs_360_start,
	.hangup = rs_360_hangup,
	
	
	.tiocmget = rs_360_tiocmget,
	.tiocmset = rs_360_tiocmset,
	.get_icount = rs_360_get_icount,
};

static int __init rs_360_init(void)
{
	struct serial_state * state;
	ser_info_t	*info;
	void       *mem_addr;
	uint 		dp_addr, iobits;
	int		    i, j, idx;
	ushort		chan;
	QUICC_BD	*bdp;
	volatile	QUICC		*cp;
	volatile	struct smc_regs	*sp;
	volatile	struct smc_uart_pram	*up;
	volatile	struct scc_regs	*scp;
	volatile	struct uart_pram	*sup;
	
	
	serial_driver = alloc_tty_driver(NR_PORTS);
	if (!serial_driver)
		return -1;

	show_serial_version();

	serial_driver->name = "ttyS";
	serial_driver->major = TTY_MAJOR;
	serial_driver->minor_start = 64;
	serial_driver->type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver->subtype = SERIAL_TYPE_NORMAL;
	serial_driver->init_termios = tty_std_termios;
	serial_driver->init_termios.c_cflag =
		baud_idx | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver->flags = TTY_DRIVER_REAL_RAW;
	tty_set_operations(serial_driver, &rs_360_ops);
	
	if (tty_register_driver(serial_driver))
		panic("Couldn't register serial driver\n");

	cp = pquicc;	
		


	cp->pio_papar |= 0x00fc;
	cp->pio_padir &= ~0x00fc;
	






	cp->si_sicr &= ~0x00ffff00;
	cp->si_sicr |=  0x001b1200;

#ifdef CONFIG_PP04
	immap->im_ioport.iop_pcdir |= 0x000c;
	immap->im_ioport.iop_pcpar &= ~0x000c;
	immap->im_ioport.iop_pcdat &= ~0x000c;

	cp->cp_pbpar &= ~0x6000;
	cp->cp_pbdat &= ~0x6000;
#endif

	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
		state->magic = SSTATE_MAGIC;
		state->line = i;
		state->type = PORT_UNKNOWN;
		state->custom_divisor = 0;
		state->close_delay = 5*HZ/10;
		state->closing_wait = 30*HZ;
		state->icount.cts = state->icount.dsr = 
			state->icount.rng = state->icount.dcd = 0;
		state->icount.rx = state->icount.tx = 0;
		state->icount.frame = state->icount.parity = 0;
		state->icount.overrun = state->icount.brk = 0;
		printk(KERN_INFO "ttyS%d at irq 0x%02x is an %s\n",
		       i, (unsigned int)(state->irq),
		       (state->smc_scc_num & NUM_IS_SCC) ? "SCC" : "SMC");

#ifdef CONFIG_SERIAL_CONSOLE
		if (i == CONFIG_SERIAL_CONSOLE_PORT)
			mdelay(8);




#endif
		
		info = &quicc_ser_info[i];
		if (info) {
			memset (info, 0, sizeof(ser_info_t));
			info->magic = SERIAL_MAGIC;
			info->line = i;
			info->flags = state->flags;
			INIT_WORK(&info->tqueue, do_softint, info);
			INIT_WORK(&info->tqueue_hangup, do_serial_hangup, info);
			init_waitqueue_head(&info->open_wait);
			init_waitqueue_head(&info->close_wait);
			info->state = state;
			state->info = (struct async_struct *)info;

			dp_addr = m360_cpm_dpalloc(sizeof(QUICC_BD) * RX_NUM_FIFO);

			
			
			mem_addr = &rx_buf_pool[i * RX_NUM_FIFO * RX_BUF_SIZE];

			bdp = (QUICC_BD *)((uint)pquicc + dp_addr);
			info->rx_cur = info->rx_bd_base = bdp;

			
			for (j=0; j<(RX_NUM_FIFO-1); j++) {
				bdp->buf = &rx_buf_pool[(i * RX_NUM_FIFO + j ) * RX_BUF_SIZE];
				bdp->status = BD_SC_EMPTY | BD_SC_INTRPT;
				mem_addr += RX_BUF_SIZE;
				bdp++;
			}
			bdp->buf = &rx_buf_pool[(i * RX_NUM_FIFO + j ) * RX_BUF_SIZE];
			bdp->status = BD_SC_WRAP | BD_SC_EMPTY | BD_SC_INTRPT;


			idx = PORT_NUM(info->state->smc_scc_num);
			if (info->state->smc_scc_num & NUM_IS_SCC) {

#if defined (CONFIG_UCQUICC) && 1
				
				sipex_mode_bits &= ~(uint)SIPEX_MODE(idx,0x0f); 
				sipex_mode_bits |= (uint)SIPEX_MODE(idx,0x02);
				*(uint *)_periph_base = sipex_mode_bits;
				
#endif
			}

			dp_addr = m360_cpm_dpalloc(sizeof(QUICC_BD) * TX_NUM_FIFO);

			
			
			mem_addr = &tx_buf_pool[i * TX_NUM_FIFO * TX_BUF_SIZE];

			
			bdp = (QUICC_BD *)((uint)pquicc + dp_addr);
			info->tx_cur = info->tx_bd_base = (QUICC_BD *)bdp;

			
			for (j=0; j<(TX_NUM_FIFO-1); j++) {
				bdp->buf = &tx_buf_pool[(i * TX_NUM_FIFO + j ) * TX_BUF_SIZE];
				bdp->status = BD_SC_INTRPT;
				mem_addr += TX_BUF_SIZE;
				bdp++;
			}
			bdp->buf = &tx_buf_pool[(i * TX_NUM_FIFO + j ) * TX_BUF_SIZE];
			bdp->status = (BD_SC_WRAP | BD_SC_INTRPT);

			if (info->state->smc_scc_num & NUM_IS_SCC) {
				scp = &pquicc->scc_regs[idx];
				sup = &pquicc->pram[info->state->port].scc.pscc.u;
				sup->rbase = dp_addr;
				sup->tbase = dp_addr;

				sup->rfcr = SMC_EB;
				sup->tfcr = SMC_EB;

				sup->mrblr = 1;
				sup->max_idl = 0;
				sup->brkcr = 1;
				sup->parec = 0;
				sup->frmer = 0;
				sup->nosec = 0;
				sup->brkec = 0;
				sup->uaddr1 = 0;
				sup->uaddr2 = 0;
				sup->toseq = 0;
				{
					int i;
					for (i=0;i<8;i++)
						sup->cc[i] = 0x8000;
				}
				sup->rccm = 0xc0ff;

				chan = scc_chan_map[idx];

				
				cp->cp_cr = mk_cr_cmd(chan, CPM_CR_INIT_TRX) | CPM_CR_FLG;
				while (cp->cp_cr & CPM_CR_FLG);

				scp->scc_gsmr.w.high = 0;
				scp->scc_gsmr.w.low = 
					(SCC_GSMRL_MODE_UART | SCC_GSMRL_TDCR_16 | SCC_GSMRL_RDCR_16);

				scp->scc_sccm = 0;
				scp->scc_scce = 0xffff;
				scp->scc_dsr = 0x7e7e;
				scp->scc_psmr = 0x3000;

#ifdef CONFIG_SERIAL_CONSOLE
				if (i == CONFIG_SERIAL_CONSOLE_PORT)
					scp->scc_gsmr.w.low |= (SCC_GSMRL_ENR | SCC_GSMRL_ENT);
#endif
			}
			else {
				up = &pquicc->pram[info->state->port].scc.pothers.idma_smc.psmc.u;
				up->rbase = dp_addr;

				iobits = 0xc0 << (idx * 4);
				cp->pip_pbpar |= iobits;
				cp->pip_pbdir &= ~iobits;
				cp->pip_pbodr &= ~iobits;


				cp->si_simode &= ~(0xffff << (idx * 16));
				cp->si_simode |= (i << ((idx * 16) + 12));

				up->tbase = dp_addr;

				up->rfcr = SMC_EB;
				up->tfcr = SMC_EB;

				up->mrblr = 1;
				up->max_idl = 0;
				up->brkcr = 1;

				chan = smc_chan_map[idx];

				cp->cp_cr = mk_cr_cmd(chan,
									  CPM_CR_INIT_TRX) | CPM_CR_FLG;
#ifdef CONFIG_SERIAL_CONSOLE
				if (i == CONFIG_SERIAL_CONSOLE_PORT)
					printk("");
#endif
				while (cp->cp_cr & CPM_CR_FLG);

				sp = &cp->smc_regs[idx];
				sp->smc_smcmr = smcr_mk_clen(9) | SMCMR_SM_UART;

				sp->smc_smcm = 0;
				sp->smc_smce = 0xff;

#ifdef CONFIG_SERIAL_CONSOLE
				if (i == CONFIG_SERIAL_CONSOLE_PORT)
					sp->smc_smcmr |= SMCMR_REN | SMCMR_TEN;
#endif
			}

			
			
			request_irq(state->irq, rs_360_interrupt, 0, "ttyS",
				    (void *)info);

			m360_cpm_setbrg(i, baud_table[baud_idx]);

		}
	}

	return 0;
}
module_init(rs_360_init);

int serial_console_setup( struct console *co, char *options)
{
	struct		serial_state	*ser;
	uint		mem_addr, dp_addr, bidx, idx, iobits;
	ushort		chan;
	QUICC_BD	*bdp;
	volatile	QUICC			*cp;
	volatile	struct smc_regs	*sp;
	volatile	struct scc_regs	*scp;
	volatile	struct smc_uart_pram	*up;
	volatile	struct uart_pram		*sup;


 

 	for (bidx = 0; bidx < (sizeof(baud_table) / sizeof(int)); bidx++)
	 
 		if (CONSOLE_BAUDRATE == baud_table[bidx])
			break;

	
	baud_idx = bidx;

	ser = rs_table + CONFIG_SERIAL_CONSOLE_PORT;

	cp = pquicc;	

	idx = PORT_NUM(ser->smc_scc_num);
	if (ser->smc_scc_num & NUM_IS_SCC) {

		
		
	}
	else {
		iobits = 0xc0 << (idx * 4);
		cp->pip_pbpar |= iobits;
		cp->pip_pbdir &= ~iobits;
		cp->pip_pbodr &= ~iobits;

		cp->si_simode &= ~(0xffff << (idx * 16));
		cp->si_simode |= (idx << ((idx * 16) + 12));
	}


	dp_addr = m360_cpm_dpalloc(sizeof(QUICC_BD) * CONSOLE_NUM_FIFO);

	
	mem_addr = (uint)console_fifos;


	
	bdp = (QUICC_BD *)((uint)pquicc + dp_addr);
	bdp->buf = (char *)mem_addr;
	(bdp+1)->buf = (char *)(mem_addr+4);

	bdp->status = BD_SC_EMPTY | BD_SC_WRAP;
	(bdp+1)->status = BD_SC_WRAP;

	if (ser->smc_scc_num & NUM_IS_SCC) {
		scp = &cp->scc_regs[idx];
		
		sup = &pquicc->pram[ser->port].scc.pscc.u;

		sup->rbase = dp_addr;
		sup->tbase = dp_addr + sizeof(QUICC_BD);

		sup->rfcr = SMC_EB;
		sup->tfcr = SMC_EB;

		sup->mrblr = 1;
		sup->max_idl = 0;
		sup->brkcr = 1;
		sup->parec = 0;
		sup->frmer = 0;
		sup->nosec = 0;
		sup->brkec = 0;
		sup->uaddr1 = 0;
		sup->uaddr2 = 0;
		sup->toseq = 0;
		{
			int i;
			for (i=0;i<8;i++)
				sup->cc[i] = 0x8000;
		}
		sup->rccm = 0xc0ff;

		chan = scc_chan_map[idx];

		cp->cp_cr = mk_cr_cmd(chan, CPM_CR_INIT_TRX) | CPM_CR_FLG;
		while (cp->cp_cr & CPM_CR_FLG);

		scp->scc_gsmr.w.high = 0;
		scp->scc_gsmr.w.low = 
			(SCC_GSMRL_MODE_UART | SCC_GSMRL_TDCR_16 | SCC_GSMRL_RDCR_16);

		scp->scc_sccm = 0;
		scp->scc_scce = 0xffff;
		scp->scc_dsr = 0x7e7e;
		scp->scc_psmr = 0x3000;

		scp->scc_gsmr.w.low |= (SCC_GSMRL_ENR | SCC_GSMRL_ENT);

	}
	else {
		
		up = &pquicc->pram[ser->port].scc.pothers.idma_smc.psmc.u;

		up->rbase = dp_addr;	
		up->tbase = dp_addr+sizeof(QUICC_BD);	
		up->rfcr = SMC_EB;
		up->tfcr = SMC_EB;

		up->mrblr = 1;		
		up->max_idl = 0;		

		chan = smc_chan_map[idx];
		cp->cp_cr = mk_cr_cmd(chan, CPM_CR_INIT_TRX) | CPM_CR_FLG;
		while (cp->cp_cr & CPM_CR_FLG);

		sp = &cp->smc_regs[idx];
		sp->smc_smcmr = smcr_mk_clen(9) |  SMCMR_SM_UART;

		sp->smc_smcmr |= SMCMR_REN | SMCMR_TEN;
	}

	
	m360_cpm_setbrg((ser - rs_table), CONSOLE_BAUDRATE);

	return 0;
}

