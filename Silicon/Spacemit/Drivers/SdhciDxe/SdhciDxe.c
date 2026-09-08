/** @file
 *
 *  Spacemit Cores SDHCI Controller driver
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include "SdhciDxe.h"

STATIC EFI_HANDLE  mSdMmcOverrideHandle  = NULL;
STATIC EFI_HANDLE  mSdControllerHandle   = NULL;
STATIC EFI_HANDLE  mEmmcControllerHandle = NULL;

STATIC
UINTN
FieldShift (
  UINTN  Mask
  )
{
  if (Mask == 0) {
    ASSERT (FALSE);
    return 0;
  }

  UINTN  Shift = 0;
  while ((Mask & 1) == 0) {
    Shift++;
    Mask >>= 1;
  }

  return Shift;
}

#define FIELD_PREP(mask, val) \
  ({ \
    UINTN _mask = (UINTN)(mask); \
    UINTN _val  = (UINTN)(val); \
    UINTN _shift = FieldShift(_mask); \
    UINTN _max_val = _mask >> _shift; \
    ASSERT((_val & ~_max_val) == 0); \
    ((_val << _shift) & _mask); \
  })

STATIC
EFI_STATUS
SpacemitSdMmcSetBits (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               OrData,
  IN UINTN                Offset
  )
{
  return SdhciHcOrMmio (PciIo, 0, Offset, sizeof (OrData), &OrData);
}

STATIC
EFI_STATUS
SpacemitSdMmcClrBits (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               AndData,
  IN UINTN                Offset
  )
{
  AndData = ~AndData;
  return SdhciHcAndMmio (PciIo, 0, Offset, sizeof (AndData), &AndData);
}

STATIC
EFI_STATUS
SpacemitSdMmcClrSetBits (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT32               AndData,
  IN UINT32               OrData,
  IN UINTN                Offset
  )
{
  AndData = ~AndData;
  return SdhciHcAndOrMmio (PciIo, 0, Offset, sizeof (AndData), &AndData, &OrData);
}

/**
  Initialize the SdMmc PHY.

**/
STATIC
VOID
SdMmcPhyInit (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN SD_MMC_TYPE          ControllerType
  )
{
  if (ControllerType == SdMmcTypeEmmc) {
    /* use phy func mode */
    SpacemitSdMmcSetBits (
                          PciIo,
                          SDHC_PHY_FUNC_EN | SDHC_PHY_PLL_LOCK,
                          SPACEMIT_SDHC_PHY_CTRL_REG
                          );
    SpacemitSdMmcClrSetBits (
                             PciIo,
                             SDHC_PHY_DRIVE_SEL,
                             SDHC_RX_BIAS_CTRL |
                             FIELD_PREP (SDHC_PHY_DRIVE_SEL, 4),
                             SPACEMIT_SDHC_PHY_PADCFG_REG
                             );

    /* mmc card mode */
    SpacemitSdMmcSetBits (PciIo, SDHC_MMC_CARD_MODE, SPACEMIT_SDHC_MMC_CTRL_REG);
  } else {
    /* sd/sdio has no phy */
    SpacemitSdMmcSetBits (PciIo, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);
  }

  SpacemitSdMmcClrBits (PciIo, SDHC_ENHANCE_STROBE_EN, SPACEMIT_SDHC_MMC_CTRL_REG);
}

STATIC
EFI_STATUS
SdMmcClkInit (
  IN CONST CHAR8  *ControllerName,
  IN UINT64       ClockRate
  )
{
  EFI_STATUS                  Status;
  SILICON_CLOCKCTRL_PROTOCOL  *mClockCtrlProtocol;

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconClockCtrlProtocolGuid,
                                NULL,
                                (VOID **)&mClockCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __FUNCTION__, __LINE__, Status));
    return Status;
  }

  mClockCtrlProtocol->SetClockState (
                                     mClockCtrlProtocol,
                                     ControllerName,
                                     ENABLE_CLOCK
                                     );

  mClockCtrlProtocol->SetClockRate (
                                    mClockCtrlProtocol,
                                    ControllerName,
                                    ClockRate
                                    );

  return EFI_SUCCESS;
}

