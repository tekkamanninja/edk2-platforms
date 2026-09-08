/** @file
  Synopsys DesignWare Ethernet Quality-of-Service (EQoS) driver - EDK2 port

  Copyright (c) 2025, Spacemit Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DmaLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NetLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/TimerLib.h>
#include <Library/MemoryManagementLib.h>
#include <Protocol/EmbeddedGpio.h>
#include <Protocol/AdapterInformation.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/Cpu.h>

#include "GenphyDxeUtil.h"
#include "DwEqosControllerConfig.h"

#define DEBUG_LEVEL DEBUG_INFO

#define BITS(hi, lo)            ((~((~0) << ((hi) + 1))) & ((~0) << (lo)))
#define EQOS_MAC_DEVICE_SIZE                        SIZE_8KB

#define EQOS_MAC_CONFIGURATION                      0x0000
#define EQOS_MAC_CONFIGURATION_GPSLCE               (1U << 23)
#define EQOS_MAC_CONFIGURATION_CST                  (1U << 21)
#define EQOS_MAC_CONFIGURATION_ACS                  (1U << 20)
#define EQOS_MAC_CONFIGURATION_WD                   (1U << 19)
#define EQOS_MAC_CONFIGURATION_BE                   (1U << 18)
#define EQOS_MAC_CONFIGURATION_JD                   (1U << 17)
#define EQOS_MAC_CONFIGURATION_JE                   (1U << 16)
#define EQOS_MAC_CONFIGURATION_PS                   (1U << 15)
#define EQOS_MAC_CONFIGURATION_FES                  (1U << 14)
#define EQOS_MAC_CONFIGURATION_DM                   (1U << 13)
#define EQOS_MAC_CONFIGURATION_DCRS                 (1U << 9)
#define EQOS_MAC_CONFIGURATION_TE                   (1U << 1)
#define EQOS_MAC_CONFIGURATION_RE                   (1U << 0)
#define EQOS_MAC_EXT_CONFIGURATION                  0x0004
#define EQOS_MAC_PACKET_FILTER                      0x0008
#define EQOS_MAC_PACKET_FILTER_HPF                  (1U << 10)
#define EQOS_MAC_PACKET_FILTER_PCF_MASK             (3U << 6)
#define EQOS_MAC_PACKET_FILTER_PCF_ALL              (2U << 6)
#define EQOS_MAC_PACKET_FILTER_DBF                  (1U << 5)
#define EQOS_MAC_PACKET_FILTER_PM                   (1U << 4)
#define EQOS_MAC_PACKET_FILTER_HMC                  (1U << 2)
#define EQOS_MAC_PACKET_FILTER_HUC                  (1U << 1)
#define EQOS_MAC_PACKET_FILTER_PR                   (1U << 0)
#define EQOS_MAC_WATCHDOG_TIMEOUT                   0x000C
#define EQOS_MAC_HASH_TABLE_REG_BASE                0x0010
#define EQOS_MAC_HASH_TABLE_COUNT                   2
#define EQOS_MAC_PMT_CTRL                           0x00c0
#define EQOS_MAC_POWERDOWN                          (1U << 0)
#define EQOS_MAC_VLAN_TAG                           0x0050
#define EQOS_MAC_Q0_TX_FLOW_CTRL                    0x0070
#define EQOS_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT           16
#define EQOS_MAC_Q0_TX_FLOW_CTRL_TFE                (1U << 1)
#define EQOS_MAC_RX_FLOW_CTRL                       0x0090

#define EQOS_MAC_TX_PRTY_MAP0                       0x0098
#define EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_SHIFT          0
#define EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_MASK           (0xFFU << EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_SHIFT)

#define EQOS_MAC_RX_FLOW_CTRL_RFE                   (1U << 0)
#define EQOS_RXQ_CTRL0                              0x00A0
#define EQOS_RXQ_CTRL0_EN_MASK                      0x3
#define EQOS_RXQ_CTRL0_EN_AVB                       0x1
#define EQOS_RXQ_CTRL0_EN_DCB                       0x2
#define EQOS_RXQ_CTRL1                              0x00A4
#define EQOS_RXQ_CTRL2                              0x00A8
#define EQOS_MAC_RXQ_CTRL2_PSRQ0_SHIFT              0
#define EQOS_MAC_RXQ_CTRL2_PSRQ0_MASK               (0xFFU << EQOS_MAC_RXQ_CTRL2_PSRQ0_SHIFT)
#define EQOS_MAC_INTERRUPT_STATUS                   0x00B0
#define EQOS_MAC_INTERRUPT_ENABLE                   0x00B4
#define EQOS_MAC_RX_TX_STATUS                       0x00B8
#define EQOS_MAC_RX_TX_STATUS_RWT                   (1U << 8)
#define EQOS_MAC_RX_TX_STATUS_EXCOL                 (1U << 5)
#define EQOS_MAC_RX_TX_STATUS_LCOL                  (1U << 4)
#define EQOS_MAC_RX_TX_STATUS_EXDEF                 (1U << 3)
#define EQOS_MAC_RX_TX_STATUS_LCARR                 (1U << 2)
#define EQOS_MAC_RX_TX_STATUS_NCARR                 (1U << 1)
#define EQOS_MAC_RX_TX_STATUS_TJT                   (1U << 0)
#define EQOS_MAC_PMT_CONTROL_STATUS                 0x00C0
#define EQOS_MAC_RWK_PACKET_FILTER                  0x00C4
#define EQOS_MAC_LPI_CONTROL_STATUS                 0x00D0
#define EQOS_MAC_LPI_TIMERS_CONTROL                 0x00D4
#define EQOS_MAC_LPI_ENTRY_TIMER                    0x00D8
#define EQOS_MAC_1US_TIC_COUNTER                    0x00DC
#define EQOS_MAC_PHYIF_CONTROL_STATUS               0x00F8
#define EQOS_MAC_PHYIF_CONTROL_STATUS_LNKSTS        (1U << 19)
#define EQOS_MAC_PHYIF_CONTROL_STATUS_LNKSPEED      BITS(18, 17)
#define EQOS_MAC_PHYIF_CONTROL_STATUS_LNKSPEED_2_5  0x0
#define EQOS_MAC_PHYIF_CONTROL_STATUS_LNKSPEED_25   0x1
#define EQOS_MAC_PHYIF_CONTROL_STATUS_LNKSPEED_125  0x2
#define EQOS_MAC_PHYIF_CONTROL_STATUS_LNKMOD        (1U << 16)
#define EQOS_MAC_VERSION                            0x0110
#define EQOS_MAC_VERSION_USERVER_SHIFT              8
#define EQOS_MAC_VERSION_USERVER_MASK               (0xFFU << EQOS_MAC_VERSION_USERVER_SHIFT)
#define EQOS_MAC_VERSION_SNPSVER_MASK               0xFFU
#define EQOS_MAC_DEBUG                              0x0114
#define EQOS_MAC_HW_FEATURE_BASE                    0x011C
#define EQOS_MAC_HW_FEATURE1_TXFIFOSIZE             BITS(10,6)
#define EQOS_MAC_HW_FEATURE1_RXFIFOSIZE             BITS(4,0)
#define EQOS_MAC_HW_FEATURE1_ADDR64_SHIFT           14
#define EQOS_MAC_HW_FEATURE1_ADDR64_MASK            (0x3U << EQOS_MAC_HW_FEATURE1_ADDR64_SHIFT)
#define EQOS_MAC_HW_FEATURE1_ADDR64_32BIT           0x0U
#define EQOS_MAC_MDIO_ADDRESS                       0x0200
#define EQOS_MAC_MDIO_ADDRESS_PA_SHIFT              21
#define EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT             16
#define EQOS_MAC_MDIO_ADDRESS_CR_SHIFT              8

#define EQOS_MAC_MDIO_ADDRESS_CR_MASK               0x7U
#define EQOS_MAC_MDIO_ADDRESS_CR_60_100             0U
#define EQOS_MAC_MDIO_ADDRESS_CR_100_150            1U
#define EQOS_MAC_MDIO_ADDRESS_CR_20_35              2U
#define EQOS_MAC_MDIO_ADDRESS_CR_35_60              3U
#define EQOS_MAC_MDIO_ADDRESS_CR_150_250            4U
#define EQOS_MAC_MDIO_ADDRESS_CR_250_300            5U
#define EQOS_MAC_MDIO_ADDRESS_CR_300_500            6U
#define EQOS_MAC_MDIO_ADDRESS_CR_500_800            7U

#define EQOS_MAC_MDIO_ADDRESS_SKAP                  (1U << 4)
#define EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT             2
#define EQOS_MAC_MDIO_ADDRESS_GOC_READ              3U
#define EQOS_MAC_MDIO_ADDRESS_GOC_WRITE             1U
#define EQOS_MAC_MDIO_ADDRESS_C45E                  (1U << 1)
#define EQOS_MAC_MDIO_ADDRESS_GB                    (1U << 0)
#define EQOS_MAC_MDIO_DATA                          0x0204
#define EQOS_MAC_CSR_SW_CTRL                        0x0230
#define EQOS_MAC_ADDRESS0_HIGH                      0x0300
#define EQOS_MAC_ADDRESS0_HIGH_AE                   (1U << 31)
#define EQOS_MAC_ADDRESS0_LOW                       0x0304
#define EQOS_MMC_CONTROL                            0x0700
#define EQOS_MMC_CONTROL_UCDBC                      (1U << 8)
#define EQOS_MMC_CONTROL_CNTPRSTLVL                 (1U << 5)
#define EQOS_MMC_CONTROL_CNTPRST                    (1U << 4)
#define EQOS_MMC_CONTROL_CNTFREEZ                   (1U << 3)
#define EQOS_MMC_CONTROL_RSTONRD                    (1U << 2)
#define EQOS_MMC_CONTROL_CNTSTOPRO                  (1U << 1)
#define EQOS_MMC_CONTROL_CNTRST                     (1U << 0)
#define EQOS_MMC_RX_INTERRUPT                       0x0704
#define EQOS_MMC_RX_INTERRUPT_RXFOVPIS              (1U << 21)
#define EQOS_MMC_RX_INTERRUPT_RXLENERPIS            (1U << 18)
#define EQOS_MMC_RX_INTERRUPT_RXCRCERPIS            (1U << 5)
#define EQOS_MMC_RX_INTERRUPT_RXMCGPIS              (1U << 4)
#define EQOS_MMC_RX_INTERRUPT_RXGOCTIS              (1U << 2)
#define EQOS_MMC_RX_INTERRUPT_RXGBOCTIS             (1U << 1)
#define EQOS_MMC_RX_INTERRUPT_RXGBPKTIS             (1U << 0)
#define EQOS_MMC_TX_INTERRUPT                       0x0708
#define EQOS_MMC_TX_INTERRUPT_TXGPKTIS              (1U << 21)
#define EQOS_MMC_TX_INTERRUPT_TXGOCTIS              (1U << 20)
#define EQOS_MMC_TX_INTERRUPT_TXCARERPIS            (1U << 19)
#define EQOS_MMC_TX_INTERRUPT_TXFLOWERPIS           (1U << 13)
#define EQOS_MMC_TX_INTERRUPT_TXGBPKTIS             (1U << 1)
#define EQOS_MMC_TX_INTERRUPT_TXGBOCTIS             (1U << 0)
#define EQOS_MMC_RX_INTERRUPT_MASK                  0x070C
#define EQOS_MMC_TX_INTERRUPT_MASK                  0x0710
#define EQOS_TX_OCTET_COUNT_GOOD_BAD                0x0714
#define EQOS_TX_PACKET_COUNT_GOOD_BAD               0x0718
#define EQOS_TX_UNDERFLOW_ERROR_PACKETS             0x0748
#define EQOS_TX_CARRIER_ERROR_PACKETS               0x0760
#define EQOS_TX_OCTET_COUNT_GOOD                    0x0764
#define EQOS_TX_PACKET_COUNT_GOOD                   0x0768
#define EQOS_RX_PACKETS_COUNT_GOOD_BAD              0x0780
#define EQOS_RX_OCTET_COUNT_GOOD_BAD                0x0784
#define EQOS_RX_OCTET_COUNT_GOOD                    0x0788
#define EQOS_RX_MULTICAST_PACKETS_GOOD              0x0790
#define EQOS_RX_CRC_ERROR_PACKETS                   0x0794
#define EQOS_RX_LENGTH_ERROR_PACKETS                0x07C8
#define EQOS_RX_FIFO_OVERFLOW_PACKETS               0x07D4
#define EQOS_MMC_IPC_RX_INTERRUPT_MASK              0x0800
#define EQOS_MMC_IPC_RX_INTERRUPT                   0x0808
#define EQOS_RXIPV4_GOOD_PACKETS                    0x0810
#define EQOS_RXIPV4_HEADER_ERROR_PACKETS            0x0814
#define EQOS_RXIPV6_GOOD_PACKETS                    0x0824
#define EQOS_RXIPV6_HEADER_ERROR_PACKETS            0x0828
#define EQOS_RXUDP_ERROR_PACKETS                    0x0834
#define EQOS_RXTCP_ERROR_PACKETS                    0x083C
#define EQOS_RXICMP_ERROR_PACKETS                   0x0844
#define EQOS_RXIPV4_HEADER_ERROR_OCTETS             0x0854
#define EQOS_RXIPV6_HEADER_ERROR_OCTETS             0x0868
#define EQOS_RXUDP_ERROR_OCTETS                     0x0874
#define EQOS_RXTCP_ERROR_OCTETS                     0x087C
#define EQOS_RXICMP_ERROR_OCTETS                    0x0884
#define EQOS_MAC_TIMESTAMP_CONTROL                  0x0B00
#define EQOS_MAC_SUB_SECOND_INCREMENT               0x0B04
#define EQOS_MAC_SYSTEM_TIME_SECS                   0x0B08
#define EQOS_MAC_SYSTEM_TIME_NS                     0x0B0C
#define EQOS_MAC_SYS_TIME_SECS_UPDATE               0x0B10
#define EQOS_MAC_SYS_TIME_NS_UPDATE                 0x0B14
#define EQOS_MAC_TIMESTAMP_ADDEND                   0x0B18
#define EQOS_MAC_TIMESTAMP_STATUS                   0x0B20
#define EQOS_MAC_TX_TS_STATUS_NS                    0x0B30
#define EQOS_MAC_TX_TS_STATUS_SECS                  0x0B34
#define EQOS_MAC_AUXILIARY_CONTROL                  0x0B40
#define EQOS_MAC_AUXILIARY_TS_NS                    0x0B48
#define EQOS_MAC_AUXILIARY_TS_SECS                  0x0B4C
#define EQOS_MAC_TS_INGRESS_CORR_NS                 0x0B58
#define EQOS_MAC_TS_EGRESS_CORR_NS                  0x0B5C
#define EQOS_MAC_TS_INGRESS_LATENCY                 0x0B68
#define EQOS_MAC_TS_EGRESS_LATENCY                  0x0B6C
#define EQOS_MAC_PPS_CONTROL                        0x0B70
#define EQOS_MTL_DBG_CTL                            0x0C08
#define EQOS_MTL_DBG_STS                            0x0C0C
#define EQOS_MTL_FIFO_DEBUG_DATA                    0x0C10
#define EQOS_MTL_INTERRUPT_STATUS                   0x0C20
#define EQOS_MTL_INTERRUPT_STATUS_DBGIS             (1U << 17)
#define EQOS_MTL_INTERRUPT_STATUS_Q0IS              (1U << 0)
#define EQOS_MTL_TXQ0_OPERATION_MODE                0x0D00
#define EQOS_MTL_TXQ0_OPERATION_MODE_TQS            BITS(24,16)
#define EQOS_MTL_TXQ0_OPERATION_MODE_TTC            BITS(6,4)
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT    2
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_MASK     (0x3U << EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT)
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_EN       2U
#define EQOS_MTL_TXQ0_OPERATION_MODE_TSF            (1U << 1)
#define EQOS_MTL_TXQ0_OPERATION_MODE_FTQ            (1U << 0)
#define EQOS_MTL_TXQ0_UNDERFLOW                     0x0D04
#define EQOS_MTL_TXQ0_DEBUG                         0x0D08
#define EQOS_MTL_TXQ0_DEBUG_TRCSTS_SHIFT            1
#define EQOS_MTL_TXQ0_DEBUG_TRCSTS_MASK             0x3
#define EQOS_MTL_TXQ0_DEBUG_TXQSTS                  (1U << 4)
#define EQOS_MTL_TXQ0_WEIGHT                        0x0D18

#define EQOS_MTL_Q0_INTERRUPT_CTRL_STATUS           0x0D2C
#define EQOS_MTL_Q0_INTERRUPT_CTRL_STATUS_RXOIE     (1U << 24)
#define EQOS_MTL_Q0_INTERRUPT_CTRL_STATUS_RXOVFIS   (1U << 16)
#define EQOS_MTL_Q0_INTERRUPT_CTRL_STATUS_TXUIE     (1U << 8)
#define EQOS_MTL_Q0_INTERRUPT_CTRL_STATUS_TXUNFIS   (1U << 0)
#define EQOS_MTL_RXQ0_OPERATION_MODE                0x0D30
#define EQOS_MTL_RXQ0_OPERATION_MODE_RQS            BITS(29,20)
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFD            BITS(19,14)
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFA            BITS(13,8)
#define EQOS_MTL_RXQ0_OPERATION_MODE_EHFC           (1U << 7)
#define EQOS_MTL_RXQ0_OPERATION_MODE_RSF            (1U << 5)
#define EQOS_MTL_RXQ0_OPERATION_MODE_FEP            (1U << 4)
#define EQOS_MTL_RXQ0_OPERATION_MODE_FUP            (1U << 3)
#define EQOS_MTL_RXQ0_MISS_PKT_OVF_CNT              0x0D34
#define EQOS_MTL_RXQ0_DEBUG                         0x0D38
#define EQOS_MTL_RXQ0_DEBUG_PRXQ_SHIFT              16
#define EQOS_MTL_RXQ0_DEBUG_PRXQ_MASK               0x777f
#define EQOS_MTL_RXQ0_DEBUG_RXQSTS_SHIFT            4
#define EQOS_MTL_RXQ0_DEBUG_RXQSTS_MASK             0x3
#define EQOS_DMA_MODE                               0x1000
#define EQOS_DMA_MODE_SWR                           (1U << 0)
#define EQOS_DMA_SYSBUS_MODE                        0x1004
#define EQOS_DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT       24
#define EQOS_DMA_SYSBUS_MODE_WR_OSR_LMT_MASK        (0xfU << EQOS_DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT)
#define EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT       16
#define EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_MASK        (0xfU << EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT)
#define EQOS_DMA_SYSBUS_MODE_MB                     (1U << 14)
#define EQOS_DMA_SYSBUS_MODE_EAME                   (1U << 11)
#define EQOS_DMA_SYSBUS_MODE_BLEN16                 (1U << 3)
#define EQOS_DMA_SYSBUS_MODE_BLEN8                  (1U << 2)
#define EQOS_DMA_SYSBUS_MODE_BLEN4                  (1U << 1)
#define EQOS_DMA_SYSBUS_MODE_FB                     (1U << 0)
#define EQOS_DMA_INTERRUPT_STATUS                   0x1008
#define EQOS_DMA_DEBUG_STATUS0                      0x100C
#define EQOS_AXI_LPI_ENTRY_INTERVAL                 0x1040
#define EQOS_RWK_FILTER_BYTE_MASK_BASE              0x10C0
#define EQOS_RWK_FILTER01_CRC                       0x10D0
#define EQOS_RWK_FILTER23_CRC                       0x10D4
#define EQOS_RWK_FILTER_OFFSET                      0x10D8
#define EQOS_RWK_FILTER_COMMAND                     0x10DC
#define EQOS_DMA_CHAN0_CONTROL                      0x1100
#define EQOS_DMA_CHAN0_CONTROL_DSL_SHIFT            18
#define EQOS_DMA_CHAN0_CONTROL_DSL_MASK             (0x7U << EQOS_DMA_CHAN0_CONTROL_DSL_SHIFT)
#define EQOS_DMA_CHAN0_CONTROL_PBLX8                (1U << 16)
#define EQOS_DMA_CHAN0_TX_CONTROL                   0x1104
#define EQOS_DMA_CH0_TX_CONTROL_ST                  1

#define EQOS_DMA_CHAN0_TX_CONTROL_TXPBL_SHIFT       16
#define EQOS_DMA_CHAN0_TX_CONTROL_TXPBL_MASK        (0x3FU << EQOS_DMA_CHAN0_TX_CONTROL_TXPBL_SHIFT)
#define EQOS_DMA_CHAN0_TX_CONTROL_OSP               (1U << 4)
#define EQOS_DMA_CHAN0_TX_CONTROL_START             (1U << 0)
#define EQOS_DMA_CHAN0_RX_CONTROL                   0x1108
#define EQOS_DMA_CH0_RX_CONTROL_SR                  1
#define EQOS_DMA_CHAN0_RX_CONTROL_RXPBL_SHIFT       16
#define EQOS_DMA_CHAN0_RX_CONTROL_RXPBL_MASK        (0x3FU << EQOS_DMA_CHAN0_RX_CONTROL_RXPBL_SHIFT)
#define EQOS_DMA_CHAN0_RX_CONTROL_RBSZ_SHIFT        1
#define EQOS_DMA_CHAN0_RX_CONTROL_RBSZ_MASK         (0x3FFFU << EQOS_DMA_CHAN0_RX_CONTROL_RBSZ_SHIFT)
#define EQOS_DMA_CHAN0_RX_CONTROL_START             (1U << 0)
#define EQOS_DMA_CHAN0_TX_BASE_ADDR_HI              0x1110
#define EQOS_DMA_CHAN0_TX_BASE_ADDR                 0x1114
#define EQOS_DMA_CHAN0_RX_BASE_ADDR_HI              0x1118
#define EQOS_DMA_CHAN0_RX_BASE_ADDR                 0x111C
#define EQOS_DMA_CHAN0_TX_END_ADDR                  0x1120
#define EQOS_DMA_CHAN0_RX_END_ADDR                  0x1128
#define EQOS_DMA_CHAN0_TX_RING_LEN                  0x112C
#define EQOS_DMA_CHAN0_RX_RING_LEN                  0x1130
#define EQOS_DMA_CHAN0_INTR_ENABLE                  0x1134
#define EQOS_DMA_CHAN0_RX_WATCHDOG                  0x1138
#define EQOS_DMA_CHAN0_SLOT_CTRL_STATUS             0x113C
#define EQOS_DMA_CHAN0_CUR_TX_DESC                  0x1144
#define EQOS_DMA_CHAN0_CUR_RX_DESC                  0x114C
#define EQOS_DMA_CHAN0_CUR_TX_BUF_ADDR              0x1154
#define EQOS_DMA_CHAN0_CUR_RX_BUF_ADDR              0x115C
#define EQOS_DMA_CHAN0_STATUS                       0x1160
#define EQOS_DMA_CHAN0_STATUS_REB_DATA_TRANS        (1U << 21)
#define EQOS_DMA_CHAN0_STATUS_REB_DESC_ACC          (1U << 20)
#define EQOS_DMA_CHAN0_STATUS_REB_READ_TRANS        (1U << 19)
#define EQOS_DMA_CHAN0_STATUS_TEB_DATA_TRANS        (1U << 18)
#define EQOS_DMA_CHAN0_STATUS_TEB_DESC_ACC          (1U << 17)
#define EQOS_DMA_CHAN0_STATUS_TEB_READ_TRANS        (1U << 16)
#define EQOS_DMA_CHAN0_STATUS_NIS                   (1U << 15)
#define EQOS_DMA_CHAN0_STATUS_AIS                   (1U << 14)
#define EQOS_DMA_CHAN0_STATUS_CDE                   (1U << 13)
#define EQOS_DMA_CHAN0_STATUS_FBE                   (1U << 12)
#define EQOS_DMA_CHAN0_STATUS_RWT                   (1U << 9)
#define EQOS_DMA_CHAN0_STATUS_RPS                   (1U << 8)
#define EQOS_DMA_CHAN0_STATUS_RBU                   (1U << 7)
#define EQOS_DMA_CHAN0_STATUS_RI                    (1U << 6)
#define EQOS_DMA_CHAN0_STATUS_TBU                   (1U << 2)
#define EQOS_DMA_CHAN0_STATUS_TPS                   (1U << 1)
#define EQOS_DMA_CHAN0_STATUS_TI                    (1U << 0)

#define EQOS_MAC_HASH_TABLE_REG(n)                  (EQOS_MAC_HASH_TABLE_REG_BASE + 0x4 * (n))
#define EQOS_MAC_HW_FEATURE(n)                      (EQOS_MAC_HW_FEATURE_BASE + 0x4 * (n))
#define EQOS_RWK_FILTER_BYTE_MASK(n)                (EQOS_RWK_FILTER_BYTE_MASK_BASE + 0x4 * (n))

/* Descriptors */
#define EQOS_ALIGN(VALUE, ALIGN)                    (((VALUE) + ((ALIGN) - 1)) & ~((ALIGN) - 1))
#define EQOS_DESC_ALIGN                             64
#define EQOS_DESCRIPTORS_TX                         128
#define EQOS_DESCRIPTORS_RX                         128
#define EQOS_DESCRIPTORS_NUM                        (EQOS_DESCRIPTORS_TX + EQOS_DESCRIPTORS_RX)
#define EQOS_BUFFER_ALIGN                           EQOS_DESC_ALIGN
#define EQOS_MAX_PACKET_SIZE                        EQOS_ALIGN (1568, EQOS_DESC_ALIGN)
#define EQOS_TX_BUFFER_SIZE                         (EQOS_DESCRIPTORS_TX * EQOS_MAX_PACKET_SIZE)
#define EQOS_RX_BUFFER_SIZE                         (EQOS_DESCRIPTORS_RX * EQOS_MAX_PACKET_SIZE)

