/********************************************************************************
Copyright (C) 2024 Spacemit Ltd.

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include "TlvEeprom.h"

// #define TEST_TLV_EEPROM_PROTOCOL

typedef struct {
  UINT8          Tid;
  CONST CHAR8    *Description;
} TLV_CODE_NAME;

STATIC CONST TLV_CODE_NAME  mTlvCodeList[] = {
  { TLV_CODE_PRODUCT_NAME,       "Product Name"       },
  { TLV_CODE_PART_NUMBER,        "Part Number"        },
  { TLV_CODE_SERIAL_NUMBER,      "Serial Number"      },
  { TLV_CODE_MAC_BASE,           "Base MAC Address"   },
  { TLV_CODE_WIFI_MAC_ADDR,      "WiFi MAC Address"   },
  { TLV_CODE_BLUETOOTH_ADDR,     "Bluetooth Address"  },
  { TLV_CODE_MANUF_DATE,         "Manufacture Date"   },
  { TLV_CODE_DEVICE_VERSION,     "Device Version"     },
  { TLV_CODE_LABEL_REVISION,     "Label Revision"     },
  { TLV_CODE_PLATFORM_NAME,      "Platform Name"      },
  { TLV_CODE_ONIE_VERSION,       "ONIE Version"       },
  { TLV_CODE_MAC_SIZE,           "MAC Number"         },
  { TLV_CODE_MANUF_NAME,         "Manufacturer"       },
  { TLV_CODE_MANUF_COUNTRY,      "Country Code"       },
  { TLV_CODE_VENDOR_NAME,        "Vendor Name"        },
  { TLV_CODE_DIAG_VERSION,       "Diag Version"       },
  { TLV_CODE_SERVICE_TAG,        "Service Tag"        },
  { TLV_CODE_VENDOR_EXT,         "Vendor Extension"   },
  { TLV_CODE_SDK_VERSION,        "SDK Version"        },
  { TLV_CODE_DDR_CSNUM,          "DDR CS Number"      },
  { TLV_CODE_DDR_DATARATE,       "DDR Datarate"       },
  { TLV_CODE_DDR_TX_ODT,         "DDR tx odt"         },
  { TLV_CODE_DDR_TYPE,           "DDR Type"           },
  { TLV_CODE_SECOND_BOOT_DEVICE, "Second Boot Device" },
  { TLV_CODE_CRC_32,             "CRC-32"             },
};

/**
 * Convert TLV type code to description
 */
STATIC CHAR8 *
TlvTypeToDescription (
  IN UINT8  Tid
  )
{
  UINTN  i;

  for (i = 0; i < ARRAY_SIZE (mTlvCodeList); i++) {
    if (mTlvCodeList[i].Tid == Tid) {
      return (CHAR8 *)mTlvCodeList[i].Description;
    }
  }

  return (CHAR8 *)"Unknown";
}

/**
 * Check if TLV header is valid
 */
STATIC BOOLEAN
IsValidTlvInfoHeader (
  IN CONST TLV_INFO_HEADER  *Header
  )
{
  return ((AsciiStrnCmp (Header->Signature, TLV_INFO_ID_STRING, sizeof (Header->Signature)) == 0) &&
          (Header->Version == TLV_INFO_VERSION) &&
          (SwapBytes16 (Header->TotalLen) <= TLV_TOTAL_LEN_MAX));
}

/**
 * Check if TLV entry is valid
 */
STATIC BOOLEAN
IsValidTlv (
  IN CONST TLV_INFO_TLV  *Tlv
  )
{
  return ((Tlv->Tid != 0x00) && (Tlv->Tid != 0xFF));
}

/**
 * Read TLV data from EEPROM
 */
STATIC EFI_STATUS
ReadTlvFromStorage (
  IN SPACEMIT_EEPROM_PROTOCOL  *EepromProtocol,
  IN  UINT32                   Address,
  OUT UINT8                    *Buffer,
  IN  UINT32                   Size
  )
{
  EFI_STATUS  Status;

  if (EepromProtocol == NULL) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: EEPROM protocol not initialized\n"));
    return EFI_NOT_READY;
  }

  Status = EepromProtocol->Transfer (
                                     EepromProtocol,
                                     (UINT16)Address,
                                     Size,
                                     Buffer,
                                     I2C_FLAG_READ
                                     );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to read from EEPROM: %r\n", Status));
  }

  return Status;
}

/**
 * Write TLV data to EEPROM
 */
