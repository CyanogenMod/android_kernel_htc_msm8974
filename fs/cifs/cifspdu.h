/*
 *   fs/cifs/cifspdu.h
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

#ifndef _CIFSPDU_H
#define _CIFSPDU_H

#include <net/sock.h>
#include <asm/unaligned.h>
#include "smbfsctl.h"

#ifdef CONFIG_CIFS_WEAK_PW_HASH
#define LANMAN_PROT 0
#define LANMAN2_PROT 1
#define CIFS_PROT   2
#else
#define CIFS_PROT   0
#endif
#define POSIX_PROT  (CIFS_PROT+1)
#define BAD_PROT 0xFFFF

#define SMB_COM_CREATE_DIRECTORY      0x00 
#define SMB_COM_DELETE_DIRECTORY      0x01 
#define SMB_COM_CLOSE                 0x04 
#define SMB_COM_FLUSH                 0x05 
#define SMB_COM_DELETE                0x06 
#define SMB_COM_RENAME                0x07 
#define SMB_COM_QUERY_INFORMATION     0x08 
#define SMB_COM_SETATTR               0x09 
#define SMB_COM_LOCKING_ANDX          0x24 
#define SMB_COM_COPY                  0x29 
#define SMB_COM_ECHO                  0x2B 
#define SMB_COM_OPEN_ANDX             0x2D 
#define SMB_COM_READ_ANDX             0x2E
#define SMB_COM_WRITE_ANDX            0x2F
#define SMB_COM_TRANSACTION2          0x32
#define SMB_COM_TRANSACTION2_SECONDARY 0x33
#define SMB_COM_FIND_CLOSE2           0x34 
#define SMB_COM_TREE_DISCONNECT       0x71 
#define SMB_COM_NEGOTIATE             0x72
#define SMB_COM_SESSION_SETUP_ANDX    0x73
#define SMB_COM_LOGOFF_ANDX           0x74 
#define SMB_COM_TREE_CONNECT_ANDX     0x75
#define SMB_COM_NT_TRANSACT           0xA0
#define SMB_COM_NT_TRANSACT_SECONDARY 0xA1
#define SMB_COM_NT_CREATE_ANDX        0xA2
#define SMB_COM_NT_CANCEL             0xA4 
#define SMB_COM_NT_RENAME             0xA5 

#define TRANS2_OPEN                   0x00
#define TRANS2_FIND_FIRST             0x01
#define TRANS2_FIND_NEXT              0x02
#define TRANS2_QUERY_FS_INFORMATION   0x03
#define TRANS2_SET_FS_INFORMATION     0x04
#define TRANS2_QUERY_PATH_INFORMATION 0x05
#define TRANS2_SET_PATH_INFORMATION   0x06
#define TRANS2_QUERY_FILE_INFORMATION 0x07
#define TRANS2_SET_FILE_INFORMATION   0x08
#define TRANS2_GET_DFS_REFERRAL       0x10
#define TRANS2_REPORT_DFS_INCOSISTENCY 0x11

#define TRANS_SET_NMPIPE_STATE      0x0001
#define TRANS_RAW_READ_NMPIPE       0x0011
#define TRANS_QUERY_NMPIPE_STATE    0x0021
#define TRANS_QUERY_NMPIPE_INFO     0x0022
#define TRANS_PEEK_NMPIPE           0x0023
#define TRANS_TRANSACT_NMPIPE       0x0026
#define TRANS_RAW_WRITE_NMPIPE      0x0031
#define TRANS_READ_NMPIPE           0x0036
#define TRANS_WRITE_NMPIPE          0x0037
#define TRANS_WAIT_NMPIPE           0x0053
#define TRANS_CALL_NMPIPE           0x0054

#define NT_TRANSACT_CREATE            0x01
#define NT_TRANSACT_IOCTL             0x02
#define NT_TRANSACT_SET_SECURITY_DESC 0x03
#define NT_TRANSACT_NOTIFY_CHANGE     0x04
#define NT_TRANSACT_RENAME            0x05
#define NT_TRANSACT_QUERY_SECURITY_DESC 0x06
#define NT_TRANSACT_GET_USER_QUOTA    0x07
#define NT_TRANSACT_SET_USER_QUOTA    0x08

#define MAX_CIFS_SMALL_BUFFER_SIZE 448 
#define MAX_CIFS_HDR_SIZE 0x58 
#define CIFS_SMALL_PATH 120 


#define CIFS_MAX_MSGSIZE (4*4096)

#define CIFS_ENCPWD_SIZE (16)

#define CIFS_CRYPTO_KEY_SIZE (8)

#define CIFS_AUTH_RESP_SIZE (24)

#define CIFS_SESS_KEY_SIZE (16)

#define CIFS_CLIENT_CHALLENGE_SIZE (8)
#define CIFS_SERVER_CHALLENGE_SIZE (8)
#define CIFS_HMAC_MD5_HASH_SIZE (16)
#define CIFS_CPHTXT_SIZE (16)
#define CIFS_NTHASH_SIZE (16)

#define CIFS_UNLEN (20)

#define SMBOPEN_WRITE_THROUGH 0x4000
#define SMBOPEN_DENY_ALL      0x0010
#define SMBOPEN_DENY_WRITE    0x0020
#define SMBOPEN_DENY_READ     0x0030
#define SMBOPEN_DENY_NONE     0x0040
#define SMBOPEN_READ          0x0000
#define SMBOPEN_WRITE         0x0001
#define SMBOPEN_READWRITE     0x0002
#define SMBOPEN_EXECUTE       0x0003

#define SMBOPEN_OCREATE       0x0010
#define SMBOPEN_OTRUNC        0x0002
#define SMBOPEN_OAPPEND       0x0001

#define SMBFLG_EXTD_LOCK 0x01	
#define SMBFLG_RCV_POSTED 0x02	
#define SMBFLG_RSVD 0x04
#define SMBFLG_CASELESS 0x08	
#define SMBFLG_CANONICAL_PATH_FORMAT 0x10	
#define SMBFLG_OLD_OPLOCK 0x20	
#define SMBFLG_OLD_OPLOCK_NOTIFY 0x40	
#define SMBFLG_RESPONSE 0x80	

#define SMBFLG2_KNOWS_LONG_NAMES cpu_to_le16(1)	
#define SMBFLG2_KNOWS_EAS cpu_to_le16(2)
#define SMBFLG2_SECURITY_SIGNATURE cpu_to_le16(4)
#define SMBFLG2_COMPRESSED (8)
#define SMBFLG2_SECURITY_SIGNATURE_REQUIRED (0x10)
#define SMBFLG2_IS_LONG_NAME cpu_to_le16(0x40)
#define SMBFLG2_REPARSE_PATH (0x400)
#define SMBFLG2_EXT_SEC cpu_to_le16(0x800)
#define SMBFLG2_DFS cpu_to_le16(0x1000)
#define SMBFLG2_PAGING_IO cpu_to_le16(0x2000)
#define SMBFLG2_ERR_STATUS cpu_to_le16(0x4000)
#define SMBFLG2_UNICODE cpu_to_le16(0x8000)


#define FILE_READ_DATA        0x00000001  
#define FILE_WRITE_DATA       0x00000002  /* Data can be written to the file  */
#define FILE_APPEND_DATA      0x00000004  
#define FILE_READ_EA          0x00000008  
					  
#define FILE_WRITE_EA         0x00000010  
					  /* with the file can be written     */
#define FILE_EXECUTE          0x00000020  
					  
#define FILE_DELETE_CHILD     0x00000040
#define FILE_READ_ATTRIBUTES  0x00000080  
					  
#define FILE_WRITE_ATTRIBUTES 0x00000100  
					  /* file can be written              */
#define DELETE                0x00010000  
#define READ_CONTROL          0x00020000  
					  
					  
#define WRITE_DAC             0x00040000  
					  
					  /* file can be written.             */
#define WRITE_OWNER           0x00080000  
					  /* with the file can be written     */
#define SYNCHRONIZE           0x00100000  
					  
					  
#define GENERIC_ALL           0x10000000
#define GENERIC_EXECUTE       0x20000000
#define GENERIC_WRITE         0x40000000
#define GENERIC_READ          0x80000000
					 
					 
					 
					 
					 

#define FILE_READ_RIGHTS (FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES)
#define FILE_WRITE_RIGHTS (FILE_WRITE_DATA | FILE_APPEND_DATA \
				| FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES)
#define FILE_EXEC_RIGHTS (FILE_EXECUTE)

#define SET_FILE_READ_RIGHTS (FILE_READ_DATA | FILE_READ_EA | FILE_WRITE_EA \
				| FILE_READ_ATTRIBUTES \
				| FILE_WRITE_ATTRIBUTES \
				| DELETE | READ_CONTROL | WRITE_DAC \
				| WRITE_OWNER | SYNCHRONIZE)
#define SET_FILE_WRITE_RIGHTS (FILE_WRITE_DATA | FILE_APPEND_DATA \
				| FILE_READ_EA | FILE_WRITE_EA \
				| FILE_DELETE_CHILD | FILE_READ_ATTRIBUTES \
				| FILE_WRITE_ATTRIBUTES \
				| DELETE | READ_CONTROL | WRITE_DAC \
				| WRITE_OWNER | SYNCHRONIZE)
