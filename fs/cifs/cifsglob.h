/*
 *   fs/cifs/cifsglob.h
 *
 *   Copyright (C) International Business Machines  Corp., 2002,2008
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *              Jeremy Allison (jra@samba.org)
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
 */
#ifndef _CIFS_GLOB_H
#define _CIFS_GLOB_H

#include <linux/in.h>
#include <linux/in6.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "cifs_fs_sb.h"
#include "cifsacl.h"
#include <crypto/internal/hash.h>
#include <linux/scatterlist.h>

#define MAX_UID_INFO 16
#define MAX_SES_INFO 2
#define MAX_TCON_INFO 4

#define MAX_TREE_SIZE (2 + MAX_SERVER_SIZE + 1 + MAX_SHARE_SIZE + 1)
#define MAX_SERVER_SIZE 15
#define MAX_SHARE_SIZE 80
#define MAX_USERNAME_SIZE 256	
#define MAX_PASSWORD_SIZE 512	

#define CIFS_MIN_RCV_POOL 4

#define CIFS_DEF_ACTIMEO (1 * HZ)

#define CIFS_MAX_ACTIMEO (1 << 30)

#define CIFS_MAX_REQ 32767

#define RFC1001_NAME_LEN 15
#define RFC1001_NAME_LEN_WITH_NULL (RFC1001_NAME_LEN + 1)

#define SERVER_NAME_LENGTH 40
#define SERVER_NAME_LEN_WITH_NULL     (SERVER_NAME_LENGTH + 1)

#define MAX_NAME 514

#include "cifspdu.h"

#ifndef XATTR_DOS_ATTRIB
#define XATTR_DOS_ATTRIB "user.DOSATTRIB"
#endif


enum statusEnum {
	CifsNew = 0,
	CifsGood,
	CifsExiting,
	CifsNeedReconnect,
	CifsNeedNegotiate
};

enum securityEnum {
	LANMAN = 0,			
	NTLM,			
	NTLMv2,			
	RawNTLMSSP,		
 
	Kerberos,		
};

enum protocolEnum {
	TCP = 0,
	SCTP
	
};

struct session_key {
	unsigned int len;
	char *response;
};

struct sdesc {
	struct shash_desc shash;
	char ctx[];
};

struct cifs_secmech {
	struct crypto_shash *hmacmd5; 
	struct crypto_shash *md5; 
	struct sdesc *sdeschmacmd5;  
	struct sdesc *sdescmd5; 
};

struct ntlmssp_auth {
	__u32 client_flags; 
	__u32 server_flags; 
	unsigned char ciphertext[CIFS_CPHTXT_SIZE]; 
	char cryptkey[CIFS_CRYPTO_KEY_SIZE]; 
};

struct cifs_cred {
	int uid;
	int gid;
	int mode;
	int cecount;
	struct cifs_sid osid;
	struct cifs_sid gsid;
	struct cifs_ntace *ntaces;
	struct cifs_ace *aces;
};


struct smb_vol {
	char *username;
	char *password;
	char *domainname;
	char *UNC;
	char *UNCip;
	char *iocharset;  
	char source_rfc1001_name[RFC1001_NAME_LEN_WITH_NULL]; 
	char target_rfc1001_name[RFC1001_NAME_LEN_WITH_NULL]; 
	uid_t cred_uid;
	uid_t linux_uid;
	gid_t linux_gid;
	uid_t backupuid;
	gid_t backupgid;
	umode_t file_mode;
	umode_t dir_mode;
	unsigned secFlg;
	bool retry:1;
	bool intr:1;
	bool setuids:1;
	bool override_uid:1;
	bool override_gid:1;
	bool dynperm:1;
	bool noperm:1;
	bool no_psx_acl:1; 
	bool cifs_acl:1;
	bool backupuid_specified; 
	bool backupgid_specified; 
	bool no_xattr:1;   
	bool server_ino:1; 
	bool direct_io:1;
	bool strict_io:1; 
	bool remap:1;      
	bool posix_paths:1; 
	bool no_linux_ext:1;
	bool sfu_emul:1;
	bool nullauth:1;   
	bool nocase:1;     
	bool nobrl:1;      
	bool mand_lock:1;  
	bool seal:1;       
	bool nodfs:1;      
	bool local_lease:1; 
	bool noblocksnd:1;
	bool noautotune:1;
	bool nostrictsync:1; 
	bool fsc:1;	
	bool mfsymlinks:1; 
	bool multiuser:1;
	bool rwpidforward:1; 
	unsigned int rsize;
	unsigned int wsize;
	bool sockopt_tcp_nodelay:1;
	unsigned short int port;
	unsigned long actimeo; 
	char *prepath;
	struct sockaddr_storage srcaddr; 
	struct nls_table *local_nls;
};

