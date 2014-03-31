
/*
 *	mcfuart.h -- ColdFire internal UART support defines.
 *
 *	(C) Copyright 1999-2003, Greg Ungerer (gerg@snapgear.com)
 * 	(C) Copyright 2000, Lineo Inc. (www.lineo.com) 
 */

#ifndef	mcfuart_h
#define	mcfuart_h

#include <linux/serial_core.h>
#include <linux/platform_device.h>

struct mcf_platform_uart {
	unsigned long	mapbase;	
	void __iomem	*membase;	
	unsigned int	irq;		
	unsigned int	uartclk;	
};

#define	MCFUART_UMR		0x00		
#define	MCFUART_USR		0x04		
#define	MCFUART_UCSR		0x04		
#define	MCFUART_UCR		0x08		
#define	MCFUART_URB		0x0c		
#define	MCFUART_UTB		0x0c		
#define	MCFUART_UIPCR		0x10		
#define	MCFUART_UACR		0x10		
#define	MCFUART_UISR		0x14		
#define	MCFUART_UIMR		0x14		
#define	MCFUART_UBG1		0x18		
#define	MCFUART_UBG2		0x1c		
#ifdef	CONFIG_M5272
#define	MCFUART_UTF		0x28		
#define	MCFUART_URF		0x2c		
#define	MCFUART_UFPD		0x30		
#endif
#if defined(CONFIG_M5206) || defined(CONFIG_M5206e) || \
        defined(CONFIG_M5249) || defined(CONFIG_M5307) || \
        defined(CONFIG_M5407)
#define	MCFUART_UIVR		0x30		
#endif
#define	MCFUART_UIPR		0x34		
#define	MCFUART_UOP1		0x38		
#define	MCFUART_UOP0		0x3c		


#define	MCFUART_MR1_RXRTS	0x80		
#define	MCFUART_MR1_RXIRQFULL	0x40		
#define	MCFUART_MR1_RXIRQRDY	0x00		
#define	MCFUART_MR1_RXERRBLOCK	0x20		
#define	MCFUART_MR1_RXERRCHAR	0x00		

#define	MCFUART_MR1_PARITYNONE	0x10		
#define	MCFUART_MR1_PARITYEVEN	0x00		
#define	MCFUART_MR1_PARITYODD	0x04		
#define	MCFUART_MR1_PARITYSPACE	0x08		
#define	MCFUART_MR1_PARITYMARK	0x0c		

#define	MCFUART_MR1_CS5		0x00		
#define	MCFUART_MR1_CS6		0x01		
#define	MCFUART_MR1_CS7		0x02		
#define	MCFUART_MR1_CS8		0x03		

#define	MCFUART_MR2_LOOPBACK	0x80		
#define	MCFUART_MR2_REMOTELOOP	0xc0		
#define	MCFUART_MR2_AUTOECHO	0x40		
#define	MCFUART_MR2_TXRTS	0x20		
#define	MCFUART_MR2_TXCTS	0x10		

#define	MCFUART_MR2_STOP1	0x07		
#define	MCFUART_MR2_STOP15	0x08		
#define	MCFUART_MR2_STOP2	0x0f		

#define	MCFUART_USR_RXBREAK	0x80		
#define	MCFUART_USR_RXFRAMING	0x40		
#define	MCFUART_USR_RXPARITY	0x20		
#define	MCFUART_USR_RXOVERRUN	0x10		
#define	MCFUART_USR_TXEMPTY	0x08		
#define	MCFUART_USR_TXREADY	0x04		
#define	MCFUART_USR_RXFULL	0x02		
#define	MCFUART_USR_RXREADY	0x01		

#define	MCFUART_USR_RXERR	(MCFUART_USR_RXBREAK | MCFUART_USR_RXFRAMING | \
				MCFUART_USR_RXPARITY | MCFUART_USR_RXOVERRUN)

#define	MCFUART_UCSR_RXCLKTIMER	0xd0		
#define	MCFUART_UCSR_RXCLKEXT16	0xe0		
#define	MCFUART_UCSR_RXCLKEXT1	0xf0		

#define	MCFUART_UCSR_TXCLKTIMER	0x0d		
#define	MCFUART_UCSR_TXCLKEXT16	0x0e		
#define	MCFUART_UCSR_TXCLKEXT1	0x0f		

#define	MCFUART_UCR_CMDNULL		0x00	
#define	MCFUART_UCR_CMDRESETMRPTR	0x10	
#define	MCFUART_UCR_CMDRESETRX		0x20	
#define	MCFUART_UCR_CMDRESETTX		0x30	
#define	MCFUART_UCR_CMDRESETERR		0x40	
#define	MCFUART_UCR_CMDRESETBREAK	0x50	
#define	MCFUART_UCR_CMDBREAKSTART	0x60	
#define	MCFUART_UCR_CMDBREAKSTOP	0x70	

#define	MCFUART_UCR_TXNULL	0x00		
#define	MCFUART_UCR_TXENABLE	0x04		
#define	MCFUART_UCR_TXDISABLE	0x08		
#define	MCFUART_UCR_RXNULL	0x00		
#define	MCFUART_UCR_RXENABLE	0x01		
#define	MCFUART_UCR_RXDISABLE	0x02		

#define	MCFUART_UIPCR_CTSCOS	0x10		
#define	MCFUART_UIPCR_CTS	0x01		

#define	MCFUART_UIPR_CTS	0x01		

#define	MCFUART_UOP_RTS		0x01		

#define	MCFUART_UACR_IEC	0x01		

#define	MCFUART_UIR_COS		0x80		
#define	MCFUART_UIR_DELTABREAK	0x04		
#define	MCFUART_UIR_RXREADY	0x02		
#define	MCFUART_UIR_TXREADY	0x01		

#ifdef	CONFIG_M5272
#define	MCFUART_UTF_TXB		0x1f		
#define	MCFUART_UTF_FULL	0x20		
#define	MCFUART_UTF_TXS		0xc0		

#define	MCFUART_URF_RXB		0x1f		
#define	MCFUART_URF_FULL	0x20		
#define	MCFUART_URF_RXS		0xc0		
#endif

#if defined(CONFIG_M54xx)
#define MCFUART_TXFIFOSIZE	512
#elif defined(CONFIG_M5272)
#define MCFUART_TXFIFOSIZE	25
#else
#define MCFUART_TXFIFOSIZE	1
#endif
#endif	