#define SET_FILE_EXEC_RIGHTS (FILE_READ_EA | FILE_WRITE_EA | FILE_EXECUTE \
				| FILE_READ_ATTRIBUTES \
				| FILE_WRITE_ATTRIBUTES \
				| DELETE | READ_CONTROL | WRITE_DAC \
				| WRITE_OWNER | SYNCHRONIZE)

#define SET_MINIMUM_RIGHTS (FILE_READ_EA | FILE_READ_ATTRIBUTES \
				| READ_CONTROL | SYNCHRONIZE)


#define CIFS_NO_HANDLE        0xFFFF

#define NO_CHANGE_64          0xFFFFFFFFFFFFFFFFULL
#define NO_CHANGE_32          0xFFFFFFFFUL

#define CIFS_IPC_RESOURCE "\x49\x50\x43\x24"

#define CIFS_IPC_UNICODE_RESOURCE "\x00\x49\x00\x50\x00\x43\x00\x24\x00\x00"

#define UNICODE_NULL "\x00\x00"
#define ASCII_NULL 0x00

#define CIFS_SV_TYPE_DC     0x00000008
#define CIFS_SV_TYPE_BACKDC 0x00000010

#define CIFS_ALIAS_TYPE_FILE 0x0001
#define CIFS_SHARE_TYPE_FILE 0x0000

#define ATTR_READONLY  0x0001
#define ATTR_HIDDEN    0x0002
#define ATTR_SYSTEM    0x0004
#define ATTR_VOLUME    0x0008
#define ATTR_DIRECTORY 0x0010
#define ATTR_ARCHIVE   0x0020
#define ATTR_DEVICE    0x0040
#define ATTR_NORMAL    0x0080
#define ATTR_TEMPORARY 0x0100
#define ATTR_SPARSE    0x0200
#define ATTR_REPARSE   0x0400
#define ATTR_COMPRESSED 0x0800
#define ATTR_OFFLINE    0x1000	
#define ATTR_NOT_CONTENT_INDEXED 0x2000
#define ATTR_ENCRYPTED  0x4000
#define ATTR_POSIX_SEMANTICS 0x01000000
#define ATTR_BACKUP_SEMANTICS 0x02000000
#define ATTR_DELETE_ON_CLOSE 0x04000000
#define ATTR_SEQUENTIAL_SCAN 0x08000000
#define ATTR_RANDOM_ACCESS   0x10000000
#define ATTR_NO_BUFFERING    0x20000000
#define ATTR_WRITE_THROUGH   0x80000000

#define FILE_NO_SHARE     0x00000000
#define FILE_SHARE_READ   0x00000001
#define FILE_SHARE_WRITE  0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define FILE_SHARE_ALL    0x00000007

#define FILE_SUPERSEDE    0x00000000
#define FILE_OPEN         0x00000001
#define FILE_CREATE       0x00000002
#define FILE_OPEN_IF      0x00000003
#define FILE_OVERWRITE    0x00000004
#define FILE_OVERWRITE_IF 0x00000005

#define CREATE_NOT_FILE		0x00000001	
#define CREATE_WRITE_THROUGH	0x00000002
#define CREATE_SEQUENTIAL       0x00000004
#define CREATE_NO_BUFFER        0x00000008      
#define CREATE_SYNC_ALERT       0x00000010	
#define CREATE_ASYNC_ALERT      0x00000020	
#define CREATE_NOT_DIR		0x00000040    
#define CREATE_TREE_CONNECTION  0x00000080	
#define CREATE_COMPLETE_IF_OPLK 0x00000100	
#define CREATE_NO_EA_KNOWLEDGE  0x00000200
#define CREATE_EIGHT_DOT_THREE  0x00000400	
#define CREATE_OPEN_FOR_RECOVERY 0x00000400
#define CREATE_RANDOM_ACCESS	0x00000800
#define CREATE_DELETE_ON_CLOSE	0x00001000
#define CREATE_OPEN_BY_ID       0x00002000
#define CREATE_OPEN_BACKUP_INTENT 0x00004000
#define CREATE_NO_COMPRESSION   0x00008000
#define CREATE_RESERVE_OPFILTER 0x00100000	
#define OPEN_REPARSE_POINT	0x00200000
#define OPEN_NO_RECALL          0x00400000
#define OPEN_FREE_SPACE_QUERY   0x00800000	
#define CREATE_OPTIONS_MASK     0x007FFFFF
#define CREATE_OPTION_READONLY	0x10000000
#define CREATE_OPTION_SPECIAL   0x20000000   

#define SECURITY_ANONYMOUS      0
#define SECURITY_IDENTIFICATION 1
#define SECURITY_IMPERSONATION  2
#define SECURITY_DELEGATION     3

#define SECURITY_CONTEXT_TRACKING 0x01
#define SECURITY_EFFECTIVE_ONLY   0x02

#define CIFS_DFT_PID  0x1234

#define CIFS_COPY_OP 1
#define CIFS_RENAME_OP 2

#define GETU16(var)  (*((__u16 *)var))	
#define GETU32(var)  (*((__u32 *)var))	

struct smb_hdr {
	__be32 smb_buf_length;	
	__u8 Protocol[4];
	__u8 Command;
	union {
		struct {
			__u8 ErrorClass;
			__u8 Reserved;
			__le16 Error;
		} __attribute__((packed)) DosError;
		__le32 CifsError;
	} __attribute__((packed)) Status;
	__u8 Flags;
	__le16 Flags2;		
	__le16 PidHigh;
	union {
		struct {
			__le32 SequenceNumber;  
			__u32 Reserved; 
		} __attribute__((packed)) Sequence;
		__u8 SecuritySignature[8];	
	} __attribute__((packed)) Signature;
	__u8 pad[2];
	__u16 Tid;
	__le16 Pid;
	__u16 Uid;
	__u16 Mid;
	__u8 WordCount;
} __attribute__((packed));

static inline void *
BCC(struct smb_hdr *smb)
{
	return (void *)smb + sizeof(*smb) + 2 * smb->WordCount;
}

#define pByteArea(smb_var) (BCC(smb_var) + 2)

static inline __u16
get_bcc(struct smb_hdr *hdr)
{
	__le16 *bc_ptr = (__le16 *)BCC(hdr);

	return get_unaligned_le16(bc_ptr);
}

static inline void
put_bcc(__u16 count, struct smb_hdr *hdr)
{
	__le16 *bc_ptr = (__le16 *)BCC(hdr);

	put_unaligned_le16(count, bc_ptr);
}

#define CNLEN 15


#define MAXCOMMENTLEN 40

#define MAX_PATHCONF 256


typedef struct negotiate_req {
	struct smb_hdr hdr;	
	__le16 ByteCount;
	unsigned char DialectsArray[1];
} __attribute__((packed)) NEGOTIATE_REQ;


#define MIN_TZ_ADJ (15 * 60) 

typedef struct lanman_neg_rsp {
	struct smb_hdr hdr;	
	__le16 DialectIndex;
	__le16 SecurityMode;
	__le16 MaxBufSize;
	__le16 MaxMpxCount;
	__le16 MaxNumberVcs;
	__le16 RawMode;
	__le32 SessionKey;
	struct {
		__le16 Time;
		__le16 Date;
	} __attribute__((packed)) SrvTime;
	__le16 ServerTimeZone;
	__le16 EncryptionKeyLength;
	__le16 Reserved;
	__u16  ByteCount;
	unsigned char EncryptionKey[1];
} __attribute__((packed)) LANMAN_NEG_RSP;

#define READ_RAW_ENABLE 1
#define WRITE_RAW_ENABLE 2
#define RAW_ENABLE (READ_RAW_ENABLE | WRITE_RAW_ENABLE)

typedef struct negotiate_rsp {
	struct smb_hdr hdr;	
	__le16 DialectIndex; 
	__u8 SecurityMode;
	__le16 MaxMpxCount;
	__le16 MaxNumberVcs;
	__le32 MaxBufferSize;
	__le32 MaxRawSize;
	__le32 SessionKey;
	__le32 Capabilities;	
	__le32 SystemTimeLow;
	__le32 SystemTimeHigh;
	__le16 ServerTimeZone;
	__u8 EncryptionKeyLength;
	__u16 ByteCount;
	union {
		unsigned char EncryptionKey[1];	
		
		
		
		struct {
			unsigned char GUID[16];
			unsigned char SecurityBlob[1];
		} __attribute__((packed)) extended_response;
	} __attribute__((packed)) u;
} __attribute__((packed)) NEGOTIATE_RSP;

#define SECMODE_USER          0x01	
#define SECMODE_PW_ENCRYPT    0x02
#define SECMODE_SIGN_ENABLED  0x04	
#define SECMODE_SIGN_REQUIRED 0x08	