#define CIFS_MOUNT_MASK (CIFS_MOUNT_NO_PERM | CIFS_MOUNT_SET_UID | \
			 CIFS_MOUNT_SERVER_INUM | CIFS_MOUNT_DIRECT_IO | \
			 CIFS_MOUNT_NO_XATTR | CIFS_MOUNT_MAP_SPECIAL_CHR | \
			 CIFS_MOUNT_UNX_EMUL | CIFS_MOUNT_NO_BRL | \
			 CIFS_MOUNT_CIFS_ACL | CIFS_MOUNT_OVERR_UID | \
			 CIFS_MOUNT_OVERR_GID | CIFS_MOUNT_DYNPERM | \
			 CIFS_MOUNT_NOPOSIXBRL | CIFS_MOUNT_NOSSYNC | \
			 CIFS_MOUNT_FSCACHE | CIFS_MOUNT_MF_SYMLINKS | \
			 CIFS_MOUNT_MULTIUSER | CIFS_MOUNT_STRICT_IO | \
			 CIFS_MOUNT_CIFS_BACKUPUID | CIFS_MOUNT_CIFS_BACKUPGID)

#define CIFS_MS_MASK (MS_RDONLY | MS_MANDLOCK | MS_NOEXEC | MS_NOSUID | \
		      MS_NODEV | MS_SYNCHRONOUS)

struct cifs_mnt_data {
	struct cifs_sb_info *cifs_sb;
	struct smb_vol *vol;
	int flags;
};

static inline unsigned int
get_rfc1002_length(void *buf)
{
	return be32_to_cpu(*((__be32 *)buf));
}

struct TCP_Server_Info {
	struct list_head tcp_ses_list;
	struct list_head smb_ses_list;
	int srv_count; 
	
	char server_RFC1001_name[RFC1001_NAME_LEN_WITH_NULL];
	enum statusEnum tcpStatus; 
	char *hostname; 
	struct socket *ssocket;
	struct sockaddr_storage dstaddr;
	struct sockaddr_storage srcaddr; 
#ifdef CONFIG_NET_NS
	struct net *net;
#endif
	wait_queue_head_t response_q;
	wait_queue_head_t request_q; 
	struct list_head pending_mid_q;
	bool noblocksnd;		
	bool noautotune;		
	bool tcp_nodelay;
	int credits;  
	unsigned int in_flight;  
	spinlock_t req_lock;  
	struct mutex srv_mutex;
	struct task_struct *tsk;
	char server_GUID[16];
	char sec_mode;
	bool session_estab; 
	u16 dialect; 
	enum securityEnum secType;
	bool oplocks:1; 
	unsigned int maxReq;	
	
	
	unsigned int maxBuf;	
	
	
	
	unsigned int max_rw;	
	
	
	unsigned int max_vcs;	
	int capabilities; 
	int timeAdj;  
	__u64 CurrentMid;         
	char cryptkey[CIFS_CRYPTO_KEY_SIZE]; 
	
	char workstation_RFC1001_name[RFC1001_NAME_LEN_WITH_NULL];
	__u32 sequence_number; 
	struct session_key session_key;
	unsigned long lstrp; 
	struct cifs_secmech secmech; 
	
