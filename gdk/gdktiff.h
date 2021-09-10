/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 Red Hat, Inc.
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

#ifndef __GDK_TIFF_H__
#define __GDK_TIFF_H__

#include "gdkmemorytexture.h"
#include <gio/gio.h>

GdkTexture *gdk_load_tiff         (GInputStream     *stream,
                                   GError          **error);

void        gdk_load_tiff_async   (GInputStream           *stream,
                                   GCancellable           *cancellable,
                                   GAsyncReadyCallback     callback,
                                   gpointer                user_data);

GdkTexture *gdk_load_tiff_finish  (GAsyncResult           *result,
                                   GError                **error);

gboolean    gdk_save_tiff         (GOutputStream    *stream,
                                   const guchar     *data,
                                   int               width,
                                   int               height,
                                   int               stride,
                                   GdkMemoryFormat   format,
                                   GError          **error);

void        gdk_save_tiff_async  (GOutputStream          *stream,
                                  const guchar           *data,
                                  int                     width,
                                  int                     height,
                                  int                     stride,
                                  GdkMemoryFormat         format,
                                  GCancellable           *cancellable,
                                  GAsyncReadyCallback     callback,
                                  gpointer                user_data);

gboolean    gdk_save_tiff_finish (GAsyncResult           *result,
                                  GError                **error);

#endif
