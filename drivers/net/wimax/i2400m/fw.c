/*
 * Intel Wireless WiMAX Connection 2400m
 * Firmware uploader
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Intel Corporation <linux-wimax@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Initial implementation
 *
 *
 * THE PROCEDURE
 *
 * The 2400m and derived devices work in two modes: boot-mode or
 * normal mode. In boot mode we can execute only a handful of commands
 * targeted at uploading the firmware and launching it.
 *
 * The 2400m enters boot mode when it is first connected to the
 * system, when it crashes and when you ask it to reboot. There are
 * two submodes of the boot mode: signed and non-signed. Signed takes
 * firmwares signed with a certain private key, non-signed takes any
 * firmware. Normal hardware takes only signed firmware.
 *
 * On boot mode, in USB, we write to the device using the bulk out
 * endpoint and read from it in the notification endpoint. In SDIO we
 * talk to it via the write address and read from the read address.
 *
 * Upon entrance to boot mode, the device sends (preceded with a few
 * zero length packets (ZLPs) on the notification endpoint in USB) a
 * reboot barker (4 le32 words with the same value). We ack it by
 * sending the same barker to the device. The device acks with a
 * reboot ack barker (4 le32 words with value I2400M_ACK_BARKER) and
 * then is fully booted. At this point we can upload the firmware.
 *
 * Note that different iterations of the device and EEPROM
 * configurations will send different [re]boot barkers; these are
 * collected in i2400m_barker_db along with the firmware
 * characteristics they require.
 *
 * This process is accomplished by the i2400m_bootrom_init()
 * function. All the device interaction happens through the
 * i2400m_bm_cmd() [boot mode command]. Special return values will
 * indicate if the device did reset during the process.
 *
 * After this, we read the MAC address and then (if needed)
 * reinitialize the device. We need to read it ahead of time because
 * in the future, we might not upload the firmware until userspace
 * 'ifconfig up's the device.
 *
 * We can then upload the firmware file. The file is composed of a BCF
 * header (basic data, keys and signatures) and a list of write
 * commands and payloads. Optionally more BCF headers might follow the
 * main payload. We first upload the header [i2400m_dnload_init()] and
 * then pass the commands and payloads verbatim to the i2400m_bm_cmd()
 * function [i2400m_dnload_bcf()]. Then we tell the device to jump to
 * the new firmware [i2400m_dnload_finalize()].
 *
 * Once firmware is uploaded, we are good to go :)
 *
 * When we don't know in which mode we are, we first try by sending a
 * warm reset request that will take us to boot-mode. If we time out
 * waiting for a reboot barker, that means maybe we are already in
 * boot mode, so we send a reboot barker.
 *
 * COMMAND EXECUTION
 *
 * This code (and process) is single threaded; for executing commands,
 * we post a URB to the notification endpoint, post the command, wait
 * for data on the notification buffer. We don't need to worry about
 * others as we know we are the only ones in there.
 *
 * BACKEND IMPLEMENTATION
 *
 * This code is bus-generic; the bus-specific driver provides back end
 * implementations to send a boot mode command to the device and to
 * read an acknolwedgement from it (or an asynchronous notification)
 * from it.
 *
 * FIRMWARE LOADING
 *
 * Note that in some cases, we can't just load a firmware file (for
 * example, when resuming). For that, we might cache the firmware
 * file. Thus, when doing the bootstrap, if there is a cache firmware
 * file, it is used; if not, loading from disk is attempted.
 *
 * ROADMAP
 *
 * i2400m_barker_db_init              Called by i2400m_driver_init()
 *   i2400m_barker_db_add
 *
 * i2400m_barker_db_exit              Called by i2400m_driver_exit()
 *
 * i2400m_dev_bootstrap               Called by __i2400m_dev_start()
 *   request_firmware
 *   i2400m_fw_bootstrap
 *     i2400m_fw_check
 *       i2400m_fw_hdr_check
 *     i2400m_fw_dnload
 *   release_firmware
 *
 * i2400m_fw_dnload
 *   i2400m_bootrom_init
 *     i2400m_bm_cmd
 *     i2400m_reset
 *   i2400m_dnload_init
 *     i2400m_dnload_init_signed
 *     i2400m_dnload_init_nonsigned
 *       i2400m_download_chunk
 *         i2400m_bm_cmd
 *   i2400m_dnload_bcf
 *     i2400m_bm_cmd
 *   i2400m_dnload_finalize
 *     i2400m_bm_cmd
 *
 * i2400m_bm_cmd
 *   i2400m->bus_bm_cmd_send()
 *   i2400m->bus_bm_wait_for_ack
 *   __i2400m_bm_ack_verify
 *     i2400m_is_boot_barker
 *
 * i2400m_bm_cmd_prepare              Used by bus-drivers to prep
 *                                    commands before sending
 *
 * i2400m_pm_notifier                 Called on Power Management events
 *   i2400m_fw_cache
 *   i2400m_fw_uncache
 */
#include <linux/firmware.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/export.h>
#include "i2400m.h"


#define D_SUBMODULE fw
#include "debug-levels.h"


static const __le32 i2400m_ACK_BARKER[4] = {
	cpu_to_le32(I2400M_ACK_BARKER),
	cpu_to_le32(I2400M_ACK_BARKER),
	cpu_to_le32(I2400M_ACK_BARKER),
	cpu_to_le32(I2400M_ACK_BARKER)
};


void i2400m_bm_cmd_prepare(struct i2400m_bootrom_header *cmd)
{
	if (i2400m_brh_get_use_checksum(cmd)) {
		int i;
		u32 checksum = 0;
		const u32 *checksum_ptr = (void *) cmd->payload;
		for (i = 0; i < cmd->data_size / 4; i++)
			checksum += cpu_to_le32(*checksum_ptr++);
		checksum += cmd->command + cmd->target_addr + cmd->data_size;
		cmd->block_checksum = cpu_to_le32(checksum);
	}
}
EXPORT_SYMBOL_GPL(i2400m_bm_cmd_prepare);


