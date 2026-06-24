/** @file
  PCI Host Bridge Library instance for Spacemit K3

  Adapted from K1 driver with K3-specific hardware changes.
  PHY initialization code preserved from K1.

  Copyright (c) 2017, Linaro Ltd. All rights reserved.<BR>
  Copyright (c) 2019 Marvell International Ltd. All rights reserved.<BR>
  Copyright (c) 2025 Spacemit Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <IndustryStandard/Pci22.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/ClockCtrl.h>
#include <Library/MemoryManagementLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>

#include <PciRegs.h>
#include <Library/DesignWarePcieControllerLib.h>
#include <Protocol/PinCtrl.h>
#include <Library/DmaIoMmuConfig.h>
#include <Protocol/EmbeddedGpio.h>

#define PCIE_REF_CLK_OUTPUT

#include "K3PciePcdConfig.h"

#define K3_PCIE_VENDOR_ID  0x201F
#define K3_PCIE_DEVICE_ID  0x0002


#define PCIE_LINK_CAPABILITY    0x7c
#define PCIE_LINK_CTL_2         0xa0
#define TARGET_LINK_SPEED_MASK  0xf
#define LINK_SPEED_GEN_1        0x1
#define LINK_SPEED_GEN_2        0x2
#define LINK_SPEED_GEN_3        0x3

/* Synopsys-specific PCIe configuration registers for Link Width */
#define PCIE_PORT_LINK_CONTROL    0x710
#define PORT_LINK_DLL_LINK_EN     BIT5
#define PORT_LINK_FAST_LINK_MODE  BIT7

/* Synopsys PCIe Debug register */
#define PCIE_PORT_DEBUG1           0x72C
#define PCIE_PORT_DEBUG1_LINK_UP           BIT4
#define PCIE_PORT_DEBUG1_LINK_IN_TRAINING  BIT29
#define PORT_LINK_MODE_MASK       0x003F0000// GENMASK(21, 16)
#define PORT_LINK_MODE_1_LANES    0x00010000// 0x1 << 16
#define PORT_LINK_MODE_2_LANES    0x00030000// 0x3 << 16
#define PORT_LINK_MODE_4_LANES    0x00070000// 0x7 << 16
#define PORT_LINK_MODE_8_LANES    0x000F0000// 0xF << 16

#define PCIE_LINK_WIDTH_SPEED_CONTROL  0x80C
#define PORT_LOGIC_LINK_WIDTH_MASK     0x00001F00// GENMASK(12, 8)
#define PORT_LOGIC_LINK_WIDTH_1_LANES  0x00000100// 0x1 << 8
#define PORT_LOGIC_LINK_WIDTH_2_LANES  0x00000200// 0x2 << 8
#define PORT_LOGIC_LINK_WIDTH_4_LANES  0x00000400// 0x4 << 8
#define PORT_LOGIC_LINK_WIDTH_8_LANES  0x00000800// 0x8 << 8

/* PCIe Capability Link Capabilities register */
// #define PCI_EXP_LNKCAP                   0x0C  // Offset from capability base
#define PCI_EXP_LNKCAP_MLW_MASK  0x000003F0 // Maximum Link Width, GENMASK(9, 4)

#define PORT_LOGIC_LTSSM_STATE_MASK  0x1f
#define PORT_LOGIC_LTSSM_STATE_L0    0x11

#define PCIE_LINK_UP_TIMEOUT_MS  1000

#define PCIE_CLK_RESET_CTRL  0x0000
#define LTSSM_EN             BIT6
/* Perst input value in ep mode */
#define PCIE_PERST_IN  BIT7
/* K3 specific: Auxiliary power detection */
#define PCIE_AUX_PWR_DET  BIT9
/* Wake# GPIO in EP mode 1: Wake# low, 0: Wake# high */
#define PCIE_EP_WAKE      BIT13
#define APP_HOLD_PHY_RST  BIT30
/* BIT31 0: EP, 1: RC*/
#define DEVICE_TYPE_RC  BIT31

#define PCIE_CTRL_LOGIC  0x0004
/* K3: PERST# control in PCIE_CTRL_LOGIC, 1=PERST# low, 0=PERST# high */
#define PCIE_PERSTN_OE  BIT24
/* K3: PERST# output value (used together with PCIE_PERSTN_OE) */
#define PCIE_PERSTN_OUT  BIT25
/* K3: IGNORE_PERSTN moved to BIT31 (K1 was BIT2) */
#define PCIE_IGNORE_PERSTN  BIT31

#define K3_PHY_AHB_LINK_STS  0x0004
#define SMLH_LINK_UP         BIT1
#define RDLH_LINK_UP         BIT12

/* PCI DBICS registers */
#define PCIE_LINK_STATUS_REG         0x80
#define PCIE_LINK_STATUS_SPEED_OFF   16
#define PCIE_LINK_STATUS_SPEED_MASK  (0xf << PCIE_LINK_STATUS_SPEED_OFF)
#define PCIE_LINK_STATUS_WIDTH_OFF   20
#define PCIE_LINK_STATUS_WIDTH_MASK  (0xf << PCIE_LINK_STATUS_WIDTH_OFF)

#define SYS_PCI_CACHE_LINE_SIZE  64

#define K3_PCIE_MGMT_BASE  0xD42829D8

/* PCIe Capability registers (standard location for DesignWare) */
#define PCIE_CAP_BASE_OFFSET                                                                       \
        0x70 // Must be 4-byte aligned for 32-bit access
            // 0x70: Cap ID + Next Ptr (low 16 bits)
            // 0x72: PCIe FLAGS (high 16 bits)

/* PCIe link speed negotiation wait time in microseconds (default 1 second) */
#ifndef PCIE_LINK_SPEED_WAIT_US
#define PCIE_LINK_SPEED_WAIT_US  1000000
#endif

typedef struct {
  UINT32    Ctrl;
  UINT32    PhyAhb;
  UINT32    Mgmt;                 // Global lane/port mux management register (single 32-bit register)
  UINT32    PortId;               // spacemit,pcie-port (0..4)
  UINT32    DeviceDetectGpio;     // Optional GPIO used to decide Port0 lane split
  UINT32    DeviceDetectActive;   // 0/1
  UINT32    NumPhys;
  UINT32    PhyIndex[K3_PCIE_MAX_PHYS_PER_PORT];
} EFI_PCI_REG_BASE_ADDR;

#define K3_PCIE_MAX_PORTS  5

STATIC EFI_PCI_REG_BASE_ADDR  *mK3PcieReg;

STATIC DW_PCIE  **mDwPcies;
STATIC UINTN    mDwPciesCount;

//
// Port A/B lane split policy:
// - When split is enabled, Port A and Port B are configured as x2/x2.
// - Otherwise Port B is skipped and Port A uses its configured width (x4/x8).
//
STATIC BOOLEAN  mPortABSplitEnabled;

// Track PHY power-on to avoid re-initializing a PHY shared by multiple ports.
STATIC BOOLEAN  mPhyPowered[16];

STATIC
VOID
PcieApplyDefaultPinctrlState (
  IN UINT32        PortId,
  IN CONST CHAR8   *StateName
  )
{
  EFI_STATUS                Status;
  SILICON_PINCTRL_PROTOCOL  *PinCtrl;

  PinCtrl = NULL;
  Status  = gBS->LocateProtocol (
                   &gSpacemitSiliconPinCtrlProtocolGuid,
                   NULL,
                   (VOID **)&PinCtrl
                   );
  if (EFI_ERROR (Status) || (PinCtrl == NULL)) {
    DEBUG ((DEBUG_WARN, "PcieHwInit: PinCtrl protocol not available, skipping pinctrl state\n"));
    return;
  }

  if (PinCtrl->ApplyStateById == NULL) {
    DEBUG ((DEBUG_WARN, "PcieHwInit: pinctrl state API is unavailable\n"));
    return;
  }

  Status = PinCtrl->ApplyStateById (
                     PinCtrl,
                     "pcie",
                     PortId,
                     NULL,
                     StateName
                     );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DEBUG_WARN, "PcieHwInit: pinctrl default state map is not available for port %u\n", PortId));
      return;
    }

    DEBUG ((DEBUG_WARN, "PcieHwInit: failed to apply pinctrl default state for port %u: %r\n", PortId, Status));
  }
}

