
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <linux/sunrpc/stats.h>
#include <linux/sunrpc/svc_xprt.h>
#include <linux/sunrpc/svcsock.h>
#include <linux/sunrpc/xprt.h>
#include <linux/module.h>

#define RPCDBG_FACILITY	RPCDBG_SVCXPRT

static struct svc_deferred_req *svc_deferred_dequeue(struct svc_xprt *xprt);
static int svc_deferred_recv(struct svc_rqst *rqstp);
static struct cache_deferred_req *svc_defer(struct cache_req *req);
static void svc_age_temp_xprts(unsigned long closure);
static void svc_delete_xprt(struct svc_xprt *xprt);

static int svc_conn_age_period = 6*60;

static DEFINE_SPINLOCK(svc_xprt_class_lock);
static LIST_HEAD(svc_xprt_class_list);


int svc_reg_xprt_class(struct svc_xprt_class *xcl)
{
	struct svc_xprt_class *cl;
	int res = -EEXIST;

	dprintk("svc: Adding svc transport class '%s'\n", xcl->xcl_name);

	INIT_LIST_HEAD(&xcl->xcl_list);
	spin_lock(&svc_xprt_class_lock);
	
	list_for_each_entry(cl, &svc_xprt_class_list, xcl_list) {
		if (strcmp(xcl->xcl_name, cl->xcl_name) == 0)
			goto out;
	}
	list_add_tail(&xcl->xcl_list, &svc_xprt_class_list);
	res = 0;
out:
	spin_unlock(&svc_xprt_class_lock);
	return res;
}
EXPORT_SYMBOL_GPL(svc_reg_xprt_class);

void svc_unreg_xprt_class(struct svc_xprt_class *xcl)
{
	dprintk("svc: Removing svc transport class '%s'\n", xcl->xcl_name);
	spin_lock(&svc_xprt_class_lock);
	list_del_init(&xcl->xcl_list);
	spin_unlock(&svc_xprt_class_lock);
}
EXPORT_SYMBOL_GPL(svc_unreg_xprt_class);

int svc_print_xprts(char *buf, int maxlen)
{
	struct svc_xprt_class *xcl;
	char tmpstr[80];
	int len = 0;
	buf[0] = '\0';

	spin_lock(&svc_xprt_class_lock);
	list_for_each_entry(xcl, &svc_xprt_class_list, xcl_list) {
		int slen;

		sprintf(tmpstr, "%s %d\n", xcl->xcl_name, xcl->xcl_max_payload);
		slen = strlen(tmpstr);
		if (len + slen > maxlen)
			break;
		len += slen;
		strcat(buf, tmpstr);
	}
	spin_unlock(&svc_xprt_class_lock);

	return len;
}

static void svc_xprt_free(struct kref *kref)
{
	struct svc_xprt *xprt =
		container_of(kref, struct svc_xprt, xpt_ref);
	struct module *owner = xprt->xpt_class->xcl_owner;
	if (test_bit(XPT_CACHE_AUTH, &xprt->xpt_flags))
		svcauth_unix_info_release(xprt);
	put_net(xprt->xpt_net);
	
	if (xprt->xpt_bc_xprt)
		xprt_put(xprt->xpt_bc_xprt);
	xprt->xpt_ops->xpo_free(xprt);
	module_put(owner);
}

void svc_xprt_put(struct svc_xprt *xprt)
{
	kref_put(&xprt->xpt_ref, svc_xprt_free);
}
EXPORT_SYMBOL_GPL(svc_xprt_put);

void svc_xprt_init(struct net *net, struct svc_xprt_class *xcl,
		   struct svc_xprt *xprt, struct svc_serv *serv)
{
	memset(xprt, 0, sizeof(*xprt));
	xprt->xpt_class = xcl;
	xprt->xpt_ops = xcl->xcl_ops;
	kref_init(&xprt->xpt_ref);
	xprt->xpt_server = serv;
	INIT_LIST_HEAD(&xprt->xpt_list);
	INIT_LIST_HEAD(&xprt->xpt_ready);
	INIT_LIST_HEAD(&xprt->xpt_deferred);
	INIT_LIST_HEAD(&xprt->xpt_users);
	mutex_init(&xprt->xpt_mutex);
	spin_lock_init(&xprt->xpt_lock);
	set_bit(XPT_BUSY, &xprt->xpt_flags);
	rpc_init_wait_queue(&xprt->xpt_bc_pending, "xpt_bc_pending");
	xprt->xpt_net = get_net(net);
}
EXPORT_SYMBOL_GPL(svc_xprt_init);

