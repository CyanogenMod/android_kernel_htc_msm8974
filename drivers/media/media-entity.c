/*
 * Media entity
 *
 * Copyright (C) 2010 Nokia Corporation
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <media/media-entity.h>
#include <media/media-device.h>

int
media_entity_init(struct media_entity *entity, u16 num_pads,
		  struct media_pad *pads, u16 extra_links)
{
	struct media_link *links;
	unsigned int max_links = num_pads + extra_links;
	unsigned int i;

	links = kzalloc(max_links * sizeof(links[0]), GFP_KERNEL);
	if (links == NULL)
		return -ENOMEM;

	entity->group_id = 0;
	entity->max_links = max_links;
	entity->num_links = 0;
	entity->num_backlinks = 0;
	entity->num_pads = num_pads;
	entity->pads = pads;
	entity->links = links;

	for (i = 0; i < num_pads; i++) {
		pads[i].entity = entity;
		pads[i].index = i;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(media_entity_init);

void
media_entity_cleanup(struct media_entity *entity)
{
	kfree(entity->links);
}
EXPORT_SYMBOL_GPL(media_entity_cleanup);


static struct media_entity *
media_entity_other(struct media_entity *entity, struct media_link *link)
{
	if (link->source->entity == entity)
		return link->sink->entity;
	else
		return link->source->entity;
}

static void stack_push(struct media_entity_graph *graph,
		       struct media_entity *entity)
{
	if (graph->top == MEDIA_ENTITY_ENUM_MAX_DEPTH - 1) {
		WARN_ON(1);
		return;
	}
	graph->top++;
	graph->stack[graph->top].link = 0;
	graph->stack[graph->top].entity = entity;
}

static struct media_entity *stack_pop(struct media_entity_graph *graph)
{
	struct media_entity *entity;

	entity = graph->stack[graph->top].entity;
	graph->top--;

	return entity;
}

#define stack_peek(en)	((en)->stack[(en)->top - 1].entity)
#define link_top(en)	((en)->stack[(en)->top].link)
#define stack_top(en)	((en)->stack[(en)->top].entity)

void media_entity_graph_walk_start(struct media_entity_graph *graph,
				   struct media_entity *entity)
{
	graph->top = 0;
	graph->stack[graph->top].entity = NULL;
	stack_push(graph, entity);
}
EXPORT_SYMBOL_GPL(media_entity_graph_walk_start);

struct media_entity *
media_entity_graph_walk_next(struct media_entity_graph *graph)
{
	if (stack_top(graph) == NULL)
		return NULL;

	while (link_top(graph) < stack_top(graph)->num_links) {
		struct media_entity *entity = stack_top(graph);
		struct media_link *link = &entity->links[link_top(graph)];
		struct media_entity *next;

		
		if (!(link->flags & MEDIA_LNK_FL_ENABLED)) {
			link_top(graph)++;
			continue;
		}

		
		next = media_entity_other(entity, link);

		
		if (next == stack_peek(graph)) {
			link_top(graph)++;
			continue;
		}

		
		link_top(graph)++;
		stack_push(graph, next);
	}

	return stack_pop(graph);
}
EXPORT_SYMBOL_GPL(media_entity_graph_walk_next);


void media_entity_pipeline_start(struct media_entity *entity,
				 struct media_pipeline *pipe)
{
	struct media_device *mdev = entity->parent;
	struct media_entity_graph graph;

	mutex_lock(&mdev->graph_mutex);

	media_entity_graph_walk_start(&graph, entity);

	while ((entity = media_entity_graph_walk_next(&graph))) {
		entity->stream_count++;
		WARN_ON(entity->pipe && entity->pipe != pipe);
		entity->pipe = pipe;
	}

	mutex_unlock(&mdev->graph_mutex);
}
EXPORT_SYMBOL_GPL(media_entity_pipeline_start);

void media_entity_pipeline_stop(struct media_entity *entity)
{
	struct media_device *mdev = entity->parent;
	struct media_entity_graph graph;

	mutex_lock(&mdev->graph_mutex);

	media_entity_graph_walk_start(&graph, entity);

	while ((entity = media_entity_graph_walk_next(&graph))) {
		entity->stream_count--;
		if (entity->stream_count == 0)
			entity->pipe = NULL;
	}

	mutex_unlock(&mdev->graph_mutex);
}
EXPORT_SYMBOL_GPL(media_entity_pipeline_stop);


struct media_entity *media_entity_get(struct media_entity *entity)
{
	if (entity == NULL)
		return NULL;

	if (entity->parent->dev &&
	    !try_module_get(entity->parent->dev->driver->owner))
		return NULL;

	return entity;
}
EXPORT_SYMBOL_GPL(media_entity_get);

void media_entity_put(struct media_entity *entity)
{
	if (entity == NULL)
		return;

	if (entity->parent->dev)
		module_put(entity->parent->dev->driver->owner);
}
EXPORT_SYMBOL_GPL(media_entity_put);


static struct media_link *media_entity_add_link(struct media_entity *entity)
{
	if (entity->num_links >= entity->max_links) {
		struct media_link *links = entity->links;
		unsigned int max_links = entity->max_links + 2;
		unsigned int i;

		links = krealloc(links, max_links * sizeof(*links), GFP_KERNEL);
		if (links == NULL)
			return NULL;

		for (i = 0; i < entity->num_links; i++)
			links[i].reverse->reverse = &links[i];

		entity->max_links = max_links;
		entity->links = links;
	}

	return &entity->links[entity->num_links++];
}

int
media_entity_create_link(struct media_entity *source, u16 source_pad,
			 struct media_entity *sink, u16 sink_pad, u32 flags)
{
	struct media_link *link;
	struct media_link *backlink;

	BUG_ON(source == NULL || sink == NULL);
	BUG_ON(source_pad >= source->num_pads);
	BUG_ON(sink_pad >= sink->num_pads);

	link = media_entity_add_link(source);
	if (link == NULL)
		return -ENOMEM;

	link->source = &source->pads[source_pad];
	link->sink = &sink->pads[sink_pad];
	link->flags = flags;

	backlink = media_entity_add_link(sink);
	if (backlink == NULL) {
		source->num_links--;
		return -ENOMEM;
	}

	backlink->source = &source->pads[source_pad];
	backlink->sink = &sink->pads[sink_pad];
	backlink->flags = flags;

	link->reverse = backlink;
	backlink->reverse = link;

	sink->num_backlinks++;

	return 0;
}
EXPORT_SYMBOL_GPL(media_entity_create_link);

static int __media_entity_setup_link_notify(struct media_link *link, u32 flags)
{
	int ret;

	
	ret = media_entity_call(link->source->entity, link_setup,
				link->source, link->sink, flags);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return ret;

	ret = media_entity_call(link->sink->entity, link_setup,
				link->sink, link->source, flags);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		media_entity_call(link->source->entity, link_setup,
				  link->source, link->sink, link->flags);
		return ret;
	}

	link->flags = flags;
	link->reverse->flags = link->flags;

	return 0;
}

int __media_entity_setup_link(struct media_link *link, u32 flags)
{
	const u32 mask = MEDIA_LNK_FL_ENABLED;
	struct media_device *mdev;
	struct media_entity *source, *sink;
	int ret = -EBUSY;

	if (link == NULL)
		return -EINVAL;

	
	if ((link->flags & ~mask) != (flags & ~mask))
		return -EINVAL;

	if (link->flags & MEDIA_LNK_FL_IMMUTABLE)
		return link->flags == flags ? 0 : -EINVAL;

	if (link->flags == flags)
		return 0;

	source = link->source->entity;
	sink = link->sink->entity;

	if (!(link->flags & MEDIA_LNK_FL_DYNAMIC) &&
	    (source->stream_count || sink->stream_count))
		return -EBUSY;

	mdev = source->parent;

	if ((flags & MEDIA_LNK_FL_ENABLED) && mdev->link_notify) {
		ret = mdev->link_notify(link->source, link->sink,
					MEDIA_LNK_FL_ENABLED);
		if (ret < 0)
			return ret;
	}

	ret = __media_entity_setup_link_notify(link, flags);
	if (ret < 0)
		goto err;

	if (!(flags & MEDIA_LNK_FL_ENABLED) && mdev->link_notify)
		mdev->link_notify(link->source, link->sink, 0);

	return 0;

err:
	if ((flags & MEDIA_LNK_FL_ENABLED) && mdev->link_notify)
		mdev->link_notify(link->source, link->sink, 0);

	return ret;
}

int media_entity_setup_link(struct media_link *link, u32 flags)
{
	int ret;

	mutex_lock(&link->source->entity->parent->graph_mutex);
	ret = __media_entity_setup_link(link, flags);
	mutex_unlock(&link->source->entity->parent->graph_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(media_entity_setup_link);

struct media_link *
media_entity_find_link(struct media_pad *source, struct media_pad *sink)
{
	struct media_link *link;
	unsigned int i;

	for (i = 0; i < source->entity->num_links; ++i) {
		link = &source->entity->links[i];

		if (link->source->entity == source->entity &&
		    link->source->index == source->index &&
		    link->sink->entity == sink->entity &&
		    link->sink->index == sink->index)
			return link;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(media_entity_find_link);

struct media_pad *media_entity_remote_source(struct media_pad *pad)
{
	unsigned int i;

	for (i = 0; i < pad->entity->num_links; i++) {
		struct media_link *link = &pad->entity->links[i];

		if (!(link->flags & MEDIA_LNK_FL_ENABLED))
			continue;

		if (link->source == pad)
			return link->sink;

		if (link->sink == pad)
			return link->source;
	}

	return NULL;

}
EXPORT_SYMBOL_GPL(media_entity_remote_source);
