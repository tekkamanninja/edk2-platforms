/** @file
  RISC-V Platform Management Interface (RPMI) definitions

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __RPMI_H__
#define __RPMI_H__

#pragma pack(1)

//
// RPMI message types
//
typedef enum {
  RpmiMsgTypeNormalRequest,
  RpmiMsgTypePostedRequest,
  RpmiMsgTypeAcknowledgement,
  RpmiMsgTypeNotification,
} RPMI_MESSAGE_TYPE;

//
// RPMI error codes
//
#define RPMI_SUCCESS                  0
#define RPMI_ERR_FAILED               (-1)
#define RPMI_ERR_NOTSUPP              (-2)
#define RPMI_ERR_INVALID_PARAM        (-3)
#define RPMI_ERR_DENIED               (-4)
#define RPMI_ERR_INVALID_ADDR         (-5)
#define RPMI_ERR_ALREADY              (-6)
#define RPMI_ERR_EXTENSION            (-7)
#define RPMI_ERR_HW_FAULT             (-8)
#define RPMI_ERR_BUSY                 (-9)
#define RPMI_ERR_INVALID_STATE        (-10)
#define RPMI_ERR_BAD_RANGE            (-11)
#define RPMI_ERR_TIMEOUT              (-12)
#define RPMI_ERR_IO                   (-13)
#define RPMI_ERR_NO_DATA              (-14)
#define RPMI_ERR_RESERVED_START       (-15)
#define RPMI_ERR_RESERVED_END         (-127)
#define RPMI_ERR_VENDOR_START         (-128)

//
// RPMI Service Group IDs
//
#define RPMI_SRVGRP_BASE                              0x0001
#define RPMI_SRVGRP_SYSTEM_MSI                        0x0002
#define RPMI_SRVGRP_SYSTEM_RESET                      0x0003
#define RPMI_SRVGRP_SYSTEM_SUSPEND                    0x0004
#define RPMI_SRVGRP_HART_STATE_MANAGEMENT             0x0005
#define RPMI_SRVGRP_CPPC                              0x0006
#define RPMI_SRVGRP_VOLTAGE                           0x0007
#define RPMI_SRVGRP_CLOCK                             0x0008
#define RPMI_SRVGRP_DEVICE_POWER                      0x0009
#define RPMI_SRVGRP_PERFORMANCE                       0x000A
#define RPMI_SRVGRP_MAGAGEMENT_MODE                   0x000B
#define RPMI_SRVGRP_RAS_AGENT                         0x000C

//
// RPMI BASE Service Group Service IDs
//
#define RPMI_BASE_SRV_ENABLE_NOTIFICATION             0x01
#define RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION      0x02
#define RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN          0x03
#define RPMI_BASE_SRV_GET_SPEC_VERSION                0x04
#define RPMI_BASE_SRV_GET_PLATFORM_INFO               0x05
#define RPMI_BASE_SRV_PROBE_SERVICE_GROUP             0x06
#define RPMI_BASE_SRV_GET_ATTRIBUTES                  0x07

//
// RPMI SYSTEM_MSI Service Group Service IDs
//
#define RPMI_SYSMSI_SRV_ENABLE_NOTIFICATION           0x01
#define RPMI_SYSMSI_SRV_GET_ATTRIBUTES                0x02
#define RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES            0x03
#define RPMI_SYSMSI_SRV_SET_MSI_STATE                 0x04
#define RPMI_SYSMSI_SRV_GET_MSI_STATE                 0x05
#define RPMI_SYSMSI_SRV_SET_MSI_TARGET                0x06
#define RPMI_SYSMSI_SRV_GET_MSI_TARGET                0x07

//
// RPMI SYSTEM_RESET Service Group Service IDs
//
#define RPMI_SYSRST_SRV_ENABLE_NOTIFICATION           0x01
#define RPMI_SYSRST_SRV_GET_ATTRIBUTES                0x02
#define RPMI_SYSRST_SRV_SYSTEM_RESET                  0x03

//
// RPMI HART_STATE_MANAGEMENT Service Group Service IDs
//
#define RPMI_HSM_SRV_ENABLE_NOTIFICATION              0x01
#define RPMI_HSM_SRV_GET_HART_STATUS                  0x02
#define RPMI_HSM_SRV_GET_HART_LIST                    0x03
#define RPMI_HSM_SRV_GET_SUSPEND_TYPES                0x04
#define RPMI_HSM_SRV_GET_SUSPEND_INFO                 0x05
#define RPMI_HSM_SRV_HART_START                       0x06
#define RPMI_HSM_SRV_HART_STOP                        0x07
#define RPMI_HSM_SRV_HART_SUSPEND                     0x08

//
// RPMI CPPC Service Group Service IDs
//
#define RPMI_CPPC_SRV_ENABLE_NOTIFICATION             0x01
#define RPMI_CPPC_SRV_PROBE_REG                       0x02
#define RPMI_CPPC_SRV_READ_REG                        0x03
#define RPMI_CPPC_SRV_WRITE_REG                       0x04
#define RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION         0x05
#define RPMI_CPPC_SRV_GET_FAST_CHANNEL_OFFSET         0x06
#define RPMI_CPPC_SRV_GET_HART_LIST                   0x07

//
// RPMI VOLTAGE Service Group Service IDs
//
#define RPMI_VOLT_SRV_ENABLE_NOTIFICATION             0x01
#define RPMI_VOLT_SRV_GET_NUM_DOMAINS                 0x02
#define RPMI_VOLT_SRV_GET_ATTRIBUTES                  0x03
#define RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS            0x04
#define RPMI_VOLT_SRV_SET_CONFIG                      0x05
#define RPMI_VOLT_SRV_GET_CONFIG                      0x06
#define RPMI_VOLT_SRV_SET_LEVEL                       0x07
#define RPMI_VOLT_SRV_GET_LEVEL                       0x08

#define RPMI_VOLT_DOMAIN_NAME_MAX_LEN                 16

#define RPMI_VOLT_FORMAT_TYPE_SHIFT                   1
#define RPMI_VOLT_FORMAT_TYPE_MASK                    (0x07 << RPMI_VOLT_FORMAT_TYPE_SHIFT)
#define RPMI_VOLT_CONTROL_SUPPORT_SHIFT               0
#define RPMI_VOLT_CONTROL_SUPPORT_MASK                (1 << RPMI_VOLT_CONTROL_SUPPORT_SHIFT)
#define RPMI_VOLT_SUPPLY_STATE_SHIFT                  0
#define RPMI_VOLT_SUPPLY_STATE_MASK                   (1 << RPMI_VOLT_SUPPLY_STATE_SHIFT)

typedef enum {
  RPMI_VOLTAGE_SUPPLY_DISABLE = 0,
  RPMI_VOLTAGE_SUPPLY_ENABLE,
} RPMI_VOLTAGE_SUPPLY_CONFIG;

typedef enum {
  RPMI_VOLTAGE_FORMAT_DISCRETE = 0,
  RPMI_VOLTAGE_FORMAT_LINEAR,
} RPMI_VOLTAGE_FORMAT_TYPE;

typedef struct {
  INT32     Status;
  UINT32    NumDomains;
} RPMI_VOLT_GET_NUM_DOMAINS_RESP;

typedef struct {
  UINT32    DomainId;
} RPMI_VOLT_GET_ATTRIBUTES_REQ;

typedef struct {
  INT32     Status;
  UINT32    Flags;
  UINT32    NumLevels;
  /* used for get parent id */
  UINT32    TransLatency;
  CHAR8     DomainName[RPMI_VOLT_DOMAIN_NAME_MAX_LEN];
} RPMI_VOLT_GET_ATTRIBUTES_RESP;

