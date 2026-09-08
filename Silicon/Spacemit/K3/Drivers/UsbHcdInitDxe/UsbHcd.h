/** @file

  Copyright 2017, 2020 NXP
  Copyright 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef USB_HCD_H_
#define USB_HCD_H_

#include <Base.h>

/// DWC3 XHCI

/* Global constants */
#define DWC3_GSNPSID_MASK    0xffff0000
#define DWC3_SYNOPSYS_ID     0x55330000
#define DWC3_RELEASE_MASK    0xffff
#define DWC3_REG_OFFSET      0xC100
#define DWC3_RELEASE_190a    0x190a
#define DWC3_RELEASE_330a    0x330a
#define DWC3_XHCI_REGS_SIZE  0x8000

/* Global Configuration Register */
#define DWC3_GCTL_U2RSTECN  BIT16
#define DWC3_GCTL_PRTCAPDIR(N)  ((N) << 12)
#define DWC3_GCTL_PRTCAP_HOST    1
#define DWC3_GCTL_PRTCAP_OTG     3
#define DWC3_GCTL_CORESOFTRESET  BIT11
#define DWC3_GCTL_SCALEDOWN(N)  ((N) << 4)
#define DWC3_GCTL_SCALEDOWN_MASK  DWC3_GCTL_SCALEDOWN(3)
#define DWC3_GCTL_DISSCRAMBLE     BIT3
#define DWC3_GCTL_DSBLCLKGTNG     BIT0

/* Global HWPARAMS1 Register */
#define DWC3_GHWPARAMS1_EN_PWROPT(N)  (((N) & (3 << 24)) >> 24)
#define DWC3_GHWPARAMS1_EN_PWROPT_CLK  1

/* Global UCTL1 Register */
#define DWC3_GUCTL1_DEV_DECOUPLE_L1L2_EVT        BIT31
#define DWC3_GUCTL1_TX_IPGAP_LINECHECK_DIS       BIT28
#define DWC3_GUCTL1_DEV_FORCE_20_CLK_FOR_30_CLK  BIT26
#define DWC3_GUCTL1_DEV_L1_EXIT_BY_HW            BIT24
#define DWC3_GUCTL1_PARKMODE_DISABLE_SS          BIT17
#define DWC3_GUCTL1_PARKMODE_DISABLE_HS          BIT16
#define DWC3_GUCTL2_RST_ACTBITLATER              BIT14
#define DWC3_GUCTL1_RESUME_OPMODE_HS_HOST        BIT10

/* Global USB2 PHY Configuration Register */
#define DWC3_GUSB2PHYCFG_PHYSOFTRST         BIT31
#define DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS  BIT30
#define DWC3_GUSB2PHYCFG_USBTRDTIM(N)  (((N) & 0xf) << 10)
#define DWC3_GUSB2PHYCFG_USBTRDTIM_MASK  DWC3_GUSB2PHYCFG_USBTRDTIM(0xf)
#define DWC3_GUSB2PHYCFG_SUSPHY          BIT6
#define DWC3_GUSB2PHYCFG_ENBLSLPM        BIT0
#define DWC3_GUSB2PHYCFG_PHYIF(n)  ((n) << 3)
#define DWC3_GUSB2PHYCFG_PHYIF_MASK      DWC3_GUSB2PHYCFG_PHYIF(1)
#define DWC3_GUSB2PHYCFG_USBTRDTIM_MASK  DWC3_GUSB2PHYCFG_USBTRDTIM(0xf)
#define DWC3_GUSB2PHYCFG_TOUTCAL_MASK    0x7
#define USBTRDTIM_UTMI_8_BIT             9
#define USBTRDTIM_UTMI_16_BIT            5
#define UTMI_PHYIF_16_BIT                1
#define UTMI_PHYIF_8_BIT                 0

/* Global USB3 PIPE Control Register */
#define DWC3_GUSB3PIPECTL_PHYSOFTRST    BIT31
#define DWC3_GUSB3PIPECTL_U2SSINP3OK    BIT29
#define DWC3_GUSB3PIPECTL_DISRXDETINP3  BIT28
#define DWC3_GUSB3PIPECTL_UX_EXIT_PX    BIT27
#define DWC3_GUSB3PIPECTL_REQP1P2P3     BIT24
#define DWC3_GUSB3PIPECTL_DEP1P2P3(n)  ((n) << 19)
#define DWC3_GUSB3PIPECTL_DEP1P2P3_MASK  DWC3_GUSB3PIPECTL_DEP1P2P3(7)
#define DWC3_GUSB3PIPECTL_DEP1P2P3_EN    DWC3_GUSB3PIPECTL_DEP1P2P3(1)
#define DWC3_GUSB3PIPECTL_DEPOCHANGE     BIT18
#define DWC3_GUSB3PIPECTL_SUSPHY         BIT17
#define DWC3_GUSB3PIPECTL_LFPSFILT       BIT9
#define DWC3_GUSB3PIPECTL_RX_DETOPOLL    BIT8
#define DWC3_GUSB3PIPECTL_TX_DEEPH_MASK  DWC3_GUSB3PIPECTL_TX_DEEPH(3)
#define DWC3_GUSB3PIPECTL_TX_DEEPH(n)  ((n) << 1)

/* Global Frame Length Adjustment Register */
#define GFLADJ_30MHZ_REG_SEL      BIT7
#define USB3_NEED_GFLADJ_SETTING  0
#define GFLADJ_30MHZ(N)  ((N) & 0x3f)
#define GFLADJ_30MHZ_DEFAULT  0x20

