/** @file
  Synopsys DesignWare EQoS controller configuration data

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DW_EQOS_CONTROLLER_CONFIG_H__
#define __DW_EQOS_CONTROLLER_CONFIG_H__

#define DW_EQOS_CLK_NAME_MAX_LEN  20
#define DW_EQOS_PHY_MODE_MAX_LEN  16

#pragma pack(1)

typedef struct {
  BOOLEAN    ClkTuningEnable;
  BOOLEAN    TxClkFromSoc;
  BOOLEAN    PhyClkFromSoc;
  UINT8      ClkTuningWay;
  UINT8      TxPhase;
  UINT8      RxPhase;
  UINT64     Base;
  UINT64     CtrlReg;
  UINT64     DlineReg;
  UINT32     ControllerId;
  UINT32     PhyAddr;
  UINT32     MaxSpeed;
  UINT32     PhyResetGpioPin;
  CHAR8      BusClkAndRstName[DW_EQOS_CLK_NAME_MAX_LEN];
  CHAR8      TxClkName[DW_EQOS_CLK_NAME_MAX_LEN];
  CHAR8      PhyClkName[DW_EQOS_CLK_NAME_MAX_LEN];
  CHAR8      PhyMode[DW_EQOS_PHY_MODE_MAX_LEN];
} DW_EQOS_CONTROLLER_CONFIG_DATA;

typedef struct {
  UINT16                            Num;
  DW_EQOS_CONTROLLER_CONFIG_DATA    Data[0];
} DW_EQOS_CONTROLLER_CONFIGS;

#pragma pack()

#endif /* __DW_EQOS_CONTROLLER_CONFIG_H__ */
