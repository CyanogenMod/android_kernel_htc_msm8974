#ifndef __mISDNdsp_H__
#define __mISDNdsp_H__

struct mISDN_dsp_element_arg {
	char	*name;
	char	*def;
	char	*desc;
};

struct mISDN_dsp_element {
	char	*name;
	void	*(*new)(const char *arg);
	void	(*free)(void *p);
	void	(*process_tx)(void *p, unsigned char *data, int len);
	void	(*process_rx)(void *p, unsigned char *data, int len,
			unsigned int txlen);
	int	num_args;
	struct mISDN_dsp_element_arg
		*args;
};

extern int  mISDN_dsp_element_register(struct mISDN_dsp_element *elem);
extern void mISDN_dsp_element_unregister(struct mISDN_dsp_element *elem);

struct dsp_features {
	int	hfc_id; 
	int	hfc_dtmf; 
	int	hfc_conf; 
	int	hfc_loops; 
	int	hfc_echocanhw; 
	int	pcm_id; 
	int	pcm_slots; 
	int	pcm_banks; 
	int	unclocked; 
	int	unordered; 
};

#endif

