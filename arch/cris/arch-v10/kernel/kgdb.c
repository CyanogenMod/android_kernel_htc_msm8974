/*!**************************************************************************
*!
*! FILE NAME  : kgdb.c
*!
*! DESCRIPTION: Implementation of the gdb stub with respect to ETRAX 100.
*!              It is a mix of arch/m68k/kernel/kgdb.c and cris_stub.c.
*!
*!---------------------------------------------------------------------------
*! HISTORY
*!
*! DATE         NAME            CHANGES
*! ----         ----            -------
*! Apr 26 1999  Hendrik Ruijter Initial version.
*! May  6 1999  Hendrik Ruijter Removed call to strlen in libc and removed
*!                              struct assignment as it generates calls to
*!                              memcpy in libc.
*! Jun 17 1999  Hendrik Ruijter Added gdb 4.18 support. 'X', 'qC' and 'qL'.
*! Jul 21 1999  Bjorn Wesen     eLinux port
*!
*!---------------------------------------------------------------------------
*!
*! (C) Copyright 1999, Axis Communications AB, LUND, SWEDEN
*!
*!**************************************************************************/




#include <linux/string.h>
#include <linux/signal.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/linkage.h>
#include <linux/reboot.h>

#include <asm/setup.h>
#include <asm/ptrace.h>

#include <arch/svinto.h>
#include <asm/irq.h>

static int kgdb_started = 0;


typedef
struct register_image
{
	
	unsigned int     r0;   
	unsigned int     r1;   
	unsigned int     r2;   
	unsigned int     r3;   
	unsigned int     r4;   
	unsigned int     r5;   
	unsigned int     r6;   
	unsigned int     r7;   
	unsigned int     r8;   
	unsigned int     r9;   
	unsigned int    r10;   
	unsigned int    r11;   
	unsigned int    r12;   
	unsigned int    r13;   
	unsigned int     sp;   
	unsigned int     pc;   

        unsigned char    p0;   
	unsigned char    vr;   

        unsigned short   p4;   
	unsigned short  ccr;   
	
	unsigned int    mof;   
	
        unsigned int     p8;   
	unsigned int    ibr;   
	unsigned int    irp;   
	unsigned int    srp;   
	unsigned int    bar;   
	unsigned int   dccr;   
	unsigned int    brp;   
	unsigned int    usp;   
} registers;


static char *gdb_cris_strcpy (char *s1, const char *s2);

static int gdb_cris_strlen (const char *s);

static void *gdb_cris_memchr (const void *s, int c, int n);

static int gdb_cris_strtol (const char *s, char **endptr, int base);

static void copy_registers (registers *dptr, registers *sptr, int n);

static void copy_registers_from_stack (int thread_id, registers *reg);

static void copy_registers_to_stack (int thread_id, registers *reg);

static int write_register (int regno, char *val);

static write_stack_register (int thread_id, int regno, char *valptr);

static int read_register (char regno, unsigned int *valptr);

int getDebugChar (void);

void putDebugChar (int val);

void enableDebugIRQ (void);

static int hex (char ch);

static char *mem2hex (char *buf, unsigned char *mem, int count);

/* Convert the array, in hexadecimal representation, pointed to by buf into
   binary representation. Put the result in mem, and return a pointer to
   the character after the last byte written. */
static unsigned char *hex2mem (unsigned char *mem, char *buf, int count);

/* Put the content of the array, in binary representation, pointed to by buf
   into memory pointed to by mem, and return a pointer to
   the character after the last byte written. */
static unsigned char *bin2mem (unsigned char *mem, unsigned char *buf, int count);

static void getpacket (char *buffer);

static void putpacket (char *buffer);

static void stub_is_stopped (int sigval);

static void handle_exception (int sigval);

static void kill_restart (void);


void putDebugString (const unsigned char *str, int length); 

void handle_breakpoint (void);                          

void handle_interrupt (void);                           

void breakpoint (void);                                 

