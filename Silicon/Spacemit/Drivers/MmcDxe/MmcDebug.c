/** @file
*
*  Copyright (c) 2011-2013, ARM Limited. All rights reserved.
*  Copyright (c) 2025, Spacemit Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "Mmc.h"

VOID
PrintSdCid (
  IN SD_CID  *Cid
  )
{
  DEBUG ((DEBUG_INFO, "== Dump Sd Cid Register==\n"));
  DEBUG ((DEBUG_INFO, "\t- Manufacturing date: %d/%d\n", Cid->ManufacturingDate & 0xF, (Cid->ManufacturingDate >> 4) & 0xFF));
  DEBUG ((DEBUG_INFO, "\t- Product serial number: 0x%X\n", ReadUnaligned32 ((UINT32 *)Cid->ProductSerialNumber)));
  DEBUG ((DEBUG_INFO, "\t- Product revision: %d\n", Cid->ProductRevision));
  DEBUG ((DEBUG_INFO, "\t- Product name: %a\n", (char *)Cid->ProductName));
  DEBUG ((DEBUG_INFO, "\t- OEM ID: 0x%x 0x%x, Manufacturer Id: 0x%x\n", Cid->OemId[0], Cid->OemId[1], Cid->ManufacturerId));
}

/**
  Decode and print SD CSD Register content.

  @param[in] Csd           Pointer to SD_CSD data structure.

**/
VOID
PrintSdCsd (
  IN SD_CSD  *Csd
  )
{
  SD_CSD2  *Csd2;

  DEBUG ((DEBUG_INFO, "== Dump Sd Csd Register==\n"));
  DEBUG ((DEBUG_INFO, "  CSD structure                    0x%x\n", Csd->CsdStructure));
  DEBUG ((DEBUG_INFO, "  Data read access-time 1          0x%x\n", Csd->Taac));
  DEBUG ((DEBUG_INFO, "  Data read access-time 2          0x%x\n", Csd->Nsac));
  DEBUG ((DEBUG_INFO, "  Max. bus clock frequency         0x%x\n", Csd->TranSpeed));
  DEBUG ((DEBUG_INFO, "  Device command classes           0x%x\n", Csd->Ccc));
  DEBUG ((DEBUG_INFO, "  Max. read data block length      0x%x\n", Csd->ReadBlLen));
  DEBUG ((DEBUG_INFO, "  Partial blocks for read allowed  0x%x\n", Csd->ReadBlPartial));
  DEBUG ((DEBUG_INFO, "  Write block misalignment         0x%x\n", Csd->WriteBlkMisalign));
  DEBUG ((DEBUG_INFO, "  Read block misalignment          0x%x\n", Csd->ReadBlkMisalign));
  DEBUG ((DEBUG_INFO, "  DSR implemented                  0x%x\n", Csd->DsrImp));
  if (Csd->CsdStructure == 0) {
    DEBUG ((DEBUG_INFO, "  Device size                      0x%x\n", Csd->CSizeLow | (Csd->CSizeHigh << 2)));
    DEBUG ((DEBUG_INFO, "  Max. read current @ VDD min      0x%x\n", Csd->VddRCurrMin));
    DEBUG ((DEBUG_INFO, "  Max. read current @ VDD max      0x%x\n", Csd->VddRCurrMax));
    DEBUG ((DEBUG_INFO, "  Max. write current @ VDD min     0x%x\n", Csd->VddWCurrMin));
    DEBUG ((DEBUG_INFO, "  Max. write current @ VDD max     0x%x\n", Csd->VddWCurrMax));
  } else {
    Csd2 = (SD_CSD2 *)(VOID *)Csd;
    DEBUG ((DEBUG_INFO, "  Device size                      0x%x\n", Csd2->CSizeLow | (Csd->CSizeHigh << 16)));
  }

  DEBUG ((DEBUG_INFO, "  Erase sector size                0x%x\n", Csd->SectorSize));
  DEBUG ((DEBUG_INFO, "  Erase single block enable        0x%x\n", Csd->EraseBlkEn));
  DEBUG ((DEBUG_INFO, "  Write protect group size         0x%x\n", Csd->WpGrpSize));
  DEBUG ((DEBUG_INFO, "  Write protect group enable       0x%x\n", Csd->WpGrpEnable));
  DEBUG ((DEBUG_INFO, "  Write speed factor               0x%x\n", Csd->R2WFactor));
  DEBUG ((DEBUG_INFO, "  Max. write data block length     0x%x\n", Csd->WriteBlLen));
  DEBUG ((DEBUG_INFO, "  Partial blocks for write allowed 0x%x\n", Csd->WriteBlPartial));
  DEBUG ((DEBUG_INFO, "  File format group                0x%x\n", Csd->FileFormatGrp));
  DEBUG ((DEBUG_INFO, "  Copy flag (OTP)                  0x%x\n", Csd->Copy));
  DEBUG ((DEBUG_INFO, "  Permanent write protection       0x%x\n", Csd->PermWriteProtect));
  DEBUG ((DEBUG_INFO, "  Temporary write protection       0x%x\n", Csd->TmpWriteProtect));
  DEBUG ((DEBUG_INFO, "  File format                      0x%x\n", Csd->FileFormat));
}

