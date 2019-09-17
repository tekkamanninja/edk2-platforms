# Introduction

## U500 Platform Package
This is a sample RISC-V EDK2 platform package used agaist SiFive Freedom U500 VC707 FPGA Dev Kit, please refer to "SiFive Freedom U500 VC707 FPGA Getting Started Guide" on https://www.sifive.com/documentation. This package is built with below common packages, <br>
- **RiscVPlatformPkg**, edk2-platform/Platform/RiscV
- **RiscVPkg**, edk2 master branch (Currently is in edk2-staging/RISC-V branch)
<br>
This package provides librareis and modules which are SiFive U500 platform implementation-specific and incorporate with common RISC-V packages mentioned above.

## Download the sources
```
git clone https://github.com/tianocore/edk2-staging.git
# Checkout RISC-V-V2 branch
git clone https://github.com/tianocore/edk2-platforms.git
# Checkout devel-riscv-v2 branch
git clone https://github.com/tianocore/edk2-non-osi.git
```

## Platform Owners
Chang, Abner <abner.chang@hpe.com><br>
Chen, Gilbert <gilbert.chen@hpe.com>

## Platform Status
Currently the binary built from U500Pkg can boot SiFive Freedom U500 VC707 FPGA to EFI shell with console in/out enabled.

## Linux Build Instructions
You can build the RISC-V platform using below script, <br>
`build -a RISCV64 -p Platform/RiscV/SiFive/U500Pkg/U500.dsc -t GCCRISCV`

## Supported Operating Systems
Only support to boot to EFI Shell so far

## Known Issues and Limitations
Only RISC-V RV64 is verified on this platform.

## Related Materials
- [RISC-V OpenSbi](https://github.com/riscv/opensbi)<br>
- [SiFive U500 VC707 FPGA Getting Started Guide](https://sifive.cdn.prismic.io/sifive%2Fc248fabc-5e44-4412-b1c3-6bb6aac73a2c_sifive-u500-vc707-gettingstarted-v0.2.pdf)<br>
- [SiFive RISC-V Core Document](https://www.sifive.com/documentation)

## U500 Platform Libraries and Drivers
### OpneSbiPlatformLib
In order to reduce the dependencies with RISC-V OpenSBI project (https://github.com/riscv/opensbi) and less burdens to EDK2 build process, the implementation of RISC-V EDK2 platform is leverage platform source code from OpenSBI code tree. The "platform.c" under OpenSbiPlatformLib  is cloned from RISC-V OpenSBI code tree (in EDK2 RiscVPkg) and built based on EDK2 build environment.

### PeiCoreInfoHobLib
This is the library to create RISC-V core characteristics for building up RISC-V related SMBIOS records to support the unified boot loader and OS image. This library leverage the silicon libraries provided in Silicon/SiFive.

### RiscVPlatformTimerLib
This is U500 platform timer library which has the platform-specific timer implementation.

### PlatformPei
This is the platform-implementation specific library which is executed in early PEI phase for platform initialization.

### TimerDxe
This is U500 platform timer DXE driver whcih has the platform-specific timer implementation.

## U500 Platform PCD settings

| **PCD name** |**Usage**|
|----------------|----------|
|PcdNumberofU5Cores| Number of U5 core enabled on U500 platform|
|PcdE5MCSupported| Indicates whether or not the Monitor Core (E5) is supported on U500 platform|
