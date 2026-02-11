# RISC-V EFI Bootloader

A minimal UEFI bootloader for RISC-V 64-bit that loads a raw binary kernel from the EFI System Partition.

## Features

- Loads raw binary kernel from `\kernel.bin` on the ESP
- Passes hart ID in `a0` and device tree pointer in `a1` (Linux boot protocol compatible)
- Works with QEMU virt machine and EDK2 UEFI firmware
- Uses gnu-efi library for RISC-V

## Prerequisites

Download the gnu-efi package for riscv64:

```bash
wget http://ports.ubuntu.com/ubuntu-ports/pool/main/g/gnu-efi/gnu-efi_3.0.18-1_riscv64.deb
mkdir -p gnu-efi
dpkg-deb -x gnu-efi_3.0.18-1_riscv64.deb gnu-efi-extracted
cp -r gnu-efi-extracted/usr/include/efi gnu-efi/include
cp gnu-efi-extracted/usr/lib/*.a gnu-efi-extracted/usr/lib/*.o gnu-efi-extracted/usr/lib/*.lds gnu-efi/
```

## Building

```bash
make
```

Requires `riscv64-linux-gnu-gcc` cross-compiler.

## Usage

1. Build your kernel as a raw binary linked at `0x80200000`
2. Create the ESP directory structure:
   ```bash
   make image
   cp your-kernel.bin image/kernel.bin
   ```
3. Run with QEMU:
   ```bash
   make qemu
   ```

## Boot Flow

```
QEMU/Hardware → OpenSBI → UEFI (EDK2) → loader.efi → kernel.bin
```

## Kernel Requirements

Your kernel must:

1. Be a flat binary (not ELF) - use `objcopy -O binary`
2. Be linked at `0x80200000` (standard Linux RISC-V load address)
3. Entry point at the start of the binary
4. Accept:
   - `a0` = hart ID (current CPU)
   - `a1` = pointer to device tree blob (FDT), may be NULL

## Configuration

Edit `loader.c` to change:

- `KERNEL_PATH` - path to kernel on ESP (default: `\kernel.bin`)
- `KERNEL_LOAD_ADDR` - memory address to load kernel (default: `0x80200000`)

## Testing with riscv-real-world-hello-uart

```bash
# Build the kernel (ensure link.ld uses 0x80200000)
cd /path/to/riscv-real-world-hello-uart
make hello.img

# Copy to ESP
cd /path/to/riscv-efi-loader
make image
cp /path/to/riscv-real-world-hello-uart/hello.img image/kernel.bin
echo 'FS0:\loader.efi' > image/startup.nsh

# Run
make qemu
```

Expected output:
```
========================================
  RISC-V EFI Bootloader
========================================

Getting loaded image protocol... OK
Getting file system protocol... OK
Opening root directory... OK
Opening kernel file \kernel.bin... OK
Getting kernel file info... OK (342 bytes)
Allocating memory at 0x80200000... OK at 0x80200000
Loading kernel into memory... OK
Looking for device tree... OK at 0x8FA54798
Getting boot hart ID... OK (hart 0)

Preparing to exit boot services...
Exiting boot services...
hello world!
```

## Key Implementation Details

- Uses `EfiLoaderCode` memory type for kernel allocation (ensures executable pages)
- The PE/COFF header comes from gnu-efi's `crt0-efi-riscv64.o` `.text.head` section
- Uses `-O binary` objcopy (not `--target efi-app-*`) to preserve the embedded PE header
- Gets DTB from EFI configuration tables (`EFI_DTB_TABLE_GUID`)
- Gets boot hart ID via RISC-V EFI boot protocol

## Files

- `loader.c` - Main bootloader code
- `Makefile` - Build system
- `gnu-efi/` - gnu-efi library and headers for RISC-V

## License

Public domain / CC0
