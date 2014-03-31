#ifndef _ASM_X86_OLPC_OFW_H
#define _ASM_X86_OLPC_OFW_H

#define OLPC_OFW_PDE_NR 1022

#define OLPC_OFW_SIG 0x2057464F	

#ifdef CONFIG_OLPC

extern bool olpc_ofw_is_installed(void);

#define olpc_ofw(name, args, res) \
	__olpc_ofw((name), ARRAY_SIZE(args), args, ARRAY_SIZE(res), res)

extern int __olpc_ofw(const char *name, int nr_args, const void **args, int nr_res,
		void **res);

extern void olpc_ofw_detect(void);

extern void setup_olpc_ofw_pgd(void);

extern bool olpc_ofw_present(void);

extern void olpc_dt_build_devicetree(void);

#else 
static inline void olpc_ofw_detect(void) { }
static inline void setup_olpc_ofw_pgd(void) { }
static inline void olpc_dt_build_devicetree(void) { }
#endif 

#endif 
