#include <linux/kernel.h>

struct product_info {
	const char     *pi_name;
	const char     *pi_type;
};

struct vendor_info {
	const char     *vi_name;
	const struct product_info *vi_product_info;
};

static const char * const txt_base_models[] = {
	"MQ 2", "MQ Pro", "SP 25", "SP 50", "SP 100", "SP 5000", "SP 7000",
	"SP 1000", "Unknown"
};
#define N_BASE_MODELS (ARRAY_SIZE(txt_base_models) - 1)

static const char txt_en_mq[] = "Masquerade";
static const char txt_en_sp[] = "Safepipe";

static const struct product_info product_info_eicon[] = {
	{ txt_en_mq, "II"   }, 
	{ txt_en_mq, "Pro"  }, 
	{ txt_en_sp, "25"   }, 
	{ txt_en_sp, "50"   }, 
	{ txt_en_sp, "100"  }, 
	{ txt_en_sp, "5000" }, 
	{ txt_en_sp, "7000" }, 
	{ txt_en_sp, "30"   }, 
	{ txt_en_sp, "5100" }, 
	{ txt_en_sp, "7100" }, 
	{ txt_en_sp, "1110" }, 
	{ txt_en_sp, "3020" }, 
	{ txt_en_sp, "3030" }, 
	{ txt_en_sp, "5020" }, 
	{ txt_en_sp, "5030" }, 
	{ txt_en_sp, "1120" }, 
	{ txt_en_sp, "1130" }, 
	{ txt_en_sp, "6010" }, 
	{ txt_en_sp, "6110" }, 
	{ txt_en_sp, "6210" }, 
	{ txt_en_sp, "1020" }, 
	{ txt_en_sp, "1040" }, 
	{ txt_en_sp, "1050" }, 
	{ txt_en_sp, "1060" }, 
};

#define N_PRIDS ARRAY_SIZE(product_info_eicon)

static struct vendor_info const vendor_info_table[] = {
	{ "Eicon Networks",	product_info_eicon   },
};

#define N_VENDORS ARRAY_SIZE(vendor_info_table)
