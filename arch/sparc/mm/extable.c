
#include <linux/module.h>
#include <asm/uaccess.h>

void sort_extable(struct exception_table_entry *start,
		  struct exception_table_entry *finish)
{
}

const struct exception_table_entry *
search_extable(const struct exception_table_entry *start,
	       const struct exception_table_entry *last,
	       unsigned long value)
{
	const struct exception_table_entry *walk;


	
	for (walk = start; walk <= last; walk++) {
		if (walk->fixup == 0) {
			
			walk++;
			continue;
		}

		
		if (walk->fixup == -1)
			continue;

		if (walk->insn == value)
			return walk;
	}

	
	for (walk = start; walk <= (last - 1); walk++) {
		if (walk->fixup)
			continue;

		if (walk[0].insn <= value && walk[1].insn > value)
			return walk;

		walk++;
	}

        return NULL;
}

#ifdef CONFIG_MODULES
void trim_init_extable(struct module *m)
{
	unsigned int i;
	bool range;

	for (i = 0; i < m->num_exentries; i += range ? 2 : 1) {
		range = m->extable[i].fixup == 0;

		if (within_module_init(m->extable[i].insn, m)) {
			m->extable[i].fixup = -1;
			if (range)
				m->extable[i+1].fixup = -1;
		}
		if (range)
			i++;
	}
}
#endif 

unsigned long search_extables_range(unsigned long addr, unsigned long *g2)
{
	const struct exception_table_entry *entry;

	entry = search_exception_tables(addr);
	if (!entry)
		return 0;

	
	if (!entry->fixup) {
		*g2 = (addr - entry->insn) / 4;
		return (entry + 1)->fixup;
	}

	return entry->fixup;
}
