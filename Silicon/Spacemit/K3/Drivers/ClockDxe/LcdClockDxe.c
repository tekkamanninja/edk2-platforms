/** @file
 *  Spacemit K3 silicon lcd clock driver implementation.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>

#include <ClockDxe.h>

// PLL clock operations from Pll.c
extern CLOCK_RATE_OPERATIONS  Pll1ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Pll2ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Pll3ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Pll4ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Pll5ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Pll6ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Pll7ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Pll8ClockRateOps;

typedef struct {
  CONST CHAR8     *Name;
  UINTN           ResetCtrlReg;
  UINTN           EnableCtrlReg;
  UINTN           FCEnableCtrlReg;
  UINTN           SelCtrlReg;
  UINTN           DivCtrlReg;

  UINT32          ResetBit;
  UINT32          EnableBit;
  UINT32          FCEnableBit;

  UINT32          ClkSelMask;
  UINT32          ClkSelShift;

  UINT32          ClkDivMask;
  UINT32          ClkDivShift;

  CONST UINT32    *ClockSourceTable;
  UINT32          ClockSourceCount;
  UINT32          DefaultClkIndex;

  UINT64          MinClock;
  UINT64          MaxClock;
  UINT32          ClkDiv;
} LCD_CLOCK_CONFIG;

STATIC UINT32  LcdEscClockSourceMhz[] = {
  52,
  48,
  26,
  78,
};

STATIC UINT32  LcdDscClockSourceMhz[] = {
  624,
  491,
  560,
  533,
  428,
  750,
  52,
};

STATIC UINT32  LcdPixClockSourceMhz[] = {
  624,    // PLL1_624M
  499,    // PLL1_499M
  560,    // PLL7_DIV5
  533,    // PLL6_DIV6
  428,    // PLL2_DIV7
  750,    // PLL2_DIV4
  52,     // CLK_52M
  375,    // PLL2_DIV8
};

STATIC CONST UINT32  LcdPixClockSourcePll[] = {
  1,      // PLL1_624M
  1,      // PLL1_499M
  7,      // PLL7_DIV5
  6,      // PLL6_DIV6
  2,      // PLL2_DIV7
  2,      // PLL2_DIV4
  0,      // CLK_52M (no PLL needed)
  2,      // PLL2_DIV8
};

STATIC UINT32  LcdMClockSourceMhz[] = {
  416,
  499,
  624,
  312,
};

STATIC UINT32  LcdAClkClockSourceMhz[] = {
  409,
  491,
  614,
  307,
  750,
};

/*
* dpu0
*/
LCD_CLOCK_CONFIG  LcdEscClockConfig = {
  .Name          = "Esc",
  .EnableCtrlReg = K3_LCD_CLK_RES_CTRL1,
  .SelCtrlReg    = K3_LCD_CLK_RES_CTRL1,
  .DivCtrlReg    = K3_LCD_CLK_RES_CTRL1,

  .EnableBit        = BIT2,

  .ClkSelMask  = 0x3,
  .ClkSelShift = 0,

  .ClkDivMask  = 0x0,
  .ClkDivShift = 0,

  .ClockSourceTable = LcdEscClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdEscClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 26000000,
  .MaxClock = 78000000,

  .ClkDiv           = 1
};

LCD_CLOCK_CONFIG  LcdDscClockConfig = {
  .Name            = "Dsc",
  .EnableCtrlReg   = K3_LCD_CLK_RES_CTRL2,
  .FCEnableCtrlReg = K3_LCD_CLK_RES_CTRL1,
  .SelCtrlReg      = K3_LCD_CLK_RES_CTRL2,
  .DivCtrlReg      = K3_LCD_CLK_RES_CTRL2,

  .EnableBit   = BIT14,
  .FCEnableBit = BIT26,

  .ClkSelMask  = 0x7,
  .ClkSelShift = 29,

  .ClkDivMask  = 0x7,
  .ClkDivShift = 25,

  .ClockSourceTable = LcdDscClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdDscClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 6500000,
  .MaxClock = 750000000,

  .ClkDiv           = 8
};

