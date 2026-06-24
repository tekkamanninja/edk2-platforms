/** @file
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "GenericBdsLibInternal.h"

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
  )
{
  EFI_DEVICE_PATH_PROTOCOL    *Node;

  if ((UsbPathTxt == NULL) ||
      (FileSysPathTxt == NULL) ||
      (FileSysPath == NULL)) {
    return BBS_TYPE_UNKNOWN;
  }

  if (StrnCmp (UsbPathTxt, FileSysPathTxt, StrLen (UsbPathTxt)) == 0) {
    Node = FileSysPath;
    while (!IsDevicePathEnd (Node)) {
      if ((DevicePathType (Node) == MEDIA_DEVICE_PATH) &&
          (DevicePathSubType (Node) == MEDIA_CDROM_DP)) {
        return BBS_TYPE_CDROM;
      }
      Node = NextDevicePathNode (Node);
    }
  }

  return BBS_TYPE_UNKNOWN;
}

/**
  Internal function to get bbs device type by specificed usb boot device path.

  @param[in]  DevicePath    Pointer to the usb boot device path.

  @retval Device type from definitions of BBS specification.

**/
UINT16
InternalGetBBSTypeFromUsbPath (
  IN CONST EFI_DEVICE_PATH_PROTOCOL   *UsbPath
  )
{
  EFI_STATUS                          Status;
  EFI_HANDLE                          *FileSystemHandles;
  UINTN                               NumberFileSystemHandles;
  UINTN                               Index;
  EFI_DEVICE_PATH_PROTOCOL            *FileSysPath;
  CHAR16                              *UsbPathTxt;
  CHAR16                              *FileSysPathTxt;
  UINT16                              Result;

  Result = BBS_TYPE_UNKNOWN;

  UsbPathTxt = ConvertDevicePathToText (UsbPath, TRUE, TRUE);
  if (UsbPathTxt == NULL) {
    return Result;
  }

  Status = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &gEfiSimpleFileSystemProtocolGuid,
                        NULL,
                        &NumberFileSystemHandles,
                        &FileSystemHandles
                        );
  if (EFI_ERROR (Status)) {
    FreePool (UsbPathTxt);
    return BBS_TYPE_UNKNOWN;
  }

  for (Index = 0; Index < NumberFileSystemHandles; Index++) {
    FileSysPath = DevicePathFromHandle (FileSystemHandles[Index]);
    FileSysPathTxt = ConvertDevicePathToText (FileSysPath, TRUE, TRUE);

    if (FileSysPathTxt == NULL) {
      continue;
    }

    Result = InternalGetBBSTypeFromFileSysPath (UsbPathTxt, FileSysPathTxt, FileSysPath);
    FreePool (FileSysPathTxt);

    if (Result != BBS_TYPE_UNKNOWN) {
      break;
    }
  }

  if (NumberFileSystemHandles != 0) {
    FreePool (FileSystemHandles);
  }

  FreePool (UsbPathTxt);

  return Result;
}

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
  )
{
  VENDOR_DEVICE_PATH          *Vendor;
  UINT16                      Result;

  Result = BBS_TYPE_UNKNOWN;

  if ((DevicePath == NULL) || (Node == NULL)) {
    return Result;
  }

  switch (DevicePathSubType (Node)) {
    case MSG_MAC_ADDR_DP:
      Result = BBS_TYPE_EMBEDDED_NETWORK;
      break;

    case MSG_USB_DP:
      Result = InternalGetBBSTypeFromUsbPath (DevicePath);
      if (Result == BBS_TYPE_UNKNOWN) {
        Result = BBS_TYPE_USB;
      }
      break;

    case MSG_SATA_DP:
    case MSG_NVME_NAMESPACE_DP:
      Result = BBS_TYPE_HARDDRIVE;
      break;

    case MSG_VENDOR_DP:
      Vendor = (VENDOR_DEVICE_PATH *) (Node);
      if (&Vendor->Guid != NULL) {
        if (CompareGuid (&Vendor->Guid, &((EFI_GUID) DEVICE_PATH_MESSAGING_SAS))) {
          Result = BBS_TYPE_HARDDRIVE;
        }
      }
      break;

    default:
      Result = BBS_TYPE_UNKNOWN;
      break;
  }

  return Result;
}

/**
  Internal function to get bbs device type by specificed boot device path.

  @param[in]  DevicePath    Pointer to the boot device path.

  @retval Device type from definitions of BBS specification.

**/
UINT16
InternalGetBBSTypeByDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL    *Node;
  UINT16                      Result;

  Result = BBS_TYPE_UNKNOWN;

  if (DevicePath == NULL) {
    return Result;
  }

  Node = DevicePath;
  while (!IsDevicePathEnd (Node)) {
    switch (DevicePathType (Node)) {
      case MEDIA_DEVICE_PATH:
        if (DevicePathSubType (Node) == MEDIA_CDROM_DP) {
          Result = BBS_TYPE_CDROM;
        }
        break;

      case MESSAGING_DEVICE_PATH:
        Result = InternalGetBBSTypeFromMessagingDevicePath (DevicePath, Node);
        break;

      default:
        Result = BBS_TYPE_UNKNOWN;
        break;
    }

    if (Result != BBS_TYPE_UNKNOWN) {
      break;
    }

    Node = NextDevicePathNode (Node);
  }

  return Result;
}
