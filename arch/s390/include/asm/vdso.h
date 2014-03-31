#ifndef __S390_VDSO_H__
#define __S390_VDSO_H__

#ifdef __KERNEL__

#define VDSO32_LBASE	0
#define VDSO64_LBASE	0

#define VDSO_VERSION_STRING	LINUX_2.6.29

#ifndef __ASSEMBLY__


struct vdso_data {
	__u64 tb_update_count;		
	__u64 xtime_tod_stamp;		
	__u64 xtime_clock_sec;		
	__u64 xtime_clock_nsec;		
	__u64 wtom_clock_sec;		
	__u64 wtom_clock_nsec;		
	__u32 tz_minuteswest;		
	__u32 tz_dsttime;		
	__u32 ectg_available;
	__u32 ntp_mult;			
};

struct vdso_per_cpu_data {
	__u64 ectg_timer_base;
	__u64 ectg_user_time;
};

extern struct vdso_data *vdso_data;

#ifdef CONFIG_64BIT
int vdso_alloc_per_cpu(struct _lowcore *lowcore);
void vdso_free_per_cpu(struct _lowcore *lowcore);
#endif

#endif 

#endif 

#endif 