#define CAP_RAW_MODE           0x00000001
#define CAP_MPX_MODE           0x00000002
#define CAP_UNICODE            0x00000004
#define CAP_LARGE_FILES        0x00000008
#define CAP_NT_SMBS            0x00000010	
#define CAP_RPC_REMOTE_APIS    0x00000020
#define CAP_STATUS32           0x00000040
#define CAP_LEVEL_II_OPLOCKS   0x00000080
#define CAP_LOCK_AND_READ      0x00000100
#define CAP_NT_FIND            0x00000200
#define CAP_DFS                0x00001000
#define CAP_INFOLEVEL_PASSTHRU 0x00002000
#define CAP_LARGE_READ_X       0x00004000
#define CAP_LARGE_WRITE_X      0x00008000
#define CAP_LWIO               0x00010000 
#define CAP_UNIX               0x00800000
#define CAP_COMPRESSED_DATA    0x02000000
#define CAP_DYNAMIC_REAUTH     0x20000000
#define CAP_PERSISTENT_HANDLES 0x40000000
#define CAP_EXTENDED_SECURITY  0x80000000

typedef union smb_com_session_setup_andx {
	struct {		
		struct smb_hdr hdr;	
		__u8 AndXCommand;
		__u8 AndXReserved;
		__le16 AndXOffset;
		__le16 MaxBufferSize;
		__le16 MaxMpxCount;
		__le16 VcNumber;
		__u32 SessionKey;
		__le16 SecurityBlobLength;
		__u32 Reserved;
		__le32 Capabilities;	
		__le16 ByteCount;
		unsigned char SecurityBlob[1];	
		
		
	} __attribute__((packed)) req;	

	struct {		
		struct smb_hdr hdr;	
		__u8 AndXCommand;
		__u8 AndXReserved;
		__le16 AndXOffset;
		__le16 MaxBufferSize;
		__le16 MaxMpxCount;
		__le16 VcNumber;
		__u32 SessionKey;
		__le16 CaseInsensitivePasswordLength; 
		__le16 CaseSensitivePasswordLength; 
		__u32 Reserved;	
		__le32 Capabilities;
		__le16 ByteCount;
		unsigned char CaseInsensitivePassword[1];     
		
		
		
		
		
	} __attribute__((packed)) req_no_secext; 

	struct {		
		struct smb_hdr hdr;	
		__u8 AndXCommand;
		__u8 AndXReserved;
		__le16 AndXOffset;
		__le16 Action;	
		__le16 SecurityBlobLength;
		__u16 ByteCount;
		unsigned char SecurityBlob[1];	
	} __attribute__((packed)) resp;	

	struct {		
		struct smb_hdr hdr;	
		__u8 AndXCommand;
		__u8 AndXReserved;
		__le16 AndXOffset;
		__le16 MaxBufferSize;
		__le16 MaxMpxCount;
		__le16 VcNumber;
		__u32 SessionKey;
		__le16 PasswordLength;
		__u32 Reserved; 
		__le16 ByteCount;
		unsigned char AccountPassword[1];	
		
		
		
		
	} __attribute__((packed)) old_req; 

	struct {		
		struct smb_hdr hdr;	
		__u8 AndXCommand;
		__u8 AndXReserved;
		__le16 AndXOffset;
		__le16 Action;	
		__u16 ByteCount;
		unsigned char NativeOS[1];	
	} __attribute__((packed)) old_resp; 
} __attribute__((packed)) SESSION_SETUP_ANDX;


#define NTLMSSP_SERVER_TYPE	1
#define NTLMSSP_DOMAIN_TYPE	2
#define NTLMSSP_FQ_DOMAIN_TYPE	3
#define NTLMSSP_DNS_DOMAIN_TYPE	4
#define NTLMSSP_DNS_PARENT_TYPE	5

struct ntlmssp2_name {
	__le16 type;
	__le16 length;
} __attribute__((packed));

struct ntlmv2_resp {
	char ntlmv2_hash[CIFS_ENCPWD_SIZE];
	__le32 blob_signature;
	__u32  reserved;
	__le64  time;
	__u64  client_chal; 
	__u32  reserved2;
	
} __attribute__((packed));


#define CIFS_NETWORK_OPSYS "CIFS VFS Client for Linux"

#define CAP_UNICODE            0x00000004
#define CAP_LARGE_FILES        0x00000008
#define CAP_NT_SMBS            0x00000010
#define CAP_STATUS32           0x00000040
#define CAP_LEVEL_II_OPLOCKS   0x00000080
#define CAP_NT_FIND            0x00000200	
#define CAP_BULK_TRANSFER      0x20000000
#define CAP_EXTENDED_SECURITY  0x80000000

#define GUEST_LOGIN 1

typedef struct smb_com_tconx_req {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__le16 Flags;		
	__le16 PasswordLength;
	__le16 ByteCount;
	unsigned char Password[1];	
	
} __attribute__((packed)) TCONX_REQ;

typedef struct smb_com_tconx_rsp {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__le16 OptionalSupport;	
	__u16 ByteCount;
	unsigned char Service[1];	
	
} __attribute__((packed)) TCONX_RSP;

typedef struct smb_com_tconx_rsp_ext {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__le16 OptionalSupport;	
	__le32 MaximalShareAccessRights;
	__le32 GuestMaximalShareAccessRights;
	__u16 ByteCount;
	unsigned char Service[1];	
	
} __attribute__((packed)) TCONX_RSP_EXT;


#define DISCONNECT_TID          0x0001
#define TCON_EXTENDED_SIGNATURES 0x0004
#define TCON_EXTENDED_SECINFO   0x0008

#define SMB_SUPPORT_SEARCH_BITS 0x0001	
#define SMB_SHARE_IS_IN_DFS     0x0002
#define SMB_CSC_MASK               0x000C
#define SMB_CSC_CACHE_MANUAL_REINT 0x0000
#define SMB_CSC_CACHE_AUTO_REINT   0x0004
#define SMB_CSC_CACHE_VDO          0x0008
#define SMB_CSC_NO_CACHING         0x000C
#define SMB_UNIQUE_FILE_NAME    0x0010
#define SMB_EXTENDED_SIGNATURES 0x0020


typedef struct smb_com_echo_req {
	struct	smb_hdr hdr;
	__le16	EchoCount;
	__le16	ByteCount;
	char	Data[1];
} __attribute__((packed)) ECHO_REQ;

typedef struct smb_com_echo_rsp {
	struct	smb_hdr hdr;
	__le16	SequenceNumber;
	__le16	ByteCount;
	char	Data[1];
} __attribute__((packed)) ECHO_RSP;

typedef struct smb_com_logoff_andx_req {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__u16 AndXOffset;
	__u16 ByteCount;
} __attribute__((packed)) LOGOFF_ANDX_REQ;

typedef struct smb_com_logoff_andx_rsp {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__u16 AndXOffset;
	__u16 ByteCount;
} __attribute__((packed)) LOGOFF_ANDX_RSP;

typedef union smb_com_tree_disconnect {	
					
	struct {
		struct smb_hdr hdr;	
		__u16 ByteCount;	
	} __attribute__((packed)) req;
	struct {
		struct smb_hdr hdr;	
		__u16 ByteCount;	
	} __attribute__((packed)) resp;
} __attribute__((packed)) TREE_DISCONNECT;

typedef struct smb_com_close_req {
	struct smb_hdr hdr;	
	__u16 FileID;
	__u32 LastWriteTime;	
	__u16 ByteCount;	
} __attribute__((packed)) CLOSE_REQ;

typedef struct smb_com_close_rsp {
	struct smb_hdr hdr;	
	__u16 ByteCount;	
} __attribute__((packed)) CLOSE_RSP;

typedef struct smb_com_flush_req {
	struct smb_hdr hdr;	
	__u16 FileID;
	__u16 ByteCount;	
} __attribute__((packed)) FLUSH_REQ;

typedef struct smb_com_findclose_req {
	struct smb_hdr hdr; 
	__u16 FileID;
	__u16 ByteCount;    
} __attribute__((packed)) FINDCLOSE_REQ;

#define REQ_MORE_INFO      0x00000001  
#define REQ_OPLOCK         0x00000002
#define REQ_BATCHOPLOCK    0x00000004
#define REQ_OPENDIRONLY    0x00000008
#define REQ_EXTENDED_INFO  0x00000010

#define DISK_TYPE		0x0000
#define BYTE_PIPE_TYPE		0x0001
#define MESSAGE_PIPE_TYPE	0x0002
#define PRINTER_TYPE		0x0003
#define COMM_DEV_TYPE		0x0004
#define UNKNOWN_TYPE		0xFFFF

#define NO_EAS			0x0001
#define NO_SUBSTREAMS		0x0002
#define NO_REPARSETAG		0x0004
#define ICOUNT_MASK		0x00FF
#define PIPE_READ_MODE		0x0100
#define NAMED_PIPE_TYPE		0x0400
#define PIPE_END_POINT		0x4000
#define BLOCKING_NAMED_PIPE	0x8000

typedef struct smb_com_open_req {	
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u8 Reserved;		
	__le16 NameLength;
	__le32 OpenFlags;
	__u32  RootDirectoryFid;
	__le32 DesiredAccess;
	__le64 AllocationSize;
	__le32 FileAttributes;
	__le32 ShareAccess;
	__le32 CreateDisposition;
	__le32 CreateOptions;
	__le32 ImpersonationLevel;
	__u8 SecurityFlags;
	__le16 ByteCount;
	char fileName[1];
} __attribute__((packed)) OPEN_REQ;

#define OPLOCK_NONE  	 0
#define OPLOCK_EXCLUSIVE 1
#define OPLOCK_BATCH	 2
#define OPLOCK_READ	 3  

#define CIFS_CREATE_ACTION 0x20000 

