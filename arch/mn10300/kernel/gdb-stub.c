/* MN10300 GDB stub
 *
 * Originally written by Glenn Engel, Lake Stevens Instrument Division
 *
 * Contributed by HP Systems
 *
 * Modified for SPARC by Stu Grossman, Cygnus Support.
 *
 * Modified for Linux/MIPS (and MIPS in general) by Andreas Busse
 * Send complaints, suggestions etc. to <andy@waldorf-gmbh.de>
 *
 * Copyright (C) 1995 Andreas Busse
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Modified for Linux/mn10300 by David Howells <dhowells@redhat.com>
 */


#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/bug.h>

#include <asm/pgtable.h>
#include <asm/gdb-stub.h>
#include <asm/exceptions.h>
#include <asm/debugger.h>
#include <asm/serial-regs.h>
#include <asm/busctl-regs.h>
#include <unit/leds.h>
#include <unit/serial.h>

#undef GDBSTUB_USE_F7F7_AS_BREAKPOINT

#define BUFMAX 2048

static const char gdbstub_banner[] =
	"Linux/MN10300 GDB Stub (c) RedHat 2007\n";

u8	gdbstub_rx_buffer[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
u32	gdbstub_rx_inp;
u32	gdbstub_rx_outp;
u8	gdbstub_busy;
u8	gdbstub_rx_overflow;
u8	gdbstub_rx_unget;

static u8	gdbstub_flush_caches;
static char	input_buffer[BUFMAX];
static char	output_buffer[BUFMAX];
static char	trans_buffer[BUFMAX];

struct gdbstub_bkpt {
	u8	*addr;		
	u8	len;		
	u8	origbytes[7];	
};

static struct gdbstub_bkpt gdbstub_bkpts[256];

static void getpacket(char *buffer);
static int putpacket(char *buffer);
static int computeSignal(enum exception_code excep);
static int hex(unsigned char ch);
static int hexToInt(char **ptr, int *intValue);
static unsigned char *mem2hex(const void *mem, char *buf, int count,
			      int may_fault);
static const char *hex2mem(const char *buf, void *_mem, int count,
			   int may_fault);

static int hex(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	return -1;
}

#ifdef CONFIG_GDBSTUB_DEBUGGING

void debug_to_serial(const char *p, int n)
{
	__debug_to_serial(p, n);
	
}

void gdbstub_printk(const char *fmt, ...)
{
	va_list args;
	int len;

	
	va_start(args, fmt);
	len = vsnprintf(trans_buffer, sizeof(trans_buffer), fmt, args);
	va_end(args);
	debug_to_serial(trans_buffer, len);
}

#endif

static inline char *gdbstub_strcpy(char *dst, const char *src)
{
	int loop = 0;
	while ((dst[loop] = src[loop]))
	       loop++;
	return dst;
}

static void getpacket(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	unsigned char ch;
	int count, i, ret, error;

	for (;;) {
		do {
			gdbstub_io_rx_char(&ch, 0);
		} while (ch != '$');

		checksum = 0;
		xmitcsum = -1;
		count = 0;
		error = 0;

		while (count < BUFMAX) {
			ret = gdbstub_io_rx_char(&ch, 0);
			if (ret < 0)
				error = ret;

			if (ch == '#')
				break;
			checksum += ch;
			buffer[count] = ch;
			count++;
		}

		if (error == -EIO) {
			gdbstub_proto("### GDB Rx Error - Skipping packet"
				      " ###\n");
			gdbstub_proto("### GDB Tx NAK\n");
			gdbstub_io_tx_char('-');
			continue;
		}

		if (count >= BUFMAX || error)
			continue;

		buffer[count] = 0;

		
		ret = gdbstub_io_rx_char(&ch, 0);
		if (ret < 0)
			error = ret;
		xmitcsum = hex(ch) << 4;

		ret = gdbstub_io_rx_char(&ch, 0);
		if (ret < 0)
			error = ret;
		xmitcsum |= hex(ch);

		if (error) {
			if (error == -EIO)
				gdbstub_io("### GDB Rx Error -"
					   " Skipping packet\n");
			gdbstub_io("### GDB Tx NAK\n");
			gdbstub_io_tx_char('-');
			continue;
		}

		
		if (checksum != xmitcsum) {
			gdbstub_io("### GDB Tx NAK\n");
			gdbstub_io_tx_char('-');	
			continue;
		}

		gdbstub_proto("### GDB Rx '$%s#%02x' ###\n", buffer, checksum);
		gdbstub_io("### GDB Tx ACK\n");
		gdbstub_io_tx_char('+'); 

		if (buffer[2] == ':') {
			gdbstub_io_tx_char(buffer[0]);
			gdbstub_io_tx_char(buffer[1]);

			count = 0;
			while (buffer[count])
				count++;
			for (i = 3; i <= count; i++)
				buffer[i - 3] = buffer[i];
		}

		break;
	}
}

static int putpacket(char *buffer)
{
	unsigned char checksum;
	unsigned char ch;
	int count;

	gdbstub_proto("### GDB Tx $'%s'#?? ###\n", buffer);

	do {
		gdbstub_io_tx_char('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count]) != 0) {
			gdbstub_io_tx_char(ch);
			checksum += ch;
			count += 1;
		}

		gdbstub_io_tx_char('#');
		gdbstub_io_tx_char(hex_asc_hi(checksum));
		gdbstub_io_tx_char(hex_asc_lo(checksum));

	} while (gdbstub_io_rx_char(&ch, 0),
		 ch == '-' && (gdbstub_io("### GDB Rx NAK\n"), 0),
		 ch != '-' && ch != '+' &&
		 (gdbstub_io("### GDB Rx ??? %02x\n", ch), 0),
		 ch != '+' && ch != '$');

	if (ch == '+') {
		gdbstub_io("### GDB Rx ACK\n");
		return 0;
	}

	gdbstub_io("### GDB Tx Abandoned\n");
	gdbstub_rx_unget = ch;
	return 1;
}

static int hexToInt(char **ptr, int *intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue < 0)
			break;

		*intValue = (*intValue << 4) | hexValue;
		numChars++;

		(*ptr)++;
	}

	return (numChars);
}

#ifdef CONFIG_GDBSTUB_ALLOW_SINGLE_STEP
static struct gdb_bp_save {
	u8	*addr;
	u8	opcode[2];
} step_bp[2];

static const unsigned char gdbstub_insn_sizes[256] =
{
	
	1, 3, 3, 3, 1, 3, 3, 3, 1, 3, 3, 3, 1, 3, 3, 3,	
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2, 2, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 
	1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 
	1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 
	2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 
	2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 
	2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 2, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	0, 2, 2, 2, 2, 2, 2, 4, 0, 3, 0, 4, 0, 6, 7, 1  
};

