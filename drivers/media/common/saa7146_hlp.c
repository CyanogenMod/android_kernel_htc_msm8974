#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/export.h>
#include <media/saa7146_vv.h>

static void calculate_output_format_register(struct saa7146_dev* saa, u32 palette, u32* clip_format)
{
	
	*clip_format &= 0x0000ffff;
	
	*clip_format |=  (( ((palette&0xf00)>>8) << 30) | ((palette&0x00f) << 24) | (((palette&0x0f0)>>4) << 16));
}

static void calculate_hps_source_and_sync(struct saa7146_dev *dev, int source, int sync, u32* hps_ctrl)
{
	*hps_ctrl &= ~(MASK_30 | MASK_31 | MASK_28);
	*hps_ctrl |= (source << 30) | (sync << 28);
}

static void calculate_hxo_and_hyo(struct saa7146_vv *vv, u32* hps_h_scale, u32* hps_ctrl)
{
	int hyo = 0, hxo = 0;

	hyo = vv->standard->v_offset;
	hxo = vv->standard->h_offset;

	*hps_h_scale	&= ~(MASK_B0 | 0xf00);
	*hps_h_scale	|= (hxo <<  0);

	*hps_ctrl	&= ~(MASK_W0 | MASK_B2);
	*hps_ctrl	|= (hyo << 12);
}


static struct {
	u16 hps_coeff;
	u16 weight_sum;
} hps_h_coeff_tab [] = {
	{0x00,   2}, {0x02,   4}, {0x00,   4}, {0x06,   8}, {0x02,   8},
	{0x08,   8}, {0x00,   8}, {0x1E,  16}, {0x0E,   8}, {0x26,   8},
	{0x06,   8}, {0x42,   8}, {0x02,   8}, {0x80,   8}, {0x00,   8},
	{0xFE,  16}, {0xFE,   8}, {0x7E,   8}, {0x7E,   8}, {0x3E,   8},
	{0x3E,   8}, {0x1E,   8}, {0x1E,   8}, {0x0E,   8}, {0x0E,   8},
	{0x06,   8}, {0x06,   8}, {0x02,   8}, {0x02,   8}, {0x00,   8},
	{0x00,   8}, {0xFE,  16}, {0xFE,   8}, {0xFE,   8}, {0xFE,   8},
	{0xFE,   8}, {0xFE,   8}, {0xFE,   8}, {0xFE,   8}, {0xFE,   8},
	{0xFE,   8}, {0xFE,   8}, {0xFE,   8}, {0xFE,   8}, {0xFE,   8},
	{0xFE,   8}, {0xFE,   8}, {0xFE,   8}, {0xFE,   8}, {0x7E,   8},
	{0x7E,   8}, {0x3E,   8}, {0x3E,   8}, {0x1E,   8}, {0x1E,   8},
	{0x0E,   8}, {0x0E,   8}, {0x06,   8}, {0x06,   8}, {0x02,   8},
	{0x02,   8}, {0x00,   8}, {0x00,   8}, {0xFE,  16}
};

static u8 h_attenuation[] = { 1, 2, 4, 8, 2, 4, 8, 16, 0};

static int calculate_h_scale_registers(struct saa7146_dev *dev,
	int in_x, int out_x, int flip_lr,
	u32* hps_ctrl, u32* hps_v_gain, u32* hps_h_prescale, u32* hps_h_scale)
{
	
	u32 dcgx = 0, xpsc = 0, xacm = 0, cxy = 0, cxuv = 0;
	
	u32 xim = 0, xp = 0, xsci =0;
	
	u32 pfuv = 0;

	
	u32 h_atten = 0, i = 0;

	if ( 0 == out_x ) {
		return -EINVAL;
	}

	
	*hps_ctrl &= ~MASK_29;

	if (in_x > out_x) {
		xpsc = in_x / out_x;
	}
	else {
		
		xpsc = 1;
	}

	if ( 0 != flip_lr ) {

		
		*hps_ctrl |= MASK_29;

		while (in_x / xpsc >= 384 )
			xpsc++;
	}
	else {
		while ( in_x / xpsc >= 768 )
			xpsc++;
	}

	
	if ( xpsc > 64 )
		xpsc = 64;

	
	xacm = 0;

	
	cxy = hps_h_coeff_tab[( (xpsc - 1) < 63 ? (xpsc - 1) : 63 )].hps_coeff;
	cxuv = cxy;

	

	
	if ( (in_x == out_x) && ( 1 == xpsc ) )
		xsci = 0x400;
	else
		xsci = ( (1024 * in_x) / (out_x * xpsc) ) + xpsc;

	
	xp = 0;

	
	if ( 0x400 == xsci )
		xim = 1;
	else
		xim = 0;

	if( 1 == xpsc ) {
		xacm = 1;
		dcgx = 0;
	} else {
		xacm = 0;
		h_atten = hps_h_coeff_tab[( (xpsc - 1) < 63 ? (xpsc - 1) : 63 )].weight_sum;

		for (i = 0; h_attenuation[i] != 0; i++) {
			if (h_attenuation[i] >= h_atten)
				break;
		}

		dcgx = i;
	}

	if ( xsci == 0x400)
		pfuv = 0x00;
	else if ( xsci < 0x600)
		pfuv = 0x01;
	else if ( xsci < 0x680)
		pfuv = 0x11;
	else if ( xsci < 0x700)
		pfuv = 0x22;
	else
		pfuv = 0x33;


	*hps_v_gain  &= MASK_W0|MASK_B2;
	*hps_v_gain  |= (pfuv << 24);

	*hps_h_scale	&= ~(MASK_W1 | 0xf000);
	*hps_h_scale	|= (xim << 31) | (xp << 24) | (xsci << 12);

	*hps_h_prescale	|= (dcgx << 27) | ((xpsc-1) << 18) | (xacm << 17) | (cxy << 8) | (cxuv << 0);

	return 0;
}

