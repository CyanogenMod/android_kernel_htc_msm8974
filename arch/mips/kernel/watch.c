/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 David Daney
 */

#include <linux/sched.h>

#include <asm/processor.h>
#include <asm/watch.h>

void mips_install_watch_registers(void)
{
	struct mips3264_watch_reg_state *watches =
		&current->thread.watch.mips3264;
	switch (current_cpu_data.watch_reg_use_cnt) {
	default:
		BUG();
	case 4:
		write_c0_watchlo3(watches->watchlo[3]);
		write_c0_watchhi3(0x40000007 | watches->watchhi[3]);
	case 3:
		write_c0_watchlo2(watches->watchlo[2]);
		write_c0_watchhi2(0x40000007 | watches->watchhi[2]);
	case 2:
		write_c0_watchlo1(watches->watchlo[1]);
		write_c0_watchhi1(0x40000007 | watches->watchhi[1]);
	case 1:
		write_c0_watchlo0(watches->watchlo[0]);
		write_c0_watchhi0(0x40000007 | watches->watchhi[0]);
	}
}

void mips_read_watch_registers(void)
{
	struct mips3264_watch_reg_state *watches =
		&current->thread.watch.mips3264;
	switch (current_cpu_data.watch_reg_use_cnt) {
	default:
		BUG();
	case 4:
		watches->watchhi[3] = (read_c0_watchhi3() & 0x0fff);
	case 3:
		watches->watchhi[2] = (read_c0_watchhi2() & 0x0fff);
	case 2:
		watches->watchhi[1] = (read_c0_watchhi1() & 0x0fff);
	case 1:
		watches->watchhi[0] = (read_c0_watchhi0() & 0x0fff);
	}
	if (current_cpu_data.watch_reg_use_cnt == 1 &&
	    (watches->watchhi[0] & 7) == 0) {
		watches->watchhi[0] |= (watches->watchlo[0] & 7);
	}
 }

void mips_clear_watch_registers(void)
{
	switch (current_cpu_data.watch_reg_count) {
	default:
		BUG();
	case 8:
		write_c0_watchlo7(0);
	case 7:
		write_c0_watchlo6(0);
	case 6:
		write_c0_watchlo5(0);
	case 5:
		write_c0_watchlo4(0);
	case 4:
		write_c0_watchlo3(0);
	case 3:
		write_c0_watchlo2(0);
	case 2:
		write_c0_watchlo1(0);
	case 1:
		write_c0_watchlo0(0);
	}
}

__cpuinit void mips_probe_watch_registers(struct cpuinfo_mips *c)
{
	unsigned int t;

	if ((c->options & MIPS_CPU_WATCH) == 0)
		return;
	write_c0_watchlo0(7);
	t = read_c0_watchlo0();
	write_c0_watchlo0(0);
	c->watch_reg_masks[0] = t & 7;

	c->watch_reg_count = 1;
	c->watch_reg_use_cnt = 1;
	t = read_c0_watchhi0();
	write_c0_watchhi0(t | 0xff8);
	t = read_c0_watchhi0();
	c->watch_reg_masks[0] |= (t & 0xff8);
	if ((t & 0x80000000) == 0)
		return;

	write_c0_watchlo1(7);
	t = read_c0_watchlo1();
	write_c0_watchlo1(0);
	c->watch_reg_masks[1] = t & 7;

	c->watch_reg_count = 2;
	c->watch_reg_use_cnt = 2;
	t = read_c0_watchhi1();
	write_c0_watchhi1(t | 0xff8);
	t = read_c0_watchhi1();
	c->watch_reg_masks[1] |= (t & 0xff8);
	if ((t & 0x80000000) == 0)
		return;

	write_c0_watchlo2(7);
	t = read_c0_watchlo2();
	write_c0_watchlo2(0);
	c->watch_reg_masks[2] = t & 7;

	c->watch_reg_count = 3;
	c->watch_reg_use_cnt = 3;
	t = read_c0_watchhi2();
	write_c0_watchhi2(t | 0xff8);
	t = read_c0_watchhi2();
	c->watch_reg_masks[2] |= (t & 0xff8);
	if ((t & 0x80000000) == 0)
		return;

	write_c0_watchlo3(7);
	t = read_c0_watchlo3();
	write_c0_watchlo3(0);
	c->watch_reg_masks[3] = t & 7;

	c->watch_reg_count = 4;
	c->watch_reg_use_cnt = 4;
	t = read_c0_watchhi3();
	write_c0_watchhi3(t | 0xff8);
	t = read_c0_watchhi3();
	c->watch_reg_masks[3] |= (t & 0xff8);
	if ((t & 0x80000000) == 0)
		return;

	
	c->watch_reg_count = 5;
	t = read_c0_watchhi4();
	if ((t & 0x80000000) == 0)
		return;

	c->watch_reg_count = 6;
	t = read_c0_watchhi5();
	if ((t & 0x80000000) == 0)
		return;

	c->watch_reg_count = 7;
	t = read_c0_watchhi6();
	if ((t & 0x80000000) == 0)
		return;

	c->watch_reg_count = 8;
}
