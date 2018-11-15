/*-
 * Copyright (c) 2017 Ilya Bakulin.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This file contains functions to work with SDIO cards.
 * All non-static functions should be useable both from the kernel and
 * from the userland.
 * XXX-BZ though they are all currently hidden behind #ifdef _KERNEL.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/sysctl.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h> /* for cam_path */
#include <cam/cam_debug.h>

#include "sdio_subr.h"
#include "opt_cam.h"

#ifdef DEBUG
#define	DPRINTF(...)		printf(__VA_ARGS__)
#else
#define	DPRINTF(...)
#endif

#ifndef _KERNEL
#define	KASSERT(_x, ...)	assert((_x))
#endif

#ifdef _KERNEL
#define warnx(fmt, ...)	\
    CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_PERIPH, (fmt, ##__VA_ARGS__))

static int
sdioerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{

	return(cam_periph_error(ccb, cam_flags, sense_flags, NULL));
}

/* CMD52: direct byte access */
int
sdio_rw_direct(union ccb *ccb, uint8_t func_number, uint32_t addr,
    uint8_t is_write, uint8_t *data, uint8_t *resp)
{
	struct ccb_mmcio *mmcio;
	uint32_t flags;
	uint32_t arg;
	int retval;

	KASSERT((resp != NULL), ("%s resp passed as NULL\n", __func__));
	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE,
	    ("%s(f=%d, wr=%d, addr=%#02x, data=%#02x)\n", __func__,
	    func_number, is_write, addr, (data == NULL ? 0 : *data)));
	mmcio = &ccb->mmcio;

	flags = MMC_RSP_R5 | MMC_CMD_AC;
	arg = SD_IO_RW_FUNC(func_number) | SD_IO_RW_ADR(addr);
	if (is_write)
		arg |= SD_IO_RW_WR | SD_IO_RW_RAW | SD_IO_RW_DAT(*data);
	cam_fill_mmcio(mmcio,
		       /*retries*/ 0,
		       /*cbfcnp*/ NULL,
		       /*flags*/ CAM_DIR_NONE,
		       /*mmc_opcode*/ SD_IO_RW_DIRECT,
		       /*mmc_arg*/ arg,
		       /*mmc_flags*/ flags,
		       /*mmc_data*/ 0,
		       /*timeout*/ 5000);
	retval = cam_periph_runccb(ccb, sdioerror, CAM_FLAG_NONE, 0, NULL);
	if (retval != 0)
		return (retval);

	/* TODO: Add handling of MMC errors */
	*resp = ccb->mmcio.cmd.resp[0] & 0xff;
	return (0);
}

uint8_t
sdio_read_1(union ccb *ccb, uint8_t func_number, uint32_t addr, int *ret)
{
	uint8_t val;

	KASSERT((ret != NULL), ("%s ret passed as NULL\n", __func__));
	*ret = sdio_rw_direct(ccb, func_number, addr, 0, NULL, &val);
	return val;
}

int
sdio_func_read_cis(union ccb *ccb, uint8_t func_number, uint32_t cis_addr,
    struct cis_info *info)
{
	char cis1_info_buf[256];
	char *cis1_info[4];
	int start, i, ch, count, ret;
	uint32_t addr;
	uint8_t tuple_id, tuple_len, tuple_count;