static struct svc_xprt *__svc_xpo_create(struct svc_xprt_class *xcl,
					 struct svc_serv *serv,
					 struct net *net,
					 const int family,
					 const unsigned short port,
					 int flags)
{
	struct sockaddr_in sin = {
		.sin_family		= AF_INET,
		.sin_addr.s_addr	= htonl(INADDR_ANY),
		.sin_port		= htons(port),
	};
#if IS_ENABLED(CONFIG_IPV6)
	struct sockaddr_in6 sin6 = {
		.sin6_family		= AF_INET6,
		.sin6_addr		= IN6ADDR_ANY_INIT,
		.sin6_port		= htons(port),
	};
#endif
	struct sockaddr *sap;
	size_t len;

	switch (family) {
	case PF_INET:
		sap = (struct sockaddr *)&sin;
		len = sizeof(sin);
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case PF_INET6:
		sap = (struct sockaddr *)&sin6;
		len = sizeof(sin6);
		break;
#endif
	default:
		return ERR_PTR(-EAFNOSUPPORT);
	}

	return xcl->xcl_ops->xpo_create(serv, net, sap, len, flags);
}

int svc_create_xprt(struct svc_serv *serv, const char *xprt_name,
		    struct net *net, const int family,
		    const unsigned short port, int flags)
{
	struct svc_xprt_class *xcl;

	dprintk("svc: creating transport %s[%d]\n", xprt_name, port);
	spin_lock(&svc_xprt_class_lock);
	list_for_each_entry(xcl, &svc_xprt_class_list, xcl_list) {
		struct svc_xprt *newxprt;
		unsigned short newport;

		if (strcmp(xprt_name, xcl->xcl_name))
			continue;

		if (!try_module_get(xcl->xcl_owner))
			goto err;

		spin_unlock(&svc_xprt_class_lock);
		newxprt = __svc_xpo_create(xcl, serv, net, family, port, flags);
		if (IS_ERR(newxprt)) {
			module_put(xcl->xcl_owner);
			return PTR_ERR(newxprt);
		}

		clear_bit(XPT_TEMP, &newxprt->xpt_flags);
		spin_lock_bh(&serv->sv_lock);
		list_add(&newxprt->xpt_list, &serv->sv_permsocks);
		spin_unlock_bh(&serv->sv_lock);
		newport = svc_xprt_local_port(newxprt);
		clear_bit(XPT_BUSY, &newxprt->xpt_flags);
		return newport;
	}
 err:
	spin_unlock(&svc_xprt_class_lock);
	dprintk("svc: transport %s not found\n", xprt_name);

	return -EPROTONOSUPPORT;
}
EXPORT_SYMBOL_GPL(svc_create_xprt);

void svc_xprt_copy_addrs(struct svc_rqst *rqstp, struct svc_xprt *xprt)
{
	memcpy(&rqstp->rq_addr, &xprt->xpt_remote, xprt->xpt_remotelen);
	rqstp->rq_addrlen = xprt->xpt_remotelen;

	memcpy(&rqstp->rq_daddr, &xprt->xpt_local, xprt->xpt_locallen);
	rqstp->rq_daddrlen = xprt->xpt_locallen;
}
EXPORT_SYMBOL_GPL(svc_xprt_copy_addrs);

char *svc_print_addr(struct svc_rqst *rqstp, char *buf, size_t len)
{
	return __svc_print_addr(svc_addr(rqstp), buf, len);
}
EXPORT_SYMBOL_GPL(svc_print_addr);

static void svc_thread_enqueue(struct svc_pool *pool, struct svc_rqst *rqstp)
{
	list_add(&rqstp->rq_list, &pool->sp_threads);
}

static void svc_thread_dequeue(struct svc_pool *pool, struct svc_rqst *rqstp)
{
	list_del(&rqstp->rq_list);
}

static bool svc_xprt_has_something_to_do(struct svc_xprt *xprt)
{
	if (xprt->xpt_flags & ((1<<XPT_CONN)|(1<<XPT_CLOSE)))
		return true;
	if (xprt->xpt_flags & ((1<<XPT_DATA)|(1<<XPT_DEFERRED)))
		return xprt->xpt_ops->xpo_has_wspace(xprt);
	return false;
}