typedef struct {
  UINT32    Tdes0;
  UINT32    Tdes1;
  UINT32    Tdes2;
  #define EQOS_TDES2_TX_IOC  (1U << 31)                       /* TX */
  #define EQOS_TDES2_RX_DAF  (1U << 17)                       /* RX (WB) */
  #define EQOS_TDES2_RX_SAF  (1U << 16)                       /* RX (WB) */
  UINT32    Tdes3;
  #define EQOS_TDES3_TX_OWN          (1U << 31)               /* TX */
  #define EQOS_TDES3_TX_FD           (1U << 29)               /* TX */
  #define EQOS_TDES3_TX_LD           (1U << 28)               /* TX */
  #define EQOS_TDES3_TX_DE           (1U << 23)               /* TX (WB) */
  #define EQOS_TDES3_TX_EUE          (1U << 16)               /* TX (WB) */
  #define EQOS_TDES3_TX_ES           (1U << 15)               /* TX (WB) */
  #define EQOS_TDES3_TX_JT           (1U << 14)               /* TX (WB) */
  #define EQOS_TDES3_TX_PF           (1U << 13)               /* TX (WB) */
  #define EQOS_TDES3_TX_PCE          (1U << 12)               /* TX (WB) */
  #define EQOS_TDES3_TX_LOC          (1U << 11)               /* TX (WB) */
  #define EQOS_TDES3_TX_NC           (1U << 10)               /* TX (WB) */
  #define EQOS_TDES3_TX_LC           (1U << 9)                /* TX (WB) */
  #define EQOS_TDES3_TX_EC           (1U << 8)                /* TX (WB) */
  #define EQOS_TDES3_TX_ED           (1U << 3)                /* TX (WB) */
  #define EQOS_TDES3_TX_UF           (1U << 2)                /* TX (WB) */
  #define EQOS_TDES3_TX_IHE          (1U << 0)                /* TX (WB) */
  #define EQOS_TDES3_RX_OWN          (1U << 31)               /* RX */
  #define EQOS_TDES3_RX_IOC          (1U << 30)               /* RX */
  #define EQOS_TDES3_RX_BUF1V        (1U << 24)               /* RX */
  #define EQOS_TDES3_RX_CTXT         (1U << 30)               /* RX (WB) */
  #define EQOS_TDES3_RX_FD           (1U << 29)               /* RX (WB) */
  #define EQOS_TDES3_RX_LD           (1U << 28)               /* RX (WB) */
  #define EQOS_TDES3_RX_CE           (1U << 24)               /* RX (WB) */
  #define EQOS_TDES3_RX_GP           (1U << 23)               /* RX (WB) */
  #define EQOS_TDES3_RX_RWT          (1U << 22)               /* RX (WB) */
  #define EQOS_TDES3_RX_OE           (1U << 21)               /* RX (WB) */
  #define EQOS_TDES3_RX_RE           (1U << 20)               /* RX (WB) */
  #define EQOS_TDES3_RX_DE           (1U << 19)               /* RX (WB) */
  #define EQOS_TDES3_RX_ES           (1U << 15)               /* RX (WB) */
  #define EQOS_TDES3_RX_LENGTH_MASK  0x7FFFU                  /* RX */
} EQOS_DESC;

