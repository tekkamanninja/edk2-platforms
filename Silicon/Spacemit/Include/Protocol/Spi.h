/*******************************************************************************
Copyright (C) 2016 Marvell International Ltd.
Copyright (c) 2024, SpacemiT Co., Ltd.

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/
#ifndef __SPI_MASTER_PROTOCOL_H__
#define __SPI_MASTER_PROTOCOL_H__

extern EFI_GUID gSpacemitSpiMasterProtocolGuid;

typedef struct _SPI_MASTER_PROTOCOL SPI_MASTER_PROTOCOL;

typedef enum {
  SPI_MODE0, // CPOL = 0 & CPHA = 0
  SPI_MODE1, // CPOL = 0 & CPHA = 1
  SPI_MODE2, // CPOL = 1 & CPHA = 0
  SPI_MODE3  // CPOL = 1 & CPHA = 1
} SPI_MODE;

enum {
  SPI_XFER_NO_DATA = 0,  // no data transfer
  SPI_XFER_RX_DATA = 1,  // data coming from the SPI device
  SPI_XFER_TX_DATA = 2,  // data sent to the SPI device
};

typedef struct {
  // number of IO lines used to transmit the command
  UINT8 BusWidth;
  UINT8 Nbytes;
  UINT8 Opcode;
} SPI_CMD;

typedef struct {
  // number of IO lines used to transmit the address cycles
  UINT8  BusWidth;
  UINT8  Nbytes;
  UINT64 Val;
} SPI_ADDR;

typedef struct {
  // number of IO lanes used to transmit the dummy bytes
  UINT8 BusWidth;
  UINT8 Nbytes;
} SPI_DUMMY;

typedef struct {
  // number of IO lanes for data transfer
  UINT8  BusWidth;
  // direction of the transfer
  UINT8  Dir;
  UINT32 Nbytes;
  VOID  *Buf;
} SPI_DATA;

typedef struct {
  SPI_CMD    Cmd;
  SPI_ADDR   Addr;
  SPI_DUMMY  Dummy;
  SPI_DATA   Data;
} SPI_XFER_OP;

typedef struct {
  UINT8      Cs;
  SPI_MODE   Mode;
  UINT8      AddrSize;
  UINT32     PageSize;
  UINTN      FlashSize;
  VOID       *Info;
} SPI_DEVICE;

typedef
EFI_STATUS
(EFIAPI *SPI_INIT) (
  IN SPI_MASTER_PROTOCOL *This
);

typedef
EFI_STATUS
(EFIAPI *SPI_TRANSFER) (
  IN SPI_MASTER_PROTOCOL *This,
  IN SPI_DEVICE          *Slave,
  IN SPI_XFER_OP         *Op
);

typedef
EFI_STATUS
(EFIAPI *SPI_ADJUST_OP_SIZE) (
  IN SPI_MASTER_PROTOCOL *This,
  IN SPI_XFER_OP         *Op
);

typedef
EFI_STATUS
(EFIAPI *SPI_CONFIG_RT) (
  IN SPI_MASTER_PROTOCOL *This
);

struct _SPI_MASTER_PROTOCOL {
  SPI_INIT           Init;
  SPI_ADJUST_OP_SIZE AjustOPSize;
  SPI_TRANSFER       Transfer;
  SPI_CONFIG_RT      ConfigRuntime;
};

#endif // __SPI_MASTER_PROTOCOL_H__