static int __gdbstub_mark_bp(u8 *addr, int ix)
{
	
	if (((u8 *) VMALLOC_START <= addr) && (addr < (u8 *) VMALLOC_END))
		goto okay;
	
	if (((u8 *) 0x80000000UL <= addr) && (addr < (u8 *) 0xa0000000UL))
		goto okay;
	return 0;

okay:
	if (gdbstub_read_byte(addr + 0, &step_bp[ix].opcode[0]) < 0 ||
	    gdbstub_read_byte(addr + 1, &step_bp[ix].opcode[1]) < 0)
		return 0;

	step_bp[ix].addr = addr;
	return 1;
}

static inline void __gdbstub_restore_bp(void)
{
#ifdef GDBSTUB_USE_F7F7_AS_BREAKPOINT
	if (step_bp[0].addr) {
		gdbstub_write_byte(step_bp[0].opcode[0], step_bp[0].addr + 0);
		gdbstub_write_byte(step_bp[0].opcode[1], step_bp[0].addr + 1);
	}
	if (step_bp[1].addr) {
		gdbstub_write_byte(step_bp[1].opcode[0], step_bp[1].addr + 0);
		gdbstub_write_byte(step_bp[1].opcode[1], step_bp[1].addr + 1);
	}
#else
	if (step_bp[0].addr)
		gdbstub_write_byte(step_bp[0].opcode[0], step_bp[0].addr + 0);
	if (step_bp[1].addr)
		gdbstub_write_byte(step_bp[1].opcode[0], step_bp[1].addr + 0);
#endif

	gdbstub_flush_caches = 1;

	step_bp[0].addr		= NULL;
	step_bp[0].opcode[0]	= 0;
	step_bp[0].opcode[1]	= 0;
	step_bp[1].addr		= NULL;
	step_bp[1].opcode[0]	= 0;
	step_bp[1].opcode[1]	= 0;
}

static int gdbstub_single_step(struct pt_regs *regs)
{
	unsigned size;
	uint32_t x;
	uint8_t cur, *pc, *sp;

	step_bp[0].addr		= NULL;
	step_bp[0].opcode[0]	= 0;
	step_bp[0].opcode[1]	= 0;
	step_bp[1].addr		= NULL;
	step_bp[1].opcode[0]	= 0;
	step_bp[1].opcode[1]	= 0;
	x = 0;

	pc = (u8 *) regs->pc;
	sp = (u8 *) (regs + 1);
	if (gdbstub_read_byte(pc, &cur) < 0)
		return -EFAULT;

	gdbstub_bkpt("Single Step from %p { %02x }\n", pc, cur);

	gdbstub_flush_caches = 1;

	size = gdbstub_insn_sizes[cur];
	if (size > 0) {
		if (!__gdbstub_mark_bp(pc + size, 0))
			goto fault;
	} else {
		switch (cur) {
			
		case 0xc0 ... 0xca:
			if (gdbstub_read_byte(pc + 1, (u8 *) &x) < 0)
				goto fault;
			if (!__gdbstub_mark_bp(pc + 2, 0))
				goto fault;
			if ((x < 0 || x > 2) &&
			    !__gdbstub_mark_bp(pc + (s8) x, 1))
				goto fault;
			break;

			
		case 0xd0 ... 0xda:
			if (!__gdbstub_mark_bp(pc + 1, 0))
				goto fault;
			if (regs->pc != regs->lar &&
			    !__gdbstub_mark_bp((u8 *) regs->lar, 1))
				goto fault;
			break;

		case 0xdb:
			if (!__gdbstub_mark_bp(pc + 1, 0))
				goto fault;
			break;

			
		case 0xcc:
		case 0xcd:
			if (gdbstub_read_byte(pc + 1, ((u8 *) &x) + 0) < 0 ||
			    gdbstub_read_byte(pc + 2, ((u8 *) &x) + 1) < 0)
				goto fault;
			if (!__gdbstub_mark_bp(pc + (s16) x, 0))
				goto fault;
			break;

			
		case 0xdc:
		case 0xdd:
			if (gdbstub_read_byte(pc + 1, ((u8 *) &x) + 0) < 0 ||
			    gdbstub_read_byte(pc + 2, ((u8 *) &x) + 1) < 0 ||
			    gdbstub_read_byte(pc + 3, ((u8 *) &x) + 2) < 0 ||
			    gdbstub_read_byte(pc + 4, ((u8 *) &x) + 3) < 0)
				goto fault;
			if (!__gdbstub_mark_bp(pc + (s32) x, 0))
				goto fault;
			break;

			
		case 0xde:
			if (!__gdbstub_mark_bp((u8 *) regs->mdr, 0))
				goto fault;
			break;

			
		case 0xdf:
			if (gdbstub_read_byte(pc + 2, (u8 *) &x) < 0)
				goto fault;
			sp += (s8)x;
			if (gdbstub_read_byte(sp + 0, ((u8 *) &x) + 0) < 0 ||
			    gdbstub_read_byte(sp + 1, ((u8 *) &x) + 1) < 0 ||
			    gdbstub_read_byte(sp + 2, ((u8 *) &x) + 2) < 0 ||
			    gdbstub_read_byte(sp + 3, ((u8 *) &x) + 3) < 0)
				goto fault;
			if (!__gdbstub_mark_bp((u8 *) x, 0))
				goto fault;
			break;

		case 0xf0:
			if (gdbstub_read_byte(pc + 1, &cur) < 0)
				goto fault;

			if (cur >= 0xf0 && cur <= 0xf7) {
				
				switch (cur & 3) {
				case 0: x = regs->a0; break;
				case 1: x = regs->a1; break;
				case 2: x = regs->a2; break;
				case 3: x = regs->a3; break;
				}
				if (!__gdbstub_mark_bp((u8 *) x, 0))
					goto fault;
			} else if (cur == 0xfc) {
				
				if (gdbstub_read_byte(
					    sp + 0, ((u8 *) &x) + 0) < 0 ||
				    gdbstub_read_byte(
					    sp + 1, ((u8 *) &x) + 1) < 0 ||
				    gdbstub_read_byte(
					    sp + 2, ((u8 *) &x) + 2) < 0 ||
				    gdbstub_read_byte(
					    sp + 3, ((u8 *) &x) + 3) < 0)
					goto fault;
				if (!__gdbstub_mark_bp((u8 *) x, 0))
					goto fault;
			} else if (cur == 0xfd) {
				
				if (gdbstub_read_byte(
					    sp + 4, ((u8 *) &x) + 0) < 0 ||
				    gdbstub_read_byte(
					    sp + 5, ((u8 *) &x) + 1) < 0 ||
				    gdbstub_read_byte(
					    sp + 6, ((u8 *) &x) + 2) < 0 ||
				    gdbstub_read_byte(
					    sp + 7, ((u8 *) &x) + 3) < 0)
					goto fault;
				if (!__gdbstub_mark_bp((u8 *) x, 0))
					goto fault;
			} else {
				if (!__gdbstub_mark_bp(pc + 2, 0))
					goto fault;
			}

			break;

			
		case 0xf8:
			if (gdbstub_read_byte(pc + 1, &cur) < 0)
				goto fault;
			if (!__gdbstub_mark_bp(pc + 3, 0))
				goto fault;

			if (cur >= 0xe8 && cur <= 0xeb) {
				if (gdbstub_read_byte(
					    pc + 2, ((u8 *) &x) + 0) < 0)
					goto fault;
				if ((x < 0 || x > 3) &&
				    !__gdbstub_mark_bp(pc + (s8) x, 1))
					goto fault;
			}
			break;

		case 0xfa:
			if (gdbstub_read_byte(pc + 1, &cur) < 0)
				goto fault;

			if (cur == 0xff) {
				
				if (gdbstub_read_byte(
					    pc + 2, ((u8 *) &x) + 0) < 0 ||
				    gdbstub_read_byte(
					    pc + 3, ((u8 *) &x) + 1) < 0)
					goto fault;
				if (!__gdbstub_mark_bp(pc + (s16) x, 0))
					goto fault;
			} else {
				if (!__gdbstub_mark_bp(pc + 4, 0))
					goto fault;
			}
			break;

		case 0xfc:
			if (gdbstub_read_byte(pc + 1, &cur) < 0)
				goto fault;
			if (cur == 0xff) {
				
				if (gdbstub_read_byte(
					    pc + 2, ((u8 *) &x) + 0) < 0 ||
				    gdbstub_read_byte(
					    pc + 3, ((u8 *) &x) + 1) < 0 ||
				    gdbstub_read_byte(
					    pc + 4, ((u8 *) &x) + 2) < 0 ||
				    gdbstub_read_byte(
					    pc + 5, ((u8 *) &x) + 3) < 0)
					goto fault;
				if (!__gdbstub_mark_bp(
					    pc + (s32) x, 0))
					goto fault;
			} else {
				if (!__gdbstub_mark_bp(
					    pc + 6, 0))
					goto fault;
			}
			break;

		}
	}

	gdbstub_bkpt("Step: %02x at %p; %02x at %p\n",
		     step_bp[0].opcode[0], step_bp[0].addr,
		     step_bp[1].opcode[0], step_bp[1].addr);

	if (step_bp[0].addr) {
#ifdef GDBSTUB_USE_F7F7_AS_BREAKPOINT
		if (gdbstub_write_byte(0xF7, step_bp[0].addr + 0) < 0 ||
		    gdbstub_write_byte(0xF7, step_bp[0].addr + 1) < 0)
			goto fault;
#else
		if (gdbstub_write_byte(0xFF, step_bp[0].addr + 0) < 0)
			goto fault;
#endif
	}

	if (step_bp[1].addr) {
#ifdef GDBSTUB_USE_F7F7_AS_BREAKPOINT
		if (gdbstub_write_byte(0xF7, step_bp[1].addr + 0) < 0 ||
		    gdbstub_write_byte(0xF7, step_bp[1].addr + 1) < 0)
			goto fault;
#else
		if (gdbstub_write_byte(0xFF, step_bp[1].addr + 0) < 0)
			goto fault;
#endif
	}

	return 0;

 fault:
	
	__gdbstub_restore_bp();
	return -EFAULT;
}
#endif 

