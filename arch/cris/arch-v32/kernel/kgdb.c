/*
 *  arch/cris/arch-v32/kernel/kgdb.c
 *
 *  CRIS v32 version by Orjan Friberg, Axis Communications AB.
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com),
 *
 *  Originally written by Glenn Engel, Lake Stevens Instrument Division
 *
 *  Contributed by HP Systems
 *
 *  Modified for SPARC by Stu Grossman, Cygnus Support.
 *
 *  Modified for Linux/MIPS (and MIPS in general) by Andreas Busse
 *  Send complaints, suggestions etc. to <andy@waldorf-gmbh.de>
 *
 *  Copyright (C) 1995 Andreas Busse
 */





#include <linux/string.h>
#include <linux/signal.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/linkage.h>
#include <linux/reboot.h>

#include <asm/setup.h>
#include <asm/ptrace.h>

#include <asm/irq.h>
#include <hwregs/reg_map.h>
#include <hwregs/reg_rdwr.h>
#include <hwregs/intr_vect_defs.h>
#include <hwregs/ser_defs.h>

extern void gdb_handle_exception(void);
extern void kgdb_handle_exception(void);

static int kgdb_started = 0;


typedef
struct register_image
{
	                      
	unsigned int   r0;    
	unsigned int   r1;    
	unsigned int   r2;    
	unsigned int   r3;    
	unsigned int   r4;    
	unsigned int   r5;    
	unsigned int   r6;    
	unsigned int   r7;    
	unsigned int   r8;    
	unsigned int   r9;    
	unsigned int   r10;   
	unsigned int   r11;   
	unsigned int   r12;   
	unsigned int   r13;   
	unsigned int   sp;    
	unsigned int   acr;   

	unsigned char  bz;    
	unsigned char  vr;    
	unsigned int   pid;   
	unsigned char  srs;   
        unsigned short wz;    
	unsigned int   exs;   
	unsigned int   eda;   
	unsigned int   mof;   
	unsigned int   dz;    
	unsigned int   ebp;   
	unsigned int   erp;   
	unsigned int   srp;   
	unsigned int   nrp;   
	unsigned int   ccs;   
	unsigned int   usp;   
	unsigned int   spc;   
	unsigned int   pc;    

} registers;

typedef
struct bp_register_image
{
	
	unsigned int   s0_0;
	unsigned int   s1_0;
	unsigned int   s2_0;
	unsigned int   s3_0;
	unsigned int   s4_0;
	unsigned int   s5_0;
	unsigned int   s6_0;
	unsigned int   s7_0;
	unsigned int   s8_0;
	unsigned int   s9_0;
	unsigned int   s10_0;
	unsigned int   s11_0;
	unsigned int   s12_0;
	unsigned int   s13_0;
	unsigned int   s14_0;
	unsigned int   s15_0;

	
	unsigned int   s0_1;
	unsigned int   s1_1;
	unsigned int   s2_1;
	unsigned int   s3_1;
	unsigned int   s4_1;
	unsigned int   s5_1;
	unsigned int   s6_1;
	unsigned int   s7_1;
	unsigned int   s8_1;
	unsigned int   s9_1;
	unsigned int   s10_1;
	unsigned int   s11_1;
	unsigned int   s12_1;
	unsigned int   s13_1;
	unsigned int   s14_1;
	unsigned int   s15_1;

	
	unsigned int   s0_2;
	unsigned int   s1_2;
	unsigned int   s2_2;
	unsigned int   s3_2;
	unsigned int   s4_2;
	unsigned int   s5_2;
	unsigned int   s6_2;
	unsigned int   s7_2;
	unsigned int   s8_2;
	unsigned int   s9_2;
	unsigned int   s10_2;
	unsigned int   s11_2;
	unsigned int   s12_2;
	unsigned int   s13_2;
	unsigned int   s14_2;
	unsigned int   s15_2;

	
	unsigned int   s0_3; 
	unsigned int   s1_3; 
	unsigned int   s2_3; 
	unsigned int   s3_3; 
	unsigned int   s4_3; 
	unsigned int   s5_3; 
	unsigned int   s6_3; 
	unsigned int   s7_3; 
	unsigned int   s8_3; 
	unsigned int   s9_3; 
	unsigned int   s10_3; 
	unsigned int   s11_3; 
	unsigned int   s12_3; 
	unsigned int   s13_3; 
	unsigned int   s14_3; 
	unsigned int   s15_3; 

} support_registers;

