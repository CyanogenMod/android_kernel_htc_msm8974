/*
 * BRIEF MODULE DESCRIPTION
 *    PROM library initialisation code, assuming a version of
 *    pmon is the boot code.
 *
 * Copyright 2000,2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * This file was derived from Carsten Langgaard's
 * arch/mips/mips-boards/xx files.
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm-generic/sections.h>
#include <asm/page.h>

#include <msp_prom.h>
#include <msp_regs.h>

int prom_argc;
char **prom_argv, **prom_envp;
int *prom_vec;

int init_debug = 1;

struct prom_pmemblock mdesc[PROM_MAX_PMEMBLOCKS];

static char msp_default_features[] =
#if defined(CONFIG_PMC_MSP4200_EVAL) \
 || defined(CONFIG_PMC_MSP4200_GW)
	"ERER";
#elif defined(CONFIG_PMC_MSP7120_EVAL) \
 || defined(CONFIG_PMC_MSP7120_GW)
	"EMEMSP";
#elif defined(CONFIG_PMC_MSP7120_FPGA)
	"EMEM";
#endif

static inline unsigned char str2hexnum(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return 0; 
}

static inline int str2eaddr(unsigned char *ea, unsigned char *str)
{
	int index = 0;
	unsigned char num = 0;

	while (*str != '\0') {
		if ((*str == '.') || (*str == ':')) {
			ea[index++] = num;
			num = 0;
			str++;
		} else {
			num = num << 4;
			num |= str2hexnum(*str++);
		}
	}

	if (index == 5)	{
		ea[index++] = num;
		return 0;
	} else
		return -1;
}
EXPORT_SYMBOL(str2eaddr);

static inline unsigned long str2hex(unsigned char *str)
{
	int value = 0;

	while (*str) {
		value = value << 4;
		value |= str2hexnum(*str++);
	}

	return value;
}

const char *get_system_type(void)
{
#if defined(CONFIG_PMC_MSP4200_EVAL)
	return "PMC-Sierra MSP4200 Eval Board";
#elif defined(CONFIG_PMC_MSP4200_GW)
	return "PMC-Sierra MSP4200 VoIP Gateway";
#elif defined(CONFIG_PMC_MSP7120_EVAL)
	return "PMC-Sierra MSP7120 Eval Board";
#elif defined(CONFIG_PMC_MSP7120_GW)
	return "PMC-Sierra MSP7120 Residential Gateway";
#elif defined(CONFIG_PMC_MSP7120_FPGA)
	return "PMC-Sierra MSP7120 FPGA";
#else
	#error "What is the type of *your* MSP?"
#endif
}

int get_ethernet_addr(char *ethaddr_name, char *ethernet_addr)
{
	char *ethaddr_str;

	ethaddr_str = prom_getenv(ethaddr_name);
	if (!ethaddr_str) {
		printk(KERN_WARNING "%s not set in boot prom\n", ethaddr_name);
		return -1;
	}

	if (str2eaddr(ethernet_addr, ethaddr_str) == -1) {
		printk(KERN_WARNING "%s badly formatted-<%s>\n",
			ethaddr_name, ethaddr_str);
		return -1;
	}

	if (init_debug > 1) {
		int i;
		printk(KERN_DEBUG "get_ethernet_addr: for %s ", ethaddr_name);
		for (i = 0; i < 5; i++)
			printk(KERN_DEBUG "%02x:",
				(unsigned char)*(ethernet_addr+i));
		printk(KERN_DEBUG "%02x\n", *(ethernet_addr+i));
	}

	return 0;
}
EXPORT_SYMBOL(get_ethernet_addr);

static char *get_features(void)
{
	char *feature = prom_getenv(FEATURES);

	if (feature == NULL) {
		
		feature = msp_default_features;
	}

	return feature;
}

static char test_feature(char c)
{
	char *feature = get_features();

	while (*feature) {
		if (*feature++ == c)
			return *feature;
		feature++;
	}

	return FEATURE_NOEXIST;
}

unsigned long get_deviceid(void)
{
	char *deviceid = prom_getenv(DEVICEID);

	if (deviceid == NULL)
		return *DEV_ID_REG;
	else
		return str2hex(deviceid);
}

char identify_pci(void)
{
	return test_feature(PCI_KEY);
}
EXPORT_SYMBOL(identify_pci);

char identify_pcimux(void)
{
	return test_feature(PCIMUX_KEY);
}

char identify_sec(void)
{
	return test_feature(SEC_KEY);
}
EXPORT_SYMBOL(identify_sec);

char identify_spad(void)
{
	return test_feature(SPAD_KEY);
}
EXPORT_SYMBOL(identify_spad);

char identify_tdm(void)
{
	return test_feature(TDM_KEY);
}
EXPORT_SYMBOL(identify_tdm);

char identify_zsp(void)
{
	return test_feature(ZSP_KEY);
}
EXPORT_SYMBOL(identify_zsp);

static char identify_enetfeature(char key, unsigned long interface_num)
{
	char *feature = get_features();

	while (*feature) {
		if (*feature++ == key && interface_num-- == 0)
			return *feature;
		feature++;
	}

	return FEATURE_NOEXIST;
}

char identify_enet(unsigned long interface_num)
{
	return identify_enetfeature(ENET_KEY, interface_num);
}
EXPORT_SYMBOL(identify_enet);

char identify_enetTxD(unsigned long interface_num)
{
	return identify_enetfeature(ENETTXD_KEY, interface_num);
}
EXPORT_SYMBOL(identify_enetTxD);

unsigned long identify_family(void)
{
	unsigned long deviceid;

	deviceid = get_deviceid();

	return deviceid & CPU_DEVID_FAMILY;
}
EXPORT_SYMBOL(identify_family);

unsigned long identify_revision(void)
{
	unsigned long deviceid;

	deviceid = get_deviceid();

	return deviceid & CPU_DEVID_REVISION;
}
EXPORT_SYMBOL(identify_revision);

char *prom_getenv(char *env_name)
{

	char **var = prom_envp;
	int i = strlen(env_name);

	while (*var) {
		if (strncmp(env_name, *var, i) == 0) {
			return (*var + strlen(env_name) + 1);
		}
		var++;
	}

	return NULL;
}

void  __init prom_init_cmdline(void)
{
	char *cp;
	int actr;

	actr = 1; 

	cp = &(arcs_cmdline[0]);
	while (actr < prom_argc) {
		strcpy(cp, prom_argv[actr]);
		cp += strlen(prom_argv[actr]);
		*cp++ = ' ';
		actr++;
	}
	if (cp != &(arcs_cmdline[0])) 
		--cp;
	*cp = '\0';
}

static int __init prom_memtype_classify(unsigned int type)
{
	switch (type) {
	case yamon_free:
		return BOOT_MEM_RAM;
	case yamon_prom:
		return BOOT_MEM_ROM_DATA;
	default:
		return BOOT_MEM_RESERVED;
	}
}

void __init prom_meminit(void)
{
	struct prom_pmemblock *p;

	p = prom_getmdesc();

	while (p->size) {
		long type;
		unsigned long base, size;

		type = prom_memtype_classify(p->type);
		base = p->base;
		size = p->size;

		add_memory_region(base, size, type);
		p++;
	}
}

void __init prom_free_prom_memory(void)
{
	int	argc;
	char	**argv;
	char	**envp;
	char	*ptr;
	int	len = 0;
	int	i;
	unsigned long addr;

	for (argc = 0; argc < prom_argc; argc++) {
		len += sizeof(char *);			
		len += strlen(prom_argv[argc]) + 1;	
	}
	len += sizeof(char *);		

	argv = kmalloc(len, GFP_KERNEL);
	ptr = (char *) &argv[prom_argc + 1];	

	for (argc = 0; argc < prom_argc; argc++) {
		argv[argc] = ptr;
		strcpy(ptr, prom_argv[argc]);
		ptr += strlen(prom_argv[argc]) + 1;
	}
	argv[prom_argc] = NULL;		
	prom_argv = argv;

	
	len = 0;
	i = 0;
	for (envp = prom_envp; *envp != NULL; envp++) {
		i++;		
		len += sizeof(char *);		
		len += strlen(*envp) + 1;	
	}
	len += sizeof(char *);		

	envp = kmalloc(len, GFP_KERNEL);
	ptr = (char *) &envp[i+1];

	for (argc = 0; argc < i; argc++) {
		envp[argc] = ptr;
		strcpy(ptr, prom_envp[argc]);
		ptr += strlen(prom_envp[argc]) + 1;
	}
	envp[i] = NULL;			
	prom_envp = envp;

	for (i = 0; i < boot_mem_map.nr_map; i++) {
		if (boot_mem_map.map[i].type != BOOT_MEM_ROM_DATA)
			continue;

		addr = boot_mem_map.map[i].addr;
		free_init_pages("prom memory",
				addr, addr + boot_mem_map.map[i].size);
	}
}

struct prom_pmemblock *__init prom_getmdesc(void)
{
	static char	memsz_env[] __initdata = "memsize";
	static char	heaptop_env[] __initdata = "heaptop";
	char		*str;
	unsigned int	memsize;
	unsigned int	heaptop;
	int i;

	str = prom_getenv(memsz_env);
	if (!str) {
		ppfinit("memsize not set in boot prom, "
			"set to default (32Mb)\n");
		memsize = 0x02000000;
	} else {
		memsize = simple_strtol(str, NULL, 0);

		if (memsize == 0) {
			
			memsize = 0x02000000;
		}

		
		memsize = CPHYSADDR(memsize);
	}

	str = prom_getenv(heaptop_env);
	if (!str) {
		heaptop = CPHYSADDR((u32)&_text);
		ppfinit("heaptop not set in boot prom, "
			"set to default 0x%08x\n", heaptop);
	} else {
		heaptop = simple_strtol(str, NULL, 16);
		if (heaptop == 0) {
			
			heaptop = simple_strtol(str, NULL, 0);

			if (heaptop == 0) {
				
				heaptop = CPHYSADDR((u32)&_text);
			}
		}

		
		heaptop = CPHYSADDR((u32)heaptop);
	}

	
	i = 0;
	mdesc[i].type = BOOT_MEM_RESERVED;
	mdesc[i].base = 0x00000000;
	mdesc[i].size = PAGE_ALIGN(0x300 + 0x80);
		

	
	if (heaptop > mdesc[i].base + mdesc[i].size) {
		i++;			
		mdesc[i].type = BOOT_MEM_ROM_DATA;
		mdesc[i].base = mdesc[i-1].base + mdesc[i-1].size;
		mdesc[i].size = heaptop - mdesc[i].base;
	}

	
	if (heaptop != CPHYSADDR((u32)_text)) {
		i++;	
		mdesc[i].type = BOOT_MEM_RAM;
		mdesc[i].base = heaptop;
		mdesc[i].size = CPHYSADDR((u32)_text) - mdesc[i].base;
	}

	
	i++;			
	mdesc[i].type = BOOT_MEM_RESERVED;
	mdesc[i].base = CPHYSADDR((u32)_text);
	mdesc[i].size = CPHYSADDR(PAGE_ALIGN((u32)_end)) - mdesc[i].base;

	
	i++;			
	mdesc[i].type = yamon_free;
	mdesc[i].base = mdesc[i-1].base + mdesc[i-1].size;
	mdesc[i].size = memsize - mdesc[i].base;

	return &mdesc[0];
}
