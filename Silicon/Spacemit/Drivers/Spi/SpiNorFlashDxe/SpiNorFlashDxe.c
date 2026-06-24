/** @file
  Spi nor flash driver implementation.

  Copyright (C) 2016 Marvell International Ltd.
  Copyright (c) 2020, Arm Limited. All rights reserved.<BR>
  Copyright (c) 2024, Spacemit Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "SpiNorFlashDxe.h"

STATIC EFI_EVENT            mSpiFlashVirtualAddrChangeEvent;
STATIC SPI_FLASH_INSTANCE   *mSpiFlashInstance;
STATIC SPI_MASTER_PROTOCOL  *mSpiMasterProtocol;

/**
  Implement read or write operation to the SPI flash device.

  @param[in]      Slave          Pointer to the SPI device instance.
  @param[in]      Direction      Transfer direction.
  @param[in]      Command        Command opcode to send.
  @param[in]      DataByteCount  Number of data bytes to transfer.
                                 It should be Zero when SPI_XFER_NO_DATA
  @param[in, out] Buffer         Pointer to data buffer for the transmission.
                                 It should be NULL when SPI_XFER_NO_DATA.

  @retval EFI_SUCCESS             Command executed successfully.
  @retval EFI_INVALID_PARAMETER   Slave is NULL, or Direction/Buffer/DataByteCount
                                  mismatch.
  @retval Others                  SPI master transfer returned an error.

**/
STATIC
EFI_STATUS
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

/**
  Poll the SPI flash status register until a specific condition is met or
  timeout occurs.

  Repeatedly reads the status register and checks if the masked bits match the
  expected value.

  @param[in] Slave      Pointer to the SPI device instance.
  @param[in] Mask       Bitmask to apply to the status register value.
  @param[in] Value      Expected value after masking.
  @param[in] TimeoutMs  Timeout duration in milliseconds.

  @retval EFI_SUCCESS  The expected condition was met before timeout.
  @retval EFI_TIMEOUT  Timeout occurred before the condition was met.

**/
STATIC
EFI_STATUS
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

/**
  Wait for the SPI flash Write In Progress (WIP) bit to clear.

  Polls the status register until the WIP bit is cleared, indicating that
  any pending write or erase operation has completed.

  @param[in] Slave  Pointer to the SPI device instance.

  @retval EFI_SUCCESS  WIP bit cleared successfully.
  @retval EFI_TIMEOUT  Timeout occurred while waiting for WIP to clear.

**/
STATIC
EFI_STATUS
SpiFlashWaitNotWip (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;

  Status = SpiFlashPollStatus (Slave, STATUS_REG_POLL_WIP, 0, POLL_STATUS_TIMEOUT);
  return Status;
}

/**
  Wait for the SPI flash Write Enable Latch to be set and WIP to clear.

  Polls the status register until the WEL bit is set and the WIP bit is cleared,
  indicating that the device is ready for a write or erase operation.

  @param[in] Slave  Pointer to the SPI device instance.

  @retval EFI_SUCCESS  WEL is set and WIP is cleared.
  @retval EFI_TIMEOUT  Timeout occurred while waiting.

**/
STATIC
EFI_STATUS
SpiFlashWaitWelNotWip (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;

  Status = SpiFlashPollStatus (Slave, STATUS_REG_POLL_WEL | STATUS_REG_POLL_WIP, STATUS_REG_POLL_WEL, POLL_STATUS_TIMEOUT);
  return Status;
}

/**
  Wait for the SPI flash Write Enable Latch and Write In Progress bits to clear.

  Polls the status register until both the WEL and WIP bits are cleared,
  indicating that a write or erase operation has completed and the write
  enable has been automatically reset.

  @param[in] Slave  Pointer to the SPI device instance.

  @retval EFI_SUCCESS  Both WEL and WIP bits are cleared.
  @retval EFI_TIMEOUT  Timeout occurred while waiting.

**/
STATIC
EFI_STATUS
SpiFlashWaitNotWelNotWip (
  IN SPI_DEVICE   *Slave
  )
{
  EFI_STATUS      Status;

  Status = SpiFlashPollStatus (Slave, STATUS_REG_POLL_WEL | STATUS_REG_POLL_WIP, 0, POLL_STATUS_TIMEOUT);
  return Status;
}

/**
  Dump the contents of the SPI flash status registers.

  Reads and prints the three status registers (Status Register 1, 2, and 3).

  @param[in] Slave  Pointer to the SPI device instance.

**/
STATIC
VOID
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