typedef enum {
  EqosAxiBusWidth128 = 16,
  EqosAxiBusWidth64  = 8,
  EqosAxiBusWidth32  = 4,
} DWC_EQOS_AXI_BUS_WIDTH;

typedef enum {
  EqosAxiBlen256 = BIT7,
  EqosAxiBlen128 = BIT6,
  EqosAxiBlen64  = BIT5,
  EqosAxiBlen32  = BIT4,
  EqosAxiBlen16  = BIT3,
  EqosAxiBlen8   = BIT2,
  EqosAxiBlen4   = BIT1,
} DWC_EQOS_AXI_BLEN;

typedef struct _EQOS_DEVICE EQOS_DEVICE;

typedef struct {
  EFI_STATUS (EFIAPI *PlatInit)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *PlatDeinit)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *EnableClocks)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *DisableClocks)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *AssertReset)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *DeassertReset)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *CalibratePads)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *DisableCalibration)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *SetTxClkSpeed)(IN EQOS_DEVICE *Eqos);
  EFI_STATUS (EFIAPI *GetPermanentMac)(IN EQOS_DEVICE *Eqos);
} EQOS_PLATFORM_OPS;

typedef struct {
  UINT32                 CsrClockRate;
  INT32                  MdioWait;
  INT32                  SwrWait;
  INT32                  ConfigMac;
  INT32                  ConfigMacMdio;
  DWC_EQOS_AXI_BUS_WIDTH AxiBusWidth;
  BOOLEAN                RegAccessAlwaysOk;
  EFI_STATUS (EFIAPI *GetInterface)(IN EQOS_DEVICE *Eqos);
  EQOS_PLATFORM_OPS     *PlatOps;
} DWC_EQOS_CONFIG;

