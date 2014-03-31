/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */


#ifndef _TILE_HV_H
#define _TILE_HV_H

#include <arch/chip.h>

#ifdef __ASSEMBLER__
#define __HV_SIZE_ONE 1
#elif !defined(__tile__) && CHIP_VA_WIDTH() > 32
#define __HV_SIZE_ONE 1ULL
#else
#define __HV_SIZE_ONE 1UL
#endif

#define HV_LOG2_L1_SPAN 32

#define HV_L1_SPAN (__HV_SIZE_ONE << HV_LOG2_L1_SPAN)

#define HV_LOG2_PAGE_SIZE_SMALL 16

#define HV_PAGE_SIZE_SMALL (__HV_SIZE_ONE << HV_LOG2_PAGE_SIZE_SMALL)

#define HV_LOG2_PAGE_SIZE_LARGE 24

#define HV_PAGE_SIZE_LARGE (__HV_SIZE_ONE << HV_LOG2_PAGE_SIZE_LARGE)

#define HV_LOG2_PAGE_TABLE_ALIGN 11

#define HV_PAGE_TABLE_ALIGN (__HV_SIZE_ONE << HV_LOG2_PAGE_TABLE_ALIGN)

#define HV_GLUE_START_CPA 0x10000

#define HV_GLUE_RESERVED_SIZE 0x10000

#define HV_DISPATCH_ENTRY_SIZE 32

#define _HV_VERSION 11


#define _HV_DISPATCH_RESERVED                     0

#define HV_DISPATCH_INIT                          1

#define HV_DISPATCH_INSTALL_CONTEXT               2

#define HV_DISPATCH_SYSCONF                       3

#define HV_DISPATCH_GET_RTC                       4

#define HV_DISPATCH_SET_RTC                       5

#define HV_DISPATCH_FLUSH_ASID                    6

#define HV_DISPATCH_FLUSH_PAGE                    7

#define HV_DISPATCH_FLUSH_PAGES                   8

#define HV_DISPATCH_RESTART                       9

#define HV_DISPATCH_HALT                          10

#define HV_DISPATCH_POWER_OFF                     11

#define HV_DISPATCH_INQUIRE_PHYSICAL              12

#define HV_DISPATCH_INQUIRE_MEMORY_CONTROLLER     13

#define HV_DISPATCH_INQUIRE_VIRTUAL               14

#define HV_DISPATCH_INQUIRE_ASID                  15

#define HV_DISPATCH_NANOSLEEP                     16

#define HV_DISPATCH_CONSOLE_READ_IF_READY         17

#define HV_DISPATCH_CONSOLE_WRITE                 18

#define HV_DISPATCH_DOWNCALL_DISPATCH             19

#define HV_DISPATCH_INQUIRE_TOPOLOGY              20

#define HV_DISPATCH_FS_FINDFILE                   21

#define HV_DISPATCH_FS_FSTAT                      22

#define HV_DISPATCH_FS_PREAD                      23

#define HV_DISPATCH_PHYSADDR_READ64               24

#define HV_DISPATCH_PHYSADDR_WRITE64              25

#define HV_DISPATCH_GET_COMMAND_LINE              26

#define HV_DISPATCH_SET_CACHING                   27

#define HV_DISPATCH_BZERO_PAGE                    28

#define HV_DISPATCH_REGISTER_MESSAGE_STATE        29

#define HV_DISPATCH_SEND_MESSAGE                  30

#define HV_DISPATCH_RECEIVE_MESSAGE               31

#define HV_DISPATCH_INQUIRE_CONTEXT               32

#define HV_DISPATCH_START_ALL_TILES               33

#define HV_DISPATCH_DEV_OPEN                      34

#define HV_DISPATCH_DEV_CLOSE                     35

#define HV_DISPATCH_DEV_PREAD                     36

#define HV_DISPATCH_DEV_PWRITE                    37

#define HV_DISPATCH_DEV_POLL                      38

#define HV_DISPATCH_DEV_POLL_CANCEL               39

#define HV_DISPATCH_DEV_PREADA                    40

#define HV_DISPATCH_DEV_PWRITEA                   41

#define HV_DISPATCH_FLUSH_REMOTE                  42

#define HV_DISPATCH_CONSOLE_PUTC                  43

#define HV_DISPATCH_INQUIRE_TILES                 44

#define HV_DISPATCH_CONFSTR                       45

#define HV_DISPATCH_REEXEC                        46

#define HV_DISPATCH_SET_COMMAND_LINE              47

#if !CHIP_HAS_IPI()

#define HV_DISPATCH_CLEAR_INTR                    48

#define HV_DISPATCH_ENABLE_INTR                   49

#define HV_DISPATCH_DISABLE_INTR                  50

#define HV_DISPATCH_RAISE_INTR                    51

#define HV_DISPATCH_TRIGGER_IPI                   52

#endif 

#define HV_DISPATCH_STORE_MAPPING                 53

#define HV_DISPATCH_INQUIRE_REALPA                54

#define HV_DISPATCH_FLUSH_ALL                     55

#if CHIP_HAS_IPI()
#define HV_DISPATCH_GET_IPI_PTE                   56
#endif

