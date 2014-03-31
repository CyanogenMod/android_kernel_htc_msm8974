/*
 * include/asm-s390/vtoc.h
 *
 * This file contains volume label definitions for DASD devices.
 *
 * (C) Copyright IBM Corp. 2005
 *
 * Author(s): Volker Sameske <sameske@de.ibm.com>
 *
 */

#ifndef _ASM_S390_VTOC_H
#define _ASM_S390_VTOC_H

#include <linux/types.h>

struct vtoc_ttr
{
	__u16 tt;
	__u8 r;
} __attribute__ ((packed));

struct vtoc_cchhb
{
	__u16 cc;
	__u16 hh;
	__u8 b;
} __attribute__ ((packed));

struct vtoc_cchh
{
	__u16 cc;
	__u16 hh;
} __attribute__ ((packed));

struct vtoc_labeldate
{
	__u8 year;
	__u16 day;
} __attribute__ ((packed));

struct vtoc_volume_label_cdl
{
	char volkey[4];		
	char vollbl[4];		
	char volid[6];		
	__u8 security;		
	struct vtoc_cchhb vtoc;	
	char res1[5];		
	char cisize[4];		
				
	char blkperci[4];	
	char labperci[4];	
	char res2[4];		
	char lvtoc[14];		
	char res3[29];		
} __attribute__ ((packed));

struct vtoc_volume_label_ldl {
	char vollbl[4];		
	char volid[6];		
	char res3[69];		
	char ldl_version;	
	__u64 formatted_blocks; 
} __attribute__ ((packed));

struct vtoc_extent
{
	__u8 typeind;			
	__u8 seqno;			
	struct vtoc_cchh llimit;	
	struct vtoc_cchh ulimit;	
} __attribute__ ((packed));

struct vtoc_dev_const
{
	__u16 DS4DSCYL;	
	__u16 DS4DSTRK;	
	__u16 DS4DEVTK;	
	__u8 DS4DEVI;	
	__u8 DS4DEVL;	
	__u8 DS4DEVK;	
	__u8 DS4DEVFG;	
	__u16 DS4DEVTL;	
	__u8 DS4DEVDT;	
	__u8 DS4DEVDB;	
} __attribute__ ((packed));

struct vtoc_format1_label
{
	char DS1DSNAM[44];	
	__u8 DS1FMTID;		
	char DS1DSSN[6];	
	__u16 DS1VOLSQ;		
	struct vtoc_labeldate DS1CREDT; 
	struct vtoc_labeldate DS1EXPDT; 
	__u8 DS1NOEPV;		
	__u8 DS1NOBDB;		
	__u8 DS1FLAG1;		
	char DS1SYSCD[13];	
	struct vtoc_labeldate DS1REFD; 
	__u8 DS1SMSFG;		
	__u8 DS1SCXTF;		
	__u16 DS1SCXTV;		
	__u8 DS1DSRG1;		
	__u8 DS1DSRG2;		
	__u8 DS1RECFM;		
	__u8 DS1OPTCD;		
	__u16 DS1BLKL;		
	__u16 DS1LRECL;		
	__u8 DS1KEYL;		
	__u16 DS1RKP;		
	__u8 DS1DSIND;		
	__u8 DS1SCAL1;		
	char DS1SCAL3[3];	
	struct vtoc_ttr DS1LSTAR; 
	__u16 DS1TRBAL;		
	__u16 res1;		
	struct vtoc_extent DS1EXT1; 
	struct vtoc_extent DS1EXT2; 
	struct vtoc_extent DS1EXT3; 
	struct vtoc_cchhb DS1PTRDS; 
} __attribute__ ((packed));

struct vtoc_format4_label
{
	char DS4KEYCD[44];	
	__u8 DS4IDFMT;		
	struct vtoc_cchhb DS4HPCHR; 
	__u16 DS4DSREC;		
	struct vtoc_cchh DS4HCCHH; 
	__u16 DS4NOATK;		
	__u8 DS4VTOCI;		
	__u8 DS4NOEXT;		
	__u8 DS4SMSFG;		
	__u8 DS4DEVAC;		
	struct vtoc_dev_const DS4DEVCT;	
	char DS4AMTIM[8];	
	char DS4AMCAT[3];	
	char DS4R2TIM[8];	
	char res1[5];		
	char DS4F6PTR[5];	
	struct vtoc_extent DS4VTOCE; 
	char res2[10];		
	__u8 DS4EFLVL;		
	struct vtoc_cchhb DS4EFPTR; 
	char res3;		
	__u32 DS4DCYL;		
	char res4[2];		
	__u8 DS4DEVF2;		
	char res5;		
} __attribute__ ((packed));

struct vtoc_ds5ext
{
	__u16 t;	
	__u16 fc;	
	__u8 ft;	
} __attribute__ ((packed));

struct vtoc_format5_label
{
	char DS5KEYID[4];	
	struct vtoc_ds5ext DS5AVEXT; 
	struct vtoc_ds5ext DS5EXTAV[7]; 
	__u8 DS5FMTID;		
	struct vtoc_ds5ext DS5MAVET[18]; 
	struct vtoc_cchhb DS5PTRDS; 
} __attribute__ ((packed));

struct vtoc_ds7ext
{
	__u32 a; 
	__u32 b; 
} __attribute__ ((packed));

struct vtoc_format7_label
{
	char DS7KEYID[4];	
	struct vtoc_ds7ext DS7EXTNT[5]; 
	__u8 DS7FMTID;		
	struct vtoc_ds7ext DS7ADEXT[11]; 
	char res1[2];		
	struct vtoc_cchhb DS7PTRDS; 
} __attribute__ ((packed));

struct vtoc_cms_label {
	__u8 label_id[4];		
	__u8 vol_id[6];		
	__u16 version_id;		
	__u32 block_size;		
	__u32 origin_ptr;		
	__u32 usable_count;	
	__u32 formatted_count;	
	__u32 block_count;	
	__u32 used_count;		
	__u32 fst_size;		
	__u32 fst_count;		
	__u8 format_date[6];	
	__u8 reserved1[2];
	__u32 disk_offset;	
	__u32 map_block;		
	__u32 hblk_disp;		
	__u32 user_disp;		
	__u8 reserved2[4];
	__u8 segment_name[8];	
} __attribute__ ((packed));

#endif 
