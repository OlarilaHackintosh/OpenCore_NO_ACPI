/** @file
  Copyright (C) 2018, vit9696. All rights reserved.
  Copyright (C) 2020, PMheart. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "ocvalidate.h"
#include "OcValidateLib.h"

/**
  Callback funtion to verify whether Path is duplicated in ACPI->Add.

  @param[in]  PrimaryEntry    Primary entry to be checked.
  @param[in]  SecondaryEntry  Secondary entry to be checked.

  @retval     TRUE            If PrimaryEntry and SecondaryEntry are duplicated.
**/
STATIC
BOOLEAN
ACPIAddHasDuplication (
  IN  CONST VOID  *PrimaryEntry,
  IN  CONST VOID  *SecondaryEntry
  )
{
  CONST OC_ACPI_ADD_ENTRY    *ACPIAddPrimaryEntry;
  CONST OC_ACPI_ADD_ENTRY    *ACPIAddSecondaryEntry;
  CONST CHAR8                *ACPIAddPrimaryPathString;
  CONST CHAR8                *ACPIAddSecondaryPathString;

  ACPIAddPrimaryEntry        = *(CONST OC_ACPI_ADD_ENTRY **) PrimaryEntry;
  ACPIAddSecondaryEntry      = *(CONST OC_ACPI_ADD_ENTRY **) SecondaryEntry;
  ACPIAddPrimaryPathString   = OC_BLOB_GET (&ACPIAddPrimaryEntry->Path);
  ACPIAddSecondaryPathString = OC_BLOB_GET (&ACPIAddSecondaryEntry->Path);

  if (!ACPIAddPrimaryEntry->Enabled || !ACPIAddSecondaryEntry->Enabled) {
    return FALSE;
  }

  return StringIsDuplicated ("ACPI->Add", ACPIAddPrimaryPathString, ACPIAddSecondaryPathString);
}

UINT32
CheckACPI (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32          ErrorCount;
  UINT32          Index;
  OC_ACPI_CONFIG  *UserAcpi;
  CONST CHAR8     *Path;
  CONST CHAR8     *Comment;
  CONST UINT8     *Find;
  UINT32          FindSize;
  CONST UINT8     *Replace;
  UINT32          ReplaceSize;
  CONST UINT8     *Mask;
  UINT32          MaskSize;
  CONST UINT8     *ReplaceMask;
  UINT32          ReplaceMaskSize;
  BOOLEAN         HasCustomDSDT;

  DEBUG ((DEBUG_VERBOSE, "config loaded into ACPI checker!\n"));

  ErrorCount    = 0;
  UserAcpi      = &Config->Acpi;
  HasCustomDSDT = FALSE;

  for (Index = 0; Index < UserAcpi->Add.Count; ++Index) {
    Path         = OC_BLOB_GET (&UserAcpi->Add.Values[Index]->Path);
    Comment      = OC_BLOB_GET (&UserAcpi->Add.Values[Index]->Comment);

    //
    // Sanitise strings.
    //
    if (!AsciiFileSystemPathIsLegal (Path)) {
      DEBUG ((DEBUG_WARN, "ACPI->Add[%u]->Path contains illegal character!\n", Index));
      ++ErrorCount;
      continue;
    }
    if (!AsciiCommentIsLegal (Comment)) {
      DEBUG ((DEBUG_WARN, "ACPI->Add[%u]->Comment contains illegal character!\n", Index));
      ++ErrorCount;
    }

    if (!OcAsciiEndsWith (Path, ".aml", TRUE) && !OcAsciiEndsWith (Path, ".bin", TRUE)) {
      DEBUG ((DEBUG_WARN, "ACPI->Add[%u]->Path has filename suffix other than .aml and .bin!\n", Index));
      ++ErrorCount;
    }

    if (OcAsciiStriStr (Path, "DSDT") != NULL && UserAcpi->Add.Values[Index]->Enabled) {
      HasCustomDSDT = TRUE;
    }
  }

  //
  // Check duplicated entries in ACPI->Add.
  //
  ErrorCount += FindArrayDuplication (
    UserAcpi->Add.Values,
    UserAcpi->Add.Count,
    sizeof (UserAcpi->Add.Values[0]),
    ACPIAddHasDuplication
    );

  for (Index = 0; Index < UserAcpi->Delete.Count; ++Index) {
    Comment = OC_BLOB_GET (&UserAcpi->Delete.Values[Index]->Comment);

    //
    // Sanitise strings.
    //
    if (!AsciiCommentIsLegal (Comment)) {
      DEBUG ((DEBUG_WARN, "ACPI->Delete[%u]->Comment contains illegal character!\n", Index));
      ++ErrorCount;
    }

    //
    // Size of OemTableId and TableSignature cannot be checked,
    // as serialisation kills it.
    //
  }

  for (Index = 0; Index < UserAcpi->Patch.Count; ++Index) {
    Comment         = OC_BLOB_GET (&UserAcpi->Patch.Values[Index]->Comment);
    Find            = OC_BLOB_GET (&UserAcpi->Patch.Values[Index]->Find);
    FindSize        = UserAcpi->Patch.Values[Index]->Find.Size;
    Replace         = OC_BLOB_GET (&UserAcpi->Patch.Values[Index]->Replace);
    ReplaceSize     = UserAcpi->Patch.Values[Index]->Replace.Size;
    Mask            = OC_BLOB_GET (&UserAcpi->Patch.Values[Index]->Mask);
    MaskSize        = UserAcpi->Patch.Values[Index]->Mask.Size;
    ReplaceMask     = OC_BLOB_GET (&UserAcpi->Patch.Values[Index]->ReplaceMask);
    ReplaceMaskSize = UserAcpi->Patch.Values[Index]->ReplaceMask.Size;

    //
    // Sanitise strings.
    //
    if (!AsciiCommentIsLegal (Comment)) {
      DEBUG ((DEBUG_WARN, "ACPI->Patch[%u]->Comment contains illegal character!\n", Index));
      ++ErrorCount;
    }

    //
    // Size of OemTableId and TableSignature cannot be checked,
    // as serialisation kills it.
    //

    //
    // Checks for size.
    //
    ErrorCount += ValidatePatch (
      "ACPI->Patch",
      Index,
      FALSE,
      Find,
      FindSize,
      Replace,
      ReplaceSize,
      Mask,
      MaskSize,
      ReplaceMask,
      ReplaceMaskSize
      ); 
  }

  //
  // Check for RebaseRegions when using customised DSDT.
  //
  if (HasCustomDSDT && !UserAcpi->Quirks.RebaseRegions) {
    DEBUG ((DEBUG_WARN, "ACPI->Quirks->RebaseRegions is not enabled when customised DSDT table is in use!\n"));
    ++ErrorCount;
  }

  return ReportError (__func__, ErrorCount);
}
