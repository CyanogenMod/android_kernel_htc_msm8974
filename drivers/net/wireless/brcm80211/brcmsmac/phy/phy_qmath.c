/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "phy_qmath.h"

u16 qm_mulu16(u16 op1, u16 op2)
{
	return (u16) (((u32) op1 * (u32) op2) >> 16);
}

s16 qm_muls16(s16 op1, s16 op2)
{
	s32 result;
	if (op1 == (s16) 0x8000 && op2 == (s16) 0x8000)
		result = 0x7fffffff;
	else
		result = ((s32) (op1) * (s32) (op2));

	return (s16) (result >> 15);
}

s32 qm_add32(s32 op1, s32 op2)
{
	s32 result;
	result = op1 + op2;
	if (op1 < 0 && op2 < 0 && result > 0)
		result = 0x80000000;
	else if (op1 > 0 && op2 > 0 && result < 0)
		result = 0x7fffffff;

	return result;
}

s16 qm_add16(s16 op1, s16 op2)
{
	s16 result;
	s32 temp = (s32) op1 + (s32) op2;
	if (temp > (s32) 0x7fff)
		result = (s16) 0x7fff;
	else if (temp < (s32) 0xffff8000)
		result = (s16) 0xffff8000;
	else
		result = (s16) temp;

	return result;
}

s16 qm_sub16(s16 op1, s16 op2)
{
	s16 result;
	s32 temp = (s32) op1 - (s32) op2;
	if (temp > (s32) 0x7fff)
		result = (s16) 0x7fff;
	else if (temp < (s32) 0xffff8000)
		result = (s16) 0xffff8000;
	else
		result = (s16) temp;

	return result;
}

s32 qm_shl32(s32 op, int shift)
{
	int i;
	s32 result;
	result = op;
	if (shift > 31)
		shift = 31;
	else if (shift < -31)
		shift = -31;
	if (shift >= 0) {
		for (i = 0; i < shift; i++)
			result = qm_add32(result, result);
	} else {
		result = result >> (-shift);
	}

	return result;
}

s16 qm_shl16(s16 op, int shift)
{
	int i;
	s16 result;
	result = op;
	if (shift > 15)
		shift = 15;
	else if (shift < -15)
		shift = -15;
	if (shift > 0) {
		for (i = 0; i < shift; i++)
			result = qm_add16(result, result);
	} else {
		result = result >> (-shift);
	}

	return result;
}

s16 qm_shr16(s16 op, int shift)
{
	return qm_shl16(op, -shift);
}

s16 qm_norm32(s32 op)
{
	u16 u16extraSignBits;
	if (op == 0) {
		return 31;
	} else {
		u16extraSignBits = 0;
		while ((op >> 31) == (op >> 30)) {
			u16extraSignBits++;
			op = op << 1;
		}
	}
	return u16extraSignBits;
}

static const s16 log_table[] = {
	0,
	1455,
	2866,
	4236,
	5568,
	6863,
	8124,
	9352,
	10549,
	11716,
	12855,
	13968,
	15055,
	16117,
	17156,
	18173,
	19168,
	20143,
	21098,
	22034,
	22952,
	23852,
	24736,
	25604,
	26455,
	27292,
	28114,
	28922,
	29717,
	30498,
	31267,
	32024
};

#define LOG_TABLE_SIZE 32       
#define LOG2_LOG_TABLE_SIZE 5   
#define Q_LOG_TABLE 15          
#define LOG10_2         19728   

/*
 * Description:
 * This routine takes the input number N and its q format qN and compute
 * the log10(N). This routine first normalizes the input no N.	Then N is in
 * mag*(2^x) format. mag is any number in the range 2^30-(2^31 - 1).
 * Then log2(mag * 2^x) = log2(mag) + x is computed. From that
 * log10(mag * 2^x) = log2(mag * 2^x) * log10(2) is computed.
 * This routine looks the log2 value in the table considering
 * LOG2_LOG_TABLE_SIZE+1 MSBs. As the MSB is always 1, only next
 * LOG2_OF_LOG_TABLE_SIZE MSBs are used for table lookup. Next 16 MSBs are used
 * for interpolation.
 * Inputs:
 * N - number to which log10 has to be found.
 * qN - q format of N
 * log10N - address where log10(N) will be written.
 * qLog10N - address where log10N qformat will be written.
 * Note/Problem:
 * For accurate results input should be in normalized or near normalized form.
 */
void qm_log10(s32 N, s16 qN, s16 *log10N, s16 *qLog10N)
{
	s16 s16norm, s16tableIndex, s16errorApproximation;
	u16 u16offset;
	s32 s32log;

	
	s16norm = qm_norm32(N);
	N = N << s16norm;

	qN = qN + s16norm - 30;

	s16tableIndex = (s16) (N >> (32 - (2 + LOG2_LOG_TABLE_SIZE)));

	
	s16tableIndex =
		s16tableIndex & (s16) ((1 << LOG2_LOG_TABLE_SIZE) - 1);

	
	N = N & ((1 << (32 - (2 + LOG2_LOG_TABLE_SIZE))) - 1);

	u16offset = (u16) (N >> (32 - (2 + LOG2_LOG_TABLE_SIZE + 16)));

	
	s32log = log_table[s16tableIndex];      

	
	s16errorApproximation = (s16) qm_mulu16(u16offset,
				(u16) (log_table[s16tableIndex + 1] -
				       log_table[s16tableIndex]));

	 
	s32log = qm_add16((s16) s32log, s16errorApproximation);

	s32log = qm_add32(s32log, ((s32) -qN) << 15);   

	
	s16norm = qm_norm32(s32log);

	
	
	s32log = qm_shl32(s32log, s16norm - 16);

	*log10N = qm_muls16((s16) s32log, (s16) LOG10_2);

	
	*qLog10N = 15 + s16norm - 16 + 1;

	return;
}
