/*
 *  Driver for the Conexant CX2584x Audio/Video decoder chip and related cores
 *
 *  Integrated Consumer Infrared Controller
 *
 *  Copyright (C) 2010  Andy Walls <awalls@md.metrocast.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <media/cx25840.h>
#include <media/rc-core.h>

#include "cx25840-core.h"

static unsigned int ir_debug;
module_param(ir_debug, int, 0644);
MODULE_PARM_DESC(ir_debug, "enable integrated IR debug messages");

#define CX25840_IR_REG_BASE 	0x200

#define CX25840_IR_CNTRL_REG	0x200
#define CNTRL_WIN_3_3	0x00000000
#define CNTRL_WIN_4_3	0x00000001
#define CNTRL_WIN_3_4	0x00000002
#define CNTRL_WIN_4_4	0x00000003
#define CNTRL_WIN	0x00000003
#define CNTRL_EDG_NONE	0x00000000
#define CNTRL_EDG_FALL	0x00000004
#define CNTRL_EDG_RISE	0x00000008
#define CNTRL_EDG_BOTH	0x0000000C
#define CNTRL_EDG	0x0000000C
#define CNTRL_DMD	0x00000010
#define CNTRL_MOD	0x00000020
#define CNTRL_RFE	0x00000040
#define CNTRL_TFE	0x00000080
#define CNTRL_RXE	0x00000100
#define CNTRL_TXE	0x00000200
#define CNTRL_RIC	0x00000400
#define CNTRL_TIC	0x00000800
#define CNTRL_CPL	0x00001000
#define CNTRL_LBM	0x00002000
#define CNTRL_R		0x00004000

#define CX25840_IR_TXCLK_REG	0x204
#define TXCLK_TCD	0x0000FFFF

#define CX25840_IR_RXCLK_REG	0x208
#define RXCLK_RCD	0x0000FFFF

#define CX25840_IR_CDUTY_REG	0x20C
#define CDUTY_CDC	0x0000000F

#define CX25840_IR_STATS_REG	0x210
#define STATS_RTO	0x00000001
#define STATS_ROR	0x00000002
#define STATS_RBY	0x00000004
#define STATS_TBY	0x00000008
#define STATS_RSR	0x00000010
#define STATS_TSR	0x00000020

#define CX25840_IR_IRQEN_REG	0x214
#define IRQEN_RTE	0x00000001
#define IRQEN_ROE	0x00000002
#define IRQEN_RSE	0x00000010
#define IRQEN_TSE	0x00000020
#define IRQEN_MSK	0x00000033

#define CX25840_IR_FILTR_REG	0x218
#define FILTR_LPF	0x0000FFFF

#define CX25840_IR_FIFO_REG	0x23C
#define FIFO_RXTX	0x0000FFFF
#define FIFO_RXTX_LVL	0x00010000
#define FIFO_RXTX_RTO	0x0001FFFF
#define FIFO_RX_NDV	0x00020000
#define FIFO_RX_DEPTH	8
#define FIFO_TX_DEPTH	8

#define CX25840_VIDCLK_FREQ	108000000 
#define CX25840_IR_REFCLK_FREQ	(CX25840_VIDCLK_FREQ / 2)

union cx25840_ir_fifo_rec {
	u32 hw_fifo_data;
	struct ir_raw_event ir_core_data;
};

#define CX25840_IR_RX_KFIFO_SIZE    (256 * sizeof(union cx25840_ir_fifo_rec))
#define CX25840_IR_TX_KFIFO_SIZE    (256 * sizeof(union cx25840_ir_fifo_rec))

struct cx25840_ir_state {
	struct i2c_client *c;

	struct v4l2_subdev_ir_parameters rx_params;
	struct mutex rx_params_lock; 
	atomic_t rxclk_divider;
	atomic_t rx_invert;

	struct kfifo rx_kfifo;
	spinlock_t rx_kfifo_lock; 

	struct v4l2_subdev_ir_parameters tx_params;
	struct mutex tx_params_lock; 
	atomic_t txclk_divider;
};

static inline struct cx25840_ir_state *to_ir_state(struct v4l2_subdev *sd)
{
	struct cx25840_state *state = to_state(sd);
	return state ? state->ir_state : NULL;
}


static inline u16 count_to_clock_divider(unsigned int d)
{
	if (d > RXCLK_RCD + 1)
		d = RXCLK_RCD;
	else if (d < 2)
		d = 1;
	else
		d--;
	return (u16) d;
}

static inline u16 ns_to_clock_divider(unsigned int ns)
{
	return count_to_clock_divider(
		DIV_ROUND_CLOSEST(CX25840_IR_REFCLK_FREQ / 1000000 * ns, 1000));
}

static inline unsigned int clock_divider_to_ns(unsigned int divider)
{
	
	return DIV_ROUND_CLOSEST((divider + 1) * 1000,
				 CX25840_IR_REFCLK_FREQ / 1000000);
}

static inline u16 carrier_freq_to_clock_divider(unsigned int freq)
{
	return count_to_clock_divider(
			  DIV_ROUND_CLOSEST(CX25840_IR_REFCLK_FREQ, freq * 16));
}

static inline unsigned int clock_divider_to_carrier_freq(unsigned int divider)
{
	return DIV_ROUND_CLOSEST(CX25840_IR_REFCLK_FREQ, (divider + 1) * 16);
}

static inline u16 freq_to_clock_divider(unsigned int freq,
					unsigned int rollovers)
{
	return count_to_clock_divider(
		   DIV_ROUND_CLOSEST(CX25840_IR_REFCLK_FREQ, freq * rollovers));
}

static inline unsigned int clock_divider_to_freq(unsigned int divider,
						 unsigned int rollovers)
{
	return DIV_ROUND_CLOSEST(CX25840_IR_REFCLK_FREQ,
				 (divider + 1) * rollovers);
}

static inline u16 count_to_lpf_count(unsigned int d)
{
	if (d > FILTR_LPF)
		d = FILTR_LPF;
	else if (d < 4)
		d = 0;
	return (u16) d;
}

static inline u16 ns_to_lpf_count(unsigned int ns)
{
	return count_to_lpf_count(
		DIV_ROUND_CLOSEST(CX25840_IR_REFCLK_FREQ / 1000000 * ns, 1000));
}

static inline unsigned int lpf_count_to_ns(unsigned int count)
{
	
	return DIV_ROUND_CLOSEST(count * 1000,
				 CX25840_IR_REFCLK_FREQ / 1000000);
}

static inline unsigned int lpf_count_to_us(unsigned int count)
{
	
	return DIV_ROUND_CLOSEST(count, CX25840_IR_REFCLK_FREQ / 1000000);
}

static u32 clock_divider_to_resolution(u16 divider)
{
	return DIV_ROUND_CLOSEST((1 << 2)  * ((u32) divider + 1) * 1000,
				 CX25840_IR_REFCLK_FREQ / 1000000);
}

static u64 pulse_width_count_to_ns(u16 count, u16 divider)
{
	u64 n;
	u32 rem;

	n = (((u64) count << 2) | 0x3) * (divider + 1) * 1000; 
	rem = do_div(n, CX25840_IR_REFCLK_FREQ / 1000000);     
	if (rem >= CX25840_IR_REFCLK_FREQ / 1000000 / 2)
		n++;
	return n;
}

#if 0
static u16 ns_to_pulse_width_count(u32 ns, u16 divider)
{
	u64 n;
	u32 d;
	u32 rem;

	n = ((u64) ns) * CX25840_IR_REFCLK_FREQ / 1000000; 
	d = (1 << 2) * ((u32) divider + 1) * 1000; 
	rem = do_div(n, d);
	if (rem >= d / 2)
		n++;

	if (n > FIFO_RXTX)
		n = FIFO_RXTX;
	else if (n == 0)
		n = 1;
	return (u16) n;
}

#endif
static unsigned int pulse_width_count_to_us(u16 count, u16 divider)
{
	u64 n;
	u32 rem;

	n = (((u64) count << 2) | 0x3) * (divider + 1);    
	rem = do_div(n, CX25840_IR_REFCLK_FREQ / 1000000); 
	if (rem >= CX25840_IR_REFCLK_FREQ / 1000000 / 2)
		n++;
	return (unsigned int) n;
}

static u64 ns_to_pulse_clocks(u32 ns)
{
	u64 clocks;
	u32 rem;
	clocks = CX25840_IR_REFCLK_FREQ / 1000000 * (u64) ns; 
	rem = do_div(clocks, 1000);                         
	if (rem >= 1000 / 2)
		clocks++;
	return clocks;
}

static u16 pulse_clocks_to_clock_divider(u64 count)
{
	u32 rem;

	rem = do_div(count, (FIFO_RXTX << 2) | 0x3);

	
	if (count > RXCLK_RCD + 1)
		count = RXCLK_RCD;
	else if (count < 2)
		count = 1;
	else
		count--;
	return (u16) count;
}

enum tx_fifo_watermark {
	TX_FIFO_HALF_EMPTY = 0,
	TX_FIFO_EMPTY      = CNTRL_TIC,
};

enum rx_fifo_watermark {
	RX_FIFO_HALF_FULL = 0,
	RX_FIFO_NOT_EMPTY = CNTRL_RIC,
};

static inline void control_tx_irq_watermark(struct i2c_client *c,
					    enum tx_fifo_watermark level)
{
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~CNTRL_TIC, level);
}

static inline void control_rx_irq_watermark(struct i2c_client *c,
					    enum rx_fifo_watermark level)
{
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~CNTRL_RIC, level);
}

static inline void control_tx_enable(struct i2c_client *c, bool enable)
{
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~(CNTRL_TXE | CNTRL_TFE),
			enable ? (CNTRL_TXE | CNTRL_TFE) : 0);
}

static inline void control_rx_enable(struct i2c_client *c, bool enable)
{
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~(CNTRL_RXE | CNTRL_RFE),
			enable ? (CNTRL_RXE | CNTRL_RFE) : 0);
}

static inline void control_tx_modulation_enable(struct i2c_client *c,
						bool enable)
{
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~CNTRL_MOD,
			enable ? CNTRL_MOD : 0);
}

static inline void control_rx_demodulation_enable(struct i2c_client *c,
						  bool enable)
{
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~CNTRL_DMD,
			enable ? CNTRL_DMD : 0);
}

static inline void control_rx_s_edge_detection(struct i2c_client *c,
					       u32 edge_types)
{
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~CNTRL_EDG_BOTH,
			edge_types & CNTRL_EDG_BOTH);
}

static void control_rx_s_carrier_window(struct i2c_client *c,
					unsigned int carrier,
					unsigned int *carrier_range_low,
					unsigned int *carrier_range_high)
{
	u32 v;
	unsigned int c16 = carrier * 16;

	if (*carrier_range_low < DIV_ROUND_CLOSEST(c16, 16 + 3)) {
		v = CNTRL_WIN_3_4;
		*carrier_range_low = DIV_ROUND_CLOSEST(c16, 16 + 4);
	} else {
		v = CNTRL_WIN_3_3;
		*carrier_range_low = DIV_ROUND_CLOSEST(c16, 16 + 3);
	}

	if (*carrier_range_high > DIV_ROUND_CLOSEST(c16, 16 - 3)) {
		v |= CNTRL_WIN_4_3;
		*carrier_range_high = DIV_ROUND_CLOSEST(c16, 16 - 4);
	} else {
		v |= CNTRL_WIN_3_3;
		*carrier_range_high = DIV_ROUND_CLOSEST(c16, 16 - 3);
	}
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~CNTRL_WIN, v);
}

static inline void control_tx_polarity_invert(struct i2c_client *c,
					      bool invert)
{
	cx25840_and_or4(c, CX25840_IR_CNTRL_REG, ~CNTRL_CPL,
			invert ? CNTRL_CPL : 0);
}

static unsigned int txclk_tx_s_carrier(struct i2c_client *c,
				       unsigned int freq,
				       u16 *divider)
{
	*divider = carrier_freq_to_clock_divider(freq);
	cx25840_write4(c, CX25840_IR_TXCLK_REG, *divider);
	return clock_divider_to_carrier_freq(*divider);
}

static unsigned int rxclk_rx_s_carrier(struct i2c_client *c,
				       unsigned int freq,
				       u16 *divider)
{
	*divider = carrier_freq_to_clock_divider(freq);
	cx25840_write4(c, CX25840_IR_RXCLK_REG, *divider);
	return clock_divider_to_carrier_freq(*divider);
}

static u32 txclk_tx_s_max_pulse_width(struct i2c_client *c, u32 ns,
				      u16 *divider)
{
	u64 pulse_clocks;

	if (ns > IR_MAX_DURATION)
		ns = IR_MAX_DURATION;
	pulse_clocks = ns_to_pulse_clocks(ns);
	*divider = pulse_clocks_to_clock_divider(pulse_clocks);
	cx25840_write4(c, CX25840_IR_TXCLK_REG, *divider);
	return (u32) pulse_width_count_to_ns(FIFO_RXTX, *divider);
}

static u32 rxclk_rx_s_max_pulse_width(struct i2c_client *c, u32 ns,
				      u16 *divider)
{
	u64 pulse_clocks;

	if (ns > IR_MAX_DURATION)
		ns = IR_MAX_DURATION;
	pulse_clocks = ns_to_pulse_clocks(ns);
	*divider = pulse_clocks_to_clock_divider(pulse_clocks);
	cx25840_write4(c, CX25840_IR_RXCLK_REG, *divider);
	return (u32) pulse_width_count_to_ns(FIFO_RXTX, *divider);
}

static unsigned int cduty_tx_s_duty_cycle(struct i2c_client *c,
					  unsigned int duty_cycle)
{
	u32 n;
	n = DIV_ROUND_CLOSEST(duty_cycle * 100, 625); 
	if (n != 0)
		n--;
	if (n > 15)
		n = 15;
	cx25840_write4(c, CX25840_IR_CDUTY_REG, n);
	return DIV_ROUND_CLOSEST((n + 1) * 100, 16);
}

static u32 filter_rx_s_min_width(struct i2c_client *c, u32 min_width_ns)
{
	u32 count = ns_to_lpf_count(min_width_ns);
	cx25840_write4(c, CX25840_IR_FILTR_REG, count);
	return lpf_count_to_ns(count);
}

static inline void irqenable_rx(struct v4l2_subdev *sd, u32 mask)
{
	struct cx25840_state *state = to_state(sd);

	if (is_cx23885(state) || is_cx23887(state))
		mask ^= IRQEN_MSK;
	mask &= (IRQEN_RTE | IRQEN_ROE | IRQEN_RSE);
	cx25840_and_or4(state->c, CX25840_IR_IRQEN_REG,
			~(IRQEN_RTE | IRQEN_ROE | IRQEN_RSE), mask);
}

static inline void irqenable_tx(struct v4l2_subdev *sd, u32 mask)
{
	struct cx25840_state *state = to_state(sd);

	if (is_cx23885(state) || is_cx23887(state))
		mask ^= IRQEN_MSK;
	mask &= IRQEN_TSE;
	cx25840_and_or4(state->c, CX25840_IR_IRQEN_REG, ~IRQEN_TSE, mask);
}

int cx25840_ir_irq_handler(struct v4l2_subdev *sd, u32 status, bool *handled)
{
	struct cx25840_state *state = to_state(sd);
	struct cx25840_ir_state *ir_state = to_ir_state(sd);
	struct i2c_client *c = NULL;
	unsigned long flags;

	union cx25840_ir_fifo_rec rx_data[FIFO_RX_DEPTH];
	unsigned int i, j, k;
	u32 events, v;
	int tsr, rsr, rto, ror, tse, rse, rte, roe, kror;
	u32 cntrl, irqen, stats;

	*handled = false;
	if (ir_state == NULL)
		return -ENODEV;

	c = ir_state->c;

	
	if (!(is_cx23885(state) || is_cx23887(state)))
		return -ENODEV;

	cntrl = cx25840_read4(c, CX25840_IR_CNTRL_REG);
	irqen = cx25840_read4(c, CX25840_IR_IRQEN_REG);
	if (is_cx23885(state) || is_cx23887(state))
		irqen ^= IRQEN_MSK;
	stats = cx25840_read4(c, CX25840_IR_STATS_REG);

	tsr = stats & STATS_TSR; 
	rsr = stats & STATS_RSR; 
	rto = stats & STATS_RTO; 
	ror = stats & STATS_ROR; 

	tse = irqen & IRQEN_TSE; 
	rse = irqen & IRQEN_RSE; 
	rte = irqen & IRQEN_RTE; 
	roe = irqen & IRQEN_ROE; 

	v4l2_dbg(2, ir_debug, sd, "IR IRQ Status:  %s %s %s %s %s %s\n",
		 tsr ? "tsr" : "   ", rsr ? "rsr" : "   ",
		 rto ? "rto" : "   ", ror ? "ror" : "   ",
		 stats & STATS_TBY ? "tby" : "   ",
		 stats & STATS_RBY ? "rby" : "   ");

	v4l2_dbg(2, ir_debug, sd, "IR IRQ Enables: %s %s %s %s\n",
		 tse ? "tse" : "   ", rse ? "rse" : "   ",
		 rte ? "rte" : "   ", roe ? "roe" : "   ");

	if (tse && tsr) {
		
		irqenable_tx(sd, 0);
		events = V4L2_SUBDEV_IR_TX_FIFO_SERVICE_REQ;
		v4l2_subdev_notify(sd, V4L2_SUBDEV_IR_TX_NOTIFY, &events);
		*handled = true;
	}

	kror = 0;
	if ((rse && rsr) || (rte && rto)) {
		for (i = 0, v = FIFO_RX_NDV;
		     (v & FIFO_RX_NDV) && !kror; i = 0) {
			for (j = 0;
			     (v & FIFO_RX_NDV) && j < FIFO_RX_DEPTH; j++) {
				v = cx25840_read4(c, CX25840_IR_FIFO_REG);
				rx_data[i].hw_fifo_data = v & ~FIFO_RX_NDV;
				i++;
			}
			if (i == 0)
				break;
			j = i * sizeof(union cx25840_ir_fifo_rec);
			k = kfifo_in_locked(&ir_state->rx_kfifo,
					    (unsigned char *) rx_data, j,
					    &ir_state->rx_kfifo_lock);
			if (k != j)
				kror++; 
		}
		*handled = true;
	}

	events = 0;
	v = 0;
	if (kror) {
		events |= V4L2_SUBDEV_IR_RX_SW_FIFO_OVERRUN;
		v4l2_err(sd, "IR receiver software FIFO overrun\n");
	}
	if (roe && ror) {
		v |= CNTRL_RFE;
		events |= V4L2_SUBDEV_IR_RX_HW_FIFO_OVERRUN;
		v4l2_err(sd, "IR receiver hardware FIFO overrun\n");
	}
	if (rte && rto) {
		v |= CNTRL_RXE;
		events |= V4L2_SUBDEV_IR_RX_END_OF_RX_DETECTED;
	}
	if (v) {
		
		cx25840_write4(c, CX25840_IR_CNTRL_REG, cntrl & ~v);
		cx25840_write4(c, CX25840_IR_CNTRL_REG, cntrl);
		*handled = true;
	}
	spin_lock_irqsave(&ir_state->rx_kfifo_lock, flags);
	if (kfifo_len(&ir_state->rx_kfifo) >= CX25840_IR_RX_KFIFO_SIZE / 2)
		events |= V4L2_SUBDEV_IR_RX_FIFO_SERVICE_REQ;
	spin_unlock_irqrestore(&ir_state->rx_kfifo_lock, flags);

	if (events)
		v4l2_subdev_notify(sd, V4L2_SUBDEV_IR_RX_NOTIFY, &events);
	return 0;
}

static int cx25840_ir_rx_read(struct v4l2_subdev *sd, u8 *buf, size_t count,
			      ssize_t *num)
{
	struct cx25840_ir_state *ir_state = to_ir_state(sd);
	bool invert;
	u16 divider;
	unsigned int i, n;
	union cx25840_ir_fifo_rec *p;
	unsigned u, v, w;

	if (ir_state == NULL)
		return -ENODEV;

	invert = (bool) atomic_read(&ir_state->rx_invert);
	divider = (u16) atomic_read(&ir_state->rxclk_divider);

	n = count / sizeof(union cx25840_ir_fifo_rec)
		* sizeof(union cx25840_ir_fifo_rec);
	if (n == 0) {
		*num = 0;
		return 0;
	}

	n = kfifo_out_locked(&ir_state->rx_kfifo, buf, n,
			     &ir_state->rx_kfifo_lock);

	n /= sizeof(union cx25840_ir_fifo_rec);
	*num = n * sizeof(union cx25840_ir_fifo_rec);

	for (p = (union cx25840_ir_fifo_rec *) buf, i = 0; i < n; p++, i++) {

		if ((p->hw_fifo_data & FIFO_RXTX_RTO) == FIFO_RXTX_RTO) {
			
			u = 0;
			w = 1;
		} else {
			u = (p->hw_fifo_data & FIFO_RXTX_LVL) ? 1 : 0;
			if (invert)
				u = u ? 0 : 1;
			w = 0;
		}

		v = (unsigned) pulse_width_count_to_ns(
				  (u16) (p->hw_fifo_data & FIFO_RXTX), divider);
		if (v > IR_MAX_DURATION)
			v = IR_MAX_DURATION;

		init_ir_raw_event(&p->ir_core_data);
		p->ir_core_data.pulse = u;
		p->ir_core_data.duration = v;
		p->ir_core_data.timeout = w;

		v4l2_dbg(2, ir_debug, sd, "rx read: %10u ns  %s  %s\n",
			 v, u ? "mark" : "space", w ? "(timed out)" : "");
		if (w)
			v4l2_dbg(2, ir_debug, sd, "rx read: end of rx\n");
	}
	return 0;
}

static int cx25840_ir_rx_g_parameters(struct v4l2_subdev *sd,
				      struct v4l2_subdev_ir_parameters *p)
{
	struct cx25840_ir_state *ir_state = to_ir_state(sd);

	if (ir_state == NULL)
		return -ENODEV;

	mutex_lock(&ir_state->rx_params_lock);
	memcpy(p, &ir_state->rx_params,
				      sizeof(struct v4l2_subdev_ir_parameters));
	mutex_unlock(&ir_state->rx_params_lock);
	return 0;
}

static int cx25840_ir_rx_shutdown(struct v4l2_subdev *sd)
{
	struct cx25840_ir_state *ir_state = to_ir_state(sd);
	struct i2c_client *c;

	if (ir_state == NULL)
		return -ENODEV;

	c = ir_state->c;
	mutex_lock(&ir_state->rx_params_lock);

	
	irqenable_rx(sd, 0);
	control_rx_enable(c, false);
	control_rx_demodulation_enable(c, false);
	control_rx_s_edge_detection(c, CNTRL_EDG_NONE);
	filter_rx_s_min_width(c, 0);
	cx25840_write4(c, CX25840_IR_RXCLK_REG, RXCLK_RCD);

	ir_state->rx_params.shutdown = true;

	mutex_unlock(&ir_state->rx_params_lock);
	return 0;
}

static int cx25840_ir_rx_s_parameters(struct v4l2_subdev *sd,
				      struct v4l2_subdev_ir_parameters *p)
{
	struct cx25840_ir_state *ir_state = to_ir_state(sd);
	struct i2c_client *c;
	struct v4l2_subdev_ir_parameters *o;
	u16 rxclk_divider;

	if (ir_state == NULL)
		return -ENODEV;

	if (p->shutdown)
		return cx25840_ir_rx_shutdown(sd);

	if (p->mode != V4L2_SUBDEV_IR_MODE_PULSE_WIDTH)
		return -ENOSYS;

	c = ir_state->c;
	o = &ir_state->rx_params;

	mutex_lock(&ir_state->rx_params_lock);

	o->shutdown = p->shutdown;

	p->mode = V4L2_SUBDEV_IR_MODE_PULSE_WIDTH;
	o->mode = p->mode;

	p->bytes_per_data_element = sizeof(union cx25840_ir_fifo_rec);
	o->bytes_per_data_element = p->bytes_per_data_element;

	
	irqenable_rx(sd, 0);
	control_rx_enable(c, false);

	control_rx_demodulation_enable(c, p->modulation);
	o->modulation = p->modulation;

	if (p->modulation) {
		p->carrier_freq = rxclk_rx_s_carrier(c, p->carrier_freq,
						     &rxclk_divider);

		o->carrier_freq = p->carrier_freq;

		p->duty_cycle = 50;
		o->duty_cycle = p->duty_cycle;

		control_rx_s_carrier_window(c, p->carrier_freq,
					    &p->carrier_range_lower,
					    &p->carrier_range_upper);
		o->carrier_range_lower = p->carrier_range_lower;
		o->carrier_range_upper = p->carrier_range_upper;

		p->max_pulse_width =
			(u32) pulse_width_count_to_ns(FIFO_RXTX, rxclk_divider);
	} else {
		p->max_pulse_width =
			    rxclk_rx_s_max_pulse_width(c, p->max_pulse_width,
						       &rxclk_divider);
	}
	o->max_pulse_width = p->max_pulse_width;
	atomic_set(&ir_state->rxclk_divider, rxclk_divider);

	p->noise_filter_min_width =
			    filter_rx_s_min_width(c, p->noise_filter_min_width);
	o->noise_filter_min_width = p->noise_filter_min_width;

	p->resolution = clock_divider_to_resolution(rxclk_divider);
	o->resolution = p->resolution;

	
	control_rx_irq_watermark(c, RX_FIFO_HALF_FULL);

	control_rx_s_edge_detection(c, CNTRL_EDG_BOTH);

	o->invert_level = p->invert_level;
	atomic_set(&ir_state->rx_invert, p->invert_level);

	o->interrupt_enable = p->interrupt_enable;
	o->enable = p->enable;
	if (p->enable) {
		unsigned long flags;

		spin_lock_irqsave(&ir_state->rx_kfifo_lock, flags);
		kfifo_reset(&ir_state->rx_kfifo);
		spin_unlock_irqrestore(&ir_state->rx_kfifo_lock, flags);
		if (p->interrupt_enable)
			irqenable_rx(sd, IRQEN_RSE | IRQEN_RTE | IRQEN_ROE);
		control_rx_enable(c, p->enable);
	}

	mutex_unlock(&ir_state->rx_params_lock);
	return 0;
}

static int cx25840_ir_tx_write(struct v4l2_subdev *sd, u8 *buf, size_t count,
			       ssize_t *num)
{
	struct cx25840_ir_state *ir_state = to_ir_state(sd);
	struct i2c_client *c;

	if (ir_state == NULL)
		return -ENODEV;

	c = ir_state->c;
#if 0
	/*
	 * FIXME - the code below is an incomplete and untested sketch of what
	 * may need to be done.  The critical part is to get 4 (or 8) pulses
	 * from the tx_kfifo, or converted from ns to the proper units from the
	 * input, and push them off to the hardware Tx FIFO right away, if the
	 * HW TX fifo needs service.  The rest can be pushed to the tx_kfifo in
	 * a less critical timeframe.  Also watch out for overruning the
	 * tx_kfifo - don't let it happen and let the caller know not all his
	 * pulses were written.
	 */
	u32 *ns_pulse = (u32 *) buf;
	unsigned int n;
	u32 fifo_pulse[FIFO_TX_DEPTH];
	u32 mark;

	
	n = CX25840_IR_TX_KFIFO_SIZE - kfifo_len(ir_state->tx_kfifo);
	n = min(n, (unsigned int) count);
	n /= sizeof(u32);

	for (i = 0; i < n; ) {
		for (j = 0; j < FIFO_TX_DEPTH / 2 && i < n; j++) {
			mark = ns_pulse[i] & LEVEL_MASK;
			fifo_pulse[j] = ns_to_pulse_width_count(
					 ns_pulse[i] &
					       ~LEVEL_MASK,
					 ir_state->txclk_divider);
			if (mark)
				fifo_pulse[j] &= FIFO_RXTX_LVL;
			i++;
		}
		kfifo_put(ir_state->tx_kfifo, (u8 *) fifo_pulse,
							       j * sizeof(u32));
	}
	*num = n * sizeof(u32);
