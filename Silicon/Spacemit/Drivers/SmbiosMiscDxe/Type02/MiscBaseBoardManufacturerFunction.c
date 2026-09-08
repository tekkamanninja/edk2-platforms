/** @file
  This driver parses the mSmbiosMiscDataTable structure and reports
  any generated data using SMBIOS protocol.

  Based on files under Nt32Pkg/MiscSubClassPlatformDxe/

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.
  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>
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
  MiscBaseBoardManufacturer (Type 2) record.

  @param  RecordData                 Pointer to SMBIOS table with default values.
  @param  Smbios                     SMBIOS protocol.

  @retval EFI_SUCCESS                The SMBIOS table was successfully added.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.
  @retval EFI_OUT_OF_RESOURCES       Failed to allocate required memory.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscBaseBoardManufacturer) {
  EFI_STATUS          Status;
  CHAR8               *OptionalStrStart;
  CHAR16              *pBaseBoardManufacturer;
  CHAR16              *pBaseBoardProductName;
  CHAR16              *pVersion;
  CHAR16              *pSerialNumber;
  CHAR16              *pAssetTag;
  CHAR16              *pChassisLocation;
  UINTN               RecordLength;
  UINTN               ManuStrLen;
  UINTN               ProductNameStrLen;
  UINTN               VerStrLen;
  UINTN               SerialNumStrLen;
  UINTN               AssetTagStrLen;
  UINTN               ChassisLocaStrLen;
  UINTN               HandleCount;
  UINT16              *HandleArray;
  EFI_STRING          BaseBoardManufacturer;
  EFI_STRING          BaseBoardProductName;
  EFI_STRING          Version;
  EFI_STRING          SerialNumber;
  EFI_STRING          AssetTag;
  EFI_STRING          ChassisLocation;
  EFI_STRING_ID       TokenToGet;
  SMBIOS_TABLE_TYPE2  *SmbiosRecord;
  SMBIOS_TABLE_TYPE2  *InputData;
  EFI_STRING_ID       TokenToUpdate;

  Status                = EFI_SUCCESS;
  HandleCount           = 0;
  HandleArray           = NULL;
  BaseBoardManufacturer = NULL;
  BaseBoardProductName  = NULL;
  Version               = NULL;
  SerialNumber          = NULL;
  AssetTag              = NULL;
  ChassisLocation       = NULL;
  SmbiosRecord          = NULL;

  //
  // First check for invalid parameters.
  //
  if (RecordData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  InputData = (SMBIOS_TABLE_TYPE2 *) RecordData;

  pBaseBoardManufacturer = (CHAR16 *) PcdGetPtr (PcdSmbiosBaseBoardManufacturer);
  if (StrLen (pBaseBoardManufacturer) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BASE_BOARD_MANUFACTURER);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pBaseBoardManufacturer, NULL);
  }

  pBaseBoardProductName = (CHAR16 *) PcdGetPtr (PcdSmbiosBaseBoardProductName);
  if (StrLen (pBaseBoardProductName) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BASE_BOARD_PRODUCT_NAME);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pBaseBoardProductName, NULL);
  }

  pVersion = (CHAR16 *) PcdGetPtr (PcdSmbiosBaseBoardVersion);
  if (StrLen (pVersion) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BASE_BOARD_VERSION);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pVersion, NULL);
  }

  pSerialNumber = (CHAR16 *) PcdGetPtr (PcdSmbiosBaseBoardSerialNumber);
  if (StrLen (pSerialNumber) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BASE_BOARD_SERIAL_NUMBER);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pSerialNumber, NULL);
  }

  pAssetTag = (CHAR16 *) PcdGetPtr (PcdSmbiosBaseBoardAssertTag);
  if (StrLen (pAssetTag) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BASE_BOARD_ASSET_TAG);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pAssetTag, NULL);
  }

  pChassisLocation = (CHAR16 *) PcdGetPtr (PcdSmbiosBaseBoardChassisLocation);
  if (StrLen (pChassisLocation) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BASE_BOARD_CHASSIS_LOCATION);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pChassisLocation, NULL);
  }

  TokenToGet            = STRING_TOKEN (STR_MISC_BASE_BOARD_MANUFACTURER);
  BaseBoardManufacturer = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  ManuStrLen            = StrLen (BaseBoardManufacturer);

  TokenToGet            = STRING_TOKEN (STR_MISC_BASE_BOARD_PRODUCT_NAME);
  BaseBoardProductName  = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  ProductNameStrLen     = StrLen (BaseBoardProductName);

  TokenToGet            = STRING_TOKEN (STR_MISC_BASE_BOARD_VERSION);
  Version               = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  VerStrLen             = StrLen (Version);

  TokenToGet            = STRING_TOKEN (STR_MISC_BASE_BOARD_SERIAL_NUMBER);
  SerialNumber          = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  SerialNumStrLen       = StrLen (SerialNumber);

  TokenToGet            = STRING_TOKEN (STR_MISC_BASE_BOARD_ASSET_TAG);
  AssetTag              = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  AssetTagStrLen        = StrLen (AssetTag);

  TokenToGet            = STRING_TOKEN (STR_MISC_BASE_BOARD_CHASSIS_LOCATION);
  ChassisLocation       = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  ChassisLocaStrLen     = StrLen (ChassisLocation);

  //
  // Two zeros following the last string.
  //
  RecordLength = sizeof (SMBIOS_TABLE_TYPE2) +
                 ManuStrLen        + 1 +
                 ProductNameStrLen + 1 +
                 VerStrLen         + 1 +
                 SerialNumStrLen   + 1 +
                 AssetTagStrLen    + 1 +
                 ChassisLocaStrLen + 1 + 1;
  SmbiosRecord = AllocateZeroPool (RecordLength);
  if (SmbiosRecord == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE2));
  SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE2);

  //
  // Update Contained objects Handle
  //
  SmbiosRecord->NumberOfContainedObjectHandles = 0;
  SmbiosMiscGetLinkTypeHandle (
    EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE,
    &HandleArray,
    &HandleCount
    );
  // It's assumed there's at most a single chassis
  ASSERT (HandleCount < 2);
  if (HandleCount > 0) {
    SmbiosRecord->ChassisHandle = HandleArray[0];
  }

  if (HandleArray != NULL) {
    FreePool (HandleArray);
  }

  OptionalStrStart = (CHAR8 *)(SmbiosRecord + 1);
  UnicodeStrToAsciiStrS (BaseBoardManufacturer, OptionalStrStart, ManuStrLen + 1);
  OptionalStrStart += ManuStrLen + 1;
  UnicodeStrToAsciiStrS (BaseBoardProductName, OptionalStrStart, ProductNameStrLen + 1);
  OptionalStrStart += ProductNameStrLen + 1;
  UnicodeStrToAsciiStrS (Version, OptionalStrStart, VerStrLen + 1);
  OptionalStrStart += VerStrLen + 1;
  UnicodeStrToAsciiStrS (SerialNumber, OptionalStrStart, SerialNumStrLen + 1);
  OptionalStrStart += SerialNumStrLen + 1;
  UnicodeStrToAsciiStrS (AssetTag, OptionalStrStart, AssetTagStrLen + 1);
  OptionalStrStart += AssetTagStrLen + 1;
  UnicodeStrToAsciiStrS (ChassisLocation, OptionalStrStart, ChassisLocaStrLen + 1);

  Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[%a]:[%dL] Smbios Type02 Table Log Failed! %r \n",
      __func__,
      DEBUG_LINE_NUMBER,
      Status
      ));
  }

Exit:
  if (BaseBoardManufacturer != NULL) {
    FreePool (BaseBoardManufacturer);
  }

  if (BaseBoardProductName != NULL) {
    FreePool (BaseBoardProductName);
  }

  if (Version != NULL) {
    FreePool (Version);
  }

  if (SerialNumber != NULL) {
    FreePool (SerialNumber);
  }

  if (AssetTag != NULL) {
    FreePool (AssetTag);
  }

  if (ChassisLocation != NULL) {
    FreePool (ChassisLocation);
  }

  if (SmbiosRecord != NULL) {
    FreePool (SmbiosRecord);
  }

  return Status;
}
