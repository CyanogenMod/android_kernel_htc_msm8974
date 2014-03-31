/*	Small bzip2 deflate implementation, by Rob Landley (rob@landley.net).

	Based on bzip2 decompression code by Julian R Seward (jseward@acm.org),
	which also acknowledges contributions by Mike Burrows, David Wheeler,
	Peter Fenwick, Alistair Moffat, Radford Neal, Ian H. Witten,
	Robert Sedgewick, and Jon L. Bentley.

	This code is licensed under the LGPLv2:
		LGPL (http://www.gnu.org/copyleft/lgpl.html
*/




#ifdef STATIC
#define PREBOOT
#else
#include <linux/decompress/bunzip2.h>
#endif 

#include <linux/decompress/mm.h>

#ifndef INT_MAX
#define INT_MAX 0x7fffffff
#endif

#define MAX_GROUPS		6
#define GROUP_SIZE   		50	
#define MAX_HUFCODE_BITS 	20	
#define MAX_SYMBOLS 		258	
#define SYMBOL_RUNA		0
#define SYMBOL_RUNB		1

#define RETVAL_OK			0
#define RETVAL_LAST_BLOCK		(-1)
#define RETVAL_NOT_BZIP_DATA		(-2)
#define RETVAL_UNEXPECTED_INPUT_EOF	(-3)
#define RETVAL_UNEXPECTED_OUTPUT_EOF	(-4)
#define RETVAL_DATA_ERROR		(-5)
#define RETVAL_OUT_OF_MEMORY		(-6)
#define RETVAL_OBSOLETE_INPUT		(-7)

#define BZIP2_IOBUF_SIZE		4096

struct group_data {
	
	int limit[MAX_HUFCODE_BITS+1];
	int base[MAX_HUFCODE_BITS];
	int permute[MAX_SYMBOLS];
	int minLen, maxLen;
};

struct bunzip_data {
	
	int writeCopies, writePos, writeRunCountdown, writeCount, writeCurrent;
	
	int (*fill)(void*, unsigned int);
	int inbufCount, inbufPos ;
	unsigned char *inbuf ;
	unsigned int inbufBitCount, inbufBits;
	unsigned int crc32Table[256], headerCRC, totalCRC, writeCRC;
	
	unsigned int *dbuf, dbufSize;
	
	unsigned char selectors[32768];		
	struct group_data groups[MAX_GROUPS];	
	int io_error;			
	int byteCount[256];
	unsigned char symToByte[256], mtfSymbol[256];
};


static unsigned int INIT get_bits(struct bunzip_data *bd, char bits_wanted)
{
	unsigned int bits = 0;

	while (bd->inbufBitCount < bits_wanted) {
		if (bd->inbufPos == bd->inbufCount) {
			if (bd->io_error)
				return 0;
			bd->inbufCount = bd->fill(bd->inbuf, BZIP2_IOBUF_SIZE);
			if (bd->inbufCount <= 0) {
				bd->io_error = RETVAL_UNEXPECTED_INPUT_EOF;
				return 0;
			}
			bd->inbufPos = 0;
		}
		
		if (bd->inbufBitCount >= 24) {
			bits = bd->inbufBits&((1 << bd->inbufBitCount)-1);
			bits_wanted -= bd->inbufBitCount;
			bits <<= bits_wanted;
			bd->inbufBitCount = 0;
		}
		
		bd->inbufBits = (bd->inbufBits << 8)|bd->inbuf[bd->inbufPos++];
		bd->inbufBitCount += 8;
	}
	
	bd->inbufBitCount -= bits_wanted;
	bits |= (bd->inbufBits >> bd->inbufBitCount)&((1 << bits_wanted)-1);

	return bits;
}


