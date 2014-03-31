/*
 *  Functions for incremental construction of fcx enabled I/O control blocks.
 *
 *    Copyright IBM Corp. 2008
 *    Author(s): Peter Oberparleiter <peter.oberparleiter@de.ibm.com>
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/module.h>
#include <asm/fcx.h>
#include <asm/itcw.h>

struct itcw {
	struct tcw *tcw;
	struct tcw *intrg_tcw;
	int num_tidaws;
	int max_tidaws;
	int intrg_num_tidaws;
	int intrg_max_tidaws;
};

struct tcw *itcw_get_tcw(struct itcw *itcw)
{
	return itcw->tcw;
}
EXPORT_SYMBOL(itcw_get_tcw);

size_t itcw_calc_size(int intrg, int max_tidaws, int intrg_max_tidaws)
{
	size_t len;
	int cross_count;

	
	len = sizeof(struct itcw);
	len +=  sizeof(struct tcw) +  TCCB_MAX_SIZE +
	        sizeof(struct tsb) +
	        max_tidaws * sizeof(struct tidaw);
	
	if (intrg) {
		len +=  sizeof(struct tcw) +  TCCB_MAX_SIZE +
		        sizeof(struct tsb) +
		        intrg_max_tidaws * sizeof(struct tidaw);
	}

	
	len +=  63 +  7;

	if (max_tidaws) {
		cross_count = 1 + ((max_tidaws * sizeof(struct tidaw) - 1)
				   >> PAGE_SHIFT);
		len += cross_count * sizeof(struct tidaw);
	}
	if (intrg_max_tidaws) {
		cross_count = 1 + ((intrg_max_tidaws * sizeof(struct tidaw) - 1)
				   >> PAGE_SHIFT);
		len += cross_count * sizeof(struct tidaw);
	}
	return len;
}
EXPORT_SYMBOL(itcw_calc_size);

#define CROSS4K(x, l)	(((x) & ~4095) != ((x + l) & ~4095))

static inline void *fit_chunk(addr_t *start, addr_t end, size_t len,
			      int align, int check_4k)
{
	addr_t addr;

	addr = ALIGN(*start, align);
	if (check_4k && CROSS4K(addr, len)) {
		addr = ALIGN(addr, 4096);
		addr = ALIGN(addr, align);
	}
	if (addr + len > end)
		return ERR_PTR(-ENOSPC);
	*start = addr + len;
	return (void *) addr;
}

struct itcw *itcw_init(void *buffer, size_t size, int op, int intrg,
		       int max_tidaws, int intrg_max_tidaws)
{
	struct itcw *itcw;
	void *chunk;
	addr_t start;
	addr_t end;
	int cross_count;

	
	start = (addr_t) buffer;
	end = start + size;
	if (end > (1 << 31))
		return ERR_PTR(-EINVAL);
	memset(buffer, 0, size);
	
	chunk = fit_chunk(&start, end, sizeof(struct itcw), 1, 0);
	if (IS_ERR(chunk))
		return chunk;
	itcw = chunk;
	
	cross_count = 0;
	if (max_tidaws)
		cross_count = 1 + ((max_tidaws * sizeof(struct tidaw) - 1)
				   >> PAGE_SHIFT);
	itcw->max_tidaws = max_tidaws + cross_count;
	cross_count = 0;
	if (intrg_max_tidaws)
		cross_count = 1 + ((intrg_max_tidaws * sizeof(struct tidaw) - 1)
				   >> PAGE_SHIFT);
	itcw->intrg_max_tidaws = intrg_max_tidaws + cross_count;
	
	chunk = fit_chunk(&start, end, sizeof(struct tcw), 64, 0);
	if (IS_ERR(chunk))
		return chunk;
	itcw->tcw = chunk;
	tcw_init(itcw->tcw, (op == ITCW_OP_READ) ? 1 : 0,
		 (op == ITCW_OP_WRITE) ? 1 : 0);
	
	if (intrg) {
		chunk = fit_chunk(&start, end, sizeof(struct tcw), 64, 0);
		if (IS_ERR(chunk))
			return chunk;
		itcw->intrg_tcw = chunk;
		tcw_init(itcw->intrg_tcw, 1, 0);
		tcw_set_intrg(itcw->tcw, itcw->intrg_tcw);
	}
	
	if (max_tidaws > 0) {
		chunk = fit_chunk(&start, end, sizeof(struct tidaw) *
				  itcw->max_tidaws, 16, 0);
		if (IS_ERR(chunk))
			return chunk;
		tcw_set_data(itcw->tcw, chunk, 1);
	}
	
	if (intrg && (intrg_max_tidaws > 0)) {
		chunk = fit_chunk(&start, end, sizeof(struct tidaw) *
				  itcw->intrg_max_tidaws, 16, 0);
		if (IS_ERR(chunk))
			return chunk;
		tcw_set_data(itcw->intrg_tcw, chunk, 1);
	}
	
	chunk = fit_chunk(&start, end, sizeof(struct tsb), 8, 0);
	if (IS_ERR(chunk))
		return chunk;
	tsb_init(chunk);
	tcw_set_tsb(itcw->tcw, chunk);
	
	if (intrg) {
		chunk = fit_chunk(&start, end, sizeof(struct tsb), 8, 0);
		if (IS_ERR(chunk))
			return chunk;
		tsb_init(chunk);
		tcw_set_tsb(itcw->intrg_tcw, chunk);
	}
	
	chunk = fit_chunk(&start, end, TCCB_MAX_SIZE, 8, 0);
	if (IS_ERR(chunk))
		return chunk;
	tccb_init(chunk, TCCB_MAX_SIZE, TCCB_SAC_DEFAULT);
	tcw_set_tccb(itcw->tcw, chunk);
	
	if (intrg) {
		chunk = fit_chunk(&start, end, TCCB_MAX_SIZE, 8, 0);
		if (IS_ERR(chunk))
			return chunk;
		tccb_init(chunk, TCCB_MAX_SIZE, TCCB_SAC_INTRG);
		tcw_set_tccb(itcw->intrg_tcw, chunk);
		tccb_add_dcw(chunk, TCCB_MAX_SIZE, DCW_CMD_INTRG, 0, NULL,
			     sizeof(struct dcw_intrg_data), 0);
		tcw_finalize(itcw->intrg_tcw, 0);
	}
	return itcw;
}
EXPORT_SYMBOL(itcw_init);

struct dcw *itcw_add_dcw(struct itcw *itcw, u8 cmd, u8 flags, void *cd,
			 u8 cd_count, u32 count)
{
	return tccb_add_dcw(tcw_get_tccb(itcw->tcw), TCCB_MAX_SIZE, cmd,
			    flags, cd, cd_count, count);
}
EXPORT_SYMBOL(itcw_add_dcw);

struct tidaw *itcw_add_tidaw(struct itcw *itcw, u8 flags, void *addr, u32 count)
{
	struct tidaw *following;

	if (itcw->num_tidaws >= itcw->max_tidaws)
		return ERR_PTR(-ENOSPC);
	following = ((struct tidaw *) tcw_get_data(itcw->tcw))
		+ itcw->num_tidaws + 1;
	if (itcw->num_tidaws && !((unsigned long) following & ~PAGE_MASK)) {
		tcw_add_tidaw(itcw->tcw, itcw->num_tidaws++,
			      TIDAW_FLAGS_TTIC, following, 0);
		if (itcw->num_tidaws >= itcw->max_tidaws)
			return ERR_PTR(-ENOSPC);
	}
	return tcw_add_tidaw(itcw->tcw, itcw->num_tidaws++, flags, addr, count);
}
EXPORT_SYMBOL(itcw_add_tidaw);

void itcw_set_data(struct itcw *itcw, void *addr, int use_tidal)
{
	tcw_set_data(itcw->tcw, addr, use_tidal);
}
EXPORT_SYMBOL(itcw_set_data);

void itcw_finalize(struct itcw *itcw)
{
	tcw_finalize(itcw->tcw, itcw->num_tidaws);
}
EXPORT_SYMBOL(itcw_finalize);
