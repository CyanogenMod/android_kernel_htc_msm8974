
#ifndef XZ_H
#define XZ_H

#ifdef __KERNEL__
#	include <linux/stddef.h>
#	include <linux/types.h>
#else
#	include <stddef.h>
#	include <stdint.h>
#endif

#ifndef XZ_EXTERN
#	define XZ_EXTERN extern
#endif

enum xz_mode {
	XZ_SINGLE,
	XZ_PREALLOC,
	XZ_DYNALLOC
};

enum xz_ret {
	XZ_OK,
	XZ_STREAM_END,
	XZ_UNSUPPORTED_CHECK,
	XZ_MEM_ERROR,
	XZ_MEMLIMIT_ERROR,
	XZ_FORMAT_ERROR,
	XZ_OPTIONS_ERROR,
	XZ_DATA_ERROR,
	XZ_BUF_ERROR
};

struct xz_buf {
	const uint8_t *in;
	size_t in_pos;
	size_t in_size;

	uint8_t *out;
	size_t out_pos;
	size_t out_size;
};

struct xz_dec;

XZ_EXTERN struct xz_dec *xz_dec_init(enum xz_mode mode, uint32_t dict_max);

XZ_EXTERN enum xz_ret xz_dec_run(struct xz_dec *s, struct xz_buf *b);

XZ_EXTERN void xz_dec_reset(struct xz_dec *s);

XZ_EXTERN void xz_dec_end(struct xz_dec *s);

#ifndef XZ_INTERNAL_CRC32
#	ifdef __KERNEL__
#		define XZ_INTERNAL_CRC32 0
#	else
#		define XZ_INTERNAL_CRC32 1
#	endif
#endif

#if XZ_INTERNAL_CRC32
XZ_EXTERN void xz_crc32_init(void);

XZ_EXTERN uint32_t xz_crc32(const uint8_t *buf, size_t size, uint32_t crc);
#endif
#endif
