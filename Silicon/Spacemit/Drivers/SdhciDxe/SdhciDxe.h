/** @file
 *
 *  Spacemit Cores SDHCI Controller driver
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef __SDHCIDXE_H__
#define __SDHCIDXE_H__

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/NonDiscoverableDeviceRegistrationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryManagementLib.h>
#include <Library/PinctrlPcdConfig.h>

#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/PciIo.h>
#include <Protocol/SdMmcOverride.h>
#include <Protocol/ClockCtrl.h>
#include <Protocol/PinCtrl.h>

#include "BitOps.h"
#include "SdhciPciHci.h"

#define SDCARD_BASE   FixedPcdGet64 (PcdSdCardBaseAddress)
#define EMMC_BASE     FixedPcdGet64 (PcdEmmcBaseAddress)
#define SDHCI_REG_SZ  SIZE_4KB

#define SDCARD_CONTROLLER_NAME  (PcdGetPtr(PcdSdCardControllerName))
#define EMMC_CONTROLLER_NAME    (PcdGetPtr(PcdEmmcControllerName))

#define SDCARD_CLOCK_RATE  (PcdGet64(PcdSdCardClockRate))
#define EMMC_CLOCK_RATE    (PcdGet64(PcdEmmcClockRate))

/* Spacemit SDH vendor registers define */
#define SPACEMIT_SDHC_OP_EXT_REG  0x108
#define  SDHC_OVRRD_CLK_OEN       BIT(11)
#define  SDHC_FORCE_CLK_ON        BIT(12)

#define SPACEMIT_SDHC_MMC_CTRL_REG  0x114
#define  SDHC_MISC_INT_EN           BIT(1)
#define  SDHC_MISC_INT              BIT(2)
#define  SDHC_ENHANCE_STROBE_EN     BIT(8)
#define  SDHC_MMC_HS400             BIT(9)
#define  SDHC_MMC_HS200             BIT(10)
#define  SDHC_MMC_CARD_MODE         BIT(12)

#define SPACEMIT_SDHC_RX_CFG_REG  0x118
#define  SDHC_RX_SDCLK_SEL0       GENMASK(1, 0)
#define  SDHC_RX_SDCLK_SEL1       GENMASK(3, 2)

#define SPACEMIT_SDHC_TX_CFG_REG  0x11C
#define  SDHC_TX_INT_CLK_SEL      BIT(30)
#define  SDHC_TX_MUX_SEL          BIT(31)

#define SPACEMIT_SDHC_DLINE_CTRL_REG  0x130
#define  SDHC_DLINE_PU                BIT(0)
#define  SDHC_RX_DLINE_CODE           GENMASK(23, 16)
#define  SDHC_TX_DLINE_CODE           GENMASK(31, 24)

#define SPACEMIT_SDHC_DLINE_CFG_REG  0x134
#define  SDHC_RX_DLINE_REG           GENMASK(7, 0)
#define  SDHC_RX_DLINE_GAIN          BIT(8)
#define  SDHC_TX_DLINE_REG           GENMASK(23, 16)

#define SPACEMIT_SDHC_PHY_CTRL_REG  0x160
#define  SDHC_PHY_FUNC_EN           BIT(0)
#define  SDHC_PHY_PLL_LOCK          BIT(1)
#define  SDHC_HOST_LEGACY_MODE      BIT(31)

#define SPACEMIT_SDHC_PHY_FUNC_REG  0x164
#define  SDHC_PHY_TEST_EN           BIT(7)
#define  SDHC_HS200_USE_RFIFO       BIT(15)

#define SPACEMIT_SDHC_PHY_DLLCFG  0x168
#define  SDHC_DLL_PREDLY_NUM      GENMASK(3, 2)
#define  SDHC_DLL_FULLDLY_RANGE   GENMASK(5, 4)
#define  SDHC_DLL_VREG_CTRL       GENMASK(7, 6)
#define  SDHC_DLL_ENABLE          BIT(31)

#define SPACEMIT_SDHC_PHY_DLLCFG1  0x16C
#define  SDHC_DLL_REG1_CTRL        GENMASK(7, 0)
#define  SDHC_DLL_REG2_CTRL        GENMASK(15, 8)
#define  SDHC_DLL_REG3_CTRL        GENMASK(23, 16)
#define  SDHC_DLL_REG4_CTRL        GENMASK(31, 24)

#define SPACEMIT_SDHC_PHY_DLLSTS  0x170
#define  SDHC_DLL_LOCK_STATE      BIT(0)

#define SPACEMIT_SDHC_PHY_PADCFG_REG  0x178
#define  SDHC_PHY_DRIVE_SEL           GENMASK(2, 0)
#define  SDHC_RX_BIAS_CTRL            BIT(5)

#define SDHC_RX_TUNE_DELAY_MIN   0x0
#define SDHC_RX_TUNE_DELAY_MAX   0xFF
#define SDHC_RX_TUNE_DELAY_STEP  0x1

#define RX_TUNING_WINDOW_THRESHOLD  80
#define RX_TUNING_DLINE_REG         0x00
#define TX_TUNING_DLINE_REG         0x00
#define TX_TUNING_DELAYCODE         127

typedef enum {
  SdMmcTypeSd,
  SdMmcTypeSdio,
  SdMmcTypeEmmc,
} SD_MMC_TYPE;

typedef struct {
  UINT32    TimeoutFreq   : 6; // bit 0:5
  UINT32    Reserved      : 1; // bit 6
  UINT32    TimeoutUnit   : 1; // bit 7
  UINT32    BaseClkFreq   : 8; // bit 8:15
  UINT32    MaxBlkLen     : 2; // bit 16:17
  UINT32    BusWidth8     : 1; // bit 18
  UINT32    Adma2         : 1; // bit 19
  UINT32    Reserved2     : 1; // bit 20
  UINT32    HighSpeed     : 1; // bit 21
  UINT32    Sdma          : 1; // bit 22
  UINT32    SuspRes       : 1; // bit 23
  UINT32    Voltage33     : 1; // bit 24
  UINT32    Voltage30     : 1; // bit 25
  UINT32    Voltage18     : 1; // bit 26
  UINT32    SysBus64V4    : 1; // bit 27
  UINT32    SysBus64V3    : 1; // bit 28
  UINT32    AsyncInt      : 1; // bit 29
  UINT32    SlotType      : 2; // bit 30:31
  UINT32    Sdr50         : 1; // bit 32
  UINT32    Sdr104        : 1; // bit 33
  UINT32    Ddr50         : 1; // bit 34
  UINT32    Reserved3     : 1; // bit 35
  UINT32    DriverTypeA   : 1; // bit 36
  UINT32    DriverTypeC   : 1; // bit 37
  UINT32    DriverTypeD   : 1; // bit 38
  UINT32    DriverType4   : 1; // bit 39
  UINT32    TimerCount    : 4; // bit 40:43
  UINT32    Reserved4     : 1; // bit 44
  UINT32    TuningSDR50   : 1; // bit 45
  UINT32    RetuningMod   : 2; // bit 46:47
  UINT32    ClkMultiplier : 8; // bit 48:55
  UINT32    Reserved5     : 7; // bit 56:62
  UINT32    Hs400         : 1; // bit 63
} SD_MMC_HC_SLOT_CAP;

#endif // __SDHCIDXE_H__
