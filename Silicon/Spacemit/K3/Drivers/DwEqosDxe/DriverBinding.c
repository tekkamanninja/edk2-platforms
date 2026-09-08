/** @file

  Copyright (c) 2025, Spacemit Corporation

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/NetLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/PlatformInfo.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/NonDiscoverableDeviceRegistrationLib.h>
#include <Library/PcdLib.h>
#include "DwEqosDxeUtil.h"

//
// Per-controller non-discoverable device state
//
typedef struct {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR    MemDesc;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR    EndDesc;
} EQOS_ACPI_RES;

STATIC UINTN                    mEqosNdCount    = 0;
STATIC EFI_HANDLE               *mEqosNdHandles = NULL;
STATIC EQOS_ACPI_RES            *mEqosResArray  = NULL;
STATIC NON_DISCOVERABLE_DEVICE  *mEqosDevDescs  = NULL;

STATIC
EFI_STATUS
EFIAPI
DriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

STATIC
EFI_STATUS
EFIAPI
DriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

STATIC
EFI_STATUS
EFIAPI
DriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  );

EFI_DRIVER_BINDING_PROTOCOL  gEqosDriverBinding = {
  DriverSupported,
  DriverStart,
  DriverStop,
  0xa,
  NULL,
  NULL
};

STATIC SIMPLE_NETWORK_DEVICE_PATH  PathTemplate = {
  {
    {
      MESSAGING_DEVICE_PATH,
      MSG_MAC_ADDR_DP,
      {
        (UINT8)(sizeof (MAC_ADDR_DEVICE_PATH)),
        (UINT8)((sizeof (MAC_ADDR_DEVICE_PATH)) >> 8)
      }
    },
    {
      {
        0
      }
    },
    0
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 }
  }
};

/**
  ExitBootServices notification to stop the network controller.

  @param  Event      Pointer to this event.
  @param  Context    Event handler private data.

**/
STATIC
VOID
EFIAPI
EqosNotifyExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EQOS_DEVICE  *Eqos = (EQOS_DEVICE *)Context;

  EqosStop (Eqos);

  return;
}

STATIC
BOOLEAN
IsValidMac (
  IN CONST EFI_MAC_ADDRESS  *Mac
  )
{
  CONST UINT8  *MacOctets = (CONST UINT8 *)Mac;
  UINT8        OrAll;
  UINT8        AndAll;

  //
  // Reject all-zeros
  //
  OrAll = (UINT8)(MacOctets[0] | MacOctets[1] | MacOctets[2] |
                  MacOctets[3] | MacOctets[4] | MacOctets[5]);
  if (OrAll == 0) {
    return FALSE;
  }

  //
  // Reject all-FFs
  //
  AndAll = (UINT8)(MacOctets[0] & MacOctets[1] & MacOctets[2] &
                   MacOctets[3] & MacOctets[4] & MacOctets[5]);
  if (AndAll == 0xFF) {
    return FALSE;
  }

  //
  // Reject multicast (LSB of first byte == 1)
  //
  if ((MacOctets[0] & 0x01) != 0) {
    return FALSE;
  }

  return TRUE;
}

