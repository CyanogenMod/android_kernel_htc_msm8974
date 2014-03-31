#ifndef _ASM_IA64_MACHVEC_XEN_h
#define _ASM_IA64_MACHVEC_XEN_h

extern ia64_mv_setup_t			dig_setup;
extern ia64_mv_cpu_init_t		xen_cpu_init;
extern ia64_mv_irq_init_t		xen_irq_init;
extern ia64_mv_send_ipi_t		xen_platform_send_ipi;

#define platform_name				"xen"
#define platform_setup				dig_setup
#define platform_cpu_init			xen_cpu_init
#define platform_irq_init			xen_irq_init
#define platform_send_ipi			xen_platform_send_ipi

#endif 
