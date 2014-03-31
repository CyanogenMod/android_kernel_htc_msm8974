/*---------------------------------------------------------------------------+
 |  fpu_arith.c                                                              |
 |                                                                           |
 | Code to implement the FPU register/register arithmetic instructions       |
 |                                                                           |
 | Copyright (C) 1992,1993,1997                                              |
 |                  W. Metzenthen, 22 Parker St, Ormond, Vic 3163, Australia |
 |                  E-mail   billm@suburbia.net                              |
 |                                                                           |
 |                                                                           |
 +---------------------------------------------------------------------------*/

#include "fpu_system.h"
#include "fpu_emu.h"
#include "control_w.h"
#include "status_w.h"

void fadd__(void)
{
	
	int i = FPU_rm;
	clear_C1();
	FPU_add(&st(i), FPU_gettagi(i), 0, control_word);
}

void fmul__(void)
{
	
	int i = FPU_rm;
	clear_C1();
	FPU_mul(&st(i), FPU_gettagi(i), 0, control_word);
}

void fsub__(void)
{
	
	clear_C1();
	FPU_sub(0, FPU_rm, control_word);
}

void fsubr_(void)
{
	
	clear_C1();
	FPU_sub(REV, FPU_rm, control_word);
}

void fdiv__(void)
{
	
	clear_C1();
	FPU_div(0, FPU_rm, control_word);
}

void fdivr_(void)
{
	
	clear_C1();
	FPU_div(REV, FPU_rm, control_word);
}

void fadd_i(void)
{
	
	int i = FPU_rm;
	clear_C1();
	FPU_add(&st(i), FPU_gettagi(i), i, control_word);
}

void fmul_i(void)
{
	
	clear_C1();
	FPU_mul(&st(0), FPU_gettag0(), FPU_rm, control_word);
}

void fsubri(void)
{
	
	clear_C1();
	FPU_sub(DEST_RM, FPU_rm, control_word);
}

void fsub_i(void)
{
	
	clear_C1();
	FPU_sub(REV | DEST_RM, FPU_rm, control_word);
}

void fdivri(void)
{
	
	clear_C1();
	FPU_div(DEST_RM, FPU_rm, control_word);
}

void fdiv_i(void)
{
	
	clear_C1();
	FPU_div(REV | DEST_RM, FPU_rm, control_word);
}

void faddp_(void)
{
	
	int i = FPU_rm;
	clear_C1();
	if (FPU_add(&st(i), FPU_gettagi(i), i, control_word) >= 0)
		FPU_pop();
}

void fmulp_(void)
{
	
	clear_C1();
	if (FPU_mul(&st(0), FPU_gettag0(), FPU_rm, control_word) >= 0)
		FPU_pop();
}

void fsubrp(void)
{
	
	clear_C1();
	if (FPU_sub(DEST_RM, FPU_rm, control_word) >= 0)
		FPU_pop();
}

void fsubp_(void)
{
	
	clear_C1();
	if (FPU_sub(REV | DEST_RM, FPU_rm, control_word) >= 0)
		FPU_pop();
}

void fdivrp(void)
{
	
	clear_C1();
	if (FPU_div(DEST_RM, FPU_rm, control_word) >= 0)
		FPU_pop();
}

void fdivp_(void)
{
	
	clear_C1();
	if (FPU_div(REV | DEST_RM, FPU_rm, control_word) >= 0)
		FPU_pop();
}