void svc_xprt_enqueue(struct svc_xprt *xprt)
{
	struct svc_serv	*serv = xprt->xpt_server;
	struct svc_pool *pool;
	struct svc_rqst	*rqstp;
	int cpu;

	if (!svc_xprt_has_something_to_do(xprt))
		return;

	cpu = get_cpu();
	pool = svc_pool_for_cpu(xprt->xpt_server, cpu);
	put_cpu();

	spin_lock_bh(&pool->sp_lock);

	if (!list_empty(&pool->sp_threads) &&
	    !list_empty(&pool->sp_sockets))
		printk(KERN_ERR
		       "svc_xprt_enqueue: "
		       "threads and transports both waiting??\n");

	pool->sp_stats.packets++;

	if (test_and_set_bit(XPT_BUSY, &xprt->xpt_flags)) {
		
		dprintk("svc: transport %p busy, not enqueued\n", xprt);
		goto out_unlock;
	}

	if (!list_empty(&pool->sp_threads)) {
		rqstp = list_entry(pool->sp_threads.next,
				   struct svc_rqst,
				   rq_list);
		dprintk("svc: transport %p served by daemon %p\n",
			xprt, rqstp);
		svc_thread_dequeue(pool, rqstp);
		if (rqstp->rq_xprt)
			printk(KERN_ERR
				"svc_xprt_enqueue: server %p, rq_xprt=%p!\n",
				rqstp, rqstp->rq_xprt);
		rqstp->rq_xprt = xprt;
		svc_xprt_get(xprt);
		rqstp->rq_reserved = serv->sv_max_mesg;
		atomic_add(rqstp->rq_reserved, &xprt->xpt_reserved);
		pool->sp_stats.threads_woken++;
		wake_up(&rqstp->rq_wait);
	} else {
		dprintk("svc: transport %p put into queue\n", xprt);
		list_add_tail(&xprt->xpt_ready, &pool->sp_sockets);
		pool->sp_stats.sockets_queued++;
	}

out_unlock:
	spin_unlock_bh(&pool->sp_lock);
}
EXPORT_SYMBOL_GPL(svc_xprt_enqueue);

static struct svc_xprt *svc_xprt_dequeue(struct svc_pool *pool)
{
	struct svc_xprt	*xprt;

	if (list_empty(&pool->sp_sockets))
		return NULL;

	xprt = list_entry(pool->sp_sockets.next,
			  struct svc_xprt, xpt_ready);
	list_del_init(&xprt->xpt_ready);

	dprintk("svc: transport %p dequeued, inuse=%d\n",
		xprt, atomic_read(&xprt->xpt_ref.refcount));

	return xprt;
}

void svc_xprt_received(struct svc_xprt *xprt)
{
	BUG_ON(!test_bit(XPT_BUSY, &xprt->xpt_flags));
	svc_xprt_get(xprt);
	clear_bit(XPT_BUSY, &xprt->xpt_flags);
	svc_xprt_enqueue(xprt);
	svc_xprt_put(xprt);
}
EXPORT_SYMBOL_GPL(svc_xprt_received);

void svc_reserve(struct svc_rqst *rqstp, int space)
{
	space += rqstp->rq_res.head[0].iov_len;

	if (space < rqstp->rq_reserved) {
		struct svc_xprt *xprt = rqstp->rq_xprt;
		atomic_sub((rqstp->rq_reserved - space), &xprt->xpt_reserved);
		rqstp->rq_reserved = space;

		svc_xprt_enqueue(xprt);
	}
}
EXPORT_SYMBOL_GPL(svc_reserve);

static void svc_xprt_release(struct svc_rqst *rqstp)
{
	struct svc_xprt	*xprt = rqstp->rq_xprt;

	rqstp->rq_xprt->xpt_ops->xpo_release_rqst(rqstp);

	kfree(rqstp->rq_deferred);
	rqstp->rq_deferred = NULL;

	svc_free_res_pages(rqstp);
	rqstp->rq_res.page_len = 0;
	rqstp->rq_res.page_base = 0;

	if ((rqstp->rq_res.len) >  rqstp->rq_reserved)
		printk(KERN_ERR "RPC request reserved %d but used %d\n",
		       rqstp->rq_reserved,
		       rqstp->rq_res.len);

	rqstp->rq_res.head[0].iov_len = 0;
	svc_reserve(rqstp, 0);
	rqstp->rq_xprt = NULL;

	svc_xprt_put(xprt);
}

void svc_wake_up(struct svc_serv *serv)
{
	struct svc_rqst	*rqstp;
	unsigned int i;
	struct svc_pool *pool;

	for (i = 0; i < serv->sv_nrpools; i++) {
		pool = &serv->sv_pools[i];

		spin_lock_bh(&pool->sp_lock);
		if (!list_empty(&pool->sp_threads)) {
			rqstp = list_entry(pool->sp_threads.next,
					   struct svc_rqst,
					   rq_list);
			dprintk("svc: daemon %p woken up.\n", rqstp);
			wake_up(&rqstp->rq_wait);
		}
		spin_unlock_bh(&pool->sp_lock);
	}
}
EXPORT_SYMBOL_GPL(svc_wake_up);