/* Default to the FSL XHCI defines */
#define USB3_NEED_BURST_SETTING      0
#define USB3_ENABLE_BEAT_BURST       0xF
#define USB3_ENABLE_BEAT_BURST_MASK  0xFF
#define USB3_SET_BEAT_BURST_LIMIT    0xF00

/* DCFG Register */
#define DCFG_SPEED_MASK     (BIT2|BIT1|BIT0)
#define DCFG_SPEED_HS       0
#define DCFG_SPEED_FS       1
#define DCFG_SPEED_LS       2
#define DCFG_SPEED_SS       4
#define DCFG_SPEED_SS_PLUS  5

typedef struct {
  UINT32    GEvntAdrLo;
  UINT32    GEvntAdrHi;
  UINT32    GEvntSiz;
  UINT32    GEvntCount;
} G_EVENT_BUFFER;

typedef struct {
  UINT32    DDepCmdPar2;
  UINT32    DDepCmdPar1;
  UINT32    DDepCmdPar0;
  UINT32    DDepCmd;
} D_PHYSICAL_EP;

typedef struct {
  UINT32            GSBusCfg0;        // Offset: 0xC100
  UINT32            GSBusCfg1;        // Offset: 0xC104
  UINT32            GTxThrCfg;        // Offset: 0xC108
  UINT32            GRxThrCfg;        // Offset: 0xC10C
  UINT32            GCtl;             // Offset: 0xC110
  UINT32            GEvt;             // Offset: 0xC114
  UINT32            GSts;             // Offset: 0xC118
  UINT32            GUctl1;           // Offset: 0xC11C
  UINT32            GSnpsId;          // Offset: 0xC120
  UINT32            GGpio;            // Offset: 0xC124
  UINT32            GUid;             // Offset: 0xC128
  UINT32            GUctl;            // Offset: 0xC12C
  UINT64            GBusErrAddr;      // Offset: 0xC130
  UINT64            GPrtbImap;        // Offset: 0xC138
  UINT32            GHwParams0;       // Offset: 0xC140
  UINT32            GHwParams1;       // Offset: 0xC144
  UINT32            GHwParams2;       // Offset: 0xC148
  UINT32            GHwParams3;       // Offset: 0xC14C
  UINT32            GHwParams4;       // Offset: 0xC150
  UINT32            GHwParams5;       // Offset: 0xC154
  UINT32            GHwParams6;       // Offset: 0xC158
  UINT32            GHwParams7;       // Offset: 0xC15C
  UINT32            GDbgFifoSpace;    // Offset: 0xC160
  UINT32            GDbgLtssm;        // Offset: 0xC164
  UINT32            GDbgLnmcc;        // Offset: 0xC168
  UINT32            GDbgBmu;          // Offset: 0xC16C
  UINT32            GDbgLspMux;       // Offset: 0xC170
  UINT32            GDbgLsp;          // Offset: 0xC174
  UINT32            GDbgEpInfo0;      // Offset: 0xC178
  UINT32            GDbgEpInfo1;      // Offset: 0xC17C
  UINT64            GPrtbImapHs;      // Offset: 0xC180
  UINT64            GPrtbImapFs;      // Offset: 0xC188
  UINT32            Res2[3];          // Offset: 0xC190
  UINT32            GUctl2;           // Offset: 0xC19C
  UINT32            Res3[24];         // Offset: 0xC1A0
  UINT32            GUsb2PhyCfg[16];  // Offset: 0xC200
  UINT32            GUsb2I2cCtl[16];  // Offset: 0xC240
  UINT32            GUsb2PhyAcc[16];  // Offset: 0xC280
  UINT32            GUsb3PipeCtl[16]; // Offset: 0xC2C0
  UINT32            GTxFifoSiz[32];   // Offset: 0xC300
  UINT32            GRxFifoSiz[32];   // Offset: 0xC380
  G_EVENT_BUFFER    GEvntBuf[32];     // Offset: 0xC400
  UINT32            GHwParams8;       // Offset: 0xC480
  UINT32            Res4[11];         // Offset: 0xC484
  UINT32            GFLAdj;           // Offset: 0xC4B0
  UINT32            Res5[51];         // Offset: 0xC4B4
  UINT32            DCfg;             // Offset: 0xC580
  UINT32            DCtl;             // Offset: 0xC584
  UINT32            DEvten;           // Offset: 0xC588
  UINT32            DSts;             // Offset: 0xC58C
  UINT32            DGCmdPar;         // Offset: 0xC590
  UINT32            DGCmd;            // Offset: 0xC594
  UINT32            Res6[2];          // Offset: 0xC598
  UINT32            DAlepena;         // Offset: 0xC5A0
  UINT32            Res7[55];         // Offset: 0xC5A4
  D_PHYSICAL_EP     DPhyEpCmd[32];    // Offset: 0xC680
  UINT32            Res8[128];        // Offset: 0xC700
  UINT32            OCfg;             // Offset: 0xC900
  UINT32            OCtl;             // Offset: 0xC904
  UINT32            OEvt;             // Offset: 0xC908
  UINT32            OEvtEn;           // Offset: 0xC90C
  UINT32            OSts;             // Offset: 0xC910
  UINT32            Res9[3];          // Offset: 0xC914
  UINT32            AdpCfg;           // Offset: 0xC920
  UINT32            AdpCtl;           // Offset: 0xC924
  UINT32            AdpEvt;           // Offset: 0xC928
  UINT32            AdpEvten;         // Offset: 0xC92C
  UINT32            BcCfg;            // Offset: 0xC930
  UINT32            Res10;            // Offset: 0xC934
  UINT32            BcEvt;            // Offset: 0xC938
  UINT32            BcEvten;          // Offset: 0xC93C
} DWC3;

#endif
