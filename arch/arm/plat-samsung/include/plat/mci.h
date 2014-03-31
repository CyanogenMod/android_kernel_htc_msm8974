#ifndef _ARCH_MCI_H
#define _ARCH_MCI_H

struct s3c24xx_mci_pdata {
	unsigned int	no_wprotect:1;
	unsigned int	no_detect:1;
	unsigned int	wprotect_invert:1;
	unsigned int	detect_invert:1;	
	unsigned int	use_dma:1;

	unsigned int	gpio_detect;
	unsigned int	gpio_wprotect;
	unsigned long	ocr_avail;
	void		(*set_power)(unsigned char power_mode,
				     unsigned short vdd);
};

extern void s3c24xx_mci_set_platdata(struct s3c24xx_mci_pdata *pdata);

#endif 
