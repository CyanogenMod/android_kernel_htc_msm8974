#include "wbhal.h"
#include "wb35reg_f.h"
#include "core.h"



u32 max2825_rf_data[] = {
	(0x00<<18) | 0x000a2,
	(0x01<<18) | 0x21cc0,
	(0x02<<18) | 0x13806,
	(0x03<<18) | 0x30142,
	(0x04<<18) | 0x0b333,
	(0x05<<18) | 0x289A6,
	(0x06<<18) | 0x18008,
	(0x07<<18) | 0x38000,
	(0x08<<18) | 0x05100,
	(0x09<<18) | 0x24f08,
	(0x0A<<18) | 0x14000,
	(0x0B<<18) | 0x37d80,
	(0x0C<<18) | 0x0c100   
};

u32 max2825_channel_data_24[][3] = {
	{(0x03 << 18) | 0x30142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30941, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30943, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30942, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32941, (0x04 << 18) | 0x09999, (0x05 << 18) | 0x289A6}  
};

u32 max2825_power_data_24[] = {(0x0C << 18) | 0x0c000, (0x0C << 18) | 0x0c100};

u32 max2827_rf_data[] = {
	(0x00 << 18) | 0x000a2,
	(0x01 << 18) | 0x21cc0,
	(0x02 << 18) | 0x13806,
	(0x03 << 18) | 0x30142,
	(0x04 << 18) | 0x0b333,
	(0x05 << 18) | 0x289A6,
	(0x06 << 18) | 0x18008,
	(0x07 << 18) | 0x38000,
	(0x08 << 18) | 0x05100,
	(0x09 << 18) | 0x24f08,
	(0x0A << 18) | 0x14000,
	(0x0B << 18) | 0x37d80,
	(0x0C << 18) | 0x0c100   
};

u32 max2827_channel_data_24[][3] = {
	{(0x03 << 18) | 0x30142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30941, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30943, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30942, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32941, (0x04 << 18) | 0x09999, (0x05 << 18) | 0x289A6}  
};

u32 max2827_channel_data_50[][3] = {
	{(0x03 << 18) | 0x33cc3, (0x04 << 18) | 0x08ccc, (0x05 << 18) | 0x2A9A6}, 
	{(0x03 << 18) | 0x302c0, (0x04 << 18) | 0x08000, (0x05 << 18) | 0x2A9A6}, 
	{(0x03 << 18) | 0x302c2, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x2A9A6}, 
	{(0x03 << 18) | 0x322c1, (0x04 << 18) | 0x09999, (0x05 << 18) | 0x2A9A6}, 
	{(0x03 << 18) | 0x312c1, (0x04 << 18) | 0x0a666, (0x05 << 18) | 0x2A9A6}, 
	{(0x03 << 18) | 0x332c3, (0x04 << 18) | 0x08ccc, (0x05 << 18) | 0x2A9A6}, 
	{(0x03 << 18) | 0x30ac0, (0x04 << 18) | 0x08000, (0x05 << 18) | 0x2A9A6}, 
	{(0x03 << 18) | 0x30ac2, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x2A9A6}  
};

u32 max2827_power_data_24[] = {(0x0C << 18) | 0x0C000, (0x0C << 18) | 0x0D600, (0x0C << 18) | 0x0C100};
u32 max2827_power_data_50[] = {(0x0C << 18) | 0x0C400, (0x0C << 18) | 0x0D500, (0x0C << 18) | 0x0C300};

u32 max2828_rf_data[] = {
	(0x00 << 18) | 0x000a2,
	(0x01 << 18) | 0x21cc0,
	(0x02 << 18) | 0x13806,
	(0x03 << 18) | 0x30142,
	(0x04 << 18) | 0x0b333,
	(0x05 << 18) | 0x289A6,
	(0x06 << 18) | 0x18008,
	(0x07 << 18) | 0x38000,
	(0x08 << 18) | 0x05100,
	(0x09 << 18) | 0x24f08,
	(0x0A << 18) | 0x14000,
	(0x0B << 18) | 0x37d80,
	(0x0C << 18) | 0x0c100   
};

u32 max2828_channel_data_24[][3] = {
	{(0x03 << 18) | 0x30142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x31142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33141, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33143, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x33142, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30941, (0x04 << 18) | 0x08444, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30943, (0x04 << 18) | 0x0aeee, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30942, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x32941, (0x04 << 18) | 0x09999, (0x05 << 18) | 0x289A6}  
};

u32 max2828_channel_data_50[][3] = {
	{(0x03 << 18) | 0x33cc3, (0x04 << 18) | 0x08ccc, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x302c0, (0x04 << 18) | 0x08000, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x302c2, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x322c1, (0x04 << 18) | 0x09999, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x312c1, (0x04 << 18) | 0x0a666, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x332c3, (0x04 << 18) | 0x08ccc, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30ac0, (0x04 << 18) | 0x08000, (0x05 << 18) | 0x289A6}, 
	{(0x03 << 18) | 0x30ac2, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x289A6}  
};

u32 max2828_power_data_24[] = {(0x0C << 18) | 0x0c000, (0x0C << 18) | 0x0c100};
u32 max2828_power_data_50[] = {(0x0C << 18) | 0x0c000, (0x0C << 18) | 0x0c100};

u32 max2829_rf_data[] = {
	(0x00 << 18) | 0x000a2,
	(0x01 << 18) | 0x23520,
	(0x02 << 18) | 0x13802,
	(0x03 << 18) | 0x30142,
	(0x04 << 18) | 0x0b333,
	(0x05 << 18) | 0x28906,
	(0x06 << 18) | 0x18008,
	(0x07 << 18) | 0x3B500,
	(0x08 << 18) | 0x05100,
	(0x09 << 18) | 0x24f08,
	(0x0A << 18) | 0x14000,
	(0x0B << 18) | 0x37d80,
	(0x0C << 18) | 0x0F300 
};

u32 max2829_channel_data_24[][3] = {
	{(3 << 18) | 0x30142, (4 << 18) | 0x0b333, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x32141, (4 << 18) | 0x08444, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x32143, (4 << 18) | 0x0aeee, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x32142, (4 << 18) | 0x0b333, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x31141, (4 << 18) | 0x08444, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x31143, (4 << 18) | 0x0aeee, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x31142, (4 << 18) | 0x0b333, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x33141, (4 << 18) | 0x08444, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x33143, (4 << 18) | 0x0aeee, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x33142, (4 << 18) | 0x0b333, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x30941, (4 << 18) | 0x08444, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x30943, (4 << 18) | 0x0aeee, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x30942, (4 << 18) | 0x0b333, (5 << 18) | 0x289C6},  
	{(3 << 18) | 0x32941, (4 << 18) | 0x09999, (5 << 18) | 0x289C6},  
};

u32 max2829_channel_data_50[][4] = {
	{36, (3 << 18) | 0x33cc3, (4 << 18) | 0x08ccc, (5 << 18) | 0x2A946}, 
	{40, (3 << 18) | 0x302c0, (4 << 18) | 0x08000, (5 << 18) | 0x2A946}, 
	{44, (3 << 18) | 0x302c2, (4 << 18) | 0x0b333, (5 << 18) | 0x2A946}, 
	{48, (3 << 18) | 0x322c1, (4 << 18) | 0x09999, (5 << 18) | 0x2A946}, 
	{52, (3 << 18) | 0x312c1, (4 << 18) | 0x0a666, (5 << 18) | 0x2A946}, 
	{56, (3 << 18) | 0x332c3, (4 << 18) | 0x08ccc, (5 << 18) | 0x2A946}, 
	{60, (3 << 18) | 0x30ac0, (4 << 18) | 0x08000, (5 << 18) | 0x2A946}, 
	{64, (3 << 18) | 0x30ac2, (4 << 18) | 0x0b333, (5 << 18) | 0x2A946}, 

	{100, (3 << 18) | 0x30ec0, (4 << 18) | 0x08000, (5 << 18) | 0x2A9C6}, 
	{104, (3 << 18) | 0x30ec2, (4 << 18) | 0x0b333, (5 << 18) | 0x2A9C6}, 
	{108, (3 << 18) | 0x32ec1, (4 << 18) | 0x09999, (5 << 18) | 0x2A9C6}, 
	{112, (3 << 18) | 0x31ec1, (4 << 18) | 0x0a666, (5 << 18) | 0x2A9C6}, 
	{116, (3 << 18) | 0x33ec3, (4 << 18) | 0x08ccc, (5 << 18) | 0x2A9C6}, 
	{120, (3 << 18) | 0x301c0, (4 << 18) | 0x08000, (5 << 18) | 0x2A9C6}, 
	{124, (3 << 18) | 0x301c2, (4 << 18) | 0x0b333, (5 << 18) | 0x2A9C6}, 
	{128, (3 << 18) | 0x321c1, (4 << 18) | 0x09999, (5 << 18) | 0x2A9C6}, 
	{132, (3 << 18) | 0x311c1, (4 << 18) | 0x0a666, (5 << 18) | 0x2A9C6}, 
	{136, (3 << 18) | 0x331c3, (4 << 18) | 0x08ccc, (5 << 18) | 0x2A9C6}, 
	{140, (3 << 18) | 0x309c0, (4 << 18) | 0x08000, (5 << 18) | 0x2A9C6}, 

	{149, (3 << 18) | 0x329c2, (4 << 18) | 0x0b333, (5 << 18) | 0x2A9C6}, 
	{153, (3 << 18) | 0x319c1, (4 << 18) | 0x09999, (5 << 18) | 0x2A9C6}, 
	{157, (3 << 18) | 0x339c1, (4 << 18) | 0x0a666, (5 << 18) | 0x2A9C6}, 
	{161, (3 << 18) | 0x305c3, (4 << 18) | 0x08ccc, (5 << 18) | 0x2A9C6}, 

	
	{ 184, (3 << 18) | 0x308c2, (4 << 18) | 0x0b333, (5 << 18) | 0x2A946}, 
	{ 188, (3 << 18) | 0x328c1, (4 << 18) | 0x09999, (5 << 18) | 0x2A946}, 
	{ 192, (3 << 18) | 0x318c1, (4 << 18) | 0x0a666, (5 << 18) | 0x2A946}, 
	{ 196, (3 << 18) | 0x338c3, (4 << 18) | 0x08ccc, (5 << 18) | 0x2A946}, 
	{   8, (3 << 18) | 0x324c1, (4 << 18) | 0x09999, (5 << 18) | 0x2A946}, 
	{  12, (3 << 18) | 0x314c1, (4 << 18) | 0x0a666, (5 << 18) | 0x2A946}, 
	{  16, (3 << 18) | 0x334c3, (4 << 18) | 0x08ccc, (5 << 18) | 0x2A946}, 
	{  34, (3 << 18) | 0x31cc2, (4 << 18) | 0x0b333, (5 << 18) | 0x2A946}, 
	{  38, (3 << 18) | 0x33cc1, (4 << 18) | 0x09999, (5 << 18) | 0x2A946}, 
	{  42, (3 << 18) | 0x302c1, (4 << 18) | 0x0a666, (5 << 18) | 0x2A946}, 
	{  46, (3 << 18) | 0x322c3, (4 << 18) | 0x08ccc, (5 << 18) | 0x2A946}, 
};

u32 maxim_317_rf_data[] = {
	(0x00 << 18) | 0x000a2,
	(0x01 << 18) | 0x214c0,
	(0x02 << 18) | 0x13802,
	(0x03 << 18) | 0x30143,
	(0x04 << 18) | 0x0accc,
	(0x05 << 18) | 0x28986,
	(0x06 << 18) | 0x18008,
	(0x07 << 18) | 0x38400,
	(0x08 << 18) | 0x05108,
	(0x09 << 18) | 0x27ff8,
	(0x0A << 18) | 0x14000,
	(0x0B << 18) | 0x37f99,
	(0x0C << 18) | 0x0c000
};