static int INIT get_next_block(struct bunzip_data *bd)
{
	struct group_data *hufGroup = NULL;
	int *base = NULL;
	int *limit = NULL;
	int dbufCount, nextSym, dbufSize, groupCount, selector,
		i, j, k, t, runPos, symCount, symTotal, nSelectors, *byteCount;
	unsigned char uc, *symToByte, *mtfSymbol, *selectors;
	unsigned int *dbuf, origPtr;

	dbuf = bd->dbuf;
	dbufSize = bd->dbufSize;
	selectors = bd->selectors;
	byteCount = bd->byteCount;
	symToByte = bd->symToByte;
	mtfSymbol = bd->mtfSymbol;

	i = get_bits(bd, 24);
	j = get_bits(bd, 24);
	bd->headerCRC = get_bits(bd, 32);
	if ((i == 0x177245) && (j == 0x385090))
		return RETVAL_LAST_BLOCK;
	if ((i != 0x314159) || (j != 0x265359))
		return RETVAL_NOT_BZIP_DATA;
	if (get_bits(bd, 1))
		return RETVAL_OBSOLETE_INPUT;
	origPtr = get_bits(bd, 24);
	if (origPtr > dbufSize)
		return RETVAL_DATA_ERROR;
	t = get_bits(bd, 16);
	symTotal = 0;
	for (i = 0; i < 16; i++) {
		if (t&(1 << (15-i))) {
			k = get_bits(bd, 16);
			for (j = 0; j < 16; j++)
				if (k&(1 << (15-j)))
					symToByte[symTotal++] = (16*i)+j;
		}
	}
	
	groupCount = get_bits(bd, 3);
	if (groupCount < 2 || groupCount > MAX_GROUPS)
		return RETVAL_DATA_ERROR;
	nSelectors = get_bits(bd, 15);
	if (!nSelectors)
		return RETVAL_DATA_ERROR;
	for (i = 0; i < groupCount; i++)
		mtfSymbol[i] = i;
	for (i = 0; i < nSelectors; i++) {
		
		for (j = 0; get_bits(bd, 1); j++)
			if (j >= groupCount)
				return RETVAL_DATA_ERROR;
		
		uc = mtfSymbol[j];
		for (; j; j--)
			mtfSymbol[j] = mtfSymbol[j-1];
		mtfSymbol[0] = selectors[i] = uc;
	}
	symCount = symTotal+2;
	for (j = 0; j < groupCount; j++) {
		unsigned char length[MAX_SYMBOLS], temp[MAX_HUFCODE_BITS+1];
		int	minLen,	maxLen, pp;
		t = get_bits(bd, 5)-1;
		for (i = 0; i < symCount; i++) {
			for (;;) {
				if (((unsigned)t) > (MAX_HUFCODE_BITS-1))
					return RETVAL_DATA_ERROR;


				k = get_bits(bd, 2);
				if (k < 2) {
					bd->inbufBitCount++;
					break;
				}
				t += (((k+1)&2)-1);
			}
			length[i] = t+1;
		}
		
		minLen = maxLen = length[0];

		for (i = 1; i < symCount; i++) {
			if (length[i] > maxLen)
				maxLen = length[i];
			else if (length[i] < minLen)
				minLen = length[i];
		}

		hufGroup = bd->groups+j;
		hufGroup->minLen = minLen;
		hufGroup->maxLen = maxLen;
		base = hufGroup->base-1;
		limit = hufGroup->limit-1;
		pp = 0;
		for (i = minLen; i <= maxLen; i++) {
			temp[i] = limit[i] = 0;
			for (t = 0; t < symCount; t++)
				if (length[t] == i)
					hufGroup->permute[pp++] = t;
		}
		
		for (i = 0; i < symCount; i++)
			temp[length[i]]++;
		pp = t = 0;
		for (i = minLen; i < maxLen; i++) {
			pp += temp[i];
			limit[i] = (pp << (maxLen - i)) - 1;
			pp <<= 1;
			base[i+1] = pp-(t += temp[i]);
		}
		limit[maxLen+1] = INT_MAX; 
		limit[maxLen] = pp+temp[maxLen]-1;
		base[minLen] = 0;
	}

	for (i = 0; i < 256; i++) {
		byteCount[i] = 0;
		mtfSymbol[i] = (unsigned char)i;
	}
	
	runPos = dbufCount = symCount = selector = 0;
	for (;;) {
		
		if (!(symCount--)) {
			symCount = GROUP_SIZE-1;
			if (selector >= nSelectors)
				return RETVAL_DATA_ERROR;
			hufGroup = bd->groups+selectors[selector++];
			base = hufGroup->base-1;
			limit = hufGroup->limit-1;
		}
		
		while (bd->inbufBitCount < hufGroup->maxLen) {
			if (bd->inbufPos == bd->inbufCount) {
				j = get_bits(bd, hufGroup->maxLen);
				goto got_huff_bits;
			}
			bd->inbufBits =
				(bd->inbufBits << 8)|bd->inbuf[bd->inbufPos++];
			bd->inbufBitCount += 8;
		};
		bd->inbufBitCount -= hufGroup->maxLen;
		j = (bd->inbufBits >> bd->inbufBitCount)&
			((1 << hufGroup->maxLen)-1);
got_huff_bits:
		i = hufGroup->minLen;
		while (j > limit[i])
			++i;
		bd->inbufBitCount += (hufGroup->maxLen - i);
		
		if ((i > hufGroup->maxLen)
			|| (((unsigned)(j = (j>>(hufGroup->maxLen-i))-base[i]))
				>= MAX_SYMBOLS))
			return RETVAL_DATA_ERROR;
		nextSym = hufGroup->permute[j];
		if (((unsigned)nextSym) <= SYMBOL_RUNB) { 
			if (!runPos) {
				runPos = 1;
				t = 0;
			}
			t += (runPos << nextSym);
			

			runPos <<= 1;
			continue;
		}
		if (runPos) {
			runPos = 0;
			if (dbufCount+t >= dbufSize)
				return RETVAL_DATA_ERROR;

			uc = symToByte[mtfSymbol[0]];
			byteCount[uc] += t;
			while (t--)
				dbuf[dbufCount++] = uc;
		}
		
		if (nextSym > symTotal)
			break;
		if (dbufCount >= dbufSize)
			return RETVAL_DATA_ERROR;
		i = nextSym - 1;
		uc = mtfSymbol[i];
		do {
			mtfSymbol[i] = mtfSymbol[i-1];
		} while (--i);
		mtfSymbol[0] = uc;
		uc = symToByte[uc];
		
		byteCount[uc]++;
		dbuf[dbufCount++] = (unsigned int)uc;
	}
	
	j = 0;
	for (i = 0; i < 256; i++) {
		k = j+byteCount[i];
		byteCount[i] = j;
		j = k;
	}
	
	for (i = 0; i < dbufCount; i++) {
		uc = (unsigned char)(dbuf[i] & 0xff);
		dbuf[byteCount[uc]] |= (i << 8);
		byteCount[uc]++;
	}
	if (dbufCount) {
		if (origPtr >= dbufCount)
			return RETVAL_DATA_ERROR;
		bd->writePos = dbuf[origPtr];
		bd->writeCurrent = (unsigned char)(bd->writePos&0xff);
		bd->writePos >>= 8;
		bd->writeRunCountdown = 5;
	}
	bd->writeCount = dbufCount;

	return RETVAL_OK;
}

