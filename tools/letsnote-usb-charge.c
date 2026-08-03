// SPDX-License-Identifier: GPL-2.0-or-later
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_PATH "/sys/bus/platform/devices/MAT0021:00"

static void usage(FILE *stream)
{
	fprintf(stream,
		"Usage: letsnote-usb-charge COMMAND\n\n"
		"Commands:\n"
		"  status        Show the current firmware settings\n"
		"  enable        Enable USB charging while powered off\n"
		"  disable       Disable USB charging while powered off\n"
		"  ac-only       Charge while powered off only on AC power\n"
		"  battery-ok    Permit powered-off charging on battery\n");
}

static void attribute_path(char *buffer, size_t size, const char *name)
{
	int written = snprintf(buffer, size, "%s/%s", DEVICE_PATH, name);

	if (written < 0 || (size_t)written >= size) {
		fprintf(stderr, "letsnote-usb-charge: attribute path is too long\n");
		exit(EXIT_FAILURE);
	}
}

static unsigned int read_attribute(const char *name)
{
	char path[256];
	char value[32];
	char *end;
	unsigned long parsed;
	ssize_t count;
	int fd;

	attribute_path(path, sizeof(path), name);
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr,
			"letsnote-usb-charge: cannot open %s: %s\n"
			"Is panasonic_usb_charge loaded?\n",
			path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	count = read(fd, value, sizeof(value) - 1);
	if (count < 0) {
		fprintf(stderr, "letsnote-usb-charge: cannot read %s: %s\n",
			path, strerror(errno));
		close(fd);
		exit(EXIT_FAILURE);
	}
	close(fd);
	value[count] = '\0';
	errno = 0;
	parsed = strtoul(value, &end, 0);
	if (errno || end == value) {
		fprintf(stderr, "letsnote-usb-charge: invalid value in %s\n", path);
		exit(EXIT_FAILURE);
	}
	return (unsigned int)parsed;
}

static void write_attribute(const char *name, bool enabled)
{
	char path[256];
	const char value[] = { enabled ? '1' : '0', '\n' };
	ssize_t count;
	int fd;

	attribute_path(path, sizeof(path), name);
	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "letsnote-usb-charge: cannot open %s: %s\n",
			path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	count = write(fd, value, sizeof(value));
	if (count != (ssize_t)sizeof(value)) {
		fprintf(stderr, "letsnote-usb-charge: cannot write %s: %s\n",
			path, count < 0 ? strerror(errno) : "short write");
		close(fd);
		exit(EXIT_FAILURE);
	}
	close(fd);
}

static void show_status(void)
{
	unsigned int always_on = read_attribute("always_on_charge");
	unsigned int ac_only = read_attribute("ac_only");
	unsigned int raw = read_attribute("raw_flags");

	printf("Always-on charging: %s\n", always_on ? "enabled" : "disabled");
	printf("AC-only:            %s\n", ac_only ? "enabled" : "disabled");
	printf("Firmware flags:     0x%02x\n", raw & 0xff);
}

int main(int argc, char **argv)
{
	const char *command = argc > 1 ? argv[1] : "status";

	if (!strcmp(command, "-h") || !strcmp(command, "--help") ||
	    !strcmp(command, "help")) {
		usage(stdout);
		return EXIT_SUCCESS;
	}
	if (argc > 2) {
		usage(stderr);
		return 2;
	}
	if (!strcmp(command, "status")) {
		show_status();
	} else if (!strcmp(command, "enable")) {
		write_attribute("always_on_charge", true);
		show_status();
	} else if (!strcmp(command, "disable")) {
		write_attribute("always_on_charge", false);
		show_status();
	} else if (!strcmp(command, "ac-only")) {
		write_attribute("ac_only", true);
		show_status();
	} else if (!strcmp(command, "battery-ok")) {
		write_attribute("ac_only", false);
		show_status();
	} else {
		usage(stderr);
		return 2;
	}

	return EXIT_SUCCESS;
}
