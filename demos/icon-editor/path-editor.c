/*
 * Copyright © 2025 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "path-editor.h"
#include "path-paintable-private.h"


struct _PathEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;
  gsize path;

  GtkGrid *grid;
  GtkLabel *path_label;
  GtkDropDown *transition;
  GtkDropDown *origin;
  GtkDropDown *stroke_paint;
  GtkColorDialogButton *stroke_color;
  GtkSpinButton *line_width;
  GtkSpinButton *min_width;
  GtkSpinButton *max_width;
  GtkDropDown *line_join;
  GtkDropDown *line_cap;
  GtkDropDown *fill_paint;
  GtkColorDialogButton *fill_color;
  GtkDropDown *fill_rule;
  GtkEntry *discrete_state;
  GtkDropDown *attach_to;
  GtkScale *attach_at;
  GtkSizeGroup *sg;
  GtkButton *move_down;

  gboolean updating;
};

enum
{
  PROP_PAINTABLE = 1,
  PROP_PATH,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ Path-to-SVG */ 

static gboolean
collect_path (GskPathOperation        op,
              const graphene_point_t *pts,
              gsize                   n_pts,
              float                   weight,
              gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;

    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder, pts[1].x, pts[1].y,
                                         pts[2].x, pts[2].y);
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder, pts[1].x, pts[1].y,
                                          pts[2].x, pts[2].y,
                                          pts[3].x, pts[3].y);
      break;

    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder, pts[1].x, pts[1].y,
                                          pts[2].x, pts[2].y,
                                          weight);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static GskPath *
path_to_svg_path (GskPath *path)
{
  GskPathBuilder *builder = gsk_path_builder_new ();

  gsk_path_foreach (path,
                    GSK_PATH_FOREACH_ALLOW_QUAD | GSK_PATH_FOREACH_ALLOW_CUBIC,
                    collect_path,
                    builder);

  return gsk_path_builder_free_to_path (builder);
}

/* }}} */
/* {{{ Callbacks */

static void
transition_changed (PathEditor *self)
{
  StateTransition transition;

  if (self->updating)
    return;

  transition = (StateTransition) gtk_drop_down_get_selected (self->transition);
  path_paintable_set_path_transition (self->paintable, self->path, transition);
}

static void
origin_changed (PathEditor *self)
{
  float origin;

  if (self->updating)
    return;

  origin = gtk_range_get_value (GTK_RANGE (self->origin));
  path_paintable_set_path_origin (self->paintable, self->path, origin);
}

static void
stroke_changed (PathEditor *self)
{
  gboolean do_stroke;
  float line_width, min_width, max_width;
  GskLineJoin line_join;
  GskLineCap line_cap;
  guint selected;
  guint symbolic;
  const GdkRGBA *color;
  g_autoptr (GskStroke) stroke = NULL;
  GtkAdjustment *adj;

  if (self->updating)
    return;

  line_width = gtk_spin_button_get_value (self->line_width);
  line_join = gtk_drop_down_get_selected (self->line_join);
  line_cap = gtk_drop_down_get_selected (self->line_cap);
  min_width = gtk_spin_button_get_value (self->min_width);
  max_width = gtk_spin_button_get_value (self->max_width);

  stroke = gsk_stroke_new (line_width);
  gsk_stroke_set_line_join (stroke, line_join);
  gsk_stroke_set_line_cap (stroke, line_cap);

  selected = gtk_drop_down_get_selected (self->stroke_paint);
  if (selected == 0)
    {
      do_stroke = FALSE;
      symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
    }
  else if (selected == 5)
    {
      do_stroke = TRUE;
      symbolic = 0xffff;
    }
  else
    {
      do_stroke = TRUE;
      symbolic = selected - 1;
    }

  color = gtk_color_dialog_button_get_rgba (self->stroke_color);

  path_paintable_set_path_stroke (self->paintable, self->path,
                                  do_stroke, stroke, symbolic, color);
  path_paintable_set_path_stroke_variation (self->paintable, self->path,
                                            min_width, max_width);

  adj = gtk_spin_button_get_adjustment (self->min_width);
  gtk_adjustment_configure (adj, min_width, 0.5, line_width, 0.1, 0.1, 0);

  adj = gtk_spin_button_get_adjustment (self->max_width);
  gtk_adjustment_configure (adj, max_width, line_width, 20, 0.1, 0.1, 0);
}

