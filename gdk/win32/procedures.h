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

#include <windows.h>

typedef BOOL
(APIENTRY *IsApiSetImplemented_t) (PCSTR Contract);

typedef LONG
(WINAPI *GetCurrentPackageFullName_t) (UINT32 packageFullNameLength,
                                       PWSTR  packageFullName);

typedef HMODULE
(WINAPI *LoadPackagedLibrary_t) (LPCWSTR lpwLibFileName,
                                 DWORD   Reserved);

#define PROCEDURES \
  PROCEDURE (IsApiSetImplemented           , API_CORE_APIQUERY_2              , -1                        , -1               ) \
  PROCEDURE (GetCurrentPackageFullName     , API_APPMODEL_RUNTIME_1           , MODULE_KERNEL32           , OSVersionWindows8) \
  PROCEDURE (LoadPackagedLibrary           , API_CORE_LIBRARYLOADER_2         , MODULE_KERNEL32           , OSVersionWindows8) \

#define PROCEDURE(name, api_set_id, module_id, os_version) \
  extern name##_t ptr##name;
PROCEDURES
#undef PROCEDURE

void gdk_win32_procedures_load (void);
void gdk_win32_procedures_unload (void);

