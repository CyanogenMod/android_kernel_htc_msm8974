#ifndef _LINUX_UML_INIT_H
#define _LINUX_UML_INIT_H


#ifndef _LINUX_INIT_H
typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);

#ifndef __KERNEL__
#ifndef __section
# define __section(S) __attribute__ ((__section__(#S)))
#endif

#if __GNUC__ == 3

#if __GNUC_MINOR__ >= 3
# define __used			__attribute__((__used__))
#else
# define __used			__attribute__((__unused__))
#endif

#else
#if __GNUC__ == 4
# define __used			__attribute__((__used__))
#endif
#endif

#else
#include <linux/compiler.h>
#endif
#define __init		__section(.init.text)
#define __initdata	__section(.init.data)
#define __exitdata	__section(.exit.data)
#define __exit_call	__used __section(.exitcall.exit)

#ifdef MODULE
#define __exit		__section(.exit.text)
#else
#define __exit		__used __section(.exit.text)
#endif

#endif

#ifndef MODULE
struct uml_param {
        const char *str;
        int (*setup_func)(char *, int *);
};

extern initcall_t __uml_initcall_start, __uml_initcall_end;
extern initcall_t __uml_postsetup_start, __uml_postsetup_end;
extern const char *__uml_help_start, *__uml_help_end;
#endif

#define __uml_initcall(fn)					  	\
	static initcall_t __uml_initcall_##fn __uml_init_call = fn

#define __uml_exitcall(fn)						\
	static exitcall_t __uml_exitcall_##fn __uml_exit_call = fn

extern struct uml_param __uml_setup_start, __uml_setup_end;

#define __uml_postsetup(fn)						\
	static initcall_t __uml_postsetup_##fn __uml_postsetup_call = fn

#define __non_empty_string(dummyname,string)				\
	struct __uml_non_empty_string_struct_##dummyname		\
	{								\
		char _string[sizeof(string)-2];				\
	}

#ifndef MODULE
#define __uml_setup(str, fn, help...)					\
	__non_empty_string(fn ##_setup, str);				\
	__uml_help(fn, help);						\
	static char __uml_setup_str_##fn[] __initdata = str;		\
	static struct uml_param __uml_setup_##fn __uml_init_setup = { __uml_setup_str_##fn, fn }
#else
#define __uml_setup(str, fn, help...)					\

#endif

#define __uml_help(fn, help...)						\
	__non_empty_string(fn ##__help, help);				\
	static char __uml_help_str_##fn[] __initdata = help;		\
	static const char *__uml_help_##fn __uml_setup_help = __uml_help_str_##fn

#define __uml_init_setup	__used __section(.uml.setup.init)
#define __uml_setup_help	__used __section(.uml.help.init)
#define __uml_init_call		__used __section(.uml.initcall.init)
#define __uml_postsetup_call	__used __section(.uml.postsetup.init)
#define __uml_exit_call		__used __section(.uml.exitcall.exit)

#ifndef __KERNEL__

#define __define_initcall(level,fn) \
	static initcall_t __initcall_##fn __used \
	__attribute__((__section__(".initcall" level ".init"))) = fn

#define __initcall(fn) __define_initcall("1", fn)

#define __exitcall(fn) static exitcall_t __exitcall_##fn __exit_call = fn

#define __init_call	__used __section(.initcall.init)

#endif

#endif 
