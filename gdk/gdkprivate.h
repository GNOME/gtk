/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_PRIVATE_H__
#define __GDK_PRIVATE_H__

#include <gdk/gdktypes.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkfont.h>
#include <gdk/gdkgc.h>
#include <gdk/gdkim.h>
#include <gdk/gdkimage.h>
#include <gdk/gdkregion.h>
#include <gdk/gdkvisual.h>
#include <gdk/gdkwindow.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GDK_PARENT_RELATIVE_BG ((GdkPixmap *)1L)
#define GDK_NO_BG ((GdkPixmap *)2L)

#define GDK_DRAWABLE_TYPE(d) (((GdkDrawablePrivate *)d)->window_type)
#define GDK_IS_WINDOW(d) (GDK_DRAWABLE_TYPE(d) <= GDK_WINDOW_TEMP || \
                          GDK_DRAWABLE_TYPE(d) == GDK_WINDOW_FOREIGN)
#define GDK_IS_PIXMAP(d) (GDK_DRAWABLE_TYPE(d) == GDK_DRAWABLE_PIXMAP)
#define GDK_DRAWABLE_DESTROYED(d) (((GdkDrawablePrivate *)d)->destroyed)
#define GDK_DRAWABLE_P(d) ((GdkDrawablePrivate*)d)
#define GDK_WINDOW_P(d) ((GdkWindowPrivate*)d)
#define GDK_GC_P(d) ((GdkGCPrivate*)d)

typedef struct _GdkDrawablePrivate     GdkDrawablePrivate;
typedef struct _GdkWindowPrivate       GdkWindowPrivate;
typedef struct _GdkImageClass	       GdkImageClass;
typedef struct _GdkImagePrivate	       GdkImagePrivate;
typedef struct _GdkGCPrivate	       GdkGCPrivate;
typedef struct _GdkColormapPrivate     GdkColormapPrivate;
typedef struct _GdkColorInfo           GdkColorInfo;
typedef struct _GdkFontPrivate	       GdkFontPrivate;
typedef struct _GdkEventFilter	       GdkEventFilter;
typedef struct _GdkClientFilter	       GdkClientFilter;

struct _GdkDrawablePrivate
{
  GdkDrawable drawable;
  GdkDrawableClass *klass;
  gpointer klass_data;

  guint ref_count;

  gint width;
  gint height;

  GdkColormap *colormap;

  guint8 window_type;
  guint8 depth;
  
  guint destroyed : 2;
};

struct _GdkWindowPrivate
{
  GdkDrawablePrivate drawable;
  
  GdkWindow *parent;
  gint x;
  gint y;
  guint8 resize_count;
  guint mapped : 1;
  guint guffaw_gravity : 1;
  guint input_only : 1;

  gint extension_events;

  GList *filters;
  GList *children;

  GdkColor bg_color;
  GdkPixmap *bg_pixmap;
  
  GSList *paint_stack;
  
  GdkRegion *update_area;
  guint update_freeze_count;
};

struct _GdkImageClass 
{
  void (*destroy)   (GdkImage    *image);
  void (*image_put) (GdkImage	 *image,
		     GdkDrawable *window,
		     GdkGC	 *gc,
		     gint	  xsrc,
		     gint	  ysrc,
		     gint	  xdest,
		     gint	  ydest,
		     gint	  width,
		     gint	  height);
};

struct _GdkImagePrivate
{
  GdkImage image;

  guint ref_count;
  GdkImageClass *klass;
};

struct _GdkFontPrivate
{
  GdkFont font;
  guint ref_count;
};

struct _GdkGCPrivate
{
  guint ref_count;
  GdkGCClass *klass;
  gpointer klass_data;

  gint clip_x_origin;
  gint clip_y_origin;
  gint ts_x_origin;
  gint ts_y_origin;
};

typedef enum {
  GDK_COLOR_WRITEABLE = 1 << 0
} GdkColorInfoFlags;

struct _GdkColorInfo
{
  GdkColorInfoFlags flags;
  guint ref_count;
};

struct _GdkColormapPrivate
{
  GdkColormap colormap;
  GdkVisual *visual;

  guint ref_count;
};

struct _GdkEventFilter {
  GdkFilterFunc function;
  gpointer data;
};

struct _GdkClientFilter {
  GdkAtom       type;
  GdkFilterFunc function;
  gpointer      data;
};

void gdk_window_destroy_notify	     (GdkWindow *window);

GDKVAR GdkWindow  	*gdk_parent_root;
GDKVAR gint		 gdk_error_code;
GDKVAR gint		 gdk_error_warnings;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GDK_PRIVATE_H__ */





