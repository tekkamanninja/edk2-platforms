/** @file
  Spacemit Qspi master driver implementation.

  Copyright (C) 2016 Marvell International Ltd.
  Copyright (c) 2024~2025, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "QspiDxe.h"
#include <Library/MemoryManagementLib.h>

STATIC EFI_EVENT     mSpiMasterVirtualAddrChangeEvent;
SPI_MASTER_INSTANCE  *mSpiMasterInstance;

STATIC UINT32  Reg_offset_table[] = {
  QSPI_MCR,     QSPI_TCR,     QSPI_IPCR,    QSPI_FLSHCR,
  QSPI_BUF0CR,  QSPI_BUF1CR,  QSPI_BUF2CR,  QSPI_BUF3CR,
  QSPI_BFGENCR, QSPI_SOCCR,   QSPI_BUF0IND, QSPI_BUF1IND,
  QSPI_BUF2IND, QSPI_SFAR,    QSPI_SFACR,   QSPI_SMPR,
  QSPI_RBSR,    QSPI_RBCT,    QSPI_TBSR,    QSPI_TBDR,
  QSPI_TBCT,    QSPI_SR,      QSPI_FR,      QSPI_RSER,
  QSPI_SPNDST,  QSPI_SPTRCLR, QSPI_SFA1AD,  QSPI_SFA2AD,
  QSPI_SFB1AD,  QSPI_SFB2AD,  QSPI_DLPV,    QSPI_LUTKEY,
  QSPI_LCKCR
};

STATIC
VOID
QspiRegwrite32 (
  IN _QSPI_HOST  *QspiHost,
  IN UINT32      Offset,
  IN UINT32      Val
  )
{
  MmioWrite32 (QspiHost->RegisterBase + Offset, Val);
}

STATIC
UINT32
QspiRegRead32 (
  IN _QSPI_HOST  *QspiHost,
  IN UINT32      Offset
  )
{
  return MmioRead32 (QspiHost->RegisterBase + Offset);
}

STATIC
VOID
QspiDumpReg (
  IN _QSPI_HOST  *QspiHost
  )
{
  UINT32  Reg = 0;
  INT32   I;

  if (EfiAtRuntime ()) {
    return;
  }

  DEBUG ((DEBUG_INFO, "Dump QSPI host register:\n"));
  for (I = 0; I < ARRAY_SIZE (Reg_offset_table); I++) {
    if ((I > 0) && (I % 4 == 0)) {
      DEBUG ((DEBUG_INFO, "\n"));
    }

    Reg = QspiRegRead32 (QspiHost, Reg_offset_table[I]);
    DEBUG (
           (DEBUG_INFO, "offset[0x%03x]:0x%08x\t\t",
            Reg_offset_table[I], Reg)
           );
  }

  DEBUG ((DEBUG_INFO, "\ndump AHB read LUT:\n"));
  for (I = 0; I < 4; I++) {
    Reg = QspiRegRead32 (QspiHost, QSPI_LUT_REG (SEQID_LUT_AHBREAD_ID, I));
    DEBUG (
           (DEBUG_INFO, "lut_reg[0x%03x]:0x%08x\t\t",
            QSPI_LUT_REG (SEQID_LUT_AHBREAD_ID, I), Reg)
           );
  }

  DEBUG ((DEBUG_INFO, "\ndump shared LUT:\n"));
  for (I = 0; I < 4; I++) {
    Reg = QspiRegRead32 (QspiHost, QSPI_LUT_REG (SEQID_LUT_SHARED_ID, I));
    DEBUG (
           (DEBUG_INFO, "lut_reg[0x%03x]:0x%08x\t\t",
            QSPI_LUT_REG (SEQID_LUT_SHARED_ID, I), Reg)
           );
  }

  DEBUG ((DEBUG_INFO, "\n"));
}

STATIC
VOID
QspiApplyPinctrlState (
  VOID
  )
{
  EFI_STATUS                Status;
  SILICON_PINCTRL_PROTOCOL  *PinCtrlProtocol;

  if (mSpiMasterInstance == NULL) {
    return;
  }

  PinCtrlProtocol = mSpiMasterInstance->PinCtrlProtocol;
  if (NULL == PinCtrlProtocol) {
    DEBUG ((DEBUG_ERROR, "%a: PinCtrlProtocol NOT found\n", __func__));
    return;
  }

  if (PinCtrlProtocol->ApplyStateById == NULL) {
    DEBUG ((DEBUG_WARN, "%a: pinctrl state API is unavailable\n", __func__));
    return;
  }

  Status = PinCtrlProtocol->ApplyStateById (
                                            PinCtrlProtocol,
                                            "qspi",
                                            ST_QSPI_CONTROLLER_ID,
                                            NULL,
                                            PINCTRL_STATE_DEFAULT
                                            );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DEBUG_WARN, "%a: no pinctrl default state map for qspi[%u]\n", __func__, ST_QSPI_CONTROLLER_ID));
      return;
    }

    DEBUG ((DEBUG_WARN, "%a: failed to apply pinctrl default state for qspi[%u]: %r\n", __func__, ST_QSPI_CONTROLLER_ID, Status));
  }
}

STATIC
VOID
QspiMmioRemap (
  VOID
  )
{
  MapRegToGcdRunTimeMmioSpace (QSPI_FLASH_A1_BASE, QSPI_FLASH_B2_TOP - QSPI_FLASH_A1_BASE);
  MapRegToGcdRunTimeMmioSpace (ST_QSPI_REG_BASE, SIZE_4KB);
}

STATIC
EFI_STATUS
QspiPollRegStatus (
  IN _QSPI_HOST  *QspiHost,
  IN UINT32      RegOffset,
  IN UINT32      Mask,
  IN UINT32      Value,
  IN UINT32      TimeoutMs
  )
{
  UINT32  TimeoutUs;

  TimeoutUs = TimeoutMs * 1000;
  while (1) {
    if (Value == (QspiRegRead32 (QspiHost, RegOffset) & Mask)) {
      break;
    }

    MicroSecondDelay (1);
    if (!TimeoutUs--) {
      DEBUG (
             (DEBUG_ERROR,
              "Timeout while poll Qspi 0x%x for Val 0x%x!\n", RegOffset, Value)
             );
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

/*
 * IP Command Trigger could not be executed Error Flag may happen for write
 * access to RBCT/SFAR register, need retry for these two register
 */
