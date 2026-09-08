/** @file
  PCI Root Bridge Resource Configurea Data instance for Spacemit RISCV

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __PCI_ROOT_BRIDGE_RESOURCE_CONFIG_H__
#define __PCI_ROOT_BRIDGE_RESOURCE_CONFIG_H__

#pragma pack(1)

typedef struct {
  UINT64      PciBase;
  UINT64      PciSize;
  UINT64      CpuBase;
} PCI_ROOT_BRIDGE_RESOURCE_CONFIG_APERTURE;

typedef struct {
  UINT16                                    Segment;
  UINT64                                    ConfigBase;
  UINT64                                    ConfigSize;
  UINT64                                    BusBase;
  UINT64                                    BusLimit;
  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_APERTURE  Io;
  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_APERTURE  Mem;
  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_APERTURE  Mem64;
  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_APERTURE  PMem;
  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_APERTURE  PMem64;
  BOOLEAN                                   IsEnabled;
} PCI_ROOT_BRIDGE_RESOURCE_CONFIG_DATA;

typedef struct {
  UINT16                                    ArrayNum;
  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_DATA      ArrayData[0];
} PCI_ROOT_BRIDGE_RESOURCE_CONFIG_ARRAY;

#pragma pack()

#endif /* #ifndef __PCI_ROOT_BRIDGE_RESOURCE_CONFIG_H__ */