u32 maxim_317_channel_data_24[][3] = {
	{(0x03 << 18) | 0x30143, (0x04 << 18) | 0x0accc, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x32140, (0x04 << 18) | 0x09111, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x32142, (0x04 << 18) | 0x0bbbb, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x32143, (0x04 << 18) | 0x0accc, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x31140, (0x04 << 18) | 0x09111, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x31142, (0x04 << 18) | 0x0bbbb, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x31143, (0x04 << 18) | 0x0accc, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x33140, (0x04 << 18) | 0x09111, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x33142, (0x04 << 18) | 0x0bbbb, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x33143, (0x04 << 18) | 0x0accc, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x30940, (0x04 << 18) | 0x09111, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x30942, (0x04 << 18) | 0x0bbbb, (0x05 << 18) | 0x28986}, 
	{(0x03 << 18) | 0x30943, (0x04 << 18) | 0x0accc, (0x05 << 18) | 0x28986}  
};

u32 maxim_317_channel_data_50[][3] = {
	{(0x03 << 18) | 0x33cc0, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x2a986}, 
	{(0x03 << 18) | 0x302c0, (0x04 << 18) | 0x08000, (0x05 << 18) | 0x2a986}, 
	{(0x03 << 18) | 0x302c3, (0x04 << 18) | 0x0accc, (0x05 << 18) | 0x2a986}, 
	{(0x03 << 18) | 0x322c1, (0x04 << 18) | 0x09666, (0x05 << 18) | 0x2a986}, 
	{(0x03 << 18) | 0x312c2, (0x04 << 18) | 0x09999, (0x05 << 18) | 0x2a986}, 
	{(0x03 << 18) | 0x332c0, (0x04 << 18) | 0x0b333, (0x05 << 18) | 0x2a99e}, 
	{(0x03 << 18) | 0x30ac0, (0x04 << 18) | 0x08000, (0x05 << 18) | 0x2a99e}, 
	{(0x03 << 18) | 0x30ac3, (0x04 << 18) | 0x0accc, (0x05 << 18) | 0x2a99e}  
};

u32 maxim_317_power_data_24[] = {(0x0C << 18) | 0x0c000, (0x0C << 18) | 0x0c100};
u32 maxim_317_power_data_50[] = {(0x0C << 18) | 0x0c000, (0x0C << 18) | 0x0c100};

u32 al2230_rf_data[] = {
	(0x00 << 20) | 0x09EFC,
	(0x01 << 20) | 0x8CCCC,
	(0x02 << 20) | 0x40058,
	(0x03 << 20) | 0xCFFF0,
	(0x04 << 20) | 0x24100,
	(0x05 << 20) | 0xA3B2F,
	(0x06 << 20) | 0x6DA01,
	(0x07 << 20) | 0xE3628,
	(0x08 << 20) | 0x11600,
	(0x09 << 20) | 0x9DC02,
	(0x0A << 20) | 0x5ddb0,
	(0x0B << 20) | 0xD9900,
	(0x0C << 20) | 0x3FFBD,
	(0x0D << 20) | 0xB0000,
	(0x0F << 20) | 0xF01A0
};

u32 al2230s_rf_data[] = {
	(0x00 << 20) | 0x09EFC,
	(0x01 << 20) | 0x8CCCC,
	(0x02 << 20) | 0x40058,
	(0x03 << 20) | 0xCFFF0,
	(0x04 << 20) | 0x24100,
	(0x05 << 20) | 0xA3B2F,
	(0x06 << 20) | 0x6DA01,
	(0x07 << 20) | 0xE3628,
	(0x08 << 20) | 0x11600,
	(0x09 << 20) | 0x9DC02,
	(0x0A << 20) | 0x5DDB0,
	(0x0B << 20) | 0xD9900,
	(0x0C << 20) | 0x3FFBD,
	(0x0D << 20) | 0xB0000,
	(0x0F << 20) | 0xF01A0
};

u32 al2230_channel_data_24[][2] = {
	{(0x00 << 20) | 0x09EFC, (0x01 << 20) | 0x8CCCC}, 
	{(0x00 << 20) | 0x09EFC, (0x01 << 20) | 0x8CCCD}, 
	{(0x00 << 20) | 0x09E7C, (0x01 << 20) | 0x8CCCC}, 
	{(0x00 << 20) | 0x09E7C, (0x01 << 20) | 0x8CCCD}, 
	{(0x00 << 20) | 0x05EFC, (0x01 << 20) | 0x8CCCC}, 
	{(0x00 << 20) | 0x05EFC, (0x01 << 20) | 0x8CCCD}, 
	{(0x00 << 20) | 0x05E7C, (0x01 << 20) | 0x8CCCC}, 
	{(0x00 << 20) | 0x05E7C, (0x01 << 20) | 0x8CCCD}, 
	{(0x00 << 20) | 0x0DEFC, (0x01 << 20) | 0x8CCCC}, 
	{(0x00 << 20) | 0x0DEFC, (0x01 << 20) | 0x8CCCD}, 
	{(0x00 << 20) | 0x0DE7C, (0x01 << 20) | 0x8CCCC}, 
	{(0x00 << 20) | 0x0DE7C, (0x01 << 20) | 0x8CCCD}, 
	{(0x00 << 20) | 0x03EFC, (0x01 << 20) | 0x8CCCC}, 
	{(0x00 << 20) | 0x03E7C, (0x01 << 20) | 0x86666}  
};

#define AIROHA_TXVGA_LOW_INDEX		31	
#define AIROHA_TXVGA_MIDDLE_INDEX	12	
#define AIROHA_TXVGA_HIGH_INDEX		8	

u32 al2230_txvga_data[][2] = {
	
	{0x090202, 0},
	{0x094202, 2},
	{0x092202, 4},
	{0x096202, 6},
	{0x091202, 8},
	{0x095202, 10},
	{0x093202, 12},
	{0x097202, 14},
	{0x090A02, 16},
	{0x094A02, 18},
	{0x092A02, 20},
	{0x096A02, 22},
	{0x091A02, 24},
	{0x095A02, 26},
	{0x093A02, 28},
	{0x097A02, 30},
	{0x090602, 32},
	{0x094602, 34},
	{0x092602, 36},
	{0x096602, 38},
	{0x091602, 40},
	{0x095602, 42},
	{0x093602, 44},
	{0x097602, 46},
	{0x090E02, 48},
	{0x098E02, 49},
	{0x094E02, 50},
	{0x09CE02, 51},
	{0x092E02, 52},
	{0x09AE02, 53},
	{0x096E02, 54},
	{0x09EE02, 55},
	{0x091E02, 56},
	{0x099E02, 57},
	{0x095E02, 58},
	{0x09DE02, 59},
	{0x093E02, 60},
	{0x09BE02, 61},
	{0x097E02, 62},
	{0x09FE02, 63}
};


u32 al7230_rf_data_24[]	= {
	(0x00 << 24) | 0x003790,
	(0x01 << 24) | 0x133331,
	(0x02 << 24) | 0x841FF2,
	(0x03 << 24) | 0x3FDFA3,
	(0x04 << 24) | 0x7FD784,
	(0x05 << 24) | 0x802B55,
	(0x06 << 24) | 0x56AF36,
	(0x07 << 24) | 0xCE0207,
	(0x08 << 24) | 0x6EBC08,
	(0x09 << 24) | 0x221BB9,
	(0x0A << 24) | 0xE0000A,
	(0x0B << 24) | 0x08071B,
	(0x0C << 24) | 0x000A3C,
	(0x0D << 24) | 0xFFFFFD,
	(0x0E << 24) | 0x00000E,
	(0x0F << 24) | 0x1ABA8F
};

u32 al7230_channel_data_24[][2] = {
	{(0x00 << 24) | 0x003790, (0x01 << 24) | 0x133331}, 
	{(0x00 << 24) | 0x003790, (0x01 << 24) | 0x1B3331}, 
	{(0x00 << 24) | 0x003790, (0x01 << 24) | 0x033331}, 
	{(0x00 << 24) | 0x003790, (0x01 << 24) | 0x0B3331}, 
	{(0x00 << 24) | 0x0037A0, (0x01 << 24) | 0x133331}, 
	{(0x00 << 24) | 0x0037A0, (0x01 << 24) | 0x1B3331}, 
	{(0x00 << 24) | 0x0037A0, (0x01 << 24) | 0x033331}, 
	{(0x00 << 24) | 0x0037A0, (0x01 << 24) | 0x0B3331}, 
	{(0x00 << 24) | 0x0037B0, (0x01 << 24) | 0x133331}, 
	{(0x00 << 24) | 0x0037B0, (0x01 << 24) | 0x1B3331}, 
	{(0x00 << 24) | 0x0037B0, (0x01 << 24) | 0x033331}, 
	{(0x00 << 24) | 0x0037B0, (0x01 << 24) | 0x0B3331}, 
	{(0x00 << 24) | 0x0037C0, (0x01 << 24) | 0x133331}, 
	{(0x00 << 24) | 0x0037C0, (0x01 << 24) | 0x066661}  
};

u32 al7230_rf_data_50[]	= {
	(0x00 << 24) | 0x0FF520,
	(0x01 << 24) | 0x000001,
	(0x02 << 24) | 0x451FE2,
	(0x03 << 24) | 0x5FDFA3,
	(0x04 << 24) | 0x6FD784,
	(0x05 << 24) | 0x853F55,
	(0x06 << 24) | 0x56AF36,
	(0x07 << 24) | 0xCE0207,
	(0x08 << 24) | 0x6EBC08,
	(0x09 << 24) | 0x221BB9,
	(0x0A << 24) | 0xE0600A,
	(0x0B << 24) | 0x08044B,
	(0x0C << 24) | 0x00143C,
	(0x0D << 24) | 0xFFFFFD,
	(0x0E << 24) | 0x00000E,
	(0x0F << 24) | 0x12BACF  
};

