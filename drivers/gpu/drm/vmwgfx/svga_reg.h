/**********************************************************
 * Copyright 1998-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/


#ifndef _SVGA_REG_H_
#define _SVGA_REG_H_

#define PCI_VENDOR_ID_VMWARE            0x15AD
#define PCI_DEVICE_ID_VMWARE_SVGA2      0x0405

#define SVGA_REG_ENABLE_DISABLE     0
#define SVGA_REG_ENABLE_ENABLE      1
#define SVGA_REG_ENABLE_HIDE        2
#define SVGA_REG_ENABLE_ENABLE_HIDE (SVGA_REG_ENABLE_ENABLE |\
				     SVGA_REG_ENABLE_HIDE)

#define SVGA_CURSOR_ON_HIDE            0x0   
#define SVGA_CURSOR_ON_SHOW            0x1   
#define SVGA_CURSOR_ON_REMOVE_FROM_FB  0x2   
#define SVGA_CURSOR_ON_RESTORE_TO_FB   0x3   

#define SVGA_FB_MAX_TRACEABLE_SIZE      0x1000000

#define SVGA_MAX_PSEUDOCOLOR_DEPTH      8
#define SVGA_MAX_PSEUDOCOLORS           (1 << SVGA_MAX_PSEUDOCOLOR_DEPTH)
#define SVGA_NUM_PALETTE_REGS           (3 * SVGA_MAX_PSEUDOCOLORS)

#define SVGA_MAGIC         0x900000UL
#define SVGA_MAKE_ID(ver)  (SVGA_MAGIC << 8 | (ver))

#define SVGA_VERSION_2     2
#define SVGA_ID_2          SVGA_MAKE_ID(SVGA_VERSION_2)

#define SVGA_VERSION_1     1
#define SVGA_ID_1          SVGA_MAKE_ID(SVGA_VERSION_1)

#define SVGA_VERSION_0     0
#define SVGA_ID_0          SVGA_MAKE_ID(SVGA_VERSION_0)

#define SVGA_ID_INVALID    0xFFFFFFFF

#define SVGA_INDEX_PORT         0x0
#define SVGA_VALUE_PORT         0x1
#define SVGA_BIOS_PORT          0x2
#define SVGA_IRQSTATUS_PORT     0x8

#define SVGA_IRQFLAG_ANY_FENCE            0x1    
#define SVGA_IRQFLAG_FIFO_PROGRESS        0x2    
#define SVGA_IRQFLAG_FENCE_GOAL           0x4    


enum {
   SVGA_REG_ID = 0,
   SVGA_REG_ENABLE = 1,
   SVGA_REG_WIDTH = 2,
   SVGA_REG_HEIGHT = 3,
   SVGA_REG_MAX_WIDTH = 4,
   SVGA_REG_MAX_HEIGHT = 5,
   SVGA_REG_DEPTH = 6,
   SVGA_REG_BITS_PER_PIXEL = 7,       
   SVGA_REG_PSEUDOCOLOR = 8,
   SVGA_REG_RED_MASK = 9,
   SVGA_REG_GREEN_MASK = 10,
   SVGA_REG_BLUE_MASK = 11,
   SVGA_REG_BYTES_PER_LINE = 12,
   SVGA_REG_FB_START = 13,            
   SVGA_REG_FB_OFFSET = 14,
   SVGA_REG_VRAM_SIZE = 15,
   SVGA_REG_FB_SIZE = 16,

   

   SVGA_REG_CAPABILITIES = 17,
   SVGA_REG_MEM_START = 18,           
   SVGA_REG_MEM_SIZE = 19,
   SVGA_REG_CONFIG_DONE = 20,         
   SVGA_REG_SYNC = 21,                
   SVGA_REG_BUSY = 22,                
   SVGA_REG_GUEST_ID = 23,            
   SVGA_REG_CURSOR_ID = 24,           
   SVGA_REG_CURSOR_X = 25,            
   SVGA_REG_CURSOR_Y = 26,            
   SVGA_REG_CURSOR_ON = 27,           
   SVGA_REG_HOST_BITS_PER_PIXEL = 28, 
   SVGA_REG_SCRATCH_SIZE = 29,        
   SVGA_REG_MEM_REGS = 30,            
   SVGA_REG_NUM_DISPLAYS = 31,        
   SVGA_REG_PITCHLOCK = 32,           
   SVGA_REG_IRQMASK = 33,             

   
   SVGA_REG_NUM_GUEST_DISPLAYS = 34,
   SVGA_REG_DISPLAY_ID = 35,        
   SVGA_REG_DISPLAY_IS_PRIMARY = 36,
   SVGA_REG_DISPLAY_POSITION_X = 37,
   SVGA_REG_DISPLAY_POSITION_Y = 38,
   SVGA_REG_DISPLAY_WIDTH = 39,     
   SVGA_REG_DISPLAY_HEIGHT = 40,    

   
   SVGA_REG_GMR_ID = 41,
   SVGA_REG_GMR_DESCRIPTOR = 42,
   SVGA_REG_GMR_MAX_IDS = 43,
   SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH = 44,

   SVGA_REG_TRACES = 45,            
   SVGA_REG_GMRS_MAX_PAGES = 46,    
   SVGA_REG_MEMORY_SIZE = 47,       
   SVGA_REG_TOP = 48,               

   SVGA_PALETTE_BASE = 1024,        
   

   SVGA_SCRATCH_BASE = SVGA_PALETTE_BASE + SVGA_NUM_PALETTE_REGS
                                    
};


/*
 * Guest memory regions (GMRs):
 *
 * This is a new memory mapping feature available in SVGA devices
 * which have the SVGA_CAP_GMR bit set. Previously, there were two
 * fixed memory regions available with which to share data between the
 * device and the driver: the FIFO ('MEM') and the framebuffer. GMRs
 * are our name for an extensible way of providing arbitrary DMA
 * buffers for use between the driver and the SVGA device. They are a
 * new alternative to framebuffer memory, usable for both 2D and 3D
 * graphics operations.
 *
 * Since GMR mapping must be done synchronously with guest CPU
 * execution, we use a new pair of SVGA registers:
 *
 *   SVGA_REG_GMR_ID --
 *
 *     Read/write.
 *     This register holds the 32-bit ID (a small positive integer)
 *     of a GMR to create, delete, or redefine. Writing this register
 *     has no side-effects.
 *
 *   SVGA_REG_GMR_DESCRIPTOR --
 *
 *     Write-only.
 *     Writing this register will create, delete, or redefine the GMR
 *     specified by the above ID register. If this register is zero,
 *     the GMR is deleted. Any pointers into this GMR (including those
 *     currently being processed by FIFO commands) will be
 *     synchronously invalidated.
 *
 *     If this register is nonzero, it must be the physical page
 *     number (PPN) of a data structure which describes the physical
 *     layout of the memory region this GMR should describe. The
 *     descriptor structure will be read synchronously by the SVGA
 *     device when this register is written. The descriptor need not
 *     remain allocated for the lifetime of the GMR.
 *
 *     The guest driver should write SVGA_REG_GMR_ID first, then
 *     SVGA_REG_GMR_DESCRIPTOR.
 *
 *   SVGA_REG_GMR_MAX_IDS --
 *
 *     Read-only.
 *     The SVGA device may choose to support a maximum number of
 *     user-defined GMR IDs. This register holds the number of supported
 *     IDs. (The maximum supported ID plus 1)
 *
 *   SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH --
 *
 *     Read-only.
 *     The SVGA device may choose to put a limit on the total number
 *     of SVGAGuestMemDescriptor structures it will read when defining
 *     a single GMR.
 *
 * The descriptor structure is an array of SVGAGuestMemDescriptor
 * structures. Each structure may do one of three things:
 *
 *   - Terminate the GMR descriptor list.
 *     (ppn==0, numPages==0)
 *
 *   - Add a PPN or range of PPNs to the GMR's virtual address space.
 *     (ppn != 0, numPages != 0)
 *
 *   - Provide the PPN of the next SVGAGuestMemDescriptor, in order to
 *     support multi-page GMR descriptor tables without forcing the
 *     driver to allocate physically contiguous memory.
 *     (ppn != 0, numPages == 0)
 *
 * Note that each physical page of SVGAGuestMemDescriptor structures
 * can describe at least 2MB of guest memory. If the driver needs to
 * use more than one page of descriptor structures, it must use one of
 * its SVGAGuestMemDescriptors to point to an additional page.  The
 * device will never automatically cross a page boundary.
 *
 * Once the driver has described a GMR, it is immediately available
 * for use via any FIFO command that uses an SVGAGuestPtr structure.
 * These pointers include a GMR identifier plus an offset into that
 * GMR.
 *
 * The driver must check the SVGA_CAP_GMR bit before using the GMR
 * registers.
 */

