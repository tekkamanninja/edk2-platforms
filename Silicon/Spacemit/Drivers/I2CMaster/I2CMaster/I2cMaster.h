/** @file
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _SPACEMIT_I2C_H_
#define _SPACEMIT_I2C_H_

#include <Base.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Protocol/I2cEnumerate.h>
#include <Protocol/I2cBusConfigurationManagement.h>
#include <Protocol/I2cMaster.h>
#include <Protocol/PinCtrl.h>

#include "I2cPcdConfig.h"

EFI_GUID  gEfiI2cDeviceGuid = {
  0xAC5DAA9E, 0x4D7B, 0x4C36, {
    0x9E,     0x37,   0x48,   0xC7, 0x9A, 0xC5, 0x31, 0x2D
  }
};

extern EFI_DRIVER_BINDING_PROTOCOL  mI2cMasterDriverBinding;

// Default Clock Frequency (100KHz)
#define I2C_DEFAULT_SPEED  100000

/* Shall the current transfer have a start/stop condition? */
#define I2C_COND_NORMAL  0
#define I2C_COND_START   1
#define I2C_COND_STOP    2

/* Shall the current transfer be ack/nacked or being waited for it? */
#define I2C_ACKNAK_WAITACK  1
#define I2C_ACKNAK_SENDACK  2
#define I2C_ACKNAK_SENDNAK  4

/* Specify who shall transfer the data (master or slave) */
#define I2C_WRITE  0
#define I2C_READ   1

  #if (CONFIG_SYS_I2C_SPEED == 400000)
#define I2C_ICR_INIT  (ICR_FM | ICR_BEIE | ICR_IRFIE | ICR_ITEIE | ICR_GCD | ICR_SCLE)
  #else
#define I2C_ICR_INIT  (ICR_BEIE | ICR_IRFIE | ICR_ITEIE | ICR_GCD | ICR_SCLE)
  #endif

/* ----- Control register bits ---------------------------------------- */

#define ICR_START      0x1    /* start bit */
#define ICR_STOP       0x2    /* stop bit */
#define ICR_ACKNAK     0x4    /* send ACK(0) or NAK(1) */
#define ICR_TB         0x8    /* transfer byte bit */
#define ICR_MA         BIT12  /* master abort */
#define ICR_SCLE       BIT13  /* master clock enable, mona SCLEA */
#define ICR_IUE        BIT14  /* unit enable */
#define ICR_GCD        BIT21  /* general call disable */
#define ICR_ITEIE      BIT19  /* enable tx interrupts */
#define ICR_IRFIE      BIT20  /* enable rx interrupts, mona: DRFIE */
#define ICR_BEIE       BIT22  /* enable bus error ints */
#define ICR_SSDIE      BIT24  /* slave STOP detected int enable */
#define ICR_ALDIE      BIT18  /* enable arbitration interrupt */
#define ICR_SADIE      BIT23  /* slave address detected int enable */
#define ICR_UR         BIT10  /* unit reset */
#define ICR_SM         (0x0)  /* Standard Mode */
#define ICR_FM         BIT8   /* Fast Mode */
#define ICR_MODE_MASK  (0x300)/* Mode mask */
/* ----- Status register bits ----------------------------------------- */

#define ISR_RWM     BIT13/* read/write mode */
#define ISR_ACKNAK  BIT14/* ack/nak status */
#define ISR_UB      BIT15/* unit busy */
#define ISR_IBB     BIT16/* bus busy */
#define ISR_SSD     BIT24/* slave stop detected */
#define ISR_ALD     BIT18/* arbitration loss detected */
#define ISR_ITE     BIT19/* tx buffer empty */
#define ISR_IRF     BIT20/* rx buffer full */
#define ISR_GCAD    BIT21/* general call address detected */
#define ISR_SAD     BIT23/* slave address detected */
#define ISR_BED     BIT22/* bus error no ACK/NAK */

#define I2C_ISR_INIT  0x1FDE000

#define ICR_OFFSET   0x00/* I2C Control Register */
#define ISR_OFFSET   0x04/* I2C Status Register */
#define ISAR_OFFSET  0x08/* I2C Slave Address Register */
#define IDBR_OFFSET  0x0c/* I2C Data Register */