int svc_port_is_privileged(struct sockaddr *sin)
{
	switch (sin->sa_family) {
	case AF_INET:
		return ntohs(((struct sockaddr_in *)sin)->sin_port)
			< PROT_SOCK;
	case AF_INET6:
		return ntohs(((struct sockaddr_in6 *)sin)->sin6_port)
			< PROT_SOCK;
	default:
		return 0;
	}
}

static void svc_check_conn_limits(struct svc_serv *serv)
{
	unsigned int limit = serv->sv_maxconn ? serv->sv_maxconn :
				(serv->sv_nrthreads+3) * 20;

	if (serv->sv_tmpcnt > limit) {
		struct svc_xprt *xprt = NULL;
		spin_lock_bh(&serv->sv_lock);
		if (!list_empty(&serv->sv_tempsocks)) {
			if (net_ratelimit()) {
				
				printk(KERN_NOTICE "%s: too many open  "
				       "connections, consider increasing %s\n",
				       serv->sv_name, serv->sv_maxconn ?
				       "the max number of connections." :
				       "the number of threads.");
			}
			xprt = list_entry(serv->sv_tempsocks.prev,
					  struct svc_xprt,
					  xpt_list);
			set_bit(XPT_CLOSE, &xprt->xpt_flags);
			svc_xprt_get(xprt);
		}
		spin_unlock_bh(&serv->sv_lock);

		if (xprt) {
			svc_xprt_enqueue(xprt);
			svc_xprt_put(xprt);
		}
	}
}

