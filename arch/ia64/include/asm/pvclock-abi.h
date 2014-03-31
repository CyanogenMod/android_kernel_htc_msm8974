
#ifndef _ASM_IA64__PVCLOCK_ABI_H
#define _ASM_IA64__PVCLOCK_ABI_H
#ifndef __ASSEMBLY__


struct pvclock_vcpu_time_info {
	u32   version;
	u32   pad0;
	u64   tsc_timestamp;
	u64   system_time;
	u32   tsc_to_system_mul;
	s8    tsc_shift;
	u8    pad[3];
} __attribute__((__packed__)); 

struct pvclock_wall_clock {
	u32   version;
	u32   sec;
	u32   nsec;
} __attribute__((__packed__));

#endif 
#endif 