u32 al7230_channel_data_5[][4] = {
	
	
	{184, (0x00 << 24) | 0x0FF520, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{188, (0x00 << 24) | 0x0FF520, (0x01 << 24) | 0x0AAAA1, (0x04 << 24) | 0x77F784}, 
	{192, (0x00 << 24) | 0x0FF530, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{196, (0x00 << 24) | 0x0FF530, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{8,   (0x00 << 24) | 0x0FF540, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{12,  (0x00 << 24) | 0x0FF540, (0x01 << 24) | 0x0AAAA1, (0x04 << 24) | 0x77F784}, 
	{16,  (0x00 << 24) | 0x0FF550, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{34,  (0x00 << 24) | 0x0FF560, (0x01 << 24) | 0x055551, (0x04 << 24) | 0x77F784}, 
	{38,  (0x00 << 24) | 0x0FF570, (0x01 << 24) | 0x100001, (0x04 << 24) | 0x77F784}, 
	{42,  (0x00 << 24) | 0x0FF570, (0x01 << 24) | 0x1AAAA1, (0x04 << 24) | 0x77F784}, 
	{46,  (0x00 << 24) | 0x0FF570, (0x01 << 24) | 0x055551, (0x04 << 24) | 0x77F784}, 
	
	{36,  (0x00 << 24) | 0x0FF560, (0x01 << 24) | 0x0AAAA1, (0x04 << 24) | 0x77F784}, 
	{40,  (0x00 << 24) | 0x0FF570, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{44,  (0x00 << 24) | 0x0FF570, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{48,  (0x00 << 24) | 0x0FF570, (0x01 << 24) | 0x0AAAA1, (0x04 << 24) | 0x77F784}, 
	{52,  (0x00 << 24) | 0x0FF580, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{56,  (0x00 << 24) | 0x0FF580, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{60,  (0x00 << 24) | 0x0FF580, (0x01 << 24) | 0x0AAAA1, (0x04 << 24) | 0x77F784}, 
	{64,  (0x00 << 24) | 0x0FF590, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{100, (0x00 << 24) | 0x0FF5C0, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{104, (0x00 << 24) | 0x0FF5C0, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{108, (0x00 << 24) | 0x0FF5C0, (0x01 << 24) | 0x0AAAA1, (0x04 << 24) | 0x77F784}, 
	{112, (0x00 << 24) | 0x0FF5D0, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{116, (0x00 << 24) | 0x0FF5D0, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{120, (0x00 << 24) | 0x0FF5D0, (0x01 << 24) | 0x0AAAA1, (0x04 << 24) | 0x77F784}, 
	{124, (0x00 << 24) | 0x0FF5E0, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{128, (0x00 << 24) | 0x0FF5E0, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{132, (0x00 << 24) | 0x0FF5E0, (0x01 << 24) | 0x0AAAA1, (0x04 << 24) | 0x77F784}, 
	{136, (0x00 << 24) | 0x0FF5F0, (0x01 << 24) | 0x155551, (0x04 << 24) | 0x77F784}, 
	{140, (0x00 << 24) | 0x0FF5F0, (0x01 << 24) | 0x000001, (0x04 << 24) | 0x67F784}, 
	{149, (0x00 << 24) | 0x0FF600, (0x01 << 24) | 0x180001, (0x04 << 24) | 0x77F784}, 
	{153, (0x00 << 24) | 0x0FF600, (0x01 << 24) | 0x02AAA1, (0x04 << 24) | 0x77F784}, 
	{157, (0x00 << 24) | 0x0FF600, (0x01 << 24) | 0x0D5551, (0x04 << 24) | 0x77F784}, 
	{161, (0x00 << 24) | 0x0FF610, (0x01 << 24) | 0x180001, (0x04 << 24) | 0x77F784}, 
	{165, (0x00 << 24) | 0x0FF610, (0x01 << 24) | 0x02AAA1, (0x04 << 24) | 0x77F784}  
};


u32 al7230_txvga_data[][2] = {
	{0x08040B, 0}, 
	{0x08041B, 1}, 
	{0x08042B, 2}, 
	{0x08043B, 3}, 
	{0x08044B, 4}, 
	{0x08045B, 5}, 
	{0x08046B, 6}, 
	{0x08047B, 7}, 
	{0x08048B, 8}, 
	{0x08049B, 9}, 
	{0x0804AB, 10}, 
	{0x0804BB, 11}, 
	{0x0804CB, 12}, 
	{0x0804DB, 13}, 
	{0x0804EB, 14}, 
	{0x0804FB, 15}, 
	{0x08050B, 16}, 
	{0x08051B, 17}, 
	{0x08052B, 18}, 
	{0x08053B, 19}, 
	{0x08054B, 20}, 
	{0x08055B, 21}, 
	{0x08056B, 22}, 
	{0x08057B, 23}, 
	{0x08058B, 24}, 
	{0x08059B, 25}, 
	{0x0805AB, 26}, 
	{0x0805BB, 27}, 
	{0x0805CB, 28}, 
	{0x0805DB, 29}, 
	{0x0805EB, 30}, 
	{0x0805FB, 31}, 
	{0x08060B, 32}, 
	{0x08061B, 33}, 
	{0x08062B, 34}, 
	{0x08063B, 35}, 
	{0x08064B, 36}, 
	{0x08065B, 37}, 
	{0x08066B, 38}, 
	{0x08067B, 39}, 
	{0x08068B, 40}, 
	{0x08069B, 41}, 
	{0x0806AB, 42}, 
	{0x0806BB, 43}, 
	{0x0806CB, 44}, 
	{0x0806DB, 45}, 
	{0x0806EB, 46}, 
	{0x0806FB, 47}, 
	{0x08070B, 48}, 
	{0x08071B, 49}, 
	{0x08072B, 50}, 
	{0x08073B, 51}, 
	{0x08074B, 52}, 
	{0x08075B, 53}, 
	{0x08076B, 54}, 
	{0x08077B, 55}, 
	{0x08078B, 56}, 
	{0x08079B, 57}, 
	{0x0807AB, 58}, 
	{0x0807BB, 59}, 
	{0x0807CB, 60}, 
	{0x0807DB, 61}, 
	{0x0807EB, 62}, 
	{0x0807FB, 63}, 
};

u32 w89rf242_rf_data[] = {
	(0x00 << 24) | 0xF86100, 
	(0x01 << 24) | 0xEFFFC2, 
	(0x02 << 24) | 0x102504, 
	(0x03 << 24) | 0x026286, 
	(0x04 << 24) | 0x000208, 
	(0x05 << 24) | 0x24C60A, 
	(0x06 << 24) | 0x3432CC, 
	(0x07 << 24) | 0x0C68CE, 
	(0x08 << 24) | 0x100010, 
	(0x09 << 24) | 0x004012, 
	(0x0A << 24) | 0x704014, 
	(0x0B << 24) | 0x18BDD6, 
	(0x0C << 24) | 0x575558, 
	(0x0D << 24) | 0x55545A, 
	(0x0E << 24) | 0x5557DC, 
	(0x10 << 24) | 0x000C20, 
	(0x11 << 24) | 0x0C0022, 
	(0x12 << 24) | 0x000024  
};

u32 w89rf242_channel_data_24[][2] = {
	{(0x03 << 24) | 0x025B06, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x025C46, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x025D86, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x025EC6, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x026006, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x026146, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x026286, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x0263C6, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x026506, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x026646, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x026786, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x0268C6, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x026A06, (0x04 << 24) | 0x080408}, 
	{(0x03 << 24) | 0x026D06, (0x04 << 24) | 0x080408}  
};

u32 w89rf242_power_data_24[] = {(0x05 << 24) | 0x24C48A, (0x05 << 24) | 0x24C48A, (0x05 << 24) | 0x24C48A};

u32 w89rf242_txvga_old_mapping[][2] = {
	{0, 0} , 
	{1, 1} ,
	{2, 2} ,
	{3, 3} ,
	{4, 4} ,
	{6, 5} ,
	{8, 6},
	{10, 7},
	{12, 8},
	{14, 9},
	{16, 10},
	{18, 11},
	{20, 12},
	{22, 13},
	{24, 14},
	{26, 15},
	{28, 16},
	{30, 17},
	{32, 18},
	{34, 19},
};

u32 w89rf242_txvga_data[][5] = {
	
	{(0x05 << 24) | 0x24C00A, 0, 0x00292315, 0x0800FEFF, 0x52523131}, 
	{(0x05 << 24) | 0x24C80A, 1, 0x00292315, 0x0800FEFF, 0x52523131},
	{(0x05 << 24) | 0x24C04A, 2, 0x00292315, 0x0800FEFF, 0x52523131}, 
	{(0x05 << 24) | 0x24C84A, 3, 0x00292315, 0x0800FEFF, 0x52523131},

	
	{(0x05 << 24) | 0x24C40A, 4, 0x00292315, 0x0800FEFF, 0x60603838},
	{(0x05 << 24) | 0x24C40A, 5, 0x00262114, 0x0700FEFF, 0x65653B3B},

	
	{ (0x05 << 24) | 0x24C44A, 6, 0x00241F13, 0x0700FFFF, 0x58583333},
	{ (0x05 << 24) | 0x24C44A, 7, 0x00292315, 0x0800FEFF, 0x5E5E3737},

	
	{(0x05 << 24) | 0x24C48A, 8, 0x00262114, 0x0700FEFF, 0x53533030},
	{(0x05 << 24) | 0x24C48A, 9, 0x00241F13, 0x0700FFFF, 0x59593434},

	
	{(0x05 << 24) | 0x24C4CA, 10, 0x00292315, 0x0800FEFF, 0x52523030},
	{(0x05 << 24) | 0x24C4CA, 11, 0x00262114, 0x0700FEFF, 0x56563232},

	
	{(0x05 << 24) | 0x24C50A, 12, 0x00292315, 0x0800FEFF, 0x54543131},
	{(0x05 << 24) | 0x24C50A, 13, 0x00262114, 0x0700FEFF, 0x58583434},

	
	{(0x05 << 24) | 0x24C54A, 14, 0x00292315, 0x0800FEFF, 0x54543131},
	{(0x05 << 24) | 0x24C54A, 15, 0x00262114, 0x0700FEFF, 0x59593434},

	
	{(0x05 << 24) | 0x24C58A, 16, 0x00292315, 0x0800FEFF, 0x55553131},
	{(0x05 << 24) | 0x24C58A, 17, 0x00292315, 0x0800FEFF, 0x5B5B3535},

	
	{(0x05 << 24) | 0x24C5CA, 18, 0x00262114, 0x0700FEFF, 0x51512F2F},
	{(0x05 << 24) | 0x24C5CA, 19, 0x00241F13, 0x0700FFFF, 0x55553131},

	
	{(0x05 << 24) | 0x24C60A, 20, 0x00292315, 0x0800FEFF, 0x4F4F2E2E},
	{(0x05 << 24) | 0x24C60A, 21, 0x00262114, 0x0700FEFF, 0x53533030},

	
	{(0x05 << 24) | 0x24C64A, 22, 0x00292315, 0x0800FEFF, 0x4E4E2D2D},
	{(0x05 << 24) | 0x24C64A, 23, 0x00262114, 0x0700FEFF, 0x53533030},

	
	{(0x05 << 24) | 0x24C68A, 24, 0x00292315, 0x0800FEFF, 0x50502E2E},
	{(0x05 << 24) | 0x24C68A, 25, 0x00262114, 0x0700FEFF, 0x55553131},

	
	{(0x05 << 24) | 0x24C6CA, 26, 0x00262114, 0x0700FEFF, 0x53533030},
	{(0x05 << 24) | 0x24C6CA, 27, 0x00292315, 0x0800FEFF, 0x5A5A3434},

	
	{(0x05 << 24) | 0x24C70A, 28, 0x00292315, 0x0800FEFF, 0x55553131},
	{(0x05 << 24) | 0x24C70A, 29, 0x00292315, 0x0800FEFF, 0x5D5D3636},

	
	{(0x05 << 24) | 0x24C74A, 30, 0x00292315, 0x0800FEFF, 0x5F5F3737},
	{(0x05 << 24) | 0x24C74A, 31, 0x00262114, 0x0700FEFF, 0x65653B3B},

	
	{(0x05 << 24) | 0x24C78A, 32, 0x00292315, 0x0800FEFF, 0x66663B3B},
	{(0x05 << 24) | 0x24C78A, 33, 0x00262114, 0x0700FEFF, 0x70704141},

	
	{(0x05 << 24) | 0x24C7CA, 34, 0x00292315, 0x0800FEFF, 0x72724242}
};




void Uxx_ReadEthernetAddress(struct hw_data *pHwData)
{
	u32	ltmp;

	Wb35Reg_WriteSync(pHwData, 0x03b4, 0x08000000); 
	Wb35Reg_ReadSync(pHwData, 0x03b4, &ltmp);
	*(u16 *)pHwData->PermanentMacAddress = cpu_to_le16((u16) ltmp);
	Wb35Reg_WriteSync(pHwData, 0x03b4, 0x08010000); 
	Wb35Reg_ReadSync(pHwData, 0x03b4, &ltmp);
	*(u16 *)(pHwData->PermanentMacAddress + 2) = cpu_to_le16((u16) ltmp);
	Wb35Reg_WriteSync(pHwData, 0x03b4, 0x08020000); 
	Wb35Reg_ReadSync(pHwData, 0x03b4, &ltmp);
	*(u16 *)(pHwData->PermanentMacAddress + 4) = cpu_to_le16((u16) ltmp);
	*(u16 *)(pHwData->PermanentMacAddress + 6) = 0;
	Wb35Reg_WriteSync(pHwData, 0x03e8, cpu_to_le32(*(u32 *)pHwData->PermanentMacAddress));
	Wb35Reg_WriteSync(pHwData, 0x03ec, cpu_to_le32(*(u32 *)(pHwData->PermanentMacAddress + 4)));
}


void CardGetMulticastBit(u8 Address[ETH_ALEN], u8 *Byte, u8 *Value)
{
	u32 Crc;
	u32 BitNumber;

	
	Crc = CardComputeCrc(Address, ETH_ALEN);

	
	
	BitNumber = (u32) ((Crc >> 26) & 0x3f);

	*Byte  = (u8) (BitNumber >> 3); 
	*Value = (u8) ((u8) 1 << (BitNumber % 8));
}

void Uxx_power_on_procedure(struct hw_data *pHwData)
{
	u32	ltmp, loop;

	if (pHwData->phy_type <= RF_MAXIM_V1)
		Wb35Reg_WriteSync(pHwData, 0x03d4, 0xffffff38);
	else {
		Wb35Reg_WriteSync(pHwData, 0x03f4, 0xFF5807FF);
		Wb35Reg_WriteSync(pHwData, 0x03d4, 0x80); 
		msleep(10);
		Wb35Reg_WriteSync(pHwData, 0x03d4, 0xb8); 
		msleep(10);
		ltmp = 0x4968;
		if ((pHwData->phy_type == RF_WB_242) ||
			(RF_WB_242_1 == pHwData->phy_type))
			ltmp = 0x4468;

		Wb35Reg_WriteSync(pHwData, 0x03d0, ltmp);
		Wb35Reg_WriteSync(pHwData, 0x03d4, 0xa0); 

		msleep(20);
		Wb35Reg_ReadSync(pHwData, 0x03d0, &ltmp);
		loop = 500; 
		while (!(ltmp & 0x20) && loop--) {
			msleep(10);
			if (!Wb35Reg_ReadSync(pHwData, 0x03d0, &ltmp))
				break;
		}

		Wb35Reg_WriteSync(pHwData, 0x03d4, 0xe0); 
	}

	Wb35Reg_WriteSync(pHwData, 0x03b0, 1); 
	msleep(10);

	
	Wb35Reg_WriteSync(pHwData, 0x03f8, 0x7ff);
}

void Set_ChanIndep_RfData_al7230_24(struct hw_data *pHwData, u32 *pltmp , char number)
{
	u8	i;

	for (i = 0; i < number; i++) {
		pHwData->phy_para[i] = al7230_rf_data_24[i];
		pltmp[i] = (1 << 31) | (0 << 30) | (24 << 24) | (al7230_rf_data_24[i] & 0xffffff);
	}
}

void Set_ChanIndep_RfData_al7230_50(struct hw_data *pHwData, u32 *pltmp, char number)
{
	u8	i;

	for (i = 0; i < number; i++) {
		pHwData->phy_para[i] = al7230_rf_data_50[i];
		pltmp[i] = (1 << 31) | (0 << 30) | (24 << 24) | (al7230_rf_data_50[i] & 0xffffff);
	}
}


void RFSynthesizer_initial(struct hw_data *pHwData)
{
	u32	altmp[32];
	u32	*pltmp = altmp;
	u32	ltmp;
	u8	number = 0x00; 
	u8	i;

	switch (pHwData->phy_type) {
	case RF_MAXIM_2825:
	case RF_MAXIM_V1: 
		number = ARRAY_SIZE(max2825_rf_data);
		for (i = 0; i < number; i++) {
			pHwData->phy_para[i] = max2825_rf_data[i]; 
			pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2825_rf_data[i], 18);
		}
		break;
	case RF_MAXIM_2827:
		number = ARRAY_SIZE(max2827_rf_data);
		for (i = 0; i < number; i++) {
			pHwData->phy_para[i] = max2827_rf_data[i];
			pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2827_rf_data[i], 18);
		}
		break;
	case RF_MAXIM_2828:
		number = ARRAY_SIZE(max2828_rf_data);
		for (i = 0; i < number; i++) {
			pHwData->phy_para[i] = max2828_rf_data[i];
			pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2828_rf_data[i], 18);
		}
		break;
	case RF_MAXIM_2829:
		number = ARRAY_SIZE(max2829_rf_data);
		for (i = 0; i < number; i++) {
			pHwData->phy_para[i] = max2829_rf_data[i];
			pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2829_rf_data[i], 18);
		}
		break;
	case RF_AIROHA_2230:
		number = ARRAY_SIZE(al2230_rf_data);
		for (i = 0; i < number; i++) {
			pHwData->phy_para[i] = al2230_rf_data[i];
			pltmp[i] = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse(al2230_rf_data[i], 20);
		}
		break;
	case RF_AIROHA_2230S:
		number = ARRAY_SIZE(al2230s_rf_data);
		for (i = 0; i < number; i++) {
			pHwData->phy_para[i] = al2230s_rf_data[i];
			pltmp[i] = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse(al2230s_rf_data[i], 20);
		}
		break;
	case RF_AIROHA_7230:
		
		Wb35Reg_WriteSync(pHwData, 0x03dc, 0x00000000);
		pr_debug("* PLL_ON    low\n");
		number = ARRAY_SIZE(al7230_rf_data_24);
		Set_ChanIndep_RfData_al7230_24(pHwData, pltmp, number);
		break;
	case RF_WB_242:
	case RF_WB_242_1:
		number = ARRAY_SIZE(w89rf242_rf_data);
		for (i = 0; i < number; i++) {
			ltmp = w89rf242_rf_data[i];
			if (i == 4) { 
				ltmp &= ~0xff0; 
				ltmp |= pHwData->VCO_trim << 4;
			}

			pHwData->phy_para[i] = ltmp;
			pltmp[i] = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse(ltmp, 24);
		}
		break;
	}

	pHwData->phy_number = number;

	
	if (number > 12) {
		for (i = 0; i < 12; i++) 
			Wb35Reg_WriteSync(pHwData, 0x0864, pltmp[i]);

		pltmp += 12;
		number -= 12;
	}

	
	for (i = 0; i < number; i++)
		Wb35Reg_WriteSync(pHwData, 0x864, pltmp[i]);

	
	if (pHwData->CalOneTime)
		return;
	pHwData->CalOneTime = 1;

	switch (pHwData->phy_type) {
	case RF_AIROHA_2230:
		ltmp = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse((0x07 << 20) | 0xE168E, 20);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(10);
		ltmp = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse(al2230_rf_data[7], 20);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(10);
	case RF_AIROHA_2230S:
		Wb35Reg_WriteSync(pHwData, 0x03d4, 0x80); 
		msleep(10);
		Wb35Reg_WriteSync(pHwData, 0x03d4, 0xa0); 
		msleep(10);
		Wb35Reg_WriteSync(pHwData, 0x03d4, 0xe0); 
		Wb35Reg_WriteSync(pHwData, 0x03b0, 1); 
		msleep(10);
		

		
		ltmp = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse((0x0F<<20) | 0xF01A0, 20);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);

		ltmp = pHwData->reg.BB5C & 0xfffff000;
		Wb35Reg_WriteSync(pHwData, 0x105c, ltmp);
		pHwData->reg.BB50 |= 0x13; 
		Wb35Reg_WriteSync(pHwData, 0x1050, pHwData->reg.BB50);
		msleep(5);

		ltmp = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse((0x0F << 20) | 0xF01B0, 20);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);

		ltmp = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse((0x0F << 20) | 0xF01E0, 20);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);

		ltmp = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse((0x0F << 20) | 0xF01A0, 20);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp) ;

		Wb35Reg_WriteSync(pHwData, 0x105c, pHwData->reg.BB5C);
		pHwData->reg.BB50 &= ~0x13; 
		Wb35Reg_WriteSync(pHwData, 0x1050, pHwData->reg.BB50);
		break;
	case RF_AIROHA_7230:
		
		Wb35Reg_WriteSync(pHwData, 0x03dc, 0x00000080);
		pr_debug("* PLL_ON    high\n");

		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | 0x9ABA8F;
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | 0x3ABA8F;
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | 0x1ABA8F;
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);

		
		Wb35Reg_WriteSync(pHwData, 0x03dc, 0x00000000);
		pr_debug("* PLL_ON    low\n");

		number = ARRAY_SIZE(al7230_rf_data_50);
		Set_ChanIndep_RfData_al7230_50(pHwData, pltmp, number);
		
		for (i = 0; i < number; i++)
			Wb35Reg_WriteSync(pHwData, 0x0864, pltmp[i]);
		msleep(5);

		Wb35Reg_WriteSync(pHwData, 0x03dc, 0x00000080);
		pr_debug("* PLL_ON    high\n");

		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | 0x9ABA8F;
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | 0x3ABA8F;
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | 0x12BACF;
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);
		break;
	case RF_WB_242:
	case RF_WB_242_1:
		 
		ltmp = pHwData->reg.BB5C & 0xfffff000;
		Wb35Reg_WriteSync(pHwData, 0x105c, ltmp);
		Wb35Reg_WriteSync(pHwData, 0x1058, 0);
		pHwData->reg.BB50 |= 0x3; 
		Wb35Reg_WriteSync(pHwData, 0x1050, pHwData->reg.BB50);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x0F<<24) | 0x00101E, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFE69c0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(2);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xF8EBC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x07<<24) | 0x0C68CE, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x0F<<24) | 0x00201E, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFCEBC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		udelay(150); 
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xF8EBC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFAEDC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x07<<24) | 0x0C68CE, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x0F<<24) | 0x00401E, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFEEDC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		udelay(150); 
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFAEDC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFD6BC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		udelay(150); 

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFAEDC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x01<<24) | 0xEBFFC2, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x06<<24) | 0x343FCC, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFF6DC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(2);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFAEDC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x06<<24) | 0x342FCC, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFF6DC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(2);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFAEDC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x06<<24) | 0x341FCC, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFF6DC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(2);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFAEDC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x06<<24) | 0x340FCC, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFF6DC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(2);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xFAEDC0, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x01<<24) | 0xEFFFC2, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);

		
		
		ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse((0x00<<24) | 0xF86100, 24);
		Wb35Reg_WriteSync(pHwData, 0x0864, ltmp);
		msleep(5);
		break;
	}
}

