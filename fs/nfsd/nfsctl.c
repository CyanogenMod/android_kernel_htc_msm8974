/*
 * Syscall interface to knfsd.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/ctype.h>

#include <linux/sunrpc/svcsock.h>
#include <linux/lockd/lockd.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/gss_api.h>
#include <linux/sunrpc/gss_krb5_enctypes.h>
#include <linux/sunrpc/rpc_pipe_fs.h>
#include <linux/module.h>

#include "idmap.h"
#include "nfsd.h"
#include "cache.h"
#include "fault_inject.h"
#include "netns.h"

enum {
	NFSD_Root = 1,
	NFSD_List,
	NFSD_Export_features,
	NFSD_Fh,
	NFSD_FO_UnlockIP,
	NFSD_FO_UnlockFS,
	NFSD_Threads,
	NFSD_Pool_Threads,
	NFSD_Pool_Stats,
	NFSD_Versions,
	NFSD_Ports,
	NFSD_MaxBlkSize,
	NFSD_SupportedEnctypes,
#ifdef CONFIG_NFSD_V4
	NFSD_Leasetime,
	NFSD_Gracetime,
	NFSD_RecoveryDir,
#endif
};

static ssize_t write_filehandle(struct file *file, char *buf, size_t size);
static ssize_t write_unlock_ip(struct file *file, char *buf, size_t size);
static ssize_t write_unlock_fs(struct file *file, char *buf, size_t size);
static ssize_t write_threads(struct file *file, char *buf, size_t size);
static ssize_t write_pool_threads(struct file *file, char *buf, size_t size);
static ssize_t write_versions(struct file *file, char *buf, size_t size);
static ssize_t write_ports(struct file *file, char *buf, size_t size);
static ssize_t write_maxblksize(struct file *file, char *buf, size_t size);
#ifdef CONFIG_NFSD_V4
static ssize_t write_leasetime(struct file *file, char *buf, size_t size);
static ssize_t write_gracetime(struct file *file, char *buf, size_t size);
static ssize_t write_recoverydir(struct file *file, char *buf, size_t size);
#endif

static ssize_t (*write_op[])(struct file *, char *, size_t) = {
	[NFSD_Fh] = write_filehandle,
	[NFSD_FO_UnlockIP] = write_unlock_ip,
	[NFSD_FO_UnlockFS] = write_unlock_fs,
	[NFSD_Threads] = write_threads,
	[NFSD_Pool_Threads] = write_pool_threads,
	[NFSD_Versions] = write_versions,
	[NFSD_Ports] = write_ports,
	[NFSD_MaxBlkSize] = write_maxblksize,
#ifdef CONFIG_NFSD_V4
	[NFSD_Leasetime] = write_leasetime,
	[NFSD_Gracetime] = write_gracetime,
	[NFSD_RecoveryDir] = write_recoverydir,
#endif
};

static ssize_t nfsctl_transaction_write(struct file *file, const char __user *buf, size_t size, loff_t *pos)
{
	ino_t ino =  file->f_path.dentry->d_inode->i_ino;
	char *data;
	ssize_t rv;

	if (ino >= ARRAY_SIZE(write_op) || !write_op[ino])
		return -EINVAL;

	data = simple_transaction_get(file, buf, size);
	if (IS_ERR(data))
		return PTR_ERR(data);

	rv =  write_op[ino](file, data, size);
	if (rv >= 0) {
		simple_transaction_set(file, rv);
		rv = size;
	}
	return rv;
}

static ssize_t nfsctl_transaction_read(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	if (! file->private_data) {
		ssize_t rv = nfsctl_transaction_write(file, buf, 0, pos);
		if (rv < 0)
			return rv;
	}
	return simple_transaction_read(file, buf, size, pos);
}

static const struct file_operations transaction_ops = {
	.write		= nfsctl_transaction_write,
	.read		= nfsctl_transaction_read,
	.release	= simple_transaction_release,
	.llseek		= default_llseek,
};

static int exports_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &nfs_exports_op);
}

static const struct file_operations exports_operations = {
	.open		= exports_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.owner		= THIS_MODULE,
};

static int export_features_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x 0x%x\n", NFSEXP_ALLFLAGS, NFSEXP_SECINFO_FLAGS);
	return 0;
}

static int export_features_open(struct inode *inode, struct file *file)
{
	return single_open(file, export_features_show, NULL);
}

static struct file_operations export_features_operations = {
	.open		= export_features_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#if defined(CONFIG_SUNRPC_GSS) || defined(CONFIG_SUNRPC_GSS_MODULE)
static int supported_enctypes_show(struct seq_file *m, void *v)
{
	seq_printf(m, KRB5_SUPPORTED_ENCTYPES);
	return 0;
}

static int supported_enctypes_open(struct inode *inode, struct file *file)
{
	return single_open(file, supported_enctypes_show, NULL);
}

static struct file_operations supported_enctypes_ops = {
	.open		= supported_enctypes_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif 

extern int nfsd_pool_stats_open(struct inode *inode, struct file *file);
extern int nfsd_pool_stats_release(struct inode *inode, struct file *file);

static const struct file_operations pool_stats_operations = {
	.open		= nfsd_pool_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= nfsd_pool_stats_release,
	.owner		= THIS_MODULE,
};



static ssize_t write_unlock_ip(struct file *file, char *buf, size_t size)
{
	struct sockaddr_storage address;
	struct sockaddr *sap = (struct sockaddr *)&address;
	size_t salen = sizeof(address);
	char *fo_path;

	
	if (size == 0)
		return -EINVAL;

	if (buf[size-1] != '\n')
		return -EINVAL;

	fo_path = buf;
	if (qword_get(&buf, fo_path, size) < 0)
		return -EINVAL;

	if (rpc_pton(&init_net, fo_path, size, sap, salen) == 0)
		return -EINVAL;

	return nlmsvc_unlock_all_by_ip(sap);
}

static ssize_t write_unlock_fs(struct file *file, char *buf, size_t size)
{
	struct path path;
	char *fo_path;
	int error;

	
	if (size == 0)
		return -EINVAL;

	if (buf[size-1] != '\n')
		return -EINVAL;

	fo_path = buf;
	if (qword_get(&buf, fo_path, size) < 0)
		return -EINVAL;

	error = kern_path(fo_path, 0, &path);
	if (error)
		return error;

	error = nlmsvc_unlock_all_by_sb(path.dentry->d_sb);

	path_put(&path);
	return error;
}

static ssize_t write_filehandle(struct file *file, char *buf, size_t size)
{
	char *dname, *path;
	int uninitialized_var(maxsize);
	char *mesg = buf;
	int len;
	struct auth_domain *dom;
	struct knfsd_fh fh;

	if (size == 0)
		return -EINVAL;

	if (buf[size-1] != '\n')
		return -EINVAL;
	buf[size-1] = 0;

	dname = mesg;
	len = qword_get(&mesg, dname, size);
	if (len <= 0)
		return -EINVAL;
	
	path = dname+len+1;
	len = qword_get(&mesg, path, size);
	if (len <= 0)
		return -EINVAL;

	len = get_int(&mesg, &maxsize);
	if (len)
		return len;

	if (maxsize < NFS_FHSIZE)
		return -EINVAL;
	if (maxsize > NFS3_FHSIZE)
		maxsize = NFS3_FHSIZE;

	if (qword_get(&mesg, mesg, size)>0)
		return -EINVAL;

	
	dom = unix_domain_find(dname);
	if (!dom)
		return -ENOMEM;

	len = exp_rootfh(dom, path, &fh,  maxsize);
	auth_domain_put(dom);
	if (len)
		return len;
	
	mesg = buf;
	len = SIMPLE_TRANSACTION_LIMIT;
	qword_addhex(&mesg, &len, (char*)&fh.fh_base, fh.fh_size);
	mesg[-1] = '\n';
	return mesg - buf;	
}

static ssize_t write_threads(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	int rv;
	if (size > 0) {
		int newthreads;
		rv = get_int(&mesg, &newthreads);
		if (rv)
			return rv;
		if (newthreads < 0)
			return -EINVAL;
		rv = nfsd_svc(NFS_PORT, newthreads);
		if (rv < 0)
			return rv;
	} else
		rv = nfsd_nrthreads();

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%d\n", rv);
}

static ssize_t write_pool_threads(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	int i;
	int rv;
	int len;
	int npools;
	int *nthreads;

	mutex_lock(&nfsd_mutex);
	npools = nfsd_nrpools();
	if (npools == 0) {
		mutex_unlock(&nfsd_mutex);
		strcpy(buf, "0\n");
		return strlen(buf);
	}

	nthreads = kcalloc(npools, sizeof(int), GFP_KERNEL);
	rv = -ENOMEM;
	if (nthreads == NULL)
		goto out_free;

	if (size > 0) {
		for (i = 0; i < npools; i++) {
			rv = get_int(&mesg, &nthreads[i]);
			if (rv == -ENOENT)
				break;		
			if (rv)
				goto out_free;	
			rv = -EINVAL;
			if (nthreads[i] < 0)
				goto out_free;
		}
		rv = nfsd_set_nrthreads(i, nthreads);
		if (rv)
			goto out_free;
	}

	rv = nfsd_get_nrthreads(npools, nthreads);
	if (rv)
		goto out_free;

	mesg = buf;
	size = SIMPLE_TRANSACTION_LIMIT;
	for (i = 0; i < npools && size > 0; i++) {
		snprintf(mesg, size, "%d%c", nthreads[i], (i == npools-1 ? '\n' : ' '));
		len = strlen(mesg);
		size -= len;
		mesg += len;
	}
	rv = mesg - buf;
out_free:
	kfree(nthreads);
	mutex_unlock(&nfsd_mutex);
	return rv;
}

static ssize_t __write_versions(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	char *vers, *minorp, sign;
	int len, num, remaining;
	unsigned minor;
	ssize_t tlen = 0;
	char *sep;

	if (size>0) {
		if (nfsd_serv)
			return -EBUSY;
		if (buf[size-1] != '\n')
			return -EINVAL;
		buf[size-1] = 0;

		vers = mesg;
		len = qword_get(&mesg, vers, size);
		if (len <= 0) return -EINVAL;
		do {
			sign = *vers;
			if (sign == '+' || sign == '-')
				num = simple_strtol((vers+1), &minorp, 0);
			else
				num = simple_strtol(vers, &minorp, 0);
			if (*minorp == '.') {
				if (num < 4)
					return -EINVAL;
				minor = simple_strtoul(minorp+1, NULL, 0);
				if (minor == 0)
					return -EINVAL;
				if (nfsd_minorversion(minor, sign == '-' ?
						     NFSD_CLEAR : NFSD_SET) < 0)
					return -EINVAL;
				goto next;
			}
			switch(num) {
			case 2:
			case 3:
			case 4:
				nfsd_vers(num, sign == '-' ? NFSD_CLEAR : NFSD_SET);
				break;
			default:
				return -EINVAL;
			}
		next:
			vers += len + 1;
		} while ((len = qword_get(&mesg, vers, size)) > 0);
		nfsd_reset_versions();
	}

	
	len = 0;
	sep = "";
	remaining = SIMPLE_TRANSACTION_LIMIT;
	for (num=2 ; num <= 4 ; num++)
		if (nfsd_vers(num, NFSD_AVAIL)) {
			len = snprintf(buf, remaining, "%s%c%d", sep,
				       nfsd_vers(num, NFSD_TEST)?'+':'-',
				       num);
			sep = " ";

			if (len > remaining)
				break;
			remaining -= len;
			buf += len;
			tlen += len;
		}
	if (nfsd_vers(4, NFSD_AVAIL))
		for (minor = 1; minor <= NFSD_SUPPORTED_MINOR_VERSION;
		     minor++) {
			len = snprintf(buf, remaining, " %c4.%u",
					(nfsd_vers(4, NFSD_TEST) &&
					 nfsd_minorversion(minor, NFSD_TEST)) ?
						'+' : '-',
					minor);

			if (len > remaining)
				break;
			remaining -= len;
			buf += len;
			tlen += len;
		}

	len = snprintf(buf, remaining, "\n");
	if (len > remaining)
		return -EINVAL;
	return tlen + len;
}

static ssize_t write_versions(struct file *file, char *buf, size_t size)
{
	ssize_t rv;

	mutex_lock(&nfsd_mutex);
	rv = __write_versions(file, buf, size);
	mutex_unlock(&nfsd_mutex);
	return rv;
}

static ssize_t __write_ports_names(char *buf)
{
	if (nfsd_serv == NULL)
		return 0;
	return svc_xprt_names(nfsd_serv, buf, SIMPLE_TRANSACTION_LIMIT);
}

/*
 * A single 'fd' number was written, in which case it must be for
 * a socket of a supported family/protocol, and we use it as an
 * nfsd listener.
 */