STATIC
EFI_STATUS
K3PcieLoadRegsFromPcd (
  IN UINT32  Index
  )
{
  CONST K3_PCIE_PORT_CONFIG_ARRAY  *PortCfg;
  CONST K3_PCIE_PORT_RESOURCE      *Res;
  UINT32                            I;

  PortCfg = (CONST K3_PCIE_PORT_CONFIG_ARRAY *)PcdGetPtr (PcdK3PciePortConfigs);
  if ((PortCfg == NULL) || (PortCfg->Num <= Index)) {
    return EFI_NOT_FOUND;
  }

  Res = &PortCfg->Port[Index];

  mK3PcieReg[Index].Ctrl             = (UINT32)Res->AppBase;
  mK3PcieReg[Index].PhyAhb           = (UINT32)Res->PhyAhbBase;
  mK3PcieReg[Index].Mgmt             = (UINT32)K3_PCIE_MGMT_BASE;
  mK3PcieReg[Index].PortId           = Res->PortId;
  mK3PcieReg[Index].DeviceDetectGpio = Res->DeviceDetectGpio;
  mK3PcieReg[Index].DeviceDetectActive = Res->DeviceDetectActiveLevel;
  mK3PcieReg[Index].NumPhys          = Res->NumPhys;
  if (mK3PcieReg[Index].NumPhys > K3_PCIE_MAX_PHYS_PER_PORT) {
    mK3PcieReg[Index].NumPhys = K3_PCIE_MAX_PHYS_PER_PORT;
  }

  for (I = 0; I < K3_PCIE_MAX_PHYS_PER_PORT; I++) {
    mK3PcieReg[Index].PhyIndex[I] = Res->PhyIndex[I];
  }

  DEBUG ((DEBUG_INFO, "%a: Ctrl%u PortId%u app=0x%x phy_ahb=0x%x mgmt=0x%x phys=%u\n",
          __func__,
          Index,
          mK3PcieReg[Index].PortId,
          mK3PcieReg[Index].Ctrl,
          mK3PcieReg[Index].PhyAhb,
          mK3PcieReg[Index].Mgmt,
          mK3PcieReg[Index].NumPhys));

  return EFI_SUCCESS;
}

STATIC inline UINT32
K3PcieCtrlRead32 (
  IN UINT32  Port,
  IN UINT32  Offset
  )
{
  return MmioRead32 (mK3PcieReg[Port].Ctrl + Offset);
}

STATIC inline void
K3PcieCtrlWrite32 (
  IN UINT32  Port,
  IN UINT32  Offset,
  IN UINT32  Value
  )
{
  MmioWrite32 (mK3PcieReg[Port].Ctrl + Offset, Value);
}

STATIC inline UINT32
K3PcieDbiRead32 (
  IN UINT32  Port,
  IN UINT32  Offset
  )
{
  ASSERT (mDwPcies[Port] != NULL);
  return DwPcieReadDbi32 (mDwPcies[Port], Offset);
}

STATIC inline void
K3PcieDbiWrite32 (
  IN UINT32  Port,
  IN UINT32  Offset,
  IN UINT32  Value
  )
{
  ASSERT (mDwPcies[Port] != NULL);
  DwPcieWriteDbi32 (mDwPcies[Port], Offset, Value);
}

STATIC inline UINT32
K3PciePhyAhbRead32 (
  IN UINT32  Port,
  IN UINT32  Offset
  )
{
  return MmioRead32 (mK3PcieReg[Port].PhyAhb + Offset);
}

STATIC inline void
K3PciePhyAhbWrite32 (
  IN UINT32  Port,
  IN UINT32  Offset,
  IN UINT32  Value
  )
{
  MmioWrite32 (mK3PcieReg[Port].PhyAhb + Offset, Value);
}

STATIC inline void
PcieDbiWriteEnable (
  IN UINT32   Port,
  IN BOOLEAN  En
  )
{
  UINT32  Val;

  Val = K3PcieDbiRead32 (Port, DW_PCIE_MISC_CONTROL_1_OFF);
  if (En) {
    Val |= DW_PCIE_DBI_RO_WR_EN;
  } else {
    Val &= ~DW_PCIE_DBI_RO_WR_EN;
  }

  K3PcieDbiWrite32 (Port, DW_PCIE_MISC_CONTROL_1_OFF, Val);
}

STATIC
VOID
PcieEqPreset (
  IN UINT32  Port
  )
{
  UINT32  Val;

  Val  = K3PcieDbiRead32 (Port, DW_PCIE_GEN3_EQ_CONTROL_OFF);
  Val &= ~(0xFFFFU << 8);
  Val |= ((0x1U << 7) << 8);
  K3PcieDbiWrite32 (Port, DW_PCIE_GEN3_EQ_CONTROL_OFF, Val);
}

INT32
GetLinkSpeed (
  IN UINT32  Port
  )
{
  return (K3PcieDbiRead32 (Port, PCIE_LINK_STATUS_REG) & PCIE_LINK_STATUS_SPEED_MASK) >>
         PCIE_LINK_STATUS_SPEED_OFF;
}

INT32
GetLinkWidth (
  IN UINT32  Port
  )
{
  return (K3PcieDbiRead32 (Port, PCIE_LINK_STATUS_REG) & PCIE_LINK_STATUS_WIDTH_MASK) >>
         PCIE_LINK_STATUS_WIDTH_OFF;
}

/**
 * SetMaxLinkWidth() - Configure maximum link width
 *
 * @Port: PCIe port number
 * @NumLanes: Number of lanes (1, 2, 4, or 8)
 *
 * Configure the maximum link width in the PCIe root complex.
 * This function configures three registers:
 * 1. PCIE_PORT_LINK_CONTROL - Port link mode
 * 2. PCIE_LINK_WIDTH_SPEED_CONTROL - Link width configuration
 * 3. PCI_EXP_LNKCAP - PCIe Capability Maximum Link Width
 */
STATIC VOID
SetMaxLinkWidth (
  IN UINT32  Port,
  IN UINT32  NumLanes
  )
{
  UINT32  Plc, Lwsc, LnkCap;
  UINT32  PlcMode, LwscWidth, LnkCapWidth;

  if (NumLanes == 0) {
    return;
  }

  /* Determine register values based on number of lanes */
  switch (NumLanes) {
    case 1:
      PlcMode     = PORT_LINK_MODE_1_LANES;
      LwscWidth   = PORT_LOGIC_LINK_WIDTH_1_LANES;
      LnkCapWidth = 1 << 4;           // Shift to bit[9:4]
      break;
    case 2:
      PlcMode     = PORT_LINK_MODE_2_LANES;
      LwscWidth   = PORT_LOGIC_LINK_WIDTH_2_LANES;
      LnkCapWidth = 2 << 4;
      break;
    case 4:
      PlcMode     = PORT_LINK_MODE_4_LANES;
      LwscWidth   = PORT_LOGIC_LINK_WIDTH_4_LANES;
      LnkCapWidth = 4 << 4;
      break;
    case 8:
      PlcMode     = PORT_LINK_MODE_8_LANES;
      LwscWidth   = PORT_LOGIC_LINK_WIDTH_8_LANES;
      LnkCapWidth = 8 << 4;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "SetMaxLinkWidth: Invalid num-lanes %u\n", NumLanes));
      return;
  }

  /* Configure PCIE_PORT_LINK_CONTROL register (0x710) */
  Plc  = K3PcieDbiRead32 (Port, PCIE_PORT_LINK_CONTROL);
  Plc &= ~PORT_LINK_FAST_LINK_MODE;      // Clear fast link mode
  Plc |= PORT_LINK_DLL_LINK_EN;          // Enable DLL link
  Plc &= ~PORT_LINK_MODE_MASK;           // Clear lane mode bits
  Plc |= PlcMode;                        // Set lane mode
  K3PcieDbiWrite32 (Port, PCIE_PORT_LINK_CONTROL, Plc);

  /* Configure PCIE_LINK_WIDTH_SPEED_CONTROL register (0x80C) */
  Lwsc  = K3PcieDbiRead32 (Port, PCIE_LINK_WIDTH_SPEED_CONTROL);
  Lwsc &= ~PORT_LOGIC_LINK_WIDTH_MASK;      // Clear link width bits
  Lwsc |= LwscWidth;                        // Set link width
  K3PcieDbiWrite32 (Port, PCIE_LINK_WIDTH_SPEED_CONTROL, Lwsc);

  /* Configure PCI Express Capability Link Capabilities register */
  LnkCap  = K3PcieDbiRead32 (Port, PCIE_LINK_CAPABILITY + PCI_EXP_LNKCAP);
  LnkCap &= ~PCI_EXP_LNKCAP_MLW_MASK;      // Clear Maximum Link Width
  LnkCap |= LnkCapWidth;                   // Set Maximum Link Width
  K3PcieDbiWrite32 (Port, PCIE_LINK_CAPABILITY + PCI_EXP_LNKCAP, LnkCap);

  DEBUG ((DEBUG_INFO, "SetMaxLinkWidth: Configured x%u lanes\n", NumLanes));
}

