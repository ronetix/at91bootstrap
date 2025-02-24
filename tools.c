/*
 * tools.c
 *
 *  Created on: Feb 21, 2025
 *      Author: Ilko Iliev
 */
#include "autoconf.h"
#include "common.h"
#include "hardware.h"
#include "pmc.h"
#include "debug.h"
#include "gpio.h"
#include "string.h"
#include "arch/at91_pio.h"
#include "include/tools.h"

#define GPIOTEST_PREFIX		"GPIO-TEST: "
#define MEMTEST_PREFIX		"MEM-TEST: "

extern uint32_t crc32 (uint32_t crc, const uint8_t *p, int len);

#ifdef CONFIG_GPIO_TEST
/*!
 * @brief Count number of pin pairs
 * @param base
 * @return number of pin pairs
 */
static int get_num_pin_pairs(pins_t *base)
{
	/* count number of pin pairs */
	int n = 0;
	while (base->in || base->out)
	{
		base++;
		n++;
	}

	return n;
}

/*!
 * @brief Print pin name
 * @param pin_desc - pin
 * @param suffix - string to append
 */
static void print_pin_name(unsigned pin_desc, char *suffix)
{
	int pio = pin_desc / PIO_NUM_IO;
	int pin = pin_desc % PIO_NUM_IO;

	dbg_info("P%c%d%s", 'A' + pio, pin, suffix ? suffix : "");
};

/*!
 * @brief Initialize GPIO pin pairs
 * 		  A pair consists of one output and one input
 * @param base - list with pairs
 * @param n - number of pin pairs
 */
static void init_test_pins(pins_t *base, int n)
{
	struct pio_desc pin_desc[3];

	dbg_info(GPIOTEST_PREFIX "initializing %d pin pairs: ", n);

	pin_desc[0].pin_name = "PIN-OUT";
	pin_desc[0].default_value = 0;
	pin_desc[0].attribute = PIO_DEFAULT;
	pin_desc[0].type = PIO_OUTPUT;
	pin_desc[1].pin_name = "PIN-IN";
	pin_desc[1].default_value = 0;
	pin_desc[1].attribute = PIO_PULLUP;
	pin_desc[1].type = PIO_INPUT;
	pin_desc[2].pin_name = NULL;

	pins_t *p = base;

	while (n--)
	{
		pin_desc[0].pin_num = p->out;
		pin_desc[1].pin_num = p->in;
		p->flag_failed = false;

		pio_configure(pin_desc);
		p++;
	}

	dbg_info("done\n", n);
}

/*!
 * @brief Test all pin pairs
 * @param base
 */
bool test_pin_pairs(pins_t *base)
{
	pins_t *p = base;
	int i, k, v, q, n, num_err = 0;

	/* count number of pin pairs */
	n = get_num_pin_pairs(base);

	init_test_pins(base, n);

	for (i = 0; i < n; i++)
	{
		/* set a walking '1' to one output */
		p = base;
		for (k = 0; k < n; k++, p++)
		{
			v = (k == i) ? 1 : 0;
			pio_set_value(p->out, v);
		}

		/* check the inputs, only one should be '1' */
		p = base;
		for (k = 0; k < n; k++, p++)
		{
			if (p->flag_failed)
				continue;

			v = (k == i) ? 1 : 0;
			q = pio_get_value(p->in);

			if (q != v)
			{
				p->flag_failed = true;
				dbg_info(GPIOTEST_PREFIX);
				print_pin_name(p->out, " -> ");
				print_pin_name(p->in, " - failed\n");
				num_err++;
			}
			else
				p->flag_failed = false;
		}
	}

	dbg_info(GPIOTEST_PREFIX "%s\n", num_err ? "failed"  : "OK");

	return num_err;
}
#endif

/*!
 * @brief Test memory
 * @param addr
 * @param len
 * @return
 */
bool mem_test(uint8_t *addr, int len)
{
	int i, l = len;
	uint32_t *p = (uint32_t *)addr;
	uint32_t crc1, crc2;

	dbg_info(MEMTEST_PREFIX"start %x, length %x ... ", (uint32_t)addr, len);

	len /= 4;		/* convert to double words */

	for (i = 0; i < len; i++)
		*p++ = i;

	crc1 = crc32(0, (const uint8_t *)addr, l);

	p = (uint32_t *)addr;
	for (i = 0; i < len; i++)
		if (*p++ != i)
		{
			dbg_info("failed at %x\n", (uint32_t)p);
			return true;
		}

	crc2 = crc32(0, (const uint8_t *)addr, l);
	if (crc1 != crc2)
	{
		dbg_info("CRC32 check failed (1)\n");
		return true;
	}

	p = (uint32_t *)addr;
	for (i = 0; i < len; i++)
		*p++ = 0xFFFFFFFF - i;

	crc1 = crc32(0, (const uint8_t *)addr, l);

	p = (uint32_t *)addr;
	for (i = 0; i < len; i++)
	{
		if (*p++ != 0xFFFFFFFF - i)
		{
			dbg_info("failed at %x\n", (uint32_t)p);
			return 1;
		}
	}

	crc2 = crc32(0, (const uint8_t *)addr, l);
	if (crc1 != crc2)
	{
		dbg_info("CRC32 check failed (2)\n");
		return 1;
	}

	dbg_info("done\n");
	return false;
}