int svc_recv(struct svc_rqst *rqstp, long timeout)
{
	struct svc_xprt		*xprt = NULL;
	struct svc_serv		*serv = rqstp->rq_server;
	struct svc_pool		*pool = rqstp->rq_pool;
	int			len, i;
	int			pages;
	struct xdr_buf		*arg;
	DECLARE_WAITQUEUE(wait, current);
	long			time_left;

	dprintk("svc: server %p waiting for data (to = %ld)\n",
		rqstp, timeout);

	if (rqstp->rq_xprt)
		printk(KERN_ERR
			"svc_recv: service %p, transport not NULL!\n",
			 rqstp);
	if (waitqueue_active(&rqstp->rq_wait))
		printk(KERN_ERR
			"svc_recv: service %p, wait queue active!\n",
			 rqstp);

	
	pages = (serv->sv_max_mesg + PAGE_SIZE) / PAGE_SIZE;
	for (i = 0; i < pages ; i++)
		while (rqstp->rq_pages[i] == NULL) {
			struct page *p = alloc_page(GFP_KERNEL);
			if (!p) {
				set_current_state(TASK_INTERRUPTIBLE);
				if (signalled() || kthread_should_stop()) {
					set_current_state(TASK_RUNNING);
					return -EINTR;
				}
				schedule_timeout(msecs_to_jiffies(500));
			}
			rqstp->rq_pages[i] = p;
		}
	rqstp->rq_pages[i++] = NULL; 
	BUG_ON(pages >= RPCSVC_MAXPAGES);

	
	arg = &rqstp->rq_arg;
	arg->head[0].iov_base = page_address(rqstp->rq_pages[0]);
	arg->head[0].iov_len = PAGE_SIZE;
	arg->pages = rqstp->rq_pages + 1;
	arg->page_base = 0;
	
	arg->page_len = (pages-2)*PAGE_SIZE;
	arg->len = (pages-1)*PAGE_SIZE;
	arg->tail[0].iov_len = 0;

	try_to_freeze();
	cond_resched();
	if (signalled() || kthread_should_stop())
		return -EINTR;

	rqstp->rq_chandle.thread_wait = 5*HZ;

	spin_lock_bh(&pool->sp_lock);
	xprt = svc_xprt_dequeue(pool);
	if (xprt) {
		rqstp->rq_xprt = xprt;
		svc_xprt_get(xprt);
		rqstp->rq_reserved = serv->sv_max_mesg;
		atomic_add(rqstp->rq_reserved, &xprt->xpt_reserved);

		rqstp->rq_chandle.thread_wait = 1*HZ;
	} else {
		
		svc_thread_enqueue(pool, rqstp);

		set_current_state(TASK_INTERRUPTIBLE);

		if (kthread_should_stop()) {
			set_current_state(TASK_RUNNING);
			spin_unlock_bh(&pool->sp_lock);
			return -EINTR;
		}

		add_wait_queue(&rqstp->rq_wait, &wait);
		spin_unlock_bh(&pool->sp_lock);

		time_left = schedule_timeout(timeout);

		try_to_freeze();

		spin_lock_bh(&pool->sp_lock);
		remove_wait_queue(&rqstp->rq_wait, &wait);
		if (!time_left)
			pool->sp_stats.threads_timedout++;

		xprt = rqstp->rq_xprt;
		if (!xprt) {
			svc_thread_dequeue(pool, rqstp);
			spin_unlock_bh(&pool->sp_lock);
			dprintk("svc: server %p, no data yet\n", rqstp);
			if (signalled() || kthread_should_stop())
				return -EINTR;
			else
				return -EAGAIN;
		}
	}
	spin_unlock_bh(&pool->sp_lock);

	len = 0;
	if (test_bit(XPT_CLOSE, &xprt->xpt_flags)) {
		dprintk("svc_recv: found XPT_CLOSE\n");
		svc_delete_xprt(xprt);
		
		goto out;
	}
	if (test_bit(XPT_LISTENER, &xprt->xpt_flags)) {
		struct svc_xprt *newxpt;
		newxpt = xprt->xpt_ops->xpo_accept(xprt);
		if (newxpt) {
			__module_get(newxpt->xpt_class->xcl_owner);
			svc_check_conn_limits(xprt->xpt_server);
			spin_lock_bh(&serv->sv_lock);
			set_bit(XPT_TEMP, &newxpt->xpt_flags);
			list_add(&newxpt->xpt_list, &serv->sv_tempsocks);
			serv->sv_tmpcnt++;
			if (serv->sv_temptimer.function == NULL) {
				
				setup_timer(&serv->sv_temptimer,
					    svc_age_temp_xprts,
					    (unsigned long)serv);
				mod_timer(&serv->sv_temptimer,
					  jiffies + svc_conn_age_period * HZ);
			}
			spin_unlock_bh(&serv->sv_lock);
			svc_xprt_received(newxpt);
		}
	} else if (xprt->xpt_ops->xpo_has_wspace(xprt)) {
		dprintk("svc: server %p, pool %u, transport %p, inuse=%d\n",
			rqstp, pool->sp_id, xprt,
			atomic_read(&xprt->xpt_ref.refcount));
		rqstp->rq_deferred = svc_deferred_dequeue(xprt);
		if (rqstp->rq_deferred)
			len = svc_deferred_recv(rqstp);
		else
			len = xprt->xpt_ops->xpo_recvfrom(rqstp);
		dprintk("svc: got len=%d\n", len);
	}
	svc_xprt_received(xprt);

	
	if (len == 0 || len == -EAGAIN)
		goto out;

	clear_bit(XPT_OLD, &xprt->xpt_flags);

	rqstp->rq_secure = svc_port_is_privileged(svc_addr(rqstp));
	rqstp->rq_chandle.defer = svc_defer;

	if (serv->sv_stats)
		serv->sv_stats->netcnt++;
	return len;
out:
	rqstp->rq_res.len = 0;
	svc_xprt_release(rqstp);
	return -EAGAIN;
}
EXPORT_SYMBOL_GPL(svc_recv);

void svc_drop(struct svc_rqst *rqstp)
{
	dprintk("svc: xprt %p dropped request\n", rqstp->rq_xprt);
	svc_xprt_release(rqstp);
}
EXPORT_SYMBOL_GPL(svc_drop);

int svc_send(struct svc_rqst *rqstp)
{
	struct svc_xprt	*xprt;
	int		len;
	struct xdr_buf	*xb;

	xprt = rqstp->rq_xprt;
	if (!xprt)
		return -EFAULT;

	
	rqstp->rq_xprt->xpt_ops->xpo_release_rqst(rqstp);

	
	xb = &rqstp->rq_res;
	xb->len = xb->head[0].iov_len +
		xb->page_len +
		xb->tail[0].iov_len;

	
	mutex_lock(&xprt->xpt_mutex);
	if (test_bit(XPT_DEAD, &xprt->xpt_flags))
		len = -ENOTCONN;
	else
		len = xprt->xpt_ops->xpo_sendto(rqstp);
	mutex_unlock(&xprt->xpt_mutex);
	rpc_wake_up(&xprt->xpt_bc_pending);
	svc_xprt_release(rqstp);

	if (len == -ECONNREFUSED || len == -ENOTCONN || len == -EAGAIN)
		return 0;
	return len;
}