#define _HV_DISPATCH_END                          57


#ifndef __ASSEMBLER__

#ifdef __KERNEL__
#include <asm/types.h>
typedef u32 __hv32;        
typedef u64 __hv64;        
#else
#include <stdint.h>
typedef uint32_t __hv32;   
typedef uint64_t __hv64;   
#endif


typedef __hv64 HV_PhysAddr;

#if CHIP_VA_WIDTH() > 32
typedef __hv64 HV_VirtAddr;
#else
typedef __hv32 HV_VirtAddr;
#endif 

typedef unsigned int HV_ASID;

typedef unsigned int HV_LOTAR;

typedef unsigned long HV_PageSize;

typedef struct
{
  __hv64 val;                
} HV_PTE;

typedef int HV_Errno;

#endif 

#define HV_OK           0    
#define HV_EINVAL      -801  
#define HV_ENODEV      -802  
#define HV_ENOENT      -803  
#define HV_EBADF       -804  
#define HV_EFAULT      -805  
#define HV_ERECIP      -806  
#define HV_E2BIG       -807  
#define HV_ENOTSUP     -808  
#define HV_EBUSY       -809  
#define HV_ENOSYS      -810  
#define HV_EPERM       -811  
#define HV_ENOTREADY   -812  
#define HV_EIO         -813  
#define HV_ENOMEM      -814  
#define HV_EAGAIN      -815  

#define HV_ERR_MAX     -801  
#define HV_ERR_MIN     -815  

#ifndef __ASSEMBLER__

typedef enum { HV_VERSION = _HV_VERSION } HV_VersionNumber;

void hv_init(HV_VersionNumber interface_version_number,
             int chip_num, int chip_rev_num);


typedef enum {
  
  _HV_SYSCONF_RESERVED       = 0,

  
  HV_SYSCONF_GLUE_SIZE       = 1,

  
  HV_SYSCONF_PAGE_SIZE_SMALL = 2,

  
  HV_SYSCONF_PAGE_SIZE_LARGE = 3,

  
  HV_SYSCONF_CPU_SPEED       = 4,

  HV_SYSCONF_CPU_TEMP        = 5,

  HV_SYSCONF_BOARD_TEMP      = 6

} HV_SysconfQuery;

#define HV_SYSCONF_TEMP_KTOC 273

#define HV_SYSCONF_OVERTEMP 999

long hv_sysconf(HV_SysconfQuery query);


typedef enum {
  
  _HV_CONFSTR_RESERVED        = 0,

  
  HV_CONFSTR_BOARD_PART_NUM   = 1,

  
  HV_CONFSTR_BOARD_SERIAL_NUM = 2,

  
  HV_CONFSTR_CHIP_SERIAL_NUM  = 3,

  
  HV_CONFSTR_BOARD_REV        = 4,

  
  HV_CONFSTR_HV_SW_VER        = 5,

  
  HV_CONFSTR_CHIP_MODEL       = 6,

  
  HV_CONFSTR_BOARD_DESC       = 7,

  
  HV_CONFSTR_HV_CONFIG        = 8,

  HV_CONFSTR_HV_CONFIG_VER    = 9,

  
  HV_CONFSTR_MEZZ_PART_NUM   = 10,

  
  HV_CONFSTR_MEZZ_SERIAL_NUM = 11,

  
  HV_CONFSTR_MEZZ_REV        = 12,

  
  HV_CONFSTR_MEZZ_DESC       = 13,

  
  HV_CONFSTR_SWITCH_CONTROL  = 14,

  
  HV_CONFSTR_CHIP_REV        = 15

} HV_ConfstrQuery;

int hv_confstr(HV_ConfstrQuery query, HV_VirtAddr buf, int len);

typedef struct
{
  
  int x;

  
  int y;
} HV_Coord;


#if CHIP_HAS_IPI()

int hv_get_ipi_pte(HV_Coord tile, int pl, HV_PTE* pte);

#else 

typedef __hv32 HV_IntrMask;

#define HV_MAX_IPI_INTERRUPT 7

void hv_enable_intr(HV_IntrMask enab_mask);

void hv_disable_intr(HV_IntrMask disab_mask);

void hv_clear_intr(HV_IntrMask clear_mask);

void hv_raise_intr(HV_IntrMask raise_mask);

HV_Errno hv_trigger_ipi(HV_Coord tile, int interrupt);

#endif 

int hv_store_mapping(HV_VirtAddr va, unsigned int len, HV_PhysAddr pa);

HV_PhysAddr hv_inquire_realpa(HV_PhysAddr cpa, unsigned int len);

#define HV_RTC_NO_CHIP     0x1

#define HV_RTC_LOW_VOLTAGE 0x2

typedef struct {
#if CHIP_WORD_SIZE() > 32
  __hv64 tm_sec;   
  __hv64 tm_min;   
  __hv64 tm_hour;  
  __hv64 tm_mday;  
  __hv64 tm_mon;   
  __hv64 tm_year;  
  __hv64 flags;    
#else
  __hv32 tm_sec;   
  __hv32 tm_min;   
  __hv32 tm_hour;  
  __hv32 tm_mday;  
  __hv32 tm_mon;   
  __hv32 tm_year;  
  __hv32 flags;    
#endif
} HV_RTCTime;

