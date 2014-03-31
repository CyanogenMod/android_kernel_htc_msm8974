
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include <wait.h>


#include <sys/prctl.h>
#include <linux/prctl.h>

#ifndef PR_GET_TSC
#define PR_GET_TSC 25
#define PR_SET_TSC 26
# define PR_TSC_ENABLE		1   
# define PR_TSC_SIGSEGV		2   
#endif


uint64_t rdtsc() {
uint32_t lo, hi;
__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
return (uint64_t)hi << 32 | lo;
}

int should_segv = 0;

void sigsegv_cb(int sig)
{
	if (!should_segv)
	{
		fprintf(stderr, "FATAL ERROR, rdtsc() failed while enabled\n");
		exit(0);
	}
	if (prctl(PR_SET_TSC, PR_TSC_ENABLE) < 0)
	{
		perror("prctl");
		exit(0);
	}
	should_segv = 0;

	rdtsc();
}

void task(void)
{
	signal(SIGSEGV, sigsegv_cb);
	alarm(10);
	for(;;)
	{
		rdtsc();
		if (should_segv)
		{
			fprintf(stderr, "FATAL ERROR, rdtsc() succeeded while disabled\n");
			exit(0);
		}
		if (prctl(PR_SET_TSC, PR_TSC_SIGSEGV) < 0)
		{
			perror("prctl");
			exit(0);
		}
		should_segv = 1;
	}
}


int main(int argc, char **argv)
{
	int n_tasks = 100, i;

	fprintf(stderr, "[No further output means we're allright]\n");

	for (i=0; i<n_tasks; i++)
		if (fork() == 0)
			task();

	for (i=0; i<n_tasks; i++)
		wait(NULL);

	exit(0);
}

