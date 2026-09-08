/** @file

  Copyright 2017, 2020 NXP
  Copyright 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BoardPinctrlMapLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryManagementLib.h>
#include <Library/NonDiscoverableDeviceRegistrationLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "ClockResetLib.h"
#include "PinGpioLib.h"
#include "UsbHcd.h"
#include "UsbHostPcdConfig.h"
#include "UsbPhy.h"

STATIC
VOID
Dwc3SetQuirk (
  IN DWC3  *Dwc3Reg
  )
{
  UINT32  ClearBits, SetBits;

  ClearBits = 0;
  /* snps,dis_u3_susphy_quirk */
  ClearBits |= DWC3_GUSB3PIPECTL_SUSPHY;
  /* snps,dis-del-phy-power-chg-quirk */
  ClearBits |= DWC3_GUSB3PIPECTL_DEPOCHANGE;
  MmioAnd32 ((UINTN)&Dwc3Reg->GUsb3PipeCtl[0], ~ClearBits);

  ClearBits = 0;
  /* snps,dis_enblslpm_quirk */
  ClearBits |= DWC3_GUSB2PHYCFG_ENBLSLPM;
  /* snps,dis_u2_susphy_quirk */
  ClearBits |= DWC3_GUSB2PHYCFG_SUSPHY;
  MmioAnd32 ((UINTN)&Dwc3Reg->GUsb2PhyCfg[0], ~ClearBits);

  SetBits = 0;
  /* snps,dis-tx-ipgap-linecheck-quirk */
  SetBits |= DWC3_GUCTL1_TX_IPGAP_LINECHECK_DIS;
  /* snps,parkmode-disable-ss-quirk */
  SetBits |= DWC3_GUCTL1_PARKMODE_DISABLE_SS;
  MmioOr32 ((UINTN)&Dwc3Reg->GUctl1, SetBits);
}

STATIC
VOID
Dwc3SetMaxHighSpeed (
  IN UINT64  UsbReg
  )
{
  UINT32  SetBits;
  DWC3    *Dwc3Reg;

  Dwc3Reg = (VOID *)(UsbReg + DWC3_REG_OFFSET);

  SetBits  = 0;
  SetBits |= DWC3_GUCTL1_DEV_FORCE_20_CLK_FOR_30_CLK;
  MmioOr32 ((UINTN)&Dwc3Reg->GUctl1, SetBits);
}

STATIC
EFI_STATUS
Dwc3CoreInit (
  IN DWC3  *Dwc3Reg
  )
{
  UINT32  Revision;
  UINT32  Reg;
  UINTN   Dwc3Hwparams1;

  Revision = MmioRead32 ((UINTN)&Dwc3Reg->GSnpsId);
  //
  // This should read as 0x5533, ascii of U3(DWC_usb3) followed by revision num
  //
  if ((Revision & DWC3_GSNPSID_MASK) != DWC3_SYNOPSYS_ID) {
    DEBUG (
           (DEBUG_ERROR,
            "This is not a DesignWare USB3 DRD Core. GSnpsId: %x on 0x%lX\n",
            Revision, (UINTN)&Dwc3Reg->GSnpsId)
           );
    return EFI_NOT_FOUND;
  }

  Reg  = MmioRead32 ((UINTN)&Dwc3Reg->GCtl);
  Reg &= ~DWC3_GCTL_SCALEDOWN_MASK;
  Reg &= ~DWC3_GCTL_DISSCRAMBLE;

  Dwc3Hwparams1 = MmioRead32 ((UINTN)&Dwc3Reg->GHwParams1);

  if (DWC3_GHWPARAMS1_EN_PWROPT (Dwc3Hwparams1) ==
      DWC3_GHWPARAMS1_EN_PWROPT_CLK)
  {
    Reg &= ~DWC3_GCTL_DSBLCLKGTNG;
  } else {
    DEBUG ((DEBUG_WARN, "No power optimization available.\n"));
  }

  if ((Revision & DWC3_RELEASE_MASK) < DWC3_RELEASE_190a) {
    Reg |= DWC3_GCTL_U2RSTECN;
  }

  MmioWrite32 ((UINTN)&Dwc3Reg->GCtl, Reg);

  return EFI_SUCCESS;
}