STATIC
VOID
QspiWriteRbct (
  IN _QSPI_HOST  *QspiHost,
  IN UINT32      Val
  )
{
  UINT32  Temp;

  do {
    QspiRegwrite32 (QspiHost, QSPI_RBCT, Val);
    Temp = QspiRegRead32 (QspiHost, QSPI_FR);
    if (!(Temp & QSPI_FR_IPIEF)) {
      break;
    }

    Temp &= QSPI_FR_IPIEF;
    QspiRegwrite32 (QspiHost, QSPI_FR, Temp);

    MicroSecondDelay (1);
  } while (1);
}

STATIC
VOID
QspiWriteSfar (
  IN _QSPI_HOST  *QspiHost,
  IN UINT32      Val
  )
{
  UINT32  Temp;

  do {
    QspiRegwrite32 (QspiHost, QSPI_SFAR, Val);
    Temp = QspiRegRead32 (QspiHost, QSPI_FR);
    if (!(Temp & QSPI_FR_IPIEF)) {
      break;
    }

    Temp &= QSPI_FR_IPIEF;
    QspiRegwrite32 (QspiHost, QSPI_FR, Temp);

    MicroSecondDelay (1);
  } while (1);
}

STATIC
VOID
QspiSwtichMode (
  IN _QSPI_HOST  *QspiHost,
  UINT32         Mode
  )
{
  UINT32  Mcr;

  Mcr = QspiRegRead32 (QspiHost, QSPI_MCR);
  if (QSPI_NORMAL_MODE == Mode) {
    Mcr &= ~(0x1 << 14);
  } else if (QSPI_DISABLE_MODE == Mode) {
    Mcr |= (0x1 << 14);
  }

  QspiRegwrite32 (QspiHost, QSPI_MCR, Mcr);
}

VOID
QspiLockLut (
  IN _QSPI_HOST  *QspiHost
  )
{
  UINT32  Lckcr;

  Lckcr = QspiRegRead32 (QspiHost, QSPI_LCKCR);
  if (Lckcr & QSPI_LCKER_LOCK) {
    return;
  }

  QspiRegwrite32 (QspiHost, QSPI_LUTKEY, QSPI_LUTKEY_VALUE);
  QspiRegwrite32 (QspiHost, QSPI_LCKCR, QSPI_LCKER_LOCK);
}