#define SVGA_GMR_NULL         ((uint32) -1)
#define SVGA_GMR_FRAMEBUFFER  ((uint32) -2)  

typedef
struct SVGAGuestMemDescriptor {
   uint32 ppn;
   uint32 numPages;
} SVGAGuestMemDescriptor;

typedef
struct SVGAGuestPtr {
   uint32 gmrId;
   uint32 offset;
} SVGAGuestPtr;



typedef
struct SVGAGMRImageFormat {
   union {
      struct {
         uint32 bitsPerPixel : 8;
         uint32 colorDepth   : 8;
         uint32 reserved     : 16;  
      };

      uint32 value;
   };
} SVGAGMRImageFormat;

typedef
struct SVGAGuestImage {
   SVGAGuestPtr         ptr;

   uint32 pitch;
} SVGAGuestImage;


typedef
struct SVGAColorBGRX {
   union {
      struct {
         uint32 b : 8;
         uint32 g : 8;
         uint32 r : 8;
         uint32 x : 8;  
      };

      uint32 value;
   };
} SVGAColorBGRX;



typedef
struct SVGASignedRect {
   int32  left;
   int32  top;
   int32  right;
   int32  bottom;
} SVGASignedRect;

typedef
struct SVGASignedPoint {
   int32  x;
   int32  y;
} SVGASignedPoint;



