#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <asm/string.h>
#include <linux/kmod.h>
#include <linux/sysctl.h>

#include <net/ip_vs.h>

static LIST_HEAD(ip_vs_pe);

static DEFINE_SPINLOCK(ip_vs_pe_lock);

void ip_vs_bind_pe(struct ip_vs_service *svc, struct ip_vs_pe *pe)
{
	svc->pe = pe;
}

void ip_vs_unbind_pe(struct ip_vs_service *svc)
{
	svc->pe = NULL;
}

struct ip_vs_pe *__ip_vs_pe_getbyname(const char *pe_name)
{
	struct ip_vs_pe *pe;

	IP_VS_DBG(10, "%s(): pe_name \"%s\"\n", __func__,
		  pe_name);

	spin_lock_bh(&ip_vs_pe_lock);

	list_for_each_entry(pe, &ip_vs_pe, n_list) {
		
		if (pe->module &&
		    !try_module_get(pe->module)) {
			
			continue;
		}
		if (strcmp(pe_name, pe->name)==0) {
			
			spin_unlock_bh(&ip_vs_pe_lock);
			return pe;
		}
		if (pe->module)
			module_put(pe->module);
	}

	spin_unlock_bh(&ip_vs_pe_lock);
	return NULL;
}

struct ip_vs_pe *ip_vs_pe_getbyname(const char *name)
{
	struct ip_vs_pe *pe;

	
	pe = __ip_vs_pe_getbyname(name);

	
	if (!pe) {
		request_module("ip_vs_pe_%s", name);
		pe = __ip_vs_pe_getbyname(name);
	}

	return pe;
}

int register_ip_vs_pe(struct ip_vs_pe *pe)
{
	struct ip_vs_pe *tmp;

	
	ip_vs_use_count_inc();

	spin_lock_bh(&ip_vs_pe_lock);

	if (!list_empty(&pe->n_list)) {
		spin_unlock_bh(&ip_vs_pe_lock);
		ip_vs_use_count_dec();
		pr_err("%s(): [%s] pe already linked\n",
		       __func__, pe->name);
		return -EINVAL;
	}

	list_for_each_entry(tmp, &ip_vs_pe, n_list) {
		if (strcmp(tmp->name, pe->name) == 0) {
			spin_unlock_bh(&ip_vs_pe_lock);
			ip_vs_use_count_dec();
			pr_err("%s(): [%s] pe already existed "
			       "in the system\n", __func__, pe->name);
			return -EINVAL;
		}
	}
	
	list_add(&pe->n_list, &ip_vs_pe);
	spin_unlock_bh(&ip_vs_pe_lock);

	pr_info("[%s] pe registered.\n", pe->name);

	return 0;
}
EXPORT_SYMBOL_GPL(register_ip_vs_pe);

int unregister_ip_vs_pe(struct ip_vs_pe *pe)
{
	spin_lock_bh(&ip_vs_pe_lock);
	if (list_empty(&pe->n_list)) {
		spin_unlock_bh(&ip_vs_pe_lock);
		pr_err("%s(): [%s] pe is not in the list. failed\n",
		       __func__, pe->name);
		return -EINVAL;
	}

	
	list_del(&pe->n_list);
	spin_unlock_bh(&ip_vs_pe_lock);

	
	ip_vs_use_count_dec();

	pr_info("[%s] pe unregistered.\n", pe->name);

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_ip_vs_pe);
