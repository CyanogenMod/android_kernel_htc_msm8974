#include <asm/cpu_device_id.h>
#include <asm/processor.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/slab.h>

const struct x86_cpu_id *x86_match_cpu(const struct x86_cpu_id *match)
{
	const struct x86_cpu_id *m;
	struct cpuinfo_x86 *c = &boot_cpu_data;

	for (m = match; m->vendor | m->family | m->model | m->feature; m++) {
		if (m->vendor != X86_VENDOR_ANY && c->x86_vendor != m->vendor)
			continue;
		if (m->family != X86_FAMILY_ANY && c->x86 != m->family)
			continue;
		if (m->model != X86_MODEL_ANY && c->x86_model != m->model)
			continue;
		if (m->feature != X86_FEATURE_ANY && !cpu_has(c, m->feature))
			continue;
		return m;
	}
	return NULL;
}
EXPORT_SYMBOL(x86_match_cpu);

ssize_t arch_print_cpu_modalias(struct device *dev,
				struct device_attribute *attr,
				char *bufptr)
{
	int size = PAGE_SIZE;
	int i, n;
	char *buf = bufptr;

	n = snprintf(buf, size, "x86cpu:vendor:%04X:family:%04X:"
		     "model:%04X:feature:",
		boot_cpu_data.x86_vendor,
		boot_cpu_data.x86,
		boot_cpu_data.x86_model);
	size -= n;
	buf += n;
	size -= 1;
	for (i = 0; i < NCAPINTS*32; i++) {
		if (boot_cpu_has(i)) {
			n = snprintf(buf, size, ",%04X", i);
			if (n >= size) {
				WARN(1, "x86 features overflow page\n");
				break;
			}
			size -= n;
			buf += n;
		}
	}
	*buf++ = '\n';
	return buf - bufptr;
}

int arch_cpu_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	char *buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf) {
		arch_print_cpu_modalias(NULL, NULL, buf);
		add_uevent_var(env, "MODALIAS=%s", buf);
		kfree(buf);
	}
	return 0;
}