static ssize_t __write_ports_addfd(char *buf)
{
	char *mesg = buf;
	int fd, err;

	err = get_int(&mesg, &fd);
	if (err != 0 || fd < 0)
		return -EINVAL;

	err = nfsd_create_serv();
	if (err != 0)
		return err;

	err = svc_addsock(nfsd_serv, fd, buf, SIMPLE_TRANSACTION_LIMIT);
	if (err < 0) {
		svc_destroy(nfsd_serv);
		return err;
	}

	
	nfsd_serv->sv_nrthreads--;
	return err;
}

static ssize_t __write_ports_delfd(char *buf)
{
	char *toclose;
	int len = 0;

	toclose = kstrdup(buf + 1, GFP_KERNEL);
	if (toclose == NULL)
		return -ENOMEM;

	if (nfsd_serv != NULL)
		len = svc_sock_names(nfsd_serv, buf,
					SIMPLE_TRANSACTION_LIMIT, toclose);
	kfree(toclose);
	return len;
}

static ssize_t __write_ports_addxprt(char *buf)
{
	char transport[16];
	struct svc_xprt *xprt;
	int port, err;

	if (sscanf(buf, "%15s %4u", transport, &port) != 2)
		return -EINVAL;

	if (port < 1 || port > USHRT_MAX)
		return -EINVAL;

	err = nfsd_create_serv();
	if (err != 0)
		return err;

	err = svc_create_xprt(nfsd_serv, transport, &init_net,
				PF_INET, port, SVC_SOCK_ANONYMOUS);
	if (err < 0)
		goto out_err;

	err = svc_create_xprt(nfsd_serv, transport, &init_net,
				PF_INET6, port, SVC_SOCK_ANONYMOUS);
	if (err < 0 && err != -EAFNOSUPPORT)
		goto out_close;

	
	nfsd_serv->sv_nrthreads--;
	return 0;
out_close:
	xprt = svc_find_xprt(nfsd_serv, transport, &init_net, PF_INET, port);
	if (xprt != NULL) {
		svc_close_xprt(xprt);
		svc_xprt_put(xprt);
	}
out_err:
	svc_destroy(nfsd_serv);
	return err;
}