/**
 * PciePreLinkConfigure() - Configure link capabilities and speed
 *
 * @regs_base: A pointer to the PCIe controller registers
 * @CapSpeed: The capabilities and speed to configure
 *
 * Configure the link capabilities and speed in the PCIe root complex.
 */
STATIC void
PciePreLinkConfigure (
  IN UINT32  Port,
  IN UINT32  CapSpeed
  )
{
  UINT32  Val;

  PcieDbiWriteEnable (Port, TRUE);

  Val  = K3PcieDbiRead32 (Port, PCIE_LINK_CAPABILITY);
  Val &= ~TARGET_LINK_SPEED_MASK;
  Val |= CapSpeed;
  K3PcieDbiWrite32 (Port, PCIE_LINK_CAPABILITY, Val);

  Val  = K3PcieDbiRead32 (Port, PCIE_LINK_CTL_2);
  Val &= ~TARGET_LINK_SPEED_MASK;
  Val |= CapSpeed;
  K3PcieDbiWrite32 (Port, PCIE_LINK_CTL_2, Val);

  /* Configure link width (driven by DT/PCD num-lanes) */
  SetMaxLinkWidth (Port, mDwPcies[Port]->NumLanes);

  PcieDbiWriteEnable (Port, FALSE);
}

/**
 * IsLinkUp() - Return the link state
 *
 * @Port: PCIe port number
 *
 * Return: TRUE for active link and FALSE for no link
 *
 * Check link status by reading PHY AHB LINK_STS register.
 * Both SMLH (SubModule Link Handler) and RDLH (Receive Data Link Handler)
 * must report link up for the link to be considered established.
 */
STATIC BOOLEAN
IsLinkUp (
  IN UINT32  Port
  )
{
  UINT32  Val;
  UINT32  Debug1Val;

  Val = K3PciePhyAhbRead32 (Port, K3_PHY_AHB_LINK_STS);

  /* Also read PCIE_PORT_DEBUG1 for debugging (not used for return value) */
  Debug1Val = K3PcieDbiRead32 (Port, PCIE_PORT_DEBUG1);
  DEBUG ((DEBUG_VERBOSE,
          "[PCIe%u] IsLinkUp: PHY_AHB=0x%08X [RDLH=%u SMLH=%u], DEBUG1=0x%08X [LINK_UP=%u IN_TRAINING=%u]\n",
          Port, Val, (Val >> 12) & 1, (Val >> 1) & 1,
          Debug1Val, (Debug1Val >> 4) & 1, (Debug1Val >> 29) & 1));

  return ((Val & RDLH_LINK_UP) && (Val & SMLH_LINK_UP));
}

/**
 * WaitLinkUp() - Wait for the link to come up
 *
 * @regs_base: A pointer to the PCIe controller registers
 *
 * Return: 1 (true) for active line and 0 (false) for no link (timeout)
 */
STATIC INT32
WaitLinkUp (
  IN UINT32  Port
  )
{
  INT32  Timeout;

  Timeout = PCIE_LINK_UP_TIMEOUT_MS;
  while (!IsLinkUp (Port)) {
    if (Timeout <= 0) {
      return 0;
    }

    gBS->Stall (1 * 1000);
    Timeout--;
  }

  return 1;
}

INT32
PcieLinkUp (
  IN UINT32  Port,
  IN UINT32  CapSpeed
  )
{
  UINT32  Val;

  if (IsLinkUp (Port)) {
    DEBUG ((DEBUG_INFO, "PCI Link already up before configuration!\n"));
    return 1;
  }

  /* DW pre link configurations */
  PciePreLinkConfigure (Port, CapSpeed);

  /* Initiate link training */
  Val  = K3PcieCtrlRead32 (Port, PCIE_CLK_RESET_CTRL);
  Val |= LTSSM_EN;
  Val &= ~APP_HOLD_PHY_RST;
  K3PcieCtrlWrite32 (Port, PCIE_CLK_RESET_CTRL, Val);

  /* Debug: Print register values */
  DEBUG (
         (DEBUG_INFO, "PCIE_CLK_RESET_CTRL after LTSSM_EN: 0x%x (prints 0x40000338)\n",
          Val)
         );
  Val = K3PcieDbiRead32 (Port, PCI_COMMAND_OFFSET);
  DEBUG ((DEBUG_INFO, "Command register before WaitLinkUp: 0x%04x\n", Val & 0xFFFF));

  /* Check that link was established */
  if (!WaitLinkUp (Port)) {
    Val = K3PcieDbiRead32 (Port, PCI_COMMAND_OFFSET);
    DEBUG ((DEBUG_ERROR, "Link down, Command register: 0x%04x\n", Val & 0xFFFF));
    return 0;
  }

  /* Debug: Check Command register after link up */
  Val = K3PcieDbiRead32 (Port, PCI_COMMAND_OFFSET);
  DEBUG ((DEBUG_INFO, "Command register after WaitLinkUp: 0x%04x\n", Val & 0xFFFF));

  /*
   * Link can be established in Gen 1. Still need to wait
   * for MAC speed negotiation to complete.
   * Check if link speed has stabilized before full wait.
   */
  UINT32  InitialSpeed, CurrentSpeed;
  UINT32  StableCount   = 0;
  UINT32  CheckInterval = 10000;      // 10ms
  UINT32  MaxWait       = PCIE_LINK_SPEED_WAIT_US;
  UINT32  Elapsed       = 0;

  InitialSpeed = GetLinkSpeed (Port);
  while (Elapsed < MaxWait) {
    gBS->Stall (CheckInterval);
    Elapsed += CheckInterval;

    CurrentSpeed = GetLinkSpeed (Port);
    if (CurrentSpeed == InitialSpeed) {
      StableCount++;
      // If speed stable for 50ms, negotiation likely complete
      if (StableCount >= 5) {
        DEBUG (
               (DEBUG_INFO, "Link speed stabilized at Gen%d after %dus\n",
                CurrentSpeed, Elapsed)
               );
        break;
      }
    } else {
      InitialSpeed = CurrentSpeed;
      StableCount  = 0;
    }
  }

  return 1;
}

