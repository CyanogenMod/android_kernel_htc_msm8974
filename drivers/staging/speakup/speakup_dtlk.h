
#define SYNTH_IO_EXTENT	0x02
#define SYNTH_CLEAR	0x18		
	
#define TTS_READABLE	0x80	
#define TTS_SPEAKING	0x40	
#define TTS_SPEAKING2	0x20	
#define TTS_WRITABLE	0x10	
#define TTS_ALMOST_FULL	0x08	
#define TTS_ALMOST_EMPTY 0x04	

				
struct synth_settings {
	u_short serial_number;	
	u_char rom_version[24]; 
	u_char mode;		
	u_char punc_level;	
	u_char formant_freq;	
	u_char pitch;		
	u_char speed;		
	u_char volume;		
	u_char tone;		
	u_char expression;	
	u_char ext_dict_loaded; 
	u_char ext_dict_status; 
	u_char free_ram;	
	u_char articulation;	
	u_char reverb;		
	u_char eob;		
	u_char has_indexing;	
};
