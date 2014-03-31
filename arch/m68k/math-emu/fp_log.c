/*

  fp_trig.c: floating-point math routines for the Linux-m68k
  floating point emulator.

  Copyright (c) 1998-1999 David Huggins-Daines / Roman Zippel.

  I hereby give permission, free of charge, to copy, modify, and
  redistribute this software, in source or binary form, provided that
  the above copyright notice and the following disclaimer are included
  in all such copies.

  THIS SOFTWARE IS PROVIDED "AS IS", WITH ABSOLUTELY NO WARRANTY, REAL
  OR IMPLIED.

*/

#include "fp_emu.h"

static const struct fp_ext fp_one =
{
	.exp = 0x3fff,
};

extern struct fp_ext *fp_fadd(struct fp_ext *dest, const struct fp_ext *src);
extern struct fp_ext *fp_fdiv(struct fp_ext *dest, const struct fp_ext *src);

struct fp_ext *
fp_fsqrt(struct fp_ext *dest, struct fp_ext *src)
{
	struct fp_ext tmp, src2;
	int i, exp;

	dprint(PINSTR, "fsqrt\n");

	fp_monadic_check(dest, src);

	if (IS_ZERO(dest))
		return dest;

	if (dest->sign) {
		fp_set_nan(dest);
		return dest;
	}
	if (IS_INF(dest))
		return dest;

	exp = dest->exp;
	dest->exp = 0x3FFF;
	if (!(exp & 1))		
		dest->exp++;
	fp_copy_ext(&src2, dest);

	fp_fadd(dest, &fp_one);
	dest->exp--;		

	for (i = 0; i < 9; i++) {
		fp_copy_ext(&tmp, &src2);

		fp_fdiv(&tmp, dest);
		fp_fadd(dest, &tmp);
		dest->exp--;
	}

	dest->exp += (exp - 0x3FFF) / 2;

	return dest;
}

struct fp_ext *
fp_fetoxm1(struct fp_ext *dest, struct fp_ext *src)
{
	uprint("fetoxm1\n");

	fp_monadic_check(dest, src);

	return dest;
}

struct fp_ext *
fp_fetox(struct fp_ext *dest, struct fp_ext *src)
{
	uprint("fetox\n");

	fp_monadic_check(dest, src);

	return dest;
}

struct fp_ext *
fp_ftwotox(struct fp_ext *dest, struct fp_ext *src)
{
	uprint("ftwotox\n");

	fp_monadic_check(dest, src);

	return dest;
}

struct fp_ext *
fp_ftentox(struct fp_ext *dest, struct fp_ext *src)
{
	uprint("ftentox\n");

	fp_monadic_check(dest, src);

	return dest;
}

struct fp_ext *
fp_flogn(struct fp_ext *dest, struct fp_ext *src)
{
	uprint("flogn\n");

	fp_monadic_check(dest, src);

	return dest;
}

struct fp_ext *
fp_flognp1(struct fp_ext *dest, struct fp_ext *src)
{
	uprint("flognp1\n");

	fp_monadic_check(dest, src);

	return dest;
}

struct fp_ext *
fp_flog10(struct fp_ext *dest, struct fp_ext *src)
{
	uprint("flog10\n");

	fp_monadic_check(dest, src);

	return dest;
}

struct fp_ext *
fp_flog2(struct fp_ext *dest, struct fp_ext *src)
{
	uprint("flog2\n");

	fp_monadic_check(dest, src);

	return dest;
}

struct fp_ext *
fp_fgetexp(struct fp_ext *dest, struct fp_ext *src)
{
	dprint(PINSTR, "fgetexp\n");

	fp_monadic_check(dest, src);

	if (IS_INF(dest)) {
		fp_set_nan(dest);
		return dest;
	}
	if (IS_ZERO(dest))
		return dest;

	fp_conv_long2ext(dest, (int)dest->exp - 0x3FFF);

	fp_normalize_ext(dest);

	return dest;
}

struct fp_ext *
fp_fgetman(struct fp_ext *dest, struct fp_ext *src)
{
	dprint(PINSTR, "fgetman\n");

	fp_monadic_check(dest, src);

	if (IS_ZERO(dest))
		return dest;

	if (IS_INF(dest))
		return dest;

	dest->exp = 0x3FFF;

	return dest;
}