static struct i2400m_barker_db {
	__le32 data[4];
} *i2400m_barker_db;
static size_t i2400m_barker_db_used, i2400m_barker_db_size;


static
int i2400m_zrealloc_2x(void **ptr, size_t *_count, size_t el_size,
		       gfp_t gfp_flags)
{
	size_t old_count = *_count,
		new_count = old_count ? 2 * old_count : 2,
		old_size = el_size * old_count,
		new_size = el_size * new_count;
	void *nptr = krealloc(*ptr, new_size, gfp_flags);
	if (nptr) {
		if (old_size == 0)
			memset(nptr, 0, new_size);
		else
			memset(nptr + old_size, 0, old_size);
		*_count = new_count;
		*ptr = nptr;
		return 0;
	} else
		return -ENOMEM;
}


static
int i2400m_barker_db_add(u32 barker_id)
{
	int result;

	struct i2400m_barker_db *barker;
	if (i2400m_barker_db_used >= i2400m_barker_db_size) {
		result = i2400m_zrealloc_2x(
			(void **) &i2400m_barker_db, &i2400m_barker_db_size,
			sizeof(i2400m_barker_db[0]), GFP_KERNEL);
		if (result < 0)
			return result;
	}
	barker = i2400m_barker_db + i2400m_barker_db_used++;
	barker->data[0] = le32_to_cpu(barker_id);
	barker->data[1] = le32_to_cpu(barker_id);
	barker->data[2] = le32_to_cpu(barker_id);
	barker->data[3] = le32_to_cpu(barker_id);
	return 0;
}


void i2400m_barker_db_exit(void)
{
	kfree(i2400m_barker_db);
	i2400m_barker_db = NULL;
	i2400m_barker_db_size = 0;
	i2400m_barker_db_used = 0;
}


static
int i2400m_barker_db_known_barkers(void)
{
	int result;

	result = i2400m_barker_db_add(I2400M_NBOOT_BARKER);
	if (result < 0)
		goto error_add;
	result = i2400m_barker_db_add(I2400M_SBOOT_BARKER);
	if (result < 0)
		goto error_add;
	result = i2400m_barker_db_add(I2400M_SBOOT_BARKER_6050);
	if (result < 0)
		goto error_add;
error_add:
       return result;
}


int i2400m_barker_db_init(const char *_options)
{
	int result;
	char *options = NULL, *options_orig, *token;

	i2400m_barker_db = NULL;
	i2400m_barker_db_size = 0;
	i2400m_barker_db_used = 0;

	result = i2400m_barker_db_known_barkers();
	if (result < 0)
		goto error_add;
	
	if (_options != NULL) {
		unsigned barker;

		options_orig = kstrdup(_options, GFP_KERNEL);
		if (options_orig == NULL)
			goto error_parse;
		options = options_orig;

		while ((token = strsep(&options, ",")) != NULL) {
			if (*token == '\0')	
				continue;
			if (sscanf(token, "%x", &barker) != 1
			    || barker > 0xffffffff) {
				printk(KERN_ERR "%s: can't recognize "
				       "i2400m.barkers value '%s' as "
				       "a 32-bit number\n",
				       __func__, token);
				result = -EINVAL;
				goto error_parse;
			}
			if (barker == 0) {
				
				i2400m_barker_db_exit();
				continue;
			}
			result = i2400m_barker_db_add(barker);
			if (result < 0)
				goto error_add;
		}
		kfree(options_orig);
	}
	return 0;

error_parse:
error_add:
	kfree(i2400m_barker_db);
	return result;
}


int i2400m_is_boot_barker(struct i2400m *i2400m,
			  const void *buf, size_t buf_size)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct i2400m_barker_db *barker;
	int i;

	result = -ENOENT;
	if (buf_size != sizeof(i2400m_barker_db[i].data))
		return result;

	if (i2400m->barker
	    && !memcmp(buf, i2400m->barker, sizeof(i2400m->barker->data))) {
		unsigned index = (i2400m->barker - i2400m_barker_db)
			/ sizeof(*i2400m->barker);
		d_printf(2, dev, "boot barker cache-confirmed #%u/%08x\n",
			 index, le32_to_cpu(i2400m->barker->data[0]));
		return 0;
	}

	for (i = 0; i < i2400m_barker_db_used; i++) {
		barker = &i2400m_barker_db[i];
		BUILD_BUG_ON(sizeof(barker->data) != 16);
		if (memcmp(buf, barker->data, sizeof(barker->data)))
			continue;

		if (i2400m->barker == NULL) {
			i2400m->barker = barker;
			d_printf(1, dev, "boot barker set to #%u/%08x\n",
				 i, le32_to_cpu(barker->data[0]));
			if (barker->data[0] == le32_to_cpu(I2400M_NBOOT_BARKER))
				i2400m->sboot = 0;
			else
				i2400m->sboot = 1;
		} else if (i2400m->barker != barker) {
			dev_err(dev, "HW inconsistency: device "
				"reports a different boot barker "
				"than set (from %08x to %08x)\n",
				le32_to_cpu(i2400m->barker->data[0]),
				le32_to_cpu(barker->data[0]));
			result = -EIO;
		} else
			d_printf(2, dev, "boot barker confirmed #%u/%08x\n",
				 i, le32_to_cpu(barker->data[0]));
		result = 0;
		break;
	}
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_is_boot_barker);