	bool	sec_ntlmssp;		
	bool	sec_kerberosu2u;	
	bool	sec_kerberos;		
	bool	sec_mskerberos;		
	bool	large_buf;		
	struct delayed_work	echo; 
	struct kvec *iov;	
	unsigned int nr_iov;	
	char	*smallbuf;	
	char	*bigbuf;	
	unsigned int total_read; 
#ifdef CONFIG_CIFS_FSCACHE
	struct fscache_cookie   *fscache; 
#endif
#ifdef CONFIG_CIFS_STATS2
	atomic_t in_send; 
	atomic_t num_waiters;   
#endif
};

static inline unsigned int
in_flight(struct TCP_Server_Info *server)
{
	unsigned int num;
	spin_lock(&server->req_lock);
	num = server->in_flight;
	spin_unlock(&server->req_lock);
	return num;
}

static inline int*
get_credits_field(struct TCP_Server_Info *server)
{
	return &server->credits;
}

static inline bool
has_credits(struct TCP_Server_Info *server, int *credits)
{
	int num;
	spin_lock(&server->req_lock);
	num = *credits;
	spin_unlock(&server->req_lock);
	return num > 0;
}

static inline size_t
header_size(void)
{
	return sizeof(struct smb_hdr);
}

static inline size_t
max_header_size(void)
{
	return MAX_CIFS_HDR_SIZE;
}


#ifdef CONFIG_NET_NS

static inline struct net *cifs_net_ns(struct TCP_Server_Info *srv)
{
	return srv->net;
}

static inline void cifs_set_net_ns(struct TCP_Server_Info *srv, struct net *net)
{
	srv->net = net;
}

#else

static inline struct net *cifs_net_ns(struct TCP_Server_Info *srv)
{
	return &init_net;
}

static inline void cifs_set_net_ns(struct TCP_Server_Info *srv, struct net *net)
{
}

#endif

struct cifs_ses {
	struct list_head smb_ses_list;
	struct list_head tcon_list;
	struct mutex session_mutex;
	struct TCP_Server_Info *server;	
	int ses_count;		
	enum statusEnum status;
	unsigned overrideSecFlg;  
	__u16 ipc_tid;		
	__u16 flags;
	__u16 vcnum;
	char *serverOS;		
	char *serverNOS;	
	char *serverDomain;	
	int Suid;		
	uid_t linux_uid;        
	uid_t cred_uid;		
	int capabilities;
	char serverName[SERVER_NAME_LEN_WITH_NULL * 2];	
	char *user_name;	
	char *domainName;
	char *password;
	struct session_key auth_key;
	struct ntlmssp_auth *ntlmssp; 
	bool need_reconnect:1; 
};
#define CIFS_SES_NT4 1
#define CIFS_SES_OS2 2
#define CIFS_SES_W9X 4
#define CIFS_SES_LANMAN 8
struct cifs_tcon {
	struct list_head tcon_list;
	int tc_count;
	struct list_head openFileList;
	struct cifs_ses *ses;	
	char treeName[MAX_TREE_SIZE + 1]; 
	char *nativeFileSystem;
	char *password;		
	__u16 tid;		
	__u16 Flags;		
	enum statusEnum tidStatus;
#ifdef CONFIG_CIFS_STATS
	atomic_t num_smbs_sent;
	atomic_t num_writes;
	atomic_t num_reads;
	atomic_t num_flushes;
	atomic_t num_oplock_brks;
	atomic_t num_opens;
	atomic_t num_closes;
	atomic_t num_deletes;
	atomic_t num_mkdirs;
	atomic_t num_posixopens;
	atomic_t num_posixmkdirs;
	atomic_t num_rmdirs;
	atomic_t num_renames;
	atomic_t num_t2renames;
	atomic_t num_ffirst;
	atomic_t num_fnext;
	atomic_t num_fclose;
	atomic_t num_hardlinks;
	atomic_t num_symlinks;
	atomic_t num_locks;
	atomic_t num_acl_get;
	atomic_t num_acl_set;
#ifdef CONFIG_CIFS_STATS2
	unsigned long long time_writes;
	unsigned long long time_reads;
	unsigned long long time_opens;
	unsigned long long time_deletes;
	unsigned long long time_closes;
	unsigned long long time_mkdirs;
	unsigned long long time_rmdirs;
	unsigned long long time_renames;
	unsigned long long time_t2renames;
	unsigned long long time_ffirst;
	unsigned long long time_fnext;
	unsigned long long time_fclose;
#endif 
	__u64    bytes_read;
	__u64    bytes_written;
	spinlock_t stat_lock;
#endif 
	FILE_SYSTEM_DEVICE_INFO fsDevInfo;
	FILE_SYSTEM_ATTRIBUTE_INFO fsAttrInfo; 
	FILE_SYSTEM_UNIX_INFO fsUnixInfo;
	bool ipc:1;		
	bool retry:1;
	bool nocase:1;
	bool seal:1;      
	bool unix_ext:1;  
	bool local_lease:1; 
	bool broken_posix_open; 
	bool need_reconnect:1; 
#ifdef CONFIG_CIFS_FSCACHE
	u64 resource_id;		
	struct fscache_cookie *fscache;	
#endif
	
};

