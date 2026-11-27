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
NE2K_PEER_TIMEOUT ?= 20
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
INSPECT_DIR ?= build/inspect

all: build/kernel.bin

build:
	mkdir -p build

build/boot.o: boot/boot.S | build
	$(AS) --32 $< -o $@

build/kernel.o: kernel/kernel.c kernel/fs.h kernel/gui.h kernel/keyboard.h kernel/desktop_input.h kernel/ps2_wait.h kernel/net.h kernel/arp.h kernel/ipv4.h kernel/icmp.h kernel/pci.h kernel/ne2k.h kernel/ata_pio.h kernel/volume.h kernel/blockdev.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/gui.o: kernel/gui.c kernel/gui.h kernel/fs.h kernel/ide_file.h kernel/terminal.h kernel/desktop_input.h kernel/app_catalog.h kernel/editor.h kernel/tinylang_check.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/editor.o: kernel/editor.c kernel/editor.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/tinylang_check.o: kernel/tinylang_check.c kernel/tinylang_check.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/fs.o: kernel/fs.c kernel/fs.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ide_file.o: kernel/ide_file.c kernel/ide_file.h kernel/editor.h kernel/fs.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/keyboard.o: kernel/keyboard.c kernel/keyboard.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/desktop_input.o: kernel/desktop_input.c kernel/desktop_input.h kernel/app_catalog.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/app_catalog.o: kernel/app_catalog.c kernel/app_catalog.h kernel/desktop_input.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/terminal.o: kernel/terminal.c kernel/terminal.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ps2_wait.o: kernel/ps2_wait.c kernel/ps2_wait.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/net.o: kernel/net.c kernel/net.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/arp.o: kernel/arp.c kernel/arp.h kernel/net.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ipv4.o: kernel/ipv4.c kernel/ipv4.h kernel/net.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/icmp.o: kernel/icmp.c kernel/icmp.h kernel/ipv4.h kernel/net.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/pci.o: kernel/pci.c kernel/pci.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ne2k.o: kernel/ne2k.c kernel/ne2k.h kernel/net.h kernel/pci.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ata_pio.o: kernel/ata_pio.c kernel/ata_pio.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/volume.o: kernel/volume.c kernel/volume.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/blockdev.o: kernel/blockdev.c kernel/blockdev.h kernel/volume.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/test-volume: kernel/volume.c kernel/volume.h tests/test_volume.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/volume.c tests/test_volume.c -o $@

check-volume: build/test-volume
	./build/test-volume

build/test-blockdev: kernel/blockdev.c kernel/blockdev.h kernel/volume.h tests/test_blockdev.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/blockdev.c tests/test_blockdev.c -o $@

check-blockdev: build/test-blockdev
	./build/test-blockdev

build/test-editor: kernel/editor.c kernel/editor.h tests/test_editor.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/editor.c tests/test_editor.c -o $@

check-editor: build/test-editor
	./build/test-editor

build/test-tinylang-check: kernel/tinylang_check.c kernel/tinylang_check.h tests/test_tinylang_check.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/tinylang_check.c tests/test_tinylang_check.c -o $@

check-tinylang-check: build/test-tinylang-check
	./build/test-tinylang-check

build/test-gui: kernel/gui.c kernel/gui.h kernel/editor.c kernel/editor.h kernel/tinylang_check.c kernel/tinylang_check.h kernel/fs.c kernel/fs.h kernel/ide_file.c kernel/ide_file.h kernel/keyboard.c kernel/keyboard.h kernel/desktop_input.c kernel/desktop_input.h kernel/app_catalog.c kernel/app_catalog.h kernel/terminal.c kernel/terminal.h tests/test_gui.c | build
	$(HOSTCC) $(HOSTCFLAGS) -DASTEROS_GUI_TEST -Ikernel kernel/gui.c kernel/editor.c kernel/tinylang_check.c kernel/fs.c kernel/ide_file.c kernel/keyboard.c kernel/desktop_input.c kernel/app_catalog.c kernel/terminal.c tests/test_gui.c -o $@

