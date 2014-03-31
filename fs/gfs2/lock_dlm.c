/*
 * Copyright (C) Sistina Software, Inc.  1997-2003 All rights reserved.
 * Copyright 2004-2011 Red Hat, Inc.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License version 2.
 */

#include <linux/fs.h>
#include <linux/dlm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/gfs2_ondisk.h>

#include "incore.h"
#include "glock.h"
#include "util.h"
#include "sys.h"
#include "trace_gfs2.h"

extern struct workqueue_struct *gfs2_control_wq;


static inline void gfs2_update_stats(struct gfs2_lkstats *s, unsigned index,
				     s64 sample)
{
	s64 delta = sample - s->stats[index];
	s->stats[index] += (delta >> 3);
	index++;
	s->stats[index] += ((abs64(delta) - s->stats[index]) >> 2);
}

static inline void gfs2_update_reply_times(struct gfs2_glock *gl)
{
	struct gfs2_pcpu_lkstats *lks;
	const unsigned gltype = gl->gl_name.ln_type;
	unsigned index = test_bit(GLF_BLOCKING, &gl->gl_flags) ?
			 GFS2_LKS_SRTTB : GFS2_LKS_SRTT;
	s64 rtt;

	preempt_disable();
	rtt = ktime_to_ns(ktime_sub(ktime_get_real(), gl->gl_dstamp));
	lks = this_cpu_ptr(gl->gl_sbd->sd_lkstats);
	gfs2_update_stats(&gl->gl_stats, index, rtt);		
	gfs2_update_stats(&lks->lkstats[gltype], index, rtt);	
	preempt_enable();

	trace_gfs2_glock_lock_time(gl, rtt);
}


static inline void gfs2_update_request_times(struct gfs2_glock *gl)
{
	struct gfs2_pcpu_lkstats *lks;
	const unsigned gltype = gl->gl_name.ln_type;
	ktime_t dstamp;
	s64 irt;

	preempt_disable();
	dstamp = gl->gl_dstamp;
	gl->gl_dstamp = ktime_get_real();
	irt = ktime_to_ns(ktime_sub(gl->gl_dstamp, dstamp));
	lks = this_cpu_ptr(gl->gl_sbd->sd_lkstats);
	gfs2_update_stats(&gl->gl_stats, GFS2_LKS_SIRT, irt);		
	gfs2_update_stats(&lks->lkstats[gltype], GFS2_LKS_SIRT, irt);	
	preempt_enable();
}
 
static void gdlm_ast(void *arg)
{
	struct gfs2_glock *gl = arg;
	unsigned ret = gl->gl_state;

	gfs2_update_reply_times(gl);
	BUG_ON(gl->gl_lksb.sb_flags & DLM_SBF_DEMOTED);

	if (gl->gl_lksb.sb_flags & DLM_SBF_VALNOTVALID)
		memset(gl->gl_lvb, 0, GDLM_LVB_SIZE);

	switch (gl->gl_lksb.sb_status) {
	case -DLM_EUNLOCK: 
		gfs2_glock_free(gl);
		return;
	case -DLM_ECANCEL: 
		ret |= LM_OUT_CANCELED;
		goto out;
	case -EAGAIN: 
	case -EDEADLK: 
		goto out;
	case -ETIMEDOUT: 
		ret |= LM_OUT_ERROR;
		goto out;
	case 0: 
		break;
	default: 
		BUG();
	}

	ret = gl->gl_req;
	if (gl->gl_lksb.sb_flags & DLM_SBF_ALTMODE) {
		if (gl->gl_req == LM_ST_SHARED)
			ret = LM_ST_DEFERRED;
		else if (gl->gl_req == LM_ST_DEFERRED)
			ret = LM_ST_SHARED;
		else
			BUG();
	}

	set_bit(GLF_INITIAL, &gl->gl_flags);
	gfs2_glock_complete(gl, ret);
	return;
out:
	if (!test_bit(GLF_INITIAL, &gl->gl_flags))
		gl->gl_lksb.sb_lkid = 0;
	gfs2_glock_complete(gl, ret);
}

static void gdlm_bast(void *arg, int mode)
{
	struct gfs2_glock *gl = arg;

	switch (mode) {
	case DLM_LOCK_EX:
		gfs2_glock_cb(gl, LM_ST_UNLOCKED);
		break;
	case DLM_LOCK_CW:
		gfs2_glock_cb(gl, LM_ST_DEFERRED);
		break;
	case DLM_LOCK_PR:
		gfs2_glock_cb(gl, LM_ST_SHARED);
		break;
	default:
		printk(KERN_ERR "unknown bast mode %d", mode);
		BUG();
	}
}


