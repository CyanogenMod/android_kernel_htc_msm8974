
#ifndef	__ALTJUART_H
#define	__ALTJUART_H

#define ALTERA_JTAGUART_MAJOR	204
#define ALTERA_JTAGUART_MINOR	186

struct altera_jtaguart_platform_uart {
	unsigned long mapbase;	
	unsigned int irq;	
};

#endif 
