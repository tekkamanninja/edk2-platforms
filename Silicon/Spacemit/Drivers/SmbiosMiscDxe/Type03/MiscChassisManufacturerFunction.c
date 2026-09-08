/** @file
  This driver parses the mMiscSubclassDataTable structure and reports
  any generated data to smbios.

  Based on files under Nt32Pkg/MiscSubClassPlatformDxe/

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.
  Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>
  Copyright (c) 2009 - 2011, Intel Corporation. All rights reserved.<BR>
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
  MiscChassisManufacturer (Type 3) record.

  @param  RecordData                 Pointer to SMBIOS table with default values.
  @param  Smbios                     SMBIOS protocol.

  @retval EFI_SUCCESS                The SMBIOS table was successfully added.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.
  @retval EFI_OUT_OF_RESOURCES       Failed to allocate required memory.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscChassisManufacturer) {
  EFI_STATUS          Status;
  CHAR8               *OptionalStrStart;
  UINT8               *SkuNumberField;
  CHAR16              *pManufacturer;
  CHAR16              *pVersion;
  CHAR16              *pSerialNumber;
  CHAR16              *pAssertTag;
  CHAR16              *pChassisSkuNumber;
  UINTN               RecordLength;
  UINTN               ManuStrLen;
  UINTN               VerStrLen;
  UINTN               AssertTagStrLen;
  UINTN               SerialNumStrLen;
  UINTN               ChaNumStrLen;
  EFI_STRING          Manufacturer;
  EFI_STRING          Version;
  EFI_STRING          SerialNumber;
  EFI_STRING          AssertTag;
  EFI_STRING          ChassisSkuNumber;
  EFI_STRING_ID       TokenToGet;
  EFI_STRING_ID       TokenToUpdate;
  SMBIOS_TABLE_TYPE3  *SmbiosRecord;
  SMBIOS_TABLE_TYPE3  *InputData;

  Manufacturer     = NULL;
  Version          = NULL;
  SerialNumber     = NULL;
  AssertTag        = NULL;
  ChassisSkuNumber = NULL;
  SmbiosRecord     = NULL;

  //
  // First check for invalid parameters.
  //
  if (RecordData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  InputData = (SMBIOS_TABLE_TYPE3 *)RecordData;

  pManufacturer = (CHAR16 *) PcdGetPtr (PcdSmbiosClassisManufacturer);
  if (StrLen (pManufacturer) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_CHASSIS_MANUFACTURER);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pManufacturer, NULL);
  }

  pVersion = (CHAR16 *) PcdGetPtr (PcdSmbiosClassisVersion);
  if (StrLen (pVersion) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_CHASSIS_VERSION);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pVersion, NULL);
  }

  pSerialNumber = (CHAR16 *) PcdGetPtr (PcdSmbiosClassisSerialNumber);
  if (StrLen (pSerialNumber) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_CHASSIS_SERIAL_NUMBER);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pSerialNumber, NULL);
  }

  pAssertTag = (CHAR16 *) PcdGetPtr (PcdSmbiosClassisAssertTag);
  if (StrLen (pAssertTag) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_CHASSIS_ASSET_TAG);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pAssertTag, NULL);
  }

  pChassisSkuNumber = (CHAR16 *) PcdGetPtr (PcdSmbiosClassisSKU);
  if (StrLen (pChassisSkuNumber) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_CHASSIS_SKU_NUMBER);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pChassisSkuNumber, NULL);
  }

  TokenToGet       = STRING_TOKEN (STR_MISC_CHASSIS_MANUFACTURER);
  Manufacturer     = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  ManuStrLen       = StrLen (Manufacturer);

  TokenToGet       = STRING_TOKEN (STR_MISC_CHASSIS_VERSION);
  Version          = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  VerStrLen        = StrLen (Version);

  TokenToGet       = STRING_TOKEN (STR_MISC_CHASSIS_SERIAL_NUMBER);
  SerialNumber     = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  SerialNumStrLen  = StrLen (SerialNumber);

  TokenToGet       = STRING_TOKEN (STR_MISC_CHASSIS_ASSET_TAG);
  AssertTag        = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  AssertTagStrLen  = StrLen (AssertTag);

  TokenToGet       = STRING_TOKEN (STR_MISC_CHASSIS_SKU_NUMBER);
  ChassisSkuNumber = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  ChaNumStrLen     = StrLen (ChassisSkuNumber);

  //
  // Two zeros following the last string.
  //
  RecordLength = sizeof (SMBIOS_TABLE_TYPE3) + 1 +
                 ManuStrLen      + 1 +
                 VerStrLen       + 1 +
                 SerialNumStrLen + 1 +
                 AssertTagStrLen + 1 +
                 ChaNumStrLen    + 1 + 1;
  SmbiosRecord = AllocateZeroPool (RecordLength);
  if (SmbiosRecord == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE3));

  SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE3) + 1;
  SmbiosRecord->Type       = PcdGet8 (PcdSmbiosClassisType);
  SmbiosRecord->Height     = PcdGet8 (PcdSmbiosClassisHeight);

  // ChassisSkuNumber
  SkuNumberField = (UINT8 *)SmbiosRecord + sizeof (SMBIOS_TABLE_TYPE3);

  *SkuNumberField = 5;

  OptionalStrStart = (CHAR8 *)((UINT8 *) SmbiosRecord + sizeof (SMBIOS_TABLE_TYPE3) + 1);
  UnicodeStrToAsciiStrS (Manufacturer, OptionalStrStart, ManuStrLen + 1);
  OptionalStrStart += ManuStrLen + 1;
  UnicodeStrToAsciiStrS (Version, OptionalStrStart, VerStrLen + 1);
  OptionalStrStart += VerStrLen + 1;
  UnicodeStrToAsciiStrS (SerialNumber, OptionalStrStart, SerialNumStrLen + 1);
  OptionalStrStart += SerialNumStrLen + 1;
  UnicodeStrToAsciiStrS (AssertTag, OptionalStrStart, AssertTagStrLen + 1);
  OptionalStrStart += AssertTagStrLen + 1;
  UnicodeStrToAsciiStrS (ChassisSkuNumber, OptionalStrStart, ChaNumStrLen + 1);

  //
  // Now we have got the full smbios record, call smbios protocol to add this record.
  //
  Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[%a]:[%dL] Smbios Type03 Table Log Failed! %r \n",
      __func__,
      DEBUG_LINE_NUMBER,
      Status
      ));
  }

Exit:
  if (Manufacturer != NULL) {
    FreePool (Manufacturer);
  }

  if (Version != NULL) {
    FreePool (Version);
  }

  if (SerialNumber != NULL) {
    FreePool (SerialNumber);
  }

  if (AssertTag != NULL) {
    FreePool (AssertTag);
  }

  if (ChassisSkuNumber != NULL) {
    FreePool (ChassisSkuNumber);
  }

  if (SmbiosRecord != NULL) {
    FreePool (SmbiosRecord);
  }

  return 0;
}