#ifdef CONFIG_GDBSTUB_CONSOLE

void gdbstub_console_write(struct console *con, const char *p, unsigned n)
{
	static const char gdbstub_cr[] = { 0x0d };
	char outbuf[26];
	int qty;
	u8 busy;

	busy = gdbstub_busy;
	gdbstub_busy = 1;

	outbuf[0] = 'O';

	while (n > 0) {
		qty = 1;

		while (n > 0 && qty < 20) {
			mem2hex(p, outbuf + qty, 2, 0);
			qty += 2;
			if (*p == 0x0a) {
				mem2hex(gdbstub_cr, outbuf + qty, 2, 0);
				qty += 2;
			}
			p++;
			n--;
		}

		outbuf[qty] = 0;
		putpacket(outbuf);
	}

	gdbstub_busy = busy;
}

static kdev_t gdbstub_console_dev(struct console *con)
{
	return MKDEV(1, 3); 
}

static struct console gdbstub_console = {
	.name	= "gdb",
	.write	= gdbstub_console_write,
	.device	= gdbstub_console_dev,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
};

#endif

static
unsigned char *mem2hex(const void *_mem, char *buf, int count, int may_fault)
{
	const u8 *mem = _mem;
	u8 ch[4];

	if ((u32) mem & 1 && count >= 1) {
		if (gdbstub_read_byte(mem, ch) != 0)
			return 0;
		buf = hex_byte_pack(buf, ch[0]);
		mem++;
		count--;
	}

	if ((u32) mem & 3 && count >= 2) {
		if (gdbstub_read_word(mem, ch) != 0)
			return 0;
		buf = hex_byte_pack(buf, ch[0]);
		buf = hex_byte_pack(buf, ch[1]);
		mem += 2;
		count -= 2;
	}

	while (count >= 4) {
		if (gdbstub_read_dword(mem, ch) != 0)
			return 0;
		buf = hex_byte_pack(buf, ch[0]);
		buf = hex_byte_pack(buf, ch[1]);
		buf = hex_byte_pack(buf, ch[2]);
		buf = hex_byte_pack(buf, ch[3]);
		mem += 4;
		count -= 4;
	}

	if (count >= 2) {
		if (gdbstub_read_word(mem, ch) != 0)
			return 0;
		buf = hex_byte_pack(buf, ch[0]);
		buf = hex_byte_pack(buf, ch[1]);
		mem += 2;
		count -= 2;
	}

	if (count >= 1) {
		if (gdbstub_read_byte(mem, ch) != 0)
			return 0;
		buf = hex_byte_pack(buf, ch[0]);
	}

	*buf = 0;
	return buf;
}

/*
 * convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written
 * may_fault is non-zero if we are reading from arbitrary memory, but is
 * currently not used.
 */
