/** @file
  CTF2301 I2C PWM controller DXE driver.

  Write the max PWM value (0xFF) to register 0x4C on bind to initialise
  the backlight controller for full brightness at boot.

  Copyright (c) 2026, Spacemit Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "Ctf2301Dxe.h"
#include "Ctf2301PcdConfig.h"

EFI_DRIVER_BINDING_PROTOCOL  gCtf2301DriverBinding = {
  Ctf2301Supported,
  Ctf2301Start,
  Ctf2301Stop,
  0x10,
  NULL,
  NULL
};

EFI_STATUS
EFIAPI
Ctf2301Supported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS            Status;
  EFI_I2C_IO_PROTOCOL   *I2cIo;
  CTF2301_CONFIG_ARRAY  *Configs;
  UINTN                 I;

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiI2cIoProtocolGuid,
                  (VOID **)&I2cIo
                  );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  Configs = (CTF2301_CONFIG_ARRAY *)PcdGetPtr (PcdCtf2301Configs);
  if ((Configs == NULL) || (Configs->Num == 0)) {
    return EFI_UNSUPPORTED;
  }

  for (I = 0; I < Configs->Num; I++) {
    if (I2C_DEVICE_ADDRESS (I2cIo->DeviceIndex) == Configs->Data[I].SlaveAddress) {
      Status = gBS->OpenProtocol (
                      ControllerHandle,
                      &gEfiI2cIoProtocolGuid,
                      (VOID **)&I2cIo,
                      This->DriverBindingHandle,
                      ControllerHandle,
                      EFI_OPEN_PROTOCOL_BY_DRIVER
                      );
      if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
      }

      gBS->CloseProtocol (
             ControllerHandle,
             &gEfiI2cIoProtocolGuid,
             This->DriverBindingHandle,
             ControllerHandle
             );
      return EFI_SUCCESS;
    }
  }

  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
Ctf2301Start (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS              Status;
  EFI_I2C_IO_PROTOCOL     *I2cIo;
  EFI_I2C_REQUEST_PACKET  *RequestPacket;
  UINTN                   RequestPacketSize;
  UINT8                   RegAddr;
  UINT8                   RegValue;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiI2cIoProtocolGuid,
                  (VOID **)&I2cIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: OpenProtocol I2cIo failed: %r\n", __func__, Status));
    return Status;
  }

  //
  // Write 0xFF to register 0x4C (PWM max duty).
  //
  RegAddr  = CTF2301_REG_PWM_VALUE;
  RegValue = CTF2301_PWM_MAX_VALUE;

  RequestPacketSize = sizeof (UINTN) + sizeof (EFI_I2C_OPERATION) * 2;
  RequestPacket     = AllocateZeroPool (RequestPacketSize);
  if (RequestPacket == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrCloseProto;
  }

  RequestPacket->Operation[0].Buffer = AllocateZeroPool (1);
  if (RequestPacket->Operation[0].Buffer == NULL) {
    FreePool (RequestPacket);
    return EFI_OUT_OF_RESOURCES;
  }

  RequestPacket->OperationCount             = 2;
  RequestPacket->Operation[0].Flags         = 0;
  RequestPacket->Operation[0].LengthInBytes = 1;
  RequestPacket->Operation[0].Buffer[0]     = RegAddr;
  RequestPacket->Operation[1].Flags         = 0;
  RequestPacket->Operation[1].LengthInBytes = 1;
  RequestPacket->Operation[1].Buffer        = &RegValue;

  Status = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);

  FreePool (RequestPacket->Operation[0].Buffer);
  FreePool (RequestPacket);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: I2C write failed: %r\n", __func__, Status));
    goto ErrCloseProto;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: CTF2301 initialised on addr 0x%02x\n",
    __func__,
    I2C_DEVICE_ADDRESS (I2cIo->DeviceIndex)
    ));

  return EFI_SUCCESS;

ErrCloseProto:
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiI2cIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );
  return Status;
}

EFI_STATUS
EFIAPI
Ctf2301Stop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiI2cIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Ctf2301EntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (PcdGetBool (PcdCtf2301Enable)) {
    return EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gCtf2301DriverBinding,
             ImageHandle,
             NULL,
             NULL
             );
  } else {
    return EFI_UNSUPPORTED;
  }
}