LCD_CLOCK_CONFIG  LcdPixClockConfig = {
  .Name            = "Pix",
  .EnableCtrlReg   = K3_LCD_CLK_RES_CTRL2,
  .FCEnableCtrlReg = K3_LCD_CLK_RES_CTRL1,
  .SelCtrlReg      = K3_LCD_CLK_RES_CTRL2,
  .DivCtrlReg      = K3_LCD_CLK_RES_CTRL2,

  .EnableBit   = BIT16,
  .FCEnableBit = BIT30,

  .ClkSelMask  = 0x7,
  .ClkSelShift = 21,

  .ClkDivMask  = 0x7,
  .ClkDivShift = 17,

  .ClockSourceTable = LcdPixClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdPixClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 6500000,
  .MaxClock = 750000000,

  .ClkDiv           = 8
};

LCD_CLOCK_CONFIG  LcdMclkClockConfig = {
  .Name            = "MClk",
  .EnableCtrlReg   = K3_LCD_CLK_RES_CTRL2,
  .FCEnableCtrlReg = K3_LCD_CLK_RES_CTRL1,
  .SelCtrlReg      = K3_LCD_CLK_RES_CTRL2,
  .DivCtrlReg      = K3_LCD_CLK_RES_CTRL2,

  .EnableBit   = BIT0,
  .FCEnableBit = BIT29,

  .ClkSelMask  = 0x7,
  .ClkSelShift = 5,

  .ClkDivMask  = 0xf,
  .ClkDivShift = 1,

  .ClockSourceTable = LcdMClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdMClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 39000000,
  .MaxClock = 624000000,

  .ClkDiv           = 16
};

LCD_CLOCK_CONFIG  LcdAclkClockConfig = {
  .Name            = "AClk",
  .EnableCtrlReg   = K3_LCD_CLK_RES_CTRL5,
  .FCEnableCtrlReg = K3_LCD_CLK_RES_CTRL5,
  .SelCtrlReg      = K3_LCD_CLK_RES_CTRL5,
  .DivCtrlReg      = K3_LCD_CLK_RES_CTRL5,

  .EnableBit   = BIT16,
  .FCEnableBit = BIT31,

  .ClkSelMask  = 0x7,
  .ClkSelShift = 20,

  .ClkDivMask  = 0x7,
  .ClkDivShift = 17,

  .ClockSourceTable = LcdAClkClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdAClkClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 51000000,
  .MaxClock = 750000000,

  .ClkDiv           = 8
};

/*
* dpu1
*/
LCD_CLOCK_CONFIG  LcdDsi4Ln2EscClockConfig = {
  .Name          = "DSI4LN2_ESCCLK",
  .EnableCtrlReg = K3_LCD_CLK_RES_CTRL3,
  .SelCtrlReg    = K3_LCD_CLK_RES_CTRL3,
  .DivCtrlReg    = K3_LCD_CLK_RES_CTRL3,

  .EnableBit        = BIT2,

  .ClkSelMask  = 0x3,
  .ClkSelShift = 0,

  .ClkDivMask  = 0x0,
  .ClkDivShift = 0,

  .ClockSourceTable = LcdEscClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdEscClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 26000000,
  .MaxClock = 78000000,

  .ClkDiv           = 1
};

LCD_CLOCK_CONFIG  LcdDsi4Ln2DscClockConfig = {
  .Name            = "DSI4LN2_DSCCLK",
  .EnableCtrlReg   = K3_LCD_CLK_RES_CTRL4,
  .FCEnableCtrlReg = K3_LCD_CLK_RES_CTRL3,
  .SelCtrlReg      = K3_LCD_CLK_RES_CTRL4,
  .DivCtrlReg      = K3_LCD_CLK_RES_CTRL4,

  .EnableBit   = BIT14,
  .FCEnableBit = BIT26,

  .ClkSelMask  = 0x7,
  .ClkSelShift = 29,

  .ClkDivMask  = 0x7,
  .ClkDivShift = 25,

  .ClockSourceTable = LcdDscClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdDscClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 6500000,
  .MaxClock = 750000000,

  .ClkDiv           = 8
};