static struct {
	u16 hps_coeff;
	u16 weight_sum;
} hps_v_coeff_tab [] = {
 {0x0100,   2},  {0x0102,   4},  {0x0300,   4},  {0x0106,   8},  {0x0502,   8},
 {0x0708,   8},  {0x0F00,   8},  {0x011E,  16},  {0x110E,  16},  {0x1926,  16},
 {0x3906,  16},  {0x3D42,  16},  {0x7D02,  16},  {0x7F80,  16},  {0xFF00,  16},
 {0x01FE,  32},  {0x01FE,  32},  {0x817E,  32},  {0x817E,  32},  {0xC13E,  32},
 {0xC13E,  32},  {0xE11E,  32},  {0xE11E,  32},  {0xF10E,  32},  {0xF10E,  32},
 {0xF906,  32},  {0xF906,  32},  {0xFD02,  32},  {0xFD02,  32},  {0xFF00,  32},
 {0xFF00,  32},  {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},
 {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},
 {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},
 {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},  {0x01FE,  64},  {0x817E,  64},
 {0x817E,  64},  {0xC13E,  64},  {0xC13E,  64},  {0xE11E,  64},  {0xE11E,  64},
 {0xF10E,  64},  {0xF10E,  64},  {0xF906,  64},  {0xF906,  64},  {0xFD02,  64},
 {0xFD02,  64},  {0xFF00,  64},  {0xFF00,  64},  {0x01FE, 128}
};

static u16 v_attenuation[] = { 2, 4, 8, 16, 32, 64, 128, 256, 0};

static int calculate_v_scale_registers(struct saa7146_dev *dev, enum v4l2_field field,
	int in_y, int out_y, u32* hps_v_scale, u32* hps_v_gain)
{
	int lpi = 0;

	
	u32 yacm = 0, ysci = 0, yacl = 0, ypo = 0, ype = 0;
	
	u32 dcgy = 0, cya_cyb = 0;

	
	u32 v_atten = 0, i = 0;

	
	if ( in_y < out_y ) {
		return -EINVAL;
	}


	if (V4L2_FIELD_HAS_BOTH(field)) {
		if( 2*out_y >= in_y) {
			lpi = 1;
		}
	} else if (field == V4L2_FIELD_TOP
		|| field == V4L2_FIELD_ALTERNATE
		|| field == V4L2_FIELD_BOTTOM) {
		if( 4*out_y >= in_y ) {
			lpi = 1;
		}
		out_y *= 2;
	}
	if( 0 != lpi ) {

		yacm = 0;
		yacl = 0;
		cya_cyb = 0x00ff;

		
		if ( in_y > out_y )
			ysci = ((1024 * in_y) / (out_y + 1)) - 1024;
		else
			ysci = 0;

		dcgy = 0;

		
		ype = ysci / 16;
		ypo = ype + (ysci / 64);

	} else {
		yacm = 1;

		
		ysci = (((10 * 1024 * (in_y - out_y - 1)) / in_y) + 9) / 10;

		
		ypo = ype = ((ysci + 15) / 16);

		if ( ysci < 512) {
			yacl = 0;
		} else {
			yacl = ( ysci / (1024 - ysci) );
		}

		
		cya_cyb = hps_v_coeff_tab[ (yacl < 63 ? yacl : 63 ) ].hps_coeff;

		
		v_atten = hps_v_coeff_tab[ (yacl < 63 ? yacl : 63 ) ].weight_sum;

		for (i = 0; v_attenuation[i] != 0; i++) {
			if (v_attenuation[i] >= v_atten)
				break;
		}

		dcgy = i;
	}

	
	*hps_v_scale	|= (yacm << 31) | (ysci << 21) | (yacl << 15) | (ypo << 8 ) | (ype << 1);

	*hps_v_gain	&= ~(MASK_W0|MASK_B2);
	*hps_v_gain	|= (dcgy << 16) | (cya_cyb << 0);

	return 0;
}

