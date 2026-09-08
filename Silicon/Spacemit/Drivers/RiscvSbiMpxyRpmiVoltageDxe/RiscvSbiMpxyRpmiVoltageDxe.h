/** @file
 *  Spacemit voltage modification driver header.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef __RPMI_VOLTAGE_DXE_H__
#define __RPMI_VOLTAGE_DXE_H__

#include <Library/BaseLib.h>
#include <Library/RiscVSbiMpxyLib.h>
#include <Library/RiscVSbiMpxyRpmiLib.h>
#include <Protocol/Voltage.h>
#include <IndustryStandard/Rpmi.h>

#define RPMI_VOLTAGE_SIGNATURE  SIGNATURE_32('R', 'L', 'T', 'G')
#define VOLTAGE_INSTANCE_FROM_THIS(a)  CR (a, VOLTAGE_INSTANCE, VoltageProtocol, RPMI_VOLTAGE_SIGNATURE)

typedef struct {
  CHAR8                   DomainName[RPMI_VOLT_DOMAIN_NAME_MAX_LEN];
  UINT32                  NameCrc32;
  UINT32                  DomainId;
  UINT32                  ControlSupport;
  UINT32                  NumLevels;
  UINT32                  TransLatency;

  UINT32                  VoltType;
  UINT32                  SupportVoltNum;
  DISCRETE_VOLTAGE        *DiscreteVolt;
  LINEAR_VOLTAGE_RANGE    *LinearVoltRange;
} VOLTAGE_DEVICE;

typedef struct {
  UINTN                       Signature;
  EFI_HANDLE                  Handle;

  SILICON_VOLTAGE_PROTOCOL    VoltageProtocol;
  MPXY_RPMI_CHANNEL           *MpxyRpmiChan;
  VOLTAGE_DEVICE              *VoltageDevice;
  UINT32                      VoltageDeviceNum;
} VOLTAGE_INSTANCE;

#endif /* __RPMI_VOLTAGE_DXE_H__ */