static ssize_t __write_ports_delxprt(char *buf)
{
	struct svc_xprt *xprt;
	char transport[16];
	int port;

	if (sscanf(&buf[1], "%15s %4u", transport, &port) != 2)
		return -EINVAL;

	if (port < 1 || port > USHRT_MAX || nfsd_serv == NULL)
		return -EINVAL;

	xprt = svc_find_xprt(nfsd_serv, transport, &init_net, AF_UNSPEC, port);
	if (xprt == NULL)
		return -ENOTCONN;

	svc_close_xprt(xprt);
	svc_xprt_put(xprt);
	return 0;
}

static ssize_t __write_ports(struct file *file, char *buf, size_t size)
{
	if (size == 0)
		return __write_ports_names(buf);

	if (isdigit(buf[0]))
		return __write_ports_addfd(buf);

	if (buf[0] == '-' && isdigit(buf[1]))
		return __write_ports_delfd(buf);

	if (isalpha(buf[0]))
		return __write_ports_addxprt(buf);

	if (buf[0] == '-' && isalpha(buf[1]))
		return __write_ports_delxprt(buf);

	return -EINVAL;
}

static ssize_t write_ports(struct file *file, char *buf, size_t size)
{
	ssize_t rv;

	mutex_lock(&nfsd_mutex);
	rv = __write_ports(file, buf, size);
	mutex_unlock(&nfsd_mutex);
	return rv;
}


