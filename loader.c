/*
 * x86_64 EFI Bootloader
 *
 * Loads a raw binary kernel from the EFI System Partition,
 * exits boot services, and jumps to the kernel entry point.
 *
 * The kernel is expected at \kernel.bin on the ESP.
 *
 * Kernel entry convention:
 *   RDI = pointer to boot info structure
 *
 * Note: efi_main uses System V ABI because gnu-efi's crt0 converts
 * from MS ABI (UEFI) to System V ABI before calling efi_main.
 */

#include <efi.h>

/* Configuration */
#define KERNEL_PATH        L"\\kernel.bin"
#define KERNEL_LOAD_ADDR   0x100000ULL  /* 1MB - traditional x86 kernel load address */
#define MAX_MEMORY_MAP     16384

/* Boot info passed to kernel */
struct boot_info {
    UINT64 magic;             /* 0x424F4F54494E464F "BOOTINFO" */
    UINT64 mem_map_addr;      /* Physical address of memory map */
    UINT64 mem_map_size;      /* Size of memory map in bytes */
    UINT64 mem_map_desc_size; /* Size of each descriptor */
    UINT64 framebuffer_addr;  /* Framebuffer address (0 if none) */
    UINT64 framebuffer_width;
    UINT64 framebuffer_height;
    UINT64 framebuffer_pitch;
    UINT64 acpi_rsdp;         /* ACPI RSDP address */
    UINT64 num_cpus;          /* Number of CPUs (from ACPI MADT) */
};

#define BOOT_INFO_MAGIC 0x424F4F54494E464FULL

/* Serial port output for debugging */
#define COM1 0x3F8

static inline void outb(UINT16 port, UINT8 val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline UINT8 inb(UINT16 port) {
    UINT8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_init(void) {
    outb(COM1 + 1, 0x00);  /* Disable interrupts */
    outb(COM1 + 3, 0x80);  /* Enable DLAB */
    outb(COM1 + 0, 0x01);  /* Divisor low (115200 baud) */
    outb(COM1 + 1, 0x00);  /* Divisor high */
    outb(COM1 + 3, 0x03);  /* 8N1 */
    outb(COM1 + 2, 0xC7);  /* FIFO */
    outb(COM1 + 4, 0x0B);  /* RTS/DSR set */
}

static void serial_putc(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0);
    outb(COM1, c);
}

static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

static void serial_puthex(UINT64 n) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(n >> i) & 0xf]);
    }
}

static void serial_putint(UINT64 n) {
    char buf[21];
    int i = 0;
    if (n == 0) {
        serial_putc('0');
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

/* ACPI GUIDs */
static EFI_GUID Acpi20TableGuid = { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81} };
static EFI_GUID Acpi10TableGuid = { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d} };

/* Protocol GUIDs we need */
static EFI_GUID LoadedImageProtocol = { 0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} };
static EFI_GUID SimpleFileSystemProtocol = { 0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b} };
static EFI_GUID FileInfoGuid = { 0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b} };

/*
 * Find ACPI RSDP in EFI configuration tables
 */
static VOID *FindAcpiRsdp(EFI_SYSTEM_TABLE *ST)
{
    UINTN i;
    
    /* Try ACPI 2.0 first */
    for (i = 0; i < ST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *entry = &ST->ConfigurationTable[i];
        EFI_GUID *guid = &entry->VendorGuid;
        
        if (guid->Data1 == Acpi20TableGuid.Data1 &&
            guid->Data2 == Acpi20TableGuid.Data2 &&
            guid->Data3 == Acpi20TableGuid.Data3) {
            return entry->VendorTable;
        }
    }
    
    /* Fall back to ACPI 1.0 */
    for (i = 0; i < ST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *entry = &ST->ConfigurationTable[i];
        EFI_GUID *guid = &entry->VendorGuid;
        
        if (guid->Data1 == Acpi10TableGuid.Data1 &&
            guid->Data2 == Acpi10TableGuid.Data2 &&
            guid->Data3 == Acpi10TableGuid.Data3) {
            return entry->VendorTable;
        }
    }
    
    return 0;
}

/*
 * Kernel entry point type (System V ABI)
 */
typedef VOID (*kernel_entry_t)(struct boot_info *info);

/*
 * EFI application entry point
 * Note: Uses System V ABI because crt0 converts from MS ABI
 */
EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Volume;
    EFI_FILE_PROTOCOL *RootDir, *KernelFile;
    EFI_FILE_INFO *FileInfo;
    UINTN FileInfoSize;
    UINT8 FileInfoBuffer[512];
    UINTN KernelSize;
    EFI_PHYSICAL_ADDRESS KernelAddr;
    UINTN Pages;
    VOID *AcpiRsdp;
    
    /* Memory map variables */
    static UINT8 MemoryMapBuffer[MAX_MEMORY_MAP];
    UINTN MemoryMapSize, MapKey, DescriptorSize;
    UINT32 DescriptorVersion;
    
    /* Boot info - static so it survives ExitBootServices */
    static struct boot_info BootInfo;
    
    kernel_entry_t KernelEntry;

    /* Initialize serial for debug output */
    serial_init();

    /* Print banner */
    serial_puts("\n");
    serial_puts("========================================\n");
    serial_puts("  x86_64 EFI Bootloader\n");
    serial_puts("========================================\n\n");

    /* Get loaded image protocol */
    serial_puts("Getting loaded image protocol... ");
    status = BS->HandleProtocol(ImageHandle, &LoadedImageProtocol,
                                (VOID **)&LoadedImage);
    if (EFI_ERROR(status)) {
        serial_puts("FAILED\n");
        goto halt;
    }
    serial_puts("OK\n");

    /* Get file system protocol */
    serial_puts("Getting file system protocol... ");
    status = BS->HandleProtocol(LoadedImage->DeviceHandle,
                                &SimpleFileSystemProtocol,
                                (VOID **)&Volume);
    if (EFI_ERROR(status)) {
        serial_puts("FAILED\n");
        goto halt;
    }
    serial_puts("OK\n");

    /* Open root directory */
    serial_puts("Opening root directory... ");
    status = Volume->OpenVolume(Volume, &RootDir);
    if (EFI_ERROR(status)) {
        serial_puts("FAILED\n");
        goto halt;
    }
    serial_puts("OK\n");

    /* Open kernel file */
    serial_puts("Opening kernel file... ");
    status = RootDir->Open(RootDir, &KernelFile, KERNEL_PATH,
                           EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        serial_puts("FAILED - kernel.bin not found\n");
        goto halt;
    }
    serial_puts("OK\n");

    /* Get kernel file info */
    serial_puts("Getting kernel file info... ");
    FileInfoSize = sizeof(FileInfoBuffer);
    status = KernelFile->GetInfo(KernelFile, &FileInfoGuid,
                                  &FileInfoSize, FileInfoBuffer);
    if (EFI_ERROR(status)) {
        serial_puts("FAILED\n");
        goto halt;
    }
    FileInfo = (EFI_FILE_INFO *)FileInfoBuffer;
    KernelSize = FileInfo->FileSize;
    serial_puts("OK (");
    serial_putint(KernelSize);
    serial_puts(" bytes)\n");

    /* Allocate memory for kernel */
    serial_puts("Allocating memory at ");
    serial_puthex(KERNEL_LOAD_ADDR);
    serial_puts("... ");
    KernelAddr = KERNEL_LOAD_ADDR;
    Pages = (KernelSize + 4095) / 4096;
    status = BS->AllocatePages(AllocateAddress, EfiLoaderCode, Pages, &KernelAddr);
    if (EFI_ERROR(status)) {
        serial_puts("(trying any address) ");
        status = BS->AllocatePages(AllocateAnyPages, EfiLoaderCode, Pages, &KernelAddr);
        if (EFI_ERROR(status)) {
            serial_puts("FAILED\n");
            goto halt;
        }
    }
    serial_puts("OK at ");
    serial_puthex(KernelAddr);
    serial_puts("\n");

    /* Load kernel into memory */
    serial_puts("Loading kernel into memory... ");
    status = KernelFile->Read(KernelFile, &KernelSize, (VOID *)KernelAddr);
    if (EFI_ERROR(status)) {
        serial_puts("FAILED\n");
        goto halt;
    }
    serial_puts("OK\n");

    /* Close file handles */
    KernelFile->Close(KernelFile);
    RootDir->Close(RootDir);

    /* Find ACPI RSDP */
    serial_puts("Looking for ACPI RSDP... ");
    AcpiRsdp = FindAcpiRsdp(SystemTable);
    if (AcpiRsdp) {
        serial_puts("OK at ");
        serial_puthex((UINT64)AcpiRsdp);
        serial_puts("\n");
    } else {
        serial_puts("NOT FOUND\n");
    }

    /* Get memory map for ExitBootServices */
    serial_puts("\nPreparing to exit boot services...\n");
    MemoryMapSize = sizeof(MemoryMapBuffer);
    status = BS->GetMemoryMap(&MemoryMapSize, (EFI_MEMORY_DESCRIPTOR *)MemoryMapBuffer,
                              &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(status)) {
        serial_puts("Failed to get memory map\n");
        goto halt;
    }

    /* Prepare boot info structure */
    BootInfo.magic = BOOT_INFO_MAGIC;
    BootInfo.mem_map_addr = (UINT64)MemoryMapBuffer;
    BootInfo.mem_map_size = MemoryMapSize;
    BootInfo.mem_map_desc_size = DescriptorSize;
    BootInfo.framebuffer_addr = 0;  /* No framebuffer in text mode */
    BootInfo.framebuffer_width = 0;
    BootInfo.framebuffer_height = 0;
    BootInfo.framebuffer_pitch = 0;
    BootInfo.acpi_rsdp = (UINT64)AcpiRsdp;
    BootInfo.num_cpus = 0;  /* Kernel will enumerate via ACPI */

    /* Exit boot services */
    serial_puts("Exiting boot services...\n");
    status = BS->ExitBootServices(ImageHandle, MapKey);
    if (EFI_ERROR(status)) {
        /* Memory map may have changed, try once more */
        MemoryMapSize = sizeof(MemoryMapBuffer);
        BS->GetMemoryMap(&MemoryMapSize, (EFI_MEMORY_DESCRIPTOR *)MemoryMapBuffer,
                         &MapKey, &DescriptorSize, &DescriptorVersion);
        BootInfo.mem_map_size = MemoryMapSize;
        status = BS->ExitBootServices(ImageHandle, MapKey);
    }

    if (EFI_ERROR(status)) {
        while (1) {
            __asm__ volatile("hlt");
        }
    }

    serial_puts("Jumping to kernel at ");
    serial_puthex(KernelAddr);
    serial_puts("...\n\n");

    /*
     * Boot services are now terminated!
     * Jump to kernel with:
     *   RDI = pointer to boot_info
     */
    KernelEntry = (kernel_entry_t)KernelAddr;
    KernelEntry(&BootInfo);

    /* Should never reach here */
    while (1) {
        __asm__ volatile("hlt");
    }

halt:
    serial_puts("\nBoot failed. System halted.\n");
    while (1) {
        __asm__ volatile("hlt");
    }
    return EFI_LOAD_ERROR;
}
