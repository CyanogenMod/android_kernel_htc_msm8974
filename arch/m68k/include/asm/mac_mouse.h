#ifndef _ASM_MAC_MOUSE_H
#define _ASM_MAC_MOUSE_H


struct mouse_status {
	char		buttons;
	short		dx;
	short		dy;
	int		ready;
	int		active;
	wait_queue_head_t wait;
	struct fasync_struct *fasyncptr;
};

#endif
