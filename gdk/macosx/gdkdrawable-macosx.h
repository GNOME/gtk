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

#ifndef __GDK_DRAWABLE_MACOSX_H__
#define __GDK_DRAWABLE_MACOSX_H__

#include <config.h>
#include <gdk/gdkdrawable.h>

#include <ApplicationServices/ApplicationServices.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
	
/* Drawable implementation for MacOSX
 */
	
typedef enum
{
	GDK_MACOSX_FORMAT_NONE,
	GDK_MACOSX_FORMAT_EXACT_MASK,
	GDK_MACOSX_FORMAT_ARGB_MASK,
	GDK_MACOSX_FORMAT_ARGB
} GdkMacOSXFormatType;

typedef struct _GdkDrawableImplMacOSX GdkDrawableImplMacOSX;
typedef struct _GdkDrawableImplMacOSXClass GdkDrawableImplMacOSXClass;

#define GDK_TYPE_DRAWABLE_IMPL_MACOSX              (_gdk_drawable_impl_macosx_get_type ())
#define GDK_DRAWABLE_IMPL_MACOSX(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAWABLE_IMPL_MACOSX, GdkDrawableImplMacOSX))
#define GDK_DRAWABLE_IMPL_MACOSX_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAWABLE_IMPL_MACOSX, GdkDrawableImplMacOSXClass))
#define GDK_IS_DRAWABLE_IMPL_MACOSX(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAWABLE_IMPL_MACOSX))
#define GDK_IS_DRAWABLE_IMPL_MACOSX_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAWABLE_IMPL_MACOSX))
#define GDK_DRAWABLE_IMPL_MACOSX_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAWABLE_IMPL_MACOSX, GdkDrawableImplMacOSXClass))


struct _GdkDrawableImplMacOSX
{
	GdkDrawable parent_instance;
	
	GdkDrawable *wrapper;
	
	GdkColormap *colormap;
	
	CGContextRef cg;
};
 
struct _GdkDrawableImplMacOSXClass 
{
	GdkDrawableClass parent_class;
};

GType _gdk_drawable_impl_macosx_get_type (void);

void  _gdk_macosx_convert_to_format      (guchar           *src_buf,
                                       gint              src_rowstride,
                                       guchar           *dest_buf,
                                       gint              dest_rowstride,
                                       GdkMacOSXFormatType  dest_format,
                                       GdkByteOrder      dest_byteorder,
                                       gint              width,
                                       gint              height);
/* Note that the following take GdkDrawableImplMacOSX, not the wrapper drawable */
/*
void _gdk_macosx_drawable_draw_xtrapezoids (GdkDrawable  *drawable,
					 GdkGC        *gc,
					 XTrapezoid   *xtrapezoids,
					 int           n_trapezoids);
void _gdk_macosx_drawable_draw_xft_glyphs  (GdkDrawable  *drawable,
					 GdkGC        *gc,
					 XftFont      *xft_font,
					 XftGlyphSpec *glyphs,
					 gint          n_glyphs);
*/
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_DRAWABLE_MacOSX_H__ */