static
ssize_t __i2400m_bm_ack_verify(struct i2400m *i2400m, int opcode,
			       struct i2400m_bootrom_header *ack,
			       size_t ack_size, int flags)
{
	ssize_t result = -ENOMEM;
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(8, dev, "(i2400m %p opcode %d ack %p size %zu)\n",
		  i2400m, opcode, ack, ack_size);
	if (ack_size < sizeof(*ack)) {
		result = -EIO;
		dev_err(dev, "boot-mode cmd %d: HW BUG? notification didn't "
			"return enough data (%zu bytes vs %zu expected)\n",
			opcode, ack_size, sizeof(*ack));
		goto error_ack_short;
	}
	result = i2400m_is_boot_barker(i2400m, ack, ack_size);
	if (result >= 0) {
		result = -ERESTARTSYS;
		d_printf(6, dev, "boot-mode cmd %d: HW boot barker\n", opcode);
		goto error_reboot;
	}
	if (ack_size == sizeof(i2400m_ACK_BARKER)
		 && memcmp(ack, i2400m_ACK_BARKER, sizeof(*ack)) == 0) {
		result = -EISCONN;
		d_printf(3, dev, "boot-mode cmd %d: HW reboot ack barker\n",
			 opcode);
		goto error_reboot_ack;
	}
	result = 0;
	if (flags & I2400M_BM_CMD_RAW)
		goto out_raw;
	ack->data_size = le32_to_cpu(ack->data_size);
	ack->target_addr = le32_to_cpu(ack->target_addr);
	ack->block_checksum = le32_to_cpu(ack->block_checksum);
	d_printf(5, dev, "boot-mode cmd %d: notification for opcode %u "
		 "response %u csum %u rr %u da %u\n",
		 opcode, i2400m_brh_get_opcode(ack),
		 i2400m_brh_get_response(ack),
		 i2400m_brh_get_use_checksum(ack),
		 i2400m_brh_get_response_required(ack),
		 i2400m_brh_get_direct_access(ack));
	result = -EIO;
	if (i2400m_brh_get_signature(ack) != 0xcbbc) {
		dev_err(dev, "boot-mode cmd %d: HW BUG? wrong signature "
			"0x%04x\n", opcode, i2400m_brh_get_signature(ack));
		goto error_ack_signature;
	}
	if (opcode != -1 && opcode != i2400m_brh_get_opcode(ack)) {
		dev_err(dev, "boot-mode cmd %d: HW BUG? "
			"received response for opcode %u, expected %u\n",
			opcode, i2400m_brh_get_opcode(ack), opcode);
		goto error_ack_opcode;
	}
	if (i2400m_brh_get_response(ack) != 0) {	
		dev_err(dev, "boot-mode cmd %d: error; hw response %u\n",
			opcode, i2400m_brh_get_response(ack));
		goto error_ack_failed;
	}
	if (ack_size < ack->data_size + sizeof(*ack)) {
		dev_err(dev, "boot-mode cmd %d: SW BUG "
			"driver provided only %zu bytes for %zu bytes "
			"of data\n", opcode, ack_size,
			(size_t) le32_to_cpu(ack->data_size) + sizeof(*ack));
		goto error_ack_short_buffer;
	}
	result = ack_size;
error_ack_short_buffer:
error_ack_failed:
error_ack_opcode:
error_ack_signature:
out_raw:
error_reboot_ack:
error_reboot:
error_ack_short:
	d_fnend(8, dev, "(i2400m %p opcode %d ack %p size %zu) = %d\n",
		i2400m, opcode, ack, ack_size, (int) result);
	return result;
}


static
ssize_t i2400m_bm_cmd(struct i2400m *i2400m,
		      const struct i2400m_bootrom_header *cmd, size_t cmd_size,
		      struct i2400m_bootrom_header *ack, size_t ack_size,
		      int flags)
{
	ssize_t result = -ENOMEM, rx_bytes;
	struct device *dev = i2400m_dev(i2400m);
	int opcode = cmd == NULL ? -1 : i2400m_brh_get_opcode(cmd);

	d_fnstart(6, dev, "(i2400m %p cmd %p size %zu ack %p size %zu)\n",
		  i2400m, cmd, cmd_size, ack, ack_size);
	BUG_ON(ack_size < sizeof(*ack));
	BUG_ON(i2400m->boot_mode == 0);

	if (cmd != NULL) {		
		result = i2400m->bus_bm_cmd_send(i2400m, cmd, cmd_size, flags);
		if (result < 0)
			goto error_cmd_send;
		if ((flags & I2400M_BM_CMD_RAW) == 0)
			d_printf(5, dev,
				 "boot-mode cmd %d csum %u rr %u da %u: "
				 "addr 0x%04x size %u block csum 0x%04x\n",
				 opcode, i2400m_brh_get_use_checksum(cmd),
				 i2400m_brh_get_response_required(cmd),
				 i2400m_brh_get_direct_access(cmd),
				 cmd->target_addr, cmd->data_size,
				 cmd->block_checksum);
	}
	result = i2400m->bus_bm_wait_for_ack(i2400m, ack, ack_size);
	if (result < 0) {
		dev_err(dev, "boot-mode cmd %d: error waiting for an ack: %d\n",
			opcode, (int) result);	
		goto error_wait_for_ack;
	}
	rx_bytes = result;
	result = __i2400m_bm_ack_verify(i2400m, opcode, ack, ack_size, flags);
	if (result < 0)
		goto error_bad_ack;
	result = rx_bytes;
error_bad_ack:
error_wait_for_ack:
error_cmd_send:
	d_fnend(6, dev, "(i2400m %p cmd %p size %zu ack %p size %zu) = %d\n",
		i2400m, cmd, cmd_size, ack, ack_size, (int) result);
	return result;
}


