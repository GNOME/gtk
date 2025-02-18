#!/usr/bin/env python3

# Author  : Simos Xenitellis <simos at gnome dot org>.
# Author  : Bastien Nocera <hadess@hadess.net>
# Author  : Alberto Ruiz <aruiz@gnome.org>
# Version : 1.2
#
# Notes   : It downloads keysymdef.h from the Internet,
# Notes   : and creates an updated clutter-keysyms.h

import os
import sys
import re
from typing import TextIO, Iterable

import requests  # type: ignore[import-untyped]

KEYSYMDEF_URL = "https://gitlab.freedesktop.org/xorg/proto/xorgproto/-/raw/master/include/X11/keysymdef.h"
XF86KEYSYM_URL = "https://gitlab.freedesktop.org/xorg/proto/xorgproto/-/raw/master/include/X11/XF86keysym.h"

LICENSE_HEADER = f"""/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2005, 2006, 2007, 2009 GNOME Foundation
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

/*
 * File auto-generated from script at:
 * https://gitlab.gnome.org/GNOME/gtk/-/raw/main/gdk/gdk-keysyms-update.py?ref_type=heads&inline=false
 *
 * using the input files:
 *  {KEYSYMDEF_URL}
 * and
 *  {XF86KEYSYM_URL}
 */

#pragma once
"""

GDK_KEYSYMS_H = "gdkkeysyms.h"


def get_and_filter_header(
    url: str,
    prefix: str,
    output_file: TextIO,
    filter_out: Iterable[str] | None = None,
    replaces: dict[str, str] | None = None,
):
    """
    Fetches and processes a header file containing macro definitions, filtering
    and transforming entries based on the specified prefix, exclusions, and
    replacements.

    Args:
        url: The URL to fetch the file content from.
        prefix: The prefix to filter the macro definitions in the file.
        output_file: The file object where the filtered and transformed
                     definitions will be written.
        filter_out: An iterable producing macro names to exclude from
                    processing. Defaults to None.
        replaces: A dictionary mapping macro names to their replacements.
                  Defaults to None.
    """
    for line in requests.get(url, timeout=5).text.splitlines():
        if not line.startswith("#define " + prefix):
            continue

        keysymelements = line.split()
        if len(keysymelements) < 3:
            sys.exit(
                f"Internal error, no properly defined keysym entry: {line}",
            )

        if keysymelements[2].startswith("_EVDEVK("):
            value = int(keysymelements[2][8:-1], 0)
            value += 0x10081000  # _EVDEVK macro does this
            keysymelements[2] = hex(value)
        elif not keysymelements[2].startswith("0x"):
            sys.exit(
                f'Internal error, was expecting "0x*", found: {keysymelements[2]}',
            )

        if filter_out and keysymelements[1] in filter_out:
            continue

        if replaces and keysymelements[1] in replaces.keys():
            keysymelements[1] = replaces[keysymelements[1]]

        binding = re.sub(f"^{prefix}", "GDK_KEY_", keysymelements[1])
        print(
            f"#define {binding} 0x{int(keysymelements[2], 0):03x}",
            file=output_file,
        )


if __name__ == "__main__":
    if os.path.exists(GDK_KEYSYMS_H):
        sys.exit(
            f"""There is already a {GDK_KEYSYMS_H} file in this directory. We are not overwriting it.
Please move it somewhere else in order to run this script.
Exiting...""",
        )

    with open(GDK_KEYSYMS_H, "x", encoding="utf8") as output_file:
        print(LICENSE_HEADER, file=output_file)

        get_and_filter_header(KEYSYMDEF_URL, "XK_", output_file)

        # Work-around https://bugs.freedesktop.org/show_bug.cgi?id=11193
        # XF86XK_Clear could end up a dupe of XK_Clear
        # XF86XK_Select could end up a dupe of XK_Select
        # Ignore XF86XK_Qu, XF86XK_Calculater is misspelled, and a dupe
        get_and_filter_header(
            XF86KEYSYM_URL,
            "XF86XK_",
            output_file,
            ("XF86XK_Q", "XF86XK_Calculater", "XF86XK_Break"),
            {
                "XF86XK_XF86BackForward": "XF86XK_AudioForward",
                "XF86XK_Clear": "XF86XK_WindowClear",
                "XF86XK_Select": "XF86XK_SelectButton",
            },
        )