extern unsigned char executing_task;

#define HEXCHARS_IN_THREAD_ID 16

#define USEDVAR(name)    { if (name) { ; } }
#define USEDFUN(name) { void (*pf)(void) = (void *)name; USEDVAR(pf) }

#define BUFMAX 512

#define RUNLENMAX 64

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

enum error_type
{
	SUCCESS, E01, E02, E03, E04, E05, E06, E07
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
	"E07 Change register content - P - the register is not stored on the stack"
};
enum register_name
{
	R0,  R1,   R2,  R3,
	R4,  R5,   R6,  R7,
	R8,  R9,   R10, R11,
	R12, R13,  SP,  PC,
	P0,  VR,   P2,  P3,
	P4,  CCR,  P6,  MOF,
	P8,  IBR,  IRP, SRP,
	BAR, DCCR, BRP, USP
};

static int register_size[] =
{
	4, 4, 4, 4,
	4, 4, 4, 4,
	4, 4, 4, 4,
	4, 4, 4, 4,
	1, 1, 0, 0,
	2, 2, 0, 4,
	4, 4, 4, 4,
	4, 4, 4, 4
};

static registers reg;

static int consistency_status = SUCCESS;


static int current_thread_c = 0;
static int current_thread_g = 0;

static registers reg_g;

#define INTERNAL_STACK_SIZE 1024
static char internal_stack[INTERNAL_STACK_SIZE];

static unsigned char is_dyn_brkp = 0;


static char*
gdb_cris_strcpy (char *s1, const char *s2)
{
	char *s = s1;
	
	for (s = s1; (*s++ = *s2++) != '\0'; )
		;
	return (s1);
}

static int
gdb_cris_strlen (const char *s)
{
	const char *sc;
	
	for (sc = s; *sc != '\0'; sc++)
		;
	return (sc - s);
}

static void*
gdb_cris_memchr (const void *s, int c, int n)
{
	const unsigned char uc = c;
	const unsigned char *su;
	
	for (su = s; 0 < n; ++su, --n)
		if (*su == uc)
			return ((void *)su);
	return (NULL);
}
static int
gdb_cris_strtol (const char *s, char **endptr, int base)
{
	char *s1;
	char *sd;
	int x = 0;
	
	for (s1 = (char*)s; (sd = gdb_cris_memchr(hex_asc, *s1, base)) != NULL; ++s1)
		x = x * base + (sd - hex_asc);
        
        if (endptr)
        {
                
                *endptr = s1;
        }
        
	return x;
}

static void
copy_registers (registers *dptr, registers *sptr, int n)
{
	unsigned char *dreg;
	unsigned char *sreg;
	
	for (dreg = (unsigned char*)dptr, sreg = (unsigned char*)sptr; n > 0; n--)
		*dreg++ = *sreg++;
}

#ifdef PROCESS_SUPPORT
static void
copy_registers_from_stack (int thread_id, registers *regptr)
{
	int j;
	stack_registers *s = (stack_registers *)stack_list[thread_id];
	unsigned int *d = (unsigned int *)regptr;
	
	for (j = 13; j >= 0; j--)
		*d++ = s->r[j];
	regptr->sp = (unsigned int)stack_list[thread_id];
	regptr->pc = s->pc;
	regptr->dccr = s->dccr;
	regptr->srp = s->srp;
}

static void
copy_registers_to_stack (int thread_id, registers *regptr)
{
	int i;
	stack_registers *d = (stack_registers *)stack_list[thread_id];
	unsigned int *s = (unsigned int *)regptr;
	
	for (i = 0; i < 14; i++) {
		d->r[i] = *s++;
	}
	d->pc = regptr->pc;
	d->dccr = regptr->dccr;
	d->srp = regptr->srp;
}
#endif

