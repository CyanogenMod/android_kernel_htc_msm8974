#include <media/saa7146_vv.h>

static int vbi_pixel_to_capture = 720 * 2;

static int vbi_workaround(struct saa7146_dev *dev)
{
	struct saa7146_vv *vv = dev->vv_data;

	u32          *cpu;
	dma_addr_t   dma_addr;

	int count = 0;
	int i;

	DECLARE_WAITQUEUE(wait, current);

	DEB_VBI("dev:%p\n", dev);


	cpu = pci_alloc_consistent(dev->pci, 4096, &dma_addr);
	if (NULL == cpu)
		return -ENOMEM;

	
	saa7146_write(dev, BASE_EVEN3,	dma_addr);
	saa7146_write(dev, BASE_ODD3,	dma_addr+vbi_pixel_to_capture);
	saa7146_write(dev, PROT_ADDR3,	dma_addr+4096);
	saa7146_write(dev, PITCH3,	vbi_pixel_to_capture);
	saa7146_write(dev, BASE_PAGE3,	0x0);
	saa7146_write(dev, NUM_LINE_BYTE3, (2<<16)|((vbi_pixel_to_capture)<<0));
	saa7146_write(dev, MC2, MASK_04|MASK_20);

	
	WRITE_RPS1(CMD_WR_REG | (1 << 8) | (BRS_CTRL/4));
	
	WRITE_RPS1(0xc000008c);
	
	if ( 0 != (SAA7146_USE_PORT_B_FOR_VBI & dev->ext_vv_data->flags)) {
		DEB_D("...using port b\n");
		WRITE_RPS1(CMD_PAUSE | CMD_OAN | CMD_SIG1 | CMD_E_FID_B);
		WRITE_RPS1(CMD_PAUSE | CMD_OAN | CMD_SIG1 | CMD_O_FID_B);
	} else {
		DEB_D("...using port a\n");
		WRITE_RPS1(CMD_PAUSE | MASK_10);
	}
	
	WRITE_RPS1(CMD_UPLOAD | MASK_08);
	
	WRITE_RPS1(CMD_WR_REG | (1 << 8) | (BRS_CTRL/4));
	
	WRITE_RPS1(((1728-(vbi_pixel_to_capture)) << 7) | MASK_19);
	
	WRITE_RPS1(CMD_PAUSE | MASK_08);
	
	WRITE_RPS1(CMD_UPLOAD | MASK_08);
	
	WRITE_RPS1(CMD_WR_REG | (1 << 8) | (NUM_LINE_BYTE3/4));
	
	WRITE_RPS1((2 << 16) | (vbi_pixel_to_capture));
	
	WRITE_RPS1(CMD_WR_REG | (1 << 8) | (BRS_CTRL/4));
	
	WRITE_RPS1((540 << 7) | (5 << 19));  
	
	WRITE_RPS1(CMD_PAUSE | MASK_08);
	
	WRITE_RPS1(CMD_UPLOAD | MASK_08 | MASK_04);
	
	WRITE_RPS1(CMD_WR_REG | (1 << 8) | (MC1/4));
	WRITE_RPS1(MASK_20 | MASK_04);
	
	WRITE_RPS1(CMD_INTERRUPT);
	
	WRITE_RPS1(CMD_STOP);

	for(i = 0; i < 2; i++) {

		
		saa7146_write(dev, MC2, MASK_31|MASK_15);

		saa7146_write(dev, NUM_LINE_BYTE3, (1<<16)|(2<<0));
		saa7146_write(dev, MC2, MASK_04|MASK_20);

		
		SAA7146_IER_ENABLE(dev,MASK_28);

		
		add_wait_queue(&vv->vbi_wq, &wait);
		current->state = TASK_INTERRUPTIBLE;

		
		saa7146_write(dev, RPS_ADDR1, dev->d_rps1.dma_handle);
		saa7146_write(dev, MC1, (MASK_13 | MASK_29));

		schedule();

		DEB_VBI("brs bug workaround %d/1\n", i);

		remove_wait_queue(&vv->vbi_wq, &wait);
		current->state = TASK_RUNNING;

		
		SAA7146_IER_DISABLE(dev,MASK_28);

		
		saa7146_write(dev, MC1, MASK_20);

		if(signal_pending(current)) {

			DEB_VBI("aborted (rps:0x%08x)\n",
				saa7146_read(dev, RPS_ADDR1));

			
			saa7146_write(dev, MC1, MASK_29);

			pci_free_consistent(dev->pci, 4096, cpu, dma_addr);
			return -EINTR;
		}
	}

	pci_free_consistent(dev->pci, 4096, cpu, dma_addr);
	return 0;
}

