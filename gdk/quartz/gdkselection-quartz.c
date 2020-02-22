/* gdkselection-quartz.c
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 * Copyright (C) 2005 Imendio AB
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

#include "config.h"

#include "gdkquartz.h"

GdkAtom
gdk_quartz_pasteboard_type_to_atom_libgtk_only (NSString *type)
{
  if ([type isEqualToString:NSStringPboardType])
    return g_intern_static_string ("UTF8_STRING");
  else if ([type isEqualToString:NSTIFFPboardType])
    return g_intern_static_string ("image/tiff");
  else if ([type isEqualToString:NSColorPboardType])
    return g_intern_static_string ("application/x-color");
  else if ([type isEqualToString:NSURLPboardType])
    return g_intern_static_string ("text/uri-list");
  else
    return g_intern_string ([type UTF8String]);
}

NSString *
gdk_quartz_target_to_pasteboard_type_libgtk_only (const char *target)
{
  if (strcmp (target, "UTF8_STRING") == 0)
    return NSStringPboardType;
  else if (strcmp (target, "image/tiff") == 0)
    return NSTIFFPboardType;
  else if (strcmp (target, "application/x-color") == 0)
    return NSColorPboardType;
  else if (strcmp (target, "text/uri-list") == 0)
    return NSURLPboardType;
  else
    return [NSString stringWithUTF8String:target];
}

NSString *
gdk_quartz_atom_to_pasteboard_type_libgtk_only (GdkAtom atom)
{
  const char *target = (const char *)atom;
  NSString *ret = gdk_quartz_target_to_pasteboard_type_libgtk_only (target);

  return ret;
}
