/** @file
  Spacemit Glue Layer For EQoS Driver

  Copyright (c) 2025, Spacemit Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/DebugLib.h>
#include <Protocol/ClockCtrl.h>
#include <Protocol/EmbeddedGpio.h>
#include <Protocol/PinCtrl.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BoardPinctrlMapLib.h>

#include "DwEqosDxeUtil.h"

#define CLK_PHASE_CNT     256
#define CLK_PHASE_REVERT  180

#define TX_PHASE  1
#define RX_PHASE  0

typedef enum {
  /* fpga clk tuning register */
  CLK_TUNING_BY_REG,
  /* zebu/evb rgmii delayline register */
  CLK_TUNING_BY_DLINE,
  /* evb rmii only revert tx/rx clock for clk tuning */
  CLK_TUNING_BY_CLK_REVERT,
  CLK_TUNING_MAX,
} CLK_TUNING_WAY;

typedef struct {
  EFI_PHYSICAL_ADDRESS          CtrlReg;
  EFI_PHYSICAL_ADDRESS          DlineReg;
  PHY_INTERFACE                 PhyInterface;
  EMBEDDED_GPIO                 *Gpio;
  UINT32                        PhyResetGpioPin;
  UINT8                         TxClkPhase;
  UINT8                         RxClkPhase;
  CLK_TUNING_WAY                ClkTuningWay;
  BOOLEAN                       ClkTuningEnable;
  SILICON_CLOCKCTRL_PROTOCOL    *Clock;
  SILICON_PINCTRL_PROTOCOL      *PinCtrl;
  BOOLEAN                       TxClkFromSoc;
  BOOLEAN                       PhyClkFromSoc;
  CONST CHAR8                   *BusClkAndRstName;
  CONST CHAR8                   *TxClkName;
  CONST CHAR8                   *PhyClkName;
} SPACEMIT_PLAT_DATA;

STATIC
VOID
EqosSetPlatPriv (
  IN EQOS_DEVICE         *Eqos,
  IN SPACEMIT_PLAT_DATA  *Priv
  )
{
  Eqos->Plat = (VOID *)Priv;
}

STATIC
SPACEMIT_PLAT_DATA *
EqosGetPlatPriv (
  IN EQOS_DEVICE  *Eqos
  )
{
  return (SPACEMIT_PLAT_DATA *)Eqos->Plat;
}

STATIC
VOID
EqosClockEnable (
  IN SPACEMIT_PLAT_DATA  *Priv,
  IN CONST CHAR8         *Name
  )
{
  EFI_STATUS                  Status;
  SILICON_CLOCKCTRL_PROTOCOL  *Clock;

  Clock  = Priv->Clock;
  Status = Clock->SetClockState (
                    Clock,
                    Name,
                    ENABLE_CLOCK
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Enable %a clock failed: %r\n",
      __func__,
      Name,
      Status
      ));
  }
}

STATIC
VOID
EqosClockDisable (
  IN SPACEMIT_PLAT_DATA  *Priv,
  IN CONST CHAR8         *Name
  )
{
  EFI_STATUS                  Status;
  SILICON_CLOCKCTRL_PROTOCOL  *Clock;

  Clock  = Priv->Clock;
  Status = Clock->SetClockState (
                    Clock,
                    Name,
                    DISABLE_CLOCK
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Disable %a clock failed: %r\n",
      __func__,
      Name,
      Status
      ));
  }
}

/**
  K3 SoC-specific macros/ops.
**/
#define PHY_INTF_MODE_OFFSET  3
#define PHY_INTF_MODE_MASK    (0x3U << PHY_INTF_MODE_OFFSET)

#define PHY_INTF_RMII   (0x0U << PHY_INTF_MODE_OFFSET)
#define PHY_INTF_RGMII  (0x1U << PHY_INTF_MODE_OFFSET)
#define PHY_INTF_MII    (0x3U << PHY_INTF_MODE_OFFSET)

// Only valid for RMII, invert TX clock.
#define RMII_TX_CLK_SEL  (1U << 6)