typedef struct {
  UINT32    DomainId;
  UINT32    VoltLevelIndex;
} RPMI_VOLT_GET_SUPPORTED_LEVELS_REQ;

typedef struct {
  UINT32    MinVolt;
  UINT32    MaxVolt;
  UINT32    VoltStep;
} LINEAR_VOLTAGE_RANGE;

typedef struct {
  UINT32    Volt;
} DISCRETE_VOLTAGE;

typedef struct {
  INT32     Status;
  UINT32    Flags;
  UINT32    Remaining;
  UINT32    Returned;
  UINT32    VoltLevel[0];
} RPMI_VOLT_GET_SUPPORTED_LEVELS_RESP;

typedef struct {
  UINT32    DomainId;
  UINT32    Config;
} RPMI_VOLT_SET_CONFIG_REQ;

typedef struct {
  INT32     Status;
} RPMI_VOLT_SET_CONFIG_RESP;

typedef struct {
  UINT32    DomainId;
} RPMI_VOLT_GET_CONFIG_REQ;

typedef struct  {
  INT32     Status;
  UINT32    Config;
} RPMI_VOLT_GET_CONFIG_RESP;

typedef struct {
  UINT32    DomainId;
  UINT32    VoltLevel;
} RPMI_VOLT_SET_LEVEL_REQ;

