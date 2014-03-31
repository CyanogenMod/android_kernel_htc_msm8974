#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <mach/devices_dtb.h>

static int delta = 50;
static int flag = 0;

static int set_delta(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_int(val, kp);
	pr_info("%s: delta = %d\n", KBUILD_MODNAME, delta);

	return ret;
}

static struct kernel_param_ops delta_ops = {
	.set = set_delta,
	.get = param_get_int,
};
module_param_cb(delta, &delta_ops, &delta, 0644);

static int set_flag(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_bool(val, kp);
	pr_info("%s: flag = %d\n", KBUILD_MODNAME, flag);

	return ret;
}

static struct kernel_param_ops flag_ops = {
	.set = set_flag,
	.get = param_get_bool,
};
module_param_cb(flag, &flag_ops, &flag, 0644);

static int __init htc_thermal_init(void)
{
	if (get_kernel_flag() & KERNEL_FLAG_FAKE_ID) {
		delta = 70;
		flag = 1;
	} else {
		flag = 0;
	}

	return 0;
}

static void __exit htc_thermal_exit(void)
{
	return;
}

module_init(htc_thermal_init);
module_exit(htc_thermal_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jim Hsia <jim_hsia@htc.com>");
