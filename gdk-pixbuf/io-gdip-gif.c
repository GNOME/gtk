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

static gboolean
gdk_pixbuf__gdip_image_save_GIF_to_callback (GdkPixbufSaveFunc   save_func,
                                             gpointer            user_data,
                                             GdkPixbuf          *pixbuf,
                                             gchar             **keys,
                                             gchar             **values,
                                             GError            **error)
{
  return gdip_save_pixbuf (pixbuf, L"image/gif", NULL, save_func, user_data, error);
}

static gboolean
gdk_pixbuf__gdip_image_save_GIF (FILE          *f,
                                 GdkPixbuf     *pixbuf,
                                 gchar        **keys,
                                 gchar        **values,
                                 GError       **error)
{
  return gdk_pixbuf__gdip_image_save_GIF_to_callback (gdip_save_to_file_callback, f, pixbuf, keys, values, error);
}

#ifndef INCLUDE_gdiplus
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__gdip_gif_ ## function
#endif

MODULE_ENTRY (fill_vtable) (GdkPixbufModule *module)
{
  gdip_fill_vtable (module);

  module->save_to_callback = gdk_pixbuf__gdip_image_save_GIF_to_callback;
  module->save = gdk_pixbuf__gdip_image_save_GIF; /* for gtk < 2.14, you need to implement both. otherwise gdk-pixbuf-queryloaders fails */
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
  static GdkPixbufModulePattern signature[] = {
    { "GIF8", NULL, 100 }, /* GIF */
    { NULL, NULL, 0 }
  };

  static gchar *mime_types[] = {
    "image/gif",
    NULL
  };

  static gchar *extensions[] = {
    "gif",
    NULL
  };

  info->name        = "gif";
  info->signature   = signature;
  info->description = _("The GIF image format");
  info->mime_types  = mime_types;
  info->extensions  = extensions;
  info->flags       = GDK_PIXBUF_FORMAT_WRITABLE | GDK_PIXBUF_FORMAT_THREADSAFE;
  info->license     = "LGPL";
}