static int
write_register (int regno, char *val)
{
	int status = SUCCESS;
	registers *current_reg = &reg;

        if (regno >= R0 && regno <= PC) {
		
		hex2mem ((unsigned char *)current_reg + regno * sizeof(unsigned int),
			 val, sizeof(unsigned int));
	}
        else if (regno == P0 || regno == VR || regno == P4 || regno == P8) {
		
		status = E02;
	}
        else if (regno == CCR) {
		hex2mem ((unsigned char *)&(current_reg->ccr) + (regno-CCR) * sizeof(unsigned short),
			 val, sizeof(unsigned short));
	}
	else if (regno >= MOF && regno <= USP) {
		
		hex2mem ((unsigned char *)&(current_reg->ibr) + (regno-IBR) * sizeof(unsigned int),
			 val, sizeof(unsigned int));
	} 
        else {
		
		status = E05;
	}
	return status;
}

#ifdef PROCESS_SUPPORT
static int
write_stack_register (int thread_id, int regno, char *valptr)
{
	int status = SUCCESS;
	stack_registers *d = (stack_registers *)stack_list[thread_id];
	unsigned int val;
	
	hex2mem ((unsigned char *)&val, valptr, sizeof(unsigned int));
	if (regno >= R0 && regno < SP) {
		d->r[regno] = val;
	}
	else if (regno == SP) {
		stack_list[thread_id] = val;
	}
	else if (regno == PC) {
		d->pc = val;
	}
	else if (regno == SRP) {
		d->srp = val;
	}
	else if (regno == DCCR) {
		d->dccr = val;
	}
	else {
		
		status = E07;
	}
	return status;
}
#endif

static int
read_register (char regno, unsigned int *valptr)
{
	registers *current_reg = &reg;

	if (regno >= R0 && regno <= PC) {
		
		*valptr = *(unsigned int *)((char *)current_reg + regno * sizeof(unsigned int));
                return SUCCESS;
	}
	else if (regno == P0 || regno == VR) {
		
		*valptr = (unsigned int)(*(unsigned char *)
                                         ((char *)&(current_reg->p0) + (regno-P0) * sizeof(char)));
                return SUCCESS;
	}
	else if (regno == P4 || regno == CCR) {
		
		*valptr = (unsigned int)(*(unsigned short *)
                                         ((char *)&(current_reg->p4) + (regno-P4) * sizeof(unsigned short)));
                return SUCCESS;
	}
	else if (regno >= MOF && regno <= USP) {
		
		*valptr = *(unsigned int *)((char *)&(current_reg->p8)
                                            + (regno-P8) * sizeof(unsigned int));
                return SUCCESS;
	}
	else {
		
		consistency_status = E05;
		return E05;
	}
}

static int
hex (char ch)
{
	if ((ch >= 'a') && (ch <= 'f'))
		return (ch - 'a' + 10);
	if ((ch >= '0') && (ch <= '9'))
		return (ch - '0');
	if ((ch >= 'A') && (ch <= 'F'))
		return (ch - 'A' + 10);
	return (-1);
}


static int do_printk = 0;

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
	return (buf);
}

/* Convert the array, in hexadecimal representation, pointed to by buf into
   binary representation. Put the result in mem, and return a pointer to
   the character after the last byte written. */
static unsigned char*
hex2mem (unsigned char *mem, char *buf, int count)
{
	int i;
	unsigned char ch;
	for (i = 0; i < count; i++) {
		ch = hex (*buf++) << 4;
		ch = ch + hex (*buf++);
		*mem++ = ch;
	}
	return (mem);
}

/* Put the content of the array, in binary representation, pointed to by buf
   into memory pointed to by mem, and return a pointer to the character after
   the last byte written.
   Gdb will escape $, #, and the escape char (0x7d). */
static unsigned char*
bin2mem (unsigned char *mem, unsigned char *buf, int count)
{
	int i;
	unsigned char *next;
	for (i = 0; i < count; i++) {
		if (*buf == 0x7d) {
			next = buf + 1;
			if (*next == 0x3 || *next == 0x4 || *next == 0x5D) 
				{
					buf++;
					*buf += 0x20;
				}
		}
		*mem++ = *buf++;
	}
	return (mem);
}

