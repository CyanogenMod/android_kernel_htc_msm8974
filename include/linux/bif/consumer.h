/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_BIF_CONSUMER_H_
#define _LINUX_BIF_CONSUMER_H_

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/notifier.h>

#define BIF_DEVICE_ID_BYTE_LENGTH	8
#define BIF_UNIQUE_ID_BYTE_LENGTH	10
#define BIF_UNIQUE_ID_BIT_LENGTH	80

#define BIF_PRIMARY_SLAVE_DEV_ADR	0x01

enum bif_transaction {
	BIF_TRANS_WD	= 0x00,
	BIF_TRANS_ERA	= 0x01,
	BIF_TRANS_WRA	= 0x02,
	BIF_TRANS_RRA	= 0x03,
	BIF_TRANS_BC	= 0x04,
	BIF_TRANS_EDA	= 0x05,
	BIF_TRANS_SDA	= 0x06,
};

#define BIF_SLAVE_RD_ACK		0x200
#define BIF_SLAVE_RD_EOT		0x100
#define BIF_SLAVE_RD_DATA		0x0FF
#define BIF_SLAVE_RD_ERR		0x0FF
#define BIF_SLAVE_TACK_ACK		0x200
#define BIF_SLAVE_TACK_WCNT		0x0FF
#define BIF_SLAVE_TACK_ERR		0x0FF

enum bif_bus_command {
	BIF_CMD_BRES	= 0x00,
	BIF_CMD_PDWN	= 0x02,
	BIF_CMD_STBY	= 0x03,
	BIF_CMD_EINT	= 0x10,
	BIF_CMD_ISTS	= 0x11,
	BIF_CMD_RBL	= 0x20,
	BIF_CMD_RBE	= 0x30,
	BIF_CMD_DASM	= 0x40,
	BIF_CMD_DISS	= 0x80,
	BIF_CMD_DILC	= 0x81,
	BIF_CMD_DIE0	= 0x84,
	BIF_CMD_DIE1	= 0x85,
	BIF_CMD_DIP0	= 0x86,
	BIF_CMD_DIP1	= 0x87,
	BIF_CMD_DRES	= 0xC0,
	BIF_CMD_TQ	= 0xC2,
	BIF_CMD_AIO	= 0xC4,
};

struct bif_ddb_l1_data {
	u8	revision;
	u8	level;
	u16	device_class;
	u16	manufacturer_id;
	u16	product_id;
	u16	length;
};

struct bif_ddb_l2_data {
	u8	function_type;
	u8	function_version;
	u16	function_pointer;
};

enum bif_mipi_function_type {
	BIF_FUNC_PROTOCOL	= 0x01,
	BIF_FUNC_SLAVE_CONTROL	= 0x02,
	BIF_FUNC_TEMPERATURE	= 0x03,
	BIF_FUNC_NVM		= 0x04,
	BIF_FUNC_AUTHENTICATION	= 0x05,
};

#define BIF_DDB_L1_BASE_ADDR	0x0000
#define BIF_DDB_L2_BASE_ADDR	0x000A

enum bif_slave_error_code {
	BIF_ERR_NONE		= 0x00,
	BIF_ERR_GENERAL		= 0x10,
	BIF_ERR_PARITY		= 0x11,
	BIF_ERR_INVERSION	= 0x12,
	BIF_ERR_BAD_LENGTH	= 0x13,
	BIF_ERR_TIMING		= 0x14,
	BIF_ERR_UNKNOWN_CMD	= 0x15,
	BIF_ERR_CMD_SEQ		= 0x16,
	BIF_ERR_BUS_COLLISION	= 0x1F,
	BIF_ERR_SLAVE_BUSY	= 0x20,
	BIF_ERR_FATAL		= 0x7F,
};

struct bif_protocol_function {
	struct bif_ddb_l2_data *l2_entry;
	u16	protocol_pointer;
	u16	device_id_pointer;
	u8	device_id[BIF_DEVICE_ID_BYTE_LENGTH]; 
};

