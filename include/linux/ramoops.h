#ifndef __RAMOOPS_H
#define __RAMOOPS_H


struct ramoops_platform_data {
	unsigned long	mem_size;
	unsigned long	mem_address;
	unsigned long	record_size;
	int		dump_oops;
};

#endif
