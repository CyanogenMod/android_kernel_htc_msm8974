#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>

#include <asm/errno.h>
#include "of_private.h"

const struct of_device_id *of_match_device(const struct of_device_id *matches,
					   const struct device *dev)
{
	if ((!matches) || (!dev->of_node))
		return NULL;
	return of_match_node(matches, dev->of_node);
}
EXPORT_SYMBOL(of_match_device);

struct platform_device *of_dev_get(struct platform_device *dev)
{
	struct device *tmp;

	if (!dev)
		return NULL;
	tmp = get_device(&dev->dev);
	if (tmp)
		return to_platform_device(tmp);
	else
		return NULL;
}
EXPORT_SYMBOL(of_dev_get);

void of_dev_put(struct platform_device *dev)
{
	if (dev)
		put_device(&dev->dev);
}
EXPORT_SYMBOL(of_dev_put);

int of_device_add(struct platform_device *ofdev)
{
	BUG_ON(ofdev->dev.of_node == NULL);

	ofdev->name = dev_name(&ofdev->dev);
	ofdev->id = -1;

	if (!ofdev->dev.parent)
		set_dev_node(&ofdev->dev, of_node_to_nid(ofdev->dev.of_node));

	return device_add(&ofdev->dev);
}

int of_device_register(struct platform_device *pdev)
{
	device_initialize(&pdev->dev);
	return of_device_add(pdev);
}
EXPORT_SYMBOL(of_device_register);

void of_device_unregister(struct platform_device *ofdev)
{
	device_unregister(&ofdev->dev);
}
EXPORT_SYMBOL(of_device_unregister);

ssize_t of_device_get_modalias(struct device *dev, char *str, ssize_t len)
{
	const char *compat;
	int cplen, i;
	ssize_t tsize, csize, repend;

	
	csize = snprintf(str, len, "of:N%sT%s", dev->of_node->name,
			 dev->of_node->type);

	
	compat = of_get_property(dev->of_node, "compatible", &cplen);
	if (!compat)
		return csize;

	
	for (i = (cplen - 1); i >= 0 && !compat[i]; i--)
		cplen--;
	if (!cplen)
		return csize;
	cplen++;

	
	tsize = csize + cplen;
	repend = tsize;

	if (csize >= len)		
		return tsize;

	if (tsize >= len) {		
		cplen = len - csize - 1;
		repend = len;
	}

	
	memcpy(&str[csize + 1], compat, cplen);
	for (i = csize; i < repend; i++) {
		char c = str[i];
		if (c == '\0')
			str[i] = 'C';
		else if (c == ' ')
			str[i] = '_';
	}

	return tsize;
}

void of_device_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	const char *compat;
	struct alias_prop *app;
	int seen = 0, cplen, sl;

	if ((!dev) || (!dev->of_node))
		return;

	add_uevent_var(env, "OF_NAME=%s", dev->of_node->name);
	add_uevent_var(env, "OF_FULLNAME=%s", dev->of_node->full_name);
	if (dev->of_node->type && strcmp("<NULL>", dev->of_node->type) != 0)
		add_uevent_var(env, "OF_TYPE=%s", dev->of_node->type);

	compat = of_get_property(dev->of_node, "compatible", &cplen);
	while (compat && *compat && cplen > 0) {
		add_uevent_var(env, "OF_COMPATIBLE_%d=%s", seen, compat);
		sl = strlen(compat) + 1;
		compat += sl;
		cplen -= sl;
		seen++;
	}
	add_uevent_var(env, "OF_COMPATIBLE_N=%d", seen);

	seen = 0;
	mutex_lock(&of_aliases_mutex);
	list_for_each_entry(app, &aliases_lookup, link) {
		if (dev->of_node == app->np) {
			add_uevent_var(env, "OF_ALIAS_%d=%s", seen,
				       app->alias);
			seen++;
		}
	}

	if (seen)
		add_uevent_var(env, "OF_ALIAS_N=%d", seen);

	mutex_unlock(&of_aliases_mutex);
}

int of_device_uevent_modalias(struct device *dev, struct kobj_uevent_env *env)
{
	int sl;

	if ((!dev) || (!dev->of_node))
		return -ENODEV;

	
	if (add_uevent_var(env, "MODALIAS="))
		return -ENOMEM;

	sl = of_device_get_modalias(dev, &env->buf[env->buflen-1],
				    sizeof(env->buf) - env->buflen);
	if (sl >= (sizeof(env->buf) - env->buflen))
		return -ENOMEM;
	env->buflen += sl;

	return 0;
}
