#ifndef LINUX_HTIRQ_H
#define LINUX_HTIRQ_H

struct ht_irq_msg {
	u32	address_lo;	
	u32	address_hi;	
};

void fetch_ht_irq_msg(unsigned int irq, struct ht_irq_msg *msg);
void write_ht_irq_msg(unsigned int irq, struct ht_irq_msg *msg);
struct irq_data;
void mask_ht_irq(struct irq_data *data);
void unmask_ht_irq(struct irq_data *data);

int arch_setup_ht_irq(unsigned int irq, struct pci_dev *dev);

typedef void (ht_irq_update_t)(struct pci_dev *dev, int irq,
			       struct ht_irq_msg *msg);
int __ht_create_irq(struct pci_dev *dev, int idx, ht_irq_update_t *update);

#endif 
