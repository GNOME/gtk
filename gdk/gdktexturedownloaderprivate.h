/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2023 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "gdktexturedownloader.h"

#include "gdkmemorylayoutprivate.h"

G_BEGIN_DECLS

struct _GdkTextureDownloader
{
  /*< private >*/
  GdkTexture *texture;
  GdkMemoryFormat format;
  GdkColorState *color_state;
};

void                    gdk_texture_downloader_init                     (GdkTextureDownloader           *self,
                                                                         GdkTexture                     *texture);
void                    gdk_texture_downloader_finish                   (GdkTextureDownloader           *self);

void                    gdk_texture_downloader_download_into_layout     (const GdkTextureDownloader     *self,
                                                                         guchar                         *data,
                                                                         const GdkMemoryLayout          *layout);
GBytes *                gdk_texture_downloader_download_bytes_layout    (const GdkTextureDownloader     *self,
                                                                         GdkMemoryLayout                *out_layout);

G_END_DECLS

