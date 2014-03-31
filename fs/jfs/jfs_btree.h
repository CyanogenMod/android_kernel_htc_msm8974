/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef	_H_JFS_BTREE
#define _H_JFS_BTREE



#define BT_TYPE		0x07	
#define	BT_ROOT		0x01	
#define	BT_LEAF		0x02	
#define	BT_INTERNAL	0x04	
#define	BT_RIGHTMOST	0x10	
#define	BT_LEFTMOST	0x20	
#define	BT_SWAPPED	0x80	

#define	BT_RANDOM		0x0000
#define	BT_SEQUENTIAL		0x0001
#define	BT_LOOKUP		0x0010
#define	BT_INSERT		0x0020
#define	BT_DELETE		0x0040

#define BT_IS_ROOT(MP) (((MP)->xflag & COMMIT_PAGE) == 0)

#define BT_PAGE(IP, MP, TYPE, ROOT)\
	(BT_IS_ROOT(MP) ? (TYPE *)&JFS_IP(IP)->ROOT : (TYPE *)(MP)->data)

#define BT_GETPAGE(IP, BN, MP, TYPE, SIZE, P, RC, ROOT)\
{\
	if ((BN) == 0)\
	{\
		MP = (struct metapage *)&JFS_IP(IP)->bxflag;\
		P = (TYPE *)&JFS_IP(IP)->ROOT;\
		RC = 0;\
	}\
	else\
	{\
		MP = read_metapage((IP), BN, SIZE, 1);\
		if (MP) {\
			RC = 0;\
			P = (MP)->data;\
		} else {\
			P = NULL;\
			jfs_err("bread failed!");\
			RC = -EIO;\
		}\
	}\
}

#define BT_MARK_DIRTY(MP, IP)\
{\
	if (BT_IS_ROOT(MP))\
		mark_inode_dirty(IP);\
	else\
		mark_metapage_dirty(MP);\
}

#define BT_PUTPAGE(MP)\
{\
	if (! BT_IS_ROOT(MP)) \
		release_metapage(MP); \
}


struct btframe {	
	s64 bn;			
	s16 index;		
	s16 lastindex;		
	struct metapage *mp;	
};				

struct btstack {
	struct btframe *top;
	int nsplit;
	struct btframe stack[MAXTREEHEIGHT];
};

#define BT_CLR(btstack)\
	(btstack)->top = (btstack)->stack

#define BT_STACK_FULL(btstack)\
	( (btstack)->top == &((btstack)->stack[MAXTREEHEIGHT-1]))

#define BT_PUSH(BTSTACK, BN, INDEX)\
{\
	assert(!BT_STACK_FULL(BTSTACK));\
	(BTSTACK)->top->bn = BN;\
	(BTSTACK)->top->index = INDEX;\
	++(BTSTACK)->top;\
}

#define BT_POP(btstack)\
	( (btstack)->top == (btstack)->stack ? NULL : --(btstack)->top )

#define BT_STACK(btstack)\
	( (btstack)->top == (btstack)->stack ? NULL : (btstack)->top )

static inline void BT_STACK_DUMP(struct btstack *btstack)
{
	int i;
	printk("btstack dump:\n");
	for (i = 0; i < MAXTREEHEIGHT; i++)
		printk(KERN_ERR "bn = %Lx, index = %d\n",
		       (long long)btstack->stack[i].bn,
		       btstack->stack[i].index);
}

#define BT_GETSEARCH(IP, LEAF, BN, MP, TYPE, P, INDEX, ROOT)\
{\
	BN = (LEAF)->bn;\
	MP = (LEAF)->mp;\
	if (BN)\
		P = (TYPE *)MP->data;\
	else\
		P = (TYPE *)&JFS_IP(IP)->ROOT;\
	INDEX = (LEAF)->index;\
}

#define BT_PUTSEARCH(BTSTACK)\
{\
	if (! BT_IS_ROOT((BTSTACK)->top->mp))\
		release_metapage((BTSTACK)->top->mp);\
}
#endif				