LCD_CLOCK_CONFIG  LcdDsi4Ln2PixClockConfig = {
  .Name            = "DSI4LN2_PIX",
  .EnableCtrlReg   = K3_LCD_CLK_RES_CTRL4,
  .FCEnableCtrlReg = K3_LCD_CLK_RES_CTRL3,
  .SelCtrlReg      = K3_LCD_CLK_RES_CTRL4,
  .DivCtrlReg      = K3_LCD_CLK_RES_CTRL4,

  .EnableBit   = BIT16,
  .FCEnableBit = BIT30,

  .ClkSelMask  = 0x7,
  .ClkSelShift = 21,

  .ClkDivMask  = 0x7,
  .ClkDivShift = 17,

  .ClockSourceTable = LcdPixClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdPixClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 6500000,
  .MaxClock = 750000000,

  .ClkDiv           = 8
};

LCD_CLOCK_CONFIG  LcdDsi4Ln2MclkClockConfig = {
  .Name            = "DSI4LN2_MCLK",
  .EnableCtrlReg   = K3_LCD_CLK_RES_CTRL4,
  .FCEnableCtrlReg = K3_LCD_CLK_RES_CTRL3,
  .SelCtrlReg      = K3_LCD_CLK_RES_CTRL4,
  .DivCtrlReg      = K3_LCD_CLK_RES_CTRL4,

  .EnableBit   = BIT0,
  .FCEnableBit = BIT29,

  .ClkSelMask  = 0x7,
  .ClkSelShift = 5,

  .ClkDivMask  = 0xf,
  .ClkDivShift = 1,

  .ClockSourceTable = LcdMClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdMClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 39000000,
  .MaxClock = 624000000,

  .ClkDiv           = 16
};

LCD_CLOCK_CONFIG  LcdDsi4Ln2AclkClockConfig = {
  .Name            = "DSI4LN2_ACLK",
  .EnableCtrlReg   = K3_LCD_CLK_RES_CTRL5,
  .FCEnableCtrlReg = K3_LCD_CLK_RES_CTRL5,
  .SelCtrlReg      = K3_LCD_CLK_RES_CTRL5,
  .DivCtrlReg      = K3_LCD_CLK_RES_CTRL5,

  .EnableBit   = BIT1,
  .FCEnableBit = BIT30,

  .ClkSelMask  = 0x7,
  .ClkSelShift = 5,

  .ClkDivMask  = 0x7,
  .ClkDivShift = 2,

  .ClockSourceTable = LcdAClkClockSourceMhz,
  .ClockSourceCount = ARRAY_SIZE (LcdAClkClockSourceMhz),

  .DefaultClkIndex  = 0,

  .MinClock = 51000000,
  .MaxClock = 750000000,

  .ClkDiv           = 8
};

typedef struct {
  UINT32    ClockSourceIndex;
  UINT32    DividerValue;
} CLOCK_CONFIG_RESULT;

STATIC
CLOCK_CONFIG_RESULT
FindLcdMatchFreq (
  IN UINT64                  Freq,
  IN CONST LCD_CLOCK_CONFIG  *Cfg
  )
{
  UINT32               I, J;
  UINT32               Clock, ClockDiff, MinClockDiff, FreqMhz;
  CLOCK_CONFIG_RESULT  Result = { 0, 1 };

  FreqMhz      = Freq / 1000000;
  MinClockDiff = 0xFFFFFFFFU;

  for (I = 1; I <= Cfg->ClkDiv; I++) {
    for (J = 0; J < Cfg->ClockSourceCount; J++) {
      Clock = Cfg->ClockSourceTable[J] / I;
      if (Clock == 0) {
        continue;
      }

      if (Clock >= FreqMhz) {
        ClockDiff = (Clock - FreqMhz) * 10;
      } else {
        ClockDiff = (FreqMhz - Clock) * 10;
      }

      if (ClockDiff < MinClockDiff) {
        MinClockDiff            = ClockDiff;
        Result.ClockSourceIndex = J;
        Result.DividerValue     = I;
        if (0 == MinClockDiff) {
          return Result;
        }
      }
    }
  }

  if (Result.ClockSourceIndex >= Cfg->ClockSourceCount) {
    Result.ClockSourceIndex = Cfg->DefaultClkIndex;
  }

  return Result;
}

