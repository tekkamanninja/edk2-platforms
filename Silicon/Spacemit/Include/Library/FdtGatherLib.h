/** @file
  Provides interfaces about gathering information from FDT.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FDT_GATHER_LIB_H__
#define __FDT_GATHER_LIB_H__

#include <Base.h>

typedef struct _FDT_GATHER_ENTITY FDT_GATHER_ENTITY;

/**
  Get information from FDT.

  @param  This            Pointer to this FDT_GATHER entity.
  @param  FdtBase         FDT base address.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
typedef
EFI_STATUS
(EFIAPI *FDT_GATHER_OP_GET_INFO) (
  IN          FDT_GATHER_ENTITY         *This,
  IN  CONST   VOID                      *FdtBase
  );

//
// FDT_GATHER entity operations
//
typedef struct {
  FDT_GATHER_OP_GET_INFO          GetInfo;
} FDT_GATHER_OPS;

//
// FDT_GATHER entity
//
struct _FDT_GATHER_ENTITY {
  LIST_ENTRY            Link;

  CONST CHAR16          *Name;
  FDT_GATHER_OPS        *Ops;

  VOID                  *Private;
};

/**
  Get FDT base address form HOB.

  @param    VOID

  @retval   The FDT base address on success, otherwise NULL on failure.

**/
VOID *
EFIAPI
FdtGetBaseFromHob (
  VOID
  );

/**
  Register a FDT_GATHER entity.

  @param  Name          Entity name (Unicode string).
  @param  Ops           Pointer to entity operations.
  @param  Private       Pointer to entity private data.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
FdtGatherRegisterEntity (
  IN CONST  CHAR16                *Name,
  IN        FDT_GATHER_OPS        *Ops,
  IN        VOID                  *Private
  );

/**
  Deregister a FDT_GATHER entity.

  @param  Name            Entity name (Unicode string).

  @retval EFI_SUCCESS     Succeed.
  @retval EFI_NOT_FOUND   Entity with this name not found.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtGatherDeregisterEntity (
  IN CONST  CHAR16    *Name
  );

/**
  Iterate all FDT_GATHER entites and execute their GetInfo() operations.

  @param  FdtBase         FDT base address.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtGatherExecuteGetInfo (
  IN CONST  VOID    *FdtBase
  );

#endif /* ifndef __FDT_GATHER_LIB_H__ */