VOID
PrintRCA (
  IN UINT32  Rca
  )
{
  DEBUG ((DEBUG_INFO, "- PrintRCA: 0x%X\n", Rca));
  DEBUG ((DEBUG_INFO, "\t- Status: 0x%X\n", Rca & 0xFFFF));
  DEBUG ((DEBUG_INFO, "\t- RCA: 0x%X\n", (Rca >> 16) & 0xFFFF));
}

VOID
PrintOCR (
  IN UINT32  Ocr
  )
{
  UINTN  MinV;
  UINTN  MaxV;
  UINTN  Volts;
  UINTN  Loop;

  MinV  = 36;  // 3.6
  MaxV  = 20;  // 2.0
  Volts = 20;  // 2.0

  // The MMC register bits [23:8] indicate the working range of the card
  for (Loop = 8; Loop < 24; Loop++) {
    if (Ocr & (1 << Loop)) {
      if (MinV > Volts) {
        MinV = Volts;
      }

      if (MaxV < Volts) {
        MaxV = Volts + 1;
      }
    }

    Volts++;
  }

  DEBUG ((DEBUG_INFO, "- PrintOCR Ocr (0x%X)\n", Ocr));
  DEBUG ((DEBUG_INFO, "\t- Card operating voltage: %d.%d to %d.%d\n", MinV/10, MinV % 10, MaxV/10, MaxV % 10));
  if (((Ocr >> 29) & 3) == 0) {
    DEBUG ((DEBUG_INFO, "\t- AccessMode: Byte Mode\n"));
  } else {
    DEBUG ((DEBUG_INFO, "\t- AccessMode: Block Mode (0x%X)\n", ((Ocr >> 29) & 3)));
  }

  if (Ocr & MMC_OCR_POWERUP) {
    DEBUG ((DEBUG_INFO, "\t- PowerUp\n"));
  } else {
    DEBUG ((DEBUG_INFO, "\t- Voltage Not Supported\n"));
  }
}

VOID
PrintResponseR1 (
  IN  UINT32  Response
  )
{
  DEBUG ((DEBUG_INFO, "Response: 0x%X\n", Response));
  if (Response & MMC_R0_READY_FOR_DATA) {
    DEBUG ((DEBUG_INFO, "\t- READY_FOR_DATA\n"));
  }

  switch ((Response >> 9) & 0xF) {
    case 0:
      DEBUG ((DEBUG_INFO, "\t- State: Idle\n"));
      break;
    case 1:
      DEBUG ((DEBUG_INFO, "\t- State: Ready\n"));
      break;
    case 2:
      DEBUG ((DEBUG_INFO, "\t- State: Ident\n"));
      break;
    case 3:
      DEBUG ((DEBUG_INFO, "\t- State: StandBy\n"));
      break;
    case 4:
      DEBUG ((DEBUG_INFO, "\t- State: Tran\n"));
      break;
    case 5:
      DEBUG ((DEBUG_INFO, "\t- State: Data\n"));
      break;
    case 6:
      DEBUG ((DEBUG_INFO, "\t- State: Rcv\n"));
      break;
    case 7:
      DEBUG ((DEBUG_INFO, "\t- State: Prg\n"));
      break;
    case 8:
      DEBUG ((DEBUG_INFO, "\t- State: Dis\n"));
      break;
    default:
      DEBUG ((DEBUG_INFO, "\t- State: Reserved\n"));
      break;
  }
}