STATIC
EFI_STATUS
SetClockRate (
  IN UINT64                  ClockRate,
  IN CONST LCD_CLOCK_CONFIG  *Cfg
  )
{
  UINT32               ClkDiv;
  UINT64               ClkSource;
  CLOCK_CONFIG_RESULT  Result;

  Result    = FindLcdMatchFreq (ClockRate, Cfg);
  ClkSource = (UINT64)Cfg->ClockSourceTable[Result.ClockSourceIndex] * 1000000;
  ClkDiv    = Result.DividerValue;

  DEBUG (
         (
          DEBUG_INFO,
          "%a clock source: %lluHz, real clock: %lluHz\n",
          Cfg->Name,
          ClkSource,
          ClkSource / ClkDiv
         )
         );

  MmioOr32 (Cfg->EnableCtrlReg, Cfg->EnableBit);

  MmioAndThenOr32 (
                   Cfg->SelCtrlReg,
                   ~(Cfg->ClkSelMask << Cfg->ClkSelShift),
                   (Result.ClockSourceIndex << Cfg->ClkSelShift)
                   );

  MmioAndThenOr32 (
                   Cfg->DivCtrlReg,
                   ~(Cfg->ClkDivMask << Cfg->ClkDivShift),
                   ((ClkDiv - 1) << Cfg->ClkDivShift)
                   );

  if (Cfg->FCEnableCtrlReg != 0) {
    MmioOr32 (Cfg->FCEnableCtrlReg, Cfg->FCEnableBit);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetCurrentClockRate (
  OUT UINT64                 *ClockRate,
  IN CONST LCD_CLOCK_CONFIG  *Cfg
  )
{
  UINT32  ClkSel, ClkDiv;
  UINT64  ClkSource;

  ClkSel     = (MmioRead32 (Cfg->SelCtrlReg) & (Cfg->ClkSelMask << Cfg->ClkSelShift)) >> Cfg->ClkSelShift;
  ClkDiv     = (MmioRead32 (Cfg->DivCtrlReg) & (Cfg->ClkDivMask << Cfg->ClkDivShift)) >> Cfg->ClkDivShift;
  ClkSource  = (UINT64)Cfg->ClockSourceTable[ClkSel] * 1000000;
  *ClockRate = ClkSource / (ClkDiv + 1);

  DEBUG (
         (
          DEBUG_INFO,
          "%a clock source: %lluHz, divider: %d, real clock: %lluHz\n",
          Cfg->Name,
          ClkSource,
          ClkDiv + 1,
          *ClockRate
         )
         );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetMaxClockRate (
  OUT UINT64                 *ClockRate,
  IN CONST LCD_CLOCK_CONFIG  *Cfg
  )
{
  if (ClockRate == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ClockRate = Cfg->MaxClock;

  DEBUG ((DEBUG_INFO, "%a Max Clock: %llu Hz\n", Cfg->Name, *ClockRate));

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetMinClockRate (
  OUT UINT64                 *ClockRate,
  IN CONST LCD_CLOCK_CONFIG  *Cfg
  )
{
  if (ClockRate == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ClockRate = Cfg->MinClock;

  DEBUG ((DEBUG_INFO, "%a Min Clock: %llu Hz\n", Cfg->Name, *ClockRate));

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetEscCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdEscClockConfig);
}

STATIC
EFI_STATUS
GetEscMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdEscClockConfig);
}

STATIC
EFI_STATUS
GetEscMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdEscClockConfig);
}

STATIC
EFI_STATUS
SetEscClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdEscClockConfig);
}

STATIC
EFI_STATUS
GetDscCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdDscClockConfig);
}

STATIC
EFI_STATUS
GetDscMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdDscClockConfig);
}

STATIC
EFI_STATUS
GetDscMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdDscClockConfig);
}