STATIC
VOID
Dwc3SetMode (
  IN DWC3    *Dwc3Reg,
  IN UINT32  Mode
  )
{
  MmioAndThenOr32 (
                   (UINTN)&Dwc3Reg->GCtl,
                   ~(DWC3_GCTL_PRTCAPDIR (DWC3_GCTL_PRTCAP_OTG)),
                   DWC3_GCTL_PRTCAPDIR (Mode)
                   );
}

STATIC
EFI_STATUS
XhciCoreInit (
  IN UINTN  UsbReg
  )
{
  EFI_STATUS  Status;
  DWC3        *Dwc3Reg;

  Dwc3Reg = (VOID *)(UsbReg + DWC3_REG_OFFSET);

  /* UTMI 8 mode */
  MmioAndThenOr32 (
                   (UINTN)&Dwc3Reg->GUsb2PhyCfg[0],
                   ~(DWC3_GUSB2PHYCFG_PHYIF_MASK | DWC3_GUSB2PHYCFG_USBTRDTIM_MASK),
                   DWC3_GUSB2PHYCFG_PHYIF (UTMI_PHYIF_8_BIT) |
                   DWC3_GUSB2PHYCFG_USBTRDTIM (USBTRDTIM_UTMI_8_BIT) |
                   DWC3_GUSB2PHYCFG_TOUTCAL_MASK
                   );

  Dwc3SetQuirk (Dwc3Reg);

  Status = Dwc3CoreInit (Dwc3Reg);
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR, "Dwc3CoreInit Failed for controller 0x%lX (%r) \n",
            UsbReg, Status)
           );

    return Status;
  }

  Dwc3SetMode (Dwc3Reg, DWC3_GCTL_PRTCAP_HOST);

  return Status;
}

