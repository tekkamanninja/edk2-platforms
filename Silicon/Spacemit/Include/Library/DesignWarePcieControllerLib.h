/** @file
  Synopsys DesignWare PCIe controller interfaces

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DESIGN_WARE_PCIE_CONTROLLER_LIB_H__
#define __DESIGN_WARE_PCIE_CONTROLLER_LIB_H__

#include <Base.h>
#include <PiDxe.h>
#include <BitOps.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DesignWarePcieControllerConfig.h>
#include <Library/PciRootBrdigeResourceConfig.h>
#include <Library/DmaIoMmuConfig.h>

//
// DWC PCIe IP-core versions (native support since v4.70a)
//
#define DW_PCIE_VER_365A    0x3336352a
#define DW_PCIE_VER_460A    0x3436302a
#define DW_PCIE_VER_470A    0x3437302a
#define DW_PCIE_VER_480A    0x3438302a
#define DW_PCIE_VER_490A    0x3439302a
#define DW_PCIE_VER_520A    0x3532302a
#define DW_PCIE_VER_540A    0x3534302a

#define __DW_PCIE_VER_CMP(PtrToDwPcie, Ver, Op) \
  ((PtrToDwPcie)->Version Op Ver)

#define DW_PCIE_VER_IS(PtrToDwPcie, Ver)      __DW_PCIE_VER_CMP(PtrToDwPcie, Ver, ==)
#define DW_PCIE_VER_IS_GE(PtrToDwPcie, Ver)   __DW_PCIE_VER_CMP(PtrToDwPcie, Ver, >=)

//
// DWC PCIe controller capabilities
//
#define DW_PCIE_CAP_REQ_RES       0
#define DW_PCIE_CAP_IATU_UNROLL   1
#define DW_PCIE_CAP_CDM_CHECK     2

#define DW_PCIE_CAP_IS(PtrToDwPcie, CapBit) \
  ((PtrToDwPcie)->Caps & (1UL << (CapBit)))

#define DW_PCIE_CAP_SET(PtrToDwPcie, CapBit) \
  ((PtrToDwPcie)->Caps |= (1UL << (CapBit)))

//
// Parameters for the waiting for link up routine
//
#define DW_PCIE_LINK_WAIT_MAX_RETRIES             10
#define DW_PCIE_LINK_WAIT_SLEEP_MS                90

//
// Parameters for the waiting for iATU enabled routine
//
#define DW_PCIE_LINK_WAIT_MAX_IATU_RETRIES        5
#define DW_PCIE_LINK_WAIT_IATU_SLEEP_MS           9

//
// Synopsys-specific PCIe configuration registers
//
#define DW_PCIE_PORT_FORCE                        0x708
#define DW_PCIE_PORT_FORCE_DO_DESKEW_FOR_SRIS     BIT(23)

#define DW_PCIE_PORT_AFR                          0x70C
#define DW_PCIE_PORT_AFR_N_FTS_MASK               GENMASK(15, 8)
#define DW_PCIE_PORT_AFR_N_FTS(n)                 (((n) & 0xFF) << 8)
#define DW_PCIE_PORT_AFR_CC_N_FTS_MASK            GENMASK(23, 16)
#define DW_PCIE_PORT_AFR_CC_N_FTS(n)              (((n) & 0xFF) << 16)
#define DW_PCIE_PORT_AFR_ENTER_ASPM               BIT(30)
#define DW_PCIE_PORT_AFR_L0S_ENTRANCE_LAT_SHIFT   24
#define DW_PCIE_PORT_AFR_L0S_ENTRANCE_LAT_MASK    GENMASK(26, 24)
#define DW_PCIE_PORT_AFR_L1_ENTRANCE_LAT_SHIFT    27
#define DW_PCIE_PORT_AFR_L1_ENTRANCE_LAT_MASK     GENMASK(29, 27)

#define DW_PCIE_PORT_LINK_CONTROL                 0x710
#define DW_PCIE_PORT_LINK_DLL_LINK_EN             BIT(5)
#define DW_PCIE_PORT_LINK_FAST_LINK_MODE          BIT(7)
#define DW_PCIE_PORT_LINK_MODE_MASK               GENMASK(21, 16)
#define DW_PCIE_PORT_LINK_MODE(n)                 (((n) && 0x3F) << 16)
#define DW_PCIE_PORT_LINK_MODE_1_LANES            DW_PCIE_PORT_LINK_MODE(0x1)
#define DW_PCIE_PORT_LINK_MODE_2_LANES            DW_PCIE_PORT_LINK_MODE(0x3)
#define DW_PCIE_PORT_LINK_MODE_4_LANES            DW_PCIE_PORT_LINK_MODE(0x7)
#define DW_PCIE_PORT_LINK_MODE_8_LANES            DW_PCIE_PORT_LINK_MODE(0xf)
#define DW_PCIE_PORT_LINK_MODE_16_LANES           DW_PCIE_PORT_LINK_MODE(0x1f)

#define DW_PCIE_PORT_LANE_SKEW                    0x714

#define DW_PCIE_PORT_DEBUG0                       0x728

#define DW_PCIE_LINK_WIDTH_SPEED_CONTROL          0x80C
#define DW_PCIE_PORT_LOGIC_N_FTS_MASK             GENMASK(7, 0)
#define DW_PCIE_PORT_LOGIC_SPEED_CHANGE           BIT(17)
#define DW_PCIE_PORT_LOGIC_LINK_WIDTH_MASK        GENMASK(12, 8)
#define DW_PCIE_PORT_LOGIC_LINK_WIDTH(n)          (((n) & 0x1F) << 8)
#define DW_PCIE_PORT_LOGIC_LINK_WIDTH_1_LANES     DW_PCIE_PORT_LOGIC_LINK_WIDTH(0x1)
#define DW_PCIE_PORT_LOGIC_LINK_WIDTH_2_LANES     DW_PCIE_PORT_LOGIC_LINK_WIDTH(0x2)
#define DW_PCIE_PORT_LOGIC_LINK_WIDTH_4_LANES     DW_PCIE_PORT_LOGIC_LINK_WIDTH(0x4)
#define DW_PCIE_PORT_LOGIC_LINK_WIDTH_8_LANES     DW_PCIE_PORT_LOGIC_LINK_WIDTH(0x8)
#define DW_PCIE_PORT_LOGIC_LINK_WIDTH_16_LANES    DW_PCIE_PORT_LOGIC_LINK_WIDTH(0x10)

#define DW_PCIE_MSI_ADDR_LO                       0x820

#define DW_PCIE_MSI_ADDR_HI                       0x824

#define DW_PCIE_MSI_INTR0_ENABLE                  0x828

#define DW_PCIE_MSI_INTR0_MASK                    0x82C

#define DW_PCIE_MSI_INTR0_STATUS                  0x830

#define DW_PCIE_GEN3_RELATED_OFF                  0x890

#define DW_PCIE_GEN3_EQ_CONTROL_OFF               0x8A8

#define DW_PCIE_GEN3_EQ_FB_MODE_DIR_CHANGE_OFF    0x8AC

#define DW_PCIE_PORT_MULTI_LANE_CTRL              0x8C0

#define DW_PCIE_VERSION_NUMBER                    0x8F8

#define DW_PCIE_VERSION_TYPE                      0x8FC

//
// iATU inbound and outbound windows CSRs. Before the IP-core v4.80a each
// iATU region CSRs had been indirectly accessible by means of the dedicated
// viewport selector. The iATU/eDMA CSRs space was re-designed in DWC PCIe
// v4.80a in a way so the viewport was unrolled into the directly accessible
// iATU/eDMA CSRs space.
//
#define DW_PCIE_ATU_VIEWPORT                      0x900
#define DW_PCIE_ATU_REGION_DIR_IB                 BIT(31)
#define DW_PCIE_ATU_REGION_DIR_OB                 0
#define DW_PCIE_ATU_VIEWPORT_BASE                 0x904
#define DW_PCIE_ATU_UNROLL_BASE(dir, index) \
  (((index) << 9) | ((dir == DW_PCIE_ATU_REGION_DIR_IB) ? BIT(8) : 0))
#define DW_PCIE_ATU_VIEWPORT_SIZE                 0x2C
#define DW_PCIE_ATU_REGION_CTRL1                  0x000
#define DW_PCIE_ATU_INCREASE_REGION_SIZE          BIT(13)
#define DW_PCIE_ATU_TYPE_MEM                      0x0
#define DW_PCIE_ATU_TYPE_IO                       0x2
#define DW_PCIE_ATU_TYPE_CFG0                     0x4
#define DW_PCIE_ATU_TYPE_CFG1                     0x5
#define DW_PCIE_ATU_TYPE_MSG                      0x10
#define DW_PCIE_ATU_TD                            BIT(8)
#define DW_PCIE_ATU_FUNC_NUM(pf)                  ((pf) << 20)
#define DW_PCIE_ATU_REGION_CTRL2                  0x004
#define DW_PCIE_ATU_ENABLE                        BIT(31)
#define DW_PCIE_ATU_BAR_MODE_ENABLE               BIT(30)
#define DW_PCIE_ATU_CFG_SHIFT_MODE_ENABLE         BIT(28)
#define DW_PCIE_ATU_INHIBIT_PAYLOAD               BIT(22)
#define DW_PCIE_ATU_FUNC_NUM_MATCH_EN             BIT(19)
#define DW_PCIE_ATU_LOWER_BASE                    0x008
#define DW_PCIE_ATU_UPPER_BASE                    0x00C
#define DW_PCIE_ATU_LIMIT                         0x010
#define DW_PCIE_ATU_LOWER_TARGET                  0x014
#define DW_PCIE_ATU_BUS_CFG_SHIFT_OFF(x)         (((x) & 0xFF) << 24)
#define DW_PCIE_ATU_DEV_CFG_SHIFT_OFF(x)         (((x) & 0x1F) << 19)
#define DW_PCIE_ATU_FUNC_CFG_SHIFT_OFF(x)        (((x) & 0x07) << 16)
#define DW_PCIE_ATU_BUS_CFG_SHIFT_ON(x)          (((x) & 0xFF) << 20)
#define DW_PCIE_ATU_DEV_CFG_SHIFT_ON(x)          (((x) & 0x1F) << 15)
#define DW_PCIE_ATU_FUNC_CFG_SHIFT_ON(x)         (((x) & 0x07) << 12)
#define DW_PCIE_ATU_UPPER_TARGET                  0x018
#define DW_PCIE_ATU_UPPER_LIMIT                   0x020

#define DW_PCIE_MISC_CONTROL_1_OFF                0x8BC
#define DW_PCIE_DBI_RO_WR_EN                      BIT(0)

#define DW_PCIE_MSIX_DOORBELL                     0x948
#define DW_PCIE_MSIX_DOORBELL_PF_SHIFT            24

//
// eDMA CSRs. DW PCIe IP-core v4.70a and older had the eDMA registers accessible
// over the Port Logic registers space. Afterwards the unrolled mapping was
// introduced so eDMA and iATU could be accessed via a dedicated registers
// space.
//
#define DW_PCIE_DMA_VIEWPORT_BASE                 0x970
#define DW_PCIE_DMA_UNROLL_BASE                   0x80000
#define DW_PCIE_DMA_CTRL                          0x008
#define DW_PCIE_DMA_NUM_WR_CHAN                   GENMASK(3, 0)
#define DW_PCIE_DMA_NUM_RD_CHAN                   GENMASK(19, 16)

#define DW_PCIE_PL_CHK_REG_CONTROL_STATUS            0xB20
#define DW_PCIE_PL_CHK_REG_CHK_REG_START             BIT(0)
#define DW_PCIE_PL_CHK_REG_CHK_REG_CONTINUOUS        BIT(1)
#define DW_PCIE_PL_CHK_REG_CHK_REG_COMPARISON_ERROR  BIT(16)
#define DW_PCIE_PL_CHK_REG_CHK_REG_LOGIC_ERROR       BIT(17)
#define DW_PCIE_PL_CHK_REG_CHK_REG_COMPLETE          BIT(18)

#define DW_PCIE_PL_CHK_REG_ERR_ADDR                  0xB28

#define DW_PCIE_ECAM_BASE_ADDR_LWR                0xC70
#define DW_PCIE_ECAM_BASE_ADDR_UPPER              0xC74

#define DW_PCIE_ECAM_CTRL                         0xC78
#define DW_PCIE_ECAM_CTRL_DSP_ECAM_EN             BIT(0)

typedef struct {
  LIST_ENTRY Link;

  UINT64  PciBase;
  UINT64  CpuBase;
  UINT64  Size;
  UINT32  Flags;
} DW_PCIE_RESOURCE;

//
// Flags in DW_PCIE_RESOURCE
//
#define DW_PCIE_RESOURCE_TYPE_MASK              GENMASK(2, 0)   // Indicate the space type
#define DW_PCIE_RESOURCE_TYPE_SHIFT             0
#define   DW_PCIE_RESOURCE_TYPE_IO              0x1             // IO space
#define   DW_PCIE_RESOURCE_TYPE_MEM32           0x2             // 32-bit memory space
#define   DW_PCIE_RESOURCE_TYPE_MEM64           0x3             // 64-bit memory space
#define DW_PCIE_RESOURCE_PREFETCHABLE_MASK      BIT(3)          // Indicate whether prefetchable
#define DW_PCIE_RESOURCE_PREFETCHABLE_SHIFT     3
#define   DW_PCIE_RESOURCE_PREFETCHABLE         0x1

#define DW_PCIE_RESOURCE_FLAGS_SET(Name, Value)   \
  (((Value) << (DW_PCIE_RESOURCE_ ## Name ## _SHIFT)) & (DW_PCIE_RESOURCE_ ## Name ## _MASK))
#define DW_PCIE_RESOURCE_FLAGS_GET(Name, Flags)   \
  (((Flags) & (DW_PCIE_RESOURCE_ ## Name ## _MASK)) >> (DW_PCIE_RESOURCE_ ## Name ## _SHIFT))

#define DW_PCIE_RESOURCE_TYPE_SET(Type)           \
  DW_PCIE_RESOURCE_FLAGS_SET (TYPE, DW_PCIE_RESOURCE_TYPE_ ## Type)
#define DW_PCIE_RESOURCE_TYPE_IS_IO(Flags)        \
  (DW_PCIE_RESOURCE_FLAGS_GET (TYPE, Flags) == DW_PCIE_RESOURCE_TYPE_IO)
#define DW_PCIE_RESOURCE_TYPE_IS_MEM32(Flags)     \
  (DW_PCIE_RESOURCE_FLAGS_GET (TYPE, Flags) == DW_PCIE_RESOURCE_TYPE_MEM32)
#define DW_PCIE_RESOURCE_TYPE_IS_MEM64(Flags)     \
  (DW_PCIE_RESOURCE_FLAGS_GET (TYPE, Flags) == DW_PCIE_RESOURCE_TYPE_MEM64)
#define DW_PCIE_RESOURCE_TYPE_IS_MEM(Flags)       \
  (DW_PCIE_RESOURCE_TYPE_IS_MEM32 (Flags) || DW_PCIE_RESOURCE_TYPE_IS_MEM64 (Flags))

#define DW_PCIE_RESOURCE_PREFETCHABLE_SET()       \
  DW_PCIE_RESOURCE_FLAGS_SET (PREFETCHABLE, DW_PCIE_RESOURCE_PREFETCHABLE)
#define DW_PCIE_RESOURCE_IS_PREFETCHABLE(Flags)   \
  (DW_PCIE_RESOURCE_FLAGS_GET (PREFETCHABLE, Flags) == DW_PCIE_RESOURCE_PREFETCHABLE)

typedef struct {
  // Outbound iATU-capable memory-region which will be used to access the
  // peripheral PCIe devices configuration space.
  UINT64        ConfigBase;
  UINT64        ConfigSize;

  // Bus range
  UINT32        BusMin;
  UINT32        BusMax;

  // Segment number
  UINT32        Segment;

  // A list for outbound resources.
  LIST_ENTRY    Resources;
  // A list for inbound resources.
  LIST_ENTRY    IbResources;
} DW_PCIE_ROOT_PORT;

typedef struct {
  DW_PCIE_REG_SPACE     Reg;
  UINT32                Id;
  UINT32                Version;
  UINT32                Type;
  UINT32                Caps;
  UINT32                NumObWindows;
  UINT32                NumIbWindows;
  UINT32                RegionAlign;
  UINT64                RegionLimit;
  UINT32                NumLanes;
  UINT32                MaxLinkSpeed;
  UINT8                 NFts[2];
  BOOLEAN               CfgShiftModeEnabled;
  BOOLEAN               EcamEnabled;
  DW_PCIE_ROOT_PORT     Rp;
} DW_PCIE;

#define DW_PCIE_ROOT_PORT_TO_DW_PCIE(RootPort)  BASE_CR (RootPort, DW_PCIE, Rp)

typedef struct {
  INTN    Index;
  INTN    Type;
  UINT8   FuncNo;
  UINT8   Code;
  UINT8   Routing;
  UINT32  Ctrl2;
  UINT64  CpuAddr;
  UINT64  PciAddr;
  UINT64  Size;
} DW_PCIE_OB_ATU_CFG;

typedef struct {
  INTN    Index;
  INTN    Type;
  UINT32  Ctrl2;
  UINT64  CpuAddr;
  UINT64  PciAddr;
  UINT64  Size;
} DW_PCIE_IB_ATU_CFG;

//
// An array to store all the pointers to DW_PCIE_ROOT_PORTs.
// It use segment number as the array index.
//
extern CONST DW_PCIE_ROOT_PORT **gDwPcieRootPorts;

/**
  Allocate a DW_PCIE instance.

  @param  Id          The ID of the DW_PCIE instance.

  @retval   A pointer to the allocated DW_PCIE instances, or NULL on failure.

**/
DW_PCIE *
EFIAPI
DwPcieAllocateInstance (
  IN  UINT32   Id
  );