static int sort_and_eliminate(u32* values, int* count)
{
	int low = 0, high = 0, top = 0, temp = 0;
	int cur = 0, next = 0;

	
	if( (0 > *count) || (NULL == values) ) {
		return -EINVAL;
	}

	
	for( top = *count; top > 0; top--) {
		for( low = 0, high = 1; high < top; low++, high++) {
			if( values[low] > values[high] ) {
				temp = values[low];
				values[low] = values[high];
				values[high] = temp;
			}
		}
	}

	
	for( cur = 0, next = 1; next < *count; next++) {
		if( values[cur] != values[next])
			values[++cur] = values[next];
	}

	*count = cur + 1;

	return 0;
}

static void calculate_clipping_registers_rect(struct saa7146_dev *dev, struct saa7146_fh *fh,
	struct saa7146_video_dma *vdma2, u32* clip_format, u32* arbtr_ctrl, enum v4l2_field field)
{
	struct saa7146_vv *vv = dev->vv_data;
	__le32 *clipping = vv->d_clipping.cpu_addr;

	int width = fh->ov.win.w.width;
	int height =  fh->ov.win.w.height;
	int clipcount = fh->ov.nclips;

	u32 line_list[32];
	u32 pixel_list[32];
	int numdwords = 0;

	int i = 0, j = 0;
	int cnt_line = 0, cnt_pixel = 0;

	int x[32], y[32], w[32], h[32];

	
	memset(&line_list[0],  0x00, sizeof(u32)*32);
	memset(&pixel_list[0], 0x00, sizeof(u32)*32);
	memset(clipping,  0x00, SAA7146_CLIPPING_MEM);

	
	for(i = 0; i < clipcount; i++) {
		int l = 0, r = 0, t = 0, b = 0;

		x[i] = fh->ov.clips[i].c.left;
		y[i] = fh->ov.clips[i].c.top;
		w[i] = fh->ov.clips[i].c.width;
		h[i] = fh->ov.clips[i].c.height;

		if( w[i] < 0) {
			x[i] += w[i]; w[i] = -w[i];
		}
		if( h[i] < 0) {
			y[i] += h[i]; h[i] = -h[i];
		}
		if( x[i] < 0) {
			w[i] += x[i]; x[i] = 0;
		}
		if( y[i] < 0) {
			h[i] += y[i]; y[i] = 0;
		}
		if( 0 != vv->vflip ) {
			y[i] = height - y[i] - h[i];
		}

		l = x[i];
		r = x[i]+w[i];
		t = y[i];
		b = y[i]+h[i];

		
		pixel_list[ 2*i   ] = min_t(int, l, width);
		pixel_list[(2*i)+1] = min_t(int, r, width);
		
		line_list[ 2*i   ] = min_t(int, t, height);
		line_list[(2*i)+1] = min_t(int, b, height);
	}

	
	cnt_line = cnt_pixel = 2*clipcount;
	sort_and_eliminate( &pixel_list[0], &cnt_pixel );
	sort_and_eliminate( &line_list[0], &cnt_line );

	
	numdwords = max_t(int, (cnt_line+1), (cnt_pixel+1))*2;
	numdwords = max_t(int, 4, numdwords);
	numdwords = min_t(int, 64, numdwords);

	
	for(i = 0; i < cnt_pixel; i++) {
		clipping[2*i] |= cpu_to_le32(pixel_list[i] << 16);
	}
	for(i = 0; i < cnt_line; i++) {
		clipping[(2*i)+1] |= cpu_to_le32(line_list[i] << 16);
	}

	
	for(j = 0; j < clipcount; j++) {

		for(i = 0; i < cnt_pixel; i++) {

			if( x[j] < 0)
				x[j] = 0;

			if( pixel_list[i] < (x[j] + w[j])) {

				if ( pixel_list[i] >= x[j] ) {
					clipping[2*i] |= cpu_to_le32(1 << j);
				}
			}
		}
		for(i = 0; i < cnt_line; i++) {

			if( y[j] < 0)
				y[j] = 0;

			if( line_list[i] < (y[j] + h[j]) ) {

				if( line_list[i] >= y[j] ) {
					clipping[(2*i)+1] |= cpu_to_le32(1 << j);
				}
			}
		}
	}

	
	*arbtr_ctrl &= 0xffff00ff;
	*arbtr_ctrl |= 0x00001c00;