void BBProcessor_AL7230_2400(struct hw_data *pHwData)
{
	struct wb35_reg *reg = &pHwData->reg;
	u32	pltmp[12];

	pltmp[0] = 0x16A8337A; 
	pltmp[1] = 0x9AFF9AA6; 
	pltmp[2] = 0x55D00A04; 
	pltmp[3] = 0xFFF72031; 
	reg->BB0C = 0xFFF72031;
	pltmp[4] = 0x0FacDCC5; 
	pltmp[5] = 0x00CAA333; 
	pltmp[6] = 0xF2211111; 
	pltmp[7] = 0x0FA3F0ED; 
	pltmp[8] = 0x06443440; 
	pltmp[9] = 0xA8002A79; 
	pltmp[10] = 0x40000528;
	pltmp[11] = 0x232D7F30; 
	reg->BB2C = 0x232D7F30;
	Wb35Reg_BurstWrite(pHwData, 0x1000, pltmp, 12, AUTO_INCREMENT);

	pltmp[0] = 0x00002c54; 
	reg->BB30 = 0x00002c54;
	pltmp[1] = 0x00C0D6C5; 
	pltmp[2] = 0x5B2C8769; 
	pltmp[3] = 0x00000000; 
	reg->BB3C = 0x00000000;
	pltmp[4] = 0x00003F29; 
	pltmp[5] = 0x0EFEFBFE; 
	pltmp[6] = 0x00332C1B; 
	pltmp[7] = 0x0A00FEFF; 
	pltmp[8] = 0x2B106208; 
	reg->BB50 = 0x2B106208;
	pltmp[9] = 0;		
	reg->BB54 = 0x00000000;
	pltmp[10] = 0x52524242; 
	reg->BB58 = 0x52524242;
	pltmp[11] = 0xAA0AC000; 
	Wb35Reg_BurstWrite(pHwData, 0x1030, pltmp, 12, AUTO_INCREMENT);
}

