# RISC-V 64-bit EFI Bootloader Makefile (using gnu-efi)

ARCH = riscv64
CROSS_COMPILE ?= riscv64-linux-gnu-
CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# gnu-efi paths (extracted from Ubuntu riscv64 package)
GNUEFI_DIR   = gnu-efi
INCLUDE_DIRS = -I$(GNUEFI_DIR)/include -I$(GNUEFI_DIR)/include/$(ARCH)
LDSCRIPT     = $(GNUEFI_DIR)/elf_$(ARCH)_efi.lds
CRT_EFI      = $(GNUEFI_DIR)/crt0-efi-$(ARCH).o
LIBEFI       = $(GNUEFI_DIR)/libefi.a
LIBGNUEFI    = $(GNUEFI_DIR)/libgnuefi.a

# Compiler flags
CFLAGS  = $(INCLUDE_DIRS)
CFLAGS += -ffreestanding
CFLAGS += -fno-stack-protector -fno-stack-check
CFLAGS += -fshort-wchar
CFLAGS += -fPIC
CFLAGS += -fno-builtin
CFLAGS += -mno-relax
CFLAGS += -march=rv64gc -mabi=lp64d -mcmodel=medany
CFLAGS += -Wall -Wextra -O2

# Linker flags
LDFLAGS  = -nostdlib
LDFLAGS += -shared -Bsymbolic
LDFLAGS += -T $(LDSCRIPT)
LDFLAGS += --no-relax

# QEMU settings
QEMU = qemu-system-riscv64
OVMF_CODE = /usr/share/qemu-efi-riscv64/RISCV_VIRT_CODE.fd
OVMF_VARS = /usr/share/qemu-efi-riscv64/RISCV_VIRT_VARS.fd

all: loader.efi

# Convert ELF shared object to PE/COFF binary
# The -O binary preserves the PE header from crt0's .text.head section
loader.efi: loader.so
	$(OBJCOPY) -O binary $< $@
	@echo "Built $@ ($$(stat -c%s $@) bytes)"

# Link EFI shared object
loader.so: loader.o
	$(LD) $(LDFLAGS) $(CRT_EFI) $< $(LIBGNUEFI) $(LIBEFI) -o $@

loader.o: loader.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create ESP image directory
image: loader.efi
	mkdir -p image/EFI/BOOT
	cp loader.efi image/EFI/BOOT/BOOTRISCV64.EFI
	@echo "Place your kernel.bin in image/ directory"

# Create OVMF vars copy
ovmf_vars.fd:
	cp $(OVMF_VARS) $@

# Run in QEMU
qemu: image ovmf_vars.fd
	@if [ ! -f image/kernel.bin ]; then \
		echo "WARNING: image/kernel.bin not found!"; \
	fi
	$(QEMU) -M virt -m 256M -smp 2 -nographic \
		-drive if=pflash,format=raw,unit=0,file=$(OVMF_CODE),readonly=on \
		-drive if=pflash,format=raw,unit=1,file=ovmf_vars.fd \
		-drive file=fat:rw:image,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0

clean:
	rm -f *.o *.so *.efi ovmf_vars.fd
	rm -rf image

.PHONY: all clean image qemu