check-gui: build/test-gui
	./build/test-gui

build/test-fs: kernel/fs.c kernel/fs.h tests/test_fs.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/fs.c tests/test_fs.c -o $@

check-fs: build/test-fs
	./build/test-fs

build/test-ide-file: kernel/ide_file.c kernel/ide_file.h kernel/editor.c kernel/editor.h kernel/fs.c kernel/fs.h tests/test_ide_file.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/ide_file.c kernel/editor.c kernel/fs.c tests/test_ide_file.c -o $@

check-ide-file: build/test-ide-file
	./build/test-ide-file

build/test-terminal: kernel/terminal.c kernel/terminal.h tests/test_terminal.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/terminal.c tests/test_terminal.c -o $@

check-terminal: build/test-terminal
	./build/test-terminal

build/test-ps2-wait: kernel/ps2_wait.c kernel/ps2_wait.h tests/test_ps2_wait.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/ps2_wait.c tests/test_ps2_wait.c -o $@

check-ps2-wait: build/test-ps2-wait
	./build/test-ps2-wait

build/test-net: kernel/net.c kernel/net.h tests/test_net.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/net.c tests/test_net.c -o $@

check-net: build/test-net
	./build/test-net

build/test-arp: kernel/arp.c kernel/arp.h kernel/net.c kernel/net.h tests/test_arp.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/arp.c kernel/net.c tests/test_arp.c -o $@

check-arp: build/test-arp
	./build/test-arp

build/test-ipv4: kernel/ipv4.c kernel/ipv4.h kernel/net.c kernel/net.h tests/test_ipv4.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/ipv4.c kernel/net.c tests/test_ipv4.c -o $@

check-ipv4: build/test-ipv4
	./build/test-ipv4

build/test-icmp: kernel/icmp.c kernel/icmp.h kernel/ipv4.c kernel/ipv4.h kernel/net.c kernel/net.h tests/test_icmp.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/icmp.c kernel/ipv4.c kernel/net.c tests/test_icmp.c -o $@

check-icmp: build/test-icmp
	./build/test-icmp

build/test-pci: kernel/pci.c kernel/pci.h tests/test_pci.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/pci.c tests/test_pci.c -o $@

check-pci: build/test-pci
	./build/test-pci

build/test-ne2k: kernel/ne2k.c kernel/ne2k.h kernel/net.c kernel/net.h kernel/pci.c kernel/pci.h tests/test_ne2k.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/ne2k.c kernel/net.c kernel/pci.c tests/test_ne2k.c -o $@

check-ne2k: build/test-ne2k
	./build/test-ne2k

build/test-ata-pio: kernel/ata_pio.c kernel/ata_pio.h tests/test_ata_pio.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/ata_pio.c tests/test_ata_pio.c -o $@

check-ata-pio: build/test-ata-pio
	./build/test-ata-pio

build/test-desktop-input: kernel/desktop_input.c kernel/desktop_input.h kernel/app_catalog.c kernel/app_catalog.h tests/test_desktop_input.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/desktop_input.c kernel/app_catalog.c tests/test_desktop_input.c -o $@

check-desktop-input: build/test-desktop-input
	./build/test-desktop-input

build/test-app-catalog: kernel/app_catalog.c kernel/app_catalog.h kernel/desktop_input.h tests/test_app_catalog.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/app_catalog.c tests/test_app_catalog.c -o $@

check-app-catalog: build/test-app-catalog
	./build/test-app-catalog

build/test-keyboard: kernel/keyboard.c kernel/keyboard.h tests/test_keyboard.c | build
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel kernel/keyboard.c tests/test_keyboard.c -o $@

check-keyboard: build/test-keyboard
	./build/test-keyboard

