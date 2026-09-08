/** @file
  Spi nor flash driver implementation.

  Copyright (C) 2016 Marvell International Ltd.
  Copyright (c) 2024, Spacemit Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef __SPI_NOR_FLASH_DXE_H__
#define __SPI_NOR_FLASH_DXE_H__

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/NorFlashInfoLib.h>
#include <Library/MemoryManagementLib.h>
#include <Protocol/SpiFlash.h>

#define SPI_FLASH_SIGNATURE               SIGNATURE_32('s', 'n', 'f', 'm')
#define SPI_FLASH_INSTANCE_FROM_THIS(a)   CR (a, SPI_FLASH_INSTANCE, SpiFlashProtocol, SPI_FLASH_SIGNATURE)

#define CMD_READ_ID                       0x9F
#define CMD_WRITE_ENABLE                  0x06
#define CMD_READ_STATUS                   0x05
#define   STATUS_REG_POLL_WIP             BIT0
#define   STATUS_REG_POLL_WEL             BIT1
#define CMD_READ_STATUS2                  0x35
#define CMD_READ_STATUS3                  0x15
#define   STATUS_REG_ADS                  BIT0
#define CMD_FLAG_STATUS                   0x70
#define CMD_WRITE_STATUS_REG              0x01
#define CMD_READ_ARRAY_FAST               0x0B
#define CMD_READ_EXT_ADDR                 0xC8
#define CMD_WRITE_EXT_ADDR                0xC5
#define CMD_PAGE_PROGRAM                  0x02
#define CMD_ERASE_4K                      0x20
#define CMD_ERASE_32K                     0x52
#define CMD_ERASE_64K                     0xD8
#define CMD_RESET_ENABLE                  0x66
#define CMD_RESET                         0x99

#define CMD_4B_ADDR_ENABLE                0xB7
#define CMD_4B_ADDR_DISABLE               0xE9
#define CMD_4B_READ_ARRAY_FAST            0x0C
#define CMD_4B_PAGE_PROGRAM               0x12
#define CMD_4B_ERASE_4K                   0x21
#define CMD_4B_ERASE_32K                  0x52
#define CMD_4B_ERASE_64K                  0xDC

#define MANUFACTURER_ID_MACRONIX          0xC2
#define MANUFACTURER_ID_SPANSION          0x01
#define MANUFACTURER_ID_WINBOND           0xEF


#define SR_QUAD_EN_MX                     BIT6    /* Macronix Quad I/O */
#define CR_QUAD_EN_SPAN                   BIT1    /* Spansion Quad I/O */
#define CR_QUAD_EN_WINB                   BIT1    /* Winbond  Quad I/O */

#define POLL_STATUS_TIMEOUT               2000    /* timeout in millisecond */

typedef struct {
  UINTN                Signature;
  EFI_HANDLE           Handle;
  SPI_FLASH_PROTOCOL   SpiFlashProtocol;
  SPI_DEVICE           SpiDev;
} SPI_FLASH_INSTANCE;

#endif // __SPI_NOR_FLASH_DXE_H__