struct _EQOS_DEVICE {
  UINT32                               Signature;
  EFI_HANDLE                           ControllerHandle;

  EFI_LOCK                             Lock;
  EFI_EVENT                            ExitBootServicesEvent;

  EFI_SIMPLE_NETWORK_PROTOCOL          Snp;
  EFI_SIMPLE_NETWORK_MODE              SnpMode;

  EFI_CPU_ARCH_PROTOCOL                *Cpu;

  //DWC_EQOS_PLATFORM_DEVICE_PROTOCOL    *Platform;
  DWC_EQOS_CONFIG                      *Config;
  MII_DEV                              *Mdio;

  PHY_DEVICE                           *PhyDev;

  UINT32                               DescSize;
  UINTN                                DescPages;

  VOID                                 *Descs;
  VOID                                 *RxBuffer;
  VOID                                 *TxBuffer;

  UINT32                               TxDescIdx;
  UINT32                               RxDescIdx;
  BOOLEAN                              Started;
  BOOLEAN                              RegAccessOk;


  UINT32                               HwFeatures[4];

  EFI_PHYSICAL_ADDRESS                 Base;
  VOID                                 *Plat;

  CONST DW_EQOS_CONTROLLER_CONFIG_DATA *ControllerConfig;

  PHY_INTERFACE                        PhyInterface;
};