VOID
QspiUnlockLut (
  IN _QSPI_HOST  *QspiHost
  )
{
  UINT32  Lckcr;

  Lckcr = QspiRegRead32 (QspiHost, QSPI_LCKCR);
  if (Lckcr & QSPI_LCKER_UNLOCK) {
    return;
  }

  QspiRegwrite32 (QspiHost, QSPI_LUTKEY, QSPI_LUTKEY_VALUE);
  QspiRegwrite32 (QspiHost, QSPI_LCKCR, QSPI_LCKER_UNLOCK);
}

/*
 * If the slave device content being changed by Write/Erase, need to
 * invalidate the AHB buffer. This can be achieved by doing the reset
 * of controller after setting MCR0[SWRESET] bit.
 */
STATIC
VOID
QspiReset (
  IN _QSPI_HOST  *QspiHost
  )
{
  UINT32  Reg, Mask;

  do {
    if (!(QspiRegRead32 (QspiHost, QSPI_SR) & QSPI_SR_BUSY) &&
        !(QspiRegRead32 (QspiHost, QSPI_FR) & QSPI_FR_XIP_ON))
    {
      break;
    }
  } while (1);

  /* qspi softreset first */
  Mask = QSPI_MCR_SWRSTHD | QSPI_MCR_SWRSTSD;
  Reg  = QspiRegRead32 (QspiHost, QSPI_MCR);
  Reg |= Mask;
  QspiRegwrite32 (QspiHost, QSPI_MCR, Reg);

  Reg = QspiRegRead32 (QspiHost, QSPI_MCR);
  if ((Reg & Mask) != Mask) {
    DEBUG ((DEBUG_VERBOSE, "Qspi reset ignored 0x%x\n", Reg));
  }

  /*
   * The minimum delay : 1 AHB + 2 SFCK clocks.
   * Delay 1 us is enough.
   */
  MicroSecondDelay (1);

  Reg &= ~Mask;
  QspiRegwrite32 (QspiHost, QSPI_MCR, Reg);
}

STATIC
VOID
QspiConfigXipRead (
  IN _QSPI_HOST  *QspiHost,
  IN UINT32      DataSize,
  IN UINT32      SeqId
  )
{
  UINT32  BufCfg;

  BufCfg = QSPI_BUF3CR_ALLMST_MASK | QSPI_BUF3CR_ADATSZ (DataSize / 8);

  /*
   * Config the ahb buffer
   * Disable BUF0~BUF1, use BUF3 for all masters
   */
  QspiRegwrite32 (QspiHost, QSPI_BUF0IND, 0);
  QspiRegwrite32 (QspiHost, QSPI_BUF1IND, 0);
  QspiRegwrite32 (QspiHost, QSPI_BUF2IND, 0);

  /* AHB Master port */
  QspiRegwrite32 (QspiHost, QSPI_BUF0CR, 0xe);
  QspiRegwrite32 (QspiHost, QSPI_BUF1CR, 0xe);
  QspiRegwrite32 (QspiHost, QSPI_BUF2CR, 0xe);
  QspiRegwrite32 (QspiHost, QSPI_BUF3CR, BufCfg);      // other masters

  QspiRegwrite32 (QspiHost, QSPI_BFGENCR, QSPI_BFGENCR_SEQID (SeqId));
  DEBUG ((DEBUG_INFO, "XIP read max size: %d\n", DataSize));
}

STATIC
BOOLEAN
IsReadFromCacheOpcode (
  IN UINT8  opcode
  )
{
  BOOLEAN  ret;

  ret = ((opcode == SPI_FLASH_READ) ||
         (opcode == SPI_FLASH_FAST_READ));

  return ret;
}

