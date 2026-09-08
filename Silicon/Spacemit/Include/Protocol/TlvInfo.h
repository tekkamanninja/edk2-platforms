/** @file
  TLV info access API definitions.

  Copyright (c) 2025 Spacemit Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __TLV_INFO__
#define __TLV_INFO__

#include <Uefi.h>

// TLV Info Protocol GUID
extern EFI_GUID gSpacemitTlvInfoProtocolGuid;

//
// TLV Type Codes
//
#define TLV_CODE_PRODUCT_NAME        0x21
#define TLV_CODE_PART_NUMBER         0x22
#define TLV_CODE_SERIAL_NUMBER       0x23
#define TLV_CODE_MAC_BASE            0x24
#define TLV_CODE_MANUF_DATE          0x25
#define TLV_CODE_DEVICE_VERSION      0x26
#define TLV_CODE_LABEL_REVISION      0x27
#define TLV_CODE_PLATFORM_NAME       0x28
#define TLV_CODE_ONIE_VERSION        0x29
#define TLV_CODE_MAC_SIZE            0x2A
#define TLV_CODE_MANUF_NAME          0x2B
#define TLV_CODE_MANUF_COUNTRY       0x2C
#define TLV_CODE_VENDOR_NAME         0x2D
#define TLV_CODE_DIAG_VERSION        0x2E
#define TLV_CODE_SERVICE_TAG         0x2F
#define TLV_CODE_SDK_VERSION         0x40
#define TLV_CODE_DDR_CSNUM           0x41
#define TLV_CODE_DDR_TYPE            0x42
#define TLV_CODE_DDR_DATARATE        0x43
#define TLV_CODE_DDR_TX_ODT          0x44
#define TLV_CODE_WIFI_MAC_ADDR       0x60
#define TLV_CODE_BLUETOOTH_ADDR      0x61
#define TLV_CODE_PMIC_TYPE           0x80
#define TLV_CODE_EEPROM_I2C_INDEX    0x81
#define TLV_CODE_EEPROM_PIN_GROUP    0x82
#define TLV_CODE_SECOND_BOOT_DEVICE  0x83
#define TLV_CODE_VENDOR_EXT          0xFD
#define TLV_CODE_CRC_32              0xFE

typedef struct _SPACEMIT_TLV_INFO_PROTOCOL SPACEMIT_TLV_INFO_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *GET_TLV_INFO) (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  IN  UINT8                        TCode,
  OUT CHAR8                       *Buffer,
  IN  UINTN                        BufferSize
  );

typedef
EFI_STATUS
(EFIAPI *SET_TLV_INFO) (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  IN UINT8                         TCode,
  IN CHAR8                        *Buffer
  );

typedef
EFI_STATUS
(EFIAPI *FLUSH_TLV_INFO) (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  );

typedef
EFI_STATUS
(EFIAPI *CLEAR_TLV_INFO) (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  );

typedef
EFI_STATUS
(EFIAPI *SHOW_TLV_INFO) (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  );

typedef
EFI_STATUS
(EFIAPI *DUMP_TLV_INFO) (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  OUT UINT8                       *Buffer,
  IN  UINTN                        BufferSize
  );

struct _SPACEMIT_TLV_INFO_PROTOCOL {
  GET_TLV_INFO   GetTlvInfo;
  SET_TLV_INFO   SetTlvInfo;
  FLUSH_TLV_INFO FlushTlvInfo;
  CLEAR_TLV_INFO ClearTlvInfo;
  SHOW_TLV_INFO  ShowTlvInfo;
  DUMP_TLV_INFO  DumpTlvInfo;
};

#endif // __TLV_INFO__
