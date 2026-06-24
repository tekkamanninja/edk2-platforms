/** @file

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.
  Copyright (c) 2022 - 2023, Ampere Computing LLC. All rights reserved.<BR>
  Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>
  Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
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

typedef struct {
  CONST CHAR8    *MonthStr;
  UINT32         MonthInt;
} MONTH_DESCRIPTION;

STATIC CONST
MONTH_DESCRIPTION  mMonthDescription[] = {
  { "Jan", 1  },
  { "Feb", 2  },
  { "Mar", 3  },
  { "Apr", 4  },
  { "May", 5  },
  { "Jun", 6  },
  { "Jul", 7  },
  { "Aug", 8  },
  { "Sep", 9  },
  { "Oct", 10 },
  { "Nov", 11 },
  { "Dec", 12 },
  { "???", 1  }, // Use 1 as default month
};

/**
  Field Filling Function. Transform an EFI_EXP_BASE2_DATA to a byte, with '64k'
  as the unit.

  @param  Value              Pointer to Base2_Data

  @retval

**/
UINT8
Base2ToByteWith64KUnit (
  IN  UINTN  Value
  )
{
  UINT8  Size;

  Size = ((Value + (SIZE_64KB - 1)) >> 16);

  return Size;
}

/**
  Returns the date and time this file (and firmware) was built.

  @param[out] *Time Pointer to the EFI_TIME structure to fill in.
**/
VOID
GetReleaseTime (
  OUT EFI_TIME  *Time
  )
{
  CONST CHAR8  *ReleaseDate = __DATE__;
  CONST CHAR8  *ReleaseTime = __TIME__;
  UINTN        i;

  for (i = 0; i < 12; i++) {
    if (AsciiStrnCmp (ReleaseDate, mMonthDescription[i].MonthStr, 3) == 0) {
      break;
    }
  }

  Time->Month  = mMonthDescription[i].MonthInt;
  Time->Day    = AsciiStrDecimalToUintn (ReleaseDate + 4);
  Time->Year   = AsciiStrDecimalToUintn (ReleaseDate + 7);
  Time->Hour   = AsciiStrDecimalToUintn (ReleaseTime);
  Time->Minute = AsciiStrDecimalToUintn (ReleaseTime + 3);
  Time->Second = AsciiStrDecimalToUintn (ReleaseTime + 6);
}

/**
  Fetches the firmware ('BIOS') release date from the
  FirmwareVersionInfo HOB.

  @return The release date as a UTF-16 string
**/
CHAR16 *
GetBiosReleaseDate (
  VOID
  )
{
  CHAR16    *ReleaseDate;
  EFI_TIME  BuildTime;

  ReleaseDate = AllocateZeroPool ((sizeof (CHAR16)) * SMBIOS_STRING_MAX_LENGTH);
  if (ReleaseDate == NULL) {
    return NULL;
  }

  GetReleaseTime (&BuildTime);

  (VOID)UnicodeSPrintAsciiFormat (
          ReleaseDate,
          (sizeof (CHAR16)) * SMBIOS_STRING_MAX_LENGTH,
          "%02d/%02d/%4d",
          BuildTime.Month,
          BuildTime.Day,
          BuildTime.Year
          );

  return ReleaseDate;
}

