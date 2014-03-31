/* gf128mul.h - GF(2^128) multiplication functions
 *
 * Copyright (c) 2003, Dr Brian Gladman, Worcester, UK.
 * Copyright (c) 2006 Rik Snel <rsnel@cube.dyndns.org>
 *
 * Based on Dr Brian Gladman's (GPL'd) work published at
 * http://fp.gladman.plus.com/cryptography_technology/index.htm
 * See the original copyright notice below.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */
/*
 ---------------------------------------------------------------------------
 Copyright (c) 2003, Dr Brian Gladman, Worcester, UK.   All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products
      built using this software without specific written permission.

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue Date: 31/01/2006

 An implementation of field multiplication in Galois Field GF(128)
*/

#ifndef _CRYPTO_GF128MUL_H
#define _CRYPTO_GF128MUL_H

#include <crypto/b128ops.h>
#include <linux/slab.h>


void gf128mul_lle(be128 *a, const be128 *b);

void gf128mul_bbe(be128 *a, const be128 *b);

void gf128mul_x_ble(be128 *a, const be128 *b);


struct gf128mul_4k {
	be128 t[256];
};

struct gf128mul_4k *gf128mul_init_4k_lle(const be128 *g);
struct gf128mul_4k *gf128mul_init_4k_bbe(const be128 *g);
void gf128mul_4k_lle(be128 *a, struct gf128mul_4k *t);
void gf128mul_4k_bbe(be128 *a, struct gf128mul_4k *t);

static inline void gf128mul_free_4k(struct gf128mul_4k *t)
{
	kfree(t);
}



struct gf128mul_64k {
	struct gf128mul_4k *t[16];
};

struct gf128mul_64k *gf128mul_init_64k_lle(const be128 *g);
struct gf128mul_64k *gf128mul_init_64k_bbe(const be128 *g);
void gf128mul_free_64k(struct gf128mul_64k *t);
void gf128mul_64k_lle(be128 *a, struct gf128mul_64k *t);
void gf128mul_64k_bbe(be128 *a, struct gf128mul_64k *t);

#endif 
