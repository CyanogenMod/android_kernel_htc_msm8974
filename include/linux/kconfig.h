#ifndef __LINUX_KCONFIG_H
#define __LINUX_KCONFIG_H

#include <generated/autoconf.h>


#define __ARG_PLACEHOLDER_1 0,
#define config_enabled(cfg) _config_enabled(cfg)
#define _config_enabled(value) __config_enabled(__ARG_PLACEHOLDER_##value)
#define __config_enabled(arg1_or_junk) ___config_enabled(arg1_or_junk 1, 0)
#define ___config_enabled(__ignored, val, ...) val

#define IS_ENABLED(option) \
	(config_enabled(option) || config_enabled(option##_MODULE))

#define IS_BUILTIN(option) config_enabled(option)

#define IS_MODULE(option) config_enabled(option##_MODULE)

#endif 