// Payload (1500B) + Ethernet Header (14B) + Optional Double VLAN Tag (8B)
#define MAX_ETHERNET_PACKET_SIZE    1522

// Minimum Ethernet frame size (64B) - FCS (4B) = 60B
// Includes Ethernet Header (14B) + Minimum Payload (46B)
#define MIN_ETHERNET_PACKET_SIZE    60

typedef struct {
  MAC_ADDR_DEVICE_PATH                   MacAddrDP;
  EFI_DEVICE_PATH_PROTOCOL               End;
} SIMPLE_NETWORK_DEVICE_PATH;

extern EFI_COMPONENT_NAME_PROTOCOL   gEqosComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gEqosComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL   gEqosDriverBinding;

extern CONST EFI_SIMPLE_NETWORK_PROTOCOL       gEqosSnpTemplate;
extern CONST EFI_ADAPTER_INFORMATION_PROTOCOL  gEqosAipTemplate;

#define EQOS_DRIVER_SIGNATURE  SIGNATURE_32 ('E', 'Q', 'o', 'S')
#define EQOS_PRIVATE_DATA_FROM_SNP_THIS(a)  CR (a, EQOS_DEVICE, Snp, EQOS_DRIVER_SIGNATURE)

EFI_STATUS
EFIAPI
EqosGetInterface (
  IN EQOS_DEVICE  *Eqos
  );