static int i2400m_download_chunk(struct i2400m *i2400m, const void *chunk,
				 size_t __chunk_len, unsigned long addr,
				 unsigned int direct, unsigned int do_csum)
{
	int ret;
	size_t chunk_len = ALIGN(__chunk_len, I2400M_PL_ALIGN);
	struct device *dev = i2400m_dev(i2400m);
	struct {
		struct i2400m_bootrom_header cmd;
		u8 cmd_payload[chunk_len];
	} __packed *buf;
	struct i2400m_bootrom_header ack;

	d_fnstart(5, dev, "(i2400m %p chunk %p __chunk_len %zu addr 0x%08lx "
		  "direct %u do_csum %u)\n", i2400m, chunk, __chunk_len,
		  addr, direct, do_csum);
	buf = i2400m->bm_cmd_buf;
	memcpy(buf->cmd_payload, chunk, __chunk_len);
	memset(buf->cmd_payload + __chunk_len, 0xad, chunk_len - __chunk_len);

	buf->cmd.command = i2400m_brh_command(I2400M_BRH_WRITE,
					      __chunk_len & 0x3 ? 0 : do_csum,
					      __chunk_len & 0xf ? 0 : direct);
	buf->cmd.target_addr = cpu_to_le32(addr);
	buf->cmd.data_size = cpu_to_le32(__chunk_len);
	ret = i2400m_bm_cmd(i2400m, &buf->cmd, sizeof(buf->cmd) + chunk_len,
			    &ack, sizeof(ack), 0);
	if (ret >= 0)
		ret = 0;
	d_fnend(5, dev, "(i2400m %p chunk %p __chunk_len %zu addr 0x%08lx "
		"direct %u do_csum %u) = %d\n", i2400m, chunk, __chunk_len,
		addr, direct, do_csum, ret);
	return ret;
}


static
ssize_t i2400m_dnload_bcf(struct i2400m *i2400m,
			  const struct i2400m_bcf_hdr *bcf, size_t bcf_len)
{
	ssize_t ret;
	struct device *dev = i2400m_dev(i2400m);
	size_t offset,		
		data_size,	
		section_size,	
		section = 1;
	const struct i2400m_bootrom_header *bh;
	struct i2400m_bootrom_header ack;

	d_fnstart(3, dev, "(i2400m %p bcf %p bcf_len %zu)\n",
		  i2400m, bcf, bcf_len);
	offset = le32_to_cpu(bcf->header_len) * sizeof(u32);
	while (1) {	
		bh = (void *) bcf + offset;
		data_size = le32_to_cpu(bh->data_size);
		section_size = ALIGN(sizeof(*bh) + data_size, 4);
		d_printf(7, dev,
			 "downloading section #%zu (@%zu %zu B) to 0x%08x\n",
			 section, offset, sizeof(*bh) + data_size,
			 le32_to_cpu(bh->target_addr));
		if (i2400m_brh_get_opcode(bh) == I2400M_BRH_SIGNED_JUMP ||
			i2400m_brh_get_opcode(bh) == I2400M_BRH_JUMP) {
			d_printf(5, dev,  "jump found @%zu\n", offset);
			break;
		}
		if (offset + section_size > bcf_len) {
			dev_err(dev, "fw %s: bad section #%zu, "
				"end (@%zu) beyond EOF (@%zu)\n",
				i2400m->fw_name, section,
				offset + section_size,  bcf_len);
			ret = -EINVAL;
			goto error_section_beyond_eof;
		}
		__i2400m_msleep(20);
		ret = i2400m_bm_cmd(i2400m, bh, section_size,
				    &ack, sizeof(ack), I2400M_BM_CMD_RAW);
		if (ret < 0) {
			dev_err(dev, "fw %s: section #%zu (@%zu %zu B) "
				"failed %d\n", i2400m->fw_name, section,
				offset, sizeof(*bh) + data_size, (int) ret);
			goto error_send;
		}
		offset += section_size;
		section++;
	}
	ret = offset;
error_section_beyond_eof:
error_send:
	d_fnend(3, dev, "(i2400m %p bcf %p bcf_len %zu) = %d\n",
		i2400m, bcf, bcf_len, (int) ret);
	return ret;
}


static
unsigned i2400m_boot_is_signed(struct i2400m *i2400m)
{
	return likely(i2400m->sboot);
}


static
int i2400m_dnload_finalize(struct i2400m *i2400m,
			   const struct i2400m_bcf_hdr *bcf_hdr,
			   const struct i2400m_bcf_hdr *bcf, size_t offset)
{
	int ret = 0;
	struct device *dev = i2400m_dev(i2400m);
	struct i2400m_bootrom_header *cmd, ack;
	struct {
		struct i2400m_bootrom_header cmd;
		u8 cmd_pl[0];
	} __packed *cmd_buf;
	size_t signature_block_offset, signature_block_size;

	d_fnstart(3, dev, "offset %zu\n", offset);
	cmd = (void *) bcf + offset;
	if (i2400m_boot_is_signed(i2400m) == 0) {
		struct i2400m_bootrom_header jump_ack;
		d_printf(1, dev, "unsecure boot, jumping to 0x%08x\n",
			le32_to_cpu(cmd->target_addr));
		cmd_buf = i2400m->bm_cmd_buf;
		memcpy(&cmd_buf->cmd, cmd, sizeof(*cmd));
		cmd = &cmd_buf->cmd;
		
		i2400m_brh_set_opcode(cmd, I2400M_BRH_JUMP);
		cmd->data_size = 0;
		ret = i2400m_bm_cmd(i2400m, cmd, sizeof(*cmd),
				    &jump_ack, sizeof(jump_ack), 0);
	} else {
		d_printf(1, dev, "secure boot, jumping to 0x%08x\n",
			 le32_to_cpu(cmd->target_addr));
		cmd_buf = i2400m->bm_cmd_buf;
		memcpy(&cmd_buf->cmd, cmd, sizeof(*cmd));
		signature_block_offset =
			sizeof(*bcf_hdr)
			+ le32_to_cpu(bcf_hdr->key_size) * sizeof(u32)
			+ le32_to_cpu(bcf_hdr->exponent_size) * sizeof(u32);
		signature_block_size =
			le32_to_cpu(bcf_hdr->modulus_size) * sizeof(u32);
		memcpy(cmd_buf->cmd_pl,
		       (void *) bcf_hdr + signature_block_offset,
		       signature_block_size);
		ret = i2400m_bm_cmd(i2400m, &cmd_buf->cmd,
				    sizeof(cmd_buf->cmd) + signature_block_size,
				    &ack, sizeof(ack), I2400M_BM_CMD_RAW);
	}
	d_fnend(3, dev, "returning %d\n", ret);
	return ret;
}


