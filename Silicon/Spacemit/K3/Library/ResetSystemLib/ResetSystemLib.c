/** @file
  Reset System Library functions for Spacemit platforms

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2025, Spacemit Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiMultiPhase.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>

#include <Library/MemoryManagementLib.h>

#define K3_SOC_WDT_BASE  (FixedPcdGet64(PcdSpacemitWDTRegBase))
#define K3_MPMU_BASE     (FixedPcdGet64(PcdSpacemitMPMURegBase))

/* Application Clock Gating Register */
#define PMU_WDTCP_CTRL  (K3_MPMU_BASE + 0x0200)
#define PMU_ACER        (K3_MPMU_BASE + 0x1020)
#define PMU_ACGR        (K3_MPMU_BASE + 0x1024)

STATIC
VOID
WdtWriteAccess (
  VOID
  )
{
  MmioWrite32 (K3_SOC_WDT_BASE + 0x00b0, 0xbaba);
  MmioWrite32 (K3_SOC_WDT_BASE + 0x00b4, 0xeb10);
}

STATIC
VOID
WdtRegWrite (
  UINT32  Val,
  UINT64  Reg
  )
{
  WdtWriteAccess ();
  MmioWrite32 (Reg, Val);
}

VOID
WatchDogReset (
  VOID
  )
{
  UINT32  Value;

  MapRegToGcdMmioSpace (K3_SOC_WDT_BASE, SIZE_4KB);
  MapRegToGcdMmioSpace (K3_MPMU_BASE, SIZE_4KB * 3);

  /*enable wdt clk & reset*/
  Value = MmioRead32 (PMU_ACGR);
  MmioWrite32 (PMU_ACGR, Value | BIT19);

  Value = MmioRead32 (PMU_WDTCP_CTRL);
  MmioWrite32 (PMU_WDTCP_CTRL, 0x3 | Value);
  Value = MmioRead32 (PMU_WDTCP_CTRL);
  MmioWrite32 (PMU_WDTCP_CTRL, (~BIT2) & Value);

  // switch to reset SOC internally, 0: reset by PMIC, 1: reset by watchdog
  MmioAnd32 (K3_MPMU_BASE + 0x2058, ~BIT14);

  /*set watch dog*/
  WdtRegWrite (0x0, K3_SOC_WDT_BASE + 0xc0);
  // set watchdog timeout counter
  WdtRegWrite (10, K3_SOC_WDT_BASE + 0xbc);
  WdtRegWrite (0x3, K3_SOC_WDT_BASE + 0xb8);
  WdtRegWrite (0x1, K3_SOC_WDT_BASE + 0xc8);

  Value = MmioRead32 (PMU_ACER);
  MmioWrite32 (PMU_ACER, BIT4 | Value);
}

/**
  Calling this function causes a system-wide reset. This sets
  all circuitry within the system to its initial state. This type of reset
  is asynchronous to system operation and operates without regard to
  cycle boundaries.

  System reset should not return, if it returns, it means the system does
  not support cold reset.
**/
VOID
EFIAPI
ResetCold (
  VOID
  )
{
  WatchDogReset ();
  CpuDeadLoop ();
}

/**
  Calling this function causes a system-wide initialization. The processors
  are set to their initial state, and pending cycles are not corrupted.

  System reset should not return, if it returns, it means the system does
  not support warm reset.
**/
VOID
EFIAPI
ResetWarm (
  VOID
  )
{
  WatchDogReset ();
  CpuDeadLoop ();
}

/**
  Calling this function causes the system to enter a power state equivalent
  to the ACPI G2/S5 or G3 states.

  System shutdown should not return, if it returns, it means the system does
  not support shut down reset.
**/
VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  ASSERT (FALSE);
}

/**
  This function causes a systemwide reset. The exact type of the reset is
  defined by the EFI_GUID that follows the Null-terminated Unicode string passed
  into ResetData. If the platform does not recognize the EFI_GUID in ResetData
  the platform must pick a supported reset type to perform.The platform may
  optionally log the parameters from any non-normal reset that occurs.

  @param[in]  DataSize   The size, in bytes, of ResetData.
  @param[in]  ResetData  The data buffer starts with a Null-terminated string,
                         followed by the EFI_GUID.
**/
VOID
EFIAPI
ResetPlatformSpecific (
  IN UINTN  DataSize,
  IN VOID   *ResetData
  )
{
  ResetCold ();
}

/**
  The ResetSystem function resets the entire platform.

  @param[in] ResetType      The type of reset to perform.
  @param[in] ResetStatus    The status code for the reset.
  @param[in] DataSize       The size, in bytes, of ResetData.
  @param[in] ResetData      For a ResetType of EfiResetCold, EfiResetWarm, or EfiResetShutdown
                            the data buffer starts with a Null-terminated string, optionally
                            followed by additional binary data. The string is a description
                            that the caller may use to further indicate the reason for the
                            system reset.
**/
VOID
EFIAPI
ResetSystem (
  IN EFI_RESET_TYPE  ResetType,
  IN EFI_STATUS      ResetStatus,
  IN UINTN           DataSize,
  IN VOID            *ResetData OPTIONAL
  )
{
  switch (ResetType) {
    case EfiResetWarm:
      ResetWarm ();
      break;

    case EfiResetCold:
      ResetCold ();
      break;

    case EfiResetShutdown:
      ResetShutdown ();
      return;

    case EfiResetPlatformSpecific:
      ResetPlatformSpecific (DataSize, ResetData);
      return;

    default:
      return;
  }
}