STATIC EFI_STATUS
WriteTlvToStorage (
  IN SPACEMIT_EEPROM_PROTOCOL  *EepromProtocol,
  IN UINT32                    Address,
  IN UINT8                     *Buffer,
  IN UINT32                    Size
  )
{
  EFI_STATUS  Status;

  if (EepromProtocol == NULL) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: EEPROM protocol not initialized\n"));
    return EFI_NOT_READY;
  }

  Status = EepromProtocol->Transfer (
                                     EepromProtocol,
                                     (UINT16)Address,
                                     Size,
                                     Buffer,
                                     0                  // Write operation
                                     );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to write to EEPROM: %r\n", Status));
  }

  return Status;
}

/**
 * Validate checksum in TLV data
 */
STATIC BOOLEAN
IsChecksumValid (
  IN UINT8  *TlvData
  )
{
  TLV_INFO_HEADER  *TlvHdr;
  TLV_INFO_TLV     *TlvCrc;
  UINT32           CalcCrc;
  UINT32           StoredCrc;
  UINT16           TotalLen;

  TlvHdr = (TLV_INFO_HEADER *)TlvData;

  // Check if TLV header is valid
  if (!IsValidTlvInfoHeader (TlvHdr)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Invalid TLV header\n"));
    return FALSE;
  }

  TotalLen = SwapBytes16 (TlvHdr->TotalLen);

  // Check if the last TLV is CRC
  TlvCrc = (TLV_INFO_TLV *)(&TlvData[sizeof (TLV_INFO_HEADER) + TotalLen - (sizeof (TLV_INFO_TLV) + 4)]);
  if ((TlvCrc->Tid != TLV_CODE_CRC_32) || (TlvCrc->Length != 4)) {
    return FALSE;
  }

  // Calculate checksum
  CalcCrc   = CalculateCrc32 (TlvData, sizeof (TLV_INFO_HEADER) + TotalLen - 4);
  StoredCrc = (TlvCrc->Value[0] << 24) | (TlvCrc->Value[1] << 16) | (TlvCrc->Value[2] << 8) | TlvCrc->Value[3];

  return CalcCrc == StoredCrc;
}

/**
 * Update CRC-32 TLV
 */
STATIC VOID
UpdateCrc (
  IN UINT8  *TlvData
  )
{
  TLV_INFO_HEADER  *TlvHdr;
  TLV_INFO_TLV     *TlvCrc;
  UINT32           CalcCrc;
  INT32            Index;
  UINT16           TotalLen;

  TlvHdr   = (TLV_INFO_HEADER *)TlvData;
  TotalLen = SwapBytes16 (TlvHdr->TotalLen);

  // Find CRC TLV
  if (!TlvInfoFindTlv (TlvData, TLV_CODE_CRC_32, &Index)) {
    if ((TotalLen + sizeof (TLV_INFO_TLV) + 4) > TLVINFO_SIZE_MAX_TLV_LEN) {
      return;
    }

    Index            = sizeof (TLV_INFO_HEADER) + TotalLen;
    TlvHdr->TotalLen = SwapBytes16 (TotalLen + sizeof (TLV_INFO_TLV) + 4);
  }

  TlvCrc         = (TLV_INFO_TLV *)(&TlvData[Index]);
  TlvCrc->Tid    = TLV_CODE_CRC_32;
  TlvCrc->Length = 4;

  // Calculate checksum
  CalcCrc          = CalculateCrc32 (TlvData, sizeof (TLV_INFO_HEADER) + SwapBytes16 (TlvHdr->TotalLen) - 4);
  TlvCrc->Value[0] = (UINT8)((CalcCrc >> 24) & 0xFF);
  TlvCrc->Value[1] = (UINT8)((CalcCrc >> 16) & 0xFF);
  TlvCrc->Value[2] = (UINT8)((CalcCrc >> 8) & 0xFF);
  TlvCrc->Value[3] = (UINT8)(CalcCrc & 0xFF);
}

/**
 * Find TLV with specified code
 */
BOOLEAN
TlvInfoFindTlv (
  IN  UINT8  *TlvData,
  IN  UINT8  Tid,
  OUT INT32  *Index
  )
{
  TLV_INFO_HEADER  *TlvHdr;
  TLV_INFO_TLV     *Tlv;
  INT32            TlvEnd;
  UINT16           TotalLen;

  TlvHdr   = (TLV_INFO_HEADER *)TlvData;
  TotalLen = SwapBytes16 (TlvHdr->TotalLen);

  // Traverse TLVs to find the first match
  *Index = sizeof (TLV_INFO_HEADER);
  TlvEnd = sizeof (TLV_INFO_HEADER) + TotalLen;
  while (*Index < TlvEnd) {
    Tlv = (TLV_INFO_TLV *)&TlvData[*Index];
    if (!IsValidTlv (Tlv)) {
      return FALSE;
    }

    if (Tlv->Tid == Tid) {
      return TRUE;
    }

    *Index += sizeof (TLV_INFO_TLV) + Tlv->Length;
  }

  return FALSE;
}

