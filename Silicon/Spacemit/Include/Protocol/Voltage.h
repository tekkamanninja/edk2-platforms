/** @file
  API related to voltage modification

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SPACEMIT_VOLTAGE_H__
#define __SPACEMIT_VOLTAGE_H__

#define SPACEMIT_SILICON_VOLTAGE_PROTOCOL_GUID \
  { 0x94715D38, 0x5143, 0x4F8E, { 0x9B, 0xA6, 0x69, 0x38, 0x9C, 0xA5, 0x33, 0xED }}

typedef struct _SILICON_VOLTAGE_PROTOCOL SILICON_VOLTAGE_PROTOCOL;

#define EFI_VOLTAGE_PROTOCOL_REVISION  0x00010000

/**
 * Lists supported voltage ranges for a given voltage domain.
 *
 * @param[in] This           Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in] DomainName     Name of the voltage domain to query
 *
 * @retval EFI_SUCCESS           Successfully listed voltage ranges
 * @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 * @retval EFI_NOT_FOUND         Specified voltage domain not found
 */
typedef
EFI_STATUS
(EFIAPI *LIST_VOLTAGE) (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName
  );

/**
 * Enables voltage output of a specified voltage domain.
 *
 * @param[in] This          Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in] DomainName    Name of the voltage domain to enable
 *
 * @retval EFI_SUCCESS           Successfully enabled the voltage domain
 * @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 * @retval EFI_NOT_READY         RPMI channel not initialized
 * @retval EFI_NOT_FOUND         Specified voltage domain not found
 * @retval Others                Error occurred during RPMI message handling
 */
typedef
EFI_STATUS
(EFIAPI *ENABLE_VOLTAGE) (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName
  );

/**
 * Disables voltage output of a specified voltage domain.
 *
 * @param[in] This          Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in] DomainName    Name of the voltage domain to disable
 *
 * @retval EFI_SUCCESS           Successfully disabled the voltage domain
 * @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 * @retval EFI_NOT_READY         RPMI channel not initialized
 * @retval EFI_NOT_FOUND         Specified voltage domain not found
 * @retval Others                Error occurred during RPMI message handling
 */
typedef
EFI_STATUS
(EFIAPI *DISABLE_VOLTAGE) (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName
  );

/**
 * Checks if a specified voltage domain is enabled.
 *
 * @param[in]  This           Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in]  DomainName     Name of the voltage domain to check
 * @param[out] Enabled        Pointer to store the enabled status (TRUE if enabled)
 *
 * @retval EFI_SUCCESS            Status was successfully retrieved
 * @retval EFI_INVALID_PARAMETER  One or more parameters are invalid
 * @retval EFI_NOT_READY          RPMI channel not initialized
 * @retval EFI_NOT_FOUND          Specified voltage domain not found
 * @retval Others                 Error occurred during RPMI message handling
 */
typedef
EFI_STATUS
(EFIAPI *IS_ENABLED) (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName,
  IN BOOLEAN                   *Enabled
  );

/**
 * Sets the voltage level for a specified voltage domain.
 *
 * @param[in] This              Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in] DomainName        Name of the voltage domain to set voltage for
 * @param[in] VoltageMicroVolt  Target voltage level in microvolts
 *
 * @retval EFI_SUCCESS           Voltage was set successfully
 * @retval EFI_INVALID_PARAMETER If This or DomainName is NULL
 * @retval Other                 Other errors that may occur during voltage setting
 **/
typedef
EFI_STATUS
(EFIAPI *SET_VOLTAGE) (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName,
  IN UINTN                     VoltageMicroVolt
  );

/**
 * Gets the voltage level for a specified voltage domain.
 *
 * @param[in]  This              Pointer to the SILICON_VOLTAGE_PROTOCOL instance
 * @param[in]  DomainName        Name of the voltage domain to query
 * @param[out] VoltageMicroVolt  Pointer to store the voltage level in microvolts
 *
 * @retval EFI_SUCCESS            Voltage level was successfully retrieved
 * @retval EFI_INVALID_PARAMETER  One or more parameters are invalid
 * @retval Others                 Error occurred during RPMI message handling
 */
typedef
EFI_STATUS
(EFIAPI *GET_VOLTAGE) (
  IN SILICON_VOLTAGE_PROTOCOL  *This,
  IN CONST CHAR8               *DomainName,
  OUT UINTN                    *VoltageMicroVolt
  );

struct _SILICON_VOLTAGE_PROTOCOL {
  UINT64             Revision;
  LIST_VOLTAGE       ListVoltageRange;
  ENABLE_VOLTAGE     EnableVoltage;
  DISABLE_VOLTAGE    DisableVoltage;
  IS_ENABLED         IsVoltageEnabled;
  SET_VOLTAGE        SetVoltage;
  GET_VOLTAGE        GetVoltage;
};

extern EFI_GUID  gSpacemitSiliconVoltageProtocolGuid;

#endif /* ifndef __SPACEMIT_VOLTAGE_H__ */
