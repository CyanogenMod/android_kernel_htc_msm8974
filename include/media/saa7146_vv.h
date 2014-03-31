#ifndef __SAA7146_VV__
#define __SAA7146_VV__

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/saa7146.h>
#include <media/videobuf-dma-sg.h>

#define MAX_SAA7146_CAPTURE_BUFFERS	32	
#define BUFFER_TIMEOUT     (HZ/2)  

#define WRITE_RPS0(x) do { \
	dev->d_rps0.cpu_addr[ count++ ] = cpu_to_le32(x); \
	} while (0);

#define WRITE_RPS1(x) do { \
	dev->d_rps1.cpu_addr[ count++ ] = cpu_to_le32(x); \
	} while (0);

struct	saa7146_video_dma {
	u32 base_odd;
	u32 base_even;
	u32 prot_addr;
	u32 pitch;
	u32 base_page;
	u32 num_line_byte;
};

#define FORMAT_BYTE_SWAP	0x1
#define FORMAT_IS_PLANAR	0x2

struct saa7146_format {
	char	*name;
	u32	pixelformat;
	u32	trans;
	u8	depth;
	u8	flags;
	u8	swap;
};

struct saa7146_standard
{
	char          *name;
	v4l2_std_id   id;

	int v_offset;	
	int v_field;	

	int h_offset;	
	int h_pixels;	

	int v_max_out;
	int h_max_out;
};

struct saa7146_buf {
	
	struct videobuf_buffer vb;

	
	struct v4l2_pix_format  *fmt;
	int (*activate)(struct saa7146_dev *dev,
			struct saa7146_buf *buf,
			struct saa7146_buf *next);

	
	struct saa7146_pgtable  pt[3];
};

struct saa7146_dmaqueue {
	struct saa7146_dev	*dev;
	struct saa7146_buf	*curr;
	struct list_head	queue;
	struct timer_list	timeout;
};

struct saa7146_overlay {
	struct saa7146_fh	*fh;
	struct v4l2_window	win;
	struct v4l2_clip	clips[16];
	int			nclips;
};

struct saa7146_fh {
	struct saa7146_dev	*dev;
	
	enum v4l2_buf_type	type;

	
	struct saa7146_overlay	ov;

	
	struct videobuf_queue	video_q;
	struct v4l2_pix_format	video_fmt;

	
	struct videobuf_queue	vbi_q;
	struct v4l2_vbi_format	vbi_fmt;
	struct timer_list	vbi_read_timeout;

	unsigned int resources;	
};

#define STATUS_OVERLAY	0x01
#define STATUS_CAPTURE	0x02

struct saa7146_vv
{
	
	struct saa7146_dmaqueue		vbi_q;
	
	wait_queue_head_t		vbi_wq;
	int				vbi_fieldcount;
	struct saa7146_fh		*vbi_streaming;

	int				video_status;
	struct saa7146_fh		*video_fh;

	
	struct v4l2_framebuffer		ov_fb;
	struct saa7146_format		*ov_fmt;
	struct saa7146_overlay		*ov_data;
	struct saa7146_fh		*ov_suspend;

	
	struct saa7146_dmaqueue		video_q;
	enum v4l2_field			last_field;

	struct saa7146_standard	*standard;

	int	vflip;
	int	hflip;
	int	current_hps_source;
	int	current_hps_sync;

	struct saa7146_dma	d_clipping;	

	unsigned int resources;	
};

#define SAA7146_USE_PORT_B_FOR_VBI	0x2     

struct saa7146_ext_vv
{
	
	int	inputs;
	int	audios;
	u32	capabilities;
	int	flags;

	
	struct saa7146_standard *stds;
	int num_stds;
	int (*std_callback)(struct saa7146_dev*, struct saa7146_standard *);

	
	struct v4l2_ioctl_ops ops;
	
	const struct v4l2_ioctl_ops *core_ops;

	struct v4l2_file_operations vbi_fops;
};