static void svc_age_temp_xprts(unsigned long closure)
{
	struct svc_serv *serv = (struct svc_serv *)closure;
	struct svc_xprt *xprt;
	struct list_head *le, *next;
	LIST_HEAD(to_be_aged);

	dprintk("svc_age_temp_xprts\n");

	if (!spin_trylock_bh(&serv->sv_lock)) {
		
		dprintk("svc_age_temp_xprts: busy\n");
		mod_timer(&serv->sv_temptimer, jiffies + HZ);
		return;
	}

	list_for_each_safe(le, next, &serv->sv_tempsocks) {
		xprt = list_entry(le, struct svc_xprt, xpt_list);

		if (!test_and_set_bit(XPT_OLD, &xprt->xpt_flags))
			continue;
		if (atomic_read(&xprt->xpt_ref.refcount) > 1 ||
		    test_bit(XPT_BUSY, &xprt->xpt_flags))
			continue;
		svc_xprt_get(xprt);
		list_move(le, &to_be_aged);
		set_bit(XPT_CLOSE, &xprt->xpt_flags);
		set_bit(XPT_DETACHED, &xprt->xpt_flags);
	}
	spin_unlock_bh(&serv->sv_lock);

	while (!list_empty(&to_be_aged)) {
		le = to_be_aged.next;
		
		list_del_init(le);
		xprt = list_entry(le, struct svc_xprt, xpt_list);

		dprintk("queuing xprt %p for closing\n", xprt);

		
		svc_xprt_enqueue(xprt);
		svc_xprt_put(xprt);
	}

	mod_timer(&serv->sv_temptimer, jiffies + svc_conn_age_period * HZ);
}

static void call_xpt_users(struct svc_xprt *xprt)
{
	struct svc_xpt_user *u;

	spin_lock(&xprt->xpt_lock);
	while (!list_empty(&xprt->xpt_users)) {
		u = list_first_entry(&xprt->xpt_users, struct svc_xpt_user, list);
		list_del(&u->list);
		u->callback(u);
	}
	spin_unlock(&xprt->xpt_lock);
}

static void svc_delete_xprt(struct svc_xprt *xprt)
{
	struct svc_serv	*serv = xprt->xpt_server;
	struct svc_deferred_req *dr;

	
	if (test_and_set_bit(XPT_DEAD, &xprt->xpt_flags))
		BUG();

	dprintk("svc: svc_delete_xprt(%p)\n", xprt);
	xprt->xpt_ops->xpo_detach(xprt);

	spin_lock_bh(&serv->sv_lock);
	if (!test_and_set_bit(XPT_DETACHED, &xprt->xpt_flags))
		list_del_init(&xprt->xpt_list);
	BUG_ON(!list_empty(&xprt->xpt_ready));
	if (test_bit(XPT_TEMP, &xprt->xpt_flags))
		serv->sv_tmpcnt--;
	spin_unlock_bh(&serv->sv_lock);

	while ((dr = svc_deferred_dequeue(xprt)) != NULL)
		kfree(dr);

	call_xpt_users(xprt);
	svc_xprt_put(xprt);
}

void svc_close_xprt(struct svc_xprt *xprt)
{
	set_bit(XPT_CLOSE, &xprt->xpt_flags);
	if (test_and_set_bit(XPT_BUSY, &xprt->xpt_flags))
		
		return;
	svc_delete_xprt(xprt);
}
EXPORT_SYMBOL_GPL(svc_close_xprt);

static void svc_close_list(struct list_head *xprt_list, struct net *net)
{
	struct svc_xprt *xprt;

	list_for_each_entry(xprt, xprt_list, xpt_list) {
		if (xprt->xpt_net != net)
			continue;
		set_bit(XPT_CLOSE, &xprt->xpt_flags);
		set_bit(XPT_BUSY, &xprt->xpt_flags);
	}
}

static void svc_clear_pools(struct svc_serv *serv, struct net *net)
{
	struct svc_pool *pool;
	struct svc_xprt *xprt;
	struct svc_xprt *tmp;
	int i;

	for (i = 0; i < serv->sv_nrpools; i++) {
		pool = &serv->sv_pools[i];

		spin_lock_bh(&pool->sp_lock);
		list_for_each_entry_safe(xprt, tmp, &pool->sp_sockets, xpt_ready) {
			if (xprt->xpt_net != net)
				continue;
			list_del_init(&xprt->xpt_ready);
		}
		spin_unlock_bh(&pool->sp_lock);
	}
}

