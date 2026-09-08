/** @file
 *  Spacemit K3 silicon PLL driver implementation.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>

#include <ClockDxe.h>

STATIC UINT32  PLLCLockValue[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

STATIC CONST UINT32  PllSettings[][4] = {
  // pll value(Mhz), PLL_REG1~4, PLL_REG0(bit8~bit15), PLL_REG5~8
  { 1850, 0x0b4d0555, 0x00005500, 0xa0458585 },
  { 2400, 0x0b320000, 0x00000000, 0xa0558989 },
  { 2600, 0x0b360aaa, 0x0000ab00, 0xa0558a8a },
};

STATIC CONST UINT32  PllSwCtrlReg[][2] = {
  // PLL control register address, PLL value(Mhz)
  { PLL1_SW_CTRL_REG, 2458 },
  { PLL2_SW_CTRL_REG, 3000 },
  { PLL3_SW_CTRL_REG, 2200 },
  { PLL4_SW_CTRL_REG, 2200 },
  { PLL5_SW_CTRL_REG, 2000 },
  { PLL6_SW_CTRL_REG, 3200 },
  { PLL7_SW_CTRL_REG, 2800 },
  { PLL8_SW_CTRL_REG, 2000 }
};

STATIC
EFI_STATUS
SetPllClockRate (
  UINT32  PllId,
  UINT64  FreqHz
  )
{
  UINT32  I, J, FreqMhz;

  I = PllId - 1;
  if (I < ARRAY_SIZE (PllSwCtrlReg)) {
    if (0 == FreqHz) {
      FreqMhz = PllSwCtrlReg[I][1];
    } else {
      FreqMhz = FreqHz / 1000000;
    }

    PLLCLockValue[I] = FreqMhz;
    if (FreqMhz != PllSwCtrlReg[I][1]) {
      for (J = 0; J < ARRAY_SIZE (PllSettings); J++) {
        if (PllSettings[J][0] == FreqMhz) {
          MmioWrite32 (PllSwCtrlReg[I][0] - 4, PllSettings[J][1]);
          MmioAndThenOr32 (PllSwCtrlReg[I][0], ~0xFF00, PllSettings[J][2]);
          MmioWrite32 (PllSwCtrlReg[I][0] + 4, PllSettings[J][3]);
          break;
        }
      }

      if (J == ARRAY_SIZE (PllSettings)) {
        DEBUG ((DEBUG_ERROR, "Unsupported PLL frequency %dMHz\n", FreqMhz));
        PLLCLockValue[I] = 0;
        return EFI_UNSUPPORTED;
      }
    }

    // start frequency change
    MmioOr32 (PllSwCtrlReg[I][0], BIT16);

    while (0 == (MmioRead32 (PLL_XO_STATUS_REG) & (1 << (24 + I)))) {
      // wait for PLL locked
    }

    // enable PLL DIV1~DIV8, DIV10~DIV13
    MmioOr32 (PllSwCtrlReg[I][0], 0xFF | BIT21);
  }

  DEBUG ((DEBUG_INFO, "enable PLL%d with frequency %dMHz\n", PllId, FreqMhz));
  return EFI_SUCCESS;

  return EFI_INVALID_PARAMETER;
}

STATIC
EFI_STATUS
GetPllCurrentClockRate (
  IN  UINT32  PllId,
  OUT UINT64  *ClockRate
  )
{
  UINT32  I = PllId - 1;

  if (I < ARRAY_SIZE (PLLCLockValue)) {
    *ClockRate = (UINT64)PLLCLockValue[I] * 1000000;
    DEBUG ((DEBUG_INFO, "PLL%d clock rate: %dMHz\n", PllId, PLLCLockValue[I]));
    return EFI_SUCCESS;
  } else {
    *ClockRate = 0;
    DEBUG ((DEBUG_ERROR, "Invalid PLL ID %d\n", PllId));
    return EFI_INVALID_PARAMETER;
  }
}

STATIC
EFI_STATUS
SetPll1ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetPllClockRate (1, ClockRate);
}

STATIC
EFI_STATUS
GetPll1CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetPllCurrentClockRate (1, ClockRate);
}

STATIC
EFI_STATUS
SetPll2ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetPllClockRate (2, ClockRate);
}

STATIC
EFI_STATUS
GetPll2CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetPllCurrentClockRate (2, ClockRate);
}

STATIC
EFI_STATUS
SetPll3ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetPllClockRate (3, ClockRate);
}

STATIC
EFI_STATUS
GetPll3CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetPllCurrentClockRate (3, ClockRate);
}

STATIC
EFI_STATUS
SetPll4ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetPllClockRate (4, ClockRate);
}

STATIC
EFI_STATUS
GetPll4CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetPllCurrentClockRate (4, ClockRate);
}

STATIC
EFI_STATUS
SetPll5ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetPllClockRate (5, ClockRate);
}

STATIC
EFI_STATUS
GetPll5CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetPllCurrentClockRate (5, ClockRate);
}

STATIC
EFI_STATUS
SetPll6ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetPllClockRate (6, ClockRate);
}

STATIC
EFI_STATUS
GetPll6CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetPllCurrentClockRate (6, ClockRate);
}

STATIC
EFI_STATUS
SetPll7ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetPllClockRate (7, ClockRate);
}

STATIC
EFI_STATUS
GetPll7CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetPllCurrentClockRate (7, ClockRate);
}

STATIC
EFI_STATUS
SetPll8ClockRate (
  IN UINT64  ClockRate
  )
{
  return SetPllClockRate (8, ClockRate);
}

STATIC
EFI_STATUS
GetPll8CurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetPllCurrentClockRate (8, ClockRate);
}

STATIC
EFI_STATUS
GetPllMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  // 3.2GHz
  *ClockRate = (UINT64)3200 * 1000000;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetPllMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  // 1GHz
  *ClockRate = (UINT64)1000 * 1000000;
  return EFI_SUCCESS;
}

CLOCK_RATE_OPERATIONS  Pll1ClockRateOps = {
  GetPll1CurrentClockRate,
  GetPllMaxClockRate,
  GetPllMinClockRate,
  SetPll1ClockRate
};

CLOCK_RATE_OPERATIONS  Pll2ClockRateOps = {
  GetPll2CurrentClockRate,
  GetPllMaxClockRate,
  GetPllMinClockRate,
  SetPll2ClockRate
};

CLOCK_RATE_OPERATIONS  Pll3ClockRateOps = {
  GetPll3CurrentClockRate,
  GetPllMaxClockRate,
  GetPllMinClockRate,
  SetPll3ClockRate
};

CLOCK_RATE_OPERATIONS  Pll4ClockRateOps = {
  GetPll4CurrentClockRate,
  GetPllMaxClockRate,
  GetPllMinClockRate,
  SetPll4ClockRate
};

CLOCK_RATE_OPERATIONS  Pll5ClockRateOps = {
  GetPll5CurrentClockRate,
  GetPllMaxClockRate,
  GetPllMinClockRate,
  SetPll5ClockRate
};

CLOCK_RATE_OPERATIONS  Pll6ClockRateOps = {
  GetPll6CurrentClockRate,
  GetPllMaxClockRate,
  GetPllMinClockRate,
  SetPll6ClockRate
};

CLOCK_RATE_OPERATIONS  Pll7ClockRateOps = {
  GetPll7CurrentClockRate,
  GetPllMaxClockRate,
  GetPllMinClockRate,
  SetPll7ClockRate
};

CLOCK_RATE_OPERATIONS  Pll8ClockRateOps = {
  GetPll8CurrentClockRate,
  GetPllMaxClockRate,
  GetPllMinClockRate,
  SetPll8ClockRate
};