void BBProcessor_AL7230_5000(struct hw_data *pHwData)
{
	struct wb35_reg *reg = &pHwData->reg;
	u32	pltmp[12];

	pltmp[0] = 0x16AA6678; 
	pltmp[1] = 0x9AFFA0B2; 
	pltmp[2] = 0x55D00A04; 
	pltmp[3] = 0xEFFF233E; 
	reg->BB0C = 0xEFFF233E;
	pltmp[4] = 0x0FacDCC5; 
	pltmp[5] = 0x00CAA333; 
	pltmp[6] = 0xF2432111; 
	pltmp[7] = 0x0FA3F0ED; 
	pltmp[8] = 0x05C43440; 
	pltmp[9] = 0x00002A79; 
	pltmp[10] = 0x40000528;
	pltmp[11] = 0x232FDF30;
	reg->BB2C = 0x232FDF30;
	Wb35Reg_BurstWrite(pHwData, 0x1000, pltmp, 12, AUTO_INCREMENT);

	pltmp[0] = 0x80002C7C; 
	pltmp[1] = 0x00C0D6C5; 
	pltmp[2] = 0x5B2C8769; 
	pltmp[3] = 0x00000000; 
	reg->BB3C = 0x00000000;
	pltmp[4] = 0x00003F29; 
	pltmp[5] = 0x0EFEFBFE; 
	pltmp[6] = 0x00332C1B; 
	pltmp[7] = 0x0A00FEFF; 
	pltmp[8] = 0x2B107208; 
	reg->BB50 = 0x2B107208;
	pltmp[9] = 0;		
	reg->BB54 = 0x00000000;
	pltmp[10] = 0x52524242; 
	reg->BB58 = 0x52524242;
	pltmp[11] = 0xAA0AC000; 
	Wb35Reg_BurstWrite(pHwData, 0x1030, pltmp, 12, AUTO_INCREMENT);
}

void BBProcessor_initial(struct hw_data *pHwData)
{
	struct wb35_reg *reg = &pHwData->reg;
	u32	i, pltmp[12];

	switch (pHwData->phy_type) {
	case RF_MAXIM_V1: 
		pltmp[0] = 0x16F47E77; 
		pltmp[1] = 0x9AFFAEA4; 
		pltmp[2] = 0x55D00A04; 
		pltmp[3] = 0xEFFF1A34; 
		reg->BB0C = 0xEFFF1A34;
		pltmp[4] = 0x0FABE0B7; 
		pltmp[5] = 0x00CAA332; 
		pltmp[6] = 0xF6632111; 
		pltmp[7] = 0x0FA3F0ED; 
		pltmp[8] = 0x04CC3640; 
		pltmp[9] = 0x00002A79; 
		pltmp[10] = (pHwData->phy_type == 3) ? 0x40000a28 : 0x40000228; 
		pltmp[11] = 0x232FDF30; 
		reg->BB2C = 0x232FDF30; 
		Wb35Reg_BurstWrite(pHwData, 0x1000, pltmp, 12, AUTO_INCREMENT);

		pltmp[0] = 0x00002C54; 
		reg->BB30 = 0x00002C54;
		pltmp[1] = 0x00C0D6C5; 
		pltmp[2] = 0x5B6C8769; 
		pltmp[3] = 0x00000000; 
		reg->BB3C = 0x00000000;
		pltmp[4] = 0x00003F29; 
		pltmp[5] = 0x0EFEFBFE; 
		pltmp[6] = 0x00453B24; 
		pltmp[7] = 0x0E00FEFF; 
		pltmp[8] = 0x27106208; 
		reg->BB50 = 0x27106208;
		pltmp[9] = 0;		
		reg->BB54 = 0x00000000;
		pltmp[10] = 0x64646464; 
		reg->BB58 = 0x64646464;
		pltmp[11] = 0xAA0AC000; 
		Wb35Reg_BurstWrite(pHwData, 0x1030, pltmp, 12, AUTO_INCREMENT);

		Wb35Reg_Write(pHwData, 0x1070, 0x00000045);
		break;

	case RF_MAXIM_2825:
	case RF_MAXIM_2827:
	case RF_MAXIM_2828:
		pltmp[0] = 0x16b47e77; 
		pltmp[1] = 0x9affaea4; 
		pltmp[2] = 0x55d00a04; 
		pltmp[3] = 0xefff1a34; 
		reg->BB0C = 0xefff1a34;
		pltmp[4] = 0x0fabe0b7; 
		pltmp[5] = 0x00caa332; 
		pltmp[6] = 0xf6632111; 
		pltmp[7] = 0x0FA3F0ED; 
		pltmp[8] = 0x04CC3640; 
		pltmp[9] = 0x00002A79; 
		pltmp[10] = 0x40000528;
		pltmp[11] = 0x232fdf30; 
		reg->BB2C = 0x232fdf30; 
		Wb35Reg_BurstWrite(pHwData, 0x1000, pltmp, 12, AUTO_INCREMENT);

		pltmp[0] = 0x00002C54; 
		reg->BB30 = 0x00002C54;
		pltmp[1] = 0x00C0D6C5; 
		pltmp[2] = 0x5B6C8769; 
		pltmp[3] = 0x00000000; 
		reg->BB3C = 0x00000000;
		pltmp[4] = 0x00003F29; 
		pltmp[5] = 0x0EFEFBFE; 
		pltmp[6] = 0x00453B24; 
		pltmp[7] = 0x0D00FDFF; 
		pltmp[8] = 0x27106208; 
		reg->BB50 = 0x27106208;
		pltmp[9] = 0;		
		reg->BB54 = 0x00000000;
		pltmp[10] = 0x64646464; 
		reg->BB58 = 0x64646464;
		pltmp[11] = 0xAA28C000; 
		Wb35Reg_BurstWrite(pHwData, 0x1030, pltmp, 12, AUTO_INCREMENT);

		Wb35Reg_Write(pHwData, 0x1070, 0x00000045);
		break;

	case RF_MAXIM_2829:
		pltmp[0] = 0x16b47e77; 
		pltmp[1] = 0x9affaea4; 
		pltmp[2] = 0x55d00a04; 
		pltmp[3] = 0xf4ff1632; 
		reg->BB0C = 0xf4ff1632;
		pltmp[4] = 0x0fabe0b7; 
		pltmp[5] = 0x00caa332; 
		pltmp[6] = 0xf8632112; 
		pltmp[7] = 0x0FA3F0ED; 
		pltmp[8] = 0x04CC3640; 
		pltmp[9] = 0x00002A79; 
		pltmp[10] = 0x40000528;
		pltmp[11] = 0x232fdf30; 
		reg->BB2C = 0x232fdf30; 
		Wb35Reg_BurstWrite(pHwData, 0x1000, pltmp, 12, AUTO_INCREMENT);

		pltmp[0] = 0x00002C54; 
		reg->BB30 = 0x00002C54;
		pltmp[1] = 0x00C0D6C5; 
		pltmp[2] = 0x5b2c8769; 
		pltmp[3] = 0x00000000; 
		reg->BB3C = 0x00000000;
		pltmp[4] = 0x00003F29; 
		pltmp[5] = 0x0EFEFBFE; 
		pltmp[6] = 0x002c2617; 
		pltmp[7] = 0x0800feff; 
		pltmp[8] = 0x27106208; 
		reg->BB50 = 0x27106208;
		pltmp[9] = 0;		
		reg->BB54 = 0x00000000;
		pltmp[10] = 0x64644a4a; 
		reg->BB58 = 0x64646464;
		pltmp[11] = 0xAA28C000; 
		Wb35Reg_BurstWrite(pHwData, 0x1030, pltmp, 12, AUTO_INCREMENT);
		Wb35Reg_Write(pHwData, 0x1070, 0x00000045);
		break;
	case RF_AIROHA_2230:
		pltmp[0] = 0X16764A77; 
		pltmp[1] = 0x9affafb2; 
		pltmp[2] = 0x55d00a04; 
		pltmp[3] = 0xFFFd203c; 
		reg->BB0C = 0xFFFd203c;
		pltmp[4] = 0X0FBFDCc5; 
		pltmp[5] = 0x00caa332; 
		pltmp[6] = 0XF6632111; 
		pltmp[7] = 0x0FA3F0ED; 
		pltmp[8] = 0x04C43640; 
		pltmp[9] = 0x00002A79; 
		pltmp[10] = 0X40000528;
		pltmp[11] = 0x232dfF30; 
		reg->BB2C = 0x232dfF30; 
		Wb35Reg_BurstWrite(pHwData, 0x1000, pltmp, 12, AUTO_INCREMENT);

		pltmp[0] = 0x00002C54; 
		reg->BB30 = 0x00002C54;
		pltmp[1] = 0x00C0D6C5; 
		pltmp[2] = 0x5B2C8769; 
		pltmp[3] = 0x00000000; 
		reg->BB3C = 0x00000000;
		pltmp[4] = 0x00003F29; 
		pltmp[5] = 0x0EFEFBFE; 
		pltmp[6] = BB48_DEFAULT_AL2230_11G; 
		reg->BB48 = BB48_DEFAULT_AL2230_11G; 
		pltmp[7] = BB4C_DEFAULT_AL2230_11G; 
		reg->BB4C = BB4C_DEFAULT_AL2230_11G;
		pltmp[8] = 0x27106200; 
		reg->BB50 = 0x27106200;
		pltmp[9] = 0;		
		reg->BB54 = 0x00000000;
		pltmp[10] = 0x52524242; 
		reg->BB58 = 0x52524242;
		pltmp[11] = 0xAA0AC000; 
		Wb35Reg_BurstWrite(pHwData, 0x1030, pltmp, 12, AUTO_INCREMENT);

		Wb35Reg_Write(pHwData, 0x1070, 0x00000045);
		break;
	case RF_AIROHA_2230S:
		pltmp[0] = 0X16764A77; 
		pltmp[1] = 0x9affafb2; 
		pltmp[2] = 0x55d00a04; 
		pltmp[3] = 0xFFFd203c; 
		reg->BB0C = 0xFFFd203c;
		pltmp[4] = 0X0FBFDCc5; 
		pltmp[5] = 0x00caa332; 
		pltmp[6] = 0XF6632111; 
		pltmp[7] = 0x0FA3F0ED; 
		pltmp[8] = 0x04C43640; 
		pltmp[9] = 0x00002A79; 
		pltmp[10] = 0X40000528;
		pltmp[11] = 0x232dfF30; 
		reg->BB2C = 0x232dfF30; 
		Wb35Reg_BurstWrite(pHwData, 0x1000, pltmp, 12, AUTO_INCREMENT);

		pltmp[0] = 0x00002C54; 
		reg->BB30 = 0x00002C54;
		pltmp[1] = 0x00C0D6C5; 
		pltmp[2] = 0x5B2C8769; 
		pltmp[3] = 0x00000000; 
		reg->BB3C = 0x00000000;
		pltmp[4] = 0x00003F29; 
		pltmp[5] = 0x0EFEFBFE; 
		pltmp[6] = BB48_DEFAULT_AL2230_11G; 
		reg->BB48 = BB48_DEFAULT_AL2230_11G; 
		pltmp[7] = BB4C_DEFAULT_AL2230_11G; 
		reg->BB4C = BB4C_DEFAULT_AL2230_11G;
		pltmp[8] = 0x27106200; 
		reg->BB50 = 0x27106200;
		pltmp[9] = 0;		
		reg->BB54 = 0x00000000;
		pltmp[10] = 0x52523232; 
		reg->BB58 = 0x52523232;
		pltmp[11] = 0xAA0AC000; 
		Wb35Reg_BurstWrite(pHwData, 0x1030, pltmp, 12, AUTO_INCREMENT);

		Wb35Reg_Write(pHwData, 0x1070, 0x00000045);
		break;
	case RF_AIROHA_7230:
		BBProcessor_AL7230_2400(pHwData);

		Wb35Reg_Write(pHwData, 0x1070, 0x00000045);
		break;
	case RF_WB_242:
	case RF_WB_242_1:
		pltmp[0] = 0x16A8525D; 
		pltmp[1] = 0x9AFF9ABA; 
		pltmp[2] = 0x55D00A04; 
		pltmp[3] = 0xEEE91C32; 
		reg->BB0C = 0xEEE91C32;
		pltmp[4] = 0x0FACDCC5; 
		pltmp[5] = 0x000AA344; 
		pltmp[6] = 0x22222221; 
		pltmp[7] = 0x0FA3F0ED; 
		pltmp[8] = 0x04CC3440; 
		pltmp[9] = 0xA9002A79; 
		pltmp[10] = 0x40000528; 
		pltmp[11] = 0x23457F30; 
		reg->BB2C = 0x23457F30;
		Wb35Reg_BurstWrite(pHwData, 0x1000, pltmp, 12, AUTO_INCREMENT);

		pltmp[0] = 0x00002C54; 
		reg->BB30 = 0x00002C54;
		pltmp[1] = 0x00C0D6C5; 
		pltmp[2] = 0x5B2C8769; 
		pltmp[3] = pHwData->BB3c_cal; 
		reg->BB3C = pHwData->BB3c_cal;
		pltmp[4] = 0x00003F29; 
		pltmp[5] = 0x0EFEFBFE; 
		pltmp[6] = BB48_DEFAULT_WB242_11G; 
		reg->BB48 = BB48_DEFAULT_WB242_11G;
		pltmp[7] = BB4C_DEFAULT_WB242_11G; 
		reg->BB4C = BB4C_DEFAULT_WB242_11G;
		pltmp[8] = 0x27106208; 
		reg->BB50 = 0x27106208;
		pltmp[9] = pHwData->BB54_cal; 
		reg->BB54 = pHwData->BB54_cal;
		pltmp[10] = 0x52523131; 
		reg->BB58 = 0x52523131;
		pltmp[11] = 0xAA0AC000; 
		Wb35Reg_BurstWrite(pHwData, 0x1030, pltmp, 12, AUTO_INCREMENT);

		Wb35Reg_Write(pHwData, 0x1070, 0x00000045);
		break;
	}

	
	reg->LNAValue[0] = (u8) (reg->BB0C & 0xff);
	reg->LNAValue[1] = 0;
	reg->LNAValue[2] = (u8) ((reg->BB0C & 0xff00) >> 8);
	reg->LNAValue[3] = 0;

	
	for (i = 0; i < MAX_SQ3_FILTER_SIZE; i++)
		reg->SQ3_filter[i] = 0x2f; 
}

