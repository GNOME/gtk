/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gdkinput.h"
#include "gdkdevicemanager.h"
#include "gdkdisplay.h"
#include "gdkalias.h"

GList *
gdk_devices_list (void)
{
  return gdk_display_list_devices (gdk_display_get_default ());
}

void
gdk_input_set_extension_events (GdkWindow        *window,
                                gint              mask,
                                GdkExtensionMode  mode)
{
  /* FIXME: Not implemented */
}

#define __GDK_INPUT_C__
#include "gdkaliasdef.c"
