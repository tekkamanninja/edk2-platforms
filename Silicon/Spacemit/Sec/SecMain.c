/** @file
  RISC-V SEC phase module.

  Copyright (c) 2008 - 2023, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2022, Ventana Micro Systems Inc. All rights reserved.<BR>
  Copyright (c) 2023, Academy of Intelligent Innovation, Shandong Universiy, China.P.R. All rights reserved.<BR>
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseRiscVSbiLib.h>
#include <Library/PrePiLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/SerialPortLib.h>

#include <Library/SpacemitSecLib.h>
#include "SecMain.h"

/**
  Initialize the memory and CPU, setting the boot mode, and platform
  initialization. It also builds the core information HOB.

  @return EFI_SUCCESS     Status.
**/
STATIC
EFI_STATUS
EFIAPI
SecInitializePlatform (
  IN  VOID  *DeviceTreeAddress
  )
{
  EFI_STATUS  Status;

  MemoryPeimInitialization (DeviceTreeAddress);

  CpuPeimInitialization (DeviceTreeAddress);

  // Set the Boot Mode
  SetBootMode (BOOT_WITH_FULL_CONFIGURATION);

  Status = PlatformPeimInitialization (DeviceTreeAddress);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  Entry point to the C language phase of SEC. After the SEC assembly
  code has initialized some temporary memory and set up the stack,
  the control is transferred to this function.

  @param[in]  BootHartId         Hardware thread ID of boot hart.
  @param[in]  DeviceTreeAddress  Pointer to Device Tree (DTB)
**/
VOID
NORETURN
EFIAPI
SecStartup (
  IN  UINTN  BootHartId,
  IN  VOID   *DeviceTreeAddress
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE  *HobList;
  EFI_RISCV_FIRMWARE_CONTEXT  FirmwareContext;
  EFI_STATUS                  Status;
  UINT64                      UefiMemoryBase;
  UINT64                      UefiMemorySize;
  UINT64                      StackBase;
  UINT64                      StackSize;

  SerialPortInitialize ();

  DEBUG ((DEBUG_ERROR, ">>>>>>>> UEFI Boot Start <<<<<<<<\n"));

  //
  // Report Status Code to indicate entering SEC core
  //
  DEBUG ((
    DEBUG_INFO,
    "%a() BootHartId: 0x%x, DeviceTreeAddress=0x%x\n",
    __func__,
    BootHartId,
    DeviceTreeAddress
    ));

  FirmwareContext.BootHartId          = BootHartId;
  SetFirmwareContextPointer (&FirmwareContext);

  StackBase      = FixedPcdGet64 (PcdSecStackBase);
  StackSize      = FixedPcdGet64 (PcdSecStackSize);
  UefiMemorySize = FixedPcdGet64 (PcdSecUefiMemorySize);
  UefiMemoryBase = StackBase + StackSize - UefiMemorySize;

  // Declare the PI/UEFI memory region
  HobList = HobConstructor (
              (VOID *)UefiMemoryBase,
              UefiMemorySize,
              (VOID *)UefiMemoryBase,
              (VOID *)StackBase // The top of the UEFI Memory is reserved for the stacks
              );
  PrePeiSetHobList (HobList);

  SecInitializePlatform (DeviceTreeAddress);

  BuildStackHob (StackBase, StackSize);

  //
  // Process all libraries constructor function linked to SecMain.
  //
  ProcessLibraryConstructorList ();

  // Assume the FV that contains the SEC (our code) also contains a compressed FV.
  Status = DecompressFirstFv ();
  ASSERT_EFI_ERROR (Status);

  // Load the DXE Core and transfer control to it
  Status = LoadDxeCoreFromFv (NULL, 0);
  ASSERT_EFI_ERROR (Status);
  //
  // Should not come here.
  //
  UNREACHABLE ();
}
