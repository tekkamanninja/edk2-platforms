/** @file
  Provides FDT helper interfaces based on FdtLib.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Library/FdtHelperLib.h>
#include "FdtHelperLibInternal.h"

STATIC
EFI_STATUS
CreateCpuMapInfo (
  IN    UINTN                         Id,
  IN    FDT_CPU_MAP_HIERARCHY_TYPE    HierarchyType,
  IN    UINTN                         HierarchyLevel,
  OUT   FDT_CPU_MAP_INFO              **CpuMapInfo
  )
{
  FDT_CPU_MAP_INFO    *Info;

  Info = AllocateZeroPool (sizeof (FDT_CPU_MAP_INFO));
  if (Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Info->Id = Id;
  Info->HierarchyType = HierarchyType;
  Info->HierarchyLevel = HierarchyLevel;
  InitializeListHead (&Info->ChildList);

  *CpuMapInfo = Info;

  return EFI_SUCCESS;
}

STATIC
VOID
FreeCpuMapInfoListEntries (
  IN OUT    LIST_ENTRY    *CpuMapInfoList
  )
{
  LIST_ENTRY            *Entry;
  LIST_ENTRY            *NextEntry;
  FDT_CPU_MAP_INFO      *CpuMapInfo;

  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, CpuMapInfoList) {
    CpuMapInfo = LIST_ENTRY_TO_FDT_CPU_MAP_INFO (Entry);

    if (!IsListEmpty (&CpuMapInfo->ChildList)) {
      FreeCpuMapInfoListEntries (&CpuMapInfo->ChildList);
    }

    RemoveEntryList (Entry);
    FreePool (CpuMapInfo);
  }
}

/**
  Get the CPU ID for the node which has "cpu" property.

  @param  Fdt             The poiner to FDT blob.
  @param  NodeOffset      The offset to the given node.
  @param  CpuId           The CPU ID got.

  @retval EFI_SUCCESS     Succeed.
  @retval EFI_NOT_FOUND   The node doesn't have "cpu" property.
  @rerval EFI_NOT_READY   The target CPU node is disabled.
  @retval Other           Other errors.

**/
STATIC
EFI_STATUS
GetCpuIdForNode (
  IN CONST  VOID      *Fdt,
  IN        INTN      NodeOffset,
  OUT       UINTN     *CpuId
  )
{
  EFI_STATUS        Status;
  CONST UINT32      *Val;
  INT32             Len;
  UINT32            Phandle;
  INT32             CpuNodeOffset;
  UINT64            Id;

  Val = (CONST UINT32 *) FdtGetProp (Fdt, NodeOffset, "cpu", &Len);
  if (Val == NULL || Len <= 0) {
    return EFI_NOT_FOUND;
  }

  Phandle = Fdt32ToCpu (*Val);
  CpuNodeOffset = FdtNodeOffsetByPhandle (Fdt, Phandle);
  if (CpuNodeOffset < 0) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid \"cpu\" phandle in FDT %a\n",
            __func__, FdtGetName ((VOID *) Fdt, NodeOffset, NULL)));
    return EFI_UNSUPPORTED;
  }

  Status = FdtGetNodeAddrSize (Fdt, CpuNodeOffset, 0, &Id, NULL);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid \"reg\" in FDT %a\n",
            __func__, FdtGetName ((VOID *) Fdt, CpuNodeOffset, NULL)));
    return EFI_UNSUPPORTED;
  }

  *CpuId = Id;

  if (!FdtNodeIsEnabled (Fdt, CpuNodeOffset)) {
    return EFI_NOT_READY;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FdtParseCpuMapChildNodes (
  IN CONST  VOID                          *Fdt,
  IN        INTN                          NodeOffset,
  IN        FDT_CPU_MAP_HIERARCHY_TYPE    ExpectedHierarchyType,
  IN        UINTN                         HierarchyLevel,
  IN OUT    LIST_ENTRY                    *CpuMapInfoList
  )
{
  EFI_STATUS                    Status;
  BOOLEAN                       IsLeaf;
  BOOLEAN                       LeafIsEnabled;
  INT32                         ChildOffset;
  INT32                         Len;
  CONST CHAR8                   *ChildName;
  CONST CHAR8                   *IdStr;
  UINTN                         CpuId;
  FDT_CPU_MAP_HIERARCHY_TYPE    HierarchyType;
  FDT_CPU_MAP_HIERARCHY_TYPE    ChildHierarchyType;
  FDT_CPU_MAP_INFO              *Info;
  LIST_ENTRY                    *Entry;
  FDT_CPU_MAP_INFO              *ChildInfo;

  FdtForEachSubnode (ChildOffset, Fdt, NodeOffset) {
    IsLeaf = FALSE;
    LeafIsEnabled = FALSE;

    ChildName = FdtGetName ((VOID *) Fdt, ChildOffset, &Len);
    if (ChildName == NULL || Len <= 0) {
      Status = EFI_UNSUPPORTED;
      goto FreeCpuMapInfoListEntries;
    }

    HierarchyType = ExpectedHierarchyType;
    switch (HierarchyType) {
      case FdtCpuMapHierarchyTypeSocket:
        if (0 == AsciiStrnCmp (ChildName, "socket", 6)) {
          IdStr = ChildName + 6;
          ChildHierarchyType = FdtCpuMapHierarchyTypeCluster;
        } else if (0 == AsciiStrnCmp (ChildName, "cluster", 7)) {
          HierarchyType = FdtCpuMapHierarchyTypeCluster;
          IdStr = ChildName + 7;
          // Child nodes of cluster can be still clusters.
          ChildHierarchyType = FdtCpuMapHierarchyTypeCluster;
        } else {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Invalid node name \"%a\" in FDT cpu-map. Expects \"socketXX\" or \"clusterXX\"\n",
            __func__,
            ChildName
            ));
          Status = EFI_UNSUPPORTED;
          goto FreeCpuMapInfoListEntries;
        }
        break;

      case FdtCpuMapHierarchyTypeCluster:
        if (0 == AsciiStrnCmp (ChildName, "cluster", 7)) {
          IdStr = ChildName + 7;
          ChildHierarchyType = FdtCpuMapHierarchyTypeCluster;
        } else if (0 == AsciiStrnCmp (ChildName, "core", 4)) {
          HierarchyType = FdtCpuMapHierarchyTypeCore;
        } else {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Invalid node name \"%a\" in FDT cpu-map. Expects \"clusterXX\" or \"coreXX\"\n",
            __func__,
            ChildName
            ));
          Status = EFI_UNSUPPORTED;
          goto FreeCpuMapInfoListEntries;
        }
        // If this is not a core node, break. Otherwise fall-through to the next case.
        if (HierarchyType == FdtCpuMapHierarchyTypeCluster) {
          break;
        }

      case FdtCpuMapHierarchyTypeCore:
        //
        // Not need to check node name here. This Core case can only be entered
        // by fall-through from the Cluster case.
        //

        IdStr = ChildName + 4;
        ChildHierarchyType = FdtCpuMapHierarchyTypeThread;

        //
        // If this core node has "cpu" property, it means it is a leaf node.
        //
        Status = GetCpuIdForNode (Fdt, ChildOffset, &CpuId);
        if (Status == EFI_SUCCESS) {
          // The node has "cpu" property and we got the cpu id successfully.
          IsLeaf = TRUE;
          LeafIsEnabled = TRUE;
        } else if (Status == EFI_NOT_READY) {
          // The node has "cpu" property but the target cpu node is disabled.
          IsLeaf = TRUE;
          LeafIsEnabled = FALSE;
        } else if (Status == EFI_NOT_FOUND) {
          // The node doesn't have "cpu" property.
          IsLeaf = FALSE;
        } else {
          DEBUG ((DEBUG_ERROR, "%a: Invalid core node %a in FDT cpu-map\n", __func__, ChildName));
          goto FreeCpuMapInfoListEntries;
        }
        break;

      case FdtCpuMapHierarchyTypeThread:
        IdStr = ChildName + 6;
        IsLeaf = TRUE;
        Status = GetCpuIdForNode (Fdt, ChildOffset, &CpuId);
        if (Status == EFI_SUCCESS) {
          // Got the cpu id successfully.
          LeafIsEnabled = TRUE;
        } else if (Status == EFI_NOT_READY) {
          // The target cpu node is disabled.
          LeafIsEnabled = FALSE;
        } else {
          DEBUG ((DEBUG_ERROR,
                  "%a: Failed to get CPU ID for thread node %a in FDT cpu-map\n",
                  __func__,
                  ChildName));
          goto FreeCpuMapInfoListEntries;
        }
        break;

      default:
        DEBUG ((DEBUG_ERROR, "%a: Invalid node type in FDT cpu-map\n", __func__));
        Status = EFI_UNSUPPORTED;
        goto FreeCpuMapInfoListEntries;
        break;
    }

    if (IsLeaf) {
      //
      // Leaf node
      //

      Status = CreateCpuMapInfo (CpuId, HierarchyType, HierarchyLevel, &Info);
      if (Status != EFI_SUCCESS) {
        goto FreeCpuMapInfoListEntries;
      }
      Info->IsEnabled = LeafIsEnabled;
      InsertTailList (CpuMapInfoList, &Info->Link);

    } else {
      //
      // Non-leaf node
      //

      Status = CreateCpuMapInfo (AsciiStrDecimalToUintn (IdStr),
                                 HierarchyType,
                                 HierarchyLevel,
                                 &Info);
      if (Status != EFI_SUCCESS) {
        goto FreeCpuMapInfoListEntries;
      }

      //
      // Recursively parse its child nodes.
      //
      Status = FdtParseCpuMapChildNodes (Fdt,
                                         ChildOffset,
                                         ChildHierarchyType,
                                         HierarchyLevel + 1,
                                         &Info->ChildList);
      if (Status != EFI_SUCCESS) {
        FreePool (Info);
        goto FreeCpuMapInfoListEntries;
      }

      //
      // For a non-leaf node, if it doesn't have any enabled child node, set
      // it disabled.
      //
      Info->IsEnabled = FALSE;
      BASE_LIST_FOR_EACH (Entry, &Info->ChildList) {
        ChildInfo = LIST_ENTRY_TO_FDT_CPU_MAP_INFO (Entry);
        if (ChildInfo->IsEnabled) {
          Info->IsEnabled = TRUE;
          break;
        }
      }

      InsertTailList (CpuMapInfoList, &Info->Link);
    }
  }

  return EFI_SUCCESS;

