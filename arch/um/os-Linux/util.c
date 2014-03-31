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
#include <termios.h>
#include <wait.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include "os.h"

void stack_protections(unsigned long address)
{
	if (mprotect((void *) address, UM_THREAD_SIZE,
		    PROT_READ | PROT_WRITE | PROT_EXEC) < 0)
		panic("protecting stack failed, errno = %d", errno);
}

int raw(int fd)
{
	struct termios tt;
	int err;

	CATCH_EINTR(err = tcgetattr(fd, &tt));
	if (err < 0)
		return -errno;

	cfmakeraw(&tt);

	CATCH_EINTR(err = tcsetattr(fd, TCSADRAIN, &tt));
	if (err < 0)
		return -errno;

	return 0;
}

void setup_machinename(char *machine_out)
{
	struct utsname host;

	uname(&host);
#ifdef UML_CONFIG_UML_X86
# ifndef UML_CONFIG_64BIT
	if (!strcmp(host.machine, "x86_64")) {
		strcpy(machine_out, "i686");
		return;
	}
# else
	if (!strcmp(host.machine, "i686")) {
		strcpy(machine_out, "x86_64");
		return;
	}
# endif
#endif
	strcpy(machine_out, host.machine);
}

void setup_hostinfo(char *buf, int len)
{
	struct utsname host;

	uname(&host);
	snprintf(buf, len, "%s %s %s %s %s", host.sysname, host.nodename,
		 host.release, host.version, host.machine);
}

static inline void __attribute__ ((noreturn)) uml_abort(void)
{
	sigset_t sig;

	fflush(NULL);

	if (!sigemptyset(&sig) && !sigaddset(&sig, SIGABRT))
		sigprocmask(SIG_UNBLOCK, &sig, 0);

	for (;;)
		if (kill(getpid(), SIGABRT) < 0)
			exit(127);
}

void os_dump_core(void)
{
	int pid;

	signal(SIGSEGV, SIG_DFL);


	signal(SIGTERM, SIG_IGN);
	kill(0, SIGTERM);
	kill(0, SIGCONT);


	while ((pid = waitpid(-1, NULL, WNOHANG | __WALL)) > 0)
		os_kill_ptraced_process(pid, 0);

	uml_abort();
}

void um_early_printk(const char *s, unsigned int n)
{
	printf("%.*s", n, s);
}