typedef struct smb_com_open_rsp {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u8 OplockLevel;
	__u16 Fid;
	__le32 CreateAction;
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le32 FileAttributes;
	__le64 AllocationSize;
	__le64 EndOfFile;
	__le16 FileType;
	__le16 DeviceState;
	__u8 DirectoryFlag;
	__u16 ByteCount;	
} __attribute__((packed)) OPEN_RSP;

typedef struct smb_com_open_rsp_ext {
	struct smb_hdr hdr;     
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u8 OplockLevel;
	__u16 Fid;
	__le32 CreateAction;
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le32 FileAttributes;
	__le64 AllocationSize;
	__le64 EndOfFile;
	__le16 FileType;
	__le16 DeviceState;
	__u8 DirectoryFlag;
	__u8 VolumeGUID[16];
	__u64 FileId; 
	__le32 MaximalAccessRights;
	__le32 GuestMaximalAccessRights;
	__u16 ByteCount;        
} __attribute__((packed)) OPEN_RSP_EXT;


typedef struct smb_com_openx_req {
	struct smb_hdr	hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__le16 OpenFlags;
	__le16 Mode;
	__le16 Sattr; 
	__le16 FileAttributes;  
	__le32 CreateTime; 
	__le16 OpenFunction;
	__le32 EndOfFile;
	__le32 Timeout;
	__le32 Reserved;
	__le16  ByteCount;  
	char   fileName[1];
} __attribute__((packed)) OPENX_REQ;

typedef struct smb_com_openx_rsp {
	struct smb_hdr	hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u16  Fid;
	__le16 FileAttributes;
	__le32 LastWriteTime; 
	__le32 EndOfFile;
	__le16 Access;
	__le16 FileType;
	__le16 IPCState;
	__le16 Action;
	__u32  FileId;
	__u16  Reserved;
	__u16  ByteCount;
} __attribute__((packed)) OPENX_RSP;


typedef struct smb_com_writex_req {
	struct smb_hdr hdr;     
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u16 Fid;
	__le32 OffsetLow;
	__u32 Reserved; 
	__le16 WriteMode; 
	__le16 Remaining;
	__le16 Reserved2;
	__le16 DataLengthLow;
	__le16 DataOffset;
	__le16 ByteCount;
	__u8 Pad;		
	char Data[0];
} __attribute__((packed)) WRITEX_REQ;

typedef struct smb_com_write_req {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u16 Fid;
	__le32 OffsetLow;
	__u32 Reserved;
	__le16 WriteMode;
	__le16 Remaining;
	__le16 DataLengthHigh;
	__le16 DataLengthLow;
	__le16 DataOffset;
	__le32 OffsetHigh;
	__le16 ByteCount;
	__u8 Pad;		
	char Data[0];
} __attribute__((packed)) WRITE_REQ;

typedef struct smb_com_write_rsp {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__le16 Count;
	__le16 Remaining;
	__le16 CountHigh;
	__u16  Reserved;
	__u16 ByteCount;
} __attribute__((packed)) WRITE_RSP;

typedef struct smb_com_readx_req {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u16 Fid;
	__le32 OffsetLow;
	__le16 MaxCount;
	__le16 MinCount;	
	__le32 Reserved;
	__le16 Remaining;
	__le16 ByteCount;
} __attribute__((packed)) READX_REQ;

typedef struct smb_com_read_req {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u16 Fid;
	__le32 OffsetLow;
	__le16 MaxCount;
	__le16 MinCount;		
	__le32 MaxCountHigh;
	__le16 Remaining;
	__le32 OffsetHigh;
	__le16 ByteCount;
} __attribute__((packed)) READ_REQ;

typedef struct smb_com_read_rsp {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__le16 Remaining;
	__le16 DataCompactionMode;
	__le16 Reserved;
	__le16 DataLength;
	__le16 DataOffset;
	__le16 DataLengthHigh;
	__u64 Reserved2;
	__u16 ByteCount;
	
} __attribute__((packed)) READ_RSP;

typedef struct locking_andx_range {
	__le16 Pid;
	__le16 Pad;
	__le32 OffsetHigh;
	__le32 OffsetLow;
	__le32 LengthHigh;
	__le32 LengthLow;
} __attribute__((packed)) LOCKING_ANDX_RANGE;

#define LOCKING_ANDX_SHARED_LOCK     0x01
#define LOCKING_ANDX_OPLOCK_RELEASE  0x02
#define LOCKING_ANDX_CHANGE_LOCKTYPE 0x04
#define LOCKING_ANDX_CANCEL_LOCK     0x08
#define LOCKING_ANDX_LARGE_FILES     0x10	

typedef struct smb_com_lock_req {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u16 Fid;
	__u8 LockType;
	__u8 OplockLevel;
	__le32 Timeout;
	__le16 NumberOfUnlocks;
	__le16 NumberOfLocks;
	__le16 ByteCount;
	LOCKING_ANDX_RANGE Locks[1];
} __attribute__((packed)) LOCK_REQ;

#define CIFS_RDLCK	0
#define CIFS_WRLCK	1
#define CIFS_UNLCK      2
typedef struct cifs_posix_lock {
	__le16  lock_type;  
	__le16  lock_flags; 
	__le32  pid;
	__le64	start;
	__le64	length;
	
} __attribute__((packed)) CIFS_POSIX_LOCK;

typedef struct smb_com_lock_rsp {
	struct smb_hdr hdr;	
	__u8 AndXCommand;
	__u8 AndXReserved;
	__le16 AndXOffset;
	__u16 ByteCount;
} __attribute__((packed)) LOCK_RSP;

typedef struct smb_com_rename_req {
	struct smb_hdr hdr;	
	__le16 SearchAttributes;	
	__le16 ByteCount;
	__u8 BufferFormat;	
	unsigned char OldFileName[1];
	
	
} __attribute__((packed)) RENAME_REQ;

	
#define COPY_MUST_BE_FILE      0x0001
#define COPY_MUST_BE_DIR       0x0002
#define COPY_TARGET_MODE_ASCII 0x0004 
#define COPY_SOURCE_MODE_ASCII 0x0008 
#define COPY_VERIFY_WRITES     0x0010
#define COPY_TREE              0x0020

typedef struct smb_com_copy_req {
	struct smb_hdr hdr;	
	__u16 Tid2;
	__le16 OpenFunction;
	__le16 Flags;
	__le16 ByteCount;
	__u8 BufferFormat;	
	unsigned char OldFileName[1];
	
	
} __attribute__((packed)) COPY_REQ;

typedef struct smb_com_copy_rsp {
	struct smb_hdr hdr;     
	__le16 CopyCount;    
	__u16 ByteCount;    
	__u8 BufferFormat;  
	unsigned char ErrorFileName[1]; 
} __attribute__((packed)) COPY_RSP;

#define CREATE_HARD_LINK		0x103
#define MOVEFILE_COPY_ALLOWED		0x0002
#define MOVEFILE_REPLACE_EXISTING	0x0001

typedef struct smb_com_nt_rename_req {	
	struct smb_hdr hdr;	
	__le16 SearchAttributes;	
	__le16 Flags;		
	__le32 ClusterCount;
	__le16 ByteCount;
	__u8 BufferFormat;	
	unsigned char OldFileName[1];
	
	
} __attribute__((packed)) NT_RENAME_REQ;

typedef struct smb_com_rename_rsp {
	struct smb_hdr hdr;	
	__u16 ByteCount;	
} __attribute__((packed)) RENAME_RSP;

typedef struct smb_com_delete_file_req {
	struct smb_hdr hdr;	
	__le16 SearchAttributes;
	__le16 ByteCount;
	__u8 BufferFormat;	
	unsigned char fileName[1];
} __attribute__((packed)) DELETE_FILE_REQ;

typedef struct smb_com_delete_file_rsp {
	struct smb_hdr hdr;	
	__u16 ByteCount;	
} __attribute__((packed)) DELETE_FILE_RSP;

typedef struct smb_com_delete_directory_req {
	struct smb_hdr hdr;	
	__le16 ByteCount;
	__u8 BufferFormat;	
	unsigned char DirName[1];
} __attribute__((packed)) DELETE_DIRECTORY_REQ;

typedef struct smb_com_delete_directory_rsp {
	struct smb_hdr hdr;	
	__u16 ByteCount;	
} __attribute__((packed)) DELETE_DIRECTORY_RSP;

typedef struct smb_com_create_directory_req {
	struct smb_hdr hdr;	
	__le16 ByteCount;
	__u8 BufferFormat;	
	unsigned char DirName[1];
} __attribute__((packed)) CREATE_DIRECTORY_REQ;

typedef struct smb_com_create_directory_rsp {
	struct smb_hdr hdr;	
	__u16 ByteCount;	
} __attribute__((packed)) CREATE_DIRECTORY_RSP;

typedef struct smb_com_query_information_req {
	struct smb_hdr hdr;     
	__le16 ByteCount;	
	__u8 BufferFormat;      
	unsigned char FileName[1];
} __attribute__((packed)) QUERY_INFORMATION_REQ;

typedef struct smb_com_query_information_rsp {
	struct smb_hdr hdr;     
	__le16 attr;
	__le32  last_write_time;
	__le32 size;
	__u16  reserved[5];
	__le16 ByteCount;	
} __attribute__((packed)) QUERY_INFORMATION_RSP;

