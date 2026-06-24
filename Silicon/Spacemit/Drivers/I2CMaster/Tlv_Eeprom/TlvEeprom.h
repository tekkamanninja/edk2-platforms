/********************************************************************************
Copyright (C) 2024 Spacemit Ltd.

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/

#ifndef TLV_EEPROM_H_
#define TLV_EEPROM_H_

#include <Uefi.h>
#include <Protocol/I2cIo.h>
#include <Protocol/TlvInfo.h>
#include "../Eeprom/Eeprom.h"

//
// TLV Info definitions
//
#define TLV_INFO_ID_STRING  "TlvInfo"
#define TLV_INFO_VERSION    0x01
#define TLV_TOTAL_LEN_MAX   (2048 - sizeof (TLV_INFO_HEADER))

#define TLVINFO_MAX_SIZE          512
#define TLVINFO_SIZE_MAX_TLV_LEN  (TLVINFO_MAX_SIZE - sizeof (TLV_INFO_HEADER))
#define TLV_VALUE_MAX_LEN         255
#define DECODE_VALUE_MAX          ((5 * TLV_VALUE_MAX_LEN) + 1)

#define TLV_INFO_INSTANCE_SIGNATURE  SIGNATURE_32('T', 'L', 'V', 'I')
#define TLV_INFO_INSTANCE_FROM_THIS(a)  CR (a, TLV_INFO_INSTANCE, TlvInfoProtocol, TLV_INFO_INSTANCE_SIGNATURE)

typedef struct {
  UINTN                         Signature;
  EFI_HANDLE                    TlvHandle;
  BOOLEAN                       HadReadTlvInfo;

  SPACEMIT_TLV_INFO_PROTOCOL    TlvInfoProtocol;
  SPACEMIT_EEPROM_PROTOCOL      *EepromProtocol;
  UINT8                         TlvInfoBuffer[TLVINFO_MAX_SIZE];
} TLV_INFO_INSTANCE;

//
// TLV Info Header
//
#pragma pack(1)
typedef struct {
  CHAR8     Signature[8];
  UINT8     Version;
  UINT16    TotalLen;
} TLV_INFO_HEADER;

//
// TLV Entry
//
typedef struct {
  UINT8    Tid;
  UINT8    Length;
  UINT8    Value[0];
} TLV_INFO_TLV;
#pragma pack()

//
// Function Prototypes
//

/**
 * Find a TLV with the specified code
 */
BOOLEAN
TlvInfoFindTlv (
  IN  UINT8  *TlvData,
  IN  UINT8  Tid,
  OUT INT32  *Index
  );

/**
 * Show TLV information
 */
EFI_STATUS
EFIAPI
ShowTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  );

/**
 * Get TLV information by type code
 */
EFI_STATUS
EFIAPI
GetTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  IN  UINT8                       Tid,
  OUT CHAR8                       *Buffer,
  IN  UINTN                       BufferSize
  );

/**
 * Set TLV information by type code
 */
EFI_STATUS
EFIAPI
SetTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  IN UINT8                        Tid,
  IN CHAR8                        *Value
  );

/**
 * Flush TLV information to EEPROM
 */
EFI_STATUS
EFIAPI
FlushTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  );

/**
 * Clear TLV information
 */
EFI_STATUS
EFIAPI
ClearTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  );

/**
 * Dump TLV information to buffer
 */
EFI_STATUS
EFIAPI
DumpTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  OUT UINT8                       *Buffer,
  IN  UINTN                       BufferSize
  );

/**
 * Initialize TLV EEPROM driver
 */
EFI_STATUS
InitializeTlvEeprom (
  IN SPACEMIT_EEPROM_PROTOCOL  *EepromProtocol
  );

#endif // TLV_EEPROM_H_
