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

#include <stdbool.h>

typedef enum {
  OSVersionWindows7,
  OSVersionWindows8,
  OSVersionWindows8_1,
  OSVersionWindows10,
  OSVersionWindows11,
} OSVersion;

bool                  gdk_win32_check_app_container (void);

bool                  gdk_win32_check_high_integrity (void);

bool                  gdk_win32_check_manually_elevated (void);

OSVersion             gdk_win32_get_os_version (void);