int i2400m_bootrom_init(struct i2400m *i2400m, enum i2400m_bri flags)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct i2400m_bootrom_header *cmd;
	struct i2400m_bootrom_header ack;
	int count = i2400m->bus_bm_retries;
	int ack_timeout_cnt = 1;
	unsigned i;

	BUILD_BUG_ON(sizeof(*cmd) != sizeof(i2400m_barker_db[0].data));
	BUILD_BUG_ON(sizeof(ack) != sizeof(i2400m_ACK_BARKER));

	d_fnstart(4, dev, "(i2400m %p flags 0x%08x)\n", i2400m, flags);
	result = -ENOMEM;
	cmd = i2400m->bm_cmd_buf;
	if (flags & I2400M_BRI_SOFT)
		goto do_reboot_ack;
do_reboot:
	ack_timeout_cnt = 1;
	if (--count < 0)
		goto error_timeout;
	d_printf(4, dev, "device reboot: reboot command [%d # left]\n",
		 count);
	if ((flags & I2400M_BRI_NO_REBOOT) == 0)
		i2400m_reset(i2400m, I2400M_RT_WARM);
	result = i2400m_bm_cmd(i2400m, NULL, 0, &ack, sizeof(ack),
			       I2400M_BM_CMD_RAW);
	flags &= ~I2400M_BRI_NO_REBOOT;
	switch (result) {
	case -ERESTARTSYS:
		d_printf(4, dev, "device reboot: got reboot barker\n");
		break;
	case -EISCONN:	
		d_printf(4, dev, "device reboot: got ack barker - whatever\n");
		goto do_reboot;
	case -ETIMEDOUT:
		if (i2400m->barker != NULL) {
			dev_err(dev, "device boot: reboot barker timed out, "
				"trying (set) %08x echo/ack\n",
				le32_to_cpu(i2400m->barker->data[0]));
			goto do_reboot_ack;
		}
		for (i = 0; i < i2400m_barker_db_used; i++) {
			struct i2400m_barker_db *barker = &i2400m_barker_db[i];
			memcpy(cmd, barker->data, sizeof(barker->data));
			result = i2400m_bm_cmd(i2400m, cmd, sizeof(*cmd),
					       &ack, sizeof(ack),
					       I2400M_BM_CMD_RAW);
			if (result == -EISCONN) {
				dev_warn(dev, "device boot: got ack barker "
					 "after sending echo/ack barker "
					 "#%d/%08x; rebooting j.i.c.\n",
					 i, le32_to_cpu(barker->data[0]));
				flags &= ~I2400M_BRI_NO_REBOOT;
				goto do_reboot;
			}
		}
		dev_err(dev, "device boot: tried all the echo/acks, could "
			"not get device to respond; giving up");
		result = -ESHUTDOWN;
	case -EPROTO:
	case -ESHUTDOWN:	
	case -EINTR:		
		goto error_dev_gone;
	default:
		dev_err(dev, "device reboot: error %d while waiting "
			"for reboot barker - rebooting\n", result);
		d_dump(1, dev, &ack, result);
		goto do_reboot;
	}
do_reboot_ack:
	d_printf(4, dev, "device reboot ack: sending ack [%d # left]\n", count);
	memcpy(cmd, i2400m->barker->data, sizeof(i2400m->barker->data));
	result = i2400m_bm_cmd(i2400m, cmd, sizeof(*cmd),
			       &ack, sizeof(ack), I2400M_BM_CMD_RAW);
	switch (result) {
	case -ERESTARTSYS:
		d_printf(4, dev, "reboot ack: got reboot barker - retrying\n");
		if (--count < 0)
			goto error_timeout;
		goto do_reboot_ack;
	case -EISCONN:
		d_printf(4, dev, "reboot ack: got ack barker - good\n");
		break;
	case -ETIMEDOUT:	
		if (ack_timeout_cnt-- < 0) {
			d_printf(4, dev, "reboot ack timedout: retrying\n");
			goto do_reboot_ack;
		} else {
			dev_err(dev, "reboot ack timedout too long: "
				"trying reboot\n");
			goto do_reboot;
		}
		break;
	case -EPROTO:
	case -ESHUTDOWN:	
		goto error_dev_gone;
	default:
		dev_err(dev, "device reboot ack: error %d while waiting for "
			"reboot ack barker - rebooting\n", result);
		goto do_reboot;
	}
	d_printf(2, dev, "device reboot ack: got ack barker - boot done\n");
	result = 0;
exit_timeout:
error_dev_gone:
	d_fnend(4, dev, "(i2400m %p flags 0x%08x) = %d\n",
		i2400m, flags, result);
	return result;

error_timeout:
	dev_err(dev, "Timed out waiting for reboot ack\n");
	result = -ETIMEDOUT;
	goto exit_timeout;
}