/**
 * Read TLV info from EEPROM
 */
STATIC EFI_STATUS
ReadTlvInfo (
  TLV_INFO_INSTANCE  *TlvInfoInstance
  )
{
  EFI_STATUS       Status;
  TLV_INFO_HEADER  TlvHdr;

  if (TlvInfoInstance->HadReadTlvInfo) {
    return EFI_SUCCESS;
  }

  Status = ReadTlvFromStorage (
                               TlvInfoInstance->EepromProtocol,
                               0,
                               (UINT8 *)&TlvHdr,
                               sizeof (TLV_INFO_HEADER)
                               );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!IsValidTlvInfoHeader (&TlvHdr)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Invalid TLV header in EEPROM\n"));
    return EFI_DEVICE_ERROR;
  }

  Status = ReadTlvFromStorage (
                               TlvInfoInstance->EepromProtocol,
                               0,
                               TlvInfoInstance->TlvInfoBuffer,
                               sizeof (TLV_INFO_HEADER) + SwapBytes16 (TlvHdr.TotalLen)
                               );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!IsChecksumValid (TlvInfoInstance->TlvInfoBuffer)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Invalid TLV checksum\n"));
    return EFI_CRC_ERROR;
  }

  TlvInfoInstance->HadReadTlvInfo = TRUE;
  return EFI_SUCCESS;
}

/**
 * Initialize TLV info
 */
STATIC EFI_STATUS
InitializeTlvInfo (
  TLV_INFO_INSTANCE  *TlvInfoInstance
  )
{
  TLV_INFO_HEADER  *TlvHdr;

  // Clear buffer
  ZeroMem (TlvInfoInstance->TlvInfoBuffer, sizeof (TlvInfoInstance->TlvInfoBuffer));

  // Initialize TLV header
  TlvHdr = (TLV_INFO_HEADER *)TlvInfoInstance->TlvInfoBuffer;
  AsciiStrCpyS (TlvHdr->Signature, sizeof (TlvHdr->Signature), TLV_INFO_ID_STRING);
  TlvHdr->Version  = TLV_INFO_VERSION;
  TlvHdr->TotalLen = 0;

  // Add CRC TLV
  UpdateCrc (TlvInfoInstance->TlvInfoBuffer);

  TlvInfoInstance->HadReadTlvInfo = TRUE;
  return EFI_SUCCESS;
}

/**
 * Set TLV byte data
 */
STATIC EFI_STATUS
SetBytes (
  OUT UINT8       *TlvData,
  IN UINT8        Tid,
  IN CONST CHAR8  *Value
  )
{
  TLV_INFO_HEADER  *TlvHdr;
  TLV_INFO_TLV     *Tlv;
  UINT16           TotalLen;
  UINT16           Index;
  UINT16           ValueLen;
  BOOLEAN          Found;

  TlvHdr   = (TLV_INFO_HEADER *)TlvData;
  TotalLen = SwapBytes16 (TlvHdr->TotalLen);
  ValueLen = (UINT16)AsciiStrLen (Value);

  // delete CRC item temporarily, which will be recalculated
  // and appended by the end of this function
  if (TotalLen >= 6) {
    Tlv = (TLV_INFO_TLV *)&TlvData[sizeof (TLV_INFO_HEADER) + TotalLen - 6];
    if (Tlv->Tid == TLV_CODE_CRC_32) {
      TotalLen -= 6;
      TlvHdr->TotalLen = SwapBytes16 (TotalLen);
    }
  }

  // Find if there is already a TLV with the same type
  Found = FALSE;
  Index = sizeof (TLV_INFO_HEADER);
  while (Index < (sizeof (TLV_INFO_HEADER) + TotalLen)) {
    Tlv = (TLV_INFO_TLV *)&TlvData[Index];
    if (!IsValidTlv (Tlv)) {
      break;
    }

    if (Tlv->Tid == Tid) {
      // Found a TLV with the same type, update it
      Found = TRUE;

      // If the new value length is the same as the old value length, replace it directly
      if (Tlv->Length == ValueLen) {
        CopyMem (Tlv->Value, Value, ValueLen);
      } else {
        // Otherwise, delete the old TLV and add a new TLV later
        UINT16  OldTlvSize    = sizeof (TLV_INFO_TLV) + Tlv->Length;
        UINT16  RemainingSize = TotalLen - (Index - sizeof (TLV_INFO_HEADER)) - OldTlvSize;

        // Move the TLV data behind it
        if (RemainingSize > 0) {
          CopyMem (
                   &TlvData[Index],
                   &TlvData[Index + OldTlvSize],
                   RemainingSize
                   );
        }

        // Update total length
        TotalLen        -= OldTlvSize;
        TlvHdr->TotalLen = SwapBytes16 (TotalLen);

        // Mark as not found to add later
        Found = FALSE;
      }

      break;
    }

    Index += sizeof (TLV_INFO_TLV) + Tlv->Length;
  }

  // If no TLV with the same type is found, add a new TLV
  if (!Found) {
    // Check if there is enough space
    if ((TotalLen + sizeof (TLV_INFO_TLV) + ValueLen) > TLVINFO_SIZE_MAX_TLV_LEN) {
      return EFI_BUFFER_TOO_SMALL;
    }

    // Add new TLV
    Tlv         = (TLV_INFO_TLV *)&TlvData[sizeof (TLV_INFO_HEADER) + TotalLen];
    Tlv->Tid    = Tid;
    Tlv->Length = (UINT8)ValueLen;
    CopyMem (Tlv->Value, Value, ValueLen);

    // Update total length
    TotalLen        += sizeof (TLV_INFO_TLV) + ValueLen;
    TlvHdr->TotalLen = SwapBytes16 (TotalLen);
  }

  // Update CRC
  UpdateCrc (TlvData);

  return EFI_SUCCESS;
}

