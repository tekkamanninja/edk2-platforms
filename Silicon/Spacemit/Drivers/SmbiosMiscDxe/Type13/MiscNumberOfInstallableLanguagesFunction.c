/** @file

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>

#include "SmbiosMisc.h"

typedef struct {
  SMBIOS_TYPE13_BIOS_LANGUAGE_INFORMATION_STRING  LangString;
  BOOLEAN                                         LangInstalled;
} SMBIOS_LANG_INFO;

STATIC
SMBIOS_LANG_INFO mSmbiosLangInfo[] = {
  { {"en-US",  STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_ENG_LONG),      STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_ENG_ABBREVIATE)      }, FALSE },
  { {"zh-chs", STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_SIMPLECH_LONG), STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_SIMPLECH_ABBREVIATE) }, FALSE },
  { {"zh-cht", STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_CHN_LONG),      STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_CHN_ABBREVIATE)      }, FALSE },
  { {"fr-FR",  STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_FRA_LONG),      STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_FRA_ABBREVIATE)      }, FALSE },
  { {"ja-JP",  STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_JPN_LONG),      STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_JPN_ABBREVIATE)      }, FALSE },
  { {"it-IT",  STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_ITA_LONG),      STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_ITA_ABBREVIATE)      }, FALSE },
  { {"es-ES",  STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_SPA_LONG),      STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_SPA_ABBREVIATE)      }, FALSE },
  { {"de-DE",  STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_GER_LONG),      STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_GER_ABBREVIATE)      }, FALSE },
  { {"pt-BR",  STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_POR_LONG),      STRING_TOKEN (STR_MISC_BIOS_LANGUAGES_POR_ABBREVIATE)      }, FALSE }
};

/**
  This function makes boot time changes to the contents of the
  MiscNumberOfInstallableLanguages (Type 13) record.

  @param  RecordData                 Pointer to SMBIOS table with default values.
  @param  Smbios                     SMBIOS protocol.

  @retval EFI_SUCCESS                The SMBIOS table was successfully added.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.
  @retval EFI_OUT_OF_RESOURCES       Failed to allocate required memory.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscNumberOfInstallableLanguages) {
  EFI_STATUS            Status;
  SMBIOS_TABLE_TYPE13   *InputData;
  SMBIOS_TABLE_TYPE13   *SmbiosRecord;
  CHAR8                 *LanguageString;
  CHAR8                 *Languages;
  CHAR8                 *LangStr;
  UINTN                 LangNum;
  UINTN                 CurLang;
  CHAR8                 *OptionalStrStart;
  UINTN                 OptionStrSize;
  UINTN                 BufferSize;
  UINTN                 LangIdx, StringIdx;
  EFI_STRING_ID         TokenToGet;
  CHAR16                *Char16String;

  InputData      = NULL;
  SmbiosRecord   = NULL;
  LanguageString = NULL;
  Languages      = NULL;
  LangStr        = NULL;
  LangNum        = 0;
  CurLang        = 0;
  OptionStrSize  = 0;

  //
  // First check for invalid parameters.
  //
  if (RecordData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  InputData = (SMBIOS_TABLE_TYPE13 *) RecordData;

  Status = GetEfiGlobalVariable2 (L"PlatformLangCodes", (VOID **) &LanguageString, NULL);
  if (EFI_ERROR (Status)) {
    BufferSize = AsciiStrSize ((CHAR8 *) PcdGetPtr (PcdUefiVariableDefaultPlatformLangCodes));
    LanguageString = AllocateCopyPool (BufferSize, (CHAR8 *) PcdGetPtr (PcdUefiVariableDefaultPlatformLangCodes));
    if (LanguageString == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto _Exit;
    }
  }

  Status = GetEfiGlobalVariable2 (L"PlatformLang", (VOID **) &Languages, NULL);
  if (EFI_ERROR (Status)) {
    BufferSize = AsciiStrSize ((CHAR8 *) PcdGetPtr (PcdUefiVariableDefaultPlatformLang));
    Languages = AllocateCopyPool (BufferSize, (CHAR8 *) PcdGetPtr (PcdUefiVariableDefaultPlatformLang));
    if (Languages == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto _Exit;
    }
  }

  LangStr    = LanguageString;
  BufferSize = AsciiStrSize (LanguageString);

  for (LangIdx = 0; LangIdx < BufferSize; LangIdx++) {
    if ((LanguageString[LangIdx] == ';') || (LanguageString[LangIdx] == '\0')) {
      LanguageString[LangIdx] = '\0';

      for (StringIdx = 0; StringIdx < ARRAY_SIZE (mSmbiosLangInfo); StringIdx++) {
        if (!AsciiStrCmp (LangStr, mSmbiosLangInfo[StringIdx].LangString.LanguageSignature)) {
          mSmbiosLangInfo[StringIdx].LangInstalled = TRUE;
          if (InputData->Flags & BIT0) {
            TokenToGet = mSmbiosLangInfo[StringIdx].LangString.InstallableLanguageAbbreviateString;
          } else {
            TokenToGet = mSmbiosLangInfo[StringIdx].LangString.InstallableLanguageLongString;
          }
          Char16String = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
          OptionStrSize += StrLen (Char16String) + 1;
          LangNum++;
          if (!AsciiStrCmp (LangStr, Languages)) {
            CurLang = LangNum;
          }
          break;
        }
      }

      LangStr = &LanguageString[LangIdx + 1];
    }
  }

  if ((LangNum == 0) || (CurLang == 0)) {
    Status = EFI_NOT_FOUND;
    goto _Exit;
  }

  SmbiosRecord = AllocateZeroPool (sizeof (SMBIOS_TABLE_TYPE13) + OptionStrSize + 1);
  if (SmbiosRecord == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto _Exit;
  }

  (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE13));

  SmbiosRecord->Hdr.Length           = sizeof (SMBIOS_TABLE_TYPE13);
  SmbiosRecord->InstallableLanguages = LangNum;
  SmbiosRecord->CurrentLanguages     = CurLang;

  OptionalStrStart = (CHAR8 *) (SmbiosRecord + 1);

  for (StringIdx = 0; StringIdx < ARRAY_SIZE (mSmbiosLangInfo); StringIdx++) {
    if (mSmbiosLangInfo[StringIdx].LangInstalled == TRUE) {
      if (SmbiosRecord->Flags & BIT0) {
        TokenToGet = mSmbiosLangInfo[StringIdx].LangString.InstallableLanguageAbbreviateString;
      } else {
        TokenToGet = mSmbiosLangInfo[StringIdx].LangString.InstallableLanguageLongString;
      }
      Char16String = HiiGetPackageString (&gEfiCallerIdGuid, TokenToGet, NULL);
      UnicodeStrToAsciiStrS (Char16String, OptionalStrStart, StrLen (Char16String) + 1);
      OptionalStrStart += StrLen (Char16String) + 1;
    }
  }

  //
  // Now we have got the full smbios record, call smbios protocol to add this record.
  //
  Status = SmbiosMiscAddRecord ((UINT8 *) SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[%a]:[%dL] Smbios Type13 Table Log Failed! %r \n",
      __func__,
      DEBUG_LINE_NUMBER,
      Status
      ));
  }

_Exit:
  if (SmbiosRecord != NULL) {
    FreePool (SmbiosRecord);
  }

  if (LanguageString != NULL) {
    FreePool (LanguageString);
  }

  if (Languages != NULL) {
    FreePool (Languages);
  }

  return Status;
}
