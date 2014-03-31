/*  
 * ioctl defines for ethernet driver
 *
 * Copyright (c) 2001 Axis Communications AB
 * 
 * Author: Mikael Starvik 
 *
 */

#ifndef _CRIS_ETHERNET_H
#define _CRIS_ETHERNET_H
#define SET_ETH_SPEED_AUTO      SIOCDEVPRIVATE          
#define SET_ETH_SPEED_10        SIOCDEVPRIVATE+1        
#define SET_ETH_SPEED_100       SIOCDEVPRIVATE+2        
#define SET_ETH_DUPLEX_AUTO     SIOCDEVPRIVATE+3        
#define SET_ETH_DUPLEX_HALF     SIOCDEVPRIVATE+4        
#define SET_ETH_DUPLEX_FULL     SIOCDEVPRIVATE+5        
#define SET_ETH_ENABLE_LEDS     SIOCDEVPRIVATE+6        
#define SET_ETH_DISABLE_LEDS    SIOCDEVPRIVATE+7        
#define SET_ETH_AUTONEG         SIOCDEVPRIVATE+8
#endif 
