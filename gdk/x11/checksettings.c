/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2006 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdktypes.h"

#include <glib.h>
#include <string.h>

#include "gdksettings.c"

int
main (int   argc,
      char *argv[])
{
  guint i, accu = 0;

  for (i = 0; i < GDK_SETTINGS_N_ELEMENTS(); i++)
    {
      if (gdk_settings_map[i].xsettings_offset != accu)
        g_error ("settings_map[%u].xsettings_offset != %u\n", i, accu);
      accu += strlen (gdk_settings_names + accu) + 1;
      if (gdk_settings_map[i].gdk_offset != accu)
        g_error ("settings_map[%u].gdk_offset != %u\n", i, accu);
      accu += strlen (gdk_settings_names + accu) + 1;
    }

  return 0;
}
