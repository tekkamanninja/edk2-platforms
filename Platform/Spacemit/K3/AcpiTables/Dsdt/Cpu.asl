/** @file
  Differentiated System Description Table Fields (DSDT)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CPPC_ENABLE
#define CPU_DEVICE(CpuName, Uid)    \
  Device (CpuName) {                \
    Name (_HID, "ACPI0007")         \
    Name (_UID, Uid)                \
  }
#else
#define CPU_DEVICE(CpuName, Uid)    \
  Device (CpuName) {                \
    Name (_HID, "ACPI0007")         \
    Name (_UID, Uid)                \
    Name (_CPC,                     \
      Package () {                  \
        23,                         \
        3,                          \
        3200,                       \
        2500,                       \
        20,                         \
        20,                         \
        ResourceTemplate () {       \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(FFixedHW, 64, 0,                       \
               0x1000000000000005,                        \
               4)                                         \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(FFixedHW, 64, 0,                       \
               0x100000000000000B,                        \
               4)                                         \
        },                                                \
        ResourceTemplate () {                             \
          Register(FFixedHW, 64, 0,                       \
               0x2000000000000C00,                        \
               4)                                         \
        },                                                \
        ResourceTemplate () {                             \
          Register(FFixedHW, 64, 0,                       \
               0x100000000000000D,                        \
               4)                                         \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        800,                                              \
        20,                                               \
        2500,                                             \
      }                                                   \
    )                                                     \
  }
#endif

Scope (_SB) {
  CPU_DEVICE (CPU0, 0)
  CPU_DEVICE (CPU1, 1)
  CPU_DEVICE (CPU2, 2)
  CPU_DEVICE (CPU3, 3)
  CPU_DEVICE (CPU4, 4)
  CPU_DEVICE (CPU5, 5)
  CPU_DEVICE (CPU6, 6)
  CPU_DEVICE (CPU7, 7)
}
