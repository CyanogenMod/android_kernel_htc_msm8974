#ifndef __LINUX_BRIDGE_EBT_REDIRECT_H
#define __LINUX_BRIDGE_EBT_REDIRECT_H

struct ebt_redirect_info {
	
	int target;
};
#define EBT_REDIRECT_TARGET "redirect"

#endif
