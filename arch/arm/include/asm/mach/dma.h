/*
 *  arch/arm/include/asm/mach/dma.h
 *
 *  Copyright (C) 1998-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This header file describes the interface between the generic DMA handler
 *  (dma.c) and the architecture-specific DMA backends (dma-*.c)
 */

struct dma_struct;
typedef struct dma_struct dma_t;

struct dma_ops {
	int	(*request)(unsigned int, dma_t *);		
	void	(*free)(unsigned int, dma_t *);			
	void	(*enable)(unsigned int, dma_t *);		
	void 	(*disable)(unsigned int, dma_t *);		
	int	(*residue)(unsigned int, dma_t *);		
	int	(*setspeed)(unsigned int, dma_t *, int);	
	const char *type;
};

struct dma_struct {
	void		*addr;		
	unsigned long	count;		
	struct scatterlist buf;		
	int		sgcount;	
	struct scatterlist *sg;		

	unsigned int	active:1;	
	unsigned int	invalid:1;	

	unsigned int	dma_mode;	
	int		speed;		

	unsigned int	lock;		
	const char	*device_id;	

	const struct dma_ops *d_ops;
};

extern int isa_dma_add(unsigned int, dma_t *dma);

extern void isa_init_dma(void);