int nfsd_max_blksize;

static ssize_t write_maxblksize(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	if (size > 0) {
		int bsize;
		int rv = get_int(&mesg, &bsize);
		if (rv)
			return rv;
		if (bsize < 1024)
			bsize = 1024;
		if (bsize > NFSSVC_MAXBLKSIZE)
			bsize = NFSSVC_MAXBLKSIZE;
		bsize &= ~(1024-1);
		mutex_lock(&nfsd_mutex);
		if (nfsd_serv) {
			mutex_unlock(&nfsd_mutex);
			return -EBUSY;
		}
		nfsd_max_blksize = bsize;
		mutex_unlock(&nfsd_mutex);
	}

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%d\n",
							nfsd_max_blksize);
}

#ifdef CONFIG_NFSD_V4
static ssize_t __nfsd4_write_time(struct file *file, char *buf, size_t size, time_t *time)
{
	char *mesg = buf;
	int rv, i;

	if (size > 0) {
		if (nfsd_serv)
			return -EBUSY;
		rv = get_int(&mesg, &i);
		if (rv)
			return rv;
		if (i < 10 || i > 3600)
			return -EINVAL;
		*time = i;
	}

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%ld\n", *time);
}

static ssize_t nfsd4_write_time(struct file *file, char *buf, size_t size, time_t *time)
{
	ssize_t rv;

	mutex_lock(&nfsd_mutex);
	rv = __nfsd4_write_time(file, buf, size, time);
	mutex_unlock(&nfsd_mutex);
	return rv;
}

