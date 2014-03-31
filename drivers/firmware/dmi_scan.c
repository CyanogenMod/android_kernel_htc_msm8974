#include <linux/types.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/dmi.h>
#include <linux/efi.h>
#include <linux/bootmem.h>
#include <asm/dmi.h>

static char dmi_empty_string[] = "        ";

static int dmi_initialized;

static const char * __init dmi_string_nosave(const struct dmi_header *dm, u8 s)
{
	const u8 *bp = ((u8 *) dm) + dm->length;

	if (s) {
		s--;
		while (s > 0 && *bp) {
			bp += strlen(bp) + 1;
			s--;
		}

		if (*bp != 0) {
			size_t len = strlen(bp)+1;
			size_t cmp_len = len > 8 ? 8 : len;

			if (!memcmp(bp, dmi_empty_string, cmp_len))
				return dmi_empty_string;
			return bp;
		}
	}

	return "";
}

static char * __init dmi_string(const struct dmi_header *dm, u8 s)
{
	const char *bp = dmi_string_nosave(dm, s);
	char *str;
	size_t len;

	if (bp == dmi_empty_string)
		return dmi_empty_string;

	len = strlen(bp) + 1;
	str = dmi_alloc(len);
	if (str != NULL)
		strcpy(str, bp);
	else
		printk(KERN_ERR "dmi_string: cannot allocate %Zu bytes.\n", len);

	return str;
}

static void dmi_table(u8 *buf, int len, int num,
		      void (*decode)(const struct dmi_header *, void *),
		      void *private_data)
{
	u8 *data = buf;
	int i = 0;

	while ((i < num) && (data - buf + sizeof(struct dmi_header)) <= len) {
		const struct dmi_header *dm = (const struct dmi_header *)data;

		data += dm->length;
		while ((data - buf < len - 1) && (data[0] || data[1]))
			data++;
		if (data - buf < len - 1)
			decode(dm, private_data);
		data += 2;
		i++;
	}
}

static u32 dmi_base;
static u16 dmi_len;
static u16 dmi_num;

static int __init dmi_walk_early(void (*decode)(const struct dmi_header *,
		void *))
{
	u8 *buf;

	buf = dmi_ioremap(dmi_base, dmi_len);
	if (buf == NULL)
		return -1;

	dmi_table(buf, dmi_len, dmi_num, decode, NULL);

	dmi_iounmap(buf, dmi_len);
	return 0;
}

static int __init dmi_checksum(const u8 *buf)
{
	u8 sum = 0;
	int a;

	for (a = 0; a < 15; a++)
		sum += buf[a];

	return sum == 0;
}

static char *dmi_ident[DMI_STRING_MAX];
static LIST_HEAD(dmi_devices);
int dmi_available;

static void __init dmi_save_ident(const struct dmi_header *dm, int slot, int string)
{
	const char *d = (const char*) dm;
	char *p;

	if (dmi_ident[slot])
		return;

	p = dmi_string(dm, d[string]);
	if (p == NULL)
		return;

	dmi_ident[slot] = p;
}

static void __init dmi_save_uuid(const struct dmi_header *dm, int slot, int index)
{
	const u8 *d = (u8*) dm + index;
	char *s;
	int is_ff = 1, is_00 = 1, i;

	if (dmi_ident[slot])
		return;

	for (i = 0; i < 16 && (is_ff || is_00); i++) {
		if(d[i] != 0x00) is_ff = 0;
		if(d[i] != 0xFF) is_00 = 0;
	}

	if (is_ff || is_00)
		return;

	s = dmi_alloc(16*2+4+1);
	if (!s)
		return;

	sprintf(s, "%pUB", d);

        dmi_ident[slot] = s;
}

static void __init dmi_save_type(const struct dmi_header *dm, int slot, int index)
{
	const u8 *d = (u8*) dm + index;
	char *s;

	if (dmi_ident[slot])
		return;

	s = dmi_alloc(4);
	if (!s)
		return;

	sprintf(s, "%u", *d & 0x7F);
	dmi_ident[slot] = s;
}

