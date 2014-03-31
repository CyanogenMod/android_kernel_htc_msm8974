/*
 *  cpufreq.h - definitions for libcpufreq
 *
 *  Copyright (C) 2004-2009  Dominik Brodowski <linux@dominikbrodowski.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _CPUFREQ_H
#define _CPUFREQ_H 1

struct cpufreq_policy {
	unsigned long min;
	unsigned long max;
	char *governor;
};

struct cpufreq_available_governors {
	char *governor;
	struct cpufreq_available_governors *next;
	struct cpufreq_available_governors *first;
};

struct cpufreq_available_frequencies {
	unsigned long frequency;
	struct cpufreq_available_frequencies *next;
	struct cpufreq_available_frequencies *first;
};


struct cpufreq_affected_cpus {
	unsigned int cpu;
	struct cpufreq_affected_cpus *next;
	struct cpufreq_affected_cpus *first;
};

struct cpufreq_stats {
	unsigned long frequency;
	unsigned long long time_in_state;
	struct cpufreq_stats *next;
	struct cpufreq_stats *first;
};



#ifdef __cplusplus
extern "C" {
#endif


extern int cpufreq_cpu_exists(unsigned int cpu);


extern unsigned long cpufreq_get_freq_kernel(unsigned int cpu);

extern unsigned long cpufreq_get_freq_hardware(unsigned int cpu);

#define cpufreq_get(cpu) cpufreq_get_freq_kernel(cpu);


extern unsigned long cpufreq_get_transition_latency(unsigned int cpu);



extern int cpufreq_get_hardware_limits(unsigned int cpu,
				unsigned long *min,
				unsigned long *max);



extern char *cpufreq_get_driver(unsigned int cpu);

extern void cpufreq_put_driver(char *ptr);




extern struct cpufreq_policy *cpufreq_get_policy(unsigned int cpu);

extern void cpufreq_put_policy(struct cpufreq_policy *policy);




extern struct cpufreq_available_governors
*cpufreq_get_available_governors(unsigned int cpu);

extern void cpufreq_put_available_governors(
	struct cpufreq_available_governors *first);



extern struct cpufreq_available_frequencies
*cpufreq_get_available_frequencies(unsigned int cpu);

extern void cpufreq_put_available_frequencies(
		struct cpufreq_available_frequencies *first);



extern struct cpufreq_affected_cpus *cpufreq_get_affected_cpus(unsigned
							int cpu);

extern void cpufreq_put_affected_cpus(struct cpufreq_affected_cpus *first);



extern struct cpufreq_affected_cpus *cpufreq_get_related_cpus(unsigned
							int cpu);

extern void cpufreq_put_related_cpus(struct cpufreq_affected_cpus *first);



extern struct cpufreq_stats *cpufreq_get_stats(unsigned int cpu,
					unsigned long long *total_time);

extern void cpufreq_put_stats(struct cpufreq_stats *stats);

extern unsigned long cpufreq_get_transitions(unsigned int cpu);



extern int cpufreq_set_policy(unsigned int cpu, struct cpufreq_policy *policy);



extern int cpufreq_modify_policy_min(unsigned int cpu, unsigned long min_freq);
extern int cpufreq_modify_policy_max(unsigned int cpu, unsigned long max_freq);
extern int cpufreq_modify_policy_governor(unsigned int cpu, char *governor);



extern int cpufreq_set_frequency(unsigned int cpu,
				unsigned long target_frequency);

#ifdef __cplusplus
}
#endif

#endif 
