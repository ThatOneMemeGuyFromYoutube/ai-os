TARGET ?= i386-elf
CC ?= $(TARGET)-gcc
LD ?= $(TARGET)-ld
AS ?= $(TARGET)-as
OBJCOPY ?= $(TARGET)-objcopy
SIZE ?= $(TARGET)-size
PYTHON ?= python3
HOSTCC ?= cc
HOSTCFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror
GRUB_MKRESCUE ?= grub-mkrescue
GRUB_FILE ?= grub-file
QEMU ?= qemu-system-i386
QEMU_TIMEOUT ?= 8
CFLAGS ?= -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Wall -Wextra
LDFLAGS ?= -m elf_i386 -T linker.ld

CC_BIN := $(firstword $(CC))
LD_BIN := $(firstword $(LD))
AS_BIN := $(firstword $(AS))

TINYLANG_EXAMPLES := $(sort $(wildcard toolchain/examples/*.tl))
TINYLANG_IMAGES := $(patsubst toolchain/examples/%.tl,build/examples/%.com,$(TINYLANG_EXAMPLES))
TINYLANG_LISTING_IMAGES := $(patsubst toolchain/examples/%.tl,build/listings/%.com,$(TINYLANG_EXAMPLES))
TINYLANG_LISTINGS := $(patsubst toolchain/examples/%.tl,build/listings/%.lst,$(TINYLANG_EXAMPLES))
.SECONDARY: $(TINYLANG_LISTING_IMAGES)

# `make compile SOURCE=... [OUTPUT=...]` builds one user-selected source.
SOURCE ?= toolchain/examples/hello.tl
OUTPUT ?= build/$(basename $(notdir $(SOURCE))).com
MAP ?=
LISTING ?=

all: build/kernel.bin

build:
	mkdir -p build

build/boot.o: boot/boot.S | build
	$(AS) --32 $< -o $@

build/kernel.o: kernel/kernel.c kernel/gui.h kernel/keyboard.h kernel/ps2_wait.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/gui.o: kernel/gui.c kernel/gui.h kernel/terminal.h kernel/desktop_input.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/keyboard.o: kernel/keyboard.c kernel/keyboard.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/desktop_input.o: kernel/desktop_input.c kernel/desktop_input.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/terminal.o: kernel/terminal.c kernel/terminal.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ps2_wait.o: kernel/ps2_wait.c kernel/ps2_wait.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/test-terminal: kernel/terminal.c kernel/terminal.h tests/test_terminal.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/terminal.c tests/test_terminal.c -o $@

check-terminal: build/test-terminal
	./build/test-terminal

build/test-ps2-wait: kernel/ps2_wait.c kernel/ps2_wait.h tests/test_ps2_wait.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/ps2_wait.c tests/test_ps2_wait.c -o $@

check-ps2-wait: build/test-ps2-wait
	./build/test-ps2-wait

build/test-desktop-input: kernel/desktop_input.c kernel/desktop_input.h tests/test_desktop_input.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/desktop_input.c tests/test_desktop_input.c -o $@

check-desktop-input: build/test-desktop-input
	./build/test-desktop-input

build/test-keyboard: kernel/keyboard.c kernel/keyboard.h tests/test_keyboard.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/keyboard.c tests/test_keyboard.c -o $@

check-keyboard: build/test-keyboard
	./build/test-keyboard

build/kernel.bin: build/boot.o build/kernel.o build/gui.o build/keyboard.o build/desktop_input.o build/terminal.o build/ps2_wait.o linker.ld
	$(LD) $(LDFLAGS) -o $@ build/boot.o build/kernel.o build/gui.o build/keyboard.o build/desktop_input.o build/terminal.o build/ps2_wait.o

build/hello.com: toolchain/tinylang.py toolchain/examples/hello.tl | build
	$(PYTHON) toolchain/tinylang.py toolchain/examples/hello.tl $@

build/examples/%.com: toolchain/examples/%.tl toolchain/tinylang.py | build
	mkdir -p $(dir $@)
	$(PYTHON) toolchain/tinylang.py $< $@

examples: $(TINYLANG_IMAGES)

# Generate a source-correlated listing and matching image for every example.
build/listings/%.com: toolchain/examples/%.tl toolchain/tinylang.py | build
	mkdir -p $(dir $@)
	$(PYTHON) toolchain/tinylang.py $< $@

build/listings/%.lst: toolchain/examples/%.tl build/listings/%.com build/examples/%.com toolchain/tinylang.py | build
	$(PYTHON) toolchain/tinylang.py --listing $@ $< build/listings/$*.com
	test -s "$@"
	cmp build/examples/$*.com build/listings/$*.com

listings: $(TINYLANG_LISTING_IMAGES) $(TINYLANG_LISTINGS)

check-listings: listings examples
	@$(foreach image,$(TINYLANG_LISTING_IMAGES),test -s "$(image)" && ) true

check-examples: examples

# Exercise both the derived default path and a caller-selected nested output.
check-compile: build/hello.com build/examples/hello.com build/examples/arithmetic.com build/examples/jump_over_data.com
	$(MAKE) --no-print-directory compile SOURCE=toolchain/examples/hello.tl
	cmp build/hello.com build/examples/hello.com
	$(MAKE) --no-print-directory compile SOURCE=toolchain/examples/arithmetic.tl OUTPUT=build/compile-check/arithmetic.com
	cmp build/compile-check/arithmetic.com build/examples/arithmetic.com
	$(MAKE) --no-print-directory compile SOURCE=toolchain/examples/jump_over_data.tl OUTPUT=build/compile-check/jump_over_data.com MAP=build/compile-check/jump_over_data.map LISTING=build/compile-check/jump_over_data.lst
	cmp build/compile-check/jump_over_data.com build/examples/jump_over_data.com
	grep -Fx '0x00000105 0x00000005 message' build/compile-check/jump_over_data.map
	grep -Fx '0x0000010C 0x0000000C program' build/compile-check/jump_over_data.map
	grep -Fx '0002 0x00000100  E9 07 00 00 00 | jmp program' build/compile-check/jump_over_data.lst
	grep -Fx '0004 0x0000010C  B8 2A 00 00 00 | program: mov eax, 42' build/compile-check/jump_over_data.lst

program: build/hello.com

# Compile one source without requiring a hand-written Python command.
compile: | build
	@test -n "$(SOURCE)" || { echo "error: set SOURCE to a TinyLang file" >&2; exit 1; }
	@test -f "$(SOURCE)" || { echo "error: TinyLang source not found: $(SOURCE)" >&2; exit 1; }
	@test -n "$(OUTPUT)" || { echo "error: set OUTPUT to a .com destination" >&2; exit 1; }
	@mkdir -p "$(dir $(OUTPUT))"
	@if [ -n "$(MAP)" ] && [ -n "$(LISTING)" ]; then \
		$(PYTHON) toolchain/tinylang.py --map "$(MAP)" --listing "$(LISTING)" "$(SOURCE)" "$(OUTPUT)"; \
	elif [ -n "$(MAP)" ]; then \
		$(PYTHON) toolchain/tinylang.py --map "$(MAP)" "$(SOURCE)" "$(OUTPUT)"; \
	elif [ -n "$(LISTING)" ]; then \
		$(PYTHON) toolchain/tinylang.py --listing "$(LISTING)" "$(SOURCE)" "$(OUTPUT)"; \
	else \
		$(PYTHON) toolchain/tinylang.py "$(SOURCE)" "$(OUTPUT)"; \
	fi

# Validate one edited source without creating or replacing a program image.
check-source:
	@test -n "$(SOURCE)" || { echo "error: set SOURCE to a TinyLang file" >&2; exit 1; }
	@test -f "$(SOURCE)" || { echo "error: TinyLang source not found: $(SOURCE)" >&2; exit 1; }
	$(PYTHON) toolchain/tinylang.py --check "$(SOURCE)"

iso: build/ai-os.iso

build/ai-os.iso: build/kernel.bin iso/boot/grub/grub.cfg
	rm -rf build/isodir
	mkdir -p build/isodir/boot/grub
	cp build/kernel.bin build/isodir/boot/kernel.bin
	cp iso/boot/grub/grub.cfg build/isodir/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ build/isodir

check-toolchain:
	@command -v $(CC_BIN) >/dev/null || (echo "error: missing compiler $(CC_BIN)" >&2; exit 1)
	@command -v $(LD_BIN) >/dev/null || (echo "error: missing linker $(LD_BIN)" >&2; exit 1)
	@command -v $(AS_BIN) >/dev/null || (echo "error: missing assembler $(AS_BIN)" >&2; exit 1)
	@$(CC) -dumpmachine | grep -Eq '(^|-)i[3-6]86(-|$$)' || (echo "error: $(CC) is not an i386-targeting compiler" >&2; exit 1)
	@$(LD) -V 2>&1 | grep -q 'elf_i386' || (echo "error: $(LD) does not advertise elf_i386 support" >&2; exit 1)

check-artifacts: all program
	@test -s build/kernel.bin || (echo "error: kernel artifact is empty" >&2; exit 1)
	@test -s build/hello.com || (echo "error: TinyLang program artifact is empty" >&2; exit 1)
	@$(SIZE) build/kernel.bin
	@command -v $(GRUB_FILE) >/dev/null || (echo "error: missing grub-file" >&2; exit 1)
	@$(GRUB_FILE) --is-x86-multiboot build/kernel.bin || (echo "error: kernel.bin is not recognized as a Multiboot v1 kernel" >&2; exit 1)

check-iso: iso
	@command -v $(QEMU) >/dev/null || (echo "error: missing QEMU binary $(QEMU)" >&2; exit 1)
	@timeout $(QEMU_TIMEOUT)s $(QEMU) -cdrom build/ai-os.iso -display none -serial stdio -no-reboot -no-shutdown || test $$? -eq 124

check: check-toolchain check-keyboard check-terminal check-desktop-input check-ps2-wait check-examples check-listings check-compile check-source
	PYTHONPATH=toolchain $(PYTHON) toolchain/test_tinylang.py
	$(MAKE) check-artifacts

test: check

package-release:
	bash scripts/package-release.sh $(OUT_DIR)

run: build/kernel.bin
	$(QEMU) -kernel build/kernel.bin -no-reboot -no-shutdown

run-iso: build/ai-os.iso
	$(QEMU) -cdrom build/ai-os.iso -display none -serial stdio -no-reboot -no-shutdown

clean:
	rm -rf build

.PHONY: all check check-artifacts check-iso check-keyboard check-terminal check-desktop-input check-ps2-wait check-toolchain check-examples check-listings check-compile examples listings compile check-source clean iso package-release program run run-iso test
