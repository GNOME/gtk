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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __GTK_STYLE_H__
#define __GTK_STYLE_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <pango/pango.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkStyle       GtkStyle;
typedef struct _GtkStyleClass  GtkStyleClass;

#define GTK_TYPE_STYLE              (gtk_style_get_type ())
#define GTK_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_STYLE, GtkStyle))
#define GTK_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_STYLE, GtkStyleClass))
#define GTK_IS_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_STYLE))
#define GTK_IS_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_STYLE))
#define GTK_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_STYLE, GtkStyleClass))

/* Some forward declarations needed to rationalize the header
 * files.
 */
typedef struct _GtkThemeEngine GtkThemeEngine;
typedef struct _GtkRcStyle     GtkRcStyle;
typedef struct _GtkIconSet     GtkIconSet;
typedef struct _GtkIconSource  GtkIconSource;

/* We make this forward declaration here, since we pass
 * GtkWidgt's to the draw functions.
 */
typedef struct _GtkWidget      GtkWidget;

/* This is used for having dynamic style changing stuff
 * fg, bg, light, dark, mid, text, base
 */
#define GTK_STYLE_NUM_STYLECOLORS()	(7 * 5)

#define GTK_STYLE_ATTACHED(style)	(((GtkStyle*) (style))->attach_count > 0)

struct _GtkStyle
{
  GObject parent_instance;

  /*< public >*/
  
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
  PangoFontDescription *font_desc;
  
  gint xthickness;
  gint ythickness;
  
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

  /*< private >*/
  
  gint attach_count;
  
  gint depth;
  GdkColormap *colormap;
  
  GtkRcStyle	 *rc_style;	/* the Rc style from which this style
				 * was created
				 */
  GSList	 *styles;

  GSList         *icon_factories;
};

struct _GtkStyleClass
{
  GObjectClass parent_class;

  /* Initialize for a particular colormap/depth
   * combination. style->colormap/style->depth will have
   * been set at this point. Will typically chain to parent.
   */
  void (*realize)               (GtkStyle               *style);

  /* Clean up for a particular colormap/depth combination. Will
   * typically chain to parent.
   */
  void (*unrealize)             (GtkStyle               *style);

  /* Make style an exact duplicate of src.
   */
  void (*copy)                  (GtkStyle               *style,
				 GtkStyle               *src);

  /* Create an empty style of the same type as this style.
   * The default implementation, which does
   * g_object_new (G_OBJECT_TYPE (style), NULL);
   * should work in most cases.
   */
  GtkStyle *(*clone)             (GtkStyle               *style);

  /* Initialize the GtkStyle with the values in the GtkRcStyle.
   * should chain to the parent implementation.
   */
  void     (*init_from_rc)      (GtkStyle               *style,
				 GtkRcStyle             *rc_style);

  void (*set_background)        (GtkStyle               *style,
				 GdkWindow              *window,
				 GtkStateType            state_type);


  GdkPixbuf * (* render_icon)   (GtkStyle               *style,
                                 const GtkIconSource    *source,
                                 GtkTextDirection        direction,
                                 GtkStateType            state,
                                 const gchar            *size,
                                 GtkWidget              *widget,
                                 const gchar            *detail);
  
  /* Drawing functions
   */
  
  void (*draw_hline)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x1,
				 gint			 x2,
				 gint			 y);
  void (*draw_vline)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 y1,
				 gint			 y2,
				 gint			 x);
  void (*draw_shadow)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height);
  void (*draw_polygon)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 GdkPoint		*point,
				 gint			 npoints,
				 gboolean		 fill);
  void (*draw_arrow)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 GtkArrowType		 arrow_type,
				 gboolean		 fill,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height);
  void (*draw_diamond)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height);
  void (*draw_string)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 const gchar		*string);
  void (*draw_box)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height);
  void (*draw_flat_box)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height);
  void (*draw_check)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height);
  void (*draw_option)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height);
  void (*draw_tab)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height); 
  void (*draw_shadow_gap)	(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height,
				 GtkPositionType	 gap_side,
				 gint			 gap_x,
				 gint			 gap_width);
  void (*draw_box_gap)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height,
				 GtkPositionType	 gap_side,
				 gint			 gap_x,
				 gint			 gap_width);
  void (*draw_extension)	(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height,
				 GtkPositionType	 gap_side);
  void (*draw_focus)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height);
  void (*draw_slider)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height,
				 GtkOrientation		 orientation);
  void (*draw_handle)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GtkShadowType		 shadow_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
				 gint			 width,
				 gint			 height,
				 GtkOrientation		 orientation);

  void (*draw_expander)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
                                 gboolean                is_open);
  void (*draw_layout)		(GtkStyle		*style,
				 GdkWindow		*window,
				 GtkStateType		 state_type,
				 GdkRectangle		*area,
				 GtkWidget		*widget,
				 const gchar		*detail,
				 gint			 x,
				 gint			 y,
                                 PangoLayout            *layout);
  
};

GType     gtk_style_get_type                 (void) G_GNUC_CONST;
GtkStyle* gtk_style_new			     (void);
GtkStyle* gtk_style_copy		     (GtkStyle	    *style);
GtkStyle* gtk_style_attach		     (GtkStyle	    *style,
					      GdkWindow	    *window);
