/** @file
  Synopsys DesignWare PCIe controller configuration data

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DESIGN_WARE_PCIE_CONTROLLER_CONFIG_H__
#define __DESIGN_WARE_PCIE_CONTROLLER_CONFIG_H__

#define DW_PCIE_CONTROLLER_MODE_RC          0
#define DW_PCIE_CONTROLLER_MODE_EP          1
#define DW_PCIE_CONTROLLER_MODE_INVALID     0xffffffff

#pragma pack(1)

typedef struct {
  // Basic DWC PCIe controller configuration-space accessible over the DBI interface.
  UINT64  DbiBase;
  UINT64  DbiSize;
  // Shadow DWC PCIe config-space registers. DBI2 is mainly useful for the endpoint controller.
  UINT64  Dbi2Base;
  UINT64  Dbi2Size;
  // iATU/eDMA registers common for all device functions.
  UINT64  AtuBase;
  UINT64  AtuSize;
} DW_PCIE_REG_SPACE;

typedef struct {
  DW_PCIE_REG_SPACE     Reg;
  UINT32                NumLanes;

  //
  // MaxLinkSpeed:
  //  0: not limit the maximum link speed
  //  1: 2.5 GT/s
  //  2: 5.0 GT/s
  //  3: 8.0 GT/s
  //  4: 16.0 GT/s
  //  5: 32.0 GT/s
  //  6: 64.0 GT/s
  //
  UINT32                MaxLinkSpeed;

  //
  // ControllerMode:
  //  0: RC
  //  1: EP
  //
  UINT32                ControllerMode;

  //
  // Determine whether to enable CFG Shift Mode.
  // This is valid only when EcamEnabled == FALSE.
  //
  BOOLEAN               CfgShiftModeEnabled;

  //
  // Determine whether to enable ECAM.
  //
  BOOLEAN               EcamEnabled;
} DW_PCIE_CONTROLLER_CONFIG_DATA;

typedef struct {
  UINT16                            Num;
  DW_PCIE_CONTROLLER_CONFIG_DATA    Data[0];
} DW_PCIE_CONTROLLER_CONFIGS;

#pragma pack()

#endif /* ifndef __DESIGN_WARE_PCIE_CONTROLLER_CONFIG_H__ */
