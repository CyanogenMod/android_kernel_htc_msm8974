#include "cache.h"
#include "run-command.h"
#include "sigchain.h"


static int spawned_pager;

static void pager_preexec(void)
{
	fd_set in;

	FD_ZERO(&in);
	FD_SET(0, &in);
	select(1, &in, NULL, &in, NULL);

	setenv("LESS", "FRSX", 0);
}

static const char *pager_argv[] = { "sh", "-c", NULL, NULL };
static struct child_process pager_process;

static void wait_for_pager(void)
{
	fflush(stdout);
	fflush(stderr);
	
	close(1);
	close(2);
	finish_command(&pager_process);
}

static void wait_for_pager_signal(int signo)
{
	wait_for_pager();
	sigchain_pop(signo);
	raise(signo);
}

void setup_pager(void)
{
	const char *pager = getenv("PERF_PAGER");

	if (!isatty(1))
		return;
	if (!pager) {
		if (!pager_program)
			perf_config(perf_default_config, NULL);
		pager = pager_program;
	}
	if (!pager)
		pager = getenv("PAGER");
	if (!pager)
		pager = "less";
	else if (!*pager || !strcmp(pager, "cat"))
		return;

	spawned_pager = 1; 

	
	pager_argv[2] = pager;
	pager_process.argv = pager_argv;
	pager_process.in = -1;
	pager_process.preexec_cb = pager_preexec;

	if (start_command(&pager_process))
		return;

	
	dup2(pager_process.in, 1);
	if (isatty(2))
		dup2(pager_process.in, 2);
	close(pager_process.in);

	
	sigchain_push_common(wait_for_pager_signal);
	atexit(wait_for_pager);
}

int pager_in_use(void)
{
	const char *env;

	if (spawned_pager)
		return 1;

	env = getenv("PERF_PAGER_IN_USE");
	return env ? perf_config_bool("PERF_PAGER_IN_USE", env) : 0;
}