/**
  Perform PCIE slot reset using external GPIO pin.

  @param [in] *PcieResetGpio  GPIO pin description.

  @retval EFI_SUCEESS         PCIE slot reset succeeded.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
PcieResetSlot (
  IN UINT32  Port
  )
{
  UINT32  Val;

  //
  // Assert fundamental reset (PERST#)
  // - Enable PERST# output
  // - Drive it low
  //
  Val  = K3PcieCtrlRead32 (Port, PCIE_CTRL_LOGIC);
  Val |= PCIE_PERSTN_OE;
  Val |= PCIE_PERSTN_OUT;
  K3PcieCtrlWrite32 (Port, PCIE_CTRL_LOGIC, Val);

  Val  = K3PcieCtrlRead32 (Port, PCIE_CTRL_LOGIC);
  Val &= ~PCIE_PERSTN_OUT;
  K3PcieCtrlWrite32 (Port, PCIE_CTRL_LOGIC, Val);

  DEBUG ((DEBUG_INFO, "PERST# pulled low (Port %d)\n", Port));

  return EFI_SUCCESS;
}

STATIC
VOID
PcieHostInit (
  IN UINT32  Port
  )
{
  UINT32  Val;

  /* Wait 100ms for power and clock stabilization */
  gBS->Stall (100 * 1000);

  /* Deassert PERST# (drive high). */
  Val  = K3PcieCtrlRead32 (Port, PCIE_CTRL_LOGIC);
  Val |= PCIE_PERSTN_OUT;
  K3PcieCtrlWrite32 (Port, PCIE_CTRL_LOGIC, Val);

  DEBUG ((DEBUG_INFO, "PERST# released high (Port %d)\n", Port));
}

/**
  Enable PCIe clock via ClockCtrl Protocol.

  @param[in]  Port     PCIe port index.
  @param[in]  PortId   PCIe port ID (used for clock name).
**/
STATIC VOID
PcieEnableClock (
  IN UINT32  Port,
  IN UINT32  PortId
  )
{
  EFI_STATUS                  Status;
  SILICON_CLOCKCTRL_PROTOCOL  *ClockCtrlProtocol;
  CHAR8                       ClockName[16];
  UINT32                      Val;

  AsciiSPrint (ClockName, sizeof (ClockName), "PCIE%d", PortId);

  Status = gBS->LocateProtocol (
                   &gSpacemitSiliconClockCtrlProtocolGuid,
                   NULL,
                   (VOID **)&ClockCtrlProtocol
                   );
  if (!EFI_ERROR (Status) && (ClockCtrlProtocol != NULL) &&
      (ClockCtrlProtocol->SetClockState != NULL))
  {
    ClockCtrlProtocol->SetClockState (
                        ClockCtrlProtocol,
                        ClockName,
                        ENABLE_CLOCK
                        );

    Val = K3PcieCtrlRead32 (Port, PCIE_CLK_RESET_CTRL);
    DEBUG (
           (DEBUG_INFO,
            "PCIe%u: PCIE_CLK_RESET_CTRL(after ClockCtrl enable)=0x%08x\n",
            PortId,
            Val)
           );
  } else {
    DEBUG (
           (DEBUG_WARN,
            "PCIe%d: ClockCtrl Protocol unavailable, falling back to direct register writes\n",
            PortId)
           );
  }
}

STATIC VOID
PcieHwInit (
  IN UINT32  Port
  )
{
  UINT32  Val;
  UINT32  PortId;

  ASSERT (mK3PcieReg != NULL);
  PortId = mK3PcieReg[Port].PortId;

  //
  // Map PCIe controller wrapper (app) and PHY AHB (link) registers.
  // The DT regions are small and not necessarily page-aligned, map by 4KB pages.
  //
  MapRegToGcdMmioSpace (mK3PcieReg[Port].Ctrl, 0x100);
  MapRegToGcdMmioSpace (mK3PcieReg[Port].PhyAhb, SIZE_4KB);
  if (mK3PcieReg[Port].Mgmt != 0) {
    MapRegToGcdMmioSpace (mK3PcieReg[Port].Mgmt, sizeof (UINT32));
  }

  /* Configure PCIe pins using board map (controller="pcie", id=PortId, state="default"). */
  PcieApplyDefaultPinctrlState (PortId, PcdGetPtr (PcdPcieHostPinState));

  PcieEnableClock (Port, PortId);

  /* hold the ltssm in detect quiet */
  Val  = K3PcieCtrlRead32 (Port, PCIE_CLK_RESET_CTRL);
  Val &= ~LTSSM_EN;
  K3PcieCtrlWrite32 (Port, PCIE_CLK_RESET_CTRL, Val);
}

//
// K3 PCIe lane/port mux management.
//
#define PORTA_MODE_MASK          (BIT4 | BIT3)
#define PORTA_MODE_X8            (0)
#define PORTA_MODE_X4            (BIT4)
#define PORTA_MODE_X2_PORTB_X2   (BIT4 | BIT3)

#define PORTC_LANE_MASK          (BIT4 | BIT2 | BIT1)
#define PORTC_MODE_X2            (BIT4 | (0))
#define PORTC_MODE_X1_PHY2       (BIT4 | BIT1)
#define PORTC_MODE_X1_PHY3       (BIT4 | BIT2)

#define PORTD_LANE_MASK          (BIT4 | BIT0)
#define PORTD_MODE_PCIE          (BIT4 | (0))

#define PORTE_LANE_MASK          (BIT4)
#define PORTE_MODE_PCIE          (BIT4)

STATIC
INT32
K3PcieFindIndexByPortId (
  IN UINT32  PortId
  )
{
  UINTN  Index;

  if (mK3PcieReg == NULL) {
    return -1;
  }

  for (Index = 0; Index < mDwPciesCount; Index++) {
    if (mK3PcieReg[Index].PortId == PortId) {
      return (INT32)Index;
    }
  }

  return -1;
}

STATIC
UINT32
K3PcieGetPrimaryPhyId (
  IN UINT32  Port
  )
{
  CONST K3_PCIE_PHY_CONFIG_ARRAY  *PhyCfg;
  UINT32                          PhyIndex;

  if ((mK3PcieReg == NULL) || (mK3PcieReg[Port].NumPhys == 0)) {
    return 2;
  }

  PhyCfg = (CONST K3_PCIE_PHY_CONFIG_ARRAY *)PcdGetPtr (PcdK3PciePhyConfigs);
  if ((PhyCfg == NULL) || (PhyCfg->Num == 0)) {
    return 2;
  }

  PhyIndex = mK3PcieReg[Port].PhyIndex[0];
  if (PhyIndex >= PhyCfg->Num) {
    return 2;
  }

  return PhyCfg->Phy[PhyIndex].PhyId;
}

STATIC
EFI_STATUS
K3PcieMapLanes (
  IN UINT32  Port
  )
{
  UINT32      PortId;
  UINT32      NumLanes;
  UINT32      Mask;
  UINT32      Val;
  UINT32      Tmp;

  ASSERT (mK3PcieReg != NULL);
  if (mK3PcieReg[Port].Mgmt == 0) {
    return EFI_SUCCESS;
  }

  PortId = mK3PcieReg[Port].PortId;
  NumLanes = (mDwPcies[Port] != NULL) ? mDwPcies[Port]->NumLanes : 0;
  if (NumLanes == 0) {
    NumLanes = 1;
  }

  Mask = 0;
  Val  = 0;

  switch (PortId) {
    case 0:
      Mask = PORTA_MODE_MASK;
      if (mPortABSplitEnabled) {
        // Force x2 split when Port B is enabled.
        Val = PORTA_MODE_X2_PORTB_X2;
        if (mDwPcies[Port] != NULL) {
          mDwPcies[Port]->NumLanes = 2;
        }
        // when Port A is forced to x2, only init one PHY.
        if (mK3PcieReg[Port].NumPhys > 1) {
          mK3PcieReg[Port].NumPhys = 1;
        }
      } else if (NumLanes == 8) {
        Val = PORTA_MODE_X8;
      } else if (NumLanes == 4) {
        Val = PORTA_MODE_X4;
      } else if (NumLanes == 2) {
        Val = PORTA_MODE_X2_PORTB_X2;
      } else {
        return EFI_INVALID_PARAMETER;
      }
      break;

    case 1:
      //
      // Port A shares lanes with Port B. If Port A is x4/x8, Port B must be disabled.
      //
      if (!mPortABSplitEnabled) {
        return EFI_UNSUPPORTED;
      }

      Mask = PORTA_MODE_MASK;
      Val  = PORTA_MODE_X2_PORTB_X2;
      break;

    case 2:
      Mask = PORTC_LANE_MASK;
      if (NumLanes == 2) {
        Val = PORTC_MODE_X2;
      } else if (NumLanes == 1) {
        Val = (K3PcieGetPrimaryPhyId (Port) == 2) ? PORTC_MODE_X1_PHY2 : PORTC_MODE_X1_PHY3;
      } else {
        return EFI_INVALID_PARAMETER;
      }
      break;

    case 3:
      Mask = PORTD_LANE_MASK;
      if (NumLanes == 1) {
        Val = PORTD_MODE_PCIE;
      } else {
        return EFI_INVALID_PARAMETER;
      }
      break;

    case 4:
      Mask = PORTE_LANE_MASK;
      if (NumLanes == 1) {
        Val = PORTE_MODE_PCIE;
      } else {
        return EFI_INVALID_PARAMETER;
      }
      break;

    default:
      return EFI_INVALID_PARAMETER;
  }

  Tmp  = MmioRead32 (mK3PcieReg[Port].Mgmt);
  Tmp &= ~Mask;
  Tmp |= Val;
  MmioWrite32 (mK3PcieReg[Port].Mgmt, Tmp);

  DEBUG ((DEBUG_INFO, "K3PcieMapLanes: PortId %u NumLanes %u -> mgmt=0x%x\n", PortId, NumLanes, Tmp));
  return EFI_SUCCESS;
}

