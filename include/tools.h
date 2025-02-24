/*
 * tools.h
 *
 *  Created on: Feb 21, 2025
 *      Author: Ilko Iliev
 */

#ifndef INCLUDE_TOOLS_H_
#define INCLUDE_TOOLS_H_

#include <types.h>

typedef struct
{
	unsigned in;
	unsigned out;
	int flag_failed;
} pins_t;

extern bool test_pin_pairs(pins_t *base);
extern bool mem_test(uint8_t *addr, int len);

#endif /* INCLUDE_TOOLS_H_ */