enum register_name
{
	R0,  R1,  R2,  R3,
	R4,  R5,  R6,  R7,
	R8,  R9,  R10, R11,
	R12, R13, SP,  ACR,

	BZ,  VR,  PID, SRS,
	WZ,  EXS, EDA, MOF,
	DZ,  EBP, ERP, SRP,
	NRP, CCS, USP, SPC,
	PC,

	S0,  S1,  S2,  S3,
	S4,  S5,  S6,  S7,
	S8,  S9,  S10, S11,
	S12, S13, S14, S15

};

static int register_size[] =
{
	4, 4, 4, 4,
	4, 4, 4, 4,
	4, 4, 4, 4,
	4, 4, 4, 4,

	1, 1, 4, 1,
	2, 4, 4, 4,
	4, 4, 4, 4,
	4, 4, 4, 4,

	4,

	4, 4, 4, 4,
	4, 4, 4, 4,
	4, 4, 4, 4,
	4, 4, 4

};

registers reg;
support_registers sreg;


static char *gdb_cris_strcpy(char *s1, const char *s2);

static int gdb_cris_strlen(const char *s);

static void *gdb_cris_memchr(const void *s, int c, int n);

static int gdb_cris_strtol(const char *s, char **endptr, int base);


static int write_register(int regno, char *val);

static int read_register(char regno, unsigned int *valptr);

int getDebugChar(void);

#ifdef CONFIG_ETRAX_VCS_SIM
int getDebugChar(void)
{
  return socketread();
}
#endif

void putDebugChar(int val);

#ifdef CONFIG_ETRAX_VCS_SIM
void putDebugChar(int val)
{
  socketwrite((char *)&val, 1);
}
#endif

static int hex(char ch);

static char *mem2hex(char *buf, unsigned char *mem, int count);

/* Convert the array, in hexadecimal representation, pointed to by buf into
   binary representation. Put the result in mem, and return a pointer to
   the character after the last byte written. */
static unsigned char *hex2mem(unsigned char *mem, char *buf, int count);

/* Put the content of the array, in binary representation, pointed to by buf
   into memory pointed to by mem, and return a pointer to
   the character after the last byte written. */
static unsigned char *bin2mem(unsigned char *mem, unsigned char *buf, int count);

static void getpacket(char *buffer);

static void putpacket(char *buffer);

static void stub_is_stopped(int sigval);

void handle_exception(int sigval);

static void kill_restart(void);


void putDebugString(const unsigned char *str, int len);

void breakpoint(void);

#define USEDVAR(name)    { if (name) { ; } }
#define USEDFUN(name) { void (*pf)(void) = (void *)name; USEDVAR(pf) }

#define BUFMAX 512

#define RUNLENMAX 64

static char input_buffer[BUFMAX];
static char output_buffer[BUFMAX];

enum error_type
{
	SUCCESS, E01, E02, E03, E04, E05, E06,
};

static char *error_message[] =
{
	"",
	"E01 Set current or general thread - H[c,g] - internal error.",
	"E02 Change register content - P - cannot change read-only register.",
	"E03 Thread is not alive.", 
	"E04 The command is not supported - [s,C,S,!,R,d,r] - internal error.",
	"E05 Change register content - P - the register is not implemented..",
	"E06 Change memory content - M - internal error.",
};

#define INTERNAL_STACK_SIZE 1024
char internal_stack[INTERNAL_STACK_SIZE];

static int dynamic_bp = 0;