STATIC
VOID
SdMmcPinCtrlInit (
  IN UINT32  ControllerId
  )
{
  EFI_STATUS                Status;
  SILICON_PINCTRL_PROTOCOL  *PinCtrlProtocol;

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconPinCtrlProtocolGuid,
                                NULL,
                                (VOID **)&PinCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __func__, __LINE__, Status));
    return;
  }

  if (PinCtrlProtocol->ApplyStateById == NULL) {
    DEBUG ((DEBUG_WARN, "%a: pinctrl state API is unavailable\n", __func__));
    return;
  }

  Status = PinCtrlProtocol->ApplyStateById (
                                            PinCtrlProtocol,
                                            "sdhci",
                                            ControllerId,
                                            NULL,
                                            PINCTRL_STATE_DEFAULT
                                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: failed to set pinctrl group Status=%r\n", __func__, Status));
  }
}

STATIC
EFI_STATUS
SdMmcGetPciIo (
  IN     EFI_HANDLE           ControllerHandle,
  IN OUT EFI_PCI_IO_PROTOCOL  **PciIo
  )
{
  EFI_STATUS  Status;

  *PciIo = NULL;
  Status = gBS->OpenProtocol (
                              ControllerHandle,
                              &gEfiPciIoProtocolGuid,
                              (VOID **)PciIo,
                              mSdMmcOverrideHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL
                              );
  return Status;
}

STATIC
VOID
SdMmcPhyDllInit (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS  Status;
  UINT32      Dllsts;

  SpacemitSdMmcClrSetBits (
                           PciIo,
                           SDHC_DLL_PREDLY_NUM |
                           SDHC_DLL_FULLDLY_RANGE |
                           SDHC_DLL_VREG_CTRL,
                           FIELD_PREP (SDHC_DLL_PREDLY_NUM, 1) |
                           FIELD_PREP (SDHC_DLL_FULLDLY_RANGE, 1) |
                           FIELD_PREP (SDHC_DLL_VREG_CTRL, 1),
                           SPACEMIT_SDHC_PHY_DLLCFG
                           );

  SpacemitSdMmcClrSetBits (
                           PciIo,
                           SDHC_DLL_REG1_CTRL,
                           FIELD_PREP (SDHC_DLL_REG1_CTRL, 0x92),
                           SPACEMIT_SDHC_PHY_DLLCFG1
                           );

  SpacemitSdMmcSetBits (PciIo, SDHC_DLL_ENABLE, SPACEMIT_SDHC_PHY_DLLCFG);

  Status = SdhciHcWaitMmioSet (
                               PciIo,
                               0,
                               SPACEMIT_SDHC_PHY_DLLSTS,
                               sizeof (Dllsts),
                               SDHC_DLL_LOCK_STATE,
                               SDHC_DLL_LOCK_STATE,
                               100
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "SdMmcPhyDllInit: wait dll lock failed with %r\n", Status));
  }
}

STATIC
VOID
SdMmcPreHs400Downgrade (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  SpacemitSdMmcClrBits (
                        PciIo,
                        SDHC_PHY_FUNC_EN | SDHC_PHY_PLL_LOCK,
                        SPACEMIT_SDHC_PHY_CTRL_REG
                        );
  SpacemitSdMmcClrBits (
                        PciIo,
                        SDHC_MMC_HS400 | SDHC_MMC_HS200 | SDHC_ENHANCE_STROBE_EN,
                        SPACEMIT_SDHC_MMC_CTRL_REG
                        );
  SpacemitSdMmcClrBits (PciIo, SDHC_HS200_USE_RFIFO, SPACEMIT_SDHC_PHY_FUNC_REG);

  gBS->Stall (5);

  SpacemitSdMmcSetBits (
                        PciIo,
                        SDHC_PHY_FUNC_EN | SDHC_PHY_PLL_LOCK,
                        SPACEMIT_SDHC_PHY_CTRL_REG
                        );
}

STATIC
VOID
SdMmcPreSelectHs400 (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  SpacemitSdMmcSetBits (PciIo, SDHC_MMC_HS400, SPACEMIT_SDHC_MMC_CTRL_REG);
}

STATIC
VOID
SdMmcPostSelectHs400 (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  return SdMmcPhyDllInit (PciIo);
}