static int make_mode(const unsigned int lmstate)
{
	switch (lmstate) {
	case LM_ST_UNLOCKED:
		return DLM_LOCK_NL;
	case LM_ST_EXCLUSIVE:
		return DLM_LOCK_EX;
	case LM_ST_DEFERRED:
		return DLM_LOCK_CW;
	case LM_ST_SHARED:
		return DLM_LOCK_PR;
	}
	printk(KERN_ERR "unknown LM state %d", lmstate);
	BUG();
	return -1;
}

static u32 make_flags(struct gfs2_glock *gl, const unsigned int gfs_flags,
		      const int req)
{
	u32 lkf = DLM_LKF_VALBLK;
	u32 lkid = gl->gl_lksb.sb_lkid;

	if (gfs_flags & LM_FLAG_TRY)
		lkf |= DLM_LKF_NOQUEUE;

	if (gfs_flags & LM_FLAG_TRY_1CB) {
		lkf |= DLM_LKF_NOQUEUE;
		lkf |= DLM_LKF_NOQUEUEBAST;
	}

	if (gfs_flags & LM_FLAG_PRIORITY) {
		lkf |= DLM_LKF_NOORDER;
		lkf |= DLM_LKF_HEADQUE;
	}

	if (gfs_flags & LM_FLAG_ANY) {
		if (req == DLM_LOCK_PR)
			lkf |= DLM_LKF_ALTCW;
		else if (req == DLM_LOCK_CW)
			lkf |= DLM_LKF_ALTPR;
		else
			BUG();
	}

	if (lkid != 0) {
		lkf |= DLM_LKF_CONVERT;
		if (test_bit(GLF_BLOCKING, &gl->gl_flags))
			lkf |= DLM_LKF_QUECVT;
	}

	return lkf;
}

static void gfs2_reverse_hex(char *c, u64 value)
{
	while (value) {
		*c-- = hex_asc[value & 0x0f];
		value >>= 4;
	}
}

static int gdlm_lock(struct gfs2_glock *gl, unsigned int req_state,
		     unsigned int flags)
{
	struct lm_lockstruct *ls = &gl->gl_sbd->sd_lockstruct;
	int req;
	u32 lkf;
	char strname[GDLM_STRNAME_BYTES] = "";

	req = make_mode(req_state);
	lkf = make_flags(gl, flags, req);
	gfs2_glstats_inc(gl, GFS2_LKS_DCOUNT);
	gfs2_sbstats_inc(gl, GFS2_LKS_DCOUNT);
	if (gl->gl_lksb.sb_lkid) {
		gfs2_update_request_times(gl);
	} else {
		memset(strname, ' ', GDLM_STRNAME_BYTES - 1);
		strname[GDLM_STRNAME_BYTES - 1] = '\0';
		gfs2_reverse_hex(strname + 7, gl->gl_name.ln_type);
		gfs2_reverse_hex(strname + 23, gl->gl_name.ln_number);
		gl->gl_dstamp = ktime_get_real();
	}

	return dlm_lock(ls->ls_dlm, req, &gl->gl_lksb, lkf, strname,
			GDLM_STRNAME_BYTES - 1, 0, gdlm_ast, gl, gdlm_bast);
}

static void gdlm_put_lock(struct gfs2_glock *gl)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	int error;

	if (gl->gl_lksb.sb_lkid == 0) {
		gfs2_glock_free(gl);
		return;
	}

	clear_bit(GLF_BLOCKING, &gl->gl_flags);
	gfs2_glstats_inc(gl, GFS2_LKS_DCOUNT);
	gfs2_sbstats_inc(gl, GFS2_LKS_DCOUNT);
	gfs2_update_request_times(gl);
	error = dlm_unlock(ls->ls_dlm, gl->gl_lksb.sb_lkid, DLM_LKF_VALBLK,
			   NULL, gl);
	if (error) {
		printk(KERN_ERR "gdlm_unlock %x,%llx err=%d\n",
		       gl->gl_name.ln_type,
		       (unsigned long long)gl->gl_name.ln_number, error);
		return;
	}
}

