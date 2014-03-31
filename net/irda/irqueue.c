/*********************************************************************
 *
 * Filename:      irqueue.c
 * Version:       0.3
 * Description:   General queue implementation
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Jun  9 13:29:31 1998
 * Modified at:   Sun Dec 12 13:48:22 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * Modified at:   Thu Jan  4 14:29:10 CET 2001
 * Modified by:   Marc Zyngier <mzyngier@freesurf.fr>
 *
 *     Copyright (C) 1998-1999, Aage Kvalnes <aage@cs.uit.no>
 *     Copyright (C) 1998, Dag Brattli,
 *     All Rights Reserved.
 *
 *     This code is taken from the Vortex Operating System written by Aage
 *     Kvalnes. Aage has agreed that this code can use the GPL licence,
 *     although he does not use that licence in his own code.
 *
 *     This copyright does however _not_ include the ELF hash() function
 *     which I currently don't know which licence or copyright it
 *     has. Please inform me if you know.
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is
 *     provided "AS-IS" and at no charge.
 *
 ********************************************************************/


#include <linux/module.h>
#include <linux/slab.h>

#include <net/irda/irda.h>
#include <net/irda/irqueue.h>


#define GET_HASHBIN(x) ( x & HASHBIN_MASK )

static __u32 hash( const char* name)
{
	__u32 h = 0;
	__u32 g;

	while(*name) {
		h = (h<<4) + *name++;
		if ((g = (h & 0xf0000000)))
			h ^=g>>24;
		h &=~g;
	}
	return h;
}

static void enqueue_first(irda_queue_t **queue, irda_queue_t* element)
{

	IRDA_DEBUG( 4, "%s()\n", __func__);

	if ( *queue == NULL ) {
		element->q_next = element->q_prev = *queue = element;

	} else {
		element->q_next          = (*queue);
		(*queue)->q_prev->q_next = element;
		element->q_prev          = (*queue)->q_prev;
		(*queue)->q_prev         = element;
		(*queue)                 = element;
	}
}


static irda_queue_t *dequeue_first(irda_queue_t **queue)
{
	irda_queue_t *ret;

	IRDA_DEBUG( 4, "dequeue_first()\n");

	ret =  *queue;

	if ( *queue == NULL ) {
	} else if ( (*queue)->q_next == *queue ) {
		*queue = NULL;
	} else {
		(*queue)->q_prev->q_next = (*queue)->q_next;
		(*queue)->q_next->q_prev = (*queue)->q_prev;
		*queue = (*queue)->q_next;
	}

	return ret;
}

static irda_queue_t *dequeue_general(irda_queue_t **queue, irda_queue_t* element)
{
	irda_queue_t *ret;

	IRDA_DEBUG( 4, "dequeue_general()\n");

	ret =  *queue;

	if ( *queue == NULL ) {
	} else if ( (*queue)->q_next == *queue ) {
		*queue = NULL;

	} else {
		element->q_prev->q_next = element->q_next;
		element->q_next->q_prev = element->q_prev;
		if ( (*queue) == element)
			(*queue) = element->q_next;
	}

	return ret;
}


hashbin_t *hashbin_new(int type)
{
	hashbin_t* hashbin;

	hashbin = kzalloc(sizeof(*hashbin), GFP_ATOMIC);
	if (!hashbin)
		return NULL;

	hashbin->hb_type = type;
	hashbin->magic = HB_MAGIC;
	

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_lock_init(&hashbin->hb_spinlock);
	}

	return hashbin;
}
EXPORT_SYMBOL(hashbin_new);


#ifdef CONFIG_LOCKDEP
static int hashbin_lock_depth = 0;
#endif
int hashbin_delete( hashbin_t* hashbin, FREE_FUNC free_func)
{
	irda_queue_t* queue;
	unsigned long flags = 0;
	int i;

	IRDA_ASSERT(hashbin != NULL, return -1;);
	IRDA_ASSERT(hashbin->magic == HB_MAGIC, return -1;);

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_lock_irqsave_nested(&hashbin->hb_spinlock, flags,
					 hashbin_lock_depth++);
	}

	for (i = 0; i < HASHBIN_SIZE; i ++ ) {
		queue = dequeue_first((irda_queue_t**) &hashbin->hb_queue[i]);
		while (queue ) {
			if (free_func)
				(*free_func)(queue);
			queue = dequeue_first(
				(irda_queue_t**) &hashbin->hb_queue[i]);
		}
	}

	
	hashbin->hb_current = NULL;
	hashbin->magic = ~HB_MAGIC;

	
	if ( hashbin->hb_type & HB_LOCK) {
		spin_unlock_irqrestore(&hashbin->hb_spinlock, flags);
#ifdef CONFIG_LOCKDEP
		hashbin_lock_depth--;
#endif
	}

	kfree(hashbin);

	return 0;
}
EXPORT_SYMBOL(hashbin_delete);