#else
	
	irqenable_tx(sd, IRQEN_TSE);
	*num = count;
#endif
	return 0;
}

static int cx25840_ir_tx_g_parameters(struct v4l2_subdev *sd,
				      struct v4l2_subdev_ir_parameters *p)
{
	struct cx25840_ir_state *ir_state = to_ir_state(sd);

	if (ir_state == NULL)
		return -ENODEV;

	mutex_lock(&ir_state->tx_params_lock);
	memcpy(p, &ir_state->tx_params,
				      sizeof(struct v4l2_subdev_ir_parameters));
	mutex_unlock(&ir_state->tx_params_lock);
	return 0;
}

static int cx25840_ir_tx_shutdown(struct v4l2_subdev *sd)
{
	struct cx25840_ir_state *ir_state = to_ir_state(sd);
	struct i2c_client *c;

	if (ir_state == NULL)
		return -ENODEV;

	c = ir_state->c;
	mutex_lock(&ir_state->tx_params_lock);

	
	irqenable_tx(sd, 0);
	control_tx_enable(c, false);
	control_tx_modulation_enable(c, false);
	cx25840_write4(c, CX25840_IR_TXCLK_REG, TXCLK_TCD);

	ir_state->tx_params.shutdown = true;

	mutex_unlock(&ir_state->tx_params_lock);
	return 0;
}