//
// PHY init/power-on sequence
//
#define APB_SPARE31_REG_OFF  0x0
#define APB_SPARE32_REG_OFF  0x4

STATIC
UINT32
PhyRead32 (
  IN UINT64  Base,
  IN UINT32  Offset
  )
{
  return MmioRead32 ((UINTN)Base + Offset);
}

STATIC
VOID
PhyWrite32 (
  IN UINT64  Base,
  IN UINT32  Offset,
  IN UINT32  Value
  )
{
  MmioWrite32 ((UINTN)Base + Offset, Value);
}

STATIC
VOID
PhyModBit (
  IN UINT64  Base,
  IN UINT32  Offset,
  IN UINT32  Mask,
  IN BOOLEAN Set
  )
{
  UINT32  Value;

  Value = PhyRead32 (Base, Offset);
  Value &= ~Mask;
  if (Set) {
    Value |= Mask;
  }
  PhyWrite32 (Base, Offset, Value);
}

STATIC
VOID
InitX1Phy (
  IN UINT64  PhyBase
  )
{
  UINT32  RdData;
  UINT32  RxRegs[7] = { 0x10, 0x78, 0x98, 0xdf, 0xb4, 0x88, 0x28 };
  INT32   Id;
  INT32   Lsh;

#ifndef PCIE_100M_REF_CLK
  // select 24Mhz refclock input pll_reg2[7:4]=2
  RdData = PhyRead32 (PhyBase, (0x16 << 2));
  RdData &= 0xffff0fff;
  RdData |= 0x00002000;
  PhyWrite32 (PhyBase, (0x16 << 2), RdData);

  PhyModBit (PhyBase, (0x17 << 2), (0x1 << 21), FALSE);
  PhyModBit (PhyBase, (0x14 << 2), 0x3, FALSE);

#ifdef PCIE_REF_CLK_OUTPUT
  PhyModBit (PhyBase, (0x17 << 2), (0x1 << 20), TRUE);
  PhyWrite32 (PhyBase, (0x14 << 2), 0x00006505);
#endif
#endif

  // pll_reg1 of lane0, disable ssc pll_reg4[3:0]=4'h0
  RdData = PhyRead32 (PhyBase, (0x16 << 2));
  RdData &= 0xf0ffffff;
  PhyWrite32 (PhyBase, (0x16 << 2), RdData);

  PhyModBit (PhyBase, (0x10 << 2), (0x1 << 13), TRUE);

  // cdr fix bypass
  PhyModBit (PhyBase, 0x4, (0x1 << 6), FALSE);
  // dynamic lock
  PhyModBit (PhyBase, 0xC, (0x1 << 2), TRUE);

  // rx_reg 0~3 packed into 0x60
  for (Id = 0; Id < 4; Id++) {
    RdData = PhyRead32 (PhyBase, 0x60);
    Lsh = Id * 8;
    RdData &= ~(0xffU << Lsh);
    RdData |= (RxRegs[Id] << Lsh);
    PhyWrite32 (PhyBase, 0x60, RdData);
  }

  // rx_reg 4~6 packed into 0x64
  for (Id = 4; Id < 7; Id++) {
    RdData = PhyRead32 (PhyBase, 0x64);
    Lsh = (Id - 4) * 8;
    RdData &= ~(0xffU << Lsh);
    RdData |= (RxRegs[Id] << Lsh);
    PhyWrite32 (PhyBase, 0x64, RdData);
  }

  // cfg_sw_phy_init_done
  PhyModBit (PhyBase, (0x02 << 2), (0x1 << 11), TRUE);
}

STATIC
VOID
InitX2Phy (
  IN UINT64  PhyBase
  )
{
  UINT32  RdData;
  UINT32  RxRegs[7] = { 0x10, 0x78, 0x98, 0xdf, 0xb4, 0x88, 0x28 };
  INT32   I;
  INT32   Id;
  INT32   Lsh;
  UINT64  LaneBase;

#ifndef PCIE_100M_REF_CLK
  // select 24Mhz refclock input pll_reg2[7:4]=2
  RdData = PhyRead32 (PhyBase, (0x16 << 2));
  RdData &= 0xffff0fff;
  RdData |= 0x00002000;
  PhyWrite32 (PhyBase, (0x16 << 2), RdData);

  PhyModBit (PhyBase, (0x17 << 2), (0x1 << 21), FALSE);
  for (I = 0; I < 2; I++) {
    PhyModBit (PhyBase + (0x400 * I), (0x14 << 2), 0x3, FALSE);
  }

#ifdef PCIE_REF_CLK_OUTPUT
  PhyModBit (PhyBase, (0x17 << 2), (0x1 << 20), TRUE);
  PhyWrite32 (PhyBase, (0x14 << 2), 0x00006505);
#endif
#endif

  // pll_reg1 of lane0, disable ssc pll_reg4[3:0]=4'h0
  RdData = PhyRead32 (PhyBase, (0x16 << 2));
  RdData &= 0xf0ffffff;
  PhyWrite32 (PhyBase, (0x16 << 2), RdData);

  for (I = 0; I < 2; I++) {
    LaneBase = PhyBase + (0x400 * I);
    PhyModBit (LaneBase, (0x10 << 2), (0x1 << 13), TRUE);
    // cdr fix bypass
    PhyModBit (LaneBase, 0x4, (0x1 << 6), FALSE);
    // dynamic lock
    PhyModBit (LaneBase, 0xC, (0x1 << 2), TRUE);
  }

  for (I = 0; I < 2; I++) {
    LaneBase = PhyBase + (0x400 * I);

    for (Id = 0; Id < 4; Id++) {
      RdData = PhyRead32 (LaneBase, 0x60);
      Lsh = Id * 8;
      RdData &= ~(0xffU << Lsh);
      RdData |= (RxRegs[Id] << Lsh);
      PhyWrite32 (LaneBase, 0x60, RdData);
    }

    for (Id = 4; Id < 7; Id++) {
      RdData = PhyRead32 (LaneBase, 0x64);
      Lsh = (Id - 4) * 8;
      RdData &= ~(0xffU << Lsh);
      RdData |= (RxRegs[Id] << Lsh);
      PhyWrite32 (LaneBase, 0x64, RdData);
    }
  }

  for (I = 0; I < 2; I++) {
    LaneBase = PhyBase + (0x400 * I);
    // cfg_sw_phy_init_done
    PhyModBit (LaneBase, (0x02 << 2), (0x1 << 11), TRUE);
  }
}

