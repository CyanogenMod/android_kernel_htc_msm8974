#include "wakeup.h"
#include "boot.h"

static void udelay(int loops)
{
	while (loops--)
		io_delay();	
}

static void beep(unsigned int hz)
{
	u8 enable;

	if (!hz) {
		enable = 0x00;		
	} else {
		u16 div = 1193181/hz;

		outb(0xb6, 0x43);	
		io_delay();
		outb(div, 0x42);	
		io_delay();
		outb(div >> 8, 0x42);	
		io_delay();

		enable = 0x03;		
	}
	inb(0x61);		
	io_delay();
	outb(enable, 0x61);	
	io_delay();
}

#define DOT_HZ		880
#define DASH_HZ		587
#define US_PER_DOT	125000

static void send_morse(const char *pattern)
{
	char s;

	while ((s = *pattern++)) {
		switch (s) {
		case '.':
			beep(DOT_HZ);
			udelay(US_PER_DOT);
			beep(0);
			udelay(US_PER_DOT);
			break;
		case '-':
			beep(DASH_HZ);
			udelay(US_PER_DOT * 3);
			beep(0);
			udelay(US_PER_DOT);
			break;
		default:	
			udelay(US_PER_DOT * 3);
			break;
		}
	}
}

void main(void)
{
	
	if (wakeup_header.real_magic != 0x12345678)
		while (1);

	if (wakeup_header.realmode_flags & 4)
		send_morse("...-");

	if (wakeup_header.realmode_flags & 1)
		asm volatile("lcallw   $0xc000,$3");

	if (wakeup_header.realmode_flags & 2) {
		
		probe_cards(0);
		set_mode(wakeup_header.video_mode);
	}
}
