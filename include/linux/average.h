#ifndef _LINUX_AVERAGE_H
#define _LINUX_AVERAGE_H



struct ewma {
	unsigned long internal;
	unsigned long factor;
	unsigned long weight;
};

extern void ewma_init(struct ewma *avg, unsigned long factor,
		      unsigned long weight);

extern struct ewma *ewma_add(struct ewma *avg, unsigned long val);

static inline unsigned long ewma_read(const struct ewma *avg)
{
	return avg->internal >> avg->factor;
}

#endif 