/**
 * Get TLV info
 */
EFI_STATUS
EFIAPI
GetTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  IN  UINT8                       Tid,
  OUT CHAR8                       *Buffer,
  IN  UINTN                       BufferSize
  )
{
  EFI_STATUS         Status;
  INT32              Index;
  TLV_INFO_TLV       *Tlv;
  TLV_INFO_INSTANCE  *TlvInfoInstance;

  if ((Buffer == NULL) || (BufferSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  TlvInfoInstance = TLV_INFO_INSTANCE_FROM_THIS (This);
  // Ensure TLV info is loaded
  Status = ReadTlvInfo (TlvInfoInstance);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Find the specified TLV
  if (!TlvInfoFindTlv (TlvInfoInstance->TlvInfoBuffer, Tid, &Index)) {
    return EFI_NOT_FOUND;
  }

  Tlv = (TLV_INFO_TLV *)&TlvInfoInstance->TlvInfoBuffer[Index];

  // Check buffer size
  if (BufferSize < Tlv->Length) {
    return EFI_BUFFER_TOO_SMALL;
  }

  // Copy TLV value to buffer
  CopyMem (Buffer, Tlv->Value, Tlv->Length);
  Buffer[Tlv->Length] = '\0';

  return EFI_SUCCESS;
}

/**
 * Set TLV info
 */
EFI_STATUS
EFIAPI
SetTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  IN UINT8                        Tid,
  IN CHAR8                        *Value
  )
{
  UINTN              ValueLen;
  CHAR8              *TempValue;
  EFI_STATUS         Status;
  TLV_INFO_INSTANCE  *TlvInfoInstance;

  if (Value == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Ensure string ends with NULL
  ValueLen  = AsciiStrLen (Value);
  TempValue = AllocateZeroPool (ValueLen + 1);
  if (TempValue == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  TlvInfoInstance = TLV_INFO_INSTANCE_FROM_THIS (This);
  AsciiStrnCpyS (TempValue, ValueLen + 1, Value, ValueLen);

  // Ensure TLV info is loaded
  Status = ReadTlvInfo (TlvInfoInstance);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SetBytes (TlvInfoInstance->TlvInfoBuffer, Tid, TempValue);

  FreePool (TempValue);
  return Status;
}

/**
 * Write TLV info to EEPROM
 */
EFI_STATUS
EFIAPI
FlushTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  )
{
  EFI_STATUS         Status;
  TLV_INFO_HEADER    *TlvHdr;
  UINT16             TotalLen;
  TLV_INFO_INSTANCE  *TlvInfoInstance;

  TlvInfoInstance = TLV_INFO_INSTANCE_FROM_THIS (This);

  if (!TlvInfoInstance->HadReadTlvInfo) {
    return EFI_NOT_READY;
  }

  if (!IsChecksumValid (TlvInfoInstance->TlvInfoBuffer)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Invalid TLV checksum\n"));
    return EFI_CRC_ERROR;
  }

  TlvHdr   = (TLV_INFO_HEADER *)TlvInfoInstance->TlvInfoBuffer;
  TotalLen = SwapBytes16 (TlvHdr->TotalLen);

  // Write TLV data to EEPROM
  Status = WriteTlvToStorage (
                              TlvInfoInstance->EepromProtocol,
                              0,
                              TlvInfoInstance->TlvInfoBuffer,
                              sizeof (TLV_INFO_HEADER) + TotalLen
                              );

  return Status;
}

/**
 * Clear TLV info
 */
EFI_STATUS
EFIAPI
ClearTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  )
{
  EFI_STATUS         Status;
  TLV_INFO_INSTANCE  *TlvInfoInstance;

  TlvInfoInstance = TLV_INFO_INSTANCE_FROM_THIS (This);

  // Initialize a new TLV info
  Status = InitializeTlvInfo (TlvInfoInstance);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Write to EEPROM
  return FlushTlvInfo (This);
}

/**
 * Show TLV info
 */
EFI_STATUS
EFIAPI
ShowTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  )
{
  EFI_STATUS         Status;
  TLV_INFO_HEADER    *TlvHdr;
  TLV_INFO_TLV       *Tlv;
  UINT16             TotalLen;
  UINT16             Index;
  CHAR8              ValueStr[DECODE_VALUE_MAX];
  UINT16             MacCount;
  TLV_INFO_INSTANCE  *TlvInfoInstance;

  TlvInfoInstance = TLV_INFO_INSTANCE_FROM_THIS (This);

  // Ensure TLV info is loaded
  Status = ReadTlvInfo (TlvInfoInstance);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  TlvHdr   = (TLV_INFO_HEADER *)TlvInfoInstance->TlvInfoBuffer;
  TotalLen = SwapBytes16 (TlvHdr->TotalLen);

  DEBUG ((DEBUG_INFO, "TLV Info Header:\n"));
  DEBUG ((DEBUG_INFO, "  Signature: %a\n", TlvHdr->Signature));
  DEBUG ((DEBUG_INFO, "  Version:   0x%02x\n", TlvHdr->Version));
  DEBUG ((DEBUG_INFO, "  Length:    %d bytes\n", TotalLen));

  DEBUG ((DEBUG_INFO, "TLV Data:\n"));

  Index = sizeof (TLV_INFO_HEADER);
  while (Index < (sizeof (TLV_INFO_HEADER) + TotalLen)) {
    Tlv = (TLV_INFO_TLV *)&TlvInfoInstance->TlvInfoBuffer[Index];
    if (!IsValidTlv (Tlv)) {
      break;
    }

    switch (Tlv->Tid) {
      case TLV_CODE_MAC_BASE:
      case TLV_CODE_WIFI_MAC_ADDR:
      case TLV_CODE_BLUETOOTH_ADDR:
        // MAC address displayed in hexadecimal format
        DEBUG (
               (
                DEBUG_INFO,
                "  %-20a (0x%02x): %02X:%02X:%02X:%02X:%02X:%02X\n",
                TlvTypeToDescription (Tlv->Tid),
                Tlv->Tid,
                Tlv->Value[0], Tlv->Value[1], Tlv->Value[2],
                Tlv->Value[3], Tlv->Value[4], Tlv->Value[5]
               )
               );
        break;

      case TLV_CODE_MAC_SIZE:
        // MAC address count displayed as a number
        MacCount = ((UINT8)Tlv->Value[0] << 8) | (UINT8)Tlv->Value[1];
        DEBUG (
               (
                DEBUG_INFO,
                "  %-20a (0x%02x): %u\n",
                "MAC Addresses",
                Tlv->Tid,
                MacCount
               )
               );
        break;

      case TLV_CODE_CRC_32:
        // CRC-32 displayed in hexadecimal format
        DEBUG (
               (
                DEBUG_INFO,
                "  %-20a (0x%02x): 0x%02X%02X%02X%02X\n",
                TlvTypeToDescription (Tlv->Tid),
                Tlv->Tid,
                Tlv->Value[0], Tlv->Value[1], Tlv->Value[2], Tlv->Value[3]
               )
               );
        break;

      default:
        // Other types displayed as ASCII strings
        AsciiStrnCpyS (ValueStr, sizeof (ValueStr), (CHAR8 *)Tlv->Value, Tlv->Length);
        ValueStr[Tlv->Length] = '\0';

        DEBUG (
               (
                DEBUG_INFO,
                "  %-20a (0x%02x): %a\n",
                TlvTypeToDescription (Tlv->Tid),
                Tlv->Tid,
                ValueStr
               )
               );
        break;
    }

    Index += sizeof (TLV_INFO_TLV) + Tlv->Length;
  }

  return EFI_SUCCESS;
}

