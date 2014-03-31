/******************************************************************************
 * elfnote.h
 *
 * Definitions used for the Xen ELF notes.
 *
 * Copyright (c) 2006, Ian Campbell, XenSource Ltd.
 */

#ifndef __XEN_PUBLIC_ELFNOTE_H__
#define __XEN_PUBLIC_ELFNOTE_H__


#define XEN_ELFNOTE_INFO           0

#define XEN_ELFNOTE_ENTRY          1

#define XEN_ELFNOTE_HYPERCALL_PAGE 2

#define XEN_ELFNOTE_VIRT_BASE      3

#define XEN_ELFNOTE_PADDR_OFFSET   4

#define XEN_ELFNOTE_XEN_VERSION    5

#define XEN_ELFNOTE_GUEST_OS       6

#define XEN_ELFNOTE_GUEST_VERSION  7

#define XEN_ELFNOTE_LOADER         8

#define XEN_ELFNOTE_PAE_MODE       9

#define XEN_ELFNOTE_FEATURES      10

#define XEN_ELFNOTE_BSD_SYMTAB    11

#define XEN_ELFNOTE_HV_START_LOW  12

#define XEN_ELFNOTE_L1_MFN_VALID  13

#define XEN_ELFNOTE_SUSPEND_CANCEL 14

#endif 