STATIC
VOID
QspiConfigLookupTable (
  IN _QSPI_HOST   *QspiHost,
  IN SPI_XFER_OP  *Op,
  IN UINT32       SeqId
  )
{
  UINT32  LutValue;
  UINT16  LutEntry[8];
  UINT8   Index = 0, I, Opcode;

  Opcode = Op->Cmd.Opcode;

  /* qspi cmd */
  LutEntry[Index++] = LUT_DEF (
                               LUT_INSTR_CMD,
                               LUT_PAD (Op->Cmd.BusWidth),
                               Opcode
                               );

  /* addr bytes */
  if (Op->Addr.Nbytes) {
    LutEntry[Index++] = LUT_DEF (
                                 LUT_INSTR_ADDR,
                                 LUT_PAD (Op->Addr.BusWidth),
                                 Op->Addr.Nbytes * 8 / Op->Addr.BusWidth
                                 );
  }

  /* dummy bytes */
  if (Op->Dummy.Nbytes) {
    LutEntry[Index++] = LUT_DEF (
                                 LUT_INSTR_DUMMY,
                                 LUT_PAD (Op->Dummy.BusWidth),
                                 Op->Dummy.Nbytes * 8 / Op->Dummy.BusWidth
                                 );
  }

  /* read/write data bytes */
  if (Op->Data.Nbytes) {
    LutEntry[Index++] = LUT_DEF (
                                 SPI_XFER_RX_DATA == Op->Data.Dir ?
                                 LUT_INSTR_READ : LUT_INSTR_WRITE,
                                 LUT_PAD (Op->Data.BusWidth),
                                 0
                                 );
  }

  /* Add stop at the end */
  LutEntry[Index++] = LUT_DEF (LUT_INSTR_STOP, 0, 0);

  /* unlock LUT */
  QspiUnlockLut (QspiHost);

  // QSPI controller has 16 group LUT sequence, each sequence support 8
  // instruction max, which contains 4*32bit QSPI_LUT register.
  // instruction is configured in 16bit register as below:
  // bit10~15: instruction code; bit8~9: pad numbers bit0~7: command
  for (I = 0; I < Index / 2; I++) {
    LutValue = LutEntry[I * 2] | (LutEntry[I * 2 + 1] << 16);
    QspiRegwrite32 (QspiHost, QSPI_LUT0 + SeqId * 0x10 + I * 0x4, LutValue);
  }

  // the last odd index
  if (Index % 2) {
    LutValue = LutEntry[Index - 1];
    QspiRegwrite32 (QspiHost, QSPI_LUT0 + SeqId * 0x10 + (Index / 2) * 0x4, LutValue);
  }

  /* lock LUT */
  QspiLockLut (QspiHost);
}

VOID
QspiHostSetClock (
  IN _QSPI_HOST  *QspiHost,
  IN UINT32      Freq
  )
{
  UINT64                      QspiFreq;
  SILICON_CLOCKCTRL_PROTOCOL  *ClockCtrlProtocol;

  ClockCtrlProtocol = mSpiMasterInstance->ClockCtrlProtocol;
  if (NULL == ClockCtrlProtocol) {
    DEBUG ((DEBUG_ERROR, "No Clock Control Protocol found!\n"));
    return;
  }

  ClockCtrlProtocol->SetClockState (
                                    ClockCtrlProtocol,
                                    (CONST CHAR8 *)ST_QSPI_CONTROLLER_NAME,
                                    ENABLE_CLOCK
                                    );

  ClockCtrlProtocol->SetClockRate (
                                   ClockCtrlProtocol,
                                   (CONST CHAR8 *)ST_QSPI_CONTROLLER_NAME,
                                   (UINT64)Freq
                                   );

  ClockCtrlProtocol->GetClockRate (
                                   ClockCtrlProtocol,
                                   (CONST CHAR8 *)ST_QSPI_CONTROLLER_NAME,
                                   &QspiFreq
                                   );

  QspiFreq /= 1000000;

  /* clock settings */
  QspiSwtichMode (QspiHost, QSPI_DISABLE_MODE);

  /* sampled by sfif_clk_b; half cycle delay; */
  if (QspiFreq < 104) {
    QspiRegwrite32 (QspiHost, QSPI_SMPR, 0x0);
  } else {
    QspiRegwrite32 (QspiHost, QSPI_SMPR, 0x1 << 5);
  }

  /* Module enabled */
  QspiSwtichMode (QspiHost, QSPI_NORMAL_MODE);

  DEBUG ((DEBUG_INFO, "Set qspi bus clock: %lluMHz\n", QspiFreq));
}

