/** @file

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __DISK_INFO_LIB_H__
#define __DISK_INFO_LIB_H__

#include <Uefi.h>
#include <Protocol/DevicePath.h>

typedef enum {
  DiskSsd,
  DiskHdd,
  DiskNvme,
  DiskUnknown
} DISK_TYPE;

typedef struct {
  UINT8                     DiskType;
  EFI_HANDLE                DeviceHandle;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  CHAR16                    *DescString;
  CHAR16                    *SerialNoString;
  CHAR16                    *FirmwareVerString;
  UINTN                     CapacityInGB;
} DISK_INFO_CONTEXT;

/**
  Print context of disk informations array.

  @param[in]  DiskCount   Number of entries in the disk information array.
  @param[in]  DiskInfo    Pointer to disk information array.

**/
VOID
EFIAPI
DiskInfoLibPrintContext (
  IN UINTN                DiskCount,
  IN DISK_INFO_CONTEXT    *DiskInfo
  );

/**
  Get disk type unicode string.

  @param[in]  Type    Disk Type.

  @retval !NULL       Disk type unicode string.
  @retval NULL        Disk type is invalid.

**/
CHAR16 *
EFIAPI
DiskInfoLibGetTypeString (
  IN UINT8                Type
  );

/**
  Free context of disk informations array.

  @param[in]  DiskCount   Number of entries in the disk information array.
  @param[in]  DiskInfo    Pointer to disk information array.

**/
VOID
EFIAPI
DiskInfoLibFreeContext (
  IN UINTN                DiskCount,
  IN DISK_INFO_CONTEXT    *DiskInfo
  );

/**
  Get context of disk information array.

  @param[out] DiskCount   Number of entries in the disk information array.
  @param[out] DiskInfo    Pointer to disk information array.
                          It's caller's responsibility to free the buffer.

  @retval EFI_SUCCESS     Get disk information array successfully.
  @retval Others          The return value of LocateHandleBuffer().

**/
EFI_STATUS
EFIAPI
DiskInfoLibGetContext (
  OUT UINTN               *DiskCount,
  OUT DISK_INFO_CONTEXT   **DiskInfo
  );

#endif // __DISK_INFO_LIB_H__