int i2400m_read_mac_addr(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct i2400m_bootrom_header *cmd;
	struct {
		struct i2400m_bootrom_header ack;
		u8 ack_pl[16];
	} __packed ack_buf;

	d_fnstart(5, dev, "(i2400m %p)\n", i2400m);
	cmd = i2400m->bm_cmd_buf;
	cmd->command = i2400m_brh_command(I2400M_BRH_READ, 0, 1);
	cmd->target_addr = cpu_to_le32(0x00203fe8);
	cmd->data_size = cpu_to_le32(6);
	result = i2400m_bm_cmd(i2400m, cmd, sizeof(*cmd),
			       &ack_buf.ack, sizeof(ack_buf), 0);
	if (result < 0) {
		dev_err(dev, "BM: read mac addr failed: %d\n", result);
		goto error_read_mac;
	}
	d_printf(2, dev, "mac addr is %pM\n", ack_buf.ack_pl);
	if (i2400m->bus_bm_mac_addr_impaired == 1) {
		ack_buf.ack_pl[0] = 0x00;
		ack_buf.ack_pl[1] = 0x16;
		ack_buf.ack_pl[2] = 0xd3;
		get_random_bytes(&ack_buf.ack_pl[3], 3);
		dev_err(dev, "BM is MAC addr impaired, faking MAC addr to "
			"mac addr is %pM\n", ack_buf.ack_pl);
		result = 0;
	}
	net_dev->addr_len = ETH_ALEN;
	memcpy(net_dev->perm_addr, ack_buf.ack_pl, ETH_ALEN);
	memcpy(net_dev->dev_addr, ack_buf.ack_pl, ETH_ALEN);
error_read_mac:
	d_fnend(5, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}


static
int i2400m_dnload_init_nonsigned(struct i2400m *i2400m)
{
	unsigned i = 0;
	int ret = 0;
	struct device *dev = i2400m_dev(i2400m);
	d_fnstart(5, dev, "(i2400m %p)\n", i2400m);
	if (i2400m->bus_bm_pokes_table) {
		while (i2400m->bus_bm_pokes_table[i].address) {
			ret = i2400m_download_chunk(
				i2400m,
				&i2400m->bus_bm_pokes_table[i].data,
				sizeof(i2400m->bus_bm_pokes_table[i].data),
				i2400m->bus_bm_pokes_table[i].address, 1, 1);
			if (ret < 0)
				break;
			i++;
		}
	}
	d_fnend(5, dev, "(i2400m %p) = %d\n", i2400m, ret);
	return ret;
}


static
int i2400m_dnload_init_signed(struct i2400m *i2400m,
			      const struct i2400m_bcf_hdr *bcf_hdr)
{
	int ret;
	struct device *dev = i2400m_dev(i2400m);
	struct {
		struct i2400m_bootrom_header cmd;
		struct i2400m_bcf_hdr cmd_pl;
	} __packed *cmd_buf;
	struct i2400m_bootrom_header ack;

	d_fnstart(5, dev, "(i2400m %p bcf_hdr %p)\n", i2400m, bcf_hdr);
	cmd_buf = i2400m->bm_cmd_buf;
	cmd_buf->cmd.command =
		i2400m_brh_command(I2400M_BRH_HASH_PAYLOAD_ONLY, 0, 0);
	cmd_buf->cmd.target_addr = 0;
	cmd_buf->cmd.data_size = cpu_to_le32(sizeof(cmd_buf->cmd_pl));
	memcpy(&cmd_buf->cmd_pl, bcf_hdr, sizeof(*bcf_hdr));
	ret = i2400m_bm_cmd(i2400m, &cmd_buf->cmd, sizeof(*cmd_buf),
			    &ack, sizeof(ack), 0);
	if (ret >= 0)
		ret = 0;
	d_fnend(5, dev, "(i2400m %p bcf_hdr %p) = %d\n", i2400m, bcf_hdr, ret);
	return ret;
}


static
int i2400m_dnload_init(struct i2400m *i2400m,
		       const struct i2400m_bcf_hdr *bcf_hdr)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);

	if (i2400m_boot_is_signed(i2400m)) {
		d_printf(1, dev, "signed boot\n");
		result = i2400m_dnload_init_signed(i2400m, bcf_hdr);
		if (result == -ERESTARTSYS)
			return result;
		if (result < 0)
			dev_err(dev, "firmware %s: signed boot download "
				"initialization failed: %d\n",
				i2400m->fw_name, result);
	} else {
		
		d_printf(1, dev, "non-signed boot\n");
		result = i2400m_dnload_init_nonsigned(i2400m);
		if (result == -ERESTARTSYS)
			return result;
		if (result < 0)
			dev_err(dev, "firmware %s: non-signed download "
				"initialization failed: %d\n",
				i2400m->fw_name, result);
	}
	return result;
}


static
int i2400m_fw_hdr_check(struct i2400m *i2400m,
			const struct i2400m_bcf_hdr *bcf_hdr,
			size_t index, size_t offset)
{
	struct device *dev = i2400m_dev(i2400m);

	unsigned module_type, header_len, major_version, minor_version,
		module_id, module_vendor, date, size;

	module_type = le32_to_cpu(bcf_hdr->module_type);
	header_len = sizeof(u32) * le32_to_cpu(bcf_hdr->header_len);
	major_version = (le32_to_cpu(bcf_hdr->header_version) & 0xffff0000)
		>> 16;
	minor_version = le32_to_cpu(bcf_hdr->header_version) & 0x0000ffff;
	module_id = le32_to_cpu(bcf_hdr->module_id);
	module_vendor = le32_to_cpu(bcf_hdr->module_vendor);
	date = le32_to_cpu(bcf_hdr->date);
	size = sizeof(u32) * le32_to_cpu(bcf_hdr->size);

	d_printf(1, dev, "firmware %s #%zd@%08zx: BCF header "
		 "type:vendor:id 0x%x:%x:%x v%u.%u (%u/%u B) built %08x\n",
		 i2400m->fw_name, index, offset,
		 module_type, module_vendor, module_id,
		 major_version, minor_version, header_len, size, date);

	
	if (major_version != 1) {
		dev_err(dev, "firmware %s #%zd@%08zx: major header version "
			"v%u.%u not supported\n",
			i2400m->fw_name, index, offset,
			major_version, minor_version);
		return -EBADF;
	}

	if (module_type != 6) {		
		dev_err(dev, "firmware %s #%zd@%08zx: unexpected module "
			"type 0x%x; aborting\n",
			i2400m->fw_name, index, offset,
			module_type);
		return -EBADF;
	}

	if (module_vendor != 0x8086) {
		dev_err(dev, "firmware %s #%zd@%08zx: unexpected module "
			"vendor 0x%x; aborting\n",
			i2400m->fw_name, index, offset, module_vendor);
		return -EBADF;
	}

	if (date < 0x20080300)
		dev_warn(dev, "firmware %s #%zd@%08zx: build date %08x "
			 "too old; unsupported\n",
			 i2400m->fw_name, index, offset, date);
	return 0;
}


