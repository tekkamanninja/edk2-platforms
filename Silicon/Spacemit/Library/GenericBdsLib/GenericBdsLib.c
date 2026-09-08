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

/**
  Normalize a file path string by ensuring it starts with a backslash,
  removing consecutive duplicate backslashes, and stripping the trailing
  backslash.

  The caller is responsible for freeing the returned string with FreePool().

  @param[in] PathName  The file path string to normalize.

  @retval NULL    PathName is NULL, empty, or memory allocation failed.
  @retval Other   A pointer to the newly allocated normalized path string.

**/
CHAR16 *
EFIAPI
GenericBdsLibRegularFilePathName (
  IN CHAR16     *PathName
  )
{
  CHAR16        *String;
  UINTN         StringLen;
  UINTN         Index, StringIndex;

  if ((PathName == NULL) ||
      (*PathName == L'\0')) {
    return NULL;
  }

  String = AllocateZeroPool (StrSize (PathName) + StrLen (L"\\") * sizeof (CHAR16));
  if (String == NULL) {
    return NULL;
  }

  StringIndex           = 0;
  String[StringIndex++] = L'\\';
  StringLen             = StrLen (PathName);

  for (Index = 0; Index < StringLen; Index++) {
    if ((PathName[Index] == L'\\') &&
        ((StringIndex > 0) && (String[StringIndex - 1] == L'\\'))) {
      continue;
    }

    String[StringIndex++] = PathName[Index];
  }

  if ((StringIndex > 1) && (String[StringIndex - 1] == L'\\')) {
    StringIndex--;
  }

  String[StringIndex] = L'\0';

  return String;
}

/**
  Extract and concatenate all file path names from consecutive
  MEDIA_FILEPATH_DP device path nodes, then return the normalized
  result.

  This function locates the first MEDIA_FILEPATH_DP node in the given
  device path, concatenates the PathName of all consecutive file path
  nodes separated by backslashes, and normalizes the combined string
  via GenericBdsLibRegularFilePathName().

  The caller is responsible for freeing the returned string with FreePool().

  @param[in] DevicePath  A pointer to the device path protocol instance.

  @retval NULL    No MEDIA_FILEPATH_DP node found, or memory allocation failed.
  @retval Other   A pointer to the newly allocated normalized file path string.

**/
CHAR16 *
EFIAPI
GenericBdsLibGetFilePathName (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL      *FilePathNode;
  EFI_DEVICE_PATH_PROTOCOL      *Node;
  UINTN                         StringLen;
  CHAR16                        *String;
  CHAR16                        *RegularPathName;

  StringLen       = 0;
  String          = NULL;
  RegularPathName = NULL;

  FilePathNode = GenericBdsLibGetNextDevicePathNode (DevicePath, MEDIA_DEVICE_PATH, MEDIA_FILEPATH_DP);
  if (FilePathNode == NULL) {
    return NULL;
  }

  Node = FilePathNode;
  while (!IsDevicePathEnd (Node)) {
    if (!((DevicePathType (Node) == MEDIA_DEVICE_PATH) &&
          (DevicePathSubType (Node) == MEDIA_FILEPATH_DP))) {
      break;
    }

    StringLen += StrLen (((FILEPATH_DEVICE_PATH *) Node)->PathName);
    StringLen += StrLen (L"\\");

    Node = NextDevicePathNode (Node);
  }

  if (StringLen == 0) {
    return NULL;
  }

  String = AllocateZeroPool ((StringLen + 1) * sizeof (CHAR16));
  if (String == NULL) {
    return NULL;
  }

  Node = FilePathNode;
  while (!IsDevicePathEnd (Node)) {
    if (!((DevicePathType (Node) == MEDIA_DEVICE_PATH) &&
          (DevicePathSubType (Node) == MEDIA_FILEPATH_DP))) {
      break;
    }

    StrCatS (
      String,
      StringLen + 1,
      ((FILEPATH_DEVICE_PATH *) Node)->PathName
      );

    StrCatS (
      String,
      StringLen + 1,
      L"\\"
      );

    Node = NextDevicePathNode (Node);
  }

  RegularPathName = GenericBdsLibRegularFilePathName (String);

  FreePool (String);

  return RegularPathName;
}