HV_RTCTime hv_get_rtc(void);


void hv_set_rtc(HV_RTCTime time);

int hv_install_context(HV_PhysAddr page_table, HV_PTE access, HV_ASID asid,
                       __hv32 flags);

#endif 

#define HV_CTX_DIRECTIO     0x1   

#ifndef __ASSEMBLER__

typedef struct
{
  
  HV_PhysAddr page_table;

  
  HV_PTE access;

  
  HV_ASID asid;

  
  __hv32 flags;
} HV_Context;

HV_Context hv_inquire_context(void);


int hv_flush_asid(HV_ASID asid);


int hv_flush_page(HV_VirtAddr address, HV_PageSize page_size);


int hv_flush_pages(HV_VirtAddr start, HV_PageSize page_size,
                   unsigned long size);


int hv_flush_all(int preserve_global);


void hv_restart(HV_VirtAddr cmd, HV_VirtAddr args);


void hv_halt(void);


void hv_power_off(void);


int hv_reexec(HV_PhysAddr entry);


typedef struct
{
  
  HV_Coord coord;

  
  int width;

  
  int height;

} HV_Topology;

HV_Topology hv_inquire_topology(void);

typedef enum {
  
  _HV_INQ_TILES_RESERVED       = 0,

  
  HV_INQ_TILES_AVAIL           = 1,

  
  HV_INQ_TILES_HFH_CACHE       = 2,

  
  HV_INQ_TILES_LOTAR           = 3
} HV_InqTileSet;

HV_Errno hv_inquire_tiles(HV_InqTileSet set, HV_VirtAddr cpumask, int length);


typedef int HV_MemoryController;

typedef struct
{
  HV_PhysAddr start;   
  __hv64 size;         
  HV_MemoryController controller;  
} HV_PhysAddrRange;

HV_PhysAddrRange hv_inquire_physical(int idx);

typedef enum
{
  NO_DIMM                    = 0,  
  DDR2                       = 1,  
  DDR3                       = 2   
} HV_DIMM_Type;

#ifdef __tilegx__

#define HV_MSH_MIN_DIMM_SIZE_SHIFT 29

#define HV_MSH_MAX_DIMMS 8

#else

#define HV_MSH_MIN_DIMM_SIZE_SHIFT 26

#define HV_MSH_MAX_DIMMS 2

#endif

#define HV_DIMM_TYPE_SHIFT 0

#define HV_DIMM_TYPE_MASK 0xf

#define HV_DIMM_SIZE_SHIFT 4

#define HV_DIMM_SIZE_MASK 0xf

typedef struct
{
  HV_Coord coord;   
  __hv64 speed;     
} HV_MemoryControllerInfo;

HV_MemoryControllerInfo hv_inquire_memory_controller(HV_Coord coord,
                                                     int controller);


typedef struct
{
  HV_VirtAddr start;   
  __hv64 size;         
} HV_VirtAddrRange;

HV_VirtAddrRange hv_inquire_virtual(int idx);


typedef struct
{
  HV_ASID start;        
  unsigned int size;    
} HV_ASIDRange;

HV_ASIDRange hv_inquire_asid(int idx);


void hv_nanosleep(int nanosecs);


int hv_console_read_if_ready(void);


void hv_console_putc(int byte);


/** Writes a string to the console, blocking if the console is busy.
 * @param bytes Pointer to characters to write.
 * @param len Number of characters to write.
 * @return Number of characters written, or HV_EFAULT if the buffer is invalid.
 */
int hv_console_write(HV_VirtAddr bytes, int len);


void hv_downcall_dispatch(void);

#endif 

#define INT_MESSAGE_RCV_DWNCL    INT_BOOT_ACCESS
#define INT_DMATLB_MISS_DWNCL    INT_DMA_ASID
#define INT_SNITLB_MISS_DWNCL    INT_SNI_ASID
#define INT_DMATLB_ACCESS_DWNCL  INT_DMA_CPL
#define INT_DEV_INTR_DWNCL       INT_WORLD_ACCESS

#ifndef __ASSEMBLER__

int hv_fs_findfile(HV_VirtAddr filename);


typedef struct
{
  int size;             
  unsigned int flags;   
} HV_FS_StatInfo;

typedef enum
{
  HV_FS_ISDIR    = 0x0001   
} HV_FS_FSTAT_FLAGS;

HV_FS_StatInfo hv_fs_fstat(int inode);


int hv_fs_pread(int inode, HV_VirtAddr buf, int length, int offset);


unsigned long long hv_physaddr_read64(HV_PhysAddr addr, HV_PTE access);


void hv_physaddr_write64(HV_PhysAddr addr, HV_PTE access,
                         unsigned long long val);


int hv_get_command_line(HV_VirtAddr buf, int length);


HV_Errno hv_set_command_line(HV_VirtAddr buf, int length);

#define HV_COMMAND_LINE_LEN  256

void hv_set_caching(unsigned int bitmask);


void hv_bzero_page(HV_VirtAddr va, unsigned int size);


