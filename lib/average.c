/*
 * lib/average.c
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <linux/export.h>
#include <linux/average.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/log2.h>


void ewma_init(struct ewma *avg, unsigned long factor, unsigned long weight)
{
	WARN_ON(!is_power_of_2(weight) || !is_power_of_2(factor));

	avg->weight = ilog2(weight);
	avg->factor = ilog2(factor);
	avg->internal = 0;
}
EXPORT_SYMBOL(ewma_init);

struct ewma *ewma_add(struct ewma *avg, unsigned long val)
{
	avg->internal = avg->internal  ?
		(((avg->internal << avg->weight) - avg->internal) +
			(val << avg->factor)) >> avg->weight :
		(val << avg->factor);
	return avg;
}
EXPORT_SYMBOL(ewma_add);