static
const char *hex2mem(const char *buf, void *_mem, int count, int may_fault)
{
	u8 *mem = _mem;
	union {
		u32 val;
		u8 b[4];
	} ch;

	if ((u32) mem & 1 && count >= 1) {
		ch.b[0]  = hex(*buf++) << 4;
		ch.b[0] |= hex(*buf++);
		if (gdbstub_write_byte(ch.val, mem) != 0)
			return 0;
		mem++;
		count--;
	}

	if ((u32) mem & 3 && count >= 2) {
		ch.b[0]  = hex(*buf++) << 4;
		ch.b[0] |= hex(*buf++);
		ch.b[1]  = hex(*buf++) << 4;
		ch.b[1] |= hex(*buf++);
		if (gdbstub_write_word(ch.val, mem) != 0)
			return 0;
		mem += 2;
		count -= 2;
	}

	while (count >= 4) {
		ch.b[0]  = hex(*buf++) << 4;
		ch.b[0] |= hex(*buf++);
		ch.b[1]  = hex(*buf++) << 4;
		ch.b[1] |= hex(*buf++);
		ch.b[2]  = hex(*buf++) << 4;
		ch.b[2] |= hex(*buf++);
		ch.b[3]  = hex(*buf++) << 4;
		ch.b[3] |= hex(*buf++);
		if (gdbstub_write_dword(ch.val, mem) != 0)
			return 0;
		mem += 4;
		count -= 4;
	}

	if (count >= 2) {
		ch.b[0]  = hex(*buf++) << 4;
		ch.b[0] |= hex(*buf++);
		ch.b[1]  = hex(*buf++) << 4;
		ch.b[1] |= hex(*buf++);
		if (gdbstub_write_word(ch.val, mem) != 0)
			return 0;
		mem += 2;
		count -= 2;
	}

	if (count >= 1) {
		ch.b[0]  = hex(*buf++) << 4;
		ch.b[0] |= hex(*buf++);
		if (gdbstub_write_byte(ch.val, mem) != 0)
			return 0;
	}

	return buf;
}

static const struct excep_to_sig_map {
	enum exception_code	excep;	
	unsigned char		signo;	
} excep_to_sig_map[] = {
	{ EXCEP_ITLBMISS,	SIGSEGV		},
	{ EXCEP_DTLBMISS,	SIGSEGV		},
	{ EXCEP_TRAP,		SIGTRAP		},
	{ EXCEP_ISTEP,		SIGTRAP		},
	{ EXCEP_IBREAK,		SIGTRAP		},
	{ EXCEP_OBREAK,		SIGTRAP		},
	{ EXCEP_UNIMPINS,	SIGILL		},
	{ EXCEP_UNIMPEXINS,	SIGILL		},
	{ EXCEP_MEMERR,		SIGSEGV		},
	{ EXCEP_MISALIGN,	SIGSEGV		},
	{ EXCEP_BUSERROR,	SIGBUS		},
	{ EXCEP_ILLINSACC,	SIGSEGV		},
	{ EXCEP_ILLDATACC,	SIGSEGV		},
	{ EXCEP_IOINSACC,	SIGSEGV		},
	{ EXCEP_PRIVINSACC,	SIGSEGV		},
	{ EXCEP_PRIVDATACC,	SIGSEGV		},
	{ EXCEP_FPU_DISABLED,	SIGFPE		},
	{ EXCEP_FPU_UNIMPINS,	SIGFPE		},
	{ EXCEP_FPU_OPERATION,	SIGFPE		},
	{ EXCEP_WDT,		SIGALRM		},
	{ EXCEP_NMI,		SIGQUIT		},
	{ EXCEP_IRQ_LEVEL0,	SIGINT		},
	{ EXCEP_IRQ_LEVEL1,	SIGINT		},
	{ EXCEP_IRQ_LEVEL2,	SIGINT		},
	{ EXCEP_IRQ_LEVEL3,	SIGINT		},
	{ EXCEP_IRQ_LEVEL4,	SIGINT		},
	{ EXCEP_IRQ_LEVEL5,	SIGINT		},
	{ EXCEP_IRQ_LEVEL6,	SIGINT		},
	{ 0, 0}
};

static int computeSignal(enum exception_code excep)
{
	const struct excep_to_sig_map *map;

	for (map = excep_to_sig_map; map->signo; map++)
		if (map->excep == excep)
			return map->signo;

	return SIGHUP; 
}

static u32 gdbstub_fpcr, gdbstub_fpufs_array[32];

static void gdbstub_store_fpu(void)
{
#ifdef CONFIG_FPU

	asm volatile(
		"or %2,epsw\n"
#ifdef CONFIG_MN10300_PROC_MN103E010
		"nop\n"
		"nop\n"
#endif
		"mov %1, a1\n"
		"fmov fs0,  (a1+)\n"
		"fmov fs1,  (a1+)\n"
		"fmov fs2,  (a1+)\n"
		"fmov fs3,  (a1+)\n"
		"fmov fs4,  (a1+)\n"
		"fmov fs5,  (a1+)\n"
		"fmov fs6,  (a1+)\n"
		"fmov fs7,  (a1+)\n"
		"fmov fs8,  (a1+)\n"
		"fmov fs9,  (a1+)\n"
		"fmov fs10, (a1+)\n"
		"fmov fs11, (a1+)\n"
		"fmov fs12, (a1+)\n"
		"fmov fs13, (a1+)\n"
		"fmov fs14, (a1+)\n"
		"fmov fs15, (a1+)\n"
		"fmov fs16, (a1+)\n"
		"fmov fs17, (a1+)\n"
		"fmov fs18, (a1+)\n"
		"fmov fs19, (a1+)\n"
		"fmov fs20, (a1+)\n"
		"fmov fs21, (a1+)\n"
		"fmov fs22, (a1+)\n"
		"fmov fs23, (a1+)\n"
		"fmov fs24, (a1+)\n"
		"fmov fs25, (a1+)\n"
		"fmov fs26, (a1+)\n"
		"fmov fs27, (a1+)\n"
		"fmov fs28, (a1+)\n"
		"fmov fs29, (a1+)\n"
		"fmov fs30, (a1+)\n"
		"fmov fs31, (a1+)\n"
		"fmov fpcr, %0\n"
		: "=d"(gdbstub_fpcr)
		: "g" (&gdbstub_fpufs_array), "i"(EPSW_FE)
		: "a1"
		);
#endif
}

