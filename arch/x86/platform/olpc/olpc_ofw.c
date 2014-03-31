#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/page.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/olpc_ofw.h>

static int (*olpc_ofw_cif)(int *);

u32 olpc_ofw_pgd __initdata;

static DEFINE_SPINLOCK(ofw_lock);

#define MAXARGS 10

void __init setup_olpc_ofw_pgd(void)
{
	pgd_t *base, *ofw_pde;

	if (!olpc_ofw_cif)
		return;

	
	base = early_ioremap(olpc_ofw_pgd, sizeof(olpc_ofw_pgd) * PTRS_PER_PGD);
	if (!base) {
		printk(KERN_ERR "failed to remap OFW's pgd - disabling OFW!\n");
		olpc_ofw_cif = NULL;
		return;
	}
	ofw_pde = &base[OLPC_OFW_PDE_NR];

	
	set_pgd(&swapper_pg_dir[OLPC_OFW_PDE_NR], *ofw_pde);
	

	early_iounmap(base, sizeof(olpc_ofw_pgd) * PTRS_PER_PGD);
}

int __olpc_ofw(const char *name, int nr_args, const void **args, int nr_res,
		void **res)
{
	int ofw_args[MAXARGS + 3];
	unsigned long flags;
	int ret, i, *p;

	BUG_ON(nr_args + nr_res > MAXARGS);

	if (!olpc_ofw_cif)
		return -EIO;

	ofw_args[0] = (int)name;
	ofw_args[1] = nr_args;
	ofw_args[2] = nr_res;

	p = &ofw_args[3];
	for (i = 0; i < nr_args; i++, p++)
		*p = (int)args[i];

	
	spin_lock_irqsave(&ofw_lock, flags);
	ret = olpc_ofw_cif(ofw_args);
	spin_unlock_irqrestore(&ofw_lock, flags);

	if (!ret) {
		for (i = 0; i < nr_res; i++, p++)
			*((int *)res[i]) = *p;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(__olpc_ofw);

bool olpc_ofw_present(void)
{
	return olpc_ofw_cif != NULL;
}
EXPORT_SYMBOL_GPL(olpc_ofw_present);

#define OFW_MIN 0xff000000

#define OFW_BOUND (1<<20)

void __init olpc_ofw_detect(void)
{
	struct olpc_ofw_header *hdr = &boot_params.olpc_ofw_header;
	unsigned long start;

	
	if (hdr->ofw_magic != OLPC_OFW_SIG)
		return;

	olpc_ofw_cif = (int (*)(int *))hdr->cif_handler;

	if ((unsigned long)olpc_ofw_cif < OFW_MIN) {
		printk(KERN_ERR "OFW detected, but cif has invalid address 0x%lx - disabling.\n",
				(unsigned long)olpc_ofw_cif);
		olpc_ofw_cif = NULL;
		return;
	}

	
	start = round_down((unsigned long)olpc_ofw_cif, OFW_BOUND);
	printk(KERN_INFO "OFW detected in memory, cif @ 0x%lx (reserving top %ldMB)\n",
			(unsigned long)olpc_ofw_cif, (-start) >> 20);
	reserve_top_address(-start);
}

bool __init olpc_ofw_is_installed(void)
{
	return olpc_ofw_cif != NULL;
}