STATIC
EFI_STATUS
Usb2PhyEnable (
  VOID  *PhyBase
  )
{
  UINT32  Loop = 50 * 1000;
  UINT32  Temp;

  if (PhyBase == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: PhyBase is NULL\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  do {
    Temp = MmioRead32 ((UINTN)PhyBase + USB2_PHY_REG01);
    if (Temp & USB2_PHY_REG01_PLL_IS_READY) {
      break;
    }

    gBS->Stall (50);
  } while (--Loop);

  if (Loop == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Wait PHY_REG01[PLLREADY] timeout\n", __func__));
    return EFI_DEVICE_ERROR;
  }

  /* release usb2 phy internal reset and enable clock gating */
  MmioWrite32 ((UINTN)PhyBase + USB2_PHY_REG01, 0x60ef);
  MmioWrite32 ((UINTN)PhyBase + USB2_PHY_REG0D, 0x1c);

  MmioAndThenOr32 (
                   (UINTN)PhyBase + USB2_ANALOG_REG14_13,
                   ~(USB2_ANALOG_HSDAC_ISEL_MASK),
                   (USB2_ANALOG_HSDAC_ISEL_15_INC | USB2_ANALOG_HSDAC_IREG_EN)
                   );

  /* auto clear host disc*/
  MmioOr32 ((UINTN)PhyBase + USB2_PHY_REG04, USB2_PHY_REG04_AUTO_CLEAR_DIS);

  return EFI_SUCCESS;
}

STATIC
VOID
Usb3PhyComboSetUsb (
  IN BOOLEAN  Usb,
  IN UINT8    PhySelBit
  )
{
  UINT64  ApmuBase;
  UINT32  ComboModeMask;
  UINT32  ComboModeVal;

  ApmuBase = PcdGet64 (PcdSpacemitAPMURegBase);
  if (ApmuBase == 0) {
    DEBUG ((DEBUG_ERROR, "%a: APMU base is zero\n", __func__));
    return;
  }

  MapRegToGcdMmioSpace (ApmuBase, SIZE_4KB);

  ComboModeMask = (1 << PhySelBit);
  ComboModeVal  = Usb ? (1 << PhySelBit) : 0;

  ComboModeMask |= PU_MATRIX_CONF_X8_DISABLE;
  ComboModeVal  |= Usb ? PU_MATRIX_CONF_X8_DISABLE : 0;

  MmioAndThenOr32 (
                   (UINTN)(ApmuBase + PMUA_PCIE_SUBSYS_MGMT),
                   ~ComboModeMask,
                   ComboModeVal
                   );

  DEBUG (
         (DEBUG_INFO, "Update Combo Mode %d to %s Mode\n", PhySelBit,
          Usb ? "USB" : "PCIE")
         );
}

STATIC
VOID
Usb3PhyForceDisabled (
  IN CONST USB_HOST_CONTROLLER_HW  *Hw
  )
{
  MapRegToGcdMmioSpace (Hw->PipePhyBase, SIZE_4KB);
  MmioOr32 ((UINTN)(Hw->PipePhyBase + PHY_PU_SEL), OVRD_STATUS);
  if (Hw->PipePhy1Base != 0) {
    MapRegToGcdMmioSpace (Hw->PipePhy1Base, SIZE_4KB);
    MmioOr32 ((UINTN)(Hw->PipePhy1Base + PHY_PU_SEL), OVRD_STATUS);
  }

  gBS->Stall (200);
}

STATIC
EFI_STATUS
Usb3PhyInitSingle (
  IN VOID  *PhyBase
  )
{
  UINT64  ApbSpareBase;
  UINT32  Version;
  UINT32  Reg;
  UINT32  Loops;

  if (PhyBase == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: PhyBase is NULL\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  ApbSpareBase = PcdGet64 (PcdSpacemitAPBSpareRegBase);
  if (ApbSpareBase == 0) {
    DEBUG ((DEBUG_ERROR, "%a: APB_SPARE base is zero\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  MapRegToGcdMmioSpace (ApbSpareBase, SIZE_4KB);

  Version = MmioRead32 ((UINTN)PhyBase + PHY_VERSION);
  DEBUG ((DEBUG_INFO, "PHY 0x%x version: 0x%x\n", PhyBase, Version));

  /* Enable PU_CAL */
  MmioOr32 ((UINTN)(ApbSpareBase + APB_SPARE_PU_CAL), PU_CAL);

  /* Wait for PU_CAL_DONE */
  Loops = PU_CAL_TIMEOUT / POLL_DELAY;
  do {
    Reg = MmioRead32 ((UINTN)(ApbSpareBase + APB_SPARE_RCAL_HSIO));
    if (Reg & PU_CAL_DONE) {
      break;
    }

    gBS->Stall (POLL_DELAY);
  } while (--Loops);

  if (Loops == 0) {
    DEBUG (
           (DEBUG_WARN, "%a: PU PHY 0x%x RCAL timeout, trim override\n",
            __func__, PhyBase)
           );

    /* Set trim values */
    Reg  = MmioRead32 ((UINTN)(ApbSpareBase + APB_SPARE_RCAL_HSIO));
    Reg &= ~R_CAL_OVRD_TRIM_EN;
    Reg &= ~R_CAL_OVRD_NTRIM_MASK;
    Reg &= ~R_CAL_OVRD_PTRIM_MASK;
    Reg |= R_CAL_OVRD_TRIM_EN;
    Reg |= R_CAL_OVRD_STABLE_VAL;
    Reg |= R_CAL_OVRD_NTRIM (NTRIM_DEFAULT);
    Reg |= R_CAL_OVRD_PTRIM (PTRIM_DEFAULT);
    MmioWrite32 ((UINTN)(ApbSpareBase + APB_SPARE_RCAL_HSIO), Reg);

    /* Enable stable override */
    MmioOr32 ((UINTN)(ApbSpareBase + APB_SPARE_RCAL_HSIO), R_CAL_OVRD_STABLE_EN);
  }

  /* Do not wait CDR lock before sampling data */
  MmioAnd32 ((UINTN)PhyBase + PHY_RESET_CFG, ~EN_SAMPLE_DATA_AFTER_LOCK);

  /* Power down 100MHz refclk buffer */
  MmioAnd32 ((UINTN)PhyBase + PHY_PU_CK_REG, ~PU_REFCLK_100);

  /* Program PLL REG1 configure the SSC */
  Reg = SSC_MODE (SSC_DOWN_SPREAD1) | SSC_DEP_SEL (SSC_5000PPM) |
        FREF_SEL (FREF_24M);
  MmioWrite32 ((UINTN)PhyBase + PHY_PLL_REG1, Reg);

  /* Un-select 100MHz PLL reference */
  MmioAnd32 ((UINTN)PhyBase + PHY_PLL_REG2, ~SEL_REF100);

  /* USB LFPS period configuration */
  Reg  = MmioRead32 ((UINTN)PhyBase + PHY_MODE_CFG);
  Reg &= ~CFG_LFPS_TPERIOD_MASK;
  Reg |= CFG_LFPS_TPERIOD (LFPS_TPERIOD_USB);
  MmioWrite32 ((UINTN)PhyBase + PHY_MODE_CFG, Reg);

  /* Force AFE adaptation reset */
  Reg = (AFE_ADPT_RST_OVRD_EN | AFE_ADPT_RST_OVRD_VAL);
  MmioWrite32 ((UINTN)PhyBase + PHY_ADPT_CFG0, Reg);

  /* Override driver amplitude value to 900m */
  Reg = (RXEQ_TIME_OVRD_AMP_SOC | RXEQ_TIME_CFG_AMP_SOC (AMP_SOC_900M));
  MmioOr32 ((UINTN)PhyBase + PHY_RXEQ_TIME, Reg);

  /* Configure RX parameters */
  MmioOr32 ((UINTN)PhyBase + PHY_RX_REG_A, RX_REG0_RLOAD);

  Reg = RX_REG1_RC_CALI_REG (RX_REG1_RC_CALI_REG_DEFAULT) |
        RX_REG1_RTERM_REG (RX_REG1_RTERM_REG_DEFAULT);
  MmioAndThenOr32 ((UINTN)PhyBase + PHY_RX_REG_A, ~RX_REG1_MASK, Reg);

  Reg = RX_REG2_PSEL (RX_REG2_PSEL_DEFAULT) | RX_REG2_FORCE_CSEL |
        RX_REG2_CSEL (RX_REG2_CSEL_DEFAULT);
  MmioAndThenOr32 ((UINTN)PhyBase + PHY_RX_REG_A, ~RX_REG2_MASK, Reg);

  Reg = RX_REG3_RDEG1 (RX_REG3_RDEG1_DEFAULT) |
        RX_REG3_ADJ_BIAS (RX_REG3_ADJ_BIAS_DEFAULT) | RX_REG3_SEL_CBOOST_CODE |
        RX_REG3_I_LOAD_REG (RX_REG3_I_LOAD_REG_DEFAULT);
  MmioAndThenOr32 ((UINTN)PhyBase + PHY_RX_REG_A, ~RX_REG3_MASK, Reg);

  Reg = RX_REG4_MANUAL_CFG | RX_REG4_RTERM_SEL | RX_REG4_ENVOS |
        RX_REG4_RDEG2 (RX_REG4_RDEG2_DEFAULT);
  MmioAndThenOr32 ((UINTN)PhyBase + PHY_RX_REG_B, ~RX_REG4_MASK, Reg);

  Reg = RX_REG5_RCELL_BIAS (RX_REG5_RCELL_BIAS_DEFAULT) |
        RX_REG5_RCELL_VCM (RX_REG5_RCELL_VCM_DEFAULT);
  MmioAndThenOr32 ((UINTN)PhyBase + PHY_RX_REG_B, ~RX_REG5_MASK, Reg);

  Reg = RX_REG6_ADAPT_GAIN (RX_REG6_ADAPT_GAIN_DEFAULT) |
        RX_REG6_H1_REG (RX_REG6_H1_REG_DEFAULT);
  MmioAndThenOr32 ((UINTN)PhyBase + PHY_RX_REG_B, ~RX_REG6_MASK, Reg);

  DEBUG ((DEBUG_INFO, "PUPHY 0x%x Rx Reg Configured\n", PhyBase));

  /* Inform PHY that all PLL-related configuration is done */
  Reg = CFG_SW_INIT_DONE | CFG_PU_SSC_OUT | CFG_REFCLK_FREQ (REFCLK_24M) |
        CFG_RXCLK_EN | CFG_PCLK_EN | CFG_PIPE_PCLK_EN | CFG_TXCLK_EN;
  MmioWrite32 ((UINTN)PhyBase + PHY_CLK_CFG, Reg);

  /* Wait for PLL to be ready */
  Loops = PLL_TIMEOUT / POLL_DELAY;
  do {
    Reg = MmioRead32 ((UINTN)PhyBase + PHY_CLK_CFG);
    if (Reg & PLL_READY) {
      break;
    }

    gBS->Stall (POLL_DELAY);
  } while (--Loops);

  if (Loops == 0) {
    DEBUG (
           (DEBUG_ERROR, "%a: PHY 0x%x PLL polling Timeout!\n", __func__,
            PhyBase)
           );
    return EFI_TIMEOUT;
  }

  DEBUG (
         (DEBUG_INFO, "PHY 0x%x version: 0x%x init as USB3 mode\n", PhyBase,
          Version)
         );
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
Usb3PhyInit (
  IN CONST USB_HOST_CONTROLLER_HW  *Hw
  )
{
  EFI_STATUS  Status;

  if (Hw == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  MapRegToGcdMmioSpace (Hw->PipePhyBase, SIZE_4KB);
  Status = Usb3PhyInitSingle ((VOID *)Hw->PipePhyBase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Hw->PipePhy1Base == 0) {
    return EFI_SUCCESS;
  }

  MapRegToGcdMmioSpace (Hw->PipePhy1Base, SIZE_4KB);
  return Usb3PhyInitSingle ((VOID *)Hw->PipePhy1Base);
}

STATIC
BOOLEAN
UsbHasBoardPinctrlMap (
  IN UINT8  ControllerId
  )
{
  EFI_STATUS                 Status;
  CONST PINCTRL_BOARD_MAP    *BoardMap;
  UINTN                      DeviceIndex;
  CONST PINCTRL_DEVICE_DESC  *Device;

  Status = BoardPinctrlGetMap (&BoardMap);
  if (EFI_ERROR (Status) || (BoardMap == NULL) || (BoardMap->Devices == NULL)) {
    return FALSE;
  }

  for (DeviceIndex = 0; DeviceIndex < BoardMap->DeviceCount; DeviceIndex++) {
    Device = &BoardMap->Devices[DeviceIndex];

    if ((Device->ControllerType == NULL) ||
        (Device->ControllerType[0] == '\0'))
    {
      continue;
    }

    if (AsciiStrCmp (Device->ControllerType, "usb") != 0) {
      continue;
    }

    if (Device->ControllerId == (UINT32)ControllerId) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
VOID
UsbApplyPinctrlState (
  IN UINT8        ControllerId,
  IN CONST CHAR8  *NodeLabel
  )
{
  EFI_STATUS  Status;

  if (!UsbHasBoardPinctrlMap (ControllerId)) {
    DEBUG (
           (DEBUG_INFO, "USB: no board pinctrl map for controller[%u] (%a)\n",
            ControllerId, (NodeLabel != NULL) ? NodeLabel : "node")
           );
    return;
  }

  Status = PinctrlApplyStateById (
                                  "usb",
                                  (UINT32)ControllerId,
                                  NULL,
                                  PINCTRL_STATE_DEFAULT
                                  );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      DEBUG (
             (DEBUG_WARN,
              "USB: pinctrl default state map is not available for "
              "controller[%u] (%a)\n",
              ControllerId, (NodeLabel != NULL) ? NodeLabel : "node")
             );
      return;
    }

    if (Status == EFI_UNSUPPORTED) {
      DEBUG (
             (DEBUG_WARN,
              "USB: pinctrl state API is unavailable for controller[%u] (%a)\n",
              ControllerId, (NodeLabel != NULL) ? NodeLabel : "node")
             );
      return;
    }

    DEBUG (
           (DEBUG_WARN,
            "USB: failed to apply pinctrl default state for controller[%u] "
            "(%a): %r\n",
            ControllerId, (NodeLabel != NULL) ? NodeLabel : "node", Status)
           );
  }
}

VOID
UsbGpioEnable (
  USB_HOST_VBUS_GPIO_CONFIG_ARRAY  *Cfg
  )
{
  UINT8  I;

  if ((Cfg == NULL) || (Cfg->Num == 0)) {
    DEBUG ((DEBUG_INFO, "UsbGpioEnable: No USB Gpio need to config\n"));
    return;
  }

  DEBUG (
         (DEBUG_INFO, "UsbGpioEnable: have %d GPIOs need to config\n", Cfg->Num)
         );
  for (I = 0; I < Cfg->Num; I++) {
    UINT32   GpioPin   = Cfg->VbusOrHub[I].Gpio;
    BOOLEAN  ActiveLow = Cfg->VbusOrHub[I].ActiveLow;

    DEBUG (
           (DEBUG_INFO, "UsbGpioEnable: enable GPIO %d Vbus/HUB GPIO\n", GpioPin)
           );

    GpioConfigureOutput (GpioPin, !ActiveLow);

    // Delay for HUB/VBUS stable power sequences
    gBS->Stall (10 * 1000);
  }
}

EFI_STATUS
EFIAPI
InitializeXhciController (
  IN NON_DISCOVERABLE_DEVICE  *This
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  UsbReg = This->Resources->AddrRangeMin;

  DEBUG ((DEBUG_ERROR, "XHCI: Initialize DWC3 at 0x%lX\n", UsbReg));

  Status = XhciCoreInit (UsbReg);

  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR, "XHCI: Controller init Failed for 0x%lX (%r)\n", UsbReg,
            Status)
           );
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
RegisterXhciController (
  UINT64  XhciControllerAddr
  )
{
  EFI_STATUS  Status;

  Status = RegisterNonDiscoverableMmioDevice (
                                              NonDiscoverableDeviceTypeXhci,
                                              NonDiscoverableDeviceDmaTypeNonCoherent,
                                              InitializeXhciController,
                                              NULL,
                                              1,
                                              XhciControllerAddr,
                                              DWC3_XHCI_REGS_SIZE
                                              );
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR, "Failed to register XHCI device 0x%x, error %r\n",
            XhciControllerAddr, Status)
           );
  }
}

STATIC
EFI_STATUS
ProcessUsbControllerEntry (
  IN UINT8                              ControllerId,
  IN CONST USB_HOST_CONTROLLER_HW       *Hw,
  IN UINT8                              EffectiveMaxSpeed
  )
{
  EFI_STATUS  Status;

  if ((Hw == NULL) || (Hw->ControllerBase == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  UsbApplyPinctrlState (ControllerId, "USB controller");

  if ((EffectiveMaxSpeed >= USB_SPEED_SUPER) && Hw->IsCombo) {
    Usb3PhyComboSetUsb (TRUE, Hw->PhySelBit);
    DEBUG (
           (DEBUG_INFO, "select USB3 function in combo phy BIT(%d)\n",
            Hw->PhySelBit)
           );
  }

  Status = ClockEnableAndResetDeassert ((CHAR8 *)Hw->ClockResetName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (EffectiveMaxSpeed >= USB_SPEED_SUPER) {
    Status = Usb3PhyInit (Hw);
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_ERROR, "%a: Usb3PhyInit of Controller 0x%lx failed (%r)\n",
              __func__, Hw->ControllerBase, Status)
             );
      return Status;
    }
  } else if (Hw->HsForceDisableU3Phy) {
    Usb3PhyForceDisabled (Hw);
    DEBUG (
           (DEBUG_INFO, "%a: Limit MaxSpeed=HighSpeed of Controller 0x%lx\n",
            __func__, Hw->ControllerBase)
           );
  }

  MapRegToGcdMmioSpace (Hw->UtmiPhyBase, SIZE_4KB);
  Status = Usb2PhyEnable ((VOID *)Hw->UtmiPhyBase);
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR, "%a: Usb2PhyEnable failed for 0x%lx (%r)\n", __func__,
            Hw->UtmiPhyBase, Status)
           );
    return Status;
  }

  MapRegToGcdMmioSpace (Hw->ControllerBase, SIZE_64KB);

  if (EffectiveMaxSpeed <= USB_SPEED_HIGH) {
    Dwc3SetMaxHighSpeed (Hw->ControllerBase);
  }

  RegisterXhciController (Hw->ControllerBase);

  return EFI_SUCCESS;
}

