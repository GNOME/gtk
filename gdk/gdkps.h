/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GDK_PS_H__
#define __GDK_PS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*typedef struct _GdkPsDrawable GdkPsDrawable;*/

typedef struct {
	gint page;
	gint fd;
	gint width;
	gint height;
	GString *sbuf;
	gint xoff;
	gint yoff;
	gint intile;
	gint inframe;
	GdkFont* font;
	GdkColor fg;
	GdkColor bg;
	GdkCapStyle cap_style;
	GdkJoinStyle join_style;
	gint line_width;
	gint valid;
	gint valid_fg;
	gint nrects;
	GdkRectangle *rects;
	gint clipped;
} GdkPsDrawable;


GdkDrawable* gdk_ps_drawable_new (gint   fd, 
				  gchar *title, 
				  gchar *author);

void	     gdk_ps_drawable_put_data (GdkDrawable *d,
				       gchar       *data,
				       guint        len);

void	     gdk_ps_drawable_end (GdkDrawable *d);
void         gdk_ps_drawable_page_start (GdkDrawable *d,
					 gint         orientation,
					 gint         count,
					 gint         plex,
					 gint         resolution,
					 gint         width,
					 gint         height);
void         gdk_ps_drawable_page_end   (GdkDrawable *d);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GDK_PS_H__ */