static void
fill_changed (PathEditor *self)
{
  gboolean do_fill;
  guint selected;
  guint symbolic;
  const GdkRGBA *color;
  GskFillRule fill_rule;

  if (self->updating)
    return;

  fill_rule = gtk_drop_down_get_selected (self->fill_rule);

  selected = gtk_drop_down_get_selected (self->fill_paint);
  if (selected == 0)
    {
      do_fill = FALSE;
      symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
    }
  else if (selected == 5)
    {
      do_fill = TRUE;
      symbolic = 0xffff;
    }
  else
    {
      do_fill = TRUE;
      symbolic = selected - 1;
    }

  color = gtk_color_dialog_button_get_rgba (self->fill_color);

  path_paintable_set_path_fill (self->paintable, self->path,
                                do_fill, fill_rule, symbolic, color);
}

static void
states_changed (PathEditor *self)
{
  const char *text;
  guint64 states;

  if (self->updating)
    return;

  if (!self->paintable)
    return;

  text = gtk_editable_get_text (GTK_EDITABLE (self->discrete_state));
  if (!states_parse (text, &states))
    return;

  path_paintable_set_path_states (self->paintable, self->path, states);
}

static void
attach_changed (PathEditor *self)
{
  gsize selected;
  float pos;

  if (self->updating)
    return;

  selected = gtk_drop_down_get_selected (self->attach_to);
  pos = gtk_range_get_value (GTK_RANGE (self->attach_at));

  if (selected == 0)
    path_paintable_attach_path (self->paintable, self->path, (gsize) -1, pos);
  else if (selected <= self->path)
    path_paintable_attach_path (self->paintable, self->path, selected - 1, pos);
  else
    path_paintable_attach_path (self->paintable, self->path, selected, pos);
}

static gboolean
bool_and_bool (GObject  *object,
               gboolean  b1,
               gboolean  b2)
{
  return b1 && b2;
}

static void
show_error (PathEditor *self,
            const char *title,
            const char *detail)
{
  GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  g_autoptr (GtkAlertDialog) alert = NULL;

  alert = gtk_alert_dialog_new ("title");
  gtk_alert_dialog_set_detail (alert, detail);
  gtk_alert_dialog_show (alert, window);
}

static void
temp_file_changed (GFileMonitor      *monitor,
                   GFile             *file,
                   GFile             *other,
                   GFileMonitorEvent  event,
                   PathEditor        *self)
{
  if (event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
      g_autoptr (GFileMonitor) the_monitor = monitor;
      g_autoptr (GBytes) bytes = NULL;
      g_autoptr (GError) error = NULL;
      g_autoptr (PathPaintable) paintable = NULL;

      bytes = g_file_load_bytes (file, NULL, NULL, &error);
      if (!bytes)
        {
          show_error (self, "Editing Failed", error->message);
          return;
        }

      paintable = path_paintable_new_from_bytes (bytes, &error);
      if (!paintable)
        {
          show_error (self, "Editing Failed", error->message);
          return;
        }

      path_paintable_set_path (self->paintable, self->path,
                               path_paintable_get_path (paintable, 0));
    }
}

static void
edit_path (PathEditor *self)
{
  GString *str;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileIOStream) iostream = NULL;
  GOutputStream *ostream;
  gssize written;
  g_autoptr (GError) error = NULL;
  GFileMonitor *monitor;
  g_autoptr (GSubprocess) subprocess = NULL;
  g_autoptr (GskStroke) stroke = NULL;
  guint symbolic;
  GdkRGBA color;
  const char *linecap[] = { "butt", "round", "square" };
  const char *linejoin[] = { "miter", "round", "bevel" };
  g_autoptr (GskPath) path = NULL;

  path = path_to_svg_path (path_paintable_get_path (self->paintable, self->path));

  stroke = gsk_stroke_new (1);
  path_paintable_get_path_stroke (self->paintable, self->path,
                                  stroke, &symbolic, &color);

  str = g_string_new ("");
  g_string_append_printf (str, "<svg width='%g' height='%g'>\n",
                          path_paintable_get_width (self->paintable),
                          path_paintable_get_height (self->paintable));

  g_string_append_printf (str, "<path id='path%lu'\n"
                               "      d='",
                          self->path);
  gsk_path_print (path, str);
  g_string_append_printf (str, "'\n"
                          "      fill='none'\n"
                          "      stroke='black'\n"
                          "      stroke-width='%g'\n"
                          "      stroke-linejoin='%s'\n"
                          "      stroke-linecap='%s'/>\n",
                          gsk_stroke_get_line_width (stroke),
                          linejoin[gsk_stroke_get_line_join (stroke)],
                          linecap[gsk_stroke_get_line_cap (stroke)]);
  g_string_append (str, "</svg>");
  bytes = g_string_free_to_bytes (str);

  file = g_file_new_tmp ("gtkXXXXXX.svg", &iostream, &error);
  if (!file)
    {
      show_error (self, "Editing Failed", error->message);
      return;
    }

  ostream = g_io_stream_get_output_stream (G_IO_STREAM (iostream));

  written = g_output_stream_write_bytes (ostream, bytes, NULL, &error);
  if (written == -1)
    {
      show_error (self, "Editing Failed", error->message);
      return;
    }
  g_output_stream_close (ostream, NULL, NULL);

  monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
  if (error)
    {
      show_error (self, "Editing Failed", error->message);
      return;
    }

  g_signal_connect_object (monitor, "changed",
                           G_CALLBACK (temp_file_changed), self, G_CONNECT_DEFAULT);

  g_print ("Running: flatpak run --command=inkscape --file-forwarding org.inkscape.Inkscape @@ %s @@\n", g_file_peek_path (file));

  subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE, &error,
                                 "flatpak",
                                 "run",
                                 "--command=inkscape",
                                 "--file-forwarding",
                                 "org.inkscape.Inkscape",
                                 "@@",
                                 g_file_peek_path (file),
                                 "@@",
                                 NULL);

  if (!subprocess)
    {
      show_error (self, "Editing Failed", error->message);
      return;
    }
}