typedef struct
{
#if CHIP_VA_WIDTH() > 32
  __hv64 opaque[2]; 
#else
  __hv32 opaque[2]; 
#endif
}
HV_MsgState;

HV_Errno hv_register_message_state(HV_MsgState* msgstate);

typedef enum
{
  HV_TO_BE_SENT,    
  HV_SENT,          
  HV_BAD_RECIP      
} HV_Recip_State;

typedef struct
{
  
  unsigned int x:11;

  
  unsigned int y:11;

  
  HV_Recip_State state:10;
} HV_Recipient;

int hv_send_message(HV_Recipient *recips, int nrecip,
                    HV_VirtAddr buf, int buflen);

#define HV_MAX_MESSAGE_SIZE 28


typedef struct
{
  int msglen;     
  __hv32 source;  
} HV_RcvMsgInfo;

#define HV_MSG_TILE 0x0         
#define HV_MSG_INTR 0x1         

/** Receive a message.
 *
 * This routine retrieves a message from the client's incoming message
 * buffer.
 *
 * Multiple messages sent from a particular sending tile to a particular
 * receiving tile are received in the order that they were sent; however,
 * no ordering is guaranteed between messages sent by different tiles.
 *
 * Whenever the a client's message buffer is empty, the first message
 * subsequently received will cause the client's MESSAGE_RCV_DWNCL
 * interrupt vector to be invoked through the interrupt downcall mechanism
 * (see the description of the hv_downcall_dispatch() routine for details
 * on downcalls).
 *
 * Another message-available downcall will not occur until a call to
 * this routine is made when the message buffer is empty, and a message
 * subsequently arrives.  Note that such a downcall could occur while
 * this routine is executing.  If the calling code does not wish this
 * to happen, it is recommended that this routine be called with the
 * INTCTRL_1 interrupt masked, or inside an interrupt critical section.
 *
 * msgstate is the value previously passed to hv_register_message_state().
 * buf is the virtual address of the buffer into which the message will
 * be written; buflen is the length of the buffer.
 *
 * This routine returns an HV_RcvMsgInfo structure.  The msglen member
 * of that structure is the length of the message received, zero if no
 * message is available, or HV_E2BIG if the message is too large for the
 * specified buffer.  If the message is too large, it is not consumed,
 * and may be retrieved by a subsequent call to this routine specifying
 * a sufficiently large buffer.  A buffer which is HV_MAX_MESSAGE_SIZE
 * bytes long is guaranteed to be able to receive any possible message.
 *
 * The source member of the HV_RcvMsgInfo structure describes the sender
 * of the message.  For messages sent by another client tile via an
 * hv_send_message() call, this value is HV_MSG_TILE; for messages sent
 * as a result of a device interrupt, this value is HV_MSG_INTR.
 */

HV_RcvMsgInfo hv_receive_message(HV_MsgState msgstate, HV_VirtAddr buf,
                                 int buflen);


void hv_start_all_tiles(void);


int hv_dev_open(HV_VirtAddr name, __hv32 flags);


int hv_dev_close(int devhdl);


/** Read data from a hypervisor device synchronously.
 *
 *  This service transfers data from a hypervisor device to a memory buffer.
 *  When the service returns, the data has been written from the memory buffer,
 *  and the buffer will not be further modified by the driver.
 *
 *  No ordering is guaranteed between requests issued from different tiles.
 *
 *  Devices may choose to support both the synchronous and asynchronous read
 *  operations, only one of them, or neither of them.
 *
 * @param devhdl Device handle of the device to be read from.
 * @param flags Flags (HV_DEV_xxx).
 * @param va Virtual address of the target data buffer.  This buffer must
 *        be mapped in the currently installed page table; if not, HV_EFAULT
 *        may be returned.
 * @param len Number of bytes to be transferred.
 * @param offset Driver-dependent offset.  For a random-access device, this is
 *        often a byte offset from the beginning of the device; in other cases,
 *        like on a control device, it may have a different meaning.
 * @return A non-negative value if the read was at least partially successful;
 *         otherwise, a negative error code.  The precise interpretation of
 *         the return value is driver-dependent, but many drivers will return
 *         the number of bytes successfully transferred.
 */
int hv_dev_pread(int devhdl, __hv32 flags, HV_VirtAddr va, __hv32 len,
                 __hv64 offset);

#define HV_DEV_NB_EMPTY     0x1   
#define HV_DEV_NB_PARTIAL   0x2   
#define HV_DEV_NOCACHE      0x4   

#define HV_DEV_ALLFLAGS     (HV_DEV_NB_EMPTY | HV_DEV_NB_PARTIAL | \
                             HV_DEV_NOCACHE)   