static void gdlm_cancel(struct gfs2_glock *gl)
{
	struct lm_lockstruct *ls = &gl->gl_sbd->sd_lockstruct;
	dlm_unlock(ls->ls_dlm, gl->gl_lksb.sb_lkid, DLM_LKF_CANCEL, NULL, gl);
}


#define JID_BITMAP_OFFSET 8 

static void control_lvb_read(struct lm_lockstruct *ls, uint32_t *lvb_gen,
			     char *lvb_bits)
{
	uint32_t gen;
	memcpy(lvb_bits, ls->ls_control_lvb, GDLM_LVB_SIZE);
	memcpy(&gen, lvb_bits, sizeof(uint32_t));
	*lvb_gen = le32_to_cpu(gen);
}

static void control_lvb_write(struct lm_lockstruct *ls, uint32_t lvb_gen,
			      char *lvb_bits)
{
	uint32_t gen;
	memcpy(ls->ls_control_lvb, lvb_bits, GDLM_LVB_SIZE);
	gen = cpu_to_le32(lvb_gen);
	memcpy(ls->ls_control_lvb, &gen, sizeof(uint32_t));
}

static int all_jid_bits_clear(char *lvb)
{
	int i;
	for (i = JID_BITMAP_OFFSET; i < GDLM_LVB_SIZE; i++) {
		if (lvb[i])
			return 0;
	}
	return 1;
}

static void sync_wait_cb(void *arg)
{
	struct lm_lockstruct *ls = arg;
	complete(&ls->ls_sync_wait);
}

static int sync_unlock(struct gfs2_sbd *sdp, struct dlm_lksb *lksb, char *name)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	int error;

	error = dlm_unlock(ls->ls_dlm, lksb->sb_lkid, 0, lksb, ls);
	if (error) {
		fs_err(sdp, "%s lkid %x error %d\n",
		       name, lksb->sb_lkid, error);
		return error;
	}

	wait_for_completion(&ls->ls_sync_wait);

	if (lksb->sb_status != -DLM_EUNLOCK) {
		fs_err(sdp, "%s lkid %x status %d\n",
		       name, lksb->sb_lkid, lksb->sb_status);
		return -1;
	}
	return 0;
}

static int sync_lock(struct gfs2_sbd *sdp, int mode, uint32_t flags,
		     unsigned int num, struct dlm_lksb *lksb, char *name)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	char strname[GDLM_STRNAME_BYTES];
	int error, status;

	memset(strname, 0, GDLM_STRNAME_BYTES);
	snprintf(strname, GDLM_STRNAME_BYTES, "%8x%16x", LM_TYPE_NONDISK, num);

	error = dlm_lock(ls->ls_dlm, mode, lksb, flags,
			 strname, GDLM_STRNAME_BYTES - 1,
			 0, sync_wait_cb, ls, NULL);
	if (error) {
		fs_err(sdp, "%s lkid %x flags %x mode %d error %d\n",
		       name, lksb->sb_lkid, flags, mode, error);
		return error;
	}

	wait_for_completion(&ls->ls_sync_wait);

	status = lksb->sb_status;

	if (status && status != -EAGAIN) {
		fs_err(sdp, "%s lkid %x flags %x mode %d status %d\n",
		       name, lksb->sb_lkid, flags, mode, status);
	}

	return status;
}

static int mounted_unlock(struct gfs2_sbd *sdp)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	return sync_unlock(sdp, &ls->ls_mounted_lksb, "mounted_lock");
}

static int mounted_lock(struct gfs2_sbd *sdp, int mode, uint32_t flags)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	return sync_lock(sdp, mode, flags, GFS2_MOUNTED_LOCK,
			 &ls->ls_mounted_lksb, "mounted_lock");
}

static int control_unlock(struct gfs2_sbd *sdp)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	return sync_unlock(sdp, &ls->ls_control_lksb, "control_lock");
}

static int control_lock(struct gfs2_sbd *sdp, int mode, uint32_t flags)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	return sync_lock(sdp, mode, flags, GFS2_CONTROL_LOCK,
			 &ls->ls_control_lksb, "control_lock");
}