STATIC
VOID
SdMmcRxTuningPrepare (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN SD_MMC_BUS_MODE      Timing,
  IN UINT8                Dline
  )
{
  EFI_STATUS  Status;
  UINT32      Cfg;

  Status = SdhciHcRwMmio (PciIo, 0, SPACEMIT_SDHC_DLINE_CFG_REG, TRUE, sizeof (Cfg), &Cfg);
  if (EFI_ERROR (Status)) {
    return;
  }

  if ((Timing == SdMmcUhsSdr50) && (Cfg & 0x40)) {
    SpacemitSdMmcClrSetBits (
                             PciIo,
                             SDHC_RX_DLINE_REG |
                             SDHC_RX_DLINE_GAIN,
                             FIELD_PREP (SDHC_RX_DLINE_REG, Dline) |
                             FIELD_PREP (SDHC_RX_DLINE_GAIN, 1),
                             SPACEMIT_SDHC_DLINE_CFG_REG
                             );
  } else {
    SpacemitSdMmcClrSetBits (
                             PciIo,
                             SDHC_RX_DLINE_REG |
                             SDHC_RX_DLINE_GAIN,
                             FIELD_PREP (SDHC_RX_DLINE_REG, Dline),
                             SPACEMIT_SDHC_DLINE_CFG_REG
                             );
  }

  SpacemitSdMmcSetBits (PciIo, SDHC_DLINE_PU, SPACEMIT_SDHC_DLINE_CTRL_REG);
  gBS->Stall (5);
  SpacemitSdMmcClrSetBits (
                           PciIo,
                           SDHC_RX_SDCLK_SEL1,
                           FIELD_PREP (SDHC_RX_SDCLK_SEL1, 1),
                           SPACEMIT_SDHC_RX_CFG_REG
                           );

  if (Timing == SdMmcMmcHs200) {
    SpacemitSdMmcSetBits (PciIo, SDHC_HS200_USE_RFIFO, SPACEMIT_SDHC_PHY_FUNC_REG);
  }
}

STATIC
VOID
SdMmcTxTuningPrepare (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT8                TxDelaycode
  )
{
  /* set TX_DLINE_REG */
  SpacemitSdMmcClrSetBits (
                           PciIo,
                           SDHC_RX_DLINE_GAIN,
                           FIELD_PREP (SDHC_TX_DLINE_REG, TX_TUNING_DLINE_REG),
                           SPACEMIT_SDHC_DLINE_CFG_REG
                           );
  /* set TX_DLINE_CODE */
  SpacemitSdMmcClrSetBits (
                           PciIo,
                           SDHC_TX_DLINE_CODE,
                           FIELD_PREP (SDHC_TX_DLINE_CODE, TxDelaycode),
                           SPACEMIT_SDHC_DLINE_CTRL_REG
                           );
  /* set SDHC_TX_INT_CLK_SEL */
  SpacemitSdMmcSetBits (PciIo, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);
  SpacemitSdMmcSetBits (PciIo, SDHC_DLINE_PU, SPACEMIT_SDHC_DLINE_CTRL_REG);
}

STATIC
EFI_STATUS
SdMmcTuningPre (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN SD_MMC_BUS_MODE      Timing
  )
{
  DEBUG ((DEBUG_INFO, "%a: Timing %d\n", __FUNCTION__, Timing));
  UINT8  TxDelaycode;

  if (PcdGet8 (PcdSdCardTxDelayCode) != 0) {
    TxDelaycode = PcdGet8 (PcdSdCardTxDelayCode);
  } else {
    TxDelaycode = TX_TUNING_DELAYCODE;
  }

  if (Timing != SdMmcMmcHs200) {
    DEBUG ((DEBUG_INFO, "Set Tx Delaycode: %d\n", TxDelaycode));
    SdMmcTxTuningPrepare (PciIo, TxDelaycode);
  } else {
    DEBUG ((DEBUG_INFO, "Use Default TxTiming\n"));
  }

  SdMmcRxTuningPrepare (PciIo, Timing, RX_TUNING_DLINE_REG);

  return EFI_SUCCESS;
}

/**
  Set SD Host Controller control 2 registry according to selected speed.

  @param[in] ControllerHandle The EFI_HANDLE of the controller.
  @param[in] Slot             The slot number of the SD card to send the command to.
  @param[in] Timing           The timing to select.

  @retval EFI_SUCCESS         The override function completed successfully.
  @retval EFI_NOT_FOUND       The specified controller or slot does not exist.
**/
STATIC
EFI_STATUS
SdMmcHcUhsSignaling (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT8                Slot,
  IN SD_MMC_BUS_MODE      Timing
  )
{
  DEBUG ((DEBUG_INFO, "%a: Timing %d\n", __FUNCTION__, Timing));

  return EFI_SUCCESS;
}