typedef struct smb_com_setattr_req {
	struct smb_hdr hdr; 
	__le16 attr;
	__le16 time_low;
	__le16 time_high;
	__le16 reserved[5]; 
	__u16  ByteCount;
	__u8   BufferFormat; 
	unsigned char fileName[1];
} __attribute__((packed)) SETATTR_REQ;

typedef struct smb_com_setattr_rsp {
	struct smb_hdr hdr;     
	__u16 ByteCount;        
} __attribute__((packed)) SETATTR_RSP;


typedef struct smb_com_ntransact_req {
	struct smb_hdr hdr; 
	__u8 MaxSetupCount;
	__u16 Reserved;
	__le32 TotalParameterCount;
	__le32 TotalDataCount;
	__le32 MaxParameterCount;
	__le32 MaxDataCount;
	__le32 ParameterCount;
	__le32 ParameterOffset;
	__le32 DataCount;
	__le32 DataOffset;
	__u8 SetupCount; 
	
	__le16 SubCommand; 
	
	__le16 ByteCount;
	__u8 Pad[3];
	__u8 Parms[0];
} __attribute__((packed)) NTRANSACT_REQ;

typedef struct smb_com_ntransact_rsp {
	struct smb_hdr hdr;     
	__u8 Reserved[3];
	__le32 TotalParameterCount;
	__le32 TotalDataCount;
	__le32 ParameterCount;
	__le32 ParameterOffset;
	__le32 ParameterDisplacement;
	__le32 DataCount;
	__le32 DataOffset;
	__le32 DataDisplacement;
	__u8 SetupCount;   
	__u16 ByteCount;
	
	
} __attribute__((packed)) NTRANSACT_RSP;

typedef struct smb_com_transaction_ioctl_req {
	struct smb_hdr hdr;	
	__u8 MaxSetupCount;
	__u16 Reserved;
	__le32 TotalParameterCount;
	__le32 TotalDataCount;
	__le32 MaxParameterCount;
	__le32 MaxDataCount;
	__le32 ParameterCount;
	__le32 ParameterOffset;
	__le32 DataCount;
	__le32 DataOffset;
	__u8 SetupCount; 
	
	__le16 SubCommand; 
	__le32 FunctionCode;
	__u16 Fid;
	__u8 IsFsctl;  
	__u8 IsRootFlag; 
	__le16 ByteCount;
	__u8 Pad[3];
	__u8 Data[1];
} __attribute__((packed)) TRANSACT_IOCTL_REQ;

typedef struct smb_com_transaction_ioctl_rsp {
	struct smb_hdr hdr;	
	__u8 Reserved[3];
	__le32 TotalParameterCount;
	__le32 TotalDataCount;
	__le32 ParameterCount;
	__le32 ParameterOffset;
	__le32 ParameterDisplacement;
	__le32 DataCount;
	__le32 DataOffset;
	__le32 DataDisplacement;
	__u8 SetupCount;	
	__le16 ReturnedDataLen;
	__u16 ByteCount;
} __attribute__((packed)) TRANSACT_IOCTL_RSP;

#define CIFS_ACL_OWNER 1
#define CIFS_ACL_GROUP 2
#define CIFS_ACL_DACL  4
#define CIFS_ACL_SACL  8

typedef struct smb_com_transaction_qsec_req {
	struct smb_hdr hdr;     
	__u8 MaxSetupCount;
	__u16 Reserved;
	__le32 TotalParameterCount;
	__le32 TotalDataCount;
	__le32 MaxParameterCount;
	__le32 MaxDataCount;
	__le32 ParameterCount;
	__le32 ParameterOffset;
	__le32 DataCount;
	__le32 DataOffset;
	__u8 SetupCount; 
	
	__le16 SubCommand; 
	__le16 ByteCount; 
	__u8 Pad[3];
	__u16 Fid;
	__u16 Reserved2;
	__le32 AclFlags;
} __attribute__((packed)) QUERY_SEC_DESC_REQ;


typedef struct smb_com_transaction_ssec_req {
	struct smb_hdr hdr;     
	__u8 MaxSetupCount;
	__u16 Reserved;
	__le32 TotalParameterCount;
	__le32 TotalDataCount;
	__le32 MaxParameterCount;
	__le32 MaxDataCount;
	__le32 ParameterCount;
	__le32 ParameterOffset;
	__le32 DataCount;
	__le32 DataOffset;
	__u8 SetupCount; 
	
	__le16 SubCommand; 
	__le16 ByteCount; 
	__u8 Pad[3];
	__u16 Fid;
	__u16 Reserved2;
	__le32 AclFlags;
} __attribute__((packed)) SET_SEC_DESC_REQ;

typedef struct smb_com_transaction_change_notify_req {
	struct smb_hdr hdr;     
	__u8 MaxSetupCount;
	__u16 Reserved;
	__le32 TotalParameterCount;
	__le32 TotalDataCount;
	__le32 MaxParameterCount;
	__le32 MaxDataCount;
	__le32 ParameterCount;
	__le32 ParameterOffset;
	__le32 DataCount;
	__le32 DataOffset;
	__u8 SetupCount; 
	
	__le16 SubCommand;
	__le32 CompletionFilter;  
	__u16 Fid;
	__u8 WatchTree;  
	__u8 Reserved2;
	__le16 ByteCount;
} __attribute__((packed)) TRANSACT_CHANGE_NOTIFY_REQ;

typedef struct smb_com_transaction_change_notify_rsp {
	struct smb_hdr hdr;	
	__u8 Reserved[3];
	__le32 TotalParameterCount;
	__le32 TotalDataCount;
	__le32 ParameterCount;
	__le32 ParameterOffset;
	__le32 ParameterDisplacement;
	__le32 DataCount;
	__le32 DataOffset;
	__le32 DataDisplacement;
	__u8 SetupCount;   
	__u16 ByteCount;
	
} __attribute__((packed)) TRANSACT_CHANGE_NOTIFY_RSP;
#define FILE_NOTIFY_CHANGE_FILE_NAME    0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME     0x00000002
#define FILE_NOTIFY_CHANGE_NAME         0x00000003
#define FILE_NOTIFY_CHANGE_ATTRIBUTES   0x00000004
#define FILE_NOTIFY_CHANGE_SIZE         0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE   0x00000010
#define FILE_NOTIFY_CHANGE_LAST_ACCESS  0x00000020
#define FILE_NOTIFY_CHANGE_CREATION     0x00000040
#define FILE_NOTIFY_CHANGE_EA           0x00000080
#define FILE_NOTIFY_CHANGE_SECURITY     0x00000100
#define FILE_NOTIFY_CHANGE_STREAM_NAME  0x00000200
#define FILE_NOTIFY_CHANGE_STREAM_SIZE  0x00000400
#define FILE_NOTIFY_CHANGE_STREAM_WRITE 0x00000800

#define FILE_ACTION_ADDED		0x00000001
#define FILE_ACTION_REMOVED		0x00000002
#define FILE_ACTION_MODIFIED		0x00000003
#define FILE_ACTION_RENAMED_OLD_NAME	0x00000004
#define FILE_ACTION_RENAMED_NEW_NAME	0x00000005
#define FILE_ACTION_ADDED_STREAM	0x00000006
#define FILE_ACTION_REMOVED_STREAM	0x00000007
#define FILE_ACTION_MODIFIED_STREAM	0x00000008

struct file_notify_information {
	__le32 NextEntryOffset;
	__le32 Action;
	__le32 FileNameLength;
	__u8  FileName[0];
} __attribute__((packed));

struct reparse_data {
	__u32	ReparseTag;
	__u16	ReparseDataLength;
	__u16	Reserved;
	__u16	AltNameOffset;
	__u16	AltNameLen;
	__u16	TargetNameOffset;
	__u16	TargetNameLen;
	char	LinkNamesBuf[1];
} __attribute__((packed));

struct cifs_quota_data {
	__u32	rsrvd1;  
	__u32	sid_size;
	__u64	rsrvd2;  
	__u64	space_used;
	__u64	soft_limit;
	__u64	hard_limit;
	char	sid[1];  
} __attribute__((packed));

#define QUOTA_LIST_CONTINUE	    0
#define QUOTA_LIST_START	0x100
#define QUOTA_FOR_SID		0x101

struct trans2_req {
	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 DataCount;
	__le16 DataOffset;
	__u8 SetupCount;
	__u8 Reserved3;
	__le16 SubCommand; 
	__le16 ByteCount;
} __attribute__((packed));

struct smb_t2_req {
	struct smb_hdr hdr;
	struct trans2_req t2_req;
} __attribute__((packed));

struct trans2_resp {
	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__u16 Reserved;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 ParameterDisplacement;
	__le16 DataCount;
	__le16 DataOffset;
	__le16 DataDisplacement;
	__u8 SetupCount;
	__u8 Reserved1;
	
} __attribute__((packed));

struct smb_t2_rsp {
	struct smb_hdr hdr;
	struct trans2_resp t2_rsp;
} __attribute__((packed));

