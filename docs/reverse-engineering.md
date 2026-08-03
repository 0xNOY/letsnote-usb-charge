# Reverse-engineering notes

## Scope and provenance

This project is an independent Linux implementation based on static analysis
of software already installed on a CF-SZ6. No Panasonic executable, driver,
resource, cryptographic key, or decompiled source is redistributed.

The analyzed Microsoft Store package was
`PanasonicCorporation.PanasonicPCSettingsUtility_10.12910.100.0_x64__eewqyxmkz9mn0`.
The relevant executable was `Control/Ui0024/UsbCharge.exe`:

- size: 257,728 bytes
- PE timestamp: 2017-07-18 18:50:21
- SHA-256: `91f6c53618519a5cfe7cdfb3f868108ebdd08a55c5247a31be6a8662b82f5a48`
- embedded product name: `USB Charge Setting Utility`

The Windows System Interface Device driver installed for `ACPI\\MAT0021` was
`sid0021.sys`:

- SHA-256: `afb534b29d1287ae248e2906022615e5e89846c4db73568040e53090f9c88b0a`
- embedded description: `System Interface Device Driver - 0021`

The driver-store INF (`sid0040.inf`, version `5.0.1200.0`) associates
`ACPI\\MAT0021` with that driver. Linux exposes the same firmware device as
`/sys/bus/acpi/devices/MAT0021:00`, with ACPI path `\\_SB_.MISC`.

## Application protocol

The PC Settings Utility calls the vendor driver with two consecutive MISC
packets wrapped in the driver's authenticated IOCTL transport. The inner packet
used on CF-SZ6 (the application's `LIGHT` model path) has this 12-byte layout:

| Offset | Size | Meaning |
| ---: | ---: | --- |
| 0 | 1 | function (`0x1e`; overwritten with result) |
| 1 | 1 | subfunction (`0x02`) |
| 2 | 2 | input size (`0x000c`) |
| 4 | 2 | output size (`0x000c`) |
| 6 | 2 | reserved (`0`) |
| 8 | 2 | USB-charge selector (`0x8bc0`) |
| 10 | 1 | query opcode (`0x80`) or settings flags |
| 11 | 1 | query (`0`) or write mask (`0xff`) |

For a write, bit `0x02` is always set, bit `0x04` controls power-off/always-on
charging, and bit `0x08` selects AC-only operation. The application first
queries the current flags and then constructs a write packet so the two user
settings remain consistent.

## Firmware transport

The Windows driver does not evaluate an ACPI method for this operation. It:

1. maps physical `0xf0000..0xfffff`;
2. searches every `0x80` bytes from `0xfc000` for signature `MEI_`;
3. validates an 8-bit additive checksum, with the length read from table byte
   `0x14` in units of `0x10` bytes;
4. reads ASMI type from table offset `0x17`, the SMI port from `0x52`, and the
   SMI command value from `0x54`;
5. puts the low 32 bits of the physical packet address in `ESI`; and
6. triggers the synchronous SMI with `OUT DX, AL`.

The Linux driver reproduces only this small transport and only on Panasonic
CF-SZ6 systems. It discovers and validates the firmware table at runtime; it
does not hard-code the machine-specific SMI port or command. Probe performs a
read-only query. A firmware write happens only after root writes a sysfs
attribute.

## Hardware validation

The read-only probe and status query were tested on the target CF-SZ6-1L with
BIOS V1.11L10 and Linux 7.1.4. The runtime-discovered values were:

- MISC table physical address: `0xfe600`
- ASMI type: `2` (memory mode)
- SMI trigger port: `0xb2`
- initial firmware flags: `0x10` (always-on and AC-only both disabled)

Enabling always-on charging succeeded and two immediate readbacks both
returned `0x16`. This confirms that bit `0x04` was set while AC-only bit `0x08`
remained clear; the firmware-provided bit `0x10` was also retained.

These values are recorded as validation evidence only. The driver continues to
discover them from firmware and does not use them as constants.

## Public references

- [Panasonic PC Settings Utility in Microsoft Store](https://apps.microsoft.com/detail/9n960x393mtv)
- [Panasonic USB Charge Setting Utility support page](https://global-pc-support.connect.panasonic.com/dldocs/69612)
- [Linux `panasonic-laptop` driver](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/platform/x86/panasonic-laptop.c)
