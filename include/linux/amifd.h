#ifndef _AMIFD_H
#define _AMIFD_H


#include <linux/fd.h>

#define FD_MAX_UNITS    4	
#define FLOPPY_MAX_SECTORS	22	

#ifndef ASSEMBLER

struct fd_data_type {
    char *name;			
    int sects;			
#ifdef __STDC__
    int (*read_fkt)(int);
    void (*write_fkt)(int);
#else
    int (*read_fkt)();		
    void (*write_fkt)();		
#endif
};


struct fd_drive_type {
    unsigned long code;		
    char *name;			
    unsigned int tracks;	
    unsigned int heads;		
    unsigned int read_size;	
    unsigned int write_size;	
    unsigned int sect_mult;	
    unsigned int precomp1;	
    unsigned int precomp2;	
    unsigned int step_delay;	
    unsigned int settle_time;	
    unsigned int side_time;	
};

struct amiga_floppy_struct {
    struct fd_drive_type *type;	
    struct fd_data_type *dtype;	
    int track;			
    unsigned char *trackbuf;    

    int blocks;			

    int changed;		
    int disk;			
    int motor;			
    int busy;			
    int dirty;			
    int status;			
    struct gendisk *gendisk;
};
#endif

#endif