static void __init dmi_save_one_device(int type, const char *name)
{
	struct dmi_device *dev;

	
	if (dmi_find_device(type, name, NULL))
		return;

	dev = dmi_alloc(sizeof(*dev) + strlen(name) + 1);
	if (!dev) {
		printk(KERN_ERR "dmi_save_one_device: out of memory.\n");
		return;
	}

	dev->type = type;
	strcpy((char *)(dev + 1), name);
	dev->name = (char *)(dev + 1);
	dev->device_data = NULL;
	list_add(&dev->list, &dmi_devices);
}

static void __init dmi_save_devices(const struct dmi_header *dm)
{
	int i, count = (dm->length - sizeof(struct dmi_header)) / 2;

	for (i = 0; i < count; i++) {
		const char *d = (char *)(dm + 1) + (i * 2);

		
		if ((*d & 0x80) == 0)
			continue;

		dmi_save_one_device(*d & 0x7f, dmi_string_nosave(dm, *(d + 1)));
	}
}

static void __init dmi_save_oem_strings_devices(const struct dmi_header *dm)
{
	int i, count = *(u8 *)(dm + 1);
	struct dmi_device *dev;

	for (i = 1; i <= count; i++) {
		char *devname = dmi_string(dm, i);

		if (devname == dmi_empty_string)
			continue;

		dev = dmi_alloc(sizeof(*dev));
		if (!dev) {
			printk(KERN_ERR
			   "dmi_save_oem_strings_devices: out of memory.\n");
			break;
		}

		dev->type = DMI_DEV_TYPE_OEM_STRING;
		dev->name = devname;
		dev->device_data = NULL;

		list_add(&dev->list, &dmi_devices);
	}
}

static void __init dmi_save_ipmi_device(const struct dmi_header *dm)
{
	struct dmi_device *dev;
	void * data;

	data = dmi_alloc(dm->length);
	if (data == NULL) {
		printk(KERN_ERR "dmi_save_ipmi_device: out of memory.\n");
		return;
	}

	memcpy(data, dm, dm->length);

	dev = dmi_alloc(sizeof(*dev));
	if (!dev) {
		printk(KERN_ERR "dmi_save_ipmi_device: out of memory.\n");
		return;
	}

	dev->type = DMI_DEV_TYPE_IPMI;
	dev->name = "IPMI controller";
	dev->device_data = data;

	list_add_tail(&dev->list, &dmi_devices);
}

static void __init dmi_save_dev_onboard(int instance, int segment, int bus,
					int devfn, const char *name)
{
	struct dmi_dev_onboard *onboard_dev;

	onboard_dev = dmi_alloc(sizeof(*onboard_dev) + strlen(name) + 1);
	if (!onboard_dev) {
		printk(KERN_ERR "dmi_save_dev_onboard: out of memory.\n");
		return;
	}
	onboard_dev->instance = instance;
	onboard_dev->segment = segment;
	onboard_dev->bus = bus;
	onboard_dev->devfn = devfn;

	strcpy((char *)&onboard_dev[1], name);
	onboard_dev->dev.type = DMI_DEV_TYPE_DEV_ONBOARD;
	onboard_dev->dev.name = (char *)&onboard_dev[1];
	onboard_dev->dev.device_data = onboard_dev;

	list_add(&onboard_dev->dev.list, &dmi_devices);
}

static void __init dmi_save_extended_devices(const struct dmi_header *dm)
{
	const u8 *d = (u8*) dm + 5;

	
	if ((*d & 0x80) == 0)
		return;

	dmi_save_dev_onboard(*(d+1), *(u16 *)(d+2), *(d+4), *(d+5),
			     dmi_string_nosave(dm, *(d-1)));
	dmi_save_one_device(*d & 0x7f, dmi_string_nosave(dm, *(d - 1)));
}

