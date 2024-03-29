/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2006, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "autoconf.h"
#include "common.h"
#include "board.h"
#include "usart.h"
#include "slowclk.h"
#include "board_hw_info.h"
#include "tz_utils.h"
#include "pm.h"
#include "act8865.h"
#include "backup.h"
#include "secure.h"
#include "sfr_aicredir.h"
#include "debug.h"

#ifdef CONFIG_HW_DISPLAY_BANNER
static void display_banner (void)
{
	usart_puts(BANNER);
}
#endif

struct image_info boot_images[] =
{
	{
		.offset = IMG_ADDRESS,
		.length = IMG_SIZE,
		.dest = (unsigned char *)JUMP_ADDR,

#ifdef CONFIG_OF_LIBFDT
		.of_offset = OF_OFFSET,
		.of_dest = (unsigned char *)OF_ADDRESS,
#endif
	},
#ifdef CONFIG_REDUNDANT_IMAGES
	{
		.offset = IMG_1_ADDRESS,
		.length = IMG_SIZE,
		.dest = (unsigned char *)JUMP_ADDR
	},
	{
		.offset = IMG_2_ADDRESS,
		.length = IMG_SIZE,
		.dest = (unsigned char *)JUMP_ADDR
	},
#endif
	{
		.length = 0
	}
};

#define NUM_IMAGES	(sizeof(boot_images) / sizeof(boot_images[0]))

static int mem_test(uint8_t *addr, int len)
{
	int i;
	uint32_t *p = (uint32_t *)addr;

	dbg_info("MEMTEST: start %x, length %x ... ", (uint32_t)addr, len);

	len /= 4;		/* convert to double words */

	for (i = 0; i < len; i++)
		*p++ = i;

	p = (uint32_t *)addr;
	for (i = 0; i < len; i++)
		if (*p++ != i)
		{
			dbg_info("failed at %x\n", (uint32_t)p);
			return 1;
		}

	p = (uint32_t *)addr;
	for (i = 0; i < len; i++)
		*p++ = 0xFFFFFFFF - i;

	p = (uint32_t *)addr;
	for (i = 0; i < len; i++)
		if (*p++ != 0xFFFFFFFF - i)
		{
			dbg_info("failed at %x\n", (uint32_t)p);
			return 1;
		}

	dbg_info("done\n");
	return 0;
}

int main(void)
{
#if !defined(CONFIG_LOAD_NONE)
	struct image_info image;
#endif
	int ret = 0;

#ifdef CONFIG_HW_INIT
	hw_init();
#endif

#ifdef CONFIG_OCMS_STATIC
	ocms_init_keys();
	ocms_enable();
#endif

#if defined(CONFIG_SCLK)
#if !defined(CONFIG_SCLK_BYPASS)
	slowclk_enable_osc32();
#endif
#elif defined(CONFIG_SCLK_INTRC)
	slowclk_switch_rc32();
#endif

#ifdef CONFIG_BACKUP_MODE
	ret = backup_mode_resume();
	if (ret) {
		/* Backup+Self-Refresh mode detected... */
#ifdef CONFIG_REDIRECT_ALL_INTS_AIC
		redirect_interrupts_to_nsaic();
#endif
		slowclk_switch_osc32();

		/* ...jump to Linux here */
		return ret;
	}
	usart_puts("Backup mode enabled\n");
#endif

#ifdef CONFIG_HW_DISPLAY_BANNER
	display_banner();
#endif

#ifdef CONFIG_REDIRECT_ALL_INTS_AIC
	redirect_interrupts_to_nsaic();
#endif

#ifdef CONFIG_LOAD_HW_INFO
	load_board_hw_info();
#endif

#ifdef CONFIG_PM
	at91_board_pm();
#endif

#ifdef CONFIG_ACT8865
	act8865_workaround();

	act8945a_suspend_charger();
#endif

#if !defined(CONFIG_LOAD_NONE) && !defined(CONFIG_SKIP_COPY_IMAGE)
	init_load_image(&image);

#if defined(CONFIG_SECURE)
	image.dest -= sizeof(at91_secure_header_t);
#endif

	/* test destination memory */
	ret = mem_test((uint8_t *)JUMP_ADDR, IMG_SIZE);
	while (ret);

	struct image_info *img = boot_images;
	do {

#if defined(CONFIG_SECURE)
		img->dest -= sizeof(at91_secure_header_t);
#endif
		ret = (*load_image)(img);

	} while (ret && ((++img)->length));

#if defined(CONFIG_SECURE)
	if (!ret)
		ret = secure_check(image.dest);
	image.dest += sizeof(at91_secure_header_t);
#endif

#endif

	load_image_done(ret);

#ifdef CONFIG_SCLK
#ifdef CONFIG_SCLK_BYPASS
	slowclk_switch_osc32_bypass();
#else
	slowclk_switch_osc32();
#endif
#endif

#if defined(CONFIG_ENTER_NWD)
	switch_normal_world();

	/* point never reached with TZ support */
#endif

#if !defined(CONFIG_LOAD_NONE)
	return JUMP_ADDR;
#else
	return 0;
#endif
}