static void gfs2_control_func(struct work_struct *work)
{
	struct gfs2_sbd *sdp = container_of(work, struct gfs2_sbd, sd_control_work.work);
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	char lvb_bits[GDLM_LVB_SIZE];
	uint32_t block_gen, start_gen, lvb_gen, flags;
	int recover_set = 0;
	int write_lvb = 0;
	int recover_size;
	int i, error;

	spin_lock(&ls->ls_recover_spin);
	if (!test_bit(DFL_MOUNT_DONE, &ls->ls_recover_flags) ||
	     test_bit(DFL_FIRST_MOUNT, &ls->ls_recover_flags)) {
		spin_unlock(&ls->ls_recover_spin);
		return;
	}
	block_gen = ls->ls_recover_block;
	start_gen = ls->ls_recover_start;
	spin_unlock(&ls->ls_recover_spin);


	if (block_gen == start_gen)
		return;


	error = control_lock(sdp, DLM_LOCK_EX, DLM_LKF_CONVERT|DLM_LKF_VALBLK);
	if (error) {
		fs_err(sdp, "control lock EX error %d\n", error);
		return;
	}

	control_lvb_read(ls, &lvb_gen, lvb_bits);

	spin_lock(&ls->ls_recover_spin);
	if (block_gen != ls->ls_recover_block ||
	    start_gen != ls->ls_recover_start) {
		fs_info(sdp, "recover generation %u block1 %u %u\n",
			start_gen, block_gen, ls->ls_recover_block);
		spin_unlock(&ls->ls_recover_spin);
		control_lock(sdp, DLM_LOCK_NL, DLM_LKF_CONVERT);
		return;
	}

	recover_size = ls->ls_recover_size;

	if (lvb_gen <= start_gen) {
		for (i = 0; i < recover_size; i++) {
			if (ls->ls_recover_result[i] != LM_RD_SUCCESS)
				continue;

			ls->ls_recover_result[i] = 0;

			if (!test_bit_le(i, lvb_bits + JID_BITMAP_OFFSET))
				continue;

			__clear_bit_le(i, lvb_bits + JID_BITMAP_OFFSET);
			write_lvb = 1;
		}
	}

	if (lvb_gen == start_gen) {
		for (i = 0; i < recover_size; i++) {
			if (!ls->ls_recover_submit[i])
				continue;
			if (ls->ls_recover_submit[i] < lvb_gen)
				ls->ls_recover_submit[i] = 0;
		}
	} else if (lvb_gen < start_gen) {
		for (i = 0; i < recover_size; i++) {
			if (!ls->ls_recover_submit[i])
				continue;
			if (ls->ls_recover_submit[i] < start_gen) {
				ls->ls_recover_submit[i] = 0;
				__set_bit_le(i, lvb_bits + JID_BITMAP_OFFSET);
			}
		}
		write_lvb = 1;
	} else {
	}
	spin_unlock(&ls->ls_recover_spin);

	if (write_lvb) {
		control_lvb_write(ls, start_gen, lvb_bits);
		flags = DLM_LKF_CONVERT | DLM_LKF_VALBLK;
	} else {
		flags = DLM_LKF_CONVERT;
	}

	error = control_lock(sdp, DLM_LOCK_NL, flags);
	if (error) {
		fs_err(sdp, "control lock NL error %d\n", error);
		return;
	}


	for (i = 0; i < recover_size; i++) {
		if (test_bit_le(i, lvb_bits + JID_BITMAP_OFFSET)) {
			fs_info(sdp, "recover generation %u jid %d\n",
				start_gen, i);
			gfs2_recover_set(sdp, i);
			recover_set++;
		}
	}
	if (recover_set)
		return;


	spin_lock(&ls->ls_recover_spin);
	if (ls->ls_recover_block == block_gen &&
	    ls->ls_recover_start == start_gen) {
		clear_bit(DFL_BLOCK_LOCKS, &ls->ls_recover_flags);
		spin_unlock(&ls->ls_recover_spin);
		fs_info(sdp, "recover generation %u done\n", start_gen);
		gfs2_glock_thaw(sdp);
	} else {
		fs_info(sdp, "recover generation %u block2 %u %u\n",
			start_gen, block_gen, ls->ls_recover_block);
		spin_unlock(&ls->ls_recover_spin);
	}
}

