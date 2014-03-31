/*
 * PMC-Sierra MSP board specific pci fixups.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Copyright 2005-2007 PMC-Sierra, Inc
 *
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
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

#ifdef CONFIG_PCI

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/byteorder.h>

#include <msp_pci.h>
#include <msp_cic_int.h>

#define IRQ4	MSP_INT_EXT4
#define IRQ5	MSP_INT_EXT5
#define IRQ6	MSP_INT_EXT6

#if defined(CONFIG_PMC_MSP7120_GW)
static char irq_tab[][5] __initdata = {
	
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     IRQ4,   IRQ4,   0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     IRQ5,   IRQ5,   0,      0 },    
	{0,     IRQ6,   IRQ6,   0,      0 }     
};

#elif defined(CONFIG_PMC_MSP7120_EVAL)

static char irq_tab[][5] __initdata = {
	
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     IRQ6,   IRQ6,   0,      0 },    
	{0,     IRQ5,   IRQ5,   0,      0 },    
	{0,     IRQ4,   IRQ4,   IRQ4,   IRQ4},  
	{0,     IRQ5,   IRQ5,   IRQ5,   IRQ5},  
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 }     
};

#else

static char irq_tab[][5] __initdata = {
	
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 },    
	{0,     0,      0,      0,      0 }     
};
#endif

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return PCIBIOS_SUCCESSFUL;
}

int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
#if !defined(CONFIG_PMC_MSP7120_GW) && !defined(CONFIG_PMC_MSP7120_EVAL)
	printk(KERN_WARNING "PCI: unknown board, no PCI IRQs assigned.\n");
#endif
	printk(KERN_WARNING "PCI: irq_tab returned %d for slot=%d pin=%d\n",
		irq_tab[slot][pin], slot, pin);

	return irq_tab[slot][pin];
}

#endif	