STATIC
EFI_STATUS
ReadMacAddress (
  OUT EFI_MAC_ADDRESS  *Mac
  )
{
  EFI_STATUS              Status;
  PLATFORM_INFO_PROTOCOL  *PlatformInfo;

  if (Mac == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Locate the PlatformInfo protocol
  Status = gBS->LocateProtocol (
                  &gSpacemitPlatformInfoProtocolGuid,
                  NULL,
                  (void **)&PlatformInfo
                  );
  if (EFI_ERROR (Status) || (PlatformInfo == NULL)) {
    return EFI_NOT_FOUND;
  }

  if (!EFI_ERROR (
         PlatformInfo->GetPlatformInfo (
                         PlatformInfo,
                         "ethaddr",
                         Mac,
                         sizeof (EFI_MAC_ADDRESS)
                         )
         ))
  {
    return EFI_SUCCESS;
  }

  CopyMem (Mac, PcdGetPtr (PcdDwGmacDefaultMacAddress), sizeof (EFI_MAC_ADDRESS));
  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
GenerateFallbackMac (
  OUT EFI_MAC_ADDRESS  *Mac
  )
{
  UINT64  MacSeed;

  if (Mac == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Use timestamp + address noise as a simple entropy source (good enough for fallback)
  MacSeed  = (UINT64)GetTimeInNanoSecond (GetPerformanceCounter ());
  MacSeed ^= (UINT64)(UINTN)Mac;
  MacSeed ^= (UINT64)(UINTN)&MacSeed;

  ((UINT8 *)Mac)[0] = 0xFE;
  ((UINT8 *)Mac)[1] = 0xFE;
  ((UINT8 *)Mac)[2] = 0xFE;
  ((UINT8 *)Mac)[3] = (UINT8)(MacSeed >> 0);
  ((UINT8 *)Mac)[4] = (UINT8)(MacSeed >> 8);
  ((UINT8 *)Mac)[5] = (UINT8)(MacSeed >> 16);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetPlatformMacAddress (
  OUT EFI_MAC_ADDRESS  *Mac
  )
{
  EFI_STATUS  Status;

  Status = ReadMacAddress (Mac);
  if (!EFI_ERROR (Status) && IsValidMac (Mac)) {
    return EFI_SUCCESS;
  }

  DEBUG (
    (
     DEBUG_WARN,
     "MAC: EEPROM TLV read/validate failed (%r), using fallback random MAC\n",
     Status
    )
    );

  GenerateFallbackMac (Mac);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
DriverSupported (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS               Status;
  NON_DISCOVERABLE_DEVICE  *Dev;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Dev,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!CompareGuid (Dev->Type, &gSpacemitDwGmacNonDiscoverableDeviceGuid)) {
    Status = EFI_UNSUPPORTED;
  }

  gBS->CloseProtocol (
         Controller,
         &gEdkiiNonDiscoverableDeviceProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
DriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath   OPTIONAL
  )
{
  EFI_STATUS                            Status;
  EQOS_DEVICE                           *Eqos;
  NON_DISCOVERABLE_DEVICE               *Dev;
  SIMPLE_NETWORK_DEVICE_PATH            *DevicePath;
  UINT64                                Base;
  DW_EQOS_CONTROLLER_CONFIGS            *Configs;
  CONST DW_EQOS_CONTROLLER_CONFIG_DATA  *CfgData;
  UINT16                                I;

  Eqos       = NULL;
  DevicePath = NULL;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Dev,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: OpenProtocol failed: %r\n", __func__, Status));
    return Status;
  }

  //
  // Identify which controller this is by matching the MMIO base from the
  // device resources against the PCD config table.
  //
  Base    = Dev->Resources[0].AddrRangeMin;
  Configs = (DW_EQOS_CONTROLLER_CONFIGS *)PcdGetPtr (PcdDwEqosControllerConfigs);
  CfgData = NULL;
  if (Configs != NULL) {
    for (I = 0; I < Configs->Num; I++) {
      if (Configs->Data[I].Base == Base) {
        CfgData = &Configs->Data[I];
        break;
      }
    }
  }

  if (CfgData == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No config found for Base=0x%lx\n", __func__, Base));
    Status = EFI_NOT_FOUND;
    goto ErrCloseProtocol;
  }

  Eqos = AllocateZeroPool (sizeof (EQOS_DEVICE));
  if (Eqos == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate device context!\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrCloseProtocol;
  }

  Eqos->Signature        = EQOS_DRIVER_SIGNATURE;
  Eqos->ControllerConfig = CfgData;
  Eqos->Base             = Base;
  MapRegToGcdMmioSpace (Eqos->Base, EQOS_MAC_DEVICE_SIZE);

  EfiInitializeLock (&Eqos->Lock, TPL_CALLBACK);

  CopyMem (&Eqos->Snp, &gEqosSnpTemplate, sizeof Eqos->Snp);

  Eqos->Snp.Mode                  = &Eqos->SnpMode;
  Eqos->SnpMode.State             = EfiSimpleNetworkStopped;
  Eqos->SnpMode.HwAddressSize     = NET_ETHER_ADDR_LEN;
  Eqos->SnpMode.MediaHeaderSize   = sizeof (ETHER_HEAD);
  Eqos->SnpMode.MaxPacketSize     = EQOS_MAX_PACKET_SIZE;
  Eqos->SnpMode.NvRamSize         = 0;
  Eqos->SnpMode.NvRamAccessSize   = 0;
  Eqos->SnpMode.ReceiveFilterMask = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
                                    EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST |
                                    EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST |
                                    EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS |
                                    EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST;

  // We do not intend to receive anything for the time being.
  Eqos->SnpMode.ReceiveFilterSetting = 0;

  Eqos->SnpMode.MaxMCastFilterCount = MAX_MCAST_FILTER_CNT;
  Eqos->SnpMode.MCastFilterCount    = 0;
  ZeroMem (&Eqos->SnpMode.MCastFilter, MAX_MCAST_FILTER_CNT * sizeof (EFI_MAC_ADDRESS));

  Eqos->SnpMode.IfType                = NET_IFTYPE_ETHERNET;
  Eqos->SnpMode.MacAddressChangeable  = TRUE;
  Eqos->SnpMode.MultipleTxSupported   = FALSE;
  Eqos->SnpMode.MediaPresentSupported = TRUE;
  Eqos->SnpMode.MediaPresent          = FALSE;

  SetMem (&Eqos->SnpMode.BroadcastAddress, sizeof (EFI_MAC_ADDRESS), 0xFF);

  GetPlatformMacAddress (&Eqos->SnpMode.PermanentAddress);
  CopyMem (&Eqos->SnpMode.CurrentAddress, &Eqos->SnpMode.PermanentAddress, NET_ETHER_ADDR_LEN);

  Status = EqosInit (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR, "%a: Failed to init Eqos device. Status=%r\n",
       __func__,
       Status
      )
      );
    goto ErrFreeDevice;
  }

  DevicePath = (SIMPLE_NETWORK_DEVICE_PATH *)AllocateCopyPool (sizeof (SIMPLE_NETWORK_DEVICE_PATH), &PathTemplate);
  if (DevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrDeviceDeinit;
  }

  // Assign fields for device path
  CopyMem (&DevicePath->MacAddrDP.MacAddress, &Eqos->SnpMode.CurrentAddress, NET_ETHER_ADDR_LEN);
  DevicePath->MacAddrDP.IfType = Eqos->SnpMode.IfType;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  EqosNotifyExitBootServices,
                  Eqos,
                  &gEfiEventExitBootServicesGuid,
                  &Eqos->ExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Failed to create ExitBootServices event - %r\n", __func__, Status));
    goto ErrFreeDevicePath;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEfiSimpleNetworkProtocolGuid,
                  &Eqos->Snp,
                  &gEfiDevicePathProtocolGuid,
                  DevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces failed - %r\n", __func__, Status));
    goto ErrCloseEvent;
  }

  Eqos->ControllerHandle = Controller;
  return EFI_SUCCESS;

ErrCloseEvent:
  gBS->CloseEvent (Eqos->ExitBootServicesEvent);
  Eqos->ExitBootServicesEvent = NULL;

ErrFreeDevicePath:
  FreePool (DevicePath);

ErrDeviceDeinit:
  EqosDeinit (Eqos);

ErrFreeDevice:
  FreePool (Eqos);

ErrCloseProtocol:
  gBS->CloseProtocol (
         Controller,
         &gEdkiiNonDiscoverableDeviceProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
DriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  EFI_STATUS                   Status;
  EFI_SIMPLE_NETWORK_PROTOCOL  *Snp;
  EQOS_DEVICE                  *Eqos;
  EFI_DEVICE_PATH_PROTOCOL     *DevicePath;

  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiSimpleNetworkProtocolGuid,
                  (VOID **)&Snp
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath
                  );
  ASSERT_EFI_ERROR (Status);

  Eqos = EQOS_PRIVATE_DATA_FROM_SNP_THIS (Snp);

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Controller,
                  &gEfiSimpleNetworkProtocolGuid,
                  &Eqos->Snp,
                  &gEfiDevicePathProtocolGuid,
                  DevicePath,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  if (Eqos->ExitBootServicesEvent != NULL) {
    gBS->CloseEvent (Eqos->ExitBootServicesEvent);
  }

  FreePool (DevicePath);

  EqosDeinit (Eqos);
  FreePool (Eqos);

  Status = gBS->CloseProtocol (
                  Controller,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  This->DriverBindingHandle,
                  Controller
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
RegisterEqosDevices (
  VOID
  )
{
  EFI_STATUS                  Status;
  DW_EQOS_CONTROLLER_CONFIGS  *Configs;
  UINT16                      I;
  UINTN                       J;
  UINT64                      Base;
  UINT8                       GmacUseMask;

  if (mEqosNdHandles != NULL) {
    return EFI_ALREADY_STARTED;
  }

  Configs = (DW_EQOS_CONTROLLER_CONFIGS *)PcdGetPtr (PcdDwEqosControllerConfigs);
  if ((Configs == NULL) || (Configs->Num == 0)) {
    DEBUG ((DEBUG_ERROR, "%a: No EQoS controller configs found\n", __func__));
    return EFI_NOT_FOUND;
  }

  mEqosNdHandles = AllocateZeroPool (sizeof (EFI_HANDLE) * Configs->Num);
  mEqosResArray  = AllocateZeroPool (sizeof (EQOS_ACPI_RES) * Configs->Num);
  mEqosDevDescs  = AllocateZeroPool (sizeof (NON_DISCOVERABLE_DEVICE) * Configs->Num);
  if ((mEqosNdHandles == NULL) || (mEqosResArray == NULL) || (mEqosDevDescs == NULL)) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorFreeAll;
  }

  J = 0;
  for (I = 0; I < Configs->Num; I++) {
    //
    // PcdGmacUseMask: bit I = 1 means controller I is active for this board.
    //
    GmacUseMask = PcdGet8 (PcdGmacUseMask);
    if (!(GmacUseMask & (1 << I))) {
      DEBUG ((DEBUG_INFO, "%a: skipping controller %u (not active for this board)\n", __func__, I));
      continue;
    }

    Base = Configs->Data[I].Base;

    mEqosResArray[J].MemDesc.Desc                  = ACPI_ADDRESS_SPACE_DESCRIPTOR;
    mEqosResArray[J].MemDesc.Len                   = sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3;
    mEqosResArray[J].MemDesc.ResType               = ACPI_ADDRESS_SPACE_TYPE_MEM;
    mEqosResArray[J].MemDesc.GenFlag               = 0;
    mEqosResArray[J].MemDesc.SpecificFlag          = 0;
    mEqosResArray[J].MemDesc.AddrSpaceGranularity  = 32;
    mEqosResArray[J].MemDesc.AddrRangeMin          = Base;
    mEqosResArray[J].MemDesc.AddrRangeMax          = Base + EQOS_MAC_DEVICE_SIZE - 1;
    mEqosResArray[J].MemDesc.AddrTranslationOffset = 0;
    mEqosResArray[J].MemDesc.AddrLen               = EQOS_MAC_DEVICE_SIZE;
    mEqosResArray[J].EndDesc.Desc                  = ACPI_END_TAG_DESCRIPTOR;

    mEqosDevDescs[J].Type       = &gSpacemitDwGmacNonDiscoverableDeviceGuid;
    mEqosDevDescs[J].DmaType    = NonDiscoverableDeviceDmaTypeNonCoherent;
    mEqosDevDescs[J].Initialize = NULL;
    mEqosDevDescs[J].Resources  = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)&mEqosResArray[J];

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &mEqosNdHandles[J],
                    &gEdkiiNonDiscoverableDeviceProtocolGuid,
                    &mEqosDevDescs[J],
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG (
        (DEBUG_ERROR, "%a: Install ND protocol failed for controller %u: %r\n",
         __func__, I, Status)
        );
      goto ErrorUninstall;
    }

    J++;
  }

  if (J == 0) {
    DEBUG ((DEBUG_WARN, "%a: All EQoS controllers are disabled\n", __func__));
    Status = EFI_NOT_FOUND;
    goto ErrorFreeAll;
  }

  mEqosNdCount = J;
  return EFI_SUCCESS;

ErrorUninstall:
  for (UINTN K = 0; K < J; K++) {
    gBS->UninstallMultipleProtocolInterfaces (
           mEqosNdHandles[K],
           &gEdkiiNonDiscoverableDeviceProtocolGuid,
           &mEqosDevDescs[K],
           NULL
           );
  }

ErrorFreeAll:
  if (mEqosNdHandles != NULL) {
    FreePool (mEqosNdHandles);
    mEqosNdHandles = NULL;
  }

  if (mEqosResArray  != NULL) {
    FreePool (mEqosResArray);
    mEqosResArray = NULL;
  }

  if (mEqosDevDescs  != NULL) {
    FreePool (mEqosDevDescs);
    mEqosDevDescs = NULL;
  }

  mEqosNdCount = 0;
  return Status;
}