/** Write data to a hypervisor device synchronously.
 *
 *  This service transfers data from a memory buffer to a hypervisor device.
 *  When the service returns, the data has been read from the memory buffer,
 *  and the buffer may be overwritten by the client; the data may not
 *  necessarily have been conveyed to the actual hardware I/O interface.
 *
 *  No ordering is guaranteed between requests issued from different tiles.
 *
 *  Devices may choose to support both the synchronous and asynchronous write
 *  operations, only one of them, or neither of them.
 *
 * @param devhdl Device handle of the device to be written to.
 * @param flags Flags (HV_DEV_xxx).
 * @param va Virtual address of the source data buffer.  This buffer must
 *        be mapped in the currently installed page table; if not, HV_EFAULT
 *        may be returned.
 * @param len Number of bytes to be transferred.
 * @param offset Driver-dependent offset.  For a random-access device, this is
 *        often a byte offset from the beginning of the device; in other cases,
 *        like on a control device, it may have a different meaning.
 * @return A non-negative value if the write was at least partially successful;
 *         otherwise, a negative error code.  The precise interpretation of
 *         the return value is driver-dependent, but many drivers will return
 *         the number of bytes successfully transferred.
 */
int hv_dev_pwrite(int devhdl, __hv32 flags, HV_VirtAddr va, __hv32 len,
                  __hv64 offset);


#if CHIP_VA_WIDTH() > 32
typedef __hv64 HV_IntArg;
#else
typedef __hv32 HV_IntArg;
#endif


typedef struct
{
  HV_IntArg intarg;  
  HV_IntArg intdata; 
} HV_IntrMsg;

int hv_dev_poll(int devhdl, __hv32 events, HV_IntArg intarg);

#define HV_DEVPOLL_READ     0x1   
#define HV_DEVPOLL_WRITE    0x2   
#define HV_DEVPOLL_FLUSH    0x4   


int hv_dev_poll_cancel(int devhdl);


typedef struct
#if CHIP_VA_WIDTH() <= 32
__attribute__ ((packed, aligned(4)))
#endif
{
  HV_PhysAddr pa;  
  HV_PTE pte;      
  __hv32 len;      
} HV_SGL;

#define HV_SGL_MAXLEN 16  

/** Read data from a hypervisor device asynchronously.
 *
 *  This service transfers data from a hypervisor device to a memory buffer.
 *  When the service returns, the read has been scheduled.  When the read
 *  completes, an interrupt message will be delivered, and the buffer will
 *  not be further modified by the driver.
 *
 *  The number of possible outstanding asynchronous requests is defined by
 *  each driver, but it is recommended that it be at least two requests
 *  per tile per device.
 *
 *  No ordering is guaranteed between synchronous and asynchronous requests,
 *  even those issued on the same tile.
 *
 *  The completion interrupt message could come directly, or via the downcall
 *  (intctrl1) method, depending on what the tile is doing when the read
 *  completes.  Interrupts do not coalesce; one is delivered for each
 *  asynchronous I/O request.  Note that it is possible for the requested
 *  interrupt to be delivered after this service is called but before it
 *  returns.
 *
 *  Devices may choose to support both the synchronous and asynchronous read
 *  operations, only one of them, or neither of them.
 *
 * @param devhdl Device handle of the device to be read from.
 * @param flags Flags (HV_DEV_xxx).
 * @param sgl_len Number of elements in the scatter-gather list.
 * @param sgl Scatter-gather list describing the memory to which data will be
 *        written.
 * @param offset Driver-dependent offset.  For a random-access device, this is
 *        often a byte offset from the beginning of the device; in other cases,
 *        like on a control device, it may have a different meaning.
 * @param intarg Value which will be delivered as the intarg member of the
 *        eventual interrupt message; the intdata member will be set to the
 *        normal return value from the read request.
 * @return Zero if the read was successfully scheduled; otherwise, a negative
 *         error code.  Note that some drivers may choose to pre-validate
 *         their arguments, and may thus detect certain device error
 *         conditions at this time rather than when the completion notification
 *         occurs, but this is not required.
 */
int hv_dev_preada(int devhdl, __hv32 flags, __hv32 sgl_len,
                  HV_SGL sgl[], __hv64 offset, HV_IntArg intarg);


/** Write data to a hypervisor device asynchronously.
 *
 *  This service transfers data from a memory buffer to a hypervisor
 *  device.  When the service returns, the write has been scheduled.
 *  When the write completes, an interrupt message will be delivered,
 *  and the buffer may be overwritten by the client; the data may not
 *  necessarily have been conveyed to the actual hardware I/O interface.
 *
 *  The number of possible outstanding asynchronous requests is defined by
 *  each driver, but it is recommended that it be at least two requests
 *  per tile per device.
 *
 *  No ordering is guaranteed between synchronous and asynchronous requests,
 *  even those issued on the same tile.
 *
 *  The completion interrupt message could come directly, or via the downcall
 *  (intctrl1) method, depending on what the tile is doing when the read
 *  completes.  Interrupts do not coalesce; one is delivered for each
 *  asynchronous I/O request.  Note that it is possible for the requested
 *  interrupt to be delivered after this service is called but before it
 *  returns.
 *
 *  Devices may choose to support both the synchronous and asynchronous write
 *  operations, only one of them, or neither of them.
 *
 * @param devhdl Device handle of the device to be read from.
 * @param flags Flags (HV_DEV_xxx).
 * @param sgl_len Number of elements in the scatter-gather list.
 * @param sgl Scatter-gather list describing the memory from which data will be
 *        read.
 * @param offset Driver-dependent offset.  For a random-access device, this is
 *        often a byte offset from the beginning of the device; in other cases,
 *        like on a control device, it may have a different meaning.
 * @param intarg Value which will be delivered as the intarg member of the
 *        eventual interrupt message; the intdata member will be set to the
 *        normal return value from the write request.
 * @return Zero if the write was successfully scheduled; otherwise, a negative
 *         error code.  Note that some drivers may choose to pre-validate
 *         their arguments, and may thus detect certain device error
 *         conditions at this time rather than when the completion notification
 *         occurs, but this is not required.
 */