static
int i2400m_fw_check(struct i2400m *i2400m, const void *bcf, size_t bcf_size)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	size_t headers = 0;
	const struct i2400m_bcf_hdr *bcf_hdr;
	const void *itr, *next, *top;
	size_t slots = 0, used_slots = 0;

	for (itr = bcf, top = itr + bcf_size;
	     itr < top;
	     headers++, itr = next) {
		size_t leftover, offset, header_len, size;

		leftover = top - itr;
		offset = itr - (const void *) bcf;
		if (leftover <= sizeof(*bcf_hdr)) {
			dev_err(dev, "firmware %s: %zu B left at @%zx, "
				"not enough for BCF header\n",
				i2400m->fw_name, leftover, offset);
			break;
		}
		bcf_hdr = itr;
		header_len = sizeof(u32) * le32_to_cpu(bcf_hdr->header_len);
		size = sizeof(u32) * le32_to_cpu(bcf_hdr->size);
		if (headers == 0)
			next = itr + size;
		else
			next = itr + header_len;

		result = i2400m_fw_hdr_check(i2400m, bcf_hdr, headers, offset);
		if (result < 0)
			continue;
		if (used_slots + 1 >= slots) {
			result = i2400m_zrealloc_2x(
				(void **) &i2400m->fw_hdrs, &slots,
				sizeof(i2400m->fw_hdrs[0]),
				GFP_KERNEL);
			if (result < 0)
				goto error_zrealloc;
		}
		i2400m->fw_hdrs[used_slots] = bcf_hdr;
		used_slots++;
	}
	if (headers == 0) {
		dev_err(dev, "firmware %s: no usable headers found\n",
			i2400m->fw_name);
		result = -EBADF;
	} else
		result = 0;
error_zrealloc:
	return result;
}


static
unsigned i2400m_bcf_hdr_match(struct i2400m *i2400m,
			      const struct i2400m_bcf_hdr *bcf_hdr)
{
	u32 barker = le32_to_cpu(i2400m->barker->data[0])
		& 0x7fffffff;
	u32 module_id = le32_to_cpu(bcf_hdr->module_id)
		& 0x7fffffff;	

	
	if (barker == I2400M_SBOOT_BARKER && module_id == 0)
		return 1;
	if (module_id == barker)
		return 1;
	return 0;
}

static
const struct i2400m_bcf_hdr *i2400m_bcf_hdr_find(struct i2400m *i2400m)
{
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_bcf_hdr **bcf_itr, *bcf_hdr;
	unsigned i = 0;
	u32 barker = le32_to_cpu(i2400m->barker->data[0]);

	d_printf(2, dev, "finding BCF header for barker %08x\n", barker);
	if (barker == I2400M_NBOOT_BARKER) {
		bcf_hdr = i2400m->fw_hdrs[0];
		d_printf(1, dev, "using BCF header #%u/%08x for non-signed "
			 "barker\n", 0, le32_to_cpu(bcf_hdr->module_id));
		return bcf_hdr;
	}
	for (bcf_itr = i2400m->fw_hdrs; *bcf_itr != NULL; bcf_itr++, i++) {
		bcf_hdr = *bcf_itr;
		if (i2400m_bcf_hdr_match(i2400m, bcf_hdr)) {
			d_printf(1, dev, "hit on BCF hdr #%u/%08x\n",
				 i, le32_to_cpu(bcf_hdr->module_id));
			return bcf_hdr;
		} else
			d_printf(1, dev, "miss on BCF hdr #%u/%08x\n",
				 i, le32_to_cpu(bcf_hdr->module_id));
	}
	dev_err(dev, "cannot find a matching BCF header for barker %08x\n",
		barker);
	return NULL;
}


static
int i2400m_fw_dnload(struct i2400m *i2400m, const struct i2400m_bcf_hdr *bcf,
		     size_t fw_size, enum i2400m_bri flags)
{
	int ret = 0;
	struct device *dev = i2400m_dev(i2400m);
	int count = i2400m->bus_bm_retries;
	const struct i2400m_bcf_hdr *bcf_hdr;
	size_t bcf_size;

	d_fnstart(5, dev, "(i2400m %p bcf %p fw size %zu)\n",
		  i2400m, bcf, fw_size);
	i2400m->boot_mode = 1;
	wmb();		
hw_reboot:
	if (count-- == 0) {
		ret = -ERESTARTSYS;
		dev_err(dev, "device rebooted too many times, aborting\n");
		goto error_too_many_reboots;
	}
	if (flags & I2400M_BRI_MAC_REINIT) {
		ret = i2400m_bootrom_init(i2400m, flags);
		if (ret < 0) {
			dev_err(dev, "bootrom init failed: %d\n", ret);
			goto error_bootrom_init;
		}
	}
	flags |= I2400M_BRI_MAC_REINIT;

	ret = -EBADF;
	bcf_hdr = i2400m_bcf_hdr_find(i2400m);
	if (bcf_hdr == NULL)
		goto error_bcf_hdr_find;

	ret = i2400m_dnload_init(i2400m, bcf_hdr);
	if (ret == -ERESTARTSYS)
		goto error_dev_rebooted;
	if (ret < 0)
		goto error_dnload_init;

	bcf_size = sizeof(u32) * le32_to_cpu(bcf_hdr->size);
	ret = i2400m_dnload_bcf(i2400m, bcf, bcf_size);
	if (ret == -ERESTARTSYS)
		goto error_dev_rebooted;
	if (ret < 0) {
		dev_err(dev, "fw %s: download failed: %d\n",
			i2400m->fw_name, ret);
		goto error_dnload_bcf;
	}

	ret = i2400m_dnload_finalize(i2400m, bcf_hdr, bcf, ret);
	if (ret == -ERESTARTSYS)
		goto error_dev_rebooted;
	if (ret < 0) {
		dev_err(dev, "fw %s: "
			"download finalization failed: %d\n",
			i2400m->fw_name, ret);
		goto error_dnload_finalize;
	}

	d_printf(2, dev, "fw %s successfully uploaded\n",
		 i2400m->fw_name);
	i2400m->boot_mode = 0;
	wmb();		
error_dnload_finalize:
error_dnload_bcf:
error_dnload_init:
error_bcf_hdr_find:
error_bootrom_init:
error_too_many_reboots:
	d_fnend(5, dev, "(i2400m %p bcf %p size %zu) = %d\n",
		i2400m, bcf, fw_size, ret);
	return ret;

error_dev_rebooted:
	dev_err(dev, "device rebooted, %d tries left\n", count);
	
	flags |= I2400M_BRI_SOFT;
	goto hw_reboot;
}

