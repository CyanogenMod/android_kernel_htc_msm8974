#ifndef __ASM_CPU_TYPE_H
#define __ASM_CPU_TYPE_H

enum sparc_cpu {
  sun4        = 0x00,
  sun4c       = 0x01,
  sun4m       = 0x02,
  sun4d       = 0x03,
  sun4e       = 0x04,
  sun4u       = 0x05, 
  sun_unknown = 0x06,
  ap1000      = 0x07, 
  sparc_leon  = 0x08, 
};

#ifdef CONFIG_SPARC32
extern enum sparc_cpu sparc_cpu_model;

#define ARCH_SUN4C (sparc_cpu_model==sun4c)

#define SUN4M_NCPUS            4              

#else

#define sparc_cpu_model sun4u

#define ARCH_SUN4C 0
#endif

#endif 
