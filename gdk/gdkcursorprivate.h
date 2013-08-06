/* GDK - The GIMP Drawing Kit
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

#ifndef __GDK_CURSOR_PRIVATE_H__
#define __GDK_CURSOR_PRIVATE_H__

#include <gdk/gdkcursor.h>

G_BEGIN_DECLS

#define GDK_CURSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CURSOR, GdkCursorClass))
#define GDK_IS_CURSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CURSOR))
#define GDK_CURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CURSOR, GdkCursorClass))

typedef struct _GdkCursorClass GdkCursorClass;

struct _GdkCursor
{
  GObject parent_instance;

  GdkDisplay *display;
  GdkCursorType type;
};

struct _GdkCursorClass
{
  GObjectClass parent_class;

  cairo_surface_t * (* get_surface) (GdkCursor *cursor,
				     gdouble   *x_hot,
				     gdouble   *y_hot);
};

G_END_DECLS

#endif /* __GDK_CURSOR_PRIVATE_H__ */