struct tcon_link {
	struct rb_node		tl_rbnode;
	uid_t			tl_uid;
	unsigned long		tl_flags;
#define TCON_LINK_MASTER	0
#define TCON_LINK_PENDING	1
#define TCON_LINK_IN_TREE	2
	unsigned long		tl_time;
	atomic_t		tl_count;
	struct cifs_tcon	*tl_tcon;
};

extern struct tcon_link *cifs_sb_tlink(struct cifs_sb_info *cifs_sb);

static inline struct cifs_tcon *
tlink_tcon(struct tcon_link *tlink)
{
	return tlink->tl_tcon;
}

extern void cifs_put_tlink(struct tcon_link *tlink);

static inline struct tcon_link *
cifs_get_tlink(struct tcon_link *tlink)
{
	if (tlink && !IS_ERR(tlink))
		atomic_inc(&tlink->tl_count);
	return tlink;
}

extern struct cifs_tcon *cifs_sb_master_tcon(struct cifs_sb_info *cifs_sb);

struct cifsLockInfo {
	struct list_head llist;	
	struct list_head blist; 
	wait_queue_head_t block_q;
	__u64 offset;
	__u64 length;
	__u32 pid;
	__u8 type;
	__u16 netfid;
};

struct cifs_search_info {
	loff_t index_of_last_entry;
	__u16 entries_in_buffer;
	__u16 info_level;
	__u32 resume_key;
	char *ntwrk_buf_start;
	char *srch_entries_start;
	char *last_entry;
	const char *presume_name;
	unsigned int resume_name_len;
	bool endOfSearch:1;
	bool emptyDir:1;
	bool unicode:1;
	bool smallBuf:1; 
};

struct cifsFileInfo {
	struct list_head tlist;	
	struct list_head flist;	
	unsigned int uid;	
	__u32 pid;		
	__u16 netfid;		
	 ;
	
	struct dentry *dentry;
	unsigned int f_flags;
	struct tcon_link *tlink;
	bool invalidHandle:1;	
	bool oplock_break_cancelled:1;
	int count;		
	struct mutex fh_mutex; 
	struct cifs_search_info srch_inf;
	struct work_struct oplock_break; 
};

struct cifs_io_parms {
	__u16 netfid;
	__u32 pid;
	__u64 offset;
	unsigned int length;
	struct cifs_tcon *tcon;
};

static inline
struct cifsFileInfo *cifsFileInfo_get(struct cifsFileInfo *cifs_file)
{
	++cifs_file->count;
	return cifs_file;
}

void cifsFileInfo_put(struct cifsFileInfo *cifs_file);


struct cifsInodeInfo {
	struct list_head llist;		
	bool can_cache_brlcks;
	struct mutex lock_mutex;	
	
	struct list_head openFileList;
	__u32 cifsAttrs; 
	bool clientCanCacheRead;	
	bool clientCanCacheAll;		
	bool delete_pending;		
	bool invalid_mapping;		
	unsigned long time;		
	u64  server_eof;		
	u64  uniqueid;			
	u64  createtime;		
#ifdef CONFIG_CIFS_FSCACHE
	struct fscache_cookie *fscache;
#endif
	struct inode vfs_inode;
};

