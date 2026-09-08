/** @file
 *  Spacemit K3 silicon SD Host Controller clock driver implementation.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>

#include <ClockDxe.h>

#define SDHC_MAX_CLOCK_DIVISION  (8)

// SDHC0 and SDHC1 share the same clock source table
STATIC UINT32  Sdhc0ClockSourceMhz[] = {
  409,          // PLL1_DIV6
  614,          // PLL1_DIV4
  375,          // PLL2_DIV8
  600,          // PLL1_DIV5
  0,
  0,
  106,          // PLL1_DIV23
};

// SDHC2 has different clock source table
STATIC UINT32  Sdhc2ClockSourceMhz[] = {
  409,          // PLL1_DIV6
  614,          // PLL1_DIV4
  375,          // PLL2_DIV8
  819,          // PLL1_DIV3
  0,
  0,
  106,          // PLL1_DIV23
};

STATIC
EFI_STATUS
SetSdhcClockRate (
  IN UINT64  ClockRate,
  IN UINTN   SdhcClockRegAddress,
  IN UINT32  *ClockTable
  )
{
  UINT32      ClkSel, ClkDiv;
  UINT64      ClkSource;
  EFI_STATUS  Status;

  // use 106MHz clock as default
  ClkSel = FindBestMatchFreq (
                              ClockRate,
                              ClockTable,
                              ARRAY_SIZE (Sdhc0ClockSourceMhz),
                              8,
                              6
                              );
  ClkSource = (UINT64)ClockTable[ClkSel] * 1000000;
  ClkDiv    = (ClkSource + ClockRate - 1) / ClockRate;
  DEBUG (
         (DEBUG_INFO, "SDHC clock source: %lluHz, real clock: %lluHz\n",
          ClkSource, ClkSource / ClkDiv)
         );

  MmioAndThenOr32 (
                   SdhcClockRegAddress,
                   ~((0x7 << 5) | (0x7 << 8)),
                   (ClkSel << 5) | ((ClkDiv - 1) << 8) | BIT11
                   );

  Status = PollRegStatus (
                          SdhcClockRegAddress,
                          BIT11,
                          0,
                          10 * 1000
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Set SDHC clock rate timeout!\n"));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetSdhcCurrentClockRate (
  OUT UINT64  *ClockRate,
  IN UINTN    SdhcClockRegAddress,
  IN UINT32   *ClockTable
  )
{
  UINT32  ClkSel, ClkDiv;
  UINT64  ClkSource;

  ClkSel     = (MmioRead32 (SdhcClockRegAddress) & (0x7 << 5)) >> 5;
  ClkDiv     = (MmioRead32 (SdhcClockRegAddress) & (0x7 << 8)) >> 8;
  ClkSource  = (UINT64)ClockTable[ClkSel] * 1000000;
  *ClockRate = ClkSource / (ClkDiv + 1);

  DEBUG (
         (DEBUG_INFO, "SDHC clock source: %lluHz, real clock: %lluHz\n",
          ClkSource, *ClockRate)
         );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetSdhc0ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetSdhcClockRate (
                           ClockRate,
                           K3_SDH0_CLK_RES_CTRL,
                           Sdhc0ClockSourceMhz
                           );
}

STATIC
EFI_STATUS
SetSdhc1ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetSdhcClockRate (
                           ClockRate,
                           K3_SDH1_CLK_RES_CTRL,
                           Sdhc0ClockSourceMhz
                           );
}

STATIC
EFI_STATUS
SetSdhc2ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetSdhcClockRate (
                           ClockRate,
                           K3_SDH2_CLK_RES_CTRL,
                           Sdhc2ClockSourceMhz
                           );
}

STATIC
EFI_STATUS
GetSdhc0CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetSdhcCurrentClockRate (
                                  ClockRate,
                                  K3_SDH0_CLK_RES_CTRL,
                                  Sdhc0ClockSourceMhz
                                  );
}

STATIC
EFI_STATUS
GetSdhc1CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetSdhcCurrentClockRate (
                                  ClockRate,
                                  K3_SDH1_CLK_RES_CTRL,
                                  Sdhc0ClockSourceMhz
                                  );
}

STATIC
EFI_STATUS
GetSdhc2CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetSdhcCurrentClockRate (
                                  ClockRate,
                                  K3_SDH2_CLK_RES_CTRL,
                                  Sdhc2ClockSourceMhz
                                  );
}

STATIC
EFI_STATUS
GetSdhc0MaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  *ClockRate = 614 * 1000000;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetSdhc2MaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  *ClockRate = 819 * 1000000;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetSdhcMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  *ClockRate = 106 * 1000000 / 8;
  return EFI_SUCCESS;
}

CLOCK_RATE_OPERATIONS  Sdhc0ClockRateOps = {
  GetSdhc0CurrentClockRate,
  GetSdhc0MaxClockRate,
  GetSdhcMinClockRate,
  SetSdhc0ClockRate
};

CLOCK_RATE_OPERATIONS  Sdhc1ClockRateOps = {
  GetSdhc1CurrentClockRate,
  GetSdhc0MaxClockRate,
  GetSdhcMinClockRate,
  SetSdhc1ClockRate
};

CLOCK_RATE_OPERATIONS  Sdhc2ClockRateOps = {
  GetSdhc2CurrentClockRate,
  GetSdhc2MaxClockRate,
  GetSdhcMinClockRate,
  SetSdhc2ClockRate
};
