/*******************************************************************************
Copyright (C) 2016 Marvell International Ltd.
Copyright (c) 2024~2025, SpacemiT Co., Ltd. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/
#ifndef __QSPI_MASTER_H__
#define __QSPI_MASTER_H__

#include <IndustryStandard/SpiNorFlashJedecSfdp.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>

#include <Protocol/ClockCtrl.h>
#include <Protocol/PinCtrl.h>
#include <Protocol/Spi.h>

#define SPI_MASTER_SIGNATURE  SIGNATURE_32 ('M', 'S', 'P', 'I')
#define SPI_MASTER_INSTANCE_FROM_THIS(a)  CR (a, SPI_MASTER_INSTANCE, SpiMaster, SPI_MASTER_SIGNATURE)

#define ENABLE_QSPI_XIP_READ
#define ENABLE_QSPI_DMA

#define QSPI_RX_FIFO_MAX        (128)
#define QSPI_TX_FIFO_MAX        (256)
#define QSPI_TX_BUFF_POP_MIN    (16)
#define QSPI_XIP_BUFF_MAX_SIZE  (512)

#define ST_QSPI_REG_BASE         (FixedPcdGet64(PcdSTQspiRegBase))
#define ST_QSPI_MAX_FREQ         (FixedPcdGet32(PcdSTQspiMaxFrequency))
#define ST_QSPI_CONTROLLER_ID    (FixedPcdGet8(PcdSTQspiControllerId))
#define ST_QSPI_CONTROLLER_NAME  (PcdGetPtr(PcdSTQspiControllerName))

// Multi-function pin
#define MFPR_PULL_SEL     (1 << 15)
#define MFPR_PULLUP_EN    (1 << 14)
#define MFPR_PULLDN_EN    (1 << 13)
#define PULLUP_SEL        (MFPR_PULL_SEL | MFPR_PULLUP_EN)
#define PULLDN_SEL        (MFPR_PULL_SEL | MFPR_PULLDN_EN)
#define PULL_STATE_CLR    (~(0x7 << 13))
#define MFPR_DRIVE_SHIFT  (10)
#define DRIVE_DS_MEDIUM   (0x2 << MFPR_DRIVE_SHIFT)
#define DRIVE_DS_FAST     (0x6 << MFPR_DRIVE_SHIFT)

#define AF_SEL_FN0  0x00
#define AF_SEL_FN1  0x01
#define AF_SEL_FN2  0x02
#define AF_CLEAR    (~0x07)

// ---------------------------------------------------------------
// QSPI Memory Map
// ---------------------------------------------------------------
#define QSPI_FLASH_A1_BASE  FixedPcdGet64(PcdSFMemMapBaseAddress)
#define QSPI_FLASH_A1_TOP   (QSPI_FLASH_A1_BASE + 0xa00000)
#define QSPI_FLASH_A2_BASE  QSPI_FLASH_A1_TOP
#define QSPI_FLASH_A2_TOP   (QSPI_FLASH_A2_BASE + 0x100000)
#define QSPI_FLASH_B1_BASE  QSPI_FLASH_A2_TOP
#define QSPI_FLASH_B1_TOP   (QSPI_FLASH_B1_BASE + 0x100000)
#define QSPI_FLASH_B2_BASE  QSPI_FLASH_B1_TOP
#define QSPI_FLASH_B2_TOP   (QSPI_FLASH_B2_BASE + 0x100000)

// ---------------------------------------------------------------
// Register definitions
// ---------------------------------------------------------------
#define QSPI_MCR      (0x000)
#define QSPI_TCR      (0x004)
#define QSPI_IPCR     (0x008)
#define QSPI_FLSHCR   (0x00C)
#define QSPI_BUF0CR   (0x010)
#define QSPI_BUF1CR   (0x014)
#define QSPI_BUF2CR   (0x018)
#define QSPI_BUF3CR   (0x01C)
#define QSPI_BFGENCR  (0x020)
#define QSPI_SOCCR    (0x024)
#define QSPI_BUF0IND  (0x030)
#define QSPI_BUF1IND  (0x034)
#define QSPI_BUF2IND  (0x038)
#define QSPI_SFAR     (0x100)
#define QSPI_SFACR    (0x104)
#define QSPI_SMPR     (0x108)
#define QSPI_RBSR     (0x10C)
#define QSPI_RBCT     (0x110)
#define QSPI_TBSR     (0x150)
#define QSPI_TBDR     (0x154)
#define QSPI_TBCT     (0x158)
#define QSPI_SR       (0x15C)
#define QSPI_FR       (0x160)
#define QSPI_RSER     (0x164)
#define QSPI_SPNDST   (0x168)
#define QSPI_SPTRCLR  (0x16C)
#define QSPI_SFA1AD   (0x180)
#define QSPI_SFA2AD   (0x184)
#define QSPI_SFB1AD   (0x188)
#define QSPI_SFB2AD   (0x18C)
#define QSPI_DLPV     (0x190)
#define QSPI_RBDR0    (0x200)
#define QSPI_LUTKEY   (0x300)
#define QSPI_LCKCR    (0x304)
#define QSPI_LUT0     (0x310)
#define QSPI_LUT1     (0x314)
#define QSPI_LUT2     (0x318)
#define QSPI_LUT3     (0x31C)

#define QSPI_TBSR_TRBFL_SHIFT  8
#define QSPI_TBSR_TRBFL_MASK   (0xff << QSPI_TBSR_TRBFL_SHIFT)

#define QSPI_MCR_XIP_EN         BIT23
#define QSPI_MCR_SW_PROG_ERASE  BIT22
#define QSPI_MCR_CLR_TXF        BIT11
#define QSPI_MCR_CLR_RXF        BIT10
#define QSPI_MCR_DDR_EN         BIT7
#define QSPI_MCR_SWRSTHD        BIT1
#define QSPI_MCR_SWRSTSD        BIT0

#define QSPI_BUF3CR_ALLMST_MASK  BIT31

#define QSPI_SR_AHB_ACC  BIT2
#define QSPI_SR_IP_ACC   BIT1
#define QSPI_SR_BUSY     BIT0

#define QSPI_FR_DLPFF        BIT31
#define QSPI_FR_TBFF         BIT27
#define QSPI_FR_TBUF         BIT26
#define QSPI_FR_ILLINE       BIT23
#define QSPI_FR_RBOF         BIT17
#define QSPI_FR_RBDF         BIT16
#define QSPI_FR_ABSEF        BIT15
#define QSPI_FR_AITEF        BIT14
#define QSPI_FR_AIBSEF       BIT13
#define QSPI_FR_ABOF         BIT12
#define QSPI_FR_IUEF         BIT11
#define QSPI_FR_IPAEF        BIT7
#define QSPI_FR_IPIEF        BIT6
#define QSPI_FR_IPGEF        BIT4
#define QSPI_FR_XIP_SUSPEND  BIT3
#define QSPI_FR_XIP_RESUME   BIT2
#define QSPI_FR_XIP_ON       BIT1
#define QSPI_FR_TFF          BIT0

#define QSPI_RBCT_RXBRD_MASK  BIT8

#define QSPI_RSER_DLPFIE   BIT31
#define QSPI_RSER_TBFIE    BIT27
#define QSPI_RSER_TBUIE    BIT26
#define QSPI_RSER_TBFDE    BIT25
#define QSPI_RSER_ILLINIE  BIT23
#define QSPI_RSER_RBDDE    BIT21
#define QSPI_RSER_RBOIE    BIT17
#define QSPI_RSER_RBDIE    BIT16
#define QSPI_RSER_ABSEIE   BIT15
#define QSPI_RSER_AITIE    BIT14
#define QSPI_RSER_AIBSIE   BIT13
#define QSPI_RSER_ABOIE    BIT12
#define QSPI_RSER_IUEIE    BIT11
#define QSPI_RSER_IPIEIE   BIT6
#define QSPI_RSER_IPGEIE   BIT4
#define QSPI_RSER_XIP_ON   BIT1
#define QSPI_RSER_TFIE     BIT0

#define QSPI_SPTRCLR_IPPTRC  BIT8
#define QSPI_SPTRCLR_BFPTRC  BIT0

#define QSPI_LUTKEY_VALUE  0x5af05af0
#define QSPI_LCKER_LOCK    BIT0
#define QSPI_LCKER_UNLOCK  BIT1

#define QSPI_BUF3CR_ADATSZ(x)  ((x) << 8)
#define QSPI_BFGENCR_SEQID(x)  ((x) << 12)

#define QSPI_IPCR_SEQID(x)  ((x) << 24)
#define QSPI_RBDR(x)        (QSPI_RBDR0 + ((x) * 4))

/* 16Bytes per sequence */
#define QSPI_LUT_REG(seqid, i)  (QSPI_LUT0 + (seqid) * 16 + (i) * 4)

