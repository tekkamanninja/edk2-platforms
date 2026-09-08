/** @file
  General PHY driver - EDK2 port

  Copyright (c) 2025, Spacemit Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include "GenphyDxeUtil.h"

STATIC CONST CHAR8 *CONST PhyInterfaceStrings[] = {
  [PHY_INTERFACE_MODE_NA]        = "",
  [PHY_INTERFACE_MODE_MII]       = "mii",
  [PHY_INTERFACE_MODE_GMII]      = "gmii",
  [PHY_INTERFACE_MODE_SGMII]     = "sgmii",
  [PHY_INTERFACE_MODE_SGMII_2500] = "sgmii-2500",
  [PHY_INTERFACE_MODE_QSGMII]    = "qsgmii",
  [PHY_INTERFACE_MODE_TBI]       = "tbi",
  [PHY_INTERFACE_MODE_RMII]      = "rmii",
  [PHY_INTERFACE_MODE_RGMII]     = "rgmii",
  [PHY_INTERFACE_MODE_RGMII_ID]  = "rgmii-id",
  [PHY_INTERFACE_MODE_RGMII_RXID] = "rgmii-rxid",
  [PHY_INTERFACE_MODE_RGMII_TXID] = "rgmii-txid",
  [PHY_INTERFACE_MODE_RTBI]      = "rtbi",
  [PHY_INTERFACE_MODE_1000BASEX] = "1000base-x",
  [PHY_INTERFACE_MODE_2500BASEX] = "2500base-x",
  [PHY_INTERFACE_MODE_XGMII]     = "xgmii",
  [PHY_INTERFACE_MODE_XAUI]      = "xaui",
  [PHY_INTERFACE_MODE_RXAUI]     = "rxaui",
  [PHY_INTERFACE_MODE_SFI]       = "sfi",
  [PHY_INTERFACE_MODE_INTERNAL]  = "internal",
  [PHY_INTERFACE_MODE_25G_AUI]   = "25g-aui",
  [PHY_INTERFACE_MODE_XLAUI]     = "xlaui4",
  [PHY_INTERFACE_MODE_CAUI2]     = "caui2",
  [PHY_INTERFACE_MODE_CAUI4]     = "caui4",
  [PHY_INTERFACE_MODE_NCSI]      = "NC-SI",
  [PHY_INTERFACE_MODE_10GBASER]  = "10gbase-r",
  [PHY_INTERFACE_MODE_USXGMII]   = "usxgmii",
};

EFI_STATUS
EFIAPI
PhyGetInterfaceByName (
  IN  CONST CHAR8    *PhyModeStr,
  OUT PHY_INTERFACE  *Interface
  )
{
  UINTN  Index;

  if (Interface == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Interface = PHY_INTERFACE_MODE_NA;

  if ((PhyModeStr == NULL) || (PhyModeStr[0] == '\0')) {
    return EFI_NOT_FOUND;
  }

  for (Index = 0; Index < PHY_INTERFACE_MODE_MAX; Index++) {
    if (AsciiStrCmp (PhyModeStr, PhyInterfaceStrings[Index]) == 0) {
      *Interface = (PHY_INTERFACE)Index;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
MdioAlloc (
  OUT MII_DEV **Mdio
  )
{
  MII_DEV *MiiDevice;

  MiiDevice = AllocateZeroPool (sizeof (MII_DEV));
  if (MiiDevice == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  MiiDevice->Priv = NULL;
  MiiDevice->Read = NULL;
  MiiDevice->Write = NULL;
  MiiDevice->Reset = NULL;

  *Mdio = MiiDevice;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MdioFree (
  IN MII_DEV *Mdio
  )
{
  if (Mdio != NULL) {
    FreePool (Mdio);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyRead (
  IN PHY_DEVICE *PhyDev,
  IN INT32 Devad,
  IN INT32 Regnum,
  OUT INT32 *Value
  )
{
  MII_DEV *Bus = PhyDev->Bus;

  if (Bus == NULL || Bus->Read == NULL) {
    DEBUG ((DEBUG_ERROR, "%s: No bus configured\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  return Bus->Read (Bus, PhyDev->Addr, Devad, Regnum, Value);
}

EFI_STATUS
EFIAPI
PhyWrite (
  IN PHY_DEVICE *PhyDev,
  IN INT32 Devad,
  IN INT32 Regnum,
  IN UINT16 Value
  )
{
  MII_DEV *Bus = PhyDev->Bus;

  if (Bus == NULL || Bus->Write == NULL) {
    DEBUG ((DEBUG_ERROR, "%s: No bus configured\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  return Bus->Write (Bus, PhyDev->Addr, Devad, Regnum, Value);
}

EFI_STATUS
EFIAPI
PhyModify (
  IN PHY_DEVICE *PhyDev,
  IN INT32 Devad,
  IN INT32 Regnum,
  IN UINT16 Mask,
  IN UINT16 Set
  )
{
  EFI_STATUS Status;
  INT32 Val, Old, New;

  Status = PhyRead (PhyDev, Devad, Regnum, &Val);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Old = (UINT16) Val;

  New = (Old & ~Mask) | (Set & Mask);

  // If no change, return EFI_SUCCESS
  if (New == Old) {
    return EFI_SUCCESS;
  }

  return PhyWrite (PhyDev, Devad, Regnum, New);
}

EFI_STATUS
EFIAPI
PhyDumpRegisters (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;
  INT32 Val;
  INT32 Reg;

  if (PhyDev == NULL) {
    DEBUG ((DEBUG_ERROR, "%s: invalid argument\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "Dump PHY registers (addr=%d):\n", PhyDev->Addr));

  for (Reg = 0; Reg < 32; Reg++) {
    Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, Reg, &Val);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_INFO, "  Reg %02d: <read error %r>\n", Reg, Status));
    } else {
      DEBUG ((DEBUG_INFO, "  Reg %02d: 0x%04x\n", Reg, (UINT16) Val));
    }
  }

  return EFI_SUCCESS;
}

/* Generic PHY support and helper functions */