	/* Use to prevent infinite loop in case of parse errors. */
	tuple_count = 0;
	memset(cis1_info_buf, 0, 256);
	do {
		addr = cis_addr;
		tuple_id = sdio_read_1(ccb, 0, addr++, &ret);
		if (tuple_id == SD_IO_CISTPL_END)
			break;
		if (tuple_id == 0) {
			cis_addr++;
			continue;
		}
		tuple_len = sdio_read_1(ccb, 0, addr++, &ret);
		if (tuple_len == 0 && tuple_id != 0x00) {
			warnx("%s: parse error: 0-length tuple %#02x\n",
			    __func__, tuple_id);
			return (-1);
		}

		switch (tuple_id) {
		case SD_IO_CISTPL_VERS_1:
			addr += 2;
			for (count = 0, start = 0, i = 0;
			     (count < 4) && ((i + 4) < 256); i++) {
				ch = sdio_read_1(ccb, 0, addr + i, &ret);
				DPRINTF("%s: count=%d, start=%d, i=%d, got "
				    "(%#02x)\n", __func_, count, start, i, ch);
				if (ch == 0xff)
					break;
				cis1_info_buf[i] = ch;
				if (ch == 0) {
					cis1_info[count] =
					    cis1_info_buf + start;
					start = i + 1;
					count++;
				}
			}
			/* XXX-BZ printfs as part of a parsing API seem odd. */
			printf("Card info:");
			for (i=0; i < 4; i++)
				if (cis1_info[i])
					printf(" %s", cis1_info[i]);
			printf("\n");
			break;
		case SD_IO_CISTPL_MANFID:
			KASSERT((info != NULL),
			     ("%s info passed as NULL\n", __func__));
			info->man_id =  sdio_read_1(ccb, 0, addr++, &ret);
			info->man_id |= sdio_read_1(ccb, 0, addr++, &ret) << 8;
			info->prod_id =  sdio_read_1(ccb, 0, addr++, &ret);
			info->prod_id |= sdio_read_1(ccb, 0, addr, &ret) << 8;
			break;
		case SD_IO_CISTPL_FUNCID:
			/* not sure if we need to parse it? */
			break;
		case SD_IO_CISTPL_FUNCE:
			if (tuple_len < 4) {
				printf("%s: FUNCE is too short: %d\n",
				    __func__, tuple_len);
				break;
			}
			if (func_number == 0)
				/* Skip extended_data. */
				addr++;
			else
				addr += 0xc;
			/* XXX-BZ error checking? */
			info->max_block_size = sdio_read_1(ccb, 0, addr, &ret);
			info->max_block_size |=
			    sdio_read_1(ccb, 0, addr+1, &ret) << 8;
			break;
		default:
			warnx("%s: skipping tuple ID %#02x len %#02x\n",
			    __func__, tuple_id, tuple_len);
		}
		cis_addr += tuple_len + 2;
		tuple_count++;
	} while (tuple_count < 20);

	return (0);
}

uint32_t
sdio_get_common_cis_addr(union ccb *ccb)
{
	uint32_t addr;
	int ret;

	/* XXX-BZ error checking on the first rather than the last, all? */
	addr =  sdio_read_1(ccb, 0, SD_IO_CCCR_CISPTR, &ret);
	addr |= sdio_read_1(ccb, 0, SD_IO_CCCR_CISPTR + 1, &ret) << 8;
	addr |= sdio_read_1(ccb, 0, SD_IO_CCCR_CISPTR + 2, &ret) << 16;
	if (ret != 0) {
		warnx("%s: Failed to read CIS address\n", __func__);
		return (0xff);
	}

	if (addr < SD_IO_CIS_START || addr > SD_IO_CIS_START + SD_IO_CIS_SIZE) {
		warnx("%s: bad CIS address: %#04x\n", __func__, addr);
		addr = 0xff;
	}

	return (addr);
}

int
get_sdio_card_info(union ccb *ccb, struct card_info *ci)
{
	struct mmc_params *mmcp;
	uint32_t cis_addr;
	uint32_t fbr_addr;
	int ret;

	cis_addr = sdio_get_common_cis_addr(ccb);
	if (cis_addr == 0xff)
		return (-1);

	KASSERT((ci != NULL), ("%s ci passed as NULL\n", __func__));
	memset(ci, 0, sizeof(struct card_info));
	ret = sdio_func_read_cis(ccb, 0, cis_addr, &ci->f[0]);
	if (ret !=0)
		return (ret);
	DPRINTF("%s: F0: Vendor %#04x product %#04x max block size %d bytes\n",
	    __func__, ci->f[0].man_id, ci->f[0].prod_id,
	    ci->f[0].max_block_size);

	mmcp = &ccb->ccb_h.path->device->mmc_ident_data;
	for (int i = 1; i <= mmcp->sdio_func_count; i++) {
		/* XXX-BZ magic number */
		fbr_addr = SD_IO_FBR_START * i + 0x9;
		/* XXX-BZ error checking */
		cis_addr =  sdio_read_1(ccb, 0, fbr_addr++, &ret);
		cis_addr |= sdio_read_1(ccb, 0, fbr_addr++, &ret) << 8;
		cis_addr |= sdio_read_1(ccb, 0, fbr_addr++, &ret) << 16;
		sdio_func_read_cis(ccb, i, cis_addr, &ci->f[i]);
		DPRINTF("%s: F%d: Vendor %#04x product %#04x max block size "
		    "%d bytes\n", __func__, i,
		    ci->f[i].man_id, ci->f[i].prod_id, ci->f[i].max_block_size);
		if (ci->f[i].man_id == 0) {
			DPRINTF("%s: F%d doesn't exist\n", __func__, i);
			break;
		}
		ci->num_funcs++;
	}

	return (0);
}

#endif /* _KERNEL */
