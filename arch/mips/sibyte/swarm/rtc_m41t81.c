/*
 * Copyright (C) 2000, 2001 Broadcom Corporation
 *
 * Copyright (C) 2002 MontaVista Software Inc.
 * Author: jsun@mvista.com or jsun@junsun.net
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/bcd.h>
#include <linux/types.h>
#include <linux/time.h>

#include <asm/time.h>
#include <asm/addrspace.h>
#include <asm/io.h>

#include <asm/sibyte/sb1250.h>
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_smbus.h>




#define M41T81REG_SC_ST		0x80		
#define M41T81REG_HR_CB		0x40		
#define M41T81REG_HR_CEB	0x80		
#define M41T81REG_CTL_S		0x20		
#define M41T81REG_CTL_FT	0x40		
#define M41T81REG_CTL_OUT	0x80		
#define M41T81REG_WD_RB0	0x01		
#define M41T81REG_WD_RB1	0x02		
#define M41T81REG_WD_BMB0	0x04		
#define M41T81REG_WD_BMB1	0x08		
#define M41T81REG_WD_BMB2	0x10		
#define M41T81REG_WD_BMB3	0x20		
#define M41T81REG_WD_BMB4	0x40		
#define M41T81REG_AMO_ABE	0x20		
#define M41T81REG_AMO_SQWE	0x40		
#define M41T81REG_AMO_AFE	0x80		
#define M41T81REG_ADT_RPT5	0x40		
#define M41T81REG_ADT_RPT4	0x80		
#define M41T81REG_AHR_RPT3	0x80		
#define M41T81REG_AHR_HT	0x40		
#define M41T81REG_AMN_RPT2	0x80		
#define M41T81REG_ASC_RPT1	0x80		
#define M41T81REG_FLG_AF	0x40		
#define M41T81REG_FLG_WDF	0x80		
#define M41T81REG_SQW_RS0	0x10		
#define M41T81REG_SQW_RS1	0x20		
#define M41T81REG_SQW_RS2	0x40		
#define M41T81REG_SQW_RS3	0x80		



#define M41T81REG_TSC	0x00		
#define M41T81REG_SC	0x01		
#define M41T81REG_MN	0x02		
#define M41T81REG_HR	0x03		
#define M41T81REG_DY	0x04		
#define M41T81REG_DT	0x05		
#define M41T81REG_MO	0x06		
#define M41T81REG_YR	0x07		
#define M41T81REG_CTL	0x08		
#define M41T81REG_WD	0x09		
#define M41T81REG_AMO	0x0A		
#define M41T81REG_ADT	0x0B		
#define M41T81REG_AHR	0x0C		
#define M41T81REG_AMN	0x0D		
#define M41T81REG_ASC	0x0E		
#define M41T81REG_FLG	0x0F		
#define M41T81REG_SQW	0x13		

#define M41T81_CCR_ADDRESS	0x68

#define SMB_CSR(reg)	IOADDR(A_SMB_REGISTER(1, reg))

static int m41t81_read(uint8_t addr)
{
	while (__raw_readq(SMB_CSR(R_SMB_STATUS)) & M_SMB_BUSY)
		;

	__raw_writeq(addr & 0xff, SMB_CSR(R_SMB_CMD));
	__raw_writeq(V_SMB_ADDR(M41T81_CCR_ADDRESS) | V_SMB_TT_WR1BYTE,
		     SMB_CSR(R_SMB_START));

	while (__raw_readq(SMB_CSR(R_SMB_STATUS)) & M_SMB_BUSY)
		;

	__raw_writeq(V_SMB_ADDR(M41T81_CCR_ADDRESS) | V_SMB_TT_RD1BYTE,
		     SMB_CSR(R_SMB_START));

	while (__raw_readq(SMB_CSR(R_SMB_STATUS)) & M_SMB_BUSY)
		;

	if (__raw_readq(SMB_CSR(R_SMB_STATUS)) & M_SMB_ERROR) {
		
		__raw_writeq(M_SMB_ERROR, SMB_CSR(R_SMB_STATUS));
		return -1;
	}

	return (__raw_readq(SMB_CSR(R_SMB_DATA)) & 0xff);
}

static int m41t81_write(uint8_t addr, int b)
{
	while (__raw_readq(SMB_CSR(R_SMB_STATUS)) & M_SMB_BUSY)
		;

	__raw_writeq(addr & 0xff, SMB_CSR(R_SMB_CMD));
	__raw_writeq(b & 0xff, SMB_CSR(R_SMB_DATA));
	__raw_writeq(V_SMB_ADDR(M41T81_CCR_ADDRESS) | V_SMB_TT_WR2BYTE,
		     SMB_CSR(R_SMB_START));

	while (__raw_readq(SMB_CSR(R_SMB_STATUS)) & M_SMB_BUSY)
		;

	if (__raw_readq(SMB_CSR(R_SMB_STATUS)) & M_SMB_ERROR) {
		
		__raw_writeq(M_SMB_ERROR, SMB_CSR(R_SMB_STATUS));
		return -1;
	}

	/* read the same byte again to make sure it is written */
	__raw_writeq(V_SMB_ADDR(M41T81_CCR_ADDRESS) | V_SMB_TT_RD1BYTE,
		     SMB_CSR(R_SMB_START));

	while (__raw_readq(SMB_CSR(R_SMB_STATUS)) & M_SMB_BUSY)
		;

	return 0;
}