static void saa7146_set_vbi_capture(struct saa7146_dev *dev, struct saa7146_buf *buf, struct saa7146_buf *next)
{
	struct saa7146_vv *vv = dev->vv_data;

	struct saa7146_video_dma vdma3;

	int count = 0;
	unsigned long e_wait = vv->current_hps_sync == SAA7146_HPS_SYNC_PORT_A ? CMD_E_FID_A : CMD_E_FID_B;
	unsigned long o_wait = vv->current_hps_sync == SAA7146_HPS_SYNC_PORT_A ? CMD_O_FID_A : CMD_O_FID_B;

	vdma3.base_even	= buf->pt[2].offset;
	vdma3.base_odd	= buf->pt[2].offset + 16 * vbi_pixel_to_capture;
	vdma3.prot_addr	= buf->pt[2].offset + 16 * 2 * vbi_pixel_to_capture;
	vdma3.pitch	= vbi_pixel_to_capture;
	vdma3.base_page	= buf->pt[2].dma | ME1;
	vdma3.num_line_byte = (16 << 16) | vbi_pixel_to_capture;

	saa7146_write_out_dma(dev, 3, &vdma3);

	
	count = 0;

	


	
	WRITE_RPS1(CMD_WR_REG | (1 << 8) | (MC2/4));
	WRITE_RPS1(MASK_28 | MASK_12);

	
	WRITE_RPS1(CMD_WR_REG_MASK | (MC1/4));
	WRITE_RPS1(MASK_04 | MASK_20);			
	WRITE_RPS1(MASK_04 | MASK_20);			

	
	WRITE_RPS1(CMD_PAUSE | o_wait);
	WRITE_RPS1(CMD_PAUSE | e_wait);

	
	WRITE_RPS1(CMD_INTERRUPT);

	
	WRITE_RPS1(CMD_STOP);

	
	SAA7146_IER_ENABLE(dev, MASK_28);

	
	saa7146_write(dev, RPS_ADDR1, dev->d_rps1.dma_handle);

	
	saa7146_write(dev, MC1, (MASK_13 | MASK_29));
}

static int buffer_activate(struct saa7146_dev *dev,
			   struct saa7146_buf *buf,
			   struct saa7146_buf *next)
{
	struct saa7146_vv *vv = dev->vv_data;
	buf->vb.state = VIDEOBUF_ACTIVE;

	DEB_VBI("dev:%p, buf:%p, next:%p\n", dev, buf, next);
	saa7146_set_vbi_capture(dev,buf,next);

	mod_timer(&vv->vbi_q.timeout, jiffies+BUFFER_TIMEOUT);
	return 0;
}

static int buffer_prepare(struct videobuf_queue *q, struct videobuf_buffer *vb,enum v4l2_field field)
{
	struct file *file = q->priv_data;
	struct saa7146_fh *fh = file->private_data;
	struct saa7146_dev *dev = fh->dev;
	struct saa7146_buf *buf = (struct saa7146_buf *)vb;

	int err = 0;
	int lines, llength, size;

	lines   = 16 * 2 ; 
	llength = vbi_pixel_to_capture;
	size = lines * llength;

	DEB_VBI("vb:%p\n", vb);

	if (0 != buf->vb.baddr  &&  buf->vb.bsize < size) {
		DEB_VBI("size mismatch\n");
		return -EINVAL;
	}

	if (buf->vb.size != size)
		saa7146_dma_free(dev,q,buf);

	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		struct videobuf_dmabuf *dma=videobuf_to_dma(&buf->vb);

		buf->vb.width  = llength;
		buf->vb.height = lines;
		buf->vb.size   = size;
		buf->vb.field  = field;	

		saa7146_pgtable_free(dev->pci, &buf->pt[2]);
		saa7146_pgtable_alloc(dev->pci, &buf->pt[2]);

		err = videobuf_iolock(q,&buf->vb, NULL);
		if (err)
			goto oops;
		err = saa7146_pgtable_build_single(dev->pci, &buf->pt[2],
						 dma->sglist, dma->sglen);
		if (0 != err)
			return err;
	}
	buf->vb.state = VIDEOBUF_PREPARED;
	buf->activate = buffer_activate;

	return 0;

 oops:
	DEB_VBI("error out\n");
	saa7146_dma_free(dev,q,buf);

	return err;
}