int hv_dev_pwritea(int devhdl, __hv32 flags, __hv32 sgl_len,
                   HV_SGL sgl[], __hv64 offset, HV_IntArg intarg);


typedef struct
{
  
  unsigned int x:11;

  
  unsigned int y:11;

  
  HV_ASID asid:10;
} HV_Remote_ASID;

int hv_flush_remote(HV_PhysAddr cache_pa, unsigned long cache_control,
                    unsigned long* cache_cpumask,
                    HV_VirtAddr tlb_va, unsigned long tlb_length,
                    unsigned long tlb_pgsize, unsigned long* tlb_cpumask,
                    HV_Remote_ASID* asids, int asidcount);

#define HV_FLUSH_EVICT_L2 (1UL << 31)

#define HV_FLUSH_EVICT_L1I (1UL << 30)

#define HV_FLUSH_MAX_CACHE_LEN ((1UL << 30) - 1)

#define HV_FLUSH_ALL -1UL

#else   

#define HV_FLUSH_EVICT_L2 (1 << 31)

#define HV_FLUSH_EVICT_L1I (1 << 30)

#define HV_FLUSH_MAX_CACHE_LEN ((1 << 30) - 1)

#define HV_FLUSH_ALL -1

#endif  

#ifndef __ASSEMBLER__

#define hv_pte_val(pte) ((pte).val)

#define hv_pte(val) ((HV_PTE) { val })

#endif  


#define HV_LOG2_PTE_SIZE 3

#define HV_PTE_SIZE (1 << HV_LOG2_PTE_SIZE)


#define HV_PTE_INDEX_PRESENT          0  
#define HV_PTE_INDEX_MIGRATING        1  
#define HV_PTE_INDEX_CLIENT0          2  
#define HV_PTE_INDEX_CLIENT1          3  
#define HV_PTE_INDEX_NC               4  
#define HV_PTE_INDEX_NO_ALLOC_L1      5  
#define HV_PTE_INDEX_NO_ALLOC_L2      6  
#define HV_PTE_INDEX_CACHED_PRIORITY  7  
#define HV_PTE_INDEX_PAGE             8  
#define HV_PTE_INDEX_GLOBAL           9  
#define HV_PTE_INDEX_USER            10  
#define HV_PTE_INDEX_ACCESSED        11  
#define HV_PTE_INDEX_DIRTY           12  /**< Page has been written */
#define HV_PTE_INDEX_MODE            16  
#define HV_PTE_MODE_BITS              3  
#define HV_PTE_INDEX_LOTAR           20  
#define HV_PTE_LOTAR_BITS            12  

#define HV_PTE_INDEX_READABLE        32  
#define HV_PTE_INDEX_WRITABLE        33  
#define HV_PTE_INDEX_EXECUTABLE      34  
#define HV_PTE_INDEX_PTFN            35  
#define HV_PTE_PTFN_BITS             29  

#define HV_PTE_INDEX_PFN (HV_PTE_INDEX_PTFN + (HV_LOG2_PAGE_SIZE_SMALL - \
                                               HV_LOG2_PAGE_TABLE_ALIGN))

#define HV_PTE_INDEX_PFN_BITS (HV_PTE_INDEX_PTFN_BITS - \
                               (HV_LOG2_PAGE_SIZE_SMALL - \
                                HV_LOG2_PAGE_TABLE_ALIGN))

#define HV_PTE_MODE_UNCACHED          1

#define HV_PTE_MODE_CACHE_NO_L3       2

#define HV_PTE_MODE_CACHE_TILE_L3     3

#define HV_PTE_MODE_CACHE_HASH_L3     4

#define HV_PTE_MODE_MMIO              5


#ifdef __ASSEMBLER__
#define __HV_PTE_ONE 1        
#else
#define __HV_PTE_ONE 1ULL     
#endif

#define HV_PTE_PRESENT               (__HV_PTE_ONE << HV_PTE_INDEX_PRESENT)

#define HV_PTE_PAGE                  (__HV_PTE_ONE << HV_PTE_INDEX_PAGE)

#define HV_PTE_GLOBAL                (__HV_PTE_ONE << HV_PTE_INDEX_GLOBAL)

#define HV_PTE_USER                  (__HV_PTE_ONE << HV_PTE_INDEX_USER)

#define HV_PTE_ACCESSED              (__HV_PTE_ONE << HV_PTE_INDEX_ACCESSED)

