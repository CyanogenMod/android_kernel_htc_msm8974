#ifndef __LINUX_FBIO_H
#define __LINUX_FBIO_H

#include <linux/compiler.h>
#include <linux/types.h>


#define FBTYPE_NOTYPE           -1
#define FBTYPE_SUN1BW           0   
#define FBTYPE_SUN1COLOR        1 
#define FBTYPE_SUN2BW           2 
#define FBTYPE_SUN2COLOR        3 
#define FBTYPE_SUN2GP           4 
#define FBTYPE_SUN5COLOR        5 
#define FBTYPE_SUN3COLOR        6 
#define FBTYPE_MEMCOLOR         7 
#define FBTYPE_SUN4COLOR        8 
 
#define FBTYPE_NOTSUN1          9 
#define FBTYPE_NOTSUN2          10
#define FBTYPE_NOTSUN3          11
 
#define FBTYPE_SUNFAST_COLOR    12  
#define FBTYPE_SUNROP_COLOR     13
#define FBTYPE_SUNFB_VIDEO      14
#define FBTYPE_SUNGIFB          15
#define FBTYPE_SUNGPLAS         16
#define FBTYPE_SUNGP3           17
#define FBTYPE_SUNGT            18
#define FBTYPE_SUNLEO           19      
#define FBTYPE_MDICOLOR         20      
#define FBTYPE_TCXCOLOR		21	

#define FBTYPE_LASTPLUSONE      21	

#define FBTYPE_CREATOR          22
#define FBTYPE_PCI_IGA1682	23
#define FBTYPE_P9100COLOR	24

#define FBTYPE_PCI_GENERIC	1000
#define FBTYPE_PCI_MACH64	1001

struct  fbtype {
        int     fb_type;        
        int     fb_height;      
        int     fb_width;       
        int     fb_depth;
        int     fb_cmsize;      
        int     fb_size;        
};
#define FBIOGTYPE _IOR('F', 0, struct fbtype)

struct  fbcmap {
        int             index;          
        int             count;
        unsigned char   __user *red;
        unsigned char   __user *green;
        unsigned char   __user *blue;
};

#ifdef __KERNEL__
#define FBIOPUTCMAP_SPARC _IOW('F', 3, struct fbcmap)
#define FBIOGETCMAP_SPARC _IOW('F', 4, struct fbcmap)
#else
#define FBIOPUTCMAP _IOW('F', 3, struct fbcmap)
#define FBIOGETCMAP _IOW('F', 4, struct fbcmap)
#endif

#define FB_ATTR_NDEVSPECIFIC    8
#define FB_ATTR_NEMUTYPES       4
 
struct fbsattr {
        int     flags;
        int     emu_type;	
        int     dev_specific[FB_ATTR_NDEVSPECIFIC];
};
 
struct fbgattr {
        int     real_type;	
        int     owner;		
        struct fbtype fbtype;	
        struct fbsattr sattr;   
        int     emu_types[FB_ATTR_NEMUTYPES]; 
};
#define FBIOSATTR  _IOW('F', 5, struct fbgattr) 
#define FBIOGATTR  _IOR('F', 6, struct fbgattr)	

#define FBIOSVIDEO _IOW('F', 7, int)
#define FBIOGVIDEO _IOR('F', 8, int)

struct fbcursor {
        short set;              
        short enable;           
        struct fbcurpos pos;    
        struct fbcurpos hot;    
        struct fbcmap cmap;     
        struct fbcurpos size;   
        char __user *image;     
        char __user *mask;      
};

#define FBIOSCURSOR     _IOW('F', 24, struct fbcursor)
#define FBIOGCURSOR     _IOWR('F', 25, struct fbcursor)
 
#define FBIOSCURPOS     _IOW('F', 26, struct fbcurpos)
#define FBIOGCURPOS     _IOW('F', 27, struct fbcurpos)
 
#define FBIOGCURMAX     _IOR('F', 28, struct fbcurpos)

struct fb_wid_alloc {
#define FB_WID_SHARED_8		0
#define FB_WID_SHARED_24	1
#define FB_WID_DBL_8		2
#define FB_WID_DBL_24		3
	__u32	wa_type;
	__s32	wa_index;	
	__u32	wa_count;	
};
struct fb_wid_item {
	__u32	wi_type;
	__s32	wi_index;
	__u32	wi_attrs;
	__u32	wi_values[32];
};
struct fb_wid_list {
	__u32	wl_flags;
	__u32	wl_count;
	struct fb_wid_item	*wl_list;
};

#define FBIO_WID_ALLOC	_IOWR('F', 30, struct fb_wid_alloc)
#define FBIO_WID_FREE	_IOW('F', 31, struct fb_wid_alloc)
#define FBIO_WID_PUT	_IOW('F', 32, struct fb_wid_list)
#define FBIO_WID_GET	_IOWR('F', 33, struct fb_wid_list)

#define FFB_IOCTL	('F'<<8)
#define FFB_SYS_INFO		(FFB_IOCTL|80)
#define FFB_CLUTREAD		(FFB_IOCTL|81)
#define FFB_CLUTPOST		(FFB_IOCTL|82)
#define FFB_SETDIAGMODE		(FFB_IOCTL|83)
#define FFB_GETMONITORID	(FFB_IOCTL|84)
#define FFB_GETVIDEOMODE	(FFB_IOCTL|85)
#define FFB_SETVIDEOMODE	(FFB_IOCTL|86)
#define FFB_SETSERVER		(FFB_IOCTL|87)
#define FFB_SETOVCTL		(FFB_IOCTL|88)
#define FFB_GETOVCTL		(FFB_IOCTL|89)
#define FFB_GETSAXNUM		(FFB_IOCTL|90)
#define FFB_FBDEBUG		(FFB_IOCTL|91)

