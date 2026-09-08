/********************************************************************************
Copyright (C) 2016 Marvell International Ltd.
Copyright (c) 2024, Spacemit Co., Ltd. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/

#ifndef __SPACEMIT_EEPROM_H__
#define __SPACEMIT_EEPROM_H__

#include <Uefi.h>
#include "EepromPcdConfig.h"

#define EEPROM_SIGNATURE  SIGNATURE_32 ('S', 'E', 'E', 'P')

#define I2C_DEVICE_INDEX(Bus, Address)   (((Bus) << 8) | (Address))
#define I2C_DEVICE_ADDRESS(DeviceIndex)  ((DeviceIndex) & 0xFF)

#define EEPROM_SC_FROM_EEPROM(a)  CR (a, EEPROM_CONTEXT, EepromProtocol, EEPROM_SIGNATURE)

// Add PCD declarations
#define PcdEepromAddressWidth      FixedPcdGetPtr(PcdEepromAddressWidth)
#define PcdEepromAddressWidthSize  FixedPcdGetSize(PcdEepromAddressWidth)
#define PcdEepromPageSize          FixedPcdGetPtr(PcdEepromPageSize)

typedef struct _SPACEMIT_EEPROM_PROTOCOL SPACEMIT_EEPROM_PROTOCOL;

struct _SPACEMIT_EEPROM_PROTOCOL {
  EFI_STATUS (EFIAPI *Transfer)(
                                IN CONST SPACEMIT_EEPROM_PROTOCOL  *This,
                                IN UINT16                          Address,
                                IN UINT32                          Length,
                                IN UINT8                           *Buffer,
                                IN UINT8                           Operation
                                );
  UINT32    Identifier;
};

typedef struct {
  UINT32                      Signature;
  EFI_HANDLE                  ControllerHandle;
  EFI_I2C_IO_PROTOCOL         *I2cIo;
  SPACEMIT_EEPROM_PROTOCOL    EepromProtocol;
} EEPROM_CONTEXT;

EFI_STATUS
EFIAPI
SpacemitEepromTransfer (
  IN CONST SPACEMIT_EEPROM_PROTOCOL  *This,
  IN UINT16                          Address,
  IN UINT32                          Length,
  IN UINT8                           *Buffer,
  IN UINT8                           Operation
  );

EFI_STATUS
EFIAPI
SpacemitEepromInitialise (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

EFI_STATUS
EFIAPI
SpacemitEepromSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
SpacemitEepromStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
SpacemitEepromStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  );

#endif // __SPACEMIT_EEPROM_H__