#define PROTOCOL_FUNC_DEV_ADR_ADDR(protocol_pointer)	((protocol_pointer) + 0)
#define PROTOCOL_FUNC_ERR_CODE_ADDR(protocol_pointer)	((protocol_pointer) + 2)
#define PROTOCOL_FUNC_ERR_CNT_ADDR(protocol_pointer)	((protocol_pointer) + 3)
#define PROTOCOL_FUNC_WORD_CNT_ADDR(protocol_pointer)	((protocol_pointer) + 4)

struct bif_slave_control_function {
	struct bif_ddb_l2_data		*l2_entry;
	u16				slave_ctrl_pointer;
	unsigned int			task_count;
	struct blocking_notifier_head	*irq_notifier_list;
};

#define SLAVE_CTRL_TASKS_PER_SET	8

static inline bool
bif_slave_control_task_is_valid(struct bif_slave_control_function *func,
				unsigned int task)
{
	return func ? task < func->task_count : false;
}

#define SLAVE_CTRL_FUNC_IRQ_EN_ADDR(slave_ctrl_pointer, task) \
	((slave_ctrl_pointer) + 4 * ((task) / SLAVE_CTRL_TASKS_PER_SET) + 0)

#define SLAVE_CTRL_FUNC_IRQ_STATUS_ADDR(slave_ctrl_pointer, task) \
	((slave_ctrl_pointer) + 4 * ((task) / SLAVE_CTRL_TASKS_PER_SET) + 1)
#define SLAVE_CTRL_FUNC_IRQ_CLEAR_ADDR(slave_ctrl_pointer, task) \
	SLAVE_CTRL_FUNC_IRQ_STATUS_ADDR(slave_ctrl_pointer, task)

#define SLAVE_CTRL_FUNC_TASK_TRIGGER_ADDR(slave_ctrl_pointer, task) \
	((slave_ctrl_pointer) + 4 * ((task) / SLAVE_CTRL_TASKS_PER_SET) + 2)
#define SLAVE_CTRL_FUNC_TASK_BUSY_ADDR(slave_ctrl_pointer, task) \
	SLAVE_CTRL_FUNC_TASK_TRIGGER_ADDR(slave_ctrl_pointer, task)

#define SLAVE_CTRL_FUNC_TASK_AUTO_TRIGGER_ADDR(slave_ctrl_pointer, task) \
	((slave_ctrl_pointer) + 4 * ((task) / SLAVE_CTRL_TASKS_PER_SET) + 3)

struct bif_temperature_function {
	u16	temperature_pointer;
	u8	slave_control_channel;
	u16	accuracy_pointer;
};

enum bif_mipi_object_type {
	BIF_OBJ_END_OF_LIST	= 0x00,
	BIF_OBJ_SEC_SLAVE	= 0x01,
	BIF_OBJ_BATT_PARAM	= 0x02,
};

struct bif_object {
	u8			type;
	u8			version;
	u16			manufacturer_id;
	u16			length;
	u8			*data;
	u16			crc;
	struct list_head	list;
	u16			addr;
};

struct bif_nvm_function {
	u16			nvm_pointer;
	u8			slave_control_channel;
	u8			write_buffer_size;
	u16			nvm_base_address;
	u16			nvm_size;
	u16			nvm_lock_offset;
	int			object_count;
	struct list_head	object_list;
};

struct bif_ctrl;

struct bif_slave;

enum bif_bus_state {
	BIF_BUS_STATE_MASTER_DISABLED,
	BIF_BUS_STATE_POWER_DOWN,
	BIF_BUS_STATE_STANDBY,
	BIF_BUS_STATE_ACTIVE,
	BIF_BUS_STATE_INTERRUPT,
};