FreeCpuMapInfoListEntries:
  FreeCpuMapInfoListEntries (CpuMapInfoList);
  return Status;
}

/**
  Get information from the FDT cpu-map node.

  @param  Fdt               The poiner to FDT blob.
  @param  CpuMapNodeOffset  The offset to the cpu-map node.
  @param  CpuMapInfoList    Pointer to a list storing the cpu-map information.
                            It should be released via FdtReleaseCpuMapInfoList()
                            when it is no longer needed.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtGetCpuMapInfo (
  IN CONST  VOID          *Fdt,
  IN        INTN          CpuMapNodeOffset,
  OUT       LIST_ENTRY    **CpuMapInfoList
  )
{
  EFI_STATUS      Status;
  LIST_ENTRY      *InfoList;

  InfoList = AllocateZeroPool (sizeof (LIST_ENTRY));
  if (InfoList == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrOut;
  }
  InitializeListHead (InfoList);

  Status = FdtParseCpuMapChildNodes (Fdt,
                                     CpuMapNodeOffset,
                                     FdtCpuMapHierarchyTypeSocket,
                                     1,
                                     InfoList);
  if (Status != EFI_SUCCESS) {
    goto FreeInfoList;
  }

  *CpuMapInfoList = InfoList;

  return EFI_SUCCESS;

FreeInfoList:
  FreeCpuMapInfoListEntries (InfoList);
  FreePool (InfoList);
ErrOut:
  return Status;
}

/**
  Release the information list got via FdtGetCpuMapInfo().

  @param  CpuMapInfoList  Pointer to the information list got via FdtGetCpuMapInfo().

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtReleaseCpuMapInfo (
  IN OUT    LIST_ENTRY    *CpuMapInfoList
  )
{
  FreeCpuMapInfoListEntries (CpuMapInfoList);
  FreePool (CpuMapInfoList);

  return EFI_SUCCESS;
}