/**

  Additional operations specific for PciIo controller

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in]      Timing                The timing which should be set by
                                        PciIo controller.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.

**/
STATIC
EFI_STATUS
SwitchClockFreqPost (
  IN EFI_HANDLE       ControllerHandle,
  IN UINT8            Slot,
  IN SD_MMC_BUS_MODE  Timing
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;

  DEBUG ((DEBUG_INFO, "%a: Timing %d\n", __FUNCTION__, Timing));

  Status = SdMmcGetPciIo (ControllerHandle, &PciIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* according to the SDHC_TX_CFG_REG(0x11c<bit>),P
  * set TX_INT_CLK_SEL to gurantee the hold time
  * at default speed mode or HS/SDR12/SDR25/SDR50 mode.
  */
  if ((Timing == SdMmcMmcLegacy) ||
      (Timing == SdMmcMmcHsSdr) ||
      (Timing == SdMmcMmcHsDdr) ||
      (Timing == SdMmcSdDs) ||
      (Timing == SdMmcSdHs) ||
      (Timing == SdMmcUhsSdr12) ||
      (Timing == SdMmcUhsSdr25) ||
      (Timing == SdMmcUhsSdr50))
  {
    SpacemitSdMmcSetBits (PciIo, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);
  } else {
    SpacemitSdMmcClrBits (PciIo, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);
  }

  if ((Timing == SdMmcMmcHs200) ||
      (Timing == SdMmcMmcHs400))
  {
    SpacemitSdMmcSetBits (
                          PciIo,
                          (Timing == SdMmcMmcHs200) ? SDHC_MMC_HS200 : SDHC_MMC_HS400,
                          SPACEMIT_SDHC_MMC_CTRL_REG
                          );
  } else {
    SpacemitSdMmcClrBits (
                          PciIo,
                          SDHC_MMC_HS400 | SDHC_MMC_HS200 | SDHC_ENHANCE_STROBE_EN,
                          SPACEMIT_SDHC_MMC_CTRL_REG
                          );
  }

  switch (Timing) {
    case SdMmcMmcHsSdr:
      SdMmcPreHs400Downgrade (PciIo);
      break;
    case SdMmcMmcHs200:
    case SdMmcUhsSdr50:
    case SdMmcUhsSdr104:
      SdMmcTuningPre (PciIo, Timing);
      break;
    case SdMmcMmcHs400:
      SdMmcPreSelectHs400 (PciIo);
      SdMmcPostSelectHs400 (PciIo);
      break;
    default:
      return EFI_SUCCESS;
  }

  return EFI_SUCCESS;
}

/**

  Override function for SDHCI controller operations

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in]      PhaseType             The type of operation and whether the
                                        hook is invoked right before (pre) or
                                        right after (post)
  @param[in,out]  PhaseData             The pointer to a phase-specific data.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER PhaseType is invalid

**/
STATIC
EFI_STATUS
EFIAPI
SdMmcNotifyPhase (
  IN     EFI_HANDLE               ControllerHandle,
  IN     UINT8                    Slot,
  IN     EDKII_SD_MMC_PHASE_TYPE  PhaseType,
  IN OUT VOID                     *PhaseData
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  SD_MMC_BUS_MODE      *Timing;
  SD_MMC_TYPE          ControllerType;

  DEBUG (
         (DEBUG_INFO, "%a: ControllerHandle: %x, PhaseType: %d\n", __FUNCTION__,
          ControllerHandle, PhaseType)
         );

  if (ControllerHandle == mSdControllerHandle) {
    ControllerType = SdMmcTypeSd;
  } else if (ControllerHandle == mEmmcControllerHandle) {
    ControllerType = SdMmcTypeEmmc;
  } else {
    return EFI_NOT_FOUND;
  }

  ASSERT (Slot == 0);

  switch (PhaseType) {
    case EdkiiSdMmcInitHostPre:
      Status = SdMmcGetPciIo (ControllerHandle, &PciIo);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = PciIo->Attributes (
                                  PciIo,
                                  EfiPciIoAttributeOperationEnable,
                                  EFI_PCI_IO_ATTRIBUTE_DUAL_ADDRESS_CYCLE,
                                  NULL
                                  );

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "%a: failed to enable 64-bit DMA (%r)\n", __FUNCTION__, Status));
      }

      SdMmcPhyInit (PciIo, ControllerType);

      break;

    case EdkiiSdMmcUhsSignaling:
      if (PhaseData == NULL) {
        return EFI_INVALID_PARAMETER;
      }

      Timing = (SD_MMC_BUS_MODE *)PhaseData;

      Status = SdMmcHcUhsSignaling (
                                    ControllerHandle,
                                    Slot,
                                    *Timing
                                    );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      break;

    case EdkiiSdMmcSwitchClockFreqPost:
      if (PhaseData == NULL) {
        return EFI_INVALID_PARAMETER;
      }

      Timing = (SD_MMC_BUS_MODE *)PhaseData;

      Status = SwitchClockFreqPost (
                                    ControllerHandle,
                                    Slot,
                                    *Timing
                                    );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      break;

    default:
      break;
  }

  return EFI_SUCCESS;
}

