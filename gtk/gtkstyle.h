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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_STYLE_H__
#define __GTK_STYLE_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct _GtkStyle       GtkStyle;
typedef struct _GtkStyleClass  GtkStyleClass;

/* This is used for having dynamic style changing stuff */
/* fg, bg, light, dark, mid, text, base */
#define GTK_STYLE_NUM_STYLECOLORS() 7*5

#define	GTK_STYLE_ATTACHED(style)	(((GtkStyle*)(style))->attach_count > 0)

struct _GtkStyle
{
  GdkColor fg[5];
  GdkColor bg[5];
  GdkColor light[5];
  GdkColor dark[5];
  GdkColor mid[5];
  GdkColor text[5];
  GdkColor base[5];

  GdkColor black;
  GdkColor white;
  GdkFont *font;

  GdkGC *fg_gc[5];
  GdkGC *bg_gc[5];
  GdkGC *light_gc[5];
  GdkGC *dark_gc[5];
  GdkGC *mid_gc[5];
  GdkGC *text_gc[5];
  GdkGC *base_gc[5];
  GdkGC *black_gc;
  GdkGC *white_gc;

  GdkPixmap *bg_pixmap[5];

  gint ref_count;
  gint attach_count;

  gint depth;
  GdkColormap *colormap;

  GtkStyleClass *klass;
};

struct _GtkStyleClass
{
  gint xthickness;
  gint ythickness;

  void (*draw_hline)   (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			gint	       x1,
			gint	       x2,
			gint	       y);
  void (*draw_vline)   (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			gint	       y1,
			gint	       y2,
			gint	       x);
  void (*draw_shadow)  (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			GtkShadowType  shadow_type,
			gint	       x,
			gint	       y,
			gint	       width,
			gint	       height);
  void (*draw_polygon) (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			GtkShadowType  shadow_type,
			GdkPoint      *point,
			gint	       npoints,
			gint	       fill);
  void (*draw_arrow)   (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			GtkShadowType  shadow_type,
			GtkArrowType   arrow_type,
			gint	       fill,
			gint	       x,
			gint	       y,
			gint	       width,
			gint	       height);
  void (*draw_diamond) (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			GtkShadowType  shadow_type,
			gint	       x,
			gint	       y,
			gint	       width,
			gint	       height);
  void (*draw_oval)    (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			GtkShadowType  shadow_type,
			gint	       x,
			gint	       y,
			gint	       width,
			gint	       height);
  void (*draw_string)  (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			gint	       x,
			gint	       y,
			const gchar   *string);
};


GtkStyle* gtk_style_new		   (void);
GtkStyle* gtk_style_copy	   (GtkStyle	 *style);
GtkStyle* gtk_style_attach	   (GtkStyle	 *style,
				    GdkWindow	 *window);
void	  gtk_style_detach	   (GtkStyle	 *style);
GtkStyle* gtk_style_ref		   (GtkStyle	 *style);
void	  gtk_style_unref	   (GtkStyle	 *style);
void	  gtk_style_set_background (GtkStyle	 *style,
				    GdkWindow	 *window,
				    GtkStateType  state_type);


void gtk_draw_hline   (GtkStyle	     *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       gint	      x1,
		       gint	      x2,
		       gint	      y);
void gtk_draw_vline   (GtkStyle	     *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       gint	      y1,
		       gint	      y2,
		       gint	      x);
void gtk_draw_shadow  (GtkStyle	     *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       GtkShadowType  shadow_type,
		       gint	      x,
		       gint	      y,
		       gint	      width,
		       gint	      height);
void gtk_draw_polygon (GtkStyle	     *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       GtkShadowType  shadow_type,
		       GdkPoint	     *points,
		       gint	      npoints,
		       gint	      fill);
void gtk_draw_arrow   (GtkStyle	     *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       GtkShadowType  shadow_type,
		       GtkArrowType   arrow_type,
		       gint	      fill,
		       gint	      x,
		       gint	      y,
		       gint	      width,
		       gint	      height);
void gtk_draw_diamond (GtkStyle	     *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       GtkShadowType  shadow_type,
		       gint	      x,
		       gint	      y,
		       gint	      width,
		       gint	      height);
void gtk_draw_oval    (GtkStyle	     *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       GtkShadowType  shadow_type,
		       gint	      x,
		       gint	      y,
		       gint	      width,
		       gint	      height);
void gtk_draw_string  (GtkStyle	     *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       gint	      x,
		       gint	      y,
		       const gchar   *string);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_STYLE_H__ */
