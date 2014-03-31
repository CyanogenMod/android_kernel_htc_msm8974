/******************************************************************************

(c) 2007 Network Appliance, Inc.  All Rights Reserved.
(c) 2009 NetApp.  All Rights Reserved.

NetApp provides this source code under the GPL v2 License.
The GPL v2 license is available at
http://opensource.org/licenses/gpl-license.php.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include <linux/tcp.h>
#include <linux/slab.h>
#include <linux/sunrpc/xprt.h>
#include <linux/export.h>
#include <linux/sunrpc/bc_xprt.h>

#ifdef RPC_DEBUG
#define RPCDBG_FACILITY	RPCDBG_TRANS
#endif

static inline int xprt_need_to_requeue(struct rpc_xprt *xprt)
{
	return xprt->bc_alloc_count > 0;
}

static inline void xprt_inc_alloc_count(struct rpc_xprt *xprt, unsigned int n)
{
	xprt->bc_alloc_count += n;
}

static inline int xprt_dec_alloc_count(struct rpc_xprt *xprt, unsigned int n)
{
	return xprt->bc_alloc_count -= n;
}

static void xprt_free_allocation(struct rpc_rqst *req)
{
	struct xdr_buf *xbufp;

	dprintk("RPC:        free allocations for req= %p\n", req);
	BUG_ON(test_bit(RPC_BC_PA_IN_USE, &req->rq_bc_pa_state));
	xbufp = &req->rq_private_buf;
	free_page((unsigned long)xbufp->head[0].iov_base);
	xbufp = &req->rq_snd_buf;
	free_page((unsigned long)xbufp->head[0].iov_base);
	list_del(&req->rq_bc_pa_list);
	kfree(req);
}

int xprt_setup_backchannel(struct rpc_xprt *xprt, unsigned int min_reqs)
{
	struct page *page_rcv = NULL, *page_snd = NULL;
	struct xdr_buf *xbufp = NULL;
	struct rpc_rqst *req, *tmp;
	struct list_head tmp_list;
	int i;

	dprintk("RPC:       setup backchannel transport\n");

	INIT_LIST_HEAD(&tmp_list);
	for (i = 0; i < min_reqs; i++) {
		
		req = kzalloc(sizeof(struct rpc_rqst), GFP_KERNEL);
		if (req == NULL) {
			printk(KERN_ERR "Failed to create bc rpc_rqst\n");
			goto out_free;
		}

		
		dprintk("RPC:       adding req= %p\n", req);
		list_add(&req->rq_bc_pa_list, &tmp_list);

		req->rq_xprt = xprt;
		INIT_LIST_HEAD(&req->rq_list);
		INIT_LIST_HEAD(&req->rq_bc_list);

		
		page_rcv = alloc_page(GFP_KERNEL);
		if (page_rcv == NULL) {
			printk(KERN_ERR "Failed to create bc receive xbuf\n");
			goto out_free;
		}
		xbufp = &req->rq_rcv_buf;
		xbufp->head[0].iov_base = page_address(page_rcv);
		xbufp->head[0].iov_len = PAGE_SIZE;
		xbufp->tail[0].iov_base = NULL;
		xbufp->tail[0].iov_len = 0;
		xbufp->page_len = 0;
		xbufp->len = PAGE_SIZE;
		xbufp->buflen = PAGE_SIZE;

		
		page_snd = alloc_page(GFP_KERNEL);
		if (page_snd == NULL) {
			printk(KERN_ERR "Failed to create bc snd xbuf\n");
			goto out_free;
		}

		xbufp = &req->rq_snd_buf;
		xbufp->head[0].iov_base = page_address(page_snd);
		xbufp->head[0].iov_len = 0;
		xbufp->tail[0].iov_base = NULL;
		xbufp->tail[0].iov_len = 0;
		xbufp->page_len = 0;
		xbufp->len = 0;
		xbufp->buflen = PAGE_SIZE;
	}

	spin_lock_bh(&xprt->bc_pa_lock);
	list_splice(&tmp_list, &xprt->bc_pa_list);
	xprt_inc_alloc_count(xprt, min_reqs);
	spin_unlock_bh(&xprt->bc_pa_lock);

	dprintk("RPC:       setup backchannel transport done\n");
	return 0;

out_free:
	list_for_each_entry_safe(req, tmp, &tmp_list, rq_bc_pa_list)
		xprt_free_allocation(req);

	dprintk("RPC:       setup backchannel transport failed\n");
	return -1;
}
EXPORT_SYMBOL_GPL(xprt_setup_backchannel);

void xprt_destroy_backchannel(struct rpc_xprt *xprt, unsigned int max_reqs)
{
	struct rpc_rqst *req = NULL, *tmp = NULL;

	dprintk("RPC:        destroy backchannel transport\n");

	BUG_ON(max_reqs == 0);
	spin_lock_bh(&xprt->bc_pa_lock);
	xprt_dec_alloc_count(xprt, max_reqs);
	list_for_each_entry_safe(req, tmp, &xprt->bc_pa_list, rq_bc_pa_list) {
		dprintk("RPC:        req=%p\n", req);
		xprt_free_allocation(req);
		if (--max_reqs == 0)
			break;
	}
	spin_unlock_bh(&xprt->bc_pa_lock);

	dprintk("RPC:        backchannel list empty= %s\n",
		list_empty(&xprt->bc_pa_list) ? "true" : "false");
}
EXPORT_SYMBOL_GPL(xprt_destroy_backchannel);

struct rpc_rqst *xprt_alloc_bc_request(struct rpc_xprt *xprt)
{
	struct rpc_rqst *req;

	dprintk("RPC:       allocate a backchannel request\n");
	spin_lock(&xprt->bc_pa_lock);
	if (!list_empty(&xprt->bc_pa_list)) {
		req = list_first_entry(&xprt->bc_pa_list, struct rpc_rqst,
				rq_bc_pa_list);
		list_del(&req->rq_bc_pa_list);
	} else {
		req = NULL;
	}
	spin_unlock(&xprt->bc_pa_lock);

	if (req != NULL) {
		set_bit(RPC_BC_PA_IN_USE, &req->rq_bc_pa_state);
		req->rq_reply_bytes_recvd = 0;
		req->rq_bytes_sent = 0;
		memcpy(&req->rq_private_buf, &req->rq_rcv_buf,
			sizeof(req->rq_private_buf));
	}
	dprintk("RPC:       backchannel req=%p\n", req);
	return req;
}

void xprt_free_bc_request(struct rpc_rqst *req)
{
	struct rpc_xprt *xprt = req->rq_xprt;

	dprintk("RPC:       free backchannel req=%p\n", req);

	smp_mb__before_clear_bit();
	BUG_ON(!test_bit(RPC_BC_PA_IN_USE, &req->rq_bc_pa_state));
	clear_bit(RPC_BC_PA_IN_USE, &req->rq_bc_pa_state);
	smp_mb__after_clear_bit();

	if (!xprt_need_to_requeue(xprt)) {
		dprintk("RPC:       Last session removed req=%p\n", req);
		xprt_free_allocation(req);
		return;
	}

	spin_lock_bh(&xprt->bc_pa_lock);
	list_add(&req->rq_bc_pa_list, &xprt->bc_pa_list);
	spin_unlock_bh(&xprt->bc_pa_lock);
}

