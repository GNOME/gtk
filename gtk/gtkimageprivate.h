/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_IMAGE_PRIVATE_H__
#define __GTK_IMAGE_PRIVATE_H__


#include "gtkimage.h"


G_BEGIN_DECLS

typedef struct _GtkImagePixbufData  GtkImagePixbufData;
typedef struct _GtkImageStockData   GtkImageStockData;
typedef struct _GtkImageIconSetData GtkImageIconSetData;
typedef struct _GtkImageAnimationData GtkImageAnimationData;
typedef struct _GtkImageIconNameData  GtkImageIconNameData;
typedef struct _GtkImageGIconData     GtkImageGIconData;

struct _GtkImagePixbufData
{
  GdkPixbuf *pixbuf;
};

struct _GtkImageStockData
{
  gchar *stock_id;
};

struct _GtkImageIconSetData
{
  GtkIconSet *icon_set;
};

struct _GtkImageAnimationData
{
  GdkPixbufAnimation *anim;
  GdkPixbufAnimationIter *iter;
  guint frame_timeout;
};

struct _GtkImageIconNameData
{
  gchar *icon_name;
  GdkPixbuf *pixbuf;
  guint theme_change_id;
};

struct _GtkImageGIconData
{
  GIcon *icon;
  GdkPixbuf *pixbuf;
};

void _gtk_image_gicon_data_clear (GtkImageGIconData *data);

G_END_DECLS

#endif /* __GTK_IMAGE_H__ */
