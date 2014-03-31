/*
 * Copyright (c) 2009 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/export.h>
#include <asm/unaligned.h>

#include "ath.h"
#include "reg.h"

#define REG_READ	(common->ops->read)
#define REG_WRITE	(common->ops->write)

void ath_hw_setbssidmask(struct ath_common *common)
{
	void *ah = common->ah;

	REG_WRITE(ah, get_unaligned_le32(common->bssidmask), AR_BSSMSKL);
	REG_WRITE(ah, get_unaligned_le16(common->bssidmask + 4), AR_BSSMSKU);
}
EXPORT_SYMBOL(ath_hw_setbssidmask);


void ath_hw_cycle_counters_update(struct ath_common *common)
{
	u32 cycles, busy, rx, tx;
	void *ah = common->ah;

	
	REG_WRITE(ah, AR_MIBC_FMC, AR_MIBC);

	
	cycles = REG_READ(ah, AR_CCCNT);
	busy = REG_READ(ah, AR_RCCNT);
	rx = REG_READ(ah, AR_RFCNT);
	tx = REG_READ(ah, AR_TFCNT);

	
	REG_WRITE(ah, 0, AR_CCCNT);
	REG_WRITE(ah, 0, AR_RFCNT);
	REG_WRITE(ah, 0, AR_RCCNT);
	REG_WRITE(ah, 0, AR_TFCNT);

	
	REG_WRITE(ah, 0, AR_MIBC);

	
	common->cc_ani.cycles += cycles;
	common->cc_ani.rx_busy += busy;
	common->cc_ani.rx_frame += rx;
	common->cc_ani.tx_frame += tx;

	common->cc_survey.cycles += cycles;
	common->cc_survey.rx_busy += busy;
	common->cc_survey.rx_frame += rx;
	common->cc_survey.tx_frame += tx;
}
EXPORT_SYMBOL(ath_hw_cycle_counters_update);

int32_t ath_hw_get_listen_time(struct ath_common *common)
{
	struct ath_cycle_counters *cc = &common->cc_ani;
	int32_t listen_time;

	listen_time = (cc->cycles - cc->rx_frame - cc->tx_frame) /
		      (common->clockrate * 1000);

	memset(cc, 0, sizeof(*cc));

	return listen_time;
}
EXPORT_SYMBOL(ath_hw_get_listen_time);
