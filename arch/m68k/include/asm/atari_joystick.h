#ifndef _LINUX_ATARI_JOYSTICK_H
#define _LINUX_ATARI_JOYSTICK_H


void atari_joystick_interrupt(char*);
int atari_joystick_init(void);
extern int atari_mouse_buttons;

struct joystick_status {
	char		fire;
	char		dir;
	int		ready;
	int		active;
	wait_queue_head_t wait;
};

#endif