/*
 * The PAD definitions for LUT register.
 *
 * The pad stands for the number of IO lines [0:3].
 * For example, the quad read needs four IO lines,
 * so you should use LUT_PAD(4).
 */
#define LUT_PAD(x)  (fls(x) - 1)

/*
 * One sequence must be consisted of 4 LUT enteries(16Bytes).
 * LUT entries with the following register layout:
 * b'31                                                                     b'0
 *  ---------------------------------------------------------------------------
 *  |INSTR1[15~10]|PAD1[9~8]|OPRND1[7~0] | INSTR0[15~10]|PAD0[9~8]|OPRND0[7~0]|
 *  ---------------------------------------------------------------------------
 */
#define LUT_DEF(ins, pad, opr)  (((ins) << 10) | ((pad) << 8) | (opr))

/*
 * QSPI Sequence index.
 * index 0 is preset at boot for AHB read,
 * index 1 is used for other command.
 */
#define SEQID_LUT_AHBREAD_ID  0
#define SEQID_LUT_SHARED_ID   1

// ---------------------------------------------------------------
// Enumeration & Structure
// ---------------------------------------------------------------
enum QSPI_INST_E {
  LUT_INSTR_STOP       = 0x0,
  LUT_INSTR_CMD        = 0x1,
  LUT_INSTR_ADDR       = 0x2,
  LUT_INSTR_DUMMY      = 0x3,
  LUT_INSTR_MODE       = 0x4,
  LUT_INSTR_MODE2      = 0x5,
  LUT_INSTR_MODE4      = 0x6,
  LUT_INSTR_READ       = 0x7,
  LUT_INSTR_WRITE      = 0x8,
  LUT_INSTR_JMP_ON_CS  = 0x9,
  LUT_INSTR_ADDR_DDR   = 0xA,
  LUT_INSTR_MODE_DDR   = 0xB,
  LUT_INSTR_MODE2_DDR  = 0xC,
  LUT_INSTR_MODE4_DDR  = 0xD,
  LUT_INSTR_READ_DDR   = 0xE,
  LUT_INSTR_WRITE_DDR  = 0xF,
  LUT_INSTR_DATA_LEARN = 0x10
};

