/*
 * Linux 2.6.32 and later Kernel module for VMware MVP OEK Test
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>

#define DRV_NAME	"mvp-fake-kb"
#define ENABLE_FILE	"enable"
#define SEND_KEY_FILE	"key"


static struct proc_dir_entry *dir;

static struct input_dev *key_dev;

static DEFINE_MUTEX(dev_mutex);

static void unregister_keyboard_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(unregister_keyboard_work,
			    unregister_keyboard_handler);

static unsigned long time_to_unregister = INITIAL_JIFFIES;

static void
register_keyboard_locked(int timeout)
{
	
	if (!key_dev) {
		int i;
		key_dev = input_allocate_device();
		if (!key_dev) {
			printk(KERN_ERR "Cannot allocate input device\n");
			return;
		}

		key_dev->name = DRV_NAME;
		key_dev->evbit[0] = BIT_MASK(EV_KEY);
		
		for (i = 0; i <= BIT_WORD(KEY_MAX); i++)
			key_dev->keybit[i] = (unsigned long) -1;

		if (input_register_device(key_dev)) {
			printk(KERN_ERR "Cannot register input device\n");
			input_free_device(key_dev);
			key_dev = NULL;
			return;
		}
	}

	
	if (timeout > 0) {
		
		unsigned long new_timeout =
			jiffies + msecs_to_jiffies(timeout * 1000);
		if (time_after(new_timeout, time_to_unregister)) {
			cancel_delayed_work(&unregister_keyboard_work);
			schedule_delayed_work(&unregister_keyboard_work,
					      msecs_to_jiffies(timeout * 1000));
			time_to_unregister = new_timeout;
		}
	} else {
		
		cancel_delayed_work(&unregister_keyboard_work);
		time_to_unregister = MAX_JIFFY_OFFSET;
	}
}

static void
unregister_keyboard_locked(void)
{
	
	cancel_delayed_work(&unregister_keyboard_work);
	time_to_unregister = INITIAL_JIFFIES;
	if (key_dev != NULL) {
		input_unregister_device(key_dev);
		key_dev = NULL;
	}
}

static void
unregister_keyboard_handler(struct work_struct *work)
{
	mutex_lock(&dev_mutex);
	
	if (time_is_before_jiffies(time_to_unregister)) {
		unregister_keyboard_locked();
		printk(KERN_INFO "Keyboard unregistered after timeout\n");
	}
	mutex_unlock(&dev_mutex);
}

static int
keyboard_state(char *buffer,
	       char **start,
	       off_t offset,
	       int count,
	       int *eof,
	       void *data)
{
	int len;

	if (offset > 0 || count < 3) {
		*eof = 1;
		return 0;
	}

	mutex_lock(&dev_mutex);
	len = sprintf(buffer, "%c\n", (key_dev != NULL) ? '1' : '0');
	*eof = 1;
	mutex_unlock(&dev_mutex);

	return len;
}

static int
enable_keyboard(struct file *file,
		const char __user *buffer,
		unsigned long count,
		void *data)
{
	int enable = -1;
	int timeout = 0;
	int items;

	items = sscanf(buffer, "%d %d", &enable, &timeout);
	if (items < 1 || items > 2 || enable < 0) {
		printk(KERN_ERR "Malformed value to enable/disable keyboard\n");
		return -1;
	}

	mutex_lock(&dev_mutex);
	if (enable > 0) 
		register_keyboard_locked(timeout);
	else 
		unregister_keyboard_locked();
	mutex_unlock(&dev_mutex);

	return count;
}

static int
send_key(struct file *file,
	 const char __user *buffer,
	 unsigned long count,
	 void *data)
{
	int key = -1;
	int value = -1;
	int items = 0;

	items = sscanf(buffer, "%d %d", &key, &value);
	if (items != 2 || key < 0 || value < 0) {
		printk(KERN_ERR "Malformed value to send a key\n");
		return -1;
	}

	mutex_lock(&dev_mutex);
	if (key_dev != NULL) {
		printk(KERN_INFO "Send key=%d Value=%d\n", key, value);
		input_report_key(key_dev, key, value);
		input_sync(key_dev);
	}
	mutex_unlock(&dev_mutex);
	return count;
}

static int __init
mvp_keypad_init(void)
{
	int ret = 0;
	struct proc_dir_entry *enable_file, *key_file;
	dir = proc_mkdir(DRV_NAME, NULL);
	if (!dir) {
		ret = -EIO;
		goto error;
	}

	
	enable_file = create_proc_entry(ENABLE_FILE, 0666, dir);
	if (!enable_file) {
		ret = -EIO;
		goto err_remove_dir;
	}
	enable_file->write_proc = enable_keyboard;
	enable_file->read_proc = keyboard_state;

	
	key_file = create_proc_entry(SEND_KEY_FILE, 0222, dir);
	if (!key_file) {
		ret = -EIO;
		goto err_remove_dir_and_key;
	}
	key_file->write_proc = send_key;

	printk(KERN_INFO "Init done for "DRV_NAME"\n");
	return ret;

err_remove_dir_and_key:
	remove_proc_entry("enable", dir);
err_remove_dir:
	remove_proc_entry(DRV_NAME, NULL);
error:
	return ret;
}

static void __exit
mvp_keypad_exit(void)
{
	remove_proc_entry(SEND_KEY_FILE, dir);
	remove_proc_entry(ENABLE_FILE, dir);
	remove_proc_entry(DRV_NAME, NULL);

	mutex_lock(&dev_mutex);
	unregister_keyboard_locked();
	mutex_unlock(&dev_mutex);
}

module_init(mvp_keypad_init);
module_exit(mvp_keypad_exit);

MODULE_AUTHOR("VMware");
MODULE_DESCRIPTION("MVP fake keyboard");
MODULE_LICENSE("GPL");
