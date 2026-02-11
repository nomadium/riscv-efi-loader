/*
 * RISC-V EFI Bootloader
 *
 * Loads a raw binary kernel from the EFI System Partition,
 * exits boot services, and jumps to the kernel entry point.
 *
 * The kernel is expected at \kernel.bin on the ESP.
 * It will be loaded at the address specified by KERNEL_LOAD_ADDR.
 *
 * Kernel entry convention (compatible with Linux RISC-V boot protocol):
 *   a0 = hart id (current CPU)
 *   a1 = pointer to device tree blob (FDT)
 */

#include "efi.h"

/* Configuration */
#define KERNEL_PATH        L"\\kernel.bin"
#define KERNEL_LOAD_ADDR   0x80000000ULL  /* Load address for the kernel (must match kernel's link.ld) */
#define MAX_MEMORY_MAP     16384          /* Max size for memory map */

/* Global EFI pointers (set by crt0.S) */
extern EFI_HANDLE _image_handle;
extern EFI_SYSTEM_TABLE *_system_table;

/* Shorthand macros */
#define ST _system_table
#define BS _system_table->BootServices

/* Protocol GUIDs */
static EFI_GUID LoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID SimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID FileInfoGuid = EFI_FILE_INFO_GUID;
static EFI_GUID DtbTableGuid = EFI_DTB_TABLE_GUID;
static EFI_GUID RiscvBootProtocolGuid = RISCV_EFI_BOOT_PROTOCOL_GUID;

/*
 * Print a string to the console
 */
static void Print(CHAR16 *str)
{
    if (ST && ST->ConOut) {
        ST->ConOut->OutputString(ST->ConOut, str);
    }
}

/*
 * Print a string followed by a status code
 */
static void PrintStatus(CHAR16 *str, EFI_STATUS status)
{
    Print(str);
    if (EFI_ERROR(status)) {
        Print(L" [FAILED]\r\n");
    } else {
        Print(L" [OK]\r\n");
    }
}

/*
 * Print a 64-bit hex value
 */
static void PrintHex64(CHAR16 *prefix, UINT64 value)
{
    static CHAR16 hex[] = L"0123456789ABCDEF";
    CHAR16 buf[32];
    int i;

    Print(prefix);

    buf[0] = L'0';
    buf[1] = L'x';
    for (i = 0; i < 16; i++) {
        buf[2 + i] = hex[(value >> (60 - i * 4)) & 0xF];
    }
    buf[18] = L'\r';
    buf[19] = L'\n';
    buf[20] = L'\0';
    Print(buf);
}



/*
 * Find the Device Tree Blob in EFI configuration tables
 */
static VOID *FindDtb(void)
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
 * Returns 0 if protocol not available (will use hart 0)
 */
static UINTN GetBootHartId(void)
{
    EFI_STATUS status;
    RISCV_EFI_BOOT_PROTOCOL *riscv_boot;
    UINTN hart_id = 0;

    status = BS->LocateProtocol(&RiscvBootProtocolGuid, NULL, (VOID **)&riscv_boot);
    if (!EFI_ERROR(status) && riscv_boot && riscv_boot->GetBootHartId) {
        riscv_boot->GetBootHartId(riscv_boot, &hart_id);
    }
    return hart_id;
}

/*
 * Kernel entry point type
 *
 * a0 = hart id
 * a1 = device tree pointer
 */
typedef void (*kernel_entry_t)(UINTN hart_id, VOID *dtb);

/*
 * EFI application entry point
 */
EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable __attribute__((unused)))
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_PROTOCOL *root, *file;
    EFI_FILE_INFO *file_info;
    UINT8 file_info_buf[256];
    UINTN file_info_size;
    UINTN kernel_size;
    EFI_PHYSICAL_ADDRESS kernel_addr;
    UINTN pages;
    VOID *dtb;
    UINTN hart_id;
    
    /* Memory map variables */
    UINT8 memory_map[MAX_MEMORY_MAP];
    UINTN map_size, map_key, desc_size;
    UINT32 desc_version;
    
    kernel_entry_t kernel_entry;

    /* Clear screen and print banner */
    if (ST->ConOut) {
        ST->ConOut->ClearScreen(ST->ConOut);
    }
    Print(L"\r\n");
    Print(L"========================================\r\n");
    Print(L"  RISC-V EFI Bootloader\r\n");
    Print(L"========================================\r\n\r\n");

    /* Get loaded image protocol to find our device */
    Print(L"Getting loaded image protocol...");
    status = BS->HandleProtocol(ImageHandle, &LoadedImageProtocolGuid,
                                (VOID **)&loaded_image);
    if (EFI_ERROR(status)) {
        PrintStatus(L"", status);
        goto halt;
    }
    PrintStatus(L"", status);

    /* Get file system protocol from our device */
    Print(L"Getting file system protocol...");
    status = BS->HandleProtocol(loaded_image->DeviceHandle,
                                &SimpleFileSystemProtocolGuid,
                                (VOID **)&fs);
    if (EFI_ERROR(status)) {
        PrintStatus(L"", status);
        goto halt;
    }
    PrintStatus(L"", status);

    /* Open the root directory */
    Print(L"Opening root directory...");
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        PrintStatus(L"", status);
        goto halt;
    }
    PrintStatus(L"", status);

    /* Open the kernel file */
    Print(L"Opening kernel file: ");
    Print(KERNEL_PATH);
    Print(L"...");
    status = root->Open(root, &file, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        PrintStatus(L"", status);
        Print(L"\r\nERROR: Kernel file not found!\r\n");
        Print(L"Please place your kernel at ");
        Print(KERNEL_PATH);
        Print(L" on the EFI System Partition.\r\n");
        goto halt;
    }
    PrintStatus(L"", status);

    /* Get kernel file size */
    Print(L"Getting kernel file info...");
    file_info_size = sizeof(file_info_buf);
    status = file->GetInfo(file, &FileInfoGuid, &file_info_size, file_info_buf);
    if (EFI_ERROR(status)) {
        PrintStatus(L"", status);
        goto halt;
    }
    file_info = (EFI_FILE_INFO *)file_info_buf;
    kernel_size = file_info->FileSize;
    PrintStatus(L"", status);
    PrintHex64(L"  Kernel size: ", kernel_size);

    /* Allocate memory for kernel at the load address */
    Print(L"Allocating memory for kernel...");
    kernel_addr = KERNEL_LOAD_ADDR;
    pages = EFI_SIZE_TO_PAGES(kernel_size);
    status = BS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &kernel_addr);
    if (EFI_ERROR(status)) {
        /* Try allocating anywhere if specific address fails */
        Print(L" (trying any address)...");
        status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &kernel_addr);
        if (EFI_ERROR(status)) {
            PrintStatus(L"", status);
            goto halt;
        }
    }
    PrintStatus(L"", status);
    PrintHex64(L"  Load address: ", kernel_addr);

    /* Read kernel into memory */
    Print(L"Loading kernel into memory...");
    status = file->Read(file, &kernel_size, (VOID *)kernel_addr);
    if (EFI_ERROR(status)) {
        PrintStatus(L"", status);
        goto halt;
    }
    PrintStatus(L"", status);

    /* Close file handles */
    file->Close(file);
    root->Close(root);

    /* Find device tree blob */
    Print(L"Looking for device tree blob...");
    dtb = FindDtb();
    if (dtb) {
        PrintStatus(L"", EFI_SUCCESS);
        PrintHex64(L"  DTB address: ", (UINT64)dtb);
    } else {
        Print(L" [NOT FOUND - kernel may fail]\r\n");
    }

    /* Get boot hart ID */
    Print(L"Getting boot hart ID...");
    hart_id = GetBootHartId();
    PrintStatus(L"", EFI_SUCCESS);
    PrintHex64(L"  Hart ID: ", hart_id);

    /* Prepare to exit boot services */
    Print(L"\r\nPreparing to exit boot services...\r\n");

    /* Get memory map (required for ExitBootServices) */
    map_size = sizeof(memory_map);
    status = BS->GetMemoryMap(&map_size, (EFI_MEMORY_DESCRIPTOR *)memory_map,
                              &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) {
        Print(L"Failed to get memory map!\r\n");
        goto halt;
    }

    /* Exit boot services - point of no return */
    Print(L"Exiting boot services...\r\n");
    status = BS->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        /* Memory map may have changed, try again */
        map_size = sizeof(memory_map);
        status = BS->GetMemoryMap(&map_size, (EFI_MEMORY_DESCRIPTOR *)memory_map,
                                  &map_key, &desc_size, &desc_version);
        if (!EFI_ERROR(status)) {
            status = BS->ExitBootServices(ImageHandle, map_key);
        }
    }

    if (EFI_ERROR(status)) {
        /* We can't print anymore after failed ExitBootServices attempt */
        goto halt;
    }

    /*
     * Boot services are now terminated!
     * - No more EFI console output
     * - No more memory allocation
     * - Only runtime services available
     */

    /* Set up kernel entry point */
    kernel_entry = (kernel_entry_t)kernel_addr;

    /* Jump to kernel!
     * a0 = hart id
     * a1 = device tree pointer
     */
    kernel_entry(hart_id, dtb);

    /* Should never reach here */
    while (1) {
        __asm__ volatile("wfi");
    }

halt:
    Print(L"\r\nBoot failed. System halted.\r\n");
    Print(L"Press any key to continue...\r\n");
    
    /* Wait for keypress */
    {
        UINTN index;
        EFI_INPUT_KEY key;
        BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
        ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
    }
    
    while (1) {
        __asm__ volatile("wfi");
    }
    
    return EFI_LOAD_ERROR;
}