enum QSPI_PAD_E {
  QSPI_PAD_1X   = 0x0,
  QSPI_PAD_2X   = 0x1,
  QSPI_PAD_4X   = 0x2,
  QSPI_PAD_RSVD = 0x3
};

// Marvell Flash Device Controller Registers
#define SPI_CTRL_REG       (0x00)
#define SPI_CONF_REG       (0x04)
#define SPI_DATA_OUT_REG   (0x08)
#define SPI_DATA_IN_REG    (0x0c)
#define SPI_INT_CAUSE_REG  (0x10)

// Serial Memory Interface Control Register Masks
#define SPI_CS_NUM_OFFSET   2
#define SPI_CS_NUM_MASK     (0x7 << SPI_CS_NUM_OFFSET)
#define SPI_MEM_READY_MASK  (0x1 << 1)
#define SPI_CS_EN_MASK      (0x1 << 0)

// Serial Memory Interface Configuration Register Masks
#define SPI_BYTE_LENGTH_OFFSET  5
#define SPI_BYTE_LENGTH         (0x1  << SPI_BYTE_LENGTH_OFFSET)
#define SPI_CPOL_OFFSET         11
#define SPI_CPOL_MASK           (0x1 << SPI_CPOL_OFFSET)
#define SPI_CPHA_OFFSET         12
#define SPI_CPHA_MASK           (0x1 << SPI_CPHA_OFFSET)
#define SPI_TXLSBF_OFFSET       13
#define SPI_TXLSBF_MASK         (0x1 << SPI_TXLSBF_OFFSET)
#define SPI_RXLSBF_OFFSET       14
#define SPI_RXLSBF_MASK         (0x1 << SPI_RXLSBF_OFFSET)

