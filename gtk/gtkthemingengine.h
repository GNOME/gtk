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

typedef struct GtkThemingEngine GtkThemingEngine;
typedef struct GtkThemingEngineClass GtkThemingEngineClass;

struct GtkThemingEngine
{
  GObject parent_object;
  gpointer priv;
};

struct GtkThemingEngineClass
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
};

GType gtk_theming_engine_get_type (void) G_GNUC_CONST;

void _gtk_theming_engine_set_context (GtkThemingEngine *engine,
                                      GtkStyleContext  *context);

void gtk_theming_engine_get_property (GtkThemingEngine *engine,
                                      const gchar      *property,
                                      GtkStateType      state,
                                      GValue           *value);
void gtk_theming_engine_get_valist   (GtkThemingEngine *engine,
                                      GtkStateType      state,
                                      va_list           args);
void gtk_theming_engine_get          (GtkThemingEngine *engine,
                                      GtkStateType      state,
                                      ...) G_GNUC_NULL_TERMINATED;

G_CONST_RETURN GtkWidgetPath * gtk_theming_engine_get_path (GtkThemingEngine *engine);

gboolean gtk_theming_engine_has_class        (GtkThemingEngine *engine,
                                              const gchar      *style_class);
gboolean gtk_theming_engine_has_child_class (GtkThemingEngine   *engine,
                                             const gchar        *style_class,
                                             GtkChildClassFlags *flags);

GtkStateFlags gtk_theming_engine_get_state     (GtkThemingEngine *engine);
gboolean      gtk__theming_engine_is_state_set (GtkThemingEngine *engine,
                                                GtkStateType      state);

G_CONST_RETURN GtkThemingEngine * gtk_theming_engine_load (const gchar *name);


G_END_DECLS

#endif /* __GTK_THEMING_ENGINE_H__ */
