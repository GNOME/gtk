/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_STYLE_CONTEXT_H__
#define __GTK_STYLE_CONTEXT_H__

#include <glib-object.h>
#include <gtk/gtkstyleprovider.h>
#include <gtk/gtkwidgetpath.h>

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_CONTEXT         (gtk_style_context_get_type ())
#define GTK_STYLE_CONTEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContext))
#define GTK_STYLE_CONTEXT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextClass))
#define GTK_IS_STYLE_CONTEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STYLE_CONTEXT))
#define GTK_IS_STYLE_CONTEXT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_STYLE_CONTEXT))
#define GTK_STYLE_CONTEXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextClass))

typedef struct GtkStyleContext GtkStyleContext;
typedef struct GtkStyleContextClass GtkStyleContextClass;

struct GtkStyleContext
{
  GObject parent_object;
  gpointer priv;
};

struct GtkStyleContextClass
{
  GObjectClass parent_class;
};

GType gtk_style_context_get_type (void) G_GNUC_CONST;

void gtk_style_context_add_provider    (GtkStyleContext  *context,
                                        GtkStyleProvider *provider,
                                        guint             priority);

void gtk_style_context_remove_provider (GtkStyleContext  *context,
                                        GtkStyleProvider *provider);

void gtk_style_context_save    (GtkStyleContext *context);
void gtk_style_context_restore (GtkStyleContext *context);

void gtk_style_context_get_property (GtkStyleContext *context,
                                     const gchar     *property,
                                     GtkStateFlags    state,
                                     GValue          *value);
void gtk_style_context_get_valist   (GtkStyleContext *context,
                                     GtkStateFlags    state,
                                     va_list          args);
void gtk_style_context_get          (GtkStyleContext *context,
                                     GtkStateFlags    state,
                                     ...) G_GNUC_NULL_TERMINATED;

void          gtk_style_context_set_state    (GtkStyleContext *context,
                                              GtkStateFlags    flags);
GtkStateFlags gtk_style_context_get_state    (GtkStyleContext *context);

gboolean      gtk_style_context_is_state_set (GtkStyleContext *context,
                                              GtkStateType     state,
                                              gdouble         *progress);

void          gtk_style_context_set_path     (GtkStyleContext *context,
                                              GtkWidgetPath   *path);
G_CONST_RETURN GtkWidgetPath * gtk_style_context_get_path (GtkStyleContext *context);

GList *  gtk_style_context_list_classes (GtkStyleContext *context);

void     gtk_style_context_set_class   (GtkStyleContext *context,
                                        const gchar     *class_name);
void     gtk_style_context_unset_class (GtkStyleContext *context,
                                        const gchar     *class_name);
gboolean gtk_style_context_has_class   (GtkStyleContext *context,
                                        const gchar     *class_name);

GList *  gtk_style_context_list_regions (GtkStyleContext *context);

void     gtk_style_context_set_region   (GtkStyleContext    *context,
                                         const gchar        *region_name,
                                         GtkRegionFlags      flags);
void     gtk_style_context_unset_region (GtkStyleContext    *context,
                                         const gchar        *region_name);
gboolean gtk_style_context_has_region   (GtkStyleContext    *context,
                                         const gchar        *region_name,
                                         GtkRegionFlags     *flags_return);

void gtk_style_context_get_style_property (GtkStyleContext *context,
                                           const gchar     *property_name,
                                           GValue          *value);
void gtk_style_context_get_style_valist   (GtkStyleContext *context,
                                           va_list          args);
void gtk_style_context_get_style          (GtkStyleContext *context,
                                           ...);

GtkIconSet * gtk_style_context_lookup_icon_set (GtkStyleContext *context,
						const gchar     *stock_id);

void        gtk_style_context_set_screen (GtkStyleContext *context,
                                          GdkScreen       *screen);
GdkScreen * gtk_style_context_get_screen (GtkStyleContext *context);

void             gtk_style_context_set_direction (GtkStyleContext  *context,
                                                  GtkTextDirection  direction);
GtkTextDirection gtk_style_context_get_direction (GtkStyleContext  *context);