/**
  This function gets registered as a callback to perform USB controller
intialization

  @param  Event         Event whose notification function is being invoked.
  @param  Context       Pointer to the notification function's context.

**/
VOID EFIAPI
UsbHostControllersInit (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  USB_HOST_CONTROLLER_HW_ARRAY     *HwArray;
  USB_HOST_VBUS_GPIO_CONFIG_ARRAY  *VbusCfg;
  UINT8       EnableMask;
  UINT8       HsOnlyMask;
  UINT64      PortaSwitchBase;
  BOOLEAN     PortaSwitchFlip;
  UINTN       I;
  UINT16      NumControllers;
  EFI_STATUS  Status;

  if (Event != NULL) {
    gBS->CloseEvent (Event);
  }

  HwArray = (USB_HOST_CONTROLLER_HW_ARRAY *)PcdGetPtr (PcdUsbHostControllers);
  if ((HwArray == NULL) || (HwArray->Num == 0)) {
    DEBUG ((DEBUG_ERROR, "PcdUsbHostControllers is empty\n"));
    return;
  }

  EnableMask = PcdGet8 (PcdUsbHostEnableMask);
  HsOnlyMask = PcdGet8 (PcdUsbHostHsOnlyMask);

  NumControllers = HwArray->Num;
  DEBUG ((DEBUG_INFO, "USB: %u controllers, EnableMask=0x%02x, HsOnlyMask=0x%02x\n",
          (UINT32)NumControllers, EnableMask, HsOnlyMask));

  VbusCfg = (USB_HOST_VBUS_GPIO_CONFIG_ARRAY *)PcdGetPtr (PcdUsbHostVbusConfigs);

  PortaSwitchBase = PcdGet64 (PcdUsbHostPortaSwitchBase);
  PortaSwitchFlip = PcdGetBool (PcdUsbHostPortaSwitchFlip);

  if (PortaSwitchFlip) {
    MapRegToGcdMmioSpace (PortaSwitchBase & ~(0xFFF), SIZE_4KB);
    MmioOr32 ((UINTN)(PortaSwitchBase), USB3_CC_CONTROL_SWITCH_FLIP);
    DEBUG ((DEBUG_INFO, "PortaSwitchFlip set\n"));
  }

  for (I = 0; I < NumControllers; I++) {
    CONST USB_HOST_CONTROLLER_HW  *Hw = &HwArray->Controller[I];
    UINT8                         EffectiveMaxSpeed;

    if ((EnableMask != 0xFF) && !(EnableMask & (1 << I))) {
      DEBUG ((DEBUG_INFO, "USB[%u] skipped (not in EnableMask)\n", (UINT32)I));
      continue;
    }

    EffectiveMaxSpeed = Hw->MaxSpeed;
    if (HsOnlyMask & (1 << I)) {
      EffectiveMaxSpeed = USB_SPEED_HIGH;
    }

    DEBUG ((DEBUG_INFO, "USB[%u] base=0x%lx maxSpeed=%u\n",
            (UINT32)I, Hw->ControllerBase, EffectiveMaxSpeed));

    Status = ProcessUsbControllerEntry ((UINT8)I, Hw, EffectiveMaxSpeed);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "USB[%u] init failed (%r) - continuing\n",
              (UINT32)I, Status));
    }
  }

  UsbGpioEnable (VbusCfg);
}

/**
  The Entry Point of module. It follows the standard UEFI driver model.

  @param[in] ImageHandle   The firmware allocated handle for the EFI image.
  @param[in] SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS      The entry point is executed successfully.
  @retval other            Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeUsbHcd (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UsbHostControllersInit (NULL, NULL);
  return EFI_SUCCESS;
}