build/kernel.bin: build/boot.o build/kernel.o build/gui.o build/editor.o build/tinylang_check.o build/fs.o build/ide_file.o build/keyboard.o build/desktop_input.o build/app_catalog.o build/terminal.o build/ps2_wait.o build/net.o build/arp.o build/ipv4.o build/icmp.o build/pci.o build/ne2k.o build/ata_pio.o build/volume.o build/blockdev.o linker.ld
	$(LD) $(LDFLAGS) -o $@ build/boot.o build/kernel.o build/gui.o build/editor.o build/tinylang_check.o build/fs.o build/ide_file.o build/keyboard.o build/desktop_input.o build/app_catalog.o build/terminal.o build/ps2_wait.o build/net.o build/arp.o build/ipv4.o build/icmp.o build/pci.o build/ne2k.o build/ata_pio.o build/volume.o build/blockdev.o

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


# Build one source and collect its executable, label map, and byte listing.
inspect: | build
	@test -n "$(SOURCE)" || { echo "error: set SOURCE to a TinyLang file" >&2; exit 1; }
	@test -f "$(SOURCE)" || { echo "error: TinyLang source not found: $(SOURCE)" >&2; exit 1; }
	@test -n "$(INSPECT_DIR)" || { echo "error: set INSPECT_DIR to a destination directory" >&2; exit 1; }
	$(MAKE) --no-print-directory compile SOURCE="$(SOURCE)" \
		OUTPUT="$(INSPECT_DIR)/$(basename $(notdir $(SOURCE))).com" \
		MAP="$(INSPECT_DIR)/$(basename $(notdir $(SOURCE))).map" \
		LISTING="$(INSPECT_DIR)/$(basename $(notdir $(SOURCE))).lst"

# Verify the one-command inspection bundle and all three generated artifacts.
check-inspect: build/examples/jump_over_data.com
	rm -rf build/inspect-check
	$(MAKE) --no-print-directory inspect SOURCE=toolchain/examples/jump_over_data.tl INSPECT_DIR=build/inspect-check
	cmp build/examples/jump_over_data.com build/inspect-check/jump_over_data.com
	grep -Fx '0x00000105 0x00000005 message' build/inspect-check/jump_over_data.map
	grep -Fx '0002 0x00000100  E9 07 00 00 00 | jmp program' build/inspect-check/jump_over_data.lst
	test -s build/inspect-check/jump_over_data.com
	test -s build/inspect-check/jump_over_data.map
	test -s build/inspect-check/jump_over_data.lst


# Validate one edited source without creating or replacing a program image.
check-source:
	@test -n "$(SOURCE)" || { echo "error: set SOURCE to a TinyLang file" >&2; exit 1; }
	@test -f "$(SOURCE)" || { echo "error: TinyLang source not found: $(SOURCE)" >&2; exit 1; }
	$(PYTHON) toolchain/tinylang.py --check "$(SOURCE)"

# Optional AsterOS-branded SeaBIOS derivative; not required for the default build.
asterbios: build/firmware/asterbios.bin

build/firmware/asterbios.bin: firmware/asterbios/build.sh | build
	bash firmware/asterbios/build.sh

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
	@QEMU="$(QEMU)" QEMU_TIMEOUT="$(QEMU_TIMEOUT)" bash scripts/qemu-smoke.sh -cdrom build/ai-os.iso -display none -serial stdio -no-reboot -no-shutdown

# Exercise Ctrl+Esc, Start-menu navigation, Run cancellation/launch, and Tab
# switching against the actual VGA text buffer of a running QEMU guest.
check-start-menu-iso: iso scripts/qemu-start-menu-smoke.py
	$(PYTHON) scripts/qemu-start-menu-smoke.py --qemu "$(QEMU)" --iso build/ai-os.iso --timeout "$(QEMU_TIMEOUT)"

# Boot with the same PCI RTL8029-compatible NE2000 device used by the v86 selector.
check-ne2k-iso: iso
	@command -v $(QEMU) >/dev/null || (echo "error: missing QEMU binary $(QEMU)" >&2; exit 1)
	@QEMU="$(QEMU)" QEMU_TIMEOUT="$(QEMU_TIMEOUT)" bash scripts/qemu-smoke.sh -cdrom build/ai-os.iso -display none -serial stdio -no-reboot -no-shutdown -netdev user,id=net0 -device ne2k_pci,netdev=net0