enum bif_bus_event {
	BIF_BUS_EVENT_BATTERY_INSERTED,
	BIF_BUS_EVENT_BATTERY_REMOVED,
};

#define BIF_MATCH_MANUFACTURER_ID	BIT(0)
#define BIF_MATCH_PRODUCT_ID		BIT(1)
#define BIF_MATCH_FUNCTION_TYPE		BIT(2)
#define BIF_MATCH_FUNCTION_VERSION	BIT(3)
#define BIF_MATCH_IGNORE_PRESENCE	BIT(4)
#define BIF_MATCH_OBJ_TYPE		BIT(5)
#define BIF_MATCH_OBJ_VERSION		BIT(6)
#define BIF_MATCH_OBJ_MANUFACTURER_ID	BIT(7)

struct bif_match_criteria {
	u32	match_mask;
	u16	manufacturer_id;
	u16	product_id;
	u8	function_type;
	u8	function_version;
	bool	ignore_presence;
	u8	obj_type;
	u8	obj_version;
	u16	obj_manufacturer_id;
};

#define BIF_OBJ_MATCH_TYPE		BIT(0)
#define BIF_OBJ_MATCH_VERSION		BIT(1)
#define BIF_OBJ_MATCH_MANUFACTURER_ID	BIT(2)

struct bif_obj_match_criteria {
	u32	match_mask;
	u8	type;
	u8	version;
	u16	manufacturer_id;
};

enum bif_battery_rid_ranges {
	BIF_BATT_RID_SPECIAL1_MIN	= 0,
	BIF_BATT_RID_SPECIAL1_MAX	= 1,
	BIF_BATT_RID_SPECIAL2_MIN	= 7350,
	BIF_BATT_RID_SPECIAL2_MAX	= 7650,
	BIF_BATT_RID_SPECIAL3_MIN	= 12740,
	BIF_BATT_RID_SPECIAL3_MAX	= 13260,
	BIF_BATT_RID_LOW_COST_MIN	= 19600,
	BIF_BATT_RID_LOW_COST_MAX	= 140000,
	BIF_BATT_RID_SMART_MIN		= 240000,
	BIF_BATT_RID_SMART_MAX		= 450000,
};

#ifdef CONFIG_BIF

int bif_request_irq(struct bif_slave *slave, unsigned int task,
			struct notifier_block *nb);
int bif_free_irq(struct bif_slave *slave, unsigned int task,
			struct notifier_block *nb);

int bif_trigger_task(struct bif_slave *slave, unsigned int task);
int bif_enable_auto_task(struct bif_slave *slave, unsigned int task);
int bif_disable_auto_task(struct bif_slave *slave, unsigned int task);
int bif_task_is_busy(struct bif_slave *slave, unsigned int task);

int bif_ctrl_count(void);
struct bif_ctrl *bif_ctrl_get_by_id(unsigned int id);
struct bif_ctrl *bif_ctrl_get(struct device *consumer_dev);
void bif_ctrl_put(struct bif_ctrl *ctrl);

int bif_ctrl_signal_battery_changed(struct bif_ctrl *ctrl);

int bif_slave_match_count(struct bif_ctrl *ctrl,
			const struct bif_match_criteria *match_criteria);

struct bif_slave *bif_slave_match_get(struct bif_ctrl *ctrl,
	unsigned int id, const struct bif_match_criteria *match_criteria);

void bif_slave_put(struct bif_slave *slave);

int bif_ctrl_notifier_register(struct bif_ctrl *ctrl,
				struct notifier_block *nb);

int bif_ctrl_notifier_unregister(struct bif_ctrl *ctrl,
				struct notifier_block *nb);

struct bif_ctrl *bif_get_ctrl_handle(struct bif_slave *slave);

int bif_slave_find_function(struct bif_slave *slave, u8 function, u8 *version,
				u16 *function_pointer);

int bif_object_match_count(struct bif_slave *slave,
			const struct bif_obj_match_criteria *match_criteria);

