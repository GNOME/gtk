/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2024 the GTK team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "config.h"

#include "gdkmain-win32.h"

#include <windows.h>
#include <stdbool.h>

#undef API_SET

/* Api sets whose name starts with 'api' can be removed from the table once
 * support for older OSes is dropped. On the countrary, api sets whose name
 * starts with 'ext' must remain in the table because they are not present
 * on all Windows editions (Core, Desktop, Hololens, etc.) */

#define API_SETS \
  API_SET (API_CORE_APIQUERY_2                  , "api-ms-win-core-apiquery-l2-1-0.dll"                  , OSVersionWindows10) \
  API_SET (API_APPMODEL_RUNTIME_1               , "api-ms-win-appmodel-runtime-l1-1-0.dll"               , OSVersionWindows8 ) \
  API_SET (API_CORE_LIBRARYLOADER_2             , "api-ms-win-core-libraryloader-l2-1-0.dll"             , OSVersionWindows8 ) \

#define MODULES \
  MODULE (MODULE_KERNEL32      , "kernel32.dll"         , FOLDER_SYSTEM32        , 0                             ) \
  MODULE (MODULE_USER32        , "user32.dll"           , FOLDER_SYSTEM32        , 0                             ) \

#define API_SET(id, name, os_version) id,
enum ApiSets {
  API_SETS

  API_SET_COUNT
};
#undef API_SET

#define MODULE(id, name, folder, flags) id,
enum Modules {
  MODULES

  MODULE_COUNT
};
#undef MODULE

struct api_set {
  const char    *name_narrow;
  const wchar_t *name_wide;
  HMODULE        module_handle;
  bool           checked;
  OSVersion      os_version;
};

struct api_set * gdk_win32_get_api_set (int api_set_id);

enum ModuleFolder {
  FOLDER_SYSTEM32,
  FOLDER_APP,
};

#define MODULE_FLAG_PACKAGED (1U << 0)
#define MODULE_FLAG_DIRECT (1U << 1)

struct module {
  const wchar_t *name;
  enum ModuleFolder folder;
  unsigned int flags;
  HMODULE module_handle;
  bool checked;
};

struct module * gdk_win32_get_module (int module_id);