static char*
gdb_cris_strcpy(char *s1, const char *s2)
{
	char *s = s1;

	for (s = s1; (*s++ = *s2++) != '\0'; )
		;
	return s1;
}

static int
gdb_cris_strlen(const char *s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; sc++)
		;
	return (sc - s);
}

static void*
gdb_cris_memchr(const void *s, int c, int n)
{
	const unsigned char uc = c;
	const unsigned char *su;

	for (su = s; 0 < n; ++su, --n)
		if (*su == uc)
			return (void *)su;
	return NULL;
}
static int
gdb_cris_strtol(const char *s, char **endptr, int base)
{
	char *s1;
	char *sd;
	int x = 0;

	for (s1 = (char*)s; (sd = gdb_cris_memchr(hex_asc, *s1, base)) != NULL; ++s1)
		x = x * base + (sd - hex_asc);

        if (endptr) {
                
                *endptr = s1;
        }

	return x;
}


static int
write_register(int regno, char *val)
{
	int status = SUCCESS;

        if (regno >= R0 && regno <= ACR) {
		
		hex2mem((unsigned char *)&reg.r0 + (regno - R0) * sizeof(unsigned int),
			val, sizeof(unsigned int));

	} else if (regno == BZ || regno == VR || regno == WZ || regno == DZ) {
		
		status = E02;

	} else if (regno == PID) {
		hex2mem((unsigned char *)&reg.pid, val, sizeof(unsigned int));

	} else if (regno == SRS) {
		
		hex2mem((unsigned char *)&reg.srs, val, sizeof(unsigned char));

	} else if (regno >= EXS && regno <= SPC) {
		
		hex2mem((unsigned char *)&reg.exs + (regno - EXS) * sizeof(unsigned int),
			 val, sizeof(unsigned int));

       } else if (regno == PC) {
               
               status = E02;

       } else if (regno >= S0 && regno <= S15) {
               
               hex2mem((unsigned char *)&sreg.s0_0 + (reg.srs * 16 * sizeof(unsigned int)) + (regno - S0) * sizeof(unsigned int), val, sizeof(unsigned int));
	} else {
		
		status = E05;
	}
	return status;
}

static int
read_register(char regno, unsigned int *valptr)
{
	int status = SUCCESS;


	if (regno >= R0 && regno <= ACR) {
		
		*valptr = *(unsigned int *)((char *)&reg.r0 + (regno - R0) * sizeof(unsigned int));

	} else if (regno == BZ || regno == VR) {
		
		*valptr = (unsigned int)(*(unsigned char *)
                                         ((char *)&reg.bz + (regno - BZ) * sizeof(char)));

	} else if (regno == PID) {
		
		*valptr =  *(unsigned int *)((char *)&reg.pid);

	} else if (regno == SRS) {
		
		*valptr = (unsigned int)(*(unsigned char *)((char *)&reg.srs));

	} else if (regno == WZ) {
		
		*valptr = (unsigned int)(*(unsigned short *)(char *)&reg.wz);

	} else if (regno >= EXS && regno <= PC) {
		
		*valptr = *(unsigned int *)((char *)&reg.exs + (regno - EXS) * sizeof(unsigned int));

	} else if (regno >= S0 && regno <= S15) {
		
		*valptr = *(unsigned int *)((char *)&sreg.s0_0 + (reg.srs * 16 * sizeof(unsigned int)) + (regno - S0) * sizeof(unsigned int));

	} else {
		
		status = E05;
	}
	return status;

}

static int
hex(char ch)
{
	if ((ch >= 'a') && (ch <= 'f'))
		return (ch - 'a' + 10);
	if ((ch >= '0') && (ch <= '9'))
		return (ch - '0');
	if ((ch >= 'A') && (ch <= 'F'))
		return (ch - 'A' + 10);
	return -1;
}


