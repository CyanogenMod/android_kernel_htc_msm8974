#ifndef __ASM_CRIS_ARCH_USER_H
#define __ASM_CRIS_ARCH_USER_H

struct user_regs_struct {
        unsigned long r0;       
        unsigned long r1;
        unsigned long r2;
        unsigned long r3;
        unsigned long r4;
        unsigned long r5;
        unsigned long r6;
        unsigned long r7;
        unsigned long r8;
        unsigned long r9;
        unsigned long r10;
        unsigned long r11;
        unsigned long r12;
        unsigned long r13;
        unsigned long sp;       
        unsigned long pc;       
        unsigned long p0;       
        unsigned long vr;       
        unsigned long p2;       
        unsigned long p3;       
        unsigned long p4;       
        unsigned long ccr;      
        unsigned long p6;       
        unsigned long mof;      
        unsigned long p8;       
        unsigned long ibr;      
        unsigned long irp;      
        unsigned long srp;      
        unsigned long bar;      
        unsigned long dccr;     
        unsigned long brp;      
        unsigned long usp;      
        unsigned long csrinstr; 
        unsigned long csraddr;
        unsigned long csrdata;
};

#endif
