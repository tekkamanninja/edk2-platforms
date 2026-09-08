/********************************************************************************
Copyright (C) 2016 Marvell International Ltd.
Copyright (c) 2024, Spacemit Co., Ltd. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/

#ifndef _EEPROM_PCD_CONFIG_H_
#define _EEPROM_PCD_CONFIG_H_

// EEPROM Device Configuration Structure
typedef struct {
  UINT8    BusNumber;    // I2C bus number
  UINT8    SlaveAddress; // I2C slave address
  UINT8    AddressWidth; // EEPROM address width in bytes (1 or 2)
  UINT8    PageSize;     // EEPROM page size in bytes
} EEPROM_CONFIG;

typedef struct {
  UINT16           Num;     // Number of EEPROM devices
  EEPROM_CONFIG    Data[0]; // Array of EEPROM configs
} EEPROM_CONFIG_ARRAY;

#endif // _EEPROM_PCD_CONFIG_H_
