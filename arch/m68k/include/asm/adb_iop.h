/*
 * ADB through the IOP
 * Written by Joshua M. Thompson
 */


#define ADB_IOP		IOP_NUM_ISM
#define ADB_CHAN	2


#define ADB_IOP_LISTEN	0x01
#define ADB_IOP_TALK	0x02
#define ADB_IOP_EXISTS	0x04
#define ADB_IOP_FLUSH	0x08
#define ADB_IOP_RESET	0x10
#define ADB_IOP_INT	0x20
#define ADB_IOP_POLL	0x40
#define ADB_IOP_UNINT	0x80

#define AIF_RESET	0x00
#define AIF_FLUSH	0x01
#define AIF_LISTEN	0x08
#define AIF_TALK	0x0C


#define ADB_IOP_EXPLICIT	0x80	
#define ADB_IOP_AUTOPOLL	0x40	
#define ADB_IOP_SRQ		0x04	
#define ADB_IOP_TIMEOUT		0x02	

#ifndef __ASSEMBLY__

struct adb_iopmsg {
	__u8 flags;		
	__u8 count;		
	__u8 cmd;		
	__u8 data[8];		
	__u8 spare[21];		
};

#endif 
