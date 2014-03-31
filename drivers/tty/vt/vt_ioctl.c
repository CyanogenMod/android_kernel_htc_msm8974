/*
 *  Copyright (C) 1992 obz under the linux copyright
 *
 *  Dynamic diacritical handling - aeb@cwi.nl - Dec 1993
 *  Dynamic keymap and string allocation - aeb@cwi.nl - May 1994
 *  Restrict VT switching via ioctl() - grif@cs.ucr.edu - Dec 1995
 *  Some code moved for less code duplication - Andi Kleen - Mar 1997
 *  Check put/get_user, cleanups - acme@conectiva.com.br - Jun 2001
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/compat.h>
#include <linux/module.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/console.h>
#include <linux/consolemap.h>
#include <linux/signal.h>
#include <linux/timex.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/kbd_kern.h>
#include <linux/vt_kern.h>
#include <linux/kbd_diacr.h>
#include <linux/selection.h>

char vt_dont_switch;
extern struct tty_driver *console_driver;

#define VT_IS_IN_USE(i)	(console_driver->ttys[i] && console_driver->ttys[i]->count)
#define VT_BUSY(i)	(VT_IS_IN_USE(i) || i == fg_console || vc_cons[i].d == sel_cons)


#ifdef CONFIG_X86
#include <linux/syscalls.h>
#endif

static void complete_change_console(struct vc_data *vc);


struct vt_event_wait {
	struct list_head list;
	struct vt_event event;
	int done;
};

static LIST_HEAD(vt_events);
static DEFINE_SPINLOCK(vt_event_lock);
static DECLARE_WAIT_QUEUE_HEAD(vt_event_waitqueue);


void vt_event_post(unsigned int event, unsigned int old, unsigned int new)
{
	struct list_head *pos, *head;
	unsigned long flags;
	int wake = 0;

	spin_lock_irqsave(&vt_event_lock, flags);
	head = &vt_events;

	list_for_each(pos, head) {
		struct vt_event_wait *ve = list_entry(pos,
						struct vt_event_wait, list);
		if (!(ve->event.event & event))
			continue;
		ve->event.event = event;
		ve->event.oldev = old + 1;
		ve->event.newev = new + 1;
		wake = 1;
		ve->done = 1;
	}
	spin_unlock_irqrestore(&vt_event_lock, flags);
	if (wake)
		wake_up_interruptible(&vt_event_waitqueue);
}

static void __vt_event_queue(struct vt_event_wait *vw)
{
	unsigned long flags;
	
	INIT_LIST_HEAD(&vw->list);
	vw->done = 0;
	
	spin_lock_irqsave(&vt_event_lock, flags);
	list_add(&vw->list, &vt_events);
	spin_unlock_irqrestore(&vt_event_lock, flags);
}

static void __vt_event_wait(struct vt_event_wait *vw)
{
	
	wait_event_interruptible(vt_event_waitqueue, vw->done);
}

static void __vt_event_dequeue(struct vt_event_wait *vw)
{
	unsigned long flags;

	
	spin_lock_irqsave(&vt_event_lock, flags);
	list_del(&vw->list);
	spin_unlock_irqrestore(&vt_event_lock, flags);
}


static void vt_event_wait(struct vt_event_wait *vw)
{
	__vt_event_queue(vw);
	__vt_event_wait(vw);
	__vt_event_dequeue(vw);
}


static int vt_event_wait_ioctl(struct vt_event __user *event)
{
	struct vt_event_wait vw;

	if (copy_from_user(&vw.event, event, sizeof(struct vt_event)))
		return -EFAULT;
	
	if (vw.event.event & ~VT_MAX_EVENT)
		return -EINVAL;

	vt_event_wait(&vw);
	
	if (vw.done) {
		if (copy_to_user(event, &vw.event, sizeof(struct vt_event)))
			return -EFAULT;
		return 0;
	}
	return -EINTR;
}


int vt_waitactive(int n)
{
	struct vt_event_wait vw;
	do {
		vw.event.event = VT_EVENT_SWITCH;
		__vt_event_queue(&vw);
		if (n == fg_console + 1) {
			__vt_event_dequeue(&vw);
			break;
		}
		__vt_event_wait(&vw);
		__vt_event_dequeue(&vw);
		if (vw.done == 0)
			return -EINTR;
	} while (vw.event.newev != n);
	return 0;
}

#define GPFIRST 0x3b4
#define GPLAST 0x3df
#define GPNUM (GPLAST - GPFIRST + 1)



static inline int 
do_fontx_ioctl(int cmd, struct consolefontdesc __user *user_cfd, int perm, struct console_font_op *op)
{
	struct consolefontdesc cfdarg;
	int i;

	if (copy_from_user(&cfdarg, user_cfd, sizeof(struct consolefontdesc))) 
		return -EFAULT;
 	
	switch (cmd) {
	case PIO_FONTX:
		if (!perm)
			return -EPERM;
		op->op = KD_FONT_OP_SET;
		op->flags = KD_FONT_FLAG_OLD;
		op->width = 8;
		op->height = cfdarg.charheight;
		op->charcount = cfdarg.charcount;
		op->data = cfdarg.chardata;
		return con_font_op(vc_cons[fg_console].d, op);
	case GIO_FONTX: {
		op->op = KD_FONT_OP_GET;
		op->flags = KD_FONT_FLAG_OLD;
		op->width = 8;
		op->height = cfdarg.charheight;
		op->charcount = cfdarg.charcount;
		op->data = cfdarg.chardata;
		i = con_font_op(vc_cons[fg_console].d, op);
		if (i)
			return i;
		cfdarg.charheight = op->height;
		cfdarg.charcount = op->charcount;
		if (copy_to_user(user_cfd, &cfdarg, sizeof(struct consolefontdesc)))
			return -EFAULT;
		return 0;
		}
	}
	return -EINVAL;
}

static inline int 
do_unimap_ioctl(int cmd, struct unimapdesc __user *user_ud, int perm, struct vc_data *vc)
{
	struct unimapdesc tmp;

	if (copy_from_user(&tmp, user_ud, sizeof tmp))
		return -EFAULT;
	if (tmp.entries)
		if (!access_ok(VERIFY_WRITE, tmp.entries,
				tmp.entry_ct*sizeof(struct unipair)))
			return -EFAULT;
	switch (cmd) {
	case PIO_UNIMAP:
		if (!perm)
			return -EPERM;
		return con_set_unimap(vc, tmp.entry_ct, tmp.entries);
	case GIO_UNIMAP:
		if (!perm && fg_console != vc->vc_num)
			return -EPERM;
		return con_get_unimap(vc, tmp.entry_ct, &(user_ud->entry_ct), tmp.entries);
	}
	return 0;
}



int vt_ioctl(struct tty_struct *tty,
	     unsigned int cmd, unsigned long arg)
{
	struct vc_data *vc = tty->driver_data;
	struct console_font_op op;	
	unsigned int console;
	unsigned char ucval;
	unsigned int uival;
	void __user *up = (void __user *)arg;
	int i, perm;
	int ret = 0;

	console = vc->vc_num;


	if (!vc_cons_allocated(console)) { 	
		ret = -ENOIOCTLCMD;
		goto out;
	}


	perm = 0;
	if (current->signal->tty == tty || capable(CAP_SYS_TTY_CONFIG))
		perm = 1;
 
	switch (cmd) {
	case TIOCLINUX:
		ret = tioclinux(tty, arg);
		break;
	case KIOCSOUND:
		if (!perm)
			return -EPERM;
		if (arg)
			arg = PIT_TICK_RATE / arg;
		kd_mksound(arg, 0);
		break;

	case KDMKTONE:
		if (!perm)
			return -EPERM;
	{
		unsigned int ticks, count;
		
		ticks = HZ * ((arg >> 16) & 0xffff) / 1000;
		count = ticks ? (arg & 0xffff) : 0;
		if (count)
			count = PIT_TICK_RATE / count;
		kd_mksound(count, ticks);
		break;
	}

	case KDGKBTYPE:
		ucval = KB_101;
		ret = put_user(ucval, (char __user *)arg);
		break;

#ifdef CONFIG_X86
	case KDADDIO:
	case KDDELIO:
		if (arg < GPFIRST || arg > GPLAST) {
			ret = -EINVAL;
			break;
		}
		ret = sys_ioperm(arg, 1, (cmd == KDADDIO)) ? -ENXIO : 0;
		break;

	case KDENABIO:
	case KDDISABIO:
		ret = sys_ioperm(GPFIRST, GPNUM,
				  (cmd == KDENABIO)) ? -ENXIO : 0;
		break;
#endif

	
		
	case KDKBDREP:
	{
		struct kbd_repeat kbrep;
		
		if (!capable(CAP_SYS_TTY_CONFIG))
			return -EPERM;

		if (copy_from_user(&kbrep, up, sizeof(struct kbd_repeat))) {
			ret =  -EFAULT;
			break;
		}
		ret = kbd_rate(&kbrep);
		if (ret)
			break;
		if (copy_to_user(up, &kbrep, sizeof(struct kbd_repeat)))
			ret = -EFAULT;
		break;
	}

	case KDSETMODE:
		if (!perm)
			return -EPERM;
		switch (arg) {
		case KD_GRAPHICS:
			break;
		case KD_TEXT0:
		case KD_TEXT1:
			arg = KD_TEXT;
		case KD_TEXT:
			break;
		default:
			ret = -EINVAL;
			goto out;
		}
		
		if (vc->vc_mode == (unsigned char) arg)
			break;
		vc->vc_mode = (unsigned char) arg;
		if (console != fg_console)
			break;
		console_lock();
		if (arg == KD_TEXT)
			do_unblank_screen(1);
		else
			do_blank_screen(1);
		console_unlock();
		break;

	case KDGETMODE:
		uival = vc->vc_mode;
		goto setint;

	case KDMAPDISP:
	case KDUNMAPDISP:
		ret = -EINVAL;
		break;

	case KDSKBMODE:
		if (!perm)
			return -EPERM;
		ret = vt_do_kdskbmode(console, arg);
		if (ret == 0)
			tty_ldisc_flush(tty);
		break;

	case KDGKBMODE:
		uival = vt_do_kdgkbmode(console);
		ret = put_user(uival, (int __user *)arg);
		break;

	case KDSKBMETA:
		ret = vt_do_kdskbmeta(console, arg);
		break;

	case KDGKBMETA:
		
		uival = vt_do_kdgkbmeta(console);
	setint:
		ret = put_user(uival, (int __user *)arg);
		break;

	case KDGETKEYCODE:
	case KDSETKEYCODE:
		if(!capable(CAP_SYS_TTY_CONFIG))
			perm = 0;
		ret = vt_do_kbkeycode_ioctl(cmd, up, perm);
		break;

	case KDGKBENT:
	case KDSKBENT:
		ret = vt_do_kdsk_ioctl(cmd, up, perm, console);
		break;

	case KDGKBSENT:
	case KDSKBSENT:
		ret = vt_do_kdgkb_ioctl(cmd, up, perm);
		break;

	case KDGKBDIACR:
	case KDGKBDIACRUC:
	case KDSKBDIACR:
	case KDSKBDIACRUC:
		ret = vt_do_diacrit(cmd, up, perm);
		break;

	
	
	case KDGKBLED:
	case KDSKBLED:
	case KDGETLED:
	case KDSETLED:
		ret = vt_do_kdskled(console, cmd, arg, perm);
		break;

	case KDSIGACCEPT:
	{
		if (!perm || !capable(CAP_KILL))
			return -EPERM;
		if (!valid_signal(arg) || arg < 1 || arg == SIGKILL)
			ret = -EINVAL;
		else {
			spin_lock_irq(&vt_spawn_con.lock);
			put_pid(vt_spawn_con.pid);
			vt_spawn_con.pid = get_pid(task_pid(current));
			vt_spawn_con.sig = arg;
			spin_unlock_irq(&vt_spawn_con.lock);
		}
		break;
	}

	case VT_SETMODE:
	{
		struct vt_mode tmp;

		if (!perm)
			return -EPERM;
		if (copy_from_user(&tmp, up, sizeof(struct vt_mode))) {
			ret = -EFAULT;
			goto out;
		}
		if (tmp.mode != VT_AUTO && tmp.mode != VT_PROCESS) {
			ret = -EINVAL;
			goto out;
		}
		console_lock();
		vc->vt_mode = tmp;
		
		vc->vt_mode.frsig = 0;
		put_pid(vc->vt_pid);
		vc->vt_pid = get_pid(task_pid(current));
		
		vc->vt_newvt = -1;
		console_unlock();
		break;
	}

	case VT_GETMODE:
	{
		struct vt_mode tmp;
		int rc;

		console_lock();
		memcpy(&tmp, &vc->vt_mode, sizeof(struct vt_mode));
		console_unlock();

		rc = copy_to_user(up, &tmp, sizeof(struct vt_mode));
		if (rc)
			ret = -EFAULT;
		break;
	}

	case VT_GETSTATE:
	{
		struct vt_stat __user *vtstat = up;
		unsigned short state, mask;

		
		if (put_user(fg_console + 1, &vtstat->v_active))
			ret = -EFAULT;
		else {
			state = 1;	
			for (i = 0, mask = 2; i < MAX_NR_CONSOLES && mask;
							++i, mask <<= 1)
				if (VT_IS_IN_USE(i))
					state |= mask;
			ret = put_user(state, &vtstat->v_state);
		}
		break;
	}

	case VT_OPENQRY:
		
		for (i = 0; i < MAX_NR_CONSOLES; ++i)
			if (! VT_IS_IN_USE(i))
				break;
		uival = i < MAX_NR_CONSOLES ? (i+1) : -1;
		goto setint;		 

	case VT_ACTIVATE:
		if (!perm)
			return -EPERM;
		if (arg == 0 || arg > MAX_NR_CONSOLES)
			ret =  -ENXIO;
		else {
			arg--;
			console_lock();
			ret = vc_allocate(arg);
			console_unlock();
			if (ret)
				break;
			set_console(arg);
		}
		break;

	case VT_SETACTIVATE:
	{
		struct vt_setactivate vsa;

		if (!perm)
			return -EPERM;

		if (copy_from_user(&vsa, (struct vt_setactivate __user *)arg,
					sizeof(struct vt_setactivate))) {
			ret = -EFAULT;
			goto out;
		}
		if (vsa.console == 0 || vsa.console > MAX_NR_CONSOLES)
			ret = -ENXIO;
		else {
			vsa.console--;
			console_lock();
			ret = vc_allocate(vsa.console);
			if (ret == 0) {
				struct vc_data *nvc;
				nvc = vc_cons[vsa.console].d;
				nvc->vt_mode = vsa.mode;
				nvc->vt_mode.frsig = 0;
				put_pid(nvc->vt_pid);
				nvc->vt_pid = get_pid(task_pid(current));
			}
			console_unlock();
			if (ret)
				break;
			
			
			set_console(vsa.console);
		}
		break;
	}

	case VT_WAITACTIVE:
		if (!perm)
			return -EPERM;
		if (arg == 0 || arg > MAX_NR_CONSOLES)
			ret = -ENXIO;
		else
			ret = vt_waitactive(arg);
		break;

	case VT_RELDISP:
		if (!perm)
			return -EPERM;

		console_lock();
		if (vc->vt_mode.mode != VT_PROCESS) {
			console_unlock();
			ret = -EINVAL;
			break;
		}
		if (vc->vt_newvt >= 0) {
			if (arg == 0)
				vc->vt_newvt = -1;

			else {
				int newvt;
				newvt = vc->vt_newvt;
				vc->vt_newvt = -1;
				ret = vc_allocate(newvt);
				if (ret) {
					console_unlock();
					break;
				}
				complete_change_console(vc_cons[newvt].d);
			}
		} else {
			if (arg != VT_ACKACQ)
				ret = -EINVAL;
		}
		console_unlock();
		break;

	 case VT_DISALLOCATE:
		if (arg > MAX_NR_CONSOLES) {
			ret = -ENXIO;
			break;
		}
		if (arg == 0) {
		    
			console_lock();
			for (i=1; i<MAX_NR_CONSOLES; i++)
				if (! VT_BUSY(i))
					vc_deallocate(i);
			console_unlock();
		} else {
			
			arg--;
			if (VT_BUSY(arg))
				ret = -EBUSY;
			else if (arg) {			      
				console_lock();
				vc_deallocate(arg);
				console_unlock();
			}
		}
		break;

	case VT_RESIZE:
	{
		struct vt_sizes __user *vtsizes = up;
		struct vc_data *vc;

		ushort ll,cc;
		if (!perm)
			return -EPERM;
		if (get_user(ll, &vtsizes->v_rows) ||
		    get_user(cc, &vtsizes->v_cols))
			ret = -EFAULT;
		else {
			console_lock();
			for (i = 0; i < MAX_NR_CONSOLES; i++) {
				vc = vc_cons[i].d;

				if (vc) {
					vc->vc_resize_user = 1;
					
					vc_resize(vc_cons[i].d, cc, ll);
				}
			}
			console_unlock();
		}
		break;
	}

	case VT_RESIZEX:
	{
		struct vt_consize __user *vtconsize = up;
		ushort ll,cc,vlin,clin,vcol,ccol;
		if (!perm)
			return -EPERM;
		if (!access_ok(VERIFY_READ, vtconsize,
				sizeof(struct vt_consize))) {
			ret = -EFAULT;
			break;
		}
		
		__get_user(ll, &vtconsize->v_rows);
		__get_user(cc, &vtconsize->v_cols);
		__get_user(vlin, &vtconsize->v_vlin);
		__get_user(clin, &vtconsize->v_clin);
		__get_user(vcol, &vtconsize->v_vcol);
		__get_user(ccol, &vtconsize->v_ccol);
		vlin = vlin ? vlin : vc->vc_scan_lines;
		if (clin) {
			if (ll) {
				if (ll != vlin/clin) {
					
					ret = -EINVAL;
					break;
				}
			} else 
				ll = vlin/clin;
		}
		if (vcol && ccol) {
			if (cc) {
				if (cc != vcol/ccol) {
					ret = -EINVAL;
					break;
				}
			} else
				cc = vcol/ccol;
		}

		if (clin > 32) {
			ret =  -EINVAL;
			break;
		}
		    
		for (i = 0; i < MAX_NR_CONSOLES; i++) {
			if (!vc_cons[i].d)
				continue;
			console_lock();
			if (vlin)
				vc_cons[i].d->vc_scan_lines = vlin;
			if (clin)
				vc_cons[i].d->vc_font.height = clin;
			vc_cons[i].d->vc_resize_user = 1;
			vc_resize(vc_cons[i].d, cc, ll);
			console_unlock();
		}
		break;
	}

	case PIO_FONT: {
		if (!perm)
			return -EPERM;
		op.op = KD_FONT_OP_SET;
		op.flags = KD_FONT_FLAG_OLD | KD_FONT_FLAG_DONT_RECALC;	
		op.width = 8;
		op.height = 0;
		op.charcount = 256;
		op.data = up;
		ret = con_font_op(vc_cons[fg_console].d, &op);
		break;
	}

	case GIO_FONT: {
		op.op = KD_FONT_OP_GET;
		op.flags = KD_FONT_FLAG_OLD;
		op.width = 8;
		op.height = 32;
		op.charcount = 256;
		op.data = up;
		ret = con_font_op(vc_cons[fg_console].d, &op);
		break;
	}

	case PIO_CMAP:
                if (!perm)
			ret = -EPERM;
		else
	                ret = con_set_cmap(up);
		break;

	case GIO_CMAP:
                ret = con_get_cmap(up);
		break;

	case PIO_FONTX:
	case GIO_FONTX:
		ret = do_fontx_ioctl(cmd, up, perm, &op);
		break;

	case PIO_FONTRESET:
	{
		if (!perm)
			return -EPERM;

#ifdef BROKEN_GRAPHICS_PROGRAMS
		ret = -ENOSYS;
		break;
#else
		{
		op.op = KD_FONT_OP_SET_DEFAULT;
		op.data = NULL;
		ret = con_font_op(vc_cons[fg_console].d, &op);
		if (ret)
			break;
		con_set_default_unimap(vc_cons[fg_console].d);
		break;
		}
#endif
	}

	case KDFONTOP: {
		if (copy_from_user(&op, up, sizeof(op))) {
			ret = -EFAULT;
			break;
		}
		if (!perm && op.op != KD_FONT_OP_GET)
			return -EPERM;
		ret = con_font_op(vc, &op);
		if (ret)
			break;
		if (copy_to_user(up, &op, sizeof(op)))
			ret = -EFAULT;
		break;
	}

	case PIO_SCRNMAP:
		if (!perm)
			ret = -EPERM;
		else {
			tty_lock();
			ret = con_set_trans_old(up);
			tty_unlock();
		}
		break;

	case GIO_SCRNMAP:
		tty_lock();
		ret = con_get_trans_old(up);
		tty_unlock();
		break;

	case PIO_UNISCRNMAP:
		if (!perm)
			ret = -EPERM;
		else {
			tty_lock();
			ret = con_set_trans_new(up);
			tty_unlock();
		}
		break;

	case GIO_UNISCRNMAP:
		tty_lock();
		ret = con_get_trans_new(up);
		tty_unlock();
		break;

	case PIO_UNIMAPCLR:
	      { struct unimapinit ui;
		if (!perm)
			return -EPERM;
		ret = copy_from_user(&ui, up, sizeof(struct unimapinit));
		if (ret)
			ret = -EFAULT;
		else {
			tty_lock();
			con_clear_unimap(vc, &ui);
			tty_unlock();
		}
		break;
	      }

	case PIO_UNIMAP:
	case GIO_UNIMAP:
		tty_lock();
		ret = do_unimap_ioctl(cmd, up, perm, vc);
		tty_unlock();
		break;

	case VT_LOCKSWITCH:
		if (!capable(CAP_SYS_TTY_CONFIG))
			return -EPERM;
		vt_dont_switch = 1;
		break;
	case VT_UNLOCKSWITCH:
		if (!capable(CAP_SYS_TTY_CONFIG))
			return -EPERM;
		vt_dont_switch = 0;
		break;
	case VT_GETHIFONTMASK:
		ret = put_user(vc->vc_hi_font_mask,
					(unsigned short __user *)arg);
		break;
	case VT_WAITEVENT:
		ret = vt_event_wait_ioctl((struct vt_event __user *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}
out:
	return ret;
}

void reset_vc(struct vc_data *vc)
{
	vc->vc_mode = KD_TEXT;
	vt_reset_unicode(vc->vc_num);
	vc->vt_mode.mode = VT_AUTO;
	vc->vt_mode.waitv = 0;
	vc->vt_mode.relsig = 0;
	vc->vt_mode.acqsig = 0;
	vc->vt_mode.frsig = 0;
	put_pid(vc->vt_pid);
	vc->vt_pid = NULL;
	vc->vt_newvt = -1;
	if (!in_interrupt())    
		reset_palette(vc);
}

void vc_SAK(struct work_struct *work)
{
	struct vc *vc_con =
		container_of(work, struct vc, SAK_work);
	struct vc_data *vc;
	struct tty_struct *tty;

	console_lock();
	vc = vc_con->d;
	if (vc) {
		
		tty = vc->port.tty;
		if (tty)
			__do_SAK(tty);
		reset_vc(vc);
	}
	console_unlock();
}

#ifdef CONFIG_COMPAT

struct compat_consolefontdesc {
	unsigned short charcount;       
	unsigned short charheight;      
	compat_caddr_t chardata;	
};

static inline int
compat_fontx_ioctl(int cmd, struct compat_consolefontdesc __user *user_cfd,
			 int perm, struct console_font_op *op)
{
	struct compat_consolefontdesc cfdarg;
	int i;

	if (copy_from_user(&cfdarg, user_cfd, sizeof(struct compat_consolefontdesc)))
		return -EFAULT;

	switch (cmd) {
	case PIO_FONTX:
		if (!perm)
			return -EPERM;
		op->op = KD_FONT_OP_SET;
		op->flags = KD_FONT_FLAG_OLD;
		op->width = 8;
		op->height = cfdarg.charheight;
		op->charcount = cfdarg.charcount;
		op->data = compat_ptr(cfdarg.chardata);
		return con_font_op(vc_cons[fg_console].d, op);
	case GIO_FONTX:
		op->op = KD_FONT_OP_GET;
		op->flags = KD_FONT_FLAG_OLD;
		op->width = 8;
		op->height = cfdarg.charheight;
		op->charcount = cfdarg.charcount;
		op->data = compat_ptr(cfdarg.chardata);
		i = con_font_op(vc_cons[fg_console].d, op);
		if (i)
			return i;
		cfdarg.charheight = op->height;
		cfdarg.charcount = op->charcount;
		if (copy_to_user(user_cfd, &cfdarg, sizeof(struct compat_consolefontdesc)))
			return -EFAULT;
		return 0;
	}
	return -EINVAL;
}

struct compat_console_font_op {
	compat_uint_t op;        
	compat_uint_t flags;     
	compat_uint_t width, height;     
	compat_uint_t charcount;
	compat_caddr_t data;    
};

static inline int
compat_kdfontop_ioctl(struct compat_console_font_op __user *fontop,
			 int perm, struct console_font_op *op, struct vc_data *vc)
{
	int i;

	if (copy_from_user(op, fontop, sizeof(struct compat_console_font_op)))
		return -EFAULT;
	if (!perm && op->op != KD_FONT_OP_GET)
		return -EPERM;
	op->data = compat_ptr(((struct compat_console_font_op *)op)->data);
	i = con_font_op(vc, op);
	if (i)
		return i;
	((struct compat_console_font_op *)op)->data = (unsigned long)op->data;
	if (copy_to_user(fontop, op, sizeof(struct compat_console_font_op)))
		return -EFAULT;
	return 0;
}

struct compat_unimapdesc {
	unsigned short entry_ct;
	compat_caddr_t entries;
};

static inline int
compat_unimap_ioctl(unsigned int cmd, struct compat_unimapdesc __user *user_ud,
			 int perm, struct vc_data *vc)
{
	struct compat_unimapdesc tmp;
	struct unipair __user *tmp_entries;

	if (copy_from_user(&tmp, user_ud, sizeof tmp))
		return -EFAULT;
	tmp_entries = compat_ptr(tmp.entries);
	if (tmp_entries)
		if (!access_ok(VERIFY_WRITE, tmp_entries,
				tmp.entry_ct*sizeof(struct unipair)))
			return -EFAULT;
	switch (cmd) {
	case PIO_UNIMAP:
		if (!perm)
			return -EPERM;
		return con_set_unimap(vc, tmp.entry_ct, tmp_entries);
	case GIO_UNIMAP:
		if (!perm && fg_console != vc->vc_num)
			return -EPERM;
		return con_get_unimap(vc, tmp.entry_ct, &(user_ud->entry_ct), tmp_entries);
	}
	return 0;
}

long vt_compat_ioctl(struct tty_struct *tty,
	     unsigned int cmd, unsigned long arg)
{
	struct vc_data *vc = tty->driver_data;
	struct console_font_op op;	
	unsigned int console;
	void __user *up = (void __user *)arg;
	int perm;
	int ret = 0;

	console = vc->vc_num;

	if (!vc_cons_allocated(console)) { 	
		ret = -ENOIOCTLCMD;
		goto out;
	}

	perm = 0;
	if (current->signal->tty == tty || capable(CAP_SYS_TTY_CONFIG))
		perm = 1;

	switch (cmd) {
	case PIO_FONTX:
	case GIO_FONTX:
		ret = compat_fontx_ioctl(cmd, up, perm, &op);
		break;

	case KDFONTOP:
		ret = compat_kdfontop_ioctl(up, perm, &op, vc);
		break;

	case PIO_UNIMAP:
	case GIO_UNIMAP:
		tty_lock();
		ret = compat_unimap_ioctl(cmd, up, perm, vc);
		tty_unlock();
		break;

	case KIOCSOUND:
	case KDMKTONE:
#ifdef CONFIG_X86
	case KDADDIO:
	case KDDELIO:
#endif
	case KDSETMODE:
	case KDMAPDISP:
	case KDUNMAPDISP:
	case KDSKBMODE:
	case KDSKBMETA:
	case KDSKBLED:
	case KDSETLED:
	case KDSIGACCEPT:
	case VT_ACTIVATE:
	case VT_WAITACTIVE:
	case VT_RELDISP:
	case VT_DISALLOCATE:
	case VT_RESIZE:
	case VT_RESIZEX:
		goto fallback;

	default:
		arg = (unsigned long)compat_ptr(arg);
		goto fallback;
	}
out:
	return ret;

fallback:
	return vt_ioctl(tty, cmd, arg);
}


#endif 


static void complete_change_console(struct vc_data *vc)
{
	unsigned char old_vc_mode;
	int old = fg_console;

	last_console = fg_console;

	old_vc_mode = vc_cons[fg_console].d->vc_mode;
	switch_screen(vc);

	if (old_vc_mode != vc->vc_mode) {
		if (vc->vc_mode == KD_TEXT)
			do_unblank_screen(1);
		else
			do_blank_screen(1);
	}

	if (vc->vt_mode.mode == VT_PROCESS) {
		if (kill_pid(vc->vt_pid, vc->vt_mode.acqsig, 1) != 0) {
			reset_vc(vc);

			if (old_vc_mode != vc->vc_mode) {
				if (vc->vc_mode == KD_TEXT)
					do_unblank_screen(1);
				else
					do_blank_screen(1);
			}
		}
	}

	vt_event_post(VT_EVENT_SWITCH, old, vc->vc_num);
	return;
}

void change_console(struct vc_data *new_vc)
{
	struct vc_data *vc;

	if (!new_vc || new_vc->vc_num == fg_console || vt_dont_switch)
		return;

	vc = vc_cons[fg_console].d;
	if (vc->vt_mode.mode == VT_PROCESS) {
		vc->vt_newvt = new_vc->vc_num;
		if (kill_pid(vc->vt_pid, vc->vt_mode.relsig, 1) == 0) {
			return;
		}

		reset_vc(vc);

	}

	if (vc->vc_mode == KD_GRAPHICS)
		return;

	complete_change_console(new_vc);
}


static int disable_vt_switch;

int vt_move_to_console(unsigned int vt, int alloc)
{
	int prev;

	console_lock();
	
	if (disable_vt_switch) {
		console_unlock();
		return 0;
	}
	prev = fg_console;

	if (alloc && vc_allocate(vt)) {
		console_unlock();
		return -ENOSPC;
	}

	if (set_console(vt)) {
		console_unlock();
		return -EIO;
	}
	console_unlock();
	if (vt_waitactive(vt + 1)) {
		pr_debug("Suspend: Can't switch VCs.");
		return -EINTR;
	}
	return prev;
}

void pm_set_vt_switch(int do_switch)
{
	console_lock();
	disable_vt_switch = !do_switch;
	console_unlock();
}
EXPORT_SYMBOL(pm_set_vt_switch);