/* Undo burrows-wheeler transform on intermediate buffer to produce output.
   If start_bunzip was initialized with out_fd =-1, then up to len bytes of
   data are written to outbuf.  Return value is number of bytes written or
   error (all errors are negative numbers).  If out_fd!=-1, outbuf and len
   are ignored, data is written to out_fd and return is RETVAL_OK or error.
*/

static int INIT read_bunzip(struct bunzip_data *bd, char *outbuf, int len)
{
	const unsigned int *dbuf;
	int pos, xcurrent, previous, gotcount;

	
	if (bd->writeCount < 0)
		return bd->writeCount;

	gotcount = 0;
	dbuf = bd->dbuf;
	pos = bd->writePos;
	xcurrent = bd->writeCurrent;


	if (bd->writeCopies) {
		
		--bd->writeCopies;
		
		for (;;) {
			if (gotcount >= len) {
				bd->writePos = pos;
				bd->writeCurrent = xcurrent;
				bd->writeCopies++;
				return len;
			}
			
			outbuf[gotcount++] = xcurrent;
			bd->writeCRC = (((bd->writeCRC) << 8)
				^bd->crc32Table[((bd->writeCRC) >> 24)
				^xcurrent]);
			if (bd->writeCopies) {
				--bd->writeCopies;
				continue;
			}
decode_next_byte:
			if (!bd->writeCount--)
				break;
			previous = xcurrent;
			pos = dbuf[pos];
			xcurrent = pos&0xff;
			pos >>= 8;
			if (--bd->writeRunCountdown) {
				if (xcurrent != previous)
					bd->writeRunCountdown = 4;
			} else {
				bd->writeCopies = xcurrent;
				xcurrent = previous;
				bd->writeRunCountdown = 5;
				if (!bd->writeCopies)
					goto decode_next_byte;
				--bd->writeCopies;
			}
		}
		
		bd->writeCRC = ~bd->writeCRC;
		bd->totalCRC = ((bd->totalCRC << 1) |
				(bd->totalCRC >> 31)) ^ bd->writeCRC;
		
		if (bd->writeCRC != bd->headerCRC) {
			bd->totalCRC = bd->headerCRC+1;
			return RETVAL_LAST_BLOCK;
		}
	}

	
	previous = get_next_block(bd);
	if (previous) {
		bd->writeCount = previous;
		return (previous != RETVAL_LAST_BLOCK) ? previous : gotcount;
	}
	bd->writeCRC = 0xffffffffUL;
	pos = bd->writePos;
	xcurrent = bd->writeCurrent;
	goto decode_next_byte;
}

