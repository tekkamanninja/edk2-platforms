/** @file
 *  Spacemit K3 silicon pin controller driver implementation.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtHelperLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryManagementLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Guid/Fdt.h>
#include <Guid/FdtHob.h>

#include <PinCtrlDxe.h>

STATIC CONST UINT32  EdgeDetectValueArray[] = {
  PIN_EDGE_FALL, PIN_EDGE_RISE,
  PIN_EDGE_BOTH, PIN_EDGE_NONE
};

STATIC CONST UINT32  PullValueArray[] = {
  PIN_PULL_DOWN, PIN_PULL_UP,
  PIN_PULL_DIS
};

typedef enum {
  PINCTRL_EDGE_FALL = 0,
  PINCTRL_EDGE_RISE,
  PINCTRL_EDGE_BOTH,
  PINCTRL_EDGE_NONE,
} PINCTRL_EDGE_MODE;

typedef enum {
  PINCTRL_PULL_DOWN = 0,
  PINCTRL_PULL_UP,
  PINCTRL_PULL_NONE,
} PINCTRL_PULL_MODE;

#define PINCTRL_FUNCTION_DEFAULT           "default"
#define PINCTRL_VOLTAGE_DOMAIN_COMPATIBLE  "spacemit,pinctrl-voltage-domain"
#define PINCTRL_VOLTAGE_MAP_PROP           "spacemit,pin-voltage-map"
#define PINCTRL_VOLTAGE_MAP_STRIDE         3U

STATIC BOOLEAN  mVoltageDomainConfigured = FALSE;

STATIC
CONST VOID *
GetFdtBaseFromConfigTable (
  VOID
  )
{
  UINTN  Index;
  VOID   *FdtBase;

  if ((gST == NULL) || (gST->ConfigurationTable == NULL)) {
    return NULL;
  }

  for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
    if (!CompareGuid (
                      &gST->ConfigurationTable[Index].VendorGuid,
                      &gFdtTableGuid
                      ))
    {
      continue;
    }

    FdtBase = gST->ConfigurationTable[Index].VendorTable;
    if ((FdtBase != NULL) && (FdtCheckHeader (FdtBase) == 0)) {
      return FdtBase;
    }

    return NULL;
  }

  return NULL;
}

STATIC
CONST VOID *
GetFdtBaseFromGuidHob (
  VOID
  )
{
  VOID    *Hob;
  VOID    *FdtBase;
  UINT64  FdtAddress;

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

STATIC
CONST VOID *
GetRuntimeFdtBase (
  VOID
  )
{
  CONST VOID  *FdtBase;

  FdtBase = GetFdtBaseFromConfigTable ();
  if (FdtBase != NULL) {
    return FdtBase;
  }

  return GetFdtBaseFromGuidHob ();
}

STATIC
BOOLEAN
AsciiFieldEquals (
  IN CONST CHAR8  *Field,
  IN UINTN        FieldLen,
  IN CONST CHAR8  *Value
  )
{
  UINTN  ValueLen;

  if ((Field == NULL) || (Value == NULL) || (Value[0] == '\0')) {
    return FALSE;
  }

  ValueLen = AsciiStrLen (Value);
  if (ValueLen >= FieldLen) {
    return FALSE;
  }

  return (AsciiStrnCmp (Field, Value, FieldLen) == 0);
}

STATIC
CONST CHAR8 *
GetEffectiveName (
  IN CONST CHAR8  *Name,
  IN CONST CHAR8  *DefaultName
  )
{
  if ((Name != NULL) && (Name[0] != '\0')) {
    return Name;
  }

  return DefaultName;
}

STATIC
UINT8
DecodeEdgeDetectMode (
  IN UINT32  RawConfig
  )
{
  UINT32  EdgeBits;

  if ((RawConfig & PIN_EDGE_NONE) != 0) {
    return PINCTRL_EDGE_NONE;
  }

  EdgeBits = RawConfig & (PIN_EDGE_RISE | PIN_EDGE_FALL);

  if (EdgeBits == (PIN_EDGE_RISE | PIN_EDGE_FALL)) {
    return PINCTRL_EDGE_BOTH;
  }

  if (EdgeBits == PIN_EDGE_RISE) {
    return PINCTRL_EDGE_RISE;
  }

  if (EdgeBits == PIN_EDGE_FALL) {
    return PINCTRL_EDGE_FALL;
  }

  return PINCTRL_EDGE_NONE;
}

STATIC
UINT8
DecodePullMode (
  IN UINT32  RawConfig
  )
{
  UINT32  PullBits;

  PullBits = RawConfig & (7U << PIN_PULL_SHIFT);

  if (PullBits == PIN_PULL_UP) {
    return PINCTRL_PULL_UP;
  }

  if (PullBits == PIN_PULL_DOWN) {
    return PINCTRL_PULL_DOWN;
  }

  return PINCTRL_PULL_NONE;
}

STATIC
INT32
K3PinToPowerDomainOffset (
  IN UINT32  PinId
  )
{
  if (PinId <= 20U) {
    return K3_AIB_GPIO1_IO_REG;
  }

  if ((PinId >= 21U) && (PinId <= 41U)) {
    return K3_AIB_GPIO2_IO_REG;
  }

  if ((PinId >= 76U) && (PinId <= 98U)) {
    return K3_AIB_GPIO4_IO_REG;
  }

  if ((PinId >= 99U) && (PinId <= 127U)) {
    return K3_AIB_GPIO5_IO_REG;
  }

  if ((PinId >= 134U) && (PinId <= 139U)) {
    return K3_AIB_SD_IO_REG;
  }

  if ((PinId >= 140U) && (PinId <= 146U)) {
    return K3_AIB_QSPI_IO_REG;
  }

  return -1;
}

STATIC
CONST CHAR8 *
PowerDomainNameByOffset (
  IN UINT32  PowerDomainOffset
  )
{
  switch (PowerDomainOffset) {
    case K3_AIB_GPIO1_IO_REG:
      return "GPIO1";
    case K3_AIB_GPIO2_IO_REG:
      return "GPIO2";
    case K3_AIB_GPIO4_IO_REG:
      return "GPIO4";
    case K3_AIB_GPIO5_IO_REG:
      return "GPIO5";
    case K3_AIB_SD_IO_REG:
      return "SD";
    case K3_AIB_QSPI_IO_REG:
      return "QSPI";
    default:
      return "UNKNOWN";
  }
}

STATIC
VOID
UnlockPowerDomainAccess (
  VOID
  )
{
  MmioWrite32 (K3_APBC_ASFAR, K3_APBC_AKEY_ASFAR);
  MmioWrite32 (K3_APBC_ASFAR + sizeof (UINT32), K3_APBC_AKEY_ASSAR);
}

STATIC
EFI_STATUS
SetPowerDomainByOffset (
  IN UINT32  PowerDomainOffset,
  IN UINT32  PowerSource
  )
{
  UINT32  DomainConfig;
  UINTN   DomainReg;
  UINT32  DomainConfigAfter;

  if (PowerSource == 1800U) {
    DomainConfig = K3_IO_PWR_DOMAIN_1V8EN;
  } else if (PowerSource == 3300U) {
    DomainConfig = K3_IO_PWR_DOMAIN_3V3EN;
  } else {
    return EFI_INVALID_PARAMETER;
  }

  DomainReg = (UINTN)(K3_IOPWRDOM_BASE + PowerDomainOffset);
  // APBC unlock allows one subsequent access to IO power-domain registers.
  UnlockPowerDomainAccess ();
  MmioWrite32 (DomainReg, DomainConfig);

  UnlockPowerDomainAccess ();
  DomainConfigAfter = MmioRead32 (DomainReg);
  if (DomainConfigAfter != DomainConfig) {
    DEBUG (
           (DEBUG_WARN,
            "%a: domain=%a write did not latch (new=0x%08x expect=0x%08x)\n",
            __func__, PowerDomainNameByOffset (PowerDomainOffset),
            DomainConfigAfter, DomainConfig)
           );
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetPowerDomainForPin (
  IN UINT32  PinId,
  IN UINT32  PowerSource
  )
{
  INT32  PowerDomainOffset;

  PowerDomainOffset = K3PinToPowerDomainOffset (PinId);
  if (PowerDomainOffset < 0) {
    return EFI_UNSUPPORTED;
  }

  return SetPowerDomainByOffset ((UINT32)PowerDomainOffset, PowerSource);
}

STATIC
EFI_STATUS
ConfigureVoltageDomainsFromFdt (
  IN CONST VOID  *Fdt
  )
{
  INT32         NodeOffset;
  CONST UINT32  *DomainMap;
  UINT32        EntryCount;
  UINT32        Entry;
  UINT32        StartPin;
  UINT32        EndPin;
  UINT32        PowerSource;
  INT32         StartOffset;
  INT32         EndOffset;
  INT32         Len;
  EFI_STATUS    Status;

  NodeOffset =
    FdtNodeOffsetByCompatible (Fdt, -1, PINCTRL_VOLTAGE_DOMAIN_COMPATIBLE);
  if (NodeOffset < 0) {
    return EFI_NOT_FOUND;
  }

  DomainMap = (CONST UINT32 *)FdtGetProp (
                                          Fdt,
                                          NodeOffset,
                                          PINCTRL_VOLTAGE_MAP_PROP,
                                          &Len
                                          );
  if ((DomainMap == NULL) || (Len <= 0)) {
    return EFI_NOT_FOUND;
  }

  if ((Len % (INT32)(PINCTRL_VOLTAGE_MAP_STRIDE * sizeof (UINT32))) != 0) {
    DEBUG (
           (DEBUG_ERROR, "%a: invalid %a property length (%d)\n", __func__,
            PINCTRL_VOLTAGE_MAP_PROP, Len)
           );
    return EFI_COMPROMISED_DATA;
  }

  EntryCount = (UINT32)Len / (PINCTRL_VOLTAGE_MAP_STRIDE * sizeof (UINT32));
  for (Entry = 0; Entry < EntryCount; Entry++) {
    StartPin    = Fdt32ToCpu (DomainMap[Entry * PINCTRL_VOLTAGE_MAP_STRIDE]);
    EndPin      = Fdt32ToCpu (DomainMap[Entry * PINCTRL_VOLTAGE_MAP_STRIDE + 1]);
    PowerSource = Fdt32ToCpu (DomainMap[Entry * PINCTRL_VOLTAGE_MAP_STRIDE + 2]);

    if ((StartPin >= MAX_PIN_NUMBER) || (EndPin >= MAX_PIN_NUMBER) ||
        (StartPin > EndPin))
    {
      DEBUG (
             (DEBUG_ERROR, "%a: invalid voltage-domain pin range %u..%u\n",
              __func__, StartPin, EndPin)
             );
      return EFI_COMPROMISED_DATA;
    }

    StartOffset = K3PinToPowerDomainOffset (StartPin);
    EndOffset   = K3PinToPowerDomainOffset (EndPin);
    if ((StartOffset < 0) || (EndOffset < 0) || (StartOffset != EndOffset)) {
      DEBUG (
             (DEBUG_ERROR,
              "%a: pin range %u..%u must stay in one configurable power domain\n",
              __func__, StartPin, EndPin)
             );
      return EFI_COMPROMISED_DATA;
    }

    Status = SetPowerDomainByOffset ((UINT32)StartOffset, PowerSource);
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_ERROR, "%a: failed to set power domain 0x%x -> %u mV (%r)\n",
              __func__, (UINT32)StartOffset, PowerSource, Status)
             );
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EnsureVoltageDomainConfigured (
  IN CONST VOID  *Fdt
  )
{
  EFI_STATUS  Status;

  if (mVoltageDomainConfigured) {
    return EFI_SUCCESS;
  }

  Status = ConfigureVoltageDomainsFromFdt (Fdt);
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    return Status;
  }

  mVoltageDomainConfigured = TRUE;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
WritePinConfig (
  IN UINT32  PinId,
  IN UINT8   FunctionSelect,
  IN UINT8   PadDrive,
  IN UINT8   EdgeDetect,
  IN UINT8   Pull
  )
{
  UINT32  MuxConfig;
  UINT32  EncodedFunctionSelect;
  UINT32  EncodedPadDrive;
  UINT32  EncodedEdgeDetect;
  UINT32  EncodedPull;

  if (PinId >= MAX_PIN_NUMBER) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid PinId %d\n", __func__, PinId));
    return EFI_INVALID_PARAMETER;
  }

  EncodedFunctionSelect = MIN (FunctionSelect, PIN_MUX_MODE7) << PIN_MUX_SHIFT;
  EncodedPadDrive       = MIN (PadDrive, PIN_DS15) << PIN_DRIVE_SHIFT;
  EncodedEdgeDetect     = EdgeDetectValueArray[MIN (
                                                    EdgeDetect,
                                                    ARRAY_SIZE (EdgeDetectValueArray) - 1
                                                    )];
  EncodedPull = PullValueArray[MIN (Pull, ARRAY_SIZE (PullValueArray) - 1)];

  MuxConfig =
    EncodedFunctionSelect | EncodedEdgeDetect | EncodedPadDrive | EncodedPull;
  MmioWrite32 (PIN_CONFIG_REG_OFFSET (PinId), MuxConfig);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ApplyRawPinConfig (
  IN UINT32  PinOffset,
  IN UINT32  FunctionSelect,
  IN UINT32  RawConfig
  )
{
  UINT8   PadDrive;
  UINT8   EdgeDetect;
  UINT8   Pull;
  UINT32  PinId;

  if ((PinOffset & (sizeof (UINT32) - 1)) != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid pin offset 0x%x\n", __func__, PinOffset));
    return EFI_INVALID_PARAMETER;
  }

  PinId = PinOffset / sizeof (UINT32);
  if (PinId >= MAX_PIN_NUMBER) {
    DEBUG (
           (DEBUG_ERROR, "%a: Pin offset 0x%x exceeds max pin number\n",
            __func__, PinOffset)
           );
    return EFI_INVALID_PARAMETER;
  }

  PadDrive   = (UINT8)((RawConfig >> PIN_DRIVE_SHIFT) & PIN_DS15);
  EdgeDetect = DecodeEdgeDetectMode (RawConfig);
  Pull       = DecodePullMode (RawConfig);

  return WritePinConfig (
                         PinId,
                         (UINT8)(FunctionSelect & PIN_MUX_MODE7),
                         PadDrive,
                         EdgeDetect,
                         Pull
                         );
}

STATIC
EFI_STATUS
ConfigurePinGroup (
  IN CONST VOID  *Fdt,
  IN INT32       GroupNode,
  IN BOOLEAN     SkipPowerSource
  )
{
  CONST CHAR8   *GroupName;
  CONST UINT32  *PinData;
  CONST UINT32  *PinctrlCells;
  CONST UINT32  *PowerSourceProp;
  INT32         PinDataLen;
  INT32         Len;
  INT32         ParentNode;
  UINT32        NumCells;
  UINT32        Stride;
  UINT32        Index;
  UINT32        Count;
  UINT32        PinOffset;
  UINT32        FunctionSelect;
  UINT32        RawConfig;
  UINT32        PowerSource;
  UINT32        FirstPinId;
  BOOLEAN       HasPowerSource;
  EFI_STATUS    Status;

  GroupName = FdtGetName ((VOID *)Fdt, GroupNode, NULL);
  if (GroupName == NULL) {
    GroupName = "<unknown>";
  }

  PowerSource    = 0;
  HasPowerSource = FALSE;
  if (!SkipPowerSource) {
    PowerSourceProp =
      (CONST UINT32 *)FdtGetProp (Fdt, GroupNode, "power-source", &Len);
    if (PowerSourceProp != NULL) {
      if (Len != sizeof (UINT32)) {
        DEBUG (
               (DEBUG_ERROR, "%a: invalid power-source property length (%d)\n",
                __func__, Len)
               );
        return EFI_COMPROMISED_DATA;
      }

      PowerSource    = Fdt32ToCpu (*PowerSourceProp);
      HasPowerSource = TRUE;
    }
  }

  PinData = (CONST UINT32 *)FdtGetProp (
                                        Fdt,
                                        GroupNode,
                                        "pinctrl-single,pins",
                                        &PinDataLen
                                        );
  if ((PinData == NULL) || (PinDataLen <= 0)) {
    return EFI_NOT_FOUND;
  }

  if ((PinDataLen % sizeof (UINT32)) != 0) {
    DEBUG (
           (DEBUG_ERROR, "%a: invalid pin property length (%d)\n", __func__,
            PinDataLen)
           );
    return EFI_COMPROMISED_DATA;
  }

  NumCells   = 1;
  ParentNode = FdtParentOffset (Fdt, GroupNode);
  if (ParentNode >= 0) {
    PinctrlCells =
      (CONST UINT32 *)FdtGetProp (Fdt, ParentNode, "#pinctrl-cells", &Len);
    if ((PinctrlCells != NULL) && (Len == sizeof (UINT32))) {
      NumCells = Fdt32ToCpu (*PinctrlCells);
    }
  }

  Stride = NumCells + 1;
  Count  = (UINT32)(PinDataLen / sizeof (UINT32));
  if ((Stride < 2) || ((Count % Stride) != 0)) {
    if ((Count % 3) == 0) {
      Stride = 3;
    } else if ((Count % 2) == 0) {
      Stride = 2;
    } else {
      DEBUG (
             (DEBUG_ERROR, "%a: unsupported pinctrl-single,pins format\n",
              __func__)
             );
      return EFI_COMPROMISED_DATA;
    }
  }

  if (HasPowerSource) {
    PinOffset = Fdt32ToCpu (PinData[0]);
    if ((PinOffset & (sizeof (UINT32) - 1)) == 0) {
      FirstPinId = PinOffset / sizeof (UINT32);
      Status     = SetPowerDomainForPin (FirstPinId, PowerSource);
      if (EFI_ERROR (Status)) {
        DEBUG (
               (DEBUG_WARN,
                "%a: group %a failed to set pin %u power-source=%u mV (%r)\n",
                __func__, GroupName, FirstPinId, PowerSource, Status)
               );
      }
    } else {
      DEBUG (
             (DEBUG_WARN,
              "%a: group %a skip power-source due to unaligned pin offset 0x%x\n",
              __func__, GroupName, PinOffset)
             );
    }
  }

  for (Index = 0; Index < Count; Index += Stride) {
    PinOffset = Fdt32ToCpu (PinData[Index]);

    if (Stride == 3) {
      FunctionSelect = Fdt32ToCpu (PinData[Index + 1]);
      RawConfig      = Fdt32ToCpu (PinData[Index + 2]);
    } else {
      RawConfig      = Fdt32ToCpu (PinData[Index + 1]);
      FunctionSelect = RawConfig & PIN_MUX_MODE7;
    }

    Status = ApplyRawPinConfig (PinOffset, FunctionSelect, RawConfig);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
NodeNameMatches (
  IN CONST CHAR8  *NodeName,
  IN CONST CHAR8  *TargetName
  )
{
  CONST CHAR8  *AtSign;
  UINTN        NameLen;

  if ((NodeName == NULL) || (TargetName == NULL)) {
    return FALSE;
  }

  if (AsciiStrCmp (NodeName, TargetName) == 0) {
    return TRUE;
  }

  AtSign = AsciiStrStr (NodeName, "@");
  if (AtSign == NULL) {
    return FALSE;
  }

  NameLen = (UINTN)(AtSign - NodeName);
  return ((AsciiStrLen (TargetName) == NameLen) &&
          (AsciiStrnCmp (NodeName, TargetName, NameLen) == 0));
}

STATIC
EFI_STATUS
FindNodeByName (
  IN CONST VOID   *Fdt,
  IN CONST CHAR8  *NodeName,
  OUT INT32       *NodeOffset
  )
{
  INT32        Node;
  CONST CHAR8  *CurrentName;

  if ((NodeName == NULL) || (NodeOffset == NULL) || (NodeName[0] == '\0')) {
    return EFI_INVALID_PARAMETER;
  }

  if (NodeName[0] == '/') {
    Node = FdtPathOffset (Fdt, NodeName);
    if (Node < 0) {
      return EFI_NOT_FOUND;
    }

    *NodeOffset = Node;
    return EFI_SUCCESS;
  }

  for (Node = FdtNextNode (Fdt, 0, NULL); Node >= 0;
       Node = FdtNextNode (Fdt, Node, NULL))
  {
    CurrentName = FdtGetName ((VOID *)Fdt, Node, NULL);
    if (NodeNameMatches (CurrentName, NodeName)) {
      *NodeOffset = Node;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
GetFirstPinIdFromGroupNode (
  IN CONST VOID  *Fdt,
  IN INT32       GroupNode,
  OUT UINT32     *PinId
  )
{
  CONST UINT32  *PinData;
  INT32         PinDataLen;
  UINT32        PinOffset;
  UINT32        LocalPinId;

  if (PinId == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PinData = (CONST UINT32 *)FdtGetProp (
                                        Fdt,
                                        GroupNode,
                                        "pinctrl-single,pins",
                                        &PinDataLen
                                        );
  if ((PinData == NULL) || (PinDataLen < (INT32)sizeof (UINT32))) {
    return EFI_NOT_FOUND;
  }

  PinOffset = Fdt32ToCpu (PinData[0]);
  if ((PinOffset & (sizeof (UINT32) - 1)) != 0) {
    return EFI_COMPROMISED_DATA;
  }

  LocalPinId = PinOffset / sizeof (UINT32);
  if (LocalPinId >= MAX_PIN_NUMBER) {
    return EFI_COMPROMISED_DATA;
  }

  *PinId = LocalPinId;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ApplyStateGroupRef (
  IN CONST VOID                     *Fdt,
  IN CONST PINCTRL_STATE_GROUP_REF  *GroupRef
  )
{
  EFI_STATUS  Status;
  INT32       GroupNode;
  UINT32      PinId;

  Status = FindNodeByName (Fdt, GroupRef->GroupName, &GroupNode);
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_WARN, "%a: pinctrl group \"%a\" not found\n", __func__,
            GroupRef->GroupName)
           );
    return Status;
  }

  if (GroupRef->HasVoltageOverride) {
    Status = GetFirstPinIdFromGroupNode (Fdt, GroupNode, &PinId);
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_WARN,
              "%a: failed to parse first pin for group \"%a\" (%r)\n",
              __func__, GroupRef->GroupName, Status)
             );
      return Status;
    }

    Status = SetPowerDomainForPin (PinId, GroupRef->VoltageMv);
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_WARN,
              "%a: failed to apply voltage override %umV for group \"%a\" (%r)\n",
              __func__, GroupRef->VoltageMv, GroupRef->GroupName, Status)
             );
      return Status;
    }
  }

  return ConfigurePinGroup (Fdt, GroupNode, GroupRef->HasVoltageOverride);
}

STATIC
EFI_STATUS
BuildStateIndex (
  IN OUT PINCTRL_INSTANCE  *Instance
  )
{
  UINTN                        DeviceIndex;
  UINTN                        FunctionIndex;
  UINTN                        StateIndex;
  CONST PINCTRL_BOARD_MAP      *BoardMap;
  CONST PINCTRL_DEVICE_DESC    *Device;
  CONST PINCTRL_FUNCTION_DESC  *Function;
  CONST PINCTRL_STATE_DESC     *State;
  PINCTRL_STATE_INDEX_ENTRY    *IndexEntry;

  if (Instance == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  BoardMap                  = Instance->BoardMap;
  Instance->StateIndexCount = 0;
  if ((BoardMap == NULL) || (BoardMap->Devices == NULL) || (BoardMap->DeviceCount == 0)) {
    return EFI_NOT_FOUND;
  }

  for (DeviceIndex = 0; DeviceIndex < BoardMap->DeviceCount; DeviceIndex++) {
    Device = &BoardMap->Devices[DeviceIndex];
    if ((Device->ControllerType == NULL) || (Device->ControllerType[0] == '\0') ||
        (Device->Functions == NULL) || (Device->FunctionCount == 0))
    {
      return EFI_COMPROMISED_DATA;
    }

    for (FunctionIndex = 0; FunctionIndex < Device->FunctionCount; FunctionIndex++) {
      Function = &Device->Functions[FunctionIndex];
      if ((Function->FunctionName == NULL) || (Function->FunctionName[0] == '\0') ||
          (Function->States == NULL) || (Function->StateCount == 0))
      {
        return EFI_COMPROMISED_DATA;
      }

      for (StateIndex = 0; StateIndex < Function->StateCount; StateIndex++) {
        State = &Function->States[StateIndex];
        if ((State->StateName == NULL) || (State->StateName[0] == '\0') ||
            (State->Groups == NULL) || (State->GroupCount == 0))
        {
          return EFI_COMPROMISED_DATA;
        }

        if (State->GroupCount > PINCTRL_MAX_GROUPS_PER_STATE) {
          DEBUG (
                 (DEBUG_ERROR,
                  "%a: %a[%u] function=\"%a\" state=\"%a\" group-count=%u exceeds limit=%u\n",
                  __func__,
                  Device->ControllerType,
                  Device->ControllerId,
                  Function->FunctionName,
                  State->StateName,
                  State->GroupCount,
                  PINCTRL_MAX_GROUPS_PER_STATE)
                 );
          return EFI_COMPROMISED_DATA;
        }

        if (Instance->StateIndexCount >= PINCTRL_MAX_STATE_INDEX_ENTRIES) {
          return EFI_OUT_OF_RESOURCES;
        }

        IndexEntry           = &Instance->StateIndex[Instance->StateIndexCount++];
        IndexEntry->Device   = Device;
        IndexEntry->Function = Function;
        IndexEntry->State    = State;
      }
    }
  }

  return (Instance->StateIndexCount == 0) ? EFI_NOT_FOUND : EFI_SUCCESS;
}

STATIC
CONST PINCTRL_STATE_INDEX_ENTRY *
FindStateIndexEntry (
  IN PINCTRL_INSTANCE  *Instance,
  IN CONST CHAR8       *ControllerType,
  IN UINT32            ControllerId,
  IN CONST CHAR8       *FunctionName,
  IN CONST CHAR8       *StateName
  )
{
  UINTN                            Index;
  CONST PINCTRL_STATE_INDEX_ENTRY  *Entry;

  if ((Instance == NULL) || (ControllerType == NULL) || (FunctionName == NULL) ||
      (StateName == NULL))
  {
    return NULL;
  }

  for (Index = 0; Index < Instance->StateIndexCount; Index++) {
    Entry = &Instance->StateIndex[Index];
    if ((Entry->Device == NULL) || (Entry->Function == NULL) || (Entry->State == NULL)) {
      continue;
    }

    if (AsciiStrCmp (Entry->Device->ControllerType, ControllerType) != 0) {
      continue;
    }

    if (Entry->Device->ControllerId != ControllerId) {
      continue;
    }

    if (AsciiStrCmp (Entry->Function->FunctionName, FunctionName) != 0) {
      continue;
    }

    if (AsciiStrCmp (Entry->State->StateName, StateName) != 0) {
      continue;
    }

    return Entry;
  }

  return NULL;
}

STATIC
EFI_STATUS
ResolveControllerByHandle (
  IN PINCTRL_INSTANCE  *Instance,
  IN EFI_HANDLE        ControllerHandle,
  OUT CONST CHAR8      **ControllerType,
  OUT UINT32           *ControllerId
  )
{
  UINTN  Index;

  if ((Instance == NULL) || (ControllerHandle == NULL) || (ControllerType == NULL) ||
      (ControllerId == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < PINCTRL_MAX_REGISTERED_CONTROLLERS; Index++) {
    if (!Instance->ControllerMap[Index].Valid) {
      continue;
    }

    if (Instance->ControllerMap[Index].ControllerHandle != ControllerHandle) {
      continue;
    }

    *ControllerType = Instance->ControllerMap[Index].ControllerType;
    *ControllerId   = Instance->ControllerMap[Index].ControllerId;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
RegisterControllerInternal (
  IN OUT PINCTRL_INSTANCE  *Instance,
  IN EFI_HANDLE            ControllerHandle,
  IN CONST CHAR8           *ControllerType,
  IN UINT32                ControllerId
  )
{
  UINTN  Index;
  UINTN  FreeSlot;

  if ((Instance == NULL) || (ControllerHandle == NULL) || (ControllerType == NULL) ||
      (ControllerType[0] == '\0'))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (AsciiStrLen (ControllerType) >= sizeof (Instance->ControllerMap[0].ControllerType)) {
    return EFI_INVALID_PARAMETER;
  }

  FreeSlot = PINCTRL_MAX_REGISTERED_CONTROLLERS;
  for (Index = 0; Index < PINCTRL_MAX_REGISTERED_CONTROLLERS; Index++) {
    if (!Instance->ControllerMap[Index].Valid) {
      if (FreeSlot == PINCTRL_MAX_REGISTERED_CONTROLLERS) {
        FreeSlot = Index;
      }

      continue;
    }

    if (Instance->ControllerMap[Index].ControllerHandle != ControllerHandle) {
      continue;
    }

    AsciiStrnCpyS (
                   Instance->ControllerMap[Index].ControllerType,
                   sizeof (Instance->ControllerMap[Index].ControllerType),
                   ControllerType,
                   sizeof (Instance->ControllerMap[Index].ControllerType) - 1
                   );
    Instance->ControllerMap[Index].ControllerId = ControllerId;
    return EFI_SUCCESS;
  }

  if (FreeSlot == PINCTRL_MAX_REGISTERED_CONTROLLERS) {
    return EFI_OUT_OF_RESOURCES;
  }

  Instance->ControllerMap[FreeSlot].Valid            = TRUE;
  Instance->ControllerMap[FreeSlot].ControllerHandle = ControllerHandle;
  Instance->ControllerMap[FreeSlot].ControllerId     = ControllerId;
  AsciiStrnCpyS (
                 Instance->ControllerMap[FreeSlot].ControllerType,
                 sizeof (Instance->ControllerMap[FreeSlot].ControllerType),
                 ControllerType,
                 sizeof (Instance->ControllerMap[FreeSlot].ControllerType) - 1
                 );

  return EFI_SUCCESS;
}

STATIC
VOID
UpdateActiveState (
  IN OUT PINCTRL_INSTANCE  *Instance,
  IN CONST CHAR8           *ControllerType,
  IN UINT32                ControllerId,
  IN CONST CHAR8           *FunctionName,
  IN CONST CHAR8           *StateName
  )
{
  UINTN  Index;
  UINTN  FreeSlot;

  if ((Instance == NULL) || (ControllerType == NULL) || (FunctionName == NULL) ||
      (StateName == NULL))
  {
    return;
  }

  FreeSlot = PINCTRL_MAX_ACTIVE_STATE_ENTRIES;
  for (Index = 0; Index < PINCTRL_MAX_ACTIVE_STATE_ENTRIES; Index++) {
    if (!Instance->ActiveStates[Index].Valid) {
      if (FreeSlot == PINCTRL_MAX_ACTIVE_STATE_ENTRIES) {
        FreeSlot = Index;
      }

      continue;
    }

    if (!AsciiFieldEquals (
                           Instance->ActiveStates[Index].ControllerType,
                           sizeof (Instance->ActiveStates[Index].ControllerType),
                           ControllerType
                           ))
    {
      continue;
    }

    if (Instance->ActiveStates[Index].ControllerId != ControllerId) {
      continue;
    }

    if (!AsciiFieldEquals (
                           Instance->ActiveStates[Index].FunctionName,
                           sizeof (Instance->ActiveStates[Index].FunctionName),
                           FunctionName
                           ))
    {
      continue;
    }

    AsciiStrnCpyS (
                   Instance->ActiveStates[Index].StateName,
                   sizeof (Instance->ActiveStates[Index].StateName),
                   StateName,
                   sizeof (Instance->ActiveStates[Index].StateName) - 1
                   );
    return;
  }

  if (FreeSlot == PINCTRL_MAX_ACTIVE_STATE_ENTRIES) {
    return;
  }

  Instance->ActiveStates[FreeSlot].Valid        = TRUE;
  Instance->ActiveStates[FreeSlot].ControllerId = ControllerId;
  AsciiStrnCpyS (
                 Instance->ActiveStates[FreeSlot].ControllerType,
                 sizeof (Instance->ActiveStates[FreeSlot].ControllerType),
                 ControllerType,
                 sizeof (Instance->ActiveStates[FreeSlot].ControllerType) - 1
                 );
  AsciiStrnCpyS (
                 Instance->ActiveStates[FreeSlot].FunctionName,
                 sizeof (Instance->ActiveStates[FreeSlot].FunctionName),
                 FunctionName,
                 sizeof (Instance->ActiveStates[FreeSlot].FunctionName) - 1
                 );
  AsciiStrnCpyS (
                 Instance->ActiveStates[FreeSlot].StateName,
                 sizeof (Instance->ActiveStates[FreeSlot].StateName),
                 StateName,
                 sizeof (Instance->ActiveStates[FreeSlot].StateName) - 1
                 );
}

STATIC
EFI_STATUS
GetActiveStateFromCache (
  IN PINCTRL_INSTANCE  *Instance,
  IN CONST CHAR8       *ControllerType,
  IN UINT32            ControllerId,
  IN CONST CHAR8       *FunctionName,
  OUT CONST CHAR8      **StateName
  )
{
  UINTN  Index;

  if ((Instance == NULL) || (ControllerType == NULL) || (FunctionName == NULL) ||
      (StateName == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < PINCTRL_MAX_ACTIVE_STATE_ENTRIES; Index++) {
    if (!Instance->ActiveStates[Index].Valid) {
      continue;
    }

    if (!AsciiFieldEquals (
                           Instance->ActiveStates[Index].ControllerType,
                           sizeof (Instance->ActiveStates[Index].ControllerType),
                           ControllerType
                           ))
    {
      continue;
    }

    if (Instance->ActiveStates[Index].ControllerId != ControllerId) {
      continue;
    }

    if (!AsciiFieldEquals (
                           Instance->ActiveStates[Index].FunctionName,
                           sizeof (Instance->ActiveStates[Index].FunctionName),
                           FunctionName
                           ))
    {
      continue;
    }

    *StateName = Instance->ActiveStates[Index].StateName;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
ApplyStateByTypeId (
  IN OUT PINCTRL_INSTANCE  *Instance,
  IN CONST CHAR8           *ControllerType,
  IN UINT32                ControllerId,
  IN CONST CHAR8           *FunctionName,
  IN CONST CHAR8           *StateName
  )
{
  EFI_STATUS                       Status;
  CONST VOID                       *Fdt;
  CONST CHAR8                      *EffectiveFunction;
  CONST CHAR8                      *EffectiveState;
  CONST PINCTRL_STATE_INDEX_ENTRY  *StateEntry;
  UINTN                            Order;
  UINTN                            GroupIndex;
  UINT8                            MaxOrder;
  BOOLEAN                          Matched;
  CONST PINCTRL_STATE_GROUP_REF    *GroupRef;

  if ((Instance == NULL) || (ControllerType == NULL) || (ControllerType[0] == '\0')) {
    return EFI_INVALID_PARAMETER;
  }

  if (Instance->StateIndexCount == 0) {
    return EFI_NOT_FOUND;
  }

  EffectiveFunction = GetEffectiveName (FunctionName, PINCTRL_FUNCTION_DEFAULT);
  EffectiveState    = GetEffectiveName (StateName, PINCTRL_STATE_DEFAULT);

  StateEntry = FindStateIndexEntry (
                                    Instance,
                                    ControllerType,
                                    ControllerId,
                                    EffectiveFunction,
                                    EffectiveState
                                    );
  if ((StateEntry == NULL) && (FunctionName != NULL) && (FunctionName[0] != '\0') &&
      (AsciiStrCmp (FunctionName, PINCTRL_FUNCTION_DEFAULT) != 0))
  {
    StateEntry = FindStateIndexEntry (
                                      Instance,
                                      ControllerType,
                                      ControllerId,
                                      PINCTRL_FUNCTION_DEFAULT,
                                      EffectiveState
                                      );
  }

  if (StateEntry == NULL) {
    DEBUG (
           (DEBUG_WARN,
            "%a: no pinctrl state map for %a[%u] function=\"%a\" state=\"%a\"\n",
            __func__, ControllerType, ControllerId, EffectiveFunction,
            EffectiveState)
           );
    return EFI_NOT_FOUND;
  }

  Fdt = GetRuntimeFdtBase ();
  if (Fdt == NULL) {
    DEBUG ((DEBUG_WARN, "%a: runtime FDT is not available\n", __func__));
    return EFI_NOT_FOUND;
  }

  Status = EnsureVoltageDomainConfigured (Fdt);
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR,
            "%a: failed to configure pinctrl voltage domains (%r)\n", __func__,
            Status)
           );
    return Status;
  }

  MaxOrder = 0;
  for (GroupIndex = 0; GroupIndex < StateEntry->State->GroupCount; GroupIndex++) {
    GroupRef = &StateEntry->State->Groups[GroupIndex];
    if (GroupRef->Order > MaxOrder) {
      MaxOrder = GroupRef->Order;
    }
  }

  Matched = FALSE;
  for (Order = 0; Order <= MaxOrder; Order++) {
    for (GroupIndex = 0; GroupIndex < StateEntry->State->GroupCount; GroupIndex++) {
      GroupRef = &StateEntry->State->Groups[GroupIndex];
      if (GroupRef->Order != Order) {
        continue;
      }

      Matched = TRUE;
      if ((GroupRef->GroupName == NULL) || (GroupRef->GroupName[0] == '\0')) {
        return EFI_COMPROMISED_DATA;
      }

      Status = ApplyStateGroupRef (Fdt, GroupRef);
      if (EFI_ERROR (Status)) {
        DEBUG (
               (DEBUG_WARN,
                "%a: failed applying group \"%a\" for %a[%u] function=\"%a\" state=\"%a\" (%r)\n",
                __func__, GroupRef->GroupName, ControllerType, ControllerId,
                StateEntry->Function->FunctionName, StateEntry->State->StateName,
                Status)
               );
        return Status;
      }
    }
  }

  if (!Matched) {
    return EFI_NOT_FOUND;
  }

  UpdateActiveState (
                     Instance,
                     ControllerType,
                     ControllerId,
                     StateEntry->Function->FunctionName,
                     StateEntry->State->StateName
                     );
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
SetPinConfig (
  IN SILICON_PINCTRL_PROTOCOL  *This,
  IN UINT32                    PinId,
  IN CONST PIN_CONFIG          *PinConfig
  )
{
  PINCTRL_INSTANCE  *Instance;

  if ((This == NULL) || (PinConfig == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = PINCTRL_INSTANCE_FROM_THIS (This);
  if (Instance == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return WritePinConfig (
                         PinId,
                         PinConfig->FunctionSelect,
                         PinConfig->PadDrive,
                         PinConfig->EdgeDetect,
                         PinConfig->Pull
                         );
}

STATIC
EFI_STATUS
EFIAPI
SetPinGroupByName (
  IN SILICON_PINCTRL_PROTOCOL  *This,
  IN CONST CHAR8               *GroupName
  )
{
  CONST VOID  *Fdt;
  INT32       GroupNode;
  EFI_STATUS  Status;

  if ((This == NULL) || (GroupName == NULL) || (GroupName[0] == '\0')) {
    return EFI_INVALID_PARAMETER;
  }

  Fdt = GetRuntimeFdtBase ();
  if (Fdt == NULL) {
    DEBUG ((DEBUG_WARN, "%a: runtime FDT is not available\n", __func__));
    return EFI_NOT_FOUND;
  }

  Status = EnsureVoltageDomainConfigured (Fdt);
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR,
            "%a: failed to configure pinctrl voltage domains (%r)\n", __func__,
            Status)
           );
    return Status;
  }

  Status = FindNodeByName (Fdt, GroupName, &GroupNode);
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_VERBOSE, "%a: pinctrl group \"%a\" not found\n", __func__,
            GroupName)
           );
    return Status;
  }

  return ConfigurePinGroup (Fdt, GroupNode, FALSE);
}

STATIC
EFI_STATUS
EFIAPI
ApplyState (
  IN SILICON_PINCTRL_PROTOCOL          *This,
  IN CONST SILICON_PINCTRL_DEVICE_KEY  *DeviceKey,
  IN CONST CHAR8                       *StateName OPTIONAL
  )
{
  PINCTRL_INSTANCE  *Instance;
  CONST CHAR8       *ControllerType;
  UINT32            ControllerId;
  EFI_STATUS        Status;

  if ((This == NULL) || (DeviceKey == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((DeviceKey->Size != 0) &&
      (DeviceKey->Size < sizeof (SILICON_PINCTRL_DEVICE_KEY)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Instance = PINCTRL_INSTANCE_FROM_THIS (This);

  if ((DeviceKey->ControllerType != NULL) && (DeviceKey->ControllerType[0] != '\0')) {
    ControllerType = DeviceKey->ControllerType;
    ControllerId   = DeviceKey->ControllerId;
  } else {
    Status = ResolveControllerByHandle (
                                        Instance,
                                        DeviceKey->ControllerHandle,
                                        &ControllerType,
                                        &ControllerId
                                        );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return ApplyStateByTypeId (
                             Instance,
                             ControllerType,
                             ControllerId,
                             DeviceKey->FunctionName,
                             StateName
                             );
}

STATIC
EFI_STATUS
EFIAPI
ApplyStateById (
  IN SILICON_PINCTRL_PROTOCOL  *This,
  IN CONST CHAR8               *ControllerType,
  IN UINT32                    ControllerId,
  IN CONST CHAR8               *FunctionName OPTIONAL,
  IN CONST CHAR8               *StateName OPTIONAL
  )
{
  PINCTRL_INSTANCE  *Instance;

  if ((This == NULL) || (ControllerType == NULL) || (ControllerType[0] == '\0')) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = PINCTRL_INSTANCE_FROM_THIS (This);
  return ApplyStateByTypeId (
                             Instance,
                             ControllerType,
                             ControllerId,
                             FunctionName,
                             StateName
                             );
}

STATIC
EFI_STATUS
EFIAPI
GetActiveState (
  IN SILICON_PINCTRL_PROTOCOL          *This,
  IN CONST SILICON_PINCTRL_DEVICE_KEY  *DeviceKey,
  OUT CONST CHAR8                      **StateName
  )
{
  PINCTRL_INSTANCE  *Instance;
  CONST CHAR8       *ControllerType;
  CONST CHAR8       *FunctionName;
  UINT32            ControllerId;
  EFI_STATUS        Status;

  if ((This == NULL) || (DeviceKey == NULL) || (StateName == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((DeviceKey->Size != 0) &&
      (DeviceKey->Size < sizeof (SILICON_PINCTRL_DEVICE_KEY)))
  {
    return EFI_INVALID_PARAMETER;
  }

  Instance = PINCTRL_INSTANCE_FROM_THIS (This);

  if ((DeviceKey->ControllerType != NULL) && (DeviceKey->ControllerType[0] != '\0')) {
    ControllerType = DeviceKey->ControllerType;
    ControllerId   = DeviceKey->ControllerId;
  } else {
    Status = ResolveControllerByHandle (
                                        Instance,
                                        DeviceKey->ControllerHandle,
                                        &ControllerType,
                                        &ControllerId
                                        );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  FunctionName = GetEffectiveName (DeviceKey->FunctionName, PINCTRL_FUNCTION_DEFAULT);
  return GetActiveStateFromCache (
                                  Instance,
                                  ControllerType,
                                  ControllerId,
                                  FunctionName,
                                  StateName
                                  );
}

STATIC
EFI_STATUS
EFIAPI
RegisterController (
  IN SILICON_PINCTRL_PROTOCOL  *This,
  IN EFI_HANDLE                ControllerHandle,
  IN CONST CHAR8               *ControllerType,
  IN UINT32                    ControllerId
  )
{
  PINCTRL_INSTANCE  *Instance;

  if ((This == NULL) || (ControllerHandle == NULL) || (ControllerType == NULL) ||
      (ControllerType[0] == '\0'))
  {
    return EFI_INVALID_PARAMETER;
  }

  Instance = PINCTRL_INSTANCE_FROM_THIS (This);
  return RegisterControllerInternal (
                                     Instance,
                                     ControllerHandle,
                                     ControllerType,
                                     ControllerId
                                     );
}

STATIC
VOID
PinCtrlMmioRemap (
  VOID
  )
{
  // Map multi-function controller registers to MMIO space
  MapRegToGcdMmioSpace (K3_MFPR_BASE, SIZE_4KB);
  // Map APBC unlock register window used by power-source handling.
  MapRegToGcdMmioSpace (K3_APBC_ASFAR & ~(UINT64)(SIZE_4KB - 1), SIZE_4KB);
}

/**
  Register K3 silicon pin conctrl Protocol

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
SpacemitK3PinCtrlDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS        Status;
  PINCTRL_INSTANCE  *PinCtrlInstance;

  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gSpacemitSiliconPinCtrlProtocolGuid);

  PinCtrlInstance = AllocateZeroPool (sizeof (PINCTRL_INSTANCE));
  if (NULL == PinCtrlInstance) {
    return EFI_OUT_OF_RESOURCES;
  }

  PinCtrlInstance->Signature                          = K3_PIN_SIGNATURE;
  PinCtrlInstance->PinCtrlProtocol.Revision           = SILICON_PINCTRL_PROTOCOL_REVISION;
  PinCtrlInstance->PinCtrlProtocol.SetPinConfig       = SetPinConfig;
  PinCtrlInstance->PinCtrlProtocol.SetPinGroupByName  = SetPinGroupByName;
  PinCtrlInstance->PinCtrlProtocol.RegisterController = RegisterController;
  PinCtrlInstance->PinCtrlProtocol.ApplyState         = ApplyState;
  PinCtrlInstance->PinCtrlProtocol.ApplyStateById     = ApplyStateById;
  PinCtrlInstance->PinCtrlProtocol.GetActiveState     = GetActiveState;

  Status = BoardPinctrlGetMap (&PinCtrlInstance->BoardMap);
  if (!EFI_ERROR (Status) && (PinCtrlInstance->BoardMap != NULL)) {
    Status = BuildStateIndex (PinCtrlInstance);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: board pinctrl map is invalid (%r)\n", __func__, Status));
      PinCtrlInstance->StateIndexCount = 0;
    }
  } else {
    DEBUG ((DEBUG_INFO, "%a: board pinctrl map is not provided (%r)\n", __func__, Status));
  }

  Status = gBS->InstallProtocolInterface (
                                          &PinCtrlInstance->Handle,
                                          &gSpacemitSiliconPinCtrlProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          &PinCtrlInstance->PinCtrlProtocol
                                          );

  if (!EFI_ERROR (Status)) {
    PinCtrlMmioRemap ();
  } else {
    FreePool (PinCtrlInstance);
    DEBUG (
           (DEBUG_ERROR,
            "%a: failed to install silicon pin control protocol (Status == %r)\n",
            __FUNCTION__, Status)
           );
  }

  return Status;
}