static char *
mem2hex(char *buf, unsigned char *mem, int count)
{
	int i;
	int ch;

        if (mem == NULL) {
		
                for (i = 0; i < count; i++) {
                        *buf++ = '0';
                        *buf++ = '0';
                }
        } else {
                
		for (i = 0; i < count; i++) {
			ch = *mem++;
			buf = hex_byte_pack(buf, ch);
		}
        }
        
	*buf = '\0';
	return buf;
}

static char *
mem2hex_nbo(char *buf, unsigned char *mem, int count)
{
	int i;
	int ch;

	mem += count - 1;
	for (i = 0; i < count; i++) {
		ch = *mem--;
		buf = hex_byte_pack(buf, ch);
        }

        
	*buf = '\0';
	return buf;
}

/* Convert the array, in hexadecimal representation, pointed to by buf into
   binary representation. Put the result in mem, and return a pointer to
   the character after the last byte written. */
static unsigned char*
hex2mem(unsigned char *mem, char *buf, int count)
{
	int i;
	unsigned char ch;
	for (i = 0; i < count; i++) {
		ch = hex (*buf++) << 4;
		ch = ch + hex (*buf++);
		*mem++ = ch;
	}
	return mem;
}

/* Put the content of the array, in binary representation, pointed to by buf
   into memory pointed to by mem, and return a pointer to the character after
   the last byte written.
   Gdb will escape $, #, and the escape char (0x7d). */
static unsigned char*
bin2mem(unsigned char *mem, unsigned char *buf, int count)
{
	int i;
	unsigned char *next;
	for (i = 0; i < count; i++) {
		if (*buf == 0x7d) {
			next = buf + 1;
			if (*next == 0x3 || *next == 0x4 || *next == 0x5D) {
				 
				buf++;
				*buf += 0x20;
			}
		}
		*mem++ = *buf++;
	}
	return mem;
}

static void
getpacket(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	char ch;

	do {
		while((ch = getDebugChar ()) != '$')
			;
		checksum = 0;
		xmitcsum = -1;
		count = 0;
		
		while (count < BUFMAX) {
			ch = getDebugChar();
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}

		if (count >= BUFMAX)
			continue;

		buffer[count] = 0;

		if (ch == '#') {
			xmitcsum = hex(getDebugChar()) << 4;
			xmitcsum += hex(getDebugChar());
			if (checksum != xmitcsum) {
				
				putDebugChar('-');
			} else {
				
				putDebugChar('+');
				
				if (buffer[2] == ':') {
					putDebugChar(buffer[0]);
					putDebugChar(buffer[1]);
					
					count = gdb_cris_strlen(buffer);
					for (i = 3; i <= count; i++)
						buffer[i - 3] = buffer[i];
				}
			}
		}
	} while (checksum != xmitcsum);
}


static void
putpacket(char *buffer)
{
	int checksum;
	int runlen;
	int encode;

	do {
		char *src = buffer;
		putDebugChar('$');
		checksum = 0;
		while (*src) {
			
			putDebugChar(*src);
			checksum += *src;
			runlen = 0;
			while (runlen < RUNLENMAX && *src == src[runlen]) {
				runlen++;
			}
			if (runlen > 3) {
				
				putDebugChar ('*');
				checksum += '*';
				encode = runlen + ' ' - 4;
				putDebugChar(encode);
				checksum += encode;
				src += runlen;
			} else {
				src++;
			}
		}
		putDebugChar('#');
		putDebugChar(hex_asc_hi(checksum));
		putDebugChar(hex_asc_lo(checksum));
	} while(kgdb_started && (getDebugChar() != '+'));
}

void
putDebugString(const unsigned char *str, int len)
{
	
	asm("spchere:");
	asm("move $spc, $r10");
	asm("cmp.d spchere, $r10");
	asm("bne nosstep");
	asm("nop");
	asm("move.d spccont, $r10");
	asm("move $r10, $spc");
	asm("nosstep:");

        output_buffer[0] = 'O';
        mem2hex(&output_buffer[1], (unsigned char *)str, len);
        putpacket(output_buffer);

	asm("spccont:");
}

