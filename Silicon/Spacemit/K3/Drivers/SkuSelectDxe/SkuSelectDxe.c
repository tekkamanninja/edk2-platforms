/** @file

  Read the DTB root "model" property and call LibPcdSetSku() to select
  the correct PCD SKU before any SKU-sensitive driver runs.

  SKU mapping:
    model contains "com260"   ->  SKU 1 (COM260)
    model contains "fml13v05" ->  SKU 2 (FML13V05)
    anything else             ->  SKU 0 (DEFAULT)

  Copyright (c) 2025, Spacemit Corporation

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <Guid/Fdt.h>
#include <Guid/FdtHob.h>

#define SKU_ID_DEFAULT  0
#define SKU_ID_COM260   1
#define SKU_ID_FML13V05 2

STATIC
CONST VOID *
GetFdtBase (
  VOID
  )
{
  UINTN   Index;
  VOID    *FdtBase;
  VOID    *Hob;
  UINT64  FdtAddress;

  //
  // Prefer the configuration table entry installed by FdtDxe.
  //
  if ((gST != NULL) && (gST->ConfigurationTable != NULL)) {
    for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
      if (CompareGuid (&gST->ConfigurationTable[Index].VendorGuid, &gFdtTableGuid)) {
        FdtBase = gST->ConfigurationTable[Index].VendorTable;
        if ((FdtBase != NULL) && (FdtCheckHeader (FdtBase) == 0)) {
          return FdtBase;
        }

        return NULL;
      }
    }
  }

  //
  // Fall back to the HOB left by SEC/PEI.
  //
  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64))) {
    return NULL;
  }

  FdtAddress = *((UINT64 *)GET_GUID_HOB_DATA (Hob));
  FdtBase    = (VOID *)(UINTN)FdtAddress;
  if (FdtCheckHeader (FdtBase) != 0) {
    return NULL;
  }

  return FdtBase;
}

EFI_STATUS
EFIAPI
SkuSelectDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  CONST VOID   *Fdt;
  INTN         RootOffset;
  INT32        Len;
  CONST CHAR8  *Model;
  UINTN        SkuId;

  Fdt = GetFdtBase ();
  if (Fdt == NULL) {
    DEBUG ((DEBUG_WARN, "%a: DTB not found, using DEFAULT SKU\n", __func__));
    LibPcdSetSku (SKU_ID_DEFAULT);
    return EFI_SUCCESS;
  }

  RootOffset = FdtPathOffset (Fdt, "/");
  if (RootOffset < 0) {
    DEBUG ((DEBUG_WARN, "%a: DTB root node not found, using DEFAULT SKU\n", __func__));
    LibPcdSetSku (SKU_ID_DEFAULT);
    return EFI_SUCCESS;
  }

  Model = FdtGetProp (Fdt, RootOffset, "model", &Len);
  if ((Model == NULL) || (Len <= 0)) {
    DEBUG ((DEBUG_WARN, "%a: DTB model property not found, using DEFAULT SKU\n", __func__));
    LibPcdSetSku (SKU_ID_DEFAULT);
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "%a: DTB model = \"%a\"\n", __func__, Model));

  if (AsciiStrStr (Model, "k3_com260") != NULL) {
    SkuId = SKU_ID_COM260;
    DEBUG ((DEBUG_INFO, "%a: SKU set to COM260 (%u)\n", __func__, SkuId));
  } else if (AsciiStrStr (Model, "k3-deepcomputing-fml13v05") != NULL) {
    SkuId = SKU_ID_FML13V05;
    DEBUG ((DEBUG_INFO, "%a: SKU set to FML13V05 (%u)\n", __func__, SkuId));
  } else {
    SkuId = SKU_ID_DEFAULT;
    DEBUG ((DEBUG_INFO, "%a: SKU set to DEFAULT (%u)\n", __func__, SkuId));
  }

  LibPcdSetSku (SkuId);
  return EFI_SUCCESS;
}