static void
move_path_down (PathEditor *self)
{
  path_paintable_move_path (self->paintable, self->path, self->path + 1);
}

static void
delete_path (PathEditor *self)
{
  path_paintable_delete_path (self->paintable, self->path);
}

static void
path_editor_update (PathEditor *self)
{
  if (self->paintable && self->path != (gsize) -1)
    {
      GskPath *path;
      g_autofree char *text = NULL;
      g_autofree char *id = NULL;
      g_autofree char *states = NULL;
      StateTransition transition;
      float origin;
      gboolean do_stroke;
      GskStroke *stroke;
      guint symbolic;
      GdkRGBA color;
      gboolean do_fill;
      GskFillRule fill_rule;
      gsize to;
      float pos;
      g_autoptr (GtkStringList) model = NULL;
      float width, min_width, max_width;
      GtkAdjustment *adj;

      self->updating = TRUE;

      path = path_paintable_get_path (self->paintable, self->path);
      text = gsk_path_to_string (path);
      gtk_label_set_label (self->path_label, text);

      states = states_to_string (path_paintable_get_path_states (self->paintable, self->path));
      gtk_editable_set_text (GTK_EDITABLE (self->discrete_state), states);

      transition = path_paintable_get_path_transition (self->paintable, self->path);
      gtk_drop_down_set_selected (self->transition, transition);

      origin = path_paintable_get_path_origin (self->paintable, self->path);
      gtk_range_set_value (GTK_RANGE (self->origin), origin);

      stroke = gsk_stroke_new (1);
      do_stroke = path_paintable_get_path_stroke (self->paintable, self->path,
                                                  stroke, &symbolic, &color);

      width = gsk_stroke_get_line_width (stroke);
      path_paintable_get_path_stroke_variation (self->paintable, self->path,
                                                &min_width, &max_width);

      if (!do_stroke)
        gtk_drop_down_set_selected (self->stroke_paint, 0);
      else if (symbolic == 0xffff)
        gtk_drop_down_set_selected (self->stroke_paint, 5);
      else
        gtk_drop_down_set_selected (self->stroke_paint, symbolic + 1);

      gtk_color_dialog_button_set_rgba (self->stroke_color, &color);

      gtk_spin_button_set_value (self->line_width, width);

      adj = gtk_spin_button_get_adjustment (self->min_width);
      gtk_adjustment_configure (adj, min_width, 0.5, width, 0.1, 0.1, 0);

      adj = gtk_spin_button_get_adjustment (self->max_width);
      gtk_adjustment_configure (adj, max_width, width, 20, 0.1, 0.1, 0);

      gtk_drop_down_set_selected (self->line_join, (guint) gsk_stroke_get_line_join (stroke));
      gtk_drop_down_set_selected (self->line_cap, (guint) gsk_stroke_get_line_cap (stroke));
      gsk_stroke_free (stroke);

      do_fill = path_paintable_get_path_fill (self->paintable, self->path,
                                              &fill_rule, &symbolic, &color);

      if (!do_fill)
        gtk_drop_down_set_selected (self->fill_paint, 0);
      else if (symbolic == 0xffff)
        gtk_drop_down_set_selected (self->fill_paint, 5);
      else
        gtk_drop_down_set_selected (self->fill_paint, symbolic + 1);

      gtk_color_dialog_button_set_rgba (self->fill_color, &color);

      gtk_drop_down_set_selected (self->fill_rule, (guint) fill_rule);

      model = gtk_string_list_new (NULL);
      gtk_string_list_append (model, "None");
      for (gsize i = 0; i < path_paintable_get_n_paths (self->paintable); i++)
        {
          if (i == self->path)
            continue;
          gtk_string_list_take (model, g_strdup_printf ("Path %lu", i));
        }
      gtk_drop_down_set_model (self->attach_to, G_LIST_MODEL (model));

      path_paintable_get_attach_path (self->paintable, self->path, &to, &pos);
      if (to == (gsize) -1)
        gtk_drop_down_set_selected (self->attach_to, 0);
      else if (to < self->path)
        gtk_drop_down_set_selected (self->attach_to, to + 1);
      else
        gtk_drop_down_set_selected (self->attach_to, to);

      gtk_range_set_value (GTK_RANGE (self->attach_at), pos);

      if (self->path + 1 == path_paintable_get_n_paths (self->paintable))
        gtk_widget_set_sensitive (GTK_WIDGET (self->move_down), FALSE);

      self->updating = FALSE;
    }
}

