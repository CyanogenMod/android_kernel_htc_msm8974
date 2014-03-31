/* Firmware file reading and download helpers
 *
 * See copyright notice in main.c
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/module.h>

#include "hermes.h"
#include "hermes_dld.h"
#include "orinoco.h"

#include "fw.h"

#define TEXT_END	0x1A		

struct fw_info {
	char *pri_fw;
	char *sta_fw;
	char *ap_fw;
	u32 pda_addr;
	u16 pda_size;
};

static const struct fw_info orinoco_fw[] = {
	{ NULL, "agere_sta_fw.bin", "agere_ap_fw.bin", 0x00390000, 1000 },
	{ NULL, "prism_sta_fw.bin", "prism_ap_fw.bin", 0, 1024 },
	{ "symbol_sp24t_prim_fw", "symbol_sp24t_sec_fw", NULL, 0x00003100, 512 }
};
MODULE_FIRMWARE("agere_sta_fw.bin");
MODULE_FIRMWARE("agere_ap_fw.bin");
MODULE_FIRMWARE("prism_sta_fw.bin");
MODULE_FIRMWARE("prism_ap_fw.bin");
MODULE_FIRMWARE("symbol_sp24t_prim_fw");
MODULE_FIRMWARE("symbol_sp24t_sec_fw");

struct orinoco_fw_header {
	char hdr_vers[6];       
	__le16 headersize;      
	__le32 entry_point;     
	__le32 blocks;          
	__le32 block_offset;    
	__le32 pdr_offset;      
	__le32 pri_offset;      
	__le32 compat_offset;   
	char signature[0];      
} __packed;

static const char *validate_fw(const struct orinoco_fw_header *hdr, size_t len)
{
	u16 hdrsize;

	if (len < sizeof(*hdr))
		return "image too small";
	if (memcmp(hdr->hdr_vers, "HFW", 3) != 0)
		return "format not recognised";

	hdrsize = le16_to_cpu(hdr->headersize);
	if (hdrsize > len)
		return "bad headersize";
	if ((hdrsize + le32_to_cpu(hdr->block_offset)) > len)
		return "bad block offset";
	if ((hdrsize + le32_to_cpu(hdr->pdr_offset)) > len)
		return "bad PDR offset";
	if ((hdrsize + le32_to_cpu(hdr->pri_offset)) > len)
		return "bad PRI offset";
	if ((hdrsize + le32_to_cpu(hdr->compat_offset)) > len)
		return "bad compat offset";

	
	return NULL;
}

#if defined(CONFIG_HERMES_CACHE_FW_ON_INIT) || defined(CONFIG_PM_SLEEP)
static inline const struct firmware *
orinoco_cached_fw_get(struct orinoco_private *priv, bool primary)
{
	if (primary)
		return priv->cached_pri_fw;
	else
		return priv->cached_fw;
}
#else
#define orinoco_cached_fw_get(priv, primary) (NULL)
#endif

static int
orinoco_dl_firmware(struct orinoco_private *priv,
		    const struct fw_info *fw,
		    int ap)
{
	
	__le16 *pda;

	struct hermes *hw = &priv->hw;
	const struct firmware *fw_entry;
	const struct orinoco_fw_header *hdr;
	const unsigned char *first_block;
	const void *end;
	const char *firmware;
	const char *fw_err;
	struct device *dev = priv->dev;
	int err = 0;

	pda = kzalloc(fw->pda_size, GFP_KERNEL);
	if (!pda)
		return -ENOMEM;

	if (ap)
		firmware = fw->ap_fw;
	else
		firmware = fw->sta_fw;

	dev_dbg(dev, "Attempting to download firmware %s\n", firmware);

	
	err = hw->ops->read_pda(hw, pda, fw->pda_addr, fw->pda_size);
	dev_dbg(dev, "Read PDA returned %d\n", err);
	if (err)
		goto free;

	if (!orinoco_cached_fw_get(priv, false)) {
		err = request_firmware(&fw_entry, firmware, priv->dev);

		if (err) {
			dev_err(dev, "Cannot find firmware %s\n", firmware);
			err = -ENOENT;
			goto free;
		}
	} else
		fw_entry = orinoco_cached_fw_get(priv, false);

	hdr = (const struct orinoco_fw_header *) fw_entry->data;

	fw_err = validate_fw(hdr, fw_entry->size);
	if (fw_err) {
		dev_warn(dev, "Invalid firmware image detected (%s). "
			 "Aborting download\n", fw_err);
		err = -EINVAL;
		goto abort;
	}

	
	err = hw->ops->program_init(hw, le32_to_cpu(hdr->entry_point));
	dev_dbg(dev, "Program init returned %d\n", err);
	if (err != 0)
		goto abort;

	
	first_block = (fw_entry->data +
		       le16_to_cpu(hdr->headersize) +
		       le32_to_cpu(hdr->block_offset));
	end = fw_entry->data + fw_entry->size;

	err = hermes_program(hw, first_block, end);
	dev_dbg(dev, "Program returned %d\n", err);
	if (err != 0)
		goto abort;

	
	first_block = (fw_entry->data +
		       le16_to_cpu(hdr->headersize) +
		       le32_to_cpu(hdr->pdr_offset));

	err = hermes_apply_pda_with_defaults(hw, first_block, end, pda,
					     &pda[fw->pda_size / sizeof(*pda)]);
	dev_dbg(dev, "Apply PDA returned %d\n", err);
	if (err)
		goto abort;

	
	err = hw->ops->program_end(hw);
	dev_dbg(dev, "Program end returned %d\n", err);
	if (err != 0)
		goto abort;

	
	dev_dbg(dev, "hermes_present returned %d\n", hermes_present(hw));

abort:
	
	if (!orinoco_cached_fw_get(priv, false))
		release_firmware(fw_entry);

free:
	kfree(pda);
	return err;
}

static int
symbol_dl_image(struct orinoco_private *priv, const struct fw_info *fw,
		const unsigned char *image, const void *end,
		int secondary)
{
	struct hermes *hw = &priv->hw;
	int ret = 0;
	const unsigned char *ptr;
	const unsigned char *first_block;

	
	__le16 *pda = NULL;

	
	ptr = image;
	while (*ptr++ != TEXT_END);
	first_block = ptr;

	
	if (secondary) {
		pda = kzalloc(fw->pda_size, GFP_KERNEL);
		if (!pda)
			return -ENOMEM;

		ret = hw->ops->read_pda(hw, pda, fw->pda_addr, fw->pda_size);
		if (ret)
			goto free;
	}

	/* Stop the firmware, so that it can be safely rewritten */
	if (priv->stop_fw) {
		ret = priv->stop_fw(priv, 1);
		if (ret)
			goto free;
	}

	
	ret = hermes_program(hw, first_block, end);
	if (ret)
		goto free;

	
	if (secondary) {
		size_t len = hermes_blocks_length(first_block, end);
		ptr = first_block + len;
		ret = hermes_apply_pda(hw, ptr, end, pda,
				       &pda[fw->pda_size / sizeof(*pda)]);
		kfree(pda);
		if (ret)
			return ret;
	}

	
	if (priv->stop_fw) {
		ret = priv->stop_fw(priv, 0);
		if (ret)
			return ret;
	}

	
	ret = hw->ops->init(hw);

	
	if (secondary && ret != 0)
		return -ENODEV;

	
	if (!hermes_present(hw))
		return -ENODEV;

	return 0;