VOID
PrintEmmcCid (
  IN EMMC_CID  *Cid
  )
{
  DEBUG ((DEBUG_INFO, "== Dump Emmc CID Register==\n"));
  DEBUG ((DEBUG_INFO, "\t- Manufacturing date: %d/%d\n", Cid->ManufacturingDate & 0xF, (Cid->ManufacturingDate >> 4) & 0x0F));
  DEBUG ((DEBUG_INFO, "\t- Product serial number: 0x%X\n", ReadUnaligned32 ((UINT32 *)Cid->ProductSerialNumber)));
  DEBUG ((DEBUG_INFO, "\t- Product revision: %d\n", Cid->ProductRevision));
  DEBUG ((DEBUG_INFO, "\t- Product name: %a\n", (char *)Cid->ProductName));
  DEBUG ((DEBUG_INFO, "\t- OEM ID: 0x%x, Manufacturer Id: 0x%x\n", Cid->OemId, Cid->ManufacturerId));
}

/**
  Decode and print EMMC CSD Register content.

  @param[in] Csd           Pointer to EMMC_CSD data structure.

**/
VOID
PrintEmmcCsd (
  IN EMMC_CSD  *Csd
  )
{
  DEBUG ((DEBUG_INFO, "== Dump Emmc Csd Register==\n"));
  DEBUG ((DEBUG_INFO, "  CSD structure                    0x%x\n", Csd->CsdStructure));
  DEBUG ((DEBUG_INFO, "  System specification version     0x%x\n", Csd->SpecVers));
  DEBUG ((DEBUG_INFO, "  Data read access-time 1          0x%x\n", Csd->Taac));
  DEBUG ((DEBUG_INFO, "  Data read access-time 2          0x%x\n", Csd->Nsac));
  DEBUG ((DEBUG_INFO, "  Max. bus clock frequency         0x%x\n", Csd->TranSpeed));
  DEBUG ((DEBUG_INFO, "  Device command classes           0x%x\n", Csd->Ccc));
  DEBUG ((DEBUG_INFO, "  Max. read data block length      0x%x\n", Csd->ReadBlLen));
  DEBUG ((DEBUG_INFO, "  Partial blocks for read allowed  0x%x\n", Csd->ReadBlPartial));
  DEBUG ((DEBUG_INFO, "  Write block misalignment         0x%x\n", Csd->WriteBlkMisalign));
  DEBUG ((DEBUG_INFO, "  Read block misalignment          0x%x\n", Csd->ReadBlkMisalign));
  DEBUG ((DEBUG_INFO, "  DSR implemented                  0x%x\n", Csd->DsrImp));
  DEBUG ((DEBUG_INFO, "  Device size                      0x%x\n", Csd->CSizeLow | (Csd->CSizeHigh << 2)));
  DEBUG ((DEBUG_INFO, "  Max. read current @ VDD min      0x%x\n", Csd->VddRCurrMin));
  DEBUG ((DEBUG_INFO, "  Max. read current @ VDD max      0x%x\n", Csd->VddRCurrMax));
  DEBUG ((DEBUG_INFO, "  Max. write current @ VDD min     0x%x\n", Csd->VddWCurrMin));
  DEBUG ((DEBUG_INFO, "  Max. write current @ VDD max     0x%x\n", Csd->VddWCurrMax));
  DEBUG ((DEBUG_INFO, "  Device size multiplier           0x%x\n", Csd->CSizeMult));
  DEBUG ((DEBUG_INFO, "  Erase group size                 0x%x\n", Csd->EraseGrpSize));
  DEBUG ((DEBUG_INFO, "  Erase group size multiplier      0x%x\n", Csd->EraseGrpMult));
  DEBUG ((DEBUG_INFO, "  Write protect group size         0x%x\n", Csd->WpGrpSize));
  DEBUG ((DEBUG_INFO, "  Write protect group enable       0x%x\n", Csd->WpGrpEnable));
  DEBUG ((DEBUG_INFO, "  Manufacturer default ECC         0x%x\n", Csd->DefaultEcc));
  DEBUG ((DEBUG_INFO, "  Write speed factor               0x%x\n", Csd->R2WFactor));
  DEBUG ((DEBUG_INFO, "  Max. write data block length     0x%x\n", Csd->WriteBlLen));
  DEBUG ((DEBUG_INFO, "  Partial blocks for write allowed 0x%x\n", Csd->WriteBlPartial));
  DEBUG ((DEBUG_INFO, "  Content protection application   0x%x\n", Csd->ContentProtApp));
  DEBUG ((DEBUG_INFO, "  File format group                0x%x\n", Csd->FileFormatGrp));
  DEBUG ((DEBUG_INFO, "  Copy flag (OTP)                  0x%x\n", Csd->Copy));
  DEBUG ((DEBUG_INFO, "  Permanent write protection       0x%x\n", Csd->PermWriteProtect));
  DEBUG ((DEBUG_INFO, "  Temporary write protection       0x%x\n", Csd->TmpWriteProtect));
  DEBUG ((DEBUG_INFO, "  File format                      0x%x\n", Csd->FileFormat));
  DEBUG ((DEBUG_INFO, "  ECC code                         0x%x\n", Csd->Ecc));
}