STATIC
VOID
QspiXipRead (
  IN UINT8   *Buff,
  IN UINT32  Addr,
  IN UINT32  Size
  )
{
  /* Read out the data directly from the AHB buffer. */
  DEBUG (
         (DEBUG_VERBOSE,
          "AHB read %d bytes from address: 0x%x\n",
          Size,
          Addr)
         );
  CopyMem (Buff, (UINT8 *)(UINTN)Addr, Size);
}

STATIC
VOID
QspiFillTxFifo (
  IN _QSPI_HOST    *QspiHost,
  OUT CONST UINT8  *Buff,
  IN UINT32        Size
  )
{
  INT32   I;
  UINT32  Val;

  for (I = 0; I < Size / 4 * 4; I += 4) {
    CopyMem (&Val, Buff + I, 4);
    QspiRegwrite32 (QspiHost, QSPI_TBDR, Val);
  }

  if (I < Size) {
    CopyMem (&Val, Buff + I, Size - I);
    QspiRegwrite32 (QspiHost, QSPI_TBDR, Val);
  }

  /*
   * There must be atleast 128bit data available in TX FIFO
   * for any pop operation otherwise QSPI_FR[TBUF] will be set
   */
  for (I = Size; I < QSPI_TX_BUFF_POP_MIN; I += 4) {
    QspiRegwrite32 (QspiHost, QSPI_TBDR, 0);
  }
}

STATIC
VOID
QspiReadRxFifo (
  IN _QSPI_HOST  *QspiHost,
  OUT UINT8      *Buffer,
  IN UINT32      Size
  )
{
  INT32   I;
  UINT32  Val;

  DEBUG ((DEBUG_VERBOSE, "ip read %d bytes\n", Size));
  for (I = 0; I < Size / 4 * 4; I += 4) {
    Val = QspiRegRead32 (QspiHost, QSPI_RBDR (I / 4));
    CopyMem (Buffer + I, &Val, 4);
  }

  if (I < Size) {
    Val = QspiRegRead32 (QspiHost, QSPI_RBDR (I / 4));
    CopyMem (Buffer + I, &Val, Size - I);
  }
}

STATIC
EFI_STATUS
QspiStartTransfer (
  IN _QSPI_HOST  *QspiHost,
  IN UINT32      Size,
  IN UINT32      LutIndex
  )
{
  EFI_STATUS  Status;

  /* dump Reg if need */
  // QspiDumpReg ();

  /* trigger LUT */
  QspiRegwrite32 (QspiHost, QSPI_IPCR, Size | QSPI_IPCR_SEQID (LutIndex));

  /* wait for the transaction complete */
  Status = QspiPollRegStatus (

                              QspiHost,
                              QSPI_FR,
                              QSPI_FR_TFF,
                              QSPI_FR_TFF,
                              100
                              );
  if (EFI_SUCCESS == Status) {
    Status = QspiPollRegStatus (
                                QspiHost,
                                QSPI_SR,
                                QSPI_SR_BUSY,
                                0,
                                300
                                );
  }

  if (EFI_SUCCESS != Status) {
    DEBUG ((DEBUG_ERROR, "Transaction timeout!\n"));
    QspiDumpReg (QspiHost);
  }

  return Status;
}

