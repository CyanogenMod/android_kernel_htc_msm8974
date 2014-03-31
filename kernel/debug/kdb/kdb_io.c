/*
 * Kernel Debugger Architecture Independent Console I/O handler
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1999-2006 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Wind River Systems, Inc.  All Rights Reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/nmi.h>
#include <linux/delay.h>
#include <linux/kgdb.h>
#include <linux/kdb.h>
#include <linux/kallsyms.h>
#include "kdb_private.h"

#define CMD_BUFLEN 256
char kdb_prompt_str[CMD_BUFLEN];

int kdb_trap_printk;

static int kgdb_transition_check(char *buffer)
{
	if (buffer[0] != '+' && buffer[0] != '$') {
		KDB_STATE_SET(KGDB_TRANS);
		kdb_printf("%s", buffer);
	} else {
		int slen = strlen(buffer);
		if (slen > 3 && buffer[slen - 3] == '#') {
			kdb_gdb_state_pass(buffer);
			strcpy(buffer, "kgdb");
			KDB_STATE_SET(DOING_KGDB);
			return 1;
		}
	}
	return 0;
}

static int kdb_read_get_key(char *buffer, size_t bufsize)
{
#define ESCAPE_UDELAY 1000
#define ESCAPE_DELAY (2*1000000/ESCAPE_UDELAY) 
	char escape_data[5];	
	char *ped = escape_data;
	int escape_delay = 0;
	get_char_func *f, *f_escape = NULL;
	int key;

	for (f = &kdb_poll_funcs[0]; ; ++f) {
		if (*f == NULL) {
			
			touch_nmi_watchdog();
			f = &kdb_poll_funcs[0];
		}
		if (escape_delay == 2) {
			*ped = '\0';
			ped = escape_data;
			--escape_delay;
		}
		if (escape_delay == 1) {
			key = *ped++;
			if (!*ped)
				--escape_delay;
			break;
		}
		key = (*f)();
		if (key == -1) {
			if (escape_delay) {
				udelay(ESCAPE_UDELAY);
				--escape_delay;
			}
			continue;
		}
		if (bufsize <= 2) {
			if (key == '\r')
				key = '\n';
			*buffer++ = key;
			*buffer = '\0';
			return -1;
		}
		if (escape_delay == 0 && key == '\e') {
			escape_delay = ESCAPE_DELAY;
			ped = escape_data;
			f_escape = f;
		}
		if (escape_delay) {
			*ped++ = key;
			if (f_escape != f) {
				escape_delay = 2;
				continue;
			}
			if (ped - escape_data == 1) {
				
				continue;
			} else if (ped - escape_data == 2) {
				
				if (key != '[')
					escape_delay = 2;
				continue;
			} else if (ped - escape_data == 3) {
				
				int mapkey = 0;
				switch (key) {
				case 'A': 
					mapkey = 16;
					break;
				case 'B': 
					mapkey = 14;
					break;
				case 'C': 
					mapkey = 6;
					break;
				case 'D': 
					mapkey = 2;
					break;
				case '1': 
				case '3': 
				
				case '4':
					mapkey = -1;
					break;
				}
				if (mapkey != -1) {
					if (mapkey > 0) {
						escape_data[0] = mapkey;
						escape_data[1] = '\0';
					}
					escape_delay = 2;
				}
				continue;
			} else if (ped - escape_data == 4) {
				
				int mapkey = 0;
				if (key == '~') {
					switch (escape_data[2]) {
					case '1': 
						mapkey = 1;
						break;
					case '3': 
						mapkey = 4;
						break;
					case '4': 
						mapkey = 5;
						break;
					}
				}
				if (mapkey > 0) {
					escape_data[0] = mapkey;
					escape_data[1] = '\0';
				}
				escape_delay = 2;
				continue;
			}
		}
		break;	
	}
	return key;
}


static char *kdb_read(char *buffer, size_t bufsize)
{
	char *cp = buffer;
	char *bufend = buffer+bufsize-2;	
	char *lastchar;
	char *p_tmp;
	char tmp;
	static char tmpbuffer[CMD_BUFLEN];
	int len = strlen(buffer);
	int len_tmp;
	int tab = 0;
	int count;
	int i;
	int diag, dtab_count;
	int key;
	static int last_crlf;

	diag = kdbgetintenv("DTABCOUNT", &dtab_count);
	if (diag)
		dtab_count = 30;

	if (len > 0) {
		cp += len;
		if (*(buffer+len-1) == '\n')
			cp--;
	}

	lastchar = cp;
	*cp = '\0';
	kdb_printf("%s", buffer);
poll_again:
	key = kdb_read_get_key(buffer, bufsize);
	if (key == -1)
		return buffer;
	if (key != 9)
		tab = 0;
	if (key != 10 && key != 13)
		last_crlf = 0;

	switch (key) {
	case 8: 
		if (cp > buffer) {
			if (cp < lastchar) {
				memcpy(tmpbuffer, cp, lastchar - cp);
				memcpy(cp-1, tmpbuffer, lastchar - cp);
			}
			*(--lastchar) = '\0';
			--cp;
			kdb_printf("\b%s \r", cp);
			tmp = *cp;
			*cp = '\0';
			kdb_printf(kdb_prompt_str);
			kdb_printf("%s", buffer);
			*cp = tmp;
		}
		break;
	case 10: 
	case 13: 
		
		if (last_crlf && last_crlf != key)
			break;
		last_crlf = key;
		*lastchar++ = '\n';
		*lastchar++ = '\0';
		if (!KDB_STATE(KGDB_TRANS)) {
			KDB_STATE_SET(KGDB_TRANS);
			kdb_printf("%s", buffer);
		}
		kdb_printf("\n");
		return buffer;
	case 4: 
		if (cp < lastchar) {
			memcpy(tmpbuffer, cp+1, lastchar - cp - 1);
			memcpy(cp, tmpbuffer, lastchar - cp - 1);
			*(--lastchar) = '\0';
			kdb_printf("%s \r", cp);
			tmp = *cp;
			*cp = '\0';
			kdb_printf(kdb_prompt_str);
			kdb_printf("%s", buffer);
			*cp = tmp;
		}
		break;
	case 1: 
		if (cp > buffer) {
			kdb_printf("\r");
			kdb_printf(kdb_prompt_str);
			cp = buffer;
		}
		break;
	case 5: 
		if (cp < lastchar) {
			kdb_printf("%s", cp);
			cp = lastchar;
		}
		break;
	case 2: 
		if (cp > buffer) {
			kdb_printf("\b");
			--cp;
		}
		break;
	case 14: 
		memset(tmpbuffer, ' ',
		       strlen(kdb_prompt_str) + (lastchar-buffer));
		*(tmpbuffer+strlen(kdb_prompt_str) +
		  (lastchar-buffer)) = '\0';
		kdb_printf("\r%s\r", tmpbuffer);
		*lastchar = (char)key;
		*(lastchar+1) = '\0';
		return lastchar;
	case 6: 
		if (cp < lastchar) {
			kdb_printf("%c", *cp);
			++cp;
		}
		break;
	case 16: 
		memset(tmpbuffer, ' ',
		       strlen(kdb_prompt_str) + (lastchar-buffer));
		*(tmpbuffer+strlen(kdb_prompt_str) +
		  (lastchar-buffer)) = '\0';
		kdb_printf("\r%s\r", tmpbuffer);
		*lastchar = (char)key;
		*(lastchar+1) = '\0';
		return lastchar;
	case 9: 
		if (tab < 2)
			++tab;
		p_tmp = buffer;
		while (*p_tmp == ' ')
			p_tmp++;
		if (p_tmp > cp)
			break;
		memcpy(tmpbuffer, p_tmp, cp-p_tmp);
		*(tmpbuffer + (cp-p_tmp)) = '\0';
		p_tmp = strrchr(tmpbuffer, ' ');
		if (p_tmp)
			++p_tmp;
		else
			p_tmp = tmpbuffer;
		len = strlen(p_tmp);
		count = kallsyms_symbol_complete(p_tmp,
						 sizeof(tmpbuffer) -
						 (p_tmp - tmpbuffer));
		if (tab == 2 && count > 0) {
			kdb_printf("\n%d symbols are found.", count);
			if (count > dtab_count) {
				count = dtab_count;
				kdb_printf(" But only first %d symbols will"
					   " be printed.\nYou can change the"
					   " environment variable DTABCOUNT.",
					   count);
			}
			kdb_printf("\n");
			for (i = 0; i < count; i++) {
				if (kallsyms_symbol_next(p_tmp, i) < 0)
					break;
				kdb_printf("%s ", p_tmp);
				*(p_tmp + len) = '\0';
			}
			if (i >= dtab_count)
				kdb_printf("...");
			kdb_printf("\n");
			kdb_printf(kdb_prompt_str);
			kdb_printf("%s", buffer);
		} else if (tab != 2 && count > 0) {
			len_tmp = strlen(p_tmp);
			strncpy(p_tmp+len_tmp, cp, lastchar-cp+1);
			len_tmp = strlen(p_tmp);
			strncpy(cp, p_tmp+len, len_tmp-len + 1);
			len = len_tmp - len;
			kdb_printf("%s", cp);
			cp += len;
			lastchar += len;
		}
		kdb_nextline = 1; 
		break;
	default:
		if (key >= 32 && lastchar < bufend) {
			if (cp < lastchar) {
				memcpy(tmpbuffer, cp, lastchar - cp);
				memcpy(cp+1, tmpbuffer, lastchar - cp);
				*++lastchar = '\0';
				*cp = key;
				kdb_printf("%s\r", cp);
				++cp;
				tmp = *cp;
				*cp = '\0';
				kdb_printf(kdb_prompt_str);
				kdb_printf("%s", buffer);
				*cp = tmp;
			} else {
				*++lastchar = '\0';
				*cp++ = key;
				if (!KDB_STATE(KGDB_TRANS)) {
					if (kgdb_transition_check(buffer))
						return buffer;
				} else {
					kdb_printf("%c", key);
				}
			}
			
			if (lastchar - buffer >= 5 &&
			    strcmp(lastchar - 5, "$?#3f") == 0) {
				kdb_gdb_state_pass(lastchar - 5);
				strcpy(buffer, "kgdb");
				KDB_STATE_SET(DOING_KGDB);
				return buffer;
			}
			if (lastchar - buffer >= 11 &&
			    strcmp(lastchar - 11, "$qSupported") == 0) {
				kdb_gdb_state_pass(lastchar - 11);
				strcpy(buffer, "kgdb");
				KDB_STATE_SET(DOING_KGDB);
				return buffer;
			}
		}
		break;
	}
	goto poll_again;
}


char *kdb_getstr(char *buffer, size_t bufsize, char *prompt)
{
	if (prompt && kdb_prompt_str != prompt)
		strncpy(kdb_prompt_str, prompt, CMD_BUFLEN);
	kdb_printf(kdb_prompt_str);
	kdb_nextline = 1;	
	return kdb_read(buffer, bufsize);
}


static void kdb_input_flush(void)
{
	get_char_func *f;
	int res;
	int flush_delay = 1;
	while (flush_delay) {
		flush_delay--;
empty:
		touch_nmi_watchdog();
		for (f = &kdb_poll_funcs[0]; *f; ++f) {
			res = (*f)();
			if (res != -1) {
				flush_delay = 1;
				goto empty;
			}
		}
		if (flush_delay)
			mdelay(1);
	}
}


static char kdb_buffer[256];	
static char *next_avail = kdb_buffer;
static int  size_avail;
static int  suspend_grep;

static int kdb_search_string(char *searched, char *searchfor)
{
	char firstchar, *cp;
	int len1, len2;

	
	len1 = strlen(searched)-1;
	len2 = strlen(searchfor);
	if (len1 < len2)
		return 0;
	if (kdb_grep_leading && kdb_grep_trailing && len1 != len2)
		return 0;
	if (kdb_grep_leading) {
		if (!strncmp(searched, searchfor, len2))
			return 1;
	} else if (kdb_grep_trailing) {
		if (!strncmp(searched+len1-len2, searchfor, len2))
			return 1;
	} else {
		firstchar = *searchfor;
		cp = searched;
		while ((cp = strchr(cp, firstchar))) {
			if (!strncmp(cp, searchfor, len2))
				return 1;
			cp++;
		}
	}
	return 0;
}

int vkdb_printf(const char *fmt, va_list ap)
{
	int diag;
	int linecount;
	int logging, saved_loglevel = 0;
	int saved_trap_printk;
	int got_printf_lock = 0;
	int retlen = 0;
	int fnd, len;
	char *cp, *cp2, *cphold = NULL, replaced_byte = ' ';
	char *moreprompt = "more> ";
	struct console *c = console_drivers;
	static DEFINE_SPINLOCK(kdb_printf_lock);
	unsigned long uninitialized_var(flags);

	preempt_disable();
	saved_trap_printk = kdb_trap_printk;
	kdb_trap_printk = 0;

	if (!KDB_STATE(PRINTF_LOCK)) {
		KDB_STATE_SET(PRINTF_LOCK);
		spin_lock_irqsave(&kdb_printf_lock, flags);
		got_printf_lock = 1;
		atomic_inc(&kdb_event);
	} else {
		__acquire(kdb_printf_lock);
	}

	diag = kdbgetintenv("LINES", &linecount);
	if (diag || linecount <= 1)
		linecount = 24;

	diag = kdbgetintenv("LOGGING", &logging);
	if (diag)
		logging = 0;

	if (!kdb_grepping_flag || suspend_grep) {
		
		next_avail = kdb_buffer;
		size_avail = sizeof(kdb_buffer);
	}
	vsnprintf(next_avail, size_avail, fmt, ap);


	
	if (!suspend_grep && kdb_grepping_flag) {
		cp = strchr(kdb_buffer, '\n');
		if (!cp) {
			/*
			 * Special cases that don't end with newlines
			 * but should be written without one:
			 *   The "[nn]kdb> " prompt should
			 *   appear at the front of the buffer.
			 *
			 *   The "[nn]more " prompt should also be
			 *     (MOREPROMPT -> moreprompt)
			 *   written *   but we print that ourselves,
			 *   we set the suspend_grep flag to make
			 *   it unconditional.
			 *
			 */
			if (next_avail == kdb_buffer) {
				cp2 = kdb_buffer;
				len = strlen(kdb_prompt_str);
				if (!strncmp(cp2, kdb_prompt_str, len)) {
					kdb_grepping_flag = 0;
					goto kdb_printit;
				}
			}
			len = strlen(kdb_buffer);
			next_avail = kdb_buffer + len;
			size_avail = sizeof(kdb_buffer) - len;
			goto kdb_print_out;
		}

		cp++;	 	     
		replaced_byte = *cp; 
		cphold = cp;
		*cp = '\0';	     

		fnd = kdb_search_string(kdb_buffer, kdb_grep_string);
		if (!fnd) {
			*cphold = replaced_byte;
			strcpy(kdb_buffer, cphold);
			len = strlen(kdb_buffer);
			next_avail = kdb_buffer + len;
			size_avail = sizeof(kdb_buffer) - len;
			goto kdb_print_out;
		}
	}
