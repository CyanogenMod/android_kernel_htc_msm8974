/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include "as-layout.h"
#include "init.h"
#include "kern_util.h"
#include "os.h"
#include "um_malloc.h"

#define PGD_BOUND (4 * 1024 * 1024)
#define STACKSIZE (8 * 1024 * 1024)
#define THREAD_NAME_LEN (256)

long elf_aux_hwcap;

static void set_stklim(void)
{
	struct rlimit lim;

	if (getrlimit(RLIMIT_STACK, &lim) < 0) {
		perror("getrlimit");
		exit(1);
	}
	if ((lim.rlim_cur == RLIM_INFINITY) || (lim.rlim_cur > STACKSIZE)) {
		lim.rlim_cur = STACKSIZE;
		if (setrlimit(RLIMIT_STACK, &lim) < 0) {
			perror("setrlimit");
			exit(1);
		}
	}
}

static __init void do_uml_initcalls(void)
{
	initcall_t *call;

	call = &__uml_initcall_start;
	while (call < &__uml_initcall_end) {
		(*call)();
		call++;
	}
}

static void last_ditch_exit(int sig)
{
	uml_cleanup();
	exit(1);
}

static void install_fatal_handler(int sig)
{
	struct sigaction action;

	
	sigemptyset(&action.sa_mask);

	action.sa_flags = SA_RESETHAND | SA_NODEFER;
	action.sa_restorer = NULL;
	action.sa_handler = last_ditch_exit;
	if (sigaction(sig, &action, NULL) < 0) {
		printf("failed to install handler for signal %d - errno = %d\n",
		       sig, errno);
		exit(1);
	}
}

#define UML_LIB_PATH	":" OS_LIB_PATH "/uml"

static void setup_env_path(void)
{
	char *new_path = NULL;
	char *old_path = NULL;
	int path_len = 0;

	old_path = getenv("PATH");
	if (!old_path || (path_len = strlen(old_path)) == 0) {
		if (putenv("PATH=:/bin:/usr/bin/" UML_LIB_PATH))
			perror("couldn't putenv");
		return;
	}

	
	path_len += strlen("PATH=" UML_LIB_PATH) + 1;
	new_path = malloc(path_len);
	if (!new_path) {
		perror("couldn't malloc to set a new PATH");
		return;
	}
	snprintf(new_path, path_len, "PATH=%s" UML_LIB_PATH, old_path);
	if (putenv(new_path)) {
		perror("couldn't putenv to set a new PATH");
		free(new_path);
	}
}

extern void scan_elf_aux( char **envp);

int __init main(int argc, char **argv, char **envp)
{
	char **new_argv;
	int ret, i, err;

	set_stklim();

	setup_env_path();

	new_argv = malloc((argc + 1) * sizeof(char *));
	if (new_argv == NULL) {
		perror("Mallocing argv");
		exit(1);
	}
	for (i = 0; i < argc; i++) {
		new_argv[i] = strdup(argv[i]);
		if (new_argv[i] == NULL) {
			perror("Mallocing an arg");
			exit(1);
		}
	}
	new_argv[argc] = NULL;

	install_fatal_handler(SIGINT);
	install_fatal_handler(SIGTERM);

#ifdef CONFIG_ARCH_REUSE_HOST_VSYSCALL_AREA
	scan_elf_aux(envp);
#endif

	do_uml_initcalls();
	ret = linux_main(argc, argv);

	change_sig(SIGPROF, 0);


	
	disable_timer();

	
	err = deactivate_all_fds();
	if (err)
		printf("deactivate_all_fds failed, errno = %d\n", -err);

	unblock_signals();

	
	if (ret) {
		printf("\n");
		execvp(new_argv[0], new_argv);
		perror("Failed to exec kernel");
		ret = 1;
	}
	printf("\n");
	return uml_exitcode;
}

extern void *__real_malloc(int);

void *__wrap_malloc(int size)
{
	void *ret;

	if (!kmalloc_ok)
		return __real_malloc(size);
	else if (size <= UM_KERN_PAGE_SIZE)
		
		ret = uml_kmalloc(size, UM_GFP_KERNEL);
	else ret = vmalloc(size);

	if (ret == NULL)
		errno = ENOMEM;

	return ret;
}

void *__wrap_calloc(int n, int size)
{
	void *ptr = __wrap_malloc(n * size);

	if (ptr == NULL)
		return NULL;
	memset(ptr, 0, n * size);
	return ptr;
}

extern void __real_free(void *);

extern unsigned long high_physmem;

void __wrap_free(void *ptr)
{
	unsigned long addr = (unsigned long) ptr;


	if ((addr >= uml_physmem) && (addr < high_physmem)) {
		if (kmalloc_ok)
			kfree(ptr);
	}
	else if ((addr >= start_vm) && (addr < end_vm)) {
		if (kmalloc_ok)
			vfree(ptr);
	}
	else __real_free(ptr);
}
