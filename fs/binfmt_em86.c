/*
 *  linux/fs/binfmt_em86.c
 *
 *  Based on linux/fs/binfmt_script.c
 *  Copyright (C) 1996  Martin von Löwis
 *  original #!-checking implemented by tytso.
 *
 *  em86 changes Copyright (C) 1997  Jim Paradis
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/binfmts.h>
#include <linux/elf.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/errno.h>


#define EM86_INTERP	"/usr/bin/em86"
#define EM86_I_NAME	"em86"

static int load_em86(struct linux_binprm *bprm,struct pt_regs *regs)
{
	char *interp, *i_name, *i_arg;
	struct file * file;
	int retval;
	struct elfhdr	elf_ex;

	
	elf_ex = *((struct elfhdr *)bprm->buf);

	if (memcmp(elf_ex.e_ident, ELFMAG, SELFMAG) != 0)
		return  -ENOEXEC;

	
	if ((elf_ex.e_type != ET_EXEC && elf_ex.e_type != ET_DYN) ||
		(!((elf_ex.e_machine == EM_386) || (elf_ex.e_machine == EM_486))) ||
		(!bprm->file->f_op || !bprm->file->f_op->mmap)) {
			return -ENOEXEC;
	}

	bprm->recursion_depth++; 
	allow_write_access(bprm->file);
	fput(bprm->file);
	bprm->file = NULL;

	interp = EM86_INTERP;
	i_name = EM86_I_NAME;
	i_arg = NULL;		

	remove_arg_zero(bprm);
	retval = copy_strings_kernel(1, &bprm->filename, bprm);
	if (retval < 0) return retval; 
	bprm->argc++;
	if (i_arg) {
		retval = copy_strings_kernel(1, &i_arg, bprm);
		if (retval < 0) return retval; 
		bprm->argc++;
	}
	retval = copy_strings_kernel(1, &i_name, bprm);
	if (retval < 0)	return retval;
	bprm->argc++;

	file = open_exec(interp);
	if (IS_ERR(file))
		return PTR_ERR(file);

	bprm->file = file;

	retval = prepare_binprm(bprm);
	if (retval < 0)
		return retval;

	return search_binary_handler(bprm, regs);
}

static struct linux_binfmt em86_format = {
	.module		= THIS_MODULE,
	.load_binary	= load_em86,
};

static int __init init_em86_binfmt(void)
{
	register_binfmt(&em86_format);
	return 0;
}

static void __exit exit_em86_binfmt(void)
{
	unregister_binfmt(&em86_format);
}

core_initcall(init_em86_binfmt);
module_exit(exit_em86_binfmt);
MODULE_LICENSE("GPL");
