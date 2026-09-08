/** @file
  K3 PCIe board wiring/resource configuration definitions for structured PCDs.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _K3_PCIE_PCD_CONFIG_H_
#define _K3_PCIE_PCD_CONFIG_H_

//
// Max number of PHYs that a single PCIe controller may reference.
// K3 can aggregate up to 6 PHY blocks (0..5) for Port A x8.
//
#define K3_PCIE_MAX_PHYS_PER_PORT  6

typedef struct {
  UINT32    PhyId;        // SoC PHY id (0..5), used for lane mux decisions
  UINT32    NumLanes;     // 1 or 2
  UINT64    PhyBase;      // PHY register base
  // ApbSpareBase is hardcoded to 0xD4090178
} K3_PCIE_PHY_RESOURCE;

typedef struct {
  UINT16               Num;
  K3_PCIE_PHY_RESOURCE Phy[0];
} K3_PCIE_PHY_CONFIG_ARRAY;

typedef struct {
  UINT32    PortId;                  // spacemit,pcie-port (0..4)
  UINT64    AppBase;                 // "app" wrapper register base
  UINT64    PhyAhbBase;              // "phy_ahb" base (link status etc.)
  UINT32    DeviceDetectGpio;        // Optional GPIO pin used for Port A lane split decision
  UINT32    DeviceDetectActiveLevel; // 1=active high, 0=active low
  UINT32    NumPhys;                 // Number of PHYs in PhyIndex[]
  UINT32    PhyIndex[K3_PCIE_MAX_PHYS_PER_PORT];
} K3_PCIE_PORT_RESOURCE;

typedef struct {
  UINT16               Num;
  K3_PCIE_PORT_RESOURCE Port[0];
} K3_PCIE_PORT_CONFIG_ARRAY;

#endif // _K3_PCIE_PCD_CONFIG_H_
