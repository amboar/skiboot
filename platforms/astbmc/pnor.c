/* Copyright 2013-2014 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <skiboot.h>
#include <device.h>
#include <console.h>
#include <opal.h>
#include <libflash/ipmi-hiomap.h>
#include <libflash/mbox-flash.h>
#include <libflash/libflash.h>
#include <libflash/libffs.h>
#include <libflash/blocklevel.h>
#include <ast.h>

#include "astbmc.h"

enum ast_flash_style {
    raw_flash,
    raw_mem,
    ipmi_hiomap,
    mbox_hiomap,
};

int pnor_init(void)
{
	struct spi_flash_ctrl *pnor_ctrl = NULL;
	struct blocklevel_device *bl = NULL;
	enum ast_flash_style style;
	int rc;

	if (ast_lpc_fw_needs_hiomap()) {
		style = ipmi_hiomap;
		rc = ipmi_hiomap_init(&bl);
		if (rc) {
			style = mbox_hiomap;
			rc = mbox_flash_init(&bl);
		}
	} else {
		/* Open controller and flash. If the LPC->AHB doesn't point to
		 * the PNOR flash base we assume we're booting from BMC system
		 * memory (or some other place setup by the BMC to support LPC
		 * FW reads & writes).
		 */

		if (ast_lpc_fw_maps_flash()) {
			style = raw_flash;
			rc = ast_sf_open(AST_SF_TYPE_PNOR, &pnor_ctrl);
		} else {
			printf("PLAT: Memboot detected\n");
			style = raw_mem;
			rc = ast_sf_open(AST_SF_TYPE_MEM, &pnor_ctrl);
		}
		if (rc) {
			prerror("PLAT: Failed to open PNOR flash controller\n");
			goto fail;
		}

		rc = flash_init(pnor_ctrl, &bl, NULL);
	}

	if (rc) {
		prerror("PLAT: Failed to open init PNOR driver\n");
		goto fail;
	}

	rc = flash_register(bl);
	if (!rc)
		return 0;

fail:
	if (bl) {
		switch (style) {
		case raw_flash:
		case raw_mem:
			flash_exit(bl);
			break;
		case ipmi_hiomap:
			ipmi_hiomap_exit(bl);
			break;
		case mbox_hiomap:
			mbox_flash_exit(bl);
			break;
		}
	}
	if (pnor_ctrl)
		ast_sf_close(pnor_ctrl);

	return rc;
}
