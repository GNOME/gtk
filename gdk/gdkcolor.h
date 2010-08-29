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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_COLOR_H__
#define __GDK_COLOR_H__

#include <cairo.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

/* The color type.
 *   A color consists of red, green and blue values in the
 *    range 0-65535 and a pixel value. The pixel value is unused.
 */
struct _GdkColor
{
  guint32 pixel;
  guint16 red;
  guint16 green;
  guint16 blue;
};

#define GDK_TYPE_COLOR                 (gdk_color_get_type ())

GdkColor *gdk_color_copy      (const GdkColor *color);
void      gdk_color_free      (GdkColor       *color);
gboolean  gdk_color_parse     (const gchar    *spec,
			       GdkColor       *color);
guint     gdk_color_hash      (const GdkColor *colora);
gboolean  gdk_color_equal     (const GdkColor *colora,
			       const GdkColor *colorb);
gchar *   gdk_color_to_string (const GdkColor *color);

GType     gdk_color_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GDK_COLOR_H__ */
