/*
 * Minimal EFI definitions for RISC-V 64-bit
 * Based on UEFI Specification 2.10
 */
#ifndef _EFI_H_
#define _EFI_H_

/* Basic EFI types - no stdint.h in freestanding environment */
typedef unsigned long long UINTN;
typedef long long          INTN;
typedef unsigned char      UINT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;
typedef unsigned long long UINT64;
typedef signed char        INT8;
typedef short              INT16;
typedef int                INT32;
typedef long long          INT64;
typedef unsigned char      BOOLEAN;
typedef unsigned short     CHAR16;   /* Must match -fshort-wchar */
typedef void               VOID;

typedef UINTN    EFI_STATUS;
typedef VOID*    EFI_HANDLE;
typedef VOID*    EFI_EVENT;
typedef UINT64   EFI_PHYSICAL_ADDRESS;
typedef UINT64   EFI_VIRTUAL_ADDRESS;

#define IN
#define OUT
#define OPTIONAL
#define CONST const

#define TRUE  1
#define FALSE 0
#define NULL  ((void *)0)

/* EFI calling convention for RISC-V is standard */
#define EFIAPI

/* Status codes */
#define EFI_SUCCESS              0ULL
#define EFIERR(a)                ((a) | (1ULL << 63))
#define EFI_ERROR(a)             (((INTN)(a)) < 0)
#define EFI_LOAD_ERROR           EFIERR(1)
#define EFI_INVALID_PARAMETER    EFIERR(2)
#define EFI_UNSUPPORTED          EFIERR(3)
#define EFI_BAD_BUFFER_SIZE      EFIERR(4)
#define EFI_BUFFER_TOO_SMALL     EFIERR(5)
#define EFI_NOT_READY            EFIERR(6)
#define EFI_DEVICE_ERROR         EFIERR(7)
#define EFI_NOT_FOUND            EFIERR(14)

/* Memory types */
typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

/* GUIDs */
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/* Table header */
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* Forward declarations */
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct _EFI_RUNTIME_SERVICES EFI_RUNTIME_SERVICES;
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct _EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE_PROTOCOL;
typedef struct _EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL;

/* Input key */
typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

/* Simple Text Input Protocol */
typedef EFI_STATUS (EFIAPI *EFI_INPUT_RESET)(
    IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    IN BOOLEAN ExtendedVerification);

typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY)(
    IN  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    OUT EFI_INPUT_KEY *Key);

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET    Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    EFI_EVENT          WaitForKey;
};

/* Simple Text Output Protocol */
typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    IN BOOLEAN ExtendedVerification);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    IN CHAR16 *String);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET       Reset;
    EFI_TEXT_STRING      OutputString;
    VOID                 *TestString;
    VOID                 *QueryMode;
    VOID                 *SetMode;
    VOID                 *SetAttribute;
    EFI_TEXT_CLEAR_SCREEN ClearScreen;
    VOID                 *SetCursorPosition;
    VOID                 *EnableCursor;
    VOID                 *Mode;
};

/* Memory descriptor */
typedef struct {
    UINT32                Type;
    EFI_PHYSICAL_ADDRESS  PhysicalStart;
    EFI_VIRTUAL_ADDRESS   VirtualStart;
    UINT64                NumberOfPages;
    UINT64                Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* File info */
typedef struct {
    UINT64   Size;
    UINT64   FileSize;
    UINT64   PhysicalSize;
    UINT64   CreateTime[2];      /* EFI_TIME */
    UINT64   LastAccessTime[2];
    UINT64   ModificationTime[2];
    UINT64   Attribute;
    CHAR16   FileName[1];
} EFI_FILE_INFO;

#define EFI_FILE_INFO_GUID \
    { 0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

/* File Protocol */
#define EFI_FILE_MODE_READ   0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE  0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL

#define EFI_FILE_DIRECTORY   0x0000000000000010ULL

typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    IN  EFI_FILE_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **NewHandle,
    IN  CHAR16 *FileName,
    IN  UINT64 OpenMode,
    IN  UINT64 Attributes);

typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(
    IN EFI_FILE_PROTOCOL *This);

typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(
    IN     EFI_FILE_PROTOCOL *This,
    IN OUT UINTN *BufferSize,
    OUT    VOID *Buffer);

typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(
    IN     EFI_FILE_PROTOCOL *This,
    IN     EFI_GUID *InformationType,
    IN OUT UINTN *BufferSize,
    OUT    VOID *Buffer);

typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_POSITION)(
    IN EFI_FILE_PROTOCOL *This,
    IN UINT64 Position);

