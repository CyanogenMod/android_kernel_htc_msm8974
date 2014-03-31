#ifndef __PERF_HELP_H
#define __PERF_HELP_H

struct cmdnames {
	size_t alloc;
	size_t cnt;
	struct cmdname {
		size_t len; 
		char name[FLEX_ARRAY];
	} **names;
};

static inline void mput_char(char c, unsigned int num)
{
	while(num--)
		putchar(c);
}

void load_command_list(const char *prefix,
		struct cmdnames *main_cmds,
		struct cmdnames *other_cmds);
void add_cmdname(struct cmdnames *cmds, const char *name, size_t len);
void exclude_cmds(struct cmdnames *cmds, struct cmdnames *excludes);
int is_in_cmdlist(struct cmdnames *c, const char *s);
void list_commands(const char *title, struct cmdnames *main_cmds,
		   struct cmdnames *other_cmds);

#endif 