static int control_mount(struct gfs2_sbd *sdp)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	char lvb_bits[GDLM_LVB_SIZE];
	uint32_t start_gen, block_gen, mount_gen, lvb_gen;
	int mounted_mode;
	int retries = 0;
	int error;

	memset(&ls->ls_mounted_lksb, 0, sizeof(struct dlm_lksb));
	memset(&ls->ls_control_lksb, 0, sizeof(struct dlm_lksb));
	memset(&ls->ls_control_lvb, 0, GDLM_LVB_SIZE);
	ls->ls_control_lksb.sb_lvbptr = ls->ls_control_lvb;
	init_completion(&ls->ls_sync_wait);

	set_bit(DFL_BLOCK_LOCKS, &ls->ls_recover_flags);

	error = control_lock(sdp, DLM_LOCK_NL, DLM_LKF_VALBLK);
	if (error) {
		fs_err(sdp, "control_mount control_lock NL error %d\n", error);
		return error;
	}

	error = mounted_lock(sdp, DLM_LOCK_NL, 0);
	if (error) {
		fs_err(sdp, "control_mount mounted_lock NL error %d\n", error);
		control_unlock(sdp);
		return error;
	}
	mounted_mode = DLM_LOCK_NL;

restart:
	if (retries++ && signal_pending(current)) {
		error = -EINTR;
		goto fail;
	}


	if (mounted_mode != DLM_LOCK_NL) {
		error = mounted_lock(sdp, DLM_LOCK_NL, DLM_LKF_CONVERT);
		if (error)
			goto fail;
		mounted_mode = DLM_LOCK_NL;
	}


	msleep_interruptible(500);


	error = control_lock(sdp, DLM_LOCK_EX, DLM_LKF_CONVERT|DLM_LKF_NOQUEUE|DLM_LKF_VALBLK);
	if (error == -EAGAIN) {
		goto restart;
	} else if (error) {
		fs_err(sdp, "control_mount control_lock EX error %d\n", error);
		goto fail;
	}

	error = mounted_lock(sdp, DLM_LOCK_EX, DLM_LKF_CONVERT|DLM_LKF_NOQUEUE);
	if (!error) {
		mounted_mode = DLM_LOCK_EX;
		goto locks_done;
	} else if (error != -EAGAIN) {
		fs_err(sdp, "control_mount mounted_lock EX error %d\n", error);
		goto fail;
	}

	error = mounted_lock(sdp, DLM_LOCK_PR, DLM_LKF_CONVERT|DLM_LKF_NOQUEUE);
	if (!error) {
		mounted_mode = DLM_LOCK_PR;
		goto locks_done;
	} else {
		
		fs_err(sdp, "control_mount mounted_lock PR error %d\n", error);
		goto fail;
	}

locks_done:

	control_lvb_read(ls, &lvb_gen, lvb_bits);

	if (lvb_gen == 0xFFFFFFFF) {
		
		fs_err(sdp, "control_mount control_lock disabled\n");
		error = -EINVAL;
		goto fail;
	}

	if (mounted_mode == DLM_LOCK_EX) {
		
		spin_lock(&ls->ls_recover_spin);
		clear_bit(DFL_BLOCK_LOCKS, &ls->ls_recover_flags);
		set_bit(DFL_MOUNT_DONE, &ls->ls_recover_flags);
		set_bit(DFL_FIRST_MOUNT, &ls->ls_recover_flags);
		spin_unlock(&ls->ls_recover_spin);
		fs_info(sdp, "first mounter control generation %u\n", lvb_gen);
		return 0;
	}

	error = control_lock(sdp, DLM_LOCK_NL, DLM_LKF_CONVERT);
	if (error)
		goto fail;


	if (!all_jid_bits_clear(lvb_bits)) {
		
		fs_info(sdp, "control_mount wait for journal recovery\n");
		goto restart;
	}

	spin_lock(&ls->ls_recover_spin);
	block_gen = ls->ls_recover_block;
	start_gen = ls->ls_recover_start;
	mount_gen = ls->ls_recover_mount;

	if (lvb_gen < mount_gen) {
		fs_info(sdp, "control_mount wait1 block %u start %u mount %u "
			"lvb %u flags %lx\n", block_gen, start_gen, mount_gen,
			lvb_gen, ls->ls_recover_flags);
		spin_unlock(&ls->ls_recover_spin);
		goto restart;
	}

	if (lvb_gen != start_gen) {
		fs_info(sdp, "control_mount wait2 block %u start %u mount %u "
			"lvb %u flags %lx\n", block_gen, start_gen, mount_gen,
			lvb_gen, ls->ls_recover_flags);
		spin_unlock(&ls->ls_recover_spin);
		goto restart;
	}

	if (block_gen == start_gen) {
		
		fs_info(sdp, "control_mount wait3 block %u start %u mount %u "
			"lvb %u flags %lx\n", block_gen, start_gen, mount_gen,
			lvb_gen, ls->ls_recover_flags);
		spin_unlock(&ls->ls_recover_spin);
		goto restart;
	}

	clear_bit(DFL_BLOCK_LOCKS, &ls->ls_recover_flags);
	set_bit(DFL_MOUNT_DONE, &ls->ls_recover_flags);
	memset(ls->ls_recover_submit, 0, ls->ls_recover_size*sizeof(uint32_t));
	memset(ls->ls_recover_result, 0, ls->ls_recover_size*sizeof(uint32_t));
	spin_unlock(&ls->ls_recover_spin);
	return 0;

