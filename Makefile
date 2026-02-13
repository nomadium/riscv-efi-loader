# x86_64 EFI Bootloader Makefile (using gnu-efi)

ARCH = x86_64

CC      = gcc
LD      = ld
OBJCOPY = objcopy

# gnu-efi paths (from Ubuntu package)
GNUEFI_INC   = /usr/include/efi
GNUEFI_LIB   = /usr/lib
LDSCRIPT     = $(GNUEFI_LIB)/elf_$(ARCH)_efi.lds
CRT_EFI      = $(GNUEFI_LIB)/crt0-efi-$(ARCH).o
LIBEFI       = $(GNUEFI_LIB)/libefi.a
LIBGNUEFI    = $(GNUEFI_LIB)/libgnuefi.a

# Compiler flags
CFLAGS  = -I$(GNUEFI_INC) -I$(GNUEFI_INC)/$(ARCH)
CFLAGS += -DHAVE_USE_MS_ABI
CFLAGS += -ffreestanding
CFLAGS += -fno-stack-protector -fno-stack-check
CFLAGS += -fshort-wchar
CFLAGS += -fPIC -fno-plt
CFLAGS += -fno-builtin
CFLAGS += -mno-red-zone -maccumulate-outgoing-args
CFLAGS += -Wall -O2

# Linker flags
LDFLAGS  = -nostdlib
LDFLAGS += -shared -Bsymbolic
LDFLAGS += -T $(LDSCRIPT)

# Target
TARGET = loader.efi

all: $(TARGET)

loader.o: loader.c
	$(CC) $(CFLAGS) -c -o $@ $<

loader.so: loader.o
	$(LD) $(LDFLAGS) $(CRT_EFI) $< $(LIBGNUEFI) $(LIBEFI) -o $@

$(TARGET): loader.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .rodata \
		-j .dynamic -j .dynsym -j .rel -j .rela -j .reloc \
		--target efi-app-$(ARCH) --subsystem=10 $< $@
	@echo "Built $(TARGET) ($$(stat -c%s $(TARGET)) bytes)"

# Create ESP image directory
image: $(TARGET)
	mkdir -p image/EFI/BOOT
	cp $(TARGET) image/EFI/BOOT/BOOTX64.EFI
	@echo "ESP image created in image/"

# QEMU with OVMF
OVMF_CODE = /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS = /usr/share/OVMF/OVMF_VARS_4M.fd

qemu: image
	cp $(OVMF_VARS) ovmf_vars.fd
	qemu-system-x86_64 \
		-machine q35 \
		-m 256M \
		-smp 4 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=ovmf_vars.fd \
		-drive format=raw,file=fat:rw:image \
		-nographic \
		-no-reboot

clean:
	rm -f *.o *.so *.efi ovmf_vars.fd
	rm -rf image

.PHONY: all clean image qemu
