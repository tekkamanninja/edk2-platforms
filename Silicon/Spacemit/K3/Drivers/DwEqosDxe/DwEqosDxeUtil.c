/** @file
  Synopsys DesignWare Ethernet Quality-of-Service (EQoS) driver - EDK2 port

  Copyright (c) 2025, Spacemit Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Protocol/Cpu.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "DwEqosDxeUtil.h"
#include "GenphyDxeUtil.h"

#define EQOS_DUMP_MAX_LEN  256

VOID
PrintPacket (
  IN CONST CHAR8  *Title,
  IN CONST UINT8  *Packet,
  IN UINT32       Length
  )
{
  UINT32  DumpLen;
  UINT32  i;

  if ((Packet == NULL) || (Length == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid parameter (Packet=%p, Length=%u)\n",
      __func__,
      Packet,
      Length
      ));
    return;
  }

  if (Title == NULL) {
    Title = "Packet";
  }

  DumpLen = (Length > EQOS_DUMP_MAX_LEN) ? EQOS_DUMP_MAX_LEN : Length;

  DEBUG ((DEBUG_INFO, "%a (length: %u, dump: %u):\n", Title, Length, DumpLen));

  for (i = 0; i < DumpLen; i++) {
    DEBUG ((DEBUG_INFO, "%02X ", Packet[i]));
    if (((i + 1) % 16) == 0) {
      DEBUG ((DEBUG_INFO, "\n"));
    }
  }

  if ((DumpLen % 16) != 0) {
    DEBUG ((DEBUG_INFO, "\n"));
  }

  if (DumpLen < Length) {
    DEBUG ((DEBUG_INFO, "... truncated (%u bytes not shown)\n", Length - DumpLen));
  }
}

EFI_STATUS
EFIAPI
EqosGetInterface (
  IN EQOS_DEVICE  *Eqos
  )
{
  CONST CHAR8  *PhyModeStr;
  EFI_STATUS   Status;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->ControllerConfig != NULL);

  PhyModeStr = Eqos->ControllerConfig->PhyMode;
  if ((PhyModeStr == NULL) || (PhyModeStr[0] == '\0')) {
    DEBUG ((DEBUG_ERROR, "%a: phy-mode not set\n", __func__));
    return EFI_NOT_FOUND;
  }

  Status = PhyGetInterfaceByName (PhyModeStr, &(Eqos->PhyInterface));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: unsupported phy-mode '%a'\n", __func__, PhyModeStr));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
VOID *
EqosAllocDescs (
  IN EQOS_DEVICE  *Eqos,
  IN UINT32       Num
  )
{
  UINTN  TotalSize;
  UINTN  Pages;
  VOID   *Buf;

  ASSERT (Eqos != NULL);
  ASSERT (Num > 0);

  Eqos->DescSize = EQOS_ALIGN (sizeof (EQOS_DESC), EQOS_DESC_ALIGN);
  TotalSize      = (UINTN)Num * (UINTN)Eqos->DescSize;
  Pages          = EFI_SIZE_TO_PAGES (TotalSize);

  Buf = AllocateAlignedPages (Pages, EQOS_DESC_ALIGN);
  if (Buf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: failed to allocate descriptors\n", __func__));
    Eqos->DescPages = 0;
    return NULL;
  }

  ZeroMem (Buf, EFI_PAGES_TO_SIZE (Pages));
  Eqos->DescPages = Pages;

  ASSERT (((UINTN)Buf & (EQOS_DESC_ALIGN - 1)) == 0);

  return Buf;
}

STATIC
VOID
EqosFreeDescs (
  IN EQOS_DEVICE  *Eqos
  )
{
  ASSERT (Eqos != NULL);
  ASSERT (Eqos->Descs != NULL);
  ASSERT (Eqos->DescPages > 0);

  FreePages (Eqos->Descs, Eqos->DescPages);
  Eqos->Descs     = NULL;
  Eqos->DescPages = 0;
}

STATIC
EQOS_DESC *
EqosGetDesc (
  IN EQOS_DEVICE  *Eqos,
  IN UINT32       Num,
  IN BOOLEAN      Rx
  )
{
  UINTN      Offset;
  EQOS_DESC  *Desc;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->Descs != NULL);
  ASSERT (Rx ? (Num < EQOS_DESCRIPTORS_RX) : (Num < EQOS_DESCRIPTORS_TX));

  Offset = ((Rx ? EQOS_DESCRIPTORS_TX : 0) + Num) * Eqos->DescSize;

  Desc = (EQOS_DESC *)((UINT8 *)Eqos->Descs + Offset);

  return Desc;
}

EFI_STATUS
EFIAPI
EqosNullOps (
  IN EQOS_DEVICE  *Eqos
  )
{
  ASSERT (Eqos != NULL);

  return EFI_SUCCESS;
}

STATIC
VOID
EqosInvalDescGeneric (
  IN EQOS_DEVICE  *Eqos,
  IN VOID         *Desc
  )
{
  EFI_STATUS  Status;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->Cpu != NULL);
  ASSERT (Desc != NULL);

  Status = Eqos->Cpu->FlushDataCache (
                        Eqos->Cpu,
                        (EFI_PHYSICAL_ADDRESS)(UINTN)Desc,
                        Eqos->DescSize,
                        EfiCpuFlushTypeInvalidate
                        );
  ASSERT_EFI_ERROR (Status);
}

STATIC
VOID
EqosFlushDescGeneric (
  IN EQOS_DEVICE  *Eqos,
  IN VOID         *Desc
  )
{
  EFI_STATUS  Status;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->Cpu != NULL);
  ASSERT (Desc != NULL);

  Status = Eqos->Cpu->FlushDataCache (
                        Eqos->Cpu,
                        (EFI_PHYSICAL_ADDRESS)(UINTN)Desc,
                        Eqos->DescSize,
                        EfiCpuFlushTypeWriteBackInvalidate
                        );
  ASSERT_EFI_ERROR (Status);
}

STATIC
VOID
EqosInvalBufferGeneric (
  IN EQOS_DEVICE  *Eqos,
  IN VOID         *Buf,
  IN UINTN        Size
  )
{
  EFI_STATUS  Status;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->Cpu != NULL);
  ASSERT (Buf != NULL);
  ASSERT (Size > 0);

  Status = Eqos->Cpu->FlushDataCache (
                        Eqos->Cpu,
                        (EFI_PHYSICAL_ADDRESS)(UINTN)Buf,
                        (UINT64)Size,
                        EfiCpuFlushTypeInvalidate
                        );
  ASSERT_EFI_ERROR (Status);
}

STATIC
VOID
EqosFlushBufferGeneric (
  IN EQOS_DEVICE  *Eqos,
  IN VOID         *Buf,
  IN UINTN        Size
  )
{
  EFI_STATUS  Status;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->Cpu != NULL);
  ASSERT (Buf != NULL);
  ASSERT (Size > 0);

  Status = Eqos->Cpu->FlushDataCache (
                        Eqos->Cpu,
                        (EFI_PHYSICAL_ADDRESS)(UINTN)Buf,
                        (UINT64)Size,
                        EfiCpuFlushTypeWriteBackInvalidate
                        );
  ASSERT_EFI_ERROR (Status);
}

STATIC
EFI_STATUS
EFIAPI
WaitForBitLe32 (
  IN UINTN  Addr,
  IN UINTN  Mask,
  IN UINTN  Value,
  IN UINTN  TimeoutMs
  )
{
  UINTN  TimeoutCount;

  ASSERT (TimeoutMs > 0);

  TimeoutCount = TimeoutMs;
  while (TimeoutCount--) {
    if ((MmioRead32 (Addr) & Mask) == Value) {
      return EFI_SUCCESS;
    }

    gBS->Stall (1000);
  }

  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
EqosMdioWaitIdle (
  IN EQOS_DEVICE  *Eqos
  )
{
  ASSERT (Eqos != NULL);

  return WaitForBitLe32 (
           Eqos->Base + EQOS_MAC_MDIO_ADDRESS,
           EQOS_MAC_MDIO_ADDRESS_GB,
           0,
           1000
           );
}

STATIC
EFI_STATUS
EFIAPI
EqosMdioRead (
  IN  MII_DEV  *Bus,
  IN  INT32    Addr,
  IN  INT32    Devad,
  IN  INT32    Reg,
  OUT INT32    *Val
  )
{
  EFI_STATUS   Status;
  EQOS_DEVICE  *Eqos;
  UINT32       RegValue;
  UINTN        MdioAddr;
  UINTN        MdioData;

  ASSERT (Bus != NULL);
  ASSERT (Val != NULL);
  ASSERT (Bus->Priv != NULL);

  Eqos     = (EQOS_DEVICE *)Bus->Priv;
  MdioAddr = (UINTN)(Eqos->Base + EQOS_MAC_MDIO_ADDRESS);
  MdioData = (UINTN)(Eqos->Base + EQOS_MAC_MDIO_DATA);

  ASSERT (Eqos->Config != NULL);
  ASSERT (Eqos->Config->MdioWait > 0);

  (VOID)Devad; // Clause 22 only for now

  Status = EqosMdioWaitIdle (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: MDIO not idle at entry\n", __func__));
    return Status;
  }

  RegValue  = MmioRead32 (MdioAddr);
  RegValue &= (EQOS_MAC_MDIO_ADDRESS_SKAP | EQOS_MAC_MDIO_ADDRESS_C45E);
  RegValue |= (Addr << EQOS_MAC_MDIO_ADDRESS_PA_SHIFT) |
              (Reg  << EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT) |
              (Eqos->Config->ConfigMacMdio << EQOS_MAC_MDIO_ADDRESS_CR_SHIFT) |
              (EQOS_MAC_MDIO_ADDRESS_GOC_READ << EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT) |
              EQOS_MAC_MDIO_ADDRESS_GB;

  MmioWrite32 (MdioAddr, RegValue);

  gBS->Stall (Eqos->Config->MdioWait);

  Status = EqosMdioWaitIdle (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: MDIO read did not complete\n", __func__));
    return Status;
  }

  RegValue  = MmioRead32 (MdioData);
  RegValue &= 0xffff;

  *Val = (INT32)RegValue;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
EqosMdioWrite (
  IN MII_DEV  *Bus,
  IN INT32    Addr,
  IN INT32    Devad,
  IN INT32    Reg,
  IN UINT16   Val
  )
{
  EFI_STATUS   Status;
  EQOS_DEVICE  *Eqos;
  UINT32       RegValue;
  UINTN        MdioAddr;
  UINTN        MdioData;

  ASSERT (Bus != NULL);
  ASSERT (Bus->Priv != NULL);

  Eqos     = (EQOS_DEVICE *)Bus->Priv;
  MdioAddr = (UINTN)(Eqos->Base + EQOS_MAC_MDIO_ADDRESS);
  MdioData = (UINTN)(Eqos->Base + EQOS_MAC_MDIO_DATA);

  ASSERT (Eqos->Config != NULL);
  ASSERT (Eqos->Config->MdioWait > 0);

  (VOID)Devad; // Clause 22 only for now

  Status = EqosMdioWaitIdle (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: MDIO not idle at entry\n", __func__));
    return Status;
  }

  MmioWrite32 (MdioData, Val);

  RegValue  = MmioRead32 (MdioAddr);
  RegValue &= (EQOS_MAC_MDIO_ADDRESS_SKAP | EQOS_MAC_MDIO_ADDRESS_C45E);
  RegValue |= (Addr << EQOS_MAC_MDIO_ADDRESS_PA_SHIFT) |
              (Reg  << EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT) |
              (Eqos->Config->ConfigMacMdio << EQOS_MAC_MDIO_ADDRESS_CR_SHIFT) |
              (EQOS_MAC_MDIO_ADDRESS_GOC_WRITE << EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT) |
              EQOS_MAC_MDIO_ADDRESS_GB;

  MmioWrite32 (MdioAddr, RegValue);

  gBS->Stall (Eqos->Config->MdioWait);

  Status = EqosMdioWaitIdle (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: MDIO write did not complete\n", __func__));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EqosSetFullDuplex (
  IN EQOS_DEVICE  *Eqos
  )
{
  ASSERT (Eqos != NULL);

  MmioOr32 (
    (UINTN)(Eqos->Base + EQOS_MAC_CONFIGURATION),
    EQOS_MAC_CONFIGURATION_DM
    );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EqosSetHalfDuplex (
  IN EQOS_DEVICE  *Eqos
  )
{
  ASSERT (Eqos != NULL);

  MmioAnd32 (
    (UINTN)(Eqos->Base + EQOS_MAC_CONFIGURATION),
    ~(UINT32)EQOS_MAC_CONFIGURATION_DM
    );

  MmioOr32 (
    (UINTN)(Eqos->Base + EQOS_MTL_TXQ0_OPERATION_MODE),
    EQOS_MTL_TXQ0_OPERATION_MODE_FTQ
    );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EqosSetGmiiSpeed (
  IN EQOS_DEVICE  *Eqos
  )
{
  ASSERT (Eqos != NULL);

  MmioAnd32 (
    (UINTN)(Eqos->Base + EQOS_MAC_CONFIGURATION),
    ~(UINT32)(EQOS_MAC_CONFIGURATION_PS | EQOS_MAC_CONFIGURATION_FES)
    );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EqosSetMiiSpeed100 (
  IN EQOS_DEVICE  *Eqos
  )
{
  ASSERT (Eqos != NULL);

  MmioOr32 (
    (UINTN)(Eqos->Base + EQOS_MAC_CONFIGURATION),
    EQOS_MAC_CONFIGURATION_PS | EQOS_MAC_CONFIGURATION_FES
    );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EqosSetMiiSpeed10 (
  IN EQOS_DEVICE  *Eqos
  )
{
  UINT32  Value;

  ASSERT (Eqos != NULL);

  Value  = MmioRead32 ((UINTN)(Eqos->Base + EQOS_MAC_CONFIGURATION));
  Value &= ~(UINT32)EQOS_MAC_CONFIGURATION_FES;
  Value |= EQOS_MAC_CONFIGURATION_PS;
  MmioWrite32 ((UINTN)(Eqos->Base + EQOS_MAC_CONFIGURATION), Value);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
EqosAdjustLink (
  IN EQOS_DEVICE  *Eqos
  )
{
  BOOLEAN     EnCalibration;
  EFI_STATUS  Status;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->PhyDev != NULL);
  ASSERT (Eqos->Config != NULL);
  ASSERT (Eqos->Config->PlatOps != NULL);

  if (!Eqos->PhyDev->Link) {
    DEBUG ((
      DEBUG_INFO,
      "%a: link is down\n",
      __func__
      ));
    return EFI_SUCCESS;
  }

  if (Eqos->PhyDev->Duplex != 0) {
    Status = EqosSetFullDuplex (Eqos);
  } else {
    Status = EqosSetHalfDuplex (Eqos);
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: EqosSet*Duplex() failed: %r\n",
      __func__,
      Status
      ));
    return Status;
  }

  switch (Eqos->PhyDev->Speed) {
    case SPEED_1000:
      EnCalibration = TRUE;
      Status        = EqosSetGmiiSpeed (Eqos);
      break;

    case SPEED_100:
      EnCalibration = TRUE;
      Status        = EqosSetMiiSpeed100 (Eqos);
      break;

    case SPEED_10:
      EnCalibration = FALSE;
      Status        = EqosSetMiiSpeed10 (Eqos);
      break;

    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: invalid speed %d\n",
        __func__,
        Eqos->PhyDev->Speed
        ));
      return EFI_INVALID_PARAMETER;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: EqosSet*MiiSpeed*() failed: %r\n",
      __func__,
      Status
      ));
    return Status;
  }

  if (EnCalibration) {
    Status = Eqos->Config->PlatOps->CalibratePads (Eqos);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: CalibratePads() failed: %r\n",
        __func__,
        Status
        ));
      return Status;
    }
  } else {
    Status = Eqos->Config->PlatOps->DisableCalibration (Eqos);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: DisableCalibration() failed: %r\n",
        __func__,
        Status
        ));
      return Status;
    }
  }

  Status = Eqos->Config->PlatOps->SetTxClkSpeed (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: SetTxClkSpeed() failed: %r\n",
      __func__,
      Status
      ));
    return Status;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: link is Up - %dMbps/%a\n",
    __func__,
    Eqos->PhyDev->Speed,
    (Eqos->PhyDev->Duplex == 1) ? "full" : "half"
    ));

  return EFI_SUCCESS;
}

EFI_STATUS
EqosCheckTxDescriptor (
  IN EQOS_DEVICE  *Eqos,
  IN UINT32       DescIndex
  )
{
  EQOS_DESC  *Descriptor;
  UINT32     Tdes3;

  ASSERT (Eqos != NULL);
  ASSERT (DescIndex < EQOS_DESCRIPTORS_TX);

  Descriptor = EqosGetDesc (Eqos, DescIndex, FALSE);
  Tdes3      = Descriptor->Tdes3;

  if (Tdes3 & EQOS_TDES3_TX_OWN) {
    return EFI_NOT_READY;
  }

  if (Tdes3 & EQOS_TDES3_TX_DE) {
    DEBUG ((DEBUG_ERROR, "%a: Descriptor Error\n", __func__));
    return EFI_DEVICE_ERROR;
  }

  if (Tdes3 & EQOS_TDES3_TX_ES) {
    if (Tdes3 & EQOS_TDES3_TX_EUE) {
      DEBUG ((DEBUG_ERROR, "%a: ECC Uncorrectable Error Status\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_JT) {
      DEBUG ((DEBUG_ERROR, "%a: Jabber Timeout\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_PF) {
      DEBUG ((DEBUG_ERROR, "%a: Packet Flushed\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_PCE) {
      DEBUG ((DEBUG_ERROR, "%a: Payload Checksum Error\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_LOC) {
      DEBUG ((DEBUG_ERROR, "%a: Loss of Carrier\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_NC) {
      DEBUG ((DEBUG_ERROR, "%a: No Carrier\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_LC) {
      DEBUG ((DEBUG_ERROR, "%a: Late Collision\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_EC) {
      DEBUG ((DEBUG_ERROR, "%a: Excessive Collision\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_ED) {
      DEBUG ((DEBUG_ERROR, "%a: Excessive Deferral\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_UF) {
      DEBUG ((DEBUG_ERROR, "%a: Underflow Error\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_TX_IHE) {
      DEBUG ((DEBUG_ERROR, "%a: IP Header Error\n", __func__));
    }

    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EqosCheckRxDescriptor (
  IN  EQOS_DEVICE  *Eqos,
  IN  UINT32       DescIndex,
  OUT UINT32       *FrameLength
  )
{
  EQOS_DESC  *Descriptor;
  UINT32     Tdes2;
  UINT32     Tdes3;

  ASSERT (Eqos != NULL);
  ASSERT (DescIndex < EQOS_DESCRIPTORS_RX);
  ASSERT (FrameLength != NULL);

  Descriptor = EqosGetDesc (Eqos, DescIndex, TRUE);
  Tdes2      = Descriptor->Tdes2;
  Tdes3      = Descriptor->Tdes3;

  if (Tdes3 & EQOS_TDES3_RX_OWN) {
    return EFI_NOT_READY;
  }

  if (Tdes3 & EQOS_TDES3_RX_ES) {
    if (Tdes3 & EQOS_TDES3_RX_CE) {
      DEBUG ((DEBUG_ERROR, "%a: CRC Error\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_RX_GP) {
      DEBUG ((DEBUG_ERROR, "%a: Giant Packet\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_RX_RWT) {
      DEBUG ((DEBUG_ERROR, "%a: Receive Watchdog Timeout\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_RX_OE) {
      DEBUG ((DEBUG_ERROR, "%a: Overflow Error\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_RX_RE) {
      DEBUG ((DEBUG_ERROR, "%a: Receive Error\n", __func__));
    }

    if (Tdes3 & EQOS_TDES3_RX_DE) {
      DEBUG ((DEBUG_ERROR, "%a: Dribble Bit Error\n", __func__));
    }

    if (Tdes2 & EQOS_TDES2_RX_DAF) {
      DEBUG ((DEBUG_ERROR, "%a: Destination Address Filter Fail\n", __func__));
    }

    if (Tdes2 & EQOS_TDES2_RX_SAF) {
      DEBUG ((DEBUG_ERROR, "%a: Source Address Filter Fail\n", __func__));
    }

    return EFI_DEVICE_ERROR;
  }

  *FrameLength = Tdes3 & EQOS_TDES3_RX_LENGTH_MASK;
  if ((*FrameLength == 0) || (*FrameLength < MIN_ETHERNET_PACKET_SIZE)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid Frame Length: %u\n", __func__, *FrameLength));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

VOID
EqosGetMacAddress (
  IN  EQOS_DEVICE      *Eqos,
  OUT EFI_MAC_ADDRESS  *MacAddress
  )
{
  UINT32  AddrLow;
  UINT32  AddrHigh;
  UINTN   Base;

  ASSERT (Eqos != NULL);
  ASSERT (MacAddress != NULL);
  Base = (UINTN)Eqos->Base;

  AddrLow  = MmioRead32 (Base + EQOS_MAC_ADDRESS0_LOW);
  AddrHigh = MmioRead32 (Base + EQOS_MAC_ADDRESS0_HIGH);

  MacAddress->Addr[0] = AddrLow & 0xff;
  MacAddress->Addr[1] = (AddrLow >> 8) & 0xff;
  MacAddress->Addr[2] = (AddrLow >> 16) & 0xff;
  MacAddress->Addr[3] = (AddrLow >> 24) & 0xff;
  MacAddress->Addr[4] = AddrHigh & 0xff;
  MacAddress->Addr[5] = (AddrHigh >> 8) & 0xff;
}

VOID
EqosSetMacAddress (
  IN  EQOS_DEVICE     *Eqos,
  IN EFI_MAC_ADDRESS  *MacAddress
  )
{
  UINT32  AddrLow;
  UINT32  AddrHigh;
  UINTN   Base;

  ASSERT (Eqos != NULL);
  ASSERT (MacAddress != NULL);
  Base = (UINTN)Eqos->Base;

  if (!Eqos->Config->RegAccessAlwaysOk && !Eqos->RegAccessOk) {
    return;
  }

  AddrLow   = MacAddress->Addr[0];
  AddrLow  |= MacAddress->Addr[1] << 8;
  AddrLow  |= MacAddress->Addr[2] << 16;
  AddrLow  |= MacAddress->Addr[3] << 24;
  AddrHigh  = MacAddress->Addr[4];
  AddrHigh |= MacAddress->Addr[5] << 8;
  MmioWrite32 (Base + EQOS_MAC_ADDRESS0_HIGH, AddrHigh);
  MmioWrite32 (Base + EQOS_MAC_ADDRESS0_LOW, AddrLow);
}

VOID
EqosGetDmaInterruptStatus (
  IN  EQOS_DEVICE  *Eqos,
  OUT UINT32       *InterruptStatus  OPTIONAL
  )
{
  UINT32  DmaStatus;
  UINT32  Mask;

  ASSERT (Eqos != NULL);

  if (InterruptStatus != NULL) {
    *InterruptStatus = 0;
  }

  DmaStatus = MmioRead32 (Eqos->Base + EQOS_DMA_CHAN0_STATUS);
  Mask      = 0;

  if (DmaStatus & EQOS_DMA_CHAN0_STATUS_NIS) {
    Mask |= EQOS_DMA_CHAN0_STATUS_NIS;

    if (DmaStatus & EQOS_DMA_CHAN0_STATUS_RI) {
      if (InterruptStatus != NULL) {
        Mask            |= EQOS_DMA_CHAN0_STATUS_RI;
        *InterruptStatus = EFI_SIMPLE_NETWORK_RECEIVE_INTERRUPT;
      }
    }

    if (DmaStatus & EQOS_DMA_CHAN0_STATUS_TI) {
      if (InterruptStatus != NULL) {
        Mask            |= EQOS_DMA_CHAN0_STATUS_TI;
        *InterruptStatus = EFI_SIMPLE_NETWORK_TRANSMIT_INTERRUPT;
      }
    }
  }

  if (DmaStatus & EQOS_DMA_CHAN0_STATUS_AIS) {
    Mask |= EQOS_DMA_CHAN0_STATUS_AIS;

    if (DmaStatus & EQOS_DMA_CHAN0_STATUS_CDE) {
      Mask |= EQOS_DMA_CHAN0_STATUS_CDE;
      DEBUG ((DEBUG_ERROR, "%a: Context Descriptor Error\n", __func__));
    }

    if (DmaStatus & EQOS_DMA_CHAN0_STATUS_FBE) {
      Mask |= EQOS_DMA_CHAN0_STATUS_FBE;
      DEBUG ((DEBUG_ERROR, "%a: Fatal Bus Error\n", __func__));

      if (DmaStatus & EQOS_DMA_CHAN0_STATUS_REB_DATA_TRANS) {
        DEBUG ((DEBUG_ERROR, "%a: Rx DMA Error Bits: Error during data transfer by Rx DMA\n", __func__));
      }

      if (DmaStatus & EQOS_DMA_CHAN0_STATUS_REB_DESC_ACC) {
        DEBUG ((DEBUG_ERROR, "%a: Rx DMA Error Bits: Error during descriptor access\n", __func__));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Rx DMA Error Bits: Error during data buffer access\n", __func__));
      }

      if (DmaStatus & EQOS_DMA_CHAN0_STATUS_REB_READ_TRANS) {
        DEBUG ((DEBUG_ERROR, "%a: Rx DMA Error Bits: Error during read transfer\n", __func__));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Rx DMA Error Bits: Error during write transfer\n", __func__));
      }

      if (DmaStatus & EQOS_DMA_CHAN0_STATUS_TEB_DATA_TRANS) {
        DEBUG ((DEBUG_ERROR, "%a: Tx DMA Error Bits: Error during data transfer by Tx DMA\n", __func__));
      }

      if (DmaStatus & EQOS_DMA_CHAN0_STATUS_TEB_DESC_ACC) {
        DEBUG ((DEBUG_ERROR, "%a: Tx DMA Error Bits: Error during descriptor access\n", __func__));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Tx DMA Error Bits: Error during data buffer access\n", __func__));
      }

      if (DmaStatus & EQOS_DMA_CHAN0_STATUS_TEB_READ_TRANS) {
        DEBUG ((DEBUG_ERROR, "%a: Tx DMA Error Bits: Error during read transfer\n", __func__));
      } else {
        DEBUG ((DEBUG_ERROR, "%a: Tx DMA Error Bits: Error during write transfer\n", __func__));
      }
    }
  }

  MmioOr32 (Eqos->Base + EQOS_DMA_CHAN0_STATUS, Mask);
}

EFI_STATUS
EFIAPI
EqosStart (
  IN EQOS_DEVICE  *Eqos
  )
{
  EFI_STATUS  Status;
  UINTN       Base;
  INT32       i;
  UINT64      Rate;
  UINT32      Val;
  UINT32      TxFifoSz;
  UINT32      RxFifoSz;
  UINT32      Tqs;
  UINT32      Rqs;
  UINT32      Pbl;
  UINT32      LastRxDesc;
  UINT32      DescPad;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->RxBuffer != NULL);
  ASSERT (Eqos->Descs != NULL);
  ASSERT (Eqos->Mdio != NULL);
  ASSERT (Eqos->Config != NULL);
  ASSERT (Eqos->Config->PlatOps != NULL);

  if (Eqos->Started) {
    return EFI_SUCCESS;
  }

  Base = (UINTN)Eqos->Base;

  Eqos->TxDescIdx = 0;
  Eqos->RxDescIdx = 0;

  Status = Eqos->Config->PlatOps->DeassertReset (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: DeassertReset() failed\n", __func__));
    goto ExitError;
  }

  gBS->Stall (2 * 1000);

  Eqos->RegAccessOk = TRUE;

  Status = WaitForBitLe32 (
             Base + EQOS_DMA_MODE,
             EQOS_DMA_MODE_SWR,
             0,
             Eqos->Config->SwrWait
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: EQOS_DMA_MODE_SWR stuck\n", __func__));
    goto ExitStopResets;
  }

  Status = Eqos->Config->PlatOps->CalibratePads (Eqos);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: CalibratePads() failed\n", __func__));
    goto ExitStopResets;
  }

  Rate = Eqos->Config->CsrClockRate;
  if (Rate != 0) {
    Val = (UINT32)(Rate / 1000000U) - 1U;
    MmioWrite32 (Base + EQOS_MAC_1US_TIC_COUNTER, Val);
  }

  /*
     if PHY was already connected and configured,
     don't need to reconnect/reconfigure again
  */
  if (Eqos->PhyDev == NULL) {
    UINT32  MaxSpeed;
    INT32   PhyAddr;

    MaxSpeed = Eqos->ControllerConfig->MaxSpeed;
    PhyAddr  = (INT32)Eqos->ControllerConfig->PhyAddr;

    Status = Eqos->Config->GetInterface (Eqos);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: GetInterface() failed\n", __func__));
      goto ExitStopResets;
    }

    Status = PhyConnect (
               Eqos->Mdio,
               PhyAddr,
               &(Eqos->PhyDev),
               Eqos,
               Eqos->PhyInterface
               );
    if (EFI_ERROR (Status) || (Eqos->PhyDev == NULL)) {
      DEBUG ((DEBUG_ERROR, "%a: PhyConnect() failed: %r\n", __func__, Status));
      goto ExitStopResets;
    }

    Status = PhySetSupported (Eqos->PhyDev, MaxSpeed);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: PhySetSupported() failed: %r\n", __func__, Status));
      goto ExitShutdownPhy;
    }

    Status = PhyConfig (Eqos->PhyDev);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: PhyConfig() failed: %r\n", __func__, Status));
      goto ExitShutdownPhy;
    }
  }

  // Configure MTL

  // Enable Store and Forward mode for TX
  // Program Tx operating mode
  Val  = MmioRead32 (Base + EQOS_MTL_TXQ0_OPERATION_MODE);
  Val |= EQOS_MTL_TXQ0_OPERATION_MODE_TSF;
  Val &= ~EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_MASK;
  Val |= (EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_EN << EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT);
  MmioWrite32 (Base + EQOS_MTL_TXQ0_OPERATION_MODE, Val);

  // Transmit Queue weight
  MmioWrite32 (Base + EQOS_MTL_TXQ0_WEIGHT, 0x10);

  // Enable Store and Forward mode for RX, since no jumbo frame
  MmioOr32 (Base + EQOS_MTL_RXQ0_OPERATION_MODE, EQOS_MTL_RXQ0_OPERATION_MODE_RSF);

  // Transmit/Receive queue fifo size; use all RAM for 1 queue
  Val      = MmioRead32 (Base + EQOS_MAC_HW_FEATURE (1));
  TxFifoSz = (Val & EQOS_MAC_HW_FEATURE1_TXFIFOSIZE) >> 6;
  RxFifoSz = (Val & EQOS_MAC_HW_FEATURE1_RXFIFOSIZE) >> 0;

  // r/tx_fifo_sz is encoded as log2(n / 128). Undo that by shifting.
  // r/tqs is encoded as (n / 256) - 1.
  Tqs = (UINT32)((128U << TxFifoSz) / 256U) - 1U;
  Rqs = (UINT32)((128U << RxFifoSz) / 256U) - 1U;

  Val  = MmioRead32 (Base + EQOS_MTL_TXQ0_OPERATION_MODE);
  Val &= ~EQOS_MTL_TXQ0_OPERATION_MODE_TQS;
  Val |= ((Tqs << 16) & EQOS_MTL_TXQ0_OPERATION_MODE_TQS);
  MmioWrite32 (Base + EQOS_MTL_TXQ0_OPERATION_MODE, Val);

  Val  = MmioRead32 (Base + EQOS_MTL_RXQ0_OPERATION_MODE);
  Val &= ~EQOS_MTL_RXQ0_OPERATION_MODE_RQS;
  Val |= ((Rqs << 20) & EQOS_MTL_RXQ0_OPERATION_MODE_RQS);
  MmioWrite32 (Base + EQOS_MTL_RXQ0_OPERATION_MODE, Val);

  // Flow control used only if each channel gets 4KB or more FIFO
  if (Rqs >= ((4096U / 256U) - 1U)) {
    UINT32  Rfd;
    UINT32  Rfa;

    MmioOr32 (Base + EQOS_MTL_RXQ0_OPERATION_MODE, EQOS_MTL_RXQ0_OPERATION_MODE_EHFC);

    if (Rqs == ((4096U / 256U) - 1U)) {
      Rfd = 0x3;
      Rfa = 0x1;
    } else if (Rqs == ((8192U / 256U) - 1U)) {
      Rfd = 0x6;
      Rfa = 0xA;
    } else if (Rqs == ((16384U / 256U) - 1U)) {
      Rfd = 0x6;
      Rfa = 0x12;
    } else {
      Rfd = 0x6;
      Rfa = 0x1E;
    }

    Val  = MmioRead32 (Base + EQOS_MTL_RXQ0_OPERATION_MODE);
    Val &= ~(EQOS_MTL_RXQ0_OPERATION_MODE_RFD | EQOS_MTL_RXQ0_OPERATION_MODE_RFA);
    Val |= ((Rfd << 14) & EQOS_MTL_RXQ0_OPERATION_MODE_RFD) |
           ((Rfa << 8)  & EQOS_MTL_RXQ0_OPERATION_MODE_RFA);
    MmioWrite32 (Base + EQOS_MTL_RXQ0_OPERATION_MODE, Val);
  }

  // Configure MAC
  Val  = MmioRead32 (Base + EQOS_RXQ_CTRL0);
  Val &= ~EQOS_RXQ_CTRL0_EN_MASK;
  Val |= (UINT32)(Eqos->Config->ConfigMac & EQOS_RXQ_CTRL0_EN_MASK);
  MmioWrite32 (Base + EQOS_RXQ_CTRL0, Val);

  // Multicast and Broadcast Queue Enable
  Val  = MmioRead32 (Base + EQOS_RXQ_CTRL1);
  Val |= 0x00100000U;
  MmioWrite32 (Base + EQOS_RXQ_CTRL1, Val);

  // Set TX flow control parameters
  // Set Pause Time
  Val  = MmioRead32 (Base + EQOS_MAC_Q0_TX_FLOW_CTRL);
  Val |= (0xFFFFU << EQOS_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT);
  MmioWrite32 (Base + EQOS_MAC_Q0_TX_FLOW_CTRL, Val);

  // Assign priority for TX flow control
  Val  = MmioRead32 (Base + EQOS_MAC_TX_PRTY_MAP0);
  Val &= ~EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_MASK;
  MmioWrite32 (Base + EQOS_MAC_TX_PRTY_MAP0, Val);

  // Assign priority for RX flow control
  Val  = MmioRead32 (Base + EQOS_RXQ_CTRL2);
  Val &= ~EQOS_MAC_RXQ_CTRL2_PSRQ0_MASK;
  MmioWrite32 (Base + EQOS_RXQ_CTRL2, Val);

  // Enable flow control
  MmioOr32 (Base + EQOS_MAC_Q0_TX_FLOW_CTRL, EQOS_MAC_Q0_TX_FLOW_CTRL_TFE);
  MmioOr32 (Base + EQOS_MAC_RX_FLOW_CTRL, EQOS_MAC_RX_FLOW_CTRL_RFE);

  Val  = MmioRead32 (Base + EQOS_MAC_CONFIGURATION);
  Val &= ~(EQOS_MAC_CONFIGURATION_GPSLCE |
           EQOS_MAC_CONFIGURATION_WD     |
           EQOS_MAC_CONFIGURATION_JD     |
           EQOS_MAC_CONFIGURATION_JE);
  Val |= (EQOS_MAC_CONFIGURATION_CST | EQOS_MAC_CONFIGURATION_ACS);
  MmioWrite32 (Base + EQOS_MAC_CONFIGURATION, Val);

  // Enable OSP mode
  MmioOr32 (Base + EQOS_DMA_CHAN0_TX_CONTROL, EQOS_DMA_CHAN0_TX_CONTROL_OSP);

  // RX buffer size. Must be a multiple of bus width
  Val  = MmioRead32 (Base + EQOS_DMA_CHAN0_RX_CONTROL);
  Val &= ~EQOS_DMA_CHAN0_RX_CONTROL_RBSZ_MASK;
  Val |= (EQOS_MAX_PACKET_SIZE << EQOS_DMA_CHAN0_RX_CONTROL_RBSZ_SHIFT);
  MmioWrite32 (Base + EQOS_DMA_CHAN0_RX_CONTROL, Val);

  DescPad = (Eqos->DescSize - sizeof (EQOS_DESC)) /
            (UINT32)Eqos->Config->AxiBusWidth;

  Val  = MmioRead32 (Base + EQOS_DMA_CHAN0_CONTROL);
  Val |= EQOS_DMA_CHAN0_CONTROL_PBLX8;
  Val &= ~EQOS_DMA_CHAN0_CONTROL_DSL_MASK;
  Val |= (DescPad << EQOS_DMA_CHAN0_CONTROL_DSL_SHIFT) &
         EQOS_DMA_CHAN0_CONTROL_DSL_MASK;
  MmioWrite32 (Base + EQOS_DMA_CHAN0_CONTROL, Val);

  /*
    Burst length must be < 1/2 FIFO size.
    FIFO size in tqs is encoded as (n / 256) - 1.
    Each burst is n * 8 (PBLX8) * 16 (AXI width) == 128 bytes.
    Half of n * 256 is n * 128, so pbl == tqs, modulo the -1.
  */
  Pbl = Tqs + 1U;
  if (Pbl > 32U) {
    Pbl = 32U;
  }

  Val  = MmioRead32 (Base + EQOS_DMA_CHAN0_TX_CONTROL);
  Val &= ~EQOS_DMA_CHAN0_TX_CONTROL_TXPBL_MASK;
  Val |= (Pbl << EQOS_DMA_CHAN0_TX_CONTROL_TXPBL_SHIFT) &
         EQOS_DMA_CHAN0_TX_CONTROL_TXPBL_MASK;
  MmioWrite32 (Base + EQOS_DMA_CHAN0_TX_CONTROL, Val);

  Val  = MmioRead32 (Base + EQOS_DMA_CHAN0_RX_CONTROL);
  Val &= ~EQOS_DMA_CHAN0_RX_CONTROL_RXPBL_MASK;
  Val |= (8U << EQOS_DMA_CHAN0_RX_CONTROL_RXPBL_SHIFT) &
         EQOS_DMA_CHAN0_RX_CONTROL_RXPBL_MASK;
  MmioWrite32 (Base + EQOS_DMA_CHAN0_RX_CONTROL, Val);

  // DMA performance configuration
  Val = (2U << EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT) |
        EQOS_DMA_SYSBUS_MODE_EAME |
        EQOS_DMA_SYSBUS_MODE_BLEN16 |
        EQOS_DMA_SYSBUS_MODE_BLEN8  |
        EQOS_DMA_SYSBUS_MODE_BLEN4;
  MmioWrite32 (Base + EQOS_DMA_SYSBUS_MODE, Val);

  // Set up descriptors
  ZeroMem (Eqos->Descs, Eqos->DescSize * EQOS_DESCRIPTORS_NUM);

  for (i = 0; i < (INT32)EQOS_DESCRIPTORS_TX; i++) {
    EQOS_DESC  *TxDesc;

    TxDesc = EqosGetDesc (Eqos, (UINT32)i, FALSE);
    EqosFlushDescGeneric (Eqos, TxDesc);
  }

  for (i = 0; i < (INT32)EQOS_DESCRIPTORS_RX; i++) {
    EQOS_DESC             *RxDesc;
    EFI_PHYSICAL_ADDRESS  BufAddr;
    VOID                  *Buf;
    UINTN                 Offset;

    RxDesc = EqosGetDesc (Eqos, (UINT32)i, TRUE);

    Offset  = (UINTN)i * EQOS_MAX_PACKET_SIZE;
    BufAddr = (EFI_PHYSICAL_ADDRESS)((UINTN)Eqos->RxBuffer + Offset);
    Buf     = (VOID *)(UINTN)BufAddr;

    RxDesc->Tdes0 = (UINT32)(BufAddr & 0xFFFFFFFFU);
    RxDesc->Tdes1 = (UINT32)(BufAddr >> 32);
    RxDesc->Tdes3 = EQOS_TDES3_RX_OWN |
                    EQOS_TDES3_RX_BUF1V |
                    EQOS_TDES3_RX_IOC;

    MemoryFence ();
    EqosFlushDescGeneric (Eqos, RxDesc);
    EqosInvalBufferGeneric (Eqos, Buf, EQOS_MAX_PACKET_SIZE);
  }

  {
    UINT64  TxDescBase;

    TxDescBase = (UINT64)(UINTN)EqosGetDesc (Eqos, 0, FALSE);
    MmioWrite32 (Base + EQOS_DMA_CHAN0_TX_BASE_ADDR_HI, (UINT32)(TxDescBase >> 32));
    MmioWrite32 (Base + EQOS_DMA_CHAN0_TX_BASE_ADDR, (UINT32)(TxDescBase & 0xFFFFFFFFU));
    MmioWrite32 (Base + EQOS_DMA_CHAN0_TX_RING_LEN, EQOS_DESCRIPTORS_TX - 1U);
  }

  {
    UINT64  RxDescBase;

    RxDescBase = (UINT64)(UINTN)EqosGetDesc (Eqos, 0, TRUE);
    MmioWrite32 (Base + EQOS_DMA_CHAN0_RX_BASE_ADDR_HI, (UINT32)(RxDescBase >> 32));
    MmioWrite32 (Base + EQOS_DMA_CHAN0_RX_BASE_ADDR, (UINT32)(RxDescBase & 0xFFFFFFFFU));
    MmioWrite32 (Base + EQOS_DMA_CHAN0_RX_RING_LEN, EQOS_DESCRIPTORS_RX - 1U);
  }

  // Enable everything
  Val  = MmioRead32 (Base + EQOS_DMA_CHAN0_TX_CONTROL);
  Val |= EQOS_DMA_CHAN0_TX_CONTROL_START;
  MmioWrite32 (Base + EQOS_DMA_CHAN0_TX_CONTROL, Val);

  Val  = MmioRead32 (Base + EQOS_DMA_CHAN0_RX_CONTROL);
  Val |= EQOS_DMA_CHAN0_RX_CONTROL_START;
  MmioWrite32 (Base + EQOS_DMA_CHAN0_RX_CONTROL, Val);

  Val  = MmioRead32 (Base + EQOS_MAC_CONFIGURATION);
  Val |= EQOS_MAC_CONFIGURATION_TE | EQOS_MAC_CONFIGURATION_RE;
  MmioWrite32 (Base + EQOS_MAC_CONFIGURATION, Val);

  EqosSetMacAddress (Eqos, &(Eqos->SnpMode.CurrentAddress));
  // TX tail pointer not written until we need to TX a packet

  /*
    Point RX tail pointer at last descriptor. Ideally, we'd point at the
    first descriptor, implying all descriptors were available. However,
    that's not distinguishable from none of the descriptors being
    available.
  */
  LastRxDesc = (UINT32)(UINTN)EqosGetDesc (Eqos, EQOS_DESCRIPTORS_RX - 1U, TRUE);
  MmioWrite32 (Base + EQOS_DMA_CHAN0_RX_END_ADDR, LastRxDesc);

  Eqos->Started = TRUE;

  return EFI_SUCCESS;

