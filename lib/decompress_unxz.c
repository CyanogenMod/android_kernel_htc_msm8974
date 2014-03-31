

#ifdef STATIC
#	define XZ_PREBOOT
#endif
#ifdef __KERNEL__
#	include <linux/decompress/mm.h>
#endif
#define XZ_EXTERN STATIC

#ifndef XZ_PREBOOT
#	include <linux/slab.h>
#	include <linux/xz.h>
#else
#define XZ_INTERNAL_CRC32 1

#ifdef CONFIG_X86
#	define XZ_DEC_X86
#endif
#ifdef CONFIG_PPC
#	define XZ_DEC_POWERPC
#endif
#ifdef CONFIG_ARM
#	define XZ_DEC_ARM
#endif
#ifdef CONFIG_IA64
#	define XZ_DEC_IA64
#endif
#ifdef CONFIG_SPARC
#	define XZ_DEC_SPARC
#endif

#include "xz/xz_private.h"

#undef kmalloc
#undef kfree
#undef vmalloc
#undef vfree
#define kmalloc(size, flags) malloc(size)
#define kfree(ptr) free(ptr)
#define vmalloc(size) malloc(size)
#define vfree(ptr) do { if (ptr != NULL) free(ptr); } while (0)


#ifndef memeq
static bool memeq(const void *a, const void *b, size_t size)
{
	const uint8_t *x = a;
	const uint8_t *y = b;
	size_t i;

	for (i = 0; i < size; ++i)
		if (x[i] != y[i])
			return false;

	return true;
}
#endif

#ifndef memzero
static void memzero(void *buf, size_t size)
{
	uint8_t *b = buf;
	uint8_t *e = b + size;

	while (b != e)
		*b++ = '\0';
}
#endif

#ifndef memmove
void *memmove(void *dest, const void *src, size_t size)
{
	uint8_t *d = dest;
	const uint8_t *s = src;
	size_t i;

	if (d < s) {
		for (i = 0; i < size; ++i)
			d[i] = s[i];
	} else if (d > s) {
		i = size;
		while (i-- > 0)
			d[i] = s[i];
	}

	return dest;
}
#endif


#include "xz/xz_crc32.c"
#include "xz/xz_dec_stream.c"
#include "xz/xz_dec_lzma2.c"
#include "xz/xz_dec_bcj.c"

#endif 

#define XZ_IOBUF_SIZE 4096

STATIC int INIT unxz(unsigned char *in, int in_size,
		     int (*fill)(void *dest, unsigned int size),
		     int (*flush)(void *src, unsigned int size),
		     unsigned char *out, int *in_used,
		     void (*error)(char *x))
{
	struct xz_buf b;
	struct xz_dec *s;
	enum xz_ret ret;
	bool must_free_in = false;

#if XZ_INTERNAL_CRC32
	xz_crc32_init();
#endif

	if (in_used != NULL)
		*in_used = 0;

	if (fill == NULL && flush == NULL)
		s = xz_dec_init(XZ_SINGLE, 0);
	else
		s = xz_dec_init(XZ_DYNALLOC, (uint32_t)-1);

	if (s == NULL)
		goto error_alloc_state;

	if (flush == NULL) {
		b.out = out;
		b.out_size = (size_t)-1;
	} else {
		b.out_size = XZ_IOBUF_SIZE;
		b.out = malloc(XZ_IOBUF_SIZE);
		if (b.out == NULL)
			goto error_alloc_out;
	}

	if (in == NULL) {
		must_free_in = true;
		in = malloc(XZ_IOBUF_SIZE);
		if (in == NULL)
			goto error_alloc_in;
	}

	b.in = in;
	b.in_pos = 0;
	b.in_size = in_size;
	b.out_pos = 0;

	if (fill == NULL && flush == NULL) {
		ret = xz_dec_run(s, &b);
	} else {
		do {
			if (b.in_pos == b.in_size && fill != NULL) {
				if (in_used != NULL)
					*in_used += b.in_pos;

				b.in_pos = 0;

				in_size = fill(in, XZ_IOBUF_SIZE);
				if (in_size < 0) {
					ret = XZ_BUF_ERROR;
					break;
				}

				b.in_size = in_size;
			}

			ret = xz_dec_run(s, &b);

			if (flush != NULL && (b.out_pos == b.out_size
					|| (ret != XZ_OK && b.out_pos > 0))) {
				if (flush(b.out, b.out_pos) != (int)b.out_pos)
					ret = XZ_BUF_ERROR;

				b.out_pos = 0;
			}
		} while (ret == XZ_OK);

		if (must_free_in)
			free(in);

		if (flush != NULL)
			free(b.out);
	}

	if (in_used != NULL)
		*in_used += b.in_pos;

	xz_dec_end(s);

	switch (ret) {
	case XZ_STREAM_END:
		return 0;

	case XZ_MEM_ERROR:
		
		error("XZ decompressor ran out of memory");
		break;

	case XZ_FORMAT_ERROR:
		error("Input is not in the XZ format (wrong magic bytes)");
		break;

	case XZ_OPTIONS_ERROR:
		error("Input was encoded with settings that are not "
				"supported by this XZ decoder");
		break;

	case XZ_DATA_ERROR:
	case XZ_BUF_ERROR:
		error("XZ-compressed data is corrupt");
		break;

	default:
		error("Bug in the XZ decompressor");
		break;
	}

	return -1;

error_alloc_in:
	if (flush != NULL)
		free(b.out);

error_alloc_out:
	xz_dec_end(s);

error_alloc_state:
	error("XZ decompressor ran out of memory");
	return -1;
}

#define decompress unxz
