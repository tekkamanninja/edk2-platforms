/** @file

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Base.h>
#include <Guid/MdeModuleHii.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/HiiLib.h>
#include <Library/CustomizedHiiLib.h>

#define MAX_STRING_LEN  0x100

/**
  This function create a new string in String Package or updates an existing
  string in a String Package.

  @param[in]      HiiHandle           A handle that was previously registered in the
                                      HII Database.
  @param[in]      SupportedLanguages  A pointer to a Null-terminated ASCII string of
                                      language codes.  If this parameter is NULL, then
                                      String is added or updated in the String Package
                                      associated with HiiHandle for all the languages
                                      that the String Package supports.  If this
                                      parameter is not NULL, then then String is added
                                      or updated in the String Package associated with
                                      HiiHandle for the set oflanguages specified by
                                      SupportedLanguages.  The format of
                                      SupportedLanguages must follow the language
                                      format assumed the HII Database.
  @param[in out]  StringId            On input, if StringId is null or *StringId is zero,
                                      a new string is created in the String Package associated
                                      with HiiHandle. if *StringId is non-zeor, the string
                                      specified by *StringId is updated in the String Package
                                      associated with HiiHandle.
                                      On output, the EFI_STRING_ID of the newly added or updated
                                      string is stored in *StringId.
  @param[in]      FormatString        A Null-terminated Unicode format string.
  @param[in]      ...                 The variable argument list.

  @retval EFI_SUCCESS                 The string is added or updated successfully in the String Package.
  @retval EFI_INVALID_PARAMETER       Input parameters are invalid.
  @retval EFI_ABORTED                 Error occurs in function.

**/
EFI_STATUS
EFIAPI
CustomizedHiiLibSetString (
  IN EFI_HII_HANDLE     HiiHandle,
  IN CONST CHAR8        *SupportedLanguages   OPTIONAL,
  IN OUT EFI_STRING_ID  *StringId             OPTIONAL,
  IN CONST CHAR16       *FormatString,
  ...
  )
{
  VA_LIST               Marker;
  CHAR16                String[MAX_STRING_LEN];
  UINTN                 NumberOfPrinted;
  EFI_STRING_ID         StringToken;

  NumberOfPrinted = 0;
  StringToken     = 0;

  ZeroMem (String, sizeof (String));

  if ((HiiHandle == NULL) ||
      (FormatString == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  VA_START (Marker, FormatString);
  NumberOfPrinted = UnicodeVSPrint (String, sizeof (String), FormatString, Marker);
  VA_END (Marker);

  if (NumberOfPrinted == 0) {
    return EFI_ABORTED;
  }

  if (StringId == NULL) {
    StringToken = HiiSetString (HiiHandle, 0, String, SupportedLanguages);
  } else {
    StringToken = HiiSetString (HiiHandle, *StringId, String, SupportedLanguages);
    *StringId = StringToken;
  }

  if (StringToken == 0) {
    return EFI_ABORTED;
  }

  return EFI_SUCCESS;
}

/**
  This function updates an existing format string in a String Package.

  @param[in]  HiiHandle               A handle that was previously registered in the
                                      HII Database.
  @param[in]  SupportedLanguages      A pointer to a Null-terminated ASCII string of
                                      language codes.  If this parameter is NULL, then
                                      String is added or updated in the String Package
                                      associated with HiiHandle for all the languages
                                      that the String Package supports.  If this
                                      parameter is not NULL, then then String is added
                                      or updated in the String Package associated with
                                      HiiHandle for the set oflanguages specified by
                                      SupportedLanguages.  The format of
                                      SupportedLanguages must follow the language
                                      format assumed the HII Database.
  @param[in]  StringId                The format string Id for getting from HII Database.
  @param[in]  ...                     The variable argument list.

  @retval EFI_SUCCESS                 The format string is updated successfully in the String Package.
  @retval EFI_INVALID_PARAMETER       Input parameters are invalid.
  @retval EFI_ABORTED                 Error occurs in function.

**/
EFI_STATUS
EFIAPI
CustomizedHiiLibFormatString (
  IN EFI_HII_HANDLE     HiiHandle,
  IN CONST CHAR8        *SupportedLanguages   OPTIONAL,
  IN EFI_STRING_ID      StringId,
  ...
  )
{
  EFI_STATUS            Status;
  VA_LIST               Marker;
  EFI_STRING            String;
  EFI_STRING_ID         StringToken;

  String      = NULL;
  StringToken = StringId;

  if ((HiiHandle == NULL) ||
      (StringId == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  String = HiiGetString (HiiHandle, StringId, SupportedLanguages);
  if (String == NULL) {
    return EFI_ABORTED;
  }

  VA_START (Marker, StringId);
  Status = CustomizedHiiLibSetString (HiiHandle, SupportedLanguages, &StringToken, String, Marker);
  VA_END (Marker);

  FreePool (String);

  return Status;
}

/**
  Allocates new OpCode Handles. OpCode Handles must be freed with HiiFreeOpCodeHandle() or
  CustomizedHiiLibFreeOpCodeHandles().

  @param[in]  StartLabelNumber      Label Number of start opcode.
  @param[in]  EndLabelNumber        Label Number of end opcode.
  @param[out] StartOpCodeHandle     Holds pointer to the buffer of start opcode handle.
  @param[out] EndOpCodeHandle       Holds pointer to the buffer of end opcode handle.

  @retval EFI_SUCCESS               Allocates Opcode Handles successfully.
  @retval EFI_INVALID_PARAMETER     StartOpCodeHandle or EndOpCodeHandle is null.
  @retval EFI_OUT_OF_RESOURCES      There are not enough resources to allocate new OpCode Handles.

**/
EFI_STATUS
EFIAPI
CustomizedHiiLibAllocateOpCodeHandles (
  OUT VOID      **StartOpCodeHandle,
  OUT VOID      **EndOpCodeHandle,
  IN UINT16     StartLabelNumber,
  IN UINT16     EndLabelNumber
  )
{
  EFI_IFR_GUID_LABEL   *StartLabel;
  EFI_IFR_GUID_LABEL   *EndLabel;

  if ((StartOpCodeHandle == NULL) || (EndOpCodeHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Init OpCode Handle and Allocate space for creation of Buffer
  //
  *StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  *EndOpCodeHandle   = HiiAllocateOpCodeHandle ();
  if ((*StartOpCodeHandle == NULL) || (*EndOpCodeHandle == NULL)) {
    CustomizedHiiLibFreeOpCodeHandles (2, *StartOpCodeHandle, *EndOpCodeHandle);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Create Hii Extend Label OpCode as the start opcode
  //
  StartLabel               = (EFI_IFR_GUID_LABEL *) HiiCreateGuidOpCode (*StartOpCodeHandle, &gEfiIfrTianoGuid, NULL, sizeof (EFI_IFR_GUID_LABEL));
  StartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number       = StartLabelNumber;

  //
  // Create Hii Extend Label OpCode as the end opcode
  //
  EndLabel                 = (EFI_IFR_GUID_LABEL *) HiiCreateGuidOpCode (*EndOpCodeHandle, &gEfiIfrTianoGuid, NULL, sizeof (EFI_IFR_GUID_LABEL));
  EndLabel->ExtendOpCode   = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number         = EndLabelNumber;

  return EFI_SUCCESS;
}

/**
  Frees OpCode Handles that was previously allocated with HiiFreeOpCodeHandle() or
  CustomizedHiiLibAllocateOpCodeHandles().

  When OpCode Handles is freed, all of the opcodes associated with the OpCode
  Handles are also freed.

  @param[in]      OpCodeNum           The sum of handles to the buffer of opcodes.
  @param[in out]  ...                 The variable argument list.

**/
VOID
EFIAPI
CustomizedHiiLibFreeOpCodeHandles (
  IN UINTN  OpCodeNum,
  ...
  )
{
  UINTN     Index;
  VA_LIST   Marker;
  VOID      *OpCodeHandle;

  VA_START (Marker, OpCodeNum);

  for (Index = 0; Index < OpCodeNum; Index++) {
    OpCodeHandle = VA_ARG (Marker, VOID *);
    if (OpCodeHandle) {
      HiiFreeOpCodeHandle (OpCodeHandle);
    }
  }

  VA_END (Marker);

  return;
}
