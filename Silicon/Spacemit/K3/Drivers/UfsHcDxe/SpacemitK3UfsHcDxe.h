/** @file
  SPACEMIT K3 UFS Host Controller DXE Driver Header

  Copyright (C) 2025 SPACEMIT Corporation
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SPACEMIT_K3_UFS_HC_DXE_H_
#define _SPACEMIT_K3_UFS_HC_DXE_H_

#include <IndustryStandard/UfsHci.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/ClockCtrl.h>
#include <Protocol/Cpu.h>
#include <Protocol/DevicePath.h>
#include <Protocol/UfsHostController.h>
#include <Protocol/UfsHostControllerPlatform.h>
#include <Uefi.h>

#define UFS_ACLK_DEFAULT_HZ  409600000ULL

//
// UFS HOST PHY REGISTER (offsets from UFS HC base)
//
#define UFS_ARASAN_TOP_BASE      0x1C00
#define UFS_ARASAN_PHY_MNG_BASE  0x1B00

#define UFS_MPHY_RST_CTRL   0x0
#define UFS_MPHY_PU_CTRL    0x4
#define UFS_MPHY_BKDR_CTRL  0x8
#define UFS_DEVICE_IO_CTRL  0xc

// UFS_MPHY_PU_CTRL bit definitions
#define UFS_MPHY_PU_PLL_LOCK  BIT31

#define MPHY_PU_ALL                 0x87F
#define MPHY_PU_WITH_HB8_RESET      0xB7F
#define MPHY_DEVICE_RESET_DEASSERT  0x101
#define MPHY_DEVICE_RESET_ASSERT    0x001

#define MPHY_PLL_LOCK_TIMEOUT_US  10000

//
// UFS HOST LOGIC REGISTER (offsets from UFS HC base)
//
#define UFS_SYS1CLK_1US_REG            0xC0
#define UFS_TX_SYMBOL_CLK_NS_US_REG    0xC4
#define UFS_LOCAL_PORT_ID_REG          0xC8
#define UFS_PA_ERR_CODE_REG            0xCC
#define UFS_RETRY_TIMER_REG            0xD0
#define UFS_PA_LINK_STARTUP_TIMER_REG  0xD8
#define UFS_CFG1_REG                   0xDC

#define SPACEMIT_K3_UFS_HC_MMIO_SIZE  0x40000

//
// UFS controller timing constants for ~409.6MHz SYS1CLK
//
#define UFS_SYS1CLK_1US_409MHZ          410
#define UFS_TX_SYMBOL_CLK_NS_US_409MHZ  0x800
#define UFS_PA_LINK_STARTUP_TIMER_MAX   0xffffffff
#define UFS_DL_AFC0REQTIMEOUTVAL_MAX    0xFFFF

#define UFS_MPHY_TX_GEAR_SWITCH_DELAY_US  20
#define UFS_MPHY_TUNING_SETTLE_US         5000
#define UFS_K3_POST_POWER_MODE_DELAY_US   10000
#define UFS_K3_POST_PLL_LOCK_SETTLE_US    5000
#define UFS_K3_SILENT_RESET_DELAY_US      20

#define UFS_SPACEMIT_K3_LIMIT_NUM_LANES_RX  2
#define UFS_SPACEMIT_K3_LIMIT_NUM_LANES_TX  2
#define UFS_SPACEMIT_K3_LIMIT_HSGEAR_RX     3
#define UFS_SPACEMIT_K3_LIMIT_HSGEAR_TX     3

//
// Standard UFS HCI Registers - use EDK2 standard definitions
// These are aliases for compatibility with existing code paths
//
#define REG_CONTROLLER_CAPABILITIES           UFS_HC_CAP_OFFSET       // 0x00
#define REG_UFS_VERSION                       UFS_HC_VER_OFFSET       // 0x08
#define REG_CONTROLLER_DEV_ID                 UFS_HC_DDID_OFFSET      // 0x10
#define REG_CONTROLLER_PROD_ID                UFS_HC_PMID_OFFSET      // 0x14
#define REG_INTERRUPT_STATUS                  UFS_HC_IS_OFFSET        // 0x20
#define REG_INTERRUPT_ENABLE                  UFS_HC_IE_OFFSET        // 0x24
#define REG_CONTROLLER_STATUS                 UFS_HC_STATUS_OFFSET    // 0x30
#define REG_CONTROLLER_ENABLE                 UFS_HC_ENABLE_OFFSET    // 0x34
#define REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER  UFS_HC_UECPA_OFFSET     // 0x38
#define REG_AUTO_HIBERNATE_IDLE_TIMER         UFS_HC_AHIT_OFFSET      // 0x18
#define REG_UTP_TRANSFER_REQ_DOOR_BELL        UFS_HC_UTRLDBR_OFFSET   // 0x58
#define REG_UIC_COMMAND                       UFS_HC_UIC_CMD_OFFSET   // 0x90
#define REG_UIC_COMMAND_ARG_1                 UFS_HC_UCMD_ARG1_OFFSET // 0x94
#define REG_UIC_COMMAND_ARG_2                 UFS_HC_UCMD_ARG2_OFFSET // 0x98
#define REG_UIC_COMMAND_ARG_3                 UFS_HC_UCMD_ARG3_OFFSET // 0x9C

//
// Controller Status bits - use EDK2 standard definitions
//
#define DEVICE_PRESENT     UFS_HC_HCS_DP    // BIT0
#define UIC_COMMAND_READY  UFS_HC_HCS_UCRDY // BIT3

//
// Interrupt Status bits - use EDK2 standard definitions
//
#define UIC_LINK_STARTUP     UFS_HC_IS_ULSS // BIT8
#define UIC_ERROR            BIT2           // UIC Error Status (Ue)
#define UIC_POWER_MODE       BIT4
#define UIC_HIBERNATE_EXIT   BIT5
#define UIC_HIBERNATE_ENTER  BIT6
#define UIC_COMMAND_COMPL    UFS_HC_IS_UCCS // BIT10
#define DEVICE_FATAL_ERROR   BIT11          // Device Fatal Error Status (Dfes)
#define CONTROLLER_FATAL_ERROR                                                 \
  BIT16                               // Host Controller Fatal Error Status (Hcfes)
#define SYSTEM_BUS_FATAL_ERROR  BIT17 // System Bus Fatal Error Status (Sbfes)

// Controller Enable bits
#define CONTROLLER_ENABLE   UFS_HC_HCE_EN// BIT0
#define CONTROLLER_DISABLE  0x0

// Power mode change status (UPMCRS field in HCS)
#define UIC_POWER_MODE_CHANGE_REQ_STATUS_MASK  (0x7 << 8)

#define PWR_OK         0x0
#define PWR_LOCAL      0x1
#define PWR_REMOTE     0x2
#define PWR_BUSY       0x3
#define PWR_ERROR_CAP  0x4

//
// PA Layer Gettable and settable M-PHY Specific Attributes
//
#define PA_TXHSG1SYNCLENGTH      0x1552
#define PA_TXHSG1PREPARELENGTH   0x1553
#define PA_TXHSG2SYNCLENGTH      0x1554
#define PA_TXHSG2PREPARELENGTH   0x1555
#define PA_TXHSG3SYNCLENGTH      0x1556
#define PA_TXHSG3PREPARELENGTH   0x1557
#define PA_TXMK2EXTENSION        0x155A
#define PA_PEERSCRAMBLING        0x155B
#define PA_TXSKIP                0x155C
#define PA_TXSKIPPERIOD          0x155D
#define PA_PEER_TX_LCC_ENABLE    0x155F
#define PA_ACTIVETXDATALANES     0x1560
#define PA_CONNECTEDTXDATALANES  0x1561
#define PA_TXGEAR                0x1568
#define PA_TXTERMINATION         0x1569
#define PA_HSSERIES              0x156A
#define PA_PWRMODE               0x1571
#define PA_ACTIVERXDATALANES     0x1580
#define PA_CONNECTEDRXDATALANES  0x1581
#define PA_RXGEAR                0x1583
#define PA_RXTERMINATION         0x1584
#define PA_MAXRXPWMGEAR          0x1586
#define PA_MAXRXHSGEAR           0x1587

#define PA_SCRAMBLING             0x1585
#define PA_MK2EXTENSIONGUARDBAND  0x15AB
#define PA_GRANULARITY            0x15AA
#define PA_TACTIVATE              0x15A8
#define PA_TXTRAILINGCLOCKS       0x1564
#define PA_STALLNOCONFIGTIME      0x15A3
#define PA_HIBERN8TIME            0x15A7
#define PA_LOCAL_TX_LCC_ENABLE    0x155E

//
// Special TX/RX Configuration Attributes
//
#define RX_LS_PRE_LEN_CAP          0x008D
#define RX_LANE_HB8_BKDOOR_ATTR    0x00F4
#define RX_PWRM_CLOSURE_LEN_CAP    0x008E
#define RX_MIN_STALL_CAP           0x0088
#define RX_LANE_SOF_BKDOOR_ATT     0x00F2
#define RX_LS_PREPARELEN_TIME      0x008D
#define RX_GARBAGE_COUNT_OFFSET    0x00F2
#define VS_TX_BURST_CLOSURE_DELAY  0xD084

//
// Data Link Layer Attributes (for HS power mode tuning)
//
#define DL_FC0PROTTIMEOUTVAL    0x2041
#define DL_TC0REPLAYTIMEOUTVAL  0x2042
#define DL_AFC0REQTIMEOUTVAL    0x2043
#define DL_AFC0CREDITTHRESHOLD  0x2044
#define DL_TC0OUTACKTHRESHOLD   0x2045
#define DL_TC1REPLAYTIMEOUTVAL  0x2046
#define DL_AFC1REQTIMEOUTVAL    0x2047

#define TX_HIBERN8TIME_CAPABILITY  0x000F
#define RX_HIBERN8TIME_CAPABILITY  0x0092
#define TX_LCC_ENABLE              0x002D
#define TX_MIN_ACTIVATETIME        0x0033

//
// Special analog reg
//
#define ANA_EQ_CTRL_REG_ATTR  0x00CD
#define ANA_HSGEAR_CTRL_ATTR  0x00C1

//
// UIC Command codes
//
#define UIC_CMD_DME_GET           0x01
#define UIC_CMD_DME_SET           0x02
#define UIC_CMD_DME_PEER_GET      0x03
#define UIC_CMD_DME_PEER_SET      0x04
#define UIC_CMD_DME_POWERON       0x10
#define UIC_CMD_DME_POWEROFF      0x11
#define UIC_CMD_DME_ENABLE        0x12
#define UIC_CMD_DME_RESET         0x14
#define UIC_CMD_DME_END_PT_RST    0x15
#define UIC_CMD_DME_LINK_STARTUP  0x16
#define UIC_CMD_DME_HIBER_ENTER   0x17
#define UIC_CMD_DME_HIBER_EXIT    0x18
#define UIC_CMD_DME_TEST_MODE     0x1A

// UIC Command opcodes
#define COMMAND_OPCODE_MASK  0xFF

// Interrupt masks (using definitions from above)
#define UFSHCD_UIC_MASK  UIC_COMMAND_COMPL
#define UFSHCD_UIC_PWR_MASK                                                    \
  (UIC_HIBERNATE_ENTER | UIC_HIBERNATE_EXIT | UIC_POWER_MODE)
#define UFSHCD_ERROR_MASK                                                      \
  (UIC_ERROR | DEVICE_FATAL_ERROR | CONTROLLER_FATAL_ERROR |                   \
   SYSTEM_BUS_FATAL_ERROR)

// UIC command timeout
#define UFS_UIC_CMD_TIMEOUT_MS   1000
#define UFS_UIC_COMMAND_RETRIES  3

//
// UIC ARG macros
//
#define UIC_ARG_MIB_SEL(attr, sel)                                             \
  ((((attr) & 0xFFFF) << 16) | ((sel) & 0xFFFF))
#define UIC_ARG_MIB(attr)  UIC_ARG_MIB_SEL(attr, 0)

// GenSelectorIndex calculation macros for M-PHY attributes
// JESD220 uses 4 as GenSelector base for RX lane attributes.
#define PA_MAXDATALANES  4
#define UIC_ARG_MPHY_TX_GEN_SEL_INDEX(lane)  (lane)
#define UIC_ARG_MPHY_RX_GEN_SEL_INDEX(lane)  (PA_MAXDATALANES + (lane))

//
// PA power modes and HS series
//
#define FAST_MODE      1
#define SLOW_MODE      2
#define FASTAUTO_MODE  4
#define SLOWAUTO_MODE  5

#define PA_HS_MODE_A  1
#define PA_HS_MODE_B  2

typedef struct {
  UINT8    GearRx;
  UINT8    GearTx;
  UINT8    LaneRx;
  UINT8    LaneTx;
  UINT8    PwrRx;
  UINT8    PwrTx;
  UINT8    HsRate;
} UFS_PA_LAYER_ATTR;

//
// Notify status
//
typedef enum {
  PRE_CHANGE  = 0,
  POST_CHANGE = 1,
} UFS_NOTIFY_CHANGE_STATUS;

//
// Private data structure
//
typedef struct {
  UINT32    PhyMngBase; // UFS_ARASAN_PHY_MNG_BASE
  UINT32    AtopBase;   // UFS_ARASAN_TOP_BASE
} SPACEMIT_K3_UFS_PRIV;

//
// Driver private data
//
#define SPACEMIT_K3_UFS_HC_SIGNATURE  SIGNATURE_32('S', 'K', 'U', 'F')

typedef struct {
  UINT32                                Signature;
  EDKII_UFS_HOST_CONTROLLER_PROTOCOL    UfsHc;
  EDKII_UFS_HC_PLATFORM_PROTOCOL        UfsHcPlatform; // Platform Protocol
  EFI_HANDLE                            ControllerHandle;
  UINTN                                 UfsHcBase;
  SPACEMIT_K3_UFS_PRIV                  Priv;
  BOOLEAN                               Initialized;
  BOOLEAN                               FirstHceDone;
  UINT32                                ConnectedTxLanes;
  EFI_CPU_ARCH_PROTOCOL                 *Cpu; // CPU Protocol for cache flush
  SILICON_CLOCKCTRL_PROTOCOL            *ClockCtrl;
  LIST_ENTRY                            DmaAllocList;
  EFI_DEVICE_PATH_PROTOCOL              *DevicePath;
} SPACEMIT_K3_UFS_HC_PRIVATE_DATA;

#define SPACEMIT_K3_UFS_HC_FROM_THIS(a)                                        \
  CR(a, SPACEMIT_K3_UFS_HC_PRIVATE_DATA, UfsHc, SPACEMIT_K3_UFS_HC_SIGNATURE)

//
// Function prototypes
//
EFI_STATUS
SpacemitK3UfsClkEnable (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  );

EFI_STATUS
SpacemitK3UfsClkDisable (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  );

EFI_STATUS
SpacemitK3ArasanUfsMphyInit (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  );

EFI_STATUS
SpacemitK3ArasanUfsUniproInit (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  );

EFI_STATUS
SpacemitK3ArasanUfsSilentReset (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  );

EFI_STATUS
SpacemitK3ArasanUfsLinkStartupNotify (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UFS_NOTIFY_CHANGE_STATUS         Status
  );

EFI_STATUS
SpacemitK3ArasanUfsHceEnableNotify (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UFS_NOTIFY_CHANGE_STATUS         Status
  );

EFI_STATUS
SpacemitK3ArasanUfsInit (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  );

//
// UIC command helpers
//
EFI_STATUS
SpacemitK3UfsDmeSet (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           UicArg1,
  IN UINT32                           Value
  );

EFI_STATUS
SpacemitK3UfsDmeGet (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           UicArg1,
  OUT UINT32                          *Value
  );

//
// Register access helpers (using HC base)
//
static inline UINT32
SpacemitK3UfsReadReg32 (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           Offset
  )
{
  return MmioRead32 (Private->UfsHcBase + Offset);
}

static inline VOID
SpacemitK3UfsWriteReg32 (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           Value,
  IN UINT32                           Offset
  )
{
  MmioWrite32 (Private->UfsHcBase + Offset, Value);
}

#endif // _SPACEMIT_K3_UFS_HC_DXE_H_