static int buffer_setup(struct videobuf_queue *q, unsigned int *count, unsigned int *size)
{
	int llength,lines;

	lines   = 16 * 2 ; 
	llength = vbi_pixel_to_capture;

	*size = lines * llength;
	*count = 2;

	DEB_VBI("count:%d, size:%d\n", *count, *size);

	return 0;
}

static void buffer_queue(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct file *file = q->priv_data;
	struct saa7146_fh *fh = file->private_data;
	struct saa7146_dev *dev = fh->dev;
	struct saa7146_vv *vv = dev->vv_data;
	struct saa7146_buf *buf = (struct saa7146_buf *)vb;

	DEB_VBI("vb:%p\n", vb);
	saa7146_buffer_queue(dev,&vv->vbi_q,buf);
}

static void buffer_release(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct file *file = q->priv_data;
	struct saa7146_fh *fh   = file->private_data;
	struct saa7146_dev *dev = fh->dev;
	struct saa7146_buf *buf = (struct saa7146_buf *)vb;

	DEB_VBI("vb:%p\n", vb);
	saa7146_dma_free(dev,q,buf);
}

static struct videobuf_queue_ops vbi_qops = {
	.buf_setup    = buffer_setup,
	.buf_prepare  = buffer_prepare,
	.buf_queue    = buffer_queue,
	.buf_release  = buffer_release,
};


static void vbi_stop(struct saa7146_fh *fh, struct file *file)
{
	struct saa7146_dev *dev = fh->dev;
	struct saa7146_vv *vv = dev->vv_data;
	unsigned long flags;
	DEB_VBI("dev:%p, fh:%p\n", dev, fh);

	spin_lock_irqsave(&dev->slock,flags);

	
	saa7146_write(dev, MC1, MASK_29);

	
	SAA7146_IER_DISABLE(dev, MASK_28);

	
	saa7146_write(dev, MC1, MASK_20);

	if (vv->vbi_q.curr) {
		saa7146_buffer_finish(dev,&vv->vbi_q,VIDEOBUF_DONE);
	}

	videobuf_queue_cancel(&fh->vbi_q);

	vv->vbi_streaming = NULL;

	del_timer(&vv->vbi_q.timeout);
	del_timer(&fh->vbi_read_timeout);

	spin_unlock_irqrestore(&dev->slock, flags);
}

static void vbi_read_timeout(unsigned long data)
{
	struct file *file = (struct file*)data;
	struct saa7146_fh *fh = file->private_data;
	struct saa7146_dev *dev = fh->dev;

	DEB_VBI("dev:%p, fh:%p\n", dev, fh);

	vbi_stop(fh, file);
}

static void vbi_init(struct saa7146_dev *dev, struct saa7146_vv *vv)
{
	DEB_VBI("dev:%p\n", dev);

	INIT_LIST_HEAD(&vv->vbi_q.queue);

	init_timer(&vv->vbi_q.timeout);
	vv->vbi_q.timeout.function = saa7146_buffer_timeout;
	vv->vbi_q.timeout.data     = (unsigned long)(&vv->vbi_q);
	vv->vbi_q.dev              = dev;

	init_waitqueue_head(&vv->vbi_wq);
}