// Only valid for RMII, invert RX clock.
#define RMII_RX_CLK_SEL  (1U << 7)

#define PHY_IRQ_EN     (1U << 12)
#define AXI_SINGLE_ID  (1U << 13)

#define RMII_TX_PHASE_OFFSET  16
#define RMII_TX_PHASE_MASK    (0x7U  << RMII_TX_PHASE_OFFSET)
#define RMII_RX_PHASE_OFFSET  20
#define RMII_RX_PHASE_MASK    (0x7U  << RMII_RX_PHASE_OFFSET)

#define RGMII_TX_PHASE_OFFSET  24
#define RGMII_TX_PHASE_MASK    (0x7U  << RGMII_TX_PHASE_OFFSET)
#define RGMII_RX_PHASE_OFFSET  20
#define RGMII_RX_PHASE_MASK    (0x7U  << RGMII_RX_PHASE_OFFSET)

#define EMAC_RX_DLINE_EN           (1U << 0)
#define EMAC_RX_DLINE_CODE_OFFSET  8
#define EMAC_RX_DLINE_CODE_MASK    (0xFFU << EMAC_RX_DLINE_CODE_OFFSET)

#define EMAC_TX_DLINE_EN           (1U << 16)
#define EMAC_TX_DLINE_CODE_OFFSET  24
#define EMAC_TX_DLINE_CODE_MASK    (0xFFU << EMAC_TX_DLINE_CODE_OFFSET)

STATIC
BOOLEAN
PhyIfaceIsRmii (
  IN SPACEMIT_PLAT_DATA  *Priv
  )
{
  return (Priv->PhyInterface == PHY_INTERFACE_MODE_RMII);
}

/**
  Set RMII clock phase according to platform configuration.

  @param[in]  Priv   Pointer to platform data.
  @param[in]  IsTx   TRUE to configure TX phase, FALSE for RX phase.

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_INVALID_PARAMETER Invalid clock phase tuning way.
**/
STATIC
EFI_STATUS
ClkPhaseRmiiSet (
  IN SPACEMIT_PLAT_DATA  *Priv,
  IN BOOLEAN             IsTx
  )
{
  UINT32  Val;

  switch (Priv->ClkTuningWay) {
    case CLK_TUNING_BY_REG:
      Val = MmioRead32 ((UINTN)Priv->CtrlReg);
      if (IsTx) {
        Val &= ~RMII_TX_PHASE_MASK;
        Val |= ((Priv->TxClkPhase & 0x7U) << RMII_TX_PHASE_OFFSET);
      } else {
        Val &= ~RMII_RX_PHASE_MASK;
        Val |= ((Priv->RxClkPhase & 0x7U) << RMII_RX_PHASE_OFFSET);
      }

      MmioWrite32 ((UINTN)Priv->CtrlReg, Val);
      break;

    case CLK_TUNING_BY_CLK_REVERT:
      Val = MmioRead32 ((UINTN)Priv->CtrlReg);
      if (IsTx) {
        if (Priv->TxClkPhase == CLK_PHASE_REVERT) {
          Val |= RMII_TX_CLK_SEL;
        } else {
          Val &= ~RMII_TX_CLK_SEL;
        }
      } else {
        if (Priv->RxClkPhase == CLK_PHASE_REVERT) {
          Val |= RMII_RX_CLK_SEL;
        } else {
          Val &= ~RMII_RX_CLK_SEL;
        }
      }

      MmioWrite32 ((UINTN)Priv->CtrlReg, Val);
      break;

    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: Invalid clk tuning way: %d !!\n",
        __func__,
        Priv->ClkTuningWay
        ));
      return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
  Set RGMII clock phase according to platform configuration.

  @param[in]  Priv   Pointer to platform data.
  @param[in]  IsTx   TRUE to configure TX phase, FALSE for RX phase.

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_INVALID_PARAMETER Invalid clock phase tuning way.
**/
STATIC
EFI_STATUS
ClkPhaseRgmiiSet (
  IN SPACEMIT_PLAT_DATA  *Priv,
  IN BOOLEAN             IsTx
  )
{
  UINT32  Val;

  switch (Priv->ClkTuningWay) {
    case CLK_TUNING_BY_REG:
      Val = MmioRead32 ((UINTN)Priv->CtrlReg);
      if (IsTx) {
        Val &= ~RGMII_TX_PHASE_MASK;
        Val |= ((Priv->TxClkPhase & 0x7U) << RGMII_TX_PHASE_OFFSET);
      } else {
        Val &= ~RGMII_RX_PHASE_MASK;
        Val |= ((Priv->RxClkPhase & 0x7U) << RGMII_RX_PHASE_OFFSET);
      }

      MmioWrite32 ((UINTN)Priv->CtrlReg, Val);
      break;

    case CLK_TUNING_BY_DLINE:
      Val = MmioRead32 ((UINTN)Priv->DlineReg);
      if (IsTx) {
        Val &= ~EMAC_TX_DLINE_CODE_MASK;
        Val |= ((Priv->TxClkPhase & 0xFFU) << EMAC_TX_DLINE_CODE_OFFSET);
      } else {
        Val &= ~EMAC_RX_DLINE_CODE_MASK;
        Val |= ((Priv->RxClkPhase & 0xFFU) << EMAC_RX_DLINE_CODE_OFFSET);
      }

      MmioWrite32 ((UINTN)Priv->DlineReg, Val);
      break;

    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: Invalid clk tuning way: %d !!\n",
        __func__,
        Priv->ClkTuningWay
        ));
      return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ClkPhaseSet (
  IN SPACEMIT_PLAT_DATA  *Priv,
  IN BOOLEAN             IsTx
  )
{
  EFI_STATUS  Status;

  if (PhyIfaceIsRmii (Priv)) {
    Status = ClkPhaseRmiiSet (Priv, IsTx);
  } else {
    Status = ClkPhaseRgmiiSet (Priv, IsTx);
  }

  return Status;
}

