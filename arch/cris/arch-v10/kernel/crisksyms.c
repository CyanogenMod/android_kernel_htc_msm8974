#include <linux/module.h>
#include <asm/io.h>
#include <arch/svinto.h>

EXPORT_SYMBOL(genconfig_shadow);
EXPORT_SYMBOL(port_pa_data_shadow);
EXPORT_SYMBOL(port_pa_dir_shadow);
EXPORT_SYMBOL(port_pb_data_shadow);
EXPORT_SYMBOL(port_pb_dir_shadow);
EXPORT_SYMBOL(port_pb_config_shadow);
EXPORT_SYMBOL(port_g_data_shadow);

EXPORT_SYMBOL(flush_etrax_cache);
EXPORT_SYMBOL(prepare_rx_descriptor);
