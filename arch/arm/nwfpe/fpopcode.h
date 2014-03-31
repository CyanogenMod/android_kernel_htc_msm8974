/*
    NetWinder Floating Point Emulator
    (c) Rebel.COM, 1998,1999
    (c) Philip Blundell, 2001

    Direct questions, comments to Scott Bambrough <scottb@netwinder.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __FPOPCODE_H__
#define __FPOPCODE_H__










#define BIT_PREINDEX	0x01000000
#define BIT_UP		0x00800000
#define BIT_WRITE_BACK	0x00200000
#define BIT_LOAD	0x00100000

#define MASK_CPDT		0x0c000000	
#define MASK_OFFSET		0x000000ff
#define MASK_TRANSFER_LENGTH	0x00408000
#define MASK_REGISTER_COUNT	MASK_TRANSFER_LENGTH
#define MASK_COPROCESSOR	0x00000f00

#define TRANSFER_SINGLE		0x00000000
#define TRANSFER_DOUBLE		0x00008000
#define TRANSFER_EXTENDED	0x00400000
#define TRANSFER_PACKED		MASK_TRANSFER_LENGTH

#define getCoprocessorNumber(opcode)	((opcode & MASK_COPROCESSOR) >> 8)

#define getOffset(opcode)		(opcode & MASK_OFFSET)

#define TEST_OPCODE(opcode,mask)	(((opcode) & (mask)) == (mask))

#define LOAD_OP(opcode)   TEST_OPCODE((opcode),MASK_CPDT | BIT_LOAD)
#define STORE_OP(opcode)  ((opcode & (MASK_CPDT | BIT_LOAD)) == MASK_CPDT)

#define LDF_OP(opcode)	(LOAD_OP(opcode) && (getCoprocessorNumber(opcode) == 1))
#define LFM_OP(opcode)	(LOAD_OP(opcode) && (getCoprocessorNumber(opcode) == 2))
#define STF_OP(opcode)	(STORE_OP(opcode) && (getCoprocessorNumber(opcode) == 1))
#define SFM_OP(opcode)	(STORE_OP(opcode) && (getCoprocessorNumber(opcode) == 2))

#define PREINDEXED(opcode)		((opcode & BIT_PREINDEX) != 0)
#define POSTINDEXED(opcode)		((opcode & BIT_PREINDEX) == 0)
#define BIT_UP_SET(opcode)		((opcode & BIT_UP) != 0)
#define BIT_UP_CLEAR(opcode)		((opcode & BIT_DOWN) == 0)
#define WRITE_BACK(opcode)		((opcode & BIT_WRITE_BACK) != 0)
#define LOAD(opcode)			((opcode & BIT_LOAD) != 0)
#define STORE(opcode)			((opcode & BIT_LOAD) == 0)

#define BIT_MONADIC	0x00008000
#define BIT_CONSTANT	0x00000008

#define CONSTANT_FM(opcode)		((opcode & BIT_CONSTANT) != 0)
#define MONADIC_INSTRUCTION(opcode)	((opcode & BIT_MONADIC) != 0)

#define MASK_CPDO		0x0e000000	
#define MASK_ARITHMETIC_OPCODE	0x00f08000
#define MASK_DESTINATION_SIZE	0x00080080

#define ADF_CODE	0x00000000
#define MUF_CODE	0x00100000
#define SUF_CODE	0x00200000
#define RSF_CODE	0x00300000
#define DVF_CODE	0x00400000
#define RDF_CODE	0x00500000
#define POW_CODE	0x00600000
#define RPW_CODE	0x00700000
#define RMF_CODE	0x00800000
#define FML_CODE	0x00900000
#define FDV_CODE	0x00a00000
#define FRD_CODE	0x00b00000
#define POL_CODE	0x00c00000

#define MVF_CODE	0x00008000
#define MNF_CODE	0x00108000
#define ABS_CODE	0x00208000
#define RND_CODE	0x00308000
#define SQT_CODE	0x00408000
#define LOG_CODE	0x00508000
#define LGN_CODE	0x00608000
#define EXP_CODE	0x00708000
#define SIN_CODE	0x00808000
#define COS_CODE	0x00908000
#define TAN_CODE	0x00a08000
#define ASN_CODE	0x00b08000
#define ACS_CODE	0x00c08000
#define ATN_CODE	0x00d08000
#define URD_CODE	0x00e08000
#define NRM_CODE	0x00f08000


#define MASK_CPRT		0x0e000010	
#define MASK_CPRT_CODE		0x00f00000
#define FLT_CODE		0x00000000
#define FIX_CODE		0x00100000
#define WFS_CODE		0x00200000
#define RFS_CODE		0x00300000
#define WFC_CODE		0x00400000
#define RFC_CODE		0x00500000
#define CMF_CODE		0x00900000
#define CNF_CODE		0x00b00000
#define CMFE_CODE		0x00d00000
#define CNFE_CODE		0x00f00000


#define MASK_Rd		0x0000f000
#define MASK_Rn		0x000f0000
#define MASK_Fd		0x00007000
#define MASK_Fm		0x00000007
#define MASK_Fn		0x00070000

#define CC_MASK		0xf0000000
#define CC_NEGATIVE	0x80000000
#define CC_ZERO		0x40000000
#define CC_CARRY	0x20000000
#define CC_OVERFLOW	0x10000000
#define CC_EQ		0x00000000
#define CC_NE		0x10000000
#define CC_CS		0x20000000
#define CC_HS		CC_CS
#define CC_CC		0x30000000
#define CC_LO		CC_CC
#define CC_MI		0x40000000
#define CC_PL		0x50000000
#define CC_VS		0x60000000
#define CC_VC		0x70000000
#define CC_HI		0x80000000
#define CC_LS		0x90000000
#define CC_GE		0xa0000000
#define CC_LT		0xb0000000
#define CC_GT		0xc0000000
#define CC_LE		0xd0000000
#define CC_AL		0xe0000000
#define CC_NV		0xf0000000

#define MASK_ROUNDING_MODE	0x00000060
#define ROUND_TO_NEAREST	0x00000000
#define ROUND_TO_PLUS_INFINITY	0x00000020
#define ROUND_TO_MINUS_INFINITY	0x00000040
#define ROUND_TO_ZERO		0x00000060

#define MASK_ROUNDING_PRECISION	0x00080080
#define ROUND_SINGLE		0x00000000
#define ROUND_DOUBLE		0x00000080
#define ROUND_EXTENDED		0x00080000

#define getCondition(opcode)		(opcode >> 28)

#define getRn(opcode)			((opcode & MASK_Rn) >> 16)

#define getFd(opcode)			((opcode & MASK_Fd) >> 12)

#define getFn(opcode)		((opcode & MASK_Fn) >> 16)

#define getFm(opcode)		(opcode & MASK_Fm)

#define getRd(opcode)		((opcode & MASK_Rd) >> 12)

#define getRoundingMode(opcode)		((opcode & MASK_ROUNDING_MODE) >> 5)

#ifdef CONFIG_FPE_NWFPE_XP
static inline floatx80 __pure getExtendedConstant(const unsigned int nIndex)
{
	extern const floatx80 floatx80Constant[];
	return floatx80Constant[nIndex];
}
#endif

static inline float64 __pure getDoubleConstant(const unsigned int nIndex)
{
	extern const float64 float64Constant[];
	return float64Constant[nIndex];
}

static inline float32 __pure getSingleConstant(const unsigned int nIndex)
{
	extern const float32 float32Constant[];
	return float32Constant[nIndex];
}

static inline unsigned int getTransferLength(const unsigned int opcode)
{
	unsigned int nRc;

	switch (opcode & MASK_TRANSFER_LENGTH) {
	case 0x00000000:
		nRc = 1;
		break;		
	case 0x00008000:
		nRc = 2;
		break;		
	case 0x00400000:
		nRc = 3;
		break;		
	default:
		nRc = 0;
	}

	return (nRc);
}

static inline unsigned int getRegisterCount(const unsigned int opcode)
{
	unsigned int nRc;

	switch (opcode & MASK_REGISTER_COUNT) {
	case 0x00000000:
		nRc = 4;
		break;
	case 0x00008000:
		nRc = 1;
		break;
	case 0x00400000:
		nRc = 2;
		break;
	case 0x00408000:
		nRc = 3;
		break;
	default:
		nRc = 0;
	}

	return (nRc);
}

static inline unsigned int getRoundingPrecision(const unsigned int opcode)
{
	unsigned int nRc;

	switch (opcode & MASK_ROUNDING_PRECISION) {
	case 0x00000000:
		nRc = 1;
		break;
	case 0x00000080:
		nRc = 2;
		break;
	case 0x00080000:
		nRc = 3;
		break;
	default:
		nRc = 0;
	}

	return (nRc);
}

static inline unsigned int getDestinationSize(const unsigned int opcode)
{
	unsigned int nRc;

	switch (opcode & MASK_DESTINATION_SIZE) {
	case 0x00000000:
		nRc = typeSingle;
		break;
	case 0x00000080:
		nRc = typeDouble;
		break;
	case 0x00080000:
		nRc = typeExtended;
		break;
	default:
		nRc = typeNone;
	}

	return (nRc);
}

extern const float64 float64Constant[];
extern const float32 float32Constant[];

#endif
