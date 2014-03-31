
#ifndef _LINUX_ETRAXI2C_H
#define _LINUX_ETRAXI2C_H


#define ETRAXI2C_IOCTYPE 44



#define I2C_WRITEARG(slave, reg, value) (((slave) << 16) | ((reg) << 8) | (value))
#define I2C_READARG(slave, reg) (((slave) << 16) | ((reg) << 8))

#define I2C_ARGSLAVE(arg) ((arg) >> 16)
#define I2C_ARGREG(arg) (((arg) >> 8) & 0xff)
#define I2C_ARGVALUE(arg) ((arg) & 0xff)

#define I2C_WRITEREG 0x1   
#define I2C_READREG  0x2   

#endif
