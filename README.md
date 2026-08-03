# letsnote-usb-charge

Linux driver for power-off charging from the `CHG` USB port on Panasonic
Let's Note CF-SZ6 computers.

Tested on a CF-SZ6-1L with BIOS V1.11L10 and Linux 7.1.4. The driver invokes a
vendor-specific SMI, so save your work before loading it for the first time.

## Build

Install `base-devel`, the headers for the running kernel, and optionally DKMS.
On Arch Linux:

```console
$ sudo pacman -S --needed base-devel linux-headers dkms
$ make check
$ sudo insmod ./panasonic_usb_charge.ko
```

## Usage

```console
$ ./letsnote-usb-charge status
$ sudo ./letsnote-usb-charge enable
$ sudo ./letsnote-usb-charge disable
$ sudo ./letsnote-usb-charge ac-only
$ sudo ./letsnote-usb-charge battery-ok
```

`enable` allows charging while the computer is powered off. `ac-only` limits
it to times when the AC adapter is connected. The setting is stored by the
firmware.

## DKMS installation

```console
$ sudo make dkms-install
$ sudo modprobe panasonic_usb_charge
```

To load the module at boot, put `panasonic_usb_charge` in
`/etc/modules-load.d/panasonic-usb-charge.conf`. Remove it with
`sudo make dkms-uninstall`.

The driver exposes `always_on_charge`, `ac_only`, and read-only `raw_flags`
under `/sys/bus/platform/devices/MAT0021:00/`.

Only CF-SZ6 DMI product names are accepted by default. `force=1` bypasses this
check for development on other Panasonic models.

Protocol details are in [docs/reverse-engineering.md](docs/reverse-engineering.md).
