#ifndef SMCOMMON_INCD
#define SMCOMMON_INCD


#define SMSUCCESS           0x0000 
#define ERROR               0xFFFF 
#define CORRECT             0x0001 

#define NO_ERROR            0x0000 
#define ERR_WriteFault      0x0003 
#define ERR_HwError         0x0004 
#define ERR_DataStatus      0x0010 
#define ERR_EccReadErr      0x0011 
#define ERR_CorReadErr      0x0018 
#define ERR_OutOfLBA        0x0021 
#define ERR_WrtProtect      0x0027 
#define ERR_ChangedMedia    0x0028 
#define ERR_UnknownMedia    0x0030 
#define ERR_IllegalFmt      0x0031 
#define ERR_NoSmartMedia    0x003A 

void StringCopy(char *, char *, int);
int  StringCmp(char *, char *, int);

#endif
