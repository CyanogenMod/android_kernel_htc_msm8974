/*
 *  (C) 2010,2011       Thomas Renninger <trenn@suse.de>, Novell Inc.
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 *
 * Miscellaneous helpers which do not fit or are worth
 * to put into separate headers
 */

#ifndef __CPUPOWERUTILS_HELPERS__
#define __CPUPOWERUTILS_HELPERS__

#include <libintl.h>
#include <locale.h>

#include "helpers/bitmask.h"

#ifdef NLS

#define _(String) gettext(String)
#ifndef gettext_noop
#define gettext_noop(String) String
#endif
#define N_(String) gettext_noop(String)

#else 

#define _(String) String
#define N_(String) String

#endif

extern int run_as_root;
extern struct bitmask *cpus_chosen;

#ifdef DEBUG
extern int be_verbose;

#define dprint(fmt, ...) {					\
		if (be_verbose) {				\
			fprintf(stderr, "%s: " fmt,		\
				__func__, ##__VA_ARGS__);	\
		}						\
	}
#else
static inline void dprint(const char *fmt, ...) { }
#endif
extern int be_verbose;

enum cpupower_cpu_vendor {X86_VENDOR_UNKNOWN = 0, X86_VENDOR_INTEL,
			  X86_VENDOR_AMD, X86_VENDOR_MAX};

#define CPUPOWER_CAP_INV_TSC		0x00000001
#define CPUPOWER_CAP_APERF		0x00000002
#define CPUPOWER_CAP_AMD_CBP		0x00000004
#define CPUPOWER_CAP_PERF_BIAS		0x00000008
#define CPUPOWER_CAP_HAS_TURBO_RATIO	0x00000010
#define CPUPOWER_CAP_IS_SNB		0x00000020
#define CPUPOWER_CAP_INTEL_IDA		0x00000040

#define MAX_HW_PSTATES 10

struct cpupower_cpu_info {
	enum cpupower_cpu_vendor vendor;
	unsigned int family;
	unsigned int model;
	unsigned int stepping;
	
	unsigned long long caps;
};

extern int get_cpu_info(unsigned int cpu, struct cpupower_cpu_info *cpu_info);
extern struct cpupower_cpu_info cpupower_cpu_info;


struct cpupower_topology {
	
	unsigned int cores;
	unsigned int pkgs;
	unsigned int threads; 

	
	struct {
		int pkg;
		int core;
		int cpu;

		
		unsigned int is_online:1;
	} *core_info;
};

extern int get_cpu_topology(struct cpupower_topology *cpu_top);
extern void cpu_topology_release(struct cpupower_topology cpu_top);

#if defined(__i386__) || defined(__x86_64__)

#include <pci/pci.h>

extern int read_msr(int cpu, unsigned int idx, unsigned long long *val);
extern int write_msr(int cpu, unsigned int idx, unsigned long long val);

extern int msr_intel_set_perf_bias(unsigned int cpu, unsigned int val);
extern int msr_intel_get_perf_bias(unsigned int cpu);
extern unsigned long long msr_intel_get_turbo_ratio(unsigned int cpu);


extern int amd_pci_get_num_boost_states(int *active, int *states);
extern struct pci_dev *pci_acc_init(struct pci_access **pacc, int domain,
				    int bus, int slot, int func, int vendor,
				    int dev);
extern struct pci_dev *pci_slot_func_init(struct pci_access **pacc,
					      int slot, int func);



extern int decode_pstates(unsigned int cpu, unsigned int cpu_family,
			  int boost_states, unsigned long *pstates, int *no);


extern int cpufreq_has_boost_support(unsigned int cpu, int *support,
				     int *active, int * states);
unsigned int cpuid_eax(unsigned int op);
unsigned int cpuid_ebx(unsigned int op);
unsigned int cpuid_ecx(unsigned int op);
unsigned int cpuid_edx(unsigned int op);

#else
static inline int decode_pstates(unsigned int cpu, unsigned int cpu_family,
				 int boost_states, unsigned long *pstates,
				 int *no)
{ return -1; };

static inline int read_msr(int cpu, unsigned int idx, unsigned long long *val)
{ return -1; };
static inline int write_msr(int cpu, unsigned int idx, unsigned long long val)
{ return -1; };
static inline int msr_intel_set_perf_bias(unsigned int cpu, unsigned int val)
{ return -1; };
static inline int msr_intel_get_perf_bias(unsigned int cpu)
{ return -1; };
static inline unsigned long long msr_intel_get_turbo_ratio(unsigned int cpu)
{ return 0; };


static inline int cpufreq_has_boost_support(unsigned int cpu, int *support,
					    int *active, int * states)
{ return -1; }


static inline unsigned int cpuid_eax(unsigned int op) { return 0; };
static inline unsigned int cpuid_ebx(unsigned int op) { return 0; };
static inline unsigned int cpuid_ecx(unsigned int op) { return 0; };
static inline unsigned int cpuid_edx(unsigned int op) { return 0; };
#endif 

#endif 