#define SVGA_CAP_NONE               0x00000000
#define SVGA_CAP_RECT_COPY          0x00000002
#define SVGA_CAP_CURSOR             0x00000020
#define SVGA_CAP_CURSOR_BYPASS      0x00000040   
#define SVGA_CAP_CURSOR_BYPASS_2    0x00000080   
#define SVGA_CAP_8BIT_EMULATION     0x00000100
#define SVGA_CAP_ALPHA_CURSOR       0x00000200
#define SVGA_CAP_3D                 0x00004000
#define SVGA_CAP_EXTENDED_FIFO      0x00008000
#define SVGA_CAP_MULTIMON           0x00010000   
#define SVGA_CAP_PITCHLOCK          0x00020000
#define SVGA_CAP_IRQMASK            0x00040000
#define SVGA_CAP_DISPLAY_TOPOLOGY   0x00080000   
#define SVGA_CAP_GMR                0x00100000
#define SVGA_CAP_TRACES             0x00200000
#define SVGA_CAP_GMR2               0x00400000
#define SVGA_CAP_SCREEN_OBJECT_2    0x00800000


/*
 * FIFO register indices.
 *
 * The FIFO is a chunk of device memory mapped into guest physmem.  It
 * is always treated as 32-bit words.
 *
 * The guest driver gets to decide how to partition it between
 * - FIFO registers (there are always at least 4, specifying where the
 *   following data area is and how much data it contains; there may be
 *   more registers following these, depending on the FIFO protocol
 *   version in use)
 * - FIFO data, written by the guest and slurped out by the VMX.
 * These indices are 32-bit word offsets into the FIFO.
 */