fail:
	mounted_unlock(sdp);
	control_unlock(sdp);
	return error;
}

static int dlm_recovery_wait(void *word)
{
	schedule();
	return 0;
}

static int control_first_done(struct gfs2_sbd *sdp)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	char lvb_bits[GDLM_LVB_SIZE];
	uint32_t start_gen, block_gen;
	int error;

restart:
	spin_lock(&ls->ls_recover_spin);
	start_gen = ls->ls_recover_start;
	block_gen = ls->ls_recover_block;

	if (test_bit(DFL_BLOCK_LOCKS, &ls->ls_recover_flags) ||
	    !test_bit(DFL_MOUNT_DONE, &ls->ls_recover_flags) ||
	    !test_bit(DFL_FIRST_MOUNT, &ls->ls_recover_flags)) {
		
		fs_err(sdp, "control_first_done start %u block %u flags %lx\n",
		       start_gen, block_gen, ls->ls_recover_flags);
		spin_unlock(&ls->ls_recover_spin);
		control_unlock(sdp);
		return -1;
	}

	if (start_gen == block_gen) {
		spin_unlock(&ls->ls_recover_spin);
		fs_info(sdp, "control_first_done wait gen %u\n", start_gen);

		wait_on_bit(&ls->ls_recover_flags, DFL_DLM_RECOVERY,
			    dlm_recovery_wait, TASK_UNINTERRUPTIBLE);
		goto restart;
	}

	clear_bit(DFL_FIRST_MOUNT, &ls->ls_recover_flags);
	set_bit(DFL_FIRST_MOUNT_DONE, &ls->ls_recover_flags);
	memset(ls->ls_recover_submit, 0, ls->ls_recover_size*sizeof(uint32_t));
	memset(ls->ls_recover_result, 0, ls->ls_recover_size*sizeof(uint32_t));
	spin_unlock(&ls->ls_recover_spin);

	memset(lvb_bits, 0, sizeof(lvb_bits));
	control_lvb_write(ls, start_gen, lvb_bits);

	error = mounted_lock(sdp, DLM_LOCK_PR, DLM_LKF_CONVERT);
	if (error)
		fs_err(sdp, "control_first_done mounted PR error %d\n", error);

	error = control_lock(sdp, DLM_LOCK_NL, DLM_LKF_CONVERT|DLM_LKF_VALBLK);
	if (error)
		fs_err(sdp, "control_first_done control NL error %d\n", error);

	return error;
}


#define RECOVER_SIZE_INC 16

static int set_recover_size(struct gfs2_sbd *sdp, struct dlm_slot *slots,
			    int num_slots)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	uint32_t *submit = NULL;
	uint32_t *result = NULL;
	uint32_t old_size, new_size;
	int i, max_jid;

	max_jid = 0;
	for (i = 0; i < num_slots; i++) {
		if (max_jid < slots[i].slot - 1)
			max_jid = slots[i].slot - 1;
	}

	old_size = ls->ls_recover_size;

	if (old_size >= max_jid + 1)
		return 0;

	new_size = old_size + RECOVER_SIZE_INC;

	submit = kzalloc(new_size * sizeof(uint32_t), GFP_NOFS);
	result = kzalloc(new_size * sizeof(uint32_t), GFP_NOFS);
	if (!submit || !result) {
		kfree(submit);
		kfree(result);
		return -ENOMEM;
	}

	spin_lock(&ls->ls_recover_spin);
	memcpy(submit, ls->ls_recover_submit, old_size * sizeof(uint32_t));
	memcpy(result, ls->ls_recover_result, old_size * sizeof(uint32_t));
	kfree(ls->ls_recover_submit);
	kfree(ls->ls_recover_result);
	ls->ls_recover_submit = submit;
	ls->ls_recover_result = result;
	ls->ls_recover_size = new_size;
	spin_unlock(&ls->ls_recover_spin);
	return 0;
}