static void
getpacket (char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	char ch;
	do {
		while ((ch = getDebugChar ()) != '$')
			;
		checksum = 0;
		xmitcsum = -1;
		count = 0;
		
		while (count < BUFMAX) {
			ch = getDebugChar ();
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}
		buffer[count] = '\0';
		
		if (ch == '#') {
			xmitcsum = hex (getDebugChar ()) << 4;
			xmitcsum += hex (getDebugChar ());
			if (checksum != xmitcsum) {
				
				putDebugChar ('-');
			}
			else {
				
				putDebugChar ('+');
				
				if (buffer[2] == ':') {
					putDebugChar (buffer[0]);
					putDebugChar (buffer[1]);
					
					count = gdb_cris_strlen (buffer);
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
		putDebugChar ('$');
		checksum = 0;
		while (*src) {
			
			putDebugChar (*src);
			checksum += *src;
			runlen = 0;
			while (runlen < RUNLENMAX && *src == src[runlen]) {
				runlen++;
			}
			if (runlen > 3) {
				
				putDebugChar ('*');
				checksum += '*';
				encode = runlen + ' ' - 4;
				putDebugChar (encode);
				checksum += encode;
				src += runlen;
			}
			else {
				src++;
			}
		}
		putDebugChar('#');
		putDebugChar(hex_asc_hi(checksum));
		putDebugChar(hex_asc_lo(checksum));
	} while(kgdb_started && (getDebugChar() != '+'));
}

void
putDebugString (const unsigned char *str, int length)
{
        remcomOutBuffer[0] = 'O';
        mem2hex(&remcomOutBuffer[1], (unsigned char *)str, length);
        putpacket(remcomOutBuffer);
}

static void
stub_is_stopped(int sigval)
{
	char *ptr = remcomOutBuffer;
	int regno;

	unsigned int reg_cont;
	int status;
        
	

	*ptr++ = 'T';
	ptr = hex_byte_pack(ptr, sigval);

	
	for (regno = R0; regno <= USP; regno++) {
		

                status = read_register (regno, &reg_cont);
                
		if (status == SUCCESS) {
			ptr = hex_byte_pack(ptr, regno);
                        *ptr++ = ':';

                        ptr = mem2hex(ptr, (unsigned char *)&reg_cont,
                                      register_size[regno]);
                        *ptr++ = ';';
                }
                
	}

#ifdef PROCESS_SUPPORT

	current_thread_c = executing_task;
	current_thread_g = executing_task;

	copy_registers (&reg_g, &reg, sizeof(registers));

	
	gdb_cris_strcpy (&remcomOutBuffer[pos], "thread:");
	pos += gdb_cris_strlen ("thread:");
	remcomOutBuffer[pos++] = hex_asc_hi(executing_task);
	remcomOutBuffer[pos++] = hex_asc_lo(executing_task);
	gdb_cris_strcpy (&remcomOutBuffer[pos], ";");
#endif

	

	*ptr = 0;

	putpacket (remcomOutBuffer);
}

static void
handle_exception (int sigval)
{
	

	USEDFUN(handle_exception);
	USEDVAR(internal_stack[0]);

	

	stub_is_stopped (sigval);

	for (;;) {
		remcomOutBuffer[0] = '\0';
		getpacket (remcomInBuffer);
		switch (remcomInBuffer[0]) {
			case 'g':
				
				{
#ifdef PROCESS_SUPPORT
					
					copy_registers (&reg_g, &reg, sizeof(registers));
					
					if (current_thread_g != executing_task) {
						copy_registers_from_stack (current_thread_g, &reg_g);
					}
					mem2hex ((unsigned char *)remcomOutBuffer, (unsigned char *)&reg_g, sizeof(registers));
#else
					mem2hex(remcomOutBuffer, (char *)&reg, sizeof(registers));
#endif
				}
				break;
				
			case 'G':
#ifdef PROCESS_SUPPORT
				hex2mem ((unsigned char *)&reg_g, &remcomInBuffer[1], sizeof(registers));
				if (current_thread_g == executing_task) {
					copy_registers (&reg, &reg_g, sizeof(registers));
				}
				else {
					copy_registers_to_stack(current_thread_g, &reg_g);
				}
#else
				hex2mem((char *)&reg, &remcomInBuffer[1], sizeof(registers));
#endif
				gdb_cris_strcpy (remcomOutBuffer, "OK");
				break;
				
			case 'P':
				{
					char *suffix;
					int regno = gdb_cris_strtol (&remcomInBuffer[1], &suffix, 16);
					int status;
#ifdef PROCESS_SUPPORT
					if (current_thread_g != executing_task)
						status = write_stack_register (current_thread_g, regno, suffix+1);
					else
#endif
						status = write_register (regno, suffix+1);

					switch (status) {
						case E02:
							
							gdb_cris_strcpy (remcomOutBuffer, error_message[E02]);
							break;
						case E05:
							
							gdb_cris_strcpy (remcomOutBuffer, error_message[E05]);
							break;
						case E07:
							
							gdb_cris_strcpy (remcomOutBuffer, error_message[E07]);
							break;
						default:
							
							gdb_cris_strcpy (remcomOutBuffer, "OK");
							break;
					}
				}
				break;
				
			case 'm':
				{
                                        char *suffix;
					unsigned char *addr = (unsigned char *)gdb_cris_strtol(&remcomInBuffer[1],
                                                                                               &suffix, 16);                                        int length = gdb_cris_strtol(suffix+1, 0, 16);
                                        
                                        mem2hex(remcomOutBuffer, addr, length);
                                }
				break;
				
			case 'X':
			case 'M':
				{
					char *lenptr;
					char *dataptr;
					unsigned char *addr = (unsigned char *)gdb_cris_strtol(&remcomInBuffer[1],
										      &lenptr, 16);
					int length = gdb_cris_strtol(lenptr+1, &dataptr, 16);
					if (*lenptr == ',' && *dataptr == ':') {
						if (remcomInBuffer[0] == 'M') {
							hex2mem(addr, dataptr + 1, length);
						}
						else  {
							bin2mem(addr, dataptr + 1, length);
						}
						gdb_cris_strcpy (remcomOutBuffer, "OK");
					}
					else {
						gdb_cris_strcpy (remcomOutBuffer, error_message[E06]);
					}
				}
				break;
				
			case 'c':
				if (remcomInBuffer[1] != '\0') {
					reg.pc = gdb_cris_strtol (&remcomInBuffer[1], 0, 16);
				}
				enableDebugIRQ();
				return;
				
			case 's':
				gdb_cris_strcpy (remcomOutBuffer, error_message[E04]);
				putpacket (remcomOutBuffer);
				return;
				
			case '?':
				remcomOutBuffer[0] = 'S';
				remcomOutBuffer[1] = hex_asc_hi(sigval);
				remcomOutBuffer[2] = hex_asc_lo(sigval);
				remcomOutBuffer[3] = 0;
				break;
				
			case 'D':
				putpacket ("OK");
				return;
				
			case 'k':
			case 'r':
				kill_restart ();
				break;
				
			case 'C':
			case 'S':
			case '!':
			case 'R':
			case 'd':
				gdb_cris_strcpy (remcomOutBuffer, error_message[E04]);
				break;
#ifdef PROCESS_SUPPORT

			case 'T':
				{
					int thread_id = (int)gdb_cris_strtol (&remcomInBuffer[1], 0, 16);
					
					if (thread_id >= 0 && thread_id < number_of_tasks)
						gdb_cris_strcpy (remcomOutBuffer, "OK");
				}
				break;
								
			case 'H':
				{
					int thread_id = gdb_cris_strtol (&remcomInBuffer[2], 0, 16);
					if (remcomInBuffer[1] == 'c') {
						
						gdb_cris_strcpy (remcomOutBuffer, "OK");
					}
					else if (remcomInBuffer[1] == 'g') {
						if (thread_id >= 0 && thread_id < number_of_tasks) {
							current_thread_g = thread_id;
							gdb_cris_strcpy (remcomOutBuffer, "OK");
						}
						else {
							
							gdb_cris_strcpy (remcomOutBuffer, error_message[E01]);
						}
					}
					else {
						
						gdb_cris_strcpy (remcomOutBuffer, error_message[E01]);
					}
				}
				break;
				
			case 'q':
			case 'Q':
				{
					int pos;
					int nextpos;
					int thread_id;
					
					switch (remcomInBuffer[1]) {
						case 'C':
							
							gdb_cris_strcpy (&remcomOutBuffer[0], "QC");
							remcomOutBuffer[2] = hex_asc_hi(current_thread_c);
							remcomOutBuffer[3] = hex_asc_lo(current_thread_c);
							remcomOutBuffer[4] = '\0';
							break;
						case 'L':
							gdb_cris_strcpy (&remcomOutBuffer[0], "QM");
							
							if (os_is_started()) {
								remcomOutBuffer[2] = hex_asc_hi(number_of_tasks);
								remcomOutBuffer[3] = hex_asc_lo(number_of_tasks);
							}
							else {
								remcomOutBuffer[2] = hex_asc_hi(0);
								remcomOutBuffer[3] = hex_asc_lo(1);
							}
							
							remcomOutBuffer[4] = hex_asc_lo(1);
							pos = 5;
							
							for (; pos < (5 + HEXCHARS_IN_THREAD_ID); pos++)
								remcomOutBuffer[pos] = remcomInBuffer[pos];
							
							if (os_is_started()) {
								
								for (thread_id = 0; thread_id < number_of_tasks; thread_id++) {
									nextpos = pos + HEXCHARS_IN_THREAD_ID - 1;
									for (; pos < nextpos; pos ++)
										remcomOutBuffer[pos] = hex_asc_lo(0);
									remcomOutBuffer[pos++] = hex_asc_lo(thread_id);
								}
							}
							else {
								
								nextpos = pos + HEXCHARS_IN_THREAD_ID - 1;
								for (; pos < nextpos; pos ++)
									remcomOutBuffer[pos] = hex_asc_lo(0);
								remcomOutBuffer[pos++] = hex_asc_lo(current_thread_c);
							}
							remcomOutBuffer[pos] = '\0';
							break;
						default:
							
							
							remcomOutBuffer[0] = 0;
							break;
					}
				}
				break;
#endif 
				
			default:
				remcomOutBuffer[0] = 0;
				break;
		}
		putpacket(remcomOutBuffer);
	}
}

static void
kill_restart ()
{
	machine_restart("");
}


void kgdb_handle_breakpoint(void);

asm ("
  .global kgdb_handle_breakpoint
kgdb_handle_breakpoint:
;;
;; Response to the break-instruction
;;
;; Create a register image of the caller
;;
  move     $dccr,[reg+0x5E] ; Save the flags in DCCR before disable interrupts
  di                        ; Disable interrupts
  move.d   $r0,[reg]        ; Save R0
  move.d   $r1,[reg+0x04]   ; Save R1
  move.d   $r2,[reg+0x08]   ; Save R2
  move.d   $r3,[reg+0x0C]   ; Save R3
  move.d   $r4,[reg+0x10]   ; Save R4
  move.d   $r5,[reg+0x14]   ; Save R5
  move.d   $r6,[reg+0x18]   ; Save R6
  move.d   $r7,[reg+0x1C]   ; Save R7
  move.d   $r8,[reg+0x20]   ; Save R8
  move.d   $r9,[reg+0x24]   ; Save R9
  move.d   $r10,[reg+0x28]  ; Save R10
  move.d   $r11,[reg+0x2C]  ; Save R11
  move.d   $r12,[reg+0x30]  ; Save R12
  move.d   $r13,[reg+0x34]  ; Save R13
  move.d   $sp,[reg+0x38]   ; Save SP (R14)
;; Due to the old assembler-versions BRP might not be recognized
  .word 0xE670              ; move brp,$r0
  subq     2,$r0             ; Set to address of previous instruction.
  move.d   $r0,[reg+0x3c]   ; Save the address in PC (R15)
  clear.b  [reg+0x40]      ; Clear P0
  move     $vr,[reg+0x41]   ; Save special register P1
  clear.w  [reg+0x42]      ; Clear P4
  move     $ccr,[reg+0x44]  ; Save special register CCR
  move     $mof,[reg+0x46]  ; P7
  clear.d  [reg+0x4A]      ; Clear P8
  move     $ibr,[reg+0x4E]  ; P9,
  move     $irp,[reg+0x52]  ; P10,
  move     $srp,[reg+0x56]  ; P11,
  move     $dtp0,[reg+0x5A] ; P12, register BAR, assembler might not know BAR
                            ; P13, register DCCR already saved
;; Due to the old assembler-versions BRP might not be recognized
  .word 0xE670              ; move brp,r0
;; Static (compiled) breakpoints must return to the next instruction in order
;; to avoid infinite loops. Dynamic (gdb-invoked) must restore the instruction
;; in order to execute it when execution is continued.
  test.b   [is_dyn_brkp]    ; Is this a dynamic breakpoint?
  beq      is_static         ; No, a static breakpoint
  nop
  subq     2,$r0              ; rerun the instruction the break replaced
is_static:
  moveq    1,$r1
  move.b   $r1,[is_dyn_brkp] ; Set the state variable to dynamic breakpoint
  move.d   $r0,[reg+0x62]    ; Save the return address in BRP
  move     $usp,[reg+0x66]   ; USP
;;
;; Handle the communication
;;
  move.d   internal_stack+1020,$sp ; Use the internal stack which grows upward
  moveq    5,$r10                   ; SIGTRAP
  jsr      handle_exception       ; Interactive routine
;;
;; Return to the caller
;;
   move.d  [reg],$r0         ; Restore R0
   move.d  [reg+0x04],$r1    ; Restore R1
   move.d  [reg+0x08],$r2    ; Restore R2
   move.d  [reg+0x0C],$r3    ; Restore R3
   move.d  [reg+0x10],$r4    ; Restore R4
   move.d  [reg+0x14],$r5    ; Restore R5
   move.d  [reg+0x18],$r6    ; Restore R6
   move.d  [reg+0x1C],$r7    ; Restore R7
   move.d  [reg+0x20],$r8    ; Restore R8
   move.d  [reg+0x24],$r9    ; Restore R9
   move.d  [reg+0x28],$r10   ; Restore R10
   move.d  [reg+0x2C],$r11   ; Restore R11
   move.d  [reg+0x30],$r12   ; Restore R12
   move.d  [reg+0x34],$r13   ; Restore R13
;;
;; FIXME: Which registers should be restored?
;;
   move.d  [reg+0x38],$sp    ; Restore SP (R14)
   move    [reg+0x56],$srp   ; Restore the subroutine return pointer.
   move    [reg+0x5E],$dccr  ; Restore DCCR
   move    [reg+0x66],$usp   ; Restore USP
   jump    [reg+0x62]       ; A jump to the content in register BRP works.
   nop                       ;
");


void kgdb_handle_serial(void);

asm ("
  .global kgdb_handle_serial
kgdb_handle_serial:
;;
;; Response to a serial interrupt
;;

  move     $dccr,[reg+0x5E] ; Save the flags in DCCR
  di                        ; Disable interrupts
  move.d   $r0,[reg]        ; Save R0
  move.d   $r1,[reg+0x04]   ; Save R1
  move.d   $r2,[reg+0x08]   ; Save R2
  move.d   $r3,[reg+0x0C]   ; Save R3
  move.d   $r4,[reg+0x10]   ; Save R4
  move.d   $r5,[reg+0x14]   ; Save R5
  move.d   $r6,[reg+0x18]   ; Save R6
  move.d   $r7,[reg+0x1C]   ; Save R7
  move.d   $r8,[reg+0x20]   ; Save R8
  move.d   $r9,[reg+0x24]   ; Save R9
  move.d   $r10,[reg+0x28]  ; Save R10
  move.d   $r11,[reg+0x2C]  ; Save R11
  move.d   $r12,[reg+0x30]  ; Save R12
  move.d   $r13,[reg+0x34]  ; Save R13
  move.d   $sp,[reg+0x38]   ; Save SP (R14)
  move     $irp,[reg+0x3c]  ; Save the address in PC (R15)
  clear.b  [reg+0x40]      ; Clear P0
  move     $vr,[reg+0x41]   ; Save special register P1,
  clear.w  [reg+0x42]      ; Clear P4
  move     $ccr,[reg+0x44]  ; Save special register CCR
  move     $mof,[reg+0x46]  ; P7
  clear.d  [reg+0x4A]      ; Clear P8
  move     $ibr,[reg+0x4E]  ; P9,
  move     $irp,[reg+0x52]  ; P10,
  move     $srp,[reg+0x56]  ; P11,
  move     $dtp0,[reg+0x5A] ; P12, register BAR, assembler might not know BAR
                            ; P13, register DCCR already saved
;; Due to the old assembler-versions BRP might not be recognized
  .word 0xE670              ; move brp,r0
  move.d   $r0,[reg+0x62]   ; Save the return address in BRP
  move     $usp,[reg+0x66]  ; USP

;; get the serial character (from debugport.c) and check if it is a ctrl-c

  jsr getDebugChar
  cmp.b 3, $r10
  bne goback
  nop

  move.d  [reg+0x5E], $r10		; Get DCCR
  btstq	   8, $r10			; Test the U-flag.
  bmi	   goback
  nop

;;
;; Handle the communication
;;
  move.d   internal_stack+1020,$sp ; Use the internal stack
  moveq    2,$r10                   ; SIGINT
  jsr      handle_exception       ; Interactive routine

goback:
;;
;; Return to the caller
;;
   move.d  [reg],$r0         ; Restore R0
   move.d  [reg+0x04],$r1    ; Restore R1
   move.d  [reg+0x08],$r2    ; Restore R2
   move.d  [reg+0x0C],$r3    ; Restore R3
   move.d  [reg+0x10],$r4    ; Restore R4
   move.d  [reg+0x14],$r5    ; Restore R5
   move.d  [reg+0x18],$r6    ; Restore R6
   move.d  [reg+0x1C],$r7    ; Restore R7
   move.d  [reg+0x20],$r8    ; Restore R8
   move.d  [reg+0x24],$r9    ; Restore R9
   move.d  [reg+0x28],$r10   ; Restore R10
   move.d  [reg+0x2C],$r11   ; Restore R11
   move.d  [reg+0x30],$r12   ; Restore R12
   move.d  [reg+0x34],$r13   ; Restore R13
;;
;; FIXME: Which registers should be restored?
;;
   move.d  [reg+0x38],$sp    ; Restore SP (R14)
   move    [reg+0x56],$srp   ; Restore the subroutine return pointer.
   move    [reg+0x5E],$dccr  ; Restore DCCR
   move    [reg+0x66],$usp   ; Restore USP
   reti                      ; Return from the interrupt routine
   nop
");


void
breakpoint(void)
{
	kgdb_started = 1;
	is_dyn_brkp = 0;     
	__asm__ volatile ("break 8"); 
}


void
kgdb_init(void)
{
	

        
	set_int_vector(8, kgdb_handle_serial);
	
	enableDebugIRQ();
}