enum {

   SVGA_FIFO_MIN = 0,
   SVGA_FIFO_MAX,       
   SVGA_FIFO_NEXT_CMD,
   SVGA_FIFO_STOP,


   SVGA_FIFO_CAPABILITIES = 4,
   SVGA_FIFO_FLAGS,
   
   SVGA_FIFO_FENCE,


   
   SVGA_FIFO_3D_HWVERSION,       
   
   SVGA_FIFO_PITCHLOCK,

   
   SVGA_FIFO_CURSOR_ON,          
   SVGA_FIFO_CURSOR_X,           
   SVGA_FIFO_CURSOR_Y,           
   SVGA_FIFO_CURSOR_COUNT,       
   SVGA_FIFO_CURSOR_LAST_UPDATED,

   
   SVGA_FIFO_RESERVED,           

   SVGA_FIFO_CURSOR_SCREEN_ID,

   /*
    * Valid with SVGA_FIFO_CAP_DEAD
    *
    * An arbitrary value written by the host, drivers should not use it.
    */
   SVGA_FIFO_DEAD,

   SVGA_FIFO_3D_HWVERSION_REVISED,


   SVGA_FIFO_3D_CAPS      = 32,
   SVGA_FIFO_3D_CAPS_LAST = 32 + 255,


   
   SVGA_FIFO_GUEST_3D_HWVERSION, 
   SVGA_FIFO_FENCE_GOAL,         
   SVGA_FIFO_BUSY,               

    SVGA_FIFO_NUM_REGS
};


#define SVGA_FIFO_EXTENDED_MANDATORY_REGS  (SVGA_FIFO_3D_CAPS_LAST + 1)


/*
 * FIFO Synchronization Registers
 *
 *  This explains the relationship between the various FIFO
 *  sync-related registers in IOSpace and in FIFO space.
 *
 *  SVGA_REG_SYNC --
 *
 *       The SYNC register can be used in two different ways by the guest:
 *
 *         1. If the guest wishes to fully sync (drain) the FIFO,
 *            it will write once to SYNC then poll on the BUSY
 *            register. The FIFO is sync'ed once BUSY is zero.
 *
 *         2. If the guest wants to asynchronously wake up the host,
 *            it will write once to SYNC without polling on BUSY.
 *            Ideally it will do this after some new commands have
 *            been placed in the FIFO, and after reading a zero
 *            from SVGA_FIFO_BUSY.
 *
 *       (1) is the original behaviour that SYNC was designed to
 *       support.  Originally, a write to SYNC would implicitly
 *       trigger a read from BUSY. This causes us to synchronously
 *       process the FIFO.
 *
 *       This behaviour has since been changed so that writing SYNC
 *       will *not* implicitly cause a read from BUSY. Instead, it
 *       makes a channel call which asynchronously wakes up the MKS
 *       thread.
 *
 *       New guests can use this new behaviour to implement (2)
 *       efficiently. This lets guests get the host's attention
 *       without waiting for the MKS to poll, which gives us much
 *       better CPU utilization on SMP hosts and on UP hosts while
 *       we're blocked on the host GPU.
 *
 *       Old guests shouldn't notice the behaviour change. SYNC was
 *       never guaranteed to process the entire FIFO, since it was
 *       bounded to a particular number of CPU cycles. Old guests will
 *       still loop on the BUSY register until the FIFO is empty.
 *
 *       Writing to SYNC currently has the following side-effects:
 *
 *         - Sets SVGA_REG_BUSY to TRUE (in the monitor)
 *         - Asynchronously wakes up the MKS thread for FIFO processing
 *         - The value written to SYNC is recorded as a "reason", for
 *           stats purposes.
 *
 *       If SVGA_FIFO_BUSY is available, drivers are advised to only
 *       write to SYNC if SVGA_FIFO_BUSY is FALSE. Drivers should set
 *       SVGA_FIFO_BUSY to TRUE after writing to SYNC. The MKS will
 *       eventually set SVGA_FIFO_BUSY on its own, but this approach
 *       lets the driver avoid sending multiple asynchronous wakeup
 *       messages to the MKS thread.
 *
 *  SVGA_REG_BUSY --
 *
 *       This register is set to TRUE when SVGA_REG_SYNC is written,
 *       and it reads as FALSE when the FIFO has been completely
 *       drained.
 *
 *       Every read from this register causes us to synchronously
 *       process FIFO commands. There is no guarantee as to how many
 *       commands each read will process.
 *
 *       CPU time spent processing FIFO commands will be billed to
 *       the guest.
 *
 *       New drivers should avoid using this register unless they
 *       need to guarantee that the FIFO is completely drained. It
 *       is overkill for performing a sync-to-fence. Older drivers
 *       will use this register for any type of synchronization.
 *
 *  SVGA_FIFO_BUSY --
 *
 *       This register is a fast way for the guest driver to check
 *       whether the FIFO is already being processed. It reads and
 *       writes at normal RAM speeds, with no monitor intervention.
 *
 *       If this register reads as TRUE, the host is guaranteeing that
 *       any new commands written into the FIFO will be noticed before
 *       the MKS goes back to sleep.
 *
 *       If this register reads as FALSE, no such guarantee can be
 *       made.
 *
 *       The guest should use this register to quickly determine
 *       whether or not it needs to wake up the host. If the guest
 *       just wrote a command or group of commands that it would like
 *       the host to begin processing, it should:
 *
 *         1. Read SVGA_FIFO_BUSY. If it reads as TRUE, no further
 *            action is necessary.
 *
 *         2. Write TRUE to SVGA_FIFO_BUSY. This informs future guest
 *            code that we've already sent a SYNC to the host and we
 *            don't need to send a duplicate.
 *
 *         3. Write a reason to SVGA_REG_SYNC. This will send an
 *            asynchronous wakeup to the MKS thread.
 */


