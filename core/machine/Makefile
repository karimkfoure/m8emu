CC = gcc
PYTHON ?= python3
ROOT := $(abspath ../..)
COMMON_GIT_DIR := $(shell git -C "$(ROOT)" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)
REFERENCE_ROOT ?= $(if $(M8_REFERENCE_ROOT),$(M8_REFERENCE_ROOT),$(if $(COMMON_GIT_DIR),$(abspath $(dir $(COMMON_GIT_DIR))),$(ROOT)))
STOCK_UNICORN = $(wildcard $(REFERENCE_ROOT)/teensy-emu/.venv/lib/python*/site-packages/unicorn/lib/libunicorn.so.2)
UNICORN_LIB ?= $(if $(M8_UNICORN_LIBRARY),$(M8_UNICORN_LIBRARY),$(firstword $(wildcard $(ROOT)/build/unicorn/libunicorn.so.2 $(REFERENCE_ROOT)/build/unicorn/libunicorn.so.2 $(STOCK_UNICORN))))
MACHINE_TESTS := $(ROOT)/tests/machine
AOT_DIR ?= $(firstword $(wildcard $(ROOT)/aot $(ROOT)/aot))
CPPFLAGS += -I. -I../engine -I../storage -I../platform -I../include -I../include/m8emu -I$(AOT_DIR) -I$(ROOT)/third_party/unicorn/include -Ithird_party
M8RT_CALLBACK_TIMING ?= 1
CPPFLAGS += -DM8RT_CALLBACK_TIMING_DEFAULT=$(M8RT_CALLBACK_TIMING)
CFLAGS ?= -O2
CFLAGS += -std=c11 -fPIC -Wall -Wextra
LDFLAGS += -Wl,-rpath,$(dir $(abspath $(UNICORN_LIB)))
LDLIBS += $(UNICORN_LIB) -lm -pthread
SOURCES = core.c mmio.c usb.c audio.c audit.c watchdog.c ../storage/storage.c ../storage/nor.c ../platform/stall.c ../engine/native-live.c ../engine/native-boot-app.c
OBJECTS = $(SOURCES:.c=.o)
$(OBJECTS): CPPFLAGS += -DM8RT_LOCAL_CALLS
.PHONY: all clean check check-differential check-native-boot-app FORCE
all: libm8rt.so m8rt
.unicorn-library: FORCE
	@printf '%s\n' '$(abspath $(UNICORN_LIB))' | cmp -s - $@ || printf '%s\n' '$(abspath $(UNICORN_LIB))' > $@
libm8rt.so: $(OBJECTS) $(UNICORN_LIB) .unicorn-library
	$(CC) -shared $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)
m8rt: cli.o libm8rt.so
	$(CC) $(LDFLAGS) -Wl,-rpath,'$$ORIGIN' -o $@ cli.o -L. -lm8rt
%.o: %.c internal.h ../include/m8emu/m8rt.h ../storage/storage.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
audio.o: CFLAGS += -fno-fast-math -frounding-math
core.o: ../engine/native-services.h ../include/m8emu/native-engine.h $(AOT_DIR)/native_events.h $(AOT_DIR)/native_regions.h
core.o mmio.o: mmio-state.h
test_core.o: $(MACHINE_TESTS)/test_core.c internal.h ../include/m8emu/m8rt.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
$(OBJECTS) test_core.o: ../include/m8emu/native-live.h ../include/m8emu/native-engine.h
../engine/native-live.o ../engine/native-boot-app.o: ../engine/native-services.h
test_core: test_core.o libm8rt.so
	$(CC) $(LDFLAGS) -Wl,-rpath,'$$ORIGIN' -o $@ test_core.o -L. -lm8rt $(LDLIBS)
test_usb: $(MACHINE_TESTS)/usb_test.c usb.c internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -o $@ $(MACHINE_TESTS)/usb_test.c usb.c
test_usb_services: $(MACHINE_TESTS)/usb_service_test.c $(MACHINE_TESTS)/usb_test.c usb.c internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -UNDEBUG -Werror -o $@ $(MACHINE_TESTS)/usb_service_test.c usb.c
test_watchdog: $(MACHINE_TESTS)/watchdog_test.c libm8rt.so
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror $(LDFLAGS) -Wl,-rpath,'$$ORIGIN' -o $@ $(MACHINE_TESTS)/watchdog_test.c -L. -lm8rt $(LDLIBS)
test_nor: $(MACHINE_TESTS)/nor_test.c libm8rt.so
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wl,-rpath,'$$ORIGIN' -o $@ $(MACHINE_TESTS)/nor_test.c -L. -lm8rt $(LDLIBS)
clean:
	rm -f $(OBJECTS) cli.o test_core.o libm8rt.so m8rt test_core test_usb test_usb_services test_watchdog test_nor .unicorn-library
check: all test_core test_usb test_usb_services test_watchdog test_nor
	./test_core
	./test_usb
	./test_usb_services
	./test_watchdog
	./test_nor
check-differential: check
	$(PYTHON) -B $(MACHINE_TESTS)/test_mmio.py --unicorn-library $(UNICORN_LIB)
	$(PYTHON) -B $(MACHINE_TESTS)/test_storage.py
	$(PYTHON) -B $(MACHINE_TESTS)/usb_diff.py
	$(PYTHON) -B $(MACHINE_TESTS)/test_audio.py

check-native-boot-app:
	$(PYTHON) -B $(MACHINE_TESTS)/native_boot_irq_control.py --working-tree --native --app-guard \
	  --engine "$(abspath $(UNICORN_LIB))" --output "$(ROOT)/build/native-boot-app-irq-controls"