void hashbin_insert(hashbin_t* hashbin, irda_queue_t* entry, long hashv,
		    const char* name)
{
	unsigned long flags = 0;
	int bin;

	IRDA_DEBUG( 4, "%s()\n", __func__);

	IRDA_ASSERT( hashbin != NULL, return;);
	IRDA_ASSERT( hashbin->magic == HB_MAGIC, return;);

	if ( name )
		hashv = hash( name );
	bin = GET_HASHBIN( hashv );

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_lock_irqsave(&hashbin->hb_spinlock, flags);
	} 

	entry->q_hash = hashv;
	if ( name )
		strlcpy( entry->q_name, name, sizeof(entry->q_name));

	enqueue_first( (irda_queue_t**) &hashbin->hb_queue[ bin ],
		       entry);
	hashbin->hb_size++;

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_unlock_irqrestore(&hashbin->hb_spinlock, flags);
	} 
}
EXPORT_SYMBOL(hashbin_insert);

void *hashbin_remove_first( hashbin_t *hashbin)
{
	unsigned long flags = 0;
	irda_queue_t *entry = NULL;

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_lock_irqsave(&hashbin->hb_spinlock, flags);
	} 

	entry = hashbin_get_first( hashbin);
	if ( entry != NULL) {
		int	bin;
		long	hashv;
		hashv = entry->q_hash;
		bin = GET_HASHBIN( hashv );

		dequeue_general( (irda_queue_t**) &hashbin->hb_queue[ bin ],
				 (irda_queue_t*) entry );
		hashbin->hb_size--;
		entry->q_next = NULL;
		entry->q_prev = NULL;

		if ( entry == hashbin->hb_current)
			hashbin->hb_current = NULL;
	}

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_unlock_irqrestore(&hashbin->hb_spinlock, flags);
	} 

	return entry;
}


void* hashbin_remove( hashbin_t* hashbin, long hashv, const char* name)
{
	int bin, found = FALSE;
	unsigned long flags = 0;
	irda_queue_t* entry;

	IRDA_DEBUG( 4, "%s()\n", __func__);

	IRDA_ASSERT( hashbin != NULL, return NULL;);
	IRDA_ASSERT( hashbin->magic == HB_MAGIC, return NULL;);

	if ( name )
		hashv = hash( name );
	bin = GET_HASHBIN( hashv );

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_lock_irqsave(&hashbin->hb_spinlock, flags);
	} 

	entry = hashbin->hb_queue[ bin ];
	if ( entry ) {
		do {
			if ( entry->q_hash == hashv ) {
				if ( name ) {
					if ( strcmp( entry->q_name, name) == 0)
					{
						found = TRUE;
						break;
					}
				} else {
					found = TRUE;
					break;
				}
			}
			entry = entry->q_next;
		} while ( entry != hashbin->hb_queue[ bin ] );
	}

	if ( found ) {
		dequeue_general( (irda_queue_t**) &hashbin->hb_queue[ bin ],
				 (irda_queue_t*) entry );
		hashbin->hb_size--;

		if ( entry == hashbin->hb_current)
			hashbin->hb_current = NULL;
	}

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_unlock_irqrestore(&hashbin->hb_spinlock, flags);
	} 


	
	if ( found )
		return entry;
	else
		return NULL;

}
EXPORT_SYMBOL(hashbin_remove);

