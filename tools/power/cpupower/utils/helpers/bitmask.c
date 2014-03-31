#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <helpers/bitmask.h>

#define bitsperlong (8 * sizeof(unsigned long))

#define howmany(x, y) (((x)+((y)-1))/(y))

#define longsperbits(n) howmany(n, bitsperlong)

#define max(a, b) ((a) > (b) ? (a) : (b))


struct bitmask *bitmask_alloc(unsigned int n)
{
	struct bitmask *bmp;

	bmp = malloc(sizeof(*bmp));
	if (bmp == 0)
		return 0;
	bmp->size = n;
	bmp->maskp = calloc(longsperbits(n), sizeof(unsigned long));
	if (bmp->maskp == 0) {
		free(bmp);
		return 0;
	}
	return bmp;
}

void bitmask_free(struct bitmask *bmp)
{
	if (bmp == 0)
		return;
	free(bmp->maskp);
	bmp->maskp = (unsigned long *)0xdeadcdef;  
	free(bmp);
}


static unsigned int _getbit(const struct bitmask *bmp, unsigned int n)
{
	if (n < bmp->size)
		return (bmp->maskp[n/bitsperlong] >> (n % bitsperlong)) & 1;
	else
		return 0;
}

static void _setbit(struct bitmask *bmp, unsigned int n, unsigned int v)
{
	if (n < bmp->size) {
		if (v)
			bmp->maskp[n/bitsperlong] |= 1UL << (n % bitsperlong);
		else
			bmp->maskp[n/bitsperlong] &=
				~(1UL << (n % bitsperlong));
	}
}

static int scan_was_ok(int sret, char nextc, const char *ok_next_chars)
{
	return sret == 1 ||
		(sret == 2 && strchr(ok_next_chars, nextc) != NULL);
}

static const char *nexttoken(const char *q,  int sep)
{
	if (q)
		q = strchr(q, sep);
	if (q)
		q++;
	return q;
}

struct bitmask *bitmask_setbit(struct bitmask *bmp, unsigned int i)
{
	_setbit(bmp, i, 1);
	return bmp;
}

struct bitmask *bitmask_setall(struct bitmask *bmp)
{
	unsigned int i;
	for (i = 0; i < bmp->size; i++)
		_setbit(bmp, i, 1);
	return bmp;
}

struct bitmask *bitmask_clearall(struct bitmask *bmp)
{
	unsigned int i;
	for (i = 0; i < bmp->size; i++)
		_setbit(bmp, i, 0);
	return bmp;
}

int bitmask_isallclear(const struct bitmask *bmp)
{
	unsigned int i;
	for (i = 0; i < bmp->size; i++)
		if (_getbit(bmp, i))
			return 0;
	return 1;
}

int bitmask_isbitset(const struct bitmask *bmp, unsigned int i)
{
	return _getbit(bmp, i);
}

unsigned int bitmask_first(const struct bitmask *bmp)
{
	return bitmask_next(bmp, 0);
}

unsigned int bitmask_last(const struct bitmask *bmp)
{
	unsigned int i;
	unsigned int m = bmp->size;
	for (i = 0; i < bmp->size; i++)
		if (_getbit(bmp, i))
			m = i;
	return m;
}

unsigned int bitmask_next(const struct bitmask *bmp, unsigned int i)
{
	unsigned int n;
	for (n = i; n < bmp->size; n++)
		if (_getbit(bmp, n))
			break;
	return n;
}

int bitmask_parselist(const char *buf, struct bitmask *bmp)
{
	const char *p, *q;

	bitmask_clearall(bmp);

	q = buf;
	while (p = q, q = nexttoken(q, ','), p) {
		unsigned int a;		
		unsigned int b;		
		unsigned int s;		
		const char *c1, *c2;	
		char nextc;		
		int sret;		

		sret = sscanf(p, "%u%c", &a, &nextc);
		if (!scan_was_ok(sret, nextc, ",-"))
			goto err;
		b = a;
		s = 1;
		c1 = nexttoken(p, '-');
		c2 = nexttoken(p, ',');
		if (c1 != NULL && (c2 == NULL || c1 < c2)) {
			sret = sscanf(c1, "%u%c", &b, &nextc);
			if (!scan_was_ok(sret, nextc, ",:"))
				goto err;
			c1 = nexttoken(c1, ':');
			if (c1 != NULL && (c2 == NULL || c1 < c2)) {
				sret = sscanf(c1, "%u%c", &s, &nextc);
				if (!scan_was_ok(sret, nextc, ","))
					goto err;
			}
		}
		if (!(a <= b))
			goto err;
		if (b >= bmp->size)
			goto err;
		while (a <= b) {
			_setbit(bmp, a, 1);
			a += s;
		}
	}
	return 0;
err:
	bitmask_clearall(bmp);
	return -1;
}

/*
 * emit(buf, buflen, rbot, rtop, len)
 *
 * Helper routine for bitmask_displaylist().  Write decimal number
 * or range to buf+len, suppressing output past buf+buflen, with optional
 * comma-prefix.  Return len of what would be written to buf, if it
 * all fit.
 */

static inline int emit(char *buf, int buflen, int rbot, int rtop, int len)
{
	if (len > 0)
		len += snprintf(buf + len, max(buflen - len, 0), ",");
	if (rbot == rtop)
		len += snprintf(buf + len, max(buflen - len, 0), "%d", rbot);
	else
		len += snprintf(buf + len, max(buflen - len, 0), "%d-%d",
				rbot, rtop);
	return len;
}


int bitmask_displaylist(char *buf, int buflen, const struct bitmask *bmp)
{
	int len = 0;
	
	unsigned int cur, rbot, rtop;

	if (buflen > 0)
		*buf = 0;
	rbot = cur = bitmask_first(bmp);
	while (cur < bmp->size) {
		rtop = cur;
		cur = bitmask_next(bmp, cur+1);
		if (cur >= bmp->size || cur > rtop + 1) {
			len = emit(buf, buflen, rbot, rtop, len);
			rbot = cur;
		}
	}
	return len;
}
