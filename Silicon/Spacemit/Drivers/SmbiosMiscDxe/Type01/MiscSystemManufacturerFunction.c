/** @file
  This driver parses the mMiscSubclassDataTable structure and reports
  any generated data to smbios.

  Based on files under Nt32Pkg/MiscSubClassPlatformDxe/

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.
  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>
  Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>
  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2015, Hisilicon Limited. All rights reserved.<BR>
  Copyright (c) 2015, Linaro Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "SmbiosMisc.h"

/**
  This function makes boot time changes to the contents of the
  MiscSystemManufacturer (Type 1) record.

  @param  RecordData                 Pointer to SMBIOS table with default values.
  @param  Smbios                     SMBIOS protocol.

  @retval EFI_SUCCESS                The SMBIOS table was successfully added.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.
  @retval EFI_OUT_OF_RESOURCES       Failed to allocate required memory.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscSystemManufacturer) {
  EFI_STATUS          Status;
  CHAR8               *OptionalStrStart;
  CHAR16              *pManufacturer;
  CHAR16              *pProductName;
  CHAR16              *pVersion;
  CHAR16              *pSerialNumber;
  CHAR16              *pSKUNumber;
  CHAR16              *pFamily;
  UINTN               ManuStrLen;
  UINTN               VerStrLen;
  UINTN               PdNameStrLen;
  UINTN               SerialNumStrLen;
  UINTN               SKUNumStrLen;
  UINTN               FamilyStrLen;
  UINTN               RecordLength;
  EFI_STRING          Manufacturer;
  EFI_STRING          ProductName;
  EFI_STRING          Version;
  EFI_STRING          SerialNumber;
  EFI_STRING          SKUNumber;
  EFI_STRING          Family;
  EFI_STRING_ID       TokenToGet;
  SMBIOS_TABLE_TYPE1  *SmbiosRecord;
  SMBIOS_TABLE_TYPE1  *InputData;
  EFI_STRING_ID       TokenToUpdate;

  Status       = EFI_SUCCESS;
  Manufacturer = NULL;
  ProductName  = NULL;
  Version      = NULL;
  SerialNumber = NULL;
  SKUNumber    = NULL;
  Family       = NULL;
  SmbiosRecord = NULL;

  //
  // First check for invalid parameters.
  //
  if (RecordData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  InputData = (SMBIOS_TABLE_TYPE1 *) RecordData;

  pManufacturer = (CHAR16 *) PcdGetPtr (PcdSmbiosSystemManufacturer);
  if (StrLen (pManufacturer) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_SYSTEM_MANUFACTURER);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pManufacturer, NULL);
  }

  pProductName = (CHAR16 *) PcdGetPtr (PcdSmbiosSystemProductName);
  if (StrLen (pProductName) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_SYSTEM_PRODUCT_NAME);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pProductName, NULL);
  }

  pVersion = (CHAR16 *) PcdGetPtr (PcdSmbiosSystemVersion);
  if (StrLen (pVersion) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_SYSTEM_VERSION);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pVersion, NULL);
  }

  pSerialNumber = (CHAR16 *) PcdGetPtr (PcdSmbiosSystemSerialNumber);
  if (StrLen (pSerialNumber) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_SYSTEM_SERIAL_NUMBER);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pSerialNumber, NULL);
  }

  pSKUNumber = (CHAR16 *) PcdGetPtr (PcdSmbiosSystemSKU);
  if (StrLen (pSKUNumber) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_SYSTEM_SKU_NUMBER);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pSKUNumber, NULL);
  }

  pFamily = (CHAR16 *) PcdGetPtr (PcdSmbiosSystemFamily);
  if (StrLen (pFamily) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_SYSTEM_FAMILY);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pFamily, NULL);
  }

  TokenToGet      = STRING_TOKEN (STR_MISC_SYSTEM_MANUFACTURER);
  Manufacturer    = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  ManuStrLen      = StrLen (Manufacturer);

  TokenToGet      = STRING_TOKEN (STR_MISC_SYSTEM_PRODUCT_NAME);
  ProductName     = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  PdNameStrLen    = StrLen (ProductName);

  TokenToGet      = STRING_TOKEN (STR_MISC_SYSTEM_VERSION);
  Version         = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  VerStrLen       = StrLen (Version);

  TokenToGet      = STRING_TOKEN (STR_MISC_SYSTEM_SERIAL_NUMBER);
  SerialNumber    = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  SerialNumStrLen = StrLen (SerialNumber);

  TokenToGet      = STRING_TOKEN (STR_MISC_SYSTEM_SKU_NUMBER);
  SKUNumber       = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  SKUNumStrLen    = StrLen (SKUNumber);

  TokenToGet      = STRING_TOKEN (STR_MISC_SYSTEM_FAMILY);
  Family          = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  FamilyStrLen    = StrLen (Family);

  //
  // Two zeros following the last string.
  //
  RecordLength = sizeof (SMBIOS_TABLE_TYPE1) +
                 ManuStrLen      + 1 +
                 PdNameStrLen    + 1 +
                 VerStrLen       + 1 +
                 SerialNumStrLen + 1 +
                 SKUNumStrLen    + 1 +
                 FamilyStrLen    + 1 + 1;
  SmbiosRecord = AllocateZeroPool (RecordLength);
  if (SmbiosRecord == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE1));

  SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE1);

  OptionalStrStart = (CHAR8 *)(SmbiosRecord + 1);
  UnicodeStrToAsciiStrS (Manufacturer, OptionalStrStart, ManuStrLen + 1);
  OptionalStrStart += ManuStrLen + 1;
  UnicodeStrToAsciiStrS (ProductName, OptionalStrStart, PdNameStrLen + 1);
  OptionalStrStart += PdNameStrLen + 1;
  UnicodeStrToAsciiStrS (Version, OptionalStrStart, VerStrLen + 1);
  OptionalStrStart += VerStrLen + 1;
  UnicodeStrToAsciiStrS (SerialNumber, OptionalStrStart, SerialNumStrLen + 1);
  OptionalStrStart += SerialNumStrLen + 1;
  UnicodeStrToAsciiStrS (SKUNumber, OptionalStrStart, SKUNumStrLen + 1);
  OptionalStrStart += SKUNumStrLen + 1;
  UnicodeStrToAsciiStrS (Family, OptionalStrStart, FamilyStrLen + 1);

  //
  // Now we have got the full smbios record, call smbios protocol to add this record.
  //
  Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[%a]:[%dL] Smbios Type01 Table Log Failed! %r \n",
      __func__,
      DEBUG_LINE_NUMBER,
      Status
      ));
  }

Exit:
  if (Manufacturer != NULL) {
    FreePool (Manufacturer);
  }

  if (ProductName != NULL) {
    FreePool (ProductName);
  }

  if (Version != NULL) {
    FreePool (Version);
  }

  if (SerialNumber != NULL) {
    FreePool (SerialNumber);
  }

  if (SKUNumber != NULL) {
    FreePool (SKUNumber);
  }

  if (Family != NULL) {
    FreePool (Family);
  }

  if (SmbiosRecord != NULL) {
    FreePool (SmbiosRecord);
  }

  return Status;
}