/**
  This function makes boot time changes to the contents of the
  MiscBiosVendor (Type 0) record.

  @param  RecordData                 Pointer to SMBIOS table with default values.
  @param  Smbios                     SMBIOS protocol.

  @retval EFI_SUCCESS                The SMBIOS table was successfully added.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.
  @retval EFI_OUT_OF_RESOURCES       Failed to allocate required memory.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscBiosVendor) {
  EFI_STATUS          Status;
  CHAR8               *OptionalStrStart;
  UINT64              BiosPhysicalSize;
  UINTN               RecordLength;
  CHAR16              *pVendor;
  CHAR16              *pVersion;
  CHAR16              *pReleaseDate;
  UINTN               VendorStrLen;
  UINTN               VerStrLen;
  UINTN               DateStrLen;
  EFI_STRING          Vendor;
  EFI_STRING          Version;
  EFI_STRING          ReleaseDate;
  EFI_STRING_ID       TokenToUpdate;
  EFI_STRING_ID       TokenToGet;
  SMBIOS_TABLE_TYPE0  *SmbiosRecord;
  SMBIOS_TABLE_TYPE0  *InputData;

  Status       = EFI_SUCCESS;
  Vendor       = NULL;
  Version      = NULL;
  ReleaseDate  = NULL;
  SmbiosRecord = NULL;

  //
  // First check for invalid parameters.
  //
  if (RecordData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  InputData = (SMBIOS_TABLE_TYPE0 *) RecordData;

  pVendor = (CHAR16 *) PcdGetPtr (PcdFirmwareVendor);
  if (StrLen (pVendor) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BIOS_VENDOR);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pVendor, NULL);
  }

  pVersion = (CHAR16 *) PcdGetPtr (PcdFirmwareVersionString);
  if (StrLen (pVersion) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BIOS_VERSION);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pVersion, NULL);
  }

  pReleaseDate = (CHAR16 *) PcdGetPtr (PcdFirmwareReleaseDateString);
  if (StrLen (pReleaseDate) > 0) {
    TokenToUpdate = STRING_TOKEN (STR_MISC_BIOS_RELEASE_DATE);
    HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pReleaseDate, NULL);
  } else {
    pReleaseDate = GetBiosReleaseDate ();
    if (pReleaseDate != NULL) {
      if (StrLen (pReleaseDate) > 0) {
        TokenToUpdate = STRING_TOKEN (STR_MISC_BIOS_RELEASE_DATE);
        HiiSetString (mSmbiosMiscHiiHandle, TokenToUpdate, pReleaseDate, NULL);
      }
      FreePool (pReleaseDate);
    }
  }

  TokenToGet   = STRING_TOKEN (STR_MISC_BIOS_VENDOR);
  Vendor       = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  VendorStrLen = StrLen (Vendor);

  TokenToGet   = STRING_TOKEN (STR_MISC_BIOS_VERSION);
  Version      = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  VerStrLen    = StrLen (Version);

  TokenToGet   = STRING_TOKEN (STR_MISC_BIOS_RELEASE_DATE);
  ReleaseDate  = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
  DateStrLen   = StrLen (ReleaseDate);

  //
  // Now update the BiosPhysicalSize
  //
  BiosPhysicalSize = FixedPcdGet32 (PcdFdSize);

  //
  // Two zeros following the last string.
  //
  RecordLength = sizeof (SMBIOS_TABLE_TYPE0) +
                 VendorStrLen + 1 +
                 VerStrLen + 1 +
                 DateStrLen + 1 + 1;
  SmbiosRecord = AllocateZeroPool (RecordLength);
  if (SmbiosRecord == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE0));

  SmbiosRecord->Hdr.Length  = sizeof (SMBIOS_TABLE_TYPE0);
  SmbiosRecord->BiosSegment = 0;
  if (BiosPhysicalSize < SIZE_16MB) {
    SmbiosRecord->BiosSize = Base2ToByteWith64KUnit (BiosPhysicalSize) - 1;
  } else {
    SmbiosRecord->BiosSize = 0xFF;
    if (BiosPhysicalSize < SIZE_16GB) {
      SmbiosRecord->ExtendedBiosSize.Size = BiosPhysicalSize / SIZE_1MB;
      SmbiosRecord->ExtendedBiosSize.Unit = 0; // Size is in MB
    } else {
      SmbiosRecord->ExtendedBiosSize.Size = BiosPhysicalSize / SIZE_1GB;
      SmbiosRecord->ExtendedBiosSize.Unit = 1; // Size is in GB
    }
  }

  SmbiosRecord->SystemBiosMajorRelease = (UINT8) ((PcdGet32 (PcdFirmwareRevision) >> 16) & 0xFF);
  SmbiosRecord->SystemBiosMinorRelease = (UINT8) (PcdGet32 (PcdFirmwareRevision) & 0xFF);

  SmbiosRecord->EmbeddedControllerFirmwareMajorRelease = (UINT8) ((PcdGet32 (PcdEcFirmwareRevision) >> 16) & 0xFF);
  SmbiosRecord->EmbeddedControllerFirmwareMinorRelease = (UINT8) (PcdGet32 (PcdEcFirmwareRevision) & 0xFF);

  OptionalStrStart = (CHAR8 *) (SmbiosRecord + 1);
  UnicodeStrToAsciiStrS (Vendor, OptionalStrStart, VendorStrLen + 1);
  OptionalStrStart += VendorStrLen + 1;
  UnicodeStrToAsciiStrS (Version, OptionalStrStart, VerStrLen + 1);
  OptionalStrStart += VerStrLen + 1;
  UnicodeStrToAsciiStrS (ReleaseDate, OptionalStrStart, DateStrLen + 1);
  //
  // Now we have got the full smbios record, call smbios protocol to add this record.
  //
  Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[%a]:[%dL] Smbios Type00 Table Log Failed! %r \n",
      __func__,
      DEBUG_LINE_NUMBER,
      Status
      ));
  }

Exit:
  if (Vendor != NULL) {
    FreePool (Vendor);
  }

  if (Version != NULL) {
    FreePool (Version);
  }

  if (ReleaseDate != NULL) {
    FreePool (ReleaseDate);
  }

  if (SmbiosRecord != NULL) {
    FreePool (SmbiosRecord);
  }

  return Status;
}