STATIC
EFI_STATUS
WaitPhyPllLock (
  IN UINT64  PhyBase
  )
{
  UINT32  RdData;
  UINT32  TimeoutUs;

  TimeoutUs = 1000000; // 1s
  while (TimeoutUs-- > 0) {
    RdData = PhyRead32 (PhyBase, 0x8);
    if (RdData & 0x1) {
      return EFI_SUCCESS;
    }
    gBS->Stall (1);
  }

  DEBUG ((DEBUG_ERROR, "PHY PLL Lock Timeout! Status: 0x%x\n", RdData));
  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
SpacemitPciePhyInit (
  IN UINT64  ApbSpareBase
  )
{
  // apb_spare31[17] = 1
  PhyModBit (ApbSpareBase, APB_SPARE31_REG_OFF, (0x1 << 17), TRUE);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitPciePhyPowerOn (
  IN UINT64  PhyBase,
  IN UINT64  ApbSpareBase,
  IN UINT32  NumLanes
  )
{
  UINT32  RdData;
  INT32   Timeout;

  // Wait rcal done: apb_spare32[8]
  Timeout = 100;
  do {
    gBS->Stall (10 * 1000);
    RdData = PhyRead32 (ApbSpareBase, APB_SPARE32_REG_OFF) & (0x1 << 8);
    Timeout--;
    if (Timeout == 0) {
      break;
    }
  } while (!RdData);

  if (!RdData) {
    DEBUG ((DEBUG_WARN, "rcal timeout, trim override\n"));
    RdData = PhyRead32 (ApbSpareBase, APB_SPARE32_REG_OFF);
    RdData |= (0xaU << 20) | (0x6U << 24) | (0x7U << 28);
    PhyWrite32 (ApbSpareBase, APB_SPARE32_REG_OFF, RdData);

    RdData = PhyRead32 (ApbSpareBase, APB_SPARE32_REG_OFF);
    RdData |= (0x1U << 31);
    PhyWrite32 (ApbSpareBase, APB_SPARE32_REG_OFF, RdData);
  }

  if (NumLanes == 1) {
    InitX1Phy (PhyBase);
  } else {
    InitX2Phy (PhyBase);
  }

  return WaitPhyPllLock (PhyBase);
}

STATIC
EFI_STATUS
PciePhyInit (
  IN UINT32  Port
  )
{
  CONST K3_PCIE_PHY_CONFIG_ARRAY  *PhyCfg;
  EFI_STATUS                      Status;
  UINT32                          Val;
  UINT32                          I;
    UINT64  ApbAddr;

  ASSERT (mK3PcieReg != NULL);

  PhyCfg = (CONST K3_PCIE_PHY_CONFIG_ARRAY *)PcdGetPtr (PcdK3PciePhyConfigs);
  if ((PhyCfg == NULL) || (PhyCfg->Num == 0)) {
    DEBUG ((DEBUG_ERROR, "PciePhyInit: PHY configs not found\n"));
    return EFI_NOT_FOUND;
  }

  // Release HOLD_PHY_RST before lane mux and PHY init.
  Val  = K3PcieCtrlRead32 (Port, PCIE_CLK_RESET_CTRL);
  Val &= ~APP_HOLD_PHY_RST;
  K3PcieCtrlWrite32 (Port, PCIE_CLK_RESET_CTRL, Val);

  // Lane mux management must be configured before PHY init/power-on.
  Status = K3PcieMapLanes (Port);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "PciePhyInit: skip controller %u (PortId %u): map lanes failed: %r\n",
            Port, mK3PcieReg[Port].PortId, Status));
    return Status;
  }

    ApbAddr  = 0xD4090178ULL;  // Hardcoded ApbSpareBase
    MapRegToGcdMmioSpace (ApbAddr, sizeof (UINT32) * 2);

  for (I = 0; I < mK3PcieReg[Port].NumPhys; I++) {
    UINT32  PhyIndex;
    UINT64  PhyAddr;
    UINT32  NumLanes;

    PhyIndex = mK3PcieReg[Port].PhyIndex[I];
    if (PhyIndex >= PhyCfg->Num) {
      DEBUG ((DEBUG_WARN, "PciePhyInit: Port%u invalid PhyIndex %u\n", Port, PhyIndex));
      continue;
    }

    if ((PhyIndex < ARRAY_SIZE (mPhyPowered)) && mPhyPowered[PhyIndex]) {
      continue;
    }

    PhyAddr  = PhyCfg->Phy[PhyIndex].PhyBase;
    NumLanes = PhyCfg->Phy[PhyIndex].NumLanes;
    if ((NumLanes != 1) && (NumLanes != 2)) {
      NumLanes = 1;
    }

    MapRegToGcdMmioSpace (PhyAddr, SIZE_4KB);

    DEBUG ((DEBUG_INFO, "PciePhyInit: Port%u -> phy%u@0x%lx lanes=%u apb_spare@0x%lx\n",
            Port, PhyIndex, PhyAddr, NumLanes, ApbAddr));

    Status = SpacemitPciePhyInit (ApbAddr);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = SpacemitPciePhyPowerOn (PhyAddr, ApbAddr, NumLanes);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (PhyIndex < ARRAY_SIZE (mPhyPowered)) {
      mPhyPowered[PhyIndex] = TRUE;
    }
  }

  return EFI_SUCCESS;
}

/**
  Set PCIe to RC (Root Complex) mode

  K3 Changes:
  1. Added PCIE_AUX_PWR_DET (BIT9) setting
  2. PCIE_IGNORE_PERSTN is now at BIT31 (K1 was BIT2)

  @param[in] Port  PCIe port number
**/
STATIC void
PcieSetRCMode (
  IN UINT32  Port
  )
{
  UINT32  Val;

  /* Set RC mode and enable auxiliary power detection (K3 specific) */
  Val = K3PcieCtrlRead32 (Port, PCIE_CLK_RESET_CTRL);
  // Val |= DEVICE_TYPE_RC;      // BIT31: 0=RC, 1=EP
  Val |= PCIE_AUX_PWR_DET;       // BIT9: K3 specific, auxiliary power detection
  K3PcieCtrlWrite32 (Port, PCIE_CLK_RESET_CTRL, Val);

  DEBUG ((DEBUG_INFO, "Set RC mode with AUX_PWR_DET (Port %d)\n", Port));

  /* Ignore PERSTN input (K3: BIT31, K1 was BIT2) */
  Val  = K3PcieCtrlRead32 (Port, PCIE_CTRL_LOGIC);
  Val |= PCIE_IGNORE_PERSTN;       // K3: BIT31
  K3PcieCtrlWrite32 (Port, PCIE_CTRL_LOGIC, Val);

  DEBUG ((DEBUG_INFO, "IGNORE_PERSTN enabled (BIT31)\n"));
}

void
PcieSetupHost (
  IN UINT32  Port
  )
{
  UINT32  Val;

  /* setup RC BARs */
  K3PcieDbiWrite32 (Port, PCI_BASE_ADDRESSREG_OFFSET, PCI_BASE_ADDRESS_MEM_TYPE_64);
  K3PcieDbiWrite32 (Port, PCI_BASE_ADDRESSREG_OFFSET + 4, 0);

  /* setup interrupt pins */
  Val  = K3PcieDbiRead32 (Port, PCI_INT_LINE_OFFSET);
  Val &= ~0xffff;
  Val |= 0x100;
  K3PcieDbiWrite32 (Port, PCI_INT_LINE_OFFSET, Val);

  /* setup bus numbers */
  Val  = K3PcieDbiRead32 (Port, PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET);
  Val &= ~0xffffff;
  Val |= 0xff0100;
  K3PcieDbiWrite32 (Port, PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET, Val);

  /* setup command register */
  Val  = K3PcieDbiRead32 (Port, PCI_COMMAND_OFFSET);
  Val &= ~0xffff;
  Val |= EFI_PCI_COMMAND_IO_SPACE | EFI_PCI_COMMAND_MEMORY_SPACE |
         EFI_PCI_COMMAND_BUS_MASTER | EFI_PCI_COMMAND_SERR;
  K3PcieDbiWrite32 (Port, PCI_COMMAND_OFFSET, Val);

  /* Enable write permission for the DBI read-only register */
  PcieDbiWriteEnable (Port, TRUE);
  /* program correct class for RC */
  Val  = K3PcieDbiRead32 (Port, PCI_REVISION_ID_OFFSET);
  Val &= ~0xffff0000;
  Val |= (PCI_CLASS_BRIDGE << 24) | (PCI_CLASS_BRIDGE_P2P << 16);
  K3PcieDbiWrite32 (Port, PCI_REVISION_ID_OFFSET, Val);

  /* Better disable write permission right after the update */
  PcieDbiWriteEnable (Port, FALSE);

  Val  = K3PcieDbiRead32 (Port, DW_PCIE_LINK_WIDTH_SPEED_CONTROL);
  Val |= DW_PCIE_PORT_LOGIC_SPEED_CHANGE;
  K3PcieDbiWrite32 (Port, DW_PCIE_LINK_WIDTH_SPEED_CONTROL, Val);

  Val  = K3PcieDbiRead32 (Port, PCI_CACHELINE_SIZE_OFFSET);
  Val &= ~0xFF;
  Val |= SYS_PCI_CACHE_LINE_SIZE;
  K3PcieDbiWrite32 (Port, PCI_CACHELINE_SIZE_OFFSET, Val);
}

