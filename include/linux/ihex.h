
#ifndef __LINUX_IHEX_H__
#define __LINUX_IHEX_H__

#include <linux/types.h>
#include <linux/firmware.h>
#include <linux/device.h>

struct ihex_binrec {
	__be32 addr;
	__be16 len;
	uint8_t data[0];
} __attribute__((packed));

static inline const struct ihex_binrec *
ihex_next_binrec(const struct ihex_binrec *rec)
{
	int next = ((be16_to_cpu(rec->len) + 5) & ~3) - 2;
	rec = (void *)&rec->data[next];

	return be16_to_cpu(rec->len) ? rec : NULL;
}

static inline int ihex_validate_fw(const struct firmware *fw)
{
	const struct ihex_binrec *rec;
	size_t ofs = 0;

	while (ofs <= fw->size - sizeof(*rec)) {
		rec = (void *)&fw->data[ofs];

		
		if (!be16_to_cpu(rec->len))
			return 0;

		
		ofs += (sizeof(*rec) + be16_to_cpu(rec->len) + 3) & ~3;
	}
	return -EINVAL;
}

static inline int request_ihex_firmware(const struct firmware **fw,
					const char *fw_name,
					struct device *dev)
{
	const struct firmware *lfw;
	int ret;

	ret = request_firmware(&lfw, fw_name, dev);
	if (ret)
		return ret;
	ret = ihex_validate_fw(lfw);
	if (ret) {
		dev_err(dev, "Firmware \"%s\" not valid IHEX records\n",
			fw_name);
		release_firmware(lfw);
		return ret;
	}
	*fw = lfw;
	return 0;
}
#endif 
