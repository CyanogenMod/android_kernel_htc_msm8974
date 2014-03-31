/*
 * Public header for the MPC52xx processor BestComm driver
 *
 *
 * Copyright (C) 2006      Sylvain Munaut <tnt@246tNt.com>
 * Copyright (C) 2005      Varma Electronics Oy,
 *                         ( by Andrey Volkov <avolkov@varma-el.com> )
 * Copyright (C) 2003-2004 MontaVista, Software, Inc.
 *                         ( by Dale Farnsworth <dfarnsworth@mvista.com> )
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __BESTCOMM_H__
#define __BESTCOMM_H__

struct bcom_bd {
	u32	status;
	u32	data[0];	
};


struct bcom_task {
	unsigned int	tasknum;
	unsigned int	flags;
	int		irq;

	struct bcom_bd	*bd;
	phys_addr_t	bd_pa;
	void		**cookie;
	unsigned short	index;
	unsigned short	outdex;
	unsigned int	num_bd;
	unsigned int	bd_size;

	void*		priv;
};

#define BCOM_FLAGS_NONE         0x00000000ul
#define BCOM_FLAGS_ENABLE_TASK  (1ul <<  0)

extern void bcom_enable(struct bcom_task *tsk);

extern void bcom_disable(struct bcom_task *tsk);


static inline int
bcom_get_task_irq(struct bcom_task *tsk) {
	return tsk->irq;
}


#define BCOM_BD_READY	0x40000000ul

static inline int
_bcom_next_index(struct bcom_task *tsk)
{
	return ((tsk->index + 1) == tsk->num_bd) ? 0 : tsk->index + 1;
}

static inline int
_bcom_next_outdex(struct bcom_task *tsk)
{
	return ((tsk->outdex + 1) == tsk->num_bd) ? 0 : tsk->outdex + 1;
}

static inline int
bcom_queue_empty(struct bcom_task *tsk)
{
	return tsk->index == tsk->outdex;
}

static inline int
bcom_queue_full(struct bcom_task *tsk)
{
	return tsk->outdex == _bcom_next_index(tsk);
}

static inline struct bcom_bd
*bcom_get_bd(struct bcom_task *tsk, unsigned int index)
{
	return ((void *)tsk->bd) + (index * tsk->bd_size);
}

static inline int
bcom_buffer_done(struct bcom_task *tsk)
{
	struct bcom_bd *bd;
	if (bcom_queue_empty(tsk))
		return 0;

	bd = bcom_get_bd(tsk, tsk->outdex);
	return !(bd->status & BCOM_BD_READY);
}

static inline struct bcom_bd *
bcom_prepare_next_buffer(struct bcom_task *tsk)
{
	struct bcom_bd *bd;

	bd = bcom_get_bd(tsk, tsk->index);
	bd->status = 0;	
	return bd;
}

static inline void
bcom_submit_next_buffer(struct bcom_task *tsk, void *cookie)
{
	struct bcom_bd *bd = bcom_get_bd(tsk, tsk->index);

	tsk->cookie[tsk->index] = cookie;
	mb();	
	bd->status |= BCOM_BD_READY;
	tsk->index = _bcom_next_index(tsk);
	if (tsk->flags & BCOM_FLAGS_ENABLE_TASK)
		bcom_enable(tsk);
}

static inline void *
bcom_retrieve_buffer(struct bcom_task *tsk, u32 *p_status, struct bcom_bd **p_bd)
{
	void *cookie = tsk->cookie[tsk->outdex];
	struct bcom_bd *bd = bcom_get_bd(tsk, tsk->outdex);

	if (p_status)
		*p_status = bd->status;
	if (p_bd)
		*p_bd = bd;
	tsk->outdex = _bcom_next_outdex(tsk);
	return cookie;
}

#endif 