static void
stub_is_stopped(int sigval)
{
	char *ptr = output_buffer;
	unsigned int reg_cont;

	

	*ptr++ = 'T';
	ptr = hex_byte_pack(ptr, sigval);

	if (((reg.exs & 0xff00) >> 8) == 0xc) {

		int S, bp, trig_bits = 0, rw_bits = 0;
		int trig_mask = 0;
		unsigned int *bp_d_regs = &sreg.s3_3;
		unsigned int stopped_data_address;
		
		S = (reg.exs & 0xffff0000) >> 16;

		if (S & 1) {
			
			
		} else {
			
			for (bp = 0; bp < 6; bp++) {

				
				int bitpos_trig = 1 + bp * 2;
				
				int bitpos_config = 2 + bp * 4;

				
				trig_bits = (S & (3 << bitpos_trig)) >> bitpos_trig;

				
				rw_bits = (sreg.s0_3 & (3 << bitpos_config)) >> bitpos_config;
				if (trig_bits) {
					if ((rw_bits == 0x1 && trig_bits != 0x1) ||
					    (rw_bits == 0x2 && trig_bits != 0x2))
						panic("Invalid r/w trigging for this BP");

					
					trig_mask |= (1 << bp);

					if (reg.eda >= bp_d_regs[bp * 2] &&
					    reg.eda <= bp_d_regs[bp * 2 + 1]) {
						stopped_data_address = reg.eda;
						break;
					}
				}
			}
			if (bp < 6) {
				
			} else if (trig_mask) {
				
				for (bp = 0; bp < 6; bp++) {
					
					int bitpos_config = 2 + bp * 4;

					
					rw_bits = (sreg.s0_3 & (3 << bitpos_config)) >> bitpos_config;

					if (trig_mask & (1 << bp)) {
						
						if (reg.eda + 31 >= bp_d_regs[bp * 2]) {
							stopped_data_address = bp_d_regs[bp * 2];
							break;
						} else {
							
							printk("EDA doesn't match trigged BP's range");
						}
					}
				}
			}

			
			BUG_ON(bp >= 6);
			if (rw_bits == 0x1) {
				
				strncpy(ptr, "rwatch", 6);
				ptr += 6;
			} else if (rw_bits == 0x2) {
				
				strncpy(ptr, "watch", 5);
				ptr += 5;
			} else if (rw_bits == 0x3) {
				
				strncpy(ptr, "awatch", 6);
				ptr += 6;
			} else {
				panic("Invalid r/w bits for this BP.");
			}

			*ptr++ = ':';
			
			ptr = mem2hex_nbo(ptr, (unsigned char *)&stopped_data_address, register_size[EDA]);
			*ptr++ = ';';
		}
	}
	
	read_register(PC, &reg_cont);
	ptr = hex_byte_pack(ptr, PC);
	*ptr++ = ':';
	ptr = mem2hex(ptr, (unsigned char *)&reg_cont, register_size[PC]);
	*ptr++ = ';';

	read_register(R8, &reg_cont);
	ptr = hex_byte_pack(ptr, R8);
	*ptr++ = ':';
	ptr = mem2hex(ptr, (unsigned char *)&reg_cont, register_size[R8]);
	*ptr++ = ';';

	read_register(SP, &reg_cont);
	ptr = hex_byte_pack(ptr, SP);
	*ptr++ = ':';
	ptr = mem2hex(ptr, (unsigned char *)&reg_cont, register_size[SP]);
	*ptr++ = ';';

	
        read_register(ERP, &reg_cont);
	ptr = hex_byte_pack(ptr, ERP);
        *ptr++ = ':';
        ptr = mem2hex(ptr, (unsigned char *)&reg_cont, register_size[ERP]);
        *ptr++ = ';';

	
	*ptr = 0;
	putpacket(output_buffer);
}