// I2C Controller Context Structure
typedef struct {
  UINT32                                           Signature;
  EFI_HANDLE                                       ControllerHandle;
  EFI_LOCK                                         Lock;
  UINTN                                            BaseAddress;
  UINT32                                           ClockRate;
  UINT8                                            ControllerId;
  INTN                                             Bus;
  SILICON_PINCTRL_PROTOCOL                         *PinCtrlProtocol;
  EFI_I2C_MASTER_PROTOCOL                          I2cMaster;
  EFI_I2C_ENUMERATE_PROTOCOL                       I2cEnumerate;
  EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL    I2cBusConfig;
  UINTN                                            CurrentConfiguration;
  BOOLEAN                                          PinctrlControllerRegistered;
  BOOLEAN                                          PinctrlStateApplied;
  BOOLEAN                                          PinctrlStateUnavailable;
} I2C_MASTER_CONTEXT;

typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  UINT32                      Instance;
  EFI_DEVICE_PATH_PROTOCOL    End;
} SP_I2C_DEVICE_PATH;

#define I2C_MASTER_SIGNATURE  SIGNATURE_32('I', '2', 'C', 'M')

#define I2C_MASTER_FROM_THIS(a) \
  CR(a, I2C_MASTER_CONTEXT, I2cMaster, I2C_MASTER_SIGNATURE)

#define I2C_MASTER_CONTEXT_FROM_PROTOCOL(a) \
  CR(a, I2C_MASTER_CONTEXT, I2cMaster, I2C_MASTER_SIGNATURE)

#define I2C_MASTER_FROM_BUS_CONFIG(a) \
  CR(a, I2C_MASTER_CONTEXT, I2cBusConfig, I2C_MASTER_SIGNATURE)

#define I2C_MASTER_CONTEXT_FROM_ENUMERATE(a) \
  CR(a, I2C_MASTER_CONTEXT, I2cEnumerate, I2C_MASTER_SIGNATURE)

/*
 * I2C_FLAG_NORESTART is not part of PI spec, it allows to continue
 * transmission without repeated start operation.
 */
#define I2C_FLAG_NORESTART  0x00000002 // No repeated START

// I2C Transfer Status Definitions
#define I2C_STATUS_START        0x08// START sent
#define I2C_STATUS_RPTD_START   0x10// Repeated START sent
#define I2C_STATUS_ADDR_W_ACK   0x18// Address + Write acknowledged
#define I2C_STATUS_ADDR_R_ACK   0x40// Address + Read acknowledged
#define I2C_STATUS_DATA_WR_ACK  0x28// Data write acknowledged

// I2C Control Register Bit Definitions
#define I2C_CONTROL_START  BIT0// Send START
#define I2C_CONTROL_STOP   BIT1// Send STOP
#define I2C_CONTROL_ACK    BIT2// Send ACK
#define I2C_CONTROL_IFLG   BIT3// Interrupt Flag

// I2C Transfer Timeout Definitions
#define I2C_OPERATION_TIMEOUT  1000  // 1ms
#define I2C_TRANSFER_TIMEOUT   100000// 100ms

// I2C Message Structure Definition
typedef struct {
  UINT8     Condition;// START/STOP/NORMAL
  UINT8     Direction;// Read/Write Direction
  UINT8     Data;     // Data Byte
  UINT8     AckNack;  // ACK/NAK Control
  UINT32    Flags;    // Operation Flag Bits
} SPACEMIT_I2C_MSG;

EFI_STATUS
EFIAPI
I2cMasterSetBusFrequency (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This,
  IN OUT UINTN                      *BusClockHertz
  );

EFI_STATUS
EFIAPI
I2cMasterReset (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This
  );

EFI_STATUS
EFIAPI
I2cMasterStartRequest (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This,
  IN UINTN                          SlaveAddress,
  IN EFI_I2C_REQUEST_PACKET         *RequestPacket,
  IN EFI_EVENT                      Event OPTIONAL,
  OUT EFI_STATUS                    *I2cStatus OPTIONAL
  );

EFI_STATUS
ProbeEepromType (
  IN I2C_MASTER_CONTEXT  *Context,
  IN UINT8               SlaveAddress
  );

EFI_STATUS
EFIAPI
I2cMasterDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
I2cMasterDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
I2cMasterDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  );

EFI_STATUS
EFIAPI
I2cEnumerate (
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL  *This,
  IN OUT CONST EFI_I2C_DEVICE          **Device
  );

EFI_STATUS
EFIAPI
I2cBusConfiguration (
  IN CONST EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL  *This,
  IN UINTN                                                I2cBusConfiguration,
  IN EFI_EVENT                                            Event OPTIONAL,
  IN EFI_STATUS                                           *I2cStatus OPTIONAL
  );

EFI_STATUS
I2cSetSpeed (
  IN I2C_MASTER_CONTEXT  *Context,
  IN UINTN               SpeedHz
  );

EFI_STATUS
EFIAPI
I2cGetBusFrequency (
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL  *This,
  IN UINTN                             Bus,
  OUT UINTN                            *BusClockHertz
  );

#endif