typedef struct {
  INT32     Status;
} RPMI_VOLT_SET_LEVEL_RESP;

typedef struct {
  UINT32    DomainId;
} RPMI_VOLT_GET_LEVEL_REQ;

typedef struct {
  INT32     Status;
  UINT32    VoltLevel;
} RPMI_VOLT_GET_LEVEL_RESP;

//
// RPMI CLOCK Service Group Service IDs
//
#define RPMI_CLOCK_SRV_ENABLE_NOTIFICATION            0x01
#define RPMI_CLOCK_SRV_GET_NUM_CLOCKS                 0x02
#define RPMI_CLOCK_SRV_GET_ATTRIBUTES                 0x03
#define RPMI_CLOCK_SRV_GET_SUPPORTED_RATES            0x04
#define RPMI_CLOCK_SRV_SET_CONFIG                     0x05
#define RPMI_CLOCK_SRV_GET_CONFIG                     0x06
#define RPMI_CLOCK_SRV_SET_RATE                       0x07
#define RPMI_CLOCK_SRV_GET_RATE                       0x08

//
// RPMI DEVICE_POWER Service Group Service IDs
//
#define RPMI_DPWR_SRV_ENABLE_NOTIFICATION             0x01
#define RPMI_DPWR_SRV_GET_NUM_DOMAINS                 0x02
#define RPMI_DPWR_SRV_GET_ATTRIBUTES                  0x03
#define RPMI_DPWR_SRV_SET_STATE                       0x04
#define RPMI_DPWR_SRV_GET_STATE                       0x05

//
// RPMI PERFORMANCE Service Group Service IDs
//
#define RPMI_PERF_SRV_ENABLE_NOTIFICATION             0x01
#define RPMI_PERF_SRV_GET_NUM_DOMAINS                 0x02
#define RPMI_PERF_SRV_GET_ATTRIBUTES                  0x03
#define RPMI_PERF_SRV_GET_SUPPORTED_LEVELS            0x04
#define RPMI_PERF_SRV_GET_LEVEL                       0x05
#define RPMI_PERF_SRV_SET_LEVEL                       0x06
#define RPMI_PERF_SRV_GET_LIMIT                       0x07
#define RPMI_PERF_SRV_SET_LIMIT                       0x08
#define RPMI_PERF_SRV_GET_FAST_CHANNEL_REGION         0x09
#define RPMI_PERF_SRV_GET_FAST_CHANNEL_ATTRIBUTES     0x0A

//
// RPMI MAGAGEMENT_MODE Service Group Service IDs
//
#define RPMI_MM_SRV_ENABLE_NOTIFICATION               0x01
#define RPMI_MM_SRV_GET_ATTRIBUTES                    0x02
#define RPMI_MM_SRV_COMMUNICATE                       0x03

