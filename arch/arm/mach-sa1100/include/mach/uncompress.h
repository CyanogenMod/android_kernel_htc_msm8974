
#include "hardware.h"


#define UART(x)		(*(volatile unsigned long *)(serial_port + (x)))

static void putc(int c)
{
	unsigned long serial_port;

	do {
		serial_port = _Ser3UTCR0;
		if (UART(UTCR3) & UTCR3_TXE) break;
		serial_port = _Ser1UTCR0;
		if (UART(UTCR3) & UTCR3_TXE) break;
		serial_port = _Ser2UTCR0;
		if (UART(UTCR3) & UTCR3_TXE) break;
		return;
	} while (0);

	
	while (!(UART(UTSR1) & UTSR1_TNF))
		barrier();

	
	UART(UTDR) = c;
}

static inline void flush(void)
{
}

#define arch_decomp_setup()
#define arch_decomp_wdog()