static void gdbstub_load_fpu(void)
{
#ifdef CONFIG_FPU

	asm volatile(
		"or %1,epsw\n"
#ifdef CONFIG_MN10300_PROC_MN103E010
		"nop\n"
		"nop\n"
#endif
		"mov %0, a1\n"
		"fmov (a1+), fs0\n"
		"fmov (a1+), fs1\n"
		"fmov (a1+), fs2\n"
		"fmov (a1+), fs3\n"
		"fmov (a1+), fs4\n"
		"fmov (a1+), fs5\n"
		"fmov (a1+), fs6\n"
		"fmov (a1+), fs7\n"
		"fmov (a1+), fs8\n"
		"fmov (a1+), fs9\n"
		"fmov (a1+), fs10\n"
		"fmov (a1+), fs11\n"
		"fmov (a1+), fs12\n"
		"fmov (a1+), fs13\n"
		"fmov (a1+), fs14\n"
		"fmov (a1+), fs15\n"
		"fmov (a1+), fs16\n"
		"fmov (a1+), fs17\n"
		"fmov (a1+), fs18\n"
		"fmov (a1+), fs19\n"
		"fmov (a1+), fs20\n"
		"fmov (a1+), fs21\n"
		"fmov (a1+), fs22\n"
		"fmov (a1+), fs23\n"
		"fmov (a1+), fs24\n"
		"fmov (a1+), fs25\n"
		"fmov (a1+), fs26\n"
		"fmov (a1+), fs27\n"
		"fmov (a1+), fs28\n"
		"fmov (a1+), fs29\n"
		"fmov (a1+), fs30\n"
		"fmov (a1+), fs31\n"
		"fmov %2, fpcr\n"
		:
		: "g" (&gdbstub_fpufs_array), "i"(EPSW_FE), "d"(gdbstub_fpcr)
		: "a1"
	);
#endif
}

int gdbstub_set_breakpoint(u8 *addr, int len)
{
	int bkpt, loop, xloop;

#ifdef GDBSTUB_USE_F7F7_AS_BREAKPOINT
	len = (len + 1) & ~1;
#endif

	gdbstub_bkpt("setbkpt(%p,%d)\n", addr, len);

	for (bkpt = 255; bkpt >= 0; bkpt--)
		if (!gdbstub_bkpts[bkpt].addr)
			break;
	if (bkpt < 0)
		return -ENOSPC;

	for (loop = 0; loop < len; loop++)
		if (gdbstub_read_byte(&addr[loop],
				      &gdbstub_bkpts[bkpt].origbytes[loop]
				      ) < 0)
			return -EFAULT;

	gdbstub_flush_caches = 1;

#ifdef GDBSTUB_USE_F7F7_AS_BREAKPOINT
	for (loop = 0; loop < len; loop++)
		if (gdbstub_write_byte(0xF7, &addr[loop]) < 0)
			goto restore;
#else
	for (loop = 0; loop < len; loop++)
		if (gdbstub_write_byte(0xFF, &addr[loop]) < 0)
			goto restore;
#endif

	gdbstub_bkpts[bkpt].addr = addr;
	gdbstub_bkpts[bkpt].len = len;

	gdbstub_bkpt("Set BKPT[%02x]: %p-%p {%02x%02x%02x%02x%02x%02x%02x}\n",
		     bkpt,
		     gdbstub_bkpts[bkpt].addr,
		     gdbstub_bkpts[bkpt].addr + gdbstub_bkpts[bkpt].len - 1,
		     gdbstub_bkpts[bkpt].origbytes[0],
		     gdbstub_bkpts[bkpt].origbytes[1],
		     gdbstub_bkpts[bkpt].origbytes[2],
		     gdbstub_bkpts[bkpt].origbytes[3],
		     gdbstub_bkpts[bkpt].origbytes[4],
		     gdbstub_bkpts[bkpt].origbytes[5],
		     gdbstub_bkpts[bkpt].origbytes[6]
		     );

	return 0;

restore:
	for (xloop = 0; xloop < loop; xloop++)
		gdbstub_write_byte(gdbstub_bkpts[bkpt].origbytes[xloop],
				   addr + xloop);
	return -EFAULT;
}

int gdbstub_clear_breakpoint(u8 *addr, int len)
{
	int bkpt, loop;

#ifdef GDBSTUB_USE_F7F7_AS_BREAKPOINT
	len = (len + 1) & ~1;
#endif

	gdbstub_bkpt("clearbkpt(%p,%d)\n", addr, len);

	for (bkpt = 255; bkpt >= 0; bkpt--)
		if (gdbstub_bkpts[bkpt].addr == addr &&
		    gdbstub_bkpts[bkpt].len == len)
			break;
	if (bkpt < 0)
		return -ENOENT;

	gdbstub_bkpts[bkpt].addr = NULL;

	gdbstub_flush_caches = 1;

	for (loop = 0; loop < len; loop++)
		if (gdbstub_write_byte(gdbstub_bkpts[bkpt].origbytes[loop],
				       addr + loop) < 0)
			return -EFAULT;

	return 0;
}