static int vbi_open(struct saa7146_dev *dev, struct file *file)
{
	struct saa7146_fh *fh = file->private_data;

	u32 arbtr_ctrl	= saa7146_read(dev, PCI_BT_V1);
	int ret = 0;

	DEB_VBI("dev:%p, fh:%p\n", dev, fh);

	ret = saa7146_res_get(fh, RESOURCE_DMA3_BRS);
	if (0 == ret) {
		DEB_S("cannot get vbi RESOURCE_DMA3_BRS resource\n");
		return -EBUSY;
	}

	
	arbtr_ctrl &= ~0x1f0000;
	arbtr_ctrl |=  0x1d0000;
	saa7146_write(dev, PCI_BT_V1, arbtr_ctrl);
	saa7146_write(dev, MC2, (MASK_04|MASK_20));

	memset(&fh->vbi_fmt,0,sizeof(fh->vbi_fmt));

	fh->vbi_fmt.sampling_rate	= 27000000;
	fh->vbi_fmt.offset		= 248; 
	fh->vbi_fmt.samples_per_line	= vbi_pixel_to_capture;
	fh->vbi_fmt.sample_format	= V4L2_PIX_FMT_GREY;

	
	fh->vbi_fmt.start[0] = 5;
	fh->vbi_fmt.count[0] = 16;
	fh->vbi_fmt.start[1] = 312;
	fh->vbi_fmt.count[1] = 16;

	videobuf_queue_sg_init(&fh->vbi_q, &vbi_qops,
			    &dev->pci->dev, &dev->slock,
			    V4L2_BUF_TYPE_VBI_CAPTURE,
			    V4L2_FIELD_SEQ_TB, 
			    sizeof(struct saa7146_buf),
			    file, &dev->v4l2_lock);

	init_timer(&fh->vbi_read_timeout);
	fh->vbi_read_timeout.function = vbi_read_timeout;
	fh->vbi_read_timeout.data = (unsigned long)file;

	
	if ( 0 != (SAA7146_USE_PORT_B_FOR_VBI & dev->ext_vv_data->flags)) {
		saa7146_write(dev, BRS_CTRL, MASK_30|MASK_29 | (7 << 19));
	} else {
		saa7146_write(dev, BRS_CTRL, 0x00000001);

		if (0 != (ret = vbi_workaround(dev))) {
			DEB_VBI("vbi workaround failed!\n");
			
		}
	}

	
	saa7146_write(dev, MC2, (MASK_08|MASK_24));
	return 0;
}

static void vbi_close(struct saa7146_dev *dev, struct file *file)
{
	struct saa7146_fh *fh = file->private_data;
	struct saa7146_vv *vv = dev->vv_data;
	DEB_VBI("dev:%p, fh:%p\n", dev, fh);

	if( fh == vv->vbi_streaming ) {
		vbi_stop(fh, file);
	}
	saa7146_res_free(fh, RESOURCE_DMA3_BRS);
}

static void vbi_irq_done(struct saa7146_dev *dev, unsigned long status)
{
	struct saa7146_vv *vv = dev->vv_data;
	spin_lock(&dev->slock);

	if (vv->vbi_q.curr) {
		DEB_VBI("dev:%p, curr:%p\n", dev, vv->vbi_q.curr);
		
		vv->vbi_fieldcount+=2;
		vv->vbi_q.curr->vb.field_count = vv->vbi_fieldcount;
		saa7146_buffer_finish(dev,&vv->vbi_q,VIDEOBUF_DONE);
	} else {
		DEB_VBI("dev:%p\n", dev);
	}
	saa7146_buffer_next(dev,&vv->vbi_q,1);

	spin_unlock(&dev->slock);
}

static ssize_t vbi_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct saa7146_fh *fh = file->private_data;
	struct saa7146_dev *dev = fh->dev;
	struct saa7146_vv *vv = dev->vv_data;
	ssize_t ret = 0;

	DEB_VBI("dev:%p, fh:%p\n", dev, fh);

	if( NULL == vv->vbi_streaming ) {
		
		
		vv->vbi_streaming = fh;
	}

	if( fh != vv->vbi_streaming ) {
		DEB_VBI("open %p is already using vbi capture\n",
			vv->vbi_streaming);
		return -EBUSY;
	}

	mod_timer(&fh->vbi_read_timeout, jiffies+BUFFER_TIMEOUT);
	ret = videobuf_read_stream(&fh->vbi_q, data, count, ppos, 1,
				   file->f_flags & O_NONBLOCK);
	return ret;
}

struct saa7146_use_ops saa7146_vbi_uops = {
	.init		= vbi_init,
	.open		= vbi_open,
	.release	= vbi_close,
	.irq_done	= vbi_irq_done,
	.read		= vbi_read,
};
