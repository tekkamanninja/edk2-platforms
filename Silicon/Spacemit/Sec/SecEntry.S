/*
  Copyright (c) 2022 Ventana Micro Systems Inc.
  Copyright (c) 2024 SpacemiT Co., Ltd.

  SPDX-License-Identifier: BSD-2-Clause-Patent

 */

#include <Register/RiscV64/RiscVImpl.h>
#include <Library/PcdLib.h>
#include "SecMain.h"

ASM_FUNC (_ModuleEntryPoint)
  /* Use Temp memory as the stack for calling to C code */
  li    a4, FixedPcdGet64 (PcdSecStackBase)
  li    a5, FixedPcdGet64 (PcdSecStackSize)

  /* Use Temp memory as the stack for calling to C code */
  add   sp, a4, a5

  call SecStartup