/** Is this mapping dirty?
 *
 * This bit is set by the hypervisor when the memory described by the
 * translation is written for the first time.  It is never cleared by
 * the hypervisor, but may be cleared by the client.  After the bit
 * has been cleared, subsequent references are not guaranteed to set
 * it again until the translation has been flushed from the TLB.
 *
 * This bit is ignored in level-1 PTEs unless the Page bit is set.
 */
#define HV_PTE_DIRTY                 (__HV_PTE_ONE << HV_PTE_INDEX_DIRTY)

#define HV_PTE_MIGRATING             (__HV_PTE_ONE << HV_PTE_INDEX_MIGRATING)

#define HV_PTE_CLIENT0               (__HV_PTE_ONE << HV_PTE_INDEX_CLIENT0)

#define HV_PTE_CLIENT1               (__HV_PTE_ONE << HV_PTE_INDEX_CLIENT1)

#define HV_PTE_NC                    (__HV_PTE_ONE << HV_PTE_INDEX_NC)

#define HV_PTE_NO_ALLOC_L1           (__HV_PTE_ONE << HV_PTE_INDEX_NO_ALLOC_L1)

#define HV_PTE_NO_ALLOC_L2           (__HV_PTE_ONE << HV_PTE_INDEX_NO_ALLOC_L2)

#define HV_PTE_CACHED_PRIORITY       (__HV_PTE_ONE << \
                                      HV_PTE_INDEX_CACHED_PRIORITY)

#define HV_PTE_READABLE              (__HV_PTE_ONE << HV_PTE_INDEX_READABLE)

#define HV_PTE_WRITABLE              (__HV_PTE_ONE << HV_PTE_INDEX_WRITABLE)

#define HV_PTE_EXECUTABLE            (__HV_PTE_ONE << HV_PTE_INDEX_EXECUTABLE)

#define HV_LOTAR_WIDTH 11

#define HV_XY_TO_LOTAR(x, y) ((HV_LOTAR)(((x) << HV_LOTAR_WIDTH) | (y)))

#define HV_LOTAR_X(lotar) ((lotar) >> HV_LOTAR_WIDTH)

#define HV_LOTAR_Y(lotar) ((lotar) & ((1 << HV_LOTAR_WIDTH) - 1))

#ifndef __ASSEMBLER__