struct _EFI_FILE_PROTOCOL {
    UINT64               Revision;
    EFI_FILE_OPEN        Open;
    EFI_FILE_CLOSE       Close;
    VOID                 *Delete;
    EFI_FILE_READ        Read;
    VOID                 *Write;
    VOID                 *GetPosition;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_INFO    GetInfo;
    VOID                 *SetInfo;
    VOID                 *Flush;
};

/* Simple File System Protocol */
#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    { 0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
    IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL **Root);

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64                                      Revision;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume;
};

/* Loaded Image Protocol */
#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    { 0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

struct _EFI_LOADED_IMAGE_PROTOCOL {
    UINT32                    Revision;
    EFI_HANDLE                ParentHandle;
    VOID                      *SystemTable;
    EFI_HANDLE                DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL  *FilePath;
    VOID                      *Reserved;
    UINT32                    LoadOptionsSize;
    VOID                      *LoadOptions;
    VOID                      *ImageBase;
    UINT64                    ImageSize;
    EFI_MEMORY_TYPE           ImageCodeType;
    EFI_MEMORY_TYPE           ImageDataType;
    VOID                      *Unload;
};

/* Device Path Protocol */
#define EFI_DEVICE_PATH_PROTOCOL_GUID \
    { 0x09576e91, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

struct _EFI_DEVICE_PATH_PROTOCOL {
    UINT8 Type;
    UINT8 SubType;
    UINT8 Length[2];
};

/* Boot Services */
typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES)(
    IN     EFI_ALLOCATE_TYPE Type,
    IN     EFI_MEMORY_TYPE MemoryType,
    IN     UINTN Pages,
    IN OUT EFI_PHYSICAL_ADDRESS *Memory);

typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES)(
    IN EFI_PHYSICAL_ADDRESS Memory,
    IN UINTN Pages);

typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP)(
    IN OUT UINTN *MemoryMapSize,
    IN OUT EFI_MEMORY_DESCRIPTOR *MemoryMap,
    OUT    UINTN *MapKey,
    OUT    UINTN *DescriptorSize,
    OUT    UINT32 *DescriptorVersion);

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(
    IN  EFI_MEMORY_TYPE PoolType,
    IN  UINTN Size,
    OUT VOID **Buffer);

typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(
    IN VOID *Buffer);

typedef EFI_STATUS (EFIAPI *EFI_WAIT_FOR_EVENT)(
    IN  UINTN NumberOfEvents,
    IN  EFI_EVENT *Event,
    OUT UINTN *Index);

typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL)(
    IN  EFI_HANDLE Handle,
    IN  EFI_GUID *Protocol,
    OUT VOID **Interface);

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL)(
    IN  EFI_GUID *Protocol,
    IN  VOID *Registration OPTIONAL,
    OUT VOID **Interface);

typedef EFI_STATUS (EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    IN EFI_HANDLE ImageHandle,
    IN UINTN MapKey);

typedef EFI_STATUS (EFIAPI *EFI_STALL)(
    IN UINTN Microseconds);

typedef VOID (EFIAPI *EFI_SET_MEM)(
    IN VOID  *Buffer,
    IN UINTN Size,
    IN UINT8 Value);

typedef VOID (EFIAPI *EFI_COPY_MEM)(
    IN VOID  *Destination,
    IN VOID  *Source,
    IN UINTN Length);

struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER         Hdr;

    /* Task Priority Services */
    VOID                     *RaiseTPL;
    VOID                     *RestoreTPL;

    /* Memory Services */
    EFI_ALLOCATE_PAGES       AllocatePages;
    EFI_FREE_PAGES           FreePages;
    EFI_GET_MEMORY_MAP       GetMemoryMap;
    EFI_ALLOCATE_POOL        AllocatePool;
    EFI_FREE_POOL            FreePool;

    /* Event & Timer Services */
    VOID                     *CreateEvent;
    VOID                     *SetTimer;
    EFI_WAIT_FOR_EVENT       WaitForEvent;
    VOID                     *SignalEvent;
    VOID                     *CloseEvent;
    VOID                     *CheckEvent;

    /* Protocol Handler Services */
    VOID                     *InstallProtocolInterface;
    VOID                     *ReinstallProtocolInterface;
    VOID                     *UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL      HandleProtocol;
    VOID                     *Reserved;
    VOID                     *RegisterProtocolNotify;
    VOID                     *LocateHandle;
    VOID                     *LocateDevicePath;
    VOID                     *InstallConfigurationTable;

    /* Image Services */
    VOID                     *LoadImage;
    VOID                     *StartImage;
    VOID                     *Exit;
    VOID                     *UnloadImage;
    EFI_EXIT_BOOT_SERVICES   ExitBootServices;

    /* Miscellaneous Services */
    VOID                     *GetNextMonotonicCount;
    EFI_STALL                Stall;
    VOID                     *SetWatchdogTimer;

    /* DriverSupport Services */
    VOID                     *ConnectController;
    VOID                     *DisconnectController;

    /* Open and Close Protocol Services */
    VOID                     *OpenProtocol;
    VOID                     *CloseProtocol;
    VOID                     *OpenProtocolInformation;

    /* Library Services */
    VOID                     *ProtocolsPerHandle;
    VOID                     *LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL      LocateProtocol;
    VOID                     *InstallMultipleProtocolInterfaces;
    VOID                     *UninstallMultipleProtocolInterfaces;

    /* 32-bit CRC Services */
    VOID                     *CalculateCrc32;

    /* Miscellaneous Services */
    EFI_COPY_MEM             CopyMem;
    EFI_SET_MEM              SetMem;
    VOID                     *CreateEventEx;
};

/* Configuration Table */
typedef struct {
    EFI_GUID VendorGuid;
    VOID     *VendorTable;
} EFI_CONFIGURATION_TABLE;

/* RISC-V Boot Protocol - for passing hart id and device tree */
#define RISCV_EFI_BOOT_PROTOCOL_GUID \
    { 0xccd15aa8, 0x5e42, 0x4c68, {0x88, 0x36, 0x24, 0x1c, 0x1d, 0x1c, 0x17, 0x9a} }

typedef struct {
    UINT64 Revision;
    UINT64 (*GetBootHartId)(VOID *This, UINTN *BootHartId);
} RISCV_EFI_BOOT_PROTOCOL;

/* Device Tree Table GUID (for finding DTB in config tables) */
#define EFI_DTB_TABLE_GUID \
    { 0xb1b621d5, 0xf19c, 0x41a5, {0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0} }

/* System Table */
typedef struct {
    EFI_TABLE_HEADER                  Hdr;
    CHAR16                            *FirmwareVendor;
    UINT32                            FirmwareRevision;
    EFI_HANDLE                        ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL    *ConIn;
    EFI_HANDLE                        ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *ConOut;
    EFI_HANDLE                        StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *StdErr;
    EFI_RUNTIME_SERVICES              *RuntimeServices;
    EFI_BOOT_SERVICES                 *BootServices;
    UINTN                             NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE           *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* Helper macros */
#define EFI_PAGE_SIZE  4096
#define EFI_SIZE_TO_PAGES(s) (((s) + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE)

/* GUID comparison */
static inline int CompareGuid(const EFI_GUID *a, const EFI_GUID *b) {
    const UINT64 *pa = (const UINT64 *)a;
    const UINT64 *pb = (const UINT64 *)b;
    return (pa[0] == pb[0]) && (pa[1] == pb[1]);
}

#endif /* _EFI_H_ */
