#ifndef _M68K_IRQ_H_
#define _M68K_IRQ_H_

#if defined(CONFIG_COLDFIRE)
#define NR_IRQS 256
#elif defined(CONFIG_VME) || defined(CONFIG_SUN3) || defined(CONFIG_SUN3X)
#define NR_IRQS 200
#elif defined(CONFIG_ATARI) || defined(CONFIG_MAC)
#define NR_IRQS 72
#elif defined(CONFIG_Q40)
#define NR_IRQS	43
#elif defined(CONFIG_AMIGA) || !defined(CONFIG_MMU)
#define NR_IRQS	32
#elif defined(CONFIG_APOLLO)
#define NR_IRQS	24
#elif defined(CONFIG_HP300)
#define NR_IRQS	8
#else
#define NR_IRQS	0
#endif

#if defined(CONFIG_M68020) || defined(CONFIG_M68030) || \
    defined(CONFIG_M68040) || defined(CONFIG_M68060)


#define IRQ_SPURIOUS	0

#define IRQ_AUTO_1	1	
#define IRQ_AUTO_2	2	
#define IRQ_AUTO_3	3	
#define IRQ_AUTO_4	4	
#define IRQ_AUTO_5	5	
#define IRQ_AUTO_6	6	
#define IRQ_AUTO_7	7	

#define IRQ_USER	8

struct irq_data;
struct irq_chip;
struct irq_desc;
extern unsigned int m68k_irq_startup(struct irq_data *data);
extern unsigned int m68k_irq_startup_irq(unsigned int irq);
extern void m68k_irq_shutdown(struct irq_data *data);
extern void m68k_setup_auto_interrupt(void (*handler)(unsigned int,
						      struct pt_regs *));
extern void m68k_setup_user_interrupt(unsigned int vec, unsigned int cnt);
extern void m68k_setup_irq_controller(struct irq_chip *,
				      void (*handle)(unsigned int irq,
						     struct irq_desc *desc),
				      unsigned int irq, unsigned int cnt);

extern unsigned int irq_canonicalize(unsigned int irq);

#else
#define irq_canonicalize(irq)  (irq)
#endif 

asmlinkage void do_IRQ(int irq, struct pt_regs *regs);
extern atomic_t irq_err_count;

#endif 
