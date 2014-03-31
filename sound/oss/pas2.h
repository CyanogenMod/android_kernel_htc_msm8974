
int pas_set_intr(int mask);
int pas_remove_intr(int mask);
unsigned char pas_read(int ioaddr);
void pas_write(unsigned char data, int ioaddr);

void pas_pcm_interrupt(unsigned char status, int cause);
void pas_pcm_init(struct address_info *hw_config);

int pas_init_mixer(void);

void pas_midi_init(void);
void pas_midi_interrupt(void);
