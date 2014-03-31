#ifndef _LINUX_SOUND_H
#define _LINUX_SOUND_H


#include <linux/fs.h>

#define SND_DEV_CTL		0	
#define SND_DEV_SEQ		1	
#define SND_DEV_MIDIN		2	
#define SND_DEV_DSP		3	
#define SND_DEV_AUDIO		4	
#define SND_DEV_DSP16		5	
	
#define SND_DEV_UNUSED		6
#define SND_DEV_AWFM		7	
#define SND_DEV_SEQ2		8	
	
#define SND_DEV_SYNTH		9	
#define SND_DEV_DMFM		10	
#define SND_DEV_UNKNOWN11	11
#define SND_DEV_ADSP		12	
#define SND_DEV_AMIDI		13	
#define SND_DEV_ADMMIDI		14	

#ifdef __KERNEL__
 
struct device;
extern int register_sound_special(const struct file_operations *fops, int unit);
extern int register_sound_special_device(const struct file_operations *fops, int unit, struct device *dev);
extern int register_sound_mixer(const struct file_operations *fops, int dev);
extern int register_sound_midi(const struct file_operations *fops, int dev);
extern int register_sound_dsp(const struct file_operations *fops, int dev);

extern void unregister_sound_special(int unit);
extern void unregister_sound_mixer(int unit);
extern void unregister_sound_midi(int unit);
extern void unregister_sound_dsp(int unit);
#endif 

#endif 
