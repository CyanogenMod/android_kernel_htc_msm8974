/*
 *  drivers/s390/char/tty3270.c
 *    IBM/3270 Driver - tty functions.
 *
 *  Author(s):
 *    Original 3270 Code for 2.4 written by Richard Hitt (UTS Global)
 *    Rewritten for 2.5 by Martin Schwidefsky <schwidefsky@de.ibm.com>
 *	-- Copyright (C) 2003 IBM Deutschland Entwicklung GmbH, IBM Corporation
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/interrupt.h>

#include <linux/slab.h>
#include <linux/bootmem.h>
#include <linux/compat.h>

#include <asm/ccwdev.h>
#include <asm/cio.h>
#include <asm/ebcdic.h>
#include <asm/uaccess.h>

#include "raw3270.h"
#include "tty3270.h"
#include "keyboard.h"

#define TTY3270_CHAR_BUF_SIZE 256
#define TTY3270_OUTPUT_BUFFER_SIZE 1024
#define TTY3270_STRING_PAGES 5

struct tty_driver *tty3270_driver;
static int tty3270_max_index;

static struct raw3270_fn tty3270_fn;

struct tty3270_cell {
	unsigned char character;
	unsigned char highlight;
	unsigned char f_color;
};

struct tty3270_line {
	struct tty3270_cell *cells;
	int len;
};

#define ESCAPE_NPAR 8

struct tty3270 {
	struct raw3270_view view;
	struct tty_struct *tty;		
	void **freemem_pages;		
	struct list_head freemem;	

	
	struct list_head lines;		
	struct list_head update;	
	unsigned char wcc;		
	int nr_lines;			
	int nr_up;			
	unsigned long update_flags;	
	struct string *status;		
	struct raw3270_request *write;	
	struct timer_list timer;	

	
	unsigned int cx, cy;		
	unsigned int highlight;		
	unsigned int f_color;		
	struct tty3270_line *screen;

	
	struct string *prompt;		
	struct string *input;		
	struct raw3270_request *read;	
	struct raw3270_request *kreset;	
	unsigned char inattr;		
	int throttle, attn;		
	struct tasklet_struct readlet;	
	struct kbd_data *kbd;		

	
	int esc_state, esc_ques, esc_npar;
	int esc_par[ESCAPE_NPAR];
	unsigned int saved_cx, saved_cy;
	unsigned int saved_highlight, saved_f_color;

	
	struct list_head rcl_lines;	
	struct list_head *rcl_walk;	
	int rcl_nr, rcl_max;		

	
	unsigned int char_count;
	char char_buf[TTY3270_CHAR_BUF_SIZE];
};

#define TTY_UPDATE_ERASE	1	
#define TTY_UPDATE_LIST		2	
#define TTY_UPDATE_INPUT	4	
#define TTY_UPDATE_STATUS	8	
#define TTY_UPDATE_ALL		16	

static void tty3270_update(struct tty3270 *);

static void tty3270_set_timer(struct tty3270 *tp, int expires)
{
	if (expires == 0)
		del_timer(&tp->timer);
	else
		mod_timer(&tp->timer, jiffies + expires);
}

static void
tty3270_update_prompt(struct tty3270 *tp, char *input, int count)
{
	struct string *line;
	unsigned int off;

	line = tp->prompt;
	if (count != 0)
		line->string[5] = TF_INMDT;
	else
		line->string[5] = tp->inattr;
	if (count > tp->view.cols * 2 - 11)
		count = tp->view.cols * 2 - 11;
	memcpy(line->string + 6, input, count);
	line->string[6 + count] = TO_IC;
	
	if (count < tp->view.cols * 2 - 11) {
		line->string[7 + count] = TO_RA;
		line->string[10 + count] = 0;
		off = tp->view.cols * tp->view.rows - 9;
		raw3270_buffer_address(tp->view.dev, line->string+count+8, off);
		line->len = 11 + count;
	} else
		line->len = 7 + count;
	tp->update_flags |= TTY_UPDATE_INPUT;
}

static void
tty3270_create_prompt(struct tty3270 *tp)
{
	static const unsigned char blueprint[] =
		{ TO_SBA, 0, 0, 0x6e, TO_SF, TF_INPUT,
		  
		  TO_IC, TO_RA, 0, 0, 0 };
	struct string *line;
	unsigned int offset;

	line = alloc_string(&tp->freemem,
			    sizeof(blueprint) + tp->view.cols * 2 - 9);
	tp->prompt = line;
	tp->inattr = TF_INPUT;
	
	memcpy(line->string, blueprint, sizeof(blueprint));
	line->len = sizeof(blueprint);
	
	offset = tp->view.cols * (tp->view.rows - 2);
	raw3270_buffer_address(tp->view.dev, line->string + 1, offset);
	offset = tp->view.cols * tp->view.rows - 9;
	raw3270_buffer_address(tp->view.dev, line->string + 8, offset);

	
	tp->input = alloc_string(&tp->freemem, tp->view.cols * 2 - 9 + 6);
}

static void
tty3270_update_status(struct tty3270 * tp)
{
	char *str;

	str = (tp->nr_up != 0) ? "History" : "Running";
	memcpy(tp->status->string + 8, str, 7);
	codepage_convert(tp->view.ascebc, tp->status->string + 8, 7);
	tp->update_flags |= TTY_UPDATE_STATUS;
}

static void
tty3270_create_status(struct tty3270 * tp)
{
	static const unsigned char blueprint[] =
		{ TO_SBA, 0, 0, TO_SF, TF_LOG, TO_SA, TAT_COLOR, TAC_GREEN,
		  0, 0, 0, 0, 0, 0, 0, TO_SF, TF_LOG, TO_SA, TAT_COLOR,
		  TAC_RESET };
	struct string *line;
	unsigned int offset;

	line = alloc_string(&tp->freemem,sizeof(blueprint));
	tp->status = line;
	
	memcpy(line->string, blueprint, sizeof(blueprint));
	
	offset = tp->view.cols * tp->view.rows - 9;
	raw3270_buffer_address(tp->view.dev, line->string + 1, offset);
}

static void
tty3270_update_string(struct tty3270 *tp, struct string *line, int nr)
{
	unsigned char *cp;

	raw3270_buffer_address(tp->view.dev, line->string + 1,
			       tp->view.cols * nr);
	cp = line->string + line->len - 4;
	if (*cp == TO_RA)
		raw3270_buffer_address(tp->view.dev, cp + 1,
				       tp->view.cols * (nr + 1));
}

static void
tty3270_rebuild_update(struct tty3270 *tp)
{
	struct string *s, *n;
	int line, nr_up;

	list_for_each_entry_safe(s, n, &tp->update, update)
		list_del_init(&s->update);
	line = tp->view.rows - 3;
	nr_up = tp->nr_up;
	list_for_each_entry_reverse(s, &tp->lines, list) {
		if (nr_up > 0) {
			nr_up--;
			continue;
		}
		tty3270_update_string(tp, s, line);
		list_add(&s->update, &tp->update);
		if (--line < 0)
			break;
	}
	tp->update_flags |= TTY_UPDATE_LIST;
}

static struct string *
tty3270_alloc_string(struct tty3270 *tp, size_t size)
{
	struct string *s, *n;

	s = alloc_string(&tp->freemem, size);
	if (s)
		return s;
	list_for_each_entry_safe(s, n, &tp->lines, list) {
		BUG_ON(tp->nr_lines <= tp->view.rows - 2);
		list_del(&s->list);
		if (!list_empty(&s->update))
			list_del(&s->update);
		tp->nr_lines--;
		if (free_string(&tp->freemem, s) >= size)
			break;
	}
	s = alloc_string(&tp->freemem, size);
	BUG_ON(!s);
	if (tp->nr_up != 0 &&
	    tp->nr_up + tp->view.rows - 2 >= tp->nr_lines) {
		tp->nr_up = tp->nr_lines - tp->view.rows + 2;
		tty3270_rebuild_update(tp);
		tty3270_update_status(tp);
	}
	return s;
}

static void
tty3270_blank_line(struct tty3270 *tp)
{
	static const unsigned char blueprint[] =
		{ TO_SBA, 0, 0, TO_SA, TAT_EXTHI, TAX_RESET,
		  TO_SA, TAT_COLOR, TAC_RESET, TO_RA, 0, 0, 0 };
	struct string *s;

	s = tty3270_alloc_string(tp, sizeof(blueprint));
	memcpy(s->string, blueprint, sizeof(blueprint));
	s->len = sizeof(blueprint);
	list_add_tail(&s->list, &tp->lines);
	tp->nr_lines++;
	if (tp->nr_up != 0)
		tp->nr_up++;
}

static void
tty3270_write_callback(struct raw3270_request *rq, void *data)
{
	struct tty3270 *tp;

	tp = (struct tty3270 *) rq->view;
	if (rq->rc != 0) {
		
		tp->update_flags = TTY_UPDATE_ALL;
		tty3270_set_timer(tp, 1);
	}
	raw3270_request_reset(rq);
	xchg(&tp->write, rq);
}

static void
tty3270_update(struct tty3270 *tp)
{
	static char invalid_sba[2] = { 0xff, 0xff };
	struct raw3270_request *wrq;
	unsigned long updated;
	struct string *s, *n;
	char *sba, *str;
	int rc, len;

	wrq = xchg(&tp->write, 0);
	if (!wrq) {
		tty3270_set_timer(tp, 1);
		return;
	}

	spin_lock(&tp->view.lock);
	updated = 0;
	if (tp->update_flags & TTY_UPDATE_ALL) {
		tty3270_rebuild_update(tp);
		tty3270_update_status(tp);
		tp->update_flags = TTY_UPDATE_ERASE | TTY_UPDATE_LIST |
			TTY_UPDATE_INPUT | TTY_UPDATE_STATUS;
	}
	if (tp->update_flags & TTY_UPDATE_ERASE) {
		
		raw3270_request_set_cmd(wrq, TC_EWRITEA);
		updated |= TTY_UPDATE_ERASE;
	} else
		raw3270_request_set_cmd(wrq, TC_WRITE);

	raw3270_request_add_data(wrq, &tp->wcc, 1);
	tp->wcc = TW_NONE;

	if (tp->update_flags & TTY_UPDATE_STATUS)
		if (raw3270_request_add_data(wrq, tp->status->string,
					     tp->status->len) == 0)
			updated |= TTY_UPDATE_STATUS;

	if (tp->update_flags & TTY_UPDATE_INPUT)
		if (raw3270_request_add_data(wrq, tp->prompt->string,
					     tp->prompt->len) == 0)
			updated |= TTY_UPDATE_INPUT;

	sba = invalid_sba;
	
	if (tp->update_flags & TTY_UPDATE_LIST) {
		
		list_for_each_entry_safe(s, n, &tp->update, update) {
			str = s->string;
			len = s->len;
			if (s->string[1] == sba[0] && s->string[2] == sba[1])
				str += 3, len -= 3;
			if (raw3270_request_add_data(wrq, str, len) != 0)
				break;
			list_del_init(&s->update);
			sba = s->string + s->len - 3;
		}
		if (list_empty(&tp->update))
			updated |= TTY_UPDATE_LIST;
	}
	wrq->callback = tty3270_write_callback;
	rc = raw3270_start(&tp->view, wrq);
	if (rc == 0) {
		tp->update_flags &= ~updated;
		if (tp->update_flags)
			tty3270_set_timer(tp, 1);
	} else {
		raw3270_request_reset(wrq);
		xchg(&tp->write, wrq);
	}
	spin_unlock(&tp->view.lock);
}

static void
tty3270_rcl_add(struct tty3270 *tp, char *input, int len)
{
	struct string *s;

	tp->rcl_walk = NULL;
	if (len <= 0)
		return;
	if (tp->rcl_nr >= tp->rcl_max) {
		s = list_entry(tp->rcl_lines.next, struct string, list);
		list_del(&s->list);
		free_string(&tp->freemem, s);
		tp->rcl_nr--;
	}
	s = tty3270_alloc_string(tp, len);
	memcpy(s->string, input, len);
	list_add_tail(&s->list, &tp->rcl_lines);
	tp->rcl_nr++;
}

static void
tty3270_rcl_backward(struct kbd_data *kbd)
{
	struct tty3270 *tp;
	struct string *s;

	tp = kbd->tty->driver_data;
	spin_lock_bh(&tp->view.lock);
	if (tp->inattr == TF_INPUT) {
		if (tp->rcl_walk && tp->rcl_walk->prev != &tp->rcl_lines)
			tp->rcl_walk = tp->rcl_walk->prev;
		else if (!list_empty(&tp->rcl_lines))
			tp->rcl_walk = tp->rcl_lines.prev;
		s = tp->rcl_walk ? 
			list_entry(tp->rcl_walk, struct string, list) : NULL;
		if (tp->rcl_walk) {
			s = list_entry(tp->rcl_walk, struct string, list);
			tty3270_update_prompt(tp, s->string, s->len);
		} else
			tty3270_update_prompt(tp, NULL, 0);
		tty3270_set_timer(tp, 1);
	}
	spin_unlock_bh(&tp->view.lock);
}

static void
tty3270_exit_tty(struct kbd_data *kbd)
{
	struct tty3270 *tp;

	tp = kbd->tty->driver_data;
	raw3270_deactivate_view(&tp->view);
}

static void
tty3270_scroll_forward(struct kbd_data *kbd)
{
	struct tty3270 *tp;
	int nr_up;

	tp = kbd->tty->driver_data;
	spin_lock_bh(&tp->view.lock);
	nr_up = tp->nr_up - tp->view.rows + 2;
	if (nr_up < 0)
		nr_up = 0;
	if (nr_up != tp->nr_up) {
		tp->nr_up = nr_up;
		tty3270_rebuild_update(tp);
		tty3270_update_status(tp);
		tty3270_set_timer(tp, 1);
	}
	spin_unlock_bh(&tp->view.lock);
}

static void
tty3270_scroll_backward(struct kbd_data *kbd)
{
	struct tty3270 *tp;
	int nr_up;

	tp = kbd->tty->driver_data;
	spin_lock_bh(&tp->view.lock);
	nr_up = tp->nr_up + tp->view.rows - 2;
	if (nr_up + tp->view.rows - 2 > tp->nr_lines)
		nr_up = tp->nr_lines - tp->view.rows + 2;
	if (nr_up != tp->nr_up) {
		tp->nr_up = nr_up;
		tty3270_rebuild_update(tp);
		tty3270_update_status(tp);
		tty3270_set_timer(tp, 1);
	}
	spin_unlock_bh(&tp->view.lock);
}

static void
tty3270_read_tasklet(struct raw3270_request *rrq)
{
	static char kreset_data = TW_KR;
	struct tty3270 *tp;
	char *input;
	int len;

	tp = (struct tty3270 *) rrq->view;
	spin_lock_bh(&tp->view.lock);
	input = NULL;
	len = 0;
	if (tp->input->string[0] == 0x7d) {
		
		input = tp->input->string + 6;
		len = tp->input->len - 6 - rrq->rescnt;
		if (tp->inattr != TF_INPUTN)
			tty3270_rcl_add(tp, input, len);
		if (tp->nr_up > 0) {
			tp->nr_up = 0;
			tty3270_rebuild_update(tp);
			tty3270_update_status(tp);
		}
		
		tty3270_update_prompt(tp, NULL, 0);
		tty3270_set_timer(tp, 1);
	} else if (tp->input->string[0] == 0x6d) {
		
		tp->update_flags = TTY_UPDATE_ALL;
		tty3270_set_timer(tp, 1);
	}
	spin_unlock_bh(&tp->view.lock);

	
	raw3270_request_reset(tp->kreset);
	raw3270_request_set_cmd(tp->kreset, TC_WRITE);
	raw3270_request_add_data(tp->kreset, &kreset_data, 1);
	raw3270_start(&tp->view, tp->kreset);

	
	if (tp->tty) {
		while (len-- > 0)
			kbd_keycode(tp->kbd, *input++);
		
		kbd_keycode(tp->kbd, 256 + tp->input->string[0]);
	}

	raw3270_request_reset(rrq);
	xchg(&tp->read, rrq);
	raw3270_put_view(&tp->view);
}

static void
tty3270_read_callback(struct raw3270_request *rq, void *data)
{
	raw3270_get_view(rq->view);
	
	tasklet_schedule(&((struct tty3270 *) rq->view)->readlet);
}

static void
tty3270_issue_read(struct tty3270 *tp, int lock)
{
	struct raw3270_request *rrq;
	int rc;

	rrq = xchg(&tp->read, 0);
	if (!rrq)
		
		return;
	rrq->callback = tty3270_read_callback;
	rrq->callback_data = tp;
	raw3270_request_set_cmd(rrq, TC_READMOD);
	raw3270_request_set_data(rrq, tp->input->string, tp->input->len);
	
	if (lock) {
		rc = raw3270_start(&tp->view, rrq);
	} else
		rc = raw3270_start_irq(&tp->view, rrq);
	if (rc) {
		raw3270_request_reset(rrq);
		xchg(&tp->read, rrq);
	}
}

static int
tty3270_activate(struct raw3270_view *view)
{
	struct tty3270 *tp;

	tp = (struct tty3270 *) view;
	tp->update_flags = TTY_UPDATE_ALL;
	tty3270_set_timer(tp, 1);
	return 0;
}

static void
tty3270_deactivate(struct raw3270_view *view)
{
	struct tty3270 *tp;

	tp = (struct tty3270 *) view;
	del_timer(&tp->timer);
}

static int
tty3270_irq(struct tty3270 *tp, struct raw3270_request *rq, struct irb *irb)
{
	
	if (irb->scsw.cmd.dstat & DEV_STAT_ATTENTION) {
		if (!tp->throttle)
			tty3270_issue_read(tp, 0);
		else
			tp->attn = 1;
	}

	if (rq) {
		if (irb->scsw.cmd.dstat & DEV_STAT_UNIT_CHECK)
			rq->rc = -EIO;
		else
			
			rq->rescnt = irb->scsw.cmd.count;
	}
	return RAW3270_IO_DONE;
}

static struct tty3270 *
tty3270_alloc_view(void)
{
	struct tty3270 *tp;
	int pages;

	tp = kzalloc(sizeof(struct tty3270), GFP_KERNEL);
	if (!tp)
		goto out_err;
	tp->freemem_pages =
		kmalloc(sizeof(void *) * TTY3270_STRING_PAGES, GFP_KERNEL);
	if (!tp->freemem_pages)
		goto out_tp;
	INIT_LIST_HEAD(&tp->freemem);
	for (pages = 0; pages < TTY3270_STRING_PAGES; pages++) {
		tp->freemem_pages[pages] = (void *)
			__get_free_pages(GFP_KERNEL|GFP_DMA, 0);
		if (!tp->freemem_pages[pages])
			goto out_pages;
		add_string_memory(&tp->freemem,
				  tp->freemem_pages[pages], PAGE_SIZE);
	}
	tp->write = raw3270_request_alloc(TTY3270_OUTPUT_BUFFER_SIZE);
	if (IS_ERR(tp->write))
		goto out_pages;
	tp->read = raw3270_request_alloc(0);
	if (IS_ERR(tp->read))
		goto out_write;
	tp->kreset = raw3270_request_alloc(1);
	if (IS_ERR(tp->kreset))
		goto out_read;
	tp->kbd = kbd_alloc();
	if (!tp->kbd)
		goto out_reset;
	return tp;

out_reset:
	raw3270_request_free(tp->kreset);
out_read:
	raw3270_request_free(tp->read);
out_write:
	raw3270_request_free(tp->write);
out_pages:
	while (pages--)
		free_pages((unsigned long) tp->freemem_pages[pages], 0);
	kfree(tp->freemem_pages);
out_tp:
	kfree(tp);
out_err:
	return ERR_PTR(-ENOMEM);
}

static void
tty3270_free_view(struct tty3270 *tp)
{
	int pages;

	del_timer_sync(&tp->timer);
	kbd_free(tp->kbd);
	raw3270_request_free(tp->kreset);
	raw3270_request_free(tp->read);
	raw3270_request_free(tp->write);
	for (pages = 0; pages < TTY3270_STRING_PAGES; pages++)
		free_pages((unsigned long) tp->freemem_pages[pages], 0);
	kfree(tp->freemem_pages);
	kfree(tp);
}

static int
tty3270_alloc_screen(struct tty3270 *tp)
{
	unsigned long size;
	int lines;

	size = sizeof(struct tty3270_line) * (tp->view.rows - 2);
	tp->screen = kzalloc(size, GFP_KERNEL);
	if (!tp->screen)
		goto out_err;
	for (lines = 0; lines < tp->view.rows - 2; lines++) {
		size = sizeof(struct tty3270_cell) * tp->view.cols;
		tp->screen[lines].cells = kzalloc(size, GFP_KERNEL);
		if (!tp->screen[lines].cells)
			goto out_screen;
	}
	return 0;
out_screen:
	while (lines--)
		kfree(tp->screen[lines].cells);
	kfree(tp->screen);
out_err:
	return -ENOMEM;
}

static void
tty3270_free_screen(struct tty3270 *tp)
{
	int lines;

	for (lines = 0; lines < tp->view.rows - 2; lines++)
		kfree(tp->screen[lines].cells);
	kfree(tp->screen);
}

static void
tty3270_release(struct raw3270_view *view)
{
	struct tty3270 *tp;
	struct tty_struct *tty;

	tp = (struct tty3270 *) view;
	tty = tp->tty;
	if (tty) {
		tty->driver_data = NULL;
		tp->tty = tp->kbd->tty = NULL;
		tty_hangup(tty);
		raw3270_put_view(&tp->view);
	}
}

static void
tty3270_free(struct raw3270_view *view)
{
	tty3270_free_screen((struct tty3270 *) view);
	tty3270_free_view((struct tty3270 *) view);
}

static void
tty3270_del_views(void)
{
	struct tty3270 *tp;
	int i;

	for (i = 0; i < tty3270_max_index; i++) {
		tp = (struct tty3270 *)
			raw3270_find_view(&tty3270_fn, i + RAW3270_FIRSTMINOR);
		if (!IS_ERR(tp))
			raw3270_del_view(&tp->view);
	}
}

static struct raw3270_fn tty3270_fn = {
	.activate = tty3270_activate,
	.deactivate = tty3270_deactivate,
	.intv = (void *) tty3270_irq,
	.release = tty3270_release,
	.free = tty3270_free
};

static int
tty3270_open(struct tty_struct *tty, struct file * filp)
{
	struct tty3270 *tp;
	int i, rc;

	if (tty->count > 1)
		return 0;
	
	tp = (struct tty3270 *)
		raw3270_find_view(&tty3270_fn,
				  tty->index + RAW3270_FIRSTMINOR);
	if (!IS_ERR(tp)) {
		tty->driver_data = tp;
		tty->winsize.ws_row = tp->view.rows - 2;
		tty->winsize.ws_col = tp->view.cols;
		tty->low_latency = 0;
		tp->tty = tty;
		tp->kbd->tty = tty;
		tp->inattr = TF_INPUT;
		return 0;
	}
	if (tty3270_max_index < tty->index + 1)
		tty3270_max_index = tty->index + 1;

	
	if (PTR_ERR(tp) == -ENODEV)
		return -ENODEV;

	
	tp = tty3270_alloc_view();
	if (IS_ERR(tp))
		return PTR_ERR(tp);

	INIT_LIST_HEAD(&tp->lines);
	INIT_LIST_HEAD(&tp->update);
	INIT_LIST_HEAD(&tp->rcl_lines);
	tp->rcl_max = 20;
	setup_timer(&tp->timer, (void (*)(unsigned long)) tty3270_update,
		    (unsigned long) tp);
	tasklet_init(&tp->readlet, 
		     (void (*)(unsigned long)) tty3270_read_tasklet,
		     (unsigned long) tp->read);

	rc = raw3270_add_view(&tp->view, &tty3270_fn,
			      tty->index + RAW3270_FIRSTMINOR);
	if (rc) {
		tty3270_free_view(tp);
		return rc;
	}

	rc = tty3270_alloc_screen(tp);
	if (rc) {
		raw3270_put_view(&tp->view);
		raw3270_del_view(&tp->view);
		return rc;
	}

	tp->tty = tty;
	tty->low_latency = 0;
	tty->driver_data = tp;
	tty->winsize.ws_row = tp->view.rows - 2;
	tty->winsize.ws_col = tp->view.cols;

	tty3270_create_prompt(tp);
	tty3270_create_status(tp);
	tty3270_update_status(tp);

	
	for (i = 0; i < tp->view.rows - 2; i++)
		tty3270_blank_line(tp);

	tp->kbd->tty = tty;
	tp->kbd->fn_handler[KVAL(K_INCRCONSOLE)] = tty3270_exit_tty;
	tp->kbd->fn_handler[KVAL(K_SCROLLBACK)] = tty3270_scroll_backward;
	tp->kbd->fn_handler[KVAL(K_SCROLLFORW)] = tty3270_scroll_forward;
	tp->kbd->fn_handler[KVAL(K_CONS)] = tty3270_rcl_backward;
	kbd_ascebc(tp->kbd, tp->view.ascebc);

	raw3270_activate_view(&tp->view);
	return 0;
}

static void
tty3270_close(struct tty_struct *tty, struct file * filp)
{
	struct tty3270 *tp;

	if (tty->count > 1)
		return;
	tp = (struct tty3270 *) tty->driver_data;
	if (tp) {
		tty->driver_data = NULL;
		tp->tty = tp->kbd->tty = NULL;
		raw3270_put_view(&tp->view);
	}
}

static int
tty3270_write_room(struct tty_struct *tty)
{
	return INT_MAX;
}

static void tty3270_put_character(struct tty3270 *tp, char ch)
{
	struct tty3270_line *line;
	struct tty3270_cell *cell;

	line = tp->screen + tp->cy;
	if (line->len <= tp->cx) {
		while (line->len < tp->cx) {
			cell = line->cells + line->len;
			cell->character = tp->view.ascebc[' '];
			cell->highlight = tp->highlight;
			cell->f_color = tp->f_color;
			line->len++;
		}
		line->len++;
	}
	cell = line->cells + tp->cx;
	cell->character = tp->view.ascebc[(unsigned int) ch];
	cell->highlight = tp->highlight;
	cell->f_color = tp->f_color;
}

static void
tty3270_convert_line(struct tty3270 *tp, int line_nr)
{
	struct tty3270_line *line;
	struct tty3270_cell *cell;
	struct string *s, *n;
	unsigned char highlight;
	unsigned char f_color;
	char *cp;
	int flen, i;

	
	flen = 3;		
	line = tp->screen + line_nr;
	flen += line->len;
	highlight = TAX_RESET;
	f_color = TAC_RESET;
	for (i = 0, cell = line->cells; i < line->len; i++, cell++) {
		if (cell->highlight != highlight) {
			flen += 3;	
			highlight = cell->highlight;
		}
		if (cell->f_color != f_color) {
			flen += 3;	
			f_color = cell->f_color;
		}
	}
	if (highlight != TAX_RESET)
		flen += 3;	
	if (f_color != TAC_RESET)
		flen += 3;	
	if (line->len < tp->view.cols)
		flen += 4;	

	
	i = tp->view.rows - 2 - line_nr;
	list_for_each_entry_reverse(s, &tp->lines, list)
		if (--i <= 0)
			break;
	if (s->len != flen) {
		
		n = tty3270_alloc_string(tp, flen);
		list_add(&n->list, &s->list);
		list_del_init(&s->list);
		if (!list_empty(&s->update))
			list_del_init(&s->update);
		free_string(&tp->freemem, s);
		s = n;
	}

	
	cp = s->string;
	*cp++ = TO_SBA;
	*cp++ = 0;
	*cp++ = 0;

	highlight = TAX_RESET;
	f_color = TAC_RESET;
	for (i = 0, cell = line->cells; i < line->len; i++, cell++) {
		if (cell->highlight != highlight) {
			*cp++ = TO_SA;
			*cp++ = TAT_EXTHI;
			*cp++ = cell->highlight;
			highlight = cell->highlight;
		}
		if (cell->f_color != f_color) {
			*cp++ = TO_SA;
			*cp++ = TAT_COLOR;
			*cp++ = cell->f_color;
			f_color = cell->f_color;
		}
		*cp++ = cell->character;
	}
	if (highlight != TAX_RESET) {
		*cp++ = TO_SA;
		*cp++ = TAT_EXTHI;
		*cp++ = TAX_RESET;
	}
	if (f_color != TAC_RESET) {
		*cp++ = TO_SA;
		*cp++ = TAT_COLOR;
		*cp++ = TAC_RESET;
	}
	if (line->len < tp->view.cols) {
		*cp++ = TO_RA;
		*cp++ = 0;
		*cp++ = 0;
		*cp++ = 0;
	}

	if (tp->nr_up + line_nr < tp->view.rows - 2) {
		
		tty3270_update_string(tp, s, line_nr);
		
		if (list_empty(&s->update)) {
			list_add_tail(&s->update, &tp->update);
			tp->update_flags |= TTY_UPDATE_LIST;
		}
	}
}

static void
tty3270_cr(struct tty3270 *tp)
{
	tp->cx = 0;
}

static void
tty3270_lf(struct tty3270 *tp)
{
	struct tty3270_line temp;
	int i;

	tty3270_convert_line(tp, tp->cy);
	if (tp->cy < tp->view.rows - 3) {
		tp->cy++;
		return;
	}
	
	tty3270_blank_line(tp);
	temp = tp->screen[0];
	temp.len = 0;
	for (i = 0; i < tp->view.rows - 3; i++)
		tp->screen[i] = tp->screen[i+1];
	tp->screen[tp->view.rows - 3] = temp;
	tty3270_rebuild_update(tp);
}

static void
tty3270_ri(struct tty3270 *tp)
{
	if (tp->cy > 0) {
	    tty3270_convert_line(tp, tp->cy);
	    tp->cy--;
	}
}

static void
tty3270_insert_characters(struct tty3270 *tp, int n)
{
	struct tty3270_line *line;
	int k;

	line = tp->screen + tp->cy;
	while (line->len < tp->cx) {
		line->cells[line->len].character = tp->view.ascebc[' '];
		line->cells[line->len].highlight = TAX_RESET;
		line->cells[line->len].f_color = TAC_RESET;
		line->len++;
	}
	if (n > tp->view.cols - tp->cx)
		n = tp->view.cols - tp->cx;
	k = min_t(int, line->len - tp->cx, tp->view.cols - tp->cx - n);
	while (k--)
		line->cells[tp->cx + n + k] = line->cells[tp->cx + k];
	line->len += n;
	if (line->len > tp->view.cols)
		line->len = tp->view.cols;
	while (n-- > 0) {
		line->cells[tp->cx + n].character = tp->view.ascebc[' '];
		line->cells[tp->cx + n].highlight = tp->highlight;
		line->cells[tp->cx + n].f_color = tp->f_color;
	}
}

static void
tty3270_delete_characters(struct tty3270 *tp, int n)
{
	struct tty3270_line *line;
	int i;

	line = tp->screen + tp->cy;
	if (line->len <= tp->cx)
		return;
	if (line->len - tp->cx <= n) {
		line->len = tp->cx;
		return;
	}
	for (i = tp->cx; i + n < line->len; i++)
		line->cells[i] = line->cells[i + n];
	line->len -= n;
}

static void
tty3270_erase_characters(struct tty3270 *tp, int n)
{
	struct tty3270_line *line;
	struct tty3270_cell *cell;

	line = tp->screen + tp->cy;
	while (line->len > tp->cx && n-- > 0) {
		cell = line->cells + tp->cx++;
		cell->character = ' ';
		cell->highlight = TAX_RESET;
		cell->f_color = TAC_RESET;
	}
	tp->cx += n;
	tp->cx = min_t(int, tp->cx, tp->view.cols - 1);
}

static void
tty3270_erase_line(struct tty3270 *tp, int mode)
{
	struct tty3270_line *line;
	struct tty3270_cell *cell;
	int i;

	line = tp->screen + tp->cy;
	if (mode == 0)
		line->len = tp->cx;
	else if (mode == 1) {
		for (i = 0; i < tp->cx; i++) {
			cell = line->cells + i;
			cell->character = ' ';
			cell->highlight = TAX_RESET;
			cell->f_color = TAC_RESET;
		}
		if (line->len <= tp->cx)
			line->len = tp->cx + 1;
	} else if (mode == 2)
		line->len = 0;
	tty3270_convert_line(tp, tp->cy);
}

static void
tty3270_erase_display(struct tty3270 *tp, int mode)
{
	int i;

	if (mode == 0) {
		tty3270_erase_line(tp, 0);
		for (i = tp->cy + 1; i < tp->view.rows - 2; i++) {
			tp->screen[i].len = 0;
			tty3270_convert_line(tp, i);
		}
	} else if (mode == 1) {
		for (i = 0; i < tp->cy; i++) {
			tp->screen[i].len = 0;
			tty3270_convert_line(tp, i);
		}
		tty3270_erase_line(tp, 1);
	} else if (mode == 2) {
		for (i = 0; i < tp->view.rows - 2; i++) {
			tp->screen[i].len = 0;
			tty3270_convert_line(tp, i);
		}
	}
	tty3270_rebuild_update(tp);
}

static void
tty3270_set_attributes(struct tty3270 *tp)
{
	static unsigned char f_colors[] = {
		TAC_DEFAULT, TAC_RED, TAC_GREEN, TAC_YELLOW, TAC_BLUE,
		TAC_PINK, TAC_TURQ, TAC_WHITE, 0, TAC_DEFAULT
	};
	int i, attr;

	for (i = 0; i <= tp->esc_npar; i++) {
		attr = tp->esc_par[i];
		switch (attr) {
		case 0:		
			tp->highlight = TAX_RESET;
			tp->f_color = TAC_RESET;
			break;
		
		case 4:		
			tp->highlight = TAX_UNDER;
			break;
		case 5:		
			tp->highlight = TAX_BLINK;
			break;
		case 7:		
			tp->highlight = TAX_REVER;
			break;
		case 24:	
			if (tp->highlight == TAX_UNDER)
				tp->highlight = TAX_RESET;
			break;
		case 25:	
			if (tp->highlight == TAX_BLINK)
				tp->highlight = TAX_RESET;
			break;
		case 27:	
			if (tp->highlight == TAX_REVER)
				tp->highlight = TAX_RESET;
			break;
		
		case 30:	
		case 31:	
		case 32:	
		case 33:	
		case 34:	
		case 35:	
		case 36:	
		case 37:	
		case 39:	
			tp->f_color = f_colors[attr - 30];
			break;
		}
	}
}

static inline int
tty3270_getpar(struct tty3270 *tp, int ix)
{
	return (tp->esc_par[ix] > 0) ? tp->esc_par[ix] : 1;
}

static void
tty3270_goto_xy(struct tty3270 *tp, int cx, int cy)
{
	int max_cx = max(0, cx);
	int max_cy = max(0, cy);

	tp->cx = min_t(int, tp->view.cols - 1, max_cx);
	cy = min_t(int, tp->view.rows - 3, max_cy);
	if (cy != tp->cy) {
		tty3270_convert_line(tp, tp->cy);
		tp->cy = cy;
	}
}

static void
tty3270_escape_sequence(struct tty3270 *tp, char ch)
{
	enum { ESnormal, ESesc, ESsquare, ESgetpars };

	if (tp->esc_state == ESnormal) {
		if (ch == 0x1b)
			
			tp->esc_state = ESesc;
		return;
	}
	if (tp->esc_state == ESesc) {
		tp->esc_state = ESnormal;
		switch (ch) {
		case '[':
			tp->esc_state = ESsquare;
			break;
		case 'E':
			tty3270_cr(tp);
			tty3270_lf(tp);
			break;
		case 'M':
			tty3270_ri(tp);
			break;
		case 'D':
			tty3270_lf(tp);
			break;
		case 'Z':		
			kbd_puts_queue(tp->tty, "\033[?6c");
			break;
		case '7':		
			tp->saved_cx = tp->cx;
			tp->saved_cy = tp->cy;
			tp->saved_highlight = tp->highlight;
			tp->saved_f_color = tp->f_color;
			break;
		case '8':		
			tty3270_convert_line(tp, tp->cy);
			tty3270_goto_xy(tp, tp->saved_cx, tp->saved_cy);
			tp->highlight = tp->saved_highlight;
			tp->f_color = tp->saved_f_color;
			break;
		case 'c':		
			tp->cx = tp->saved_cx = 0;
			tp->cy = tp->saved_cy = 0;
			tp->highlight = tp->saved_highlight = TAX_RESET;
			tp->f_color = tp->saved_f_color = TAC_RESET;
			tty3270_erase_display(tp, 2);
			break;
		}
		return;
	}
	if (tp->esc_state == ESsquare) {
		tp->esc_state = ESgetpars;
		memset(tp->esc_par, 0, sizeof(tp->esc_par));
		tp->esc_npar = 0;
		tp->esc_ques = (ch == '?');
		if (tp->esc_ques)
			return;
	}
	if (tp->esc_state == ESgetpars) {
		if (ch == ';' && tp->esc_npar < ESCAPE_NPAR - 1) {
			tp->esc_npar++;
			return;
		}
		if (ch >= '0' && ch <= '9') {
			tp->esc_par[tp->esc_npar] *= 10;
			tp->esc_par[tp->esc_npar] += ch - '0';
			return;
		}
	}
	tp->esc_state = ESnormal;
	if (ch == 'n' && !tp->esc_ques) {
		if (tp->esc_par[0] == 5)		
			kbd_puts_queue(tp->tty, "\033[0n");
		else if (tp->esc_par[0] == 6) {	
			char buf[40];
			sprintf(buf, "\033[%d;%dR", tp->cy + 1, tp->cx + 1);
			kbd_puts_queue(tp->tty, buf);
		}
		return;
	}
	if (tp->esc_ques)
		return;
	switch (ch) {
	case 'm':
		tty3270_set_attributes(tp);
		break;
	case 'H':	
	case 'f':
		tty3270_goto_xy(tp, tty3270_getpar(tp, 1) - 1,
				tty3270_getpar(tp, 0) - 1);
		break;
	case 'd':	
		tty3270_goto_xy(tp, tp->cx, tty3270_getpar(tp, 0) - 1);
		break;
	case 'A':	
	case 'F':
		tty3270_goto_xy(tp, tp->cx, tp->cy - tty3270_getpar(tp, 0));
		break;
	case 'B':	
	case 'e':
	case 'E':
		tty3270_goto_xy(tp, tp->cx, tp->cy + tty3270_getpar(tp, 0));
		break;
	case 'C':	
	case 'a':
		tty3270_goto_xy(tp, tp->cx + tty3270_getpar(tp, 0), tp->cy);
		break;
	case 'D':	
		tty3270_goto_xy(tp, tp->cx - tty3270_getpar(tp, 0), tp->cy);
		break;
	case 'G':	
	case '`':
		tty3270_goto_xy(tp, tty3270_getpar(tp, 0), tp->cy);
		break;
	case 'X':	
		tty3270_erase_characters(tp, tty3270_getpar(tp, 0));
		break;
	case 'J':	
		tty3270_erase_display(tp, tp->esc_par[0]);
		break;
	case 'K':	
		tty3270_erase_line(tp, tp->esc_par[0]);
		break;
	case 'P':	
		tty3270_delete_characters(tp, tty3270_getpar(tp, 0));
		break;
	case '@':	
		tty3270_insert_characters(tp, tty3270_getpar(tp, 0));
		break;
	case 's':	
		tp->saved_cx = tp->cx;
		tp->saved_cy = tp->cy;
		tp->saved_highlight = tp->highlight;
		tp->saved_f_color = tp->f_color;
		break;
	case 'u':	
		tty3270_convert_line(tp, tp->cy);
		tty3270_goto_xy(tp, tp->saved_cx, tp->saved_cy);
		tp->highlight = tp->saved_highlight;
		tp->f_color = tp->saved_f_color;
		break;
	}
}

static void
tty3270_do_write(struct tty3270 *tp, const unsigned char *buf, int count)
{
	int i_msg, i;

	spin_lock_bh(&tp->view.lock);
	for (i_msg = 0; !tp->tty->stopped && i_msg < count; i_msg++) {
		if (tp->esc_state != 0) {
			
			tty3270_escape_sequence(tp, buf[i_msg]);
			continue;
		}

		switch (buf[i_msg]) {
		case 0x07:		
			tp->wcc |= TW_PLUSALARM;
			break;
		case 0x08:		
			if (tp->cx > 0) {
				tp->cx--;
				tty3270_put_character(tp, ' ');
			}
			break;
		case 0x09:		
			for (i = tp->cx % 8; i < 8; i++) {
				if (tp->cx >= tp->view.cols) {
					tty3270_cr(tp);
					tty3270_lf(tp);
					break;
				}
				tty3270_put_character(tp, ' ');
				tp->cx++;
			}
			break;
		case 0x0a:		
			tty3270_cr(tp);
			tty3270_lf(tp);
			break;
		case 0x0c:		
			tty3270_erase_display(tp, 2);
			tp->cx = tp->cy = 0;
			break;
		case 0x0d:		
			tp->cx = 0;
			break;
		case 0x0f:		
			break;
		case 0x1b:		
			tty3270_escape_sequence(tp, buf[i_msg]);
			break;
		default:		
			if (tp->cx >= tp->view.cols) {
				tty3270_cr(tp);
				tty3270_lf(tp);
			}
			tty3270_put_character(tp, buf[i_msg]);
			tp->cx++;
			break;
		}
	}
	
	tty3270_convert_line(tp, tp->cy);

	
	if (!timer_pending(&tp->timer))
		tty3270_set_timer(tp, HZ/10);

	spin_unlock_bh(&tp->view.lock);
}

static int
tty3270_write(struct tty_struct * tty,
	      const unsigned char *buf, int count)
{
	struct tty3270 *tp;

	tp = tty->driver_data;
	if (!tp)
		return 0;
	if (tp->char_count > 0) {
		tty3270_do_write(tp, tp->char_buf, tp->char_count);
		tp->char_count = 0;
	}
	tty3270_do_write(tp, buf, count);
	return count;
}

static int tty3270_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct tty3270 *tp;

	tp = tty->driver_data;
	if (!tp || tp->char_count >= TTY3270_CHAR_BUF_SIZE)
		return 0;
	tp->char_buf[tp->char_count++] = ch;
	return 1;
}

static void
tty3270_flush_chars(struct tty_struct *tty)
{
	struct tty3270 *tp;

	tp = tty->driver_data;
	if (!tp)
		return;
	if (tp->char_count > 0) {
		tty3270_do_write(tp, tp->char_buf, tp->char_count);
		tp->char_count = 0;
	}
}

static int
tty3270_chars_in_buffer(struct tty_struct *tty)
{
	return 0;
}

static void
tty3270_flush_buffer(struct tty_struct *tty)
{
}

static void
tty3270_set_termios(struct tty_struct *tty, struct ktermios *old)
{
	struct tty3270 *tp;
	int new;

	tp = tty->driver_data;
	if (!tp)
		return;
	spin_lock_bh(&tp->view.lock);
	if (L_ICANON(tty)) {
		new = L_ECHO(tty) ? TF_INPUT: TF_INPUTN;
		if (new != tp->inattr) {
			tp->inattr = new;
			tty3270_update_prompt(tp, NULL, 0);
			tty3270_set_timer(tp, 1);
		}
	}
	spin_unlock_bh(&tp->view.lock);
}

static void
tty3270_throttle(struct tty_struct * tty)
{
	struct tty3270 *tp;

	tp = tty->driver_data;
	if (!tp)
		return;
	tp->throttle = 1;
}

static void
tty3270_unthrottle(struct tty_struct * tty)
{
	struct tty3270 *tp;

	tp = tty->driver_data;
	if (!tp)
		return;
	tp->throttle = 0;
	if (tp->attn)
		tty3270_issue_read(tp, 1);
}

static void
tty3270_hangup(struct tty_struct *tty)
{
	
}

static void
tty3270_wait_until_sent(struct tty_struct *tty, int timeout)
{
}

static int tty3270_ioctl(struct tty_struct *tty, unsigned int cmd,
			 unsigned long arg)
{
	struct tty3270 *tp;

	tp = tty->driver_data;
	if (!tp)
		return -ENODEV;
	if (tty->flags & (1 << TTY_IO_ERROR))
		return -EIO;
	return kbd_ioctl(tp->kbd, cmd, arg);
}

#ifdef CONFIG_COMPAT
static long tty3270_compat_ioctl(struct tty_struct *tty,
				 unsigned int cmd, unsigned long arg)
{
	struct tty3270 *tp;

	tp = tty->driver_data;
	if (!tp)
		return -ENODEV;
	if (tty->flags & (1 << TTY_IO_ERROR))
		return -EIO;
	return kbd_ioctl(tp->kbd, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct tty_operations tty3270_ops = {
	.open = tty3270_open,
	.close = tty3270_close,
	.write = tty3270_write,
	.put_char = tty3270_put_char,
	.flush_chars = tty3270_flush_chars,
	.write_room = tty3270_write_room,
	.chars_in_buffer = tty3270_chars_in_buffer,
	.flush_buffer = tty3270_flush_buffer,
	.throttle = tty3270_throttle,
	.unthrottle = tty3270_unthrottle,
	.hangup = tty3270_hangup,
	.wait_until_sent = tty3270_wait_until_sent,
	.ioctl = tty3270_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tty3270_compat_ioctl,
#endif
	.set_termios = tty3270_set_termios
};

static int __init tty3270_init(void)
{
	struct tty_driver *driver;
	int ret;

	driver = alloc_tty_driver(RAW3270_MAXDEVS);
	if (!driver)
		return -ENOMEM;

	driver->driver_name = "ttyTUB";
	driver->name = "ttyTUB";
	driver->major = IBM_TTY3270_MAJOR;
	driver->minor_start = RAW3270_FIRSTMINOR;
	driver->type = TTY_DRIVER_TYPE_SYSTEM;
	driver->subtype = SYSTEM_TYPE_TTY;
	driver->init_termios = tty_std_termios;
	driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(driver, &tty3270_ops);
	ret = tty_register_driver(driver);
	if (ret) {
		put_tty_driver(driver);
		return ret;
	}
	tty3270_driver = driver;
	return 0;
}

static void __exit
tty3270_exit(void)
{
	struct tty_driver *driver;

	driver = tty3270_driver;
	tty3270_driver = NULL;
	tty_unregister_driver(driver);
	tty3270_del_views();
}

MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(IBM_TTY3270_MAJOR);

module_init(tty3270_init);
module_exit(tty3270_exit);