#define SMB_INFO_STANDARD                   1
#define SMB_SET_FILE_EA                     2
#define SMB_QUERY_FILE_EA_SIZE              2
#define SMB_INFO_QUERY_EAS_FROM_LIST        3
#define SMB_INFO_QUERY_ALL_EAS              4
#define SMB_INFO_IS_NAME_VALID              6
#define SMB_QUERY_FILE_BASIC_INFO       0x101
#define SMB_QUERY_FILE_STANDARD_INFO    0x102
#define SMB_QUERY_FILE_EA_INFO          0x103
#define SMB_QUERY_FILE_NAME_INFO        0x104
#define SMB_QUERY_FILE_ALLOCATION_INFO  0x105
#define SMB_QUERY_FILE_END_OF_FILEINFO  0x106
#define SMB_QUERY_FILE_ALL_INFO         0x107
#define SMB_QUERY_ALT_NAME_INFO         0x108
#define SMB_QUERY_FILE_STREAM_INFO      0x109
#define SMB_QUERY_FILE_COMPRESSION_INFO 0x10B
#define SMB_QUERY_FILE_UNIX_BASIC       0x200
#define SMB_QUERY_FILE_UNIX_LINK        0x201
#define SMB_QUERY_POSIX_ACL             0x204
#define SMB_QUERY_XATTR                 0x205  
#define SMB_QUERY_ATTR_FLAGS            0x206  
#define SMB_QUERY_POSIX_PERMISSION      0x207
#define SMB_QUERY_POSIX_LOCK            0x208
#define SMB_QUERY_FILE__UNIX_INFO2      0x20b
#define SMB_QUERY_FILE_INTERNAL_INFO    0x3ee
#define SMB_QUERY_FILE_ACCESS_INFO      0x3f0
#define SMB_QUERY_FILE_NAME_INFO2       0x3f1 
#define SMB_QUERY_FILE_POSITION_INFO    0x3f6
#define SMB_QUERY_FILE_MODE_INFO        0x3f8
#define SMB_QUERY_FILE_ALGN_INFO        0x3f9


#define SMB_SET_FILE_BASIC_INFO	        0x101
#define SMB_SET_FILE_DISPOSITION_INFO   0x102
#define SMB_SET_FILE_ALLOCATION_INFO    0x103
#define SMB_SET_FILE_END_OF_FILE_INFO   0x104
#define SMB_SET_FILE_UNIX_BASIC         0x200
#define SMB_SET_FILE_UNIX_LINK          0x201
#define SMB_SET_FILE_UNIX_HLINK         0x203
#define SMB_SET_POSIX_ACL               0x204
#define SMB_SET_XATTR                   0x205
#define SMB_SET_ATTR_FLAGS              0x206  
#define SMB_SET_POSIX_LOCK              0x208
#define SMB_POSIX_OPEN                  0x209
#define SMB_POSIX_UNLINK                0x20a
#define SMB_SET_FILE_UNIX_INFO2         0x20b
#define SMB_SET_FILE_BASIC_INFO2        0x3ec
#define SMB_SET_FILE_RENAME_INFORMATION 0x3f2 
#define SMB_FILE_ALL_INFO2              0x3fa
#define SMB_SET_FILE_ALLOCATION_INFO2   0x3fb
#define SMB_SET_FILE_END_OF_FILE_INFO2  0x3fc
#define SMB_FILE_MOVE_CLUSTER_INFO      0x407
#define SMB_FILE_QUOTA_INFO             0x408
#define SMB_FILE_REPARSEPOINT_INFO      0x409
#define SMB_FILE_MAXIMUM_INFO           0x40d

#define SMB_FIND_FILE_INFO_STANDARD       0x001
#define SMB_FIND_FILE_QUERY_EA_SIZE       0x002
#define SMB_FIND_FILE_QUERY_EAS_FROM_LIST 0x003
#define SMB_FIND_FILE_DIRECTORY_INFO      0x101
#define SMB_FIND_FILE_FULL_DIRECTORY_INFO 0x102
#define SMB_FIND_FILE_NAMES_INFO          0x103
#define SMB_FIND_FILE_BOTH_DIRECTORY_INFO 0x104
#define SMB_FIND_FILE_ID_FULL_DIR_INFO    0x105
#define SMB_FIND_FILE_ID_BOTH_DIR_INFO    0x106
#define SMB_FIND_FILE_UNIX                0x202

typedef struct smb_com_transaction2_qpi_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 DataCount;
	__le16 DataOffset;
	__u8 SetupCount;
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad;
	__le16 InformationLevel;
	__u32 Reserved4;
	char FileName[1];
} __attribute__((packed)) TRANSACTION2_QPI_REQ;

typedef struct smb_com_transaction2_qpi_rsp {
	struct smb_hdr hdr;	
	struct trans2_resp t2;
	__u16 ByteCount;
	__u16 Reserved2; 
} __attribute__((packed)) TRANSACTION2_QPI_RSP;

typedef struct smb_com_transaction2_spi_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 DataCount;
	__le16 DataOffset;
	__u8 SetupCount;
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad;
	__u16 Pad1;
	__le16 InformationLevel;
	__u32 Reserved4;
	char FileName[1];
} __attribute__((packed)) TRANSACTION2_SPI_REQ;

typedef struct smb_com_transaction2_spi_rsp {
	struct smb_hdr hdr;	
	struct trans2_resp t2;
	__u16 ByteCount;
	__u16 Reserved2; 
} __attribute__((packed)) TRANSACTION2_SPI_RSP;

struct set_file_rename {
	__le32 overwrite;   
	__u32 root_fid;   
	__le32 target_name_len;
	char  target_name[0];  
} __attribute__((packed));

struct smb_com_transaction2_sfi_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 DataCount;
	__le16 DataOffset;
	__u8 SetupCount;
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad;
	__u16 Pad1;
	__u16 Fid;
	__le16 InformationLevel;
	__u16 Reserved4;
} __attribute__((packed));

struct smb_com_transaction2_sfi_rsp {
	struct smb_hdr hdr;	
	struct trans2_resp t2;
	__u16 ByteCount;
	__u16 Reserved2;	
} __attribute__((packed));

struct smb_t2_qfi_req {
	struct	smb_hdr hdr;
	struct	trans2_req t2;
	__u8	Pad;
	__u16	Fid;
	__le16	InformationLevel;
} __attribute__((packed));

struct smb_t2_qfi_rsp {
	struct smb_hdr hdr;     
	struct trans2_resp t2;
	__u16 ByteCount;
	__u16 Reserved2;        
} __attribute__((packed));

#define CIFS_SEARCH_CLOSE_ALWAYS  0x0001
#define CIFS_SEARCH_CLOSE_AT_END  0x0002
#define CIFS_SEARCH_RETURN_RESUME 0x0004
#define CIFS_SEARCH_CONTINUE_FROM_LAST 0x0008
#define CIFS_SEARCH_BACKUP_SEARCH 0x0010

#define CIFS_SMB_RESUME_KEY_SIZE 4

typedef struct smb_com_transaction2_ffirst_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 DataCount;
	__le16 DataOffset;
	__u8 SetupCount;	
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad;
	__le16 SearchAttributes;
	__le16 SearchCount;
	__le16 SearchFlags;
	__le16 InformationLevel;
	__le32 SearchStorageType;
	char FileName[1];
} __attribute__((packed)) TRANSACTION2_FFIRST_REQ;

typedef struct smb_com_transaction2_ffirst_rsp {
	struct smb_hdr hdr;	
	struct trans2_resp t2;
	__u16 ByteCount;
} __attribute__((packed)) TRANSACTION2_FFIRST_RSP;

typedef struct smb_com_transaction2_ffirst_rsp_parms {
	__u16 SearchHandle;
	__le16 SearchCount;
	__le16 EndofSearch;
	__le16 EAErrorOffset;
	__le16 LastNameOffset;
} __attribute__((packed)) T2_FFIRST_RSP_PARMS;

typedef struct smb_com_transaction2_fnext_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 DataCount;
	__le16 DataOffset;
	__u8 SetupCount;	
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad;
	__u16 SearchHandle;
	__le16 SearchCount;
	__le16 InformationLevel;
	__u32 ResumeKey;
	__le16 SearchFlags;
	char ResumeFileName[1];
} __attribute__((packed)) TRANSACTION2_FNEXT_REQ;

typedef struct smb_com_transaction2_fnext_rsp {
	struct smb_hdr hdr;	
	struct trans2_resp t2;
	__u16 ByteCount;
} __attribute__((packed)) TRANSACTION2_FNEXT_RSP;

typedef struct smb_com_transaction2_fnext_rsp_parms {
	__le16 SearchCount;
	__le16 EndofSearch;
	__le16 EAErrorOffset;
	__le16 LastNameOffset;
} __attribute__((packed)) T2_FNEXT_RSP_PARMS;

#define SMB_INFO_ALLOCATION         1
#define SMB_INFO_VOLUME             2
#define SMB_QUERY_FS_VOLUME_INFO    0x102
#define SMB_QUERY_FS_SIZE_INFO      0x103
#define SMB_QUERY_FS_DEVICE_INFO    0x104
#define SMB_QUERY_FS_ATTRIBUTE_INFO 0x105
#define SMB_QUERY_CIFS_UNIX_INFO    0x200
#define SMB_QUERY_POSIX_FS_INFO     0x201
#define SMB_QUERY_POSIX_WHO_AM_I    0x202
#define SMB_REQUEST_TRANSPORT_ENCRYPTION 0x203
#define SMB_QUERY_FS_PROXY          0x204 
#define SMB_QUERY_LABEL_INFO        0x3ea
#define SMB_QUERY_FS_QUOTA_INFO     0x3ee
#define SMB_QUERY_FS_FULL_SIZE_INFO 0x3ef
#define SMB_QUERY_OBJECTID_INFO     0x3f0