/**
 * Dump TLV info to buffer
 */
EFI_STATUS
EFIAPI
DumpTlvInfo (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This,
  OUT UINT8                       *Buffer,
  IN  UINTN                       BufferSize
  )
{
  EFI_STATUS         Status;
  TLV_INFO_HEADER    *TlvHdr;
  UINT16             TotalLen;
  TLV_INFO_INSTANCE  *TlvInfoInstance;

  TlvInfoInstance = TLV_INFO_INSTANCE_FROM_THIS (This);

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Ensure TLV info is loaded
  Status = ReadTlvInfo (TlvInfoInstance);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  TlvHdr   = (TLV_INFO_HEADER *)TlvInfoInstance->TlvInfoBuffer;
  TotalLen = SwapBytes16 (TlvHdr->TotalLen);

  // Check buffer size
  if (BufferSize < (sizeof (TLV_INFO_HEADER) + TotalLen)) {
    return EFI_BUFFER_TOO_SMALL;
  }

  // Copy TLV data to buffer
  CopyMem (Buffer, TlvInfoInstance->TlvInfoBuffer, sizeof (TLV_INFO_HEADER) + TotalLen);

  return EFI_SUCCESS;
}

/**
 * Implement AsciiStrChr function
 */
CHAR8 *
AsciiStrChr (
  IN CONST CHAR8  *String,
  IN CHAR8        Char
  )
{
  if (String == NULL) {
    return NULL;
  }

  while (*String != '\0') {
    if (*String == Char) {
      return (CHAR8 *)String;
    }

    String++;
  }

  return NULL;
}