	vdma2->base_even	= vv->d_clipping.dma_handle;
	vdma2->base_odd		= vv->d_clipping.dma_handle;
	vdma2->prot_addr	= vv->d_clipping.dma_handle+((sizeof(u32))*(numdwords));
	vdma2->base_page	= 0x04;
	vdma2->pitch		= 0x00;
	vdma2->num_line_byte	= (0 << 16 | (sizeof(u32))*(numdwords-1) );

	
	*clip_format &= 0xfffffff7;
	if (V4L2_FIELD_HAS_BOTH(field)) {
		*clip_format |= 0x00000008;
	} else {
		*clip_format |= 0x00000000;
	}
}

static void saa7146_disable_clipping(struct saa7146_dev *dev)
{
	u32 clip_format	= saa7146_read(dev, CLIP_FORMAT_CTRL);

	
	clip_format &= MASK_W1;

	
	saa7146_write(dev, CLIP_FORMAT_CTRL,clip_format);
	saa7146_write(dev, MC2, (MASK_05 | MASK_21));

	
	saa7146_write(dev, MC1, MASK_21);
}

static void saa7146_set_clipping_rect(struct saa7146_fh *fh)
{
	struct saa7146_dev *dev = fh->dev;
	enum v4l2_field field = fh->ov.win.field;
	struct	saa7146_video_dma vdma2;
	u32 clip_format;
	u32 arbtr_ctrl;

	
	if( fh->ov.nclips == 0 ) {
		saa7146_disable_clipping(dev);
		return;
	}

	clip_format = saa7146_read(dev, CLIP_FORMAT_CTRL);
	arbtr_ctrl = saa7146_read(dev, PCI_BT_V1);

	calculate_clipping_registers_rect(dev, fh, &vdma2, &clip_format, &arbtr_ctrl, field);

	
	clip_format &= 0xffff0008;
	clip_format |= (SAA7146_CLIPPING_RECT << 4);

	
	saa7146_write(dev, BASE_EVEN2,		vdma2.base_even);
	saa7146_write(dev, BASE_ODD2,		vdma2.base_odd);
	saa7146_write(dev, PROT_ADDR2,		vdma2.prot_addr);
	saa7146_write(dev, BASE_PAGE2,		vdma2.base_page);
	saa7146_write(dev, PITCH2,		vdma2.pitch);
	saa7146_write(dev, NUM_LINE_BYTE2,	vdma2.num_line_byte);

	
	saa7146_write(dev, CLIP_FORMAT_CTRL,clip_format);
	saa7146_write(dev, PCI_BT_V1, arbtr_ctrl);

	
	saa7146_write(dev, MC2, (MASK_05 | MASK_21 | MASK_03 | MASK_19));
	saa7146_write(dev, MC1, (MASK_05 | MASK_21));
}

static void saa7146_set_window(struct saa7146_dev *dev, int width, int height, enum v4l2_field field)
{
	struct saa7146_vv *vv = dev->vv_data;

	int source = vv->current_hps_source;
	int sync = vv->current_hps_sync;

	u32 hps_v_scale = 0, hps_v_gain  = 0, hps_ctrl = 0, hps_h_prescale = 0, hps_h_scale = 0;

	
	hps_v_scale = 0; 
	hps_v_gain  = 0; 
	calculate_v_scale_registers(dev, field, vv->standard->v_field*2, height, &hps_v_scale, &hps_v_gain);

	
	hps_ctrl	= 0;
	hps_h_prescale	= 0; 
	hps_h_scale	= 0;
	calculate_h_scale_registers(dev, vv->standard->h_pixels, width, vv->hflip, &hps_ctrl, &hps_v_gain, &hps_h_prescale, &hps_h_scale);

	
	calculate_hxo_and_hyo(vv, &hps_h_scale, &hps_ctrl);
	calculate_hps_source_and_sync(dev, source, sync, &hps_ctrl);

	
	saa7146_write(dev, HPS_V_SCALE,	hps_v_scale);
	saa7146_write(dev, HPS_V_GAIN,	hps_v_gain);
	saa7146_write(dev, HPS_CTRL,	hps_ctrl);
	saa7146_write(dev, HPS_H_PRESCALE,hps_h_prescale);
	saa7146_write(dev, HPS_H_SCALE,	hps_h_scale);

	
	saa7146_write(dev, MC2, (MASK_05 | MASK_06 | MASK_21 | MASK_22) );
}

