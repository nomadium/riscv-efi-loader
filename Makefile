# RISC-V 64-bit EFI Bootloader Makefile

CROSS_COMPILE ?= riscv64-linux-gnu-
CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Compiler flags for freestanding EFI environment
CFLAGS  = -ffreestanding
CFLAGS += -fno-stack-protector
CFLAGS += -fno-stack-check
CFLAGS += -fshort-wchar
CFLAGS += -fPIC
CFLAGS += -fno-builtin
CFLAGS += -nostdinc
CFLAGS += -mno-relax              # Important: disable linker relaxation for EFI
CFLAGS += -march=rv64gc
CFLAGS += -mabi=lp64d
CFLAGS += -mcmodel=medany
CFLAGS += -Wall -Wextra
CFLAGS += -O2

# Linker flags
LDFLAGS  = -nostdlib
LDFLAGS += -T loader.ld
LDFLAGS += --no-relax             # Match compiler flag
LDFLAGS += -z noexecstack

# QEMU settings
QEMU = qemu-system-riscv64
OVMF_CODE = /usr/share/qemu-efi-riscv64/RISCV_VIRT_CODE.fd
OVMF_VARS = /usr/share/qemu-efi-riscv64/RISCV_VIRT_VARS.fd

# Targets
all: loader.efi

loader.efi: loader.elf
	$(OBJCOPY) -O binary $< $@
	@echo "Built $@ ($(shell stat -c%s $@ 2>/dev/null || echo '?') bytes)"

loader.elf: crt0.o loader.o loader.ld
	$(LD) $(LDFLAGS) crt0.o loader.o -o $@

crt0.o: crt0.S
	$(CC) $(CFLAGS) -c $< -o $@

loader.o: loader.c efi.h
	$(CC) $(CFLAGS) -c $< -o $@

# Create ESP image directory structure
image: loader.efi
	mkdir -p image/EFI/BOOT
	cp loader.efi image/EFI/BOOT/BOOTRISCV64.EFI
	@echo "Place your kernel.bin in image/ directory"

# Run in QEMU with UEFI firmware
# Note: Requires kernel.bin in image/ directory
qemu: image
	@if [ ! -f image/kernel.bin ]; then \
		echo "WARNING: image/kernel.bin not found!"; \
		echo "The bootloader will fail to find the kernel."; \
	fi
	$(QEMU) -M virt -m 256M -smp 2 -nographic \
		-drive if=pflash,format=raw,unit=0,file=$(OVMF_CODE),readonly=on \
		-drive if=pflash,format=raw,unit=1,file=ovmf_vars.fd \
		-drive file=fat:rw:image,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0

# Create a working copy of OVMF vars
ovmf_vars.fd:
	cp $(OVMF_VARS) ovmf_vars.fd

clean:
	rm -f *.o *.elf *.efi ovmf_vars.fd
	rm -rf image

# Show disassembly
disasm: loader.elf
	$(CROSS_COMPILE)objdump -d $<

# Show ELF sections
sections: loader.elf
	$(CROSS_COMPILE)readelf -S $<

.PHONY: all clean image qemu disasm sections