#ifdef TEST_TLV_EEPROM_PROTOCOL

/**
 * Test all interfaces of the TLV EEPROM protocol
 */
STATIC
EFI_STATUS
TestTlvEepromProtocol (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  )
{
  EFI_STATUS                  Status;
  SPACEMIT_TLV_INFO_PROTOCOL  *TlvEeprom;
  UINT8                       DumpBuffer[TLVINFO_MAX_SIZE];
  CHAR8                       Buffer[64];
  UINTN                       i, j;
  UINT16                      MacCount;

  DEBUG ((DEBUG_INFO, "Starting TLV EEPROM protocol interface tests\n"));

  TlvEeprom = This;

  // First, show existing TLV info
  DEBUG ((DEBUG_INFO, "==============================================\n"));
  DEBUG ((DEBUG_INFO, "Existing TLV Info in EEPROM:\n"));
  Status = TlvEeprom->ShowTlvInfo (TlvEeprom);
  DEBUG ((DEBUG_INFO, "ShowTlvInfo (existing) test: %r\n", Status));
  DEBUG ((DEBUG_INFO, "==============================================\n"));

  // Dump existing TLV info
  DEBUG ((DEBUG_INFO, "==============================================\n"));
  DEBUG ((DEBUG_INFO, "Dumping TLV Info to buffer:\n"));
  ZeroMem (DumpBuffer, sizeof (DumpBuffer));
  Status = TlvEeprom->DumpTlvInfo (TlvEeprom, DumpBuffer, sizeof (DumpBuffer));
  DEBUG ((DEBUG_INFO, "DumpTlvInfo test: %r\n", Status));

  // Print the dumped buffer in hex
  DEBUG ((DEBUG_INFO, "Dumped TLV Info (hex):\n"));
  for (i = 0; i < 64 && i < sizeof (DumpBuffer); i++) {
    DEBUG ((DEBUG_INFO, "%02X ", DumpBuffer[i]));
    if ((i + 1) % 16 == 0) {
      DEBUG ((DEBUG_INFO, "\n"));
    }
  }

  DEBUG ((DEBUG_INFO, "\n==============================================\n"));

  // Try to get all TLV values
  DEBUG ((DEBUG_INFO, "==============================================\n"));
  DEBUG ((DEBUG_INFO, "Getting all TLV values:\n"));
  for (i = 0; i < ARRAY_SIZE (mTlvCodeList); i++) {
    ZeroMem (Buffer, sizeof (Buffer));
    Status = TlvEeprom->GetTlvInfo (TlvEeprom, mTlvCodeList[i].Tid, Buffer, sizeof (Buffer));

    DEBUG ((DEBUG_INFO, "  %a (0x%02x): ", mTlvCodeList[i].Description, mTlvCodeList[i].Tid));

    if (!EFI_ERROR (Status)) {
      // Special handling for MAC address and binary data
      if ((mTlvCodeList[i].Tid == TLV_CODE_MAC_BASE) ||
          (mTlvCodeList[i].Tid == TLV_CODE_WIFI_MAC_ADDR) ||
          (mTlvCodeList[i].Tid == TLV_CODE_BLUETOOTH_ADDR))
      {
        // Print MAC as hex
        for (j = 0; j < 6 && j < sizeof (Buffer); j++) {
          DEBUG ((DEBUG_INFO, "%02X%c", (UINT8)Buffer[j], (j < 5) ? ':' : ' '));
        }

        DEBUG ((DEBUG_INFO, "\n"));
      } else if (mTlvCodeList[i].Tid == TLV_CODE_CRC_32) {
        // Print CRC as hex
        DEBUG ((DEBUG_INFO, "0x"));
        for (j = 0; j < 4 && j < sizeof (Buffer); j++) {
          DEBUG ((DEBUG_INFO, "%02X", (UINT8)Buffer[j]));
        }

        DEBUG ((DEBUG_INFO, "\n"));
      } else if (mTlvCodeList[i].Tid == TLV_CODE_MAC_SIZE) {
        // MAC Addresses count is a 2-byte value
        MacCount = ((UINT8)Buffer[0] << 8) | (UINT8)Buffer[1];
        DEBUG ((DEBUG_INFO, "%u\n", MacCount));
      } else {
        // For text data, print as ASCII
        DEBUG ((DEBUG_INFO, "%a\n", Buffer));
      }
    } else {
      DEBUG ((DEBUG_INFO, "%r\n", Status));
    }
  }

  DEBUG ((DEBUG_INFO, "==============================================\n"));

  // Test SetTlvInfo interface for all TLV types
  DEBUG ((DEBUG_INFO, "==============================================\n"));
  DEBUG ((DEBUG_INFO, "Setting TLV values:\n"));

  // Set Product Name
  Status = TlvEeprom->SetTlvInfo (TlvEeprom, TLV_CODE_PRODUCT_NAME, "Test Product");
  DEBUG ((DEBUG_INFO, "  SetTlvInfo (Product Name): %r\n", Status));

  // Set Part Number
  Status = TlvEeprom->SetTlvInfo (TlvEeprom, TLV_CODE_PART_NUMBER, "PN12345");
  DEBUG ((DEBUG_INFO, "  SetTlvInfo (Part Number): %r\n", Status));

  // Set Serial Number
  Status = TlvEeprom->SetTlvInfo (TlvEeprom, TLV_CODE_SERIAL_NUMBER, "SN987654321");
  DEBUG ((DEBUG_INFO, "  SetTlvInfo (Serial Number): %r\n", Status));

  // Set Platform Name
  Status = TlvEeprom->SetTlvInfo (TlvEeprom, TLV_CODE_PLATFORM_NAME, "Test Platform");
  DEBUG ((DEBUG_INFO, "  SetTlvInfo (Platform Name): %r\n", Status));

  // Set Manufacturer
  Status = TlvEeprom->SetTlvInfo (TlvEeprom, TLV_CODE_MANUF_NAME, "Test Manufacturer");
  DEBUG ((DEBUG_INFO, "  SetTlvInfo (Manufacturer): %r\n", Status));

  // Show TLV info after setting values
  DEBUG ((DEBUG_INFO, "TLV Info after setting values:\n"));
  Status = TlvEeprom->ShowTlvInfo (TlvEeprom);
  DEBUG ((DEBUG_INFO, "==============================================\n"));

  // Test FlushTlvInfo interface
  DEBUG ((DEBUG_INFO, "==============================================\n"));
  DEBUG ((DEBUG_INFO, "Flushing TLV Info to EEPROM:\n"));
  Status = TlvEeprom->FlushTlvInfo (TlvEeprom);
  DEBUG ((DEBUG_INFO, "FlushTlvInfo test: %r\n", Status));

  // Show TLV info after flushing
  DEBUG ((DEBUG_INFO, "TLV Info after flushing:\n"));
  Status = TlvEeprom->ShowTlvInfo (TlvEeprom);
  DEBUG ((DEBUG_INFO, "==============================================\n"));

  // Test ClearTlvInfo interface
  DEBUG ((DEBUG_INFO, "==============================================\n"));
  DEBUG ((DEBUG_INFO, "Clearing TLV Info:\n"));
  Status = TlvEeprom->ClearTlvInfo (TlvEeprom);
  DEBUG ((DEBUG_INFO, "ClearTlvInfo test: %r\n", Status));

  // Show TLV info after clearing
  DEBUG ((DEBUG_INFO, "TLV Info after clearing:\n"));
  Status = TlvEeprom->ShowTlvInfo (TlvEeprom);
  DEBUG ((DEBUG_INFO, "==============================================\n"));

  DEBUG ((DEBUG_INFO, "TLV EEPROM protocol interface tests completed\n"));

  return EFI_SUCCESS;
}