static void saa7146_set_position(struct saa7146_dev *dev, int w_x, int w_y, int w_height, enum v4l2_field field, u32 pixelformat)
{
	struct saa7146_vv *vv = dev->vv_data;
	struct saa7146_format *sfmt = saa7146_format_by_fourcc(dev, pixelformat);

	int b_depth = vv->ov_fmt->depth;
	int b_bpl = vv->ov_fb.fmt.bytesperline;
	u32 base = (u32)(unsigned long)vv->ov_fb.base;

	struct	saa7146_video_dma vdma1;

	
	vdma1.pitch	= 2*b_bpl;
	if ( 0 == vv->vflip ) {
		vdma1.base_even = base + (w_y * (vdma1.pitch/2)) + (w_x * (b_depth / 8));
		vdma1.base_odd  = vdma1.base_even + (vdma1.pitch / 2);
		vdma1.prot_addr = vdma1.base_even + (w_height * (vdma1.pitch / 2));
	}
	else {
		vdma1.base_even = base + ((w_y+w_height) * (vdma1.pitch/2)) + (w_x * (b_depth / 8));
		vdma1.base_odd  = vdma1.base_even - (vdma1.pitch / 2);
		vdma1.prot_addr = vdma1.base_odd - (w_height * (vdma1.pitch / 2));
	}

	if (V4L2_FIELD_HAS_BOTH(field)) {
	} else if (field == V4L2_FIELD_ALTERNATE) {
		
		vdma1.base_odd = vdma1.prot_addr;
		vdma1.pitch /= 2;
	} else if (field == V4L2_FIELD_TOP) {
		vdma1.base_odd = vdma1.prot_addr;
		vdma1.pitch /= 2;
	} else if (field == V4L2_FIELD_BOTTOM) {
		vdma1.base_odd = vdma1.base_even;
		vdma1.base_even = vdma1.prot_addr;
		vdma1.pitch /= 2;
	}

	if ( 0 != vv->vflip ) {
		vdma1.pitch *= -1;
	}

	vdma1.base_page = sfmt->swap;
	vdma1.num_line_byte = (vv->standard->v_field<<16)+vv->standard->h_pixels;

	saa7146_write_out_dma(dev, 1, &vdma1);
}

static void saa7146_set_output_format(struct saa7146_dev *dev, unsigned long palette)
{
	u32 clip_format = saa7146_read(dev, CLIP_FORMAT_CTRL);

	
	calculate_output_format_register(dev,palette,&clip_format);

	
	saa7146_write(dev, CLIP_FORMAT_CTRL, clip_format);
	saa7146_write(dev, MC2, (MASK_05 | MASK_21));
}

void saa7146_set_hps_source_and_sync(struct saa7146_dev *dev, int source, int sync)
{
	struct saa7146_vv *vv = dev->vv_data;
	u32 hps_ctrl = 0;

	
	hps_ctrl = saa7146_read(dev, HPS_CTRL);

	hps_ctrl &= ~( MASK_31 | MASK_30 | MASK_28 );
	hps_ctrl |= (source << 30) | (sync << 28);

	
	saa7146_write(dev, HPS_CTRL, hps_ctrl);
	saa7146_write(dev, MC2, (MASK_05 | MASK_21));

	vv->current_hps_source = source;
	vv->current_hps_sync = sync;
}
EXPORT_SYMBOL_GPL(saa7146_set_hps_source_and_sync);

int saa7146_enable_overlay(struct saa7146_fh *fh)
{
	struct saa7146_dev *dev = fh->dev;
	struct saa7146_vv *vv = dev->vv_data;

	saa7146_set_window(dev, fh->ov.win.w.width, fh->ov.win.w.height, fh->ov.win.field);
	saa7146_set_position(dev, fh->ov.win.w.left, fh->ov.win.w.top, fh->ov.win.w.height, fh->ov.win.field, vv->ov_fmt->pixelformat);
	saa7146_set_output_format(dev, vv->ov_fmt->trans);
	saa7146_set_clipping_rect(fh);

	
	saa7146_write(dev, MC1, (MASK_06 | MASK_22));
	return 0;
}

void saa7146_disable_overlay(struct saa7146_fh *fh)
{
	struct saa7146_dev *dev = fh->dev;

	
	saa7146_disable_clipping(dev);
	saa7146_write(dev, MC1, MASK_22);
}

void saa7146_write_out_dma(struct saa7146_dev* dev, int which, struct saa7146_video_dma* vdma)
{
	int where = 0;

	if( which < 1 || which > 3) {
		return;
	}

	
	where  = (which-1)*0x18;

	saa7146_write(dev, where,	vdma->base_odd);
	saa7146_write(dev, where+0x04,	vdma->base_even);
	saa7146_write(dev, where+0x08,	vdma->prot_addr);
	saa7146_write(dev, where+0x0c,	vdma->pitch);
	saa7146_write(dev, where+0x10,	vdma->base_page);
	saa7146_write(dev, where+0x14,	vdma->num_line_byte);

	
	saa7146_write(dev, MC2, (MASK_02<<(which-1))|(MASK_18<<(which-1)));
}