/**
  Perform a software reset of the SPI flash device.

  @param[in] Slave  Pointer to the SPI device instance.

  @retval EFI_SUCCESS  Software reset completed successfully.
  @retval Others       Reset command transmission failed.

**/
STATIC
EFI_STATUS
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

/**
  Enable write operations on the SPI flash device.

  @param[in] Slave  Pointer to the SPI device instance.

  @retval EFI_SUCCESS  Write enable command completed and WEL bit is set.
  @retval Others       Write enable command failed.

**/
STATIC
EFI_STATUS
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

/**
  Enable or disable 4-byte address mode on the SPI flash device.

  Reads Status Register 3 to check the current addressing mode, and if it
  differs from the requested mode, issues a command to enable or disable
  4-byte addressing mode.

  @param[in] Slave   Pointer to the SPI device instance.
  @param[in] Enable  TRUE to enable 4-byte mode, FALSE to disable.

  @retval EFI_SUCCESS  Address mode set successfully or already in requested
                       mode.
  @retval Others       Command execution failed.

**/
STATIC
EFI_STATUS
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

/**
  Read the extended address register from the SPI flash device.

  For flash devices larger than 16MB using 3-byte addressing, the extended
  address register contains the upper address byte (A[31:24]).

  @param[in]      Slave    Pointer to the SPI device instance.
  @param[in, out] ExtAddr  Pointer to receive the extended address value.

  @retval EFI_SUCCESS             Extended address read successfully.
  @retval EFI_INVALID_PARAMETER   ExtAddr is NULL.
  @retval Others                  Read command failed.

**/
STATIC
EFI_STATUS
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

/**
  Set the extended address register on the SPI flash device.

  For flash devices larger than 16MB using 3-byte addressing, this function
  sets the extended address register to access the upper 256MB segments.
  Caches the current value to avoid redundant writes.

  @param[in] Slave    Pointer to the SPI device instance.
  @param[in] Address  The full 32-bit address. Bits [31:24] will be written to
                      the extended address register.

  @retval EFI_SUCCESS  Extended address set successfully or already at the
                       correct value.
  @retval Others       Write command failed.

**/
STATIC
EFI_STATUS
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

/**
  Transfer data to or from the SPI flash device.

  Performs read, write, or erase operations on the flash by constructing
  appropriate SPI transfer operations. Handles extended address register
  updates for >16MB flash with 3-byte addressing, and splits large transfers
  into smaller chunks based on controller limits.

  @param[in]      Slave          Pointer to the SPI device instance.
  @param[in]      Direction      Transfer direction.
  @param[in]      Command        Command opcode to send.
  @param[in]      Address        Starting address for the operation.
  @param[in]      DataByteCount  Number of data bytes to transfer.
                                 It should be Zero when SPI_XFER_NO_DATA
  @param[in, out] Buffer         Pointer to data buffer for the transmission.
                                 It should be NULL when SPI_XFER_NO_DATA.

  @retval EFI_SUCCESS             Data transfer completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter combination.
  @retval Others                  Transfer operation failed.

**/
STATIC
EFI_STATUS
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

