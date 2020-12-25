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

#include <File.h>

/*
 for fuzzing (TODO):
 clang-mp-7.0 -Dmain=__main -g -fsanitize=undefined,address,fuzzer -I../Include -I../../Include -I../../../MdePkg/Include/ -include ../Include/Base.h ConfigValidty.c ../../Library/OcXmlLib/OcXmlLib.c ../../Library/OcTemplateLib/OcTemplateLib.c ../../Library/OcSerializeLib/OcSerializeLib.c ../../Library/OcMiscLib/Base64Decode.c ../../Library/OcStringLib/OcAsciiLib.c ../../Library/OcConfigurationLib/OcConfigurationLib.c -o ConfigValidty
 rm -rf DICT fuzz*.log ; mkdir DICT ; cp ConfigValidty.plist DICT ; ./ConfigValidty -jobs=4 DICT

 rm -rf ConfigValidty.dSYM DICT fuzz*.log ConfigValidty
*/

UINT32
CheckConfig (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32  ErrorCount;
  UINTN   Index;
  STATIC CONFIG_CHECK ConfigCheckers[] = {
    &CheckACPI,
    &CheckBooter,
    &CheckDeviceProperties,
    &CheckKernel,
    &CheckMisc,
    &CheckNVRAM,
    &CheckPlatformInfo,
    &CheckUEFI
  };

  ErrorCount = 0;

  //
  // Pass config structure to all checkers.
  //
  for (Index = 0; Index < ARRAY_SIZE (ConfigCheckers); ++Index) {
    ErrorCount += ConfigCheckers[Index] (Config);
  }

  return ErrorCount;
}

int main(int argc, const char *argv[]) {
  UINT8              *ConfigFileBuffer;
  UINT32             ConfigFileSize;
  CONST CHAR8        *ConfigFileName;
  INT64              ExecTimeStart;
  OC_GLOBAL_CONFIG   Config;
  EFI_STATUS         Status;
  UINT32             ErrorCount;

  //
  // Enable PCD debug logging.
  //
  PcdGet8  (PcdDebugPropertyMask)         |= DEBUG_PROPERTY_DEBUG_CODE_ENABLED;
  PcdGet32 (PcdFixedDebugPrintErrorLevel) |= DEBUG_INFO;
  PcdGet32 (PcdDebugPrintErrorLevel)      |= DEBUG_INFO;

  //
  // Read config file.
  //
  ConfigFileName   = argc > 1 ? argv[1] : "config.plist";
  ConfigFileBuffer = readFile (ConfigFileName, &ConfigFileSize);
  if (ConfigFileBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to read %a\n", ConfigFileName));
    return -1;
  }

  //
  // Record the current time when action starts.
  //
  ExecTimeStart = GetCurrentTimestamp ();

  //
  // Initialise config structure to be checked, and exit on error.
  //
  Status = OcConfigurationInit (&Config, ConfigFileBuffer, ConfigFileSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Invalid config\n"));
    return -1;
  }

  ErrorCount = CheckConfig (&Config);
  if (ErrorCount == 0) {
    DEBUG ((
      DEBUG_ERROR,
      "Done checking %a in %llu ms\n",
      ConfigFileName,
      GetCurrentTimestamp () - ExecTimeStart
      ));
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "Done checking %a in %llu ms, but it has %u %a to be fixed\n",
      ConfigFileName,
      GetCurrentTimestamp () - ExecTimeStart,
      ErrorCount,
      ErrorCount > 1 ? "errors" : "error"
      ));
  }

  OcConfigurationFree (&Config);
  free (ConfigFileBuffer);

  return 0;
}

INT32 LLVMFuzzerTestOneInput(CONST UINT8 *Data, UINTN Size) {
  VOID *NewData = AllocatePool (Size);
  if (NewData) {
    CopyMem (NewData, Data, Size);
    OC_GLOBAL_CONFIG   Config;
    OcConfigurationInit (&Config, NewData, Size);
    OcConfigurationFree (&Config);
    FreePool (NewData);
  }
  return 0;
}