STATIC
VOID
K3DelaylineInit (
  IN SPACEMIT_PLAT_DATA  *Priv
  )
{
  UINT32  Val;

  /*
   * On K3, TX/RX delayline must be enabled for reliable DMA init.
   * This is required for all phy-modes (rgmii/rmii/mii).
   */
  Val  = MmioRead32 ((UINTN)Priv->DlineReg);
  Val |= (EMAC_TX_DLINE_EN | EMAC_RX_DLINE_EN);
  MmioWrite32 ((UINTN)Priv->DlineReg, Val);
}

/**
  Configure PHY interface mode (MII / RMII / RGMII).

  @param[in]  Priv   Pointer to platform data.
**/
STATIC
VOID
K3EqosIfaceConfig (
  IN SPACEMIT_PLAT_DATA  *Priv
  )
{
  PHY_INTERFACE  Iface;
  UINT32         Val;

  Iface = Priv->PhyInterface;

  Val  = MmioRead32 ((UINTN)Priv->CtrlReg);
  Val &= ~PHY_INTF_MODE_MASK;

  switch (Iface) {
    case PHY_INTERFACE_MODE_MII:
      Val |= PHY_INTF_MII;
      break;

    case PHY_INTERFACE_MODE_RMII:
      Val |= PHY_INTF_RMII;
      break;

    case PHY_INTERFACE_MODE_RGMII:
    case PHY_INTERFACE_MODE_RGMII_ID:
    case PHY_INTERFACE_MODE_RGMII_RXID:
    case PHY_INTERFACE_MODE_RGMII_TXID:
      Val |= PHY_INTF_RGMII;
      break;

    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unsupported phy-mode (iface=%d)\n",
        __func__,
        Iface
        ));
      return;
  }

  MmioWrite32 ((UINTN)Priv->CtrlReg, Val);
}

