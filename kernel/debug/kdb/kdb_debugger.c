/*
 * Created by: Jason Wessel <jason.wessel@windriver.com>
 *
 * Copyright (c) 2009 Wind River Systems, Inc.  All Rights Reserved.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kgdb.h>
#include <linux/kdb.h>
#include <linux/kdebug.h>
#include <linux/export.h>
#include "kdb_private.h"
#include "../debug_core.h"

get_char_func kdb_poll_funcs[] = {
	dbg_io_get_char,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};
EXPORT_SYMBOL_GPL(kdb_poll_funcs);

int kdb_poll_idx = 1;
EXPORT_SYMBOL_GPL(kdb_poll_idx);

static struct kgdb_state *kdb_ks;

int kdb_stub(struct kgdb_state *ks)
{
	int error = 0;
	kdb_bp_t *bp;
	unsigned long addr = kgdb_arch_pc(ks->ex_vector, ks->linux_regs);
	kdb_reason_t reason = KDB_REASON_OOPS;
	kdb_dbtrap_t db_result = KDB_DB_NOBPT;
	int i;

	kdb_ks = ks;
	if (KDB_STATE(REENTRY)) {
		reason = KDB_REASON_SWITCH;
		KDB_STATE_CLEAR(REENTRY);
		addr = instruction_pointer(ks->linux_regs);
	}
	ks->pass_exception = 0;
	if (atomic_read(&kgdb_setting_breakpoint))
		reason = KDB_REASON_KEYBOARD;

	for (i = 0, bp = kdb_breakpoints; i < KDB_MAXBPT; i++, bp++) {
		if ((bp->bp_enabled) && (bp->bp_addr == addr)) {
			reason = KDB_REASON_BREAK;
			db_result = KDB_DB_BPT;
			if (addr != instruction_pointer(ks->linux_regs))
				kgdb_arch_set_pc(ks->linux_regs, addr);
			break;
		}
	}
	if (reason == KDB_REASON_BREAK || reason == KDB_REASON_SWITCH) {
		for (i = 0, bp = kdb_breakpoints; i < KDB_MAXBPT; i++, bp++) {
			if (bp->bp_free)
				continue;
			if (bp->bp_addr == addr) {
				bp->bp_delay = 1;
				bp->bp_delayed = 1;
				reason = KDB_REASON_BREAK;
				db_result = KDB_DB_BPT;
				KDB_STATE_SET(SSBPT);
				break;
			}
		}
	}

	if (reason != KDB_REASON_BREAK && ks->ex_vector == 0 &&
		ks->signo == SIGTRAP) {
		reason = KDB_REASON_SSTEP;
		db_result = KDB_DB_BPT;
	}
	
	KDB_STATE_CLEAR(KGDB_TRANS);
	kdb_initial_cpu = atomic_read(&kgdb_active);
	kdb_current_task = kgdb_info[ks->cpu].task;
	kdb_current_regs = kgdb_info[ks->cpu].debuggerinfo;
	
	kdb_bp_remove();
	KDB_STATE_CLEAR(DOING_SS);
	KDB_STATE_CLEAR(DOING_SSB);
	KDB_STATE_SET(PAGER);
	
	for_each_present_cpu(i) {
		if (!cpu_online(i)) {
			kgdb_info[i].debuggerinfo = NULL;
			kgdb_info[i].task = NULL;
		}
	}
	if (ks->err_code == DIE_OOPS || reason == KDB_REASON_OOPS) {
		ks->pass_exception = 1;
		KDB_FLAG_SET(CATASTROPHIC);
	}
	if (KDB_STATE(SSBPT) && reason == KDB_REASON_SSTEP) {
		KDB_STATE_CLEAR(SSBPT);
		KDB_STATE_CLEAR(DOING_SS);
	} else {
		
		error = kdb_main_loop(KDB_REASON_ENTER, reason,
				      ks->err_code, db_result, ks->linux_regs);
	}
	kdb_initial_cpu = -1;
	kdb_current_task = NULL;
	kdb_current_regs = NULL;
	KDB_STATE_CLEAR(PAGER);
	kdbnearsym_cleanup();
	if (error == KDB_CMD_KGDB) {
		if (KDB_STATE(DOING_KGDB))
			KDB_STATE_CLEAR(DOING_KGDB);
		return DBG_PASS_EVENT;
	}
	kdb_bp_install(ks->linux_regs);
	dbg_activate_sw_breakpoints();
	
	if (KDB_STATE(DOING_SS))
		gdbstub_state(ks, "s");
	else
		gdbstub_state(ks, "c");

	KDB_FLAG_CLEAR(CATASTROPHIC);

	
	kgdb_info[ks->cpu].ret_state = gdbstub_state(ks, "e");
	if (ks->pass_exception)
		kgdb_info[ks->cpu].ret_state = 1;
	if (error == KDB_CMD_CPU) {
		KDB_STATE_SET(REENTRY);
		kgdb_single_step = 0;
		dbg_deactivate_sw_breakpoints();
		return DBG_SWITCH_CPU_EVENT;
	}
	return kgdb_info[ks->cpu].ret_state;
}

void kdb_gdb_state_pass(char *buf)
{
	gdbstub_state(kdb_ks, buf);
}
