/*
 * cifs_unicode:  Unicode kernel case support
 *
 * Function:
 *     Convert a unicode character to upper or lower case using
 *     compressed tables.
 *
 *   Copyright (c) International Business Machines  Corp., 2000,2009
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *
 * Notes:
 *     These APIs are based on the C library functions.  The semantics
 *     should match the C functions but with expanded size operands.
 *
 *     The upper/lower functions are based on a table created by mkupr.
 *     This is a compressed table of upper and lower case conversion.
 *
 */
#ifndef _CIFS_UNICODE_H
#define _CIFS_UNICODE_H

#include <asm/byteorder.h>
#include <linux/types.h>
#include <linux/nls.h>

#define  UNIUPR_NOLOWER		

#define UNI_ASTERISK    (__u16) ('*' + 0xF000)
#define UNI_QUESTION    (__u16) ('?' + 0xF000)
#define UNI_COLON       (__u16) (':' + 0xF000)
#define UNI_GRTRTHAN    (__u16) ('>' + 0xF000)
#define UNI_LESSTHAN    (__u16) ('<' + 0xF000)
#define UNI_PIPE        (__u16) ('|' + 0xF000)
#define UNI_SLASH       (__u16) ('\\' + 0xF000)

#ifndef	UNICASERANGE_DEFINED
struct UniCaseRange {
	wchar_t start;
	wchar_t end;
	signed char *table;
};
#endif				

#ifndef UNIUPR_NOUPPER
extern signed char CifsUniUpperTable[512];
extern const struct UniCaseRange CifsUniUpperRange[];
#endif				

#ifndef UNIUPR_NOLOWER
extern signed char CifsUniLowerTable[512];
extern const struct UniCaseRange CifsUniLowerRange[];
#endif				

#ifdef __KERNEL__
int cifs_from_utf16(char *to, const __le16 *from, int tolen, int fromlen,
		    const struct nls_table *codepage, bool mapchar);
int cifs_utf16_bytes(const __le16 *from, int maxbytes,
		     const struct nls_table *codepage);
int cifs_strtoUTF16(__le16 *, const char *, int, const struct nls_table *);
char *cifs_strndup_from_utf16(const char *src, const int maxlen,
			      const bool is_unicode,
			      const struct nls_table *codepage);
extern int cifsConvertToUTF16(__le16 *target, const char *source, int maxlen,
			      const struct nls_table *cp, int mapChars);

#endif

static inline wchar_t *
UniStrcat(wchar_t *ucs1, const wchar_t *ucs2)
{
	wchar_t *anchor = ucs1;	

	while (*ucs1++) ;	
	ucs1--;			
	while ((*ucs1++ = *ucs2++)) ;	
	return anchor;
}

static inline wchar_t *
UniStrchr(const wchar_t *ucs, wchar_t uc)
{
	while ((*ucs != uc) && *ucs)
		ucs++;

	if (*ucs == uc)
		return (wchar_t *) ucs;
	return NULL;
}

static inline int
UniStrcmp(const wchar_t *ucs1, const wchar_t *ucs2)
{
	while ((*ucs1 == *ucs2) && *ucs1) {
		ucs1++;
		ucs2++;
	}
	return (int) *ucs1 - (int) *ucs2;
}

static inline wchar_t *
UniStrcpy(wchar_t *ucs1, const wchar_t *ucs2)
{
	wchar_t *anchor = ucs1;	

	while ((*ucs1++ = *ucs2++)) ;
	return anchor;
}

static inline size_t
UniStrlen(const wchar_t *ucs1)
{
	int i = 0;

	while (*ucs1++)
		i++;
	return i;
}

static inline size_t
UniStrnlen(const wchar_t *ucs1, int maxlen)
{
	int i = 0;

	while (*ucs1++) {
		i++;
		if (i >= maxlen)
			break;
	}
	return i;
}

static inline wchar_t *
UniStrncat(wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	wchar_t *anchor = ucs1;	

	while (*ucs1++) ;
	ucs1--;			
	while (n-- && (*ucs1 = *ucs2)) {	
		ucs1++;
		ucs2++;
	}
	*ucs1 = 0;		
	return (anchor);
}

static inline int
UniStrncmp(const wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	if (!n)
		return 0;	
	while ((*ucs1 == *ucs2) && *ucs1 && --n) {
		ucs1++;
		ucs2++;
	}
	return (int) *ucs1 - (int) *ucs2;
}

static inline int
UniStrncmp_le(const wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	if (!n)
		return 0;	
	while ((*ucs1 == __le16_to_cpu(*ucs2)) && *ucs1 && --n) {
		ucs1++;
		ucs2++;
	}
	return (int) *ucs1 - (int) __le16_to_cpu(*ucs2);
}

static inline wchar_t *
UniStrncpy(wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	wchar_t *anchor = ucs1;

	while (n-- && *ucs2)	
		*ucs1++ = *ucs2++;

	n++;
	while (n--)		
		*ucs1++ = 0;
	return anchor;
}

static inline wchar_t *
UniStrncpy_le(wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	wchar_t *anchor = ucs1;

	while (n-- && *ucs2)	
		*ucs1++ = __le16_to_cpu(*ucs2++);

	n++;
	while (n--)		
		*ucs1++ = 0;
	return anchor;
}

static inline wchar_t *
UniStrstr(const wchar_t *ucs1, const wchar_t *ucs2)
{
	const wchar_t *anchor1 = ucs1;
	const wchar_t *anchor2 = ucs2;

	while (*ucs1) {
		if (*ucs1 == *ucs2) {
			
			ucs1++;
			ucs2++;
		} else {
			if (!*ucs2)	
				return (wchar_t *) anchor1;
			ucs1 = ++anchor1;	
			ucs2 = anchor2;
		}
	}

	if (!*ucs2)		
		return (wchar_t *) anchor1;	
	return NULL;		
}

#ifndef UNIUPR_NOUPPER
static inline wchar_t
UniToupper(register wchar_t uc)
{
	register const struct UniCaseRange *rp;

	if (uc < sizeof(CifsUniUpperTable)) {
		
		return uc + CifsUniUpperTable[uc];	
	} else {
		rp = CifsUniUpperRange;	
		while (rp->start) {
			if (uc < rp->start)	
				return uc;	
			if (uc <= rp->end)	
				return uc + rp->table[uc - rp->start];
			rp++;	
		}
	}
	return uc;		
}

static inline wchar_t *
UniStrupr(register wchar_t *upin)
{
	register wchar_t *up;

	up = upin;
	while (*up) {		
		*up = UniToupper(*up);
		up++;
	}
	return upin;		
}
#endif				

#ifndef UNIUPR_NOLOWER
static inline wchar_t
UniTolower(register wchar_t uc)
{
	register const struct UniCaseRange *rp;

	if (uc < sizeof(CifsUniLowerTable)) {
		
		return uc + CifsUniLowerTable[uc];	
	} else {
		rp = CifsUniLowerRange;	
		while (rp->start) {
			if (uc < rp->start)	
				return uc;	
			if (uc <= rp->end)	
				return uc + rp->table[uc - rp->start];
			rp++;	
		}
	}
	return uc;		
}

static inline wchar_t *
UniStrlwr(register wchar_t *upin)
{
	register wchar_t *up;

	up = upin;
	while (*up) {		
		*up = UniTolower(*up);
		up++;
	}
	return upin;		
}

#endif

#endif 