static void free_recover_size(struct lm_lockstruct *ls)
{
	kfree(ls->ls_recover_submit);
	kfree(ls->ls_recover_result);
	ls->ls_recover_submit = NULL;
	ls->ls_recover_result = NULL;
	ls->ls_recover_size = 0;
}


static void gdlm_recover_prep(void *arg)
{
	struct gfs2_sbd *sdp = arg;
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;

	spin_lock(&ls->ls_recover_spin);
	ls->ls_recover_block = ls->ls_recover_start;
	set_bit(DFL_DLM_RECOVERY, &ls->ls_recover_flags);

	if (!test_bit(DFL_MOUNT_DONE, &ls->ls_recover_flags) ||
	     test_bit(DFL_FIRST_MOUNT, &ls->ls_recover_flags)) {
		spin_unlock(&ls->ls_recover_spin);
		return;
	}
	set_bit(DFL_BLOCK_LOCKS, &ls->ls_recover_flags);
	spin_unlock(&ls->ls_recover_spin);
}


static void gdlm_recover_slot(void *arg, struct dlm_slot *slot)
{
	struct gfs2_sbd *sdp = arg;
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	int jid = slot->slot - 1;

	spin_lock(&ls->ls_recover_spin);
	if (ls->ls_recover_size < jid + 1) {
		fs_err(sdp, "recover_slot jid %d gen %u short size %d",
		       jid, ls->ls_recover_block, ls->ls_recover_size);
		spin_unlock(&ls->ls_recover_spin);
		return;
	}

	if (ls->ls_recover_submit[jid]) {
		fs_info(sdp, "recover_slot jid %d gen %u prev %u",
			jid, ls->ls_recover_block, ls->ls_recover_submit[jid]);
	}
	ls->ls_recover_submit[jid] = ls->ls_recover_block;
	spin_unlock(&ls->ls_recover_spin);
}


static void gdlm_recover_done(void *arg, struct dlm_slot *slots, int num_slots,
			      int our_slot, uint32_t generation)
{
	struct gfs2_sbd *sdp = arg;
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;

	
	set_recover_size(sdp, slots, num_slots);

	spin_lock(&ls->ls_recover_spin);
	ls->ls_recover_start = generation;

	if (!ls->ls_recover_mount) {
		ls->ls_recover_mount = generation;
		ls->ls_jid = our_slot - 1;
	}

	if (!test_bit(DFL_UNMOUNT, &ls->ls_recover_flags))
		queue_delayed_work(gfs2_control_wq, &sdp->sd_control_work, 0);

	clear_bit(DFL_DLM_RECOVERY, &ls->ls_recover_flags);
	smp_mb__after_clear_bit();
	wake_up_bit(&ls->ls_recover_flags, DFL_DLM_RECOVERY);
	spin_unlock(&ls->ls_recover_spin);
}


static void gdlm_recovery_result(struct gfs2_sbd *sdp, unsigned int jid,
				 unsigned int result)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;

	if (test_bit(DFL_NO_DLM_OPS, &ls->ls_recover_flags))
		return;

	
	if (jid == ls->ls_jid)
		return;

	spin_lock(&ls->ls_recover_spin);
	if (test_bit(DFL_FIRST_MOUNT, &ls->ls_recover_flags)) {
		spin_unlock(&ls->ls_recover_spin);
		return;
	}
	if (ls->ls_recover_size < jid + 1) {
		fs_err(sdp, "recovery_result jid %d short size %d",
		       jid, ls->ls_recover_size);
		spin_unlock(&ls->ls_recover_spin);
		return;
	}

	fs_info(sdp, "recover jid %d result %s\n", jid,
		result == LM_RD_GAVEUP ? "busy" : "success");

	ls->ls_recover_result[jid] = result;


	if (!test_bit(DFL_UNMOUNT, &ls->ls_recover_flags))
		queue_delayed_work(gfs2_control_wq, &sdp->sd_control_work,
				   result == LM_RD_GAVEUP ? HZ : 0);
	spin_unlock(&ls->ls_recover_spin);
}

const struct dlm_lockspace_ops gdlm_lockspace_ops = {
	.recover_prep = gdlm_recover_prep,
	.recover_slot = gdlm_recover_slot,
	.recover_done = gdlm_recover_done,
};