struct saa7146_use_ops  {
	void (*init)(struct saa7146_dev *, struct saa7146_vv *);
	int(*open)(struct saa7146_dev *, struct file *);
	void (*release)(struct saa7146_dev *, struct file *);
	void (*irq_done)(struct saa7146_dev *, unsigned long status);
	ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
};

int saa7146_register_device(struct video_device **vid, struct saa7146_dev* dev, char *name, int type);
int saa7146_unregister_device(struct video_device **vid, struct saa7146_dev* dev);
void saa7146_buffer_finish(struct saa7146_dev *dev, struct saa7146_dmaqueue *q, int state);
void saa7146_buffer_next(struct saa7146_dev *dev, struct saa7146_dmaqueue *q,int vbi);
int saa7146_buffer_queue(struct saa7146_dev *dev, struct saa7146_dmaqueue *q, struct saa7146_buf *buf);
void saa7146_buffer_timeout(unsigned long data);
void saa7146_dma_free(struct saa7146_dev* dev,struct videobuf_queue *q,
						struct saa7146_buf *buf);

int saa7146_vv_init(struct saa7146_dev* dev, struct saa7146_ext_vv *ext_vv);
int saa7146_vv_release(struct saa7146_dev* dev);

int saa7146_enable_overlay(struct saa7146_fh *fh);
void saa7146_disable_overlay(struct saa7146_fh *fh);

void saa7146_set_capture(struct saa7146_dev *dev, struct saa7146_buf *buf, struct saa7146_buf *next);
void saa7146_write_out_dma(struct saa7146_dev* dev, int which, struct saa7146_video_dma* vdma) ;
void saa7146_set_hps_source_and_sync(struct saa7146_dev *saa, int source, int sync);
void saa7146_set_gpio(struct saa7146_dev *saa, u8 pin, u8 data);

extern const struct v4l2_ioctl_ops saa7146_video_ioctl_ops;
extern struct saa7146_use_ops saa7146_video_uops;
int saa7146_start_preview(struct saa7146_fh *fh);
int saa7146_stop_preview(struct saa7146_fh *fh);
long saa7146_video_do_ioctl(struct file *file, unsigned int cmd, void *arg);

extern struct saa7146_use_ops saa7146_vbi_uops;

int saa7146_res_get(struct saa7146_fh *fh, unsigned int bit);
void saa7146_res_free(struct saa7146_fh *fh, unsigned int bits);

#define RESOURCE_DMA1_HPS	0x1
#define RESOURCE_DMA2_CLP	0x2
#define RESOURCE_DMA3_BRS	0x4

#define SAA7146_HPS_SOURCE_PORT_A	0x00
#define SAA7146_HPS_SOURCE_PORT_B	0x01
#define SAA7146_HPS_SOURCE_YPB_CPA	0x02
#define SAA7146_HPS_SOURCE_YPA_CPB	0x03

#define SAA7146_HPS_SYNC_PORT_A		0x00
#define SAA7146_HPS_SYNC_PORT_B		0x01

#define SAA7146_CLIPPING_MEM	(16 * 4 * sizeof(u32))

#define SAA7146_CLIPPING_RECT		0x4
#define SAA7146_CLIPPING_RECT_INVERTED	0x5
#define SAA7146_CLIPPING_MASK		0x6
#define SAA7146_CLIPPING_MASK_INVERTED	0x7

#define RGB08_COMPOSED	0x0217 
#define RGB15_COMPOSED	0x0213
#define RGB16_COMPOSED	0x0210
#define RGB24_COMPOSED	0x0201
#define RGB32_COMPOSED	0x0202

#define Y8			0x0006
#define YUV411_COMPOSED		0x0003
#define YUV422_COMPOSED		0x0000
#define YUV411_DECOMPOSED	0x100b
#define YUV422_DECOMPOSED	0x1009
#define YUV420_DECOMPOSED	0x100a

#define IS_PLANAR(x) (x & 0xf000)

#define SAA7146_NO_SWAP		(0x0)
#define SAA7146_TWO_BYTE_SWAP	(0x1)
#define SAA7146_FOUR_BYTE_SWAP	(0x2)

#endif