static int gdbstub(struct pt_regs *regs, enum exception_code excep)
{
	unsigned long *stack;
	unsigned long epsw, mdr;
	uint32_t zero, ssp;
	uint8_t broke;
	char *ptr;
	int sigval;
	int addr;
	int length;
	int loop;

	if (excep == EXCEP_FPU_DISABLED)
		return -ENOTSUPP;

	gdbstub_flush_caches = 0;

	mn10300_set_gdbleds(1);

	asm volatile("mov mdr,%0" : "=d"(mdr));
	local_save_flags(epsw);
	arch_local_change_intr_mask_level(
		NUM2EPSW_IM(CONFIG_DEBUGGER_IRQ_LEVEL + 1));

	gdbstub_store_fpu();

#ifdef CONFIG_GDBSTUB_IMMEDIATE
	
	if (regs->pc == (unsigned long) __gdbstub_pause)
		regs->pc = (unsigned long) start_kernel;
#endif

	broke = 0;
#ifdef CONFIG_GDBSTUB_ALLOW_SINGLE_STEP
	if ((step_bp[0].addr && step_bp[0].addr == (u8 *) regs->pc) ||
	    (step_bp[1].addr && step_bp[1].addr == (u8 *) regs->pc))
		broke = 1;

	__gdbstub_restore_bp();
#endif

	if (gdbstub_rx_unget) {
		sigval = SIGINT;
		if (gdbstub_rx_unget != 3)
			goto packet_waiting;
		gdbstub_rx_unget = 0;
	}

	stack = (unsigned long *) regs->sp;
	sigval = broke ? SIGTRAP : computeSignal(excep);

	
	if (!user_mode(regs) && excep == EXCEP_SYSCALL15) {
		const struct bug_entry *bug;

		bug = find_bug(regs->pc);
		if (bug)
			goto found_bug;
		length = snprintf(trans_buffer, sizeof(trans_buffer),
				  "BUG() at address %lx\n", regs->pc);
		goto send_bug_pkt;

	found_bug:
		length = snprintf(trans_buffer, sizeof(trans_buffer),
				  "BUG() at address %lx (%s:%d)\n",
				  regs->pc, bug->file, bug->line);

	send_bug_pkt:
		ptr = output_buffer;
		*ptr++ = 'O';
		ptr = mem2hex(trans_buffer, ptr, length, 0);
		*ptr = 0;
		putpacket(output_buffer);

		regs->pc -= 2;
		sigval = SIGABRT;
	} else if (regs->pc == (unsigned long) __gdbstub_bug_trap) {
		regs->pc = regs->mdr;
		sigval = SIGABRT;
	}

	if (sigval != SIGINT && sigval != SIGTRAP && sigval != SIGILL) {
		static const char title[] = "Excep ", tbcberr[] = "BCBERR ";
		static const char crlf[] = "\r\n";
		char hx;
		u32 bcberr = BCBERR;

		ptr = output_buffer;
		*ptr++ = 'O';
		ptr = mem2hex(title, ptr, sizeof(title) - 1, 0);

		hx = hex_asc_hi(excep >> 8);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_lo(excep >> 8);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_hi(excep);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_lo(excep);
		ptr = hex_byte_pack(ptr, hx);

		ptr = mem2hex(crlf, ptr, sizeof(crlf) - 1, 0);
		*ptr = 0;
		putpacket(output_buffer);	

		
		ptr = output_buffer;
		*ptr++ = 'O';
		ptr = mem2hex(tbcberr, ptr, sizeof(tbcberr) - 1, 0);

		hx = hex_asc_hi(bcberr >> 24);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_lo(bcberr >> 24);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_hi(bcberr >> 16);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_lo(bcberr >> 16);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_hi(bcberr >> 8);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_lo(bcberr >> 8);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_hi(bcberr);
		ptr = hex_byte_pack(ptr, hx);
		hx = hex_asc_lo(bcberr);
		ptr = hex_byte_pack(ptr, hx);

		ptr = mem2hex(crlf, ptr, sizeof(crlf) - 1, 0);
		*ptr = 0;
		putpacket(output_buffer);	
	}

	ptr = output_buffer;

	*ptr++ = 'T';
	ptr = hex_byte_pack(ptr, sigval);

	ptr = hex_byte_pack(ptr, GDB_REGID_PC);
	*ptr++ = ':';
	ptr = mem2hex(&regs->pc, ptr, 4, 0);
	*ptr++ = ';';

	ptr = hex_byte_pack(ptr, GDB_REGID_FP);
	*ptr++ = ':';
	ptr = mem2hex(&regs->a3, ptr, 4, 0);
	*ptr++ = ';';

	ssp = (unsigned long) (regs + 1);
	ptr = hex_byte_pack(ptr, GDB_REGID_SP);
	*ptr++ = ':';
	ptr = mem2hex(&ssp, ptr, 4, 0);
	*ptr++ = ';';

	*ptr++ = 0;
	putpacket(output_buffer);	

packet_waiting:
	while (1) {
		output_buffer[0] = 0;
		getpacket(input_buffer);

		switch (input_buffer[0]) {
			
		case '?':
			output_buffer[0] = 'S';
			output_buffer[1] = hex_asc_hi(sigval);
			output_buffer[2] = hex_asc_lo(sigval);
			output_buffer[3] = 0;
			break;

		case 'd':
			
			break;

		case 'g':
			zero = 0;
			ssp = (u32) (regs + 1);
			ptr = output_buffer;
			ptr = mem2hex(&regs->d0, ptr, 4, 0);
			ptr = mem2hex(&regs->d1, ptr, 4, 0);
			ptr = mem2hex(&regs->d2, ptr, 4, 0);
			ptr = mem2hex(&regs->d3, ptr, 4, 0);
			ptr = mem2hex(&regs->a0, ptr, 4, 0);
			ptr = mem2hex(&regs->a1, ptr, 4, 0);
			ptr = mem2hex(&regs->a2, ptr, 4, 0);
			ptr = mem2hex(&regs->a3, ptr, 4, 0);

			ptr = mem2hex(&ssp, ptr, 4, 0);		
			ptr = mem2hex(&regs->pc, ptr, 4, 0);
			ptr = mem2hex(&regs->mdr, ptr, 4, 0);
			ptr = mem2hex(&regs->epsw, ptr, 4, 0);
			ptr = mem2hex(&regs->lir, ptr, 4, 0);
			ptr = mem2hex(&regs->lar, ptr, 4, 0);
			ptr = mem2hex(&regs->mdrq, ptr, 4, 0);

			ptr = mem2hex(&regs->e0, ptr, 4, 0);	
			ptr = mem2hex(&regs->e1, ptr, 4, 0);
			ptr = mem2hex(&regs->e2, ptr, 4, 0);
			ptr = mem2hex(&regs->e3, ptr, 4, 0);
			ptr = mem2hex(&regs->e4, ptr, 4, 0);
			ptr = mem2hex(&regs->e5, ptr, 4, 0);
			ptr = mem2hex(&regs->e6, ptr, 4, 0);
			ptr = mem2hex(&regs->e7, ptr, 4, 0);

			ptr = mem2hex(&ssp, ptr, 4, 0);
			ptr = mem2hex(&regs, ptr, 4, 0);
			ptr = mem2hex(&regs->sp, ptr, 4, 0);
			ptr = mem2hex(&regs->mcrh, ptr, 4, 0);	
			ptr = mem2hex(&regs->mcrl, ptr, 4, 0);
			ptr = mem2hex(&regs->mcvf, ptr, 4, 0);

			ptr = mem2hex(&gdbstub_fpcr, ptr, 4, 0); 
			ptr = mem2hex(&zero, ptr, 4, 0);
			ptr = mem2hex(&zero, ptr, 4, 0);
			for (loop = 0; loop < 32; loop++)
				ptr = mem2hex(&gdbstub_fpufs_array[loop],
					      ptr, 4, 0); 

			break;

		case 'G':
		{
			const char *ptr;

			ptr = &input_buffer[1];
			ptr = hex2mem(ptr, &regs->d0, 4, 0);
			ptr = hex2mem(ptr, &regs->d1, 4, 0);
			ptr = hex2mem(ptr, &regs->d2, 4, 0);
			ptr = hex2mem(ptr, &regs->d3, 4, 0);
			ptr = hex2mem(ptr, &regs->a0, 4, 0);
			ptr = hex2mem(ptr, &regs->a1, 4, 0);
			ptr = hex2mem(ptr, &regs->a2, 4, 0);
			ptr = hex2mem(ptr, &regs->a3, 4, 0);

			ptr = hex2mem(ptr, &ssp, 4, 0);		
			ptr = hex2mem(ptr, &regs->pc, 4, 0);
			ptr = hex2mem(ptr, &regs->mdr, 4, 0);
			ptr = hex2mem(ptr, &regs->epsw, 4, 0);
			ptr = hex2mem(ptr, &regs->lir, 4, 0);
			ptr = hex2mem(ptr, &regs->lar, 4, 0);
			ptr = hex2mem(ptr, &regs->mdrq, 4, 0);

			ptr = hex2mem(ptr, &regs->e0, 4, 0);	
			ptr = hex2mem(ptr, &regs->e1, 4, 0);
			ptr = hex2mem(ptr, &regs->e2, 4, 0);
			ptr = hex2mem(ptr, &regs->e3, 4, 0);
			ptr = hex2mem(ptr, &regs->e4, 4, 0);
			ptr = hex2mem(ptr, &regs->e5, 4, 0);
			ptr = hex2mem(ptr, &regs->e6, 4, 0);
			ptr = hex2mem(ptr, &regs->e7, 4, 0);

			ptr = hex2mem(ptr, &ssp, 4, 0);
			ptr = hex2mem(ptr, &zero, 4, 0);
			ptr = hex2mem(ptr, &regs->sp, 4, 0);
			ptr = hex2mem(ptr, &regs->mcrh, 4, 0);	
			ptr = hex2mem(ptr, &regs->mcrl, 4, 0);
			ptr = hex2mem(ptr, &regs->mcvf, 4, 0);

			ptr = hex2mem(ptr, &zero, 4, 0);	
			ptr = hex2mem(ptr, &zero, 4, 0);
			ptr = hex2mem(ptr, &zero, 4, 0);
			for (loop = 0; loop < 32; loop++)     
				ptr = hex2mem(ptr, &zero, 4, 0);

#if 0
			unsigned long *newsp = (unsigned long *) registers[SP];
			if (sp != newsp)
				sp = memcpy(newsp, sp, 16 * 4);
#endif

			gdbstub_strcpy(output_buffer, "OK");
		}
		break;

		case 'm':
			ptr = &input_buffer[1];

			if (hexToInt(&ptr, &addr) &&
			    *ptr++ == ',' &&
			    hexToInt(&ptr, &length)
			    ) {
				if (mem2hex((char *) addr, output_buffer,
					    length, 1))
					break;
				gdbstub_strcpy(output_buffer, "E03");
			} else {
				gdbstub_strcpy(output_buffer, "E01");
			}
			break;

		case 'M':
			ptr = &input_buffer[1];

			if (hexToInt(&ptr, &addr) &&
			    *ptr++ == ',' &&
			    hexToInt(&ptr, &length) &&
			    *ptr++ == ':'
			    ) {
				if (hex2mem(ptr, (char *) addr, length, 1))
					gdbstub_strcpy(output_buffer, "OK");
				else
					gdbstub_strcpy(output_buffer, "E03");

				gdbstub_flush_caches = 1;
			} else {
				gdbstub_strcpy(output_buffer, "E02");
			}
			break;

		case 'c':

			ptr = &input_buffer[1];
			if (hexToInt(&ptr, &addr))
				regs->pc = addr;
			goto done;

		case 'k' :
			goto done;	

		case 'r':
			break;

		case 's':
#ifdef CONFIG_GDBSTUB_ALLOW_SINGLE_STEP
			if (gdbstub_single_step(regs) < 0)
				
				gdbstub_printk("unable to set single-step"
					       " bp\n");
			goto done;
#else
			gdbstub_strcpy(output_buffer, "E01");
			break;
#endif

		case 'b':
			do {
				int baudrate;

				ptr = &input_buffer[1];
				if (!hexToInt(&ptr, &baudrate)) {
					gdbstub_strcpy(output_buffer, "B01");
					break;
				}

				if (baudrate) {
					
					putpacket("OK");
					gdbstub_io_set_baud(baudrate);
				}
			} while (0);
			break;

		case 'Z':
			ptr = &input_buffer[1];

			if (!hexToInt(&ptr, &loop) || *ptr++ != ',' ||
			    !hexToInt(&ptr, &addr) || *ptr++ != ',' ||
			    !hexToInt(&ptr, &length)
			    ) {
				gdbstub_strcpy(output_buffer, "E01");
				break;
			}

			
			gdbstub_strcpy(output_buffer, "E03");
			if (loop != 0 ||
			    length < 1 ||
			    length > 7 ||
			    (unsigned long) addr < 4096)
				break;

			if (gdbstub_set_breakpoint((u8 *) addr, length) < 0)
				break;

			gdbstub_strcpy(output_buffer, "OK");
			break;

		case 'z':
			ptr = &input_buffer[1];

			if (!hexToInt(&ptr, &loop) || *ptr++ != ',' ||
			    !hexToInt(&ptr, &addr) || *ptr++ != ',' ||
			    !hexToInt(&ptr, &length)
			    ) {
				gdbstub_strcpy(output_buffer, "E01");
				break;
			}

			
			gdbstub_strcpy(output_buffer, "E03");
			if (loop != 0 ||
			    length < 1 ||
			    length > 7 ||
			    (unsigned long) addr < 4096)
				break;

			if (gdbstub_clear_breakpoint((u8 *) addr, length) < 0)
				break;

			gdbstub_strcpy(output_buffer, "OK");
			break;

		default:
			gdbstub_proto("### GDB Unsupported Cmd '%s'\n",
				      input_buffer);
			break;
		}

		
		putpacket(output_buffer);
	}

done:
	if (gdbstub_flush_caches)
		debugger_local_cache_flushinv();

	gdbstub_load_fpu();
	mn10300_set_gdbleds(0);
	if (excep == EXCEP_NMI)
		NMICR = NMICR_NMIF;

	touch_softlockup_watchdog();

	local_irq_restore(epsw);
	return 0;
}

