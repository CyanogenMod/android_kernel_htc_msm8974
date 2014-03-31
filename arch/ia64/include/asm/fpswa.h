#ifndef _ASM_IA64_FPSWA_H
#define _ASM_IA64_FPSWA_H

/*
 * Floating-point Software Assist
 *
 * Copyright (C) 1999 Intel Corporation.
 * Copyright (C) 1999 Asit Mallick <asit.k.mallick@intel.com>
 * Copyright (C) 1999 Goutham Rao <goutham.rao@intel.com>
 */

typedef struct {
	
	unsigned long fp_lp[4*2];
} fp_state_low_preserved_t;

typedef struct {
	
	unsigned long fp_lv[10 * 2];
} fp_state_low_volatile_t;

typedef	struct {
	
	unsigned long fp_hp[16 * 2];
} fp_state_high_preserved_t;

typedef struct {
	
	unsigned long fp_hv[96 * 2];
} fp_state_high_volatile_t;

typedef struct {
	unsigned long			bitmask_low64;
	unsigned long			bitmask_high64;
	fp_state_low_preserved_t	*fp_state_low_preserved;
	fp_state_low_volatile_t		*fp_state_low_volatile;
	fp_state_high_preserved_t	*fp_state_high_preserved;
	fp_state_high_volatile_t	*fp_state_high_volatile;
} fp_state_t;

typedef struct {
	unsigned long status;
	unsigned long err0;
	unsigned long err1;
	unsigned long err2;
} fpswa_ret_t;

typedef fpswa_ret_t (*efi_fpswa_t) (unsigned long trap_type, void *bundle, unsigned long *ipsr,
				    unsigned long *fsr, unsigned long *isr, unsigned long *preds,
				    unsigned long *ifs, fp_state_t *fp_state);

typedef struct {
	unsigned int	 revision;
	unsigned int	 reserved;
	efi_fpswa_t	 fpswa;
} fpswa_interface_t;

extern fpswa_interface_t *fpswa_interface;

#endif 
