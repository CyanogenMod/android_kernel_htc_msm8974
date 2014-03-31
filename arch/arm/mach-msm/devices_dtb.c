#include <mach/devices_dtb.h>
#include <linux/of.h>

#define CONFIG_DATA_PATH "/chosen/config"

static unsigned int cfg_flag_index[NUM_FLAG_INDEX];

static bool has_config_data = false;
static int config_data_init(void);

unsigned int get_debug_flag(void)
{
	if (!has_config_data)
		config_data_init();
        return cfg_flag_index[DEBUG_FLAG_INDEX];
}

unsigned int get_kernel_flag(void)
{
	if (!has_config_data)
		config_data_init();
        return cfg_flag_index[KERNEL_FLAG_INDEX];
}

unsigned int get_bootloader_flag(void)
{
	if (!has_config_data)
		config_data_init();
        return cfg_flag_index[BOOTLOADER_FLAG_INDEX];
}

unsigned int get_radio_flag(void)
{
	if (!has_config_data)
		config_data_init();
        return cfg_flag_index[RADIO_FLAG_INDEX];
}

unsigned int get_radio_flag_ex1(void)
{
	if (!has_config_data)
		config_data_init();
        return cfg_flag_index[RADIO_FLAG_EX1_INDEX];
}

unsigned int get_radio_flag_ex2(void)
{
	if (!has_config_data)
		config_data_init();
        return cfg_flag_index[RADIO_FLAG_EX1_INDEX];
}

static int config_data_init(void)
{
	struct device_node *of_chosen = of_find_node_by_path(CONFIG_DATA_PATH);
	struct property *pp;
	const __be32 *val;
	int ret = 1;

	if (of_chosen) {
		pr_info("CONFIG DATA:\n");
		for_each_property_of_node(of_chosen, pp) {
			if (pp) {
				val = pp->value;
				switch (be32_to_cpup(val++)) {
				case DEBUG_FLAG_INDEX:
					cfg_flag_index[DEBUG_FLAG_INDEX] = be32_to_cpup(val++);
					pr_info("\tDEBUG_FLAG_INDEX: %x\n", cfg_flag_index[DEBUG_FLAG_INDEX]);
					break;
				case KERNEL_FLAG_INDEX:
					cfg_flag_index[KERNEL_FLAG_INDEX] = be32_to_cpup(val++);
					pr_info("\tKERNEL_FLAG_INDEX: %x\n", cfg_flag_index[KERNEL_FLAG_INDEX]);
					break;
				case BOOTLOADER_FLAG_INDEX:
					cfg_flag_index[BOOTLOADER_FLAG_INDEX] = be32_to_cpup(val++);
					pr_info("\tBOOTLOADER_FLAG_INDEX: %x\n", cfg_flag_index[BOOTLOADER_FLAG_INDEX]);
					break;
				case RADIO_FLAG_INDEX:
					cfg_flag_index[RADIO_FLAG_INDEX] = be32_to_cpup(val++);
					pr_info("\tRADIO_FLAG_INDEX: %x\n", cfg_flag_index[RADIO_FLAG_INDEX]);
					break;
				case RADIO_FLAG_EX1_INDEX:
					cfg_flag_index[RADIO_FLAG_EX1_INDEX] = be32_to_cpup(val++);
					pr_info("\tRADIO_FLAG_EX1_INDEX: %x\n", cfg_flag_index[RADIO_FLAG_EX1_INDEX]);
					break;
				case RADIO_FLAG_EX2_INDEX:
					cfg_flag_index[RADIO_FLAG_EX2_INDEX] = be32_to_cpup(val++);
					pr_info("\tRADIO_FLAG_EX2_INDEX: %x\n", cfg_flag_index[RADIO_FLAG_EX2_INDEX]);
					break;
				default:
					break;
				}
			}
		}
		ret = 0;
		has_config_data = true;
	} else {
		pr_err("!!! COULDN'T FIND CONFIG DATA node !!!\n");
		
		cfg_flag_index[KERNEL_FLAG_INDEX] = 0x2;
	}

	return ret;
}
