# letsnote-usb-charge

Linux support for the `CHG` USB port's power-off (always-on) charging feature
on the Panasonic Let's Note CF-SZ6.

The project contains an independent GPL-licensed kernel driver for Panasonic's
`ACPI\\MAT0021` System Interface Device and a small command-line frontend. It
was derived by static analysis of the official Panasonic PC Settings Utility;
see [the reverse-engineering notes](docs/reverse-engineering.md).

The read path has been validated on a CF-SZ6-1L with BIOS V1.11L10 and Linux
7.1.4: the driver found a checksum-valid MISC table, completed the firmware
query, and reported the same setting consistently through all sysfs controls.

> [!CAUTION]
> This driver invokes a vendor-specific System Management Interrupt. It is
> restricted to CF-SZ6 by default, discovers the SMI parameters from the BIOS,
> validates the firmware table checksum, and performs no write during probe.
> Nevertheless, use it at your own risk. Save work before the first load.

## Requirements

- x86-64 Linux on a Panasonic CF-SZ6
- kernel headers for the running kernel
- GCC and Make
- root access to load the module and change the setting

On Arch Linux:

```console
$ sudo pacman -S --needed base-devel linux-headers dkms
```

## Build and first read-only test

```console
$ make check
$ sudo insmod ./panasonic_usb_charge.ko
$ dmesg | tail -n 20
$ ./letsnote-usb-charge status
```

The expected kernel message includes `ready`, an ASMI type and SMI port. Loading
the module queries current state but does not change it. If probe cannot verify
the firmware table or query the setting successfully, it refuses to bind and
creates no control files.

## Usage

Enable charging from the `CHG` port while the computer is powered off:

```console
$ sudo ./letsnote-usb-charge enable
```

Restrict powered-off charging to times when the AC adapter is connected:

```console
$ sudo ./letsnote-usb-charge ac-only
```

Inspect or disable it:

```console
$ ./letsnote-usb-charge status
$ sudo ./letsnote-usb-charge disable
```

The firmware setting is persistent, as it is when changed by Panasonic's
Windows utility. Panasonic documents that battery-powered off-state charging
stops below 10% battery and increases power use.

## Persistent installation with DKMS

From a release/source directory:

```console
$ sudo make dkms-install
$ sudo modprobe panasonic_usb_charge
```

To load the driver automatically, create `/etc/modules-load.d/panasonic-usb-charge.conf`
containing:

```text
panasonic_usb_charge
```

Uninstall with `sudo make dkms-uninstall`.

## Interface

After a successful probe, these files appear under
`/sys/bus/platform/devices/MAT0021:00/`:

- `always_on_charge` — `1` enables power-off charging.
- `ac_only` — `1` prevents power-off charging from the internal battery.
- `raw_flags` — raw firmware status byte, read-only.

Only root can write the settings. The driver serializes calls and uses a
read-modify-write sequence to preserve the companion setting.

## Supported hardware

Only DMI product names beginning with `CFSZ6` are enabled by default. The module
has a `force=1` parameter for developers investigating another Panasonic model,
but forcing an unverified model is not recommended.
