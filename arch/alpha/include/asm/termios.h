#ifndef _ALPHA_TERMIOS_H
#define _ALPHA_TERMIOS_H

#include <asm/ioctls.h>
#include <asm/termbits.h>

struct sgttyb {
	char	sg_ispeed;
	char	sg_ospeed;
	char	sg_erase;
	char	sg_kill;
	short	sg_flags;
};

struct tchars {
	char	t_intrc;
	char	t_quitc;
	char	t_startc;
	char	t_stopc;
	char	t_eofc;
	char	t_brkc;
};

struct ltchars {
	char	t_suspc;
	char	t_dsuspc;
	char	t_rprntc;
	char	t_flushc;
	char	t_werasc;
	char	t_lnextc;
};

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

#define NCC 8
struct termio {
	unsigned short c_iflag;		
	unsigned short c_oflag;		
	unsigned short c_cflag;		
	unsigned short c_lflag;		
	unsigned char c_line;		
	unsigned char c_cc[NCC];	
};

#define _VINTR	0
#define _VQUIT	1
#define _VERASE	2
#define _VKILL	3
#define _VEOF	4
#define _VMIN	4
#define _VEOL	5
#define _VTIME	5
#define _VEOL2	6
#define _VSWTC	7

#ifdef __KERNEL__
#define INIT_C_CC "\004\000\000\177\027\025\022\000\003\034\032\000\021\023\026\025\001\000"


#define user_termio_to_kernel_termios(a_termios, u_termio)			\
({										\
	struct ktermios *k_termios = (a_termios);				\
	struct termio k_termio;							\
	int canon, ret;								\
										\
	ret = copy_from_user(&k_termio, u_termio, sizeof(k_termio));		\
	if (!ret) {								\
						\
		*(unsigned short *)&k_termios->c_iflag = k_termio.c_iflag;	\
		*(unsigned short *)&k_termios->c_oflag = k_termio.c_oflag;	\
		*(unsigned short *)&k_termios->c_cflag = k_termio.c_cflag;	\
		*(unsigned short *)&k_termios->c_lflag = k_termio.c_lflag;	\
		canon = k_termio.c_lflag & ICANON;				\
										\
		k_termios->c_cc[VINTR]  = k_termio.c_cc[_VINTR];		\
		k_termios->c_cc[VQUIT]  = k_termio.c_cc[_VQUIT];		\
		k_termios->c_cc[VERASE] = k_termio.c_cc[_VERASE];		\
		k_termios->c_cc[VKILL]  = k_termio.c_cc[_VKILL];		\
		k_termios->c_cc[VEOL2]  = k_termio.c_cc[_VEOL2];		\
		k_termios->c_cc[VSWTC]  = k_termio.c_cc[_VSWTC];		\
		k_termios->c_cc[canon ? VEOF : VMIN]  = k_termio.c_cc[_VEOF];	\
		k_termios->c_cc[canon ? VEOL : VTIME] = k_termio.c_cc[_VEOL];	\
	}									\
	ret;									\
})

#define kernel_termios_to_user_termio(u_termio, a_termios)		\
({									\
	struct ktermios *k_termios = (a_termios);			\
	struct termio k_termio;						\
	int canon;							\
									\
	k_termio.c_iflag = k_termios->c_iflag;				\
	k_termio.c_oflag = k_termios->c_oflag;				\
	k_termio.c_cflag = k_termios->c_cflag;				\
	canon = (k_termio.c_lflag = k_termios->c_lflag) & ICANON;	\
									\
	k_termio.c_line = k_termios->c_line;				\
	k_termio.c_cc[_VINTR]  = k_termios->c_cc[VINTR];		\
	k_termio.c_cc[_VQUIT]  = k_termios->c_cc[VQUIT];		\
	k_termio.c_cc[_VERASE] = k_termios->c_cc[VERASE];		\
	k_termio.c_cc[_VKILL]  = k_termios->c_cc[VKILL];		\
	k_termio.c_cc[_VEOF]   = k_termios->c_cc[canon ? VEOF : VMIN];	\
	k_termio.c_cc[_VEOL]   = k_termios->c_cc[canon ? VEOL : VTIME];	\
	k_termio.c_cc[_VEOL2]  = k_termios->c_cc[VEOL2];		\
	k_termio.c_cc[_VSWTC]  = k_termios->c_cc[VSWTC];		\
									\
	copy_to_user(u_termio, &k_termio, sizeof(k_termio));		\
})

#define user_termios_to_kernel_termios(k, u) \
	copy_from_user(k, u, sizeof(struct termios))

#define kernel_termios_to_user_termios(u, k) \
	copy_to_user(u, k, sizeof(struct termios))

#endif	

#endif	
