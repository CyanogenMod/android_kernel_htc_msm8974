#include <linux/serial_sci.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <cpu/serial.h>

#define SCPCR 0xA4000116
#define SCPDR 0xA4000136

static void sh770x_sci_init_pins(struct uart_port *port, unsigned int cflag)
{
	unsigned short data;

	
	data = __raw_readw(SCPCR);
	
	__raw_writew(data & 0x0fcf, SCPCR);

	if (!(cflag & CRTSCTS)) {
		
		data = __raw_readw(SCPCR);
		__raw_writew((data & 0x0fcf) | 0x1000, SCPCR);

		data = __raw_readb(SCPDR);
		
		__raw_writeb(data & 0xbf, SCPDR);
	}
}

struct plat_sci_port_ops sh770x_sci_port_ops = {
	.init_pins	= sh770x_sci_init_pins,
};