STATIC
VOID
PcieInitId (
  IN UINT32  Port
  )
{
  PcieDbiWriteEnable (Port, TRUE);
  K3PcieDbiWrite32 (Port, PCI_VENDOR_ID_OFFSET, K3_PCIE_VENDOR_ID | (K3_PCIE_DEVICE_ID << 16));
  PcieDbiWriteEnable (Port, FALSE);
}

/**
  Obtain resources and perform a low-level PCIE controllers
  configuration.

  @param [in]  Port             PCIE root complex port number.

  @retval EFI_SUCEESS       PCIE configuration successful.
  @retval Other             Return error status.

**/
EFI_STATUS
EFIAPI
K3PciHostBridgeInit (
  UINTN  Port
  )
{
  UINT32      Val;
  UINT32      CapSpeed;
  EFI_STATUS  Status;

  ASSERT (Port < mDwPciesCount);
  ASSERT (mDwPcies != NULL);
  ASSERT (mDwPcies[Port] != NULL);
  ASSERT (mK3PcieReg != NULL);

  PcieHwInit (Port);
  Status = PciePhyInit (Port);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  PcieSetRCMode (Port);
  PcieResetSlot (Port);
  PcieHostInit (Port);
  PcieEqPreset (Port);
  PcieSetupHost (Port);
  PcieInitId (Port);

  CapSpeed = mDwPcies[Port]->MaxLinkSpeed;
  if ((CapSpeed < LINK_SPEED_GEN_1) || (CapSpeed > LINK_SPEED_GEN_3)) {
    CapSpeed = LINK_SPEED_GEN_3;
  }

  if (!PcieLinkUp (Port, CapSpeed)) {
    DEBUG ((DEBUG_ERROR, "%a: PCIE-%d: Link down\n", __FUNCTION__, Port));
  } else {
    DEBUG (
           (DEBUG_INFO, "%a: PCIE-%d: Link up (Gen%d-x%d)\n", __FUNCTION__, Port,
            GetLinkSpeed (Port), GetLinkWidth (Port))
           );
  }

  /* Final verification: Check Command register after all initialization */
  Val = K3PcieDbiRead32 (Port, PCI_COMMAND_OFFSET);
  DEBUG ((DEBUG_INFO, "[PCIe] === FINAL CHECK ===\n"));
  DEBUG ((DEBUG_INFO, "[PCIe] Command Reg final value: 0x%04X\n", Val & 0xFFFF));
  DEBUG ((DEBUG_INFO, "[PCIe]   BIT0 IO Space:      %d\n", (Val & PCI_COMMAND_IO) ? 1 : 0));
  DEBUG (
         (DEBUG_INFO, "[PCIe]   BIT1 Memory Space:  %d %s\n",
          (Val & PCI_COMMAND_MEMORY) ? 1 : 0,
          (Val & PCI_COMMAND_MEMORY) ? "OK" : "MISSING (REQUIRED)")
         );
  DEBUG (
         (DEBUG_INFO, "[PCIe]   BIT2 Bus Master:    %d %s\n",
          (Val & PCI_COMMAND_MASTER) ? 1 : 0,
          (Val & PCI_COMMAND_MASTER) ? "OK" : "MISSING (REQUIRED)")
         );
  DEBUG ((DEBUG_INFO, "[PCIe]   BIT8 SERR# Enable:  %d\n", (Val & PCI_COMMAND_SERR) ? 1 : 0));

  if ((Val & (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)) !=
      (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER))
  {
    DEBUG (
           (DEBUG_ERROR,
            "[PCIe] CRITICAL: Command register not properly configured!\n")
           );
    DEBUG (
           (DEBUG_ERROR,
            "[PCIe] Device will NOT work without Memory Space and Bus Master!\n")
           );
  }

  return EFI_SUCCESS;
}

