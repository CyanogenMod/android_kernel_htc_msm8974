
#include "misc.h"



#define STATIC		static

#undef memset
#undef memcpy
#define memzero(s, n)	memset((s), 0, (n))


static void error(char *m);

struct boot_params *real_mode;		
static int quiet;
static int debug;

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

#ifdef CONFIG_X86_64
#define memptr long
#else
#define memptr unsigned
#endif

static memptr free_mem_ptr;
static memptr free_mem_end_ptr;

static char *vidmem;
static int vidport;
static int lines, cols;

#ifdef CONFIG_KERNEL_GZIP
#include "../../../../lib/decompress_inflate.c"
#endif

#ifdef CONFIG_KERNEL_BZIP2
#include "../../../../lib/decompress_bunzip2.c"
#endif

#ifdef CONFIG_KERNEL_LZMA
#include "../../../../lib/decompress_unlzma.c"
#endif

#ifdef CONFIG_KERNEL_XZ
#include "../../../../lib/decompress_unxz.c"
#endif

#ifdef CONFIG_KERNEL_LZO
#include "../../../../lib/decompress_unlzo.c"
#endif

static void scroll(void)
{
	int i;

	memcpy(vidmem, vidmem + cols * 2, (lines - 1) * cols * 2);
	for (i = (lines - 1) * cols * 2; i < lines * cols * 2; i += 2)
		vidmem[i] = ' ';
}

#define XMTRDY          0x20

#define TXR             0       
#define LSR             5       
static void serial_putchar(int ch)
{
	unsigned timeout = 0xffff;

	while ((inb(early_serial_base + LSR) & XMTRDY) == 0 && --timeout)
		cpu_relax();

	outb(ch, early_serial_base + TXR);
}

void __putstr(int error, const char *s)
{
	int x, y, pos;
	char c;

#ifndef CONFIG_X86_VERBOSE_BOOTUP
	if (!error)
		return;
#endif
	if (early_serial_base) {
		const char *str = s;
		while (*str) {
			if (*str == '\n')
				serial_putchar('\r');
			serial_putchar(*str++);
		}
	}

	if (real_mode->screen_info.orig_video_mode == 0 &&
	    lines == 0 && cols == 0)
		return;

	x = real_mode->screen_info.orig_x;
	y = real_mode->screen_info.orig_y;

	while ((c = *s++) != '\0') {
		if (c == '\n') {
			x = 0;
			if (++y >= lines) {
				scroll();
				y--;
			}
		} else {
			vidmem[(x + cols * y) * 2] = c;
			if (++x >= cols) {
				x = 0;
				if (++y >= lines) {
					scroll();
					y--;
				}
			}
		}
	}

	real_mode->screen_info.orig_x = x;
	real_mode->screen_info.orig_y = y;

	pos = (x + cols * y) * 2;	
	outb(14, vidport);
	outb(0xff & (pos >> 9), vidport+1);
	outb(15, vidport);
	outb(0xff & (pos >> 1), vidport+1);
}

void *memset(void *s, int c, size_t n)
{
	int i;
	char *ss = s;

	for (i = 0; i < n; i++)
		ss[i] = c;
	return s;
}
#ifdef CONFIG_X86_32
void *memcpy(void *dest, const void *src, size_t n)
{
	int d0, d1, d2;
	asm volatile(
		"rep ; movsl\n\t"
		"movl %4,%%ecx\n\t"
		"rep ; movsb\n\t"
		: "=&c" (d0), "=&D" (d1), "=&S" (d2)
		: "0" (n >> 2), "g" (n & 3), "1" (dest), "2" (src)
		: "memory");

	return dest;
}
#else
void *memcpy(void *dest, const void *src, size_t n)
{
	long d0, d1, d2;
	asm volatile(
		"rep ; movsq\n\t"
		"movq %4,%%rcx\n\t"
		"rep ; movsb\n\t"
		: "=&c" (d0), "=&D" (d1), "=&S" (d2)
		: "0" (n >> 3), "g" (n & 7), "1" (dest), "2" (src)
		: "memory");

	return dest;
}
#endif

static void error(char *x)
{
	__putstr(1, "\n\n");
	__putstr(1, x);
	__putstr(1, "\n\n -- System halted");

	while (1)
		asm("hlt");
}

static void parse_elf(void *output)
{
#ifdef CONFIG_X86_64
	Elf64_Ehdr ehdr;
	Elf64_Phdr *phdrs, *phdr;
#else
	Elf32_Ehdr ehdr;
	Elf32_Phdr *phdrs, *phdr;
#endif
	void *dest;
	int i;

	memcpy(&ehdr, output, sizeof(ehdr));
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
	   ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	   ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
	   ehdr.e_ident[EI_MAG3] != ELFMAG3) {
		error("Kernel is not a valid ELF file");
		return;
	}

	if (!quiet)
		putstr("Parsing ELF... ");

	phdrs = malloc(sizeof(*phdrs) * ehdr.e_phnum);
	if (!phdrs)
		error("Failed to allocate space for phdrs");

	memcpy(phdrs, output + ehdr.e_phoff, sizeof(*phdrs) * ehdr.e_phnum);

	for (i = 0; i < ehdr.e_phnum; i++) {
		phdr = &phdrs[i];

		switch (phdr->p_type) {
		case PT_LOAD:
#ifdef CONFIG_RELOCATABLE
			dest = output;
			dest += (phdr->p_paddr - LOAD_PHYSICAL_ADDR);
#else
			dest = (void *)(phdr->p_paddr);
#endif
			memcpy(dest,
			       output + phdr->p_offset,
			       phdr->p_filesz);
			break;
		default:  break;
		}
	}

	free(phdrs);
}

asmlinkage void decompress_kernel(void *rmode, memptr heap,
				  unsigned char *input_data,
				  unsigned long input_len,
				  unsigned char *output)
{
	real_mode = rmode;

	if (cmdline_find_option_bool("quiet"))
		quiet = 1;
	if (cmdline_find_option_bool("debug"))
		debug = 1;

	if (real_mode->screen_info.orig_video_mode == 7) {
		vidmem = (char *) 0xb0000;
		vidport = 0x3b4;
	} else {
		vidmem = (char *) 0xb8000;
		vidport = 0x3d4;
	}

	lines = real_mode->screen_info.orig_video_lines;
	cols = real_mode->screen_info.orig_video_cols;

	console_init();
	if (debug)
		putstr("early console in decompress_kernel\n");

	free_mem_ptr     = heap;	
	free_mem_end_ptr = heap + BOOT_HEAP_SIZE;

	if ((unsigned long)output & (MIN_KERNEL_ALIGN - 1))
		error("Destination address inappropriately aligned");
#ifdef CONFIG_X86_64
	if (heap > 0x3fffffffffffUL)
		error("Destination address too large");
#else
	if (heap > ((-__PAGE_OFFSET-(128<<20)-1) & 0x7fffffff))
		error("Destination address too large");
#endif
#ifndef CONFIG_RELOCATABLE
	if ((unsigned long)output != LOAD_PHYSICAL_ADDR)
		error("Wrong destination address");
#endif

	if (!quiet)
		putstr("\nDecompressing Linux... ");
	decompress(input_data, input_len, NULL, NULL, output, NULL, error);
	parse_elf(output);
	if (!quiet)
		putstr("done.\nBooting the kernel.\n");
	return;
}
