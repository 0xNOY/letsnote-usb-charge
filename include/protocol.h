/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LETSNOTE_USB_CHARGE_PROTOCOL_H
#define LETSNOTE_USB_CHARGE_PROTOCOL_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/byteorder/generic.h>
#define USBCHG_PACKED __packed
typedef __le16 usbchg_le16;
#define USBCHG_CPU_TO_LE16(value) cpu_to_le16(value)
#else
#include <stdint.h>
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "The Panasonic MISC protocol implementation currently requires little endian"
#endif
#define USBCHG_PACKED __attribute__((packed))
typedef uint16_t usbchg_le16;
#define USBCHG_CPU_TO_LE16(value) ((uint16_t)(value))
#endif
#define USBCHG_PACKET_SIZE       12U
#define USBCHG_FUNCTION          0x1eU
#define USBCHG_SUBFUNCTION       0x02U
#define USBCHG_SELECTOR          0x8bc0U
#define USBCHG_QUERY             0x80U
#define USBCHG_WRITE_ENABLE      0x02U
#define USBCHG_ALWAYS_ON         0x04U
#define USBCHG_AC_ONLY           0x08U
#define USBCHG_WRITE_MASK        0xffU

struct usbchg_packet {
	uint8_t function;
	uint8_t subfunction;
	usbchg_le16 input_size;
	usbchg_le16 output_size;
	usbchg_le16 reserved;
	usbchg_le16 selector;
	uint8_t flags;
	uint8_t mask;
} USBCHG_PACKED;

static inline struct usbchg_packet usbchg_query_packet(void)
{
	struct usbchg_packet packet = {
		.function = USBCHG_FUNCTION,
		.subfunction = USBCHG_SUBFUNCTION,
		.input_size = USBCHG_CPU_TO_LE16(USBCHG_PACKET_SIZE),
		.output_size = USBCHG_CPU_TO_LE16(USBCHG_PACKET_SIZE),
		.reserved = USBCHG_CPU_TO_LE16(0),
		.selector = USBCHG_CPU_TO_LE16(USBCHG_SELECTOR),
		.flags = USBCHG_QUERY,
		.mask = 0,
	};

	return packet;
}

static inline struct usbchg_packet usbchg_set_packet(int always_on, int ac_only)
{
	struct usbchg_packet packet = usbchg_query_packet();

	packet.flags = USBCHG_WRITE_ENABLE;
	if (always_on)
		packet.flags |= USBCHG_ALWAYS_ON;
	if (ac_only)
		packet.flags |= USBCHG_AC_ONLY;
	packet.mask = USBCHG_WRITE_MASK;
	return packet;
}

#endif
