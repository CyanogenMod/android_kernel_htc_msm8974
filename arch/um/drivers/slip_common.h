#ifndef __UM_SLIP_COMMON_H
#define __UM_SLIP_COMMON_H

#define BUF_SIZE 1500
#define ENC_BUF_SIZE (2 * BUF_SIZE + 2)

#define SLIP_END             0300	
#define SLIP_ESC             0333	
#define SLIP_ESC_END         0334	
#define SLIP_ESC_ESC         0335	

static inline int slip_unesc(unsigned char c, unsigned char *buf, int *pos,
                             int *esc)
{
	int ret;

	switch(c){
	case SLIP_END:
		*esc = 0;
		ret=*pos;
		*pos=0;
		return(ret);
	case SLIP_ESC:
		*esc = 1;
		return(0);
	case SLIP_ESC_ESC:
		if(*esc){
			*esc = 0;
			c = SLIP_ESC;
		}
		break;
	case SLIP_ESC_END:
		if(*esc){
			*esc = 0;
			c = SLIP_END;
		}
		break;
	}
	buf[(*pos)++] = c;
	return(0);
}

static inline int slip_esc(unsigned char *s, unsigned char *d, int len)
{
	unsigned char *ptr = d;
	unsigned char c;


	*ptr++ = SLIP_END;


	while (len-- > 0) {
		switch(c = *s++) {
		case SLIP_END:
			*ptr++ = SLIP_ESC;
			*ptr++ = SLIP_ESC_END;
			break;
		case SLIP_ESC:
			*ptr++ = SLIP_ESC;
			*ptr++ = SLIP_ESC_ESC;
			break;
		default:
			*ptr++ = c;
			break;
		}
	}
	*ptr++ = SLIP_END;
	return (ptr - d);
}

struct slip_proto {
	unsigned char ibuf[ENC_BUF_SIZE];
	unsigned char obuf[ENC_BUF_SIZE];
	int more; 
	int pos;
	int esc;
};

static inline void slip_proto_init(struct slip_proto * slip)
{
	memset(slip->ibuf, 0, sizeof(slip->ibuf));
	memset(slip->obuf, 0, sizeof(slip->obuf));
	slip->more = 0;
	slip->pos = 0;
	slip->esc = 0;
}

extern int slip_proto_read(int fd, void *buf, int len,
			   struct slip_proto *slip);
extern int slip_proto_write(int fd, void *buf, int len,
			    struct slip_proto *slip);

#endif
