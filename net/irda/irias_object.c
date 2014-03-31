/*********************************************************************
 *
 * Filename:      irias_object.c
 * Version:       0.3
 * Description:   IAS object database and functions
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Thu Oct  1 22:50:04 1998
 * Modified at:   Wed Dec 15 11:23:16 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1998-1999 Dag Brattli, All Rights Reserved.
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

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/module.h>

#include <net/irda/irda.h>
#include <net/irda/irias_object.h>

hashbin_t *irias_objects;

struct ias_value irias_missing = { IAS_MISSING, 0, 0, 0, {0}};


struct ias_object *irias_new_object( char *name, int id)
{
	struct ias_object *obj;

	IRDA_DEBUG( 4, "%s()\n", __func__);

	obj = kzalloc(sizeof(struct ias_object), GFP_ATOMIC);
	if (obj == NULL) {
		IRDA_WARNING("%s(), Unable to allocate object!\n",
			     __func__);
		return NULL;
	}

	obj->magic = IAS_OBJECT_MAGIC;
	obj->name = kstrndup(name, IAS_MAX_CLASSNAME, GFP_ATOMIC);
	if (!obj->name) {
		IRDA_WARNING("%s(), Unable to allocate name!\n",
			     __func__);
		kfree(obj);
		return NULL;
	}
	obj->id = id;

	obj->attribs = hashbin_new(HB_LOCK);

	if (obj->attribs == NULL) {
		IRDA_WARNING("%s(), Unable to allocate attribs!\n",
			     __func__);
		kfree(obj->name);
		kfree(obj);
		return NULL;
	}

	return obj;
}
EXPORT_SYMBOL(irias_new_object);

static void __irias_delete_attrib(struct ias_attrib *attrib)
{
	IRDA_ASSERT(attrib != NULL, return;);
	IRDA_ASSERT(attrib->magic == IAS_ATTRIB_MAGIC, return;);

	kfree(attrib->name);

	irias_delete_value(attrib->value);
	attrib->magic = ~IAS_ATTRIB_MAGIC;

	kfree(attrib);
}

void __irias_delete_object(struct ias_object *obj)
{
	IRDA_ASSERT(obj != NULL, return;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);

	kfree(obj->name);

	hashbin_delete(obj->attribs, (FREE_FUNC) __irias_delete_attrib);

	obj->magic = ~IAS_OBJECT_MAGIC;

	kfree(obj);
}

int irias_delete_object(struct ias_object *obj)
{
	struct ias_object *node;

	IRDA_ASSERT(obj != NULL, return -1;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return -1;);

	
	node = hashbin_remove_this(irias_objects, (irda_queue_t *) obj);
	if (!node)
		IRDA_DEBUG( 0, "%s(), object already removed!\n",
			    __func__);

	
	__irias_delete_object(obj);

	return 0;
}
EXPORT_SYMBOL(irias_delete_object);

int irias_delete_attrib(struct ias_object *obj, struct ias_attrib *attrib,
			int cleanobject)
{
	struct ias_attrib *node;

	IRDA_ASSERT(obj != NULL, return -1;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return -1;);
	IRDA_ASSERT(attrib != NULL, return -1;);

	
	node = hashbin_remove_this(obj->attribs, (irda_queue_t *) attrib);
	if (!node)
		return 0; 

	
	__irias_delete_attrib(node);

	node = (struct ias_attrib *) hashbin_get_first(obj->attribs);
	if (cleanobject && !node)
		irias_delete_object(obj);

	return 0;
}

void irias_insert_object(struct ias_object *obj)
{
	IRDA_ASSERT(obj != NULL, return;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);

	hashbin_insert(irias_objects, (irda_queue_t *) obj, 0, obj->name);
}
EXPORT_SYMBOL(irias_insert_object);

struct ias_object *irias_find_object(char *name)
{
	IRDA_ASSERT(name != NULL, return NULL;);

	
	return hashbin_lock_find(irias_objects, 0, name);
}
EXPORT_SYMBOL(irias_find_object);

struct ias_attrib *irias_find_attrib(struct ias_object *obj, char *name)
{
	struct ias_attrib *attrib;

	IRDA_ASSERT(obj != NULL, return NULL;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return NULL;);
	IRDA_ASSERT(name != NULL, return NULL;);

	attrib = hashbin_lock_find(obj->attribs, 0, name);
	if (attrib == NULL)
		return NULL;

	
	return attrib;
}

static void irias_add_attrib(struct ias_object *obj, struct ias_attrib *attrib,
			     int owner)
{
	IRDA_ASSERT(obj != NULL, return;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);

	IRDA_ASSERT(attrib != NULL, return;);
	IRDA_ASSERT(attrib->magic == IAS_ATTRIB_MAGIC, return;);

	
	attrib->value->owner = owner;

	hashbin_insert(obj->attribs, (irda_queue_t *) attrib, 0, attrib->name);
}

int irias_object_change_attribute(char *obj_name, char *attrib_name,
				  struct ias_value *new_value)
{
	struct ias_object *obj;
	struct ias_attrib *attrib;
	unsigned long flags;

	
	obj = hashbin_lock_find(irias_objects, 0, obj_name);
	if (obj == NULL) {
		IRDA_WARNING("%s: Unable to find object: %s\n", __func__,
			     obj_name);
		return -1;
	}

	
	spin_lock_irqsave(&obj->attribs->hb_spinlock, flags);

	
	attrib = hashbin_find(obj->attribs, 0, attrib_name);
	if (attrib == NULL) {
		IRDA_WARNING("%s: Unable to find attribute: %s\n",
			     __func__, attrib_name);
		spin_unlock_irqrestore(&obj->attribs->hb_spinlock, flags);
		return -1;
	}

	if ( attrib->value->type != new_value->type) {
		IRDA_DEBUG( 0, "%s(), changing value type not allowed!\n",
			    __func__);
		spin_unlock_irqrestore(&obj->attribs->hb_spinlock, flags);
		return -1;
	}

	
	irias_delete_value(attrib->value);

	
	attrib->value = new_value;

	
	spin_unlock_irqrestore(&obj->attribs->hb_spinlock, flags);
	return 0;
}
EXPORT_SYMBOL(irias_object_change_attribute);

void irias_add_integer_attrib(struct ias_object *obj, char *name, int value,
			      int owner)
{
	struct ias_attrib *attrib;

	IRDA_ASSERT(obj != NULL, return;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);
	IRDA_ASSERT(name != NULL, return;);

	attrib = kzalloc(sizeof(struct ias_attrib), GFP_ATOMIC);
	if (attrib == NULL) {
		IRDA_WARNING("%s: Unable to allocate attribute!\n",
			     __func__);
		return;
	}

	attrib->magic = IAS_ATTRIB_MAGIC;
	attrib->name = kstrndup(name, IAS_MAX_ATTRIBNAME, GFP_ATOMIC);

	
	attrib->value = irias_new_integer_value(value);
	if (!attrib->name || !attrib->value) {
		IRDA_WARNING("%s: Unable to allocate attribute!\n",
			     __func__);
		if (attrib->value)
			irias_delete_value(attrib->value);
		kfree(attrib->name);
		kfree(attrib);
		return;
	}

	irias_add_attrib(obj, attrib, owner);
}
EXPORT_SYMBOL(irias_add_integer_attrib);


void irias_add_octseq_attrib(struct ias_object *obj, char *name, __u8 *octets,
			     int len, int owner)
{
	struct ias_attrib *attrib;

	IRDA_ASSERT(obj != NULL, return;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);

	IRDA_ASSERT(name != NULL, return;);
	IRDA_ASSERT(octets != NULL, return;);

	attrib = kzalloc(sizeof(struct ias_attrib), GFP_ATOMIC);
	if (attrib == NULL) {
		IRDA_WARNING("%s: Unable to allocate attribute!\n",
			     __func__);
		return;
	}

	attrib->magic = IAS_ATTRIB_MAGIC;
	attrib->name = kstrndup(name, IAS_MAX_ATTRIBNAME, GFP_ATOMIC);

	attrib->value = irias_new_octseq_value( octets, len);
	if (!attrib->name || !attrib->value) {
		IRDA_WARNING("%s: Unable to allocate attribute!\n",
			     __func__);
		if (attrib->value)
			irias_delete_value(attrib->value);
		kfree(attrib->name);
		kfree(attrib);
		return;
	}

	irias_add_attrib(obj, attrib, owner);
}
EXPORT_SYMBOL(irias_add_octseq_attrib);

void irias_add_string_attrib(struct ias_object *obj, char *name, char *value,
			     int owner)
{
	struct ias_attrib *attrib;

	IRDA_ASSERT(obj != NULL, return;);
	IRDA_ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);

	IRDA_ASSERT(name != NULL, return;);
	IRDA_ASSERT(value != NULL, return;);

	attrib = kzalloc(sizeof( struct ias_attrib), GFP_ATOMIC);
	if (attrib == NULL) {
		IRDA_WARNING("%s: Unable to allocate attribute!\n",
			     __func__);
		return;
	}

	attrib->magic = IAS_ATTRIB_MAGIC;
	attrib->name = kstrndup(name, IAS_MAX_ATTRIBNAME, GFP_ATOMIC);

	attrib->value = irias_new_string_value(value);
	if (!attrib->name || !attrib->value) {
		IRDA_WARNING("%s: Unable to allocate attribute!\n",
			     __func__);
		if (attrib->value)
			irias_delete_value(attrib->value);
		kfree(attrib->name);
		kfree(attrib);
		return;
	}

	irias_add_attrib(obj, attrib, owner);
}
EXPORT_SYMBOL(irias_add_string_attrib);

struct ias_value *irias_new_integer_value(int integer)
{
	struct ias_value *value;

	value = kzalloc(sizeof(struct ias_value), GFP_ATOMIC);
	if (value == NULL) {
		IRDA_WARNING("%s: Unable to kmalloc!\n", __func__);
		return NULL;
	}

	value->type = IAS_INTEGER;
	value->len = 4;
	value->t.integer = integer;

	return value;
}
EXPORT_SYMBOL(irias_new_integer_value);

struct ias_value *irias_new_string_value(char *string)
{
	struct ias_value *value;

	value = kzalloc(sizeof(struct ias_value), GFP_ATOMIC);
	if (value == NULL) {
		IRDA_WARNING("%s: Unable to kmalloc!\n", __func__);
		return NULL;
	}

	value->type = IAS_STRING;
	value->charset = CS_ASCII;
	value->t.string = kstrndup(string, IAS_MAX_STRING, GFP_ATOMIC);
	if (!value->t.string) {
		IRDA_WARNING("%s: Unable to kmalloc!\n", __func__);
		kfree(value);
		return NULL;
	}

	value->len = strlen(value->t.string);

	return value;
}

struct ias_value *irias_new_octseq_value(__u8 *octseq , int len)
{
	struct ias_value *value;

	value = kzalloc(sizeof(struct ias_value), GFP_ATOMIC);
	if (value == NULL) {
		IRDA_WARNING("%s: Unable to kmalloc!\n", __func__);
		return NULL;
	}

	value->type = IAS_OCT_SEQ;
	
	if(len > IAS_MAX_OCTET_STRING)
		len = IAS_MAX_OCTET_STRING;
	value->len = len;

	value->t.oct_seq = kmemdup(octseq, len, GFP_ATOMIC);
	if (value->t.oct_seq == NULL){
		IRDA_WARNING("%s: Unable to kmalloc!\n", __func__);
		kfree(value);
		return NULL;
	}
	return value;
}

struct ias_value *irias_new_missing_value(void)
{
	struct ias_value *value;

	value = kzalloc(sizeof(struct ias_value), GFP_ATOMIC);
	if (value == NULL) {
		IRDA_WARNING("%s: Unable to kmalloc!\n", __func__);
		return NULL;
	}

	value->type = IAS_MISSING;

	return value;
}

void irias_delete_value(struct ias_value *value)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(value != NULL, return;);

	switch (value->type) {
	case IAS_INTEGER: 
	case IAS_MISSING:
		
		break;
	case IAS_STRING:
		
		kfree(value->t.string);
		break;
	case IAS_OCT_SEQ:
		
		 kfree(value->t.oct_seq);
		 break;
	default:
		IRDA_DEBUG(0, "%s(), Unknown value type!\n", __func__);
		break;
	}
	kfree(value);
}
EXPORT_SYMBOL(irias_delete_value);
