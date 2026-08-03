ifneq ($(KERNELRELEASE),)
obj-m += panasonic_usb_charge.o
panasonic_usb_charge-y := src/driver.o
ccflags-y += -I$(src)/include
else
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: all modules clean check install uninstall dkms-install dkms-uninstall

all: modules letsnote-usb-charge

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) tests/protocol_test letsnote-usb-charge

check: modules letsnote-usb-charge tests/protocol_test
	./tests/protocol_test
	./letsnote-usb-charge --help >/dev/null

letsnote-usb-charge: tools/letsnote-usb-charge.c
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -o $@ $<

tests/protocol_test: tests/protocol_test.c include/protocol.h
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude -o $@ $<

install: modules
	install -Dm644 panasonic_usb_charge.ko /usr/lib/modules/$(shell uname -r)/extra/panasonic_usb_charge.ko
	install -Dm755 letsnote-usb-charge /usr/local/bin/letsnote-usb-charge
	depmod -a

uninstall:
	$(RM) /usr/lib/modules/$(shell uname -r)/extra/panasonic_usb_charge.ko
	$(RM) /usr/local/bin/letsnote-usb-charge
	depmod -a

dkms-install: letsnote-usb-charge
	dkms install .
	install -Dm755 letsnote-usb-charge /usr/local/bin/letsnote-usb-charge

dkms-uninstall:
	dkms remove panasonic-usb-charge/0.1.0 --all
	$(RM) /usr/local/bin/letsnote-usb-charge
endif
