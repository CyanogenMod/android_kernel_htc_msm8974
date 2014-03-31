
#include <linux/mm.h>
#include <linux/sysctl.h>

#ifndef CONFIG_SYSCTL
#error This file should not be compiled without CONFIG_SYSCTL defined
#endif

extern int sysctl_ipx_pprop_broadcasting;

static struct ctl_table ipx_table[] = {
	{
		.procname	= "ipx_pprop_broadcasting",
		.data		= &sysctl_ipx_pprop_broadcasting,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{ },
};

static struct ctl_path ipx_path[] = {
	{ .procname = "net", },
	{ .procname = "ipx", },
	{ }
};

static struct ctl_table_header *ipx_table_header;

void ipx_register_sysctl(void)
{
	ipx_table_header = register_sysctl_paths(ipx_path, ipx_table);
}

void ipx_unregister_sysctl(void)
{
	unregister_sysctl_table(ipx_table_header);
}