/**
  Reset external PHY via GPIO.

  @param[in]  Priv   Pointer to platform data.

  @retval EFI_SUCCESS   PHY reset sequence completed.
  @retval EFI_NOT_READY GPIO protocol not available.
  @retval others        GPIO operation failed.
**/
STATIC
EFI_STATUS
K3EqosPhyReset (
  IN SPACEMIT_PLAT_DATA  *Priv
  )
{
  EFI_STATUS         Status;
  EMBEDDED_GPIO      *Gpio;
  EMBEDDED_GPIO_PIN  GpioPin;

  Gpio    = Priv->Gpio;
  GpioPin = (EMBEDDED_GPIO_PIN)Priv->PhyResetGpioPin;

  //
  // initial level HIGH (1)
  //
  Status = Gpio->Set (Gpio, GpioPin, GPIO_MODE_OUTPUT_1);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to set GPIO %u direction/output HIGH, Status=%r\n",
      __func__,
      (UINT32)GpioPin,
      Status
      ));
    return Status;
  }

  gBS->Stall (2 * 1000);

  //
  // Assert reset: drive LOW
  //
  Status = Gpio->Set (Gpio, GpioPin, GPIO_MODE_OUTPUT_0);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to drive GPIO %u LOW, Status=%r\n",
      __func__,
      (UINT32)GpioPin,
      Status
      ));
    return Status;
  }

  gBS->Stall (20 * 1000);

  //
  // Deassert reset: drive HIGH
  //
  Status = Gpio->Set (Gpio, GpioPin, GPIO_MODE_OUTPUT_1);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to drive GPIO %u HIGH, Status=%r\n",
      __func__,
      (UINT32)GpioPin,
      Status
      ));
    return Status;
  }

  gBS->Stall (100 * 1000);

  return EFI_SUCCESS;
}

/**
  Validate PHY interface mode and reference clock configuration.

  @param[in]  Priv   Pointer to platform data.

  @retval EFI_SUCCESS           Configuration is valid.
  @retval EFI_INVALID_PARAMETER Unsupported mode or invalid RMII refclk config.
**/
STATIC
EFI_STATUS
K3ValidateIfaceAndRefclk (
  IN SPACEMIT_PLAT_DATA  *Priv
  )
{
  switch (Priv->PhyInterface) {
    case PHY_INTERFACE_MODE_MII:
      if (Priv->TxClkFromSoc) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: MII interface cannot use TX clock from SoC\n",
          __func__
          ));
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    case PHY_INTERFACE_MODE_RMII:
      if (Priv->TxClkFromSoc) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: RMII interface cannot use TX clock from SoC\n",
          __func__
          ));
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    case PHY_INTERFACE_MODE_RGMII:
    case PHY_INTERFACE_MODE_RGMII_ID:
    case PHY_INTERFACE_MODE_RGMII_RXID:
    case PHY_INTERFACE_MODE_RGMII_TXID:
      return EFI_SUCCESS;

    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: Unsupported PHY interface mode=%d\n",
        __func__,
        Priv->PhyInterface
        ));
      return EFI_UNSUPPORTED;
  }
}

/**
  Adjust TX/RX clock phase based on PHY interface mode.

  @param[in]  Priv   Pointer to platform data.
**/
STATIC
VOID
K3EqosSetClkPhase (
  IN SPACEMIT_PLAT_DATA  *Priv
  )
{
  PHY_INTERFACE  Iface;

  Iface = Priv->PhyInterface;

  if (!Priv->ClkTuningEnable) {
    return;
  }

  switch (Iface) {
    case PHY_INTERFACE_MODE_RGMII_ID:
      //
      // PHY already provides TX + RX delay
      //
      return;

    case PHY_INTERFACE_MODE_RGMII_TXID:
      //
      // PHY provides TX delay; only adjust RX
      //
      ClkPhaseSet (Priv, RX_PHASE);
      return;

    case PHY_INTERFACE_MODE_RGMII_RXID:
      //
      // PHY provides RX delay; only adjust TX
      //
      ClkPhaseSet (Priv, TX_PHASE);
      return;

    case PHY_INTERFACE_MODE_RMII:
    case PHY_INTERFACE_MODE_RGMII:
      //
      // RGMII/RMII: adjust both TX and RX phases
      //
      ClkPhaseSet (Priv, TX_PHASE);
      ClkPhaseSet (Priv, RX_PHASE);
      return;

    default:
      DEBUG ((
        DEBUG_INFO,
        "%a: Clk tuning skipped for phy-mode (iface=%d)\n",
        __func__,
        Iface
        ));
      return;
  }
}

