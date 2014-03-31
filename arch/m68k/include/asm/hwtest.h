
#ifndef __ASM_HWTEST_H
#define __ASM_HWTEST_H

extern int hwreg_present(volatile void *regp);
extern int hwreg_write(volatile void *regp, unsigned short val);

#endif