static int cx25840_ir_tx_s_parameters(struct v4l2_subdev *sd,
				      struct v4l2_subdev_ir_parameters *p)
{
	struct cx25840_ir_state *ir_state = to_ir_state(sd);
	struct i2c_client *c;
	struct v4l2_subdev_ir_parameters *o;
	u16 txclk_divider;

	if (ir_state == NULL)
		return -ENODEV;

	if (p->shutdown)
		return cx25840_ir_tx_shutdown(sd);

	if (p->mode != V4L2_SUBDEV_IR_MODE_PULSE_WIDTH)
		return -ENOSYS;

	c = ir_state->c;
	o = &ir_state->tx_params;
	mutex_lock(&ir_state->tx_params_lock);

	o->shutdown = p->shutdown;

	p->mode = V4L2_SUBDEV_IR_MODE_PULSE_WIDTH;
	o->mode = p->mode;

	p->bytes_per_data_element = sizeof(union cx25840_ir_fifo_rec);
	o->bytes_per_data_element = p->bytes_per_data_element;

	
	irqenable_tx(sd, 0);
	control_tx_enable(c, false);

	control_tx_modulation_enable(c, p->modulation);
	o->modulation = p->modulation;

	if (p->modulation) {
		p->carrier_freq = txclk_tx_s_carrier(c, p->carrier_freq,
						     &txclk_divider);
		o->carrier_freq = p->carrier_freq;

		p->duty_cycle = cduty_tx_s_duty_cycle(c, p->duty_cycle);
		o->duty_cycle = p->duty_cycle;

		p->max_pulse_width =
			(u32) pulse_width_count_to_ns(FIFO_RXTX, txclk_divider);
	} else {
		p->max_pulse_width =
			    txclk_tx_s_max_pulse_width(c, p->max_pulse_width,
						       &txclk_divider);
	}
	o->max_pulse_width = p->max_pulse_width;
	atomic_set(&ir_state->txclk_divider, txclk_divider);

