
static inline u8 getCx86(u8 reg)
{
	outb(reg, 0x22);
	return inb(0x23);
}

static inline void setCx86(u8 reg, u8 data)
{
	outb(reg, 0x22);
	outb(data, 0x23);
}

#define getCx86_old(reg) ({ outb((reg), 0x22); inb(0x23); })

#define setCx86_old(reg, data) do { \
	outb((reg), 0x22); \
	outb((data), 0x23); \
} while (0)

