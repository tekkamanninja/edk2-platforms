/** @file
 *  A clock driver based on RISC-V RPMI CLOCK Service Group
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef __RISCV_SBI_MPXY_RPMI_CLOCK_DXE_H__
#define __RISCV_SBI_MPXY_RPMI_CLOCK_DXE_H__

#include <IndustryStandard/Rpmi.h>
#include <Protocol/ClockCtrl.h>
#include <Library/BaseLib.h>
#include <Library/RiscVSbiMpxyRpmiLib.h>

#define RPMI_CLOCK_SIGNATURE              SIGNATURE_32('R', 'C', 'L', 'K')
#define RPMI_CLOCKCTRL_INSTANCE_FROM_THIS(a)   \
    CR (a, RPMI_CLOCKCTRL_INSTANCE, ClockCtrlProtocol, RPMI_CLOCK_SIGNATURE)

typedef struct {
  UINT32          ClockId;
  UINT32          TransitionLatency;
  UINT32          Type;
  UINT64          MinRate;
  UINT64          MaxRate;
  UINT32          NumRates;
  VOID            *Rates;
  CHAR8           ClockName[RPMI_CLK_NAME_LEN];
} RPMI_CLOCK_DEVICE;

typedef struct {
  UINTN                         Signature;
  EFI_HANDLE                    Handle;
  SILICON_CLOCKCTRL_PROTOCOL    ClockCtrlProtocol;
  MPXY_RPMI_CHANNEL             *MpxyRpmiChan;
  RPMI_CLOCK_DEVICE             *ClockDevices;
  UINT32                        NumDevices;
} RPMI_CLOCKCTRL_INSTANCE;

#endif