static int calculate_video_dma_grab_packed(struct saa7146_dev* dev, struct saa7146_buf *buf)
{
	struct saa7146_vv *vv = dev->vv_data;
	struct saa7146_video_dma vdma1;

	struct saa7146_format *sfmt = saa7146_format_by_fourcc(dev,buf->fmt->pixelformat);

	int width = buf->fmt->width;
	int height = buf->fmt->height;
	int bytesperline = buf->fmt->bytesperline;
	enum v4l2_field field = buf->fmt->field;

	int depth = sfmt->depth;

	DEB_CAP("[size=%dx%d,fields=%s]\n",
		width, height, v4l2_field_names[field]);

	if( bytesperline != 0) {
		vdma1.pitch = bytesperline*2;
	} else {
		vdma1.pitch = (width*depth*2)/8;
	}
	vdma1.num_line_byte	= ((vv->standard->v_field<<16) + vv->standard->h_pixels);
	vdma1.base_page		= buf->pt[0].dma | ME1 | sfmt->swap;

	if( 0 != vv->vflip ) {
		vdma1.prot_addr	= buf->pt[0].offset;
		vdma1.base_even	= buf->pt[0].offset+(vdma1.pitch/2)*height;
		vdma1.base_odd	= vdma1.base_even - (vdma1.pitch/2);
	} else {
		vdma1.base_even	= buf->pt[0].offset;
		vdma1.base_odd	= vdma1.base_even + (vdma1.pitch/2);
		vdma1.prot_addr	= buf->pt[0].offset+(vdma1.pitch/2)*height;
	}

	if (V4L2_FIELD_HAS_BOTH(field)) {
	} else if (field == V4L2_FIELD_ALTERNATE) {
		
		if ( vv->last_field == V4L2_FIELD_TOP ) {
			vdma1.base_odd	= vdma1.prot_addr;
			vdma1.pitch /= 2;
		} else if ( vv->last_field == V4L2_FIELD_BOTTOM ) {
			vdma1.base_odd	= vdma1.base_even;
			vdma1.base_even = vdma1.prot_addr;
			vdma1.pitch /= 2;
		}
	} else if (field == V4L2_FIELD_TOP) {
		vdma1.base_odd	= vdma1.prot_addr;
		vdma1.pitch /= 2;
	} else if (field == V4L2_FIELD_BOTTOM) {
		vdma1.base_odd	= vdma1.base_even;
		vdma1.base_even = vdma1.prot_addr;
		vdma1.pitch /= 2;
	}

	if( 0 != vv->vflip ) {
		vdma1.pitch *= -1;
	}

	saa7146_write_out_dma(dev, 1, &vdma1);
	return 0;
}

static int calc_planar_422(struct saa7146_vv *vv, struct saa7146_buf *buf, struct saa7146_video_dma *vdma2, struct saa7146_video_dma *vdma3)
{
	int height = buf->fmt->height;
	int width = buf->fmt->width;

	vdma2->pitch	= width;
	vdma3->pitch	= width;

	

	if( 0 != vv->vflip ) {
		vdma2->prot_addr	= buf->pt[1].offset;
		vdma2->base_even	= ((vdma2->pitch/2)*height)+buf->pt[1].offset;
		vdma2->base_odd		= vdma2->base_even - (vdma2->pitch/2);

		vdma3->prot_addr	= buf->pt[2].offset;
		vdma3->base_even	= ((vdma3->pitch/2)*height)+buf->pt[2].offset;
		vdma3->base_odd		= vdma3->base_even - (vdma3->pitch/2);
	} else {
		vdma3->base_even	= buf->pt[2].offset;
		vdma3->base_odd		= vdma3->base_even + (vdma3->pitch/2);
		vdma3->prot_addr	= (vdma3->pitch/2)*height+buf->pt[2].offset;

		vdma2->base_even	= buf->pt[1].offset;
		vdma2->base_odd		= vdma2->base_even + (vdma2->pitch/2);
		vdma2->prot_addr	= (vdma2->pitch/2)*height+buf->pt[1].offset;
	}

	return 0;
}

static int calc_planar_420(struct saa7146_vv *vv, struct saa7146_buf *buf, struct saa7146_video_dma *vdma2, struct saa7146_video_dma *vdma3)
{
	int height = buf->fmt->height;
	int width = buf->fmt->width;

	vdma2->pitch	= width/2;
	vdma3->pitch	= width/2;

	if( 0 != vv->vflip ) {
		vdma2->prot_addr	= buf->pt[2].offset;
		vdma2->base_even	= ((vdma2->pitch/2)*height)+buf->pt[2].offset;
		vdma2->base_odd		= vdma2->base_even - (vdma2->pitch/2);

		vdma3->prot_addr	= buf->pt[1].offset;
		vdma3->base_even	= ((vdma3->pitch/2)*height)+buf->pt[1].offset;
		vdma3->base_odd		= vdma3->base_even - (vdma3->pitch/2);

	} else {
		vdma3->base_even	= buf->pt[2].offset;
		vdma3->base_odd		= vdma3->base_even + (vdma3->pitch);
		vdma3->prot_addr	= (vdma3->pitch/2)*height+buf->pt[2].offset;

		vdma2->base_even	= buf->pt[1].offset;
		vdma2->base_odd		= vdma2->base_even + (vdma2->pitch);
		vdma2->prot_addr	= (vdma2->pitch/2)*height+buf->pt[1].offset;
	}
	return 0;
}