/*
 * FIFO Capabilities
 *
 *      Fence -- Fence register and command are supported
 *      Accel Front -- Front buffer only commands are supported
 *      Pitch Lock -- Pitch lock register is supported
 *      Video -- SVGA Video overlay units are supported
 *      Escape -- Escape command is supported
 *
 * XXX: Add longer descriptions for each capability, including a list
 *      of the new features that each capability provides.
 *
 * SVGA_FIFO_CAP_SCREEN_OBJECT --
 *
 *    Provides dynamic multi-screen rendering, for improved Unity and
 *    multi-monitor modes. With Screen Object, the guest can
 *    dynamically create and destroy 'screens', which can represent
 *    Unity windows or virtual monitors. Screen Object also provides
 *    strong guarantees that DMA operations happen only when
 *    guest-initiated. Screen Object deprecates the BAR1 guest
 *    framebuffer (GFB) and all commands that work only with the GFB.
 *
 *    New registers:
 *       FIFO_CURSOR_SCREEN_ID, VIDEO_DATA_GMRID, VIDEO_DST_SCREEN_ID
 *
 *    New 2D commands:
 *       DEFINE_SCREEN, DESTROY_SCREEN, DEFINE_GMRFB, BLIT_GMRFB_TO_SCREEN,
 *       BLIT_SCREEN_TO_GMRFB, ANNOTATION_FILL, ANNOTATION_COPY
 *
 *    New 3D commands:
 *       BLIT_SURFACE_TO_SCREEN
 *
 *    New guarantees:
 *
 *       - The host will not read or write guest memory, including the GFB,
 *         except when explicitly initiated by a DMA command.
 *
 *       - All DMA, including legacy DMA like UPDATE and PRESENT_READBACK,
 *         is guaranteed to complete before any subsequent FENCEs.
 *
 *       - All legacy commands which affect a Screen (UPDATE, PRESENT,
 *         PRESENT_READBACK) as well as new Screen blit commands will
 *         all behave consistently as blits, and memory will be read
 *         or written in FIFO order.
 *
 *         For example, if you PRESENT from one SVGA3D surface to multiple
 *         places on the screen, the data copied will always be from the
 *         SVGA3D surface at the time the PRESENT was issued in the FIFO.
 *         This was not necessarily true on devices without Screen Object.
 *
 *         This means that on devices that support Screen Object, the
 *         PRESENT_READBACK command should not be necessary unless you
 *         actually want to read back the results of 3D rendering into
 *         system memory. (And for that, the BLIT_SCREEN_TO_GMRFB
 *         command provides a strict superset of functionality.)
 *
 *       - When a screen is resized, either using Screen Object commands or
 *         legacy multimon registers, its contents are preserved.
 *
 * SVGA_FIFO_CAP_GMR2 --
 *
 *    Provides new commands to define and remap guest memory regions (GMR).
 *
 *    New 2D commands:
 *       DEFINE_GMR2, REMAP_GMR2.
 *
 * SVGA_FIFO_CAP_3D_HWVERSION_REVISED --
 *
 *    Indicates new register SVGA_FIFO_3D_HWVERSION_REVISED exists.
 *    This register may replace SVGA_FIFO_3D_HWVERSION on platforms
 *    that enforce graphics resource limits.  This allows the platform
 *    to clear SVGA_FIFO_3D_HWVERSION and disable 3D in legacy guest
 *    drivers that do not limit their resources.
 *
 *    Note this is an alias to SVGA_FIFO_CAP_GMR2 because these indicators
 *    are codependent (and thus we use a single capability bit).
 *
 * SVGA_FIFO_CAP_SCREEN_OBJECT_2 --
 *
 *    Modifies the DEFINE_SCREEN command to include a guest provided
 *    backing store in GMR memory and the bytesPerLine for the backing
 *    store.  This capability requires the use of a backing store when
 *    creating screen objects.  However if SVGA_FIFO_CAP_SCREEN_OBJECT
 *    is present then backing stores are optional.
 *
 * SVGA_FIFO_CAP_DEAD --
 *
 *    Drivers should not use this cap bit.  This cap bit can not be
 *    reused since some hosts already expose it.
 */