static void __init dmi_decode(const struct dmi_header *dm, void *dummy)
{
	switch(dm->type) {
	case 0:		
		dmi_save_ident(dm, DMI_BIOS_VENDOR, 4);
		dmi_save_ident(dm, DMI_BIOS_VERSION, 5);
		dmi_save_ident(dm, DMI_BIOS_DATE, 8);
		break;
	case 1:		
		dmi_save_ident(dm, DMI_SYS_VENDOR, 4);
		dmi_save_ident(dm, DMI_PRODUCT_NAME, 5);
		dmi_save_ident(dm, DMI_PRODUCT_VERSION, 6);
		dmi_save_ident(dm, DMI_PRODUCT_SERIAL, 7);
		dmi_save_uuid(dm, DMI_PRODUCT_UUID, 8);
		break;
	case 2:		
		dmi_save_ident(dm, DMI_BOARD_VENDOR, 4);
		dmi_save_ident(dm, DMI_BOARD_NAME, 5);
		dmi_save_ident(dm, DMI_BOARD_VERSION, 6);
		dmi_save_ident(dm, DMI_BOARD_SERIAL, 7);
		dmi_save_ident(dm, DMI_BOARD_ASSET_TAG, 8);
		break;
	case 3:		
		dmi_save_ident(dm, DMI_CHASSIS_VENDOR, 4);
		dmi_save_type(dm, DMI_CHASSIS_TYPE, 5);
		dmi_save_ident(dm, DMI_CHASSIS_VERSION, 6);
		dmi_save_ident(dm, DMI_CHASSIS_SERIAL, 7);
		dmi_save_ident(dm, DMI_CHASSIS_ASSET_TAG, 8);
		break;
	case 10:	
		dmi_save_devices(dm);
		break;
	case 11:	
		dmi_save_oem_strings_devices(dm);
		break;
	case 38:	
		dmi_save_ipmi_device(dm);
		break;
	case 41:	
		dmi_save_extended_devices(dm);
	}
}

static void __init print_filtered(const char *info)
{
	const char *p;

	if (!info)
		return;

	for (p = info; *p; p++)
		if (isprint(*p))
			printk(KERN_CONT "%c", *p);
		else
			printk(KERN_CONT "\\x%02x", *p & 0xff);
}

static void __init dmi_dump_ids(void)
{
	const char *board;	

	printk(KERN_DEBUG "DMI: ");
	print_filtered(dmi_get_system_info(DMI_SYS_VENDOR));
	printk(KERN_CONT " ");
	print_filtered(dmi_get_system_info(DMI_PRODUCT_NAME));
	board = dmi_get_system_info(DMI_BOARD_NAME);
	if (board) {
		printk(KERN_CONT "/");
		print_filtered(board);
	}
	printk(KERN_CONT ", BIOS ");
	print_filtered(dmi_get_system_info(DMI_BIOS_VERSION));
	printk(KERN_CONT " ");
	print_filtered(dmi_get_system_info(DMI_BIOS_DATE));
	printk(KERN_CONT "\n");
}

static int __init dmi_present(const char __iomem *p)
{
	u8 buf[15];

	memcpy_fromio(buf, p, 15);
	if ((memcmp(buf, "_DMI_", 5) == 0) && dmi_checksum(buf)) {
		dmi_num = (buf[13] << 8) | buf[12];
		dmi_len = (buf[7] << 8) | buf[6];
		dmi_base = (buf[11] << 24) | (buf[10] << 16) |
			(buf[9] << 8) | buf[8];

		if (buf[14] != 0)
			printk(KERN_INFO "DMI %d.%d present.\n",
			       buf[14] >> 4, buf[14] & 0xF);
		else
			printk(KERN_INFO "DMI present.\n");
		if (dmi_walk_early(dmi_decode) == 0) {
			dmi_dump_ids();
			return 0;
		}
	}
	return 1;
}

void __init dmi_scan_machine(void)
{
	char __iomem *p, *q;
	int rc;

	if (efi_enabled) {
		if (efi.smbios == EFI_INVALID_TABLE_ADDR)
			goto error;

		p = dmi_ioremap(efi.smbios, 32);
		if (p == NULL)
			goto error;

		rc = dmi_present(p + 0x10); 
		dmi_iounmap(p, 32);
		if (!rc) {
			dmi_available = 1;
			goto out;
		}
	}
	else {
		p = dmi_ioremap(0xF0000, 0x10000);
		if (p == NULL)
			goto error;

		for (q = p; q < p + 0x10000; q += 16) {
			rc = dmi_present(q);
			if (!rc) {
				dmi_available = 1;
				dmi_iounmap(p, 0x10000);
				goto out;
			}
		}
		dmi_iounmap(p, 0x10000);
	}
 error:
	printk(KERN_INFO "DMI not present or invalid.\n");
 out:
	dmi_initialized = 1;
}