static void svc_clear_list(struct list_head *xprt_list, struct net *net)
{
	struct svc_xprt *xprt;
	struct svc_xprt *tmp;

	list_for_each_entry_safe(xprt, tmp, xprt_list, xpt_list) {
		if (xprt->xpt_net != net)
			continue;
		svc_delete_xprt(xprt);
	}
	list_for_each_entry(xprt, xprt_list, xpt_list)
		BUG_ON(xprt->xpt_net == net);
}

void svc_close_net(struct svc_serv *serv, struct net *net)
{
	svc_close_list(&serv->sv_tempsocks, net);
	svc_close_list(&serv->sv_permsocks, net);

	svc_clear_pools(serv, net);
	svc_clear_list(&serv->sv_tempsocks, net);
	svc_clear_list(&serv->sv_permsocks, net);
}


static void svc_revisit(struct cache_deferred_req *dreq, int too_many)
{
	struct svc_deferred_req *dr =
		container_of(dreq, struct svc_deferred_req, handle);
	struct svc_xprt *xprt = dr->xprt;

	spin_lock(&xprt->xpt_lock);
	set_bit(XPT_DEFERRED, &xprt->xpt_flags);
	if (too_many || test_bit(XPT_DEAD, &xprt->xpt_flags)) {
		spin_unlock(&xprt->xpt_lock);
		dprintk("revisit canceled\n");
		svc_xprt_put(xprt);
		kfree(dr);
		return;
	}
	dprintk("revisit queued\n");
	dr->xprt = NULL;
	list_add(&dr->handle.recent, &xprt->xpt_deferred);
	spin_unlock(&xprt->xpt_lock);
	svc_xprt_enqueue(xprt);
	svc_xprt_put(xprt);
}

static struct cache_deferred_req *svc_defer(struct cache_req *req)
{
	struct svc_rqst *rqstp = container_of(req, struct svc_rqst, rq_chandle);
	struct svc_deferred_req *dr;

	if (rqstp->rq_arg.page_len || !rqstp->rq_usedeferral)
		return NULL; 
	if (rqstp->rq_deferred) {
		dr = rqstp->rq_deferred;
		rqstp->rq_deferred = NULL;
	} else {
		size_t skip;
		size_t size;
		
		size = sizeof(struct svc_deferred_req) + rqstp->rq_arg.len;
		dr = kmalloc(size, GFP_KERNEL);
		if (dr == NULL)
			return NULL;

		dr->handle.owner = rqstp->rq_server;
		dr->prot = rqstp->rq_prot;
		memcpy(&dr->addr, &rqstp->rq_addr, rqstp->rq_addrlen);
		dr->addrlen = rqstp->rq_addrlen;
		dr->daddr = rqstp->rq_daddr;
		dr->argslen = rqstp->rq_arg.len >> 2;
		dr->xprt_hlen = rqstp->rq_xprt_hlen;

		
		skip = rqstp->rq_arg.len - rqstp->rq_arg.head[0].iov_len;
		memcpy(dr->args, rqstp->rq_arg.head[0].iov_base - skip,
		       dr->argslen << 2);
	}
	svc_xprt_get(rqstp->rq_xprt);
	dr->xprt = rqstp->rq_xprt;
	rqstp->rq_dropme = true;

	dr->handle.revisit = svc_revisit;
	return &dr->handle;
}

static int svc_deferred_recv(struct svc_rqst *rqstp)
{
	struct svc_deferred_req *dr = rqstp->rq_deferred;

	
	rqstp->rq_arg.head[0].iov_base = dr->args + (dr->xprt_hlen>>2);
	
	rqstp->rq_arg.head[0].iov_len = (dr->argslen<<2) - dr->xprt_hlen;
	rqstp->rq_arg.page_len = 0;
	
	rqstp->rq_arg.len     = dr->argslen<<2;
	rqstp->rq_prot        = dr->prot;
	memcpy(&rqstp->rq_addr, &dr->addr, dr->addrlen);
	rqstp->rq_addrlen     = dr->addrlen;
	
	rqstp->rq_xprt_hlen   = dr->xprt_hlen;
	rqstp->rq_daddr       = dr->daddr;
	rqstp->rq_respages    = rqstp->rq_pages;
	return (dr->argslen<<2) - dr->xprt_hlen;
}


static struct svc_deferred_req *svc_deferred_dequeue(struct svc_xprt *xprt)
{
	struct svc_deferred_req *dr = NULL;

	if (!test_bit(XPT_DEFERRED, &xprt->xpt_flags))
		return NULL;
	spin_lock(&xprt->xpt_lock);
	if (!list_empty(&xprt->xpt_deferred)) {
		dr = list_entry(xprt->xpt_deferred.next,
				struct svc_deferred_req,
				handle.recent);
		list_del_init(&dr->handle.recent);
	} else
		clear_bit(XPT_DEFERRED, &xprt->xpt_flags);
	spin_unlock(&xprt->xpt_lock);
	return dr;
}

