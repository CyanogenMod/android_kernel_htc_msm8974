/*
 * This file contains the McKinley PMU register description tables
 * and pmc checker used by perfmon.c.
 *
 * Copyright (C) 2002-2003  Hewlett Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 */
static int pfm_mck_pmc_check(struct task_struct *task, pfm_context_t *ctx, unsigned int cnum, unsigned long *val, struct pt_regs *regs);

static pfm_reg_desc_t pfm_mck_pmc_desc[PMU_MAX_PMCS]={
 { PFM_REG_CONTROL , 0, 0x1UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 6, 0x0000000000800000UL, 0xfffff7fUL, NULL, pfm_mck_pmc_check, {RDEP(4),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 6, 0x0UL, 0xfffff7fUL, NULL,  pfm_mck_pmc_check, {RDEP(5),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 6, 0x0UL, 0xfffff7fUL, NULL,  pfm_mck_pmc_check, {RDEP(6),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 6, 0x0UL, 0xfffff7fUL, NULL,  pfm_mck_pmc_check, {RDEP(7),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONFIG  , 0, 0xffffffff3fffffffUL, 0xffffffff3ffffffbUL, NULL, pfm_mck_pmc_check, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONFIG  , 0, 0xffffffff3ffffffcUL, 0xffffffff3ffffffbUL, NULL, pfm_mck_pmc_check, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_MONITOR , 4, 0x0UL, 0xffffUL, NULL, pfm_mck_pmc_check, {RDEP(0)|RDEP(1),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_MONITOR , 6, 0x0UL, 0x30f01cf, NULL,  pfm_mck_pmc_check, {RDEP(2)|RDEP(3)|RDEP(17),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_MONITOR , 6, 0x0UL, 0xffffUL, NULL,  pfm_mck_pmc_check, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONFIG  , 0, 0x00002078fefefefeUL, 0x1e00018181818UL, NULL, pfm_mck_pmc_check, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONFIG  , 0, 0x0db60db60db60db6UL, 0x2492UL, NULL, pfm_mck_pmc_check, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONFIG  , 0, 0x00000000fffffff0UL, 0xfUL, NULL, pfm_mck_pmc_check, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
	    { PFM_REG_END     , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}}, 
};

static pfm_reg_desc_t pfm_mck_pmd_desc[PMU_MAX_PMDS]={
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(1),0UL, 0UL, 0UL}, {RDEP(10),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(0),0UL, 0UL, 0UL}, {RDEP(10),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(3)|RDEP(17),0UL, 0UL, 0UL}, {RDEP(11),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(2)|RDEP(17),0UL, 0UL, 0UL}, {RDEP(11),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(4),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(5),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(6),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(7),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(13)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(14)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(15)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(16),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(8)|RDEP(9)|RDEP(10)|RDEP(11)|RDEP(12)|RDEP(13)|RDEP(14)|RDEP(15),0UL, 0UL, 0UL}, {RDEP(12),0UL, 0UL, 0UL}},
 { PFM_REG_BUFFER  , 0, 0x0UL, -1UL, NULL, NULL, {RDEP(2)|RDEP(3),0UL, 0UL, 0UL}, {RDEP(11),0UL, 0UL, 0UL}},
	    { PFM_REG_END     , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}}, 
};

static int
pfm_mck_reserved(unsigned int cnum, unsigned long *val, struct pt_regs *regs)
{
	unsigned long tmp1, tmp2, ival = *val;

	
	tmp1 = ival & PMC_RSVD_MASK(cnum);

	
	tmp2 = PMC_DFL_VAL(cnum) & ~PMC_RSVD_MASK(cnum);

	*val = tmp1 | tmp2;

	DPRINT(("pmc[%d]=0x%lx, mask=0x%lx, reset=0x%lx, val=0x%lx\n",
		  cnum, ival, PMC_RSVD_MASK(cnum), PMC_DFL_VAL(cnum), *val));
	return 0;
}

static int
pfm_mck_pmc_check(struct task_struct *task, pfm_context_t *ctx, unsigned int cnum, unsigned long *val, struct pt_regs *regs)
{
	int ret = 0, check_case1 = 0;
	unsigned long val8 = 0, val14 = 0, val13 = 0;
	int is_loaded;

	
	pfm_mck_reserved(cnum, val, regs);

	
	if (ctx == NULL) return -EINVAL;

	is_loaded = ctx->ctx_state == PFM_CTX_LOADED || ctx->ctx_state == PFM_CTX_MASKED;

	DPRINT(("cnum=%u val=0x%lx, using_dbreg=%d loaded=%d\n", cnum, *val, ctx->ctx_fl_using_dbreg, is_loaded));

	if (cnum == 13 && is_loaded
	    && (*val & 0x1e00000000000UL) && (*val & 0x18181818UL) != 0x18181818UL && ctx->ctx_fl_using_dbreg == 0) {

		DPRINT(("pmc[%d]=0x%lx has active pmc13 settings, clearing dbr\n", cnum, *val));

		
		if (task && (task->thread.flags & IA64_THREAD_DBG_VALID) != 0) return -EINVAL;

		ret = pfm_write_ibr_dbr(PFM_DATA_RR, ctx, NULL, 0, regs);
		if (ret) return ret;
	}
	if (cnum == 14 && is_loaded && ((*val & 0x2222UL) != 0x2222UL) && ctx->ctx_fl_using_dbreg == 0) {

		DPRINT(("pmc[%d]=0x%lx has active pmc14 settings, clearing ibr\n", cnum, *val));

		
		if (task && (task->thread.flags & IA64_THREAD_DBG_VALID) != 0) return -EINVAL;

		ret = pfm_write_ibr_dbr(PFM_CODE_RR, ctx, NULL, 0, regs);
		if (ret) return ret;

	}

	switch(cnum) {
		case  4: *val |= 1UL << 23; 
			 break;
		case  8: val8 = *val;
			 val13 = ctx->ctx_pmcs[13];
			 val14 = ctx->ctx_pmcs[14];
			 check_case1 = 1;
			 break;
		case 13: val8  = ctx->ctx_pmcs[8];
			 val13 = *val;
			 val14 = ctx->ctx_pmcs[14];
			 check_case1 = 1;
			 break;
		case 14: val8  = ctx->ctx_pmcs[8];
			 val13 = ctx->ctx_pmcs[13];
			 val14 = *val;
			 check_case1 = 1;
			 break;
	}
	if (check_case1) {
		ret =   ((val13 >> 45) & 0xf) == 0
		   && ((val8 & 0x1) == 0)
		   && ((((val14>>1) & 0x3) == 0x2 || ((val14>>1) & 0x3) == 0x0)
		       ||(((val14>>4) & 0x3) == 0x2 || ((val14>>4) & 0x3) == 0x0));

		if (ret) DPRINT((KERN_DEBUG "perfmon: failure check_case1\n"));
	}

	return ret ? -EINVAL : 0;
}

static pmu_config_t pmu_conf_mck={
	.pmu_name      = "Itanium 2",
	.pmu_family    = 0x1f,
	.flags	       = PFM_PMU_IRQ_RESEND,
	.ovfl_val      = (1UL << 47) - 1,
	.pmd_desc      = pfm_mck_pmd_desc,
	.pmc_desc      = pfm_mck_pmc_desc,
	.num_ibrs       = 8,
	.num_dbrs       = 8,
	.use_rr_dbregs = 1 
};