void set_tx_power_per_channel_max2829(struct hw_data *pHwData,  struct chan_info Channel)
{
	RFSynthesizer_SetPowerIndex(pHwData, 100);
}

void set_tx_power_per_channel_al2230(struct hw_data *pHwData,  struct chan_info Channel)
{
	u8	index = 100;

	if (pHwData->TxVgaFor24[Channel.ChanNo - 1] != 0xff)
		index = pHwData->TxVgaFor24[Channel.ChanNo - 1];

	RFSynthesizer_SetPowerIndex(pHwData, index);
}

void set_tx_power_per_channel_al7230(struct hw_data *pHwData,  struct chan_info Channel)
{
	u8	i, index = 100;

	switch (Channel.band) {
	case BAND_TYPE_DSSS:
	case BAND_TYPE_OFDM_24:
		if (pHwData->TxVgaFor24[Channel.ChanNo - 1] != 0xff)
			index = pHwData->TxVgaFor24[Channel.ChanNo - 1];
		break;
	case BAND_TYPE_OFDM_5:
		for (i = 0; i < 35; i++) {
			if (Channel.ChanNo == pHwData->TxVgaFor50[i].ChanNo) {
				if (pHwData->TxVgaFor50[i].TxVgaValue != 0xff)
					index = pHwData->TxVgaFor50[i].TxVgaValue;
				break;
			}
		}
		break;
	}
	RFSynthesizer_SetPowerIndex(pHwData, index);
}

void set_tx_power_per_channel_wb242(struct hw_data *pHwData,  struct chan_info Channel)
{
	u8	index = 100;

	switch (Channel.band) {
	case BAND_TYPE_DSSS:
	case BAND_TYPE_OFDM_24:
		if (pHwData->TxVgaFor24[Channel.ChanNo - 1] != 0xff)
			index = pHwData->TxVgaFor24[Channel.ChanNo - 1];
		break;
	case BAND_TYPE_OFDM_5:
		break;
	}
	RFSynthesizer_SetPowerIndex(pHwData, index);
}

void RFSynthesizer_SwitchingChannel(struct hw_data *pHwData,  struct chan_info Channel)
{
	struct wb35_reg *reg = &pHwData->reg;
	u32	pltmp[16]; 
	u32	count, ltmp;
	u8	i, j, number;
	u8	ChnlTmp;

	switch (pHwData->phy_type) {
	case RF_MAXIM_2825:
	case RF_MAXIM_V1: 

		if (Channel.band <= BAND_TYPE_OFDM_24) { 
			for (i = 0; i < 3; i++)
				pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2825_channel_data_24[Channel.ChanNo-1][i], 18);
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 3, NO_INCREMENT);
		}
		RFSynthesizer_SetPowerIndex(pHwData, 100);
		break;
	case RF_MAXIM_2827:
		if (Channel.band <= BAND_TYPE_OFDM_24) { 
			for (i = 0; i < 3; i++)
				pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2827_channel_data_24[Channel.ChanNo-1][i], 18);
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 3, NO_INCREMENT);
		} else if (Channel.band == BAND_TYPE_OFDM_5) { 
			ChnlTmp = (Channel.ChanNo - 36) / 4;
			for (i = 0; i < 3; i++)
				pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2827_channel_data_50[ChnlTmp][i], 18);
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 3, NO_INCREMENT);
		}
		RFSynthesizer_SetPowerIndex(pHwData, 100);
		break;
	case RF_MAXIM_2828:
		if (Channel.band <= BAND_TYPE_OFDM_24) { 
			for (i = 0; i < 3; i++)
				pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2828_channel_data_24[Channel.ChanNo-1][i], 18);
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 3, NO_INCREMENT);
		} else if (Channel.band == BAND_TYPE_OFDM_5) { 
			ChnlTmp = (Channel.ChanNo - 36) / 4;
			for (i = 0; i < 3; i++)
				pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2828_channel_data_50[ChnlTmp][i], 18);
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 3, NO_INCREMENT);
		}
		RFSynthesizer_SetPowerIndex(pHwData, 100);
		break;
	case RF_MAXIM_2829:
		if (Channel.band <= BAND_TYPE_OFDM_24) {
			for (i = 0; i < 3; i++)
				pltmp[i] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2829_channel_data_24[Channel.ChanNo-1][i], 18);
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 3, NO_INCREMENT);
		} else if (Channel.band == BAND_TYPE_OFDM_5) {
			count = ARRAY_SIZE(max2829_channel_data_50);

			for (i = 0; i < count; i++) {
				if (max2829_channel_data_50[i][0] == Channel.ChanNo) {
					for (j = 0; j < 3; j++)
						pltmp[j] = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2829_channel_data_50[i][j+1], 18);
					Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 3, NO_INCREMENT);

					if ((max2829_channel_data_50[i][3] & 0x3FFFF) == 0x2A946) {
						ltmp = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse((5 << 18) | 0x2A906, 18);
						Wb35Reg_Write(pHwData, 0x0864, ltmp);
					} else { 
						ltmp = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse((5 << 18) | 0x2A986, 18);
						Wb35Reg_Write(pHwData, 0x0864, ltmp);
					}
				}
			}
		}
		set_tx_power_per_channel_max2829(pHwData, Channel);
		break;
	case RF_AIROHA_2230:
	case RF_AIROHA_2230S:
		if (Channel.band <= BAND_TYPE_OFDM_24) { 
			for (i = 0; i < 2; i++)
				pltmp[i] = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse(al2230_channel_data_24[Channel.ChanNo-1][i], 20);
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 2, NO_INCREMENT);
		}
		set_tx_power_per_channel_al2230(pHwData, Channel);
		break;
	case RF_AIROHA_7230:
		
		if (Channel.band != pHwData->band) {
			if (Channel.band <= BAND_TYPE_OFDM_24) {
				
				BBProcessor_AL7230_2400(pHwData);

				number = ARRAY_SIZE(al7230_rf_data_24);
				Set_ChanIndep_RfData_al7230_24(pHwData, pltmp, number);
			} else {
				
				BBProcessor_AL7230_5000(pHwData);

				number = ARRAY_SIZE(al7230_rf_data_50);
				Set_ChanIndep_RfData_al7230_50(pHwData, pltmp, number);
			}

			
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, number, NO_INCREMENT);
			pr_debug("Band changed\n");
		}

		if (Channel.band <= BAND_TYPE_OFDM_24) { 
			for (i = 0; i < 2; i++)
				pltmp[i] = (1 << 31) | (0 << 30) | (24 << 24) | (al7230_channel_data_24[Channel.ChanNo-1][i]&0xffffff);
			Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 2, NO_INCREMENT);
		} else if (Channel.band == BAND_TYPE_OFDM_5) {
			
			if ((Channel.ChanNo > 64) && (Channel.ChanNo <= 165)) {
				ltmp = (1 << 31) | (0 << 30) | (24 << 24) | 0x00143c;
				Wb35Reg_Write(pHwData, 0x0864, ltmp);
			} else	{ 
				ltmp = (1 << 31) | (0 << 30) | (24 << 24) | 0x00147c;
				Wb35Reg_Write(pHwData, 0x0864, ltmp);
			}

			count = ARRAY_SIZE(al7230_channel_data_5);

			for (i = 0; i < count; i++) {
				if (al7230_channel_data_5[i][0] == Channel.ChanNo) {
					for (j = 0; j < 3; j++)
						pltmp[j] = (1 << 31) | (0 << 30) | (24 << 24) | (al7230_channel_data_5[i][j+1] & 0xffffff);
					Wb35Reg_BurstWrite(pHwData, 0x0864, pltmp, 3, NO_INCREMENT);
				}
			}
		}
		set_tx_power_per_channel_al7230(pHwData, Channel);
		break;
	case RF_WB_242:
	case RF_WB_242_1:

		if (Channel.band <= BAND_TYPE_OFDM_24) { 
			ltmp = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse(w89rf242_channel_data_24[Channel.ChanNo-1][0], 24);
			Wb35Reg_Write(pHwData, 0x864, ltmp);
		}
		set_tx_power_per_channel_wb242(pHwData, Channel);
		break;
	}

	if (Channel.band <= BAND_TYPE_OFDM_24) {
		
		reg->BB50 &= ~(BIT(11) | BIT(12));
		Wb35Reg_Write(pHwData, 0x1050, reg->BB50); 
		
		reg->M78_ERPInformation &= ~BIT(5);
		Wb35Reg_Write(pHwData, 0x0878, reg->M78_ERPInformation);
		
		reg->BB30 &= ~BIT(31);
		Wb35Reg_Write(pHwData, 0x1030, reg->BB30);
	} else if (Channel.band == BAND_TYPE_OFDM_5) {
		
		reg->BB50 &= ~(BIT(11) | BIT(12));
		if (Channel.ChanNo <= 64)
			reg->BB50 |= BIT(12);				
		else if ((Channel.ChanNo >= 100) && (Channel.ChanNo <= 124))
			reg->BB50 |= BIT(11);				
		else if ((Channel.ChanNo >= 128) && (Channel.ChanNo <= 161))
			reg->BB50 |= (BIT(12) | BIT(11));	
		else	
			reg->BB50 |= BIT(12);
		Wb35Reg_Write(pHwData, 0x1050, reg->BB50); 

		
		
		if (pHwData->phy_type != RF_AIROHA_7230) {
			
			reg->M78_ERPInformation |= BIT(5);
			Wb35Reg_Write(pHwData, 0x0878, reg->M78_ERPInformation);

			
			reg->BB30 |= BIT(31);
			Wb35Reg_Write(pHwData, 0x1030, reg->BB30);
		}
	}
}

