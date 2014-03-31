typedef struct vmidi_devc {
	   int dev;

	
	   int opened;
	   spinlock_t lock;

	
	   int my_mididev;
	   int pair_mididev;
	   int input_opened;
	   int intr_active;
	   void (*midi_input_intr) (int dev, unsigned char data);
	} vmidi_devc;
