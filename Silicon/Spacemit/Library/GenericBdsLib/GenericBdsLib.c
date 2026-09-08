/** @file
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "GenericBdsLibInternal.h"

/**
  Get boot category value by specificed boot option device path.

  @param[in]  DevicePath    Pointer to the boot option device path.

  @retval Boot category value by specificed boot option device path.

**/
UINT8
EFIAPI
GenericBdsLibGetBootCategoryByDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
  )
{
  UINT8     BootCategory;
  UINT16    Result;

  BootCategory = EfiBootCategoryOther;

  if (DevicePath == NULL) {
    return BootCategory;
  }

  if (GenericBdsLibValiateHddShortDevicePath (DevicePath)) {
    return EfiBootCategoryHdd;
  }

  Result = InternalGetBBSTypeByDevicePath (DevicePath);

  switch (Result) {
    case BBS_TYPE_HARDDRIVE:
      BootCategory = EfiBootCategoryHdd;
      break;
    case BBS_TYPE_CDROM:
      BootCategory = EfiBootCategoryCdrom;
      break;
    case BBS_TYPE_EMBEDDED_NETWORK:
      BootCategory = EfiBootCategoryPxe;
      break;
    case BBS_TYPE_USB:
      BootCategory = EfiBootCategoryUsb;
      break;
    default:
      break;
  }

  return BootCategory;
}

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
  )
{
  EFI_DEVICE_PATH_PROTOCOL      *Node;

  if (DevicePath == NULL) {
    return NULL;
  }

  if (!IsDevicePathValid (DevicePath, 0)) {
    return NULL;
  }

  Node = DevicePath;
  while (!IsDevicePathEnd (Node)) {
    if ((DevicePathType (Node) == Type) &&
        (DevicePathSubType (Node) == SubType)) {
      break;
    }
    Node = NextDevicePathNode (Node);
  }

  if (IsDevicePathEnd (Node)) {
    return NULL;
  }

  return Node;
}

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
  )
{
  EFI_DEVICE_PATH_PROTOCOL      *Node;
  HARDDRIVE_DEVICE_PATH         *HddNode;

  HddNode = NULL;

  if (DevicePath == NULL) {
    return NULL;
  }

  if (!IsDevicePathValid (DevicePath, 0)) {
    return NULL;
  }

  Node = DevicePath;
  while (!IsDevicePathEnd (Node)) {
    Node = GenericBdsLibGetNextDevicePathNode (Node, MEDIA_DEVICE_PATH, MEDIA_HARDDRIVE_DP);
    if (Node == NULL) {
      break;
    }
    HddNode = (HARDDRIVE_DEVICE_PATH *) Node;
    Node    = NextDevicePathNode (Node);
  }

  return HddNode;
}

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
  )
{
  EFI_DEVICE_PATH_PROTOCOL      *Node;

  if (DevicePath == NULL) {
    return FALSE;
  }

  if (!((DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == MEDIA_HARDDRIVE_DP))) {
    return FALSE;
  }

  Node = NextDevicePathNode (DevicePath);
  Node = GenericBdsLibGetNextDevicePathNode (Node, MEDIA_DEVICE_PATH, MEDIA_FILEPATH_DP);
  if (Node == NULL) {
    return FALSE;
  }

  return TRUE;
}

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
  )
{
  EFI_STATUS                        Status;
  EFI_FILE_INFO                     *FileInfo;
  UINTN                             FileSize;
  UINTN                             BufferSize;
  EFI_IMAGE_DOS_HEADER              DosHeader;
  EFI_IMAGE_OPTIONAL_HEADER_UNION   PeHeader;
  EFI_IMAGE_OPTIONAL_HEADER32       *OptionalHeader;

  FileInfo = NULL;

  ZeroMem (&DosHeader, sizeof (EFI_IMAGE_DOS_HEADER));
  ZeroMem (&PeHeader, sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION));

  if (FileHandle == NULL) {
    return FALSE;
  }

  FileInfo = FileHandleGetInfo (FileHandle);
  if (FileInfo == NULL) {
    return FALSE;
  }

  if (FileInfo->Attribute & EFI_FILE_DIRECTORY) {
    FreePool (FileInfo);
    return FALSE;
  }

  FileSize = (UINTN) FileInfo->FileSize;
  FreePool (FileInfo);

  BufferSize = sizeof (EFI_IMAGE_DOS_HEADER);
  Status = FileHandleRead (FileHandle, &BufferSize, &DosHeader);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if (!((FileSize >= sizeof (EFI_IMAGE_DOS_HEADER)) &&
        (FileSize > DosHeader.e_lfanew) &&
        (BufferSize >= sizeof (EFI_IMAGE_DOS_HEADER)) &&
        (DosHeader.e_magic == EFI_IMAGE_DOS_SIGNATURE))) {
    return FALSE;
  }

  Status = FileHandleSetPosition (FileHandle, DosHeader.e_lfanew);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  BufferSize = sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION);
  Status = FileHandleRead (FileHandle, &BufferSize, &PeHeader);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if (!((FileSize >= (DosHeader.e_lfanew + sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION))) &&
        (BufferSize >= sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION)) &&
        (PeHeader.Pe32.Signature == EFI_IMAGE_NT_SIGNATURE) &&
        (EFI_IMAGE_MACHINE_TYPE_SUPPORTED (PeHeader.Pe32.FileHeader.Machine)))) {
    return FALSE;
  }

  OptionalHeader = &PeHeader.Pe32.OptionalHeader;
  if (!((OptionalHeader->Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) ||
        (OptionalHeader->Magic == EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC))) {
    return FALSE;
  }

  if (!(OptionalHeader->Subsystem == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION)) {
    return FALSE;
  }

  return TRUE;
}

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
  )
{
  EFI_STATUS        Status;

  if ((DestinationBootOption == NULL) || (SourceBootOption == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EfiBootManagerInitializeLoadOption (
                   DestinationBootOption,
                   SourceBootOption->OptionNumber,
                   SourceBootOption->OptionType,
                   SourceBootOption->Attributes,
                   SourceBootOption->Description,
                   SourceBootOption->FilePath,
                   SourceBootOption->OptionalData,
                   SourceBootOption->OptionalDataSize
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CopyGuid (&DestinationBootOption->VendorGuid, &SourceBootOption->VendorGuid);

  return EFI_SUCCESS;
}
