# Protocol notes

The implementation was derived from static analysis of software installed on
a CF-SZ6. Panasonic binaries are not included in this repository.

## Analyzed files

| File | SHA-256 |
| --- | --- |
| `Control/Ui0024/UsbCharge.exe` | `91f6c53618519a5cfe7cdfb3f868108ebdd08a55c5247a31be6a8662b82f5a48` |
| `Windows/System32/drivers/sid0021.sys` | `afb534b29d1287ae248e2906022615e5e89846c4db73568040e53090f9c88b0a` |

`sid0040.inf` version 5.0.1200.0 associates `sid0021.sys` with
`ACPI\\MAT0021`. Linux exposes it as `MAT0021:00` at ACPI path
`\\_SB_.MISC`.

## USB charge packet

CF-SZ6 uses this 12-byte MISC packet:

| Offset | Size | Value |
| ---: | ---: | --- |
| 0 | 1 | function `0x1e`; firmware result after the call |
| 1 | 1 | subfunction `0x02` |
| 2 | 2 | input size `0x000c` |
| 4 | 2 | output size `0x000c` |
| 6 | 2 | reserved |
| 8 | 2 | selector `0x8bc0` |
| 10 | 1 | query `0x80` or setting flags |
| 11 | 1 | query `0x00` or write mask `0xff` |

Write flags are `0x02`, plus `0x04` for power-off charging and `0x08` for
AC-only operation. The current flags are queried before each write.

## SMI transport

The Windows driver:

1. maps physical memory at `0xf0000..0xfffff`;
2. searches from `0xfc000` in `0x80`-byte steps for `MEI_`;
3. validates the table's 8-bit additive checksum;
4. reads ASMI type at offset `0x17`, the SMI port at `0x52`, and command value
   at `0x54`;
5. places the packet's 32-bit physical address in `ESI`; and
6. executes `OUT DX, AL`.

The Linux driver follows the same sequence. It queries the firmware during
probe and writes only through root-owned sysfs attributes.

## CF-SZ6 validation

Test system: CF-SZ6-1L, BIOS V1.11L10, Linux 7.1.4.

- MISC table: `0xfe600`
- ASMI type: `2`
- SMI port: `0xb2`
- initial flags: `0x10`
- flags after enabling power-off charging: `0x16`

The driver discovers these values at runtime; they are not hard-coded.

## References

- [Panasonic PC Settings Utility](https://apps.microsoft.com/detail/9n960x393mtv)
- [Panasonic USB Charge Setting Utility](https://global-pc-support.connect.panasonic.com/dldocs/69612)
- [Linux `panasonic-laptop` driver](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/platform/x86/panasonic-laptop.c)
