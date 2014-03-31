
#define PALMCONNECT_VID		0x0830
#define PALMCONNECT_PID		0x0080

#define KLSI_VID		0x05e9
#define KLSI_KL5KUSB105D_PID	0x00c0





enum {
	kl5kusb105a_sio_b115200 = 0,
	kl5kusb105a_sio_b57600  = 1,
	kl5kusb105a_sio_b38400  = 2,
	kl5kusb105a_sio_b19200  = 4,
	kl5kusb105a_sio_b14400  = 5,
	kl5kusb105a_sio_b9600   = 6,
	kl5kusb105a_sio_b4800   = 8,	
	kl5kusb105a_sio_b2400   = 9,	
	kl5kusb105a_sio_b1200   = 0xa,	
	kl5kusb105a_sio_b600    = 0xb	
};

#define kl5kusb105a_dtb_7   7
#define kl5kusb105a_dtb_8   8



#define KL5KUSB105A_SIO_SET_DATA  1
#define KL5KUSB105A_SIO_POLL      2
#define KL5KUSB105A_SIO_CONFIGURE      3
#define KL5KUSB105A_SIO_CONFIGURE_READ_ON      3
#define KL5KUSB105A_SIO_CONFIGURE_READ_OFF     2

#define KL5KUSB105A_DSR			((1<<4) | (1<<5))
#define KL5KUSB105A_CTS			((1<<5) | (1<<4))

#define KL5KUSB105A_WANTS_TO_SEND	0x30
#if 0
#define KL5KUSB105A_DTR			
#define KL5KUSB105A_CTS			
#define KL5KUSB105A_CD			
#define KL5KUSB105A_DSR			
#define KL5KUSB105A_RxD			

#define KL5KUSB105A_LE
#define KL5KUSB105A_RTS
#define KL5KUSB105A_ST
#define KL5KUSB105A_SR
#define KL5KUSB105A_RI			
#endif