#define SVGA_FIFO_CAP_NONE                  0
#define SVGA_FIFO_CAP_FENCE             (1<<0)
#define SVGA_FIFO_CAP_ACCELFRONT        (1<<1)
#define SVGA_FIFO_CAP_PITCHLOCK         (1<<2)
#define SVGA_FIFO_CAP_VIDEO             (1<<3)
#define SVGA_FIFO_CAP_CURSOR_BYPASS_3   (1<<4)
#define SVGA_FIFO_CAP_ESCAPE            (1<<5)
#define SVGA_FIFO_CAP_RESERVE           (1<<6)
#define SVGA_FIFO_CAP_SCREEN_OBJECT     (1<<7)
#define SVGA_FIFO_CAP_GMR2              (1<<8)
#define SVGA_FIFO_CAP_3D_HWVERSION_REVISED  SVGA_FIFO_CAP_GMR2
#define SVGA_FIFO_CAP_SCREEN_OBJECT_2   (1<<9)
#define SVGA_FIFO_CAP_DEAD              (1<<10)



#define SVGA_FIFO_FLAG_NONE                 0
#define SVGA_FIFO_FLAG_ACCELFRONT       (1<<0)
#define SVGA_FIFO_FLAG_RESERVED        (1<<31) 


#define SVGA_FIFO_RESERVED_UNKNOWN      0xffffffff



#define SVGA_NUM_OVERLAY_UNITS 32



#define SVGA_VIDEO_FLAG_COLORKEY        0x0001



