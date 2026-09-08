/** @file

  Copyright 2025, SpacemiT Co., Ltd.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef USBPHY_H_
#define USBPHY_H_

#include <Base.h>

/* USB2 PHY */
#define USB2_PHY_REG01                 0x4
#define USB2_PHY_REG01_PLL_IS_READY    (0x1 << 0)
#define USB2_PHY_REG04                 0x10
#define USB2_PHY_REG04_EN_HSTSOF       (0x1 << 0)
#define USB2_PHY_REG04_AUTO_CLEAR_DIS  (0x1 << 2)
#define USB2_PHY_REG08                 0x20
#define USB2_PHY_REG08_DISCON_DET      (0x1 << 9)
#define USB2_PHY_REG0D                 0x34
#define USB2_PHY_REG40                 0x40
#define USB2_PHY_REG40_CLR_DISC        (0x1 << 0)
#define USB2_PHY_REG26                 0x98
#define USB2_PHY_REG22                 0x88
#define USB2_CFG_FORCE_CDRCLK          (0x1 << 6)
#define USB2_PHY_REG06                 0x18
#define USB2_CFG_HS_SRC_SEL            (0x1 << 0)
#define USB2_ANALOG_REG14_13           0xa4
#define USB2_ANALOG_HSDAC_IREG_EN      (0x1 << 4)
#define USB2_ANALOG_HSDAC_ISEL_MASK    (0xf)
#define USB2_ANALOG_HSDAC_ISEL_11_INC  (0xb)
#define USB2_ANALOG_HSDAC_ISEL_25_INC  (0xf)
#define USB2_ANALOG_HSDAC_ISEL_15_INC  (0xc)
#define USB2_ANALOG_HSDAC_ISEL_17_INC  (0xd)
#define USB2_ANALOG_HSDAC_ISEL_22_INC  (0xe)

/* USB3 PHY */
#define MAX_NUM_PHY  2

#define PLL_TIMEOUT     500000 /* For PHY PLL lock (usec) */
#define PU_CAL_TIMEOUT  2000000
#define POLL_DELAY      500 /* Time between polls (usec) */

/* Selecting the combo PHY operating mode requires APMU regmap access */
#define PMUA_PCIE_SUBSYS_MGMT      0x1d8
#define PU_MATRIX_CONF_X8_DISABLE  BIT4
#define PU_MATRIX_CONF_USB_MASK    (BIT2 | BIT1 | BIT0)

#define PMUA_TYPEC_CTRL       0x110
#define TYPEC_ORIENT_FLIP     BIT2
#define TYPEC_ORIENT_OVRD_EN  BIT3
#define TYPEC_ORIENT_OVRD     BIT4

/* PHY rcal init requires APB_SPARE regmap access */
#define APB_SPARE_PU_CAL  0x178
#define PU_CAL            BIT17

#define APB_SPARE_RCAL_HSIO    0x17c
#define PU_CAL_DONE            BIT8
#define R_CAL_OVRD_STABLE_EN   BIT31
#define R_CAL_OVRD_STABLE_VAL  BIT30
#define R_CAL_OVRD_NTRIM_EN    BIT29
#define R_CAL_OVRD_PTRIM_EN    BIT28
#define R_CAL_OVRD_TRIM_EN     (R_CAL_OVRD_NTRIM_EN | R_CAL_OVRD_PTRIM_EN)
#define R_CAL_OVRD_NTRIM_MASK  (BIT27 | BIT26 | BIT25 | BIT24)
#define R_CAL_OVRD_NTRIM(n)    ((n) << 24)
#define NTRIM_DEFAULT          0x6
#define R_CAL_OVRD_PTRIM_MASK  (BIT23 | BIT22 | BIT21 | BIT20)
#define R_CAL_OVRD_PTRIM(n)    ((n) << 20)
#define PTRIM_DEFAULT          0xa

/* PHY Registers */
#define PHY_VERSION  0x0

#define PHY_RESET_CFG              0x04
#define EN_SAMPLE_DATA_AFTER_LOCK  BIT6

#define PHY_CLK_CFG         0x08
#define PLL_READY           BIT0
#define CFG_RXCLK_EN        BIT3
#define CFG_TXCLK_EN        BIT4
#define CFG_PCLK_EN         BIT5
#define CFG_PIPE_PCLK_EN    BIT6
#define CFG_REFCLK_FREQ(n)  ((n) << 7)
#define REFCLK_24M          0x2
#define CFG_SW_INIT_DONE    BIT11
#define CFG_PU_SSC_OUT      BIT23

#define PHY_MODE_CFG         0x0C
#define CFG_LFPS_TPERIOD_MASK (BIT9 | BIT8)
#define CFG_LFPS_TPERIOD(n)  ((n) << 8)
#define LFPS_TPERIOD_USB     0x3

#define PHY_PU_SEL   0x40
#define OVRD_STATUS  BIT10
#define CFG_STATUS   BIT9