//
// RPMI RAS_AGENT Service Group Service IDs
//
#define RPMI_RAS_SRV_ENABLE_NOTIFICATION              0x01
#define RPMI_RAS_SRV_GET_NUM_ERR_SRCS                 0x02
#define RPMI_RAS_SRV_GET_ERR_SRCS_ID_LIST             0x03
#define RPMI_RAS_SRV_GET_ERR_SRC_DESC                 0x04
/* CUSTOM SERVICES */
#define RPMI_RAS_SRV_A2P_REQUEST                      0x05
#define RPMI_RAS_SRV_EINJ_EXECUTE_OPERATION           0x06
#define RPMI_RAS_SRV_EINJ_TRIGGER_ERROR               0x07
#define RPMI_RAS_SRV_EINJ_GET_NUM_INSTRUCTIONS        0x08
#define RPMI_RAS_SRV_EINJ_GET_INSTRUCTION             0x09

//
// RPMI RAS_GET_NUM_ERR_SRCS service response data
//
typedef struct {
  INT32   Status;
  UINT32  NumErrSrcs;
} RPMI_RAS_GET_NUM_ERR_SRCS_RESP;

//
// RPMI RAS_GET_ERR_SRCS_ID_LIST service request data
//
typedef struct {
  UINT32  StartIndex;
} RPMI_RAS_GET_ERR_SRCS_ID_LIST_REQ;

//
// RPMI RAS_GET_ERR_SRCS_ID_LIST service response data
//
typedef struct {
  INT32   Status;
  UINT32  Flags;
  UINT32  Remaining;
  UINT32  Returned;
  UINT32  ErrSrcId[0];
} RPMI_RAS_GET_ERR_SRCS_ID_LIST_RESP;

//
// RPMI RAS_GET_ERR_SRC_DESC service request data
//
typedef struct {
  UINT32  ErrSrcId;
  UINT32  ByteOffset;
} RPMI_RAS_GET_ERR_SRC_DESC_REQ;

//
// RPMI RAS_GET_ERR_SRC_DESC service response data
//
typedef struct {
  INT32   Status;
  UINT32  Flags;
  UINT32  Remaining;
  UINT32  Returned;
  UINT8   ErrSrcDesc[0];
} RPMI_RAS_GET_ERR_SRC_DESC_RESP;

//
// Flags in RPMI RAS_GET_ERR_SRC_DESC service response data
//
#define RPMI_RAS_ERR_SRC_DESC_FLAGS_FORMAT_SHIFT          0
#define RPMI_RAS_ERR_SRC_DESC_FLAGS_FORMAT_MASK           (0xfU << RPMI_RAS_ERR_SRC_DESC_FLAGS_FORMAT_SHIFT)
#define RPMI_RAS_ERR_SRC_DESC_FLAGS_FORMAT_GHESV2         0x0
#define RPMI_RAS_ERR_SRC_DESC_FLAGS_FORMAT_IMPL           0xf

//
// RPMI RAS_EINJ_GET_NUM_INSTRUCTIONS service response data
//
typedef struct {
  INT32   Status;
  UINT32  NumInstructions;
} RPMI_RAS_EINJ_GET_NUM_INSTRUCTIONS_RESP;

//
// RPMI RAS_EINJ_GET_INSTRUCTION service request data
//
typedef struct {
  UINT32  InstructionIndex;
  UINT32  ByteOffset;
} RPMI_RAS_EINJ_GET_INSTRUCTION_REQ;

//
// RPMI RAS_EINJ_GET_INSTRUCTION service response data
//
typedef struct {
  INT32   Status;
  UINT32  Flags;
  UINT32  Remaining;
  UINT32  Returned;
  UINT8   Instruction[0];
} RPMI_RAS_EINJ_GET_INSTRUCTION_RESP;

