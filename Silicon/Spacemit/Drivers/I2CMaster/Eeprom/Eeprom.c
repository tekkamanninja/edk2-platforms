/********************************************************************************
Copyright (C) 2024 Spacemit Ltd.

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/

#include <Protocol/DriverBinding.h>
#include <Protocol/I2cIo.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Pi/PiI2c.h>

#include "Eeprom.h"

#define EEPROM_WRITE_DELAY  10000 // 10ms delay for write cycle

EFI_DRIVER_BINDING_PROTOCOL  gDriverBindingProtocol = {
  SpacemitEepromSupported,
  SpacemitEepromStart,
  SpacemitEepromStop
};

EFI_STATUS
EFIAPI
SpacemitEepromSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS           Status;
  EFI_I2C_IO_PROTOCOL  *TmpI2cIo;
  EEPROM_CONFIG_ARRAY  *EepromConfigs;
  UINTN                I;

  if ((This == NULL) || (ControllerHandle == NULL)) {
    DEBUG ((DEBUG_ERROR, "SpacemitEepromSupported: Invalid parameters\n"));
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->HandleProtocol (
                                ControllerHandle,
                                &gEfiI2cIoProtocolGuid,
                                (VOID **)&TmpI2cIo
                                );

  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  // Get EEPROM configuration from PCD
  EepromConfigs = (EEPROM_CONFIG_ARRAY *)PcdGetPtr (PcdEepromConfigs);

  if ((EepromConfigs == NULL) || (EepromConfigs->Num == 0)) {
    DEBUG ((DEBUG_ERROR, "SpacemitEepromSupported: Invalid EEPROM configuration in PCD\n"));
    return EFI_UNSUPPORTED;
  }

  // Check if this is a supported EEPROM device
  for (I = 0; I < EepromConfigs->Num; I++) {
    if (TmpI2cIo->DeviceIndex == EepromConfigs->Data[I].SlaveAddress) {
      Status = gBS->OpenProtocol (
                                  ControllerHandle,
                                  &gEfiI2cIoProtocolGuid,
                                  (VOID **)&TmpI2cIo,
                                  gImageHandle,
                                  ControllerHandle,
                                  EFI_OPEN_PROTOCOL_BY_DRIVER
                                  );

      if (!EFI_ERROR (Status)) {
        Status = gBS->CloseProtocol (
                                     ControllerHandle,
                                     &gEfiI2cIoProtocolGuid,
                                     gImageHandle,
                                     ControllerHandle
                                     );
      }

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "SpacemitEepromSupported: Protocol test failed\n"));
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;
    }
  }

  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
SpacemitEepromTransfer (
  IN CONST SPACEMIT_EEPROM_PROTOCOL  *This,
  IN UINT16                          Address,
  IN UINT32                          Length,
  IN UINT8                           *Buffer,
  IN UINT8                           Operation
  )
{
  EFI_I2C_REQUEST_PACKET  *RequestPacket;
  UINTN                   RequestPacketSize;
  EFI_STATUS              Status         = EFI_SUCCESS;
  EEPROM_CONTEXT          *EepromContext = EEPROM_SC_FROM_EEPROM (This);
  EEPROM_CONFIG_ARRAY     *EepromConfigs;
  UINT8                   AddrWidth;
  UINT8                   PageSize;
  UINTN                   I;

  if ((Buffer == NULL) || (Length == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  ASSERT (EepromContext != NULL);
  ASSERT (EepromContext->I2cIo != NULL);

  // Get EEPROM configuration
  EepromConfigs = (EEPROM_CONFIG_ARRAY *)PcdGetPtr (PcdEepromConfigs);

  if ((EepromConfigs == NULL) || (EepromConfigs->Num == 0)) {
    DEBUG ((DEBUG_ERROR, "SpacemitEepromTransfer: Invalid EEPROM configuration in PCD\n"));
    return EFI_DEVICE_ERROR;
  }

  // Get configuration for current device index
  AddrWidth = 1;  // Default value
  PageSize  = 8;  // Default value
  for (I = 0; I < EepromConfigs->Num; I++) {
    if (EepromContext->I2cIo->DeviceIndex == EepromConfigs->Data[I].SlaveAddress) {
      AddrWidth = EepromConfigs->Data[I].AddressWidth;
      PageSize  = EepromConfigs->Data[I].PageSize;
      break;
    }
  }

  RequestPacketSize = sizeof (UINTN) + sizeof (EFI_I2C_OPERATION) * 2;
  RequestPacket     = AllocateZeroPool (RequestPacketSize);
  if (RequestPacket == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  /* First operation contains address, the second is buffer */
  RequestPacket->OperationCount             = 2;
  RequestPacket->Operation[0].LengthInBytes = AddrWidth;  // Use configured address width
  RequestPacket->Operation[0].Buffer        = AllocateZeroPool (AddrWidth);
  if (RequestPacket->Operation[0].Buffer == NULL) {
    FreePool (RequestPacket);
    return EFI_OUT_OF_RESOURCES;
  }

  if (Operation != I2C_FLAG_READ) {
    // Write operation
    UINTN  RemainingLength = Length;
    UINTN  CurrentAddress  = Address;
    UINTN  CurrentOffset   = 0;

    while (RemainingLength > 0) {
      // Calculate the number of bytes that can be written in the current page
      UINTN  CurrentPage   = CurrentAddress / PageSize;
      UINTN  NextPageStart = (CurrentPage + 1) * PageSize;
      UINTN  BytesInPage   = MIN (RemainingLength, NextPageStart - CurrentAddress);

      // Set write address
      RequestPacket->Operation[0].Flags         = 0;
      RequestPacket->Operation[0].LengthInBytes = AddrWidth;
      if (AddrWidth == 2) {
        RequestPacket->Operation[0].Buffer[0] = (UINT8)(CurrentAddress >> 8);
        RequestPacket->Operation[0].Buffer[1] = (UINT8)(CurrentAddress & 0xFF);
      } else {
        RequestPacket->Operation[0].Buffer[0] = (UINT8)CurrentAddress;
      }

      // Set write data
      RequestPacket->Operation[1].Flags         = 0;
      RequestPacket->Operation[1].LengthInBytes = BytesInPage;
      RequestPacket->Operation[1].Buffer        = Buffer + CurrentOffset;

      Status = EepromContext->I2cIo->QueueRequest (
                                                   EepromContext->I2cIo,
                                                   0,
                                                   NULL,
                                                   RequestPacket,
                                                   NULL
                                                   );

      if (EFI_ERROR (Status)) {
        DEBUG (
               (DEBUG_ERROR, "SpacemitEepromTransfer: Write failed at address 0x%x: %r\n",
                CurrentAddress, Status)
               );
        break;
      }

      // Wait for write cycle to complete
      gBS->Stall (EEPROM_WRITE_DELAY);

      // Update counters
      CurrentAddress  += BytesInPage;
      CurrentOffset   += BytesInPage;
      RemainingLength -= BytesInPage;
    }
  } else {
    // Read operation
    if (AddrWidth == 2) {
      RequestPacket->Operation[0].Buffer[0] = (UINT8)(Address >> 8);
      RequestPacket->Operation[0].Buffer[1] = (UINT8)(Address & 0xFF);
    } else {
      RequestPacket->Operation[0].Buffer[0] = (UINT8)Address;
    }

    RequestPacket->Operation[1].LengthInBytes = Length;
    RequestPacket->Operation[1].Buffer        = Buffer;
    RequestPacket->Operation[1].Flags         = I2C_FLAG_READ;

    Status = EepromContext->I2cIo->QueueRequest (
                                                 EepromContext->I2cIo,
                                                 0,
                                                 NULL,
                                                 RequestPacket,
                                                 NULL
                                                 );
  }

  FreePool (RequestPacket->Operation[0].Buffer);
  FreePool (RequestPacket);
  return Status;
}