int insn_size(unsigned long pc)
{
	unsigned short opcode = *(unsigned short *)pc;
	int size = 0;

	switch ((opcode & 0x0f00) >> 8) {
	case 0x0:
	case 0x9:
	case 0xb:
		size = 2;
		break;
	case 0xe:
	case 0xf:
		size = 6;
		break;
	case 0xd:
		
		if ((opcode & 0xff) == 0xff)
			size = 4;
		else
			size = 6;
		break;
	default:
		panic("Couldn't find size of opcode 0x%x at 0x%lx\n", opcode, pc);
	}

	return size;
}

void register_fixup(int sigval)
{
	
	reg.sp += 4;

	
	reg.pc = reg.erp;
	if (reg.erp & 0x1) {
		
		if (reg.spc) {
			
			reg.pc = reg.spc;
		} else {
			reg.pc += insn_size(reg.erp & ~1) - 1 ;
		}
	}

	if ((reg.exs & 0x3) == 0x0) {
		reg.eda = 0;
	}

	if (sigval == SIGTRAP) {
		

		
		if (((reg.exs & 0xff00) >> 8) == 0x18) {

			

			if (!dynamic_bp) {
				
				dynamic_bp = 1;
			} else {

				
				if (!(reg.erp & 0x1)) {
					reg.erp -= 2;
					reg.pc -= 2;
				}
			}

		} else if (((reg.exs & 0xff00) >> 8) == 0x3) {
			
			

		} else if (((reg.exs & 0xff00) >> 8) == 0xc) {

			

			reg.spc = 0;

			
		}

	} else if (sigval == SIGINT) {
		
	}
}

static void insert_watchpoint(char type, int addr, int len)
{

	if (type < '1' || type > '4') {
		output_buffer[0] = 0;
		return;
	}

	if (type == '3')
		type = '4';

	if (type == '1') {
		
		
		if (sreg.s0_3 & 0x1) {
			
			gdb_cris_strcpy(output_buffer, error_message[E04]);
			return;
		}
		
		sreg.s1_3 = addr;
		sreg.s2_3 = (addr + len - 1);
		sreg.s0_3 |= 1;
	} else {
		int bp;
		unsigned int *bp_d_regs = &sreg.s3_3;


		
		for (bp = 0; bp < 6; bp++) {
			if (!(sreg.s0_3 & (0x3 << (2 + (bp * 4))))) {
				break;
			}
		}

		if (bp > 5) {
			
			gdb_cris_strcpy(output_buffer, error_message[E04]);
			return;
		}

		
		if (type == '3' || type == '4') {
			
			sreg.s0_3 |= (1 << (2 + bp * 4));
		}
		if (type == '2' || type == '4') {
			
			sreg.s0_3 |= (2 << (2 + bp * 4));
		}

		
		bp_d_regs[bp * 2] = addr;
		bp_d_regs[bp * 2 + 1] = (addr + len - 1);
	}

	
	reg.ccs |= (1 << (S_CCS_BITNR + CCS_SHIFT));
	gdb_cris_strcpy(output_buffer, "OK");
}

static void remove_watchpoint(char type, int addr, int len)
{
	if (type < '1' || type > '4') {
		output_buffer[0] = 0;
		return;
	}

	if (type == '3')
		type = '4';

	if (type == '1') {
		
		
		if (!(sreg.s0_3 & 0x1)) {
			
			gdb_cris_strcpy(output_buffer, error_message[E04]);
			return;
		}
		
		sreg.s1_3 = 0;
		sreg.s2_3 = 0;
		sreg.s0_3 &= ~1;
	} else {
		int bp;
		unsigned int *bp_d_regs = &sreg.s3_3;


		for (bp = 0; bp < 6; bp++) {
			if (bp_d_regs[bp * 2] == addr &&
			    bp_d_regs[bp * 2 + 1] == (addr + len - 1)) {
				
				int bitpos = 2 + bp * 4;
				int rw_bits;

				
				rw_bits = (sreg.s0_3 & (0x3 << bitpos)) >> bitpos;

				if ((type == '3' && rw_bits == 0x1) ||
				    (type == '2' && rw_bits == 0x2) ||
				    (type == '4' && rw_bits == 0x3)) {
					
					break;
				}
			}
		}

		if (bp > 5) {
			
			gdb_cris_strcpy(output_buffer, error_message[E04]);
			return;
		}

		sreg.s0_3 &= ~(3 << (2 + (bp * 4)));
		bp_d_regs[bp * 2] = 0;
		bp_d_regs[bp * 2 + 1] = 0;
	}

	
	gdb_cris_strcpy(output_buffer, "OK");
}



