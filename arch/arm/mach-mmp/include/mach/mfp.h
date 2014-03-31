#ifndef __ASM_MACH_MFP_H
#define __ASM_MACH_MFP_H

#include <plat/mfp.h>


#undef MFP_CFG
#undef MFP_CFG_DRV
#undef MFP_CFG_LPM
#undef MFP_CFG_X
#undef MFP_CFG_DEFAULT

#define MFP_CFG(pin, af)		\
	(MFP_LPM_FLOAT | MFP_PIN(MFP_PIN_##pin) | MFP_##af | MFP_DRIVE_MEDIUM)

#define MFP_CFG_DRV(pin, af, drv)	\
	(MFP_LPM_FLOAT | MFP_PIN(MFP_PIN_##pin) | MFP_##af | MFP_DRIVE_##drv)

#endif 