#endif

/**
 * Set product name to environment variable
 */
EFI_STATUS
SetProductNameToEnvVar (
  IN  SPACEMIT_TLV_INFO_PROTOCOL  *This
  )
{
  EFI_STATUS  Status;
  CHAR8       ProductName[64];

  // Get product name from TLV EEPROM
  Status = GetTlvInfo (This, TLV_CODE_PRODUCT_NAME, ProductName, sizeof (ProductName));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to get product name: %r\n", Status));
    return Status;
  }

  // Set environment variable using our custom GUID - store as ASCII
  Status = gRT->SetVariable (
                             L"product_name",
                             &gSpacemitProductVariableGuid,
                             EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                             AsciiStrLen (ProductName) + 1, // Include null terminator
                             ProductName                    // Store directly as ASCII
                             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to set product_name environment variable: %r\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "TlvEeprom: Set product_name environment variable to: %a\n", ProductName));

  // Verify the variable was set correctly by reading it back
  CHAR8  ReadBackBuffer[64];
  UINTN  ReadBackSize = sizeof (ReadBackBuffer);

  Status = gRT->GetVariable (
                             L"product_name",
                             &gSpacemitProductVariableGuid,
                             NULL,
                             &ReadBackSize,
                             ReadBackBuffer
                             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to read back product_name variable: %r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "TlvEeprom: Read back product_name variable successfully, size: %d bytes\n", ReadBackSize));
    DEBUG ((DEBUG_INFO, "TlvEeprom: product_name value: %a\n", ReadBackBuffer));
  }

  return EFI_SUCCESS;
}