#define SPI_SPR_OFFSET      0
#define SPI_SPR_MASK        (0xf << SPI_SPR_OFFSET)
#define SPI_SPPR_0_OFFSET   4
#define SPI_SPPR_0_MASK     (0x1 << SPI_SPPR_0_OFFSET)
#define SPI_SPPR_HI_OFFSET  6
#define SPI_SPPR_HI_MASK    (0x3 << SPI_SPPR_HI_OFFSET)

#define SPI_TRANSFER_BEGIN  0x01             // Assert CS before transfer
#define SPI_TRANSFER_END    0x02             // Deassert CS after transfers

#define SPI_TIMEOUT            100000
#define QSPI_RX_FIFO_MAX_SIZE  (128)
#define QSPI_TX_FIFO_MAX_SIZE  (256)

enum {
  QSPI_LUT_NOTSET = -1,
  QSPI_LUT_SEQID0 = 0,
  QSPI_LUT_SEQID1,
  QSPI_LUT_SEQID2,
  QSPI_LUT_SEQID3,
  QSPI_LUT_SEQID4,
  QSPI_LUT_SEQID5,
  QSPI_LUT_SEQID6,
  QSPI_LUT_SEQID7,
  QSPI_LUT_SEQID8,
  QSPI_LUT_SEQID9,
  QSPI_LUT_SEQID10,
  QSPI_LUT_SEQID11,
  QSPI_LUT_SEQID12,
  QSPI_LUT_SEQID13,
  QSPI_LUT_SEQID14,
  QSPI_LUT_SEQID15,
};

enum {
  QSPI_NORMAL_MODE = 0,
  QSPI_DISABLE_MODE,
  QSPI_STOP_MODE,
};

enum {
  QSPI_FUNC_CLK_409MHZ = 0,
  QSPI_FUNC_CLK_375MHZ,
  QSPI_FUNC_CLK_307MHZ,
  QSPI_FUNC_CLK_245MHZ,
  QSPI_FUNC_CLK_223MHZ,
  QSPI_FUNC_CLK_106MHZ,
  QSPI_FUNC_CLK_495MHZ,
  QSPI_FUNC_CLK_189MHZ,
};

enum {
  QSPI_CS_A1 = 0,
  QSPI_CS_A2,
  QSPI_CS_B1,
  QSPI_CS_B2,
  QSPI_CS_MAX,
};

typedef struct {
  UINT8     UseDma;
  UINT8     XipRead;
  UINT32    MaxFreq;
  UINT32    RxUnitSize;
  UINT32    TxUnitSize;
  UINT32    XipBufMax;
  UINT32    CsAddr[QSPI_CS_MAX];
  UINTN     RegisterBase;
} _QSPI_HOST;

typedef struct {
  UINTN                         Signature;
  EFI_HANDLE                    Handle;
  SPI_MASTER_PROTOCOL           SpiMaster;
  EFI_LOCK                      Lock;
  _QSPI_HOST                    QspiHost;
  SILICON_CLOCKCTRL_PROTOCOL    *ClockCtrlProtocol;
  SILICON_PINCTRL_PROTOCOL      *PinCtrlProtocol;
} SPI_MASTER_INSTANCE;

/**
 * fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */
static inline UINT32
fls (
  UINT32  x
  )
{
  UINT32  r = 32;

  if (!x) {
    return 0;
  }

  if (!(x & 0xffff0000u)) {
    x <<= 16;
    r  -= 16;
  }

  if (!(x & 0xff000000u)) {
    x <<= 8;
    r  -= 8;
  }

  if (!(x & 0xf0000000u)) {
    x <<= 4;
    r  -= 4;
  }

  if (!(x & 0xc0000000u)) {
    x <<= 2;
    r  -= 2;
  }

  if (!(x & 0x80000000u)) {
    r -= 1;
  }

  return r;
}

#endif // __QSPI_MASTER_H__