/**
  Decode and print EMMC EXT_CSD Register content.

  @param[in] ExtCsd           Pointer to the EMMC_EXT_CSD data structure.

**/
VOID
PrintEmmcExtCsd (
  IN EMMC_EXT_CSD  *ExtCsd
  )
{
  DEBUG ((DEBUG_INFO, "==Dump Emmc ExtCsd Register==\n"));
  DEBUG ((DEBUG_INFO, "  Supported Command Sets                 0x%x\n", ExtCsd->CmdSet));
  DEBUG ((DEBUG_INFO, "  HPI features                           0x%x\n", ExtCsd->HpiFeatures));
  DEBUG ((DEBUG_INFO, "  Background operations support          0x%x\n", ExtCsd->BkOpsSupport));
  DEBUG ((DEBUG_INFO, "  Background operations status           0x%x\n", ExtCsd->BkopsStatus));
  DEBUG ((DEBUG_INFO, "  Number of correctly programmed sectors 0x%x\n", *((UINT32 *)&ExtCsd->CorrectlyPrgSectorsNum[0])));
  DEBUG ((DEBUG_INFO, "  Initialization time after partitioning 0x%x\n", ExtCsd->IniTimeoutAp));
  DEBUG ((DEBUG_INFO, "  TRIM Multiplier                        0x%x\n", ExtCsd->TrimMult));
  DEBUG ((DEBUG_INFO, "  Secure Feature support                 0x%x\n", ExtCsd->SecFeatureSupport));
  DEBUG ((DEBUG_INFO, "  Secure Erase Multiplier                0x%x\n", ExtCsd->SecEraseMult));
  DEBUG ((DEBUG_INFO, "  Secure TRIM Multiplier                 0x%x\n", ExtCsd->SecTrimMult));
  DEBUG ((DEBUG_INFO, "  Boot information                       0x%x\n", ExtCsd->BootInfo));
  DEBUG ((DEBUG_INFO, "  Boot partition size                    0x%x\n", ExtCsd->BootSizeMult));
  DEBUG ((DEBUG_INFO, "  Access size                            0x%x\n", ExtCsd->AccSize));
  DEBUG ((DEBUG_INFO, "  High-capacity erase unit size          0x%x\n", ExtCsd->HcEraseGrpSize));
  DEBUG ((DEBUG_INFO, "  High-capacity erase timeout            0x%x\n", ExtCsd->EraseTimeoutMult));
  DEBUG ((DEBUG_INFO, "  Reliable write sector count            0x%x\n", ExtCsd->RelWrSecC));
  DEBUG ((DEBUG_INFO, "  High-capacity write protect group size 0x%x\n", ExtCsd->HcWpGrpSize));
  DEBUG ((DEBUG_INFO, "  Sleep/awake timeout                    0x%x\n", ExtCsd->SATimeout));
  DEBUG ((DEBUG_INFO, "  Sector Count                           0x%x\n", *((UINT32 *)&ExtCsd->SecCount[0])));
  DEBUG ((DEBUG_INFO, "  Partition switching timing             0x%x\n", ExtCsd->PartitionSwitchTime));
  DEBUG ((DEBUG_INFO, "  Out-of-interrupt busy timing           0x%x\n", ExtCsd->OutOfInterruptTime));
  DEBUG ((DEBUG_INFO, "  I/O Driver Strength                    0x%x\n", ExtCsd->DriverStrength));
  DEBUG ((DEBUG_INFO, "  Device type                            0x%x\n", ExtCsd->DeviceType));
  DEBUG ((DEBUG_INFO, "  CSD STRUCTURE                          0x%x\n", ExtCsd->CsdStructure));
  DEBUG ((DEBUG_INFO, "  Extended CSD revision                  0x%x\n", ExtCsd->ExtCsdRev));
  DEBUG ((DEBUG_INFO, "  Command set                            0x%x\n", ExtCsd->CmdSet));
  DEBUG ((DEBUG_INFO, "  Command set revision                   0x%x\n", ExtCsd->CmdSetRev));
  DEBUG ((DEBUG_INFO, "  Power class                            0x%x\n", ExtCsd->PowerClass));
  DEBUG ((DEBUG_INFO, "  High-speed interface timing            0x%x\n", ExtCsd->HsTiming));
  DEBUG ((DEBUG_INFO, "  Bus width mode                         0x%x\n", ExtCsd->BusWidth));
  DEBUG ((DEBUG_INFO, "  Erased memory content                  0x%x\n", ExtCsd->ErasedMemCont));
  DEBUG ((DEBUG_INFO, "  Partition configuration                0x%x\n", ExtCsd->PartitionConfig));
  DEBUG ((DEBUG_INFO, "  Boot config protection                 0x%x\n", ExtCsd->BootConfigProt));
  DEBUG ((DEBUG_INFO, "  Boot bus Conditions                    0x%x\n", ExtCsd->BootBusConditions));
  DEBUG ((DEBUG_INFO, "  High-density erase group definition    0x%x\n", ExtCsd->EraseGroupDef));
  DEBUG ((DEBUG_INFO, "  Boot write protection status register  0x%x\n", ExtCsd->BootWpStatus));
  DEBUG ((DEBUG_INFO, "  Boot area write protection register    0x%x\n", ExtCsd->BootWp));
  DEBUG ((DEBUG_INFO, "  User area write protection register    0x%x\n", ExtCsd->UserWp));
  DEBUG ((DEBUG_INFO, "  FW configuration                       0x%x\n", ExtCsd->FwConfig));
  DEBUG ((DEBUG_INFO, "  RPMB Size                              0x%x\n", ExtCsd->RpmbSizeMult));
  DEBUG ((DEBUG_INFO, "  H/W reset function                     0x%x\n", ExtCsd->RstFunction));
  DEBUG ((DEBUG_INFO, "  Partitioning Support                   0x%x\n", ExtCsd->PartitioningSupport));
  DEBUG (
         (
          DEBUG_INFO,
          "  Max Enhanced Area Size                 0x%02x%02x%02x\n", \
          ExtCsd->MaxEnhSizeMult[2],
          ExtCsd->MaxEnhSizeMult[1],
          ExtCsd->MaxEnhSizeMult[0]
         )
         );
  DEBUG ((DEBUG_INFO, "  Partitions attribute                   0x%x\n", ExtCsd->PartitionsAttribute));
  DEBUG ((DEBUG_INFO, "  Partitioning Setting                   0x%x\n", ExtCsd->PartitionSettingCompleted));
  DEBUG (
         (
          DEBUG_INFO,
          "  General Purpose Partition 1 Size       0x%02x%02x%02x\n", \
          ExtCsd->GpSizeMult[2],
          ExtCsd->GpSizeMult[1],
          ExtCsd->GpSizeMult[0]
         )
         );
  DEBUG (
         (
          DEBUG_INFO,
          "  General Purpose Partition 2 Size       0x%02x%02x%02x\n", \
          ExtCsd->GpSizeMult[5],
          ExtCsd->GpSizeMult[4],
          ExtCsd->GpSizeMult[3]
         )
         );
  DEBUG (
         (
          DEBUG_INFO,
          "  General Purpose Partition 3 Size       0x%02x%02x%02x\n", \
          ExtCsd->GpSizeMult[8],
          ExtCsd->GpSizeMult[7],
          ExtCsd->GpSizeMult[6]
         )
         );
  DEBUG (
         (
          DEBUG_INFO,
          "  General Purpose Partition 4 Size       0x%02x%02x%02x\n", \
          ExtCsd->GpSizeMult[11],
          ExtCsd->GpSizeMult[10],
          ExtCsd->GpSizeMult[9]
         )
         );
  DEBUG (
         (
          DEBUG_INFO,
          "  Enhanced User Data Area Size           0x%02x%02x%02x\n", \
          ExtCsd->EnhSizeMult[2],
          ExtCsd->EnhSizeMult[1],
          ExtCsd->EnhSizeMult[0]
         )
         );
  DEBUG ((DEBUG_INFO, "  Enhanced User Data Start Address       0x%x\n", *((UINT32 *)&ExtCsd->EnhStartAddr[0])));
  DEBUG ((DEBUG_INFO, "  Bad Block Management mode              0x%x\n", ExtCsd->SecBadBlkMgmnt));
  DEBUG ((DEBUG_INFO, "  Native sector size                     0x%x\n", ExtCsd->NativeSectorSize));
  DEBUG ((DEBUG_INFO, "  Sector size emulation                  0x%x\n", ExtCsd->UseNativeSector));
  DEBUG ((DEBUG_INFO, "  Sector size                            0x%x\n", ExtCsd->DataSectorSize));
}