int m41t81_set_time(unsigned long t)
{
	struct rtc_time tm;
	unsigned long flags;

	
	rtc_time_to_tm(t, &tm);


	spin_lock_irqsave(&rtc_lock, flags);
	tm.tm_sec = bin2bcd(tm.tm_sec);
	m41t81_write(M41T81REG_SC, tm.tm_sec);

	tm.tm_min = bin2bcd(tm.tm_min);
	m41t81_write(M41T81REG_MN, tm.tm_min);

	tm.tm_hour = bin2bcd(tm.tm_hour);
	tm.tm_hour = (tm.tm_hour & 0x3f) | (m41t81_read(M41T81REG_HR) & 0xc0);
	m41t81_write(M41T81REG_HR, tm.tm_hour);

	
	if (tm.tm_wday == 0) tm.tm_wday = 7;
	tm.tm_wday = bin2bcd(tm.tm_wday);
	m41t81_write(M41T81REG_DY, tm.tm_wday);

	tm.tm_mday = bin2bcd(tm.tm_mday);
	m41t81_write(M41T81REG_DT, tm.tm_mday);

	
	tm.tm_mon ++;
	tm.tm_mon = bin2bcd(tm.tm_mon);
	m41t81_write(M41T81REG_MO, tm.tm_mon);

	
	tm.tm_year %= 100;
	tm.tm_year = bin2bcd(tm.tm_year);
	m41t81_write(M41T81REG_YR, tm.tm_year);
	spin_unlock_irqrestore(&rtc_lock, flags);

	return 0;
}

unsigned long m41t81_get_time(void)
{
	unsigned int year, mon, day, hour, min, sec;
	unsigned long flags;

	for (;;) {
		spin_lock_irqsave(&rtc_lock, flags);
		sec = m41t81_read(M41T81REG_SC);
		min = m41t81_read(M41T81REG_MN);
		if (sec == m41t81_read(M41T81REG_SC)) break;
		spin_unlock_irqrestore(&rtc_lock, flags);
	}
	hour = m41t81_read(M41T81REG_HR) & 0x3f;
	day = m41t81_read(M41T81REG_DT);
	mon = m41t81_read(M41T81REG_MO);
	year = m41t81_read(M41T81REG_YR);
	spin_unlock_irqrestore(&rtc_lock, flags);

	sec = bcd2bin(sec);
	min = bcd2bin(min);
	hour = bcd2bin(hour);
	day = bcd2bin(day);
	mon = bcd2bin(mon);
	year = bcd2bin(year);

	year += 2000;

	return mktime(year, mon, day, hour, min, sec);
}

int m41t81_probe(void)
{
	unsigned int tmp;

	
	tmp = m41t81_read(M41T81REG_SC);
	m41t81_write(M41T81REG_SC, tmp & 0x7f);

	return (m41t81_read(M41T81REG_SC) != -1);
}
