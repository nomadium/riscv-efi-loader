/*
 * RISC-V EFI Bootloader
 *
 * Loads a raw binary kernel from the EFI System Partition,
 * exits boot services, and jumps to the kernel entry point.
 *
 * The kernel is expected at \kernel.bin on the ESP.
 *
 * Kernel entry convention (compatible with Linux RISC-V boot protocol):
 *   a0 = hart id (current CPU)
 *   a1 = pointer to device tree blob (FDT)
 */

#include <efi.h>
#include <efilib.h>

/* Configuration */
#define KERNEL_PATH        L"\\kernel.bin"
#define KERNEL_LOAD_ADDR   0x80200000ULL  /* Standard RISC-V Linux kernel load address */
#define DTB_LOAD_ADDR      0x82200000ULL  /* DTB location (matches OpenSBI convention) */
#define MAX_MEMORY_MAP     16384
#define FDT_MAGIC          0xd00dfeed

/* Device Tree Table GUID */
static EFI_GUID DtbTableGuid = {
    0xb1b621d5, 0xf19c, 0x41a5,
    {0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0}
};

/* RISC-V Boot Protocol GUID */
static EFI_GUID RiscvBootProtocolGuid = {
    0xccd15aa8, 0x5e42, 0x4c68,
    {0x88, 0x36, 0x24, 0x1c, 0x1d, 0x1c, 0x17, 0x9a}
};

typedef struct {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *GetBootHartId)(VOID *This, UINTN *BootHartId);
} RISCV_EFI_BOOT_PROTOCOL;

/*
 * Convert big-endian u32 to native
 */
static UINT32 fdt32_to_cpu(UINT32 x)
{
    return ((x & 0xff000000) >> 24) |
           ((x & 0x00ff0000) >> 8)  |
           ((x & 0x0000ff00) << 8)  |
           ((x & 0x000000ff) << 24);
}

/*
 * Get DTB size from header
 */
static UINT32 GetDtbSize(VOID *Dtb)
{
    UINT32 *hdr = (UINT32 *)Dtb;
    if (fdt32_to_cpu(hdr[0]) != FDT_MAGIC)
        return 0;
    return fdt32_to_cpu(hdr[1]);  /* totalsize field */
}

/*
 * Find the Device Tree Blob in EFI configuration tables
 */
static VOID *FindDtb(EFI_SYSTEM_TABLE *ST)
{
    UINTN i;

    for (i = 0; i < ST->NumberOfTableEntries; i++) {
        if (CompareGuid(&ST->ConfigurationTable[i].VendorGuid, &DtbTableGuid)) {
            return ST->ConfigurationTable[i].VendorTable;
        }
    }
    return NULL;
}

/*
 * Get the boot hart ID via RISC-V EFI boot protocol
 */
static UINTN GetBootHartId(EFI_SYSTEM_TABLE *ST)
{
    EFI_STATUS status;
    RISCV_EFI_BOOT_PROTOCOL *riscv_boot;
    UINTN hart_id = 0;

    status = LibLocateProtocol(&RiscvBootProtocolGuid, (VOID **)&riscv_boot);
    if (!EFI_ERROR(status) && riscv_boot && riscv_boot->GetBootHartId) {
        riscv_boot->GetBootHartId(riscv_boot, &hart_id);
    }
    return hart_id;
}

/*
 * Kernel entry point type
 */
typedef VOID (*kernel_entry_t)(UINTN hart_id, VOID *dtb);

/*
 * EFI application entry point
 */
EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *Volume;
    EFI_FILE_HANDLE RootDir, KernelFile;
    EFI_FILE_INFO *FileInfo;
    UINTN FileInfoSize;
    UINT8 FileInfoBuffer[512];
    UINTN KernelSize;
    EFI_PHYSICAL_ADDRESS KernelAddr;
    UINTN Pages;
    VOID *Dtb;
    UINTN HartId;
    
    /* Memory map variables */
    UINT8 MemoryMapBuffer[MAX_MEMORY_MAP];
    UINTN MemoryMapSize, MapKey, DescriptorSize;
    UINT32 DescriptorVersion;
    
    kernel_entry_t KernelEntry;

    /* Initialize gnu-efi library */
    InitializeLib(ImageHandle, SystemTable);

    /* Print banner */
    Print(L"\r\n");
    Print(L"========================================\r\n");
    Print(L"  RISC-V EFI Bootloader\r\n");
    Print(L"========================================\r\n\r\n");

    /* Get loaded image protocol */
    Print(L"Getting loaded image protocol... ");
    status = BS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid,
                                (VOID **)&LoadedImage);
    if (EFI_ERROR(status)) {
        Print(L"FAILED: %r\r\n", status);
        goto halt;
    }
    Print(L"OK\r\n");

    /* Get file system protocol */
    Print(L"Getting file system protocol... ");
    status = BS->HandleProtocol(LoadedImage->DeviceHandle,
                                &gEfiSimpleFileSystemProtocolGuid,
                                (VOID **)&Volume);
    if (EFI_ERROR(status)) {
        Print(L"FAILED: %r\r\n", status);
        goto halt;
    }
    Print(L"OK\r\n");

    /* Open root directory */
    Print(L"Opening root directory... ");
    status = Volume->OpenVolume(Volume, &RootDir);
    if (EFI_ERROR(status)) {
        Print(L"FAILED: %r\r\n", status);
        goto halt;
    }
    Print(L"OK\r\n");

    /* Open kernel file */
    Print(L"Opening kernel file %s... ", KERNEL_PATH);
    status = RootDir->Open(RootDir, &KernelFile, KERNEL_PATH,
                           EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        Print(L"FAILED: %r\r\n", status);
        Print(L"\r\nPlease place kernel at %s on the ESP.\r\n", KERNEL_PATH);
        goto halt;
    }
    Print(L"OK\r\n");

    /* Get kernel file info */
    Print(L"Getting kernel file info... ");
    FileInfoSize = sizeof(FileInfoBuffer);
    status = KernelFile->GetInfo(KernelFile, &gEfiFileInfoGuid,
                                  &FileInfoSize, FileInfoBuffer);
    if (EFI_ERROR(status)) {
        Print(L"FAILED: %r\r\n", status);
        goto halt;
    }
    FileInfo = (EFI_FILE_INFO *)FileInfoBuffer;
    KernelSize = FileInfo->FileSize;
    Print(L"OK (%d bytes)\r\n", KernelSize);

    /* Allocate memory for kernel */
    Print(L"Allocating memory at 0x%lx... ", KERNEL_LOAD_ADDR);
    KernelAddr = KERNEL_LOAD_ADDR;
    Pages = EFI_SIZE_TO_PAGES(KernelSize);
    status = BS->AllocatePages(AllocateAddress, EfiLoaderCode, Pages, &KernelAddr);
    if (EFI_ERROR(status)) {
        Print(L"(trying any address) ");
        status = BS->AllocatePages(AllocateAnyPages, EfiLoaderCode, Pages, &KernelAddr);
        if (EFI_ERROR(status)) {
            Print(L"FAILED: %r\r\n", status);
            goto halt;
        }
    }
    Print(L"OK at 0x%lx\r\n", KernelAddr);

    /* Load kernel into memory */
    Print(L"Loading kernel into memory... ");
    status = KernelFile->Read(KernelFile, &KernelSize, (VOID *)KernelAddr);
    if (EFI_ERROR(status)) {
        Print(L"FAILED: %r\r\n", status);
        goto halt;
    }
    Print(L"OK\r\n");

    /* Close file handles */
    KernelFile->Close(KernelFile);
    RootDir->Close(RootDir);

    /* Find device tree - try EFI config table first, fall back to OpenSBI location */
    Print(L"Looking for device tree... ");
    VOID *OrigDtb = FindDtb(ST);
    UINT32 DtbSize = 0;
    
    if (OrigDtb) {
        DtbSize = GetDtbSize(OrigDtb);
        if (DtbSize > 0) {
            Print(L"EFI config table at 0x%lx (%d bytes)\r\n", (UINT64)OrigDtb, DtbSize);
        } else {
            OrigDtb = NULL;  /* Invalid, try fallback */
        }
    }
    
    /* Fall back to OpenSBI's DTB location */
    if (!OrigDtb || DtbSize == 0) {
        OrigDtb = (VOID *)DTB_LOAD_ADDR;
        DtbSize = GetDtbSize(OrigDtb);
        if (DtbSize > 0) {
            Print(L"OpenSBI location at 0x%lx (%d bytes)\r\n", (UINT64)OrigDtb, DtbSize);
        } else {
            Print(L"NOT FOUND\r\n");
            OrigDtb = NULL;
        }
    }
    
    /* Use the DTB in place (it's already in a good location) */
    Dtb = OrigDtb;

    /* Get boot hart ID */
    Print(L"Getting boot hart ID... ");
    HartId = GetBootHartId(ST);
    Print(L"OK (hart %d)\r\n", HartId);

    /* Get memory map for ExitBootServices */
    Print(L"\r\nPreparing to exit boot services...\r\n");
    MemoryMapSize = sizeof(MemoryMapBuffer);
    status = BS->GetMemoryMap(&MemoryMapSize, (EFI_MEMORY_DESCRIPTOR *)MemoryMapBuffer,
                              &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(status)) {
        Print(L"Failed to get memory map: %r\r\n", status);
        goto halt;
    }

    /* Exit boot services */
    Print(L"Exiting boot services...\r\n");
    status = BS->ExitBootServices(ImageHandle, MapKey);
    if (EFI_ERROR(status)) {
        /* Memory map may have changed, try once more */
        MemoryMapSize = sizeof(MemoryMapBuffer);
        BS->GetMemoryMap(&MemoryMapSize, (EFI_MEMORY_DESCRIPTOR *)MemoryMapBuffer,
                         &MapKey, &DescriptorSize, &DescriptorVersion);
        status = BS->ExitBootServices(ImageHandle, MapKey);
    }

    if (EFI_ERROR(status)) {
        /* Can't print after failed ExitBootServices */
        while (1) {
            __asm__ volatile("wfi");
        }
    }

    /*
     * Boot services are now terminated!
     * Jump to kernel with:
     *   a0 = hart id
     *   a1 = device tree pointer
     */
    KernelEntry = (kernel_entry_t)KernelAddr;
    KernelEntry(HartId, Dtb);

    /* Should never reach here */
    while (1) {
        __asm__ volatile("wfi");
    }

halt:
    Print(L"\r\nBoot failed. Press any key...\r\n");
    WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
    return EFI_LOAD_ERROR;
}