static int gdlm_mount(struct gfs2_sbd *sdp, const char *table)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	char cluster[GFS2_LOCKNAME_LEN];
	const char *fsname;
	uint32_t flags;
	int error, ops_result;


	INIT_DELAYED_WORK(&sdp->sd_control_work, gfs2_control_func);
	spin_lock_init(&ls->ls_recover_spin);
	ls->ls_recover_flags = 0;
	ls->ls_recover_mount = 0;
	ls->ls_recover_start = 0;
	ls->ls_recover_block = 0;
	ls->ls_recover_size = 0;
	ls->ls_recover_submit = NULL;
	ls->ls_recover_result = NULL;

	error = set_recover_size(sdp, NULL, 0);
	if (error)
		goto fail;


	fsname = strchr(table, ':');
	if (!fsname) {
		fs_info(sdp, "no fsname found\n");
		error = -EINVAL;
		goto fail_free;
	}
	memset(cluster, 0, sizeof(cluster));
	memcpy(cluster, table, strlen(table) - strlen(fsname));
	fsname++;

	flags = DLM_LSFL_FS | DLM_LSFL_NEWEXCL;
	if (ls->ls_nodir)
		flags |= DLM_LSFL_NODIR;


	error = dlm_new_lockspace(fsname, cluster, flags, GDLM_LVB_SIZE,
				  &gdlm_lockspace_ops, sdp, &ops_result,
				  &ls->ls_dlm);
	if (error) {
		fs_err(sdp, "dlm_new_lockspace error %d\n", error);
		goto fail_free;
	}

	if (ops_result < 0) {
		fs_info(sdp, "dlm lockspace ops not used\n");
		free_recover_size(ls);
		set_bit(DFL_NO_DLM_OPS, &ls->ls_recover_flags);
		return 0;
	}

	if (!test_bit(SDF_NOJOURNALID, &sdp->sd_flags)) {
		fs_err(sdp, "dlm lockspace ops disallow jid preset\n");
		error = -EINVAL;
		goto fail_release;
	}


	error = control_mount(sdp);
	if (error) {
		fs_err(sdp, "mount control error %d\n", error);
		goto fail_release;
	}

	ls->ls_first = !!test_bit(DFL_FIRST_MOUNT, &ls->ls_recover_flags);
	clear_bit(SDF_NOJOURNALID, &sdp->sd_flags);
	smp_mb__after_clear_bit();
	wake_up_bit(&sdp->sd_flags, SDF_NOJOURNALID);
	return 0;

fail_release:
	dlm_release_lockspace(ls->ls_dlm, 2);
fail_free:
	free_recover_size(ls);
fail:
	return error;
}

static void gdlm_first_done(struct gfs2_sbd *sdp)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;
	int error;

	if (test_bit(DFL_NO_DLM_OPS, &ls->ls_recover_flags))
		return;

	error = control_first_done(sdp);
	if (error)
		fs_err(sdp, "mount first_done error %d\n", error);
}

static void gdlm_unmount(struct gfs2_sbd *sdp)
{
	struct lm_lockstruct *ls = &sdp->sd_lockstruct;

	if (test_bit(DFL_NO_DLM_OPS, &ls->ls_recover_flags))
		goto release;

	

	spin_lock(&ls->ls_recover_spin);
	set_bit(DFL_UNMOUNT, &ls->ls_recover_flags);
	spin_unlock(&ls->ls_recover_spin);
	flush_delayed_work_sync(&sdp->sd_control_work);

	
release:
	if (ls->ls_dlm) {
		dlm_release_lockspace(ls->ls_dlm, 2);
		ls->ls_dlm = NULL;
	}

	free_recover_size(ls);
}

static const match_table_t dlm_tokens = {
	{ Opt_jid, "jid=%d"},
	{ Opt_id, "id=%d"},
	{ Opt_first, "first=%d"},
	{ Opt_nodir, "nodir=%d"},
	{ Opt_err, NULL },
};

const struct lm_lockops gfs2_dlm_ops = {
	.lm_proto_name = "lock_dlm",
	.lm_mount = gdlm_mount,
	.lm_first_done = gdlm_first_done,
	.lm_recovery_result = gdlm_recovery_result,
	.lm_unmount = gdlm_unmount,
	.lm_put_lock = gdlm_put_lock,
	.lm_lock = gdlm_lock,
	.lm_cancel = gdlm_cancel,
	.lm_tokens = &dlm_tokens,
};