/**
 * genphy_config_advert - sanitize and advertise auto-negotiation parameters
 * @phydev: target phy_device struct
 *
 * Description: Writes MII_ADVERTISE with the appropriate values,
 *   after sanitizing the values to make sure we only advertise
 *   what is supported.   Returns < 0 on error, 0 if the PHY's advertisement
 *   hasn't changed, and > 0 if it has changed.
 */
INT32
EFIAPI
GenphyConfigAdvert (
  IN PHY_DEVICE *PhyDev
  )
{
  INT32 Advertise;
  INT32 OldAdv, Adv, Bmsr;
  INT32 Status, Changed = 0;

  PhyDev->Advertising &= PhyDev->Supported;
  Advertise = PhyDev->Advertising;

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_ADVERTISE, &Adv);
  if (EFI_ERROR(Status)) {
    return -1;
  }
  OldAdv = Adv;

  Adv &= ~(ADVERTISE_ALL | ADVERTISE_100BASE4 | ADVERTISE_PAUSE_CAP |
           ADVERTISE_PAUSE_ASYM);
  if (Advertise & ADVERTISED_10baseT_Half)
    Adv |= ADVERTISE_10HALF;
  if (Advertise & ADVERTISED_10baseT_Full)
    Adv |= ADVERTISE_10FULL;
  if (Advertise & ADVERTISED_100baseT_Half)
    Adv |= ADVERTISE_100HALF;
  if (Advertise & ADVERTISED_100baseT_Full)
    Adv |= ADVERTISE_100FULL;
  if (Advertise & ADVERTISED_Pause)
    Adv |= ADVERTISE_PAUSE_CAP;
  if (Advertise & ADVERTISED_Asym_Pause)
    Adv |= ADVERTISE_PAUSE_ASYM;
  if (Advertise & ADVERTISED_1000baseX_Half)
    Adv |= ADVERTISE_1000XHALF;
  if (Advertise & ADVERTISED_1000baseX_Full)
    Adv |= ADVERTISE_1000XFULL;

  if (Adv != OldAdv) {
    Status = PhyWrite (PhyDev, MDIO_DEVAD_NONE, MII_ADVERTISE, Adv);
    if (EFI_ERROR(Status)) {
      return -1;
    }
    Changed = 1;
  }

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMSR, &Bmsr);
  if (EFI_ERROR(Status)) {
    return -1;
  }

  if (!(Bmsr & BMSR_ESTATEN))
    return Changed;

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_CTRL1000, &Adv);
  if (EFI_ERROR(Status)) {
    return -1;
  }
  OldAdv = Adv;

  Adv &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);

  if (PhyDev->Supported & (SUPPORTED_1000baseT_Half | SUPPORTED_1000baseT_Full)) {
    if (Advertise & SUPPORTED_1000baseT_Half)
      Adv |= ADVERTISE_1000HALF;
    if (Advertise & SUPPORTED_1000baseT_Full)
      Adv |= ADVERTISE_1000FULL;
  }

  if (Adv != OldAdv)
    Changed = 1;

  Status = PhyWrite (PhyDev, MDIO_DEVAD_NONE, MII_CTRL1000, Adv);
  if (EFI_ERROR(Status)) {
    return -1;
  }

  return Changed;
}

