/*
 * Copyright (C) 2007 Casey Schaufler <casey@schaufler-ca.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, version 2.
 *
 * Author:
 *      Casey Schaufler <casey@schaufler-ca.com>
 *
 */

#ifndef _SECURITY_SMACK_H
#define _SECURITY_SMACK_H

#include <linux/capability.h>
#include <linux/spinlock.h>
#include <linux/security.h>
#include <linux/in.h>
#include <net/netlabel.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/lsm_audit.h>

#define SMK_MAXLEN	23
#define SMK_LABELLEN	(SMK_MAXLEN+1)

struct superblock_smack {
	char		*smk_root;
	char		*smk_floor;
	char		*smk_hat;
	char		*smk_default;
	int		smk_initialized;
	spinlock_t	smk_sblock;	
};

struct socket_smack {
	char		*smk_out;	
	char		*smk_in;	
	char		*smk_packet;	
};

struct inode_smack {
	char		*smk_inode;	
	char		*smk_task;	
	char		*smk_mmap;	
	struct mutex	smk_lock;	
	int		smk_flags;	
};

struct task_smack {
	char			*smk_task;	
	char			*smk_forked;	
	struct list_head	smk_rules;	
	struct mutex		smk_rules_lock;	
};

#define	SMK_INODE_INSTANT	0x01	
#define	SMK_INODE_TRANSMUTE	0x02	

struct smack_rule {
	struct list_head	list;
	char			*smk_subject;
	char			*smk_object;
	int			smk_access;
};

struct smack_cipso {
	int	smk_level;
	char	smk_catset[SMK_LABELLEN];
};

struct smk_netlbladdr {
	struct list_head	list;
	struct sockaddr_in	smk_host;	
	struct in_addr		smk_mask;	
	char			*smk_label;	
};

struct smack_known {
	struct list_head	list;
	char			smk_known[SMK_LABELLEN];
	u32			smk_secid;
	struct smack_cipso	*smk_cipso;
	spinlock_t		smk_cipsolock;	
	struct list_head	smk_rules;	
	struct mutex		smk_rules_lock;	
};

#define SMK_FSDEFAULT	"smackfsdef="
#define SMK_FSFLOOR	"smackfsfloor="
#define SMK_FSHAT	"smackfshat="
#define SMK_FSROOT	"smackfsroot="

#define SMACK_CIPSO_OPTION 	"-CIPSO"

#define SMACK_UNLABELED_SOCKET	0
#define SMACK_CIPSO_SOCKET	1

#define SMACK_MAGIC	0x43415d53 

#define SMACK_CIPSO_DOI_DEFAULT		3	
#define SMACK_CIPSO_DOI_INVALID		-1	
#define SMACK_CIPSO_DIRECT_DEFAULT	250	
#define SMACK_CIPSO_MAXCATVAL		63	
#define SMACK_CIPSO_MAXLEVEL            255     
#define SMACK_CIPSO_MAXCATNUM           239     

#define MAY_TRANSMUTE	64
#define MAY_ANYREAD	(MAY_READ | MAY_EXEC)
#define MAY_READWRITE	(MAY_READ | MAY_WRITE)
#define MAY_NOT		0

#define SMK_NUM_ACCESS_TYPE 5

struct smack_audit_data {
	const char *function;
	char *subject;
	char *object;
	char *request;
	int result;
};

struct smk_audit_info {
#ifdef CONFIG_AUDIT
	struct common_audit_data a;
	struct smack_audit_data sad;
#endif
};
struct inode_smack *new_inode_smack(char *);

int smk_access_entry(char *, char *, struct list_head *);
int smk_access(char *, char *, int, struct smk_audit_info *);
int smk_curacc(char *, u32, struct smk_audit_info *);
int smack_to_cipso(const char *, struct smack_cipso *);
char *smack_from_cipso(u32, char *);
char *smack_from_secid(const u32);
void smk_parse_smack(const char *string, int len, char *smack);
char *smk_import(const char *, int);
struct smack_known *smk_import_entry(const char *, int);
struct smack_known *smk_find_entry(const char *);
u32 smack_to_secid(const char *);