u8 RFSynthesizer_SetPowerIndex(struct hw_data *pHwData,  u8 PowerIndex)
{
	u32	Band = pHwData->band;
	u8	index = 0;

	if (pHwData->power_index == PowerIndex)
		return PowerIndex;

	if (RF_MAXIM_2825 == pHwData->phy_type) {
		
		index = RFSynthesizer_SetMaxim2825Power(pHwData, PowerIndex);
	} else if (RF_MAXIM_2827 == pHwData->phy_type) {
		if (Band <= BAND_TYPE_OFDM_24)    
			index = RFSynthesizer_SetMaxim2827_24Power(pHwData, PowerIndex);
		else 
			index = RFSynthesizer_SetMaxim2827_50Power(pHwData, PowerIndex);
	} else if (RF_MAXIM_2828 == pHwData->phy_type) {
		if (Band <= BAND_TYPE_OFDM_24) 
			index = RFSynthesizer_SetMaxim2828_24Power(pHwData, PowerIndex);
		else 
			index = RFSynthesizer_SetMaxim2828_50Power(pHwData, PowerIndex);
	} else if (RF_AIROHA_2230 == pHwData->phy_type) {
		
		index = RFSynthesizer_SetAiroha2230Power(pHwData, PowerIndex);
		index = (u8) al2230_txvga_data[index][1];
	} else if (RF_AIROHA_2230S == pHwData->phy_type) {
		
		index = RFSynthesizer_SetAiroha2230Power(pHwData, PowerIndex);
		index = (u8) al2230_txvga_data[index][1];
	} else if (RF_AIROHA_7230 == pHwData->phy_type) {
		
		index = RFSynthesizer_SetAiroha7230Power(pHwData, PowerIndex);
		index = (u8)al7230_txvga_data[index][1];
	} else if ((RF_WB_242 == pHwData->phy_type) ||
		 (RF_WB_242_1 == pHwData->phy_type)) {
		
		index = RFSynthesizer_SetWinbond242Power(pHwData, PowerIndex);
		index = (u8)w89rf242_txvga_data[index][1];
	}

	pHwData->power_index = index;  
	return index;
}

u8 RFSynthesizer_SetMaxim2828_24Power(struct hw_data *pHwData, u8 index)
{
	u32	PowerData;
	if (index > 1)
		index = 1;
	PowerData = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2828_power_data_24[index], 18);
	Wb35Reg_Write(pHwData, 0x0864, PowerData);
	return index;
}

u8 RFSynthesizer_SetMaxim2828_50Power(struct hw_data *pHwData, u8 index)
{
	u32	PowerData;
	if (index > 1)
		index = 1;
	PowerData = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2828_power_data_50[index], 18);
	Wb35Reg_Write(pHwData, 0x0864, PowerData);
	return index;
}

u8 RFSynthesizer_SetMaxim2827_24Power(struct hw_data *pHwData, u8 index)
{
	u32	PowerData;
	if (index > 1)
		index = 1;
	PowerData = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2827_power_data_24[index], 18);
	Wb35Reg_Write(pHwData, 0x0864, PowerData);
	return index;
}

u8 RFSynthesizer_SetMaxim2827_50Power(struct hw_data *pHwData, u8 index)
{
	u32	PowerData;
	if (index > 1)
		index = 1;
	PowerData = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2827_power_data_50[index], 18);
	Wb35Reg_Write(pHwData, 0x0864, PowerData);
	return index;
}

u8 RFSynthesizer_SetMaxim2825Power(struct hw_data *pHwData, u8 index)
{
	u32	PowerData;
	if (index > 1)
		index = 1;
	PowerData = (1 << 31) | (0 << 30) | (18 << 24) | BitReverse(max2825_power_data_24[index], 18);
	Wb35Reg_Write(pHwData, 0x0864, PowerData);
	return index;
}

u8 RFSynthesizer_SetAiroha2230Power(struct hw_data *pHwData, u8 index)
{
	u32	PowerData;
	u8	i, count;

	count = ARRAY_SIZE(al2230_txvga_data);
	for (i = 0; i < count; i++) {
		if (al2230_txvga_data[i][1] >= index)
			break;
	}
	if (i == count)
		i--;

	PowerData = (1 << 31) | (0 << 30) | (20 << 24) | BitReverse(al2230_txvga_data[i][0], 20);
	Wb35Reg_Write(pHwData, 0x0864, PowerData);
	return i;
}

u8 RFSynthesizer_SetAiroha7230Power(struct hw_data *pHwData, u8 index)
{
	u32	PowerData;
	u8	i, count;

	count = ARRAY_SIZE(al7230_txvga_data);
	for (i = 0; i < count; i++) {
		if (al7230_txvga_data[i][1] >= index)
			break;
	}
	if (i == count)
		i--;
	PowerData = (1 << 31) | (0 << 30) | (24 << 24) | (al7230_txvga_data[i][0] & 0xffffff);
	Wb35Reg_Write(pHwData, 0x0864, PowerData);
	return i;
}

u8 RFSynthesizer_SetWinbond242Power(struct hw_data *pHwData, u8 index)
{
	u32	PowerData;
	u8	i, count;

	count = ARRAY_SIZE(w89rf242_txvga_data);
	for (i = 0; i < count; i++) {
		if (w89rf242_txvga_data[i][1] >= index)
			break;
	}
	if (i == count)
		i--;

	
	PowerData = (1 << 31) | (0 << 30) | (24 << 24) | BitReverse(w89rf242_txvga_data[i][0], 24);
	Wb35Reg_Write(pHwData, 0x0864, PowerData);

	
	Wb35Reg_Write(pHwData, 0x1048, w89rf242_txvga_data[i][2]);
	Wb35Reg_Write(pHwData, 0x104c, w89rf242_txvga_data[i][3]);
	Wb35Reg_Write(pHwData, 0x1058, w89rf242_txvga_data[i][4]);

	return i;
}

void Dxx_initial(struct hw_data *pHwData)
{
	struct wb35_reg *reg = &pHwData->reg;

	reg->D00_DmaControl = 0xc0000004;	
						
	if (!HAL_USB_MODE_BURST(pHwData))
		reg->D00_DmaControl = 0xc0000000; 

	Wb35Reg_WriteSync(pHwData, 0x0400, reg->D00_DmaControl);
}

void Mxx_initial(struct hw_data *pHwData)
{
	struct wb35_reg *reg = &pHwData->reg;
	u32	tmp;
	u32	pltmp[11];
	u16	i;



	
	reg->M00_MacControl = 0x80000000; 

	
	reg->M24_MacControl = 0x08040042;
	pltmp[0] = reg->M24_MacControl;

	pltmp[1] = 0; 

	
	pHwData->cwmin = DEFAULT_CWMIN;
	pHwData->cwmax = DEFAULT_CWMAX;
	reg->M2C_MacControl = DEFAULT_CWMIN << 10;
	reg->M2C_MacControl |= DEFAULT_CWMAX;
	pltmp[2] = reg->M2C_MacControl;

	
	pltmp[3] = *(u32 *)pHwData->bssid;

	
	pHwData->AID = DEFAULT_AID;
	tmp = *(u16 *) (pHwData->bssid + 4);
	tmp |= DEFAULT_AID << 16;
	pltmp[4] = tmp;

	
	reg->M38_MacControl = (DEFAULT_RATE_RETRY_LIMIT << 8) | (DEFAULT_LONG_RETRY_LIMIT << 4) | DEFAULT_SHORT_RETRY_LIMIT;
	pltmp[5] = reg->M38_MacControl;

	
	tmp = (DEFAULT_PIFST << 26) | (DEFAULT_EIFST << 16) | (DEFAULT_DIFST << 8) | (DEFAULT_SIFST << 4) | DEFAULT_OSIFST ;
	reg->M3C_MacControl = tmp;
	pltmp[6] = tmp;

	
	pHwData->slot_time_select = DEFAULT_SLOT_TIME;
	tmp = (DEFAULT_ATIMWD << 16) | DEFAULT_SLOT_TIME;
	reg->M40_MacControl = tmp;
	pltmp[7] = tmp;

	
	tmp = DEFAULT_MAX_TX_MSDU_LIFE_TIME << 10; 
	reg->M44_MacControl = tmp;
	pltmp[8] = tmp;

	
	pHwData->BeaconPeriod = DEFAULT_BEACON_INTERVAL;
	pHwData->ProbeDelay = DEFAULT_PROBE_DELAY_TIME;
	tmp = (DEFAULT_BEACON_INTERVAL << 16) | DEFAULT_PROBE_DELAY_TIME;
	reg->M48_MacControl = tmp;
	pltmp[9] = tmp;

	
	reg->M4C_MacStatus = (DEFAULT_PROTOCOL_VERSION << 30) | (DEFAULT_MAC_POWER_STATE << 28) | (DEFAULT_DTIM_ALERT_TIME << 24);
	pltmp[10] = reg->M4C_MacStatus;

	for (i = 0; i < 11; i++)
		Wb35Reg_WriteSync(pHwData, 0x0824 + i * 4, pltmp[i]);

	
	Wb35Reg_WriteSync(pHwData, 0x0860, 0x12481248);
	reg->M60_MacControl = 0x12481248;

	
	Wb35Reg_WriteSync(pHwData, 0x0868, 0x00050900);
	reg->M68_MacControl = 0x00050900;

	
	Wb35Reg_WriteSync(pHwData, 0x0898, 0xffff8888);
	reg->M98_MacControl = 0xffff8888;
}


