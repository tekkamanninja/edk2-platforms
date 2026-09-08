/** @file

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MIPI_I3C_H__
#define __MIPI_I3C_H__

#include <Base.h>

#define I3C_MAX_DEVS                        11

#define I3C_MAX_SLAVE_ADDR                  0x7F

#define I3C_BROADCAST_ADDR                  0x7E
#define I3C_HOT_JOIN_ADDR                   0x02

#define I3C_BUS_MODE_PURE                   0x00      // Only I3C devices are connected to the bus. No limitation expected.
#define I3C_BUS_MODE_MIXED_FAST             0x01      // Legacy I2C devices with 50ns spike filter are present on the bus.
                                                      // The only impact in this mode is that the high SCL pulse has to stay below 50ns to trick
                                                      // I2C devices when transmitting I3C frames.
#define I3C_BUS_MODE_MIXED_LIMITED          0x02      // Legacy I2C devices without 50ns spike filter are present on the bus.
                                                      // However they allow compliance up to the maximum SDR SCL clock frequency.
#define I3C_BUS_MODE_MIXED_SLOW             0x03      // Legacy I2C devices without 50ns spike filter are present on the bus.

#define I3C_SCL_RATE_I3C_MAX                12900000
#define I3C_SCL_RATE_I3C_TYP                12500000
#define I3C_SCL_RATE_I2C_FAST_PLUS          1000000
#define I3C_SCL_RATE_I2C_FAST               400000
#define I3C_TLOW_OD_MIN_NS                  200
#define I3C_THIGH_INIT_OD_MIN_NS            200

#define I3C_PID_LENGTH                      6

//
// Bits definitions for I3C PID
//
#define I3C_PID_MANUFACTURER_ID(x)          (((x) >> 33) & 0x7FFF)
#define I3C_PID_RANDOM_FLAG                 BIT32
#define I3C_PID_RANDOM_VALUE(x)             ((x) & 0xFFFFFFFF)
#define I3C_PID_PART_ID(x)                  (((x) >> 16) & 0xFFFF)
#define I3C_PID_INSTANCE_ID(x)              (((x) >> 12) & 0xF)
#define I3C_PID_EXTRA_ID(x)                 ((x) & 0xFFF)

//
// Bits definitions for I3C BCR
//
#define I3C_BCR_DEVICE_ROLE(x)              ((x) & (BIT7 | BIT6))
#define I3C_BCR_HDR_CAP                     BIT5
#define I3C_BCR_BRIDGE                      BIT4
#define I3C_BCR_OFFLINE_CAP                 BIT3
#define I3C_BCR_IBI_PAYLOAD                 BIT2
#define I3C_BCR_IBI_REQUEST_CAP             BIT1
#define I3C_BCR_MAX_DATA_SPEED_LIMIT        BIT0

//
// I3C CCC (Common Command Codes) related definitions
//
#define I3C_CCC_BROADCAST                   0
#define I3C_CCC_DIRECT                      BIT7
#define I3C_CCC_ID(x, b)                    ((x) | ((b) ? I3C_CCC_BROADCAST : I3C_CCC_DIRECT))

//
// Commands valid in both broadcast and unicast modes
//
#define I3C_CCC_ENEC(b)                     I3C_CCC_ID(0x0, b)
#define I3C_CCC_DISEC(b)                    I3C_CCC_ID(0x1, b)
#define I3C_CCC_ENTAS(x, b)                 I3C_CCC_ID(0x2 + (x), b)
#define I3C_CCC_RSTDAA(b)                   I3C_CCC_ID(0x6, b)
#define I3C_CCC_SETMWL(b)                   I3C_CCC_ID(0x9, b)
#define I3C_CCC_SETMRL(b)                   I3C_CCC_ID(0xA, b)
#define I3C_CCC_SETXTIME(b)                 ((d) ? 0x28 : 0x98)
#define I3C_CCC_VENDOR(x, b)                ((x) + ((b) ? 0x61 : 0xE0))

//
// Broadcast-only commands
//
#define I3C_CCC_ENTDAA                      I3C_CCC_ID(0x7, TRUE)
#define I3C_CCC_DEFSLVS                     I3C_CCC_ID(0x8, TRUE)
#define I3C_CCC_ENTTM                       I3C_CCC_ID(0xB, TRUE)
#define I3C_CCC_SETBUSCON                   I3C_CCC_ID(0xC, TRUE)
#define I3C_CCC_ENTHDR(x)                   I3C_CCC_ID(0x20 + (x), TRUE)
#define I3C_CCC_SETAASA                     I3C_CCC_ID(0x29, TRUE)
#define I3C_CCC_SETHID                      I3C_CCC_ID(0x61, TRUE)
#define I3C_CCC_DEVCTRL                     I3C_CCC_ID(0x62, TRUE)

//
// Direct-only commands
//
#define I3C_CCC_SETDASA                     I3C_CCC_ID(0x7,  FALSE)
#define I3C_CCC_SETNEWDA                    I3C_CCC_ID(0x8,  FALSE)
#define I3C_CCC_GETMWL                      I3C_CCC_ID(0xB,  FALSE)
#define I3C_CCC_GETMRL                      I3C_CCC_ID(0xC,  FALSE)
#define I3C_CCC_GETPID                      I3C_CCC_ID(0xD,  FALSE)
#define I3C_CCC_GETBCR                      I3C_CCC_ID(0xE,  FALSE)
#define I3C_CCC_GETDCR                      I3C_CCC_ID(0xF,  FALSE)
#define I3C_CCC_GETSTATUS                   I3C_CCC_ID(0x10, FALSE)
#define I3C_CCC_GETACCMST                   I3C_CCC_ID(0x11, FALSE)
#define I3C_CCC_SETBRGTGT                   I3C_CCC_ID(0x13, FALSE)
#define I3C_CCC_GETMXDS                     I3C_CCC_ID(0x14, FALSE)
#define I3C_CCC_GETHDRCAP                   I3C_CCC_ID(0x15, FALSE)
#define I3C_CCC_GETXTIME                    I3C_CCC_ID(0x19, FALSE)
#define I3C_CCC_DEVCAPS                     I3C_CCC_ID(0x60, FALSE)

//
// Event bits for I3C_CCC_ENEC/I3C_CCC_DISEC
//
#define I3C_CCC_EVENT_SIR                   BIT0
#define I3C_CCC_EVENT_MR                    BIT1
#define I3C_CCC_EVENT_HJ                    BIT3

//
// Error status
//
#define I3C_CCC_ERROR_UNKNOWN               0
#define I3C_CCC_ERROR_M0                    1
#define I3C_CCC_ERROR_M1                    2
#define I3C_CCC_ERROR_M2                    3

//
// I3C CCC command destination data structure
//
typedef struct {
  UINT8           Addr;
  UINTN           Length;
  VOID            *Data;
} I3C_CCC_DEST;

//
// I3C CCC command data structure
//
typedef struct {
  BOOLEAN         Rnw;
  UINT8           CccId;
  UINTN           NumDests;
  I3C_CCC_DEST    *Dests;
  UINT8           RetError;
} I3C_CCC_CMD;

#endif // __MIPI_I3C_H__
