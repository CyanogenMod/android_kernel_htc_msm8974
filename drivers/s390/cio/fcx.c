/*
 *  Functions for assembling fcx enabled I/O control blocks.
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
#include "cio.h"

struct tcw *tcw_get_intrg(struct tcw *tcw)
{
	return (struct tcw *) ((addr_t) tcw->intrg);
}
EXPORT_SYMBOL(tcw_get_intrg);

void *tcw_get_data(struct tcw *tcw)
{
	if (tcw->r)
		return (void *) ((addr_t) tcw->input);
	if (tcw->w)
		return (void *) ((addr_t) tcw->output);
	return NULL;
}
EXPORT_SYMBOL(tcw_get_data);

struct tccb *tcw_get_tccb(struct tcw *tcw)
{
	return (struct tccb *) ((addr_t) tcw->tccb);
}
EXPORT_SYMBOL(tcw_get_tccb);

struct tsb *tcw_get_tsb(struct tcw *tcw)
{
	return (struct tsb *) ((addr_t) tcw->tsb);
}
EXPORT_SYMBOL(tcw_get_tsb);

void tcw_init(struct tcw *tcw, int r, int w)
{
	memset(tcw, 0, sizeof(struct tcw));
	tcw->format = TCW_FORMAT_DEFAULT;
	tcw->flags = TCW_FLAGS_TIDAW_FORMAT(TCW_TIDAW_FORMAT_DEFAULT);
	if (r)
		tcw->r = 1;
	if (w)
		tcw->w = 1;
}
EXPORT_SYMBOL(tcw_init);

static inline size_t tca_size(struct tccb *tccb)
{
	return tccb->tcah.tcal - 12;
}

static u32 calc_dcw_count(struct tccb *tccb)
{
	int offset;
	struct dcw *dcw;
	u32 count = 0;
	size_t size;

	size = tca_size(tccb);
	for (offset = 0; offset < size;) {
		dcw = (struct dcw *) &tccb->tca[offset];
		count += dcw->count;
		if (!(dcw->flags & DCW_FLAGS_CC))
			break;
		offset += sizeof(struct dcw) + ALIGN((int) dcw->cd_count, 4);
	}
	return count;
}

static u32 calc_cbc_size(struct tidaw *tidaw, int num)
{
	int i;
	u32 cbc_data;
	u32 cbc_count = 0;
	u64 data_count = 0;

	for (i = 0; i < num; i++) {
		if (tidaw[i].flags & TIDAW_FLAGS_LAST)
			break;
		data_count += tidaw[i].count;
		if (tidaw[i].flags & TIDAW_FLAGS_INSERT_CBC) {
			cbc_data = 4 + ALIGN(data_count, 4) - data_count;
			cbc_count += cbc_data;
			data_count += cbc_data;
		}
	}
	return cbc_count;
}

void tcw_finalize(struct tcw *tcw, int num_tidaws)
{
	struct tidaw *tidaw;
	struct tccb *tccb;
	struct tccb_tcat *tcat;
	u32 count;

	
	tidaw = tcw_get_data(tcw);
	if (num_tidaws > 0)
		tidaw[num_tidaws - 1].flags |= TIDAW_FLAGS_LAST;
	
	tccb = tcw_get_tccb(tcw);
	tcat = (struct tccb_tcat *) &tccb->tca[tca_size(tccb)];
	memset(tcat, 0, sizeof(*tcat));
	
	count = calc_dcw_count(tccb);
	if (tcw->w && (tcw->flags & TCW_FLAGS_OUTPUT_TIDA))
		count += calc_cbc_size(tidaw, num_tidaws);
	if (tcw->r)
		tcw->input_count = count;
	else if (tcw->w)
		tcw->output_count = count;
	tcat->count = ALIGN(count, 4) + 4;
	
	tcw->tccbl = (sizeof(struct tccb) + tca_size(tccb) +
		      sizeof(struct tccb_tcat) - 20) >> 2;
}
EXPORT_SYMBOL(tcw_finalize);

void tcw_set_intrg(struct tcw *tcw, struct tcw *intrg_tcw)
{
	tcw->intrg = (u32) ((addr_t) intrg_tcw);
}
EXPORT_SYMBOL(tcw_set_intrg);

void tcw_set_data(struct tcw *tcw, void *data, int use_tidal)
{
	if (tcw->r) {
		tcw->input = (u64) ((addr_t) data);
		if (use_tidal)
			tcw->flags |= TCW_FLAGS_INPUT_TIDA;
	} else if (tcw->w) {
		tcw->output = (u64) ((addr_t) data);
		if (use_tidal)
			tcw->flags |= TCW_FLAGS_OUTPUT_TIDA;
	}
}
EXPORT_SYMBOL(tcw_set_data);

void tcw_set_tccb(struct tcw *tcw, struct tccb *tccb)
{
	tcw->tccb = (u64) ((addr_t) tccb);
}
EXPORT_SYMBOL(tcw_set_tccb);

void tcw_set_tsb(struct tcw *tcw, struct tsb *tsb)
{
	tcw->tsb = (u64) ((addr_t) tsb);
}
EXPORT_SYMBOL(tcw_set_tsb);

void tccb_init(struct tccb *tccb, size_t size, u32 sac)
{
	memset(tccb, 0, size);
	tccb->tcah.format = TCCB_FORMAT_DEFAULT;
	tccb->tcah.sac = sac;
	tccb->tcah.tcal = 12;
}
EXPORT_SYMBOL(tccb_init);

void tsb_init(struct tsb *tsb)
{
	memset(tsb, 0, sizeof(*tsb));
}
EXPORT_SYMBOL(tsb_init);

struct dcw *tccb_add_dcw(struct tccb *tccb, size_t tccb_size, u8 cmd, u8 flags,
			 void *cd, u8 cd_count, u32 count)
{
	struct dcw *dcw;
	int size;
	int tca_offset;

	
	tca_offset = tca_size(tccb);
	size = ALIGN(sizeof(struct dcw) + cd_count, 4);
	if (sizeof(struct tccb_tcah) + tca_offset + size +
	    sizeof(struct tccb_tcat) > tccb_size)
		return ERR_PTR(-ENOSPC);
	
	dcw = (struct dcw *) &tccb->tca[tca_offset];
	memset(dcw, 0, size);
	dcw->cmd = cmd;
	dcw->flags = flags;
	dcw->count = count;
	dcw->cd_count = cd_count;
	if (cd)
		memcpy(&dcw->cd[0], cd, cd_count);
	tccb->tcah.tcal += size;
	return dcw;
}
EXPORT_SYMBOL(tccb_add_dcw);

struct tidaw *tcw_add_tidaw(struct tcw *tcw, int num_tidaws, u8 flags,
			    void *addr, u32 count)
{
	struct tidaw *tidaw;

	
	tidaw = ((struct tidaw *) tcw_get_data(tcw)) + num_tidaws;
	memset(tidaw, 0, sizeof(struct tidaw));
	tidaw->flags = flags;
	tidaw->count = count;
	tidaw->addr = (u64) ((addr_t) addr);
	return tidaw;
}
EXPORT_SYMBOL(tcw_add_tidaw);