	p->resolution = clock_divider_to_resolution(txclk_divider);
	o->resolution = p->resolution;

	
	control_tx_irq_watermark(c, TX_FIFO_HALF_EMPTY);

	control_tx_polarity_invert(c, p->invert_carrier_sense);
	o->invert_carrier_sense = p->invert_carrier_sense;

	o->invert_level = p->invert_level;

	o->interrupt_enable = p->interrupt_enable;
	o->enable = p->enable;
	if (p->enable) {
		
		if (p->interrupt_enable)
			irqenable_tx(sd, IRQEN_TSE);
		control_tx_enable(c, p->enable);
	}

	mutex_unlock(&ir_state->tx_params_lock);
	return 0;
}


int cx25840_ir_log_status(struct v4l2_subdev *sd)
{
	struct cx25840_state *state = to_state(sd);
	struct i2c_client *c = state->c;
	char *s;
	int i, j;
	u32 cntrl, txclk, rxclk, cduty, stats, irqen, filtr;

	
	if (is_cx23888(state))
		return 0;

	cntrl = cx25840_read4(c, CX25840_IR_CNTRL_REG);
	txclk = cx25840_read4(c, CX25840_IR_TXCLK_REG) & TXCLK_TCD;
	rxclk = cx25840_read4(c, CX25840_IR_RXCLK_REG) & RXCLK_RCD;
	cduty = cx25840_read4(c, CX25840_IR_CDUTY_REG) & CDUTY_CDC;
	stats = cx25840_read4(c, CX25840_IR_STATS_REG);
	irqen = cx25840_read4(c, CX25840_IR_IRQEN_REG);
	if (is_cx23885(state) || is_cx23887(state))
		irqen ^= IRQEN_MSK;
	filtr = cx25840_read4(c, CX25840_IR_FILTR_REG) & FILTR_LPF;

	v4l2_info(sd, "IR Receiver:\n");
	v4l2_info(sd, "\tEnabled:                           %s\n",
		  cntrl & CNTRL_RXE ? "yes" : "no");
	v4l2_info(sd, "\tDemodulation from a carrier:       %s\n",
		  cntrl & CNTRL_DMD ? "enabled" : "disabled");
	v4l2_info(sd, "\tFIFO:                              %s\n",
		  cntrl & CNTRL_RFE ? "enabled" : "disabled");
	switch (cntrl & CNTRL_EDG) {
	case CNTRL_EDG_NONE:
		s = "disabled";
		break;
	case CNTRL_EDG_FALL:
		s = "falling edge";
		break;
	case CNTRL_EDG_RISE:
		s = "rising edge";
		break;
	case CNTRL_EDG_BOTH:
		s = "rising & falling edges";
		break;
	default:
		s = "??? edge";
		break;
	}
	v4l2_info(sd, "\tPulse timers' start/stop trigger:  %s\n", s);
	v4l2_info(sd, "\tFIFO data on pulse timer overflow: %s\n",
		  cntrl & CNTRL_R ? "not loaded" : "overflow marker");
	v4l2_info(sd, "\tFIFO interrupt watermark:          %s\n",
		  cntrl & CNTRL_RIC ? "not empty" : "half full or greater");
	v4l2_info(sd, "\tLoopback mode:                     %s\n",
		  cntrl & CNTRL_LBM ? "loopback active" : "normal receive");
	if (cntrl & CNTRL_DMD) {
		v4l2_info(sd, "\tExpected carrier (16 clocks):      %u Hz\n",
			  clock_divider_to_carrier_freq(rxclk));
		switch (cntrl & CNTRL_WIN) {
		case CNTRL_WIN_3_3:
			i = 3;
			j = 3;
			break;
		case CNTRL_WIN_4_3:
			i = 4;
			j = 3;
			break;
		case CNTRL_WIN_3_4:
			i = 3;
			j = 4;
			break;
		case CNTRL_WIN_4_4:
			i = 4;
			j = 4;
			break;
		default:
			i = 0;
			j = 0;
			break;
		}
		v4l2_info(sd, "\tNext carrier edge window:          16 clocks "
			  "-%1d/+%1d, %u to %u Hz\n", i, j,
			  clock_divider_to_freq(rxclk, 16 + j),
			  clock_divider_to_freq(rxclk, 16 - i));
	}
	v4l2_info(sd, "\tMax measurable pulse width:        %u us, %llu ns\n",
		  pulse_width_count_to_us(FIFO_RXTX, rxclk),
		  pulse_width_count_to_ns(FIFO_RXTX, rxclk));
	v4l2_info(sd, "\tLow pass filter:                   %s\n",
		  filtr ? "enabled" : "disabled");
	if (filtr)
		v4l2_info(sd, "\tMin acceptable pulse width (LPF):  %u us, "
			  "%u ns\n",
			  lpf_count_to_us(filtr),
			  lpf_count_to_ns(filtr));
	v4l2_info(sd, "\tPulse width timer timed-out:       %s\n",
		  stats & STATS_RTO ? "yes" : "no");
	v4l2_info(sd, "\tPulse width timer time-out intr:   %s\n",
		  irqen & IRQEN_RTE ? "enabled" : "disabled");
	v4l2_info(sd, "\tFIFO overrun:                      %s\n",
		  stats & STATS_ROR ? "yes" : "no");
	v4l2_info(sd, "\tFIFO overrun interrupt:            %s\n",
		  irqen & IRQEN_ROE ? "enabled" : "disabled");
	v4l2_info(sd, "\tBusy:                              %s\n",
		  stats & STATS_RBY ? "yes" : "no");
	v4l2_info(sd, "\tFIFO service requested:            %s\n",
		  stats & STATS_RSR ? "yes" : "no");
	v4l2_info(sd, "\tFIFO service request interrupt:    %s\n",
		  irqen & IRQEN_RSE ? "enabled" : "disabled");

	v4l2_info(sd, "IR Transmitter:\n");
	v4l2_info(sd, "\tEnabled:                           %s\n",
		  cntrl & CNTRL_TXE ? "yes" : "no");
	v4l2_info(sd, "\tModulation onto a carrier:         %s\n",
		  cntrl & CNTRL_MOD ? "enabled" : "disabled");
	v4l2_info(sd, "\tFIFO:                              %s\n",
		  cntrl & CNTRL_TFE ? "enabled" : "disabled");
	v4l2_info(sd, "\tFIFO interrupt watermark:          %s\n",
		  cntrl & CNTRL_TIC ? "not empty" : "half full or less");
	v4l2_info(sd, "\tCarrier polarity:                  %s\n",
		  cntrl & CNTRL_CPL ? "space:burst mark:noburst"
				    : "space:noburst mark:burst");
	if (cntrl & CNTRL_MOD) {
		v4l2_info(sd, "\tCarrier (16 clocks):               %u Hz\n",
			  clock_divider_to_carrier_freq(txclk));
		v4l2_info(sd, "\tCarrier duty cycle:                %2u/16\n",
			  cduty + 1);
	}
	v4l2_info(sd, "\tMax pulse width:                   %u us, %llu ns\n",
		  pulse_width_count_to_us(FIFO_RXTX, txclk),
		  pulse_width_count_to_ns(FIFO_RXTX, txclk));
	v4l2_info(sd, "\tBusy:                              %s\n",
		  stats & STATS_TBY ? "yes" : "no");
	v4l2_info(sd, "\tFIFO service requested:            %s\n",
		  stats & STATS_TSR ? "yes" : "no");
	v4l2_info(sd, "\tFIFO service request interrupt:    %s\n",
		  irqen & IRQEN_TSE ? "enabled" : "disabled");

	return 0;
}


