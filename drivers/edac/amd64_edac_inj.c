#include "amd64_edac.h"

static ssize_t amd64_inject_section_show(struct mem_ctl_info *mci, char *buf)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	return sprintf(buf, "0x%x\n", pvt->injection.section);
}

static ssize_t amd64_inject_section_store(struct mem_ctl_info *mci,
					  const char *data, size_t count)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	unsigned long value;
	int ret = 0;

	ret = strict_strtoul(data, 10, &value);
	if (ret != -EINVAL) {

		if (value > 3) {
			amd64_warn("%s: invalid section 0x%lx\n", __func__, value);
			return -EINVAL;
		}

		pvt->injection.section = (u32) value;
		return count;
	}
	return ret;
}

static ssize_t amd64_inject_word_show(struct mem_ctl_info *mci, char *buf)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	return sprintf(buf, "0x%x\n", pvt->injection.word);
}

static ssize_t amd64_inject_word_store(struct mem_ctl_info *mci,
					const char *data, size_t count)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	unsigned long value;
	int ret = 0;

	ret = strict_strtoul(data, 10, &value);
	if (ret != -EINVAL) {

		if (value > 8) {
			amd64_warn("%s: invalid word 0x%lx\n", __func__, value);
			return -EINVAL;
		}

		pvt->injection.word = (u32) value;
		return count;
	}
	return ret;
}

static ssize_t amd64_inject_ecc_vector_show(struct mem_ctl_info *mci, char *buf)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	return sprintf(buf, "0x%x\n", pvt->injection.bit_map);
}

static ssize_t amd64_inject_ecc_vector_store(struct mem_ctl_info *mci,
					     const char *data, size_t count)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	unsigned long value;
	int ret = 0;

	ret = strict_strtoul(data, 16, &value);
	if (ret != -EINVAL) {

		if (value & 0xFFFF0000) {
			amd64_warn("%s: invalid EccVector: 0x%lx\n",
				   __func__, value);
			return -EINVAL;
		}

		pvt->injection.bit_map = (u32) value;
		return count;
	}
	return ret;
}

static ssize_t amd64_inject_read_store(struct mem_ctl_info *mci,
					const char *data, size_t count)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	unsigned long value;
	u32 section, word_bits;
	int ret = 0;

	ret = strict_strtoul(data, 10, &value);
	if (ret != -EINVAL) {

		
		section = F10_NB_ARRAY_DRAM_ECC |
				SET_NB_ARRAY_ADDRESS(pvt->injection.section);
		amd64_write_pci_cfg(pvt->F3, F10_NB_ARRAY_ADDR, section);

		word_bits = SET_NB_DRAM_INJECTION_READ(pvt->injection.word,
						pvt->injection.bit_map);

		
		amd64_write_pci_cfg(pvt->F3, F10_NB_ARRAY_DATA, word_bits);

		debugf0("section=0x%x word_bits=0x%x\n", section, word_bits);

		return count;
	}
	return ret;
}

static ssize_t amd64_inject_write_store(struct mem_ctl_info *mci,
					const char *data, size_t count)
{
	struct amd64_pvt *pvt = mci->pvt_info;
	unsigned long value;
	u32 section, word_bits;
	int ret = 0;

	ret = strict_strtoul(data, 10, &value);
	if (ret != -EINVAL) {

		
		section = F10_NB_ARRAY_DRAM_ECC |
				SET_NB_ARRAY_ADDRESS(pvt->injection.section);
		amd64_write_pci_cfg(pvt->F3, F10_NB_ARRAY_ADDR, section);

		word_bits = SET_NB_DRAM_INJECTION_WRITE(pvt->injection.word,
						pvt->injection.bit_map);

		
		amd64_write_pci_cfg(pvt->F3, F10_NB_ARRAY_DATA, word_bits);

		debugf0("section=0x%x word_bits=0x%x\n", section, word_bits);

		return count;
	}
	return ret;
}

struct mcidev_sysfs_attribute amd64_inj_attrs[] = {

	{
		.attr = {
			.name = "inject_section",
			.mode = (S_IRUGO | S_IWUSR)
		},
		.show = amd64_inject_section_show,
		.store = amd64_inject_section_store,
	},
	{
		.attr = {
			.name = "inject_word",
			.mode = (S_IRUGO | S_IWUSR)
		},
		.show = amd64_inject_word_show,
		.store = amd64_inject_word_store,
	},
	{
		.attr = {
			.name = "inject_ecc_vector",
			.mode = (S_IRUGO | S_IWUSR)
		},
		.show = amd64_inject_ecc_vector_show,
		.store = amd64_inject_ecc_vector_store,
	},
	{
		.attr = {
			.name = "inject_write",
			.mode = (S_IRUGO | S_IWUSR)
		},
		.show = NULL,
		.store = amd64_inject_write_store,
	},
	{
		.attr = {
			.name = "inject_read",
			.mode = (S_IRUGO | S_IWUSR)
		},
		.show = NULL,
		.store = amd64_inject_read_store,
	},
};