STATIC
VOID
SpiHostControllerInit (
  IN _QSPI_HOST  *QspiHost
  )
{
  UINT32  Reg;

  QspiHost->RegisterBase = ST_QSPI_REG_BASE;
  QspiHost->MaxFreq      = ST_QSPI_MAX_FREQ;

  QspiHost->CsAddr[QSPI_CS_A1] = QSPI_FLASH_A1_BASE;
  QspiHost->CsAddr[QSPI_CS_A2] = QSPI_FLASH_A2_BASE;
  QspiHost->CsAddr[QSPI_CS_B1] = QSPI_FLASH_B1_BASE;
  QspiHost->CsAddr[QSPI_CS_B2] = QSPI_FLASH_B2_BASE;

  QspiHost->XipBufMax  = QSPI_XIP_BUFF_MAX_SIZE;
  QspiHost->TxUnitSize = QSPI_TX_FIFO_MAX;
  QspiHost->RxUnitSize = QSPI_RX_FIFO_MAX;
 #ifdef ENABLE_QSPI_XIP_READ
  QspiHost->XipRead    = 1;
  QspiHost->RxUnitSize = 4096;
 #endif

  /* Controller register address remap to MMIO */
  QspiMmioRemap ();

  /* Apply pinctrl state from board map by controller type/id. */
  QspiApplyPinctrlState ();

  /* config qspi clk */
  QspiHostSetClock (QspiHost, (UINT64)QspiHost->MaxFreq);

  /* qspi softreset first */
  QspiReset (QspiHost);

  /* clock settings */
  QspiSwtichMode (QspiHost, QSPI_DISABLE_MODE);

  /* Fix wirte failure issue*/
  QspiRegwrite32 (QspiHost, QSPI_SOCCR, 0x8);
  /* Give the default source address */
  // QspiRegwrite32(QspiHost, QSPI_SFAR, QspiHost->CsAddr[QSPI_CS_A1]);
  QspiWriteSfar (QspiHost, QspiHost->CsAddr[QSPI_CS_A1]);
  QspiRegwrite32 (QspiHost, QSPI_SFACR, 0x0);

  /* config XIP read */
  QspiConfigXipRead (QspiHost, QspiHost->XipBufMax, SEQID_LUT_AHBREAD_ID);

  /* Set flash memory map */
  QspiRegwrite32 (QspiHost, QSPI_SFA1AD, QSPI_FLASH_A1_TOP & 0xfffffc00);
  QspiRegwrite32 (QspiHost, QSPI_SFA2AD, QSPI_FLASH_A2_TOP & 0xfffffc00);
  QspiRegwrite32 (QspiHost, QSPI_SFB1AD, QSPI_FLASH_B1_TOP & 0xfffffc00);
  QspiRegwrite32 (QspiHost, QSPI_SFB2AD, QSPI_FLASH_B2_TOP & 0xfffffc00);

  /*
   * ISD3FB, ISD2FB, ISD3FA, ISD2FA = 1; ENDIAN = 0x3; END_CFG=0x3
   * DELAY_CLK4X_EN = 1
   */
  Reg  = QspiRegRead32 (QspiHost, QSPI_MCR);
  Reg |= 0x000f000c;
  QspiRegwrite32 (QspiHost, QSPI_MCR, Reg);

  /* Module enabled */
  QspiSwtichMode (QspiHost, QSPI_NORMAL_MODE);

  /* Read using the IP Bus registers QSPI_RBDR0 to QSPI_RBDR31*/
  // _writel(0x1 << 8, QSPI_RBCT);
  QspiWriteRbct (QspiHost, QSPI_RBCT_RXBRD_MASK);

  /* clear all interrupt status */
  QspiRegwrite32 (QspiHost, QSPI_FR, 0xffffffff);

  DEBUG (
         (DEBUG_INFO,
          "rx fifo size:%d, tx fifo size:%d, xip buf size=%d\n",
          QspiHost->RxUnitSize, QspiHost->TxUnitSize, QspiHost->XipBufMax)
         );
  DEBUG ((DEBUG_INFO, "XIP read %a\n", QspiHost->XipRead ? "enabled" : "disabled"));
}