void
handle_exception(int sigval)
{
	

	USEDFUN(handle_exception);
	USEDVAR(internal_stack[0]);

	register_fixup(sigval);

	
	stub_is_stopped(sigval);

	for (;;) {
		output_buffer[0] = '\0';
		getpacket(input_buffer);
		switch (input_buffer[0]) {
			case 'g':
			{
				char *buf;
				
				buf = mem2hex(output_buffer, (char *)&reg, sizeof(registers));
				
				
				mem2hex(buf,
					(char *)&sreg + (reg.srs * 16 * sizeof(unsigned int)),
					16 * sizeof(unsigned int));
				break;
			}
			case 'G':
				
				hex2mem((char *)&reg, &input_buffer[1], sizeof(registers));
				
				hex2mem((char *)&sreg + (reg.srs * 16 * sizeof(unsigned int)),
					&input_buffer[1] + sizeof(registers),
					16 * sizeof(unsigned int));
				gdb_cris_strcpy(output_buffer, "OK");
				break;

			case 'P':
				{
					char *suffix;
					int regno = gdb_cris_strtol(&input_buffer[1], &suffix, 16);
					int status;

					status = write_register(regno, suffix+1);

					switch (status) {
						case E02:
							
							gdb_cris_strcpy(output_buffer, error_message[E02]);
							break;
						case E05:
							
							gdb_cris_strcpy(output_buffer, error_message[E05]);
							break;
						default:
							
							gdb_cris_strcpy(output_buffer, "OK");
							break;
					}
				}
				break;

			case 'm':
				{
                                        char *suffix;
					unsigned char *addr = (unsigned char *)gdb_cris_strtol(&input_buffer[1],
                                                                                               &suffix, 16);
					int len = gdb_cris_strtol(suffix+1, 0, 16);

					if (!((unsigned int)addr >= 0xc0000000 &&
					      (unsigned int)addr < 0xd0000000))
						addr = NULL;

                                        mem2hex(output_buffer, addr, len);
                                }
				break;

			case 'X':
			case 'M':
				{
					char *lenptr;
					char *dataptr;
					unsigned char *addr = (unsigned char *)gdb_cris_strtol(&input_buffer[1],
										      &lenptr, 16);
					int len = gdb_cris_strtol(lenptr+1, &dataptr, 16);
					if (*lenptr == ',' && *dataptr == ':') {
						if (input_buffer[0] == 'M') {
							hex2mem(addr, dataptr + 1, len);
						} else  {
							bin2mem(addr, dataptr + 1, len);
						}
						gdb_cris_strcpy(output_buffer, "OK");
					}
					else {
						gdb_cris_strcpy(output_buffer, error_message[E06]);
					}
				}
				break;

			case 'c':

				if (input_buffer[1] != '\0') {
					
					gdb_cris_strcpy(output_buffer, error_message[E04]);
					break;
				}

				

				
				reg.spc = 0;
				if ((sreg.s0_3 & 0x3fff) == 0) {
					reg.ccs &= ~(1 << (S_CCS_BITNR + CCS_SHIFT));
				}

				return;

			case 's':

				if (input_buffer[1] != '\0') {
					
					gdb_cris_strcpy(output_buffer, error_message[E04]);
					break;
				}

				reg.spc = reg.pc;

				reg.ccs |= (1 << (S_CCS_BITNR + CCS_SHIFT));
				return;

                       case 'Z':

                               {
                                       char *lenptr;
                                       char *dataptr;
                                       int addr = gdb_cris_strtol(&input_buffer[3], &lenptr, 16);
                                       int len = gdb_cris_strtol(lenptr + 1, &dataptr, 16);
                                       char type = input_buffer[1];

				       insert_watchpoint(type, addr, len);
                                       break;
                               }

                       case 'z':
                               {
                                       char *lenptr;
                                       char *dataptr;
                                       int addr = gdb_cris_strtol(&input_buffer[3], &lenptr, 16);
                                       int len = gdb_cris_strtol(lenptr + 1, &dataptr, 16);
                                       char type = input_buffer[1];

                                       remove_watchpoint(type, addr, len);
                                       break;
                               }


			case '?':
				output_buffer[0] = 'S';
				output_buffer[1] = hex_asc_hi(sigval);
				output_buffer[2] = hex_asc_lo(sigval);
				output_buffer[3] = 0;
				break;

			case 'D':
				putpacket("OK");
				return;

			case 'k':
			case 'r':
				kill_restart();
				break;

			case 'C':
			case 'S':
			case '!':
			case 'R':
			case 'd':

				gdb_cris_strcpy(output_buffer, error_message[E04]);
				break;

			default:
				output_buffer[0] = 0;
				break;
		}
		putpacket(output_buffer);
	}
}

