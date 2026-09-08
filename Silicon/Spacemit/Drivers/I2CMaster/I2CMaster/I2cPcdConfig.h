/** @file
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _I2C_PCD_CONFIG_H_
#define _I2C_PCD_CONFIG_H_

// I2C Controller Configuration Structure
typedef struct {
  UINT8      ControllerId;// I2C Controller ID
  UINT64     BaseAddress; // Controller Base Address
  UINT32     ClockRate;   // Clock Rate in Hz
  BOOLEAN    Enable;      // Enable/Disable Controller
} I2C_CONTROLLER_CONFIG;

typedef struct {
  UINT16                   Num;    // Number of Controllers
  I2C_CONTROLLER_CONFIG    Data[0];// Array of Controller Configs
} I2C_CONTROLLER_CONFIG_ARRAY;

// I2C Slave Device Configuration Structure
typedef struct {
  UINT8    BusNumber;   // I2C Bus Number
  UINT8    SlaveAddress;// I2C Slave Address
} I2C_SLAVE_CONFIG;

typedef struct {
  UINT16              Num;    // Number of Slave Devices
  I2C_SLAVE_CONFIG    Data[0];// Array of Slave Configs
} I2C_SLAVE_CONFIG_ARRAY;

#endif // _I2C_PCD_CONFIG_H_