typedef struct smb_com_transaction2_qfsi_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 DataCount;
	__le16 DataOffset;
	__u8 SetupCount;
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad;
	__le16 InformationLevel;
} __attribute__((packed)) TRANSACTION2_QFSI_REQ;

typedef struct smb_com_transaction_qfsi_rsp {
	struct smb_hdr hdr;	
	struct trans2_resp t2;
	__u16 ByteCount;
	__u8 Pad;	
} __attribute__((packed)) TRANSACTION2_QFSI_RSP;

typedef struct whoami_rsp_data { 
	__u32 flags; 
	__u32 mask; 
	__u64 unix_user_id;
	__u64 unix_user_gid;
	__u32 number_of_supplementary_gids; 
	__u32 number_of_sids; 
	__u32 length_of_sid_array; 
	__u32 pad; 
	  
	  
} __attribute__((packed)) WHOAMI_RSP_DATA;

#define SMB_SET_CIFS_UNIX_INFO    0x200

typedef struct smb_com_transaction2_setfsi_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;	
	__le16 ParameterOffset;
	__le16 DataCount;	
	__le16 DataOffset;
	__u8 SetupCount;	
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad;
	__u16 FileNum;		
	__le16 InformationLevel;
	__le16 ClientUnixMajor; 
	__le16 ClientUnixMinor;
	__le64 ClientUnixCap;   
} __attribute__((packed)) TRANSACTION2_SETFSI_REQ;

typedef struct smb_com_transaction2_setfs_enc_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;	
	__le16 ParameterOffset;
	__le16 DataCount;	
	__le16 DataOffset;
	__u8 SetupCount;	
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad;
	__u16  Reserved4;	
	__le16 InformationLevel;
	
} __attribute__((packed)) TRANSACTION2_SETFSI_ENC_REQ;

typedef struct smb_com_transaction2_setfsi_rsp {
	struct smb_hdr hdr;	
	struct trans2_resp t2;
	__u16 ByteCount;
} __attribute__((packed)) TRANSACTION2_SETFSI_RSP;

typedef struct smb_com_transaction2_get_dfs_refer_req {
	struct smb_hdr hdr;	
	__le16 TotalParameterCount;
	__le16 TotalDataCount;
	__le16 MaxParameterCount;
	__le16 MaxDataCount;
	__u8 MaxSetupCount;
	__u8 Reserved;
	__le16 Flags;
	__le32 Timeout;
	__u16 Reserved2;
	__le16 ParameterCount;
	__le16 ParameterOffset;
	__le16 DataCount;
	__le16 DataOffset;
	__u8 SetupCount;
	__u8 Reserved3;
	__le16 SubCommand;	
	__le16 ByteCount;
	__u8 Pad[3];		
	__le16 MaxReferralLevel;
	char RequestFileName[1];
} __attribute__((packed)) TRANSACTION2_GET_DFS_REFER_REQ;

#define DFS_VERSION cpu_to_le16(0x0003)

#define DFS_TYPE_LINK 0x0000  
#define DFS_TYPE_ROOT 0x0001

#define DFS_NAME_LIST_REF 0x0200 
#define DFS_TARGET_SET_BOUNDARY 0x0400 

typedef struct dfs_referral_level_3 { 
	__le16 VersionNumber;  
	__le16 Size;
	__le16 ServerType; 
	__le16 ReferralEntryFlags;
	__le32 TimeToLive;
	__le16 DfsPathOffset;
	__le16 DfsAlternatePathOffset;
	__le16 NetworkAddressOffset; 
	__u8   ServiceSiteGuid[16];  
} __attribute__((packed)) REFERRAL3;

typedef struct smb_com_transaction_get_dfs_refer_rsp {
	struct smb_hdr hdr;	
	struct trans2_resp t2;
	__u16 ByteCount;
	__u8 Pad;
	__le16 PathConsumed;
	__le16 NumberOfReferrals;
	__le32 DFSFlags;
	REFERRAL3 referrals[1];	
	
} __attribute__((packed)) TRANSACTION2_GET_DFS_REFER_RSP;

#define DFSREF_REFERRAL_SERVER  0x00000001 
#define DFSREF_STORAGE_SERVER   0x00000002 
#define DFSREF_TARGET_FAILBACK  0x00000004 



struct serverInfo {
	char name[16];
	unsigned char versionMajor;
	unsigned char versionMinor;
	unsigned long type;
	unsigned int commentOffset;
} __attribute__((packed));


struct shareInfo {
	char shareName[13];
	char pad;
	unsigned short type;
	unsigned int commentOffset;
} __attribute__((packed));

struct aliasInfo {
	char aliasName[9];
	char pad;
	unsigned int commentOffset;
	unsigned char type[2];
} __attribute__((packed));

struct aliasInfo92 {
	int aliasNameOffset;
	int serverNameOffset;
	int shareNameOffset;
} __attribute__((packed));

typedef struct {
	__le64 TotalAllocationUnits;
	__le64 FreeAllocationUnits;
	__le32 SectorsPerAllocationUnit;
	__le32 BytesPerSector;
} __attribute__((packed)) FILE_SYSTEM_INFO;	

typedef struct {
	__le32 fsid;
	__le32 SectorsPerAllocationUnit;
	__le32 TotalAllocationUnits;
	__le32 FreeAllocationUnits;
	__le16  BytesPerSector;
} __attribute__((packed)) FILE_SYSTEM_ALLOC_INFO;

typedef struct {
	__le16 MajorVersionNumber;
	__le16 MinorVersionNumber;
	__le64 Capability;
} __attribute__((packed)) FILE_SYSTEM_UNIX_INFO; 

#define CIFS_UNIX_MAJOR_VERSION 1
#define CIFS_UNIX_MINOR_VERSION 0

#define CIFS_UNIX_FCNTL_CAP             0x00000001 
#define CIFS_UNIX_POSIX_ACL_CAP         0x00000002 
#define CIFS_UNIX_XATTR_CAP             0x00000004 
#define CIFS_UNIX_EXTATTR_CAP           0x00000008 
#define CIFS_UNIX_POSIX_PATHNAMES_CAP   0x00000010 
#define CIFS_UNIX_POSIX_PATH_OPS_CAP    0x00000020 
#define CIFS_UNIX_LARGE_READ_CAP        0x00000040 
#define CIFS_UNIX_LARGE_WRITE_CAP       0x00000080
#define CIFS_UNIX_TRANSPORT_ENCRYPTION_CAP 0x00000100 
#define CIFS_UNIX_TRANSPORT_ENCRYPTION_MANDATORY_CAP  0x00000200 
#define CIFS_UNIX_PROXY_CAP             0x00000400 
#ifdef CONFIG_CIFS_POSIX
#define CIFS_UNIX_CAP_MASK              0x000003db
#else
#define CIFS_UNIX_CAP_MASK              0x00000013
#endif 


#define CIFS_POSIX_EXTENSIONS           0x00000010 

typedef struct {
	
	__le32 OptimalTransferSize;  
	__le32 BlockSize;
	__le64 TotalBlocks;
	__le64 BlocksAvail;       
	__le64 UserBlocksAvail;   
    
	__le64 TotalFileNodes;
	__le64 FreeFileNodes;
	__le64 FileSysIdentifier;   
	
	
} __attribute__((packed)) FILE_SYSTEM_POSIX_INFO;

#define FILE_DEVICE_CD_ROM              0x00000002
#define FILE_DEVICE_CD_ROM_FILE_SYSTEM  0x00000003
#define FILE_DEVICE_DFS                 0x00000006
#define FILE_DEVICE_DISK                0x00000007
#define FILE_DEVICE_DISK_FILE_SYSTEM    0x00000008
#define FILE_DEVICE_FILE_SYSTEM         0x00000009
#define FILE_DEVICE_NAMED_PIPE          0x00000011
#define FILE_DEVICE_NETWORK             0x00000012
#define FILE_DEVICE_NETWORK_FILE_SYSTEM 0x00000014
#define FILE_DEVICE_NULL                0x00000015
#define FILE_DEVICE_PARALLEL_PORT       0x00000016
#define FILE_DEVICE_PRINTER             0x00000018
#define FILE_DEVICE_SERIAL_PORT         0x0000001b
#define FILE_DEVICE_STREAMS             0x0000001e
#define FILE_DEVICE_TAPE                0x0000001f
#define FILE_DEVICE_TAPE_FILE_SYSTEM    0x00000020
#define FILE_DEVICE_VIRTUAL_DISK        0x00000024
#define FILE_DEVICE_NETWORK_REDIRECTOR  0x00000028

typedef struct {
	__le32 DeviceType;
	__le32 DeviceCharacteristics;
} __attribute__((packed)) FILE_SYSTEM_DEVICE_INFO; 

typedef struct {
	__le32 Attributes;
	__le32 MaxPathNameComponentLength;
	__le32 FileSystemNameLen;
	char FileSystemName[52]; 
} __attribute__((packed)) FILE_SYSTEM_ATTRIBUTE_INFO;