/**
 * genphy_setup_forced - configures/forces speed/duplex from @phydev
 * @phydev: target phy_device struct
 *
 * Description: Configures MII_BMCR to force speed/duplex
 *   to the values in phydev. Assumes that the values are valid.
 */
EFI_STATUS
EFIAPI
GenphySetupForced (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;
  INT32 Ctl = BMCR_ANRESTART;

  PhyDev->Pause = 0;
  PhyDev->AsymPause = 0;

  if (PhyDev->Speed == SPEED_1000)
    Ctl |= BMCR_SPEED1000;
  else if (PhyDev->Speed == SPEED_100)
    Ctl |= BMCR_SPEED100;

  if (PhyDev->Duplex == DUPLEX_FULL)
    Ctl |= BMCR_FULLDPLX;

  Status = PhyWrite (PhyDev, MDIO_DEVAD_NONE, MII_BMCR, Ctl);

  return Status;
}

EFI_STATUS
EFIAPI
GenphyRestartAneg (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;
  INT32 Ctl;

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMCR, &Ctl);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Ctl |= (BMCR_ANENABLE | BMCR_ANRESTART);

  Ctl &= ~(BMCR_ISOLATE);

  Status = PhyWrite (PhyDev, MDIO_DEVAD_NONE, MII_BMCR, Ctl);

  return Status;
}

EFI_STATUS
EFIAPI
GenphyConfigAneg (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;
  INT32 Changed;

  if (PhyDev->Autoneg != AUTONEG_ENABLE)
    return GenphySetupForced(PhyDev);

  Changed = GenphyConfigAdvert(PhyDev);

  if (Changed < 0) /* error */
    return EFI_DEVICE_ERROR;

  if (Changed == 0) {
    INT32 Ctl;
    Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMCR, &Ctl);

    if (EFI_ERROR(Status))
      return Status;

    if (!(Ctl & BMCR_ANENABLE) || (Ctl & BMCR_ISOLATE))
      Changed = 1;
  }

  /*
   * Only restart aneg if we are advertising something different
   * than we were before.
   */
  if (Changed > 0) {
    Status = GenphyRestartAneg(PhyDev);
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 * genphy_update_link - update link status in @phydev
 * @phydev: target phy_device struct
 *
 * Description: Update the value in phydev->link to reflect the
 *   current link value.  In order to do this, we need to read
 *   the status register twice, keeping the second value.
 */
EFI_STATUS
EFIAPI
GenphyUpdateLink (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;
  INT32 MiiReg;

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMSR, &MiiReg);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (PhyDev->Link && (MiiReg & BMSR_LSTATUS))
    return EFI_SUCCESS;

  if ((PhyDev->Autoneg == AUTONEG_ENABLE) && !(MiiReg & BMSR_ANEGCOMPLETE)) {
    INT32 i = 0;

    DEBUG ((DEBUG_INFO, "Waiting for PHY auto negotiation to complete"));

    while (!(MiiReg & BMSR_ANEGCOMPLETE)) {
      if (i > (PHY_ANEG_TIMEOUT / 50)) {
        DEBUG ((DEBUG_ERROR, " TIMEOUT !\n"));
        PhyDev->Link = 0;
        return EFI_TIMEOUT;
      }

      if ((i++ % 10) == 0)
        DEBUG ((DEBUG_INFO, "."));

      Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMSR, &MiiReg);
      if (EFI_ERROR(Status)) {
        return Status;
      }

      gBS->Stall(50000);  // 50 ms = 50000 microseconds
    }

    DEBUG ((DEBUG_INFO, " done\n"));
    PhyDev->Link = 1;
  } else {
    Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMSR, &MiiReg);
    if (EFI_ERROR(Status)) {
      return Status;
    }

    PhyDev->Link = !!(MiiReg & BMSR_LSTATUS);
  }

  return EFI_SUCCESS;
}