/**
  Retrieve clock resource names from the controller config.

  - BusClkAndRstName: Mandatory (Axi Bus/Reset clock).
  - TxClkName:        Optional  (Required if TxClkFromSoc is TRUE).
  - PhyClkName:       Optional  (Required if PhyClkFromSoc is TRUE).

  @param[in, out] Priv   Pointer to platform data.
  @param[in]      Cfg    Pointer to controller config data.

  @retval EFI_SUCCESS           Resources obtained successfully.
  @retval EFI_INVALID_PARAMETER Missing mandatory or optional clock name.
**/
STATIC
EFI_STATUS
K3GetClockRes (
  IN OUT SPACEMIT_PLAT_DATA                    *Priv,
  IN     CONST DW_EQOS_CONTROLLER_CONFIG_DATA  *Cfg
  )
{
  if ((Priv == NULL) || (Cfg == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Priv->TxClkFromSoc  = Cfg->TxClkFromSoc;
  Priv->PhyClkFromSoc = Cfg->PhyClkFromSoc;

  Priv->BusClkAndRstName = Cfg->BusClkAndRstName;
  if ((Priv->BusClkAndRstName == NULL) || (Priv->BusClkAndRstName[0] == '\0')) {
    return EFI_INVALID_PARAMETER;
  }

  Priv->TxClkName = Cfg->TxClkName;
  if (Priv->TxClkFromSoc && ((Priv->TxClkName == NULL) || (Priv->TxClkName[0] == '\0'))) {
    return EFI_INVALID_PARAMETER;
  }

  Priv->PhyClkName = Cfg->PhyClkName;
  if (Priv->PhyClkFromSoc && ((Priv->PhyClkName == NULL) || (Priv->PhyClkName[0] == '\0'))) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
  Spacemit platform-specific initialization.

  - Build SPACEMIT_PLAT_DATA from PCDs
  - Open GPIO and Clock protocol
  - Validate PHY interface / refclk config
  - Reset PHY, configure iface and clock phase

  @param[in]  Eqos   Pointer to EQOS device.

  @retval EFI_SUCCESS           Initialization completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failure.
  @retval EFI_INVALID_PARAMETER Invalid PHY interface or refclk config.
  @retval Others                Underlying GPIO / PHY reset errors.
**/
EFI_STATUS
EFIAPI
K3PlatInit (
  IN EQOS_DEVICE  *Eqos
  )
{
  EFI_STATUS                            Status;
  SPACEMIT_PLAT_DATA                    *Priv;
  EMBEDDED_GPIO                         *Gpio;
  SILICON_CLOCKCTRL_PROTOCOL            *mClockCtrlProtocol;
  CONST DW_EQOS_CONTROLLER_CONFIG_DATA  *Cfg;

  ASSERT (Eqos->ControllerConfig != NULL);
  Cfg = Eqos->ControllerConfig;

  //
  // Allocate platform private data
  //
  Priv = AllocateZeroPool (sizeof (SPACEMIT_PLAT_DATA));
  if (Priv == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool failed\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  EqosSetPlatPriv (Eqos, Priv);

  //
  // Get PHY interface from config
  //
  Status = Eqos->Config->GetInterface (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get interface\n", __func__));
    goto ErrorExit;
  }

  Priv->PhyInterface = Eqos->PhyInterface;
  if (Priv->PhyInterface == PHY_INTERFACE_MODE_NA) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid PHY interface from config\n", __func__));
    Status = EFI_INVALID_PARAMETER;
    goto ErrorExit;
  }

  //
  // BUS / TX / PHY clock source
  //
  Status = K3GetClockRes (Priv, Cfg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to obtain clk resource\n", __func__));
    goto ErrorExit;
  }

  //
  // Validate interface + refclk combination
  //
  Status = K3ValidateIfaceAndRefclk (Priv);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Unsupported phy-mode=%d with tx clk from %a\n",
      __func__,
      Priv->PhyInterface,
      Priv->TxClkFromSoc ? "SoC" : "PHY"
      ));
    goto ErrorExit;
  }

  Status = gBS->LocateProtocol (
                  &gSpacemitSiliconPinCtrlProtocolGuid,
                  NULL,
                  (VOID **)&Priv->PinCtrl
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = Priv->PinCtrl->ApplyStateById (
                            Priv->PinCtrl,
                            "gmac",
                            Cfg->ControllerId,
                            NULL,
                            NULL
                            );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Priv->PhyResetGpioPin = Cfg->PhyResetGpioPin;
  Status                = gBS->LocateProtocol (
                                 &gEmbeddedGpioProtocolGuid,
                                 NULL,
                                 (VOID **)&Gpio
                                 );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: LocateProtocol(EmbeddedGpio) failed\n", __func__));
    goto ErrorExit;
  }

  Priv->Gpio = Gpio;

  Status = gBS->LocateProtocol (
                  &gSpacemitSiliconClockCtrlProtocolGuid,
                  NULL,
                  (VOID **)&mClockCtrlProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: LocateProtocol(gSpacemitSiliconClockCtrl) failed\n", __func__));
    goto ErrorExit;
  }

  Priv->Clock = mClockCtrlProtocol;

  Priv->CtrlReg = (EFI_PHYSICAL_ADDRESS)Cfg->CtrlReg;
  MapRegToGcdMmioSpace (Priv->CtrlReg, 0x100);
  Priv->DlineReg = (EFI_PHYSICAL_ADDRESS)Cfg->DlineReg;
  MapRegToGcdMmioSpace (Priv->DlineReg, 0x100);

  Priv->ClkTuningEnable = Cfg->ClkTuningEnable;
  if (Priv->ClkTuningEnable) {
    CLK_TUNING_WAY  Way;

    Way = (CLK_TUNING_WAY)Cfg->ClkTuningWay;

    switch (Way) {
      case CLK_TUNING_BY_REG:
      case CLK_TUNING_BY_CLK_REVERT:
        Priv->ClkTuningWay = Way;
        break;

      case CLK_TUNING_BY_DLINE:
        Priv->ClkTuningWay = Way;
        break;

      default:
        DEBUG ((DEBUG_ERROR, "%a: Invalid ClkTuningWay=%d\n", __func__, Way));
        Status = EFI_INVALID_PARAMETER;
        goto ErrorExit;
    }

    Priv->TxClkPhase = Cfg->TxPhase;
    Priv->RxClkPhase = Cfg->RxPhase;
  }

  //
  // Ensure the PHY clock is kept always enabled by the SoC,
  // not dynamically switched.
  //
  if (Priv->PhyClkFromSoc) {
    EqosClockEnable (Priv, Priv->PhyClkName);
  }

  K3EqosIfaceConfig (Priv);
  K3DelaylineInit (Priv);
  K3EqosSetClkPhase (Priv);

  return EFI_SUCCESS;

ErrorExit:
  EqosSetPlatPriv (Eqos, NULL);
  FreePool (Priv);

  return Status;
}

