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

#ifndef __GTK_THEMING_ENGINE_H__
#define __GTK_THEMING_ENGINE_H__

#include <glib-object.h>
#include <cairo.h>

#include <gtk/gtkstylecontext.h>
#include <gtk/gtkwidgetpath.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_THEMING_ENGINE         (gtk_theming_engine_get_type ())
#define GTK_THEMING_ENGINE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_THEMING_ENGINE, GtkThemingEngine))
#define GTK_THEMING_ENGINE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_THEMING_ENGINE, GtkThemingEngineClass))
#define GTK_IS_THEMING_ENGINE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_THEMING_ENGINE))
#define GTK_IS_THEMING_ENGINE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_THEMING_ENGINE))
#define GTK_THEMING_ENGINE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_THEMING_ENGINE, GtkThemingEngineClass))

typedef struct _GtkThemingEngine GtkThemingEngine;
typedef struct _GtkThemingEngineClass GtkThemingEngineClass;

struct _GtkThemingEngine
{
  GObject parent_object;
  gpointer priv;
};

/**
 * GtkThemingEngineClass
 * @parent_class: The parent class.
 * @render_line: Renders a line between two points.
 * @render_background: Renders the background area of a widget region.
 * @render_frame: Renders the frame around a widget area.
 * @render_frame_gap: Renders the frame around a widget area with a gap in it.
 * @render_extension: Renders a extension to a box, usually a notebook tab.
 * @render_check: Renders a checkmark, as in #GtkCheckButton.
 * @render_option: Renders an option, as in #GtkRadioButton.
 * @render_arrow: Renders an arrow pointing to a certain direction.
 * @render_expander: Renders an element what will expose/expand part of
 *                   the UI, as in #GtkExpander.
 * @render_focus: Renders the focus indicator.
 * @render_layout: Renders a #PangoLayout
 * @render_slider: Renders a slider control, as in #GtkScale.
 * @render_handle: Renders a handle to drag UI elements, as in #GtkPaned.
 *
 * Base class for theming engines.
 */
struct _GtkThemingEngineClass
{
  GObjectClass parent_class;

  void (* render_line) (GtkThemingEngine *engine,
                        cairo_t          *cr,
                        gdouble           x0,
                        gdouble           y0,
                        gdouble           x1,
                        gdouble           y1);
  void (* render_background) (GtkThemingEngine *engine,
                              cairo_t          *cr,
                              gdouble           x,
                              gdouble           y,
                              gdouble           width,
                              gdouble           height);
  void (* render_frame) (GtkThemingEngine *engine,
                         cairo_t          *cr,
                         gdouble           x,
                         gdouble           y,
                         gdouble           width,
                         gdouble           height);
  void (* render_frame_gap) (GtkThemingEngine *engine,
                             cairo_t          *cr,
                             gdouble           x,
                             gdouble           y,
                             gdouble           width,
                             gdouble           height,
                             GtkPositionType   gap_side,
                             gdouble           xy0_gap,
                             gdouble           xy1_gap);
  void (* render_extension) (GtkThemingEngine *engine,
                             cairo_t          *cr,
                             gdouble           x,
                             gdouble           y,
                             gdouble           width,
                             gdouble           height,
                             GtkPositionType   gap_side);
  void (* render_check) (GtkThemingEngine *engine,
                         cairo_t          *cr,
                         gdouble           x,
                         gdouble           y,
                         gdouble           width,
                         gdouble           height);
  void (* render_option) (GtkThemingEngine *engine,
                          cairo_t          *cr,
                          gdouble           x,
                          gdouble           y,
                          gdouble           width,
                          gdouble           height);
  void (* render_arrow) (GtkThemingEngine *engine,
                         cairo_t          *cr,
                         gdouble           angle,
                         gdouble           x,
                         gdouble           y,
                         gdouble           size);
  void (* render_expander) (GtkThemingEngine *engine,
                            cairo_t          *cr,
                            gdouble           x,
                            gdouble           y,
                            gdouble           width,
                            gdouble           height);
  void (* render_focus) (GtkThemingEngine *engine,
                         cairo_t          *cr,
                         gdouble           x,
                         gdouble           y,
                         gdouble           width,
                         gdouble           height);
  void (* render_layout) (GtkThemingEngine *engine,
                          cairo_t          *cr,
                          gdouble           x,
                          gdouble           y,
                          PangoLayout      *layout);
  void (* render_slider) (GtkThemingEngine *engine,
                          cairo_t          *cr,
                          gdouble           x,
                          gdouble           y,
                          gdouble           width,
                          gdouble           height,
                          GtkOrientation    orientation);
  void (* render_handle)    (GtkThemingEngine *engine,
                             cairo_t          *cr,
                             gdouble           x,
                             gdouble           y,
                             gdouble           width,
                             gdouble           height);
};

GType gtk_theming_engine_get_type (void) G_GNUC_CONST;

void _gtk_theming_engine_set_context (GtkThemingEngine *engine,
                                      GtkStyleContext  *context);

void gtk_theming_engine_register_property (GtkThemingEngine       *engine,
                                           const gchar            *property_name,
                                           GType                   type,
                                           const GValue           *default_value,
                                           GtkStylePropertyParser  parse_func);

void gtk_theming_engine_get_property (GtkThemingEngine *engine,
                                      const gchar      *property,
                                      GtkStateFlags     state,
                                      GValue           *value);
void gtk_theming_engine_get_valist   (GtkThemingEngine *engine,
                                      GtkStateFlags     state,
                                      va_list           args);
void gtk_theming_engine_get          (GtkThemingEngine *engine,
                                      GtkStateFlags     state,
                                      ...) G_GNUC_NULL_TERMINATED;

void gtk_theming_engine_get_style_property (GtkThemingEngine *engine,
                                            const gchar      *property_name,
                                            GValue           *value);
void gtk_theming_engine_get_style_valist   (GtkThemingEngine *engine,
                                            va_list           args);
void gtk_theming_engine_get_style          (GtkThemingEngine *engine,
                                            ...);


G_CONST_RETURN GtkWidgetPath * gtk_theming_engine_get_path (GtkThemingEngine *engine);

gboolean gtk_theming_engine_has_class  (GtkThemingEngine *engine,
                                        const gchar      *style_class);
gboolean gtk_theming_engine_has_region (GtkThemingEngine *engine,
                                        const gchar      *style_class,
                                        GtkRegionFlags   *flags);

GtkStateFlags gtk_theming_engine_get_state     (GtkThemingEngine *engine);
gboolean      gtk_theming_engine_is_state_set  (GtkThemingEngine *engine,
                                                GtkStateType      state,
                                                gdouble          *progress);

GtkTextDirection gtk_theming_engine_get_direction (GtkThemingEngine *engine);

GtkJunctionSides gtk_theming_engine_get_junction_sides (GtkThemingEngine *engine);

GtkThemingEngine * gtk_theming_engine_load (const gchar *name);

GdkScreen * gtk_theming_engine_get_screen (GtkThemingEngine *engine);


G_END_DECLS

#endif /* __GTK_THEMING_ENGINE_H__ */
