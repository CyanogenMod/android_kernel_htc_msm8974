/* Random defines and structures for the HP Lance driver.
 * Copyright (C) 05/1998 Peter Maydell <pmaydell@chiark.greenend.org.uk>
 * Based on the Sun Lance driver and the NetBSD HP Lance driver
 */

#define HPLANCE_ID		0x01		
#define HPLANCE_STATUS		0x03		

#define LE_IE 0x80                                
#define LE_IR 0x40                                
#define LE_LOCK 0x08                              
#define LE_ACK 0x04                               
#define LE_JAB 0x02                               

#define HPLANCE_IDOFF 0                           
#define HPLANCE_REGOFF 0x4000                     
#define HPLANCE_MEMOFF 0x8000                     
#define HPLANCE_NVRAMOFF 0xC008                   
