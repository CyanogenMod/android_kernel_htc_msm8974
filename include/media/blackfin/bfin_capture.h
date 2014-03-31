#ifndef _BFIN_CAPTURE_H_
#define _BFIN_CAPTURE_H_

#include <linux/i2c.h>

struct v4l2_input;
struct ppi_info;

struct bcap_route {
	u32 input;
	u32 output;
};

struct bfin_capture_config {
	
	char *card_name;
	
	struct v4l2_input *inputs;
	
	int num_inputs;
	
	struct bcap_route *routes;
	
	int i2c_adapter_id;
	
	struct i2c_board_info board_info;
	
	const struct ppi_info *ppi_info;
	
	unsigned long ppi_control;
	
	u32 int_mask;
	
	int blank_clocks;
};

#endif
