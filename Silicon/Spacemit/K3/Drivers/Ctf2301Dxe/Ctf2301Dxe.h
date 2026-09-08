/** @file
  CTF2301 I2C PWM controller driver.

  Copyright (c) 2026, Spacemit Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _CTF2301_DXE_H_
#define _CTF2301_DXE_H_

#include <Uefi.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/I2cIo.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Pi/PiI2c.h>

#define CTF2301_REG_PWM_VALUE  0x4C
#define CTF2301_PWM_MAX_VALUE  0xFF

#define I2C_DEVICE_ADDRESS(DeviceIndex)  ((DeviceIndex) & 0xFF)

EFI_STATUS
EFIAPI
Ctf2301Supported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
Ctf2301Start (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
Ctf2301Stop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  );

EFI_STATUS
EFIAPI
Ctf2301EntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

#endif // _CTF2301_DXE_H_
