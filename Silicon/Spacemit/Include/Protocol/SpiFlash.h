/*******************************************************************************
Copyright (C) 2016 Marvell International Ltd.
Copyright (c) 2024, Spacemit Co., Ltd.

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/
#ifndef __SPI_FLASH__
#define __SPI_FLASH__

#include <Protocol/FirmwareManagement.h>
#include <Protocol/Spi.h>

extern EFI_GUID gSpacemitSpiFlashProtocolGuid;

typedef struct _SPI_FLASH_PROTOCOL SPI_FLASH_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *SPI_FLASH_INIT) (
  IN SPI_FLASH_PROTOCOL   *This,
  IN BOOLEAN              UseInRuntime
  );

typedef
EFI_STATUS
(EFIAPI *SPI_FLASH_READ_ID) (
  IN SPI_FLASH_PROTOCOL   *This,
  IN BOOLEAN              UseInRuntime
  );

typedef
EFI_STATUS
(EFIAPI *SPI_FLASH_READ) (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINT32               Address,
  IN UINTN                DataByteCount,
  IN VOID                 *Buffer
  );

typedef
EFI_STATUS
(EFIAPI *SPI_FLASH_WRITE) (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINT32               Address,
  IN UINTN                DataByteCount,
  IN VOID                 *Buffer
  );

typedef
EFI_STATUS
(EFIAPI *SPI_FLASH_ERASE) (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINTN                Address,
  IN UINTN                DataByteCount
  );

typedef
EFI_STATUS
(EFIAPI *SPI_FLASH_UPDATE) (
  IN SPI_FLASH_PROTOCOL   *This,
  IN UINT32               Address,
  IN UINTN                DataByteCount,
  IN UINT8                *Buffer
  );

typedef
EFI_STATUS
(EFIAPI *SPI_FLASH_UPDATE_WITH_PROGRESS) (
  IN SPI_FLASH_PROTOCOL                             *This,
  IN UINT32                                         Address,
  IN UINTN                                          DataByteCount,
  IN UINT8                                          *Buffer,
  IN EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  Progress,         OPTIONAL
  IN UINTN                                          StartPercentage,
  IN UINTN                                          EndPercentage
  );

struct _SPI_FLASH_PROTOCOL {
  SPI_FLASH_INIT                  Init;
  SPI_FLASH_READ_ID               ReadId;
  SPI_FLASH_READ                  Read;
  SPI_FLASH_WRITE                 Write;
  SPI_FLASH_ERASE                 Erase;
  SPI_FLASH_UPDATE                Update;
  SPI_FLASH_UPDATE_WITH_PROGRESS  UpdateWithProgress;
};

#endif // __SPI_FLASH__
