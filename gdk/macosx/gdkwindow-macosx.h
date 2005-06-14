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

#ifndef __GDK_WINDOW_MACOSX_H__
#define __GDK_WINDOW_MACOSX_H__

#include <gdk/macosx/gdkdrawable-macosx.h>

G_BEGIN_DECLS

typedef struct _GdkToplevelMacOSX GdkToplevelMacOSX;
typedef struct _GdkWindowImplMacOSX GdkWindowImplMacOSX;
typedef struct _GdkWindowImplMacOSXClass GdkWindowImplMacOSXClass;
typedef struct _GdkMacPositionInfo GdkMacPositionInfo;

struct _GdkMacPositionInfo
{
  gint x;
  gint y;
  gint width;
  gint height;
  gint x_offset;		/* Offsets to add to X coordinates within window */
  gint y_offset;		/*   to get GDK coodinates within window */
  guint big : 1;
  guint mapped : 1;
  guint no_bg : 1;	        /* Set when the window background is temporarily
				 * unset during resizing and scaling */
  GdkRectangle clip_rect;	/* visible rectangle of window */
};


/* Window implementation for MacOSX
 */

#define GDK_TYPE_WINDOW_IMPL_MACOSX              (gdk_window_impl_macosx_get_type ())
#define GDK_WINDOW_IMPL_MACOSX(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_MACOSX, GdkWindowImplMacOSX))
#define GDK_WINDOW_IMPL_MACOSX_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_MACOSX, GdkWindowImplMacOSXClass))
#define GDK_IS_WINDOW_IMPL_MACOSX(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_MACOSX))
#define GDK_IS_WINDOW_IMPL_MACOSX_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_MACOSX))
#define GDK_WINDOW_IMPL_MACOSX_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_MACOSX, GdkWindowImplMacOSXClass))

@class NSWindow;
@class NSView;

struct _GdkWindowImplMacOSX
{
	GdkDrawableImplMacOSX parent_instance;
	
	gint width;
	gint height;
	
	
	GdkMacPositionInfo position_info;
	GdkToplevelMacOSX *toplevel;	/* Toplevel-specific information */
	gint8 toplevel_window_type;
	NSView *v;
};
 
struct _GdkWindowImplMacOSXClass 
{
	GdkDrawableImplMacOSXClass parent_class;
};

struct _GdkToplevelMacOSX
{
	NSWindow *w;
};

GType gdk_window_impl_macosx_get_type (void);

void             gdk_macosx_window_set_user_time (GdkWindow *window,
                                               guint32    timestamp);

GdkToplevelMacOSX *_gdk_macosx_window_get_toplevel  (GdkWindow *window);
void		_gdk_macosx_window_tmp_unset_bg  (GdkWindow *window,
					       gboolean   recurse);
void            _gdk_macosx_window_tmp_reset_bg  (GdkWindow *window,
					       gboolean   recurse);


G_END_DECLS

#endif /* __GDK_WINDOW_MACOSX_H__ */