STATIC
EFI_STATUS
SetDscClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdDscClockConfig);
}

STATIC
EFI_STATUS
GetPixCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdPixClockConfig);
}

STATIC
EFI_STATUS
GetPixMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdPixClockConfig);
}

STATIC
EFI_STATUS
GetPixMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdPixClockConfig);
}

STATIC
EFI_STATUS
SetPixClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdPixClockConfig);
}

STATIC
EFI_STATUS
GetMclkCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdMclkClockConfig);
}

STATIC
EFI_STATUS
GetMclkMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdMclkClockConfig);
}

STATIC
EFI_STATUS
GetMclkMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdMclkClockConfig);
}

STATIC
EFI_STATUS
SetMclkClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdMclkClockConfig);
}

STATIC
EFI_STATUS
GetAclkCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdAclkClockConfig);
}

STATIC
EFI_STATUS
GetAclkMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdAclkClockConfig);
}

STATIC
EFI_STATUS
GetAclkMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdAclkClockConfig);
}

STATIC
EFI_STATUS
SetAclkClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdAclkClockConfig);
}

CLOCK_RATE_OPERATIONS  LcdEscClockRateOps = {
  GetEscCurrentClockRate,
  GetEscMaxClockRate,
  GetEscMinClockRate,
  SetEscClockRate
};

CLOCK_RATE_OPERATIONS  LcdDscClockRateOps = {
  GetDscCurrentClockRate,
  GetDscMaxClockRate,
  GetDscMinClockRate,
  SetDscClockRate
};

CLOCK_RATE_OPERATIONS  LcdPixClockRateOps = {
  GetPixCurrentClockRate,
  GetPixMaxClockRate,
  GetPixMinClockRate,
  SetPixClockRate
};

CLOCK_RATE_OPERATIONS  LcdMclkClockRateOps = {
  GetMclkCurrentClockRate,
  GetMclkMaxClockRate,
  GetMclkMinClockRate,
  SetMclkClockRate
};

CLOCK_RATE_OPERATIONS  LcdAclkClockRateOps = {
  GetAclkCurrentClockRate,
  GetAclkMaxClockRate,
  GetAclkMinClockRate,
  SetAclkClockRate
};

/*
* Dsi4Ln2 clocks can be controlled by the same registers as DPU0,
* but with different bit fields.
* So we can reuse the same functions to control Dsi4Ln2 clocks
* by passing different clock config.
*/
STATIC
EFI_STATUS
GetDsi4Ln2EscCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdDsi4Ln2EscClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2EscMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdDsi4Ln2EscClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2EscMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdDsi4Ln2EscClockConfig);
}

STATIC
EFI_STATUS
SetDsi4Ln2EscClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdDsi4Ln2EscClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2DscCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdDsi4Ln2DscClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2DscMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdDsi4Ln2DscClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2DscMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdDsi4Ln2DscClockConfig);
}

STATIC
EFI_STATUS
SetDsi4Ln2DscClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdDsi4Ln2DscClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2PixCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdDsi4Ln2PixClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2PixMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdDsi4Ln2PixClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2PixMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdDsi4Ln2PixClockConfig);
}