/* }}} */
/* {{{ GObject boilerplate */

struct _PathEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (PathEditor, path_editor, GTK_TYPE_WIDGET)

static void
path_editor_init (PathEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
path_editor_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  PathEditor *self = PATH_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      path_editor_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_PATH:
      path_editor_set_path (self, g_value_get_uint64 (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
path_editor_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  PathEditor *self = PATH_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_PATH:
      g_value_set_uint64 (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
path_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), PATH_EDITOR_TYPE);

  G_OBJECT_CLASS (path_editor_parent_class)->dispose (object);
}

static void
path_editor_finalize (GObject *object)
{
  PathEditor *self = PATH_EDITOR (object);

  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (path_editor_parent_class)->finalize (object);
}

static void
path_editor_class_init (PathEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = path_editor_set_property;
  object_class->get_property = path_editor_get_property;
  object_class->dispose = path_editor_dispose;
  object_class->finalize = path_editor_finalize;

  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                         PATH_PAINTABLE_TYPE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_PATH] =
    g_param_spec_uint64 ("path", NULL, NULL,
                         0, G_MAXUINT64, 0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/icon-editor/path-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, PathEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, path_label);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, transition);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, origin);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, stroke_paint);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, stroke_color);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, line_width);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, min_width);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, max_width);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, line_join);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, line_cap);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, fill_paint);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, fill_color);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, fill_rule);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, discrete_state);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, attach_to);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, attach_at);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, move_down);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, sg);

  gtk_widget_class_bind_template_callback (widget_class, transition_changed);
  gtk_widget_class_bind_template_callback (widget_class, origin_changed);
  gtk_widget_class_bind_template_callback (widget_class, stroke_changed);
  gtk_widget_class_bind_template_callback (widget_class, fill_changed);
  gtk_widget_class_bind_template_callback (widget_class, states_changed);
  gtk_widget_class_bind_template_callback (widget_class, attach_changed);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_bool);
  gtk_widget_class_bind_template_callback (widget_class, edit_path);
  gtk_widget_class_bind_template_callback (widget_class, move_path_down);
  gtk_widget_class_bind_template_callback (widget_class, delete_path);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* }}} */
/* {{{ Public API */

PathEditor *
path_editor_new (PathPaintable *paintable,
                 gsize          path)
{
  return g_object_new (PATH_EDITOR_TYPE,
                       "paintable", paintable,
                       "path", path,
                       NULL);
}

void
path_editor_set_paintable (PathEditor    *self,
                           PathPaintable *paintable)
{
  g_return_if_fail (PATH_IS_EDITOR (self));
  g_return_if_fail (paintable == NULL || PATH_IS_PAINTABLE (paintable));

  if (!g_set_object (&self->paintable, paintable))
    return;

  self->path = (gsize) -1;

  path_editor_update (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

PathPaintable *
path_editor_get_paintable (PathEditor *self)
{
  g_return_val_if_fail (PATH_IS_EDITOR (self), NULL);

  return self->paintable;
}

void
path_editor_set_path (PathEditor *self,
                      gsize       path)
{
  g_return_if_fail (PATH_IS_EDITOR (self));
  g_return_if_fail ((self->paintable == NULL && path == 0) ||
                    (self->paintable && path < path_paintable_get_n_paths (self->paintable)));

  if (self->path == path)
    return;

  self->path = path;

  path_editor_update (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

gsize
path_editor_get_path (PathEditor *self)
{
  g_return_val_if_fail (PATH_IS_EDITOR (self), 0);

  return self->path;
}

/* }}} */

/* vim:set foldmethod=marker: */