EFI_STATUS
EFIAPI
SpacemitEepromStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS      Status = EFI_SUCCESS;
  EEPROM_CONTEXT  *EepromContext;

  EepromContext = AllocateZeroPool (sizeof (EEPROM_CONTEXT));
  if (EepromContext == NULL) {
    DEBUG ((DEBUG_ERROR, "SpacemitEeprom: allocation fail\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  EepromContext->ControllerHandle        = ControllerHandle;
  EepromContext->Signature               = EEPROM_SIGNATURE;
  EepromContext->EepromProtocol.Transfer = SpacemitEepromTransfer;

  Status = gBS->OpenProtocol (
                              ControllerHandle,
                              &gEfiI2cIoProtocolGuid,
                              (VOID **)&EepromContext->I2cIo,
                              gImageHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_BY_DRIVER
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SpacemitEeprom: failed to open I2cIo\n"));
    FreePool (EepromContext);
    return Status;
  }

  EepromContext->EepromProtocol.Identifier = EepromContext->I2cIo->DeviceIndex;
  Status                                   = gBS->InstallMultipleProtocolInterfaces (
                                                                                     &ControllerHandle,
                                                                                     &gSpacemitEepromProtocolGuid,
                                                                                     &EepromContext->EepromProtocol,
                                                                                     NULL
                                                                                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SpacemitEeprom: failed to install EEPROM protocol\n"));
    goto fail;
  }

  DEBUG (
         (DEBUG_INFO, "SpacemitEeprom: Successfully initialized EEPROM at address 0x%x\n",
          I2C_DEVICE_ADDRESS (EepromContext->I2cIo->DeviceIndex))
         );
  return Status;

fail:
  FreePool (EepromContext);
  gBS->CloseProtocol (
                      ControllerHandle,
                      &gEfiI2cIoProtocolGuid,
                      gImageHandle,
                      ControllerHandle
                      );

  return Status;
}

EFI_STATUS
EFIAPI
SpacemitEepromStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                  ControllerHandle,
  IN  UINTN                       NumberOfChildren,
  IN  EFI_HANDLE                  *ChildHandleBuffer OPTIONAL
  )
{
  SPACEMIT_EEPROM_PROTOCOL  *EepromProtocol;
  EFI_STATUS                Status;
  EEPROM_CONTEXT            *EepromContext;

  Status = gBS->OpenProtocol (
                              ControllerHandle,
                              &gSpacemitEepromProtocolGuid,
                              (VOID **)&EepromProtocol,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL
                              );

  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  EepromContext = EEPROM_SC_FROM_EEPROM (EepromProtocol);

  gBS->UninstallMultipleProtocolInterfaces (
                                            &ControllerHandle,
                                            &gSpacemitEepromProtocolGuid,
                                            &EepromContext->EepromProtocol,
                                            NULL
                                            );
  gBS->CloseProtocol (
                      ControllerHandle,
                      &gEfiI2cIoProtocolGuid,
                      gImageHandle,
                      ControllerHandle
                      );
  FreePool (EepromContext);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpacemitEepromInitialise (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallMultipleProtocolInterfaces (
                                                   &ImageHandle,
                                                   &gEfiDriverBindingProtocolGuid,
                                                   &gDriverBindingProtocol,
                                                   NULL
                                                   );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "EEPROM: Failed to install driver binding protocol: %r\n", Status));
  }

  return Status;
}