/**
  Free a DW_PCIE instance.

  @param  DwPcie         Pointer to DW_PCIE instance.

**/
VOID
EFIAPI
DwPcieFreeInstance (
  IN OUT  DW_PCIE     *DwPcie
  );

/**
  Read value from DesignWare PCIe DBI space.

  @param  DwPcie      Pointer to DW_PCIE instance.
  @param  Reg         Register offset.
  @param  Size        Size to read.

  @retval The value read.

**/
UINT32
EFIAPI
DwPcieReadDbi (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg,
  IN        UINT32    Size
  );

/**
  Write value to DesignWare PCIe DBI space.

  @param  DwPcie      Pointer to DW_PCIE instance.
  @param  Reg         Register offset.
  @param  Size        Size to write.
  @param  Value       Value to write.

**/
VOID
EFIAPI
DwPcieWriteDbi (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg,
  IN        UINT32    Size,
  IN        UINT32    Value
  );

/**
  Read a 32-bit value from DesignWare PCIe DBI space.

  @param  DwPcie      Pointer to DW_PCIE instance.
  @param  Reg         Register offset.

  @retval The value read.

**/
STATIC
inline
UINT32
EFIAPI
DwPcieReadDbi32 (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg
  )
{
  return DwPcieReadDbi (DwPcie, Reg, 4);
}

