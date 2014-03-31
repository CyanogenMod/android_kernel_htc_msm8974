#ifndef DMAENGINE_H
#define DMAENGINE_H

#include <linux/bug.h>
#include <linux/dmaengine.h>

static inline void dma_cookie_init(struct dma_chan *chan)
{
	chan->cookie = DMA_MIN_COOKIE;
	chan->completed_cookie = DMA_MIN_COOKIE;
}

static inline dma_cookie_t dma_cookie_assign(struct dma_async_tx_descriptor *tx)
{
	struct dma_chan *chan = tx->chan;
	dma_cookie_t cookie;

	cookie = chan->cookie + 1;
	if (cookie < DMA_MIN_COOKIE)
		cookie = DMA_MIN_COOKIE;
	tx->cookie = chan->cookie = cookie;

	return cookie;
}

static inline void dma_cookie_complete(struct dma_async_tx_descriptor *tx)
{
	BUG_ON(tx->cookie < DMA_MIN_COOKIE);
	tx->chan->completed_cookie = tx->cookie;
	tx->cookie = 0;
}

static inline enum dma_status dma_cookie_status(struct dma_chan *chan,
	dma_cookie_t cookie, struct dma_tx_state *state)
{
	dma_cookie_t used, complete;

	used = chan->cookie;
	complete = chan->completed_cookie;
	barrier();
	if (state) {
		state->last = complete;
		state->used = used;
		state->residue = 0;
	}
	return dma_async_is_complete(cookie, complete, used);
}

static inline void dma_set_residue(struct dma_tx_state *state, u32 residue)
{
	if (state)
		state->residue = residue;
}

#endif
