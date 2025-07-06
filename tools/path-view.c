/*  Copyright 2023 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include "path-view.h"
#include "gtk-path-tool.h"

struct _PathView
{
  GtkWidget parent_instance;

  GskPath *path1;
  GskPath *path2;
  GskPath *path;
  GskStroke *stroke;
  graphene_rect_t bounds;
  GskFillRule fill_rule;
  GdkRGBA fg;
  GdkRGBA bg;
  int padding;
  gboolean do_fill;
  gboolean show_points;
  gboolean show_controls;
  gboolean show_intersections;
  GskPath *scaled_path;
  GskPath *line_path;
  GskPath *point_path;
  GdkRGBA point_color;
  GdkRGBA intersection_color;
  double zoom;
  GskPath *intersection_line_path;
  GskPath *intersection_point_path;
};

enum {
  PROP_PATH1 = 1,
  PROP_PATH2,
  PROP_DO_FILL,
  PROP_STROKE,
  PROP_FILL_RULE,
  PROP_FG_COLOR,
  PROP_BG_COLOR,
  PROP_POINT_COLOR,
  PROP_INTERSECTION_COLOR,
  PROP_SHOW_POINTS,
  PROP_SHOW_CONTROLS,
  PROP_SHOW_INTERSECTIONS,
  PROP_ZOOM,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

struct _PathViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (PathView, path_view, GTK_TYPE_WIDGET)

static void
path_view_init (PathView *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
  self->do_fill = TRUE;
  self->stroke = gsk_stroke_new (1);
  self->fill_rule = GSK_FILL_RULE_WINDING;
  self->fg = (GdkRGBA) { 0, 0, 0, 1};
  self->bg = (GdkRGBA) { 1, 1, 1, 1};
  self->point_color = (GdkRGBA) { 1, 0, 0, 1};
  self->intersection_color = (GdkRGBA) { 0, 1, 0, 1};
  self->padding = 10;
  self->zoom = 1;
}

static void
path_view_dispose (GObject *object)
{
  PathView *self = PATH_VIEW (object);

  g_clear_pointer (&self->path1, gsk_path_unref);
  g_clear_pointer (&self->path2, gsk_path_unref);
  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->stroke, gsk_stroke_free);
  g_clear_pointer (&self->line_path, gsk_path_unref);
  g_clear_pointer (&self->point_path, gsk_path_unref);
  g_clear_pointer (&self->intersection_line_path, gsk_path_unref);
  g_clear_pointer (&self->intersection_point_path, gsk_path_unref);

  G_OBJECT_CLASS (path_view_parent_class)->dispose (object);
}

static void
path_view_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  PathView *self = PATH_VIEW (object);

  switch (prop_id)
    {
    case PROP_PATH1:
      g_value_set_boxed (value, self->path1);
      break;

    case PROP_PATH2:
      g_value_set_boxed (value, self->path2);
      break;

    case PROP_DO_FILL:
      g_value_set_boolean (value, self->do_fill);
      break;

    case PROP_STROKE:
      g_value_set_boxed (value, self->stroke);
      break;

    case PROP_FILL_RULE:
      g_value_set_enum (value, self->fill_rule);
      break;

    case PROP_FG_COLOR:
      g_value_set_boxed (value, &self->fg);
      break;

    case PROP_BG_COLOR:
      g_value_set_boxed (value, &self->bg);
      break;

    case PROP_SHOW_POINTS:
      g_value_set_boolean (value, self->show_points);
      break;

    case PROP_SHOW_CONTROLS:
      g_value_set_boolean (value, self->show_controls);
      break;

    case PROP_SHOW_INTERSECTIONS:
      g_value_set_boolean (value, self->show_intersections);
      break;

    case PROP_POINT_COLOR:
      g_value_set_boxed (value, &self->point_color);
      break;

    case PROP_INTERSECTION_COLOR:
      g_value_set_boxed (value, &self->intersection_color);
      break;

    case PROP_ZOOM:
      g_value_set_double (value, self->zoom);
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
update_bounds (PathView *self)
{
  if (self->do_fill)
    {
      if (!gsk_path_get_bounds (self->scaled_path, &self->bounds))
        graphene_rect_init (&self->bounds, 0, 0, 0, 0);
    }
  else
    {
      if (!gsk_path_get_stroke_bounds (self->scaled_path, self->stroke, &self->bounds))
        graphene_rect_init (&self->bounds, 0, 0, 0, 0);
    }

  if (self->line_path)
    {
      graphene_rect_t bounds;

      if (gsk_path_get_stroke_bounds (self->line_path, self->stroke, &bounds))
        graphene_rect_union (&bounds, &self->bounds, &self->bounds);
    }

  if (self->point_path)
    {
      graphene_rect_t bounds;

      if (gsk_path_get_stroke_bounds (self->point_path, self->stroke, &bounds))
        graphene_rect_union (&bounds, &self->bounds, &self->bounds);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
update_controls (PathView *self)
{
  g_clear_pointer (&self->scaled_path, gsk_path_unref);
  g_clear_pointer (&self->line_path, gsk_path_unref);
  g_clear_pointer (&self->point_path, gsk_path_unref);

  if (self->path)
    collect_render_data (self->path,
                         self->show_points,
                         self->show_controls,
                         self->zoom,
                         &self->scaled_path,
                         &self->line_path,
                         &self->point_path);

  update_bounds (self);
}

static void
update_intersections (PathView *self)
{
  g_clear_pointer (&self->intersection_line_path, gsk_path_unref);
  g_clear_pointer (&self->intersection_point_path, gsk_path_unref);

  if (self->show_intersections && self->path1)
    collect_intersections (self->path1, self->path2, self->zoom,
                           &self->intersection_line_path,
                           &self->intersection_point_path);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
path_view_set_fill_rule (PathView    *self,
                         GskFillRule  fill_rule)
{
  if (self->fill_rule == fill_rule)
    return;

  self->fill_rule = fill_rule;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
path_view_set_zoom (PathView *self,
                    double    zoom)
{
  zoom = CLAMP (zoom, 1, 20);

  if (self->zoom == zoom)
    return;

  self->zoom = zoom;
  update_controls (self);
  update_intersections (self);
}

static void
update_path (PathView *self)
{
  GskPathBuilder *builder;

  g_clear_pointer (&self->path, gsk_path_unref);

  builder = gsk_path_builder_new ();
  if (self->path1)
    gsk_path_builder_add_path (builder, self->path1);

  if (self->path2)
    gsk_path_builder_add_path (builder, self->path2);

  self->path = gsk_path_builder_free_to_path (builder);

  update_controls (self);
  update_intersections (self);
}

static void
path_view_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  PathView *self = PATH_VIEW (object);

  switch (prop_id)
    {
    case PROP_PATH1:
      g_clear_pointer (&self->path1, gsk_path_unref);
      self->path1 = g_value_dup_boxed (value);
      update_path (self);
      break;

    case PROP_PATH2:
      g_clear_pointer (&self->path2, gsk_path_unref);
      self->path2 = g_value_dup_boxed (value);
      update_path (self);
      break;

    case PROP_DO_FILL:
      self->do_fill = g_value_get_boolean (value);
      update_bounds (self);
      break;

    case PROP_STROKE:
      gsk_stroke_free (self->stroke);
      self->stroke = g_value_get_boxed (value);
      update_bounds (self);
      break;

    case PROP_FILL_RULE:
      path_view_set_fill_rule (self, g_value_get_enum (value));
      break;

    case PROP_FG_COLOR:
      self->fg = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_BG_COLOR:
      self->bg = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_SHOW_POINTS:
      self->show_points = g_value_get_boolean (value);
      update_controls (self);
      break;

    case PROP_SHOW_CONTROLS:
      self->show_controls = g_value_get_boolean (value);
      update_controls (self);
      break;

    case PROP_SHOW_INTERSECTIONS:
      self->show_intersections = g_value_get_boolean (value);
      update_intersections (self);
      break;

    case PROP_POINT_COLOR:
      self->point_color = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_INTERSECTION_COLOR:
      self->intersection_color = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

   case PROP_ZOOM:
      path_view_set_zoom (self, g_value_get_double (value));
     break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
path_view_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  PathView *self = PATH_VIEW (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = (int) ceilf (self->bounds.origin.x + self->bounds.size.width) + 2 * self->padding;
  else
    *minimum = *natural = (int) ceilf (self->bounds.origin.y + self->bounds.size.height) + 2 * self->padding;
}

static void
path_view_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  PathView *self = PATH_VIEW (widget);
  graphene_rect_t bounds = self->bounds;

  graphene_rect_inset (&bounds, - self->padding, - self->padding);

  gtk_snapshot_save (snapshot);

  gtk_snapshot_append_color (snapshot, &self->bg, &bounds);

  if (self->do_fill)
    gtk_snapshot_append_fill (snapshot, self->scaled_path, self->fill_rule, &self->fg);
  else
    gtk_snapshot_append_stroke (snapshot, self->scaled_path, self->stroke, &self->fg);

  if (self->line_path)
    {
      GskStroke *stroke = gsk_stroke_new (1);

      gsk_stroke_set_dash (stroke, (const float[]) { 1, 1 }, 2);
      gtk_snapshot_append_stroke (snapshot, self->line_path, stroke, &self->fg);

      gsk_stroke_free (stroke);
    }

  if (self->point_path)
    {
      GskStroke *stroke = gsk_stroke_new (1);

      gtk_snapshot_append_fill (snapshot, self->point_path, GSK_FILL_RULE_WINDING, &self->point_color);
      gtk_snapshot_append_stroke (snapshot, self->point_path, stroke, &self->fg);

      gsk_stroke_free (stroke);
    }

  if (self->intersection_line_path)
    {
      GskStroke *stroke = gsk_stroke_new (gsk_stroke_get_line_width (self->stroke));

      gtk_snapshot_append_stroke (snapshot, self->intersection_line_path, stroke, &self->intersection_color);

      gsk_stroke_free (stroke);
    }

  if (self->intersection_point_path)
    {
      gtk_snapshot_append_fill (snapshot, self->intersection_point_path, GSK_FILL_RULE_WINDING, &self->intersection_color);
    }

  gtk_snapshot_restore (snapshot);
}

static void
path_view_change_zoom (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *parameter)
{
  PathView *self = PATH_VIEW (widget);
  double factor;

  g_variant_get (parameter, "d", &factor);

  path_view_set_zoom (self, self->zoom * factor);
}

static void
path_view_toggle_fill_rule (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *parameter)
{
  PathView *self = PATH_VIEW (widget);

  path_view_set_fill_rule (self, self->fill_rule == GSK_FILL_RULE_WINDING
                                 ? GSK_FILL_RULE_EVEN_ODD
                                 : GSK_FILL_RULE_WINDING);
}
static void
path_view_class_init (PathViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = path_view_dispose;
  object_class->get_property = path_view_get_property;
  object_class->set_property = path_view_set_property;

  widget_class->measure = path_view_measure;
  widget_class->snapshot = path_view_snapshot;

  properties[PROP_PATH1]
      = g_param_spec_boxed ("path1", NULL, NULL,
                            GSK_TYPE_PATH,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_PATH2]
      = g_param_spec_boxed ("path2", NULL, NULL,
                            GSK_TYPE_PATH,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_DO_FILL]
      = g_param_spec_boolean ("do-fill", NULL, NULL,
                              TRUE,
                              G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_STROKE]
      = g_param_spec_boxed ("stroke", NULL, NULL,
                            GSK_TYPE_STROKE,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_FILL_RULE]
      = g_param_spec_enum ("fill-rule", NULL, NULL,
                           GSK_TYPE_FILL_RULE,
                           GSK_FILL_RULE_WINDING,
                           G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_FG_COLOR]
      = g_param_spec_boxed ("fg-color", NULL, NULL,
                            GDK_TYPE_RGBA,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_BG_COLOR]
      = g_param_spec_boxed ("bg-color", NULL, NULL,
                            GDK_TYPE_RGBA,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_POINTS]
      = g_param_spec_boolean ("show-points", NULL, NULL,
                              FALSE,
                              G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_CONTROLS]
      = g_param_spec_boolean ("show-controls", NULL, NULL,
                              FALSE,
                              G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_INTERSECTIONS]
      = g_param_spec_boolean ("show-intersections", NULL, NULL,
                              FALSE,
                              G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_POINT_COLOR]
      = g_param_spec_boxed ("point-color", NULL, NULL,
                            GDK_TYPE_RGBA,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_INTERSECTION_COLOR]
      = g_param_spec_boxed ("intersection-color", NULL, NULL,
                            GDK_TYPE_RGBA,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ZOOM]
      = g_param_spec_double ("zoom", NULL, NULL,
                             1, 20, 1,
                             G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  gtk_widget_class_install_action (widget_class, "zoom", "d",
                                   path_view_change_zoom);

  gtk_widget_class_install_property_action (widget_class, "points", "show-points");
  gtk_widget_class_install_property_action (widget_class, "controls", "show-controls");
  gtk_widget_class_install_property_action (widget_class, "intersections", "show-intersections");

  gtk_widget_class_install_action (widget_class, "fill-rule", NULL,
                                   path_view_toggle_fill_rule);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_plus, 0, "zoom", "d", 1.2);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_minus, 0, "zoom", "d", 1/1.2);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_p, 0, "points", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_c, 0, "controls", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_i, 0, "intersections", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_f, 0, "fill-rule", NULL);
}

GtkWidget *
path_view_new (GskPath *path1,
               GskPath *path2)
{
  return g_object_new (PATH_TYPE_VIEW,
                       "path1", path1,
                       "path1", path2,
                       NULL);
}
