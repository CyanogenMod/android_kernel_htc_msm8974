/* Blackfin on-chip ROM API
 *
 * Copyright 2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __BFROM_H__
#define __BFROM_H__

#include <linux/types.h>

#define SYSCTRL_READ        0x00000000    
#define SYSCTRL_WRITE       0x00000001    
#define SYSCTRL_SYSRESET    0x00000002    
#define SYSCTRL_CORERESET   0x00000004    
#define SYSCTRL_SOFTRESET   0x00000006    
#define SYSCTRL_VRCTL       0x00000010    
#define SYSCTRL_EXTVOLTAGE  0x00000020    
#define SYSCTRL_INTVOLTAGE  0x00000000    
#define SYSCTRL_OTPVOLTAGE  0x00000040    
#define SYSCTRL_PLLCTL      0x00000100    
#define SYSCTRL_PLLDIV      0x00000200    
#define SYSCTRL_LOCKCNT     0x00000400    
#define SYSCTRL_PLLSTAT     0x00000800    

typedef struct ADI_SYSCTRL_VALUES {
	uint16_t uwVrCtl;
	uint16_t uwPllCtl;
	uint16_t uwPllDiv;
	uint16_t uwPllLockCnt;
	uint16_t uwPllStat;
} ADI_SYSCTRL_VALUES;

static uint32_t (* const bfrom_SysControl)(uint32_t action_flags, ADI_SYSCTRL_VALUES *power_settings, void *reserved) = (void *)0xEF000038;

__attribute__((__noreturn__))
static inline void bfrom_SoftReset(void *new_stack)
{
	while (1)
		__asm__ __volatile__(
			"sp = %[stack];"
			"jump (%[bfrom_syscontrol]);"
			: : [bfrom_syscontrol] "p"(bfrom_SysControl),
				"q0"(SYSCTRL_SOFTRESET),
				"q1"(0),
				"q2"(NULL),
				[stack] "p"(new_stack)
		);
}

static uint32_t (* const bfrom_OtpCommand)(uint32_t command, uint32_t value) = (void *)0xEF000018;
static uint32_t (* const bfrom_OtpRead)(uint32_t page, uint32_t flags, uint64_t *page_content) = (void *)0xEF00001A;
static uint32_t (* const bfrom_OtpWrite)(uint32_t page, uint32_t flags, uint64_t *page_content) = (void *)0xEF00001C;

#define OTP_INIT                 0x00000001
#define OTP_CLOSE                0x00000002

#define OTP_LOWER_HALF           0x00000000 
#define OTP_UPPER_HALF           0x00000001
#define OTP_NO_ECC               0x00000010 
#define OTP_LOCK                 0x00000020 
#define OTP_CHECK_FOR_PREV_WRITE 0x00000080

#define OTP_SUCCESS          0x00000000
#define OTP_MASTER_ERROR     0x001
#define OTP_WRITE_ERROR      0x003
#define OTP_READ_ERROR       0x005
#define OTP_ACC_VIO_ERROR    0x009
#define OTP_DATA_MULT_ERROR  0x011
#define OTP_ECC_MULT_ERROR   0x021
#define OTP_PREV_WR_ERROR    0x041
#define OTP_DATA_SB_WARN     0x100
#define OTP_ECC_SB_WARN      0x200

#endif