static ssize_t write_leasetime(struct file *file, char *buf, size_t size)
{
	return nfsd4_write_time(file, buf, size, &nfsd4_lease);
}

static ssize_t write_gracetime(struct file *file, char *buf, size_t size)
{
	return nfsd4_write_time(file, buf, size, &nfsd4_grace);
}

extern char *nfs4_recoverydir(void);

static ssize_t __write_recoverydir(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	char *recdir;
	int len, status;

	if (size > 0) {
		if (nfsd_serv)
			return -EBUSY;
		if (size > PATH_MAX || buf[size-1] != '\n')
			return -EINVAL;
		buf[size-1] = 0;

		recdir = mesg;
		len = qword_get(&mesg, recdir, size);
		if (len <= 0)
			return -EINVAL;

		status = nfs4_reset_recoverydir(recdir);
		if (status)
			return status;
	}

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%s\n",
							nfs4_recoverydir());
}

static ssize_t write_recoverydir(struct file *file, char *buf, size_t size)
{
	ssize_t rv;

	mutex_lock(&nfsd_mutex);
	rv = __write_recoverydir(file, buf, size);
	mutex_unlock(&nfsd_mutex);
	return rv;
}

#endif


static int nfsd_fill_super(struct super_block * sb, void * data, int silent)
{
	static struct tree_descr nfsd_files[] = {
		[NFSD_List] = {"exports", &exports_operations, S_IRUGO},
		[NFSD_Export_features] = {"export_features",
					&export_features_operations, S_IRUGO},
		[NFSD_FO_UnlockIP] = {"unlock_ip",
					&transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_FO_UnlockFS] = {"unlock_filesystem",
					&transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Fh] = {"filehandle", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Threads] = {"threads", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Pool_Threads] = {"pool_threads", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Pool_Stats] = {"pool_stats", &pool_stats_operations, S_IRUGO},
		[NFSD_Versions] = {"versions", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Ports] = {"portlist", &transaction_ops, S_IWUSR|S_IRUGO},
		[NFSD_MaxBlkSize] = {"max_block_size", &transaction_ops, S_IWUSR|S_IRUGO},
#if defined(CONFIG_SUNRPC_GSS) || defined(CONFIG_SUNRPC_GSS_MODULE)
		[NFSD_SupportedEnctypes] = {"supported_krb5_enctypes", &supported_enctypes_ops, S_IRUGO},
#endif 
#ifdef CONFIG_NFSD_V4
		[NFSD_Leasetime] = {"nfsv4leasetime", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Gracetime] = {"nfsv4gracetime", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_RecoveryDir] = {"nfsv4recoverydir", &transaction_ops, S_IWUSR|S_IRUSR},
#endif
		 {""}
	};
	return simple_fill_super(sb, 0x6e667364, nfsd_files);
}