static int calculate_video_dma_grab_planar(struct saa7146_dev* dev, struct saa7146_buf *buf)
{
	struct saa7146_vv *vv = dev->vv_data;
	struct saa7146_video_dma vdma1;
	struct saa7146_video_dma vdma2;
	struct saa7146_video_dma vdma3;

	struct saa7146_format *sfmt = saa7146_format_by_fourcc(dev,buf->fmt->pixelformat);

	int width = buf->fmt->width;
	int height = buf->fmt->height;
	enum v4l2_field field = buf->fmt->field;

	BUG_ON(0 == buf->pt[0].dma);
	BUG_ON(0 == buf->pt[1].dma);
	BUG_ON(0 == buf->pt[2].dma);

	DEB_CAP("[size=%dx%d,fields=%s]\n",
		width, height, v4l2_field_names[field]);

	


	vdma1.pitch		= width*2;
	vdma1.num_line_byte	= ((vv->standard->v_field<<16) + vv->standard->h_pixels);
	vdma1.base_page		= buf->pt[0].dma | ME1;

	if( 0 != vv->vflip ) {
		vdma1.prot_addr	= buf->pt[0].offset;
		vdma1.base_even	= ((vdma1.pitch/2)*height)+buf->pt[0].offset;
		vdma1.base_odd	= vdma1.base_even - (vdma1.pitch/2);
	} else {
		vdma1.base_even	= buf->pt[0].offset;
		vdma1.base_odd	= vdma1.base_even + (vdma1.pitch/2);
		vdma1.prot_addr	= (vdma1.pitch/2)*height+buf->pt[0].offset;
	}

	vdma2.num_line_byte	= 0; 
	vdma2.base_page		= buf->pt[1].dma | ME1;

	vdma3.num_line_byte	= 0; 
	vdma3.base_page		= buf->pt[2].dma | ME1;

	switch( sfmt->depth ) {
		case 12: {
			calc_planar_420(vv,buf,&vdma2,&vdma3);
			break;
		}
		case 16: {
			calc_planar_422(vv,buf,&vdma2,&vdma3);
			break;
		}
		default: {
			return -1;
		}
	}

	if (V4L2_FIELD_HAS_BOTH(field)) {
	} else if (field == V4L2_FIELD_ALTERNATE) {
		
		vdma1.base_odd	= vdma1.prot_addr;
		vdma1.pitch /= 2;
		vdma2.base_odd	= vdma2.prot_addr;
		vdma2.pitch /= 2;
		vdma3.base_odd	= vdma3.prot_addr;
		vdma3.pitch /= 2;
	} else if (field == V4L2_FIELD_TOP) {
		vdma1.base_odd	= vdma1.prot_addr;
		vdma1.pitch /= 2;
		vdma2.base_odd	= vdma2.prot_addr;
		vdma2.pitch /= 2;
		vdma3.base_odd	= vdma3.prot_addr;
		vdma3.pitch /= 2;
	} else if (field == V4L2_FIELD_BOTTOM) {
		vdma1.base_odd	= vdma1.base_even;
		vdma1.base_even = vdma1.prot_addr;
		vdma1.pitch /= 2;
		vdma2.base_odd	= vdma2.base_even;
		vdma2.base_even = vdma2.prot_addr;
		vdma2.pitch /= 2;
		vdma3.base_odd	= vdma3.base_even;
		vdma3.base_even = vdma3.prot_addr;
		vdma3.pitch /= 2;
	}

	if( 0 != vv->vflip ) {
		vdma1.pitch *= -1;
		vdma2.pitch *= -1;
		vdma3.pitch *= -1;
	}

	saa7146_write_out_dma(dev, 1, &vdma1);
	if( (sfmt->flags & FORMAT_BYTE_SWAP) != 0 ) {
		saa7146_write_out_dma(dev, 3, &vdma2);
		saa7146_write_out_dma(dev, 2, &vdma3);
	} else {
		saa7146_write_out_dma(dev, 2, &vdma2);
		saa7146_write_out_dma(dev, 3, &vdma3);
	}
	return 0;
}

