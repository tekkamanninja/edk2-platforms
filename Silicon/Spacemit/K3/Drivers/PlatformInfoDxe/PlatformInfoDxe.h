/** @file
*
*  Copyright (c) 2026, Spacemit Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __SPACEMIT_PLATFORM_INFO__
#define __SPACEMIT_PLATFORM_INFO__

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Protocol/TlvInfo.h>
#include <Protocol/PlatformInfo.h>

#define PLATFROM_INFO_SIGNATURE              SIGNATURE_32 ('P', 'L', 'T', 'F')
#define PLATFROM_INFO_INSTANCE_FROM_THIS(a)  CR (a, PLATFROM_INFO_INSTANCE, PlatformInfo, PLATFROM_INFO_SIGNATURE)

typedef
EFI_STATUS
(EFIAPI *EFI_READ_PLATFORM_INFO) (
  IN  PLATFORM_INFO_PROTOCOL  *This,
  IN  CHAR8                   *Name,
  OUT VOID                    *Info,
  IN  UINTN                   MaxSize
  );

typedef
EFI_STATUS
(EFIAPI *EFI_SET_PLATFORM_INFO) (
  IN  PLATFORM_INFO_PROTOCOL  *This,
  IN  CHAR8                   *Name,
  OUT VOID                    *Info,
  IN  UINTN                   InfoSize
  );

typedef struct {
  CHAR8                     *Name;
  EFI_READ_PLATFORM_INFO    ReadInfo;
  EFI_SET_PLATFORM_INFO     SetInfo;
} SPACEMIT_PLATFROM_INFO;

/**
  TLV data type.
*/
typedef enum {
  TlvDataUint8,
  TlvDataUint16,
  TlvDataUint32,
  TlvDataUint64,
  TlvDataString,
  TlvDataStructure,
} TLV_DATA_TYPE;

typedef struct {
  CONST CHAR8    *Name;
  UINT32         Tid;
  UINT32         Type;
} TLV_INFO_CONFIG;

typedef struct {
  UINTN                       Signature;
  EFI_HANDLE                  Handle;
  PLATFORM_INFO_PROTOCOL      PlatformInfo;
  SPACEMIT_TLV_INFO_PROTOCOL  *TlvInfoProtocol;
} PLATFROM_INFO_INSTANCE;

#endif // __SPACEMIT_PLATFORM_INFO__
