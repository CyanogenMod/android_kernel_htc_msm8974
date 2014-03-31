/*
 *   fs/cifs/smbfsctl.h: SMB, CIFS, SMB2 FSCTL definitions
 *
 *   Copyright (c) International Business Machines  Corp., 2002,2009
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define FSCTL_REQUEST_OPLOCK_LEVEL_1 0x00090000
#define FSCTL_REQUEST_OPLOCK_LEVEL_2 0x00090004
#define FSCTL_REQUEST_BATCH_OPLOCK   0x00090008
#define FSCTL_LOCK_VOLUME            0x00090018
#define FSCTL_UNLOCK_VOLUME          0x0009001C
#define FSCTL_IS_PATHNAME_VALID      0x0009002C 
#define FSCTL_GET_COMPRESSION        0x0009003C 
#define FSCTL_SET_COMPRESSION        0x0009C040 
#define FSCTL_QUERY_FAT_BPB          0x00090058 
#define FSCTL_FILESYSTEM_GET_STATS   0x00090060 
#define FSCTL_GET_NTFS_VOLUME_DATA   0x00090064 
#define FSCTL_GET_RETRIEVAL_POINTERS 0x00090073 
#define FSCTL_IS_VOLUME_DIRTY        0x00090078 
#define FSCTL_ALLOW_EXTENDED_DASD_IO 0x00090083 
#define FSCTL_REQUEST_FILTER_OPLOCK  0x0009008C
#define FSCTL_FIND_FILES_BY_SID      0x0009008F 
#define FSCTL_SET_OBJECT_ID          0x00090098 
#define FSCTL_GET_OBJECT_ID          0x0009009C 
#define FSCTL_DELETE_OBJECT_ID       0x000900A0 
#define FSCTL_SET_REPARSE_POINT      0x000900A4 
#define FSCTL_GET_REPARSE_POINT      0x000900A8 
#define FSCTL_DELETE_REPARSE_POINT   0x000900AC 
#define FSCTL_SET_OBJECT_ID_EXTENDED 0x000900BC 
#define FSCTL_CREATE_OR_GET_OBJECT_ID 0x000900C0 
#define FSCTL_SET_SPARSE             0x000900C4 
#define FSCTL_SET_ZERO_DATA          0x000900C8 
#define FSCTL_SET_ENCRYPTION         0x000900D7 
#define FSCTL_ENCRYPTION_FSCTL_IO    0x000900DB 
#define FSCTL_WRITE_RAW_ENCRYPTED    0x000900DF 
#define FSCTL_READ_RAW_ENCRYPTED     0x000900E3 
#define FSCTL_READ_FILE_USN_DATA     0x000900EB 
#define FSCTL_WRITE_USN_CLOSE_RECORD 0x000900EF 
#define FSCTL_SIS_COPYFILE           0x00090100 
#define FSCTL_RECALL_FILE            0x00090117 
#define FSCTL_QUERY_SPARING_INFO     0x00090138 
#define FSCTL_SET_ZERO_ON_DEALLOC    0x00090194 
#define FSCTL_SET_SHORT_NAME_BEHAVIOR 0x000901B4 
#define FSCTL_QUERY_ALLOCATED_RANGES 0x000940CF 
#define FSCTL_SET_DEFECT_MANAGEMENT  0x00098134 
#define FSCTL_SIS_LINK_FILES         0x0009C104
#define FSCTL_PIPE_PEEK              0x0011400C 
#define FSCTL_PIPE_TRANSCEIVE        0x0011C017 
#define FSCTL_PIPE_WAIT              0x00110018 
#define FSCTL_LMR_GET_LINK_TRACK_INF 0x001400E8 
#define FSCTL_LMR_SET_LINK_TRACK_INF 0x001400EC 

#define IO_REPARSE_TAG_MOUNT_POINT   0xA0000003
#define IO_REPARSE_TAG_HSM           0xC0000004
#define IO_REPARSE_TAG_SIS           0x80000007