static void program_capture_engine(struct saa7146_dev *dev, int planar)
{
	struct saa7146_vv *vv = dev->vv_data;
	int count = 0;

	unsigned long e_wait = vv->current_hps_sync == SAA7146_HPS_SYNC_PORT_A ? CMD_E_FID_A : CMD_E_FID_B;
	unsigned long o_wait = vv->current_hps_sync == SAA7146_HPS_SYNC_PORT_A ? CMD_O_FID_A : CMD_O_FID_B;

	
	WRITE_RPS0(CMD_PAUSE | CMD_OAN | CMD_SIG0 | o_wait);
	WRITE_RPS0(CMD_PAUSE | CMD_OAN | CMD_SIG0 | e_wait);

	
	WRITE_RPS0(CMD_WR_REG | (1 << 8) | (MC2/4));
	WRITE_RPS0(MASK_27 | MASK_11);

	
	WRITE_RPS0(CMD_WR_REG_MASK | (MC1/4));
	WRITE_RPS0(MASK_06 | MASK_22);			
	WRITE_RPS0(MASK_06 | MASK_22);			
	if( 0 != planar ) {
		
		WRITE_RPS0(CMD_WR_REG_MASK | (MC1/4));
		WRITE_RPS0(MASK_05 | MASK_21);			
		WRITE_RPS0(MASK_05 | MASK_21);			

		
		WRITE_RPS0(CMD_WR_REG_MASK | (MC1/4));
		WRITE_RPS0(MASK_04 | MASK_20);			
		WRITE_RPS0(MASK_04 | MASK_20);			
	}

	
	if ( vv->last_field == V4L2_FIELD_INTERLACED ) {
		WRITE_RPS0(CMD_PAUSE | o_wait);
		WRITE_RPS0(CMD_PAUSE | e_wait);
	} else if ( vv->last_field == V4L2_FIELD_TOP ) {
		WRITE_RPS0(CMD_PAUSE | (vv->current_hps_sync == SAA7146_HPS_SYNC_PORT_A ? MASK_10 : MASK_09));
		WRITE_RPS0(CMD_PAUSE | o_wait);
	} else if ( vv->last_field == V4L2_FIELD_BOTTOM ) {
		WRITE_RPS0(CMD_PAUSE | (vv->current_hps_sync == SAA7146_HPS_SYNC_PORT_A ? MASK_10 : MASK_09));
		WRITE_RPS0(CMD_PAUSE | e_wait);
	}

	
	WRITE_RPS0(CMD_WR_REG_MASK | (MC1/4));
	WRITE_RPS0(MASK_22 | MASK_06);			
	WRITE_RPS0(MASK_22);				
	if( 0 != planar ) {
		
		WRITE_RPS0(CMD_WR_REG_MASK | (MC1/4));
		WRITE_RPS0(MASK_05 | MASK_21);			
		WRITE_RPS0(MASK_21);				

		
		WRITE_RPS0(CMD_WR_REG_MASK | (MC1/4));
		WRITE_RPS0(MASK_04 | MASK_20);			
		WRITE_RPS0(MASK_20);				
	}

	
	WRITE_RPS0(CMD_INTERRUPT);

	
	WRITE_RPS0(CMD_STOP);
}

void saa7146_set_capture(struct saa7146_dev *dev, struct saa7146_buf *buf, struct saa7146_buf *next)
{
	struct saa7146_format *sfmt = saa7146_format_by_fourcc(dev,buf->fmt->pixelformat);
	struct saa7146_vv *vv = dev->vv_data;
	u32 vdma1_prot_addr;

	DEB_CAP("buf:%p, next:%p\n", buf, next);

	vdma1_prot_addr = saa7146_read(dev, PROT_ADDR1);
	if( 0 == vdma1_prot_addr ) {
		
		DEB_CAP("forcing sync to new frame\n");
		saa7146_write(dev, MC2, MASK_27 );
	}

	saa7146_set_window(dev, buf->fmt->width, buf->fmt->height, buf->fmt->field);
	saa7146_set_output_format(dev, sfmt->trans);
	saa7146_disable_clipping(dev);

	if ( vv->last_field == V4L2_FIELD_INTERLACED ) {
	} else if ( vv->last_field == V4L2_FIELD_TOP ) {
		vv->last_field = V4L2_FIELD_BOTTOM;
	} else if ( vv->last_field == V4L2_FIELD_BOTTOM ) {
		vv->last_field = V4L2_FIELD_TOP;
	}

	if( 0 != IS_PLANAR(sfmt->trans)) {
		calculate_video_dma_grab_planar(dev, buf);
		program_capture_engine(dev,1);
	} else {
		calculate_video_dma_grab_packed(dev, buf);
		program_capture_engine(dev,0);
	}


	
	saa7146_write(dev, RPS_ADDR0, dev->d_rps0.dma_handle);

	
	saa7146_write(dev, MC1, (MASK_12 | MASK_28));
}
