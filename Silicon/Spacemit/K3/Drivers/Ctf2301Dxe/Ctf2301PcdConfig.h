/** @file
  CTF2301 PCD configuration structures.

  Copyright (c) 2026, Spacemit Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _CTF2301_PCD_CONFIG_H_
#define _CTF2301_PCD_CONFIG_H_

typedef struct {
  UINT8    BusNumber;      // I2C bus (controller index)
  UINT8    SlaveAddress;   // I2C slave address
} CTF2301_CONFIG;

typedef struct {
  UINT16           Num;
  CTF2301_CONFIG   Data[0];
} CTF2301_CONFIG_ARRAY;

#endif // _CTF2301_PCD_CONFIG_H_
