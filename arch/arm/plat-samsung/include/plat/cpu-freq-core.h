/* arch/arm/plat-samsung/include/plat/cpu-freq-core.h
 *
 * Copyright (c) 2006-2009 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C CPU frequency scaling support - core support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <plat/cpu-freq.h>

struct seq_file;

#define MAX_BANKS (8)
#define S3C2412_MAX_IO	(8)

struct s3c2410_iobank_timing {
	unsigned long	bankcon;
	unsigned int	tacp;
	unsigned int	tacs;
	unsigned int	tcos;
	unsigned int	tacc;
	unsigned int	tcoh;		
	unsigned int	tcah;		
	unsigned char	nwait_en;	
};

struct s3c2412_iobank_timing {
	unsigned int	idcy;
	unsigned int	wstrd;
	unsigned int	wstwr;
	unsigned int	wstoen;
	unsigned int	wstwen;
	unsigned int	wstbrd;

	
	unsigned char	smbidcyr;
	unsigned char	smbwstrd;
	unsigned char	smbwstwr;
	unsigned char	smbwstoen;
	unsigned char	smbwstwen;
	unsigned char	smbwstbrd;
};

union s3c_iobank {
	struct s3c2410_iobank_timing	*io_2410;
	struct s3c2412_iobank_timing	*io_2412;
};

struct s3c_iotimings {
	union s3c_iobank	bank[MAX_BANKS];
};

struct s3c_plltab {
	struct s3c_pllval	*vals;
	int			 size;
};

struct s3c_cpufreq_config {
	struct s3c_freq		freq;
	struct s3c_freq		max;
	struct cpufreq_frequency_table pll;
	struct s3c_clkdivs	divs;
	struct s3c_cpufreq_info *info;	
	struct s3c_cpufreq_board *board;

	unsigned int	lock_pll:1;
};

struct s3c_cpufreq_info {
	const char		*name;
	struct s3c_freq		max;

	unsigned int		latency;

	unsigned int		locktime_m;
	unsigned int		locktime_u;
	unsigned char		locktime_bits;

	unsigned int		need_pll:1;

	

	void		(*resume_clocks)(void);

	int		(*get_iotiming)(struct s3c_cpufreq_config *cfg,
					struct s3c_iotimings *timings);

	void		(*set_iotiming)(struct s3c_cpufreq_config *cfg,
					struct s3c_iotimings *timings);

	int		(*calc_iotiming)(struct s3c_cpufreq_config *cfg,
					 struct s3c_iotimings *timings);

	int		(*calc_freqtable)(struct s3c_cpufreq_config *cfg,
					  struct cpufreq_frequency_table *t,
					  size_t table_size);

	void		(*debug_io_show)(struct seq_file *seq,
					 struct s3c_cpufreq_config *cfg,
					 union s3c_iobank *iob);

	void		(*set_refresh)(struct s3c_cpufreq_config *cfg);
	void		(*set_fvco)(struct s3c_cpufreq_config *cfg);
	void		(*set_divs)(struct s3c_cpufreq_config *cfg);
	int		(*calc_divs)(struct s3c_cpufreq_config *cfg);
};

extern int s3c_cpufreq_register(struct s3c_cpufreq_info *info);

extern int s3c_plltab_register(struct cpufreq_frequency_table *plls,
			       unsigned int plls_no);

extern struct s3c_cpufreq_config *s3c_cpufreq_getconfig(void);
extern struct s3c_iotimings *s3c_cpufreq_getiotimings(void);

#ifdef CONFIG_CPU_FREQ_S3C24XX_DEBUGFS
#define s3c_cpufreq_debugfs_call(x) x
#else
#define s3c_cpufreq_debugfs_call(x) NULL
#endif


extern struct clk *s3c_cpufreq_clk_get(struct device *, const char *);


extern void s3c2410_cpufreq_setrefresh(struct s3c_cpufreq_config *cfg);
extern void s3c2410_set_fvco(struct s3c_cpufreq_config *cfg);

#ifdef CONFIG_S3C2410_IOTIMING
extern void s3c2410_iotiming_debugfs(struct seq_file *seq,
				     struct s3c_cpufreq_config *cfg,
				     union s3c_iobank *iob);

extern int s3c2410_iotiming_calc(struct s3c_cpufreq_config *cfg,
				 struct s3c_iotimings *iot);

extern int s3c2410_iotiming_get(struct s3c_cpufreq_config *cfg,
				struct s3c_iotimings *timings);

extern void s3c2410_iotiming_set(struct s3c_cpufreq_config *cfg,
				 struct s3c_iotimings *iot);
#else
#define s3c2410_iotiming_debugfs NULL
#define s3c2410_iotiming_calc NULL
#define s3c2410_iotiming_get NULL
#define s3c2410_iotiming_set NULL
#endif 


#ifdef CONFIG_S3C2412_IOTIMING
extern void s3c2412_iotiming_debugfs(struct seq_file *seq,
				     struct s3c_cpufreq_config *cfg,
				     union s3c_iobank *iob);

extern int s3c2412_iotiming_get(struct s3c_cpufreq_config *cfg,
				struct s3c_iotimings *timings);

extern int s3c2412_iotiming_calc(struct s3c_cpufreq_config *cfg,
				 struct s3c_iotimings *iot);

extern void s3c2412_iotiming_set(struct s3c_cpufreq_config *cfg,
				 struct s3c_iotimings *iot);
#else
#define s3c2412_iotiming_debugfs NULL
#define s3c2412_iotiming_calc NULL
#define s3c2412_iotiming_get NULL
#define s3c2412_iotiming_set NULL
#endif 

#ifdef CONFIG_CPU_FREQ_S3C24XX_DEBUG
#define s3c_freq_dbg(x...) printk(KERN_INFO x)
#else
#define s3c_freq_dbg(x...) do { if (0) printk(x); } while (0)
#endif 

#ifdef CONFIG_CPU_FREQ_S3C24XX_IODEBUG
#define s3c_freq_iodbg(x...) printk(KERN_INFO x)
#else
#define s3c_freq_iodbg(x...) do { if (0) printk(x); } while (0)
#endif 

static inline int s3c_cpufreq_addfreq(struct cpufreq_frequency_table *table,
				      int index, size_t table_size,
				      unsigned int freq)
{
	if (index < 0)
		return index;

	if (table) {
		if (index >= table_size)
			return -ENOMEM;

		s3c_freq_dbg("%s: { %d = %u kHz }\n",
			     __func__, index, freq);

		table[index].index = index;
		table[index].frequency = freq;
	}

	return index + 1;
}