const struct v4l2_subdev_ir_ops cx25840_ir_ops = {
	.rx_read = cx25840_ir_rx_read,
	.rx_g_parameters = cx25840_ir_rx_g_parameters,
	.rx_s_parameters = cx25840_ir_rx_s_parameters,

	.tx_write = cx25840_ir_tx_write,
	.tx_g_parameters = cx25840_ir_tx_g_parameters,
	.tx_s_parameters = cx25840_ir_tx_s_parameters,
};


static const struct v4l2_subdev_ir_parameters default_rx_params = {
	.bytes_per_data_element = sizeof(union cx25840_ir_fifo_rec),
	.mode = V4L2_SUBDEV_IR_MODE_PULSE_WIDTH,

	.enable = false,
	.interrupt_enable = false,
	.shutdown = true,

	.modulation = true,
	.carrier_freq = 36000, 

	
	
	.noise_filter_min_width = 333333, 
	.carrier_range_lower = 35000,
	.carrier_range_upper = 37000,
	.invert_level = false,
};

static const struct v4l2_subdev_ir_parameters default_tx_params = {
	.bytes_per_data_element = sizeof(union cx25840_ir_fifo_rec),
	.mode = V4L2_SUBDEV_IR_MODE_PULSE_WIDTH,

	.enable = false,
	.interrupt_enable = false,
	.shutdown = true,

	.modulation = true,
	.carrier_freq = 36000, 
	.duty_cycle = 25,      
	.invert_level = false,
	.invert_carrier_sense = false,
};

