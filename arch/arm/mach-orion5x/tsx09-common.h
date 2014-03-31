#ifndef __ARCH_ORION5X_TSX09_COMMON_H
#define __ARCH_ORION5X_TSX09_COMMON_H

extern void qnap_tsx09_power_off(void);

extern void __init qnap_tsx09_find_mac_addr(u32 mem_base, u32 size);

extern struct mv643xx_eth_platform_data qnap_tsx09_eth_data;


#endif