#define MDI_IOCTL          ('M'<<8)
#define MDI_RESET          (MDI_IOCTL|1)
#define MDI_GET_CFGINFO    (MDI_IOCTL|2)
#define MDI_SET_PIXELMODE  (MDI_IOCTL|3)
#    define MDI_32_PIX     32
#    define MDI_16_PIX     16
#    define MDI_8_PIX      8

struct mdi_cfginfo {
	int     mdi_ncluts;     
        int     mdi_type;       
        int     mdi_height;     
        int     mdi_width;      
        int     mdi_size;       
        int     mdi_mode;       
        int     mdi_pixfreq;    
};

#define MDI_CLEAR_XLUT       (MDI_IOCTL|9)

struct fb_clut_alloc {
	__u32	clutid;	
 	__u32	flag;
 	__u32	index;
};

struct fb_clut {
#define FB_CLUT_WAIT	0x00000001	
 	__u32	flag;
 	__u32	clutid;
 	__u32	offset;
 	__u32	count;
 	char *	red;
 	char *	green;
 	char *	blue;
};

struct fb_clut32 {
 	__u32	flag;
 	__u32	clutid;
 	__u32	offset;
 	__u32	count;
 	__u32	red;
 	__u32	green;
 	__u32	blue;
};

#define LEO_CLUTALLOC	_IOWR('L', 53, struct fb_clut_alloc)
#define LEO_CLUTFREE	_IOW('L', 54, struct fb_clut_alloc)
#define LEO_CLUTREAD	_IOW('L', 55, struct fb_clut)
#define LEO_CLUTPOST	_IOW('L', 56, struct fb_clut)
#define LEO_SETGAMMA	_IOW('L', 68, int) 
#define LEO_GETGAMMA	_IOR('L', 69, int) 

#ifdef __KERNEL__
#define CG6_FBC    0x70000000
#define CG6_TEC    0x70001000
#define CG6_BTREGS 0x70002000
#define CG6_FHC    0x70004000
#define CG6_THC    0x70005000
#define CG6_ROM    0x70006000
#define CG6_RAM    0x70016000
#define CG6_DHC    0x80000000

#define CG3_MMAP_OFFSET 0x4000000

#define TCX_RAM8BIT   		0x00000000
#define TCX_RAM24BIT   		0x01000000
#define TCX_UNK3   		0x10000000
#define TCX_UNK4   		0x20000000
#define TCX_CONTROLPLANE   	0x28000000
#define TCX_UNK6   		0x30000000
#define TCX_UNK7   		0x38000000
#define TCX_TEC    		0x70000000
#define TCX_BTREGS 		0x70002000
#define TCX_THC    		0x70004000
#define TCX_DHC    		0x70008000
#define TCX_ALT	   		0x7000a000
#define TCX_SYNC   		0x7000e000
#define TCX_UNK2    		0x70010000


#define CG14_REGS        0       
#define CG14_CURSORREGS  0x1000  
#define CG14_DACREGS     0x2000  
#define CG14_XLUT        0x3000  
#define CG14_CLUT1       0x4000  
#define CG14_CLUT2       0x5000  
#define CG14_CLUT3       0x6000  
#define CG14_AUTO	 0xf000

#endif 

#define MDI_DIRECT_MAP 0x10000000
#define MDI_CTLREG_MAP 0x20000000
#define MDI_CURSOR_MAP 0x30000000
#define MDI_SHDW_VRT_MAP 0x40000000

#define MDI_CHUNKY_XBGR_MAP 0x50000000
#define MDI_CHUNKY_BGR_MAP 0x60000000

#define MDI_PLANAR_X16_MAP 0x70000000
#define MDI_PLANAR_C16_MAP 0x80000000

#define MDI_PLANAR_X32_MAP 0x90000000
#define MDI_PLANAR_B32_MAP 0xa0000000
#define MDI_PLANAR_G32_MAP 0xb0000000
#define MDI_PLANAR_R32_MAP 0xc0000000

#define LEO_SS0_MAP            0x00000000
#define LEO_LC_SS0_USR_MAP     0x00800000
#define LEO_LD_SS0_MAP         0x00801000
#define LEO_LX_CURSOR_MAP      0x00802000
#define LEO_SS1_MAP            0x00803000
#define LEO_LC_SS1_USR_MAP     0x01003000
#define LEO_LD_SS1_MAP         0x01004000
#define LEO_UNK_MAP            0x01005000
#define LEO_LX_KRN_MAP         0x01006000
#define LEO_LC_SS0_KRN_MAP     0x01007000
#define LEO_LC_SS1_KRN_MAP     0x01008000
#define LEO_LD_GBL_MAP         0x01009000
#define LEO_UNK2_MAP           0x0100a000

#ifdef __KERNEL__
struct  fbcmap32 {
	int             index;          
	int             count;
	u32		red;
	u32		green;
	u32		blue;
};

#define FBIOPUTCMAP32	_IOW('F', 3, struct fbcmap32)
#define FBIOGETCMAP32	_IOW('F', 4, struct fbcmap32)

struct fbcursor32 {
	short set;		
	short enable;		
	struct fbcurpos pos;	
	struct fbcurpos hot;	
	struct fbcmap32 cmap;	
	struct fbcurpos size;	
	u32	image;		
	u32	mask;		
};

#define FBIOSCURSOR32	_IOW('F', 24, struct fbcursor32)
#define FBIOGCURSOR32	_IOW('F', 25, struct fbcursor32)
#endif

#endif 