static inline struct cifsInodeInfo *
CIFS_I(struct inode *inode)
{
	return container_of(inode, struct cifsInodeInfo, vfs_inode);
}

static inline struct cifs_sb_info *
CIFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline char CIFS_DIR_SEP(const struct cifs_sb_info *cifs_sb)
{
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_POSIX_PATHS)
		return '/';
	else
		return '\\';
}

static inline void
convert_delimiter(char *path, char delim)
{
	int i;
	char old_delim;

	if (path == NULL)
		return;

	if (delim == '/')
		old_delim = '\\';
	else
		old_delim = '/';

	for (i = 0; path[i] != '\0'; i++) {
		if (path[i] == old_delim)
			path[i] = delim;
	}
}

#ifdef CONFIG_CIFS_STATS
#define cifs_stats_inc atomic_inc

static inline void cifs_stats_bytes_written(struct cifs_tcon *tcon,
					    unsigned int bytes)
{
	if (bytes) {
		spin_lock(&tcon->stat_lock);
		tcon->bytes_written += bytes;
		spin_unlock(&tcon->stat_lock);
	}
}

static inline void cifs_stats_bytes_read(struct cifs_tcon *tcon,
					 unsigned int bytes)
{
	spin_lock(&tcon->stat_lock);
	tcon->bytes_read += bytes;
	spin_unlock(&tcon->stat_lock);
}
#else

#define  cifs_stats_inc(field) do {} while (0)
#define  cifs_stats_bytes_written(tcon, bytes) do {} while (0)
#define  cifs_stats_bytes_read(tcon, bytes) do {} while (0)

#endif

struct mid_q_entry;

typedef int (mid_receive_t)(struct TCP_Server_Info *server,
			    struct mid_q_entry *mid);

typedef void (mid_callback_t)(struct mid_q_entry *mid);

struct mid_q_entry {
	struct list_head qhead;	
	__u64 mid;		
	__u32 pid;		
	__u32 sequence_number;  
	unsigned long when_alloc;  
#ifdef CONFIG_CIFS_STATS2
	unsigned long when_sent; 
	unsigned long when_received; 
#endif
	mid_receive_t *receive; 
	mid_callback_t *callback; 
	void *callback_data;	  
	void *resp_buf;		
	int mid_state;	
	__le16 command;		
	bool large_buf:1;	
	bool multiRsp:1;	
	bool multiEnd:1;	
};

#ifdef CONFIG_CIFS_STATS2

static inline void cifs_in_send_inc(struct TCP_Server_Info *server)
{
	atomic_inc(&server->in_send);
}

static inline void cifs_in_send_dec(struct TCP_Server_Info *server)
{
	atomic_dec(&server->in_send);
}

static inline void cifs_num_waiters_inc(struct TCP_Server_Info *server)
{
	atomic_inc(&server->num_waiters);
}

static inline void cifs_num_waiters_dec(struct TCP_Server_Info *server)
{
	atomic_dec(&server->num_waiters);
}

static inline void cifs_save_when_sent(struct mid_q_entry *mid)
{
	mid->when_sent = jiffies;
}
#else
static inline void cifs_in_send_inc(struct TCP_Server_Info *server)
{
}
static inline void cifs_in_send_dec(struct TCP_Server_Info *server)
{
}

static inline void cifs_num_waiters_inc(struct TCP_Server_Info *server)
{
}

static inline void cifs_num_waiters_dec(struct TCP_Server_Info *server)
{
}

static inline void cifs_save_when_sent(struct mid_q_entry *mid)
{
}
#endif

struct dir_notify_req {
	struct list_head lhead;
	__le16 Pid;
	__le16 PidHigh;
	__u16 Mid;
	__u16 Tid;
	__u16 Uid;
	__u16 netfid;
	__u32 filter; 
	int multishot;
	struct file *pfile;
};