/**
  Erase a region of the SPI flash device.

  Erases the specified region using the appropriate erase command (4KB, 32KB,
  or 64KB) based on flash capabilities. The address and length must be aligned
  to the erase block size.

  @param[in] This           Pointer to the SPI_FLASH_PROTOCOL instance.
  @param[in] Address        Starting address of the region to erase.
                            It must be erase-size aligned.
  @param[in] DataByteCount  Number of bytes to erase.
                            It must be multiple of erase size.

  @retval EFI_SUCCESS             Erase operation completed successfully.
  @retval EFI_INVALID_PARAMETER   Address or length not aligned to erase size,
                                  or out of range.
  @retval Others                  Erase operation failed.

**/
STATIC
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

  //
  // Check input parameters
  //
  if (Address % EraseSize || DataByteCount % EraseSize) {
    DEBUG ((DEBUG_VERBOSE, "%a(): Either erase address or length is not multiple of erase size.\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  EraseAddr   = Address;
  EraseLength = 0;
  while (EraseLength < DataByteCount) {
    //
    // Programm proper erase address
    //
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

/**
  Read data from the SPI flash device.

  Reads the specified number of bytes from the flash starting at the given
  address into the provided buffer using the fast read command.

  @param[in] This           Pointer to the SPI_FLASH_PROTOCOL instance.
  @param[in] Address        Starting address to read from.
  @param[in] DataByteCount  Number of bytes to read.
  @param[in] Buffer         Pointer to the buffer to receive the data.

  @retval EFI_SUCCESS             Read operation completed successfully.
  @retval EFI_INVALID_PARAMETER   Buffer is NULL, or address/length out of range.
  @retval Others                  Read operation failed.

**/
STATIC
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

/**
  Write data to the SPI flash device.

  Writes the specified number of bytes to the flash starting at the given
  address. The operation is split into page-aligned chunks to respect the
  flash page programming constraints.

  @param[in] This           Pointer to the SPI_FLASH_PROTOCOL instance.
  @param[in] Address        Starting address to write to.
  @param[in] DataByteCount  Number of bytes to write.
  @param[in] Buffer         Pointer to the buffer containing data to write.

  @retval EFI_SUCCESS             Write operation completed successfully.
  @retval EFI_INVALID_PARAMETER   Buffer is NULL, DataByteCount is 0, or
                                  address/length out of range.
  @retval Others                  Write operation failed.

**/
STATIC
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

    //
    // Program proper write address and write data
    //
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

/**
  Update a single erase block of the SPI flash device.

  @param[in] This       Pointer to the SPI_FLASH_PROTOCOL instance.
  @param[in] Offset     Starting address of the erase block.
  @param[in] ToUpdate   Number of bytes to update within the block.
  @param[in] Buf        Pointer to the new data to write.
  @param[in] TmpBuf     Pointer to a temporary buffer.
  @param[in] EraseSize  Size of the erase block.

  @retval EFI_SUCCESS   Block update completed successfully.
  @retval Others        Read, erase, or write operation failed.

**/
STATIC
EFI_STATUS
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

/**
  Update a region of the SPI flash device with progress indication.

  @param[in] This           Pointer to the SPI_FLASH_PROTOCOL instance.
  @param[in] Address        Starting address to update.
  @param[in] DataByteCount  Number of bytes to update.
  @param[in] Buffer         Pointer to the new data to write.

  @retval EFI_SUCCESS           Update operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate temporary buffer.
  @retval Others                Read, erase, or write operation failed.

**/
STATIC
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

/**
  Update a region of the SPI flash device with callback-based progress
  reporting.

  @param[in] This             Pointer to the SPI_FLASH_PROTOCOL instance.
  @param[in] Address          Starting address to update.
  @param[in] DataByteCount    Number of bytes to update.
  @param[in] Buffer           Pointer to the new data to write.
  @param[in] Progress         Optional callback function to report progress.
  @param[in] StartPercentage  Progress percentage at the start of this operation.
  @param[in] EndPercentage    Progress percentage at the end of this operation.

  @retval EFI_SUCCESS           Update operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate temporary buffer.
  @retval Others                Read, erase, or write operation failed.

**/
STATIC
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

/**
  Read and identify the SPI flash device by its JEDEC ID.

  Reads the flash device JEDEC ID and looks it up in the supported flash
  database. Updates the device information structure with the matched flash
  parameters.

  @param[in] This           Pointer to the SPI_FLASH_PROTOCOL instance.
  @param[in] UseInRuntime   TRUE if this flash will be used at runtime.

  @retval EFI_SUCCESS     Flash device identified successfully.
  @retval EFI_NOT_FOUND   Unrecognized or unsupported JEDEC ID.
  @retval Others          ID read operation failed.

**/
STATIC
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

/**
  Initialize the SPI flash device.

  @param[in] This           Pointer to the SPI_FLASH_PROTOCOL instance.
  @param[in] UseInRuntime   TRUE if this flash will be used at runtime.

  @retval EFI_SUCCESS     Flash device initialized successfully.
  @retval EFI_NOT_FOUND   Flash device not detected or unrecognized.
  @retval Others          Initialization operation failed.

**/
STATIC
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

/**
  Initialize the SPI flash protocol instance.

  @param[in] SpiFlashProtocol  Pointer to the SPI_FLASH_PROTOCOL instance.

  @retval EFI_SUCCESS  Protocol initialized successfully.

**/
STATIC
EFI_STATUS
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
STATIC
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

/**
  Entry point of the SPI NOR flash DXE driver.

  @param[in]  ImageHandle  The firmware-allocated handle for the EFI image.
  @param[in]  SystemTable  Pointer to the EFI System Table.

  @retval EFI_SUCCESS           The driver initialized and installed successfully.
  @retval EFI_DEVICE_ERROR      Failed to locate the SPI master protocol.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate the SPI flash instance.
  @retval Others                Protocol installation or event registration failed.

**/
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