int cx25840_ir_probe(struct v4l2_subdev *sd)
{
	struct cx25840_state *state = to_state(sd);
	struct cx25840_ir_state *ir_state;
	struct v4l2_subdev_ir_parameters default_params;

	
	if (!(is_cx23885(state) || is_cx23887(state)))
		return 0;

	ir_state = kzalloc(sizeof(struct cx25840_ir_state), GFP_KERNEL);
	if (ir_state == NULL)
		return -ENOMEM;

	spin_lock_init(&ir_state->rx_kfifo_lock);
	if (kfifo_alloc(&ir_state->rx_kfifo,
			CX25840_IR_RX_KFIFO_SIZE, GFP_KERNEL)) {
		kfree(ir_state);
		return -ENOMEM;
	}

	ir_state->c = state->c;
	state->ir_state = ir_state;

	
	if (is_cx23885(state) || is_cx23887(state))
		cx25840_write4(ir_state->c, CX25840_IR_IRQEN_REG, IRQEN_MSK);
	else
		cx25840_write4(ir_state->c, CX25840_IR_IRQEN_REG, 0);

	mutex_init(&ir_state->rx_params_lock);
	memcpy(&default_params, &default_rx_params,
		       sizeof(struct v4l2_subdev_ir_parameters));
	v4l2_subdev_call(sd, ir, rx_s_parameters, &default_params);

	mutex_init(&ir_state->tx_params_lock);
	memcpy(&default_params, &default_tx_params,
		       sizeof(struct v4l2_subdev_ir_parameters));
	v4l2_subdev_call(sd, ir, tx_s_parameters, &default_params);

	return 0;
}

int cx25840_ir_remove(struct v4l2_subdev *sd)
{
	struct cx25840_state *state = to_state(sd);
	struct cx25840_ir_state *ir_state = to_ir_state(sd);

	if (ir_state == NULL)
		return -ENODEV;

	cx25840_ir_rx_shutdown(sd);
	cx25840_ir_tx_shutdown(sd);

	kfifo_free(&ir_state->rx_kfifo);
	kfree(ir_state);
	state->ir_state = NULL;
	return 0;
}
