/*
 * This file contains the Itanium PMU register description tables
 * and pmc checker used by perfmon.c.
 *
 * Copyright (C) 2002-2003  Hewlett Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 */
static int pfm_ita_pmc_check(struct task_struct *task, pfm_context_t *ctx, unsigned int cnum, unsigned long *val, struct pt_regs *regs);

static pfm_reg_desc_t pfm_ita_pmc_desc[PMU_MAX_PMCS]={
 { PFM_REG_CONTROL , 0, 0x1UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 6, 0x0UL, -1UL, NULL, NULL, {RDEP(4),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 6, 0x0UL, -1UL, NULL, NULL, {RDEP(5),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 6, 0x0UL, -1UL, NULL, NULL, {RDEP(6),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 6, 0x0UL, -1UL, NULL, NULL, {RDEP(7),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONFIG  , 0, 0xf00000003ffffff8UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONFIG  , 0, 0xf00000003ffffff8UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_MONITOR , 6, 0x0UL, -1UL, NULL, NULL, {RDEP(0)|RDEP(1),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_MONITOR , 6, 0x0000000010000000UL, -1UL, NULL, pfm_ita_pmc_check, {RDEP(2)|RDEP(3)|RDEP(17),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_MONITOR , 6, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONFIG  , 0, 0x0003ffff00000001UL, -1UL, NULL, pfm_ita_pmc_check, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
	    { PFM_REG_END     , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}}, 
};

static pfm_reg_desc_t pfm_ita_pmd_desc[PMU_MAX_PMDS]={
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(1),0UL, 0UL, 0UL}, {RDEP(10),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(0),0UL, 0UL, 0UL}, {RDEP(10),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(3)|RDEP(17),0UL, 0UL, 0UL}, {RDEP(11),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(2)|RDEP(17),0UL, 0UL, 0UL}, {RDEP(11),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(4),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(5),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(6),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(7),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0UL, -1UL, NULL, NULL, {RDEP(2)|RDEP(3),0UL, 0UL, 0UL}, {RDEP(11),0UL, 0UL, 0UL}},
	    { PFM_REG_END     , 0, 0UL, -1UL, NULL, NULL, {0,}, {0,}}, 
};

static int
pfm_ita_pmc_check(struct task_struct *task, pfm_context_t *ctx, unsigned int cnum, unsigned long *val, struct pt_regs *regs)
{
	int ret;
	int is_loaded;

	
	if (ctx == NULL) return -EINVAL;

	is_loaded = ctx->ctx_state == PFM_CTX_LOADED || ctx->ctx_state == PFM_CTX_MASKED;

	/*
	 * we must clear the (instruction) debug registers if pmc13.ta bit is cleared
	 * before they are written (fl_using_dbreg==0) to avoid picking up stale information.
	 */
	if (cnum == 13 && is_loaded && ((*val & 0x1) == 0UL) && ctx->ctx_fl_using_dbreg == 0) {

		DPRINT(("pmc[%d]=0x%lx has active pmc13.ta cleared, clearing ibr\n", cnum, *val));

		
		if (task && (task->thread.flags & IA64_THREAD_DBG_VALID) != 0) return -EINVAL;

		ret = pfm_write_ibr_dbr(1, ctx, NULL, 0, regs);
		if (ret) return ret;
	}

	/*
	 * we must clear the (data) debug registers if pmc11.pt bit is cleared
	 * before they are written (fl_using_dbreg==0) to avoid picking up stale information.
	 */
	if (cnum == 11 && is_loaded && ((*val >> 28)& 0x1) == 0 && ctx->ctx_fl_using_dbreg == 0) {

		DPRINT(("pmc[%d]=0x%lx has active pmc11.pt cleared, clearing dbr\n", cnum, *val));

		
		if (task && (task->thread.flags & IA64_THREAD_DBG_VALID) != 0) return -EINVAL;

		ret = pfm_write_ibr_dbr(0, ctx, NULL, 0, regs);
		if (ret) return ret;
	}
	return 0;
}

static pmu_config_t pmu_conf_ita={
	.pmu_name      = "Itanium",
	.pmu_family    = 0x7,
	.ovfl_val      = (1UL << 32) - 1,
	.pmd_desc      = pfm_ita_pmd_desc,
	.pmc_desc      = pfm_ita_pmc_desc,
	.num_ibrs      = 8,
	.num_dbrs      = 8,
	.use_rr_dbregs = 1, 
};


