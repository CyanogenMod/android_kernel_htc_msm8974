#ifndef _ASM_X86_IRQ_REMAPPING_H
#define _ASM_X86_IRQ_REMAPPING_H

#define IRTE_DEST(dest) ((x2apic_mode) ? dest : dest << 8)

#ifdef CONFIG_IRQ_REMAP
static void irq_remap_modify_chip_defaults(struct irq_chip *chip);
static inline void prepare_irte(struct irte *irte, int vector,
			        unsigned int dest)
{
	memset(irte, 0, sizeof(*irte));

	irte->present = 1;
	irte->dst_mode = apic->irq_dest_mode;
	irte->trigger_mode = 0;
	irte->dlvry_mode = apic->irq_delivery_mode;
	irte->vector = vector;
	irte->dest_id = IRTE_DEST(dest);
	irte->redir_hint = 1;
}
static inline bool irq_remapped(struct irq_cfg *cfg)
{
	return cfg->irq_2_iommu.iommu != NULL;
}
#else
static void prepare_irte(struct irte *irte, int vector, unsigned int dest)
{
}
static inline bool irq_remapped(struct irq_cfg *cfg)
{
	return false;
}
static inline void irq_remap_modify_chip_defaults(struct irq_chip *chip)
{
}
#endif

#endif	
