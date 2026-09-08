/** @file
 *  Spacemit K3 silicon qspi clock driver implementation.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>

#include <ClockDxe.h>

STATIC UINT32  QspiClockSourceMhz[] = {
  409,          // PLL1_DIV6
  375,          // PLL2_DIV8
  307,          // PLL1_DIV8
  245,          // PLL1_DIV10
  0,
  0,
  491,          // PLL1_DIV5
  0,
};

UINT32
FindBestMatchFreq (
  IN UINT64  Freq,
  IN UINT32  *ClockTable,
  IN UINT32  MaxClockNum,
  IN UINT32  MaxClockDivision,
  IN UINT32  DefaultIndex
  )
{
  UINT32  I, J, Clock, ClockDiff, MinClockDiff, ClockSourceIndex = DefaultIndex;
  UINT32  FreqMhz;

  FreqMhz      = Freq / 1000000;
  MinClockDiff = FreqMhz * 10;

  for (I = 1; I <= MaxClockDivision; I++) {
    for (J = 0; J < MaxClockNum; J++) {
      if (0 == ClockTable[J]) {
        continue;
      }

      Clock = FreqMhz * I;
      if (Clock >= ClockTable[J]) {
        // times 10 to avoid float point calculation
        ClockDiff = (Clock - ClockTable[J]) * 10 / I;
        if (ClockDiff < MinClockDiff) {
          MinClockDiff     = ClockDiff;
          ClockSourceIndex = J;
          if (0 == MinClockDiff) {
            return ClockSourceIndex;
          }
        }
      }
    }
  }

  return ClockSourceIndex;
}

STATIC
EFI_STATUS
SetQspiClockRate (
  IN  UINT64  ClockRate
  )
{
  UINT32  ClkSel, ClkDiv, MaxClkDiv = 8;
  UINT64  ClkSource;

  // use 106MHz clock as default
  ClkSel = FindBestMatchFreq (
                              ClockRate,
                              QspiClockSourceMhz,
                              ARRAY_SIZE (QspiClockSourceMhz),
                              MaxClkDiv,
                              3
                              );
  ClkSource = (UINT64)QspiClockSourceMhz[ClkSel] * 1000000;
  ClkDiv    = MIN ((ClkSource + ClockRate - 1) / ClockRate, MaxClkDiv);
  DEBUG (
         (DEBUG_INFO, "QSPI clock source: %lluHz, real clock: %lluHz\n",
          ClkSource, ClkSource / ClkDiv)
         );

  if (1 == ClkSel) {
    // enable PLL2 with default value
    Pll2ClockRateOps.SetRate (0);
  } else {
    // enable PLL1 with default value
    Pll1ClockRateOps.SetRate (0);
    // If clock source is PLL1, enable more bit
    MmioOr32 (PMU_ACGR, BIT0 | BIT13 | BIT21);
  }

  // disable qspi clock
  MmioAnd32 (K3_QSPI_CLK_RES_CTRL, ~(BIT3 | BIT4));
  MmioAnd32 (K3_QSPI_CLK_RES_CTRL, ~(BIT0 | BIT1));

  MmioAndThenOr32 (
                   K3_QSPI_CLK_RES_CTRL,
                   ~((0x7 << 6) | (0x7 << 9)),
                   (ClkSel << 6) | ((ClkDiv - 1) << 9) | BIT12
                   );

  // enable and switch qspi clock
  MmioOr32 (K3_QSPI_CLK_RES_CTRL, BIT3 | BIT4);
  MmioOr32 (K3_QSPI_CLK_RES_CTRL, BIT0 | BIT1);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetQspiCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  UINT32  ClkSel, ClkDiv;
  UINT64  ClkSource;

  ClkSel     = (MmioRead32 (K3_QSPI_CLK_RES_CTRL) & (0x7 << 6)) >> 6;
  ClkDiv     = (MmioRead32 (K3_QSPI_CLK_RES_CTRL) & (0x7 << 9)) >> 9;
  ClkSource  = (UINT64)QspiClockSourceMhz[ClkSel] * 1000000;
  *ClockRate = ClkSource / (ClkDiv + 1);

  DEBUG (
         (DEBUG_INFO, "QSPI clock source: %lluHz, real clock: %lluHz\n",
          ClkSource, *ClockRate)
         );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetQspiMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  // QSPI max stable clock is 76.75MHz
  *ClockRate = (UINT64)QspiClockSourceMhz[2] * 1000000 / 4;     // 307MHz / 4
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetQspiMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  *ClockRate = (UINT64)QspiClockSourceMhz[3] * 1000000 / 8;        // 245MHz / 8
  return EFI_SUCCESS;
}

CLOCK_RATE_OPERATIONS  QspiClockRateOps = {
  GetQspiCurrentClockRate,
  GetQspiMaxClockRate,
  GetQspiMinClockRate,
  SetQspiClockRate
};
