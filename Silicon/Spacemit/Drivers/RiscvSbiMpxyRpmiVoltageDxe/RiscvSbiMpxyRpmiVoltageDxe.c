/** @file
  Voltage modification that use RPMI MPXY protocol for communication.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <IndustryStandard/Rpmi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "RiscvSbiMpxyRpmiVoltageDxe.h"

/**
 * Gets the voltage domain ID for a given name.
 *
 * @param[in]  DomainName        Name of the voltage domain to lookup
 * @param[out] DomainId          Pointer to store the found voltage domain ID
 * @param[in]  VoltageInstance   Pointer to voltage instance structure
 *
 * @retval EFI_SUCCESS           Domain ID was found successfully
 * @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 * @retval EFI_NOT_FOUND         Domain with given name was not found
 */
STATIC
EFI_STATUS
GetVoltageDomainId (
  IN CONST CHAR8    *DomainName,
  OUT UINT32        *DomainId,
  VOLTAGE_INSTANCE  *VoltageInstance
  )
{
  UINT32  I, Crc32Value;

  if ((NULL == DomainName) || (NULL == DomainId) || (NULL == VoltageInstance)) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == VoltageInstance->VoltageDevice) {
    DEBUG ((DEBUG_ERROR, "%a: VoltageDevice has NOT been init yet\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  if (EFI_ERROR (gBS->CalculateCrc32 ((VOID *)DomainName, AsciiStrLen (DomainName), &Crc32Value))) {
    return EFI_CRC_ERROR;
  }

  for (I = 0; I < VoltageInstance->VoltageDeviceNum; I++) {
    // compare CRC32 code first, save compare time
    if ((Crc32Value == VoltageInstance->VoltageDevice[I].NameCrc32)
        && (0 == AsciiStrCmp (DomainName, VoltageInstance->VoltageDevice[I].DomainName)))
    {
      *DomainId = I;
      return EFI_SUCCESS;
    }
  }

  DEBUG ((DEBUG_ERROR, "Fail to find voltage domain %a\n", DomainName));
  return EFI_NOT_FOUND;
}

/**
 * Lists supported voltage ranges for a given voltage domain.
 *
 * @param[in] This           Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in] DomainName     Name of the voltage domain to query
 *
 * @retval EFI_SUCCESS           Successfully listed voltage ranges
 * @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 * @retval EFI_NOT_FOUND         Specified voltage domain not found
 */
STATIC
EFI_STATUS
EFIAPI
ListVoltage (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName
  )
{
  UINT32            I, DomainNum, DomainId;
  VOLTAGE_INSTANCE  *VoltageInstance;
  VOLTAGE_DEVICE    *VoltageDevice;

  if ((NULL == This)) {
    return EFI_INVALID_PARAMETER;
  }

  VoltageInstance = VOLTAGE_INSTANCE_FROM_THIS (This);
  VoltageDevice   = VoltageInstance->VoltageDevice;

  if ((NULL != DomainName) &&
      !EFI_ERROR (GetVoltageDomainId (DomainName, &DomainId, VoltageInstance)))
  {
    // List voltage of specified voltage domain
    DomainNum = DomainId + 1;
  } else {
    // List voltage of all domains
    DomainNum = VoltageInstance->VoltageDeviceNum;
    DomainId  = 0;
  }

  for ( ; DomainId < DomainNum; DomainId++) {
    if (RPMI_VOLTAGE_FORMAT_DISCRETE == VoltageDevice[DomainId].VoltType) {
      DEBUG (
             (DEBUG_INFO, "%a: supported voltage level: [",
              VoltageDevice[DomainId].DomainName)
             );
      for (I = 0; I < VoltageDevice[DomainId].SupportVoltNum; I++) {
        DEBUG (
               (DEBUG_INFO, " %u uV,",
                VoltageDevice[DomainId].DiscreteVolt[I].Volt)
               );
      }

      DEBUG ((DEBUG_INFO, " ]\n"));
    } else if (RPMI_VOLTAGE_FORMAT_LINEAR == VoltageDevice[DomainId].VoltType) {
      for (I = 0; I < VoltageDevice[DomainId].SupportVoltNum; I++) {
        DEBUG (
               (DEBUG_INFO, "%a: supported voltage level %u: Min=%u uV, Max=%u uV, Step=%u uV\n",
                VoltageDevice[DomainId].DomainName,
                I,
                VoltageDevice[DomainId].LinearVoltRange[I].MinVolt,
                VoltageDevice[DomainId].LinearVoltRange[I].MaxVolt,
                VoltageDevice[DomainId].LinearVoltRange[I].VoltStep)
               );
      }
    }
  }

  return EFI_SUCCESS;
}

/**
 * Enables voltage output of a specified voltage domain.
 *
 * @param[in] This          Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in] DomainName    Name of the voltage domain to enable
 *
 * @retval EFI_SUCCESS           Successfully enabled the voltage domain
 * @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 * @retval EFI_NOT_READY         RPMI channel not initialized
 * @retval EFI_NOT_FOUND         Specified voltage domain not found
 * @retval Others                Error occurred during RPMI message handling
 */
STATIC
EFI_STATUS
EFIAPI
Enable (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName
  )
{
  UINT32                     DomainId;
  EFI_STATUS                 Status;
  MPXY_RPMI_MESSAGE          Msg;
  RPMI_VOLT_SET_CONFIG_REQ   Req;
  RPMI_VOLT_SET_CONFIG_RESP  Resp;
  VOLTAGE_INSTANCE           *VoltageInstance;

  if ((NULL == This) || (NULL == DomainName)) {
    return EFI_INVALID_PARAMETER;
  }

  VoltageInstance = VOLTAGE_INSTANCE_FROM_THIS (This);

  if (NULL == VoltageInstance->MpxyRpmiChan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage domain channel is not inited yet\n", __func__));
    return EFI_NOT_READY;
  }

  if (EFI_ERROR (GetVoltageDomainId (DomainName, &DomainId, VoltageInstance))) {
    return EFI_NOT_FOUND;
  }

  Req.DomainId = DomainId;
  Req.Config   = RPMI_VOLTAGE_SUPPLY_ENABLE;

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_SET_CONFIG;
  Msg.TxBuf     = (VOID *)&Req;
  Msg.TxLen     = sizeof (Req);
  Msg.RxBuf     = (VOID *)&Resp;
  Msg.RxBufLen  = sizeof (Resp);
  Msg.RxLen     = NULL;

  Status = MpxyRpmiSendMessage (VoltageInstance->MpxyRpmiChan, &Msg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
    return Status;
  }

  if (RPMI_SUCCESS != Resp.Status ) {
    DEBUG (
           (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_SET_CONFIG service failed (error code: %d)\n",
            __func__, Resp.Status)
           );
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
 * Disables voltage of a specified voltage domain.
 *
 * @param[in] This          Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in] DomainName    Name of the voltage domain to disable
 *
 * @retval EFI_SUCCESS           Successfully disabled the voltage domain
 * @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 * @retval EFI_NOT_READY         RPMI channel not initialized
 * @retval EFI_NOT_FOUND         Specified voltage domain not found
 * @retval Others                Error occurred during RPMI message handling
 */
STATIC
EFI_STATUS
EFIAPI
Disable (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName
  )
{
  UINT32                     DomainId;
  EFI_STATUS                 Status;
  MPXY_RPMI_MESSAGE          Msg;
  RPMI_VOLT_SET_CONFIG_REQ   Req;
  RPMI_VOLT_SET_CONFIG_RESP  Resp;
  VOLTAGE_INSTANCE           *VoltageInstance;

  if ((NULL == This) || (NULL == DomainName)) {
    return EFI_INVALID_PARAMETER;
  }

  VoltageInstance = VOLTAGE_INSTANCE_FROM_THIS (This);

  if (NULL == VoltageInstance->MpxyRpmiChan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage domain channel is not inited yet\n", __func__));
    return EFI_NOT_READY;
  }

  if (EFI_ERROR (GetVoltageDomainId (DomainName, &DomainId, VoltageInstance))) {
    return EFI_NOT_FOUND;
  }

  Req.DomainId = DomainId;
  Req.Config   = RPMI_VOLTAGE_SUPPLY_DISABLE;

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_SET_CONFIG;
  Msg.TxBuf     = (VOID *)&Req;
  Msg.TxLen     = sizeof (Req);
  Msg.RxBuf     = (VOID *)&Resp;
  Msg.RxBufLen  = sizeof (Resp);
  Msg.RxLen     = NULL;

  Status = MpxyRpmiSendMessage (VoltageInstance->MpxyRpmiChan, &Msg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
    return Status;
  }

  if (RPMI_SUCCESS != Resp.Status ) {
    DEBUG (
           (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_SET_CONFIG service failed (error code: %d)\n",
            __func__, Resp.Status)
           );
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
 * Checks if voltage of a specified voltage domain is enabled.
 *
 * @param[in]  This           Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in]  DomainName     Name of the voltage domain to check
 * @param[out] Enabled        Pointer to store the enabled status (TRUE if enabled)
 *
 * @retval EFI_SUCCESS            Status was successfully retrieved
 * @retval EFI_INVALID_PARAMETER  One or more parameters are invalid
 * @retval EFI_NOT_READY          RPMI channel not initialized
 * @retval EFI_NOT_FOUND          Specified voltage domain not found
 * @retval Others                 Error occurred during RPMI message handling
 */
STATIC
EFI_STATUS
EFIAPI
IsEnabled (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName,
  IN BOOLEAN                   *Enabled
  )
{
  UINT32                     DomainId;
  EFI_STATUS                 Status;
  MPXY_RPMI_MESSAGE          Msg;
  RPMI_VOLT_GET_CONFIG_REQ   Req;
  RPMI_VOLT_GET_CONFIG_RESP  Resp;
  VOLTAGE_INSTANCE           *VoltageInstance;

  if ((NULL == This) || (NULL == DomainName) || (NULL == Enabled)) {
    return EFI_INVALID_PARAMETER;
  }

  VoltageInstance = VOLTAGE_INSTANCE_FROM_THIS (This);

  if (NULL == VoltageInstance->MpxyRpmiChan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage domain channel is not inited yet\n", __func__));
    return EFI_NOT_READY;
  }

  if (EFI_ERROR (GetVoltageDomainId (DomainName, &DomainId, VoltageInstance))) {
    return EFI_NOT_FOUND;
  }

  Req.DomainId = DomainId;

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_GET_CONFIG;
  Msg.TxBuf     = (VOID *)&Req;
  Msg.TxLen     = sizeof (Req);
  Msg.RxBuf     = (VOID *)&Resp;
  Msg.RxBufLen  = sizeof (Resp);
  Msg.RxLen     = NULL;

  Status = MpxyRpmiSendMessage (VoltageInstance->MpxyRpmiChan, &Msg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
    return Status;
  }

  if (RPMI_SUCCESS != Resp.Status ) {
    DEBUG (
           (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_GET_CONFIG service failed (error code: %d)\n",
            __func__, Resp.Status)
           );
    return EFI_DEVICE_ERROR;
  }

  *Enabled = (((Resp.Config & RPMI_VOLT_SUPPLY_STATE_MASK) >> RPMI_VOLT_SUPPLY_STATE_SHIFT)
              == RPMI_VOLTAGE_SUPPLY_ENABLE) ? TRUE : FALSE;
  return EFI_SUCCESS;
}

/**
 * Gets the supported voltage level for a voltage domain device that is closest to the requested voltage.
 *
 * @param[in]  VoltageDevice            Pointer to the voltage domain device structure
 * @param[in]  VoltageMicroVolt         Requested voltage level in microvolts
 * @param[out] SupportVoltageMicroVolt  Pointer to store the closest supported voltage level
 *
 * @retval EFI_SUCCESS           Successfully found a supported voltage level
 * @retval EFI_INVALID_PARAMETER VoltageDevice is NULL or SupportVoltageMicroVolt is NULL
 * @retval EFI_UNSUPPORTED       Requested voltage is not in any supported range
 **/
STATIC
EFI_STATUS
GetSupportVoltage (
  VOLTAGE_DEVICE  *VoltageDevice,
  IN UINT32       VoltageMicroVolt,
  IN UINT32       *SupportVoltageMicroVolt
  )
{
  UINT32  I;

  if ((NULL == VoltageDevice) || (NULL == SupportVoltageMicroVolt)) {
    return EFI_INVALID_PARAMETER;
  }

  if (RPMI_VOLTAGE_FORMAT_DISCRETE == VoltageDevice->VoltType) {
    if ((1 == VoltageDevice->SupportVoltNum) &&
        (VoltageMicroVolt >= VoltageDevice->DiscreteVolt[0].Volt))
    {
      *SupportVoltageMicroVolt = VoltageDevice->DiscreteVolt[0].Volt;
      return EFI_SUCCESS;
    }

    for (I = 0; I < VoltageDevice->SupportVoltNum; I++) {
      if ((VoltageMicroVolt == VoltageDevice->DiscreteVolt[I].Volt))
      {
        *SupportVoltageMicroVolt = VoltageDevice->DiscreteVolt[I].Volt;
        return EFI_SUCCESS;
      }
    }
  } else if (RPMI_VOLTAGE_FORMAT_LINEAR == VoltageDevice->VoltType) {
    for (I = 0; I < VoltageDevice->SupportVoltNum; I++) {
      if ((VoltageMicroVolt >= VoltageDevice->LinearVoltRange[I].MinVolt) &&
          (VoltageMicroVolt <= VoltageDevice->LinearVoltRange[I].MaxVolt))
      {
        *SupportVoltageMicroVolt = VoltageDevice->LinearVoltRange[I].MinVolt +
                                   (((VoltageMicroVolt - VoltageDevice->LinearVoltRange[I].MinVolt) /
                                     VoltageDevice->LinearVoltRange[I].VoltStep)
                                    * VoltageDevice->LinearVoltRange[I].VoltStep);
        return EFI_SUCCESS;
      }
    }
  }

  return EFI_UNSUPPORTED;
}

/**
 * Sets the voltage level for a specified voltage domain.
 *
 * @param[in] This              Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in] DomainName        Name of the voltage domain to set voltage for
 * @param[in] VoltageMicroVolt  Target voltage level in microvolts
 *
 * @retval EFI_SUCCESS           Voltage was set successfully
 * @retval EFI_INVALID_PARAMETER If This or DomainName is NULL
 * @retval Other                 Other errors that may occur during voltage setting
 **/
STATIC
EFI_STATUS
EFIAPI
SetVoltage (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName,
  IN UINTN                     VoltageMicroVolt
  )
{
  UINT32                    DomainId;
  EFI_STATUS                Status;
  MPXY_RPMI_MESSAGE         Msg;
  RPMI_VOLT_SET_LEVEL_REQ   Req;
  RPMI_VOLT_SET_LEVEL_RESP  Resp;
  VOLTAGE_INSTANCE          *VoltageInstance;

  if ((NULL == This) || (NULL == DomainName)) {
    return EFI_INVALID_PARAMETER;
  }

  VoltageInstance = VOLTAGE_INSTANCE_FROM_THIS (This);

  if (NULL == VoltageInstance->MpxyRpmiChan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage domain channel is not inited yet\n", __func__));
    return EFI_NOT_READY;
  }

  if (EFI_ERROR (GetVoltageDomainId (DomainName, &DomainId, VoltageInstance))) {
    return EFI_NOT_FOUND;
  }

  if (EFI_ERROR (
                 GetSupportVoltage (
                                    &VoltageInstance->VoltageDevice[DomainId],
                                    (UINT32)VoltageMicroVolt,
                                    (UINT32 *)&Req.VoltLevel
                                    )
                 ))
  {
    DEBUG (
           (DEBUG_ERROR, "%a: Not support voltage %u uV for voltage domain %a\n",
            __func__, VoltageMicroVolt, DomainName)
           );
    return EFI_UNSUPPORTED;
  }

  Req.DomainId = DomainId;

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_SET_LEVEL;
  Msg.TxBuf     = (VOID *)&Req;
  Msg.TxLen     = sizeof (Req);
  Msg.RxBuf     = (VOID *)&Resp;
  Msg.RxBufLen  = sizeof (Resp);
  Msg.RxLen     = NULL;

  Status = MpxyRpmiSendMessage (VoltageInstance->MpxyRpmiChan, &Msg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
    return Status;
  }

  if (RPMI_SUCCESS != Resp.Status ) {
    DEBUG (
           (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_SET_LEVEL service failed (error code: %d)\n",
            __func__, Resp.Status)
           );
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
 * Gets the voltage level for a specified voltage domain.
 *
 * @param[in]  This              Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in]  DomainName        Name of the voltage domain to query
 * @param[out] VoltageMicroVolt  Pointer to store the voltage level in microvolts
 *
 * @retval EFI_SUCCESS            Voltage level was successfully retrieved
 * @retval EFI_INVALID_PARAMETER  One or more parameters are invalid
 * @retval Others                 Error occurred during RPMI message handling
 */
STATIC
EFI_STATUS
EFIAPI
GetVoltage (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName,
  OUT UINTN                    *VoltageMicroVolt
  )
{
  UINT32                    DomainId;
  EFI_STATUS                Status;
  MPXY_RPMI_MESSAGE         Msg;
  RPMI_VOLT_GET_LEVEL_REQ   Req;
  RPMI_VOLT_GET_LEVEL_RESP  Resp;
  VOLTAGE_INSTANCE          *VoltageInstance;

  if ((NULL == This) || (NULL == DomainName) || (NULL == VoltageMicroVolt)) {
    return EFI_INVALID_PARAMETER;
  }

  VoltageInstance = VOLTAGE_INSTANCE_FROM_THIS (This);

  if (NULL == VoltageInstance->MpxyRpmiChan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage domain channel is not inited yet\n", __func__));
    return EFI_NOT_READY;
  }

  if (EFI_ERROR (GetVoltageDomainId (DomainName, &DomainId, VoltageInstance))) {
    return EFI_NOT_FOUND;
  }

  Req.DomainId = DomainId;

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_GET_LEVEL;
  Msg.TxBuf     = (VOID *)&Req;
  Msg.TxLen     = sizeof (Req);
  Msg.RxBuf     = (VOID *)&Resp;
  Msg.RxBufLen  = sizeof (Resp);
  Msg.RxLen     = NULL;

  Status = MpxyRpmiSendMessage (VoltageInstance->MpxyRpmiChan, &Msg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
    return Status;
  }

  if (RPMI_SUCCESS != Resp.Status ) {
    DEBUG (
           (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_GET_LEVEL service failed (error code: %d)\n",
            __func__, Resp.Status)
           );
    return EFI_DEVICE_ERROR;
  }

  *VoltageMicroVolt = Resp.VoltLevel;
  return EFI_SUCCESS;
}

/**
  Init the RPMI voltage channel

  @param  VoltageInstance  The pointer to the VOLTAGE_INSTANCE.
  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
STATIC
EFI_STATUS
RpmiVoltageChannelInit (
  VOLTAGE_INSTANCE  *VoltageInstance
  )
{
  EFI_STATUS                        Status;
  UINT32                            MpxyChanCount;
  UINT32                            *MpxyChanIds;
  UINT32                            Index;
  SBI_MPXY_RPMI_CHANNEL_ATTRIBUTES  MpxyRpmiAttrs;
  UINT32                            DomainChanId;
  BOOLEAN                           DomainChanFound;

  Status = SbiMpxyGetChannelCount (&MpxyChanCount);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get MPXY channel count\n", __func__));
    return Status;
  }

  MpxyChanIds = AllocateZeroPool (sizeof (UINT32) * MpxyChanCount);
  if (MpxyChanIds == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for MPXY channel IDs\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  Status = SbiMpxyGetChannelIds (0, MpxyChanCount, MpxyChanIds);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get all MPXY channel IDs\n", __func__));
    Status = EFI_UNSUPPORTED;
    goto FreeMpxyChanIds;
  }

  //
  // Read the RPMI attributes of each MPXY channel, and find the one whose
  // service group ID matches the RPMI VOLTAGE service group.
  //
  DomainChanFound = FALSE;
  for (Index = 0; Index < MpxyChanCount; Index++) {
    Status = SbiMpxyReadChannelAttrs (
                                      MpxyChanIds[Index],
                                      SbiMpxyChanAttrMsgProtAttrStart,
                                      sizeof (MpxyRpmiAttrs) / sizeof (UINT32),
                                      (UINT32 *)&MpxyRpmiAttrs
                                      );
    if (Status != EFI_SUCCESS) {
      continue;
    }

    if (RPMI_SRVGRP_VOLTAGE == MpxyRpmiAttrs.ServicegroupId) {
      DomainChanId    = MpxyChanIds[Index];
      DomainChanFound = TRUE;
      break;
    }
  }

  if (!DomainChanFound) {
    DEBUG ((DEBUG_ERROR, "%a: MPXY channel for RPMI VOLTAGE service group not found\n", __func__));
    Status = EFI_UNSUPPORTED;
    goto FreeMpxyChanIds;
  }

  DEBUG ((DEBUG_INFO, "Found voltage MPXY channel %d successfully\n", DomainChanId));
  VoltageInstance->MpxyRpmiChan = MpxyRpmiOpenChannel (DomainChanId);
  if (NULL == VoltageInstance->MpxyRpmiChan) {
    DEBUG (
           (DEBUG_ERROR, "%a: Failed to open MPXY RPMI channel (channel ID: 0x%x)\n",
            __func__, DomainChanId)
           );
    Status = EFI_UNSUPPORTED;
  }

FreeMpxyChanIds:
  //
  // MpxyChanIds is a temporary buffer to store all the MPXY channel IDs, and
  // it always should be released at the end.
  //
  FreePool (MpxyChanIds);

  return Status;
}

/**
  Deinit the voltage RPMI channel

  @param  VoltageInstance  The pointer to the VOLTAGE_INSTANCE.

  @retval VOID

**/
VOID
RpmiVoltageChannelDeinit (
  VOLTAGE_INSTANCE  *VoltageInstance
  )
{
  if (NULL != VoltageInstance->MpxyRpmiChan) {
    MpxyRpmiCloseChannel (VoltageInstance->MpxyRpmiChan);
    VoltageInstance->MpxyRpmiChan = NULL;
  }
}

VOID
RpmiVoltageResouceRelease (
  VOLTAGE_INSTANCE  *VoltageInstance
  )
{
  UINT32  I;

  if (NULL == VoltageInstance) {
    return;
  }

  RpmiVoltageChannelDeinit (VoltageInstance);

  if (NULL != VoltageInstance->VoltageDevice) {
    for (I = 0; I < VoltageInstance->VoltageDeviceNum; I++) {
      if (NULL != VoltageInstance->VoltageDevice[I].LinearVoltRange) {
        FreePool (VoltageInstance->VoltageDevice[I].LinearVoltRange);
      }

      if (NULL != VoltageInstance->VoltageDevice[I].DiscreteVolt) {
        FreePool (VoltageInstance->VoltageDevice[I].DiscreteVolt);
      }
    }

    FreePool (VoltageInstance->VoltageDevice);
  }

  FreePool (VoltageInstance);
  VoltageInstance = NULL;
}

STATIC
EFI_STATUS
RpmiVoltageGetNum (
  MPXY_RPMI_CHANNEL  *Chan,
  UINT32             *VoltageDeviceNum
  )
{
  EFI_STATUS                      Status;
  MPXY_RPMI_MESSAGE               Msg;
  RPMI_VOLT_GET_NUM_DOMAINS_RESP  Resp;

  if (NULL == Chan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage channel is not inited\n", __func__));
    return EFI_NOT_READY;
  }

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_GET_NUM_DOMAINS;
  Msg.TxBuf     = NULL;
  Msg.TxLen     = 0;
  Msg.RxBuf     = (VOID *)&Resp;
  Msg.RxBufLen  = sizeof (Resp);
  Msg.RxLen     = NULL;

  Status = MpxyRpmiSendMessage (Chan, &Msg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
    return Status;
  }

  if (Resp.Status != RPMI_SUCCESS) {
    DEBUG (
           (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_GET_NUM_DOMAINS service failed (error code: %d)\n",
            __func__, Resp.Status)
           );
    return EFI_DEVICE_ERROR;
  }

  *VoltageDeviceNum = Resp.NumDomains;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
RpmiVoltageGetAttr (
  MPXY_RPMI_CHANNEL  *Chan,
  UINT32             DomainId,
  VOLTAGE_DEVICE     *VoltageDevice
  )
{
  UINT32                         Type, Length;
  EFI_STATUS                     Status;
  MPXY_RPMI_MESSAGE              Msg;
  RPMI_VOLT_GET_ATTRIBUTES_REQ   Req;
  RPMI_VOLT_GET_ATTRIBUTES_RESP  Resp;

  if (NULL == Chan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage channel is not inited\n", __func__));
    return EFI_NOT_READY;
  }

  if (NULL == VoltageDevice) {
    DEBUG ((DEBUG_ERROR, "%a: VoltageDevice is NULL\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Req.DomainId = DomainId;

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_GET_ATTRIBUTES;
  Msg.TxBuf     = (VOID *)&Req;
  Msg.TxLen     = sizeof (Req);
  Msg.RxBuf     = (VOID *)&Resp;
  Msg.RxBufLen  = sizeof (Resp);
  Msg.RxLen     = NULL;

  Status = MpxyRpmiSendMessage (Chan, &Msg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
    return Status;
  }

  if (Resp.Status != RPMI_SUCCESS) {
    DEBUG (
           (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_GET_ATTRIBUTES service failed (error code: %d)\n",
            __func__, Resp.Status)
           );
    return EFI_DEVICE_ERROR;
  }

  Type = (Resp.Flags & RPMI_VOLT_FORMAT_TYPE_MASK) >> RPMI_VOLT_FORMAT_TYPE_SHIFT;
  if ((RPMI_VOLTAGE_FORMAT_DISCRETE != Type) && (RPMI_VOLTAGE_FORMAT_LINEAR != Type)) {
    // Currently only support discrete or linear voltage type
    DEBUG ((DEBUG_ERROR, "%a: NOT support voltage type %d\n", __func__, Type));
    return EFI_UNSUPPORTED;
  }

  VoltageDevice->DomainId       = DomainId;
  VoltageDevice->VoltType       = Type;
  VoltageDevice->ControlSupport =
    (Resp.Flags & RPMI_VOLT_CONTROL_SUPPORT_MASK) >> RPMI_VOLT_CONTROL_SUPPORT_SHIFT;
  VoltageDevice->NumLevels    = Resp.NumLevels;
  VoltageDevice->TransLatency = Resp.TransLatency;

  Length = AsciiStrLen (Resp.DomainName);
  AsciiStrCpyS (VoltageDevice->DomainName, RPMI_VOLT_DOMAIN_NAME_MAX_LEN, Resp.DomainName);
  gBS->CalculateCrc32 ((VOID *)Resp.DomainName, Length, &VoltageDevice->NameCrc32);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
RpmiVoltageGetLinearVoltRange (
  MPXY_RPMI_CHANNEL  *Chan,
  UINT32             DomainId,
  VOLTAGE_DEVICE     *VoltageDevice
  )
{
  UINT32                               I, J, MsgLen;
  EFI_STATUS                           Status;
  MPXY_RPMI_MESSAGE                    Msg;
  RPMI_VOLT_GET_SUPPORTED_LEVELS_REQ   Req;
  RPMI_VOLT_GET_SUPPORTED_LEVELS_RESP  *Resp;
  LINEAR_VOLTAGE_RANGE                 *VoltRange;

  if (NULL == Chan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage channel is not inited\n", __func__));
    return EFI_NOT_READY;
  }

  if (NULL == VoltageDevice) {
    DEBUG ((DEBUG_ERROR, "%a: VoltageDevice is NULL\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  VoltRange = AllocateZeroPool (sizeof (LINEAR_VOLTAGE_RANGE) * VoltageDevice->NumLevels);
  if (NULL == VoltRange) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for voltage range\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  // MUST has at leaset 1 group linear volgate range
  MsgLen = MIN (
                MpxyRpmiMessageLengthMax (Chan),
                sizeof (RPMI_VOLT_GET_SUPPORTED_LEVELS_RESP) +
                VoltageDevice->NumLevels * sizeof (LINEAR_VOLTAGE_RANGE)
                );
  Resp = (RPMI_VOLT_GET_SUPPORTED_LEVELS_RESP *)AllocateZeroPool (MsgLen);
  if (NULL == Resp) {
    DEBUG (
           (DEBUG_ERROR,
            "%a: Failed to allocate memory for RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS response data\n",
            __func__)
           );
    FreePool (VoltRange);
    return EFI_OUT_OF_RESOURCES;
  }

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS;
  Msg.TxBuf     = (VOID *)&Req;
  Msg.TxLen     = sizeof (Req);
  Msg.RxBuf     = (VOID *)Resp;
  Msg.RxBufLen  = MsgLen;
  Msg.RxLen     = NULL;

  Req.DomainId = DomainId;
  I            = 0;
  while (I < VoltageDevice->NumLevels) {
    Req.VoltLevelIndex = I;

    Status = MpxyRpmiSendMessage (Chan, &Msg);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
      break;
    }

    if (Resp->Status != RPMI_SUCCESS) {
      DEBUG (
             (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS service failed (error code: %d)\n",
              __func__, Resp->Status)
             );
      break;
    }

    if (0 == Resp->Returned) {
      DEBUG ((DEBUG_ERROR, "%a: NO linear voltage tuple returned\n", __func__));
      break;
    }

    for (J = 0; J < Resp->Returned; I++, J++) {
      VoltRange[I].MinVolt  = Resp->VoltLevel[J * 3 + 0];
      VoltRange[I].MaxVolt  = Resp->VoltLevel[J * 3 + 1];
      VoltRange[I].VoltStep = Resp->VoltLevel[J * 3 + 2];
    }

    if (0 == Resp->Remaining) {
      break;
    }
  }

  FreePool (Resp);

  if (0 != I) {
    VoltageDevice->LinearVoltRange = VoltRange;
    VoltageDevice->SupportVoltNum  = I;
    return EFI_SUCCESS;
  } else {
    FreePool (VoltRange);
    return EFI_DEVICE_ERROR;
  }
}

STATIC
EFI_STATUS
RpmiVoltageGetDiscreteVolt (
  MPXY_RPMI_CHANNEL  *Chan,
  UINT32             DomainId,
  VOLTAGE_DEVICE     *VoltageDevice
  )
{
  UINT32                               I, J, MsgLen;
  EFI_STATUS                           Status;
  MPXY_RPMI_MESSAGE                    Msg;
  RPMI_VOLT_GET_SUPPORTED_LEVELS_REQ   Req;
  RPMI_VOLT_GET_SUPPORTED_LEVELS_RESP  *Resp;
  DISCRETE_VOLTAGE                     *Voltage;

  if (NULL == Chan) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI voltage channel is not inited\n", __func__));
    return EFI_NOT_READY;
  }

  if (NULL == VoltageDevice) {
    DEBUG ((DEBUG_ERROR, "%a: VoltageDevice is NULL\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Voltage = AllocateZeroPool (sizeof (DISCRETE_VOLTAGE) * VoltageDevice->NumLevels);
  if (NULL == Voltage) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for voltage buffer\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  MsgLen = MIN (
                MpxyRpmiMessageLengthMax (Chan),
                sizeof (RPMI_VOLT_GET_SUPPORTED_LEVELS_RESP) +
                VoltageDevice->NumLevels * sizeof (DISCRETE_VOLTAGE)
                );
  Resp = (RPMI_VOLT_GET_SUPPORTED_LEVELS_RESP *)AllocateZeroPool (MsgLen);
  if (NULL == Resp) {
    DEBUG (
           (DEBUG_ERROR,
            "%a: Failed to allocate memory for RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS response data\n",
            __func__)
           );
    FreePool (Voltage);
    return EFI_OUT_OF_RESOURCES;
  }

  Msg.Type      = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS;
  Msg.TxBuf     = (VOID *)&Req;
  Msg.TxLen     = sizeof (Req);
  Msg.RxBuf     = (VOID *)Resp;
  Msg.RxBufLen  = MsgLen;
  Msg.RxLen     = NULL;

  Req.DomainId = DomainId;
  I            = 0;
  while (I < VoltageDevice->NumLevels) {
    Req.VoltLevelIndex = I;

    Status = MpxyRpmiSendMessage (Chan, &Msg);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to send MPXY RPMI message\n", __func__));
      break;
    }

    if (Resp->Status != RPMI_SUCCESS) {
      DEBUG (
             (DEBUG_ERROR, "%a: RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS service failed (error code: %d)\n",
              __func__, Resp->Status)
             );
      break;
    }

    if (0 == Resp->Returned) {
      DEBUG ((DEBUG_ERROR, "%a: NO discrete voltage tuple returned\n", __func__));
      break;
    }

    for (J = 0; J < Resp->Returned; I++, J++) {
      Voltage[I].Volt = Resp->VoltLevel[J];
    }

    if (0 == Resp->Remaining) {
      break;
    }
  }

  FreePool (Resp);

  if (0 != I) {
    VoltageDevice->DiscreteVolt   = Voltage;
    VoltageDevice->SupportVoltNum = I;
    return EFI_SUCCESS;
  } else {
    FreePool (Voltage);
    return EFI_DEVICE_ERROR;
  }
}

STATIC
EFI_STATUS
RpmiVoltageEnumerate (
  VOLTAGE_INSTANCE  *VoltageInstance
  )
{
  UINT32          I;
  VOLTAGE_DEVICE  *VoltageDevice;
  EFI_STATUS      Status;

  Status = RpmiVoltageGetNum (
                              VoltageInstance->MpxyRpmiChan,
                              &VoltageInstance->VoltageDeviceNum
                              );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get number of voltage domain devices\n", __func__));
    return Status;
  }

  VoltageDevice = AllocateZeroPool (
                                    sizeof (VOLTAGE_DEVICE) *
                                    VoltageInstance->VoltageDeviceNum
                                    );
  if (VoltageDevice == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for voltage domain devices\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  VoltageInstance->VoltageDevice = VoltageDevice;
  for (I = 0; I < VoltageInstance->VoltageDeviceNum; I++) {
    Status = RpmiVoltageGetAttr (
                                 VoltageInstance->MpxyRpmiChan,
                                 I,
                                 &VoltageDevice[I]
                                 );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get attributes for voltage domain device %u\n", __func__, I));
      FreePool (VoltageDevice);
      return Status;
    }

    if (RPMI_VOLTAGE_FORMAT_DISCRETE == VoltageDevice[I].VoltType) {
      Status = RpmiVoltageGetDiscreteVolt (
                                           VoltageInstance->MpxyRpmiChan,
                                           I,
                                           &VoltageDevice[I]
                                           );
    } else if (RPMI_VOLTAGE_FORMAT_LINEAR == VoltageDevice[I].VoltType) {
      Status = RpmiVoltageGetLinearVoltRange (
                                              VoltageInstance->MpxyRpmiChan,
                                              I,
                                              &VoltageDevice[I]
                                              );
    } else {
      Status = EFI_UNSUPPORTED;
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get supported levels for voltage domain device %u\n", __func__, I));
      FreePool (VoltageDevice);
      return Status;
    }
  }

  return Status;
}

EFI_STATUS
EFIAPI
RpmiVoltageInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS        Status;
  VOLTAGE_INSTANCE  *VoltageInstance;

  VoltageInstance = AllocateZeroPool (sizeof (VOLTAGE_INSTANCE));
  if (VoltageInstance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  VoltageInstance->Signature                        = RPMI_VOLTAGE_SIGNATURE;
  VoltageInstance->VoltageProtocol.Revision         = EFI_VOLTAGE_PROTOCOL_REVISION;
  VoltageInstance->VoltageProtocol.ListVoltageRange = ListVoltage;
  VoltageInstance->VoltageProtocol.EnableVoltage    = Enable;
  VoltageInstance->VoltageProtocol.DisableVoltage   = Disable;
  VoltageInstance->VoltageProtocol.IsVoltageEnabled = IsEnabled;
  VoltageInstance->VoltageProtocol.SetVoltage       = SetVoltage;
  VoltageInstance->VoltageProtocol.GetVoltage       = GetVoltage;

  if (EFI_ERROR (RpmiVoltageChannelInit (VoltageInstance))) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to init RPMI voltage channel\n", __func__));
    FreePool (VoltageInstance);
    return EFI_DEVICE_ERROR;
  }

  if (EFI_ERROR (RpmiVoltageEnumerate (VoltageInstance))) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to enumerate voltage domain\n", __func__));
    RpmiVoltageResouceRelease (VoltageInstance);
    return EFI_DEVICE_ERROR;
  }

  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gSpacemitSiliconVoltageProtocolGuid);
  Status = gBS->InstallMultipleProtocolInterfaces (
                                                   &VoltageInstance->Handle,
                                                   &gSpacemitSiliconVoltageProtocolGuid,
                                                   &VoltageInstance->VoltageProtocol,
                                                   NULL
                                                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install voltage protocol\n", __func__));
    RpmiVoltageResouceRelease (VoltageInstance);
  } else {
    ListVoltage (&VoltageInstance->VoltageProtocol, NULL);
  }

  return Status;
}
