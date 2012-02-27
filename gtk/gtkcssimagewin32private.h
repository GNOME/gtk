/*
 * Copyright © 2011 Red Hat Inc.
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
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_CSS_IMAGE_WIN32_PRIVATE_H__
#define __GTK_CSS_IMAGE_WIN32_PRIVATE_H__

#include "gtk/gtkcssimageprivate.h"
#include "gtk/gtkwin32themeprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_WIN32           (_gtk_css_image_win32_get_type ())
#define GTK_CSS_IMAGE_WIN32(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_WIN32, GtkCssImageWin32))
#define GTK_CSS_IMAGE_WIN32_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_WIN32, GtkCssImageWin32Class))
#define GTK_IS_CSS_IMAGE_WIN32(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_WIN32))
#define GTK_IS_CSS_IMAGE_WIN32_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_WIN32))
#define GTK_CSS_IMAGE_WIN32_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_WIN32, GtkCssImageWin32Class))

typedef struct _GtkCssImageWin32           GtkCssImageWin32;
typedef struct _GtkCssImageWin32Class      GtkCssImageWin32Class;

struct _GtkCssImageWin32
{
  GtkCssImage parent;

  int part;
  int state;

  double over_alpha;
  int part2;
  int state2;

  gint margins[4];

  HTHEME theme;
};

struct _GtkCssImageWin32Class
{
  GtkCssImageClass parent_class;
};

GType          _gtk_css_image_win32_get_type             (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GTK_CSS_IMAGE_WIN32_PRIVATE_H__ */
