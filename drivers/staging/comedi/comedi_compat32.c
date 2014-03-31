/*
    comedi/comedi_compat32.c
    32-bit ioctl compatibility for 64-bit comedi kernel module.

    Author: Ian Abbott, MEV Ltd. <abbotti@mev.co.uk>
    Copyright (C) 2007 MEV Ltd. <http://www.mev.co.uk/>

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 1997-2007 David A. Schleef <ds@schleef.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#define __NO_VERSION__
#include <linux/uaccess.h>
#include "comedi.h"
#include "comedi_compat32.h"

#ifdef CONFIG_COMPAT

#define COMEDI32_CHANINFO _IOR(CIO, 3, struct comedi32_chaninfo_struct)
#define COMEDI32_RANGEINFO _IOR(CIO, 8, struct comedi32_rangeinfo_struct)
#define COMEDI32_CMD _IOR(CIO, 9, struct comedi32_cmd_struct)
#define COMEDI32_CMDTEST _IOR(CIO, 10, struct comedi32_cmd_struct)
#define COMEDI32_INSNLIST _IOR(CIO, 11, struct comedi32_insnlist_struct)
#define COMEDI32_INSN _IOR(CIO, 12, struct comedi32_insn_struct)

struct comedi32_chaninfo_struct {
	unsigned int subdev;
	compat_uptr_t maxdata_list;	
	compat_uptr_t flaglist;	
	compat_uptr_t rangelist;	
	unsigned int unused[4];
};

struct comedi32_rangeinfo_struct {
	unsigned int range_type;
	compat_uptr_t range_ptr;	
};

struct comedi32_cmd_struct {
	unsigned int subdev;
	unsigned int flags;
	unsigned int start_src;
	unsigned int start_arg;
	unsigned int scan_begin_src;
	unsigned int scan_begin_arg;
	unsigned int convert_src;
	unsigned int convert_arg;
	unsigned int scan_end_src;
	unsigned int scan_end_arg;
	unsigned int stop_src;
	unsigned int stop_arg;
	compat_uptr_t chanlist;	
	unsigned int chanlist_len;
	compat_uptr_t data;	
	unsigned int data_len;
};

struct comedi32_insn_struct {
	unsigned int insn;
	unsigned int n;
	compat_uptr_t data;	
	unsigned int subdev;
	unsigned int chanspec;
	unsigned int unused[3];
};

struct comedi32_insnlist_struct {
	unsigned int n_insns;
	compat_uptr_t insns;	
};

static int translated_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	if (!file->f_op)
		return -ENOTTY;

	if (file->f_op->unlocked_ioctl)
		return file->f_op->unlocked_ioctl(file, cmd, arg);

	return -ENOTTY;
}

static int compat_chaninfo(struct file *file, unsigned long arg)
{
	struct comedi_chaninfo __user *chaninfo;
	struct comedi32_chaninfo_struct __user *chaninfo32;
	int err;
	union {
		unsigned int uint;
		compat_uptr_t uptr;
	} temp;

	chaninfo32 = compat_ptr(arg);
	chaninfo = compat_alloc_user_space(sizeof(*chaninfo));

	
	if (!access_ok(VERIFY_READ, chaninfo32, sizeof(*chaninfo32))
	    || !access_ok(VERIFY_WRITE, chaninfo, sizeof(*chaninfo))) {
		return -EFAULT;
	}
	err = 0;
	err |= __get_user(temp.uint, &chaninfo32->subdev);
	err |= __put_user(temp.uint, &chaninfo->subdev);
	err |= __get_user(temp.uptr, &chaninfo32->maxdata_list);
	err |= __put_user(compat_ptr(temp.uptr), &chaninfo->maxdata_list);
	err |= __get_user(temp.uptr, &chaninfo32->flaglist);
	err |= __put_user(compat_ptr(temp.uptr), &chaninfo->flaglist);
	err |= __get_user(temp.uptr, &chaninfo32->rangelist);
	err |= __put_user(compat_ptr(temp.uptr), &chaninfo->rangelist);
	if (err)
		return -EFAULT;

	return translated_ioctl(file, COMEDI_CHANINFO, (unsigned long)chaninfo);
}

static int compat_rangeinfo(struct file *file, unsigned long arg)
{
	struct comedi_rangeinfo __user *rangeinfo;
	struct comedi32_rangeinfo_struct __user *rangeinfo32;
	int err;
	union {
		unsigned int uint;
		compat_uptr_t uptr;
	} temp;

	rangeinfo32 = compat_ptr(arg);
	rangeinfo = compat_alloc_user_space(sizeof(*rangeinfo));

	
	if (!access_ok(VERIFY_READ, rangeinfo32, sizeof(*rangeinfo32))
	    || !access_ok(VERIFY_WRITE, rangeinfo, sizeof(*rangeinfo))) {
		return -EFAULT;
	}
	err = 0;
	err |= __get_user(temp.uint, &rangeinfo32->range_type);
	err |= __put_user(temp.uint, &rangeinfo->range_type);
	err |= __get_user(temp.uptr, &rangeinfo32->range_ptr);
	err |= __put_user(compat_ptr(temp.uptr), &rangeinfo->range_ptr);
	if (err)
		return -EFAULT;

	return translated_ioctl(file, COMEDI_RANGEINFO,
				(unsigned long)rangeinfo);
}

static int get_compat_cmd(struct comedi_cmd __user *cmd,
			  struct comedi32_cmd_struct __user *cmd32)
{
	int err;
	union {
		unsigned int uint;
		compat_uptr_t uptr;
	} temp;

	
	if (!access_ok(VERIFY_READ, cmd32, sizeof(*cmd32))
	    || !access_ok(VERIFY_WRITE, cmd, sizeof(*cmd))) {
		return -EFAULT;
	}
	err = 0;
	err |= __get_user(temp.uint, &cmd32->subdev);
	err |= __put_user(temp.uint, &cmd->subdev);
	err |= __get_user(temp.uint, &cmd32->flags);
	err |= __put_user(temp.uint, &cmd->flags);
	err |= __get_user(temp.uint, &cmd32->start_src);
	err |= __put_user(temp.uint, &cmd->start_src);
	err |= __get_user(temp.uint, &cmd32->start_arg);
	err |= __put_user(temp.uint, &cmd->start_arg);
	err |= __get_user(temp.uint, &cmd32->scan_begin_src);
	err |= __put_user(temp.uint, &cmd->scan_begin_src);
	err |= __get_user(temp.uint, &cmd32->scan_begin_arg);
	err |= __put_user(temp.uint, &cmd->scan_begin_arg);
	err |= __get_user(temp.uint, &cmd32->convert_src);
	err |= __put_user(temp.uint, &cmd->convert_src);
	err |= __get_user(temp.uint, &cmd32->convert_arg);
	err |= __put_user(temp.uint, &cmd->convert_arg);
	err |= __get_user(temp.uint, &cmd32->scan_end_src);
	err |= __put_user(temp.uint, &cmd->scan_end_src);
	err |= __get_user(temp.uint, &cmd32->scan_end_arg);
	err |= __put_user(temp.uint, &cmd->scan_end_arg);
	err |= __get_user(temp.uint, &cmd32->stop_src);
	err |= __put_user(temp.uint, &cmd->stop_src);
	err |= __get_user(temp.uint, &cmd32->stop_arg);
	err |= __put_user(temp.uint, &cmd->stop_arg);
	err |= __get_user(temp.uptr, &cmd32->chanlist);
	err |= __put_user(compat_ptr(temp.uptr), &cmd->chanlist);
	err |= __get_user(temp.uint, &cmd32->chanlist_len);
	err |= __put_user(temp.uint, &cmd->chanlist_len);
	err |= __get_user(temp.uptr, &cmd32->data);
	err |= __put_user(compat_ptr(temp.uptr), &cmd->data);
	err |= __get_user(temp.uint, &cmd32->data_len);
	err |= __put_user(temp.uint, &cmd->data_len);
	return err ? -EFAULT : 0;
}

static int put_compat_cmd(struct comedi32_cmd_struct __user *cmd32,
			  struct comedi_cmd __user *cmd)
{
	int err;
	unsigned int temp;

	
	
	if (!access_ok(VERIFY_READ, cmd, sizeof(*cmd))
	    || !access_ok(VERIFY_WRITE, cmd32, sizeof(*cmd32))) {
		return -EFAULT;
	}
	err = 0;
	err |= __get_user(temp, &cmd->subdev);
	err |= __put_user(temp, &cmd32->subdev);
	err |= __get_user(temp, &cmd->flags);
	err |= __put_user(temp, &cmd32->flags);
	err |= __get_user(temp, &cmd->start_src);
	err |= __put_user(temp, &cmd32->start_src);
	err |= __get_user(temp, &cmd->start_arg);
	err |= __put_user(temp, &cmd32->start_arg);
	err |= __get_user(temp, &cmd->scan_begin_src);
	err |= __put_user(temp, &cmd32->scan_begin_src);
	err |= __get_user(temp, &cmd->scan_begin_arg);
	err |= __put_user(temp, &cmd32->scan_begin_arg);
	err |= __get_user(temp, &cmd->convert_src);
	err |= __put_user(temp, &cmd32->convert_src);
	err |= __get_user(temp, &cmd->convert_arg);
	err |= __put_user(temp, &cmd32->convert_arg);
	err |= __get_user(temp, &cmd->scan_end_src);
	err |= __put_user(temp, &cmd32->scan_end_src);
	err |= __get_user(temp, &cmd->scan_end_arg);
	err |= __put_user(temp, &cmd32->scan_end_arg);
	err |= __get_user(temp, &cmd->stop_src);
	err |= __put_user(temp, &cmd32->stop_src);
	err |= __get_user(temp, &cmd->stop_arg);
	err |= __put_user(temp, &cmd32->stop_arg);
	
	err |= __get_user(temp, &cmd->chanlist_len);
	err |= __put_user(temp, &cmd32->chanlist_len);
	
	err |= __get_user(temp, &cmd->data_len);
	err |= __put_user(temp, &cmd32->data_len);
	return err ? -EFAULT : 0;
}

static int compat_cmd(struct file *file, unsigned long arg)
{
	struct comedi_cmd __user *cmd;
	struct comedi32_cmd_struct __user *cmd32;
	int rc;

	cmd32 = compat_ptr(arg);
	cmd = compat_alloc_user_space(sizeof(*cmd));

	rc = get_compat_cmd(cmd, cmd32);
	if (rc)
		return rc;

	return translated_ioctl(file, COMEDI_CMD, (unsigned long)cmd);
}

static int compat_cmdtest(struct file *file, unsigned long arg)
{
	struct comedi_cmd __user *cmd;
	struct comedi32_cmd_struct __user *cmd32;
	int rc, err;

	cmd32 = compat_ptr(arg);
	cmd = compat_alloc_user_space(sizeof(*cmd));

	rc = get_compat_cmd(cmd, cmd32);
	if (rc)
		return rc;

	rc = translated_ioctl(file, COMEDI_CMDTEST, (unsigned long)cmd);
	if (rc < 0)
		return rc;

	err = put_compat_cmd(cmd32, cmd);
	if (err)
		rc = err;

	return rc;
}

static int get_compat_insn(struct comedi_insn __user *insn,
			   struct comedi32_insn_struct __user *insn32)
{
	int err;
	union {
		unsigned int uint;
		compat_uptr_t uptr;
	} temp;

	
	err = 0;
	if (!access_ok(VERIFY_READ, insn32, sizeof(*insn32))
	    || !access_ok(VERIFY_WRITE, insn, sizeof(*insn)))
		return -EFAULT;

	err |= __get_user(temp.uint, &insn32->insn);
	err |= __put_user(temp.uint, &insn->insn);
	err |= __get_user(temp.uint, &insn32->n);
	err |= __put_user(temp.uint, &insn->n);
	err |= __get_user(temp.uptr, &insn32->data);
	err |= __put_user(compat_ptr(temp.uptr), &insn->data);
	err |= __get_user(temp.uint, &insn32->subdev);
	err |= __put_user(temp.uint, &insn->subdev);
	err |= __get_user(temp.uint, &insn32->chanspec);
	err |= __put_user(temp.uint, &insn->chanspec);
	return err ? -EFAULT : 0;
}

static int compat_insnlist(struct file *file, unsigned long arg)
{
	struct combined_insnlist {
		struct comedi_insnlist insnlist;
		struct comedi_insn insn[1];
	} __user *s;
	struct comedi32_insnlist_struct __user *insnlist32;
	struct comedi32_insn_struct __user *insn32;
	compat_uptr_t uptr;
	unsigned int n_insns, n;
	int err, rc;

	insnlist32 = compat_ptr(arg);

	
	if (!access_ok(VERIFY_READ, insnlist32, sizeof(*insnlist32)))
		return -EFAULT;

	err = 0;
	err |= __get_user(n_insns, &insnlist32->n_insns);
	err |= __get_user(uptr, &insnlist32->insns);
	insn32 = compat_ptr(uptr);
	if (err)
		return -EFAULT;

	
	s = compat_alloc_user_space(offsetof(struct combined_insnlist,
					     insn[n_insns]));

	
	if (!access_ok(VERIFY_WRITE, &s->insnlist, sizeof(s->insnlist)))
		return -EFAULT;

	err |= __put_user(n_insns, &s->insnlist.n_insns);
	err |= __put_user(&s->insn[0], &s->insnlist.insns);
	if (err)
		return -EFAULT;

	
	for (n = 0; n < n_insns; n++) {
		rc = get_compat_insn(&s->insn[n], &insn32[n]);
		if (rc)
			return rc;
	}

	return translated_ioctl(file, COMEDI_INSNLIST,
				(unsigned long)&s->insnlist);
}

static int compat_insn(struct file *file, unsigned long arg)
{
	struct comedi_insn __user *insn;
	struct comedi32_insn_struct __user *insn32;
	int rc;

	insn32 = compat_ptr(arg);
	insn = compat_alloc_user_space(sizeof(*insn));

	rc = get_compat_insn(insn, insn32);
	if (rc)
		return rc;

	return translated_ioctl(file, COMEDI_INSN, (unsigned long)insn);
}

static inline int raw_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	int rc;

	switch (cmd) {
	case COMEDI_DEVCONFIG:
	case COMEDI_DEVINFO:
	case COMEDI_SUBDINFO:
	case COMEDI_BUFCONFIG:
	case COMEDI_BUFINFO:
		
		arg = (unsigned long)compat_ptr(arg);
		rc = translated_ioctl(file, cmd, arg);
		break;
	case COMEDI_LOCK:
	case COMEDI_UNLOCK:
	case COMEDI_CANCEL:
	case COMEDI_POLL:
		
		rc = translated_ioctl(file, cmd, arg);
		break;
	case COMEDI32_CHANINFO:
		rc = compat_chaninfo(file, arg);
		break;
	case COMEDI32_RANGEINFO:
		rc = compat_rangeinfo(file, arg);
		break;
	case COMEDI32_CMD:
		rc = compat_cmd(file, arg);
		break;
	case COMEDI32_CMDTEST:
		rc = compat_cmdtest(file, arg);
		break;
	case COMEDI32_INSNLIST:
		rc = compat_insnlist(file, arg);
		break;
	case COMEDI32_INSN:
		rc = compat_insn(file, arg);
		break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

long comedi_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return raw_ioctl(file, cmd, arg);
}

#endif 
