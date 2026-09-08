/** @file
 *  Spacemit K3 silicon clock controller driver implementation.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Library/MemoryManagementLib.h>
#include <ClockDxe.h>

STATIC CONST CLOCK_CONFIG  ModuleConfig[] = {
  {
    "PLL1", 0x9EDBA429, 0,
    &Pll1ClockRateOps,
  },
  {
    "PLL2", 0x07D2F593, 0,
    &Pll2ClockRateOps,
  },
  {
    "PLL3", 0x70D5C505, 0,
    &Pll3ClockRateOps,
  },
  {
    "PLL4", 0xEEB150A6, 0,
    &Pll4ClockRateOps,
  },
  {
    "PLL5", 0x99B66030, 0,
    &Pll5ClockRateOps,
  },
  {
    "PLL6", 0x00BF318A, 0,
    &Pll6ClockRateOps,
  },
  {
    "PLL7", 0x77B8011C, 0,
    &Pll7ClockRateOps,
  },
  {
    "PLL8", 0xE7071C8D, 0,
    &Pll8ClockRateOps,
  },
  {
    "GPIO", 0x8CE33965, 2,
    NULL,
    {
      { K3_GPIO_CLK_RES_CTRL,  BIT0 | BIT1,           BIT0 | BIT1,           0     },
      { K3_GPIO_CLK_RES_CTRL,  BIT2,                  0,                     BIT2  }
    }
  },
  {
    "QSPI", 0x89B48352, 2,
    &QspiClockRateOps,
    {
      { K3_QSPI_CLK_RES_CTRL,  BIT3 | BIT4,           BIT3 | BIT4,           0     },
      { K3_QSPI_CLK_RES_CTRL,  BIT0 | BIT1,           BIT0 | BIT1,           0     }
    }
  },
  {
    "SDH0", 0x9116AFED, 2,
    &Sdhc0ClockRateOps,
    {
      { K3_SDH0_CLK_RES_CTRL,  BIT3 | BIT4,           BIT3 | BIT4,           0     },
      { K3_SDH0_CLK_RES_CTRL,  BIT0 | BIT1,           BIT0 | BIT1,           0     }
    }
  },
  {
    "SDH1", 0xE6119F7B, 4,
    &Sdhc1ClockRateOps,
    {
      { K3_SDH0_CLK_RES_CTRL,  BIT3,                  BIT3,                  0     },
      { K3_SDH1_CLK_RES_CTRL,  BIT4,                  BIT4,                  0     },
      { K3_SDH0_CLK_RES_CTRL,  BIT0,                  BIT0,                  0     },
      { K3_SDH1_CLK_RES_CTRL,  BIT1,                  BIT1,                  0     }
    }
  },
  {
    "SDH2", 0x7F18CEC1, 4,
    &Sdhc2ClockRateOps,
    {
      { K3_SDH0_CLK_RES_CTRL,  BIT3,                  BIT3,                  0     },
      { K3_SDH2_CLK_RES_CTRL,  BIT4,                  BIT4,                  0     },
      { K3_SDH0_CLK_RES_CTRL,  BIT0,                  BIT0,                  0     },
      { K3_SDH2_CLK_RES_CTRL,  BIT1,                  BIT1,                  0     }
    }
  },
  {
    "UFSACLK", 0xA685D1D4, 1,
    &UfsClockRateOps,
    {
      { K3_UFS_CLK_RES_CTRL,   BIT0 | BIT1,           BIT0 | BIT1,           0     }
    }
  },
  {
    "GMAC0", 0xC95A3839, 2,
    NULL,
    {
      { K3_EMAC0_CLK_RES_CTRL, BIT0,                  BIT0,                  0     },
      { K3_EMAC0_CLK_RES_CTRL, BIT1,                  BIT1,                  0     },
    }
  },
  {
    "GMAC0_TX", 0x945D2BCE, 2,
    NULL,
    {
      { K3_EMAC0_CLK_RES_CTRL, BIT8,                  BIT8,                  0     },
    }
  },
  {
    "GMAC0_PHY", 0xF1360ECC, 2,
    NULL,
    {
      { K3_EMAC0_CLK_RES_CTRL, BIT14,                 0,                     BIT14 },
    }
  },
  {
    "GMAC1", 0xBE5D08AF, 2,
    NULL,
    {
      { K3_EMAC1_CLK_RES_CTRL, BIT0,                  BIT0,                  0     },
      { K3_EMAC1_CLK_RES_CTRL, BIT1,                  BIT1,                  0     },
    }
  },
  {
    "GMAC1_TX", 0x2CE14CAB, 2,
    NULL,
    {
      { K3_EMAC0_CLK_RES_CTRL, BIT8,                  BIT8,                  0     },
    }
  },
  {
    "GMAC1_PHY", 0xCC56277C, 2,
    NULL,
    {
      { K3_EMAC0_CLK_RES_CTRL, BIT14,                 0,                     BIT14 },
    }
  },
  {
    "GMAC2", 0x27545915, 2,
    NULL,
    {
      { K3_EMAC2_CLK_RES_CTRL, BIT0,                  BIT0,                  0     },
      { K3_EMAC2_CLK_RES_CTRL, BIT1,                  BIT1,                  0     },
    }
  },
  {
    "GMAC2_TX", 0x3E54E345, 2,
    NULL,
    {
      { K3_EMAC0_CLK_RES_CTRL, BIT8,                  BIT8,                  0     },
    }
  },
  {
    "GMAC2_PHY", 0x8BF65DAC, 2,
    NULL,
    {
      { K3_EMAC0_CLK_RES_CTRL, BIT14,                 0,                     BIT14 },
    }
  },
  {
    "PCIE0", 0xA30CF2A6, 3,
    NULL,
    {
      { K3_PCIE0_APP_BASE,     BIT0 | BIT1 | BIT2,    BIT0 | BIT1 | BIT2,    0     },
      { K3_PCIE0_APP_BASE,     BIT3 | BIT4 | BIT5 | BIT8, BIT3 | BIT4 | BIT5,    BIT8  },
      { PLL2_SW_CTRL_REG,      BIT16 | BIT5,          BIT16 | BIT5,          0     }
    }
  },
  {
    "PCIE1", 0xD40BC230, 3,
    NULL,
    {
      { K3_PCIE1_APP_BASE,     BIT0 | BIT1 | BIT2,    BIT0 | BIT1 | BIT2,    0     },
      { K3_PCIE1_APP_BASE,     BIT3 | BIT4 | BIT5 | BIT8, BIT3 | BIT4 | BIT5,    BIT8  },
      { PLL2_SW_CTRL_REG,      BIT16 | BIT5,          BIT16 | BIT5,          0     }
    }
  },
  {
    "PCIE2", 0x4D02938A, 3,
    NULL,
    {
      { K3_PCIE2_APP_BASE,     BIT0 | BIT1 | BIT2,    BIT0 | BIT1 | BIT2,    0     },
      { K3_PCIE2_APP_BASE,     BIT3 | BIT4 | BIT5 | BIT8, BIT3 | BIT4 | BIT5,    BIT8  },
      { PLL2_SW_CTRL_REG,      BIT16 | BIT5,          BIT16 | BIT5,          0     }
    }
  },
  {
    "PCIE3", 0x3A05A31C, 3,
    NULL,
    {
      { K3_PCIE3_APP_BASE,     BIT0 | BIT1 | BIT2,    BIT0 | BIT1 | BIT2,    0     },
      { K3_PCIE3_APP_BASE,     BIT3 | BIT4 | BIT5 | BIT8, BIT3 | BIT4 | BIT5,    BIT8  },
      { PLL2_SW_CTRL_REG,      BIT16 | BIT5,          BIT16 | BIT5,          0     }
    }
  },
  {
    "PCIE4", 0xA46136BF, 3,
    NULL,
    {
      { K3_PCIE4_APP_BASE,     BIT0 | BIT1 | BIT2,    BIT0 | BIT1 | BIT2,    0     },
      { K3_PCIE4_APP_BASE,     BIT3 | BIT4 | BIT5 | BIT8, BIT3 | BIT4 | BIT5,    BIT8  },
      { PLL2_SW_CTRL_REG,      BIT16 | BIT5,          BIT16 | BIT5,          0     }
    }
  },
  {
    "USB2", 0xB9F5CC62, 2,
    NULL,
    {
      { K3_USB_CLK_RES_CTRL,   BIT0,                  BIT0,                  0     },
      { K3_USB_CLK_RES_CTRL,   BIT1 | BIT2 | BIT3,    BIT1 | BIT2 | BIT3,    0     }
    }
  },
  {
    "USB3_PORTA", 0xB3D30C9C, 2,
    NULL,
    {
      { K3_USB_CLK_RES_CTRL,   BIT4,                  BIT4,                  0     },
      { K3_USB_CLK_RES_CTRL,   BIT5 | BIT6 | BIT7,    BIT5 | BIT6 | BIT7,    0     }
    }
  },
  {
    "USB3_PORTB", 0x2ADA5D26, 2,
    NULL,
    {
      { K3_USB_CLK_RES_CTRL,   BIT8,                  BIT8,                  0     },
      { K3_USB_CLK_RES_CTRL,   BIT9 | BIT10 | BIT11,  BIT9 | BIT10 | BIT11,  0     }
    }
  },
  {
    "USB3_PORTC", 0x5DDD6DB0, 2,
    NULL,
    {
      { K3_USB_CLK_RES_CTRL,   BIT12,                 BIT12,                 0     },
      { K3_USB_CLK_RES_CTRL,   BIT13 | BIT14 | BIT15, BIT13 | BIT14 | BIT15, 0     }
    }
  },
  {
    "USB3_PORTD", 0xC3B9F813, 2,
    NULL,
    {
      { K3_USB_CLK_RES_CTRL,   BIT16,                 BIT16,                 0     },
      { K3_USB_CLK_RES_CTRL,   BIT17 | BIT18 | BIT19, BIT18 | BIT17 | BIT19, 0     }
    }
  },
  {
    "LCDCLK", 0x0D632DCF, 1,
    NULL,
    {
      { K3_LCD_CLK_RES_CTRL1,  BIT4,                  BIT4,                  0     }
    }
  },
  {
    "LCDDSIESC", 0xCC8DCB99, 1,
    &LcdEscClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL1,  BIT3,                  BIT3,                  0     }
    }
  },
  {
    "LCDDSC", 0xCBADBDE6, 1,
    &LcdDscClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL2,  BIT15,                 BIT15,                 0     }
    }
  },
  {
    "LCDPIX", 0xEACA857D, 1,
    &LcdPixClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL2,  BIT16,                 BIT16,                 0     }
    }
  },
  {
    "LCDMCLK", 0x9C186D50, 1,
    &LcdMclkClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL2,  BIT9,                  BIT9,                  0     }
    }
  },
  {
    "LCDACLK", 0xD6CED2E8, 1,
    &LcdAclkClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL5,  BIT15,                 BIT15,                 0     }
    }
  },
  {
    "LCDHCLK", 0xABC69D62, 1,
    NULL,
    {
      { K3_LCD_CLK_RES_CTRL2,  BIT5,                  BIT5,                  0     }
    }
  },
  {
    "EDP0CLK", 0x18B05138, 2,
    NULL,
    {
      { K3_LCD_CLK_EDP_CTRL,   BIT0,                  BIT0,                  0     },
      { K3_LCD_CLK_EDP_CTRL,   BIT1,                  BIT1,                  0     }
    }
  },
  {
    "EDP1CLK", 0xA00C365D, 2,
    NULL,
    {
      { K3_LCD_CLK_EDP_CTRL,   BIT16,                 BIT16,                 0     },
      { K3_LCD_CLK_EDP_CTRL,   BIT17,                 BIT17,                 0     }
    }
  },
  {
    "DSI4LN2_LCDCLK", 0x349591E4, 1,
    NULL,
    {
      { K3_LCD_CLK_RES_CTRL3,  BIT4,                  BIT4,                  0     }
    }
  },
  {
    "DSI4LN2_ESCCLK", 0xEEAD6F17, 1,
    &LcdDsi4Ln2EscClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL3,  BIT3,                  BIT3,                  0     }
    }
  },
  {
    "DSI4LN2_DSCCLK", 0x25F1BCB2, 1,
    &LcdDsi4Ln2DscClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL4,  BIT15,                 BIT15,                 0     }
    }
  },
  {
    "DSI4LN2_PIX", 0xFA428D28, 1,
    &LcdDsi4Ln2PixClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL4,  BIT16,                 BIT16,                 0     }
    }
  },
  {
    "DSI4LN2_MCLK", 0x87094023, 1,
    &LcdDsi4Ln2MclkClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL4,  BIT9,                  BIT9,                  0     }
    }
  },
  {
    "DSI4LN2_ACLK", 0xCDDFFF9B, 1,
    &LcdDsi4Ln2AclkClockRateOps,
    {
      { K3_LCD_CLK_RES_CTRL5,  BIT0,                  BIT0,                  0     }
    }
  },
};

EFI_STATUS
PollRegStatus (
  IN UINTN   RegAddr,
  IN UINT32  Mask,
  IN UINT32  Value,
  IN UINT32  TimeoutUs
  )
{
  while (1) {
    if (Value == (MmioRead32 (RegAddr) & Mask)) {
      break;
    }

    gBS->Stall (1);
    if (!TimeoutUs--) {
      DEBUG (
             (DEBUG_ERROR,
              "Timeout while poll 0x%x for Val 0x%x!\n", RegAddr, Value)
             );
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetClockModuleIndex (
  IN CONST CHAR8  *Name,
  OUT UINT32      *Index
  )
{
  UINT32  I, Crc32Value;

  if (EFI_ERROR (gBS->CalculateCrc32 ((VOID *)Name, AsciiStrLen (Name), &Crc32Value))) {
    return EFI_NOT_FOUND;
  }

  for (I = 0; I < ARRAY_SIZE (ModuleConfig); I++) {
    // compare CRC32 code first, save compare time
    if ((Crc32Value == ModuleConfig[I].Crc32)
        && (0 == AsciiStrCmp (Name, ModuleConfig[I].ClockName)))
    {
      *Index = I;
      return EFI_SUCCESS;
    }
  }

  DEBUG ((DEBUG_ERROR, "Fail to find clock config for module %a\n", Name));
  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
GetCurrentClockState (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN CONST CHAR8                 *ClockName,
  OUT UINT32                     *ClockState
  )
{
  UINT32              Index;
  CLOCKCTRL_INSTANCE  *ClockCtrlInstance;

  ASSERT (ClockState != NULL);

  if (EFI_ERROR (GetClockModuleIndex (ClockName, &Index))) {
    return EFI_NOT_FOUND;
  }

  ClockCtrlInstance = CLOCKCTRL_INSTANCE_FROM_THIS (This);

  *ClockState = (0 != (ClockCtrlInstance->State[Index / 64] & (1UL << (Index % 64))))
        ? CLOCK_ENABLED : CLOCK_DISABLED;

  return EFI_SUCCESS;
}

STATIC
VOID
EnableClock (
  IN  UINT32  Index
  )
{
  UINT32  I;

  for (I = 0; I < ModuleConfig[Index].ConfigNum; I++) {
    MmioAndThenOr32 (
                     ModuleConfig[Index].Config[I].RegAddr,
                     ~ModuleConfig[Index].Config[I].RegValMask,
                     ModuleConfig[Index].Config[I].RegValEnable
                     );
  }
}

STATIC
VOID
DisableClock (
  IN  UINT32  Index
  )
{
  UINT32  I;

  for (I = 0; I < ModuleConfig[Index].ConfigNum; I++) {
    MmioAndThenOr32 (
                     ModuleConfig[Index].Config[I].RegAddr,
                     ~ModuleConfig[Index].Config[I].RegValMask,
                     ModuleConfig[Index].Config[I].RegValDisable
                     );
  }
}

STATIC
EFI_STATUS
SetClockState (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  IN  UINT32                     ClockState
  )
{
  UINT32              Index;
  CLOCKCTRL_INSTANCE  *ClockCtrlInstance;

  if (EFI_ERROR (GetClockModuleIndex (ClockName, &Index))) {
    return EFI_NOT_FOUND;
  }

  ClockCtrlInstance = CLOCKCTRL_INSTANCE_FROM_THIS (This);

  if (ENABLE_CLOCK == ClockState) {
    EnableClock (Index);
    ClockCtrlInstance->State[Index / 64] |= 1UL << (Index % 64);
  } else {
    DisableClock (Index);
    ClockCtrlInstance->State[Index / 64] &= ~(1UL << (Index % 64));
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetClockRate (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  IN  UINT64                     ClockRate
  )
{
  UINT32  Index;

  if (EFI_ERROR (GetClockModuleIndex (ClockName, &Index))) {
    return EFI_NOT_FOUND;
  }

  if ((Index < ARRAY_SIZE (ModuleConfig)) &&
      (ModuleConfig[Index].Operate != NULL) &&
      (ModuleConfig[Index].Operate->SetRate != NULL))
  {
    return ModuleConfig[Index].Operate->SetRate (ClockRate);
  }

  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
GetCurrentClockRate (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  OUT UINT64                     *ClockRate
  )
{
  UINT32  Index;

  if (EFI_ERROR (GetClockModuleIndex (ClockName, &Index))) {
    return EFI_NOT_FOUND;
  }

  if ((Index < ARRAY_SIZE (ModuleConfig)) &&
      (ModuleConfig[Index].Operate != NULL) &&
      (ModuleConfig[Index].Operate->GetRate != NULL))
  {
    return ModuleConfig[Index].Operate->GetRate (ClockRate);
  }

  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
GetMaxClockRate (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  OUT UINT64                     *ClockRate
  )
{
  UINT32  Index;

  if (EFI_ERROR (GetClockModuleIndex (ClockName, &Index))) {
    return EFI_NOT_FOUND;
  }

  if ((Index < ARRAY_SIZE (ModuleConfig)) &&
      (ModuleConfig[Index].Operate != NULL) &&
      (ModuleConfig[Index].Operate->GetMaxRate != NULL))
  {
    return ModuleConfig[Index].Operate->GetMaxRate (ClockRate);
  }

  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
GetMinClockRate (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  OUT UINT64                     *ClockRate
  )
{
  UINT32  Index;

  if (EFI_ERROR (GetClockModuleIndex (ClockName, &Index))) {
    return EFI_NOT_FOUND;
  }

  if ((Index < ARRAY_SIZE (ModuleConfig)) &&
      (ModuleConfig[Index].Operate != NULL) &&
      (ModuleConfig[Index].Operate->GetMinRate != NULL))
  {
    return ModuleConfig[Index].Operate->GetMinRate (ClockRate);
  }

  return EFI_UNSUPPORTED;
}

STATIC
VOID
ClockCtrlMmioRemap (
  VOID
  )
{
  // Map Clock related controller registers to MMIO space
  MapRegToGcdMmioSpace (K3_APB_CLOCK_BASE, SIZE_4KB);
  MapRegToGcdMmioSpace (K3_APB_SPARE_BASE, SIZE_4KB);
  MapRegToGcdMmioSpace (K3_MPMU_BASE, SIZE_8KB);
  MapRegToGcdMmioSpace (K3_APMU_BASE, SIZE_4KB);
}

/**
  Register K3 silicon clock conctrl Protocol

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
SpacemitK3ClockDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  CLOCKCTRL_INSTANCE  *ClockCtrlInstance;

  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gSpacemitSiliconClockCtrlProtocolGuid);

  ClockCtrlInstance = AllocateZeroPool (sizeof (CLOCKCTRL_INSTANCE));
  if (NULL == ClockCtrlInstance) {
    return EFI_OUT_OF_RESOURCES;
  }

  ClockCtrlInstance->Signature = K3_CLOCK_SIGNATURE;

  ClockCtrlInstance->ClockCtrlProtocol.GetClockState   = GetCurrentClockState;
  ClockCtrlInstance->ClockCtrlProtocol.SetClockState   = SetClockState;
  ClockCtrlInstance->ClockCtrlProtocol.GetClockRate    = GetCurrentClockRate;
  ClockCtrlInstance->ClockCtrlProtocol.GetMaxClockRate = GetMaxClockRate;
  ClockCtrlInstance->ClockCtrlProtocol.GetMinClockRate = GetMinClockRate;
  ClockCtrlInstance->ClockCtrlProtocol.SetClockRate    = SetClockRate;

  Status = gBS->InstallProtocolInterface (
                                          &ClockCtrlInstance->Handle,
                                          &gSpacemitSiliconClockCtrlProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          &ClockCtrlInstance->ClockCtrlProtocol
                                          );

  if (!EFI_ERROR (Status)) {
    // Map MMIO for clock controller access
    ClockCtrlMmioRemap ();
  } else {
    FreePool (ClockCtrlInstance);
    DEBUG (
           (DEBUG_ERROR,
            "%a: failed to install silicon clock control protocol (Status == %r)\n",
            __FUNCTION__, Status)
           );
  }

  return Status;
}