STATIC
EFI_STATUS
EFIAPI
SpiAjustOPSize (
  IN SPI_MASTER_PROTOCOL  *This,
  IN SPI_XFER_OP          *Op
  )
{
  SPI_MASTER_INSTANCE  *SpiMasterInstance = SPI_MASTER_INSTANCE_FROM_THIS (This);
  _QSPI_HOST           *QspiHost          = &SpiMasterInstance->QspiHost;

  if (SPI_XFER_TX_DATA == Op->Data.Dir) {
    if (Op->Data.Nbytes > QspiHost->TxUnitSize) {
      Op->Data.Nbytes = QspiHost->TxUnitSize;
    }
  } else {
    if (Op->Data.Nbytes > QspiHost->RxUnitSize) {
      Op->Data.Nbytes = QspiHost->RxUnitSize;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
SpiTransfer (
  IN SPI_MASTER_PROTOCOL  *This,
  IN SPI_DEVICE           *Slave,
  IN SPI_XFER_OP          *Op
  )
{
  EFI_STATUS  Status;
  UINT32      Address, Mask, Val, Opcode;
  UINT32      IsrCfg, SFA1AD, SFA2AD, SFB1AD, SFB2AD;

  SPI_MASTER_INSTANCE  *SpiMasterInstance = SPI_MASTER_INSTANCE_FROM_THIS (This);
  _QSPI_HOST           *QspiHost          = &SpiMasterInstance->QspiHost;

  Opcode = Op->Cmd.Opcode;

  /* wait for controller being ready */
  Mask   = QSPI_SR_BUSY | QSPI_SR_IP_ACC | QSPI_SR_AHB_ACC;
  Status = QspiPollRegStatus (QspiHost, QSPI_SR, Mask, 0, 100);
  if (EFI_SUCCESS != Status) {
    DEBUG ((DEBUG_ERROR, "QSPI controller not ready!\n"));
    return Status;
  }

  // restore QSPI controller configuration while has been modified in kernel
  if (EfiAtRuntime ()) {
    /* disable the interrupt */
    IsrCfg = QspiRegRead32 (QspiHost, QSPI_RSER);
    QspiRegwrite32 (QspiHost, QSPI_RSER, 0);

    SFA1AD = QspiRegRead32 (QspiHost, QSPI_SFA1AD);
    SFA2AD = QspiRegRead32 (QspiHost, QSPI_SFA2AD);
    SFB1AD = QspiRegRead32 (QspiHost, QSPI_SFB1AD);
    SFB2AD = QspiRegRead32 (QspiHost, QSPI_SFB2AD);

    /* update flash memory map with EDK configuration */
    QspiRegwrite32 (QspiHost, QSPI_SFA1AD, QSPI_FLASH_A1_TOP & 0xfffffc00);
    QspiRegwrite32 (QspiHost, QSPI_SFA2AD, QSPI_FLASH_A2_TOP & 0xfffffc00);
    QspiRegwrite32 (QspiHost, QSPI_SFB1AD, QSPI_FLASH_B1_TOP & 0xfffffc00);
    QspiRegwrite32 (QspiHost, QSPI_SFB2AD, QSPI_FLASH_B2_TOP & 0xfffffc00);
  }

  /* clear TX/RX buffer before transaction */
  Val  = QspiRegRead32 (QspiHost, QSPI_MCR);
  Val |= QSPI_MCR_CLR_TXF | QSPI_MCR_CLR_RXF;
  QspiRegwrite32 (QspiHost, QSPI_MCR, Val);

  /*
   * reset the sequence pointers whenever the sequence ID is changed by
   * updating the SEDID filed in QSPI_IPCR OR QSPI_BFGENCR.
   */
  Val  = QspiRegRead32 (QspiHost, QSPI_SPTRCLR);
  Val |= QSPI_SPTRCLR_IPPTRC | QSPI_SPTRCLR_BFPTRC;
  QspiRegwrite32 (QspiHost, QSPI_SPTRCLR, Val);

  /* set the flash address into the QSPI_SFAR */
  if (Op->Addr.Nbytes) {
    Address = Op->Addr.Val;
  } else {
    Address = 0;
  }

  Address += QspiHost->CsAddr[Slave->Cs + QSPI_CS_A1];
  QspiWriteSfar (QspiHost, Address);

  /* clear QSPI_FR before trigger LUT command */
  Val = QspiRegRead32 (QspiHost, QSPI_FR);
  if (Val) {
    QspiRegwrite32 (QspiHost, QSPI_FR, Val);
  }

  /*
   * read page command 13h must be done by IP command.
   * read from cache through the AHB bus by accessing the mapped memory.
   * In all other cases we use IP commands to access the flash.
   */
  if ((SPI_XFER_RX_DATA == Op->Data.Dir) && QspiHost->XipRead &&
      IsReadFromCacheOpcode (Opcode))
  {
    QspiConfigLookupTable (QspiHost, Op, SEQID_LUT_AHBREAD_ID);
    QspiXipRead (Op->Data.Buf, Address, Op->Data.Nbytes);
  } else {
    /* IP command */
    QspiConfigLookupTable (QspiHost, Op, SEQID_LUT_SHARED_ID);
    if (Op->Data.Nbytes && (SPI_XFER_TX_DATA == Op->Data.Dir)) {
      QspiFillTxFifo (QspiHost, Op->Data.Buf, Op->Data.Nbytes);
    }

    Status = QspiStartTransfer (QspiHost, Op->Data.Nbytes, SEQID_LUT_SHARED_ID);

    if ((EFI_SUCCESS == Status) && Op->Data.Nbytes
        && (SPI_XFER_RX_DATA == Op->Data.Dir))
    {
      QspiReadRxFifo (QspiHost, Op->Data.Buf, Op->Data.Nbytes);
    }
  }

  /* invalidate the data in the AHB buffer. */
  QspiReset (QspiHost);

  // restore QSPI controller configuration to kernel state
  if (EfiAtRuntime ()) {
    QspiRegwrite32 (QspiHost, QSPI_SFA1AD, SFA1AD);
    QspiRegwrite32 (QspiHost, QSPI_SFA2AD, SFA2AD);
    QspiRegwrite32 (QspiHost, QSPI_SFB1AD, SFB1AD);
    QspiRegwrite32 (QspiHost, QSPI_SFB2AD, SFB2AD);

    /* clear all interrupt status */
    QspiRegwrite32 (QspiHost, QSPI_FR, 0xffffffff);
    /* restore interrupt configuration */
    QspiRegwrite32 (QspiHost, QSPI_RSER, IsrCfg);
  }

  return Status;
}

EFI_STATUS
EFIAPI
SpiInit (
  IN SPI_MASTER_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
SpiConfigRuntime (
  IN SPI_MASTER_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

/**
  Fixup internal data so that EFI can be call in virtual mode.
  Call the passed in Child Notify event and convert any pointers in
  lib to virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
STATIC
VOID
EFIAPI
SpiMasterVirtualNotifyEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EfiConvertPointer (0x0, (VOID **)&mSpiMasterInstance->QspiHost.RegisterBase);
  EfiConvertPointer (0x0, (VOID **)&mSpiMasterInstance);
  return;
}

STATIC
EFI_STATUS
SpiMasterInitProtocol (
  IN SPI_MASTER_PROTOCOL  *SpiMaster
  )
{
  SpiMaster->Init          = SpiInit;
  SpiMaster->AjustOPSize   = SpiAjustOPSize;
  SpiMaster->Transfer      = SpiTransfer;
  SpiMaster->ConfigRuntime = SpiConfigRuntime;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpacemitQspiEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  mSpiMasterInstance = AllocateRuntimeZeroPool (sizeof (SPI_MASTER_INSTANCE));
  if (mSpiMasterInstance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  EfiInitializeLock (&mSpiMasterInstance->Lock, TPL_NOTIFY);

  SpiMasterInitProtocol (&mSpiMasterInstance->SpiMaster);

  mSpiMasterInstance->Signature = SPI_MASTER_SIGNATURE;

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconClockCtrlProtocolGuid,
                                NULL,
                                (VOID *)&mSpiMasterInstance->ClockCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __FUNCTION__, __LINE__, Status));
  } else {
    Status = gBS->LocateProtocol (
                                  &gSpacemitSiliconPinCtrlProtocolGuid,
                                  NULL,
                                  (VOID *)&mSpiMasterInstance->PinCtrlProtocol
                                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __FUNCTION__, __LINE__, Status));
    }
  }

  if (!EFI_ERROR (Status)) {
    SpiHostControllerInit (&mSpiMasterInstance->QspiHost);

    // Install protocols
    Status = gBS->InstallMultipleProtocolInterfaces (
                                                     &(mSpiMasterInstance->Handle),
                                                     &gSpacemitSpiMasterProtocolGuid,
                                                     &(mSpiMasterInstance->SpiMaster),
                                                     NULL
                                                     );
  }

  if (EFI_ERROR (Status)) {
    FreePool (mSpiMasterInstance);
    return EFI_DEVICE_ERROR;
  }

  //
  // Register for the virtual address change event
  //
  Status = gBS->CreateEventEx (
                               EVT_NOTIFY_SIGNAL,
                               TPL_NOTIFY,
                               SpiMasterVirtualNotifyEvent,
                               NULL,
                               &gEfiEventVirtualAddressChangeGuid,
                               &mSpiMasterVirtualAddrChangeEvent
                               );

  return Status;
}
