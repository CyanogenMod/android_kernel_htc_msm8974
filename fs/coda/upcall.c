/*
 * Mostly platform independent upcall operations to Venus:
 *  -- upcalls
 *  -- upcall routines
 *
 * Linux 2.0 version
 * Copyright (C) 1996 Peter J. Braam <braam@maths.ox.ac.uk>, 
 * Michael Callahan <callahan@maths.ox.ac.uk> 
 * 
 * Redone for Linux 2.1
 * Copyright (C) 1997 Carnegie Mellon University
 *
 * Carnegie Mellon University encourages users of this code to contribute
 * improvements to the Coda project. Contact Peter Braam <coda@cs.cmu.edu>.
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/vfs.h>

#include <linux/coda.h>
#include <linux/coda_psdev.h>
#include "coda_linux.h"
#include "coda_cache.h"

#include "coda_int.h"

static int coda_upcall(struct venus_comm *vc, int inSize, int *outSize,
		       union inputArgs *buffer);

static void *alloc_upcall(int opcode, int size)
{
	union inputArgs *inp;

	CODA_ALLOC(inp, union inputArgs *, size);
        if (!inp)
		return ERR_PTR(-ENOMEM);

        inp->ih.opcode = opcode;
	inp->ih.pid = current->pid;
	inp->ih.pgid = task_pgrp_nr(current);
	inp->ih.uid = current_fsuid();

	return (void*)inp;
}

#define UPARG(op)\
do {\
	inp = (union inputArgs *)alloc_upcall(op, insize); \
        if (IS_ERR(inp)) { return PTR_ERR(inp); }\
        outp = (union outputArgs *)(inp); \
        outsize = insize; \
} while (0)

#define INSIZE(tag) sizeof(struct coda_ ## tag ## _in)
#define OUTSIZE(tag) sizeof(struct coda_ ## tag ## _out)
#define SIZE(tag)  max_t(unsigned int, INSIZE(tag), OUTSIZE(tag))


int venus_rootfid(struct super_block *sb, struct CodaFid *fidp)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;

        insize = SIZE(root);
        UPARG(CODA_ROOT);

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);
	if (!error)
		*fidp = outp->coda_root.VFid;

	CODA_FREE(inp, insize);
	return error;
}

int venus_getattr(struct super_block *sb, struct CodaFid *fid, 
		     struct coda_vattr *attr) 
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;

        insize = SIZE(getattr); 
	UPARG(CODA_GETATTR);
        inp->coda_getattr.VFid = *fid;

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);
	if (!error)
		*attr = outp->coda_getattr.attr;

	CODA_FREE(inp, insize);
        return error;
}

int venus_setattr(struct super_block *sb, struct CodaFid *fid, 
		  struct coda_vattr *vattr)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
	
	insize = SIZE(setattr);
	UPARG(CODA_SETATTR);

        inp->coda_setattr.VFid = *fid;
	inp->coda_setattr.attr = *vattr;

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);

        CODA_FREE(inp, insize);
        return error;
}

int venus_lookup(struct super_block *sb, struct CodaFid *fid, 
		    const char *name, int length, int * type, 
		    struct CodaFid *resfid)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
	int offset;

	offset = INSIZE(lookup);
        insize = max_t(unsigned int, offset + length +1, OUTSIZE(lookup));
	UPARG(CODA_LOOKUP);

        inp->coda_lookup.VFid = *fid;
	inp->coda_lookup.name = offset;
	inp->coda_lookup.flags = CLU_CASE_SENSITIVE;
        
        memcpy((char *)(inp) + offset, name, length);
        *((char *)inp + offset + length) = '\0';

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);
	if (!error) {
		*resfid = outp->coda_lookup.VFid;
		*type = outp->coda_lookup.vtype;
	}

	CODA_FREE(inp, insize);
	return error;
}

int venus_close(struct super_block *sb, struct CodaFid *fid, int flags,
		vuid_t uid)
{
	union inputArgs *inp;
	union outputArgs *outp;
	int insize, outsize, error;
	
	insize = SIZE(release);
	UPARG(CODA_CLOSE);
	
	inp->ih.uid = uid;
        inp->coda_close.VFid = *fid;
        inp->coda_close.flags = flags;

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);

	CODA_FREE(inp, insize);
        return error;
}

int venus_open(struct super_block *sb, struct CodaFid *fid,
		  int flags, struct file **fh)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
       
	insize = SIZE(open_by_fd);
	UPARG(CODA_OPEN_BY_FD);

	inp->coda_open_by_fd.VFid = *fid;
	inp->coda_open_by_fd.flags = flags;

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);
	if (!error)
		*fh = outp->coda_open_by_fd.fh;

	CODA_FREE(inp, insize);
	return error;
}	

int venus_mkdir(struct super_block *sb, struct CodaFid *dirfid, 
		   const char *name, int length, 
		   struct CodaFid *newfid, struct coda_vattr *attrs)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;

	offset = INSIZE(mkdir);
	insize = max_t(unsigned int, offset + length + 1, OUTSIZE(mkdir));
	UPARG(CODA_MKDIR);

        inp->coda_mkdir.VFid = *dirfid;
        inp->coda_mkdir.attr = *attrs;
	inp->coda_mkdir.name = offset;
        
        memcpy((char *)(inp) + offset, name, length);
        *((char *)inp + offset + length) = '\0';

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);
	if (!error) {
		*attrs = outp->coda_mkdir.attr;
		*newfid = outp->coda_mkdir.VFid;
	}

	CODA_FREE(inp, insize);
	return error;        
}


int venus_rename(struct super_block *sb, struct CodaFid *old_fid, 
		 struct CodaFid *new_fid, size_t old_length, 
		 size_t new_length, const char *old_name, 
		 const char *new_name)
{
	union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error; 
	int offset, s;
	
	offset = INSIZE(rename);
	insize = max_t(unsigned int, offset + new_length + old_length + 8,
		     OUTSIZE(rename)); 
 	UPARG(CODA_RENAME);

        inp->coda_rename.sourceFid = *old_fid;
        inp->coda_rename.destFid =  *new_fid;
        inp->coda_rename.srcname = offset;

        
        s = ( old_length & ~0x3) +4; 
        memcpy((char *)(inp) + offset, old_name, old_length);
        *((char *)inp + offset + old_length) = '\0';

        
        offset += s;
        inp->coda_rename.destname = offset;
        s = ( new_length & ~0x3) +4; 
        memcpy((char *)(inp) + offset, new_name, new_length);
        *((char *)inp + offset + new_length) = '\0';

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);

	CODA_FREE(inp, insize);
	return error;
}

int venus_create(struct super_block *sb, struct CodaFid *dirfid, 
		 const char *name, int length, int excl, int mode,
		 struct CodaFid *newfid, struct coda_vattr *attrs) 
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;

        offset = INSIZE(create);
	insize = max_t(unsigned int, offset + length + 1, OUTSIZE(create));
	UPARG(CODA_CREATE);

        inp->coda_create.VFid = *dirfid;
        inp->coda_create.attr.va_mode = mode;
	inp->coda_create.excl = excl;
        inp->coda_create.mode = mode;
        inp->coda_create.name = offset;

        
        memcpy((char *)(inp) + offset, name, length);
        *((char *)inp + offset + length) = '\0';

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);
	if (!error) {
		*attrs = outp->coda_create.attr;
		*newfid = outp->coda_create.VFid;
	}

	CODA_FREE(inp, insize);
	return error;        
}

int venus_rmdir(struct super_block *sb, struct CodaFid *dirfid, 
		    const char *name, int length)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;

        offset = INSIZE(rmdir);
	insize = max_t(unsigned int, offset + length + 1, OUTSIZE(rmdir));
	UPARG(CODA_RMDIR);

        inp->coda_rmdir.VFid = *dirfid;
        inp->coda_rmdir.name = offset;
        memcpy((char *)(inp) + offset, name, length);
	*((char *)inp + offset + length) = '\0';

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);

	CODA_FREE(inp, insize);
	return error;
}

int venus_remove(struct super_block *sb, struct CodaFid *dirfid, 
		    const char *name, int length)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int error=0, insize, outsize, offset;

        offset = INSIZE(remove);
	insize = max_t(unsigned int, offset + length + 1, OUTSIZE(remove));
	UPARG(CODA_REMOVE);

        inp->coda_remove.VFid = *dirfid;
        inp->coda_remove.name = offset;
        memcpy((char *)(inp) + offset, name, length);
	*((char *)inp + offset + length) = '\0';

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);

	CODA_FREE(inp, insize);
	return error;
}

int venus_readlink(struct super_block *sb, struct CodaFid *fid, 
		      char *buffer, int *length)
{ 
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int retlen;
        char *result;
        
	insize = max_t(unsigned int,
		     INSIZE(readlink), OUTSIZE(readlink)+ *length + 1);
	UPARG(CODA_READLINK);

        inp->coda_readlink.VFid = *fid;

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);
	if (!error) {
		retlen = outp->coda_readlink.count;
		if ( retlen > *length )
			retlen = *length;
		*length = retlen;
		result =  (char *)outp + (long)outp->coda_readlink.data;
		memcpy(buffer, result, retlen);
		*(buffer + retlen) = '\0';
	}

        CODA_FREE(inp, insize);
        return error;
}



int venus_link(struct super_block *sb, struct CodaFid *fid, 
		  struct CodaFid *dirfid, const char *name, int len )
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset;

	offset = INSIZE(link);
	insize = max_t(unsigned int, offset  + len + 1, OUTSIZE(link));
        UPARG(CODA_LINK);

        inp->coda_link.sourceFid = *fid;
        inp->coda_link.destFid = *dirfid;
        inp->coda_link.tname = offset;

        
        memcpy((char *)(inp) + offset, name, len);
        *((char *)inp + offset + len) = '\0';

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);

	CODA_FREE(inp, insize);
        return error;
}

int venus_symlink(struct super_block *sb, struct CodaFid *fid,
		     const char *name, int len,
		     const char *symname, int symlen)
{
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        int offset, s;

        offset = INSIZE(symlink);
	insize = max_t(unsigned int, offset + len + symlen + 8, OUTSIZE(symlink));
	UPARG(CODA_SYMLINK);
        
         
        inp->coda_symlink.VFid = *fid;

	
        inp->coda_symlink.srcname = offset;
        s = ( symlen  & ~0x3 ) + 4; 
        memcpy((char *)(inp) + offset, symname, symlen);
        *((char *)inp + offset + symlen) = '\0';
        
	
        offset += s;
        inp->coda_symlink.tname = offset;
        s = (len & ~0x3) + 4;
        memcpy((char *)(inp) + offset, name, len);
        *((char *)inp + offset + len) = '\0';

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);

	CODA_FREE(inp, insize);
        return error;
}

int venus_fsync(struct super_block *sb, struct CodaFid *fid)
{
        union inputArgs *inp;
        union outputArgs *outp; 
	int insize, outsize, error;
	
	insize=SIZE(fsync);
	UPARG(CODA_FSYNC);

	inp->coda_fsync.VFid = *fid;
	error = coda_upcall(coda_vcp(sb), sizeof(union inputArgs),
			    &outsize, inp);

	CODA_FREE(inp, insize);
	return error;
}

int venus_access(struct super_block *sb, struct CodaFid *fid, int mask)
{
        union inputArgs *inp;
        union outputArgs *outp; 
	int insize, outsize, error;

	insize = SIZE(access);
	UPARG(CODA_ACCESS);

        inp->coda_access.VFid = *fid;
        inp->coda_access.flags = mask;

	error = coda_upcall(coda_vcp(sb), insize, &outsize, inp);

	CODA_FREE(inp, insize);
	return error;
}


int venus_pioctl(struct super_block *sb, struct CodaFid *fid,
		 unsigned int cmd, struct PioctlData *data)
{
        union inputArgs *inp;
        union outputArgs *outp;  
	int insize, outsize, error;
	int iocsize;

	insize = VC_MAXMSGSIZE;
	UPARG(CODA_IOCTL);

        
        if (data->vi.in_size > VC_MAXDATASIZE) {
		error = -EINVAL;
		goto exit;
        }

        if (data->vi.out_size > VC_MAXDATASIZE) {
		error = -EINVAL;
		goto exit;
	}

        inp->coda_ioctl.VFid = *fid;
    
        inp->coda_ioctl.cmd = (cmd & ~(PIOCPARM_MASK << 16));	
        iocsize = ((cmd >> 16) & PIOCPARM_MASK) - sizeof(char *) - sizeof(int);
        inp->coda_ioctl.cmd |= (iocsize & PIOCPARM_MASK) <<	16;	
    
        
        inp->coda_ioctl.len = data->vi.in_size;
        inp->coda_ioctl.data = (char *)(INSIZE(ioctl));
     
        
        if ( copy_from_user((char*)inp + (long)inp->coda_ioctl.data,
			    data->vi.in, data->vi.in_size) ) {
		error = -EINVAL;
	        goto exit;
	}

	error = coda_upcall(coda_vcp(sb), SIZE(ioctl) + data->vi.in_size,
			    &outsize, inp);

        if (error) {
	        printk("coda_pioctl: Venus returns: %d for %s\n", 
		       error, coda_f2s(fid));
		goto exit; 
	}

	if (outsize < (long)outp->coda_ioctl.data + outp->coda_ioctl.len) {
		error = -EINVAL;
		goto exit;
	}
        
	
        if (outp->coda_ioctl.len > data->vi.out_size) {
		error = -EINVAL;
		goto exit;
        }

	
	if (copy_to_user(data->vi.out,
			 (char *)outp + (long)outp->coda_ioctl.data,
			 outp->coda_ioctl.len)) {
		error = -EFAULT;
		goto exit;
	}

 exit:
	CODA_FREE(inp, insize);
	return error;
}

int venus_statfs(struct dentry *dentry, struct kstatfs *sfs)
{ 
        union inputArgs *inp;
        union outputArgs *outp;
        int insize, outsize, error;
        
	insize = max_t(unsigned int, INSIZE(statfs), OUTSIZE(statfs));
	UPARG(CODA_STATFS);

	error = coda_upcall(coda_vcp(dentry->d_sb), insize, &outsize, inp);
	if (!error) {
		sfs->f_blocks = outp->coda_statfs.stat.f_blocks;
		sfs->f_bfree  = outp->coda_statfs.stat.f_bfree;
		sfs->f_bavail = outp->coda_statfs.stat.f_bavail;
		sfs->f_files  = outp->coda_statfs.stat.f_files;
		sfs->f_ffree  = outp->coda_statfs.stat.f_ffree;
	}

        CODA_FREE(inp, insize);
        return error;
}

static void coda_block_signals(sigset_t *old)
{
	spin_lock_irq(&current->sighand->siglock);
	*old = current->blocked;

	sigfillset(&current->blocked);
	sigdelset(&current->blocked, SIGKILL);
	sigdelset(&current->blocked, SIGSTOP);
	sigdelset(&current->blocked, SIGINT);

	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);
}

static void coda_unblock_signals(sigset_t *old)
{
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = *old;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);
}

#define CODA_INTERRUPTIBLE(r) (!coda_hard && \
			       (((r)->uc_opcode != CODA_CLOSE && \
				 (r)->uc_opcode != CODA_STORE && \
				 (r)->uc_opcode != CODA_RELEASE) || \
				(r)->uc_flags & CODA_REQ_READ))

static inline void coda_waitfor_upcall(struct venus_comm *vcp,
				       struct upc_req *req)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long timeout = jiffies + coda_timeout * HZ;
	sigset_t old;
	int blocked;

	coda_block_signals(&old);
	blocked = 1;

	add_wait_queue(&req->uc_sleep, &wait);
	for (;;) {
		if (CODA_INTERRUPTIBLE(req))
			set_current_state(TASK_INTERRUPTIBLE);
		else
			set_current_state(TASK_UNINTERRUPTIBLE);

		
		if (req->uc_flags & (CODA_REQ_WRITE | CODA_REQ_ABORT))
			break;

		if (blocked && time_after(jiffies, timeout) &&
		    CODA_INTERRUPTIBLE(req))
		{
			coda_unblock_signals(&old);
			blocked = 0;
		}

		if (signal_pending(current)) {
			list_del(&req->uc_chain);
			break;
		}

		mutex_unlock(&vcp->vc_mutex);
		if (blocked)
			schedule_timeout(HZ);
		else
			schedule();
		mutex_lock(&vcp->vc_mutex);
	}
	if (blocked)
		coda_unblock_signals(&old);

	remove_wait_queue(&req->uc_sleep, &wait);
	set_current_state(TASK_RUNNING);
}


static int coda_upcall(struct venus_comm *vcp,
		       int inSize, int *outSize,
		       union inputArgs *buffer)
{
	union outputArgs *out;
	union inputArgs *sig_inputArgs;
	struct upc_req *req = NULL, *sig_req;
	int error;

	mutex_lock(&vcp->vc_mutex);

	if (!vcp->vc_inuse) {
		printk(KERN_NOTICE "coda: Venus dead, not sending upcall\n");
		error = -ENXIO;
		goto exit;
	}

	
	req = kmalloc(sizeof(struct upc_req), GFP_KERNEL);
	if (!req) {
		error = -ENOMEM;
		goto exit;
	}

	req->uc_data = (void *)buffer;
	req->uc_flags = 0;
	req->uc_inSize = inSize;
	req->uc_outSize = *outSize ? *outSize : inSize;
	req->uc_opcode = ((union inputArgs *)buffer)->ih.opcode;
	req->uc_unique = ++vcp->vc_seq;
	init_waitqueue_head(&req->uc_sleep);

	
	((union inputArgs *)buffer)->ih.unique = req->uc_unique;

	
	list_add_tail(&req->uc_chain, &vcp->vc_pending);

	wake_up_interruptible(&vcp->vc_waitq);

	
	coda_waitfor_upcall(vcp, req);

	
	if (req->uc_flags & CODA_REQ_WRITE) {
		out = (union outputArgs *)req->uc_data;
		
		error = -out->oh.result;
		*outSize = req->uc_outSize;
		goto exit;
	}

	error = -EINTR;
	if ((req->uc_flags & CODA_REQ_ABORT) || !signal_pending(current)) {
		printk(KERN_WARNING "coda: Unexpected interruption.\n");
		goto exit;
	}

	
	if (!(req->uc_flags & CODA_REQ_READ))
		goto exit;

	
	if (!vcp->vc_inuse) {
		printk(KERN_INFO "coda: Venus dead, not sending signal.\n");
		goto exit;
	}

	error = -ENOMEM;
	sig_req = kmalloc(sizeof(struct upc_req), GFP_KERNEL);
	if (!sig_req) goto exit;

	CODA_ALLOC((sig_req->uc_data), char *, sizeof(struct coda_in_hdr));
	if (!sig_req->uc_data) {
		kfree(sig_req);
		goto exit;
	}

	error = -EINTR;
	sig_inputArgs = (union inputArgs *)sig_req->uc_data;
	sig_inputArgs->ih.opcode = CODA_SIGNAL;
	sig_inputArgs->ih.unique = req->uc_unique;

	sig_req->uc_flags = CODA_REQ_ASYNC;
	sig_req->uc_opcode = sig_inputArgs->ih.opcode;
	sig_req->uc_unique = sig_inputArgs->ih.unique;
	sig_req->uc_inSize = sizeof(struct coda_in_hdr);
	sig_req->uc_outSize = sizeof(struct coda_in_hdr);

	
	list_add(&(sig_req->uc_chain), &vcp->vc_pending);
	wake_up_interruptible(&vcp->vc_waitq);

exit:
	kfree(req);
	mutex_unlock(&vcp->vc_mutex);
	return error;
}




int coda_downcall(struct venus_comm *vcp, int opcode, union outputArgs *out)
{
	struct inode *inode = NULL;
	struct CodaFid *fid = NULL, *newfid;
	struct super_block *sb;

	
	mutex_lock(&vcp->vc_mutex);
	sb = vcp->vc_sb;
	if (!sb || !sb->s_root)
		goto unlock_out;

	switch (opcode) {
	case CODA_FLUSH:
		coda_cache_clear_all(sb);
		shrink_dcache_sb(sb);
		if (sb->s_root->d_inode)
			coda_flag_inode(sb->s_root->d_inode, C_FLUSH);
		break;

	case CODA_PURGEUSER:
		coda_cache_clear_all(sb);
		break;

	case CODA_ZAPDIR:
		fid = &out->coda_zapdir.CodaFid;
		break;

	case CODA_ZAPFILE:
		fid = &out->coda_zapfile.CodaFid;
		break;

	case CODA_PURGEFID:
		fid = &out->coda_purgefid.CodaFid;
		break;

	case CODA_REPLACE:
		fid = &out->coda_replace.OldFid;
		break;
	}
	if (fid)
		inode = coda_fid_to_inode(fid, sb);

unlock_out:
	mutex_unlock(&vcp->vc_mutex);

	if (!inode)
		return 0;

	switch (opcode) {
	case CODA_ZAPDIR:
		coda_flag_inode_children(inode, C_PURGE);
		coda_flag_inode(inode, C_VATTR);
		break;

	case CODA_ZAPFILE:
		coda_flag_inode(inode, C_VATTR);
		break;

	case CODA_PURGEFID:
		coda_flag_inode_children(inode, C_PURGE);

		
		coda_flag_inode(inode, C_PURGE);
		d_prune_aliases(inode);
		break;

	case CODA_REPLACE:
		newfid = &out->coda_replace.NewFid;
		coda_replace_fid(inode, fid, newfid);
		break;
	}
	iput(inode);
	return 0;
}