void             gtk_style_context_set_junction_sides (GtkStyleContext  *context,
						       GtkJunctionSides  sides);
GtkJunctionSides gtk_style_context_get_junction_sides (GtkStyleContext  *context);

gboolean gtk_style_context_lookup_color (GtkStyleContext *context,
                                         const gchar     *color_name,
                                         GdkColor        *color);

void  gtk_style_context_notify_state_change (GtkStyleContext *context,
                                             GdkWindow       *window,
                                             gpointer         region_id,
                                             GtkStateType     state,
                                             gboolean         state_value);
void gtk_style_context_push_animatable_region (GtkStyleContext *context,
                                               gpointer         region_id);
void gtk_style_context_pop_animatable_region  (GtkStyleContext *context);


/* Semi-private API */
const GValue * _gtk_style_context_peek_style_property (GtkStyleContext *context,
                                                       GType            widget_type,
                                                       GParamSpec      *pspec);
void           _gtk_style_context_invalidate_animation_areas (GtkStyleContext *context);
void           _gtk_style_context_coalesce_animation_areas   (GtkStyleContext *context);

/* Animation for state changes */
void gtk_style_context_state_transition_start  (GtkStyleContext *context,
                                                gpointer         identifier,
                                                GtkWidget       *widget,
                                                GtkStateType     state,
                                                gboolean         value,
                                                GdkRectangle    *rect);
void gtk_style_context_state_transition_update (GtkStyleContext *context,
                                                gpointer         identifier,
                                                GdkRectangle    *rect,
                                                GtkStateType     state);
void gtk_style_context_state_transition_stop   (GtkStyleContext *context,
                                                gpointer         identifier);

/* Paint methods */
void gtk_render_check (GtkStyleContext *context,
                       cairo_t         *cr,
                       gdouble          x,
                       gdouble          y,
                       gdouble          width,
                       gdouble          height);
void gtk_render_option (GtkStyleContext *context,
                        cairo_t         *cr,
                        gdouble          x,
                        gdouble          y,
                        gdouble          width,
                        gdouble          height);
void gtk_render_arrow  (GtkStyleContext *context,
                        cairo_t         *cr,
                        gdouble          angle,
                        gdouble          x,
                        gdouble          y,
                        gdouble          size);
void gtk_render_background (GtkStyleContext *context,
                            cairo_t         *cr,
                            gdouble          x,
                            gdouble          y,
                            gdouble          width,
                            gdouble          height);
void gtk_render_frame  (GtkStyleContext *context,
                        cairo_t         *cr,
                        gdouble          x,
                        gdouble          y,
                        gdouble          width,
                        gdouble          height);
void gtk_render_expander (GtkStyleContext *context,
                          cairo_t         *cr,
                          gdouble          x,
                          gdouble          y,
                          gdouble          width,
                          gdouble          height);
void gtk_render_focus    (GtkStyleContext *context,
                          cairo_t         *cr,
                          gdouble          x,
                          gdouble          y,
                          gdouble          width,
                          gdouble          height);
void gtk_render_layout   (GtkStyleContext *context,
                          cairo_t         *cr,
                          gdouble          x,
                          gdouble          y,
                          PangoLayout     *layout);
void gtk_render_line     (GtkStyleContext *context,
                          cairo_t         *cr,
                          gdouble          x0,
                          gdouble          y0,
                          gdouble          x1,
                          gdouble          y1);
void gtk_render_slider   (GtkStyleContext *context,
                          cairo_t         *cr,
                          gdouble          x,
                          gdouble          y,
                          gdouble          width,
                          gdouble          height,
                          GtkOrientation   orientation);
void gtk_render_frame_gap (GtkStyleContext *context,
                           cairo_t         *cr,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height,
                           GtkPositionType  gap_side,
                           gdouble          xy0_gap,
                           gdouble          xy1_gap);
void gtk_render_extension (GtkStyleContext *context,
                           cairo_t         *cr,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height,
                           GtkPositionType  gap_side);
void gtk_render_handle    (GtkStyleContext *context,
                           cairo_t         *cr,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height,
                           GtkOrientation   orientation);

G_END_DECLS

#endif /* __GTK_STYLE_CONTEXT_H__ */
