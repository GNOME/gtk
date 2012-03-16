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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_STYLE_H__
#define __GTK_STYLE_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtktypes.h>


G_BEGIN_DECLS

#define GTK_TYPE_STYLE              (gtk_style_get_type ())
#define GTK_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_STYLE, GtkStyle))
#define GTK_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_STYLE, GtkStyleClass))
#define GTK_IS_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_STYLE))
#define GTK_IS_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_STYLE))
#define GTK_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_STYLE, GtkStyleClass))

/* Some forward declarations needed to rationalize the header
 * files.
 */
typedef struct _GtkStyleClass  GtkStyleClass;
typedef struct _GtkThemeEngine GtkThemeEngine;
typedef struct _GtkRcProperty  GtkRcProperty;

/**
 * GTK_STYLE_ATTACHED:
 * @style: a #GtkStyle.
 *
 * Returns whether the style is attached to a window.
 */
#define GTK_STYLE_ATTACHED(style)       (GTK_STYLE (style)->attach_count > 0)

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
  GdkColor text_aa[5];          /* Halfway between text/base */

  GdkColor black;
  GdkColor white;
  PangoFontDescription *font_desc;

  gint xthickness;
  gint ythickness;

  cairo_pattern_t *background[5];

  /*< private >*/

  gint attach_count;

  GdkVisual *visual;
  PangoFontDescription *private_font_desc; /* Font description for style->private_font or %NULL */

  /* the RcStyle from which this style was created */
  GtkRcStyle     *rc_style;

  GSList         *styles;         /* of type GtkStyle* */
  GArray         *property_cache;
  GSList         *icon_factories; /* of type GtkIconFactory* */
};

struct _GtkStyleClass
{
  GObjectClass parent_class;

  /* Initialize for a particular visual. style->visual
   * will have been set at this point. Will typically chain
   * to parent.
   */
  void (*realize)               (GtkStyle               *style);

  /* Clean up for a particular visual. Will typically chain
   * to parent.
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
                                 GtkIconSize             size,
                                 GtkWidget              *widget,
                                 const gchar            *detail);

  /* Drawing functions
   */

  void (*draw_hline)            (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x1,
                                 gint                    x2,
                                 gint                    y);
  void (*draw_vline)            (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    y1_,
                                 gint                    y2_,
                                 gint                    x);
  void (*draw_shadow)           (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_arrow)            (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 GtkArrowType            arrow_type,
                                 gboolean                fill,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_diamond)          (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_box)              (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_flat_box)         (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_check)            (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_option)           (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_tab)              (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_shadow_gap)       (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height,
                                 GtkPositionType         gap_side,
                                 gint                    gap_x,
                                 gint                    gap_width);
  void (*draw_box_gap)          (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height,
                                 GtkPositionType         gap_side,
                                 gint                    gap_x,
                                 gint                    gap_width);
  void (*draw_extension)        (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height,
                                 GtkPositionType         gap_side);
  void (*draw_focus)            (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_slider)           (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height,
                                 GtkOrientation          orientation);
  void (*draw_handle)           (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkShadowType           shadow_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height,
                                 GtkOrientation          orientation);

  void (*draw_expander)         (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 GtkExpanderStyle        expander_style);
  void (*draw_layout)           (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 gboolean                use_text,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 gint                    x,
                                 gint                    y,
                                 PangoLayout            *layout);
  void (*draw_resize_grip)      (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 GdkWindowEdge           edge,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);
  void (*draw_spinner)          (GtkStyle               *style,
                                 cairo_t                *cr,
                                 GtkStateType            state_type,
                                 GtkWidget              *widget,
                                 const gchar            *detail,
                                 guint                   step,
                                 gint                    x,
                                 gint                    y,
                                 gint                    width,
                                 gint                    height);

  /* Padding for future expansion */
  void (*_gtk_reserved1)  (void);
  void (*_gtk_reserved2)  (void);
  void (*_gtk_reserved3)  (void);
  void (*_gtk_reserved4)  (void);
  void (*_gtk_reserved5)  (void);
  void (*_gtk_reserved6)  (void);
  void (*_gtk_reserved7)  (void);
  void (*_gtk_reserved8)  (void);
  void (*_gtk_reserved9)  (void);
  void (*_gtk_reserved10) (void);
  void (*_gtk_reserved11) (void);
};

GType     gtk_style_get_type                 (void) G_GNUC_CONST;
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
GtkStyle* gtk_style_new                      (void);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
GtkStyle* gtk_style_copy                     (GtkStyle     *style);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
GtkStyle* gtk_style_attach                   (GtkStyle     *style,
                                              GdkWindow    *window);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void      gtk_style_detach                   (GtkStyle     *style);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void      gtk_style_set_background           (GtkStyle     *style,
                                              GdkWindow    *window,
                                              GtkStateType  state_type);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void      gtk_style_apply_default_background (GtkStyle     *style,
                                              cairo_t      *cr,
                                              GdkWindow    *window,
                                              GtkStateType  state_type,
                                              gint          x,
                                              gint          y,
                                              gint          width,
                                              gint          height);

GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
GtkIconSet* gtk_style_lookup_icon_set        (GtkStyle     *style,
                                              const gchar  *stock_id);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
gboolean    gtk_style_lookup_color           (GtkStyle     *style,
                                              const gchar  *color_name,
                                              GdkColor     *color);

GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
GdkPixbuf*  gtk_style_render_icon     (GtkStyle            *style,
                                       const GtkIconSource *source,
                                       GtkTextDirection     direction,
                                       GtkStateType         state,
                                       GtkIconSize          size,
                                       GtkWidget           *widget,
                                       const gchar         *detail);

GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_hline             (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x1,
                                  gint                x2,
                                  gint                y);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_vline             (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                y1_,
                                  gint                y2_,
                                  gint                x);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_shadow            (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_arrow             (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  GtkArrowType        arrow_type,
                                  gboolean            fill,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_diamond           (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_box               (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_flat_box          (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_check             (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_option            (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_tab               (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_shadow_gap        (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height,
                                  GtkPositionType     gap_side,
                                  gint                gap_x,
                                  gint                gap_width);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_box_gap           (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height,
                                  GtkPositionType     gap_side,
                                  gint                gap_x,
                                  gint                gap_width);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_extension         (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height,
                                  GtkPositionType     gap_side);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_focus             (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_slider            (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height,
                                  GtkOrientation      orientation);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_handle            (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkShadowType       shadow_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height,
                                  GtkOrientation      orientation);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_expander          (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  GtkExpanderStyle    expander_style);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_layout            (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  gboolean            use_text,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  gint                x,
                                  gint                y,
                                  PangoLayout        *layout);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_resize_grip       (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  GdkWindowEdge       edge,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_paint_spinner           (GtkStyle           *style,
                                  cairo_t            *cr,
                                  GtkStateType        state_type,
                                  GtkWidget          *widget,
                                  const gchar        *detail,
                                  guint               step,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height);

GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_style_get_style_property (GtkStyle    *style,
                                   GType        widget_type,
                                   const gchar *property_name,
                                   GValue      *value);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_style_get_valist         (GtkStyle    *style,
                                   GType        widget_type,
                                   const gchar *first_property_name,
                                   va_list      var_args);
GDK_DEPRECATED_IN_3_0_FOR(GtkStyleContext)
void gtk_style_get                (GtkStyle    *style,
                                   GType        widget_type,
                                   const gchar *first_property_name,
                                   ...) G_GNUC_NULL_TERMINATED;


/* --- private API --- */
GtkStyle*     _gtk_style_new_for_path     (GdkScreen          *screen,
                                           GtkWidgetPath      *path);
void          _gtk_style_shade            (const GdkColor     *a,
                                           GdkColor           *b,
                                           gdouble             k);

gboolean    gtk_style_has_context         (GtkStyle *style);

void        gtk_widget_style_attach       (GtkWidget     *widget);
gboolean    gtk_widget_has_rc_style       (GtkWidget            *widget);
void        gtk_widget_set_style          (GtkWidget            *widget,
                                           GtkStyle             *style);
void        gtk_widget_ensure_style       (GtkWidget            *widget);
GtkStyle *  gtk_widget_get_style          (GtkWidget            *widget);
void        gtk_widget_modify_style       (GtkWidget            *widget,
                                           GtkRcStyle           *style);
GtkRcStyle *gtk_widget_get_modifier_style (GtkWidget            *widget);
void        gtk_widget_modify_fg          (GtkWidget            *widget,
                                           GtkStateType          state,
                                           const GdkColor       *color);
void        gtk_widget_modify_bg          (GtkWidget            *widget,
                                           GtkStateType          state,
                                           const GdkColor       *color);
void        gtk_widget_modify_text        (GtkWidget            *widget,
                                           GtkStateType          state,
                                           const GdkColor       *color);
void        gtk_widget_modify_base        (GtkWidget            *widget,
                                           GtkStateType          state,
                                           const GdkColor       *color);
void        gtk_widget_modify_cursor      (GtkWidget            *widget,
                                           const GdkColor       *primary,
                                           const GdkColor       *secondary);
void        gtk_widget_modify_font        (GtkWidget            *widget,
                                           PangoFontDescription *font_desc);
void       gtk_widget_reset_rc_styles     (GtkWidget      *widget);
GtkStyle*  gtk_widget_get_default_style   (void);
void       gtk_widget_path                (GtkWidget *widget,
                                           guint     *path_length,
                                           gchar    **path,
                                           gchar    **path_reversed);
void       gtk_widget_class_path          (GtkWidget *widget,
                                           guint     *path_length,
                                           gchar    **path,
                                           gchar    **path_reversed);
GdkPixbuf *gtk_widget_render_icon         (GtkWidget   *widget,
                                           const gchar *stock_id,
                                           GtkIconSize  size,
                                           const gchar *detail);

G_END_DECLS

#endif /* __GTK_STYLE_H__ */