struct svc_xprt *svc_find_xprt(struct svc_serv *serv, const char *xcl_name,
			       struct net *net, const sa_family_t af,
			       const unsigned short port)
{
	struct svc_xprt *xprt;
	struct svc_xprt *found = NULL;

	
	if (serv == NULL || xcl_name == NULL)
		return found;

	spin_lock_bh(&serv->sv_lock);
	list_for_each_entry(xprt, &serv->sv_permsocks, xpt_list) {
		if (xprt->xpt_net != net)
			continue;
		if (strcmp(xprt->xpt_class->xcl_name, xcl_name))
			continue;
		if (af != AF_UNSPEC && af != xprt->xpt_local.ss_family)
			continue;
		if (port != 0 && port != svc_xprt_local_port(xprt))
			continue;
		found = xprt;
		svc_xprt_get(xprt);
		break;
	}
	spin_unlock_bh(&serv->sv_lock);
	return found;
}
EXPORT_SYMBOL_GPL(svc_find_xprt);

static int svc_one_xprt_name(const struct svc_xprt *xprt,
			     char *pos, int remaining)
{
	int len;

	len = snprintf(pos, remaining, "%s %u\n",
			xprt->xpt_class->xcl_name,
			svc_xprt_local_port(xprt));
	if (len >= remaining)
		return -ENAMETOOLONG;
	return len;
}

int svc_xprt_names(struct svc_serv *serv, char *buf, const int buflen)
{
	struct svc_xprt *xprt;
	int len, totlen;
	char *pos;

	
	if (!serv)
		return 0;

	spin_lock_bh(&serv->sv_lock);

	pos = buf;
	totlen = 0;
	list_for_each_entry(xprt, &serv->sv_permsocks, xpt_list) {
		len = svc_one_xprt_name(xprt, pos, buflen - totlen);
		if (len < 0) {
			*buf = '\0';
			totlen = len;
		}
		if (len <= 0)
			break;

		pos += len;
		totlen += len;
	}

	spin_unlock_bh(&serv->sv_lock);
	return totlen;
}
EXPORT_SYMBOL_GPL(svc_xprt_names);



static void *svc_pool_stats_start(struct seq_file *m, loff_t *pos)
{
	unsigned int pidx = (unsigned int)*pos;
	struct svc_serv *serv = m->private;

	dprintk("svc_pool_stats_start, *pidx=%u\n", pidx);

	if (!pidx)
		return SEQ_START_TOKEN;
	return (pidx > serv->sv_nrpools ? NULL : &serv->sv_pools[pidx-1]);
}

static void *svc_pool_stats_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct svc_pool *pool = p;
	struct svc_serv *serv = m->private;

	dprintk("svc_pool_stats_next, *pos=%llu\n", *pos);

	if (p == SEQ_START_TOKEN) {
		pool = &serv->sv_pools[0];
	} else {
		unsigned int pidx = (pool - &serv->sv_pools[0]);
		if (pidx < serv->sv_nrpools-1)
			pool = &serv->sv_pools[pidx+1];
		else
			pool = NULL;
	}
	++*pos;
	return pool;
}

static void svc_pool_stats_stop(struct seq_file *m, void *p)
{
}

static int svc_pool_stats_show(struct seq_file *m, void *p)
{
	struct svc_pool *pool = p;

	if (p == SEQ_START_TOKEN) {
		seq_puts(m, "# pool packets-arrived sockets-enqueued threads-woken threads-timedout\n");
		return 0;
	}

	seq_printf(m, "%u %lu %lu %lu %lu\n",
		pool->sp_id,
		pool->sp_stats.packets,
		pool->sp_stats.sockets_queued,
		pool->sp_stats.threads_woken,
		pool->sp_stats.threads_timedout);

	return 0;
}

static const struct seq_operations svc_pool_stats_seq_ops = {
	.start	= svc_pool_stats_start,
	.next	= svc_pool_stats_next,
	.stop	= svc_pool_stats_stop,
	.show	= svc_pool_stats_show,
};

int svc_pool_stats_open(struct svc_serv *serv, struct file *file)
{
	int err;

	err = seq_open(file, &svc_pool_stats_seq_ops);
	if (!err)
		((struct seq_file *) file->private_data)->private = serv;
	return err;
}
EXPORT_SYMBOL(svc_pool_stats_open);