#define PHY_PU_CK_REG  0x54
#define PU_REFCLK_100  BIT25

#define PHY_PLL_REG1       0x58
#define FREF_SEL(n)        ((n) << 13)
#define FREF_24M           0x1
#define SSC_DEP_SEL(n)     ((n) << 24)
#define SSC_5000PPM        0xa
#define SSC_MODE(n)        ((n) << 28)
#define SSC_CENTER_SPREAD  0x0
#define SSC_UP_SPREAD      0x1
#define SSC_DOWN_SPREAD    0x2
#define SSC_DOWN_SPREAD1   0x3

#define PHY_PLL_REG2  0x5c
#define SEL_REF100    BIT21

/* PHY RX Register Definitions */
#define PHY_RX_REG_A                 0x60
#define RX_REG3_MASK                 (BIT31 | BIT30 | BIT29 | BIT28 | BIT27 | BIT26 | BIT25 | BIT24)
#define RX_REG3_RDEG1(n)             ((n) << 30)
#define RX_REG3_RDEG1_DEFAULT        0x3
#define RX_REG3_ADJ_BIAS(n)          ((n) << 28)
#define RX_REG3_ADJ_BIAS_DEFAULT     0x1
#define RX_REG3_SEL_CBOOST_CODE      BIT27
#define RX_REG3_I_LOAD_REG(n)        ((n) << 24)
#define RX_REG3_I_LOAD_REG_DEFAULT   0x7
#define RX_REG2_MASK                 (BIT23 | BIT22 | BIT21 | BIT20 | BIT19 | BIT18 | BIT17 | BIT16)
#define RX_REG2_PSEL(n)              ((n) << 21)
#define RX_REG2_PSEL_DEFAULT         0x4
#define RX_REG2_FORCE_CSEL           BIT20
#define RX_REG2_CSEL(n)              ((n) << 16)
#define RX_REG2_CSEL_DEFAULT         0x8
#define RX_REG1_MASK                 (BIT15 | BIT14 | BIT13 | BIT12 | BIT11 | BIT10 | BIT9 | BIT8)
#define RX_REG1_RC_CALI_REG(n)       ((n) << 12)
#define RX_REG1_RC_CALI_REG_DEFAULT  0x7
#define RX_REG1_RTERM_REG(n)         ((n) << 8)
#define RX_REG1_RTERM_REG_DEFAULT    0x8
#define RX_REG0_MASK                 (BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0)
#define RX_REG0_RLOAD                BIT4

#define PHY_RX_REG_B                0x64
#define RX_REG6_MASK                (BIT23 | BIT22 | BIT21 | BIT20 | BIT19 | BIT18 | BIT17 | BIT16)
#define RX_REG6_BYPASS_ADPT         BIT22
#define RX_REG6_ADAPT_GAIN(n)       ((n) << 20)
#define RX_REG6_ADAPT_GAIN_DEFAULT  0x2
#define RX_REG6_H1_REG(n)           ((n) << 16)
#define RX_REG6_H1_REG_DEFAULT      0x8
#define RX_REG5_MASK                (BIT15 | BIT14 | BIT13 | BIT12 | BIT11 | BIT10 | BIT9 | BIT8)
#define RX_REG5_RCELL_BIAS(n)       ((n) << 12)
#define RX_REG5_RCELL_BIAS_DEFAULT  0x8
#define RX_REG5_RCELL_VCM(n)        ((n) << 8)
#define RX_REG5_RCELL_VCM_DEFAULT   0x8
#define RX_REG4_MASK                (BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0)
#define RX_REG4_MANUAL_CFG          BIT7
#define RX_REG4_RTERM_SEL           BIT5
#define RX_REG4_ENVOS               BIT4
#define RX_REG4_RDEG2(n)            ((n) << 1)
#define RX_REG4_RDEG2_DEFAULT       0x2

#define PHY_RXEQ_TIME             0xb4
#define RXEQ_TIME_OVRD_AMP_SOC    BIT24
#define RXEQ_TIME_CFG_AMP_SOC(n)  ((n) << 22)
#define AMP_SOC_650M              0x0
#define AMP_SOC_800M              0x1
#define AMP_SOC_870M              0x2
#define AMP_SOC_900M              0x3
#define OVRD_POST_C_SOC           BIT21
#define CFG_POST_C_SOC(n)         ((n) << 19)
#define OVRD_PRE_C_SOC            BIT18
#define CFG_PRE_C_SOC(n)          ((n) << 16)
#define CFG_RXEQ_TIMEOUT(n)       (n)

#define PHY_ADPT_CFG0            0x140
#define AFE_ADPT_RST_OVRD_EN     BIT1
#define AFE_ADPT_RST_OVRD_VAL    BIT4

#define USB3_CC_CONTROL_SWITCH_FLIP  BIT2

#endif