void	  gtk_style_detach		     (GtkStyle	   *style);
GtkStyle* gtk_style_ref			     (GtkStyle	   *style);
void	  gtk_style_unref		     (GtkStyle	   *style);
void	  gtk_style_set_background	     (GtkStyle	   *style,
					      GdkWindow	   *window,
					      GtkStateType  state_type);
void	  gtk_style_apply_default_background (GtkStyle	   *style,
					      GdkWindow	   *window,
					      gboolean	    set_bg,
					      GtkStateType  state_type, 
					      GdkRectangle *area, 
					      gint	    x, 
					      gint	    y, 
					      gint	    width, 
					      gint	    height);

GtkIconSet* gtk_style_lookup_icon_set (GtkStyle            *style,
                                       const gchar         *stock_id);
GdkPixbuf * gtk_style_render_icon     (GtkStyle            *style,
                                       const GtkIconSource *source,
                                       GtkTextDirection     direction,
                                       GtkStateType         state,
                                       const gchar *        size,
                                       GtkWidget           *widget,
                                       const gchar         *detail);
void gtk_draw_hline      (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  gint             x1,
			  gint             x2,
			  gint             y);
void gtk_draw_vline      (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  gint             y1,
			  gint             y2,
			  gint             x);
void gtk_draw_shadow     (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_polygon    (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  GdkPoint        *points,
			  gint             npoints,
			  gboolean         fill);
void gtk_draw_arrow      (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  GtkArrowType     arrow_type,
			  gboolean         fill,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_diamond    (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_string     (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  gint             x,
			  gint             y,
			  const gchar     *string);
void gtk_draw_box        (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_flat_box   (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_check      (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_option     (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_tab        (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_shadow_gap (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height,
			  GtkPositionType  gap_side,
			  gint             gap_x,
			  gint             gap_width);
void gtk_draw_box_gap    (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height,
			  GtkPositionType  gap_side,
			  gint             gap_x,
			  gint             gap_width);
void gtk_draw_extension  (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height,
			  GtkPositionType  gap_side);
void gtk_draw_focus      (GtkStyle        *style,
			  GdkWindow       *window,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height);
void gtk_draw_slider     (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height,
			  GtkOrientation   orientation);
void gtk_draw_handle     (GtkStyle        *style,
			  GdkWindow       *window,
			  GtkStateType     state_type,
			  GtkShadowType    shadow_type,
			  gint             x,
			  gint             y,
			  gint             width,
			  gint             height,
			  GtkOrientation   orientation);
void gtk_draw_expander   (GtkStyle        *style,
                          GdkWindow       *window,
                          GtkStateType     state_type,
                          gint             x,
                          gint             y,
                          gboolean         is_open);
void gtk_draw_layout     (GtkStyle        *style,
                          GdkWindow       *window,
                          GtkStateType     state_type,
                          gint             x,
                          gint             y,
                          PangoLayout     *layout);

void gtk_paint_hline      (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x1,
			   gint             x2,
			   gint             y);
void gtk_paint_vline      (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             y1,
			   gint             y2,
			   gint             x);
void gtk_paint_shadow     (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_polygon    (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   GdkPoint        *points,
			   gint             npoints,
			   gboolean         fill);
void gtk_paint_arrow      (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   GtkArrowType     arrow_type,
			   gboolean         fill,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_diamond    (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_string     (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   const gchar     *string);
void gtk_paint_box        (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_flat_box   (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_check      (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_option     (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_tab        (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_shadow_gap (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   gchar           *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height,
			   GtkPositionType  gap_side,
			   gint             gap_x,
			   gint             gap_width);
void gtk_paint_box_gap    (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   gchar           *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height,
			   GtkPositionType  gap_side,
			   gint             gap_x,
			   gint             gap_width);
void gtk_paint_extension  (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   gchar           *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height,
			   GtkPositionType  gap_side);
void gtk_paint_focus      (GtkStyle        *style,
			   GdkWindow       *window,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height);
void gtk_paint_slider     (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height,
			   GtkOrientation   orientation);
void gtk_paint_handle     (GtkStyle        *style,
			   GdkWindow       *window,
			   GtkStateType     state_type,
			   GtkShadowType    shadow_type,
			   GdkRectangle    *area,
			   GtkWidget       *widget,
			   const gchar     *detail,
			   gint             x,
			   gint             y,
			   gint             width,
			   gint             height,
			   GtkOrientation   orientation);
void gtk_paint_expander   (GtkStyle        *style,
                           GdkWindow       *window,
                           GtkStateType     state_type,
                           GdkRectangle    *area,
                           GtkWidget       *widget,
                           const gchar     *detail,
                           gint             x,
                           gint             y,
                           gboolean         is_open);
void gtk_paint_layout     (GtkStyle        *style,
                           GdkWindow       *window,
                           GtkStateType     state_type,
                           GdkRectangle    *area,
                           GtkWidget       *widget,
                           const gchar     *detail,
                           gint             x,
                           gint             y,
                           PangoLayout     *layout);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_STYLE_H__ */
