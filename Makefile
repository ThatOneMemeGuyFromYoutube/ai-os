TARGET ?= i386-elf
CC ?= $(TARGET)-gcc
LD ?= $(TARGET)-ld
AS ?= $(TARGET)-as
PYTHON ?= python3
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

run: build/kernel.bin
	qemu-system-i386 -kernel build/kernel.bin

clean:
	rm -rf build

.PHONY: all program run clean
