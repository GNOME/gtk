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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef GTK_TEXT_DISPLAY_H
#define GTK_TEXT_DISPLAY_H

#include <gtk/gtktextlayout.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* A semi-public header intended for use by code that also
 * uses GtkTextLayout
 */

/* The drawable should be pre-initialized to your preferred background.
 * widget            - Widget to grab some style info from
 * drawable          - Drawable to render to
 * x_offset/y_offset - Position of the drawable in layout coordinates
 * x/y/width/height  - Region of the layout to render. x,y must be inside
 *                     the drawable.
 */
void gtk_text_layout_draw (GtkTextLayout	*layout,
			   GtkWidget		*widget, 
			   GdkDrawable		*drawable,
			   gint			 x_offset,
			   gint			 y_offset,
			   gint			 x,
			   gint			 y,
			   gint			 width,
			   gint			 height);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* GTK_TEXT_DISPLAY_H */
