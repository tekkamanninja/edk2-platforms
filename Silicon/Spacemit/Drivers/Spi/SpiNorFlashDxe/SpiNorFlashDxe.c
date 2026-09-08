/** @file
  Spi nor flash driver implementation.

  Copyright (C) 2016 Marvell International Ltd.
  Copyright (c) 2020, Arm Limited. All rights reserved.<BR>
  Copyright (c) 2024, Spacemit Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "SpiNorFlashDxe.h"

STATIC EFI_EVENT     mSpiFlashVirtualAddrChangeEvent;
SPI_FLASH_INSTANCE   *mSpiFlashInstance;
SPI_MASTER_PROTOCOL  *mSpiMasterProtocol;

EFI_STATUS
EFIAPI
SpiFlashReadWriteRegister (
  IN     SPI_DEVICE       *Slave,
  IN     UINT8            Direction,
  IN     UINT8            Command,
  IN     UINT32           DataByteCount,  OPTIONAL
  IN OUT VOID             *Buffer         OPTIONAL
  )
{
  EFI_STATUS   Status;
  SPI_XFER_OP  Op;

  if (Slave == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (((Direction == SPI_XFER_RX_DATA) ||
       (Direction == SPI_XFER_TX_DATA)) &&
      (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Buffer != NULL) && (DataByteCount == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Op, sizeof (SPI_XFER_OP));
  Op.Cmd.BusWidth = 1;
  Op.Cmd.Nbytes   = 1;
  Op.Cmd.Opcode   = Command;

  if ((Direction == SPI_XFER_RX_DATA) ||
      (Direction == SPI_XFER_TX_DATA)) {
    Op.Data.BusWidth  = 1;
    Op.Data.Dir       = Direction;
    Op.Data.Nbytes    = DataByteCount;
    Op.Data.Buf       = Buffer;
  }

  Status = mSpiMasterProtocol->Transfer (mSpiMasterProtocol, Slave, &Op);
  return Status;
}

EFI_STATUS
EFIAPI
SpiFlashPollStatus (
  IN SPI_DEVICE   *Slave,
  IN UINT8        Mask,
  IN UINT8        Value,
  IN UINTN        TimeoutMs
  )
{
  EFI_STATUS      Status;
  UINT8           ReadStatus;
  UINTN           TimeoutCount;

  ReadStatus   = 0;
  TimeoutCount = TimeoutMs;

  while (TimeoutCount--) {
    Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_RX_DATA, CMD_READ_STATUS, sizeof (ReadStatus), &ReadStatus);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (Value == (ReadStatus & Mask)) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (1000);
  }

  return EFI_TIMEOUT;
}

EFI_STATUS
EFIAPI
SpiFlashWaitNotWip (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;

  Status = SpiFlashPollStatus (Slave, STATUS_REG_POLL_WIP, 0, POLL_STATUS_TIMEOUT);
  return Status;
}

EFI_STATUS
EFIAPI
SpiFlashWaitWelNotWip (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;

  Status = SpiFlashPollStatus (Slave, STATUS_REG_POLL_WEL | STATUS_REG_POLL_WIP, STATUS_REG_POLL_WEL, POLL_STATUS_TIMEOUT);
  return Status;
}

EFI_STATUS
EFIAPI
SpiFlashWaitNotWelNotWip (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;

  Status = SpiFlashPollStatus (Slave, STATUS_REG_POLL_WEL | STATUS_REG_POLL_WIP, 0, POLL_STATUS_TIMEOUT);
  return Status;
}

VOID
EFIAPI
SpiFlashDumpStatusRegisters (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;
  UINTN           Index;
  UINT8           ReadStatus;
  struct {
    UINT8         Command;
    CHAR16        *Name;
  } StatusRegMap[] = {
    { CMD_READ_STATUS,  L"Read status register 1" },
    { CMD_READ_STATUS2, L"Read status register 2" },
    { CMD_READ_STATUS3, L"Read status register 3" }
  };

  for (Index = 0; Index < ARRAY_SIZE (StatusRegMap); Index++) {
    ReadStatus = 0;

    Status = SpiFlashWaitNotWip (Slave);
    ASSERT_EFI_ERROR (Status);

    Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_RX_DATA, StatusRegMap[Index].Command, sizeof (ReadStatus), &ReadStatus);
    DEBUG ((DEBUG_VERBOSE, "%a(): %s Status : %r.\n", __func__, StatusRegMap[Index].Name, Status));
    if (!EFI_ERROR (Status)) {
      DEBUG ((DEBUG_VERBOSE, "%a(): %s Value : 0x%02X.\n", __func__, StatusRegMap[Index].Name, ReadStatus));
    }
  }

  return;
}

EFI_STATUS
EFIAPI
SpiFlashSoftReset (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;

  Status = SpiFlashWaitNotWip (Slave);
  ASSERT_EFI_ERROR (Status);

  Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_NO_DATA, CMD_RESET_ENABLE, 0, NULL);
  if (!EFI_ERROR (Status)) {
    Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_NO_DATA, CMD_RESET, 0, NULL);
  }

  if (!EFI_ERROR (Status)) {
    //
    // Software Reset is not instant.
    // So wait for 200us just to make sure software reset valid.
    // The delay may be adjusted by different flash.
    //
    MicroSecondDelay (200);
  }

  DEBUG ((DEBUG_VERBOSE, "%a(): Spi flash soft reset Status = %r.\n", __func__, Status));

  return Status;
}

EFI_STATUS
EFIAPI
SpiFlashWriteEnable (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;

  Status = SpiFlashWaitNotWelNotWip (Slave);
  ASSERT_EFI_ERROR (Status);

  Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_NO_DATA, CMD_WRITE_ENABLE, 0, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Spi flash write enable failed. Status = %r.\n", __func__, Status));
    return Status;
  }

  Status = SpiFlashWaitWelNotWip (Slave);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashSet4ByteMode (
  IN SPI_DEVICE   *Slave,
  IN BOOLEAN      Enable
  )
{
  EFI_STATUS      Status;
  UINT8           StatusData;

  Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_RX_DATA, CMD_READ_STATUS3, sizeof (StatusData), &StatusData);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((StatusData & STATUS_REG_ADS) == ((UINT8) Enable)) {
    return EFI_SUCCESS;
  }

  Status = SpiFlashWaitNotWip (Slave);
  ASSERT_EFI_ERROR (Status);

  Status = SpiFlashReadWriteRegister (
                          Slave,
                          SPI_XFER_NO_DATA,
                          Enable ? CMD_4B_ADDR_ENABLE : CMD_4B_ADDR_DISABLE,
                          0,
                          NULL
                          );
  return Status;
}

EFI_STATUS
EFIAPI
SpiFlashReadExtendedAddress (
  IN     SPI_DEVICE   *Slave,
  IN OUT UINT8        *ExtAddr
  )
{
  EFI_STATUS      Status;

  if (ExtAddr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ExtAddr = 0;

  Status = SpiFlashWaitNotWip (Slave);
  ASSERT_EFI_ERROR (Status);

  Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_RX_DATA, CMD_READ_EXT_ADDR, sizeof (*ExtAddr), ExtAddr);
  DEBUG ((DEBUG_VERBOSE, "%a(): Read extened address Status = %r, extened address = 0x%02x.\n", __func__, Status, *ExtAddr));
  return Status;
}

EFI_STATUS
EFIAPI
SpiFlashSetExtendedAddress (
  IN SPI_DEVICE   *Slave,
  IN UINT32       Address
  )
{
  EFI_STATUS      Status;
  static UINT8    ExtAddr = MAX_UINT8;

  if (ExtAddr == MAX_UINT8) {
    Status = SpiFlashReadExtendedAddress (Slave, &ExtAddr);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (ExtAddr == ((UINT8) (Address >> 24))) {
    return EFI_SUCCESS;
  }

  ExtAddr = (UINT8) (Address >> 24);

  Status = SpiFlashWriteEnable (Slave);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_TX_DATA, CMD_WRITE_EXT_ADDR, sizeof (ExtAddr), &ExtAddr);
  DEBUG ((DEBUG_VERBOSE, "%a(): Set extened address 0x%08X Status = %r.\n", __func__, Address, Status));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SpiFlashWaitNotWelNotWip (Slave);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashTransferData (
  IN     SPI_DEVICE       *Slave,
  IN     UINT8            Direction,
  IN     UINT8            Command,
  IN     UINT32           Address,
  IN     UINT32           DataByteCount,  OPTIONAL
  IN OUT VOID             *Buffer         OPTIONAL
  )
{
  EFI_STATUS              Status;
  UINT32                  CurrentAddress;
  INT32                   CurrentLength;
  UINT8                   *CurrentBuffer;
  SPI_XFER_OP             Op;

  if (Slave == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (((Direction == SPI_XFER_RX_DATA) ||
       (Direction == SPI_XFER_TX_DATA)) &&
      (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Buffer != NULL) && (DataByteCount == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Op, sizeof (SPI_XFER_OP));
  Op.Cmd.Nbytes    = 1;
  Op.Cmd.BusWidth  = 1;
  Op.Cmd.Opcode    = Command;

  Op.Addr.Nbytes   = Slave->AddrSize;
  Op.Addr.BusWidth = 1;
  Op.Addr.Val      = Address;

  if (Direction == SPI_XFER_NO_DATA) {
    if (Slave->AddrSize == 3 && Slave->FlashSize > SIZE_16MB) {
      Status = SpiFlashSetExtendedAddress (Slave, Address);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    Status = SpiFlashWriteEnable (Slave);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = mSpiMasterProtocol->Transfer (mSpiMasterProtocol, Slave, &Op);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = SpiFlashWaitNotWelNotWip (Slave);
    ASSERT_EFI_ERROR (Status);

    return EFI_SUCCESS;
  }

  if (Direction == SPI_XFER_RX_DATA) {
    Op.Dummy.Nbytes   = 1;
    Op.Dummy.BusWidth = 1;
  }

  Op.Data.BusWidth = 1;
  Op.Data.Dir      = Direction;

  CurrentAddress   = Address;
  CurrentLength    = DataByteCount;
  CurrentBuffer    = Buffer;
  while (CurrentLength > 0) {
    Op.Addr.Val     = CurrentAddress;
    Op.Data.Nbytes  = CurrentLength;
    Op.Data.Buf     = CurrentBuffer;

    if (mSpiMasterProtocol->AjustOPSize) {
      Status = mSpiMasterProtocol->AjustOPSize (mSpiMasterProtocol, &Op);
      if (EFI_ERROR (Status)) {
        break;
      }
    }

    if (Slave->AddrSize == 3 && Slave->FlashSize > SIZE_16MB) {
      Status = SpiFlashSetExtendedAddress (Slave, Address);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    if (Direction == SPI_XFER_TX_DATA) {
      Status = SpiFlashWriteEnable (Slave);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    } else {
      Status = SpiFlashWaitNotWip (Slave);
      ASSERT_EFI_ERROR (Status);
    }

    Status = mSpiMasterProtocol->Transfer (mSpiMasterProtocol, Slave, &Op);
    if (EFI_ERROR (Status)) {
      break;
    }

    if (Direction == SPI_XFER_TX_DATA) {
      Status = SpiFlashWaitNotWelNotWip (Slave);
      ASSERT_EFI_ERROR (Status);
    } else {
      Status = SpiFlashWaitNotWip (Slave);
      ASSERT_EFI_ERROR (Status);
    }

    CurrentAddress += Op.Data.Nbytes;
    CurrentLength  -= Op.Data.Nbytes;
    CurrentBuffer  += Op.Data.Nbytes;
  }

  return Status;
}

EFI_STATUS
EFIAPI
SpiFlashErase (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINTN                Address,
  IN UINTN                DataByteCount
  )
{
  EFI_STATUS              Status;
  SPI_FLASH_INSTANCE      *Instance;
  SPI_DEVICE              *Slave;
  NOR_FLASH_INFO          *Info;
  UINT32                  EraseAddr, EraseLength;
  UINTN                   EraseSize;
  UINT8                   Cmd;

  Instance  = SPI_FLASH_INSTANCE_FROM_THIS (This);
  Slave     = &(Instance->SpiDev);
  Info      = (NOR_FLASH_INFO *) (Slave->Info);

  if ((Address >= Slave->FlashSize) ||
      (DataByteCount > Slave->FlashSize - Address)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Info->Flags & NOR_FLASH_ERASE_4K) {
    Cmd       = (Slave->AddrSize == 4) ? CMD_4B_ERASE_4K : CMD_ERASE_4K;
    EraseSize = SIZE_4KB;
  } else if (Info->Flags & NOR_FLASH_ERASE_32K) {
    Cmd       = (Slave->AddrSize == 4) ? CMD_4B_ERASE_32K : CMD_ERASE_32K;
    EraseSize = SIZE_32KB;
  } else {
    Cmd       = (Slave->AddrSize == 4) ? CMD_4B_ERASE_64K : CMD_ERASE_64K;
    EraseSize = Info->SectorSize;
  }

  // Check input parameters
  if (Address % EraseSize || DataByteCount % EraseSize) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Either erase address or length is not multiple of erase size.\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  EraseAddr   = Address;
  EraseLength = 0;
  while (EraseLength < DataByteCount) {
    // Programm proper erase address
    Status = SpiFlashTransferData (
                          Slave,
                          SPI_XFER_NO_DATA,
                          Cmd,
                          EraseAddr,
                          0,
                          NULL
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_VERBOSE, "%a(): Spi flash erase failed at address 0x%X. Status = %r.\n", __func__, EraseAddr, Status));
      return Status;
    }

    EraseAddr   += EraseSize;
    EraseLength += EraseSize;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashRead (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINT32               Address,
  IN UINTN                DataByteCount,
  IN VOID                 *Buffer
  )
{
  EFI_STATUS              Status;
  SPI_FLASH_INSTANCE      *Instance;
  SPI_DEVICE              *Slave;

  Instance  = SPI_FLASH_INSTANCE_FROM_THIS (This);
  Slave     = &(Instance->SpiDev);

  if ((Buffer == NULL) ||
      (Address >= Slave->FlashSize) ||
      (DataByteCount > Slave->FlashSize - Address)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = SpiFlashTransferData (
                          Slave,
                          SPI_XFER_RX_DATA,
                          (Slave->AddrSize == 4) ? CMD_4B_READ_ARRAY_FAST : CMD_READ_ARRAY_FAST,
                          Address,
                          DataByteCount,
                          Buffer
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Spi flash read failed at address 0x%X. Status = %r.\n", __func__, Address, Status));
  }

  return Status;
}

EFI_STATUS
EFIAPI
SpiFlashWrite (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINT32               Address,
  IN UINTN                DataByteCount,
  IN VOID                 *Buffer
  )
{
  EFI_STATUS              Status;
  SPI_FLASH_INSTANCE      *Instance;
  SPI_DEVICE              *Slave;
  NOR_FLASH_INFO          *Info;
  UINTN                   PageSize;
  UINTN                   ByteAddr, ChunkLength, ActualIndex;
  UINT32                  WriteAddr;

  Instance  = SPI_FLASH_INSTANCE_FROM_THIS (This);
  Slave     = &(Instance->SpiDev);
  Info      = (NOR_FLASH_INFO *) (Slave->Info);
  PageSize  = Info->PageSize;

  if ((Buffer == NULL) ||
      (DataByteCount == 0) ||
      (Address >= Slave->FlashSize) ||
      (DataByteCount > Slave->FlashSize - Address)) {
    return EFI_INVALID_PARAMETER;
  }

  WriteAddr = Address;
  for (ActualIndex = 0; ActualIndex < DataByteCount; ActualIndex += ChunkLength) {
    ByteAddr    = WriteAddr % PageSize;
    ChunkLength = MIN (DataByteCount - ActualIndex, (UINT64)(PageSize - ByteAddr));

    // Program proper write address and write data
    Status = SpiFlashTransferData (
                          Slave,
                          SPI_XFER_TX_DATA,
                          (Slave->AddrSize == 4) ? CMD_4B_PAGE_PROGRAM : CMD_PAGE_PROGRAM,
                          WriteAddr,
                          ChunkLength,
                          (VOID *)(((UINTN) Buffer) + ActualIndex)
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_VERBOSE, "%a(): Spi flash write failed at address 0x%X. Status = %r.\n", __func__, WriteAddr, Status));
      return Status;
    }

    WriteAddr += ChunkLength;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashUpdateBlock (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINT32               Offset,
  IN UINTN                ToUpdate,
  IN UINT8                *Buf,
  IN UINT8                *TmpBuf,
  IN UINTN                EraseSize
  )
{
  EFI_STATUS              Status;
  SPI_FLASH_INSTANCE      *Instance;
  SPI_DEVICE              *Slave;

  Instance = SPI_FLASH_INSTANCE_FROM_THIS (This);
  Slave    = &(Instance->SpiDev);

  // Read backup
  Status = SpiFlashRead (This, Offset, EraseSize, TmpBuf);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Error while reading old data.\n", __func__));
    return Status;
  }

  // Erase entire sector
  Status = SpiFlashErase (This, Offset, EraseSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Error while erasing block.\n", __func__));
    return Status;
  }

  // Write new data
  Status = SpiFlashWrite (This, Offset, ToUpdate, Buf);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Error while writing new data.\n", __func__));
    return Status;
  }

  // Write backup
  if (ToUpdate != EraseSize) {
    Status = SpiFlashWrite (
                            This,
                            Offset + ToUpdate,
                            EraseSize - ToUpdate,
                            &TmpBuf[ToUpdate]
                            );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_VERBOSE, "%a(): Error while writing backup.\n", __func__));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashUpdate (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINT32               Address,
  IN UINTN                DataByteCount,
  IN UINT8                *Buffer
  )
{
  EFI_STATUS              Status;
  SPI_FLASH_INSTANCE      *Instance;
  SPI_DEVICE              *Slave;
  NOR_FLASH_INFO          *Info;
  UINT64                  SectorSize, ToUpdate, Scale;
  UINT8                   *TmpBuf, *End;

  Instance   = SPI_FLASH_INSTANCE_FROM_THIS (This);
  Slave      = &(Instance->SpiDev);
  Info       = (NOR_FLASH_INFO *) (Slave->Info);
  SectorSize = Info->SectorSize;
  Scale      = 1;
  End        = Buffer + DataByteCount;

  TmpBuf = (UINT8 *) AllocateZeroPool (SectorSize);
  if (TmpBuf == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (End - Buffer >= 200) {
    Scale = (End - Buffer) / 100;
  }

  for ( ; Buffer < End; Buffer += ToUpdate, Address += ToUpdate) {
    ToUpdate = MIN ((UINT64)(End - Buffer), SectorSize);
    Print (L"   \rUpdating, %d%%", 100 - (End - Buffer) / Scale);

    Status = SpiFlashUpdateBlock (This, Address, ToUpdate, Buffer, TmpBuf, SectorSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_VERBOSE, "%a(): Error while updating.\n", __func__));
      return Status;
    }
  }

  Print (L"\n");
  FreePool (TmpBuf);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashUpdateWithProgress (
  IN SPI_FLASH_PROTOCOL                             *This,
  IN UINT32                                         Address,
  IN UINTN                                          DataByteCount,
  IN UINT8                                          *Buffer,
  IN EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  Progress,         OPTIONAL
  IN UINTN                                          StartPercentage,
  IN UINTN                                          EndPercentage
  )
{
  EFI_STATUS              Status;
  SPI_FLASH_INSTANCE      *Instance;
  SPI_DEVICE              *Slave;
  NOR_FLASH_INFO          *Info;
  UINTN                   SectorSize;
  UINTN                   SectorNum;
  UINTN                   ToUpdate;
  UINTN                   Index;
  UINT8                   *TmpBuf;

  Instance   = SPI_FLASH_INSTANCE_FROM_THIS (This);
  Slave      = &(Instance->SpiDev);
  Info       = (NOR_FLASH_INFO *) (Slave->Info);

  SectorSize = Info->SectorSize;
  SectorNum  = (DataByteCount + SectorSize - 1) / SectorSize;
  ToUpdate   = SectorSize;

  TmpBuf = (UINT8 *) AllocateZeroPool (SectorSize);
  if (TmpBuf == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < SectorNum; Index++) {
    if (Progress != NULL) {
      Progress (
              StartPercentage +
              ((Index * (EndPercentage - StartPercentage)) / SectorNum)
              );
    }

    // In the last chunk update only an actual number of remaining bytes.
    if (Index + 1 == SectorNum) {
      ToUpdate = DataByteCount - Index * SectorSize;
    }

    Status = SpiFlashUpdateBlock (
                                This,
                                Address + Index * SectorSize,
                                ToUpdate,
                                Buffer + Index * SectorSize,
                                TmpBuf,
                                SectorSize
                                );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_VERBOSE, "%a(): Error while updating.\n", __func__));
      return Status;
    }
  }

  FreePool (TmpBuf);

  if (Progress != NULL) {
    Progress (EndPercentage);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashReadId (
  IN SPI_FLASH_PROTOCOL   *This,
  IN BOOLEAN              UseInRuntime
  )
{
  EFI_STATUS              Status;
  SPI_FLASH_INSTANCE      *Instance;
  SPI_DEVICE              *Slave;
  NOR_FLASH_INFO          *Info;
  UINT8                   Id[NOR_FLASH_MAX_ID_LEN];

  Instance = SPI_FLASH_INSTANCE_FROM_THIS (This);
  Slave    = &(Instance->SpiDev);

  Status = SpiFlashWaitNotWip (Slave);
  ASSERT_EFI_ERROR (Status);

  Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_RX_DATA, CMD_READ_ID, sizeof (Id), Id);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Spi flash read id failed. Status = %r.\n", __func__, Status));
    return Status;
  }

  Status = NorFlashGetInfo (Id, &Info, UseInRuntime);
  if (EFI_ERROR (Status)) {
    DEBUG ((
        DEBUG_VERBOSE,
        "%a(): Unrecognized JEDEC Id bytes: 0x%02x%02x%02x\n",
        __func__,
        Id[0],
        Id[1],
        Id[2]
        ));
    return Status;
  }

  NorFlashPrintInfo (Info);

  Slave->Info = (VOID *) Info;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashInit (
  IN SPI_FLASH_PROTOCOL   *This,
  IN BOOLEAN              UseInRuntime
  )
{
  EFI_STATUS              Status;
  SPI_FLASH_INSTANCE      *Instance;
  SPI_DEVICE              *Slave;
  NOR_FLASH_INFO          *Info;
  UINT8                   StatusData;
  UINT8                   ManufacturerId;

  Instance    = SPI_FLASH_INSTANCE_FROM_THIS (This);
  Slave       = &(Instance->SpiDev);
  StatusData  = 0;

  Slave->Cs   = 0;
  Slave->Mode = SPI_MODE0;

  if (FeaturePcdGet (PcdSpiFlashSoftReset)) {
    Status = SpiFlashSoftReset (Slave);
    ASSERT_EFI_ERROR (Status);
  }

  // Read SPI flash ID and update spiflash info
  Status = SpiFlashReadId (This, UseInRuntime);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  Info             = (NOR_FLASH_INFO *) (Slave->Info);
  Slave->PageSize  = Info->PageSize;
  Slave->FlashSize = (UINTN) (Info->SectorSize * Info->SectorCount);

  Slave->AddrSize = 3;
  if (Info->Flags & NOR_FLASH_4B_ADDR) {
    Slave->AddrSize = 4;
  }

  if (Slave->AddrSize == 4) {
    Status = SpiFlashSet4ByteMode (Slave, TRUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_VERBOSE, "%a(): Set 4B address faield. Status = %r.\n", __func__, Status));
      return Status;
    }
  }

  Status = SpiFlashWriteEnable (Slave);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ManufacturerId = Info->Id[0];

  switch (ManufacturerId) {
    case MANUFACTURER_ID_MACRONIX:
      //
      // For Macronix (ID 0xC2), set QE bit (BIT6) in status register
      //
      StatusData |= SR_QUAD_EN_MX;
      break;
    case MANUFACTURER_ID_SPANSION:
      //
      // For Spansion (ID 0x01), set QE bit (BIT1) in configuration register
      //
      StatusData |= CR_QUAD_EN_SPAN;
      break;
    case MANUFACTURER_ID_WINBOND:
      //
      // For Winbond (ID 0xEF), set QE bit (BIT1) in configuration register
      //
      StatusData |= CR_QUAD_EN_WINB;
      break;
    default:
      break;
  }

  Status = SpiFlashReadWriteRegister (Slave, SPI_XFER_TX_DATA, CMD_WRITE_STATUS_REG, sizeof (StatusData), &StatusData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Write status-1 register failed. Status = %r.\n", __func__, Status));
    return Status;
  }

  Status = SpiFlashWaitNotWelNotWip (Slave);
  ASSERT_EFI_ERROR (Status);

  if (UseInRuntime) {
    mSpiMasterProtocol->ConfigRuntime (mSpiMasterProtocol);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpiFlashInitProtocol (
  IN SPI_FLASH_PROTOCOL  *SpiFlashProtocol
  )
{
  SpiFlashProtocol->Init               = SpiFlashInit;
  SpiFlashProtocol->ReadId             = SpiFlashReadId;
  SpiFlashProtocol->Read               = SpiFlashRead;
  SpiFlashProtocol->Write              = SpiFlashWrite;
  SpiFlashProtocol->Erase              = SpiFlashErase;
  SpiFlashProtocol->Update             = SpiFlashUpdate;
  SpiFlashProtocol->UpdateWithProgress = SpiFlashUpdateWithProgress;

  return EFI_SUCCESS;
}

/**
  Fixup internal data so that EFI can be call in virtual mode.
  Call the passed in Child Notify event and convert any pointers in
  lib to virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
SpiFlashVirtualNotifyEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  //
  // Convert mSpiMasterProtocol callbacks in SpiFlashErase and
  // SpiFlashWrite required by runtime variable support.
  //
  EfiConvertPointer (0x0, (VOID **) &mSpiMasterProtocol->AjustOPSize);
  EfiConvertPointer (0x0, (VOID **) &mSpiMasterProtocol->Transfer);
  EfiConvertPointer (0x0, (VOID **) &mSpiMasterProtocol);

  EfiConvertPointer (0x0, (VOID **) &mSpiFlashInstance->SpiDev.Info);
  EfiConvertPointer (0x0, (VOID **) &mSpiFlashInstance->SpiDev);
  EfiConvertPointer (0x0, (VOID **) &mSpiFlashInstance);

  return;
}

EFI_STATUS
EFIAPI
SpiFlashEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (
                              &gSpacemitSpiMasterProtocolGuid,
                              NULL,
                              (VOID **)&mSpiMasterProtocol
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Locate SPI Master protocol failed. Status = %r.\n", __func__, Status));
    return EFI_DEVICE_ERROR;
  }

  mSpiFlashInstance = AllocateRuntimeZeroPool (sizeof (SPI_FLASH_INSTANCE));
  if (mSpiFlashInstance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SpiFlashInitProtocol (&mSpiFlashInstance->SpiFlashProtocol);

  mSpiFlashInstance->Signature = SPI_FLASH_SIGNATURE;

  Status = gBS->InstallMultipleProtocolInterfaces (
                              &(mSpiFlashInstance->Handle),
                              &gSpacemitSpiFlashProtocolGuid,
                              &(mSpiFlashInstance->SpiFlashProtocol),
                              NULL
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Install SPI flash protocol failed. Status = %r.\n", __func__, Status));
    FreePool (mSpiFlashInstance);
    return Status;
  }

  //
  // Register for the virtual address change event
  //
  mSpiFlashVirtualAddrChangeEvent = NULL;
  Status = gBS->CreateEventEx (
                              EVT_NOTIFY_SIGNAL,
                              TPL_NOTIFY,
                              SpiFlashVirtualNotifyEvent,
                              NULL,
                              &gEfiEventVirtualAddressChangeGuid,
                              &mSpiFlashVirtualAddrChangeEvent
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Failed to register VA change event. Status = %r.\n", __func__, Status));
    gBS->UninstallMultipleProtocolInterfaces (
                              &(mSpiFlashInstance->Handle),
                              &gSpacemitSpiFlashProtocolGuid,
                              NULL
                              );
    FreePool (mSpiFlashInstance);
    return Status;
  }

  return EFI_SUCCESS;
}
