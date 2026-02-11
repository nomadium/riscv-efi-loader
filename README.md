# RISC-V EFI Bootloader

A minimal UEFI bootloader for RISC-V 64-bit that loads a raw binary kernel from the EFI System Partition.

## Features

- Loads raw binary kernel from `\kernel.bin` on the ESP
- Passes hart ID in `a0` and device tree pointer in `a1` (Linux boot protocol compatible)
- Works with QEMU virt machine and EDK2 UEFI firmware
- No external dependencies (minimal hand-crafted PE/COFF header)

## Building

```bash
make
```

Requires `riscv64-linux-gnu-gcc` cross-compiler.

## Usage

1. Build your kernel as a raw binary (e.g., using `objcopy -O binary`)
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

1. Be a flat binary (not ELF)
2. Be linked at `0x80000000` (or modify `KERNEL_LOAD_ADDR` in loader.c)
3. Entry point at the start of the binary
4. Accept:
   - `a0` = hart ID (current CPU)
   - `a1` = pointer to device tree blob (FDT), may be NULL

## Configuration

Edit `loader.c` to change:

- `KERNEL_PATH` - path to kernel on ESP (default: `\kernel.bin`)
- `KERNEL_LOAD_ADDR` - memory address to load kernel (default: `0x80000000`)

## Files

- `loader.c` - Main bootloader code
- `crt0.S` - Entry point and PE/COFF header
- `efi.h` - Minimal EFI type definitions
- `loader.ld` - Linker script

## Testing with riscv-real-world-hello-uart

```bash
# Build the kernel
cd /path/to/riscv-real-world-hello-uart
make hello.img

# Copy to ESP
cd /path/to/riscv-efi-loader
make image
cp /path/to/riscv-real-world-hello-uart/hello.img image/kernel.bin

# Run
make qemu
```

## License

Public domain / CC0
