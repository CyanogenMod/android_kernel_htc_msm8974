/*
 * linux/drivers/pcmcia/soc_common.h
 *
 * Copyright (C) 2000 John G Dorsey <john+@cs.cmu.edu>
 *
 * This file contains definitions for the PCMCIA support code common to
 * integrated SOCs like the SA-11x0 and PXA2xx microprocessors.
 */
#ifndef _ASM_ARCH_PCMCIA
#define _ASM_ARCH_PCMCIA

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <pcmcia/ss.h>
#include <pcmcia/cistpl.h>


struct device;
struct pcmcia_low_level;

struct soc_pcmcia_socket {
	struct pcmcia_socket	socket;

	unsigned int		nr;
	struct clk		*clk;

	const struct pcmcia_low_level *ops;

	unsigned int		status;
	socket_state_t		cs_state;

	unsigned short		spd_io[MAX_IO_WIN];
	unsigned short		spd_mem[MAX_WIN];
	unsigned short		spd_attr[MAX_WIN];

	struct resource		res_skt;
	struct resource		res_io;
	struct resource		res_mem;
	struct resource		res_attr;
	void __iomem		*virt_io;

	struct {
		int		gpio;
		unsigned int	irq;
		const char	*name;
	} stat[4];
#define SOC_STAT_CD		0	
#define SOC_STAT_BVD1		1	
#define SOC_STAT_BVD2		2	
#define SOC_STAT_RDY		3	

	unsigned int		irq_state;

	struct timer_list	poll_timer;
	struct list_head	node;
};

struct skt_dev_info {
	int nskt;
	struct clk *clk;
	struct soc_pcmcia_socket skt[0];
};

struct pcmcia_state {
  unsigned detect: 1,
            ready: 1,
             bvd1: 1,
             bvd2: 1,
           wrprot: 1,
            vs_3v: 1,
            vs_Xv: 1;
};

struct pcmcia_low_level {
	struct module *owner;

	
	int first;
	
	int nr;

	int (*hw_init)(struct soc_pcmcia_socket *);
	void (*hw_shutdown)(struct soc_pcmcia_socket *);

	void (*socket_state)(struct soc_pcmcia_socket *, struct pcmcia_state *);
	int (*configure_socket)(struct soc_pcmcia_socket *, const socket_state_t *);

	void (*socket_init)(struct soc_pcmcia_socket *);

	void (*socket_suspend)(struct soc_pcmcia_socket *);

	unsigned int (*get_timing)(struct soc_pcmcia_socket *, unsigned int, unsigned int);
	int (*set_timing)(struct soc_pcmcia_socket *);
	int (*show_timing)(struct soc_pcmcia_socket *, char *);

#ifdef CONFIG_CPU_FREQ
	int (*frequency_change)(struct soc_pcmcia_socket *, unsigned long, struct cpufreq_freqs *);
#endif
};


struct soc_pcmcia_timing {
	unsigned short io;
	unsigned short mem;
	unsigned short attr;
};

extern void soc_common_pcmcia_get_timing(struct soc_pcmcia_socket *, struct soc_pcmcia_timing *);

void soc_pcmcia_init_one(struct soc_pcmcia_socket *skt,
	struct pcmcia_low_level *ops, struct device *dev);
void soc_pcmcia_remove_one(struct soc_pcmcia_socket *skt);
int soc_pcmcia_add_one(struct soc_pcmcia_socket *skt);


#ifdef CONFIG_PCMCIA_DEBUG

extern void soc_pcmcia_debug(struct soc_pcmcia_socket *skt, const char *func,
			     int lvl, const char *fmt, ...);

#define debug(skt, lvl, fmt, arg...) \
	soc_pcmcia_debug(skt, __func__, lvl, fmt , ## arg)

#else
#define debug(skt, lvl, fmt, arg...) do { } while (0)
#endif


#define SOC_PCMCIA_IO_ACCESS		(165)
#define SOC_PCMCIA_5V_MEM_ACCESS	(150)
#define SOC_PCMCIA_3V_MEM_ACCESS	(300)
#define SOC_PCMCIA_ATTR_MEM_ACCESS	(300)

#define SOC_PCMCIA_POLL_PERIOD    (2*HZ)


#define iostschg bvd1
#define iospkr   bvd2

#endif
