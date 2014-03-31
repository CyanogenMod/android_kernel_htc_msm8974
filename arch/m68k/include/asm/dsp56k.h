/*
 * linux/include/asm-m68k/dsp56k.h - defines and declarations for
 *                                   DSP56k device driver
 *
 * Copyright (C) 1996,1997 Fredrik Noring, lars brinkhoff & Tomas Berndtsson
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */


struct dsp56k_upload {
	int len;
	char __user *bin;
};

struct dsp56k_host_flags {
	int dir;     
	int out;     
	int status;  
};

#define DSP56K_UPLOAD	        1    
#define DSP56K_SET_TX_WSIZE	2    
#define DSP56K_SET_RX_WSIZE	3    
#define DSP56K_HOST_FLAGS	4    
#define DSP56K_HOST_CMD         5    