enum {
   SVGA_VIDEO_ENABLED = 0,
   SVGA_VIDEO_FLAGS,
   SVGA_VIDEO_DATA_OFFSET,
   SVGA_VIDEO_FORMAT,
   SVGA_VIDEO_COLORKEY,
   SVGA_VIDEO_SIZE,          
   SVGA_VIDEO_WIDTH,
   SVGA_VIDEO_HEIGHT,
   SVGA_VIDEO_SRC_X,
   SVGA_VIDEO_SRC_Y,
   SVGA_VIDEO_SRC_WIDTH,
   SVGA_VIDEO_SRC_HEIGHT,
   SVGA_VIDEO_DST_X,         
   SVGA_VIDEO_DST_Y,         
   SVGA_VIDEO_DST_WIDTH,
   SVGA_VIDEO_DST_HEIGHT,
   SVGA_VIDEO_PITCH_1,
   SVGA_VIDEO_PITCH_2,
   SVGA_VIDEO_PITCH_3,
   SVGA_VIDEO_DATA_GMRID,    
   SVGA_VIDEO_DST_SCREEN_ID, 
   SVGA_VIDEO_NUM_REGS
};



typedef struct SVGAOverlayUnit {
   uint32 enabled;
   uint32 flags;
   uint32 dataOffset;
   uint32 format;
   uint32 colorKey;
   uint32 size;
   uint32 width;
   uint32 height;
   uint32 srcX;
   uint32 srcY;
   uint32 srcWidth;
   uint32 srcHeight;
   int32  dstX;
   int32  dstY;
   uint32 dstWidth;
   uint32 dstHeight;
   uint32 pitches[3];
   uint32 dataGMRId;
   uint32 dstScreenId;
} SVGAOverlayUnit;



#define SVGA_SCREEN_MUST_BE_SET     (1 << 0) 
#define SVGA_SCREEN_HAS_ROOT SVGA_SCREEN_MUST_BE_SET 
#define SVGA_SCREEN_IS_PRIMARY      (1 << 1) 
#define SVGA_SCREEN_FULLSCREEN_HINT (1 << 2) 

#define SVGA_SCREEN_DEACTIVATE  (1 << 3)

/*
 * Added with SVGA_FIFO_CAP_SCREEN_OBJECT_2.  When this flag is set
 * the screen contents will be outputted as all black to the user
 * though the base layer contents is preserved.  The screen base layer
 * can still be read and written to like normal though the no visible
 * effect will be seen by the user.  When the flag is changed the
 * screen will be blanked or redrawn to the current contents as needed
 * without any extra commands from the driver.  This flag only has an
 * effect when the screen is not deactivated.
 */
#define SVGA_SCREEN_BLANKING (1 << 4)

typedef
struct SVGAScreenObject {
   uint32 structSize;   
   uint32 id;
   uint32 flags;
   struct {
      uint32 width;
      uint32 height;
   } size;
   struct {
      int32 x;
      int32 y;
   } root;

   SVGAGuestImage backingStore;
   uint32 cloneCount;
} SVGAScreenObject;



typedef enum {
   SVGA_CMD_INVALID_CMD           = 0,
   SVGA_CMD_UPDATE                = 1,
   SVGA_CMD_RECT_COPY             = 3,
   SVGA_CMD_DEFINE_CURSOR         = 19,
   SVGA_CMD_DEFINE_ALPHA_CURSOR   = 22,
   SVGA_CMD_UPDATE_VERBOSE        = 25,
   SVGA_CMD_FRONT_ROP_FILL        = 29,
   SVGA_CMD_FENCE                 = 30,
   SVGA_CMD_ESCAPE                = 33,
   SVGA_CMD_DEFINE_SCREEN         = 34,
   SVGA_CMD_DESTROY_SCREEN        = 35,
   SVGA_CMD_DEFINE_GMRFB          = 36,
   SVGA_CMD_BLIT_GMRFB_TO_SCREEN  = 37,
   SVGA_CMD_BLIT_SCREEN_TO_GMRFB  = 38,
   SVGA_CMD_ANNOTATION_FILL       = 39,
   SVGA_CMD_ANNOTATION_COPY       = 40,
   SVGA_CMD_DEFINE_GMR2           = 41,
   SVGA_CMD_REMAP_GMR2            = 42,
   SVGA_CMD_MAX
} SVGAFifoCmdId;