STATIC
EFI_STATUS
SetDsi4Ln2PixClockRate (
  IN UINT64  ClockRate
  )
{
  CLOCK_CONFIG_RESULT  Result;
  UINT32               PllId;
  EFI_STATUS           Status;

  Result = FindLcdMatchFreq (ClockRate, &LcdDsi4Ln2PixClockConfig);

  if (Result.ClockSourceIndex < ARRAY_SIZE (LcdPixClockSourcePll)) {
    PllId = LcdPixClockSourcePll[Result.ClockSourceIndex];

    if (PllId > 0) {
      UINT64  PllClockRate;

      PllClockRate = 0;

      switch (PllId) {
        case 1:
          if (Pll1ClockRateOps.SetRate != NULL) {
            Status = Pll1ClockRateOps.SetRate (PllClockRate);
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        case 2:
          if (Pll2ClockRateOps.SetRate != NULL) {
            Status = Pll2ClockRateOps.SetRate (PllClockRate);
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        case 3:
          if (Pll3ClockRateOps.SetRate != NULL) {
            Status = Pll3ClockRateOps.SetRate (PllClockRate);
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        case 4:
          if (Pll4ClockRateOps.SetRate != NULL) {
            Status = Pll4ClockRateOps.SetRate (PllClockRate);
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        case 5:
          if (Pll5ClockRateOps.SetRate != NULL) {
            Status = Pll5ClockRateOps.SetRate (PllClockRate);
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        case 6:
          if (Pll6ClockRateOps.SetRate != NULL) {
            Status = Pll6ClockRateOps.SetRate (PllClockRate);
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        case 7:
          if (Pll7ClockRateOps.SetRate != NULL) {
            Status = Pll7ClockRateOps.SetRate (PllClockRate);
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        case 8:
          if (Pll8ClockRateOps.SetRate != NULL) {
            Status = Pll8ClockRateOps.SetRate (PllClockRate);
          } else {
            Status = EFI_UNSUPPORTED;
          }

          break;
        default:
          Status = EFI_INVALID_PARAMETER;
          break;
      }

      if (EFI_ERROR (Status)) {
        DEBUG (
               (DEBUG_ERROR, "DSI4LN2_PIX: Failed to enable PLL%u (default freq), Status=%r\n",
                PllId, Status)
               );
        return Status;
      }
    }
  }

  return SetClockRate (ClockRate, &LcdDsi4Ln2PixClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2MclkCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdDsi4Ln2MclkClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2MclkMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdDsi4Ln2MclkClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2MclkMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdDsi4Ln2MclkClockConfig);
}

STATIC
EFI_STATUS
SetDsi4Ln2MclkClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdDsi4Ln2MclkClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2AclkCurrentClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetCurrentClockRate (ClockRate, &LcdDsi4Ln2AclkClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2AclkMaxClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMaxClockRate (ClockRate, &LcdDsi4Ln2AclkClockConfig);
}

STATIC
EFI_STATUS
GetDsi4Ln2AclkMinClockRate (
  OUT UINT64  *ClockRate
  )
{
  return GetMinClockRate (ClockRate, &LcdDsi4Ln2AclkClockConfig);
}

STATIC
EFI_STATUS
SetDsi4Ln2AclkClockRate (
  IN UINT64  ClockRate
  )
{
  return SetClockRate (ClockRate, &LcdDsi4Ln2AclkClockConfig);
}

CLOCK_RATE_OPERATIONS  LcdDsi4Ln2EscClockRateOps = {
  GetDsi4Ln2EscCurrentClockRate,
  GetDsi4Ln2EscMaxClockRate,
  GetDsi4Ln2EscMinClockRate,
  SetDsi4Ln2EscClockRate
};

CLOCK_RATE_OPERATIONS  LcdDsi4Ln2DscClockRateOps = {
  GetDsi4Ln2DscCurrentClockRate,
  GetDsi4Ln2DscMaxClockRate,
  GetDsi4Ln2DscMinClockRate,
  SetDsi4Ln2DscClockRate
};

CLOCK_RATE_OPERATIONS  LcdDsi4Ln2PixClockRateOps = {
  GetDsi4Ln2PixCurrentClockRate,
  GetDsi4Ln2PixMaxClockRate,
  GetDsi4Ln2PixMinClockRate,
  SetDsi4Ln2PixClockRate
};

CLOCK_RATE_OPERATIONS  LcdDsi4Ln2MclkClockRateOps = {
  GetDsi4Ln2MclkCurrentClockRate,
  GetDsi4Ln2MclkMaxClockRate,
  GetDsi4Ln2MclkMinClockRate,
  SetDsi4Ln2MclkClockRate
};

CLOCK_RATE_OPERATIONS  LcdDsi4Ln2AclkClockRateOps = {
  GetDsi4Ln2AclkCurrentClockRate,
  GetDsi4Ln2AclkMaxClockRate,
  GetDsi4Ln2AclkMinClockRate,
  SetDsi4Ln2AclkClockRate
};
