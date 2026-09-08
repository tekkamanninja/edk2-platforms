/** @file
  RISC-V IOMMU DXE driver

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/DmaIoMmuLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryManagementLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <Library/RiscVIoMmuConfig.h>
#include "RiscVIoMmuDxe.h"

STATIC
UINT64
RiscVIoMmuRegRead (
  IN CONST  RISCV_IOMMU_DEVICE    *Dev,
  IN        UINT32                Reg,
  IN        UINT32                Size
  )
{
  UINT64  Addr;

  Addr = Dev->RegBase + Reg;

  switch (Size) {
    case 4:
      return MmioRead32 (Addr);
    case 8:
      return MmioRead64 (Addr);
    default:
      DEBUG ((DEBUG_ERROR, "(%a) IOMMU %u: Invalid size: %u\n", __func__, Dev->Id, Size));
      return 0;
  }
}

STATIC
inline
UINT64
RiscVIoMmuRegRead32 (
  IN CONST  RISCV_IOMMU_DEVICE    *Dev,
  IN        UINT32                Reg
  )
{
  return RiscVIoMmuRegRead (Dev, Reg, 4);
}

STATIC
inline
UINT64
RiscVIoMmuRegRead64 (
  IN CONST  RISCV_IOMMU_DEVICE    *Dev,
  IN        UINT32                Reg
  )
{
  return RiscVIoMmuRegRead (Dev, Reg, 8);
}


STATIC
VOID
RiscVIoMmuRegWrite (
  IN CONST  RISCV_IOMMU_DEVICE    *Dev,
  IN        UINT32                Reg,
  IN        UINT32                Size,
  IN        UINT64                Value
  )
{
  UINT64  Addr;

  Addr = Dev->RegBase + Reg;

  switch (Size) {
    case 4:
      MmioWrite32 (Addr, Value);
      break;
    case 8:
      MmioWrite64 (Addr, Value);
      break;
    default:
      DEBUG ((DEBUG_ERROR, "(%a) IOMMU %u: Invalid size: %u\n", __func__, Dev->Id, Size));
      break;
  }
}

STATIC
inline
VOID
RiscVIoMmuRegWrite32 (
  IN CONST  RISCV_IOMMU_DEVICE    *Dev,
  IN        UINT32                Reg,
  IN        UINT64                Value
  )
{
  RiscVIoMmuRegWrite (Dev, Reg, 4, Value);
}

STATIC
inline
VOID
RiscVIoMmuRegWrite64 (
  IN CONST  RISCV_IOMMU_DEVICE    *Dev,
  IN        UINT32                Reg,
  IN        UINT64                Value
  )
{
  RiscVIoMmuRegWrite (Dev, Reg, 8, Value);
}

STATIC
EFI_STATUS
RiscVIoMmuSetDdtpMode (
  IN CONST  RISCV_IOMMU_DEVICE            *Dev,
  IN        RISCV_IOMMU_DDTP_IOMMU_MODE   Mode
  )
{
  UINT64                        DdtpRead;
  UINT64                        DdtpRequested;
  RISCV_IOMMU_DDTP_IOMMU_MODE   ModeRead;

  DdtpRead = RiscVIoMmuRegRead64 (Dev, RISCV_IOMMU_REG_DDTP);
  if (RISCV_IOMMU_DDTP_IS_BUSY (DdtpRead)) {
    // TODO: Retry to read in a timeout.
    DEBUG ((DEBUG_ERROR, "(%a) IOMMU %u: ddtp is busy\n", __func__, Dev->Id));
    return EFI_NOT_READY;
  }

  DdtpRequested = DdtpRead | Mode;
  RiscVIoMmuRegWrite64 (Dev, RISCV_IOMMU_REG_DDTP, DdtpRequested);

  DdtpRead = RiscVIoMmuRegRead64 (Dev, RISCV_IOMMU_REG_DDTP);
  if (RISCV_IOMMU_DDTP_IS_BUSY (DdtpRead)) {
    // TODO: Retry to read in a timeout.
    DEBUG ((DEBUG_ERROR, "(%a) IOMMU %u: ddtp is busy\n", __func__, Dev->Id));
    return EFI_NOT_READY;
  }

  ModeRead = DdtpRead & RISCV_IOMMU_REG_DDTP_IOMMU_MODE_MASK;
  if (ModeRead != Mode) {
    DEBUG ((DEBUG_ERROR,
            "(%a) IOMMU %u: Failed to set ddtp.iommu_mode (requested: %u, read: %u)\n",
            __func__, Dev->Id, Mode, ModeRead));
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
RiscVIoMmuDeviceInit (
  IN OUT    RISCV_IOMMU_DEVICE            *Dev,
  IN        UINT16                        Id,
  IN        RISCV_IOMMU_DDTP_IOMMU_MODE   Mode,
  IN CONST  RISCV_IOMMU_CONFIG_DATA       *Config
  )
{
  EFI_STATUS      Status;

  Dev->Id = Id;

  Dev->RegBase = Config->BaseAddress;
  Dev->RegSize = SIZE_4KB;
  MapRegToGcdMmioSpace (Dev->RegBase, Dev->RegSize);

  //
  // For now only the IOMMU mode Off or Bare is supported.
  //
  if (Mode > RISCV_IOMMU_DDTP_IOMMU_MODE_BARE) {
    DEBUG ((DEBUG_ERROR, "(%a) IOMMU %u: Invalid IOMMU mode in config: %u",
            __func__, Dev->Id, Mode));
    return EFI_UNSUPPORTED;
  }

  Status = RiscVIoMmuSetDdtpMode (Dev, Mode);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
RiscVIoMmuDxeInit (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                      Status;
  DMA_IOMMU_MODE                  DmaIoMmuMode;
  RISCV_IOMMU_DDTP_IOMMU_MODE     RiscVIoMmuMode;
  RISCV_IOMMU_CONFIGS             *RiscVIoMmuConfigs = NULL;
  RISCV_IOMMU_DEVICE              *RiscVIoMmuDevs = NULL;
  UINT16                          NumRiscVIoMmuDevs;
  UINTN                           Index;
  EFI_HANDLE                      Handle;

  DmaIoMmuMode = DmaIoMmuModeGet ();
  switch (DmaIoMmuMode) {
    case DmaIoMmuModeWithoutIoMmu:
    case DmaIoMmuModeWithSwIoMmu:
      RiscVIoMmuMode = RISCV_IOMMU_DDTP_IOMMU_MODE_BARE;
      break;
    case DmaIoMmuModeWithHwIoMmu:
      // For now DMA address translation with the RISC-V IOMMU hardware is not supported.
    default:
      DEBUG ((DEBUG_ERROR, "(%a) Not supported DMA IOMMU mode: %d\n", __func__, DmaIoMmuMode));
      Status = EFI_UNSUPPORTED;
      goto Out;
  }

  RiscVIoMmuConfigs = (RISCV_IOMMU_CONFIGS *) PcdGetPtr (PcdRiscVIoMmuConfigTable);
  if (RiscVIoMmuConfigs == NULL) {
    DEBUG ((DEBUG_ERROR, "(%a) RISC-V IOMMU configs not found\n", __func__));
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  if (RiscVIoMmuConfigs->Num == 0) {
    DEBUG ((DEBUG_ERROR, "(%a) Invalid RiscVIoMmuConfigs->Num: %u\n",
            __func__, RiscVIoMmuConfigs->Num));
    Status = EFI_UNSUPPORTED;
    goto Out;
  }
  NumRiscVIoMmuDevs = RiscVIoMmuConfigs->Num;

  RiscVIoMmuDevs = (RISCV_IOMMU_DEVICE *) AllocateZeroPool (
      sizeof (RISCV_IOMMU_DEVICE) * NumRiscVIoMmuDevs);
  if (RiscVIoMmuDevs == NULL) {
    DEBUG ((DEBUG_ERROR, "(%a) Failed to allocate RiscVIoMmuDevs\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Out;
  }

  for (Index = 0; Index < NumRiscVIoMmuDevs; Index++) {
    if (!RiscVIoMmuConfigs->Data[Index].IsEnabled) {
      continue;
    }

    Status = RiscVIoMmuDeviceInit (&RiscVIoMmuDevs[Index],
                                   Index,
                                   RiscVIoMmuMode,
                                   &RiscVIoMmuConfigs->Data[Index]);
    if (Status != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "(%a) Failed to init IOMMU device %u\n", __func__, Index));
      goto FreeRiscVIoMmuDevs;
    }
  }

  //
  // Now all enabled IOMMUs have been inited, install the RiscVIoMmuIsInited
  // protocol for the dependent modules to run.
  //
  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gRiscVIoMmuIsInitedProtocolGuid,
                  NULL,
                  NULL
                  );
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) Failed to install RiscVIoMmuIsInited protocol\n", __func__));
    goto FreeRiscVIoMmuDevs;
  }

  Status = EFI_SUCCESS;
  //
  // As this IOMMU DXE driver does nothing except setting the ddtp.iommu_mode
  // to Off or Bare, the RISCV_IOMMU_DEVICE instances are not needed any more
  // no matter in success or failure. So always free them.
  //
FreeRiscVIoMmuDevs:
  FreePool (RiscVIoMmuDevs);
Out:
  return Status;
}