static struct dentry *nfsd_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_single(fs_type, flags, data, nfsd_fill_super);
}

static struct file_system_type nfsd_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfsd",
	.mount		= nfsd_mount,
	.kill_sb	= kill_litter_super,
};

#ifdef CONFIG_PROC_FS
static int create_proc_exports_entry(void)
{
	struct proc_dir_entry *entry;

	entry = proc_mkdir("fs/nfs", NULL);
	if (!entry)
		return -ENOMEM;
	entry = proc_create("exports", 0, entry, &exports_operations);
	if (!entry)
		return -ENOMEM;
	return 0;
}
#else 
static int create_proc_exports_entry(void)
{
	return 0;
}
#endif

int nfsd_net_id;
static struct pernet_operations nfsd_net_ops = {
	.id   = &nfsd_net_id,
	.size = sizeof(struct nfsd_net),
};

static int __init init_nfsd(void)
{
	int retval;
	printk(KERN_INFO "Installing knfsd (copyright (C) 1996 okir@monad.swb.de).\n");

	retval = register_cld_notifier();
	if (retval)
		return retval;
	retval = register_pernet_subsys(&nfsd_net_ops);
	if (retval < 0)
		goto out_unregister_notifier;
	retval = nfsd4_init_slabs();
	if (retval)
		goto out_unregister_pernet;
	nfs4_state_init();
	retval = nfsd_fault_inject_init(); 
	if (retval)
		goto out_free_slabs;
	nfsd_stat_init();	
	retval = nfsd_reply_cache_init();
	if (retval)
		goto out_free_stat;
	retval = nfsd_export_init();
	if (retval)
		goto out_free_cache;
	nfsd_lockd_init();	
	retval = nfsd_idmap_init();
	if (retval)
		goto out_free_lockd;
	retval = create_proc_exports_entry();
	if (retval)
		goto out_free_idmap;
	retval = register_filesystem(&nfsd_fs_type);
	if (retval)
		goto out_free_all;
	return 0;
out_free_all:
	remove_proc_entry("fs/nfs/exports", NULL);
	remove_proc_entry("fs/nfs", NULL);
out_free_idmap:
	nfsd_idmap_shutdown();
out_free_lockd:
	nfsd_lockd_shutdown();
	nfsd_export_shutdown();
out_free_cache:
	nfsd_reply_cache_shutdown();
out_free_stat:
	nfsd_stat_shutdown();
	nfsd_fault_inject_cleanup();
out_free_slabs:
	nfsd4_free_slabs();
out_unregister_pernet:
	unregister_pernet_subsys(&nfsd_net_ops);
out_unregister_notifier:
	unregister_cld_notifier();
	return retval;
}

static void __exit exit_nfsd(void)
{
	nfsd_export_shutdown();
	nfsd_reply_cache_shutdown();
	remove_proc_entry("fs/nfs/exports", NULL);
	remove_proc_entry("fs/nfs", NULL);
	nfsd_stat_shutdown();
	nfsd_lockd_shutdown();
	nfsd_idmap_shutdown();
	nfsd4_free_slabs();
	nfsd_fault_inject_cleanup();
	unregister_filesystem(&nfsd_fs_type);
	unregister_pernet_subsys(&nfsd_net_ops);
	unregister_cld_notifier();
}

MODULE_AUTHOR("Olaf Kirch <okir@monad.swb.de>");
MODULE_LICENSE("GPL");
module_init(init_nfsd)
module_exit(exit_nfsd)
