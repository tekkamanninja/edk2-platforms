Scope (\_SB)
{
    Device (USB2)
    {
        Name (_HID, "808622B7")  // HID
        Name (_UID, 0)           // Unique ID
        Name (_CCA, 0)           // Cache Coherency Attribute

        Name (_CRS, ResourceTemplate () {
            QWordMemory (
                ResourceConsumer,
                PosDecode,
                MinFixed,
                MaxFixed,
                NonCacheable,
                ReadWrite,
                0x0,                // AddressGranularity, should be 2^n - 1
                0xC0A00000,         // AddressMinimum
                0xC0A0FFFF,         // AddressMaximum
                0x00000000,         // AddressTranslation
                0x10000             // RangeLength
                )
            Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 105 }
        })

        Method (_STA, 0, NotSerialized) {
            Return (0x0F)
        }

        Name (_DSD, Package () {
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
            Package () {
                Package () { "dr_mode", "host" },
                Package () { "phy_type", "utmi" },
                Package () { "snps,hsphy_interface", "utmi" },
                Package () { "snps,dis_enblslpm_quirk", 1 },
                Package () { "snps,dis_u2_susphy_quirk", 1 },
                Package () { "snps,dis_u3_susphy_quirk", 1 },
                Package () { "snps,dis-del-phy-power-chg-quirk", 1 },
                Package () { "snps,dis-tx-ipgap-linecheck-quirk", 1 },
                Package () { "snps,parkmode-disable-ss-quirk", 1 },
                Package () { "usb-role-switch", 1 },
                Package () { "role-switch-default-mode", "host" },
                Package () { "maximum-speed", "high-speed" },
            }
        })
    }
}
