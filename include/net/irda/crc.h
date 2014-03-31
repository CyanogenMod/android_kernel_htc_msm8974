
#ifndef IRDA_CRC_H
#define IRDA_CRC_H

#include <linux/types.h>
#include <linux/crc-ccitt.h>

#define INIT_FCS  0xffff   
#define GOOD_FCS  0xf0b8   

#define irda_fcs(fcs, c) crc_ccitt_byte(fcs, c)

#define irda_calc_crc16(fcs, buf, len) crc_ccitt(fcs, buf, len)

#endif
