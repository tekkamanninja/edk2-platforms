/** @file
*
*  Copyright (c) 2026, Spacemit Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryManagementLib.h>
#include <Protocol/SpacemitEfuse.h>

#include "PlatformInfoDxe.h"

STATIC const TLV_INFO_CONFIG  ConfigInfo[] = {
  { "product_name",     TLV_CODE_PRODUCT_NAME,   TlvDataString    },
  { "part#",            TLV_CODE_PART_NUMBER,    TlvDataString    },
  { "serial#",          TLV_CODE_SERIAL_NUMBER,  TlvDataString    },
  { "manufacture_date", TLV_CODE_MANUF_DATE,     TlvDataString    },
  { "manufacturer",     TLV_CODE_MANUF_NAME,     TlvDataString    },
  { "ddr_type",         TLV_CODE_DDR_TYPE,       TlvDataString    },
  { "ddr_cs_num",       TLV_CODE_DDR_CSNUM,      TlvDataUint8     },
  { "ddr_tx_odt",       TLV_CODE_DDR_TX_ODT,     TlvDataUint8     },
  { "ddr_datarate",     TLV_CODE_DDR_DATARATE,   TlvDataUint16    },
  { "ethaddr",          TLV_CODE_MAC_BASE,       TlvDataStructure },
  { "wifi_addr",        TLV_CODE_WIFI_MAC_ADDR,  TlvDataStructure },
  { "bt_addr",          TLV_CODE_BLUETOOTH_ADDR, TlvDataStructure }
};

STATIC
EFI_STATUS
ReadInfoFromTLV (
  IN PLATFORM_INFO_PROTOCOL  *This,
  IN CHAR8                   *Name,
  OUT VOID                   *Info,
  IN UINTN                   MaxSize
  )
{
  UINT8                   Temp;
  UINT32                  I, Tid;
  EFI_STATUS              Status        = EFI_NOT_FOUND;
  PLATFROM_INFO_INSTANCE  *InfoInstance = PLATFROM_INFO_INSTANCE_FROM_THIS (This);

  for (I = 0; I < ARRAY_SIZE (ConfigInfo); I++) {
    if (0 == AsciiStrCmp (Name, ConfigInfo[I].Name)) {
      Tid    = ConfigInfo[I].Tid;
      Status = InfoInstance->TlvInfoProtocol->GetTlvInfo (
                                                InfoInstance->TlvInfoProtocol,
                                                Tid,
                                                Info,
                                                MaxSize
                                                );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed(%r) to get TLV %a\n", Status, Name));
      } else if ((TlvDataUint16 == ConfigInfo[I].Type) && (MaxSize >= sizeof (UINT16))) {
        // convert it from big endian to little endian
        Temp               = ((UINT8 *)Info)[0];
        ((UINT8 *)Info)[0] = ((UINT8 *)Info)[1];
        ((UINT8 *)Info)[1] = Temp;
      }

      break;
    }
  }

  return Status;
}

STATIC
EFI_STATUS
GetMemLayoutInfo (
  IN PLATFORM_INFO_PROTOCOL  *This,
  IN CHAR8                   *Name,
  OUT VOID                   *Info,
  IN UINTN                   MaxItem
  )
{
  EFI_PEI_HOB_POINTERS  Hob;
  UINT64                I;
  MEMORY_LAYOUT_INFO    *MemLayout;
  EFI_STATUS            Status = EFI_NOT_FOUND;

  if (NULL == Info) {
    return EFI_INVALID_PARAMETER;
  }

  if (MaxItem < 1) {
    return EFI_BUFFER_TOO_SMALL;
  }

  //
  // Get the system memory bank info from memory hobs
  //
  Hob.Raw = GetFirstHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR);
  ASSERT (Hob.Raw != NULL);

  I         = 0;
  MemLayout = (MEMORY_LAYOUT_INFO *)Info;
  while ((Hob.Raw != NULL) && (!END_OF_HOB_LIST (Hob)) && (I < (MaxItem - 1))) {
    if (Hob.ResourceDescriptor->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
      // update memory size
      MemLayout[I].PhysicalAddress = Hob.ResourceDescriptor->PhysicalStart;
      MemLayout[I].PhysicalSize    = Hob.ResourceDescriptor->ResourceLength;
      DEBUG (
        (DEBUG_VERBOSE, "Memory node: base=0x%lx, size=0x%lx\n",
         MemLayout[I].PhysicalAddress, MemLayout[I].PhysicalSize)
        );
      I++;
      Status = EFI_SUCCESS;
    }

    Hob.Raw = GET_NEXT_HOB (Hob);
    Hob.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, Hob.Raw);
  }

  MemLayout[I].PhysicalAddress = 0;
  MemLayout[I].PhysicalSize    = 0;
  return Status;
}

STATIC
EFI_STATUS
ReadInfoFromEfuse (
  IN PLATFORM_INFO_PROTOCOL  *This,
  IN CHAR8                   *Name,
  OUT VOID                   *Info,
  IN UINTN                   MaxSize
  )
{
  EFI_STATUS               Status;
  SPACEMIT_EFUSE_PROTOCOL  *Efuse;

  Status = gBS->LocateProtocol (
                  &gSpacemitEfuseProtocolGuid,
                  NULL,
                  (VOID **)&Efuse
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate eFuse protocol: %r\n", __func__, Status));
    return Status;
  }

  return Efuse->Read (Efuse, Name, Info, (UINT32)MaxSize);
}

STATIC CONST SPACEMIT_PLATFROM_INFO  PlatformInfo[] = {
  { "product_name",     ReadInfoFromTLV,   NULL },
  { "serial#",          ReadInfoFromTLV,   NULL },
  { "part#",            ReadInfoFromTLV,   NULL },
  { "manufacture_date", ReadInfoFromTLV,   NULL },
  { "manufacturer",     ReadInfoFromTLV,   NULL },
  { "ddr_type",         ReadInfoFromTLV,   NULL },
  { "ddr_cs_num",       ReadInfoFromTLV,   NULL },
  { "ddr_datarate",     ReadInfoFromTLV,   NULL },
  { "ddr_tx_odt",       ReadInfoFromTLV,   NULL },
  { "ethaddr",          ReadInfoFromTLV,   NULL },
  { "wifi_addr",        ReadInfoFromTLV,   NULL },
  { "bt_addr",          ReadInfoFromTLV,   NULL },
  { "mem_layout",       GetMemLayoutInfo,  NULL },
  { "wafer_id",         ReadInfoFromEfuse, NULL },
  { "product_id",       ReadInfoFromEfuse, NULL },
  { "svt_dro",          ReadInfoFromEfuse, NULL }
};

STATIC
EFI_STATUS
GetK3PlatformInfo (
  IN PLATFORM_INFO_PROTOCOL  *This,
  IN CHAR8                   *Name,
  OUT VOID                   *Info,
  IN UINTN                   MaxSize
  )
{
  INTN  I;

  if ((Name == NULL) || (Info == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (I = 0; I < ARRAY_SIZE (PlatformInfo); I++) {
    if ((0 == AsciiStrCmp (Name, PlatformInfo[I].Name)) &&
        (NULL != PlatformInfo[I].ReadInfo))
    {
      SetMem (Info, MaxSize, 0);
      return PlatformInfo[I].ReadInfo (This, Name, Info, MaxSize);
    }
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
SetK3PlatformInfo (
  IN PLATFORM_INFO_PROTOCOL  *This,
  IN CHAR8                   *Name,
  IN VOID                    *Info,
  IN UINTN                   InfoSize
  )
{
  INTN  I;

  if ((Name == NULL) || (Info == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (I = 0; I < ARRAY_SIZE (PlatformInfo); I++) {
    if ((0 == AsciiStrCmp (Name, PlatformInfo[I].Name)) &&
        (NULL != PlatformInfo[I].SetInfo))
    {
      return PlatformInfo[I].SetInfo (This, Name, Info, InfoSize);
    }
  }

  return EFI_NOT_FOUND;
}

STATIC
VOID
PlatformMmioRemap (
  VOID
  )
{
}

EFI_STATUS
EFIAPI
SpacemitK3PlatformInfoEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  PLATFROM_INFO_INSTANCE      *InfoInstance;
  SPACEMIT_TLV_INFO_PROTOCOL  *Tlv;

  // Locate TLV protocol (guaranteed present by DEPEX)
  Status = gBS->LocateProtocol (
                  &gSpacemitTlvInfoProtocolGuid,
                  NULL,
                  (VOID **)&Tlv
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate TLV protocol: %r\n", __func__, Status));
    return Status;
  }

  InfoInstance = AllocateZeroPool (sizeof (PLATFROM_INFO_INSTANCE));
  if (InfoInstance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  InfoInstance->Signature                    = PLATFROM_INFO_SIGNATURE;
  InfoInstance->PlatformInfo.GetPlatformInfo = GetK3PlatformInfo;
  InfoInstance->PlatformInfo.SetPlatformInfo = SetK3PlatformInfo;
  InfoInstance->TlvInfoProtocol              = Tlv;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &InfoInstance->Handle,
                  &gSpacemitPlatformInfoProtocolGuid,
                  &InfoInstance->PlatformInfo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install PlatformInfo protocol: %r\n", __func__, Status));
    FreePool (InfoInstance);
    return Status;
  }

  PlatformMmioRemap ();
  return EFI_SUCCESS;
}
