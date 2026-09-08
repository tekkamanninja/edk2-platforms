/** @file
 *  Spacemit K3 silicon clock controller driver header.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef __K3_CLOCK_DXE_H__
#define __K3_CLOCK_DXE_H__

#include <Library/BaseLib.h>
#include <Protocol/ClockCtrl.h>

#define K3_CLOCK_SIGNATURE  SIGNATURE_32('c', 'l', 'k', '3')
#define CLOCKCTRL_INSTANCE_FROM_THIS(a)  CR (a, CLOCKCTRL_INSTANCE, ClockCtrlProtocol, K3_CLOCK_SIGNATURE)

#define K3_MPMU_BASE       (FixedPcdGet64(PcdSpacemitMPMURegBase))
#define K3_APB_CLOCK_BASE  (FixedPcdGet64(PcdSpacemitAPBClockRegBase))
#define K3_APB_SPARE_BASE  (FixedPcdGet64(PcdSpacemitAPBSpareRegBase))
#define K3_APMU_BASE       (FixedPcdGet64(PcdSpacemitAPMURegBase))

#define K3_USB_CLK_RES_CTRL   (K3_APMU_BASE + 0x5c)
#define K3_QSPI_CLK_RES_CTRL  (K3_APMU_BASE + 0x60)
#define K3_SDH0_CLK_RES_CTRL  (K3_APMU_BASE + 0x54)
#define K3_SDH1_CLK_RES_CTRL  (K3_APMU_BASE + 0x58)
#define K3_SDH2_CLK_RES_CTRL  (K3_APMU_BASE + 0xE0)
#define K3_UFS_CLK_RES_CTRL   (K3_APMU_BASE + 0x268)

#define K3_EMAC0_CLK_RES_CTRL  (K3_APMU_BASE + 0x3E4)
#define K3_EMAC1_CLK_RES_CTRL  (K3_APMU_BASE + 0x3EC)
#define K3_EMAC2_CLK_RES_CTRL  (K3_APMU_BASE + 0x248)

#define K3_PCIE0_APP_BASE      (K3_APMU_BASE + 0x1F0)
#define K3_PCIE1_APP_BASE      (K3_APMU_BASE + 0x1D0)
#define K3_PCIE2_APP_BASE      (K3_APMU_BASE + 0x1C8)
#define K3_PCIE3_APP_BASE      (K3_APMU_BASE + 0x1E0)
#define K3_PCIE4_APP_BASE      (K3_APMU_BASE + 0x1E8)

#define K3_GPIO_CLK_RES_CTRL  (K3_APB_CLOCK_BASE + 0x08)
#define K3_LCD_CLK_RES_CTRL1  (K3_APMU_BASE + 0x44)
#define K3_LCD_CLK_RES_CTRL2  (K3_APMU_BASE + 0x4C)
#define K3_LCD_CLK_RES_CTRL3  (K3_APMU_BASE + 0x26C)
#define K3_LCD_CLK_RES_CTRL4  (K3_APMU_BASE + 0x270)
#define K3_LCD_CLK_RES_CTRL5  (K3_APMU_BASE + 0x274)
#define K3_LCD_CLK_EDP_CTRL   (K3_APMU_BASE + 0x23C)

#define PLL1_SW_CTRL_REG   (K3_APB_SPARE_BASE + 0x104)
#define PLL2_SW_CTRL_REG   (K3_APB_SPARE_BASE + 0x11C)
#define PLL3_SW_CTRL_REG   (K3_APB_SPARE_BASE + 0x128)
#define PLL4_SW_CTRL_REG   (K3_APB_SPARE_BASE + 0x134)
#define PLL5_SW_CTRL_REG   (K3_APB_SPARE_BASE + 0x140)
#define PLL6_SW_CTRL_REG   (K3_APB_SPARE_BASE + 0x14C)
#define PLL7_SW_CTRL_REG   (K3_APB_SPARE_BASE + 0x15C)
#define PLL8_SW_CTRL_REG   (K3_APB_SPARE_BASE + 0x184)
#define PLL_XO_STATUS_REG  (K3_MPMU_BASE + 0x10)
#define PMU_ACGR           (K3_MPMU_BASE + 0x1024)

typedef struct {
  UINTN                         Signature;
  EFI_HANDLE                    Handle;

  // bit map for clock state, should update it when clock number change
  UINT64                        State[1];
  SILICON_CLOCKCTRL_PROTOCOL    ClockCtrlProtocol;
} CLOCKCTRL_INSTANCE;

typedef struct {
  UINT32    RegAddr;
  UINT32    RegValMask;
  UINT32    RegValEnable;
  UINT32    RegValDisable;
} CLOCK_ENABLE_CONFIG;

typedef EFI_STATUS
(*GET_RATE) (
  OUT  UINT64  *ClockRate
  );

typedef EFI_STATUS
(*GET_MAX_RATE) (
  OUT  UINT64  *ClockRate
  );

typedef EFI_STATUS
(*GET_MIN_RATE) (
  OUT  UINT64  *ClockRate
  );

typedef EFI_STATUS
(*SET_RATE) (
  IN  UINT64  ClockRate
  );

typedef struct {
  GET_RATE    GetRate;
  GET_RATE    GetMaxRate;
  GET_RATE    GetMinRate;
  SET_RATE    SetRate;
} CLOCK_RATE_OPERATIONS;

typedef struct {
  CHAR8                    *ClockName;
  UINT32                   Crc32;
  UINT32                   ConfigNum;
  CLOCK_RATE_OPERATIONS    *Operate;
  CLOCK_ENABLE_CONFIG      Config[4];
} CLOCK_CONFIG;

extern CLOCK_RATE_OPERATIONS  Pll1ClockRateOps, Pll2ClockRateOps, Pll3ClockRateOps, Pll4ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Pll5ClockRateOps, Pll6ClockRateOps, Pll7ClockRateOps, Pll8ClockRateOps;
extern CLOCK_RATE_OPERATIONS  QspiClockRateOps;
extern CLOCK_RATE_OPERATIONS  Sdhc0ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Sdhc1ClockRateOps;
extern CLOCK_RATE_OPERATIONS  Sdhc2ClockRateOps;
extern CLOCK_RATE_OPERATIONS  UfsClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdEscClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdDscClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdPixClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdMclkClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdAclkClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdDsi4Ln2EscClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdDsi4Ln2DscClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdDsi4Ln2PixClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdDsi4Ln2MclkClockRateOps;
extern CLOCK_RATE_OPERATIONS  LcdDsi4Ln2AclkClockRateOps;

EFI_STATUS
PollRegStatus (
  IN UINTN   RegAddr,
  IN UINT32  Mask,
  IN UINT32  Value,
  IN UINT32  TimeoutUs
  );

UINT32
FindBestMatchFreq (
  IN UINT64  Freq,
  IN UINT32  *ClockTable,
  IN UINT32  MaxClockNum,
  IN UINT32  MaxClockDivision,
  IN UINT32  DefaultIndex
  );

#endif /* __K3_CLOCK_DXE_H__ */
