/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_IOERROR_H
#define _ASM_IA64_SN_IOERROR_H


typedef struct io_error_s {
    
    union {
	struct {
	    unsigned                ievb_errortype:1;
	    unsigned                ievb_widgetnum:1;
	    unsigned                ievb_widgetdev:1;
	    unsigned                ievb_srccpu:1;
	    unsigned                ievb_srcnode:1;
	    unsigned                ievb_errnode:1;
	    unsigned                ievb_sysioaddr:1;
	    unsigned                ievb_xtalkaddr:1;
	    unsigned                ievb_busspace:1;
	    unsigned                ievb_busaddr:1;
	    unsigned                ievb_vaddr:1;
	    unsigned                ievb_memaddr:1;
	    unsigned		    ievb_epc:1;
	    unsigned		    ievb_ef:1;
	    unsigned		    ievb_tnum:1;
	} iev_b;
	unsigned                iev_a;
    } ie_v;

    short                   ie_errortype;	
    short                   ie_widgetnum;	
    short                   ie_widgetdev;	
    cpuid_t                 ie_srccpu;	
    cnodeid_t               ie_srcnode;		
    cnodeid_t               ie_errnode;		
    iopaddr_t               ie_sysioaddr;	
    iopaddr_t               ie_xtalkaddr;	
    iopaddr_t               ie_busspace;	
    iopaddr_t               ie_busaddr;		
    caddr_t                 ie_vaddr;	
    iopaddr_t               ie_memaddr;		
    caddr_t		    ie_epc;		
    caddr_t		    ie_ef;		
    short		    ie_tnum;		
} ioerror_t;

#define	IOERROR_INIT(e)		do { (e)->ie_v.iev_a = 0; } while (0)
#define	IOERROR_SETVALUE(e,f,v)	do { (e)->ie_ ## f = (v); (e)->ie_v.iev_b.ievb_ ## f = 1; } while (0)

#endif 
