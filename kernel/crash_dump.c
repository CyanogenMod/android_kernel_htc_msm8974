#include <linux/kernel.h>
#include <linux/crash_dump.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/export.h>

unsigned long saved_max_pfn;

unsigned long long elfcorehdr_addr = ELFCORE_ADDR_MAX;

unsigned long long elfcorehdr_size;

static int __init setup_elfcorehdr(char *arg)
{
	char *end;
	if (!arg)
		return -EINVAL;
	elfcorehdr_addr = memparse(arg, &end);
	if (*end == '@') {
		elfcorehdr_size = elfcorehdr_addr;
		elfcorehdr_addr = memparse(end + 1, &end);
	}
	return end > arg ? 0 : -EINVAL;
}
early_param("elfcorehdr", setup_elfcorehdr);