#define SVGA_CMD_MAX_ARGS           64



typedef
struct SVGAFifoCmdUpdate {
   uint32 x;
   uint32 y;
   uint32 width;
   uint32 height;
} SVGAFifoCmdUpdate;



typedef
struct SVGAFifoCmdRectCopy {
   uint32 srcX;
   uint32 srcY;
   uint32 destX;
   uint32 destY;
   uint32 width;
   uint32 height;
} SVGAFifoCmdRectCopy;



typedef
struct SVGAFifoCmdDefineCursor {
   uint32 id;             
   uint32 hotspotX;
   uint32 hotspotY;
   uint32 width;
   uint32 height;
   uint32 andMaskDepth;   
   uint32 xorMaskDepth;   
} SVGAFifoCmdDefineCursor;



typedef
struct SVGAFifoCmdDefineAlphaCursor {
   uint32 id;             
   uint32 hotspotX;
   uint32 hotspotY;
   uint32 width;
   uint32 height;
   
} SVGAFifoCmdDefineAlphaCursor;



typedef
struct SVGAFifoCmdUpdateVerbose {
   uint32 x;
   uint32 y;
   uint32 width;
   uint32 height;
   uint32 reason;
} SVGAFifoCmdUpdateVerbose;



#define  SVGA_ROP_COPY                    0x03

typedef
struct SVGAFifoCmdFrontRopFill {
   uint32 color;     
   uint32 x;
   uint32 y;
   uint32 width;
   uint32 height;
   uint32 rop;       
} SVGAFifoCmdFrontRopFill;



typedef
struct {
   uint32 fence;
} SVGAFifoCmdFence;



typedef
struct SVGAFifoCmdEscape {
   uint32 nsid;
   uint32 size;
   
} SVGAFifoCmdEscape;



typedef
struct {
   SVGAScreenObject screen;   
} SVGAFifoCmdDefineScreen;



typedef
struct {
   uint32 screenId;
} SVGAFifoCmdDestroyScreen;



typedef
struct {
   SVGAGuestPtr        ptr;
   uint32              bytesPerLine;
   SVGAGMRImageFormat  format;
} SVGAFifoCmdDefineGMRFB;



typedef
struct {
   SVGASignedPoint  srcOrigin;
   SVGASignedRect   destRect;
   uint32           destScreenId;
} SVGAFifoCmdBlitGMRFBToScreen;



typedef
struct {
   SVGASignedPoint  destOrigin;
   SVGASignedRect   srcRect;
   uint32           srcScreenId;
} SVGAFifoCmdBlitScreenToGMRFB;



typedef
struct {
   SVGAColorBGRX  color;
} SVGAFifoCmdAnnotationFill;



typedef
struct {
   SVGASignedPoint  srcOrigin;
   uint32           srcScreenId;
} SVGAFifoCmdAnnotationCopy;



typedef
struct {
   uint32 gmrId;
   uint32 numPages;
} SVGAFifoCmdDefineGMR2;



typedef enum {
   SVGA_REMAP_GMR2_PPN32         = 0,
   SVGA_REMAP_GMR2_VIA_GMR       = (1 << 0),
   SVGA_REMAP_GMR2_PPN64         = (1 << 1),
   SVGA_REMAP_GMR2_SINGLE_PPN    = (1 << 2),
} SVGARemapGMR2Flags;

typedef
struct {
   uint32 gmrId;
   SVGARemapGMR2Flags flags;
   uint32 offsetPages; 
   uint32 numPages; 
} SVGAFifoCmdRemapGMR2;

#endif
