/*
 * Generic binary BCH encoding/decoding library
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright © 2011 Parrot S.A.
 *
 * Author: Ivan Djelic <ivan.djelic@parrot.com>
 *
 * Description:
 *
 * This library provides runtime configurable encoding/decoding of binary
 * Bose-Chaudhuri-Hocquenghem (BCH) codes.
 *
 * Call init_bch to get a pointer to a newly allocated bch_control structure for
 * the given m (Galois field order), t (error correction capability) and
 * (optional) primitive polynomial parameters.
 *
 * Call encode_bch to compute and store ecc parity bytes to a given buffer.
 * Call decode_bch to detect and locate errors in received data.
 *
 * On systems supporting hw BCH features, intermediate results may be provided
 * to decode_bch in order to skip certain steps. See decode_bch() documentation
 * for details.
 *
 * Option CONFIG_BCH_CONST_PARAMS can be used to force fixed values of
 * parameters m and t; thus allowing extra compiler optimizations and providing
 * better (up to 2x) encoding performance. Using this option makes sense when
 * (m,t) are fixed and known in advance, e.g. when using BCH error correction
 * on a particular NAND flash device.
 *
 * Algorithmic details:
 *
 * Encoding is performed by processing 32 input bits in parallel, using 4
 * remainder lookup tables.
 *
 * The final stage of decoding involves the following internal steps:
 * a. Syndrome computation
 * b. Error locator polynomial computation using Berlekamp-Massey algorithm
 * c. Error locator root finding (by far the most expensive step)
 *
 * In this implementation, step c is not performed using the usual Chien search.
 * Instead, an alternative approach described in [1] is used. It consists in
 * factoring the error locator polynomial using the Berlekamp Trace algorithm
 * (BTA) down to a certain degree (4), after which ad hoc low-degree polynomial
 * solving techniques [2] are used. The resulting algorithm, called BTZ, yields
 * much better performance than Chien search for usual (m,t) values (typically
 * m >= 13, t < 32, see [1]).
 *
 * [1] B. Biswas, V. Herbert. Efficient root finding of polynomials over fields
 * of characteristic 2, in: Western European Workshop on Research in Cryptology
 * - WEWoRC 2009, Graz, Austria, LNCS, Springer, July 2009, to appear.
 * [2] [Zin96] V.A. Zinoviev. On the solution of equations of degree 10 over
 * finite fields GF(2^q). In Rapport de recherche INRIA no 2829, 1996.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <asm/byteorder.h>
#include <linux/bch.h>

#if defined(CONFIG_BCH_CONST_PARAMS)
#define GF_M(_p)               (CONFIG_BCH_CONST_M)
#define GF_T(_p)               (CONFIG_BCH_CONST_T)
#define GF_N(_p)               ((1 << (CONFIG_BCH_CONST_M))-1)
#else
#define GF_M(_p)               ((_p)->m)
#define GF_T(_p)               ((_p)->t)
#define GF_N(_p)               ((_p)->n)
#endif

#define BCH_ECC_WORDS(_p)      DIV_ROUND_UP(GF_M(_p)*GF_T(_p), 32)
#define BCH_ECC_BYTES(_p)      DIV_ROUND_UP(GF_M(_p)*GF_T(_p), 8)

#ifndef dbg
#define dbg(_fmt, args...)     do {} while (0)
#endif

struct gf_poly {
	unsigned int deg;    
	unsigned int c[0];   
};

#define GF_POLY_SZ(_d) (sizeof(struct gf_poly)+((_d)+1)*sizeof(unsigned int))

struct gf_poly_deg1 {
	struct gf_poly poly;
	unsigned int   c[2];
};

static void encode_bch_unaligned(struct bch_control *bch,
				 const unsigned char *data, unsigned int len,
				 uint32_t *ecc)
{
	int i;
	const uint32_t *p;
	const int l = BCH_ECC_WORDS(bch)-1;

	while (len--) {
		p = bch->mod8_tab + (l+1)*(((ecc[0] >> 24)^(*data++)) & 0xff);

		for (i = 0; i < l; i++)
			ecc[i] = ((ecc[i] << 8)|(ecc[i+1] >> 24))^(*p++);

		ecc[l] = (ecc[l] << 8)^(*p);
	}
}

static void load_ecc8(struct bch_control *bch, uint32_t *dst,
		      const uint8_t *src)
{
	uint8_t pad[4] = {0, 0, 0, 0};
	unsigned int i, nwords = BCH_ECC_WORDS(bch)-1;

	for (i = 0; i < nwords; i++, src += 4)
		dst[i] = (src[0] << 24)|(src[1] << 16)|(src[2] << 8)|src[3];

	memcpy(pad, src, BCH_ECC_BYTES(bch)-4*nwords);
	dst[nwords] = (pad[0] << 24)|(pad[1] << 16)|(pad[2] << 8)|pad[3];
}

static void store_ecc8(struct bch_control *bch, uint8_t *dst,
		       const uint32_t *src)
{
	uint8_t pad[4];
	unsigned int i, nwords = BCH_ECC_WORDS(bch)-1;

	for (i = 0; i < nwords; i++) {
		*dst++ = (src[i] >> 24);
		*dst++ = (src[i] >> 16) & 0xff;
		*dst++ = (src[i] >>  8) & 0xff;
		*dst++ = (src[i] >>  0) & 0xff;
	}
	pad[0] = (src[nwords] >> 24);
	pad[1] = (src[nwords] >> 16) & 0xff;
	pad[2] = (src[nwords] >>  8) & 0xff;
	pad[3] = (src[nwords] >>  0) & 0xff;
	memcpy(dst, pad, BCH_ECC_BYTES(bch)-4*nwords);
}

void encode_bch(struct bch_control *bch, const uint8_t *data,
		unsigned int len, uint8_t *ecc)
{
	const unsigned int l = BCH_ECC_WORDS(bch)-1;
	unsigned int i, mlen;
	unsigned long m;
	uint32_t w, r[l+1];
	const uint32_t * const tab0 = bch->mod8_tab;
	const uint32_t * const tab1 = tab0 + 256*(l+1);
	const uint32_t * const tab2 = tab1 + 256*(l+1);
	const uint32_t * const tab3 = tab2 + 256*(l+1);
	const uint32_t *pdata, *p0, *p1, *p2, *p3;

	if (ecc) {
		
		load_ecc8(bch, bch->ecc_buf, ecc);
	} else {
		memset(bch->ecc_buf, 0, sizeof(r));
	}

	
	m = ((unsigned long)data) & 3;
	if (m) {
		mlen = (len < (4-m)) ? len : 4-m;
		encode_bch_unaligned(bch, data, mlen, bch->ecc_buf);
		data += mlen;
		len  -= mlen;
	}

	
	pdata = (uint32_t *)data;
	mlen  = len/4;
	data += 4*mlen;
	len  -= 4*mlen;
	memcpy(r, bch->ecc_buf, sizeof(r));

	while (mlen--) {
		
		w = r[0]^cpu_to_be32(*pdata++);
		p0 = tab0 + (l+1)*((w >>  0) & 0xff);
		p1 = tab1 + (l+1)*((w >>  8) & 0xff);
		p2 = tab2 + (l+1)*((w >> 16) & 0xff);
		p3 = tab3 + (l+1)*((w >> 24) & 0xff);

		for (i = 0; i < l; i++)
			r[i] = r[i+1]^p0[i]^p1[i]^p2[i]^p3[i];

		r[l] = p0[l]^p1[l]^p2[l]^p3[l];
	}
	memcpy(bch->ecc_buf, r, sizeof(r));

	
	if (len)
		encode_bch_unaligned(bch, data, len, bch->ecc_buf);

	
	if (ecc)
		store_ecc8(bch, ecc, bch->ecc_buf);
}
EXPORT_SYMBOL_GPL(encode_bch);

static inline int modulo(struct bch_control *bch, unsigned int v)
{
	const unsigned int n = GF_N(bch);
	while (v >= n) {
		v -= n;
		v = (v & n) + (v >> GF_M(bch));
	}
	return v;
}

static inline int mod_s(struct bch_control *bch, unsigned int v)
{
	const unsigned int n = GF_N(bch);
	return (v < n) ? v : v-n;
}

static inline int deg(unsigned int poly)
{
	
	return fls(poly)-1;
}

static inline int parity(unsigned int x)
{
	x ^= x >> 1;
	x ^= x >> 2;
	x = (x & 0x11111111U) * 0x11111111U;
	return (x >> 28) & 1;
}


static inline unsigned int gf_mul(struct bch_control *bch, unsigned int a,
				  unsigned int b)
{
	return (a && b) ? bch->a_pow_tab[mod_s(bch, bch->a_log_tab[a]+
					       bch->a_log_tab[b])] : 0;
}

static inline unsigned int gf_sqr(struct bch_control *bch, unsigned int a)
{
	return a ? bch->a_pow_tab[mod_s(bch, 2*bch->a_log_tab[a])] : 0;
}

static inline unsigned int gf_div(struct bch_control *bch, unsigned int a,
				  unsigned int b)
{
	return a ? bch->a_pow_tab[mod_s(bch, bch->a_log_tab[a]+
					GF_N(bch)-bch->a_log_tab[b])] : 0;
}

static inline unsigned int gf_inv(struct bch_control *bch, unsigned int a)
{
	return bch->a_pow_tab[GF_N(bch)-bch->a_log_tab[a]];
}

static inline unsigned int a_pow(struct bch_control *bch, int i)
{
	return bch->a_pow_tab[modulo(bch, i)];
}

static inline int a_log(struct bch_control *bch, unsigned int x)
{
	return bch->a_log_tab[x];
}

static inline int a_ilog(struct bch_control *bch, unsigned int x)
{
	return mod_s(bch, GF_N(bch)-bch->a_log_tab[x]);
}

static void compute_syndromes(struct bch_control *bch, uint32_t *ecc,
			      unsigned int *syn)
{
	int i, j, s;
	unsigned int m;
	uint32_t poly;
	const int t = GF_T(bch);

	s = bch->ecc_bits;

	
	m = ((unsigned int)s) & 31;
	if (m)
		ecc[s/32] &= ~((1u << (32-m))-1);
	memset(syn, 0, 2*t*sizeof(*syn));

	
	do {
		poly = *ecc++;
		s -= 32;
		while (poly) {
			i = deg(poly);
			for (j = 0; j < 2*t; j += 2)
				syn[j] ^= a_pow(bch, (j+1)*(i+s));

			poly ^= (1 << i);
		}
	} while (s > 0);

	
	for (j = 0; j < t; j++)
		syn[2*j+1] = gf_sqr(bch, syn[j]);
}

static void gf_poly_copy(struct gf_poly *dst, struct gf_poly *src)
{
	memcpy(dst, src, GF_POLY_SZ(src->deg));
}

static int compute_error_locator_polynomial(struct bch_control *bch,
					    const unsigned int *syn)
{
	const unsigned int t = GF_T(bch);
	const unsigned int n = GF_N(bch);
	unsigned int i, j, tmp, l, pd = 1, d = syn[0];
	struct gf_poly *elp = bch->elp;
	struct gf_poly *pelp = bch->poly_2t[0];
	struct gf_poly *elp_copy = bch->poly_2t[1];
	int k, pp = -1;

	memset(pelp, 0, GF_POLY_SZ(2*t));
	memset(elp, 0, GF_POLY_SZ(2*t));

	pelp->deg = 0;
	pelp->c[0] = 1;
	elp->deg = 0;
	elp->c[0] = 1;

	
	for (i = 0; (i < t) && (elp->deg <= t); i++) {
		if (d) {
			k = 2*i-pp;
			gf_poly_copy(elp_copy, elp);
			
			tmp = a_log(bch, d)+n-a_log(bch, pd);
			for (j = 0; j <= pelp->deg; j++) {
				if (pelp->c[j]) {
					l = a_log(bch, pelp->c[j]);
					elp->c[j+k] ^= a_pow(bch, tmp+l);
				}
			}
			
			tmp = pelp->deg+k;
			if (tmp > elp->deg) {
				elp->deg = tmp;
				gf_poly_copy(pelp, elp_copy);
				pd = d;
				pp = 2*i;
			}
		}
		
		if (i < t-1) {
			d = syn[2*i+2];
			for (j = 1; j <= elp->deg; j++)
				d ^= gf_mul(bch, elp->c[j], syn[2*i+2-j]);
		}
	}
	dbg("elp=%s\n", gf_poly_str(elp));
	return (elp->deg > t) ? -1 : (int)elp->deg;
}

static int solve_linear_system(struct bch_control *bch, unsigned int *rows,
			       unsigned int *sol, int nsol)
{
	const int m = GF_M(bch);
	unsigned int tmp, mask;
	int rem, c, r, p, k, param[m];

	k = 0;
	mask = 1 << m;

	
	for (c = 0; c < m; c++) {
		rem = 0;
		p = c-k;
		
		for (r = p; r < m; r++) {
			if (rows[r] & mask) {
				if (r != p) {
					tmp = rows[r];
					rows[r] = rows[p];
					rows[p] = tmp;
				}
				rem = r+1;
				break;
			}
		}
		if (rem) {
			
			tmp = rows[p];
			for (r = rem; r < m; r++) {
				if (rows[r] & mask)
					rows[r] ^= tmp;
			}
		} else {
			
			param[k++] = c;
		}
		mask >>= 1;
	}
	
	if (k > 0) {
		p = k;
		for (r = m-1; r >= 0; r--) {
			if ((r > m-1-k) && rows[r])
				
				return 0;

			rows[r] = (p && (r == param[p-1])) ?
				p--, 1u << (m-r) : rows[r-p];
		}
	}

	if (nsol != (1 << k))
		
		return 0;

	for (p = 0; p < nsol; p++) {
		
		for (c = 0; c < k; c++)
			rows[param[c]] = (rows[param[c]] & ~1)|((p >> c) & 1);

		
		tmp = 0;
		for (r = m-1; r >= 0; r--) {
			mask = rows[r] & (tmp|1);
			tmp |= parity(mask) << (m-r);
		}
		sol[p] = tmp >> 1;
	}
	return nsol;
}

static int find_affine4_roots(struct bch_control *bch, unsigned int a,
			      unsigned int b, unsigned int c,
			      unsigned int *roots)
{
	int i, j, k;
	const int m = GF_M(bch);
	unsigned int mask = 0xff, t, rows[16] = {0,};

	j = a_log(bch, b);
	k = a_log(bch, a);
	rows[0] = c;

	
	for (i = 0; i < m; i++) {
		rows[i+1] = bch->a_pow_tab[4*i]^
			(a ? bch->a_pow_tab[mod_s(bch, k)] : 0)^
			(b ? bch->a_pow_tab[mod_s(bch, j)] : 0);
		j++;
		k += 2;
	}
	for (j = 8; j != 0; j >>= 1, mask ^= (mask << j)) {
		for (k = 0; k < 16; k = (k+j+1) & ~j) {
			t = ((rows[k] >> j)^rows[k+j]) & mask;
			rows[k] ^= (t << j);
			rows[k+j] ^= t;
		}
	}
	return solve_linear_system(bch, rows, roots, 4);
}

static int find_poly_deg1_roots(struct bch_control *bch, struct gf_poly *poly,
				unsigned int *roots)
{
	int n = 0;

	if (poly->c[0])
		
		roots[n++] = mod_s(bch, GF_N(bch)-bch->a_log_tab[poly->c[0]]+
				   bch->a_log_tab[poly->c[1]]);
	return n;
}

static int find_poly_deg2_roots(struct bch_control *bch, struct gf_poly *poly,
				unsigned int *roots)
{
	int n = 0, i, l0, l1, l2;
	unsigned int u, v, r;

	if (poly->c[0] && poly->c[1]) {

		l0 = bch->a_log_tab[poly->c[0]];
		l1 = bch->a_log_tab[poly->c[1]];
		l2 = bch->a_log_tab[poly->c[2]];

		
		u = a_pow(bch, l0+l2+2*(GF_N(bch)-l1));
		r = 0;
		v = u;
		while (v) {
			i = deg(v);
			r ^= bch->xi_tab[i];
			v ^= (1 << i);
		}
		
		if ((gf_sqr(bch, r)^r) == u) {
			
			roots[n++] = modulo(bch, 2*GF_N(bch)-l1-
					    bch->a_log_tab[r]+l2);
			roots[n++] = modulo(bch, 2*GF_N(bch)-l1-
					    bch->a_log_tab[r^1]+l2);
		}
	}
	return n;
}

static int find_poly_deg3_roots(struct bch_control *bch, struct gf_poly *poly,
				unsigned int *roots)
{
	int i, n = 0;
	unsigned int a, b, c, a2, b2, c2, e3, tmp[4];

	if (poly->c[0]) {
		
		e3 = poly->c[3];
		c2 = gf_div(bch, poly->c[0], e3);
		b2 = gf_div(bch, poly->c[1], e3);
		a2 = gf_div(bch, poly->c[2], e3);

		
		c = gf_mul(bch, a2, c2);           
		b = gf_mul(bch, a2, b2)^c2;        
		a = gf_sqr(bch, a2)^b2;            

		
		if (find_affine4_roots(bch, a, b, c, tmp) == 4) {
			
			for (i = 0; i < 4; i++) {
				if (tmp[i] != a2)
					roots[n++] = a_ilog(bch, tmp[i]);
			}
		}
	}
	return n;
}

static int find_poly_deg4_roots(struct bch_control *bch, struct gf_poly *poly,
				unsigned int *roots)
{
	int i, l, n = 0;
	unsigned int a, b, c, d, e = 0, f, a2, b2, c2, e4;

	if (poly->c[0] == 0)
		return 0;

	
	e4 = poly->c[4];
	d = gf_div(bch, poly->c[0], e4);
	c = gf_div(bch, poly->c[1], e4);
	b = gf_div(bch, poly->c[2], e4);
	a = gf_div(bch, poly->c[3], e4);

	
	if (a) {
		
		if (c) {
			
			f = gf_div(bch, c, a);
			l = a_log(bch, f);
			l += (l & 1) ? GF_N(bch) : 0;
			e = a_pow(bch, l/2);
			d = a_pow(bch, 2*l)^gf_mul(bch, b, f)^d;
			b = gf_mul(bch, a, e)^b;
		}
		
		if (d == 0)
			
			return 0;

		c2 = gf_inv(bch, d);
		b2 = gf_div(bch, a, d);
		a2 = gf_div(bch, b, d);
	} else {
		
		c2 = d;
		b2 = c;
		a2 = b;
	}
	
	if (find_affine4_roots(bch, a2, b2, c2, roots) == 4) {
		for (i = 0; i < 4; i++) {
			
			f = a ? gf_inv(bch, roots[i]) : roots[i];
			roots[i] = a_ilog(bch, f^e);
		}
		n = 4;
	}
	return n;
}

static void gf_poly_logrep(struct bch_control *bch,
			   const struct gf_poly *a, int *rep)
{
	int i, d = a->deg, l = GF_N(bch)-a_log(bch, a->c[a->deg]);

	
	for (i = 0; i < d; i++)
		rep[i] = a->c[i] ? mod_s(bch, a_log(bch, a->c[i])+l) : -1;
}

static void gf_poly_mod(struct bch_control *bch, struct gf_poly *a,
			const struct gf_poly *b, int *rep)
{
	int la, p, m;
	unsigned int i, j, *c = a->c;
	const unsigned int d = b->deg;

	if (a->deg < d)
		return;

	
	if (!rep) {
		rep = bch->cache;
		gf_poly_logrep(bch, b, rep);
	}

	for (j = a->deg; j >= d; j--) {
		if (c[j]) {
			la = a_log(bch, c[j]);
			p = j-d;
			for (i = 0; i < d; i++, p++) {
				m = rep[i];
				if (m >= 0)
					c[p] ^= bch->a_pow_tab[mod_s(bch,
								     m+la)];
			}
		}
	}
	a->deg = d-1;
	while (!c[a->deg] && a->deg)
		a->deg--;
}

static void gf_poly_div(struct bch_control *bch, struct gf_poly *a,
			const struct gf_poly *b, struct gf_poly *q)
{
	if (a->deg >= b->deg) {
		q->deg = a->deg-b->deg;
		
		gf_poly_mod(bch, a, b, NULL);
		
		memcpy(q->c, &a->c[b->deg], (1+q->deg)*sizeof(unsigned int));
	} else {
		q->deg = 0;
		q->c[0] = 0;
	}
}

static struct gf_poly *gf_poly_gcd(struct bch_control *bch, struct gf_poly *a,
				   struct gf_poly *b)
{
	struct gf_poly *tmp;

	dbg("gcd(%s,%s)=", gf_poly_str(a), gf_poly_str(b));

	if (a->deg < b->deg) {
		tmp = b;
		b = a;
		a = tmp;
	}

	while (b->deg > 0) {
		gf_poly_mod(bch, a, b, NULL);
		tmp = b;
		b = a;
		a = tmp;
	}

	dbg("%s\n", gf_poly_str(a));

	return a;
}

static void compute_trace_bk_mod(struct bch_control *bch, int k,
				 const struct gf_poly *f, struct gf_poly *z,
				 struct gf_poly *out)
{
	const int m = GF_M(bch);
	int i, j;

	
	z->deg = 1;
	z->c[0] = 0;
	z->c[1] = bch->a_pow_tab[k];

	out->deg = 0;
	memset(out, 0, GF_POLY_SZ(f->deg));

	
	gf_poly_logrep(bch, f, bch->cache);

	for (i = 0; i < m; i++) {
		
		for (j = z->deg; j >= 0; j--) {
			out->c[j] ^= z->c[j];
			z->c[2*j] = gf_sqr(bch, z->c[j]);
			z->c[2*j+1] = 0;
		}
		if (z->deg > out->deg)
			out->deg = z->deg;

		if (i < m-1) {
			z->deg *= 2;
			
			gf_poly_mod(bch, z, f, bch->cache);
		}
	}
	while (!out->c[out->deg] && out->deg)
		out->deg--;

	dbg("Tr(a^%d.X) mod f = %s\n", k, gf_poly_str(out));
}

static void factor_polynomial(struct bch_control *bch, int k, struct gf_poly *f,
			      struct gf_poly **g, struct gf_poly **h)
{
	struct gf_poly *f2 = bch->poly_2t[0];
	struct gf_poly *q  = bch->poly_2t[1];
	struct gf_poly *tk = bch->poly_2t[2];
	struct gf_poly *z  = bch->poly_2t[3];
	struct gf_poly *gcd;

	dbg("factoring %s...\n", gf_poly_str(f));

	*g = f;
	*h = NULL;

	
	compute_trace_bk_mod(bch, k, f, z, tk);

	if (tk->deg > 0) {
		
		gf_poly_copy(f2, f);
		gcd = gf_poly_gcd(bch, f2, tk);
		if (gcd->deg < f->deg) {
			
			gf_poly_div(bch, f, gcd, q);
			
			*h = &((struct gf_poly_deg1 *)f)[gcd->deg].poly;
			gf_poly_copy(*g, gcd);
			gf_poly_copy(*h, q);
		}
	}
}

static int find_poly_roots(struct bch_control *bch, unsigned int k,
			   struct gf_poly *poly, unsigned int *roots)
{
	int cnt;
	struct gf_poly *f1, *f2;

	switch (poly->deg) {
		
	case 1:
		cnt = find_poly_deg1_roots(bch, poly, roots);
		break;
	case 2:
		cnt = find_poly_deg2_roots(bch, poly, roots);
		break;
	case 3:
		cnt = find_poly_deg3_roots(bch, poly, roots);
		break;
	case 4:
		cnt = find_poly_deg4_roots(bch, poly, roots);
		break;
	default:
		
		cnt = 0;
		if (poly->deg && (k <= GF_M(bch))) {
			factor_polynomial(bch, k, poly, &f1, &f2);
			if (f1)
				cnt += find_poly_roots(bch, k+1, f1, roots);
			if (f2)
				cnt += find_poly_roots(bch, k+1, f2, roots+cnt);
		}
		break;
	}
	return cnt;
}

#if defined(USE_CHIEN_SEARCH)
static int chien_search(struct bch_control *bch, unsigned int len,
			struct gf_poly *p, unsigned int *roots)
{
	int m;
	unsigned int i, j, syn, syn0, count = 0;
	const unsigned int k = 8*len+bch->ecc_bits;

	
	gf_poly_logrep(bch, p, bch->cache);
	bch->cache[p->deg] = 0;
	syn0 = gf_div(bch, p->c[0], p->c[p->deg]);

	for (i = GF_N(bch)-k+1; i <= GF_N(bch); i++) {
		
		for (j = 1, syn = syn0; j <= p->deg; j++) {
			m = bch->cache[j];
			if (m >= 0)
				syn ^= a_pow(bch, m+j*i);
		}
		if (syn == 0) {
			roots[count++] = GF_N(bch)-i;
			if (count == p->deg)
				break;
		}
	}
	return (count == p->deg) ? count : 0;
}
#define find_poly_roots(_p, _k, _elp, _loc) chien_search(_p, len, _elp, _loc)
#endif 

int decode_bch(struct bch_control *bch, const uint8_t *data, unsigned int len,
	       const uint8_t *recv_ecc, const uint8_t *calc_ecc,
	       const unsigned int *syn, unsigned int *errloc)
{
	const unsigned int ecc_words = BCH_ECC_WORDS(bch);
	unsigned int nbits;
	int i, err, nroots;
	uint32_t sum;

	
	if (8*len > (bch->n-bch->ecc_bits))
		return -EINVAL;

	
	if (!syn) {
		if (!calc_ecc) {
			
			if (!data || !recv_ecc)
				return -EINVAL;
			encode_bch(bch, data, len, NULL);
		} else {
			
			load_ecc8(bch, bch->ecc_buf, calc_ecc);
		}
		
		if (recv_ecc) {
			load_ecc8(bch, bch->ecc_buf2, recv_ecc);
			
			for (i = 0, sum = 0; i < (int)ecc_words; i++) {
				bch->ecc_buf[i] ^= bch->ecc_buf2[i];
				sum |= bch->ecc_buf[i];
			}
			if (!sum)
				
				return 0;
		}
		compute_syndromes(bch, bch->ecc_buf, bch->syn);
		syn = bch->syn;
	}

	err = compute_error_locator_polynomial(bch, syn);
	if (err > 0) {
		nroots = find_poly_roots(bch, 1, bch->elp, errloc);
		if (err != nroots)
			err = -1;
	}
	if (err > 0) {
		
		nbits = (len*8)+bch->ecc_bits;
		for (i = 0; i < err; i++) {
			if (errloc[i] >= nbits) {
				err = -1;
				break;
			}
			errloc[i] = nbits-1-errloc[i];
			errloc[i] = (errloc[i] & ~7)|(7-(errloc[i] & 7));
		}
	}
	return (err >= 0) ? err : -EBADMSG;
}
EXPORT_SYMBOL_GPL(decode_bch);

static int build_gf_tables(struct bch_control *bch, unsigned int poly)
{
	unsigned int i, x = 1;
	const unsigned int k = 1 << deg(poly);

	
	if (k != (1u << GF_M(bch)))
		return -1;

	for (i = 0; i < GF_N(bch); i++) {
		bch->a_pow_tab[i] = x;
		bch->a_log_tab[x] = i;
		if (i && (x == 1))
			
			return -1;
		x <<= 1;
		if (x & k)
			x ^= poly;
	}
	bch->a_pow_tab[GF_N(bch)] = 1;
	bch->a_log_tab[0] = 0;

	return 0;
}

static void build_mod8_tables(struct bch_control *bch, const uint32_t *g)
{
	int i, j, b, d;
	uint32_t data, hi, lo, *tab;
	const int l = BCH_ECC_WORDS(bch);
	const int plen = DIV_ROUND_UP(bch->ecc_bits+1, 32);
	const int ecclen = DIV_ROUND_UP(bch->ecc_bits, 32);

	memset(bch->mod8_tab, 0, 4*256*l*sizeof(*bch->mod8_tab));

	for (i = 0; i < 256; i++) {
		
		for (b = 0; b < 4; b++) {
			
			tab = bch->mod8_tab + (b*256+i)*l;
			data = i << (8*b);
			while (data) {
				d = deg(data);
				
				data ^= g[0] >> (31-d);
				for (j = 0; j < ecclen; j++) {
					hi = (d < 31) ? g[j] << (d+1) : 0;
					lo = (j+1 < plen) ?
						g[j+1] >> (31-d) : 0;
					tab[j] ^= hi|lo;
				}
			}
		}
	}
}

static int build_deg2_base(struct bch_control *bch)
{
	const int m = GF_M(bch);
	int i, j, r;
	unsigned int sum, x, y, remaining, ak = 0, xi[m];

	
	for (i = 0; i < m; i++) {
		for (j = 0, sum = 0; j < m; j++)
			sum ^= a_pow(bch, i*(1 << j));

		if (sum) {
			ak = bch->a_pow_tab[i];
			break;
		}
	}
	
	remaining = m;
	memset(xi, 0, sizeof(xi));

	for (x = 0; (x <= GF_N(bch)) && remaining; x++) {
		y = gf_sqr(bch, x)^x;
		for (i = 0; i < 2; i++) {
			r = a_log(bch, y);
			if (y && (r < m) && !xi[r]) {
				bch->xi_tab[r] = x;
				xi[r] = 1;
				remaining--;
				dbg("x%d = %x\n", r, x);
				break;
			}
			y ^= ak;
		}
	}
	
	return remaining ? -1 : 0;
}

static void *bch_alloc(size_t size, int *err)
{
	void *ptr;

	ptr = kmalloc(size, GFP_KERNEL);
	if (ptr == NULL)
		*err = 1;
	return ptr;
}

static uint32_t *compute_generator_polynomial(struct bch_control *bch)
{
	const unsigned int m = GF_M(bch);
	const unsigned int t = GF_T(bch);
	int n, err = 0;
	unsigned int i, j, nbits, r, word, *roots;
	struct gf_poly *g;
	uint32_t *genpoly;

	g = bch_alloc(GF_POLY_SZ(m*t), &err);
	roots = bch_alloc((bch->n+1)*sizeof(*roots), &err);
	genpoly = bch_alloc(DIV_ROUND_UP(m*t+1, 32)*sizeof(*genpoly), &err);

	if (err) {
		kfree(genpoly);
		genpoly = NULL;
		goto finish;
	}

	
	memset(roots , 0, (bch->n+1)*sizeof(*roots));
	for (i = 0; i < t; i++) {
		for (j = 0, r = 2*i+1; j < m; j++) {
			roots[r] = 1;
			r = mod_s(bch, 2*r);
		}
	}
	
	g->deg = 0;
	g->c[0] = 1;
	for (i = 0; i < GF_N(bch); i++) {
		if (roots[i]) {
			
			r = bch->a_pow_tab[i];
			g->c[g->deg+1] = 1;
			for (j = g->deg; j > 0; j--)
				g->c[j] = gf_mul(bch, g->c[j], r)^g->c[j-1];

			g->c[0] = gf_mul(bch, g->c[0], r);
			g->deg++;
		}
	}
	
	n = g->deg+1;
	i = 0;

	while (n > 0) {
		nbits = (n > 32) ? 32 : n;
		for (j = 0, word = 0; j < nbits; j++) {
			if (g->c[n-1-j])
				word |= 1u << (31-j);
		}
		genpoly[i++] = word;
		n -= nbits;
	}
	bch->ecc_bits = g->deg;

finish:
	kfree(g);
	kfree(roots);

	return genpoly;
}

struct bch_control *init_bch(int m, int t, unsigned int prim_poly)
{
	int err = 0;
	unsigned int i, words;
	uint32_t *genpoly;
	struct bch_control *bch = NULL;

	const int min_m = 5;
	const int max_m = 15;

	
	static const unsigned int prim_poly_tab[] = {
		0x25, 0x43, 0x83, 0x11d, 0x211, 0x409, 0x805, 0x1053, 0x201b,
		0x402b, 0x8003,
	};

#if defined(CONFIG_BCH_CONST_PARAMS)
	if ((m != (CONFIG_BCH_CONST_M)) || (t != (CONFIG_BCH_CONST_T))) {
		printk(KERN_ERR "bch encoder/decoder was configured to support "
		       "parameters m=%d, t=%d only!\n",
		       CONFIG_BCH_CONST_M, CONFIG_BCH_CONST_T);
		goto fail;
	}
#endif
	if ((m < min_m) || (m > max_m))
		goto fail;

	
	if ((t < 1) || (m*t >= ((1 << m)-1)))
		
		goto fail;

	
	if (prim_poly == 0)
		prim_poly = prim_poly_tab[m-min_m];

	bch = kzalloc(sizeof(*bch), GFP_KERNEL);
	if (bch == NULL)
		goto fail;

	bch->m = m;
	bch->t = t;
	bch->n = (1 << m)-1;
	words  = DIV_ROUND_UP(m*t, 32);
	bch->ecc_bytes = DIV_ROUND_UP(m*t, 8);
	bch->a_pow_tab = bch_alloc((1+bch->n)*sizeof(*bch->a_pow_tab), &err);
	bch->a_log_tab = bch_alloc((1+bch->n)*sizeof(*bch->a_log_tab), &err);
	bch->mod8_tab  = bch_alloc(words*1024*sizeof(*bch->mod8_tab), &err);
	bch->ecc_buf   = bch_alloc(words*sizeof(*bch->ecc_buf), &err);
	bch->ecc_buf2  = bch_alloc(words*sizeof(*bch->ecc_buf2), &err);
	bch->xi_tab    = bch_alloc(m*sizeof(*bch->xi_tab), &err);
	bch->syn       = bch_alloc(2*t*sizeof(*bch->syn), &err);
	bch->cache     = bch_alloc(2*t*sizeof(*bch->cache), &err);
	bch->elp       = bch_alloc((t+1)*sizeof(struct gf_poly_deg1), &err);

	for (i = 0; i < ARRAY_SIZE(bch->poly_2t); i++)
		bch->poly_2t[i] = bch_alloc(GF_POLY_SZ(2*t), &err);

	if (err)
		goto fail;

	err = build_gf_tables(bch, prim_poly);
	if (err)
		goto fail;

	
	genpoly = compute_generator_polynomial(bch);
	if (genpoly == NULL)
		goto fail;

	build_mod8_tables(bch, genpoly);
	kfree(genpoly);

	err = build_deg2_base(bch);
	if (err)
		goto fail;

	return bch;

fail:
	free_bch(bch);
	return NULL;
}
EXPORT_SYMBOL_GPL(init_bch);

void free_bch(struct bch_control *bch)
{
	unsigned int i;

	if (bch) {
		kfree(bch->a_pow_tab);
		kfree(bch->a_log_tab);
		kfree(bch->mod8_tab);
		kfree(bch->ecc_buf);
		kfree(bch->ecc_buf2);
		kfree(bch->xi_tab);
		kfree(bch->syn);
		kfree(bch->cache);
		kfree(bch->elp);

		for (i = 0; i < ARRAY_SIZE(bch->poly_2t); i++)
			kfree(bch->poly_2t[i]);

		kfree(bch);
	}
}
EXPORT_SYMBOL_GPL(free_bch);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Djelic <ivan.djelic@parrot.com>");
MODULE_DESCRIPTION("Binary BCH encoder/decoder");
