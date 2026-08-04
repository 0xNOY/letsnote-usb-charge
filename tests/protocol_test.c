// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "protocol.h"

int main(void)
{
	static const unsigned char expected_query[USBCHG_PACKET_SIZE] = {
		0x1e, 0x02, 0x0c, 0x00, 0x0c, 0x00,
		0x00, 0x00, 0xc0, 0x8b, 0x80, 0x00,
	};
	static const unsigned char expected_set[USBCHG_PACKET_SIZE] = {
		0x1e, 0x02, 0x0c, 0x00, 0x0c, 0x00,
		0x00, 0x00, 0xc0, 0x8b, 0x0e, 0xff,
	};
	struct usbchg_packet query = usbchg_query_packet();
	struct usbchg_packet set = usbchg_set_packet(1, 1);

	assert(sizeof(struct usbchg_packet) == USBCHG_PACKET_SIZE);
	assert(memcmp(&query, expected_query, sizeof(query)) == 0);
	assert(memcmp(&set, expected_set, sizeof(set)) == 0);
	puts("protocol layout: ok");
	return 0;
}
