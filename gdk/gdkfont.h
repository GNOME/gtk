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

#if !defined(GDK_DISABLE_DEPRECATED) || defined(GDK_COMPILATION) || defined(GTK_COMPILATION)

#ifndef __GDK_FONT_H__
#define __GDK_FONT_H__

#include <gdk/gdktypes.h>
#include <pango/pango.h>

G_BEGIN_DECLS

#define GDK_TYPE_FONT gdk_font_get_type ()

/* Types of font.
 *   GDK_FONT_FONT: the font is an XFontStruct.
 *   GDK_FONT_FONTSET: the font is an XFontSet used for I18N.
 */
typedef enum
{
  GDK_FONT_FONT,
  GDK_FONT_FONTSET
} GdkFontType;

struct _GdkFont
{
  GdkFontType type;
  gint ascent;
  gint descent;
};

GType    gdk_font_get_type  (void) G_GNUC_CONST;

GdkFont* gdk_font_ref	    (GdkFont        *font);
void	 gdk_font_unref	    (GdkFont        *font);
gint	 gdk_font_id	    (const GdkFont  *font);
gboolean gdk_font_equal	    (const GdkFont  *fonta,
			     const GdkFont  *fontb);

GdkFont *gdk_font_load_for_display             (GdkDisplay           *display,
						const gchar          *font_name);
GdkFont *gdk_fontset_load_for_display          (GdkDisplay           *display,
						const gchar          *fontset_name);
GdkFont *gdk_font_from_description_for_display (GdkDisplay           *display,
						PangoFontDescription *font_desc);

#ifndef GDK_DISABLE_DEPRECATED

#ifndef GDK_MULTIHEAD_SAFE
GdkFont* gdk_font_load             (const gchar          *font_name);
GdkFont* gdk_fontset_load          (const gchar          *fontset_name);
GdkFont* gdk_font_from_description (PangoFontDescription *font_desc);
#endif

GdkDisplay * gdk_font_get_display (GdkFont *font);

#endif /* GDK_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __GDK_FONT_H__ */

#endif /* !GDK_DISABLE_DEPRECATED || GDK_COMPILATION || GTK_COMPILATION */