int at_debugger_breakpoint(struct pt_regs *regs)
{
	return 0;
}

asmlinkage int debugger_intercept(enum exception_code excep,
				  int signo, int si_code, struct pt_regs *regs)
{
	static u8 notfirst = 1;
	int ret;

	if (gdbstub_busy)
		gdbstub_printk("--> gdbstub reentered itself\n");
	gdbstub_busy = 1;

	if (notfirst) {
		unsigned long mdr;
		asm("mov mdr,%0" : "=d"(mdr));

		gdbstub_entry(
			"--> debugger_intercept(%p,%04x) [MDR=%lx PC=%lx]\n",
			regs, excep, mdr, regs->pc);

		gdbstub_entry(
			"PC:  %08lx EPSW:  %08lx  SSP: %08lx mode: %s\n",
			regs->pc, regs->epsw, (unsigned long) &ret,
			user_mode(regs) ? "User" : "Super");
		gdbstub_entry(
			"d0:  %08lx   d1:  %08lx   d2: %08lx   d3: %08lx\n",
			regs->d0, regs->d1, regs->d2, regs->d3);
		gdbstub_entry(
			"a0:  %08lx   a1:  %08lx   a2: %08lx   a3: %08lx\n",
			regs->a0, regs->a1, regs->a2, regs->a3);
		gdbstub_entry(
			"e0:  %08lx   e1:  %08lx   e2: %08lx   e3: %08lx\n",
			regs->e0, regs->e1, regs->e2, regs->e3);
		gdbstub_entry(
			"e4:  %08lx   e5:  %08lx   e6: %08lx   e7: %08lx\n",
			regs->e4, regs->e5, regs->e6, regs->e7);
		gdbstub_entry(
			"lar: %08lx   lir: %08lx  mdr: %08lx  usp: %08lx\n",
			regs->lar, regs->lir, regs->mdr, regs->sp);
		gdbstub_entry(
			"cvf: %08lx   crl: %08lx  crh: %08lx  drq: %08lx\n",
			regs->mcvf, regs->mcrl, regs->mcrh, regs->mdrq);
		gdbstub_entry(
			"threadinfo=%p task=%p)\n",
			current_thread_info(), current);
	} else {
		notfirst = 1;
	}

	ret = gdbstub(regs, excep);

	gdbstub_entry("<-- debugger_intercept()\n");
	gdbstub_busy = 0;
	return ret;
}

