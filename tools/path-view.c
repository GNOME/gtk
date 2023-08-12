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

struct _PathView
{
  GtkWidget parent_instance;

  GskPath *path;
  GskStroke *stroke;
  graphene_rect_t bounds;
  GskFillRule fill_rule;
  GdkRGBA fg;
  GdkRGBA bg;
  int padding;
  gboolean do_fill;
};

enum {
  PROP_PATH = 1,
  PROP_DO_FILL,
  PROP_STROKE,
  PROP_FILL_RULE,
  PROP_FG_COLOR,
  PROP_BG_COLOR,
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
  self->do_fill = TRUE;
  self->stroke = gsk_stroke_new (1);
  self->fill_rule = GSK_FILL_RULE_WINDING;
  self->fg = (GdkRGBA) { 0, 0, 0, 1};
  self->bg = (GdkRGBA) { 1, 1, 1, 1};
  self->padding = 10;
}

static void
path_view_dispose (GObject *object)
{
  PathView *self = PATH_VIEW (object);

  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->stroke, gsk_stroke_free);

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
    case PROP_PATH:
      g_value_set_boxed (value, self->path);
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

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
update_bounds (PathView *self)
{
  if (self->do_fill)
    gsk_path_get_bounds (self->path, &self->bounds);
  else
    gsk_path_get_stroke_bounds (self->path, self->stroke, &self->bounds);
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

    case PROP_PATH:
      g_clear_pointer (&self->path, gsk_path_unref);
      self->path = g_value_dup_boxed (value);
      update_bounds (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    case PROP_DO_FILL:
      self->do_fill = g_value_get_boolean (value);
      update_bounds (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    case PROP_STROKE:
      gsk_stroke_free (self->stroke);
      self->stroke = g_value_get_boxed (value);
      update_bounds (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    case PROP_FILL_RULE:
      self->fill_rule = g_value_get_enum (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_FG_COLOR:
      self->fg = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_BG_COLOR:
      self->bg = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
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
    *minimum = *natural = (int) ceilf (self->bounds.size.width) + 2 * self->padding;
  else
    *minimum = *natural = (int) ceilf (self->bounds.size.height) + 2 * self->padding;
}

static void
path_view_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  PathView *self = PATH_VIEW (widget);
  graphene_rect_t bounds = self->bounds;

  graphene_rect_inset (&bounds, - self->padding, - self->padding);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_append_color (snapshot, &self->bg, &self->bounds);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (self->padding, self->padding));
  if (self->do_fill)
    gtk_snapshot_push_fill (snapshot, self->path, self->fill_rule);
  else
    gtk_snapshot_push_stroke (snapshot, self->path, self->stroke);
  gtk_snapshot_append_color (snapshot, &self->fg, &self->bounds);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
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

  properties[PROP_PATH]
      = g_param_spec_boxed ("path", NULL, NULL,
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

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

GtkWidget *
path_view_new (GskPath *path)
{
  return g_object_new (PATH_TYPE_VIEW,
                       "path", path,
                       NULL);
}