EFI_STATUS
EqosNullOps(
  IN EQOS_DEVICE  *Eqos
  );

extern DWC_EQOS_CONFIG K3EqosConfig;

/*---------------------------------------------------------------------------------------------------------------------

  UEFI-Compliant functions for EFI_SIMPLE_NETWORK_PROTOCOL

  Refer to the Simple Network Protocol section (24.1) in the UEFI 2.8 Specification for related definitions

---------------------------------------------------------------------------------------------------------------------*/

/**
  PrintPacket - Utility function to print packet data.

  @param Title    Title or label for the packet.
  @param Packet   Pointer to the packet data.
  @param Length   Length of the packet data.
*/
VOID
PrintPacket (
  CONST CHAR8   *Title,
  CONST UINT8  *Packet,
  UINT32        Length
);

EFI_STATUS
EFIAPI
EqosInit (
  IN EQOS_DEVICE *Eqos
);

EFI_STATUS
EFIAPI
EqosDeinit (
  IN EQOS_DEVICE *Eqos
);

EFI_STATUS
EFIAPI
EqosStart (
  IN EQOS_DEVICE  *Eqos
  );

EFI_STATUS
EqosStop (
  IN EQOS_DEVICE  *Eqos
  );

EFI_STATUS
EFIAPI
EqosSend (
  IN EQOS_DEVICE  *Eqos,
  IN VOID         *Buffer,
  IN UINT32        Length
  );

EFI_STATUS
EqosRecv (
  IN  EQOS_DEVICE  *Eqos,
  OUT VOID         *Buffer,
  OUT UINT32       *Length
  );

EFI_STATUS
EFIAPI
EqosUpdateLink (
  IN EQOS_DEVICE  *Eqos
  );

VOID
EqosGetMacAddress (
  IN  EQOS_DEVICE           *Eqos,
  OUT EFI_MAC_ADDRESS       *MacAddress
  );

VOID
EqosSetMacAddress (
  IN  EQOS_DEVICE          *Eqos,
  IN EFI_MAC_ADDRESS       *MacAddress
  );

VOID
EqosGetDmaInterruptStatus (
  IN  EQOS_DEVICE        *Eqos,
  OUT UINT32             *InterruptStatus  OPTIONAL
  );

EFI_STATUS
EqosSetRxFilters (
  IN EQOS_DEVICE        *Eqos,
  IN UINT32             ReceiveFilterSetting,
  IN BOOLEAN            ResetMCastFilter,
  IN UINTN              MCastFilterCnt        OPTIONAL,
  IN EFI_MAC_ADDRESS    *MCastFilter          OPTIONAL
  );
