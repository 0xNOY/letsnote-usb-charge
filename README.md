# letsnote-usb-charge

Linux driver for power-off charging from the `CHG` USB port on Panasonic
Let's Note CF-SZ6 computers.

Tested on a CF-SZ6-1L with BIOS V1.11L10 and Linux 7.1.4. The driver invokes a
vendor-specific SMI, so save your work before loading it for the first time.

## Build

Install `base-devel` and the headers matching the running kernel. DKMS is only
needed for the persistent installation described below. On Arch Linux:

```bash
sudo pacman -S --needed base-devel linux-headers
make W=1 check
```

`make check` builds the kernel module and command-line tool, then runs the
protocol test.

## Usage

Load the module before using the command-line tool:

```bash
sudo insmod ./panasonic_usb_charge.ko
./letsnote-usb-charge status
sudo ./letsnote-usb-charge enable
```

| Command | Effect |
| --- | --- |
| `status` | Show the current firmware settings. |
| `enable` | Enable charging from the `CHG` port while powered off. |
| `disable` | Disable charging while powered off. |
| `ac-only` | Require the AC adapter for powered-off charging. |
| `battery-ok` | Allow powered-off charging from the laptop battery. |

The charging and power-source settings are independent and are stored by the
firmware. For example, use `enable` followed by `ac-only` to charge while
powered off only when the AC adapter is connected.

## DKMS installation

```bash
sudo pacman -S --needed dkms
sudo make dkms-install
sudo modprobe panasonic_usb_charge
```

To load the module at boot, put `panasonic_usb_charge` in
`/etc/modules-load.d/panasonic-usb-charge.conf`. Remove it with
`sudo make dkms-uninstall`.

The driver exposes `always_on_charge`, `ac_only`, and read-only `raw_flags`
under `/sys/bus/platform/devices/MAT0021:00/`.

Only CF-SZ6 DMI product names are accepted by default. `force=1` bypasses this
check for development on other Panasonic models.

Protocol details are in [docs/reverse-engineering.md](docs/reverse-engineering.md).