void* hashbin_remove_this( hashbin_t* hashbin, irda_queue_t* entry)
{
	unsigned long flags = 0;
	int	bin;
	long	hashv;

	IRDA_DEBUG( 4, "%s()\n", __func__);

	IRDA_ASSERT( hashbin != NULL, return NULL;);
	IRDA_ASSERT( hashbin->magic == HB_MAGIC, return NULL;);
	IRDA_ASSERT( entry != NULL, return NULL;);

	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_lock_irqsave(&hashbin->hb_spinlock, flags);
	} 

	
	if((entry->q_next == NULL) || (entry->q_prev == NULL)) {
		entry = NULL;
		goto out;
	}

	hashv = entry->q_hash;
	bin = GET_HASHBIN( hashv );

	dequeue_general( (irda_queue_t**) &hashbin->hb_queue[ bin ],
			 (irda_queue_t*) entry );
	hashbin->hb_size--;
	entry->q_next = NULL;
	entry->q_prev = NULL;

	if ( entry == hashbin->hb_current)
		hashbin->hb_current = NULL;
out:
	
	if ( hashbin->hb_type & HB_LOCK ) {
		spin_unlock_irqrestore(&hashbin->hb_spinlock, flags);
	} 

	return entry;
}
EXPORT_SYMBOL(hashbin_remove_this);


void* hashbin_find( hashbin_t* hashbin, long hashv, const char* name )
{
	int bin;
	irda_queue_t* entry;

	IRDA_DEBUG( 4, "hashbin_find()\n");

	IRDA_ASSERT( hashbin != NULL, return NULL;);
	IRDA_ASSERT( hashbin->magic == HB_MAGIC, return NULL;);

	if ( name )
		hashv = hash( name );
	bin = GET_HASHBIN( hashv );

	entry = hashbin->hb_queue[ bin];
	if ( entry ) {
		do {
			if ( entry->q_hash == hashv ) {
				if ( name ) {
					if ( strcmp( entry->q_name, name ) == 0 ) {
						return entry;
					}
				} else {
					return entry;
				}
			}
			entry = entry->q_next;
		} while ( entry != hashbin->hb_queue[ bin ] );
	}

	return NULL;
}
EXPORT_SYMBOL(hashbin_find);

void* hashbin_lock_find( hashbin_t* hashbin, long hashv, const char* name )
{
	unsigned long flags = 0;
	irda_queue_t* entry;

	
	spin_lock_irqsave(&hashbin->hb_spinlock, flags);

	entry = hashbin_find(hashbin, hashv, name);

	
	spin_unlock_irqrestore(&hashbin->hb_spinlock, flags);

	return entry;
}
EXPORT_SYMBOL(hashbin_lock_find);

void* hashbin_find_next( hashbin_t* hashbin, long hashv, const char* name,
			 void ** pnext)
{
	unsigned long flags = 0;
	irda_queue_t* entry;

	
	spin_lock_irqsave(&hashbin->hb_spinlock, flags);

	entry = hashbin_find(hashbin, hashv, name);

	if(entry) {
		hashbin->hb_current = entry;
		*pnext = hashbin_get_next( hashbin );
	} else
		*pnext = NULL;

	
	spin_unlock_irqrestore(&hashbin->hb_spinlock, flags);

	return entry;
}

irda_queue_t *hashbin_get_first( hashbin_t* hashbin)
{
	irda_queue_t *entry;
	int i;

	IRDA_ASSERT( hashbin != NULL, return NULL;);
	IRDA_ASSERT( hashbin->magic == HB_MAGIC, return NULL;);

	if ( hashbin == NULL)
		return NULL;

	for ( i = 0; i < HASHBIN_SIZE; i ++ ) {
		entry = hashbin->hb_queue[ i];
		if ( entry) {
			hashbin->hb_current = entry;
			return entry;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(hashbin_get_first);

irda_queue_t *hashbin_get_next( hashbin_t *hashbin)
{
	irda_queue_t* entry;
	int bin;
	int i;

	IRDA_ASSERT( hashbin != NULL, return NULL;);
	IRDA_ASSERT( hashbin->magic == HB_MAGIC, return NULL;);

	if ( hashbin->hb_current == NULL) {
		IRDA_ASSERT( hashbin->hb_current != NULL, return NULL;);
		return NULL;
	}
	entry = hashbin->hb_current->q_next;
	bin = GET_HASHBIN( entry->q_hash);

	if ( entry != hashbin->hb_queue[ bin ]) {
		hashbin->hb_current = entry;

		return entry;
	}

	if ( bin >= HASHBIN_SIZE)
		return NULL;

	bin++;
	for ( i = bin; i < HASHBIN_SIZE; i++ ) {
		entry = hashbin->hb_queue[ i];
		if ( entry) {
			hashbin->hb_current = entry;

			return entry;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(hashbin_get_next);