static int INIT nofill(void *buf, unsigned int len)
{
	return -1;
}

static int INIT start_bunzip(struct bunzip_data **bdp, void *inbuf, int len,
			     int (*fill)(void*, unsigned int))
{
	struct bunzip_data *bd;
	unsigned int i, j, c;
	const unsigned int BZh0 =
		(((unsigned int)'B') << 24)+(((unsigned int)'Z') << 16)
		+(((unsigned int)'h') << 8)+(unsigned int)'0';

	
	i = sizeof(struct bunzip_data);

	
	bd = *bdp = malloc(i);
	if (!bd)
		return RETVAL_OUT_OF_MEMORY;
	memset(bd, 0, sizeof(struct bunzip_data));
	
	bd->inbuf = inbuf;
	bd->inbufCount = len;
	if (fill != NULL)
		bd->fill = fill;
	else
		bd->fill = nofill;

	
	for (i = 0; i < 256; i++) {
		c = i << 24;
		for (j = 8; j; j--)
			c = c&0x80000000 ? (c << 1)^0x04c11db7 : (c << 1);
		bd->crc32Table[i] = c;
	}

	
	i = get_bits(bd, 32);
	if (((unsigned int)(i-BZh0-1)) >= 9)
		return RETVAL_NOT_BZIP_DATA;

	bd->dbufSize = 100000*(i-BZh0);

	bd->dbuf = large_malloc(bd->dbufSize * sizeof(int));
	if (!bd->dbuf)
		return RETVAL_OUT_OF_MEMORY;
	return RETVAL_OK;
}

STATIC int INIT bunzip2(unsigned char *buf, int len,
			int(*fill)(void*, unsigned int),
			int(*flush)(void*, unsigned int),
			unsigned char *outbuf,
			int *pos,
			void(*error)(char *x))
{
	struct bunzip_data *bd;
	int i = -1;
	unsigned char *inbuf;

	if (flush)
		outbuf = malloc(BZIP2_IOBUF_SIZE);

	if (!outbuf) {
		error("Could not allocate output buffer");
		return RETVAL_OUT_OF_MEMORY;
	}
	if (buf)
		inbuf = buf;
	else
		inbuf = malloc(BZIP2_IOBUF_SIZE);
	if (!inbuf) {
		error("Could not allocate input buffer");
		i = RETVAL_OUT_OF_MEMORY;
		goto exit_0;
	}
	i = start_bunzip(&bd, inbuf, len, fill);
	if (!i) {
		for (;;) {
			i = read_bunzip(bd, outbuf, BZIP2_IOBUF_SIZE);
			if (i <= 0)
				break;
			if (!flush)
				outbuf += i;
			else
				if (i != flush(outbuf, i)) {
					i = RETVAL_UNEXPECTED_OUTPUT_EOF;
					break;
				}
		}
	}
	
	if (i == RETVAL_LAST_BLOCK) {
		if (bd->headerCRC != bd->totalCRC)
			error("Data integrity error when decompressing.");
		else
			i = RETVAL_OK;
	} else if (i == RETVAL_UNEXPECTED_OUTPUT_EOF) {
		error("Compressed file ends unexpectedly");
	}
	if (!bd)
		goto exit_1;
	if (bd->dbuf)
		large_free(bd->dbuf);
	if (pos)
		*pos = bd->inbufPos;
	free(bd);
exit_1:
	if (!buf)
		free(inbuf);
exit_0:
	if (flush)
		free(outbuf);
	return i;
}

#ifdef PREBOOT
STATIC int INIT decompress(unsigned char *buf, int len,
			int(*fill)(void*, unsigned int),
			int(*flush)(void*, unsigned int),
			unsigned char *outbuf,
			int *pos,
			void(*error)(char *x))
{
	return bunzip2(buf, len - 4, fill, flush, outbuf, pos, error);
}
#endif
