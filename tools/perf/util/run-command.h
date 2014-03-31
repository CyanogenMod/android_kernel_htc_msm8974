#ifndef __PERF_RUN_COMMAND_H
#define __PERF_RUN_COMMAND_H

enum {
	ERR_RUN_COMMAND_FORK = 10000,
	ERR_RUN_COMMAND_EXEC,
	ERR_RUN_COMMAND_PIPE,
	ERR_RUN_COMMAND_WAITPID,
	ERR_RUN_COMMAND_WAITPID_WRONG_PID,
	ERR_RUN_COMMAND_WAITPID_SIGNAL,
	ERR_RUN_COMMAND_WAITPID_NOEXIT,
};
#define IS_RUN_COMMAND_ERR(x) (-(x) >= ERR_RUN_COMMAND_FORK)

struct child_process {
	const char **argv;
	pid_t pid;
	int in;
	int out;
	int err;
	const char *dir;
	const char *const *env;
	unsigned no_stdin:1;
	unsigned no_stdout:1;
	unsigned no_stderr:1;
	unsigned perf_cmd:1; 
	unsigned stdout_to_stderr:1;
	void (*preexec_cb)(void);
};

int start_command(struct child_process *);
int finish_command(struct child_process *);
int run_command(struct child_process *);

#define RUN_COMMAND_NO_STDIN 1
#define RUN_PERF_CMD	     2	
#define RUN_COMMAND_STDOUT_TO_STDERR 4
int run_command_v_opt(const char **argv, int opt);

#endif 