kdb_printit:

	retlen = strlen(kdb_buffer);
	if (!dbg_kdb_mode && kgdb_connected) {
		gdbstub_msg_write(kdb_buffer, retlen);
	} else {
		if (dbg_io_ops && !dbg_io_ops->is_console) {
			len = strlen(kdb_buffer);
			cp = kdb_buffer;
			while (len--) {
				dbg_io_ops->write_char(*cp);
				cp++;
			}
		}
		while (c) {
			c->write(c, kdb_buffer, retlen);
			touch_nmi_watchdog();
			c = c->next;
		}
	}
	if (logging) {
		saved_loglevel = console_loglevel;
		console_loglevel = 0;
		printk(KERN_INFO "%s", kdb_buffer);
	}

	if (KDB_STATE(PAGER) && strchr(kdb_buffer, '\n'))
		kdb_nextline++;

	
	if (kdb_nextline == linecount) {
		char buf1[16] = "";
#if defined(CONFIG_SMP)
		char buf2[32];
#endif

		kdb_nextline = 1;	

		moreprompt = kdbgetenv("MOREPROMPT");
		if (moreprompt == NULL)
			moreprompt = "more> ";

#if defined(CONFIG_SMP)
		if (strchr(moreprompt, '%')) {
			sprintf(buf2, moreprompt, get_cpu());
			put_cpu();
			moreprompt = buf2;
		}
#endif

		kdb_input_flush();
		c = console_drivers;

		if (dbg_io_ops && !dbg_io_ops->is_console) {
			len = strlen(moreprompt);
			cp = moreprompt;
			while (len--) {
				dbg_io_ops->write_char(*cp);
				cp++;
			}
		}
		while (c) {
			c->write(c, moreprompt, strlen(moreprompt));
			touch_nmi_watchdog();
			c = c->next;
		}

		if (logging)
			printk("%s", moreprompt);

		kdb_read(buf1, 2); 
		kdb_nextline = 1;	

		
		kdb_buffer[0] = '\0';
		next_avail = kdb_buffer;
		size_avail = sizeof(kdb_buffer);
		if ((buf1[0] == 'q') || (buf1[0] == 'Q')) {
			
			KDB_FLAG_SET(CMD_INTERRUPT); 
			KDB_STATE_CLEAR(PAGER);
			
			kdb_grepping_flag = 0;
			kdb_printf("\n");
		} else if (buf1[0] == ' ') {
			kdb_printf("\n");
			suspend_grep = 1; 
		} else if (buf1[0] == '\n') {
			kdb_nextline = linecount - 1;
			kdb_printf("\r");
			suspend_grep = 1; 
		} else if (buf1[0] && buf1[0] != '\n') {
			
			suspend_grep = 1; 
			kdb_printf("\nOnly 'q' or 'Q' are processed at more "
				   "prompt, input ignored\n");
		} else if (kdb_grepping_flag) {
			
			suspend_grep = 1; 
			kdb_printf("\n");
		}
		kdb_input_flush();
	}

	/*
	 * For grep searches, shift the printed string left.
	 *  replaced_byte contains the character that was overwritten with
	 *  the terminating null, and cphold points to the null.
	 * Then adjust the notion of available space in the buffer.
	 */
	if (kdb_grepping_flag && !suspend_grep) {
		*cphold = replaced_byte;
		strcpy(kdb_buffer, cphold);
		len = strlen(kdb_buffer);
		next_avail = kdb_buffer + len;
		size_avail = sizeof(kdb_buffer) - len;
	}

kdb_print_out:
	suspend_grep = 0; 
	if (logging)
		console_loglevel = saved_loglevel;
	if (KDB_STATE(PRINTF_LOCK) && got_printf_lock) {
		got_printf_lock = 0;
		spin_unlock_irqrestore(&kdb_printf_lock, flags);
		KDB_STATE_CLEAR(PRINTF_LOCK);
		atomic_dec(&kdb_event);
	} else {
		__release(kdb_printf_lock);
	}
	kdb_trap_printk = saved_trap_printk;
	preempt_enable();
	return retlen;
}

int kdb_printf(const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vkdb_printf(fmt, ap);
	va_end(ap);

	return r;
}
EXPORT_SYMBOL_GPL(kdb_printf);
