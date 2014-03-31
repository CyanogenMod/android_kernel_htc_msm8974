/*
 * This file contains the generic PMU register description tables
 * and pmc checker used by perfmon.c.
 *
 * Copyright (C) 2002-2003  Hewlett Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 */

static pfm_reg_desc_t pfm_gen_pmc_desc[PMU_MAX_PMCS]={
 { PFM_REG_CONTROL , 0, 0x1UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_CONTROL , 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {RDEP(4),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {RDEP(5),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {RDEP(6),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {RDEP(7),0UL, 0UL, 0UL}, {0UL,0UL, 0UL, 0UL}},
	    { PFM_REG_END     , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}}, 
};

static pfm_reg_desc_t pfm_gen_pmd_desc[PMU_MAX_PMDS]={
 { PFM_REG_NOTIMPL , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}},
 { PFM_REG_NOTIMPL , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}},
 { PFM_REG_NOTIMPL , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}},
 { PFM_REG_NOTIMPL , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(4),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(5),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(6),0UL, 0UL, 0UL}},
 { PFM_REG_COUNTING, 0, 0x0UL, -1UL, NULL, NULL, {0UL,0UL, 0UL, 0UL}, {RDEP(7),0UL, 0UL, 0UL}},
	    { PFM_REG_END     , 0, 0x0UL, -1UL, NULL, NULL, {0,}, {0,}}, 
};

static pmu_config_t pmu_conf_gen={
	.pmu_name   = "Generic",
	.pmu_family = 0xff, 
	.ovfl_val   = (1UL << 32) - 1,
	.num_ibrs   = 0, 
	.num_dbrs   = 0, 
	.pmd_desc   = pfm_gen_pmd_desc,
	.pmc_desc   = pfm_gen_pmc_desc
};