#define RPMI_CLK_NAME_LEN   16
//
// RPMI CLK_GET_NUM_CLOCKS service response data
//
typedef struct {
  INT32   Status;
  UINT32  NumClocks;
} RPMI_CLK_GET_NUM_CLOCKS_RESP;

//
// RPMI CLK_GET_ATTRIBUTES service request data
//
typedef struct {
  UINT32 ClockId;
} RPMI_CLK_GET_ATTRIBUTES_REQ;

//
// RPMI CLK_GET_ATTRIBUTES service response data
//
typedef struct {
  INT32   Status;
  UINT32  Flags;
  UINT32  NumRates;
  UINT32  TransitionLatency;
  UINT8   ClockName[16];
} RPMI_CLK_GET_ATTRIBUTES_RESP;

//
// RPMI CLK_GET_SUPPORTED_RATES service request data
//
typedef struct {
  UINT32 ClockId;
  UINT32 ClockRateIndex;
} RPMI_CLK_GET_SUPPORTED_RATES_REQ;

//
// RPMI CLK_GET_SUPPORTED_RATES service response data
//
typedef struct {
  INT32   Status;
  UINT32  Flags;
  UINT32  Remaining;
  UINT32  Returned;
  UINT32  ClockRate[0];
} RPMI_CLK_GET_SUPPORTED_RATES_RESP;

//
// RPMI CLK_SET_CONFIG service request data
//
typedef struct {
  UINT32  ClockId;
  UINT32  ClockConfig;
} RPMI_CLK_SET_CONFIG_REQ;

//
// RPMI CLK_SET_CONFIG service response data
//
typedef struct {
  INT32 Status;
} RPMI_CLK_SET_CONFIG_RESP;

//
// RPMI CLK_GET_CONFIG service request data
//
typedef struct {
  UINT32  ClockId;
} RPMI_CLK_GET_CONFIG_REQ;

//
// RPMI CLK_GET_CONFIG service response data
//
typedef struct {
  INT32   Status;
  UINT32  Config;
} RPMI_CLK_GET_CONFIG_RESP;

//
// RPMI CLK_SET_RATE service request data
//
typedef struct {
  UINT32  ClockId;
  UINT32  Flags;
  UINT32  ClockRateLow;
  UINT32  ClockRateHigh;
} RPMI_CLK_SET_RATE_REQ;

//
// RPMI CLK_SET_RATE service response data
//
typedef struct {
  INT32   Status;
} RPMI_CLK_SET_RATE_RESP;

//
// RPMI CLK_GET_RATE service request data
//
typedef struct {
  UINT32  ClockId;
} RPMI_CLK_GET_RATE_REQ;

//
// RPMI CLK_GET_RATE service response data
//
typedef struct {
  INT32   Status;
  UINT32  ClockRateLow;
  UINT32  ClockRateHigh;
} RPMI_CLK_GET_RATE_RESP;

//
// Flags in RPMI CLK_GET_ATTRIBUTES service response data
//
#define RPMI_CLOCK_FORMAT_DISCRETE                0
#define RPMI_CLOCK_FORMAT_LINEAR                  1
#define RPMI_CLOCK_FORMAT_MAX                     2
#define RPMI_CLK_GET_ARTTRIBUTES_FORMAT_SHIFT     0
#define RPMI_CLK_GET_ARTTRIBUTES_FORMAT_MASK      (0x3 << RPMI_CLK_GET_ARTTRIBUTES_FORMAT_SHIFT)

//
// Config in RPMI CLK_GET_CONFIG/CLK_SET_CONFIG service data
//
#define RPMI_CLOCK_CONFIG_DISABLE   0
#define RPMI_CLOCK_CONFIG_ENABLE    1
#define RPMI_CLK_CONFIG_SHIFT       0
#define RPMI_CLK_CONFIG_MASK        (0x1 << RPMI_CLK_CONFIG_SHIFT)

#pragma pack()

#endif /* ifndef __RPMI_H__ */
