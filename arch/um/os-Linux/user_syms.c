#include "linux/types.h"
#include "linux/module.h"


#undef strlen
#undef strstr
#undef memcpy
#undef memset

extern size_t strlen(const char *);
extern void *memmove(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
extern int printf(const char *, ...);

#ifdef __HAVE_ARCH_STRSTR
EXPORT_SYMBOL(strstr);
#endif

#ifndef __x86_64__
extern void *memcpy(void *, const void *, size_t);
EXPORT_SYMBOL(memcpy);
#endif

EXPORT_SYMBOL(memmove);
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(printf);

#define EXPORT_SYMBOL_PROTO(sym)       \
	int sym(void);                  \
	EXPORT_SYMBOL(sym);

extern void readdir64(void) __attribute__((weak));
EXPORT_SYMBOL(readdir64);
extern void truncate64(void) __attribute__((weak));
EXPORT_SYMBOL(truncate64);

#ifdef CONFIG_ARCH_REUSE_HOST_VSYSCALL_AREA
EXPORT_SYMBOL(vsyscall_ehdr);
EXPORT_SYMBOL(vsyscall_end);
#endif

EXPORT_SYMBOL_PROTO(__errno_location);

EXPORT_SYMBOL_PROTO(access);
EXPORT_SYMBOL_PROTO(open);
EXPORT_SYMBOL_PROTO(open64);
EXPORT_SYMBOL_PROTO(close);
EXPORT_SYMBOL_PROTO(read);
EXPORT_SYMBOL_PROTO(write);
EXPORT_SYMBOL_PROTO(dup2);
EXPORT_SYMBOL_PROTO(__xstat);
EXPORT_SYMBOL_PROTO(__lxstat);
EXPORT_SYMBOL_PROTO(__lxstat64);
EXPORT_SYMBOL_PROTO(__fxstat64);
EXPORT_SYMBOL_PROTO(lseek);
EXPORT_SYMBOL_PROTO(lseek64);
EXPORT_SYMBOL_PROTO(chown);
EXPORT_SYMBOL_PROTO(fchown);
EXPORT_SYMBOL_PROTO(truncate);
EXPORT_SYMBOL_PROTO(ftruncate64);
EXPORT_SYMBOL_PROTO(utime);
EXPORT_SYMBOL_PROTO(utimes);
EXPORT_SYMBOL_PROTO(futimes);
EXPORT_SYMBOL_PROTO(chmod);
EXPORT_SYMBOL_PROTO(fchmod);
EXPORT_SYMBOL_PROTO(rename);
EXPORT_SYMBOL_PROTO(__xmknod);

EXPORT_SYMBOL_PROTO(symlink);
EXPORT_SYMBOL_PROTO(link);
EXPORT_SYMBOL_PROTO(unlink);
EXPORT_SYMBOL_PROTO(readlink);

EXPORT_SYMBOL_PROTO(mkdir);
EXPORT_SYMBOL_PROTO(rmdir);
EXPORT_SYMBOL_PROTO(opendir);
EXPORT_SYMBOL_PROTO(readdir);
EXPORT_SYMBOL_PROTO(closedir);
EXPORT_SYMBOL_PROTO(seekdir);
EXPORT_SYMBOL_PROTO(telldir);

EXPORT_SYMBOL_PROTO(ioctl);

EXPORT_SYMBOL_PROTO(pread64);
EXPORT_SYMBOL_PROTO(pwrite64);

EXPORT_SYMBOL_PROTO(statfs);
EXPORT_SYMBOL_PROTO(statfs64);

EXPORT_SYMBOL_PROTO(getuid);

EXPORT_SYMBOL_PROTO(fsync);
EXPORT_SYMBOL_PROTO(fdatasync);

EXPORT_SYMBOL_PROTO(lstat64);
EXPORT_SYMBOL_PROTO(fstat64);
EXPORT_SYMBOL_PROTO(mknod);

extern void __stack_smash_handler(void *) __attribute__((weak));
EXPORT_SYMBOL(__stack_smash_handler);

extern long __guard __attribute__((weak));
EXPORT_SYMBOL(__guard);

#ifdef _FORTIFY_SOURCE
extern int __sprintf_chk(char *str, int flag, size_t strlen, const char *format);
EXPORT_SYMBOL(__sprintf_chk);
#endif
