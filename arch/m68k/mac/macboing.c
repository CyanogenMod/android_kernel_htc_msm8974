
#include <linux/sched.h>
#include <linux/timer.h>

#include <asm/macintosh.h>
#include <asm/mac_asc.h>

static int mac_asc_inited;
static __u8 mac_asc_wave_tab[ 0x800 ];

static const signed char sine_data[] = {
	0,  39,  75,  103,  121,  127,  121,  103,  75,  39,
	0, -39, -75, -103, -121, -127, -121, -103, -75, -39
};

static volatile __u8* mac_asc_regs = ( void* )0x50F14000;

static unsigned long mac_asc_samplespersec = 11050;
static int mac_bell_duration;
static unsigned long mac_bell_phase; 
static unsigned long mac_bell_phasepersample;

static void mac_init_asc( void );
static void mac_nosound( unsigned long );
static void mac_quadra_start_bell( unsigned int, unsigned int, unsigned int );
static void mac_quadra_ring_bell( unsigned long );
static void mac_av_start_bell( unsigned int, unsigned int, unsigned int );
static void ( *mac_special_bell )( unsigned int, unsigned int, unsigned int );

static DEFINE_TIMER(mac_sound_timer, mac_nosound, 0, 0);

static void mac_init_asc( void )
{
	int i;

	switch ( macintosh_config->ident )
	{
		case MAC_MODEL_IIFX:
			mac_asc_regs = ( void* )0x50010000;
			break;
		case MAC_MODEL_Q630:
		case MAC_MODEL_P475:
			mac_special_bell = mac_quadra_start_bell;
			mac_asc_samplespersec = 22150;
			break;
		case MAC_MODEL_C660:
		case MAC_MODEL_Q840:
			mac_special_bell = mac_av_start_bell;
			break;
		case MAC_MODEL_Q650:
		case MAC_MODEL_Q700:
		case MAC_MODEL_Q800:
		case MAC_MODEL_Q900:
		case MAC_MODEL_Q950:
			mac_special_bell = NULL;
			break;
		default:
			mac_special_bell = NULL;
			break;
	}

	for ( i = 0; i < 0x400; i++ )
	{
		mac_asc_wave_tab[ i ] = i / 4;
		mac_asc_wave_tab[ i + 0x400 ] = 0xFF - i / 4;
	}
	mac_asc_inited = 1;
}

void mac_mksound( unsigned int freq, unsigned int length )
{
	__u32 cfreq = ( freq << 5 ) / 468;
	unsigned long flags;
	int i;

	if ( mac_special_bell == NULL )
	{
		
		return;
	}

	if ( !mac_asc_inited )
		mac_init_asc();

	if ( mac_special_bell )
	{
		mac_special_bell( freq, length, 128 );
		return;
	}

	if ( freq < 20 || freq > 20000 || length == 0 )
	{
		mac_nosound( 0 );
		return;
	}

	local_irq_save(flags);

	del_timer( &mac_sound_timer );

	for ( i = 0; i < 0x800; i++ )
		mac_asc_regs[ i ] = 0;
	for ( i = 0; i < 0x800; i++ )
		mac_asc_regs[ i ] = mac_asc_wave_tab[ i ];

	for ( i = 0; i < 8; i++ )
		*( __u32* )( ( __u32 )mac_asc_regs + ASC_CONTROL + 0x814 + 8 * i ) = cfreq;

	mac_asc_regs[ 0x807 ] = 0;
	mac_asc_regs[ ASC_VOLUME ] = 128;
	mac_asc_regs[ 0x805 ] = 0;
	mac_asc_regs[ 0x80F ] = 0;
	mac_asc_regs[ ASC_MODE ] = ASC_MODE_SAMPLE;
	mac_asc_regs[ ASC_ENABLE ] = ASC_ENABLE_SAMPLE;

	mac_sound_timer.expires = jiffies + length;
	add_timer( &mac_sound_timer );

	local_irq_restore(flags);
}

static void mac_nosound( unsigned long ignored )
{
	mac_asc_regs[ ASC_ENABLE ] = 0;
}

static void mac_quadra_start_bell( unsigned int freq, unsigned int length, unsigned int volume )
{
	unsigned long flags;

	
	if ( mac_bell_duration > 0 )
	{
		mac_bell_duration += length;
		return;
	}

	mac_bell_duration = length;
	mac_bell_phase = 0;
	mac_bell_phasepersample = ( freq * sizeof( mac_asc_wave_tab ) ) / mac_asc_samplespersec;
	

	local_irq_save(flags);

	
	mac_asc_regs[ 0x806 ] = volume;

	
	if ( mac_asc_regs[ 0x801 ] != 1 )
	{
		
		mac_asc_regs[ 0x807 ] = 0;
		
		mac_asc_regs[ 0x802 ] = 0;
		
		mac_asc_regs[ 0x801 ] = 1;
		mac_asc_regs[ 0x803 ] |= 0x80;
		mac_asc_regs[ 0x803 ] &= 0x7F;
	}

	mac_sound_timer.function = mac_quadra_ring_bell;
	mac_sound_timer.expires = jiffies + 1;
	add_timer( &mac_sound_timer );

	local_irq_restore(flags);
}

static void mac_quadra_ring_bell( unsigned long ignored )
{
	int	i, count = mac_asc_samplespersec / HZ;
	unsigned long flags;


	local_irq_save(flags);

	del_timer( &mac_sound_timer );

	if ( mac_bell_duration-- > 0 )
	{
		for ( i = 0; i < count; i++ )
		{
			mac_bell_phase += mac_bell_phasepersample;
			mac_asc_regs[ 0 ] = mac_asc_wave_tab[ mac_bell_phase & ( sizeof( mac_asc_wave_tab ) - 1 ) ];
		}
		mac_sound_timer.expires = jiffies + 1;
		add_timer( &mac_sound_timer );
	}
	else
		mac_asc_regs[ 0x801 ] = 0;

	local_irq_restore(flags);
}

static void mac_av_start_bell( unsigned int freq, unsigned int length, unsigned int volume )
{
}