/*
 * Generic function which updates the speed and duplex.   If
 * autonegotiation is enabled, it uses the AND of the link
 * partner's advertised capabilities and our advertised
 * capabilities.  If autonegotiation is disabled, we use the
 * appropriate bits in the control register.
 *
 * Stolen from Linux's mii.c and phy_device.c
 */
EFI_STATUS
EFIAPI
GenphyParseLink (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;
  INT32 MiiReg;
  INT32 Lpa = 0;
  INT32 Adv = 0;
  INT32 Gblpa = 0;
  INT32 Estatus = 0;

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMSR, &MiiReg);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (PhyDev->Autoneg == AUTONEG_ENABLE) {
    if (PhyDev->Supported & (SUPPORTED_1000baseT_Full | SUPPORTED_1000baseT_Half)) {
      Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_STAT1000, &Gblpa);
      if (EFI_ERROR(Status)) {
        DEBUG ((DEBUG_ERROR, "Could not read MII_STAT1000. Ignoring gigabit capability\n"));
        Gblpa = 0;
      }

      INT32 Ctrl1000;
      Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_CTRL1000, &Ctrl1000);
      if (EFI_ERROR(Status)) {
        return Status;
      }

      Gblpa &= Ctrl1000 << 2;
    }

    PhyDev->Speed = SPEED_10;
    PhyDev->Duplex = DUPLEX_HALF;

    if (Gblpa & (PHY_1000BTSR_1000FD | PHY_1000BTSR_1000HD)) {
      PhyDev->Speed = SPEED_1000;

      if (Gblpa & PHY_1000BTSR_1000FD)
        PhyDev->Duplex = DUPLEX_FULL;

      return EFI_SUCCESS;
    }

    Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_ADVERTISE, &Adv);
    if (EFI_ERROR(Status)) {
      return Status;
    }

    Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_LPA, &Lpa);
    if (EFI_ERROR(Status)) {
      return Status;
    }

    Lpa &= Adv;

    if (Lpa & (LPA_100FULL | LPA_100HALF)) {
      PhyDev->Speed = SPEED_100;

      if (Lpa & LPA_100FULL)
        PhyDev->Duplex = DUPLEX_FULL;

    } else if (Lpa & LPA_10FULL) {
      PhyDev->Duplex = DUPLEX_FULL;
    }

    if ((MiiReg & BMSR_ESTATEN) && !(MiiReg & BMSR_ERCAP)) {
      Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_ESTATUS, &Estatus);
      if (EFI_ERROR(Status)) {
        return Status;
      }
    }

    if (Estatus & (ESTATUS_1000_XFULL | ESTATUS_1000_XHALF | ESTATUS_1000_TFULL | ESTATUS_1000_THALF)) {
      PhyDev->Speed = SPEED_1000;
      if (Estatus & (ESTATUS_1000_XFULL | ESTATUS_1000_TFULL))
        PhyDev->Duplex = DUPLEX_FULL;
    }

  } else {
    INT32 Bmcr;
    Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMCR, &Bmcr);
    if (EFI_ERROR(Status)) {
      return Status;
    }

    PhyDev->Speed = SPEED_10;
    PhyDev->Duplex = DUPLEX_HALF;

    if (Bmcr & BMCR_FULLDPLX)
      PhyDev->Duplex = DUPLEX_FULL;

    if (Bmcr & BMCR_SPEED1000)
      PhyDev->Speed = SPEED_1000;
    else if (Bmcr & BMCR_SPEED100)
      PhyDev->Speed = SPEED_100;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GenphyReadId (
  IN PHY_DEVICE *PhyDev,
  OUT UINT32 *PhyId
  )
{
  EFI_STATUS Status;
  INT32 Id1, Id2;

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_PHYSID1, &Id1);
  if (EFI_ERROR(Status)) {
    return EFI_UNSUPPORTED;
  }

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_PHYSID2, &Id2);
  if (EFI_ERROR(Status)) {
    return EFI_UNSUPPORTED;
  }

  *PhyId = ((UINT32)Id1 << 16) | (Id2 & 0xffff);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GenphyDeviceCreate (
  IN MII_DEV *Bus,
  IN INT32 Addr,
  IN UINT32 PhyId,
  OUT PHY_DEVICE **PhyDev
  )
{
  PHY_DEVICE *NewPhyDev;

  NewPhyDev = AllocateZeroPool(sizeof(PHY_DEVICE));
  if (NewPhyDev == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  NewPhyDev->Bus = Bus;
  NewPhyDev->Addr = Addr;
  NewPhyDev->PhyId = PhyId;
  NewPhyDev->Autoneg = 1;
  NewPhyDev->Supported = 0xFFFFFFFF;
  NewPhyDev->Advertising = 0xFFFFFFFF;
  NewPhyDev->Link = 0;
  NewPhyDev->Speed = 0;
  NewPhyDev->Duplex = -1;
  NewPhyDev->Iface = PHY_INTERFACE_MODE_NA;
  NewPhyDev->Flags = 0;

  *PhyDev = NewPhyDev;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyReset (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;
  INT32 Reg;
  INT32 Timeout = 100;
  INT32 Devad = MDIO_DEVAD_NONE;

  if (PhyDev->Flags & PHY_FLAG_BROKEN_RESET)
    return EFI_SUCCESS;

  Status = PhyWrite (PhyDev, Devad, MII_BMCR, BMCR_RESET);
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "PHY reset failed\n"));
    return Status;
  }
  /*
   * Poll the control register for the reset bit to go to 0 (it is
   * auto-clearing).  This should happen within 0.5 seconds per the
   * IEEE spec.
   */
  Status = PhyRead (PhyDev, Devad, MII_BMCR, &Reg);
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "PHY status read failed\n"));
    return Status;
  }

  while ((Reg & BMCR_RESET) && Timeout--) {
    Status = PhyRead (PhyDev, Devad, MII_BMCR, &Reg);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_ERROR, "PHY status read failed\n"));
      return Status;
    }
    gBS->Stall (5000);
  }

  if (Reg & BMCR_RESET) {
    DEBUG ((DEBUG_ERROR, "PHY reset timed out\n"));
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

#define PHY_MAX_ADDR 32

EFI_STATUS
EFIAPI
PhyConnect (
  IN  MII_DEV        *Bus,
  IN  INT32           Addr,
  OUT PHY_DEVICE    **PhyDev,
  IN  VOID           *Dev,
  IN  PHY_INTERFACE   Iface
  )
{
  EFI_STATUS Status;
  UINT32 PhyId = 0;
  INT32 ProbeAddr;
  PHY_DEVICE *NewPhyDev;

  if (Bus == NULL || Bus->Read == NULL || Bus->Write == NULL || PhyDev == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Addr < 0) {
    for (ProbeAddr = 0; ProbeAddr < PHY_MAX_ADDR; ProbeAddr++) {
      PHY_DEVICE Temp;
      Temp.Bus  = Bus;
      Temp.Addr = ProbeAddr;
      Status = GenphyReadId(&Temp, &PhyId);
      if (!EFI_ERROR(Status) && ((PhyId & PHY_ID_MASK) != PHY_ID_MASK)) {
        Addr = ProbeAddr;
        break;
      }
    }
    if (Addr < 0) {
      return EFI_NOT_FOUND;
    }
  } else {
    PHY_DEVICE Temp;
    Temp.Bus  = Bus;
    Temp.Addr = Addr;
    Status = GenphyReadId(&Temp, &PhyId);
    if (EFI_ERROR(Status) || ((PhyId & PHY_ID_MASK) == PHY_ID_MASK)) {
      return EFI_NOT_FOUND;
    }
  }

  Status = GenphyDeviceCreate(Bus, Addr, PhyId, &NewPhyDev);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  NewPhyDev->Iface = Iface;
  NewPhyDev->Priv = Dev;

  // Best-effort soft reset: proceed even if it fails. Some PHYs remain usable.
  PhyReset(NewPhyDev);

  *PhyDev = NewPhyDev;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhySetSupported (
  IN PHY_DEVICE *PhyDev,
  IN UINT32 MaxSpeed
  )
{
  /* The default values for phydev->supported are provided by the PHY
   * driver "features" member, we want to reset to sane defaults first
   * before supporting higher speeds.
   */
  PhyDev->Supported &= PHY_DEFAULT_FEATURES;

  switch (MaxSpeed) {
  default:
    return EFI_INVALID_PARAMETER;
  case SPEED_1000:
    PhyDev->Supported |= PHY_1000BT_FEATURES;
  // fall through
  case SPEED_100:
    PhyDev->Supported |= PHY_100BT_FEATURES;
  // fall through
  case SPEED_10:
    PhyDev->Supported |= PHY_10BT_FEATURES;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyConfig (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;
  INT32 Val;
  UINT32 Features = (SUPPORTED_TP | SUPPORTED_MII |
                     SUPPORTED_AUI | SUPPORTED_FIBRE |
                     SUPPORTED_BNC);

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMSR, &Val);
  if (EFI_ERROR(Status))
    return Status;

  if (Val & BMSR_ANEGCAPABLE)
    Features |= SUPPORTED_Autoneg;

  if (Val & BMSR_100FULL)
    Features |= SUPPORTED_100baseT_Full;
  if (Val & BMSR_100HALF)
    Features |= SUPPORTED_100baseT_Half;
  if (Val & BMSR_10FULL)
    Features |= SUPPORTED_10baseT_Full;
  if (Val & BMSR_10HALF)
    Features |= SUPPORTED_10baseT_Half;

  if (Val & BMSR_ESTATEN) {
    Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_ESTATUS, &Val);
    if (EFI_ERROR(Status))
      return Status;

    if (Val & ESTATUS_1000_TFULL)
      Features |= SUPPORTED_1000baseT_Full;
    if (Val & ESTATUS_1000_THALF)
      Features |= SUPPORTED_1000baseT_Half;
    if (Val & ESTATUS_1000_XFULL)
      Features |= SUPPORTED_1000baseX_Full;
    if (Val & ESTATUS_1000_XHALF)
      Features |= SUPPORTED_1000baseX_Half;
  }

  PhyDev->Supported &= Features;
  PhyDev->Advertising &= Features;

  Status = GenphyConfigAneg(PhyDev);
  if (EFI_ERROR(Status))
    return Status;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyUpdateLink (
  IN PHY_DEVICE  *PhyDev
  )
{
  EFI_STATUS  Status;
  INT32       Bmsr1;
  INT32       Bmsr2;
  INT32       OldLink;
  INT32       NewLink;
  BOOLEAN     LinkDownLatched;

  if (PhyDev == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OldLink = PhyDev->Link;

  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMSR, &Bmsr1);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // BMSR link status is latch-low. Read it twice to get the current link state.
  //
  Status = PhyRead (PhyDev, MDIO_DEVAD_NONE, MII_BMSR, &Bmsr2);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  LinkDownLatched = ((Bmsr1 & BMSR_LSTATUS) == 0);
  NewLink         = (Bmsr2 & BMSR_LSTATUS) ? 1 : 0;

  PhyDev->Link = NewLink;

  if (NewLink == 0) {
    return EFI_SUCCESS;
  }

  //
  // Parse link parameters when link comes up, or when a latched link-down
  // event indicates that auto-negotiation may have run again.
  //
  if ((OldLink == 0) || LinkDownLatched) {
    Status = GenphyParseLink (PhyDev);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyStartup (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;

  Status = GenphyUpdateLink(PhyDev);
  if (EFI_ERROR(Status))
    return Status;

  Status = GenphyParseLink(PhyDev);
  if (EFI_ERROR(Status))
    return Status;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PhyShutdown (
  IN PHY_DEVICE *PhyDev
  )
{
  EFI_STATUS Status;

  if (PhyDev == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = PhyWrite (PhyDev, MDIO_DEVAD_NONE, MII_BMCR, BMCR_PDOWN);
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to put PHY into power-down mode\n"));
  }

  FreePool(PhyDev);

  return EFI_SUCCESS;
}
