#ifndef SOUND_FIREWIRE_CMP_H_INCLUDED
#define SOUND_FIREWIRE_CMP_H_INCLUDED

#include <linux/mutex.h>
#include <linux/types.h>
#include "iso-resources.h"

struct fw_unit;

struct cmp_connection {
	int speed;
	
	bool connected;
	struct mutex mutex;
	struct fw_iso_resources resources;
	__be32 last_pcr_value;
	unsigned int pcr_index;
	unsigned int max_speed;
};

int cmp_connection_init(struct cmp_connection *connection,
			struct fw_unit *unit,
			unsigned int ipcr_index);
void cmp_connection_destroy(struct cmp_connection *connection);

int cmp_connection_establish(struct cmp_connection *connection,
			     unsigned int max_payload);
int cmp_connection_update(struct cmp_connection *connection);
void cmp_connection_break(struct cmp_connection *connection);

#endif
