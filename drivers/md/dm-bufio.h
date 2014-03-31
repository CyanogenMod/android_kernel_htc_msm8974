/*
 * Copyright (C) 2009-2011 Red Hat, Inc.
 *
 * Author: Mikulas Patocka <mpatocka@redhat.com>
 *
 * This file is released under the GPL.
 */

#ifndef DM_BUFIO_H
#define DM_BUFIO_H

#include <linux/blkdev.h>
#include <linux/types.h>


struct dm_bufio_client;
struct dm_buffer;

struct dm_bufio_client *
dm_bufio_client_create(struct block_device *bdev, unsigned block_size,
		       unsigned reserved_buffers, unsigned aux_size,
		       void (*alloc_callback)(struct dm_buffer *),
		       void (*write_callback)(struct dm_buffer *));

void dm_bufio_client_destroy(struct dm_bufio_client *c);


void *dm_bufio_read(struct dm_bufio_client *c, sector_t block,
		    struct dm_buffer **bp);

void *dm_bufio_get(struct dm_bufio_client *c, sector_t block,
		   struct dm_buffer **bp);

void *dm_bufio_new(struct dm_bufio_client *c, sector_t block,
		   struct dm_buffer **bp);

void dm_bufio_prefetch(struct dm_bufio_client *c,
		       sector_t block, unsigned n_blocks);

void dm_bufio_release(struct dm_buffer *b);

/*
 * Mark a buffer dirty. It should be called after the buffer is modified.
 *
 * In case of memory pressure, the buffer may be written after
 * dm_bufio_mark_buffer_dirty, but before dm_bufio_write_dirty_buffers.  So
 * dm_bufio_write_dirty_buffers guarantees that the buffer is on-disk but
 * the actual writing may occur earlier.
 */
void dm_bufio_mark_buffer_dirty(struct dm_buffer *b);

void dm_bufio_write_dirty_buffers_async(struct dm_bufio_client *c);

int dm_bufio_write_dirty_buffers(struct dm_bufio_client *c);

int dm_bufio_issue_flush(struct dm_bufio_client *c);

void dm_bufio_release_move(struct dm_buffer *b, sector_t new_block);

unsigned dm_bufio_get_block_size(struct dm_bufio_client *c);
sector_t dm_bufio_get_device_size(struct dm_bufio_client *c);
sector_t dm_bufio_get_block_number(struct dm_buffer *b);
void *dm_bufio_get_block_data(struct dm_buffer *b);
void *dm_bufio_get_aux_data(struct dm_buffer *b);
struct dm_bufio_client *dm_bufio_get_client(struct dm_buffer *b);


#endif