# Exchange ARP and IPv4 ICMP echo frames with a deterministic QEMU socket peer.
# This exercises the guest receive path and hardware-backed transmit path.
check-ne2k-peer-iso: iso
	@command -v $(QEMU) >/dev/null || (echo "error: missing QEMU binary $(QEMU)" >&2; exit 1)
	$(PYTHON) scripts/ne2k-peer-smoke.py --qemu "$(QEMU)" --iso build/ai-os.iso --timeout "$(NE2K_PEER_TIMEOUT)"

# Launch the ISO interactively with the optional AsterBIOS system ROM.
run-asterbios-iso: iso build/firmware/asterbios.bin
	$(QEMU) -bios build/firmware/asterbios.bin -cdrom build/ai-os.iso -no-reboot -no-shutdown

# Boot the same ISO with the optional AsterBIOS system ROM; SeaBIOS remains default.
check-asterbios-iso: iso build/firmware/asterbios.bin
	@command -v $(QEMU) >/dev/null || (echo "error: missing QEMU binary $(QEMU)" >&2; exit 1)
	@QEMU="$(QEMU)" QEMU_TIMEOUT="$(QEMU_TIMEOUT)" bash scripts/qemu-smoke.sh -bios build/firmware/asterbios.bin -cdrom build/ai-os.iso -display none -serial stdio -no-reboot -no-shutdown

# Boot with a disposable primary-master IDE disk so the ATA probe path is exercised.
check-ata-iso: iso
	@command -v $(QEMU) >/dev/null || (echo "error: missing QEMU binary $(QEMU)" >&2; exit 1)
	@rm -f build/asteros-ata-test.img
	@truncate -s 16M build/asteros-ata-test.img
	@QEMU="$(QEMU)" QEMU_TIMEOUT="$(QEMU_TIMEOUT)" bash scripts/qemu-smoke.sh -drive file=build/asteros-ata-test.img,format=raw,if=ide,index=0 -cdrom build/ai-os.iso -boot order=d -display none -serial stdio -no-reboot -no-shutdown

# Boot with an explicitly tagged, empty AsterOS volume descriptor. The guest
# discovers it read-only; this target does not test filesystem writes.
check-volume-iso: iso scripts/create-volume-image.py
	@command -v $(QEMU) >/dev/null || (echo "error: missing QEMU binary $(QEMU)" >&2; exit 1)
	@rm -f build/asteros-volume-test.img
	$(PYTHON) scripts/create-volume-image.py --output build/asteros-volume-test.img
	@QEMU="$(QEMU)" QEMU_TIMEOUT="$(QEMU_TIMEOUT)" bash scripts/qemu-smoke.sh -drive file=build/asteros-volume-test.img,format=raw,if=ide,index=0 -cdrom build/ai-os.iso -boot order=d -display none -serial stdio -no-reboot -no-shutdown

check: check-toolchain check-keyboard check-editor check-tinylang-check check-app-catalog check-gui check-fs check-ide-file check-terminal check-desktop-input check-ps2-wait check-net check-arp check-ipv4 check-icmp check-pci check-ne2k check-ata-pio check-volume check-blockdev check-examples check-listings check-compile check-inspect check-source
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

.PHONY: all check check-artifacts asterbios run-asterbios-iso check-iso check-start-menu-iso check-ne2k-iso check-ne2k-peer-iso check-asterbios-iso check-ata-iso check-volume-iso check-ata-pio check-volume check-blockdev check-app-catalog check-keyboard check-editor check-tinylang-check check-gui check-fs check-ide-file check-terminal check-desktop-input check-ps2-wait check-net check-arp check-ipv4 check-icmp check-pci check-ne2k check-toolchain check-examples check-listings check-compile check-inspect examples listings compile inspect check-source clean iso package-release program run run-iso test
