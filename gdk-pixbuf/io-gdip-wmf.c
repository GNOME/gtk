/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GdkPixbuf library - Win32 GDI+ Pixbuf Loader
 *
 * Copyright (C) 2008 Dominic Lachowicz
 * Copyright (C) 2008 Alberto Ruiz
 *
 * Authors: Dominic Lachowicz <domlachowicz@gmail.com>
 *          Alberto Ruiz <aruiz@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more  * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "io-gdip-utils.h"

#ifndef INCLUDE_gdiplus
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__gdip_wmf_ ## function
#endif

MODULE_ENTRY (fill_vtable) (GdkPixbufModule *module)
{
  gdip_fill_vector_vtable (module);
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
  static GdkPixbufModulePattern signature[] = {
    { "\xd7\xcd\xc6\x9a", NULL, 100 }, /* WMF */
    { "\x01\x00\x09\x00", NULL, 100 }, /* WMF */
    { NULL, NULL, 0 }
  };

  static gchar *mime_types[] = {
    "image/x-wmf",
    NULL
  };

  static gchar *extensions[] = {
    "wmf",
    "apm",
    NULL
  };

  info->name        = "wmf";
  info->signature   = signature;
  info->description = _("The WMF image format");
  info->mime_types  = mime_types;
  info->extensions  = extensions;
  info->flags       = GDK_PIXBUF_FORMAT_THREADSAFE;
  info->license     = "LGPL";
}