struct dfs_info3_param {
	int flags; 
	int path_consumed;
	int server_type;
	int ref_flag;
	char *path_name;
	char *node_name;
};


#define CIFS_FATTR_DFS_REFERRAL		0x1
#define CIFS_FATTR_DELETE_PENDING	0x2
#define CIFS_FATTR_NEED_REVAL		0x4
#define CIFS_FATTR_INO_COLLISION	0x8

struct cifs_fattr {
	u32		cf_flags;
	u32		cf_cifsattrs;
	u64		cf_uniqueid;
	u64		cf_eof;
	u64		cf_bytes;
	u64		cf_createtime;
	uid_t		cf_uid;
	gid_t		cf_gid;
	umode_t		cf_mode;
	dev_t		cf_rdev;
	unsigned int	cf_nlink;
	unsigned int	cf_dtype;
	struct timespec	cf_atime;
	struct timespec	cf_mtime;
	struct timespec	cf_ctime;
};

static inline void free_dfs_info_param(struct dfs_info3_param *param)
{
	if (param) {
		kfree(param->path_name);
		kfree(param->node_name);
		kfree(param);
	}
}

static inline void free_dfs_info_array(struct dfs_info3_param *param,
				       int number_of_items)
{
	int i;
	if ((number_of_items == 0) || (param == NULL))
		return;
	for (i = 0; i < number_of_items; i++) {
		kfree(param[i].path_name);
		kfree(param[i].node_name);
	}
	kfree(param);
}

#define   MID_FREE 0
#define   MID_REQUEST_ALLOCATED 1
#define   MID_REQUEST_SUBMITTED 2
#define   MID_RESPONSE_RECEIVED 4
#define   MID_RETRY_NEEDED      8 
#define   MID_RESPONSE_MALFORMED 0x10
#define   MID_SHUTDOWN		 0x20

#define   CIFS_NO_BUFFER        0    
#define   CIFS_SMALL_BUFFER     1
#define   CIFS_LARGE_BUFFER     2
#define   CIFS_IOVEC            4    

#define   CIFS_BLOCKING_OP      1    
#define   CIFS_ASYNC_OP         2    
#define   CIFS_TIMEOUT_MASK 0x003    
#define   CIFS_LOG_ERROR    0x010    
#define   CIFS_LARGE_BUF_OP 0x020    
#define   CIFS_NO_RESP      0x040    

#define   CIFSSEC_MAY_SIGN	0x00001
#define   CIFSSEC_MAY_NTLM	0x00002
#define   CIFSSEC_MAY_NTLMV2	0x00004
#define   CIFSSEC_MAY_KRB5	0x00008
#ifdef CONFIG_CIFS_WEAK_PW_HASH
#define   CIFSSEC_MAY_LANMAN	0x00010
#define   CIFSSEC_MAY_PLNTXT	0x00020
#else
#define   CIFSSEC_MAY_LANMAN    0
#define   CIFSSEC_MAY_PLNTXT    0
#endif 
#define   CIFSSEC_MAY_SEAL	0x00040 
#define   CIFSSEC_MAY_NTLMSSP	0x00080 

#define   CIFSSEC_MUST_SIGN	0x01001
#define   CIFSSEC_MUST_NTLM	0x02002
#define   CIFSSEC_MUST_NTLMV2	0x04004
#define   CIFSSEC_MUST_KRB5	0x08008
#ifdef CONFIG_CIFS_WEAK_PW_HASH
#define   CIFSSEC_MUST_LANMAN	0x10010
#define   CIFSSEC_MUST_PLNTXT	0x20020
#ifdef CONFIG_CIFS_UPCALL
#define   CIFSSEC_MASK          0xBF0BF 
#else
#define   CIFSSEC_MASK          0xB70B7 
#endif 
#else 
#define   CIFSSEC_MUST_LANMAN	0
#define   CIFSSEC_MUST_PLNTXT	0
#ifdef CONFIG_CIFS_UPCALL
#define   CIFSSEC_MASK          0x8F08F 
#else
#define	  CIFSSEC_MASK          0x87087 
#endif 
#endif 
#define   CIFSSEC_MUST_SEAL	0x40040 
#define   CIFSSEC_MUST_NTLMSSP	0x80080 

