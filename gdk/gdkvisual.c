/* GDK - The GIMP Drawing Kit
 * gdkvisual.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gdkvisual.h"
#include "gdkscreen.h"
#include "gdkalias.h"

/**
 * gdk_list_visuals:
 * 
 * Lists the available visuals for the default screen.
 * (See gdk_screen_list_visuals())
 * A visual describes a hardware image data format.
 * For example, a visual might support 24-bit color, or 8-bit color,
 * and might expect pixels to be in a certain format.
 *
 * Call g_list_free() on the return value when you're finished with it.
 * 
 * Return value: a list of visuals; the list must be freed, but not its contents
 **/
GList*
gdk_list_visuals (void)
{
  return gdk_screen_list_visuals (gdk_screen_get_default ());
}

/**
 * gdk_visual_get_system:
 * 
 * Get the system'sdefault visual for the default GDK screen.
 * This is the visual for the root window of the display.
 * The return value should not be freed.
 * 
 * Return value: system visual
 **/
GdkVisual*
gdk_visual_get_system (void)
{
  return gdk_screen_get_system_visual (gdk_screen_get_default());
}

#define __GDK_VISUAL_C__
#include "gdkaliasdef.c"