/**
  Write a 32-bit value to DesignWare PCIe DBI space.

  @param  DwPcie      Pointer to DW_PCIE instance.
  @param  Reg         Register offset.
  @param  Size        Size to write.
  @param  Value       Value to write.

**/
STATIC
inline
VOID
EFIAPI
DwPcieWriteDbi32 (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg,
  IN        UINT32    Value
  )
{
  DwPcieWriteDbi (DwPcie, Reg, 4, Value);
}

/**
  Read a 16-bit value from DesignWare PCIe DBI space.

  @param  DwPcie      Pointer to DW_PCIE instance.
  @param  Reg         Register offset.

  @retval The value read.

**/
STATIC
inline
UINT16
EFIAPI
DwPcieReadDbi16 (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg
  )
{
  return DwPcieReadDbi (DwPcie, Reg, 2);
}

/**
  Write a 16-bit value to DesignWare PCIe DBI space.

  @param  DwPcie      Pointer to DW_PCIE instance.
  @param  Reg         Register offset.
  @param  Size        Size to write.
  @param  Value       Value to write.

**/
STATIC
inline
VOID
EFIAPI
DwPcieWriteDbi16 (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg,
  IN        UINT16    Value
  )
{
  DwPcieWriteDbi (DwPcie, Reg, 2, Value);
}

