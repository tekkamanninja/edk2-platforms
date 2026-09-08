/** @file

  Provides library functions for common SMBIOS operations. Only available to DXE
  and UEFI module types.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.<BR>
  Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.<BR>
  Copyright (c) 2012, Apple Inc. All rights reserved.
  Portitions Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/SmbiosLib.h>

EFI_SMBIOS_PROTOCOL  *mSmbios = NULL;

/**
  Create an initial SMBIOS Table from an array of SMBIOS_TEMPLATE_ENTRY
  entries. SMBIOS_TEMPLATE_ENTRY.NULL indicates the end of the table.

  @param[in]  Template            Array of SMBIOS_TEMPLATE_ENTRY entries.

  @retval EFI_SUCCESS             New SMBIOS tables were created.
  @retval EFI_OUT_OF_RESOURCES    New SMBIOS tables were not created.

**/
EFI_STATUS
EFIAPI
SmbiosLibInitializeFromTemplate (
  IN SMBIOS_TEMPLATE_ENTRY  *Template
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  if (Template == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;

  for (Index = 0; Template[Index].Entry != NULL; Index++) {
    Status = SmbiosLibCreateEntry (Template[Index].Entry, Template[Index].StringArray);
  }

  return Status;
}

/**
  Create SMBIOS record.

  Converts a fixed SMBIOS structure and an array of pointers to strings into
  an SMBIOS record where the strings are cat'ed on the end of the fixed record
  and terminated via a double NULL and add to SMBIOS table.

  @param[in]  SmbiosEntry         Fixed SMBIOS structure
  @param[in]  StringArray         Array of strings to convert to an SMBIOS string pack.
                                  NULL is OK.

  @retval Return the status form mSmbios->Add.
  @retval EFI_OUT_OF_RESOURCES    New SMBIOS tables were not created.

**/
EFI_STATUS
EFIAPI
SmbiosLibCreateEntry (
  IN SMBIOS_STRUCTURE       *SmbiosEntry,
  IN CHAR8                  **StringArray
  )
{
  EFI_STATUS                Status;
  EFI_SMBIOS_HANDLE         SmbiosHandle;
  EFI_SMBIOS_TABLE_HEADER   *Record;
  UINTN                     Index;
  UINTN                     StringSize;
  UINTN                     Size;
  CHAR8                     *Str;

  // Calculate the size of the fixed record and optional string pack
  Size = SmbiosEntry->Length;
  if (StringArray == NULL) {
    Size += 2; // Min string section is double null
  } else if (StringArray[0] == NULL) {
    Size += 2; // Min string section is double null
  } else {
    for (Index = 0; StringArray[Index] != NULL; Index++) {
      StringSize = AsciiStrSize (StringArray[Index]);
      Size      += StringSize;
    }

    // Don't forget the terminating double null
    Size += 1;
  }

  // Copy over Template
  Record = (EFI_SMBIOS_TABLE_HEADER *)AllocateZeroPool (Size);
  if (Record == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (Record, SmbiosEntry, SmbiosEntry->Length);

  if (StringArray != NULL) {
    // Append string pack
    Str = ((CHAR8 *)Record) + Record->Length;
    for (Index = 0; StringArray[Index] != NULL; Index++) {
      StringSize = AsciiStrSize (StringArray[Index]);
      CopyMem (Str, StringArray[Index], StringSize);
      Str += StringSize;
    }

    *Str = 0;
  }

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status       = mSmbios->Add (
                            mSmbios,
                            gImageHandle,
                            &SmbiosHandle,
                            Record
                            );

  FreePool (Record);
  return Status;
}

/**
  Update the string associated with an existing SMBIOS record.

  This function allows the update of specific SMBIOS strings. The number of valid strings for any
  SMBIOS record is defined by how many strings were present when Add() was called.

  @param[in]  SmbiosHandle        SMBIOS Handle of structure that will have its string updated.
  @param[in]  StringNumber        The non-zero string number of the string to update.
  @param[in]  String              Update the StringNumber string with Ascii String.

  @retval EFI_SUCCESS             SmbiosHandle had its StringNumber String updated.
  @retval EFI_INVALID_PARAMETER   SmbiosHandle does not exist. Or String is invalid.
  @retval EFI_UNSUPPORTED         String was not added because it is longer than the SMBIOS Table supports.
  @retval EFI_NOT_FOUND           The StringNumber.is not valid for this SMBIOS record.

**/
EFI_STATUS
EFIAPI
SmbiosLibUpdateString (
  IN  EFI_SMBIOS_HANDLE     SmbiosHandle,
  IN  SMBIOS_TABLE_STRING   StringNumber,
  IN  CHAR8                 *String
  )
{
  UINTN  StringIndex;

  if (String == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (*String == '\0') {
    // A string with no data is not legal in SMBIOS
    return EFI_INVALID_PARAMETER;
  }

  StringIndex = StringNumber;
  return mSmbios->UpdateString (mSmbios, &SmbiosHandle, &StringIndex, String);
}

/**
  Update the string associated with an existing SMBIOS record.

  This function allows the update of specific SMBIOS strings. The number of valid strings for any
  SMBIOS record is defined by how many strings were present when Add() was called.

  @param[in]  SmbiosHandle        SMBIOS Handle of structure that will have its string updated.
  @param[in]  StringNumber        The non-zero string number of the string to update.
  @param[in]  String              Update the StringNumber string with Unicode String.

  @retval EFI_SUCCESS             SmbiosHandle had its StringNumber String updated.
  @retval EFI_INVALID_PARAMETER   SmbiosHandle does not exist. Or String is invalid.
  @retval EFI_UNSUPPORTED         String was not added because it is longer than the SMBIOS Table supports.
  @retval EFI_NOT_FOUND           The StringNumber.is not valid for this SMBIOS record.

**/
EFI_STATUS
EFIAPI
SmbiosLibUpdateUnicodeString (
  IN EFI_SMBIOS_HANDLE      SmbiosHandle,
  IN SMBIOS_TABLE_STRING    StringNumber,
  IN CHAR16                 *String
  )
{
  EFI_STATUS  Status;
  UINTN       StringIndex;
  CHAR8       *Ascii;

  if (String == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (*String == '\0') {
    // A string with no data is not legal in SMBIOS
    return EFI_INVALID_PARAMETER;
  }

  Ascii = AllocateZeroPool (StrSize (String));
  if (Ascii == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  UnicodeStrToAsciiStrS (String, Ascii, StrSize (String));

  StringIndex = StringNumber;
  Status      = mSmbios->UpdateString (mSmbios, &SmbiosHandle, &StringIndex, Ascii);

  FreePool (Ascii);
  return Status;
}

/**
  Allow caller to read a specific SMBIOS string.

  The returned string is a Null-terminated ASCII string.
  If the string pointer returned is non-NULL, then the caller must free the
  memory associated with this string.

  @param[in]    Header          SMBIOS record that contains the string.
  @param[in[    StringNumber    Instance of SMBIOS string 1 - N.

  @retval NULL                  Instance of Type SMBIOS string was not found.
  @retval Other                 Pointer to matching SMBIOS Ascii String.

**/
CHAR8 *
EFIAPI
SmbiosLibReadString (
  IN SMBIOS_STRUCTURE       *Header,
  IN EFI_SMBIOS_STRING      StringNumber
  )
{
  CHAR8  *Data;
  CHAR8  *Str;
  UINTN  Match;

  if (Header == NULL) {
    return NULL;
  }

  Data = (CHAR8 *) Header + Header->Length;
  for (Match = 1; !(*Data == 0 && *(Data+1) == 0); ) {
    if (StringNumber == Match) {
      Str = AllocateZeroPool (AsciiStrSize (Data));
      if (Str) {
        AsciiStrCpyS (Str, AsciiStrSize (Data), Data);
      }
      return Str;
    }

    Data++;
    if (*(Data - 1) == '\0') {
      Match++;
    }
  }

  return NULL;
}

/**
  Allow caller to read a specific SMBIOS string.

  The returned string is a Null-terminated Unicode string.
  If the string pointer returned is non-NULL, then the caller must free the
  memory associated with this string.

  @param[in]    Header          SMBIOS record that contains the string.
  @param[in[    StringNumber    Instance of SMBIOS string 1 - N.

  @retval NULL                  Instance of Type SMBIOS string was not found.
  @retval Other                 Pointer to matching SMBIOS Unicode String.

**/
CHAR16 *
EFIAPI
SmbiosLibReadUnicodeString (
  IN SMBIOS_STRUCTURE       *Header,
  IN EFI_SMBIOS_STRING      StringNumber
  )
{
  CHAR8  *Data;
  CHAR16 *Str;
  UINTN  Match;

  if (Header == NULL) {
    return NULL;
  }

  Data = (CHAR8 *) Header + Header->Length;
  for (Match = 1; !(*Data == 0 && *(Data+1) == 0); ) {
    if (StringNumber == Match) {
      Str = AllocateZeroPool (AsciiStrSize (Data) * sizeof (CHAR16));
      if (Str) {
        AsciiStrToUnicodeStrS (Data, Str, AsciiStrSize (Data) * sizeof (CHAR16));
      }
      return Str;
    }

    Data++;
    if (*(Data - 1) == '\0') {
      Match++;
    }
  }

  return NULL;
}

/**
  Allow the caller to discover a specific SMBIOS entry by Type, and patch it if necissary.

  @param[in]  Type              Type of the next SMBIOS record to return.
  @param[in]  Instance          Instance of SMBIOS record 0 - N-1.
  @param[out] SmbiosHandle      Optional returns SMBIOS handle for the matching record.

  @retval NULL                  Instance of Type SMBIOS record was not found.
  @retval Other                 Pointer to matching SMBIOS record.

**/
SMBIOS_STRUCTURE *
EFIAPI
SmbiosLibGetRecord (
  IN  EFI_SMBIOS_TYPE       Type,
  IN  UINTN                 Instance,
  OUT EFI_SMBIOS_HANDLE     *SmbiosHandle OPTIONAL
  )
{
  EFI_STATUS                Status;
  EFI_SMBIOS_HANDLE         Handle;
  EFI_SMBIOS_TABLE_HEADER   *Record;
  UINTN                     Match;

  Match  = 0;
  Handle = SMBIOS_HANDLE_PI_RESERVED;
  do {
    Status = mSmbios->GetNext (mSmbios, &Handle, &Type, &Record, NULL);
    if (!EFI_ERROR (Status)) {
      if (Instance == Match) {
        if (SmbiosHandle != NULL) {
          *SmbiosHandle = Handle;
        }

        return (SMBIOS_STRUCTURE *) Record;
      }

      Match++;
    }
  } while (!EFI_ERROR (Status));

  return NULL;
}

/**
  Allow the caller to discover a specific SMBIOS entry by Handle, and patch it if necissary.

  @param[in]  SmbiosHandle      SMBIOS handle for the matching record.

  @retval NULL                  Instance of Type SMBIOS record was not found.
  @retval Other                 Pointer to matching SMBIOS record.

**/
SMBIOS_STRUCTURE *
EFIAPI
SmbiosLibGetRecordByHandle (
  IN EFI_SMBIOS_HANDLE      SmbiosHandle
  )
{
  EFI_STATUS                Status;
  EFI_SMBIOS_HANDLE         Handle;
  EFI_SMBIOS_TABLE_HEADER   *Record;

  Handle = SMBIOS_HANDLE_PI_RESERVED;
  do {
    Status = mSmbios->GetNext (mSmbios, &Handle, NULL, &Record, NULL);
    if (!EFI_ERROR (Status)) {
      if (Handle == SmbiosHandle) {
        return (SMBIOS_STRUCTURE *) Record;
      }
    }
  } while (!EFI_ERROR (Status));

  return NULL;
}

/**
  Remove SMBIOS record by Handle.

  This function removes an SMBIOS record using the handle specified by SmbiosHandle.

  @param[in]  SmbiosHandle          The handle of the SMBIOS record to remove.

  @retval EFI_SUCCESS               SMBIOS record was removed.
  @retval EFI_INVALID_PARAMETER     SmbiosHandle does not specify a valid SMBIOS record.

**/
EFI_STATUS
EFIAPI
SmbiosLibRemove (
  IN EFI_SMBIOS_HANDLE      SmbiosHandle
  )
{
  return mSmbios->Remove (mSmbios, SmbiosHandle);
}

/**
  Remove all instances of SMBIOS record by Type.

  This function removes an SMBIOS record using the handle specified by SmbiosHandle.

  @param[in]  SmbiosType            The type of the SMBIOS record to remove.

  @retval EFI_SUCCESS               SMBIOS record was removed.

**/
EFI_STATUS
EFIAPI
SmbiosLibRemoveByType (
  IN EFI_SMBIOS_TYPE        SmbiosType
  )
{
  EFI_STATUS                Status;
  EFI_SMBIOS_TYPE           Type;
  EFI_SMBIOS_HANDLE         Handle;
  EFI_SMBIOS_TABLE_HEADER   *Record;

  Type   = SmbiosType;
  Handle = SMBIOS_HANDLE_PI_RESERVED;

  do {
    Status = mSmbios->GetNext (mSmbios, &Handle, &Type, &Record, NULL);
    if (EFI_ERROR (Status) || (Handle == SMBIOS_HANDLE_PI_RESERVED)) {
      break;
    }

    mSmbios->Remove (mSmbios, Handle);
  } while (!EFI_ERROR (Status));

  return EFI_SUCCESS;
}

/**

  @param[in]  ImageHandle  ImageHandle of the loaded driver.
  @param[in]  SystemTable  Pointer to the EFI System Table.

  @retval  EFI_SUCCESS            Register successfully.
  @retval  EFI_OUT_OF_RESOURCES   No enough memory to register this handler.
**/
EFI_STATUS
EFIAPI
SmbiosLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **) &mSmbios);
}
