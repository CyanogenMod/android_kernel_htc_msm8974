/*
 * security/tomoyo/common.h
 *
 * Header file for TOMOYO.
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#ifndef _SECURITY_TOMOYO_COMMON_H
#define _SECURITY_TOMOYO_COMMON_H

#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/kmod.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/list.h>
#include <linux/cred.h>
#include <linux/poll.h>
#include <linux/binfmts.h>
#include <linux/highmem.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/un.h>
#include <net/sock.h>
#include <net/af_unix.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/udp.h>


#define TOMOYO_HASH_BITS  8
#define TOMOYO_MAX_HASH (1u<<TOMOYO_HASH_BITS)

#define TOMOYO_SOCK_MAX 6

#define TOMOYO_EXEC_TMPSIZE     4096

#define TOMOYO_GC_IN_PROGRESS -1

#define TOMOYO_MAX_PROFILES 256

#define TOMOYO_MAX_ACL_GROUPS 256

enum tomoyo_conditions_index {
	TOMOYO_TASK_UID,             
	TOMOYO_TASK_EUID,            
	TOMOYO_TASK_SUID,            
	TOMOYO_TASK_FSUID,           
	TOMOYO_TASK_GID,             
	TOMOYO_TASK_EGID,            
	TOMOYO_TASK_SGID,            
	TOMOYO_TASK_FSGID,           
	TOMOYO_TASK_PID,             
	TOMOYO_TASK_PPID,            
	TOMOYO_EXEC_ARGC,            
	TOMOYO_EXEC_ENVC,            
	TOMOYO_TYPE_IS_SOCKET,       
	TOMOYO_TYPE_IS_SYMLINK,      
	TOMOYO_TYPE_IS_FILE,         
	TOMOYO_TYPE_IS_BLOCK_DEV,    
	TOMOYO_TYPE_IS_DIRECTORY,    
	TOMOYO_TYPE_IS_CHAR_DEV,     
	TOMOYO_TYPE_IS_FIFO,         
	TOMOYO_MODE_SETUID,          
	TOMOYO_MODE_SETGID,          
	TOMOYO_MODE_STICKY,          
	TOMOYO_MODE_OWNER_READ,      
	TOMOYO_MODE_OWNER_WRITE,     
	TOMOYO_MODE_OWNER_EXECUTE,   
	TOMOYO_MODE_GROUP_READ,      
	TOMOYO_MODE_GROUP_WRITE,     
	TOMOYO_MODE_GROUP_EXECUTE,   
	TOMOYO_MODE_OTHERS_READ,     
	TOMOYO_MODE_OTHERS_WRITE,    
	TOMOYO_MODE_OTHERS_EXECUTE,  
	TOMOYO_EXEC_REALPATH,
	TOMOYO_SYMLINK_TARGET,
	TOMOYO_PATH1_UID,
	TOMOYO_PATH1_GID,
	TOMOYO_PATH1_INO,
	TOMOYO_PATH1_MAJOR,
	TOMOYO_PATH1_MINOR,
	TOMOYO_PATH1_PERM,
	TOMOYO_PATH1_TYPE,
	TOMOYO_PATH1_DEV_MAJOR,
	TOMOYO_PATH1_DEV_MINOR,
	TOMOYO_PATH2_UID,
	TOMOYO_PATH2_GID,
	TOMOYO_PATH2_INO,
	TOMOYO_PATH2_MAJOR,
	TOMOYO_PATH2_MINOR,
	TOMOYO_PATH2_PERM,
	TOMOYO_PATH2_TYPE,
	TOMOYO_PATH2_DEV_MAJOR,
	TOMOYO_PATH2_DEV_MINOR,
	TOMOYO_PATH1_PARENT_UID,
	TOMOYO_PATH1_PARENT_GID,
	TOMOYO_PATH1_PARENT_INO,
	TOMOYO_PATH1_PARENT_PERM,
	TOMOYO_PATH2_PARENT_UID,
	TOMOYO_PATH2_PARENT_GID,
	TOMOYO_PATH2_PARENT_INO,
	TOMOYO_PATH2_PARENT_PERM,
	TOMOYO_MAX_CONDITION_KEYWORD,
	TOMOYO_NUMBER_UNION,
	TOMOYO_NAME_UNION,
	TOMOYO_ARGV_ENTRY,
	TOMOYO_ENVP_ENTRY,
};


enum tomoyo_path_stat_index {
	
	TOMOYO_PATH1,
	TOMOYO_PATH1_PARENT,
	TOMOYO_PATH2,
	TOMOYO_PATH2_PARENT,
	TOMOYO_MAX_PATH_STAT
};

enum tomoyo_mode_index {
	TOMOYO_CONFIG_DISABLED,
	TOMOYO_CONFIG_LEARNING,
	TOMOYO_CONFIG_PERMISSIVE,
	TOMOYO_CONFIG_ENFORCING,
	TOMOYO_CONFIG_MAX_MODE,
	TOMOYO_CONFIG_WANT_REJECT_LOG =  64,
	TOMOYO_CONFIG_WANT_GRANT_LOG  = 128,
	TOMOYO_CONFIG_USE_DEFAULT     = 255,
};

enum tomoyo_policy_id {
	TOMOYO_ID_GROUP,
	TOMOYO_ID_ADDRESS_GROUP,
	TOMOYO_ID_PATH_GROUP,
	TOMOYO_ID_NUMBER_GROUP,
	TOMOYO_ID_TRANSITION_CONTROL,
	TOMOYO_ID_AGGREGATOR,
	TOMOYO_ID_MANAGER,
	TOMOYO_ID_CONDITION,
	TOMOYO_ID_NAME,
	TOMOYO_ID_ACL,
	TOMOYO_ID_DOMAIN,
	TOMOYO_MAX_POLICY
};

enum tomoyo_domain_info_flags_index {
	
	TOMOYO_DIF_QUOTA_WARNED,
	TOMOYO_DIF_TRANSITION_FAILED,
	TOMOYO_MAX_DOMAIN_INFO_FLAGS
};

enum tomoyo_grant_log {
	
	TOMOYO_GRANTLOG_AUTO,
	
	TOMOYO_GRANTLOG_NO,
	
	TOMOYO_GRANTLOG_YES,
};

enum tomoyo_group_id {
	TOMOYO_PATH_GROUP,
	TOMOYO_NUMBER_GROUP,
	TOMOYO_ADDRESS_GROUP,
	TOMOYO_MAX_GROUP
};

enum tomoyo_value_type {
	TOMOYO_VALUE_TYPE_INVALID,
	TOMOYO_VALUE_TYPE_DECIMAL,
	TOMOYO_VALUE_TYPE_OCTAL,
	TOMOYO_VALUE_TYPE_HEXADECIMAL,
};

enum tomoyo_transition_type {
	
	TOMOYO_TRANSITION_CONTROL_NO_RESET,
	TOMOYO_TRANSITION_CONTROL_RESET,
	TOMOYO_TRANSITION_CONTROL_NO_INITIALIZE,
	TOMOYO_TRANSITION_CONTROL_INITIALIZE,
	TOMOYO_TRANSITION_CONTROL_NO_KEEP,
	TOMOYO_TRANSITION_CONTROL_KEEP,
	TOMOYO_MAX_TRANSITION_TYPE
};

enum tomoyo_acl_entry_type_index {
	TOMOYO_TYPE_PATH_ACL,
	TOMOYO_TYPE_PATH2_ACL,
	TOMOYO_TYPE_PATH_NUMBER_ACL,
	TOMOYO_TYPE_MKDEV_ACL,
	TOMOYO_TYPE_MOUNT_ACL,
	TOMOYO_TYPE_INET_ACL,
	TOMOYO_TYPE_UNIX_ACL,
	TOMOYO_TYPE_ENV_ACL,
	TOMOYO_TYPE_MANUAL_TASK_ACL,
};

enum tomoyo_path_acl_index {
	TOMOYO_TYPE_EXECUTE,
	TOMOYO_TYPE_READ,
	TOMOYO_TYPE_WRITE,
	TOMOYO_TYPE_APPEND,
	TOMOYO_TYPE_UNLINK,
	TOMOYO_TYPE_GETATTR,
	TOMOYO_TYPE_RMDIR,
	TOMOYO_TYPE_TRUNCATE,
	TOMOYO_TYPE_SYMLINK,
	TOMOYO_TYPE_CHROOT,
	TOMOYO_TYPE_UMOUNT,
	TOMOYO_MAX_PATH_OPERATION
};

enum tomoyo_memory_stat_type {
	TOMOYO_MEMORY_POLICY,
	TOMOYO_MEMORY_AUDIT,
	TOMOYO_MEMORY_QUERY,
	TOMOYO_MAX_MEMORY_STAT
};

enum tomoyo_mkdev_acl_index {
	TOMOYO_TYPE_MKBLOCK,
	TOMOYO_TYPE_MKCHAR,
	TOMOYO_MAX_MKDEV_OPERATION
};

enum tomoyo_network_acl_index {
	TOMOYO_NETWORK_BIND,    
	TOMOYO_NETWORK_LISTEN,  
	TOMOYO_NETWORK_CONNECT, 
	TOMOYO_NETWORK_SEND,    
	TOMOYO_MAX_NETWORK_OPERATION
};

enum tomoyo_path2_acl_index {
	TOMOYO_TYPE_LINK,
	TOMOYO_TYPE_RENAME,
	TOMOYO_TYPE_PIVOT_ROOT,
	TOMOYO_MAX_PATH2_OPERATION
};

enum tomoyo_path_number_acl_index {
	TOMOYO_TYPE_CREATE,
	TOMOYO_TYPE_MKDIR,
	TOMOYO_TYPE_MKFIFO,
	TOMOYO_TYPE_MKSOCK,
	TOMOYO_TYPE_IOCTL,
	TOMOYO_TYPE_CHMOD,
	TOMOYO_TYPE_CHOWN,
	TOMOYO_TYPE_CHGRP,
	TOMOYO_MAX_PATH_NUMBER_OPERATION
};

enum tomoyo_securityfs_interface_index {
	TOMOYO_DOMAINPOLICY,
	TOMOYO_EXCEPTIONPOLICY,
	TOMOYO_PROCESS_STATUS,
	TOMOYO_STAT,
	TOMOYO_AUDIT,
	TOMOYO_VERSION,
	TOMOYO_PROFILE,
	TOMOYO_QUERY,
	TOMOYO_MANAGER
};

enum tomoyo_special_mount {
	TOMOYO_MOUNT_BIND,            
	TOMOYO_MOUNT_MOVE,            
	TOMOYO_MOUNT_REMOUNT,         
	TOMOYO_MOUNT_MAKE_UNBINDABLE, 
	TOMOYO_MOUNT_MAKE_PRIVATE,    
	TOMOYO_MOUNT_MAKE_SLAVE,      
	TOMOYO_MOUNT_MAKE_SHARED,     
	TOMOYO_MAX_SPECIAL_MOUNT
};

enum tomoyo_mac_index {
	TOMOYO_MAC_FILE_EXECUTE,
	TOMOYO_MAC_FILE_OPEN,
	TOMOYO_MAC_FILE_CREATE,
	TOMOYO_MAC_FILE_UNLINK,
	TOMOYO_MAC_FILE_GETATTR,
	TOMOYO_MAC_FILE_MKDIR,
	TOMOYO_MAC_FILE_RMDIR,
	TOMOYO_MAC_FILE_MKFIFO,
	TOMOYO_MAC_FILE_MKSOCK,
	TOMOYO_MAC_FILE_TRUNCATE,
	TOMOYO_MAC_FILE_SYMLINK,
	TOMOYO_MAC_FILE_MKBLOCK,
	TOMOYO_MAC_FILE_MKCHAR,
	TOMOYO_MAC_FILE_LINK,
	TOMOYO_MAC_FILE_RENAME,
	TOMOYO_MAC_FILE_CHMOD,
	TOMOYO_MAC_FILE_CHOWN,
	TOMOYO_MAC_FILE_CHGRP,
	TOMOYO_MAC_FILE_IOCTL,
	TOMOYO_MAC_FILE_CHROOT,
	TOMOYO_MAC_FILE_MOUNT,
	TOMOYO_MAC_FILE_UMOUNT,
	TOMOYO_MAC_FILE_PIVOT_ROOT,
	TOMOYO_MAC_NETWORK_INET_STREAM_BIND,
	TOMOYO_MAC_NETWORK_INET_STREAM_LISTEN,
	TOMOYO_MAC_NETWORK_INET_STREAM_CONNECT,
	TOMOYO_MAC_NETWORK_INET_DGRAM_BIND,
	TOMOYO_MAC_NETWORK_INET_DGRAM_SEND,
	TOMOYO_MAC_NETWORK_INET_RAW_BIND,
	TOMOYO_MAC_NETWORK_INET_RAW_SEND,
	TOMOYO_MAC_NETWORK_UNIX_STREAM_BIND,
	TOMOYO_MAC_NETWORK_UNIX_STREAM_LISTEN,
	TOMOYO_MAC_NETWORK_UNIX_STREAM_CONNECT,
	TOMOYO_MAC_NETWORK_UNIX_DGRAM_BIND,
	TOMOYO_MAC_NETWORK_UNIX_DGRAM_SEND,
	TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_BIND,
	TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_LISTEN,
	TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_CONNECT,
	TOMOYO_MAC_ENVIRON,
	TOMOYO_MAX_MAC_INDEX
};

enum tomoyo_mac_category_index {
	TOMOYO_MAC_CATEGORY_FILE,
	TOMOYO_MAC_CATEGORY_NETWORK,
	TOMOYO_MAC_CATEGORY_MISC,
	TOMOYO_MAX_MAC_CATEGORY_INDEX
};

#define TOMOYO_RETRY_REQUEST 1

enum tomoyo_policy_stat_type {
	
	TOMOYO_STAT_POLICY_UPDATES,
	TOMOYO_STAT_POLICY_LEARNING,   
	TOMOYO_STAT_POLICY_PERMISSIVE, 
	TOMOYO_STAT_POLICY_ENFORCING,  
	TOMOYO_MAX_POLICY_STAT
};

enum tomoyo_pref_index {
	TOMOYO_PREF_MAX_AUDIT_LOG,
	TOMOYO_PREF_MAX_LEARNING_ENTRY,
	TOMOYO_MAX_PREF
};


struct tomoyo_acl_head {
	struct list_head list;
	s8 is_deleted; 
} __packed;

struct tomoyo_shared_acl_head {
	struct list_head list;
	atomic_t users;
} __packed;

struct tomoyo_policy_namespace;

struct tomoyo_request_info {
	struct tomoyo_obj_info *obj;
	struct tomoyo_execve *ee;
	struct tomoyo_domain_info *domain;
	
	union {
		struct {
			const struct tomoyo_path_info *filename;
			
			const struct tomoyo_path_info *matched_path;
			
			u8 operation;
		} path;
		struct {
			const struct tomoyo_path_info *filename1;
			const struct tomoyo_path_info *filename2;
			
			u8 operation;
		} path2;
		struct {
			const struct tomoyo_path_info *filename;
			unsigned int mode;
			unsigned int major;
			unsigned int minor;
			
			u8 operation;
		} mkdev;
		struct {
			const struct tomoyo_path_info *filename;
			unsigned long number;
			u8 operation;
		} path_number;
		struct {
			const struct tomoyo_path_info *name;
		} environ;
		struct {
			const __be32 *address;
			u16 port;
			
			u8 protocol;
			
			u8 operation;
			bool is_ipv6;
		} inet_network;
		struct {
			const struct tomoyo_path_info *address;
			
			u8 protocol;
			
			u8 operation;
		} unix_network;
		struct {
			const struct tomoyo_path_info *type;
			const struct tomoyo_path_info *dir;
			const struct tomoyo_path_info *dev;
			unsigned long flags;
			int need_dev;
		} mount;
		struct {
			const struct tomoyo_path_info *domainname;
		} task;
	} param;
	struct tomoyo_acl_info *matched_acl;
	u8 param_type;
	bool granted;
	u8 retry;
	u8 profile;
	u8 mode; 
	u8 type;
};

struct tomoyo_path_info {
	const char *name;
	u32 hash;          
	u16 const_len;     
	bool is_dir;       
	bool is_patterned; 
};

struct tomoyo_name {
	struct tomoyo_shared_acl_head head;
	struct tomoyo_path_info entry;
};

struct tomoyo_name_union {
	
	const struct tomoyo_path_info *filename;
	struct tomoyo_group *group;
};

struct tomoyo_number_union {
	unsigned long values[2];
	struct tomoyo_group *group; 
	
	u8 value_type[2];
};

struct tomoyo_ipaddr_union {
	struct in6_addr ip[2]; 
	struct tomoyo_group *group; 
	bool is_ipv6; 
};

struct tomoyo_group {
	struct tomoyo_shared_acl_head head;
	const struct tomoyo_path_info *group_name;
	struct list_head member_list;
};

struct tomoyo_path_group {
	struct tomoyo_acl_head head;
	const struct tomoyo_path_info *member_name;
};

struct tomoyo_number_group {
	struct tomoyo_acl_head head;
	struct tomoyo_number_union number;
};

struct tomoyo_address_group {
	struct tomoyo_acl_head head;
	
	struct tomoyo_ipaddr_union address;
};

struct tomoyo_mini_stat {
	uid_t uid;
	gid_t gid;
	ino_t ino;
	umode_t mode;
	dev_t dev;
	dev_t rdev;
};

struct tomoyo_page_dump {
	struct page *page;    
	char *data;           
};

struct tomoyo_obj_info {
	bool validate_done;
	
	bool stat_valid[TOMOYO_MAX_PATH_STAT];
	
	struct path path1;
	
	struct path path2;
	struct tomoyo_mini_stat stat[TOMOYO_MAX_PATH_STAT];
	struct tomoyo_path_info *symlink_target;
};

struct tomoyo_argv {
	unsigned long index;
	const struct tomoyo_path_info *value;
	bool is_not;
};

struct tomoyo_envp {
	const struct tomoyo_path_info *name;
	const struct tomoyo_path_info *value;
	bool is_not;
};

struct tomoyo_execve {
	struct tomoyo_request_info r;
	struct tomoyo_obj_info obj;
	struct linux_binprm *bprm;
	const struct tomoyo_path_info *transition;
	
	struct tomoyo_page_dump dump;
	
	char *tmp; 
};

struct tomoyo_condition_element {
	u8 left;
	u8 right;
	
	bool equals;
};

struct tomoyo_condition {
	struct tomoyo_shared_acl_head head;
	u32 size; 
	u16 condc; 
	u16 numbers_count; 
	u16 names_count; 
	u16 argc; 
	u16 envc; 
	u8 grant_log; 
	const struct tomoyo_path_info *transit; 
};

struct tomoyo_acl_info {
	struct list_head list;
	struct tomoyo_condition *cond; 
	s8 is_deleted; 
	u8 type; 
} __packed;

struct tomoyo_domain_info {
	struct list_head list;
	struct list_head acl_info_list;
	
	const struct tomoyo_path_info *domainname;
	
	struct tomoyo_policy_namespace *ns;
	u8 profile;        
	u8 group;          
	bool is_deleted;   
	bool flags[TOMOYO_MAX_DOMAIN_INFO_FLAGS];
	atomic_t users; 
};

struct tomoyo_task_acl {
	struct tomoyo_acl_info head; 
	
	const struct tomoyo_path_info *domainname;
};

struct tomoyo_path_acl {
	struct tomoyo_acl_info head; 
	u16 perm; 
	struct tomoyo_name_union name;
};

struct tomoyo_path_number_acl {
	struct tomoyo_acl_info head; 
	
	u8 perm;
	struct tomoyo_name_union name;
	struct tomoyo_number_union number;
};

struct tomoyo_mkdev_acl {
	struct tomoyo_acl_info head; 
	u8 perm; 
	struct tomoyo_name_union name;
	struct tomoyo_number_union mode;
	struct tomoyo_number_union major;
	struct tomoyo_number_union minor;
};

struct tomoyo_path2_acl {
	struct tomoyo_acl_info head; 
	u8 perm; 
	struct tomoyo_name_union name1;
	struct tomoyo_name_union name2;
};

struct tomoyo_mount_acl {
	struct tomoyo_acl_info head; 
	struct tomoyo_name_union dev_name;
	struct tomoyo_name_union dir_name;
	struct tomoyo_name_union fs_type;
	struct tomoyo_number_union flags;
};

struct tomoyo_env_acl {
	struct tomoyo_acl_info head;        
	const struct tomoyo_path_info *env; 
};

struct tomoyo_inet_acl {
	struct tomoyo_acl_info head; 
	u8 protocol;
	u8 perm; 
	struct tomoyo_ipaddr_union address;
	struct tomoyo_number_union port;
};

struct tomoyo_unix_acl {
	struct tomoyo_acl_info head; 
	u8 protocol;
	u8 perm; 
	struct tomoyo_name_union name;
};

struct tomoyo_acl_param {
	char *data;
	struct list_head *list;
	struct tomoyo_policy_namespace *ns;
	bool is_delete;
};

#define TOMOYO_MAX_IO_READ_QUEUE 64

struct tomoyo_io_buffer {
	void (*read) (struct tomoyo_io_buffer *);
	int (*write) (struct tomoyo_io_buffer *);
	unsigned int (*poll) (struct file *file, poll_table *wait);
	
	struct mutex io_sem;
	char __user *read_user_buf;
	size_t read_user_buf_avail;
	struct {
		struct list_head *ns;
		struct list_head *domain;
		struct list_head *group;
		struct list_head *acl;
		size_t avail;
		unsigned int step;
		unsigned int query_index;
		u16 index;
		u16 cond_index;
		u8 acl_group_index;
		u8 cond_step;
		u8 bit;
		u8 w_pos;
		bool eof;
		bool print_this_domain_only;
		bool print_transition_related_only;
		bool print_cond_part;
		const char *w[TOMOYO_MAX_IO_READ_QUEUE];
	} r;
	struct {
		struct tomoyo_policy_namespace *ns;
		
		struct tomoyo_domain_info *domain;
		
		size_t avail;
		bool is_delete;
	} w;
	
	char *read_buf;
	
	size_t readbuf_size;
	
	char *write_buf;
	
	size_t writebuf_size;
	
	enum tomoyo_securityfs_interface_index type;
	
	u8 users;
	
	struct list_head list;
};

struct tomoyo_transition_control {
	struct tomoyo_acl_head head;
	u8 type; 
	
	bool is_last_name;
	const struct tomoyo_path_info *domainname; 
	const struct tomoyo_path_info *program;    
};

struct tomoyo_aggregator {
	struct tomoyo_acl_head head;
	const struct tomoyo_path_info *original_name;
	const struct tomoyo_path_info *aggregated_name;
};

struct tomoyo_manager {
	struct tomoyo_acl_head head;
	bool is_domain;  
	
	const struct tomoyo_path_info *manager;
};

struct tomoyo_preference {
	unsigned int learning_max_entry;
	bool enforcing_verbose;
	bool learning_verbose;
	bool permissive_verbose;
};

struct tomoyo_profile {
	const struct tomoyo_path_info *comment;
	struct tomoyo_preference *learning;
	struct tomoyo_preference *permissive;
	struct tomoyo_preference *enforcing;
	struct tomoyo_preference preference;
	u8 default_config;
	u8 config[TOMOYO_MAX_MAC_INDEX + TOMOYO_MAX_MAC_CATEGORY_INDEX];
	unsigned int pref[TOMOYO_MAX_PREF];
};

struct tomoyo_time {
	u16 year;
	u8 month;
	u8 day;
	u8 hour;
	u8 min;
	u8 sec;
};

struct tomoyo_policy_namespace {
	
	struct tomoyo_profile *profile_ptr[TOMOYO_MAX_PROFILES];
	
	struct list_head group_list[TOMOYO_MAX_GROUP];
	
	struct list_head policy_list[TOMOYO_MAX_POLICY];
	
	struct list_head acl_group[TOMOYO_MAX_ACL_GROUPS];
	
	struct list_head namespace_list;
	
	unsigned int profile_version;
	
	const char *name;
};


bool tomoyo_address_matches_group(const bool is_ipv6, const __be32 *address,
				  const struct tomoyo_group *group);
bool tomoyo_compare_number_union(const unsigned long value,
				 const struct tomoyo_number_union *ptr);
bool tomoyo_condition(struct tomoyo_request_info *r,
		      const struct tomoyo_condition *cond);
bool tomoyo_correct_domain(const unsigned char *domainname);
bool tomoyo_correct_path(const char *filename);
bool tomoyo_correct_word(const char *string);
bool tomoyo_domain_def(const unsigned char *buffer);
bool tomoyo_domain_quota_is_ok(struct tomoyo_request_info *r);
bool tomoyo_dump_page(struct linux_binprm *bprm, unsigned long pos,
		      struct tomoyo_page_dump *dump);
bool tomoyo_memory_ok(void *ptr);
bool tomoyo_number_matches_group(const unsigned long min,
				 const unsigned long max,
				 const struct tomoyo_group *group);
bool tomoyo_parse_ipaddr_union(struct tomoyo_acl_param *param,
			       struct tomoyo_ipaddr_union *ptr);
bool tomoyo_parse_name_union(struct tomoyo_acl_param *param,
			     struct tomoyo_name_union *ptr);
bool tomoyo_parse_number_union(struct tomoyo_acl_param *param,
			       struct tomoyo_number_union *ptr);
bool tomoyo_path_matches_pattern(const struct tomoyo_path_info *filename,
				 const struct tomoyo_path_info *pattern);
bool tomoyo_permstr(const char *string, const char *keyword);
bool tomoyo_str_starts(char **src, const char *find);
char *tomoyo_encode(const char *str);
char *tomoyo_encode2(const char *str, int str_len);
char *tomoyo_init_log(struct tomoyo_request_info *r, int len, const char *fmt,
		      va_list args);
char *tomoyo_read_token(struct tomoyo_acl_param *param);
char *tomoyo_realpath_from_path(struct path *path);
char *tomoyo_realpath_nofollow(const char *pathname);
const char *tomoyo_get_exe(void);
const char *tomoyo_yesno(const unsigned int value);
const struct tomoyo_path_info *tomoyo_compare_name_union
(const struct tomoyo_path_info *name, const struct tomoyo_name_union *ptr);
const struct tomoyo_path_info *tomoyo_get_domainname
(struct tomoyo_acl_param *param);
const struct tomoyo_path_info *tomoyo_get_name(const char *name);
const struct tomoyo_path_info *tomoyo_path_matches_group
(const struct tomoyo_path_info *pathname, const struct tomoyo_group *group);
int tomoyo_check_open_permission(struct tomoyo_domain_info *domain,
				 struct path *path, const int flag);
int tomoyo_close_control(struct tomoyo_io_buffer *head);
int tomoyo_env_perm(struct tomoyo_request_info *r, const char *env);
int tomoyo_execute_permission(struct tomoyo_request_info *r,
			      const struct tomoyo_path_info *filename);
int tomoyo_find_next_domain(struct linux_binprm *bprm);
int tomoyo_get_mode(const struct tomoyo_policy_namespace *ns, const u8 profile,
		    const u8 index);
int tomoyo_init_request_info(struct tomoyo_request_info *r,
			     struct tomoyo_domain_info *domain,
			     const u8 index);
int tomoyo_mkdev_perm(const u8 operation, struct path *path,
		      const unsigned int mode, unsigned int dev);
int tomoyo_mount_permission(char *dev_name, struct path *path,
			    const char *type, unsigned long flags,
			    void *data_page);
int tomoyo_open_control(const u8 type, struct file *file);
int tomoyo_path2_perm(const u8 operation, struct path *path1,
		      struct path *path2);
int tomoyo_path_number_perm(const u8 operation, struct path *path,
			    unsigned long number);
int tomoyo_path_perm(const u8 operation, struct path *path,
		     const char *target);
unsigned int tomoyo_poll_control(struct file *file, poll_table *wait);
unsigned int tomoyo_poll_log(struct file *file, poll_table *wait);
int tomoyo_socket_bind_permission(struct socket *sock, struct sockaddr *addr,
				  int addr_len);
int tomoyo_socket_connect_permission(struct socket *sock,
				     struct sockaddr *addr, int addr_len);
int tomoyo_socket_listen_permission(struct socket *sock);
int tomoyo_socket_sendmsg_permission(struct socket *sock, struct msghdr *msg,
				     int size);
int tomoyo_supervisor(struct tomoyo_request_info *r, const char *fmt, ...)
	__printf(2, 3);
int tomoyo_update_domain(struct tomoyo_acl_info *new_entry, const int size,
			 struct tomoyo_acl_param *param,
			 bool (*check_duplicate)
			 (const struct tomoyo_acl_info *,
			  const struct tomoyo_acl_info *),
			 bool (*merge_duplicate)
			 (struct tomoyo_acl_info *, struct tomoyo_acl_info *,
			  const bool));
int tomoyo_update_policy(struct tomoyo_acl_head *new_entry, const int size,
			 struct tomoyo_acl_param *param,
			 bool (*check_duplicate)
			 (const struct tomoyo_acl_head *,
			  const struct tomoyo_acl_head *));
int tomoyo_write_aggregator(struct tomoyo_acl_param *param);
int tomoyo_write_file(struct tomoyo_acl_param *param);
int tomoyo_write_group(struct tomoyo_acl_param *param, const u8 type);
int tomoyo_write_misc(struct tomoyo_acl_param *param);
int tomoyo_write_inet_network(struct tomoyo_acl_param *param);
int tomoyo_write_transition_control(struct tomoyo_acl_param *param,
				    const u8 type);
int tomoyo_write_unix_network(struct tomoyo_acl_param *param);
ssize_t tomoyo_read_control(struct tomoyo_io_buffer *head, char __user *buffer,
			    const int buffer_len);
ssize_t tomoyo_write_control(struct tomoyo_io_buffer *head,
			     const char __user *buffer, const int buffer_len);
struct tomoyo_condition *tomoyo_get_condition(struct tomoyo_acl_param *param);
struct tomoyo_domain_info *tomoyo_assign_domain(const char *domainname,
						const bool transit);
struct tomoyo_domain_info *tomoyo_find_domain(const char *domainname);
struct tomoyo_group *tomoyo_get_group(struct tomoyo_acl_param *param,
				      const u8 idx);
struct tomoyo_policy_namespace *tomoyo_assign_namespace
(const char *domainname);
struct tomoyo_profile *tomoyo_profile(const struct tomoyo_policy_namespace *ns,
				      const u8 profile);
unsigned int tomoyo_check_flags(const struct tomoyo_domain_info *domain,
				const u8 index);
u8 tomoyo_parse_ulong(unsigned long *result, char **str);
void *tomoyo_commit_ok(void *data, const unsigned int size);
void __init tomoyo_load_builtin_policy(void);
void __init tomoyo_mm_init(void);
void tomoyo_check_acl(struct tomoyo_request_info *r,
		      bool (*check_entry) (struct tomoyo_request_info *,
					   const struct tomoyo_acl_info *));
void tomoyo_check_profile(void);
void tomoyo_convert_time(time_t time, struct tomoyo_time *stamp);
void tomoyo_del_condition(struct list_head *element);
void tomoyo_fill_path_info(struct tomoyo_path_info *ptr);
void tomoyo_get_attributes(struct tomoyo_obj_info *obj);
void tomoyo_init_policy_namespace(struct tomoyo_policy_namespace *ns);
void tomoyo_load_policy(const char *filename);
void tomoyo_normalize_line(unsigned char *buffer);
void tomoyo_notify_gc(struct tomoyo_io_buffer *head, const bool is_register);
void tomoyo_print_ip(char *buf, const unsigned int size,
		     const struct tomoyo_ipaddr_union *ptr);
void tomoyo_print_ulong(char *buffer, const int buffer_len,
			const unsigned long value, const u8 type);
void tomoyo_put_name_union(struct tomoyo_name_union *ptr);
void tomoyo_put_number_union(struct tomoyo_number_union *ptr);
void tomoyo_read_log(struct tomoyo_io_buffer *head);
void tomoyo_update_stat(const u8 index);
void tomoyo_warn_oom(const char *function);
void tomoyo_write_log(struct tomoyo_request_info *r, const char *fmt, ...)
	__printf(2, 3);
void tomoyo_write_log2(struct tomoyo_request_info *r, int len, const char *fmt,
		       va_list args);


extern bool tomoyo_policy_loaded;
extern const char * const tomoyo_condition_keyword
[TOMOYO_MAX_CONDITION_KEYWORD];
extern const char * const tomoyo_dif[TOMOYO_MAX_DOMAIN_INFO_FLAGS];
extern const char * const tomoyo_mac_keywords[TOMOYO_MAX_MAC_INDEX
					      + TOMOYO_MAX_MAC_CATEGORY_INDEX];
extern const char * const tomoyo_mode[TOMOYO_CONFIG_MAX_MODE];
extern const char * const tomoyo_path_keyword[TOMOYO_MAX_PATH_OPERATION];
extern const char * const tomoyo_proto_keyword[TOMOYO_SOCK_MAX];
extern const char * const tomoyo_socket_keyword[TOMOYO_MAX_NETWORK_OPERATION];
extern const u8 tomoyo_index2category[TOMOYO_MAX_MAC_INDEX];
extern const u8 tomoyo_pn2mac[TOMOYO_MAX_PATH_NUMBER_OPERATION];
extern const u8 tomoyo_pnnn2mac[TOMOYO_MAX_MKDEV_OPERATION];
extern const u8 tomoyo_pp2mac[TOMOYO_MAX_PATH2_OPERATION];
extern struct list_head tomoyo_condition_list;
extern struct list_head tomoyo_domain_list;
extern struct list_head tomoyo_name_list[TOMOYO_MAX_HASH];
extern struct list_head tomoyo_namespace_list;
extern struct mutex tomoyo_policy_lock;
extern struct srcu_struct tomoyo_ss;
extern struct tomoyo_domain_info tomoyo_kernel_domain;
extern struct tomoyo_policy_namespace tomoyo_kernel_namespace;
extern unsigned int tomoyo_memory_quota[TOMOYO_MAX_MEMORY_STAT];
extern unsigned int tomoyo_memory_used[TOMOYO_MAX_MEMORY_STAT];


static inline int tomoyo_read_lock(void)
{
	return srcu_read_lock(&tomoyo_ss);
}

static inline void tomoyo_read_unlock(int idx)
{
	srcu_read_unlock(&tomoyo_ss, idx);
}

static inline pid_t tomoyo_sys_getppid(void)
{
	pid_t pid;
	rcu_read_lock();
	pid = task_tgid_vnr(rcu_dereference(current->real_parent));
	rcu_read_unlock();
	return pid;
}

static inline pid_t tomoyo_sys_getpid(void)
{
	return task_tgid_vnr(current);
}

static inline bool tomoyo_pathcmp(const struct tomoyo_path_info *a,
				  const struct tomoyo_path_info *b)
{
	return a->hash != b->hash || strcmp(a->name, b->name);
}

static inline void tomoyo_put_name(const struct tomoyo_path_info *name)
{
	if (name) {
		struct tomoyo_name *ptr =
			container_of(name, typeof(*ptr), entry);
		atomic_dec(&ptr->head.users);
	}
}

static inline void tomoyo_put_condition(struct tomoyo_condition *cond)
{
	if (cond)
		atomic_dec(&cond->head.users);
}

static inline void tomoyo_put_group(struct tomoyo_group *group)
{
	if (group)
		atomic_dec(&group->head.users);
}

static inline struct tomoyo_domain_info *tomoyo_domain(void)
{
	return current_cred()->security;
}

static inline struct tomoyo_domain_info *tomoyo_real_domain(struct task_struct
							    *task)
{
	return task_cred_xxx(task, security);
}

static inline bool tomoyo_same_name_union
(const struct tomoyo_name_union *a, const struct tomoyo_name_union *b)
{
	return a->filename == b->filename && a->group == b->group;
}

static inline bool tomoyo_same_number_union
(const struct tomoyo_number_union *a, const struct tomoyo_number_union *b)
{
	return a->values[0] == b->values[0] && a->values[1] == b->values[1] &&
		a->group == b->group && a->value_type[0] == b->value_type[0] &&
		a->value_type[1] == b->value_type[1];
}

static inline bool tomoyo_same_ipaddr_union
(const struct tomoyo_ipaddr_union *a, const struct tomoyo_ipaddr_union *b)
{
	return !memcmp(a->ip, b->ip, sizeof(a->ip)) && a->group == b->group &&
		a->is_ipv6 == b->is_ipv6;
}

static inline struct tomoyo_policy_namespace *tomoyo_current_namespace(void)
{
	return tomoyo_domain()->ns;
}

#if defined(CONFIG_SLOB)

static inline int tomoyo_round2(size_t size)
{
	return size;
}

#else

static inline int tomoyo_round2(size_t size)
{
#if PAGE_SIZE == 4096
	size_t bsize = 32;
#else
	size_t bsize = 64;
#endif
	if (!size)
		return 0;
	while (size > bsize)
		bsize <<= 1;
	return bsize;
}

#endif

#define list_for_each_cookie(pos, head)					\
	if (!pos)							\
		pos =  srcu_dereference((head)->next, &tomoyo_ss);	\
	for ( ; pos != (head); pos = srcu_dereference(pos->next, &tomoyo_ss))

#endif 