/**
  Read a 8-bit value from DesignWare PCIe DBI space.

  @param  DwPcie      Pointer to DW_PCIE instance.
  @param  Reg         Register offset.

  @retval The value read.

**/
STATIC
inline
UINT8
EFIAPI
DwPcieReadDbi8 (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg
  )
{
  return DwPcieReadDbi (DwPcie, Reg, 1);
}

/**
  Write a 8-bit value to DesignWare PCIe DBI space.

  @param  DwPcie      Pointer to DW_PCIE instance.
  @param  Reg         Register offset.
  @param  Size        Size to write.
  @param  Value       Value to write.

**/
STATIC
inline
VOID
EFIAPI
DwPcieWriteDbi8 (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg,
  IN        UINT8     Value
  )
{
  DwPcieWriteDbi (DwPcie, Reg, 1, Value);
}

/**
  Configure ATU for outbound accesses.

  @param  DwPcie        Pointer to DW_PCIE instance.
  @param  AtuCfg        Pointer to ATU config.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieProgOutboundAtu (
  IN CONST  DW_PCIE             *DwPcie,
  IN CONST  DW_PCIE_OB_ATU_CFG  *AtuCfg
  );

/**
  Get configurations, such as register address, for DW_PCIE instance.

  @param  DwPcie                    Pointer to DW_PCIE instance.
  @param  DwPcieControllerConfig    Pointer to DW PCIe controller config.
  @param  RootBridgeResourceConfig  Pointer to Root Bridge resource config.
  @param  DmaMappings               Pointer to DMA mappings config. It can be
                                    NULL if we don't need to setup inbound
                                    resources.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieGetConfigs (
  IN OUT    DW_PCIE                                 *DwPcie,
  IN CONST  DW_PCIE_CONTROLLER_CONFIG_DATA          *DwPcieControllerConfig,
  IN CONST  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_DATA    *RootBridgeResourceConfig,
  IN CONST  DMA_IOMMU_MAPPINGS                      *DmaMappings                OPTIONAL
  );

/**
  Init DesignWare PCIe controller (host mode)

  @param  DwPcie                    Pointer to DW_PCIE instance.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieHostInit (
  IN OUT  DW_PCIE     *DwPcie
  );

/**
  Init the array gDwPcieRootPorts.

  All of the elements in gDwPcieRootPorts are initialized with NULL. Then
  DwPcieRootPortArrayRegisterElement() should be called to register the elements.

  Remember to call DwPcieRootPortArrayDeinit() if the gDwPcieRootPorts is no
  more used.

  @param  SegmentNumberMax    The max segment number.

  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieRootPortArrayInit (
  IN UINTN  SegmentNumberMax
  );

/**
  Deinit the array gDwPcieRootPorts.

  @param  VOID

  @retval VOID

**/
VOID
EFIAPI
DwPcieRootPortArrayDeinit (
  VOID
  );

/**
  Register an element to gDwPcieRootPorts.

  @param  Rp            The pointer to a DW_PCIE_ROOT_PORT instance.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieRootPortArrayRegisterElement (
  IN CONST  DW_PCIE_ROOT_PORT   *Rp
  );

#endif /* ifndef __DESIGN_WARE_PCIE_CONTROLLER_LIB_H__ */