free:
	kfree(pda);
	return ret;
}


static int
symbol_dl_firmware(struct orinoco_private *priv,
		   const struct fw_info *fw)
{
	struct device *dev = priv->dev;
	int ret;
	const struct firmware *fw_entry;

	if (!orinoco_cached_fw_get(priv, true)) {
		if (request_firmware(&fw_entry, fw->pri_fw, priv->dev) != 0) {
			dev_err(dev, "Cannot find firmware: %s\n", fw->pri_fw);
			return -ENOENT;
		}
	} else
		fw_entry = orinoco_cached_fw_get(priv, true);

	
	ret = symbol_dl_image(priv, fw, fw_entry->data,
			      fw_entry->data + fw_entry->size, 0);

	if (!orinoco_cached_fw_get(priv, true))
		release_firmware(fw_entry);
	if (ret) {
		dev_err(dev, "Primary firmware download failed\n");
		return ret;
	}

	if (!orinoco_cached_fw_get(priv, false)) {
		if (request_firmware(&fw_entry, fw->sta_fw, priv->dev) != 0) {
			dev_err(dev, "Cannot find firmware: %s\n", fw->sta_fw);
			return -ENOENT;
		}
	} else
		fw_entry = orinoco_cached_fw_get(priv, false);

	
	ret = symbol_dl_image(priv, fw, fw_entry->data,
			      fw_entry->data + fw_entry->size, 1);
	if (!orinoco_cached_fw_get(priv, false))
		release_firmware(fw_entry);
	if (ret)
		dev_err(dev, "Secondary firmware download failed\n");

	return ret;
}

int orinoco_download(struct orinoco_private *priv)
{
	int err = 0;
	
	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE:
		
		err = orinoco_dl_firmware(priv,
					  &orinoco_fw[priv->firmware_type], 0);
		break;

	case FIRMWARE_TYPE_SYMBOL:
		err = symbol_dl_firmware(priv,
					 &orinoco_fw[priv->firmware_type]);
		break;
	case FIRMWARE_TYPE_INTERSIL:
		break;
	}

	return err;
}

#if defined(CONFIG_HERMES_CACHE_FW_ON_INIT) || defined(CONFIG_PM_SLEEP)
void orinoco_cache_fw(struct orinoco_private *priv, int ap)
{
	const struct firmware *fw_entry = NULL;
	const char *pri_fw;
	const char *fw;

	pri_fw = orinoco_fw[priv->firmware_type].pri_fw;
	if (ap)
		fw = orinoco_fw[priv->firmware_type].ap_fw;
	else
		fw = orinoco_fw[priv->firmware_type].sta_fw;

	if (pri_fw) {
		if (request_firmware(&fw_entry, pri_fw, priv->dev) == 0)
			priv->cached_pri_fw = fw_entry;
	}

	if (fw) {
		if (request_firmware(&fw_entry, fw, priv->dev) == 0)
			priv->cached_fw = fw_entry;
	}
}

void orinoco_uncache_fw(struct orinoco_private *priv)
{
	if (priv->cached_pri_fw)
		release_firmware(priv->cached_pri_fw);
	if (priv->cached_fw)
		release_firmware(priv->cached_fw);

	priv->cached_pri_fw = NULL;
	priv->cached_fw = NULL;
}
#endif
