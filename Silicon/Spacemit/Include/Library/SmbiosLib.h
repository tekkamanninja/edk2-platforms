/** @file

  Provides library functions for common SMBIOS operations. Only available to DXE
  and UEFI module types.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.<BR>
  Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.<BR>
  Copyright (c) 2012, Apple Inc. All rights reserved. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SMBIOS_LIB_H__
#define __SMBIOS_LIB_H__

#include <IndustryStandard/SmBios.h>
#include <Protocol/Smbios.h>

///
/// Cache copy of the SMBIOS Protocol pointer
///
extern EFI_SMBIOS_PROTOCOL  *mSmbios;

///
/// Template for SMBIOS table initialization.
/// The SMBIOS_TABLE_STRING types in the formated area must match the
/// StringArray sequene.
///
typedef struct {
  //
  // formatted area of a given SMBIOS record
  //
  SMBIOS_STRUCTURE    *Entry;
  //
  // NULL terminated array of ASCII strings to be added to the SMBIOS record.
  //
  CHAR8               **StringArray;
} SMBIOS_TEMPLATE_ENTRY;

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

#endif