typedef struct { 
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le32 Attributes;
	__u32 Pad1;
	__le64 AllocationSize;
	__le64 EndOfFile;	
	__le32 NumberOfLinks;	
	__u8 DeletePending;
	__u8 Directory;
	__u16 Pad2;
	__u64 IndexNumber;
	__le32 EASize;
	__le32 AccessFlags;
	__u64 IndexNumber1;
	__le64 CurrentByteOffset;
	__le32 Mode;
	__le32 AlignmentRequirement;
	__le32 FileNameLength;
	char FileName[1];
} __attribute__((packed)) FILE_ALL_INFO;	

#define UNIX_FILE      0
#define UNIX_DIR       1
#define UNIX_SYMLINK   2
#define UNIX_CHARDEV   3
#define UNIX_BLOCKDEV  4
#define UNIX_FIFO      5
#define UNIX_SOCKET    6
typedef struct {
	__le64 EndOfFile;
	__le64 NumOfBytes;
	__le64 LastStatusChange; 
	__le64 LastAccessTime;
	__le64 LastModificationTime;
	__le64 Uid;
	__le64 Gid;
	__le32 Type;
	__le64 DevMajor;
	__le64 DevMinor;
	__le64 UniqueId;
	__le64 Permissions;
	__le64 Nlinks;
} __attribute__((packed)) FILE_UNIX_BASIC_INFO;	

typedef struct {
	char LinkDest[1];
} __attribute__((packed)) FILE_UNIX_LINK_INFO;	

typedef struct {
	__u16 Day:5;
	__u16 Month:4;
	__u16 Year:7;
} __attribute__((packed)) SMB_DATE;

typedef struct {
	__u16 TwoSeconds:5;
	__u16 Minutes:6;
	__u16 Hours:5;
} __attribute__((packed)) SMB_TIME;

typedef struct {
	__le16 CreationDate; 
	__le16 CreationTime; 
	__le16 LastAccessDate;
	__le16 LastAccessTime;
	__le16 LastWriteDate;
	__le16 LastWriteTime;
	__le32 DataSize; 
	__le32 AllocationSize;
	__le16 Attributes; 
	__le32 EASize;
} __attribute__((packed)) FILE_INFO_STANDARD;  

typedef struct {
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le32 Attributes;
	__u32 Pad;
} __attribute__((packed)) FILE_BASIC_INFO;	

struct file_allocation_info {
	__le64 AllocationSize; 
} __attribute__((packed));	

struct file_end_of_file_info {
	__le64 FileSize;		
} __attribute__((packed)); 

struct file_alt_name_info {
	__u8   alt_name[1];
} __attribute__((packed));      

struct file_stream_info {
	__le32 number_of_streams;  
};      

struct file_compression_info {
	__le64 compressed_size;
	__le16 format;
	__u8   unit_shift;
	__u8   ch_shift;
	__u8   cl_shift;
	__u8   pad[3];
} __attribute__((packed));      

#define CIFS_ACL_VERSION 1
struct cifs_posix_ace { 
	__u8  cifs_e_tag;
	__u8  cifs_e_perm;
	__le64 cifs_uid; 
} __attribute__((packed));

struct cifs_posix_acl { 
	__le16	version;
	__le16	access_entry_count;  
	__le16	default_entry_count; 
	struct cifs_posix_ace ace_array[0];
} __attribute__((packed));  




#define SMB_O_RDONLY 	 0x1
#define SMB_O_WRONLY 	0x2
#define SMB_O_RDWR 	0x4
#define SMB_O_CREAT 	0x10
#define SMB_O_EXCL 	0x20
#define SMB_O_TRUNC 	0x40
#define SMB_O_APPEND 	0x80
#define SMB_O_SYNC 	0x100
#define SMB_O_DIRECTORY 0x200
#define SMB_O_NOFOLLOW 	0x400
#define SMB_O_DIRECT 	0x800

typedef struct {
	__le32 OpenFlags; 
	__le32 PosixOpenFlags;
	__le64 Permissions;
	__le16 Level; 
} __attribute__((packed)) OPEN_PSX_REQ; 

typedef struct {
	__le16 OplockFlags;
	__u16 Fid;
	__le32 CreateAction;
	__le16 ReturnedLevel;
	__le16 Pad;
	
} __attribute__((packed)) OPEN_PSX_RSP; 

#define SMB_POSIX_UNLINK_FILE_TARGET		0
#define SMB_POSIX_UNLINK_DIRECTORY_TARGET	1

struct unlink_psx_rq { 
	__le16 type;
} __attribute__((packed));

struct file_internal_info {
	__le64  UniqueId; 
} __attribute__((packed));      

struct file_mode_info {
	__le32	Mode;
} __attribute__((packed));      

struct file_attrib_tag {
	__le32 Attribute;
	__le32 ReparseTag;
} __attribute__((packed));      



typedef struct {
	__le32 NextEntryOffset;
	__u32 ResumeKey; 
	FILE_UNIX_BASIC_INFO basic;
	char FileName[1];
} __attribute__((packed)) FILE_UNIX_INFO; 

typedef struct {
	__le32 NextEntryOffset;
	__u32 FileIndex;
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le64 EndOfFile;
	__le64 AllocationSize;
	__le32 ExtFileAttributes;
	__le32 FileNameLength;
	char FileName[1];
} __attribute__((packed)) FILE_DIRECTORY_INFO;   

typedef struct {
	__le32 NextEntryOffset;
	__u32 FileIndex;
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le64 EndOfFile;
	__le64 AllocationSize;
	__le32 ExtFileAttributes;
	__le32 FileNameLength;
	__le32 EaSize; 
	char FileName[1];
} __attribute__((packed)) FILE_FULL_DIRECTORY_INFO; 

typedef struct {
	__le32 NextEntryOffset;
	__u32 FileIndex;
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le64 EndOfFile;
	__le64 AllocationSize;
	__le32 ExtFileAttributes;
	__le32 FileNameLength;
	__le32 EaSize; 
	__le32 Reserved;
	__le64 UniqueId; 
	char FileName[1];
} __attribute__((packed)) SEARCH_ID_FULL_DIR_INFO; 

typedef struct {
	__le32 NextEntryOffset;
	__u32 FileIndex;
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le64 EndOfFile;
	__le64 AllocationSize;
	__le32 ExtFileAttributes;
	__le32 FileNameLength;
	__le32 EaSize; 
	__u8   ShortNameLength;
	__u8   Reserved;
	__u8   ShortName[12];
	char FileName[1];
} __attribute__((packed)) FILE_BOTH_DIRECTORY_INFO; 

typedef struct {
	__u32  ResumeKey;
	__le16 CreationDate; 
	__le16 CreationTime; 
	__le16 LastAccessDate;
	__le16 LastAccessTime;
	__le16 LastWriteDate;
	__le16 LastWriteTime;
	__le32 DataSize; 
	__le32 AllocationSize;
	__le16 Attributes; 
	__u8   FileNameLength;
	char FileName[1];
} __attribute__((packed)) FIND_FILE_STANDARD_INFO; 


struct win_dev {
	unsigned char type[8]; 
	__le64 major;
	__le64 minor;
} __attribute__((packed));

struct gea {
	unsigned char name_len;
	char name[1];
} __attribute__((packed));

struct gealist {
	unsigned long list_len;
	struct gea list[1];
} __attribute__((packed));

struct fea {
	unsigned char EA_flags;
	__u8 name_len;
	__le16 value_len;
	char name[1];
	
} __attribute__((packed));
#define FEA_NEEDEA         0x80	

struct fealist {
	__le32 list_len;
	struct fea list[1];
} __attribute__((packed));

struct data_blob {
	__u8 *data;
	size_t length;
	void (*free) (struct data_blob *data_blob);
} __attribute__((packed));


#ifdef CONFIG_CIFS_POSIX


struct xsymlink {
	
	char signature[4];  
	char cr0;         
	char length[4];
	char cr1;         
	__u8 md5[32];
	char cr2;        
	char path[1024];
} __attribute__((packed));

typedef struct file_xattr_info {
	
	__u32 xattr_name_len;
	__u32 xattr_value_len;
	char  xattr_name[0];
	
} __attribute__((packed)) FILE_XATTR_INFO; 


#define EXT_SECURE_DELETE		0x00000001 
#define EXT_ENABLE_UNDELETE		0x00000002 
#define EXT_SYNCHRONOUS			0x00000008 
#define EXT_IMMUTABLE_FL		0x00000010 
#define EXT_OPEN_APPEND_ONLY		0x00000020 
#define EXT_DO_NOT_BACKUP		0x00000040 
#define EXT_NO_UPDATE_ATIME		0x00000080 
#define EXT_HASH_TREE_INDEXED_DIR	0x00001000 
#define EXT_JOURNAL_THIS_FILE	0x00004000 
#define EXT_SYNCHRONOUS_DIR		0x00010000 
#define EXT_TOPDIR			0x00020000 

#define EXT_SET_MASK			0x000300FF
#define EXT_GET_MASK			0x0003DFFF

typedef struct file_chattr_info {
	__le64	mask; 
	__le64	mode; 
} __attribute__((packed)) FILE_CHATTR_INFO;  
#endif 				
#endif				