struct bif_object *bif_object_match_get(struct bif_slave *slave,
	unsigned int id, const struct bif_obj_match_criteria *match_criteria);

void bif_object_put(struct bif_object *object);

int bif_slave_read(struct bif_slave *slave, u16 addr, u8 *buf, int len);
int bif_slave_write(struct bif_slave *slave, u16 addr, u8 *buf, int len);

int bif_slave_nvm_raw_read(struct bif_slave *slave, u16 offset, u8 *buf,
				int len);
int bif_slave_nvm_raw_write(struct bif_slave *slave, u16 offset, u8 *buf,
				int len);

int bif_object_write(struct bif_slave *slave, u8 type, u8 version, u16
			manufacturer_id, const u8 *data, int data_len);

int bif_object_overwrite(struct bif_slave *slave,
	struct bif_object *object, u8 type, u8 version,
	u16 manufacturer_id, const u8 *data, int data_len);

int bif_object_delete(struct bif_slave *slave, const struct bif_object *object);

int bif_slave_is_present(struct bif_slave *slave);

int bif_slave_is_selected(struct bif_slave *slave);
int bif_slave_select(struct bif_slave *slave);

int bif_ctrl_raw_transaction(struct bif_ctrl *ctrl, int transaction, u8 data);
int bif_ctrl_raw_transaction_read(struct bif_ctrl *ctrl, int transaction,
					u8 data, int *response);
int bif_ctrl_raw_transaction_query(struct bif_ctrl *ctrl, int transaction,
		u8 data, bool *query_response);

void bif_ctrl_bus_lock(struct bif_ctrl *ctrl);
void bif_ctrl_bus_unlock(struct bif_ctrl *ctrl);

u16 bif_crc_ccitt(const u8 *buffer, unsigned int len);

int bif_ctrl_measure_rid(struct bif_ctrl *ctrl);
int bif_ctrl_get_bus_period(struct bif_ctrl *ctrl);
int bif_ctrl_set_bus_period(struct bif_ctrl *ctrl, int period_ns);
int bif_ctrl_get_bus_state(struct bif_ctrl *ctrl);
int bif_ctrl_set_bus_state(struct bif_ctrl *ctrl, enum bif_bus_state state);

#else

static inline int bif_request_irq(struct bif_slave *slave, unsigned int task,
			struct notifier_block *nb) { return -EPERM; }
static inline int bif_free_irq(struct bif_slave *slave, unsigned int task,
			struct notifier_block *nb) { return -EPERM; }

static inline int bif_trigger_task(struct bif_slave *slave, unsigned int task)
{ return -EPERM; }
static inline int bif_enable_auto_task(struct bif_slave *slave,
			unsigned int task)
{ return -EPERM; }
static inline int bif_disable_auto_task(struct bif_slave *slave,
			unsigned int task)
{ return -EPERM; }
static inline int bif_task_is_busy(struct bif_slave *slave, unsigned int task)
{ return -EPERM; }

static inline int bif_ctrl_count(void) { return -EPERM; }
static inline struct bif_ctrl *bif_ctrl_get_by_id(unsigned int id)
{ return ERR_PTR(-EPERM); }
struct bif_ctrl *bif_ctrl_get(struct device *consumer_dev)
{ return ERR_PTR(-EPERM); }
static inline void bif_ctrl_put(struct bif_ctrl *ctrl) { return; }

static inline int bif_ctrl_signal_battery_changed(struct bif_ctrl *ctrl)
{ return -EPERM; }

static inline int bif_slave_match_count(struct bif_ctrl *ctrl,
			const struct bif_match_criteria *match_criteria)
{ return -EPERM; }

static inline struct bif_slave *bif_slave_match_get(struct bif_ctrl *ctrl,
	unsigned int id, const struct bif_match_criteria *match_criteria)
{ return ERR_PTR(-EPERM); }

