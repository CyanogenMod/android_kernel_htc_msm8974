/*
 * include/asm-xtensa/shmparam.h
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file "COPYING" in the main directory of
 * this archive for more details.
 */

#ifndef _XTENSA_SHMPARAM_H
#define _XTENSA_SHMPARAM_H


#define SHMLBA	((PAGE_SIZE > DCACHE_WAY_SIZE)? PAGE_SIZE : DCACHE_WAY_SIZE)

#endif 
