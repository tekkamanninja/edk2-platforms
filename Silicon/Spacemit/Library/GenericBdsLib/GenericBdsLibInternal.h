/** @file
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __GENERIC_BDS_LIB_INTERNAL_H__
#define __GENERIC_BDS_LIB_INTERNAL_H__

#include <Uefi.h>
#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/GenericBdsLib.h>
#include <Protocol/DevicePath.h>


/**
  Internal function to get bbs device type by specificed usb boot device path and
  file system device path.

  @param[in]  UsbPathTxt        Pointer to device path text string of usb boot devcie.
  @param[in]  FileSysPathTxt    Pointer to device path text string of file system device path.
  @param[in]  FileSysPath       Pointer to the file system device path.

  @retval Device type from definitions of BBS specification.

**/
UINT16
InternalGetBBSTypeFromFileSysPath (
  IN CHAR16                   *UsbPathTxt,
  IN CHAR16                   *FileSysPathTxt,
  IN EFI_DEVICE_PATH_PROTOCOL *FileSysPath
  );

/**
  Internal function to get bbs device type by specificed usb boot device path.

  @param[in]  DevicePath    Pointer to the usb boot device path.

  @retval Device type from definitions of BBS specification.

**/
UINT16
InternalGetBBSTypeFromUsbPath (
  IN CONST EFI_DEVICE_PATH_PROTOCOL   *UsbPath
  );

/**
  Internal function to get bbs device type by specificed messaging device path node.

  @param[in]  DevicePath    Pointer to the boot device path.
  @param[in]  Node          Pointer to the messaging device path node.

  @retval Device type from definitions of BBS specification.

**/
UINT16
InternalGetBBSTypeFromMessagingDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL *DevicePath,
  IN EFI_DEVICE_PATH_PROTOCOL *Node
  );

/**
  Internal function to get bbs device type by specificed boot device path.

  @param[in]  DevicePath    Pointer to the boot device path.

  @retval Device type from definitions of BBS specification.

**/
UINT16
InternalGetBBSTypeByDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL *DevicePath
  );

#endif // __GENERIC_BDS_LIB_INTERNAL_H__
