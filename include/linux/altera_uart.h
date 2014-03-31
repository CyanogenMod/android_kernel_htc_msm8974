
#ifndef	__ALTUART_H
#define	__ALTUART_H

struct altera_uart_platform_uart {
	unsigned long mapbase;	
	unsigned int irq;	
	unsigned int uartclk;	
	unsigned int bus_shift;	
};

#endif 