/**
  Decide whether to split PortA lanes with PortB by reading the optional device-detect GPIO.
  If the GPIO read fails (or PortB is disabled), default to no split.
**/
STATIC
VOID
K3PcieInitPortABSplitPolicy (
  IN CONST PCI_ROOT_BRIDGE_RESOURCE_CONFIG_ARRAY  *RootBridgeResourceConfigs,
  IN CONST BOOLEAN                                *PortEnabled
  )
{
  INT32              PortAIndex;
  INT32              PortBIndex;
  EMBEDDED_GPIO      *Gpio;
  EMBEDDED_GPIO_PIN  DetectPin;
  UINTN              Value;
  EFI_STATUS         Status;

  mPortABSplitEnabled = FALSE;

  if ((mK3PcieReg == NULL) || (RootBridgeResourceConfigs == NULL)) {
    goto Done;
  }

  PortAIndex = K3PcieFindIndexByPortId (0);
  if (PortAIndex < 0) {
    goto Done;
  }

  PortBIndex = K3PcieFindIndexByPortId (1);
  if (PortBIndex < 0) {
    goto Done;
  }
  if (!PortEnabled[PortBIndex]) {
    goto Done;
  }

  if (mK3PcieReg[PortAIndex].DeviceDetectGpio != 0) {
    Status = gBS->LocateProtocol (&gEmbeddedGpioProtocolGuid, NULL, (VOID **)&Gpio);
    if (EFI_ERROR (Status)) {
      goto Done;
    }
    if (Gpio == NULL) {
      goto Done;
    }
    DetectPin = (EMBEDDED_GPIO_PIN)mK3PcieReg[PortAIndex].DeviceDetectGpio;
    Gpio->Set (Gpio, DetectPin, GPIO_MODE_INPUT);
    Status = Gpio->Get (Gpio, DetectPin, &Value);
    if (EFI_ERROR (Status)) {
      goto Done;
    }
    if (mK3PcieReg[PortAIndex].DeviceDetectActive != 0) {
      mPortABSplitEnabled = (Value != 0);
    } else {
      mPortABSplitEnabled = (Value == 0);
    }
  } else {
    mPortABSplitEnabled = TRUE;
  }

Done:
  DEBUG ((DEBUG_INFO, "K3Pcie: PortA/B split %a\n", mPortABSplitEnabled ? "ENABLED" : "DISABLED"));
}
/**
  Constructor to configure K1 PCI Host Bridge.

  @param  ImageHandle   The image handle.
  @param  SystemTable   The system table.

  @retval EFI_SUCCESS   Configure successfully.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
K3PcieHostBridgeLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                             Status;
  DW_PCIE_CONTROLLER_CONFIGS             *DwPcieControllerConfigs   = NULL;
  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_ARRAY  *RootBridgeResourceConfigs = NULL;
  UINTN                                  SegMax                     = 0;
  UINTN                                  Index;

  DwPcieControllerConfigs =
    (DW_PCIE_CONTROLLER_CONFIGS *)PcdGetPtr (PcdDwPcieControllerConfigTable);
  if (DwPcieControllerConfigs == NULL) {
    DEBUG ((DEBUG_ERROR, "(%a) DW PCIe controller configs not found\n", __func__));
    return EFI_UNSUPPORTED;
  }

  DEBUG (
         (DEBUG_INFO, "(%a) PCD Num=%u, Data[0].DbiBase=0x%lx\n", __func__,
          DwPcieControllerConfigs->Num, DwPcieControllerConfigs->Data[0].Reg.DbiBase)
         );

  RootBridgeResourceConfigs = (PCI_ROOT_BRIDGE_RESOURCE_CONFIG_ARRAY *)PcdGetPtr (
                                                                                  PcdBoardPciRootBridgeResourceConfigTable
                                                                                  );
  if (RootBridgeResourceConfigs == NULL) {
    DEBUG ((DEBUG_ERROR, "(%a) Root Bridge resource configs not found\n", __func__));
    return EFI_UNSUPPORTED;
  }

  //
  // On K3, each PCIe controller is treated as a Root Bridge, so the
  // number of configs should be equal.
  //
  if (DwPcieControllerConfigs->Num != RootBridgeResourceConfigs->ArrayNum) {
    DEBUG (
           (DEBUG_ERROR,
            "(%a) DwPcieControllerConfigs->Num should be equal to "
            "RootBridgeResourceConfigs->ArrayNum\n",
            __func__)
           );
    return EFI_UNSUPPORTED;
  }

  //
  // Perhaps some of the Root Bridge resource configs are disabled, it means
  // that the corresponding DW PCIe controllers are disabled too.
  // For convenience, we allocate a space for mDwPcies that it can store all of
  // the DW_PCIE* pointers, no matter the controller is enabled or not. The
  // pointers in mDwPcies for the disabled controllers will be kept NULL.
  //
  mDwPciesCount = DwPcieControllerConfigs->Num;
  mDwPcies      = (DW_PCIE **)AllocateZeroPool (sizeof (DW_PCIE *) * mDwPciesCount);
  ASSERT (mDwPcies != NULL);

  mK3PcieReg = AllocateZeroPool (sizeof (*mK3PcieReg) * mDwPciesCount);
  ASSERT (mK3PcieReg != NULL);

  //
  // Load K3 wrapper/PHY wiring from structured PCDs for all controllers.
  //
  for (Index = 0; Index < mDwPciesCount; Index++) {
    Status = K3PcieLoadRegsFromPcd ((UINT32)Index);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "K3Pcie: missing port wiring PCD for Index %u: %r\n", Index, Status));
    }
  }

  //
  // Determine per-board PCIe port enable from SKU-based PCD.
  // Store in local array instead of modifying Fixed PCD.
  //
  BOOLEAN  PortEnabled[K3_PCIE_MAX_PORTS];
  {
    UINT8  EnableMask = PcdGet8 (PcdPcieHostEnableMask);

    for (Index = 0; Index < mDwPciesCount && Index < K3_PCIE_MAX_PORTS; Index++) {
      PortEnabled[Index] = ((EnableMask >> Index) & 1) != 0;
    }
  }

  K3PcieInitPortABSplitPolicy (RootBridgeResourceConfigs, PortEnabled);

  //
  // If Port A is not split, Port B must be skipped (shared lanes).
  //
  if (!mPortABSplitEnabled) {
    for (Index = 0; Index < mDwPciesCount; Index++) {
      if ((mK3PcieReg != NULL) && (mK3PcieReg[Index].PortId == 1) &&
          PortEnabled[Index])
      {
        DEBUG ((DEBUG_INFO, "K3Pcie: skip PortB (Index %u) because PortA not split\n", Index));
        PortEnabled[Index] = FALSE;
      }
    }
  }

  for (Index = 0; Index < mDwPciesCount; Index++) {
    DEBUG ((DEBUG_INFO, "K3Pcie: Processing Index %u, IsEnabled=%d, ControllerMode=%d\n",
            Index,
            PortEnabled[Index],
            DwPcieControllerConfigs->Data[Index].ControllerMode));
    if (!PortEnabled[Index] ||
        (DwPcieControllerConfigs->Data[Index].ControllerMode !=
         DW_PCIE_CONTROLLER_MODE_RC))
    {
      DEBUG ((DEBUG_INFO, "K3Pcie: skip Index %u (IsEnabled=%d or not RC mode)\n",
              Index, PortEnabled[Index]));
      continue;
    }

    Status = K3PcieLoadRegsFromPcd ((UINT32)Index);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "K3Pcie: skip Index %u (no wiring PCD): %r\n", Index, Status));
      continue;
    }

    mDwPcies[Index] = DwPcieAllocateInstance (Index);
    if (mDwPcies[Index] == NULL) {
      DEBUG (
             (DEBUG_ERROR, "(%a) DwPcies[%u]: Failed to allocate instance\n",
              __func__, Index)
             );
      continue;
    }

    Status = DwPcieGetConfigs (
                                  mDwPcies[Index],
                                  &DwPcieControllerConfigs->Data[Index],
                                  &RootBridgeResourceConfigs->ArrayData[Index],
                                  NULL
                                  );
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_ERROR,
              "(%a) DwPcies[%u]: Failed to init DW PCIe controller resources\n",
              __func__, Index)
             );
      DwPcieFreeInstance (mDwPcies[Index]);
      mDwPcies[Index] = NULL;
      continue;
    }

    //
    // If lane split is enabled, Port A must train as x2.
    //
    if (mPortABSplitEnabled && (mK3PcieReg[Index].PortId == 0)) {
      mDwPcies[Index]->NumLanes = 2;
    }

    Status = K3PciHostBridgeInit (Index);
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_ERROR, "(%a) DwPcies[%u]: Failed to init DW PCIe controller\n",
              __func__, Index)
             );
      DwPcieFreeInstance (mDwPcies[Index]);
      mDwPcies[Index] = NULL;
      continue;
    }

    Status = DwPcieHostInit (mDwPcies[Index]);
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_ERROR,
              "(%a) DwPcies[%u]: Failed to init DW PCIe controller host mode\n",
              __func__, Index)
             );
      DwPcieFreeInstance (mDwPcies[Index]);
      mDwPcies[Index] = NULL;
      continue;
    }

    if (RootBridgeResourceConfigs->ArrayData[Index].Segment > SegMax) {
      SegMax = RootBridgeResourceConfigs->ArrayData[Index].Segment;
    }

    //
    // Mark this port as enabled in the resource config for DwPcie framework.
    //
    RootBridgeResourceConfigs->ArrayData[Index].IsEnabled = TRUE;
  }

  //
  // Mark disabled ports in the resource config.
  //
  for (Index = 0; Index < mDwPciesCount; Index++) {
    if (mDwPcies[Index] == NULL) {
      RootBridgeResourceConfigs->ArrayData[Index].IsEnabled = FALSE;
    }
  }

  Status = DwPcieRootPortArrayInit (SegMax);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcieRootPortArrayInit failed\n", __func__));
  }

  for (Index = 0; Index < mDwPciesCount; Index++) {
    if (mDwPcies[Index] == NULL) {
      continue;
    }

    Status = DwPcieRootPortArrayRegisterElement (&mDwPcies[Index]->Rp);
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_ERROR,
              "(%a) DwPcies[%u]: DwPcieRootPortArrayRegisterElement failed\n",
              __func__, Index)
             );
      DwPcieRootPortArrayDeinit ();
      break;
    }
  }

  return EFI_SUCCESS;
}

/**
  Destructor to free resources.

  @param  ImageHandle   The image handle.
  @param  SystemTable   The system table.

  @retval EFI_SUCCESS   Always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
K3PcieHostBridgeLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN  Index;

  DwPcieRootPortArrayDeinit ();

  if (mK3PcieReg != NULL) {
    FreePool (mK3PcieReg);
    mK3PcieReg = NULL;
  }

  if (mDwPcies == NULL) {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < mDwPciesCount; Index++) {
    if (mDwPcies[Index] == NULL) {
      continue;
    }

    DwPcieFreeInstance (mDwPcies[Index]);
    mDwPcies[Index] = NULL;
  }

  mDwPciesCount = 0;

  FreePool (mDwPcies);
  mDwPcies = NULL;

  return EFI_SUCCESS;
}
