#include <linux/types.h>

#include "opcode.h"

static bool opcode_is_prefix(uint8_t b)
{
	return
		
		b == 0xf0 || b == 0xf2 || b == 0xf3
		
		|| b == 0x2e || b == 0x36 || b == 0x3e || b == 0x26
		|| b == 0x64 || b == 0x65
		
		|| b == 0x66
		
		|| b == 0x67;
}

#ifdef CONFIG_X86_64
static bool opcode_is_rex_prefix(uint8_t b)
{
	return (b & 0xf0) == 0x40;
}
#else
static bool opcode_is_rex_prefix(uint8_t b)
{
	return false;
}
#endif

#define REX_W (1 << 3)

void kmemcheck_opcode_decode(const uint8_t *op, unsigned int *size)
{
	
	int operand_size_override = 4;

	
	for (; opcode_is_prefix(*op); ++op) {
		if (*op == 0x66)
			operand_size_override = 2;
	}

	
	if (opcode_is_rex_prefix(*op)) {
		uint8_t rex = *op;

		++op;
		if (rex & REX_W) {
			switch (*op) {
			case 0x63:
				*size = 4;
				return;
			case 0x0f:
				++op;

				switch (*op) {
				case 0xb6:
				case 0xbe:
					*size = 1;
					return;
				case 0xb7:
				case 0xbf:
					*size = 2;
					return;
				}

				break;
			}

			*size = 8;
			return;
		}
	}

	
	if (*op == 0x0f) {
		++op;

		if (*op == 0xb7 || *op == 0xbf)
			operand_size_override = 2;
	}

	*size = (*op & 1) ? operand_size_override : 1;
}

const uint8_t *kmemcheck_opcode_get_primary(const uint8_t *op)
{
	
	while (opcode_is_prefix(*op))
		++op;
	if (opcode_is_rex_prefix(*op))
		++op;
	return op;
}
