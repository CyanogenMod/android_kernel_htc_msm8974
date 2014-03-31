#ifndef _SPEAKUP_SERIAL_H
#define _SPEAKUP_SERIAL_H

#include <linux/serial.h>	
#include <linux/serial_reg.h>	
#ifndef __sparc__
#include <asm/serial.h>
#endif

struct old_serial_port {
	unsigned int uart; 
	unsigned int baud_base;
	unsigned int port;
	unsigned int irq;
	unsigned int flags; 
};

#define SPK_SERIAL_TIMEOUT 100000
#define SPK_XMITR_TIMEOUT 100000
#define SPK_CTS_TIMEOUT 100000
#define SPK_LO_TTY 0
#define SPK_HI_TTY 3
#define NUM_DISABLE_TIMEOUTS 3
#define SPK_TIMEOUT 100
#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

#define spk_serial_tx_busy() ((inb(speakup_info.port_tts + UART_LSR) & BOTH_EMPTY) != BOTH_EMPTY)

#ifndef BASE_BAUD
#define BASE_BAUD (1843200 / 16)
#endif
#ifndef STD_COM_FLAGS
#ifdef CONFIG_SERIAL_DETECT_IRQ
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ)
#define STD_COM4_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_AUTO_IRQ)
#else
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)
#define STD_COM4_FLAGS ASYNC_BOOT_AUTOCONF
#endif
#endif
#ifndef SERIAL_PORT_DFNS
#define SERIAL_PORT_DFNS			\
				\
	{ 0, BASE_BAUD, 0x3F8, 4, STD_COM_FLAGS },		\
	{ 0, BASE_BAUD, 0x2F8, 3, STD_COM_FLAGS },		\
	{ 0, BASE_BAUD, 0x3E8, 4, STD_COM_FLAGS },		\
	{ 0, BASE_BAUD, 0x2E8, 3, STD_COM4_FLAGS },	
#endif
#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ
#endif

#endif
