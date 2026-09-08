/** @file

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GENERIC_BDS_LIB_H__
#define __GENERIC_BDS_LIB_H__

//
// Boot category variable name and attribute
//
#define EFI_BOOT_CATEGORY_ORDER_VARIABLE_NAME       L"BootCategoryOrder"
#define EFI_BOOT_CATEGORY_ORDER_VARIABLE_ATTRIBUTE  EFI_VARIABLE_NON_VOLATILE | \
                                                    EFI_VARIABLE_BOOTSERVICE_ACCESS | \
                                                    EFI_VARIABLE_RUNTIME_ACCESS

//
// Boot category type definitions.
//
typedef enum {
  EfiBootCategoryHdd = 0,
  EfiBootCategoryCdrom,
  EfiBootCategoryPxe,
  EfiBootCategoryUsb,
  EfiBootCategoryOther,
  EfiBootCategoryMax
} EFI_BOOT_CATEGORY;

/**
  Get boot category value by specificed boot option device path.

  @param[in]  DevicePath    Pointer to the boot option device path.

  @retval Boot category value by specificed boot option device path.

**/
UINT8
EFIAPI
GenericBdsLibGetBootCategoryByDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
  );

/**
  Get the next device path node instance specified by type and subtype
  in the input device path.

  @param[in]  DevicePath    Pointer to the input device path.
  @param[in]  Type          The type field of device path node to be specified.
  @param[in]  SubType       The subtype field of device path node to be specified.

  @retval Pointer to next path node instance.
          If NULL, the device path node was not found.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
GenericBdsLibGetNextDevicePathNode (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath,
  IN UINT8                      Type,
  IN UINT8                      SubType
  );

/**
  Get the last partition device path node instance.

  @param[in]  DevicePath    Pointer to the input device path.

  @retval Pointer to the last partition device path node instance.
          If NULL, the partition device path node was not found.

**/
HARDDRIVE_DEVICE_PATH *
EFIAPI
GenericBdsLibGetLastPartitionDevicePathNode (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
  );

/**
  Vailidate the specified boot option device path is hdd short form device path.

  @param[in]  DevicePath    Pointer to the specified boot option device path.

  @retval TRUE  - Valid the device path instance.
          FALSE - Invalid the device path instance.

**/
BOOLEAN
EFIAPI
GenericBdsLibValiateHddShortDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
  );

/*
  Validate the PE header of the file specified by the file handle.

  @param[in]  FileHandle        The file handle to be validated.

  @retval TRUE  The PE header of the OS loader is valid.
  @retval FALSE The PE header of the OS loader is invalid.

*/
BOOLEAN
EFIAPI
GenericBdsLibValidatePeHeader (
  IN EFI_FILE_HANDLE            FileHandle
  );

/*
  Duplicates boot option.

  The caller is responsible for allocating memory buffer for destination boot
  option.

  @param[in] DestinationBootOption  Destination boot option buffer.
  @param[in] SourceBootOption       Source boot option buffer.

  @retval EFI_SUCCESS               Boot option duplicated successfully.
  @retval EFI_INVALID_PARAMETER     Input is not correct.
  @retval Others                    Errors returned from EfiBootManagerInitializeLoadOption.

*/
EFI_STATUS
EFIAPI
GenericBdsLibDuplicateBootOption (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION  *DestinationBootOption,
  IN EFI_BOOT_MANAGER_LOAD_OPTION   *SourceBootOption
  );

#endif // __GENERIC_BDS_LIB_H__