static bool dmi_matches(const struct dmi_system_id *dmi)
{
	int i;

	WARN(!dmi_initialized, KERN_ERR "dmi check: not initialized yet.\n");

	for (i = 0; i < ARRAY_SIZE(dmi->matches); i++) {
		int s = dmi->matches[i].slot;
		if (s == DMI_NONE)
			break;
		if (dmi_ident[s]
		    && strstr(dmi_ident[s], dmi->matches[i].substr))
			continue;
		
		return false;
	}
	return true;
}

static bool dmi_is_end_of_table(const struct dmi_system_id *dmi)
{
	return dmi->matches[0].slot == DMI_NONE;
}

int dmi_check_system(const struct dmi_system_id *list)
{
	int count = 0;
	const struct dmi_system_id *d;

	for (d = list; !dmi_is_end_of_table(d); d++)
		if (dmi_matches(d)) {
			count++;
			if (d->callback && d->callback(d))
				break;
		}

	return count;
}
EXPORT_SYMBOL(dmi_check_system);

const struct dmi_system_id *dmi_first_match(const struct dmi_system_id *list)
{
	const struct dmi_system_id *d;

	for (d = list; !dmi_is_end_of_table(d); d++)
		if (dmi_matches(d))
			return d;

	return NULL;
}
EXPORT_SYMBOL(dmi_first_match);

const char *dmi_get_system_info(int field)
{
	return dmi_ident[field];
}
EXPORT_SYMBOL(dmi_get_system_info);

int dmi_name_in_serial(const char *str)
{
	int f = DMI_PRODUCT_SERIAL;
	if (dmi_ident[f] && strstr(dmi_ident[f], str))
		return 1;
	return 0;
}

int dmi_name_in_vendors(const char *str)
{
	static int fields[] = { DMI_SYS_VENDOR, DMI_BOARD_VENDOR, DMI_NONE };
	int i;
	for (i = 0; fields[i] != DMI_NONE; i++) {
		int f = fields[i];
		if (dmi_ident[f] && strstr(dmi_ident[f], str))
			return 1;
	}
	return 0;
}
EXPORT_SYMBOL(dmi_name_in_vendors);

const struct dmi_device * dmi_find_device(int type, const char *name,
				    const struct dmi_device *from)
{
	const struct list_head *head = from ? &from->list : &dmi_devices;
	struct list_head *d;

	for(d = head->next; d != &dmi_devices; d = d->next) {
		const struct dmi_device *dev =
			list_entry(d, struct dmi_device, list);

		if (((type == DMI_DEV_TYPE_ANY) || (dev->type == type)) &&
		    ((name == NULL) || (strcmp(dev->name, name) == 0)))
			return dev;
	}

	return NULL;
}
EXPORT_SYMBOL(dmi_find_device);

bool dmi_get_date(int field, int *yearp, int *monthp, int *dayp)
{
	int year = 0, month = 0, day = 0;
	bool exists;
	const char *s, *y;
	char *e;

	s = dmi_get_system_info(field);
	exists = s;
	if (!exists)
		goto out;

	y = strrchr(s, '/');
	if (!y)
		goto out;

	y++;
	year = simple_strtoul(y, &e, 10);
	if (y != e && year < 100) {	
		year += 1900;
		if (year < 1996)	
			year += 100;
	}
	if (year > 9999)		
		year = 0;

	
	month = simple_strtoul(s, &e, 10);
	if (s == e || *e != '/' || !month || month > 12) {
		month = 0;
		goto out;
	}

	s = e + 1;
	day = simple_strtoul(s, &e, 10);
	if (s == y || s == e || *e != '/' || day > 31)
		day = 0;
out:
	if (yearp)
		*yearp = year;
	if (monthp)
		*monthp = month;
	if (dayp)
		*dayp = day;
	return exists;
}
EXPORT_SYMBOL(dmi_get_date);

int dmi_walk(void (*decode)(const struct dmi_header *, void *),
	     void *private_data)
{
	u8 *buf;

	if (!dmi_available)
		return -1;

	buf = ioremap(dmi_base, dmi_len);
	if (buf == NULL)
		return -1;

	dmi_table(buf, dmi_len, dmi_num, decode, private_data);

	iounmap(buf);
	return 0;
}
EXPORT_SYMBOL_GPL(dmi_walk);

bool dmi_match(enum dmi_field f, const char *str)
{
	const char *info = dmi_get_system_info(f);

	if (info == NULL || str == NULL)
		return info == str;

	return !strcmp(info, str);
}
EXPORT_SYMBOL_GPL(dmi_match);
