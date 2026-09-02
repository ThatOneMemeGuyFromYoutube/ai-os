TARGET ?= i386-elf
CC ?= $(TARGET)-gcc
LD ?= $(TARGET)-ld
AS ?= $(TARGET)-as
OBJCOPY ?= $(TARGET)-objcopy
SIZE ?= $(TARGET)-size
PYTHON ?= python3
GRUB_MKRESCUE ?= grub-mkrescue
CFLAGS ?= -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Wall -Wextra
LDFLAGS ?= -m elf_i386 -T linker.ld

all: build/kernel.bin

build:
	mkdir -p build

build/boot.o: boot/boot.S | build
	$(AS) --32 $< -o $@

build/kernel.o: kernel/kernel.c kernel/gui.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/gui.o: kernel/gui.c kernel/gui.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel.bin: build/boot.o build/kernel.o build/gui.o linker.ld
	$(LD) $(LDFLAGS) -o $@ build/boot.o build/kernel.o build/gui.o

build/hello.com: toolchain/tinylang.py toolchain/examples/hello.tl | build
	$(PYTHON) toolchain/tinylang.py toolchain/examples/hello.tl $@

program: build/hello.com

iso: build/ai-os.iso

build/ai-os.iso: build/kernel.bin iso/boot/grub/grub.cfg
	rm -rf build/isodir
	mkdir -p build/isodir/boot/grub
	cp build/kernel.bin build/isodir/boot/kernel.bin
	cp iso/boot/grub/grub.cfg build/isodir/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ build/isodir

check-toolchain:
	@command -v $(CC) >/dev/null || (echo "error: missing compiler $(CC)" >&2; exit 1)
	@command -v $(LD) >/dev/null || (echo "error: missing linker $(LD)" >&2; exit 1)
	@command -v $(AS) >/dev/null || (echo "error: missing assembler $(AS)" >&2; exit 1)
	@$(CC) -dumpmachine | grep -Eq '(^|-)i[3-6]86(-|$$)' || (echo "error: $(CC) is not an i386-targeting compiler" >&2; exit 1)
	@$(LD) -V 2>&1 | grep -q 'elf_i386' || (echo "error: $(LD) does not advertise elf_i386 support" >&2; exit 1)

check: check-toolchain
	PYTHONPATH=toolchain $(PYTHON) toolchain/test_tinylang.py
	$(MAKE) program

size: build/kernel.bin
	$(SIZE) $<

test: check

run: build/kernel.bin
	qemu-system-i386 -kernel build/kernel.bin

run-iso: build/ai-os.iso
	qemu-system-i386 -cdrom build/ai-os.iso -display none -serial stdio -no-reboot -no-shutdown

clean:
	rm -rf build

.PHONY: all check check-toolchain clean iso program run run-iso size test