void Uxx_power_off_procedure(struct hw_data *pHwData)
{
	
	Wb35Reg_WriteSync(pHwData, 0x03b0, 3);
	Wb35Reg_WriteSync(pHwData, 0x03f0, 0xf9);
}

void GetTxVgaFromEEPROM(struct hw_data *pHwData)
{
	u32	i, j, ltmp;
	u16	Value[MAX_TXVGA_EEPROM];
	u8	*pctmp;
	u8	ctmp = 0;

	
	for (i = 0; i < MAX_TXVGA_EEPROM; i++) {
		Wb35Reg_WriteSync(pHwData, 0x03b4, 0x08100000 + 0x00010000 * i);
		Wb35Reg_ReadSync(pHwData, 0x03b4, &ltmp);
		Value[i] = (u16) (ltmp & 0xffff); 
		Value[i] = cpu_to_le16(Value[i]); 
	}

	
	pctmp = (u8 *) Value;
	for (i = 0; i < (MAX_TXVGA_EEPROM * 2); i++) {
		if (pctmp[i] != 0xff)
			ctmp = pctmp[i];
		else
			pctmp[i] = ctmp;
	}

	
	if (pHwData->phy_type == RF_WB_242) {
		for (i = 0; i < 4; i++) { 
			for (j = 0; j < ARRAY_SIZE(w89rf242_txvga_old_mapping); j++) {
				if (pctmp[i] < (u8) w89rf242_txvga_old_mapping[j][1]) {
					pctmp[i] = (u8) w89rf242_txvga_old_mapping[j][0];
					break;
				}
			}

			if (j == ARRAY_SIZE(w89rf242_txvga_old_mapping))
				pctmp[i] = (u8)w89rf242_txvga_old_mapping[j-1][0];
		}
	}

	memcpy(pHwData->TxVgaSettingInEEPROM, pctmp, MAX_TXVGA_EEPROM * 2); 
	EEPROMTxVgaAdjust(pHwData);
}

void EEPROMTxVgaAdjust(struct hw_data *pHwData)
{
	u8	*pTxVga = pHwData->TxVgaSettingInEEPROM;
	s16	i, stmp;

	
	
	stmp = pTxVga[1] - pTxVga[0];
	for (i = 0; i < 5; i++)
		pHwData->TxVgaFor24[i] = pTxVga[0] + stmp * i / 4;
	
	stmp = pTxVga[2] - pTxVga[1];
	for (i = 5; i < 10; i++)
		pHwData->TxVgaFor24[i] = pTxVga[1] + stmp * (i - 5) / 4;
	
	stmp = pTxVga[3] - pTxVga[2];
	for (i = 10; i < 13; i++)
		pHwData->TxVgaFor24[i] = pTxVga[2] + stmp * (i - 10) / 2;
	
	pHwData->TxVgaFor24[13] = pTxVga[3];

	
	if (pHwData->phy_type == RF_AIROHA_7230) {
		
		pHwData->TxVgaFor50[0].ChanNo = 184;
		pHwData->TxVgaFor50[0].TxVgaValue = pTxVga[4];
		
		pHwData->TxVgaFor50[3].ChanNo = 196;
		pHwData->TxVgaFor50[3].TxVgaValue = pTxVga[5];
		
		pHwData->TxVgaFor50[1].ChanNo = 188;
		pHwData->TxVgaFor50[2].ChanNo = 192;
		stmp = pTxVga[5] - pTxVga[4];
		pHwData->TxVgaFor50[2].TxVgaValue = pTxVga[5] - stmp / 3;
		pHwData->TxVgaFor50[1].TxVgaValue = pTxVga[5] - stmp * 2 / 3;

		
		pHwData->TxVgaFor50[6].ChanNo = 16;
		pHwData->TxVgaFor50[6].TxVgaValue = pTxVga[6];
		pHwData->TxVgaFor50[4].ChanNo = 8;
		pHwData->TxVgaFor50[4].TxVgaValue = pTxVga[6];
		pHwData->TxVgaFor50[5].ChanNo = 12;
		pHwData->TxVgaFor50[5].TxVgaValue = pTxVga[6];

		
		pHwData->TxVgaFor50[8].ChanNo = 36;
		pHwData->TxVgaFor50[8].TxVgaValue = pTxVga[7];
		pHwData->TxVgaFor50[7].ChanNo = 34;
		pHwData->TxVgaFor50[7].TxVgaValue = pTxVga[7];
		pHwData->TxVgaFor50[9].ChanNo = 38;
		pHwData->TxVgaFor50[9].TxVgaValue = pTxVga[7];

		
		pHwData->TxVgaFor50[10].ChanNo = 40;
		pHwData->TxVgaFor50[10].TxVgaValue = pTxVga[8];
		
		pHwData->TxVgaFor50[14].ChanNo = 48;
		pHwData->TxVgaFor50[14].TxVgaValue = pTxVga[9];
		
		pHwData->TxVgaFor50[11].ChanNo = 42;
		pHwData->TxVgaFor50[12].ChanNo = 44;
		pHwData->TxVgaFor50[13].ChanNo = 46;
		stmp = pTxVga[9] - pTxVga[8];
		pHwData->TxVgaFor50[13].TxVgaValue = pTxVga[9] - stmp / 4;
		pHwData->TxVgaFor50[12].TxVgaValue = pTxVga[9] - stmp * 2 / 4;
		pHwData->TxVgaFor50[11].TxVgaValue = pTxVga[9] - stmp * 3 / 4;

		
		pHwData->TxVgaFor50[15].ChanNo = 52;
		pHwData->TxVgaFor50[15].TxVgaValue = pTxVga[10];
		
		pHwData->TxVgaFor50[18].ChanNo = 64;
		pHwData->TxVgaFor50[18].TxVgaValue = pTxVga[11];
		
		pHwData->TxVgaFor50[16].ChanNo = 56;
		pHwData->TxVgaFor50[17].ChanNo = 60;
		stmp = pTxVga[11] - pTxVga[10];
		pHwData->TxVgaFor50[17].TxVgaValue = pTxVga[11] - stmp / 3;
		pHwData->TxVgaFor50[16].TxVgaValue = pTxVga[11] - stmp * 2 / 3;

		
		pHwData->TxVgaFor50[19].ChanNo = 100;
		pHwData->TxVgaFor50[19].TxVgaValue = pTxVga[12];
		
		pHwData->TxVgaFor50[22].ChanNo = 112;
		pHwData->TxVgaFor50[22].TxVgaValue = pTxVga[13];
		
		pHwData->TxVgaFor50[20].ChanNo = 104;
		pHwData->TxVgaFor50[21].ChanNo = 108;
		stmp = pTxVga[13] - pTxVga[12];
		pHwData->TxVgaFor50[21].TxVgaValue = pTxVga[13] - stmp / 3;
		pHwData->TxVgaFor50[20].TxVgaValue = pTxVga[13] - stmp * 2 / 3;

		
		pHwData->TxVgaFor50[26].ChanNo = 128;
		pHwData->TxVgaFor50[26].TxVgaValue = pTxVga[14];
		
		pHwData->TxVgaFor50[23].ChanNo = 116;
		pHwData->TxVgaFor50[24].ChanNo = 120;
		pHwData->TxVgaFor50[25].ChanNo = 124;
		stmp = pTxVga[14] - pTxVga[13];
		pHwData->TxVgaFor50[25].TxVgaValue = pTxVga[14] - stmp / 4;
		pHwData->TxVgaFor50[24].TxVgaValue = pTxVga[14] - stmp * 2 / 4;
		pHwData->TxVgaFor50[23].TxVgaValue = pTxVga[14] - stmp * 3 / 4;

		
		pHwData->TxVgaFor50[29].ChanNo = 140;
		pHwData->TxVgaFor50[29].TxVgaValue = pTxVga[15];
		
		pHwData->TxVgaFor50[27].ChanNo = 132;
		pHwData->TxVgaFor50[28].ChanNo = 136;
		stmp = pTxVga[15] - pTxVga[14];
		pHwData->TxVgaFor50[28].TxVgaValue = pTxVga[15] - stmp / 3;
		pHwData->TxVgaFor50[27].TxVgaValue = pTxVga[15] - stmp * 2 / 3;

		
		pHwData->TxVgaFor50[30].ChanNo = 149;
		pHwData->TxVgaFor50[30].TxVgaValue = pTxVga[16];
		
		pHwData->TxVgaFor50[34].ChanNo = 165;
		pHwData->TxVgaFor50[34].TxVgaValue = pTxVga[17];
		
		pHwData->TxVgaFor50[31].ChanNo = 153;
		pHwData->TxVgaFor50[32].ChanNo = 157;
		pHwData->TxVgaFor50[33].ChanNo = 161;
		stmp = pTxVga[17] - pTxVga[16];
		pHwData->TxVgaFor50[33].TxVgaValue = pTxVga[17] - stmp / 4;
		pHwData->TxVgaFor50[32].TxVgaValue = pTxVga[17] - stmp * 2 / 4;
		pHwData->TxVgaFor50[31].TxVgaValue = pTxVga[17] - stmp * 3 / 4;
	}
}

void BBProcessor_RateChanging(struct hw_data *pHwData,  u8 rate)
{
	struct wb35_reg *reg = &pHwData->reg;
	unsigned char	Is11bRate;

	Is11bRate = (rate % 6) ? 1 : 0;
	switch (pHwData->phy_type) {
	case RF_AIROHA_2230:
	case RF_AIROHA_2230S:
		if (Is11bRate) {
			if ((reg->BB48 != BB48_DEFAULT_AL2230_11B) &&
				(reg->BB4C != BB4C_DEFAULT_AL2230_11B)) {
				Wb35Reg_Write(pHwData, 0x1048, BB48_DEFAULT_AL2230_11B);
				Wb35Reg_Write(pHwData, 0x104c, BB4C_DEFAULT_AL2230_11B);
			}
		} else {
			if ((reg->BB48 != BB48_DEFAULT_AL2230_11G) &&
				(reg->BB4C != BB4C_DEFAULT_AL2230_11G)) {
				Wb35Reg_Write(pHwData, 0x1048, BB48_DEFAULT_AL2230_11G);
				Wb35Reg_Write(pHwData, 0x104c, BB4C_DEFAULT_AL2230_11G);
			}
		}
		break;
	case RF_WB_242:
		if (Is11bRate) {
			if ((reg->BB48 != BB48_DEFAULT_WB242_11B) &&
				(reg->BB4C != BB4C_DEFAULT_WB242_11B)) {
				reg->BB48 = BB48_DEFAULT_WB242_11B;
				reg->BB4C = BB4C_DEFAULT_WB242_11B;
				Wb35Reg_Write(pHwData, 0x1048, BB48_DEFAULT_WB242_11B);
				Wb35Reg_Write(pHwData, 0x104c, BB4C_DEFAULT_WB242_11B);
			}
		} else {
			if ((reg->BB48 != BB48_DEFAULT_WB242_11G) &&
				(reg->BB4C != BB4C_DEFAULT_WB242_11G)) {
				reg->BB48 = BB48_DEFAULT_WB242_11G;
				reg->BB4C = BB4C_DEFAULT_WB242_11G;
				Wb35Reg_Write(pHwData, 0x1048, BB48_DEFAULT_WB242_11G);
				Wb35Reg_Write(pHwData, 0x104c, BB4C_DEFAULT_WB242_11G);
			}
		}
		break;
	}
}