#define _HV_BIT(name, bit)                                      \
static __inline int                                             \
hv_pte_get_##name(HV_PTE pte)                                   \
{                                                               \
  return (pte.val >> HV_PTE_INDEX_##bit) & 1;                   \
}                                                               \
                                                                \
static __inline HV_PTE                                          \
hv_pte_set_##name(HV_PTE pte)                                   \
{                                                               \
  pte.val |= 1ULL << HV_PTE_INDEX_##bit;                        \
  return pte;                                                   \
}                                                               \
                                                                \
static __inline HV_PTE                                          \
hv_pte_clear_##name(HV_PTE pte)                                 \
{                                                               \
  pte.val &= ~(1ULL << HV_PTE_INDEX_##bit);                     \
  return pte;                                                   \
}

_HV_BIT(present,         PRESENT)
_HV_BIT(page,            PAGE)
_HV_BIT(client0,         CLIENT0)
_HV_BIT(client1,         CLIENT1)
_HV_BIT(migrating,       MIGRATING)
_HV_BIT(nc,              NC)
_HV_BIT(readable,        READABLE)
_HV_BIT(writable,        WRITABLE)
_HV_BIT(executable,      EXECUTABLE)
_HV_BIT(accessed,        ACCESSED)
_HV_BIT(dirty,           DIRTY)
_HV_BIT(no_alloc_l1,     NO_ALLOC_L1)
_HV_BIT(no_alloc_l2,     NO_ALLOC_L2)
_HV_BIT(cached_priority, CACHED_PRIORITY)
_HV_BIT(global,          GLOBAL)
_HV_BIT(user,            USER)

#undef _HV_BIT

static __inline unsigned int
hv_pte_get_mode(const HV_PTE pte)
{
  return (((__hv32) pte.val) >> HV_PTE_INDEX_MODE) &
         ((1 << HV_PTE_MODE_BITS) - 1);
}

static __inline HV_PTE
hv_pte_set_mode(HV_PTE pte, unsigned int val)
{
  pte.val &= ~(((1ULL << HV_PTE_MODE_BITS) - 1) << HV_PTE_INDEX_MODE);
  pte.val |= val << HV_PTE_INDEX_MODE;
  return pte;
}

static __inline unsigned int
hv_pte_get_pfn(const HV_PTE pte)
{
  return pte.val >> HV_PTE_INDEX_PFN;
}


static __inline HV_PTE
hv_pte_set_pfn(HV_PTE pte, unsigned int val)
{
  pte.val &= ~(((1ULL << HV_PTE_PTFN_BITS) - 1) << HV_PTE_INDEX_PTFN);
  pte.val |= (__hv64) val << HV_PTE_INDEX_PFN;
  return pte;
}

static __inline unsigned long
hv_pte_get_ptfn(const HV_PTE pte)
{
  return pte.val >> HV_PTE_INDEX_PTFN;
}


static __inline HV_PTE
hv_pte_set_ptfn(HV_PTE pte, unsigned long val)
{
  pte.val &= ~(((1ULL << HV_PTE_PTFN_BITS)-1) << HV_PTE_INDEX_PTFN);
  pte.val |= (__hv64) val << HV_PTE_INDEX_PTFN;
  return pte;
}


static __inline unsigned int
hv_pte_get_lotar(const HV_PTE pte)
{
  unsigned int lotar = ((__hv32) pte.val) >> HV_PTE_INDEX_LOTAR;

  return HV_XY_TO_LOTAR( (lotar >> (HV_PTE_LOTAR_BITS / 2)),
                         (lotar & ((1 << (HV_PTE_LOTAR_BITS / 2)) - 1)) );
}


static __inline HV_PTE
hv_pte_set_lotar(HV_PTE pte, unsigned int val)
{
  unsigned int x = HV_LOTAR_X(val);
  unsigned int y = HV_LOTAR_Y(val);

  pte.val &= ~(((1ULL << HV_PTE_LOTAR_BITS)-1) << HV_PTE_INDEX_LOTAR);
  pte.val |= (x << (HV_PTE_INDEX_LOTAR + HV_PTE_LOTAR_BITS / 2)) |
             (y << HV_PTE_INDEX_LOTAR);
  return pte;
}

#endif  

#define HV_CPA_TO_PFN(p) ((p) >> HV_LOG2_PAGE_SIZE_SMALL)

#define HV_PFN_TO_CPA(p) (((HV_PhysAddr)(p)) << HV_LOG2_PAGE_SIZE_SMALL)

#define HV_CPA_TO_PTFN(p) ((p) >> HV_LOG2_PAGE_TABLE_ALIGN)

#define HV_PTFN_TO_CPA(p) (((HV_PhysAddr)(p)) << HV_LOG2_PAGE_TABLE_ALIGN)

#define HV_PTFN_TO_PFN(p) \
  ((p) >> (HV_LOG2_PAGE_SIZE_SMALL - HV_LOG2_PAGE_TABLE_ALIGN))

#define HV_PFN_TO_PTFN(p) \
  ((p) << (HV_LOG2_PAGE_SIZE_SMALL - HV_LOG2_PAGE_TABLE_ALIGN))

#if CHIP_VA_WIDTH() > 32

#define HV_LOG2_L0_ENTRIES (CHIP_VA_WIDTH() - HV_LOG2_L1_SPAN)

#define HV_L0_ENTRIES (1 << HV_LOG2_L0_ENTRIES)

#define HV_LOG2_L0_SIZE (HV_LOG2_PTE_SIZE + HV_LOG2_L0_ENTRIES)

#define HV_L0_SIZE (1 << HV_LOG2_L0_SIZE)

#ifdef __ASSEMBLER__

#define HV_L0_INDEX(va) \
  (((va) >> HV_LOG2_L1_SPAN) & (HV_L0_ENTRIES - 1))

#else

#define HV_L0_INDEX(va) \
  (((HV_VirtAddr)(va) >> HV_LOG2_L1_SPAN) & (HV_L0_ENTRIES - 1))

#endif

#endif 

#define HV_LOG2_L1_ENTRIES (HV_LOG2_L1_SPAN - HV_LOG2_PAGE_SIZE_LARGE)

#define HV_L1_ENTRIES (1 << HV_LOG2_L1_ENTRIES)

#define HV_LOG2_L1_SIZE (HV_LOG2_PTE_SIZE + HV_LOG2_L1_ENTRIES)

#define HV_L1_SIZE (1 << HV_LOG2_L1_SIZE)

#define HV_LOG2_L2_ENTRIES (HV_LOG2_PAGE_SIZE_LARGE - HV_LOG2_PAGE_SIZE_SMALL)

#define HV_L2_ENTRIES (1 << HV_LOG2_L2_ENTRIES)

#define HV_LOG2_L2_SIZE (HV_LOG2_PTE_SIZE + HV_LOG2_L2_ENTRIES)

#define HV_L2_SIZE (1 << HV_LOG2_L2_SIZE)

#ifdef __ASSEMBLER__

#if CHIP_VA_WIDTH() > 32

#define HV_L1_INDEX(va) \
  (((va) >> HV_LOG2_PAGE_SIZE_LARGE) & (HV_L1_ENTRIES - 1))

#else 

#define HV_L1_INDEX(va) \
  (((va) >> HV_LOG2_PAGE_SIZE_LARGE))

#endif 

#define HV_L2_INDEX(va) \
  (((va) >> HV_LOG2_PAGE_SIZE_SMALL) & (HV_L2_ENTRIES - 1))

#else 

#if CHIP_VA_WIDTH() > 32

#define HV_L1_INDEX(va) \
  (((HV_VirtAddr)(va) >> HV_LOG2_PAGE_SIZE_LARGE) & (HV_L1_ENTRIES - 1))

#else 

#define HV_L1_INDEX(va) \
  (((HV_VirtAddr)(va) >> HV_LOG2_PAGE_SIZE_LARGE))

#endif 

#define HV_L2_INDEX(va) \
  (((HV_VirtAddr)(va) >> HV_LOG2_PAGE_SIZE_SMALL) & (HV_L2_ENTRIES - 1))

#endif 

#endif 