/**
  Override function for SDHCI capability bits

  @param[in]      ControllerHandle      The EFI_HANDLE of the controller.
  @param[in]      Slot                  The 0 based slot index.
  @param[in,out]  SdMmcHcSlotCapability The SDHCI capability structure.
  @param[in,out]  BaseClkFreq           The base clock frequency value that
                                        optionally can be updated.

  @retval EFI_SUCCESS           The override function completed successfully.
  @retval EFI_NOT_FOUND         The specified controller or slot does not exist.
  @retval EFI_INVALID_PARAMETER SdMmcHcSlotCapability is NULL

**/
STATIC
EFI_STATUS
EFIAPI
SdMmcCapability (
  IN     EFI_HANDLE  ControllerHandle,
  IN     UINT8       Slot,
  IN OUT VOID        *SdMmcHcSlotCapability,
  IN OUT UINT32      *BaseClkFreq
  )
{
  SD_MMC_HC_SLOT_CAP  *Capability = SdMmcHcSlotCapability;
  SD_MMC_TYPE         ControllerType;

  if (SdMmcHcSlotCapability == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ControllerHandle == mSdControllerHandle) {
    ControllerType = SdMmcTypeSd;
  } else if (ControllerHandle == mEmmcControllerHandle) {
    ControllerType = SdMmcTypeEmmc;
  } else {
    return EFI_NOT_FOUND;
  }

  ASSERT (Slot == 0);

  if (ControllerType == SdMmcTypeSd) {
    /* 0: removable, 1: embedded */
    Capability->SlotType    = 0;
    Capability->BaseClkFreq = 204;
  } else {
    Capability->SlotType    = 1;
    Capability->BaseClkFreq = 187;
    Capability->Hs400       = 1;
  }

  *BaseClkFreq = Capability->BaseClkFreq;

  return EFI_SUCCESS;
}

STATIC EDKII_SD_MMC_OVERRIDE  mSdMmcOverride = {
  EDKII_SD_MMC_OVERRIDE_PROTOCOL_VERSION,
  SdMmcCapability,
  SdMmcNotifyPhase,
};

EFI_STATUS
EFIAPI
SdhciDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%a\n", __FUNCTION__));

  if (PcdGetBool (PcdSdCardIsEnabled)) {
    MapRegToGcdMmioSpace (SDCARD_BASE, SIZE_4KB);
    SdMmcClkInit ((CONST CHAR8 *)SDCARD_CONTROLLER_NAME, SDCARD_CLOCK_RATE);
    SdMmcPinCtrlInit (0);
    Status = RegisterNonDiscoverableMmioDevice (
                                                NonDiscoverableDeviceTypeSdhci,
                                                NonDiscoverableDeviceDmaTypeNonCoherent,
                                                NULL,
                                                &mSdControllerHandle,
                                                1,
                                                SDCARD_BASE,
                                                SDHCI_REG_SZ
                                                );
    ASSERT_EFI_ERROR (Status);
  }

  if (PcdGetBool (PcdEmmcIsEnabled)) {
    MapRegToGcdMmioSpace (EMMC_BASE, SIZE_4KB);
    SdMmcClkInit ((CONST CHAR8 *)EMMC_CONTROLLER_NAME, EMMC_CLOCK_RATE);
    Status = RegisterNonDiscoverableMmioDevice (
                                                NonDiscoverableDeviceTypeSdhci,
                                                NonDiscoverableDeviceDmaTypeNonCoherent,
                                                NULL,
                                                &mEmmcControllerHandle,
                                                1,
                                                EMMC_BASE,
                                                SDHCI_REG_SZ
                                                );
    ASSERT_EFI_ERROR (Status);
  }

  Status = gBS->InstallProtocolInterface (
                                          &ImageHandle,
                                          &gEdkiiSdMmcOverrideProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          (VOID **)&mSdMmcOverride
                                          );
  ASSERT_EFI_ERROR (Status);

  mSdMmcOverrideHandle = ImageHandle;

  return EFI_SUCCESS;
}