#define   CIFSSEC_DEF (CIFSSEC_MAY_SIGN | CIFSSEC_MAY_NTLM | CIFSSEC_MAY_NTLMV2)
#define   CIFSSEC_MAX (CIFSSEC_MUST_SIGN | CIFSSEC_MUST_NTLMV2)
#define   CIFSSEC_AUTH_MASK (CIFSSEC_MAY_NTLM | CIFSSEC_MAY_NTLMV2 | CIFSSEC_MAY_LANMAN | CIFSSEC_MAY_PLNTXT | CIFSSEC_MAY_KRB5 | CIFSSEC_MAY_NTLMSSP)

#define UID_HASH (16)



#ifdef DECLARE_GLOBALS_HERE
#define GLOBAL_EXTERN
#else
#define GLOBAL_EXTERN extern
#endif

GLOBAL_EXTERN struct list_head		cifs_tcp_ses_list;

GLOBAL_EXTERN spinlock_t		cifs_tcp_ses_lock;

GLOBAL_EXTERN spinlock_t	cifs_file_list_lock;

#ifdef CONFIG_CIFS_DNOTIFY_EXPERIMENTAL 
GLOBAL_EXTERN struct list_head GlobalDnotifyReqList;
GLOBAL_EXTERN struct list_head GlobalDnotifyRsp_Q;
#endif 

GLOBAL_EXTERN unsigned int GlobalCurrentXid;	
GLOBAL_EXTERN unsigned int GlobalTotalActiveXid; 
GLOBAL_EXTERN unsigned int GlobalMaxActiveXid;	
GLOBAL_EXTERN spinlock_t GlobalMid_Lock;  
					  
GLOBAL_EXTERN atomic_t sesInfoAllocCount;
GLOBAL_EXTERN atomic_t tconInfoAllocCount;
GLOBAL_EXTERN atomic_t tcpSesAllocCount;
GLOBAL_EXTERN atomic_t tcpSesReconnectCount;
GLOBAL_EXTERN atomic_t tconInfoReconnectCount;

GLOBAL_EXTERN atomic_t bufAllocCount;    
#ifdef CONFIG_CIFS_STATS2
GLOBAL_EXTERN atomic_t totBufAllocCount; 
GLOBAL_EXTERN atomic_t totSmBufAllocCount;
#endif
GLOBAL_EXTERN atomic_t smBufAllocCount;
GLOBAL_EXTERN atomic_t midCount;

GLOBAL_EXTERN unsigned int multiuser_mount; 
GLOBAL_EXTERN bool enable_oplocks;
GLOBAL_EXTERN unsigned int lookupCacheEnabled;
GLOBAL_EXTERN unsigned int global_secflags;	
GLOBAL_EXTERN unsigned int sign_CIFS_PDUs;  
GLOBAL_EXTERN unsigned int linuxExtEnabled;
GLOBAL_EXTERN unsigned int CIFSMaxBufSize;  
GLOBAL_EXTERN unsigned int cifs_min_rcv;    
GLOBAL_EXTERN unsigned int cifs_min_small;  
GLOBAL_EXTERN unsigned int cifs_max_pending; 

#ifdef CONFIG_CIFS_ACL
GLOBAL_EXTERN struct rb_root uidtree;
GLOBAL_EXTERN struct rb_root gidtree;
GLOBAL_EXTERN spinlock_t siduidlock;
GLOBAL_EXTERN spinlock_t sidgidlock;
GLOBAL_EXTERN struct rb_root siduidtree;
GLOBAL_EXTERN struct rb_root sidgidtree;
GLOBAL_EXTERN spinlock_t uidsidlock;
GLOBAL_EXTERN spinlock_t gidsidlock;
#endif 

void cifs_oplock_break(struct work_struct *work);

extern const struct slow_work_ops cifs_oplock_break_ops;
extern struct workqueue_struct *cifsiod_wq;

#endif	