/*
  Validate the PE image specified by the file handle.

  @param[in]  FileHandle        The file handle to be validated.
  @param[out] FileBuffer        Optional a pointer to buffer to store file content.
                                The buffer is allocated by this routine and it is the
                                responsibility of the caller to free the memory allocated.
  @param[out] FileBufferSize    Optional a pointer to buffer to store the file content.
                                If FileBuffer is not NULL, FileBufferSize must be not NULL too.

  @retval EFI_SUCCESS           The PE image is valid and read file successfully if FileBuffer
                                is not NULL.
  @retval EFI_INVALID_PARAMETER One of the input parameters is invalid.
  @retval EFI_OUT_OF_RESOURCES  Allocate memory failed.
  @retval EFI_DEVICE_ERROR      The file operations are failed.
  @retval Others                Other failures.

*/
EFI_STATUS
EFIAPI
GenericBdsLibValidatePeImage (
  IN  EFI_FILE_HANDLE           FileHandle,
  OUT VOID                      **FileBuffer    OPTIONAL,
  OUT UINT64                    *FileBufferSize OPTIONAL
  )
{
  EFI_STATUS                        Status;
  EFI_FILE_INFO                     *FileInfo;
  UINT64                            FileSize;
  UINTN                             BufferSize;
  EFI_IMAGE_DOS_HEADER              DosHeader;
  EFI_IMAGE_OPTIONAL_HEADER_UNION   PeHeader;
  UINT16                            Subsystem;

  FileInfo = NULL;

  ZeroMem (&DosHeader, sizeof (EFI_IMAGE_DOS_HEADER));
  ZeroMem (&PeHeader, sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION));

  if (FileHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (FileBuffer) {
    *FileBuffer = NULL;
  }

  if (FileBufferSize) {
    *FileBufferSize = 0;
  }

  FileInfo = FileHandleGetInfo (FileHandle);
  if (FileInfo == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto FreeExit;
  }

  if (FileInfo->Attribute & EFI_FILE_DIRECTORY) {
    Status = EFI_DEVICE_ERROR;
    goto FreeExit;
  }

  FileSize = FileInfo->FileSize;

  BufferSize = sizeof (EFI_IMAGE_DOS_HEADER);
  Status = FileHandleRead (FileHandle, &BufferSize, &DosHeader);
  if (EFI_ERROR (Status)) {
    goto FreeExit;
  }

  if (!((FileSize >= sizeof (EFI_IMAGE_DOS_HEADER)) &&
        (FileSize > DosHeader.e_lfanew) &&
        (BufferSize >= sizeof (EFI_IMAGE_DOS_HEADER)) &&
        (DosHeader.e_magic == EFI_IMAGE_DOS_SIGNATURE))) {
    Status = EFI_DEVICE_ERROR;
    goto FreeExit;
  }

  Status = FileHandleSetPosition (FileHandle, DosHeader.e_lfanew);
  if (EFI_ERROR (Status)) {
    goto FreeExit;
  }

  BufferSize = sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION);
  Status = FileHandleRead (FileHandle, &BufferSize, &PeHeader);
  if (EFI_ERROR (Status)) {
    goto FreeExit;
  }

  if (!((FileSize >= (DosHeader.e_lfanew + sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION))) &&
        (BufferSize >= sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION)) &&
        (PeHeader.Pe32.Signature == EFI_IMAGE_NT_SIGNATURE) &&
        (EFI_IMAGE_MACHINE_TYPE_SUPPORTED (PeHeader.Pe32.FileHeader.Machine)))) {
    Status = EFI_DEVICE_ERROR;
    goto FreeExit;
  }

  if (PeHeader.Pe32.OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    Subsystem = PeHeader.Pe32.OptionalHeader.Subsystem;
  } else if (PeHeader.Pe32Plus.OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    Subsystem = PeHeader.Pe32Plus.OptionalHeader.Subsystem;
  } else {
    Status = EFI_DEVICE_ERROR;
    goto FreeExit;
  }

  if (!(Subsystem == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION)) {
    Status = EFI_DEVICE_ERROR;
    goto FreeExit;
  }

  if (FileBuffer) {
    *FileBuffer = AllocateZeroPool (FileSize);
    if (*FileBuffer == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto FreeExit;
    }

    Status = FileHandleSetPosition (FileHandle, 0);
    if (!EFI_ERROR (Status)) {
      Status = FileHandleRead (FileHandle, (UINTN *) &FileSize, *FileBuffer);
      if (!EFI_ERROR (Status)) {
        if (FileBufferSize) {
          *FileBufferSize = FileSize;
        }
      }
    }
    if (EFI_ERROR (Status)) {
      FreePool (*FileBuffer);
      *FileBuffer = NULL;
    }
  }

FreeExit:
  FileHandleClose (FileHandle);

  if (FileInfo) {
    FreePool (FileInfo);
  }

  return Status;
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
  IN  EFI_BOOT_MANAGER_LOAD_OPTION  *SourceBootOption
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