extern int smack_cipso_direct;
extern char *smack_net_ambient;
extern char *smack_onlycap;
extern const char *smack_cipso_option;

extern struct smack_known smack_known_floor;
extern struct smack_known smack_known_hat;
extern struct smack_known smack_known_huh;
extern struct smack_known smack_known_invalid;
extern struct smack_known smack_known_star;
extern struct smack_known smack_known_web;

extern struct list_head smack_known_list;
extern struct list_head smk_netlbladdr_list;

extern struct security_operations smack_ops;

static inline void smack_catset_bit(int cat, char *catsetp)
{
	if (cat > SMK_LABELLEN * 8)
		return;

	catsetp[(cat - 1) / 8] |= 0x80 >> ((cat - 1) % 8);
}

static inline int smk_inode_transmutable(const struct inode *isp)
{
	struct inode_smack *sip = isp->i_security;
	return (sip->smk_flags & SMK_INODE_TRANSMUTE) != 0;
}

static inline char *smk_of_inode(const struct inode *isp)
{
	struct inode_smack *sip = isp->i_security;
	return sip->smk_inode;
}

static inline char *smk_of_task(const struct task_smack *tsp)
{
	return tsp->smk_task;
}

static inline char *smk_of_forked(const struct task_smack *tsp)
{
	return tsp->smk_forked;
}

static inline char *smk_of_current(void)
{
	return smk_of_task(current_security());
}

#define SMACK_AUDIT_DENIED 0x1
#define SMACK_AUDIT_ACCEPT 0x2
extern int log_policy;

void smack_log(char *subject_label, char *object_label,
		int request,
		int result, struct smk_audit_info *auditdata);

#ifdef CONFIG_AUDIT

static inline void smk_ad_init(struct smk_audit_info *a, const char *func,
			       char type)
{
	memset(a, 0, sizeof(*a));
	a->a.type = type;
	a->a.smack_audit_data = &a->sad;
	a->a.smack_audit_data->function = func;
}

static inline void smk_ad_init_net(struct smk_audit_info *a, const char *func,
				   char type, struct lsm_network_audit *net)
{
	smk_ad_init(a, func, type);
	memset(net, 0, sizeof(*net));
	a->a.u.net = net;
}

static inline void smk_ad_setfield_u_tsk(struct smk_audit_info *a,
					 struct task_struct *t)
{
	a->a.u.tsk = t;
}
static inline void smk_ad_setfield_u_fs_path_dentry(struct smk_audit_info *a,
						    struct dentry *d)
{
	a->a.u.dentry = d;
}
static inline void smk_ad_setfield_u_fs_inode(struct smk_audit_info *a,
					      struct inode *i)
{
	a->a.u.inode = i;
}
static inline void smk_ad_setfield_u_fs_path(struct smk_audit_info *a,
					     struct path p)
{
	a->a.u.path = p;
}
static inline void smk_ad_setfield_u_net_sk(struct smk_audit_info *a,
					    struct sock *sk)
{
	a->a.u.net->sk = sk;
}

#else 

static inline void smk_ad_init(struct smk_audit_info *a, const char *func,
			       char type)
{
}
static inline void smk_ad_setfield_u_tsk(struct smk_audit_info *a,
					 struct task_struct *t)
{
}
static inline void smk_ad_setfield_u_fs_path_dentry(struct smk_audit_info *a,
						    struct dentry *d)
{
}
static inline void smk_ad_setfield_u_fs_path_mnt(struct smk_audit_info *a,
						 struct vfsmount *m)
{
}
static inline void smk_ad_setfield_u_fs_inode(struct smk_audit_info *a,
					      struct inode *i)
{
}
static inline void smk_ad_setfield_u_fs_path(struct smk_audit_info *a,
					     struct path p)
{
}
static inline void smk_ad_setfield_u_net_sk(struct smk_audit_info *a,
					    struct sock *sk)
{
}
#endif

#endif  