ExitShutdownPhy:
  PhyShutdown (Eqos->PhyDev);
  Eqos->PhyDev = NULL;
ExitStopResets:
  Eqos->Config->PlatOps->AssertReset (Eqos);
ExitError:
  Eqos->Started = FALSE;
  return Status;
}

EFI_STATUS
EqosStop (
  IN EQOS_DEVICE  *Eqos
  )
{
  EFI_STATUS  Status;
  UINTN       Base;
  UINT32      i;
  UINT32      val;
  UINT32      trcsts;
  UINT32      txqsts;
  UINT32      prxq;
  UINT32      rxqsts;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->Config != NULL);
  ASSERT (Eqos->Config->PlatOps != NULL);

  if (!Eqos->Started) {
    return EFI_SUCCESS;
  }

  Base = (UINTN)Eqos->Base;

  // Disable TX DMA
  MmioAnd32 (Base + EQOS_DMA_CHAN0_TX_CONTROL, ~EQOS_DMA_CH0_TX_CONTROL_ST);

  // Wait for TX to drain out of MTL
  for (i = 0; i < 1000000; i++) {
    val    = MmioRead32 (Base + EQOS_MTL_TXQ0_DEBUG);
    trcsts = (val >> EQOS_MTL_TXQ0_DEBUG_TRCSTS_SHIFT) & EQOS_MTL_TXQ0_DEBUG_TRCSTS_MASK;
    txqsts = val & EQOS_MTL_TXQ0_DEBUG_TXQSTS;

    if ((trcsts != 1) && (txqsts == 0)) {
      break;
    }
  }

  // Turn off MAC TX and RX
  MmioAnd32 (Base + EQOS_MAC_CONFIGURATION, ~(EQOS_MAC_CONFIGURATION_TE | EQOS_MAC_CONFIGURATION_RE));

  // Wait for RX to drain out of MTL
  for (i = 0; i < 1000000; i++) {
    val    = MmioRead32 (Base + EQOS_MTL_RXQ0_DEBUG);
    prxq   = (val >> EQOS_MTL_RXQ0_DEBUG_PRXQ_SHIFT) & EQOS_MTL_RXQ0_DEBUG_PRXQ_MASK;
    rxqsts = (val >> EQOS_MTL_RXQ0_DEBUG_RXQSTS_SHIFT) & EQOS_MTL_RXQ0_DEBUG_RXQSTS_MASK;

    if ((prxq == 0) && (rxqsts == 0)) {
      break;
    }
  }

  // Turn off RX DMA
  MmioAnd32 (Base + EQOS_DMA_CHAN0_RX_CONTROL, ~EQOS_DMA_CH0_RX_CONTROL_SR);

  // Shutdown PHY if it's connected
  if (Eqos->PhyDev) {
    Status = PhyShutdown (Eqos->PhyDev);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: PhyShutdown() failed: %r\n", __func__, Status));
    }

    Eqos->PhyDev = NULL;
  }

  // Stop hardware resets
  Eqos->Config->PlatOps->AssertReset (Eqos);

  Eqos->Started     = FALSE;
  Eqos->RegAccessOk = FALSE;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EqosSend (
  IN EQOS_DEVICE  *Eqos,
  IN VOID         *Buffer,
  IN UINT32       Length
  )
{
  UINTN      Base;
  EQOS_DESC  *TxDesc;
  EQOS_DESC  *NextDesc;
  UINTN      Addr;
  VOID       *TxBuf;

  //
  // Preconditions:
  // These conditions are guaranteed by the upper layer (e.g. SNP driver / caller).
  // They should never be violated in normal runtime. ASSERTs are here only to
  // catch programming errors early during development/debug builds.
  //
  ASSERT (Eqos != NULL);
  ASSERT (Eqos->TxBuffer != NULL);
  ASSERT (Buffer != NULL);
  ASSERT (Length <= EQOS_MAX_PACKET_SIZE);

  Base  = (UINTN)Eqos->Base;
  TxBuf = (VOID *)((UINTN)Eqos->TxBuffer + (Eqos->TxDescIdx * EQOS_MAX_PACKET_SIZE));
  CopyMem (TxBuf, Buffer, Length);

  EqosFlushBufferGeneric (Eqos, TxBuf, Length);

  TxDesc = EqosGetDesc (Eqos, Eqos->TxDescIdx, FALSE);
  EqosInvalDescGeneric (Eqos, TxDesc);

  if ((TxDesc->Tdes3 & EQOS_TDES3_TX_OWN) != 0) {
    return EFI_NOT_READY;
  }

  Eqos->TxDescIdx++;
  Eqos->TxDescIdx %= EQOS_DESCRIPTORS_TX;

  Addr = (UINTN)TxBuf;

  TxDesc->Tdes0 = (UINT32)(Addr & 0xFFFFFFFFU);
  TxDesc->Tdes1 = (UINT32)(Addr >> 32);
  TxDesc->Tdes2 = Length;

  MemoryFence ();

  TxDesc->Tdes3 = EQOS_TDES3_TX_OWN |
                  EQOS_TDES3_TX_FD  |
                  EQOS_TDES3_TX_LD  |
                  Length;

  EqosFlushDescGeneric (Eqos, TxDesc);

  NextDesc = EqosGetDesc (Eqos, Eqos->TxDescIdx, FALSE);
  MmioWrite32 (Base + EQOS_DMA_CHAN0_TX_END_ADDR, (UINT32)(UINTN)NextDesc);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EqosFreePkt (
  IN EQOS_DEVICE  *Eqos,
  IN UINT8        *Packet,
  IN INT32        Length
  )
{
  UINT8      *PacketExpected;
  EQOS_DESC  *RxDesc;

  ASSERT (Eqos != NULL);

  PacketExpected = (UINT8 *)(Eqos->RxBuffer + (Eqos->RxDescIdx * EQOS_MAX_PACKET_SIZE));

  if (Packet != PacketExpected) {
    DEBUG ((DEBUG_ERROR, "%a: Unexpected packet received!\n", __func__));
    return EFI_ABORTED;
  }

  EqosInvalBufferGeneric (Eqos, Packet, Length);

  RxDesc = EqosGetDesc (Eqos, Eqos->RxDescIdx, TRUE);

  RxDesc->Tdes0 = 0;

  MemoryFence ();
  EqosFlushDescGeneric (Eqos, RxDesc);
  EqosInvalBufferGeneric (Eqos, Packet, Length);

  RxDesc->Tdes0 = (UINT32)((UINTN)Packet & 0xFFFFFFFFU);
  RxDesc->Tdes1 = (UINT32)((UINTN)Packet >> 32);
  RxDesc->Tdes2 = 0;

  /*
    Make sure that if HW sees the _OWN write below, it will see all the
    writes to the rest of the descriptor too.
  */
  MemoryFence ();
  RxDesc->Tdes3 = EQOS_TDES3_RX_OWN | EQOS_TDES3_RX_BUF1V;

  EqosFlushDescGeneric (Eqos, RxDesc);

  MmioWrite32 (Eqos->Base + EQOS_DMA_CHAN0_RX_END_ADDR, (UINT32)(UINTN)RxDesc);

  Eqos->RxDescIdx++;
  Eqos->RxDescIdx %= EQOS_DESCRIPTORS_RX;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EqosRecv (
  IN  EQOS_DEVICE  *Eqos,
  OUT VOID         *Buffer,
  OUT UINT32       *Length
  )
{
  EFI_STATUS  Status;
  EQOS_DESC   *RxDesc;
  VOID        *Packet;
  UINT32      Len;

  //
  // Preconditions:
  // These conditions are guaranteed by the upper layer (e.g. SNP driver / caller).
  // They should never be violated in normal runtime. ASSERTs are here only to
  // catch programming errors early during development/debug builds.
  //
  ASSERT (Eqos != NULL);
  ASSERT (Eqos->RxBuffer != NULL);
  ASSERT (Buffer != NULL);
  ASSERT (Length != NULL);

  // Get the RX descriptor for the current packet
  RxDesc = EqosGetDesc (Eqos, Eqos->RxDescIdx, TRUE);
  EqosInvalDescGeneric (Eqos, RxDesc);

  // Check the descriptor for errors
  Status = EqosCheckRxDescriptor (Eqos, Eqos->RxDescIdx, &Len);
  if (EFI_ERROR (Status)) {
    // If not ready, return without error
    if (Status == EFI_NOT_READY) {
      return EFI_NOT_READY;
    }

    // On RX error frames, the HW-reported length field may be unreliable/undefined.
    Len = EQOS_MAX_PACKET_SIZE;
    EqosFreePkt (Eqos, (UINT8 *)(Eqos->RxBuffer + (Eqos->RxDescIdx * EQOS_MAX_PACKET_SIZE)), Len);
    return Status;
  }

  // Set the output packet and its length
  Packet  = (UINT8 *)Eqos->RxBuffer + (Eqos->RxDescIdx * EQOS_MAX_PACKET_SIZE);
  *Length = Len;

  EqosInvalBufferGeneric (Eqos, Packet, Len);
  CopyMem (Buffer, Packet, Len);

  EqosFreePkt (Eqos, (UINT8 *)Packet, Len);

  return EFI_SUCCESS;
}

EFI_STATUS
EqosUpdateLink (
  IN EQOS_DEVICE  *Eqos
  )
{
  EFI_STATUS  Status;
  INT32       OldLink;
  INT32       NewLink;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->PhyDev != NULL);

  OldLink = Eqos->PhyDev->Link;

  Status = PhyUpdateLink (Eqos->PhyDev);
  if (EFI_ERROR (Status)) {
    Eqos->SnpMode.MediaPresent = FALSE;
    return Status;
  }

  NewLink = Eqos->PhyDev->Link;

  if (OldLink != NewLink) {
    Status = EqosAdjustLink (Eqos);
    if (EFI_ERROR (Status)) {
      Eqos->SnpMode.MediaPresent = FALSE;
      return Status;
    }
  }

  Eqos->SnpMode.MediaPresent = (NewLink != 0);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
EqosAllocateDescAndBuffer (
  IN EQOS_DEVICE  *Eqos
  )
{
  EFI_STATUS  Status;
  VOID        *Descs;
  VOID        *TxBuf;
  VOID        *RxBuf;

  ASSERT (Eqos != NULL);

  Descs = EqosAllocDescs (Eqos, EQOS_DESCRIPTORS_NUM);
  if (Descs == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: EqosAllocDescs() failed\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  Eqos->Descs = Descs;

  TxBuf = AllocatePages (EFI_SIZE_TO_PAGES (EQOS_TX_BUFFER_SIZE));
  if (TxBuf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocatePages(TxBuf) failed\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorFreeDescs;
  }

  Eqos->TxBuffer = TxBuf;

  RxBuf = AllocatePages (EFI_SIZE_TO_PAGES (EQOS_RX_BUFFER_SIZE));
  if (RxBuf == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocatePages(RxBuf) failed\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorFreeTxBuf;
  }

  Eqos->RxBuffer = RxBuf;

  EqosInvalBufferGeneric (
    Eqos,
    RxBuf,
    EQOS_RX_BUFFER_SIZE
    );

  return EFI_SUCCESS;

ErrorFreeTxBuf:
  FreePages (TxBuf, EFI_SIZE_TO_PAGES (EQOS_TX_BUFFER_SIZE));
ErrorFreeDescs:
  EqosFreeDescs (Eqos);

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
EqosFreeDescAndBuffer (
  IN EQOS_DEVICE  *Eqos
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  ASSERT (Eqos != NULL);

  if (Eqos->RxBuffer != NULL) {
    FreePages (Eqos->RxBuffer, EFI_SIZE_TO_PAGES (EQOS_RX_BUFFER_SIZE));
    Eqos->RxBuffer = NULL;
  }

  if (Eqos->TxBuffer != NULL) {
    FreePages (Eqos->TxBuffer, EFI_SIZE_TO_PAGES (EQOS_TX_BUFFER_SIZE));
    Eqos->TxBuffer = NULL;
  }

  if (Eqos->Descs != NULL) {
    EqosFreeDescs (Eqos);
  }

  return Status;
}

BOOLEAN
EFIAPI
MacIsZero (
  IN CONST UINT8  *Mac
  )
{
  UINT8  i;

  for (i = 0; i < 6; i++) {
    if (Mac[i] != 0x00) {
      return FALSE;
    }
  }

  return TRUE;
}

BOOLEAN
EFIAPI
MacIsBroadcast (
  IN CONST UINT8  *Mac
  )
{
  UINT8  i;

  for (i = 0; i < 6; i++) {
    if (Mac[i] != 0xFF) {
      return FALSE;
    }
  }

  return TRUE;
}

STATIC
UINT32
EqosEtherCrc32Le (
  IN UINT8  *Buffer,
  IN UINTN  Length
  )
{
  STATIC CONST UINT32  CrcTable[] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  UINT32  Crc;
  UINTN   Index;

  Crc = 0xffffffffU;

  for (Index = 0; Index < Length; Index++) {
    Crc ^= Buffer[Index];
    Crc  = (Crc >> 4) ^ CrcTable[Crc & 0xf];
    Crc  = (Crc >> 4) ^ CrcTable[Crc & 0xf];
  }

  return (Crc);
}

STATIC
UINT32
EqosBitReverse32 (
  IN UINT32  Value
  )
{
  Value = (((Value & 0xaaaaaaaa) >> 1) | ((Value & 0x55555555) << 1));
  Value = (((Value & 0xcccccccc) >> 2) | ((Value & 0x33333333) << 2));
  Value = (((Value & 0xf0f0f0f0) >> 4) | ((Value & 0x0f0f0f0f) << 4));
  Value = (((Value & 0xff00ff00) >> 8) | ((Value & 0x00ff00ff) << 8));

  return (Value >> 16) | (Value << 16);
}

EFI_STATUS
EqosSetRxFilters (
  IN EQOS_DEVICE      *Eqos,
  IN UINT32           ReceiveFilterSetting,
  IN BOOLEAN          ResetMCastFilter,
  IN UINTN            MCastFilterCnt        OPTIONAL,
  IN EFI_MAC_ADDRESS  *MCastFilter          OPTIONAL
  )
{
  UINT32  Index;
  UINT32  PacketFilter;
  UINT32  Crc;
  UINT32  HashReg;
  UINT32  HashBit;
  UINT32  Hash;

  ASSERT (Eqos != NULL);

  if (ResetMCastFilter) {
    for (HashReg = 0; HashReg < EQOS_MAC_HASH_TABLE_COUNT; HashReg++) {
      MmioWrite32 (Eqos->Base + EQOS_MAC_HASH_TABLE_REG (HashReg), 0x0);
    }
  }

  PacketFilter = MmioRead32 (Eqos->Base + EQOS_MAC_PACKET_FILTER);

  PacketFilter &= ~(EQOS_MAC_PACKET_FILTER_PCF_MASK |
                    EQOS_MAC_PACKET_FILTER_DBF |
                    EQOS_MAC_PACKET_FILTER_PM |
                    EQOS_MAC_PACKET_FILTER_HMC |
                    EQOS_MAC_PACKET_FILTER_PR);

  if (ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS) {
    PacketFilter |= EQOS_MAC_PACKET_FILTER_PR |
                    EQOS_MAC_PACKET_FILTER_PCF_ALL;
  } else if (ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST) {
    PacketFilter |= EQOS_MAC_PACKET_FILTER_PM;
  } else if (ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST) {
    PacketFilter |= EQOS_MAC_PACKET_FILTER_HMC;

    for (Index = 0; Index < MCastFilterCnt; Index++) {
      Crc     = EqosEtherCrc32Le (MCastFilter[Index].Addr, NET_ETHER_ADDR_LEN);
      Crc    &= 0x7f;
      Crc     = EqosBitReverse32 (~Crc) >> 26;
      HashReg = Crc >> 5;
      HashBit = 1 << (Crc & 0x1f);

      Hash  = MmioRead32 (Eqos->Base + EQOS_MAC_HASH_TABLE_REG (HashReg));
      Hash |= HashBit;
      MmioWrite32 (Eqos->Base + EQOS_MAC_HASH_TABLE_REG (HashReg), Hash);
    }
  }

  if ((ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST) == 0) {
    PacketFilter |= EQOS_MAC_PACKET_FILTER_DBF;
  }

  MmioWrite32 (Eqos->Base + EQOS_MAC_PACKET_FILTER, PacketFilter);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EqosInit (
  IN EQOS_DEVICE  *Eqos
  )
{
  EFI_STATUS  Status;

  ASSERT (Eqos != NULL);
  Eqos->Config = &K3EqosConfig;

  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&Eqos->Cpu);
  ASSERT_EFI_ERROR (Status);

  Status = EqosAllocateDescAndBuffer (Eqos);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Eqos->Config->PlatOps && Eqos->Config->PlatOps->PlatInit) {
    Status = Eqos->Config->PlatOps->PlatInit (Eqos);
    if (EFI_ERROR (Status)) {
      goto ErrReleaseDescAndBuffer;
    }
  }

  if (Eqos->Config->PlatOps && Eqos->Config->PlatOps->EnableClocks) {
    Status = Eqos->Config->PlatOps->EnableClocks (Eqos);
    if (EFI_ERROR (Status)) {
      goto ErrReleasePlat;
    }
  }

  ASSERT (Eqos->Mdio == NULL);
  Status = MdioAlloc (&(Eqos->Mdio));
  if (EFI_ERROR (Status)) {
    goto ErrStopClks;
  }

  Eqos->Mdio->Read  = EqosMdioRead;
  Eqos->Mdio->Write = EqosMdioWrite;
  Eqos->Mdio->Priv  = (VOID *)Eqos;

  return EFI_SUCCESS;

ErrStopClks:
  if (Eqos->Config->PlatOps && Eqos->Config->PlatOps->DisableClocks) {
    Eqos->Config->PlatOps->DisableClocks (Eqos);
  }

ErrReleasePlat:
  if (Eqos->Config->PlatOps && Eqos->Config->PlatOps->PlatDeinit) {
    Eqos->Config->PlatOps->PlatDeinit (Eqos);
  }

ErrReleaseDescAndBuffer:
  EqosFreeDescAndBuffer (Eqos);

  return Status;
}

EFI_STATUS
EFIAPI
EqosDeinit (
  IN EQOS_DEVICE  *Eqos
  )
{
  EFI_STATUS  Status;

  ASSERT (Eqos != NULL);
  ASSERT (Eqos->Config != NULL);

  if (Eqos->Mdio != NULL) {
    Status = MdioFree (Eqos->Mdio);
    ASSERT_EFI_ERROR (Status);
    Eqos->Mdio = NULL;
  }

  if (Eqos->Config->PlatOps && Eqos->Config->PlatOps->DisableClocks) {
    Status = Eqos->Config->PlatOps->DisableClocks (Eqos);
    ASSERT_EFI_ERROR (Status);
  }

  if (Eqos->Config->PlatOps && Eqos->Config->PlatOps->PlatDeinit) {
    Status = Eqos->Config->PlatOps->PlatDeinit (Eqos);
    ASSERT_EFI_ERROR (Status);
  }

  Status = EqosFreeDescAndBuffer (Eqos);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