/**
  Clean up and remove platform resources for Spacemit EQOS.

  @param[in]  Eqos   Pointer to EQOS device.

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_INVALID_PARAMETER Invalid EQOS device.
**/
EFI_STATUS
EFIAPI
K3PlatDeinit (
  IN EQOS_DEVICE  *Eqos
  )
{
  SPACEMIT_PLAT_DATA  *Priv;

  Priv = EqosGetPlatPriv (Eqos);
  if (Priv == NULL) {
    return EFI_SUCCESS;
  }

  if (Priv->PhyClkFromSoc) {
    EqosClockDisable (Priv, Priv->PhyClkName);
  }

  FreePool (Priv);
  Eqos->Plat = NULL;

  return EFI_SUCCESS;
}

/**
  Deassert reset for the EQOS device (typically initiates hardware).

  @param[in]  Eqos   Pointer to EQOS device.

  @retval EFI_SUCCESS           Reset deasserted successfully.
  @retval EFI_INVALID_PARAMETER Invalid EQOS device or other error.
**/
EFI_STATUS
EFIAPI
SpacemitDeassertReset (
  IN EQOS_DEVICE  *Eqos
  )
{
  SPACEMIT_PLAT_DATA  *Priv;
  EFI_STATUS          Status;

  Priv = EqosGetPlatPriv (Eqos);
  if (Priv == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  EqosClockEnable (Priv, Priv->BusClkAndRstName);

  Status = K3EqosPhyReset (Priv);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: K3EqosPhyReset() failed\n", __func__));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Assert reset for the EQOS device.

  @param[in]  Eqos   Pointer to EQOS device.

  @retval EFI_SUCCESS           Reset asserted successfully.
  @retval EFI_INVALID_PARAMETER Invalid EQOS device or other error.
**/
EFI_STATUS
EFIAPI
SpacemitAssertReset (
  IN EQOS_DEVICE  *Eqos
  )
{
  SPACEMIT_PLAT_DATA  *Priv;

  Priv = EqosGetPlatPriv (Eqos);
  if (Priv == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  EqosClockDisable (Priv, Priv->BusClkAndRstName);

  return EFI_SUCCESS;
}

/**
  Enable clocks for the EQOS device (currently no clock interface).

  @param[in]  Eqos   Pointer to EQOS device.

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_UNSUPPORTED       No clock interface available.
**/
EFI_STATUS
EFIAPI
SpacemitEnableClocks (
  IN EQOS_DEVICE  *Eqos
  )
{
  SPACEMIT_PLAT_DATA  *Priv;

  Priv = EqosGetPlatPriv (Eqos);
  if (Priv == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Priv->TxClkFromSoc) {
    EqosClockEnable (Priv, Priv->TxClkName);
  }

  return EFI_SUCCESS;
}

/**
  Disable clocks for the EQOS device (currently no clock interface).

  @param[in]  Eqos   Pointer to EQOS device.

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_UNSUPPORTED       No clock interface available.
**/
EFI_STATUS
EFIAPI
SpacemitDisableClocks (
  IN EQOS_DEVICE  *Eqos
  )
{
  SPACEMIT_PLAT_DATA  *Priv;

  Priv = EqosGetPlatPriv (Eqos);
  if (Priv == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Priv->TxClkFromSoc) {
    EqosClockDisable (Priv, Priv->TxClkName);
  }

  return EFI_SUCCESS;
}

EQOS_PLATFORM_OPS  K3PlatOps = {
  .PlatInit           = K3PlatInit,
  .PlatDeinit         = K3PlatDeinit,
  .EnableClocks       = SpacemitEnableClocks,
  .DisableClocks      = SpacemitDisableClocks,
  .AssertReset        = SpacemitAssertReset,
  .DeassertReset      = SpacemitDeassertReset,
  .CalibratePads      = EqosNullOps,
  .DisableCalibration = EqosNullOps,
  .SetTxClkSpeed      = EqosNullOps,
  .GetPermanentMac    = EqosNullOps
};

DWC_EQOS_CONFIG  K3EqosConfig = {
  .CsrClockRate      = 100000000,
  .MdioWait          = 10,
  .SwrWait           = 50,
  .ConfigMac         = EQOS_RXQ_CTRL0_EN_DCB,
  .ConfigMacMdio     = EQOS_MAC_MDIO_ADDRESS_CR_250_300,
  .AxiBusWidth       = EqosAxiBusWidth64,
  .RegAccessAlwaysOk = FALSE,
  .GetInterface      = EqosGetInterface,
  .PlatOps           = &K3PlatOps
};