/**
 * TLV EEPROM driver entry point
 */
EFI_STATUS
EFIAPI
TlvEepromDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS         Status;
  TLV_INFO_INSTANCE  *TlvInfoInstance;

  TlvInfoInstance = AllocateZeroPool (sizeof (TLV_INFO_INSTANCE));
  if (TlvInfoInstance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  TlvInfoInstance->Signature                    = TLV_INFO_INSTANCE_SIGNATURE;
  TlvInfoInstance->HadReadTlvInfo               = FALSE;
  TlvInfoInstance->TlvInfoProtocol.GetTlvInfo   = GetTlvInfo;
  TlvInfoInstance->TlvInfoProtocol.SetTlvInfo   = SetTlvInfo;
  TlvInfoInstance->TlvInfoProtocol.FlushTlvInfo = FlushTlvInfo;
  TlvInfoInstance->TlvInfoProtocol.ClearTlvInfo = ClearTlvInfo;
  TlvInfoInstance->TlvInfoProtocol.ShowTlvInfo  = ShowTlvInfo;
  TlvInfoInstance->TlvInfoProtocol.DumpTlvInfo  = DumpTlvInfo;

  // Locate EEPROM protocol
  Status = gBS->LocateProtocol (
                                &gSpacemitEepromProtocolGuid,
                                NULL,
                                (VOID **)&TlvInfoInstance->EepromProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to locate EEPROM protocol: %r\n", Status));
    FreePool (TlvInfoInstance);
    return Status;
  }

  // Install TLV EEPROM protocol
  Status = gBS->InstallProtocolInterface (
                                          &TlvInfoInstance->TlvHandle,
                                          &gSpacemitTlvInfoProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          &TlvInfoInstance->TlvInfoProtocol
                                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to install protocol: %r\n", Status));
    FreePool (TlvInfoInstance);
    return Status;
  }

 #ifdef TEST_TLV_EEPROM_PROTOCOL
  TestTlvEepromProtocol (&TlvInfoInstance->TlvInfoProtocol);
 #endif

  // Set product name to environment variable
  Status = SetProductNameToEnvVar (&TlvInfoInstance->TlvInfoProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to set product name to environment variable: %r\n", Status));
  }

  DEBUG ((DEBUG_INFO, "TlvEeprom: Driver initialized successfully\n"));
  return EFI_SUCCESS;
}

/**
 * TLV EEPROM driver unload function
 */
EFI_STATUS
EFIAPI
TlvEepromDriverUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                  Status;
  SPACEMIT_TLV_INFO_PROTOCOL  *TlvInfoProtocol;
  TLV_INFO_INSTANCE           *TlvInfoInstance;

  // Uninstall TLV EEPROM protocol
  Status = gBS->LocateProtocol (
                                &gSpacemitTlvInfoProtocolGuid,
                                NULL,
                                (VOID **)&TlvInfoProtocol
                                );
  if (!EFI_ERROR (Status) && (TlvInfoProtocol != NULL)) {
    TlvInfoInstance = TLV_INFO_INSTANCE_FROM_THIS (TlvInfoProtocol);
    Status          = gBS->UninstallMultipleProtocolInterfaces (
                                                                TlvInfoInstance->TlvHandle,
                                                                &gSpacemitTlvInfoProtocolGuid,
                                                                &TlvInfoProtocol
                                                                );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "TlvEeprom: Failed to uninstall protocol: %r\n", Status));
      return Status;
    }
  }

  // Clean up resources
  FreePool (TlvInfoInstance);

  DEBUG ((DEBUG_INFO, "TlvEeprom: Driver unloaded successfully\n"));
  return EFI_SUCCESS;
}