asmlinkage void gdbstub_exception(struct pt_regs *regs,
				  enum exception_code excep)
{
	unsigned long mdr;

	asm("mov mdr,%0" : "=d"(mdr));
	gdbstub_entry("--> gdbstub exception({%p},%04x) [MDR=%lx]\n",
		      regs, excep, mdr);

	while ((unsigned long) regs == 0xffffffff) {}

	
	if (regs->pc == (unsigned) gdbstub_read_byte_guard) {
		regs->pc = (unsigned) gdbstub_read_byte_cont;
		goto fault;
	}

	if (regs->pc == (unsigned) gdbstub_read_word_guard) {
		regs->pc = (unsigned) gdbstub_read_word_cont;
		goto fault;
	}

	if (regs->pc == (unsigned) gdbstub_read_dword_guard) {
		regs->pc = (unsigned) gdbstub_read_dword_cont;
		goto fault;
	}

	if (regs->pc == (unsigned) gdbstub_write_byte_guard) {
		regs->pc = (unsigned) gdbstub_write_byte_cont;
		goto fault;
	}

	if (regs->pc == (unsigned) gdbstub_write_word_guard) {
		regs->pc = (unsigned) gdbstub_write_word_cont;
		goto fault;
	}

	if (regs->pc == (unsigned) gdbstub_write_dword_guard) {
		regs->pc = (unsigned) gdbstub_write_dword_cont;
		goto fault;
	}

	gdbstub_printk("\n### GDB stub caused an exception ###\n");

	
	console_verbose();
	show_registers(regs);

	panic("GDB Stub caused an unexpected exception - can't continue\n");

	
fault:
	gdbstub_entry("<-- gdbstub exception() = EFAULT\n");
	regs->d0 = -EFAULT;
	return;
}

void gdbstub_exit(int status)
{
	unsigned char checksum;
	unsigned char ch;
	int count;

	gdbstub_busy = 1;
	output_buffer[0] = 'W';
	output_buffer[1] = hex_asc_hi(status);
	output_buffer[2] = hex_asc_lo(status);
	output_buffer[3] = 0;

	gdbstub_io_tx_char('$');
	checksum = 0;
	count = 0;

	while ((ch = output_buffer[count]) != 0) {
		gdbstub_io_tx_char(ch);
		checksum += ch;
		count += 1;
	}

	gdbstub_io_tx_char('#');
	gdbstub_io_tx_char(hex_asc_hi(checksum));
	gdbstub_io_tx_char(hex_asc_lo(checksum));

	
	gdbstub_io_tx_flush();

	gdbstub_busy = 0;
}

asmlinkage void __init gdbstub_init(void)
{
#ifdef CONFIG_GDBSTUB_IMMEDIATE
	unsigned char ch;
	int ret;
#endif

	gdbstub_busy = 1;

	printk(KERN_INFO "%s", gdbstub_banner);

	gdbstub_io_init();

	gdbstub_entry("--> gdbstub_init\n");

	gdbstub_io("### GDB Tx ACK\n");
	gdbstub_io_tx_char('+'); 

#ifdef CONFIG_GDBSTUB_IMMEDIATE
	gdbstub_printk("GDB Stub waiting for packet\n");

	do { gdbstub_io_rx_char(&ch, 0); } while (ch != '$');
	do { gdbstub_io_rx_char(&ch, 0); } while (ch != '#');
	
	do { ret = gdbstub_io_rx_char(&ch, 0); } while (ret != 0);
	
	do { ret = gdbstub_io_rx_char(&ch, 0); } while (ret != 0);

	gdbstub_io("### GDB Tx NAK\n");
	gdbstub_io_tx_char('-'); 

#else
	printk("GDB Stub ready\n");
#endif

	gdbstub_busy = 0;
	gdbstub_entry("<-- gdbstub_init\n");
}

#ifdef CONFIG_GDBSTUB_CONSOLE
static int __init gdbstub_postinit(void)
{
	printk(KERN_NOTICE "registering console\n");
	register_console(&gdbstub_console);
	return 0;
}

__initcall(gdbstub_postinit);
#endif

asmlinkage void gdbstub_rx_irq(struct pt_regs *regs, enum exception_code excep)
{
	char ch;
	int ret;

	gdbstub_entry("--> gdbstub_rx_irq\n");

	do {
		ret = gdbstub_io_rx_char(&ch, 1);
		if (ret != -EIO && ret != -EAGAIN) {
			if (ret != -EINTR)
				gdbstub_rx_unget = ch;
			gdbstub(regs, excep);
		}
	} while (ret != -EAGAIN);

	gdbstub_entry("<-- gdbstub_rx_irq\n");
}