STATIC
VOID
EFIAPI
UnregisterEqosDevices (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       I;

  if (mEqosNdHandles == NULL) {
    return;
  }

  for (I = 0; I < mEqosNdCount; I++) {
    if (mEqosNdHandles[I] == NULL) {
      continue;
    }

    Status = gBS->UninstallMultipleProtocolInterfaces (
                    mEqosNdHandles[I],
                    &gEdkiiNonDiscoverableDeviceProtocolGuid,
                    &mEqosDevDescs[I],
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      DEBUG (
        (DEBUG_WARN,
         "%a: Uninstall ND protocol failed for index %u: %r\n",
         __func__, I, Status)
        );
    }
  }

  FreePool (mEqosNdHandles);
  mEqosNdHandles = NULL;
  FreePool (mEqosResArray);
  mEqosResArray = NULL;
  FreePool (mEqosDevDescs);
  mEqosDevDescs = NULL;
  mEqosNdCount  = 0;
}

/**
  The EQoS SNP DXE driver entry point.

  @param[in] ImageHandle   The driver image handle.
  @param[in] SystemTable   The system table.

  @retval EFI_SUCCESS      The non-discoverable device was registered and the
                           driver binding/component name protocols were
                           installed successfully.
  @retval Others           Error returned by RegisterEqosDevice() or
                           EfiLibInstallDriverBindingComponentName2().

**/
EFI_STATUS
EFIAPI
EqosEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = RegisterEqosDevices ();
  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "%a: RegisterEqosDevices() failed: %r\n",
       __func__,
       Status
      )
      );
    return Status;
  }

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gEqosDriverBinding,
             ImageHandle,
             &gEqosComponentName,
             &gEqosComponentName2
             );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "%a: Install driver binding failed: %r\n",
       __func__,
       Status
      )
      );
    UnregisterEqosDevices ();
  }

  return Status;
}

/**
  Unload the EQoS SNP DXE driver.

  @param[in] ImageHandle   The driver image handle.

  @retval EFI_SUCCESS      The driver binding/component name protocols were
                           uninstalled successfully.
  @retval Others           Error returned by
                           EfiLibUninstallDriverBindingComponentName2().

**/
EFI_STATUS
EFIAPI
EqosUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  Status = EfiLibUninstallDriverBindingComponentName2 (
             &gEqosDriverBinding,
             &gEqosComponentName,
             &gEqosComponentName2
             );
  ASSERT_EFI_ERROR (Status);

  UnregisterEqosDevices ();

  return Status;
}
