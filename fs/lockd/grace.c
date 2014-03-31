
#include <linux/module.h>
#include <linux/lockd/bind.h>

static LIST_HEAD(grace_list);
static DEFINE_SPINLOCK(grace_lock);

void locks_start_grace(struct lock_manager *lm)
{
	spin_lock(&grace_lock);
	list_add(&lm->list, &grace_list);
	spin_unlock(&grace_lock);
}
EXPORT_SYMBOL_GPL(locks_start_grace);

void locks_end_grace(struct lock_manager *lm)
{
	spin_lock(&grace_lock);
	list_del_init(&lm->list);
	spin_unlock(&grace_lock);
}
EXPORT_SYMBOL_GPL(locks_end_grace);

int locks_in_grace(void)
{
	return !list_empty(&grace_list);
}
EXPORT_SYMBOL_GPL(locks_in_grace);