static inline void bif_slave_put(struct bif_slave *slave) { return; }

static inline int bif_ctrl_notifier_register(struct bif_ctrl *ctrl,
				struct notifier_block *nb)
{ return -EPERM; }

static inline int bif_ctrl_notifier_unregister(struct bif_ctrl *ctrl,
				struct notifier_block *nb)
{ return -EPERM; }

static inline struct bif_ctrl *bif_get_ctrl_handle(struct bif_slave *slave)
{ return ERR_PTR(-EPERM); }

static inline int bif_slave_find_function(struct bif_slave *slave, u8 function,
				u8 *version, u16 *function_pointer)
{ return -EPERM; }

static inline int bif_object_match_count(struct bif_slave *slave,
			const struct bif_obj_match_criteria *match_criteria)
{ return -EPERM; }

static inline struct bif_object *bif_object_match_get(struct bif_slave *slave,
	unsigned int id, const struct bif_obj_match_criteria *match_criteria)
{ return ERR_PTR(-EPERM); }

static inline void bif_object_put(struct bif_object *object)
{}

static inline int bif_slave_read(struct bif_slave *slave, u16 addr, u8 *buf,
				int len)
{ return -EPERM; }
static inline int bif_slave_write(struct bif_slave *slave, u16 addr, u8 *buf,
				int len)
{ return -EPERM; }

static inline int bif_slave_nvm_raw_read(struct bif_slave *slave, u16 offset,
				u8 *buf, int len)
{ return -EPERM; }
static inline int bif_slave_nvm_raw_write(struct bif_slave *slave, u16 offset,
				u8 *buf, int len)
{ return -EPERM; }

static inline int bif_object_write(struct bif_slave *slave, u8 type, u8 version,
			u16 manufacturer_id, const u8 *data, int data_len)
{ return -EPERM; }

static inline int bif_object_overwrite(struct bif_slave *slave,
	struct bif_object *object, u8 type, u8 version,
	u16 manufacturer_id, const u8 *data, int data_len)
{ return -EPERM; }

static inline int bif_object_delete(struct bif_slave *slave,
		const struct bif_object *object)
{ return -EPERM; }

static inline int bif_slave_is_present(struct bif_slave *slave)
{ return -EPERM; }

static inline int bif_slave_is_selected(struct bif_slave *slave)
{ return -EPERM; }
static inline int bif_slave_select(struct bif_slave *slave)
{ return -EPERM; }

static inline int bif_ctrl_raw_transaction(struct bif_ctrl *ctrl,
				int transaction, u8 data)
{ return -EPERM; }
static inline int bif_ctrl_raw_transaction_read(struct bif_ctrl *ctrl,
				int transaction, u8 data, int *response)
{ return -EPERM; }
static inline int bif_ctrl_raw_transaction_query(struct bif_ctrl *ctrl,
				int transaction, u8 data, bool *query_response)
{ return -EPERM; }

static inline void bif_ctrl_bus_lock(struct bif_ctrl *ctrl)
{ return -EPERM; }
static inline void bif_ctrl_bus_unlock(struct bif_ctrl *ctrl)
{ return -EPERM; }

static inline u16 bif_crc_ccitt(const u8 *buffer, unsigned int len)
{ return 0; }

static inline int bif_ctrl_measure_rid(struct bif_ctrl *ctrl) { return -EPERM; }
static inline int bif_ctrl_get_bus_period(struct bif_ctrl *ctrl)
{ return -EPERM; }
static inline int bif_ctrl_set_bus_period(struct bif_ctrl *ctrl, int period_ns)
{ return -EPERM; }
static inline int bif_ctrl_get_bus_state(struct bif_ctrl *ctrl)
{ return -EPERM; }
static inline int bif_ctrl_set_bus_state(struct bif_ctrl *ctrl,
				enum bif_bus_state state)
{ return -EPERM; }

#endif

#endif