void
kgdb_init(void)
{
	reg_intr_vect_rw_mask intr_mask;
	reg_ser_rw_intr_mask ser_intr_mask;

	
#if defined(CONFIG_ETRAX_KGDB_PORT0)
	set_exception_vector(SER0_INTR_VECT, kgdb_handle_exception);
	
	intr_mask = REG_RD(intr_vect, regi_irq, rw_mask);
	intr_mask.ser0 = 1;
	REG_WR(intr_vect, regi_irq, rw_mask, intr_mask);

	ser_intr_mask = REG_RD(ser, regi_ser0, rw_intr_mask);
	ser_intr_mask.dav = regk_ser_yes;
	REG_WR(ser, regi_ser0, rw_intr_mask, ser_intr_mask);
#elif defined(CONFIG_ETRAX_KGDB_PORT1)
	set_exception_vector(SER1_INTR_VECT, kgdb_handle_exception);
	
	intr_mask = REG_RD(intr_vect, regi_irq, rw_mask);
	intr_mask.ser1 = 1;
	REG_WR(intr_vect, regi_irq, rw_mask, intr_mask);

	ser_intr_mask = REG_RD(ser, regi_ser1, rw_intr_mask);
	ser_intr_mask.dav = regk_ser_yes;
	REG_WR(ser, regi_ser1, rw_intr_mask, ser_intr_mask);
#elif defined(CONFIG_ETRAX_KGDB_PORT2)
	set_exception_vector(SER2_INTR_VECT, kgdb_handle_exception);
	
	intr_mask = REG_RD(intr_vect, regi_irq, rw_mask);
	intr_mask.ser2 = 1;
	REG_WR(intr_vect, regi_irq, rw_mask, intr_mask);

	ser_intr_mask = REG_RD(ser, regi_ser2, rw_intr_mask);
	ser_intr_mask.dav = regk_ser_yes;
	REG_WR(ser, regi_ser2, rw_intr_mask, ser_intr_mask);
#elif defined(CONFIG_ETRAX_KGDB_PORT3)
	set_exception_vector(SER3_INTR_VECT, kgdb_handle_exception);
	
	intr_mask = REG_RD(intr_vect, regi_irq, rw_mask);
	intr_mask.ser3 = 1;
	REG_WR(intr_vect, regi_irq, rw_mask, intr_mask);

	ser_intr_mask = REG_RD(ser, regi_ser3, rw_intr_mask);
	ser_intr_mask.dav = regk_ser_yes;
	REG_WR(ser, regi_ser3, rw_intr_mask, ser_intr_mask);
#endif

}
static void
kill_restart(void)
{
	machine_restart("");
}


void
breakpoint(void)
{
	kgdb_started = 1;
	dynamic_bp = 0;     
	__asm__ volatile ("break 8"); 
}