static
int i2400m_fw_bootstrap(struct i2400m *i2400m, const struct firmware *fw,
			enum i2400m_bri flags)
{
	int ret;
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_bcf_hdr *bcf;	

	d_fnstart(5, dev, "(i2400m %p)\n", i2400m);
	bcf = (void *) fw->data;
	ret = i2400m_fw_check(i2400m, bcf, fw->size);
	if (ret >= 0)
		ret = i2400m_fw_dnload(i2400m, bcf, fw->size, flags);
	if (ret < 0)
		dev_err(dev, "%s: cannot use: %d, skipping\n",
			i2400m->fw_name, ret);
	kfree(i2400m->fw_hdrs);
	i2400m->fw_hdrs = NULL;
	d_fnend(5, dev, "(i2400m %p) = %d\n", i2400m, ret);
	return ret;
}


struct i2400m_fw {
	struct kref kref;
	const struct firmware *fw;
};


static
void i2400m_fw_destroy(struct kref *kref)
{
	struct i2400m_fw *i2400m_fw =
		container_of(kref, struct i2400m_fw, kref);
	release_firmware(i2400m_fw->fw);
	kfree(i2400m_fw);
}


static
struct i2400m_fw *i2400m_fw_get(struct i2400m_fw *i2400m_fw)
{
	if (i2400m_fw != NULL && i2400m_fw != (void *) ~0)
		kref_get(&i2400m_fw->kref);
	return i2400m_fw;
}


static
void i2400m_fw_put(struct i2400m_fw *i2400m_fw)
{
	kref_put(&i2400m_fw->kref, i2400m_fw_destroy);
}


int i2400m_dev_bootstrap(struct i2400m *i2400m, enum i2400m_bri flags)
{
	int ret, itr;
	struct device *dev = i2400m_dev(i2400m);
	struct i2400m_fw *i2400m_fw;
	const struct i2400m_bcf_hdr *bcf;	
	const struct firmware *fw;
	const char *fw_name;

	d_fnstart(5, dev, "(i2400m %p)\n", i2400m);

	ret = -ENODEV;
	spin_lock(&i2400m->rx_lock);
	i2400m_fw = i2400m_fw_get(i2400m->fw_cached);
	spin_unlock(&i2400m->rx_lock);
	if (i2400m_fw == (void *) ~0) {
		dev_err(dev, "can't load firmware now!");
		goto out;
	} else if (i2400m_fw != NULL) {
		dev_info(dev, "firmware %s: loading from cache\n",
			 i2400m->fw_name);
		ret = i2400m_fw_bootstrap(i2400m, i2400m_fw->fw, flags);
		i2400m_fw_put(i2400m_fw);
		goto out;
	}

	
	for (itr = 0, bcf = NULL, ret = -ENOENT; ; itr++) {
		fw_name = i2400m->bus_fw_names[itr];
		if (fw_name == NULL) {
			dev_err(dev, "Could not find a usable firmware image\n");
			break;
		}
		d_printf(1, dev, "trying firmware %s (%d)\n", fw_name, itr);
		ret = request_firmware(&fw, fw_name, dev);
		if (ret < 0) {
			dev_err(dev, "fw %s: cannot load file: %d\n",
				fw_name, ret);
			continue;
		}
		i2400m->fw_name = fw_name;
		ret = i2400m_fw_bootstrap(i2400m, fw, flags);
		release_firmware(fw);
		if (ret >= 0)	
			break;
		i2400m->fw_name = NULL;
	}
out:
	d_fnend(5, dev, "(i2400m %p) = %d\n", i2400m, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(i2400m_dev_bootstrap);


void i2400m_fw_cache(struct i2400m *i2400m)
{
	int result;
	struct i2400m_fw *i2400m_fw;
	struct device *dev = i2400m_dev(i2400m);

	
	spin_lock(&i2400m->rx_lock);
	i2400m_fw = i2400m->fw_cached;
	spin_unlock(&i2400m->rx_lock);
	if (i2400m_fw != NULL && i2400m_fw != (void *) ~0) {
		i2400m_fw_put(i2400m_fw);
		WARN(1, "%s:%u: still cached fw still present?\n",
		     __func__, __LINE__);
	}

	if (i2400m->fw_name == NULL) {
		dev_err(dev, "firmware n/a: can't cache\n");
		i2400m_fw = (void *) ~0;
		goto out;
	}

	i2400m_fw = kzalloc(sizeof(*i2400m_fw), GFP_ATOMIC);
	if (i2400m_fw == NULL)
		goto out;
	kref_init(&i2400m_fw->kref);
	result = request_firmware(&i2400m_fw->fw, i2400m->fw_name, dev);
	if (result < 0) {
		dev_err(dev, "firmware %s: failed to cache: %d\n",
			i2400m->fw_name, result);
		kfree(i2400m_fw);
		i2400m_fw = (void *) ~0;
	} else
		dev_info(dev, "firmware %s: cached\n", i2400m->fw_name);
out:
	spin_lock(&i2400m->rx_lock);
	i2400m->fw_cached = i2400m_fw;
	spin_unlock(&i2400m->rx_lock);
}


void i2400m_fw_uncache(struct i2400m *i2400m)
{
	struct i2400m_fw *i2400m_fw;

	spin_lock(&i2400m->rx_lock);
	i2400m_fw = i2400m->fw_cached;
	i2400m->fw_cached = NULL;
	spin_unlock(&i2400m->rx_lock);

	if (i2400m_fw != NULL && i2400m_fw != (void *) ~0)
		i2400m_fw_put(i2400m_fw);
}

