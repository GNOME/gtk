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
#include "path-paintable.h"
#include "range-editor.h"
#include "color-editor.h"
#include "mini-graph.h"


struct _PathEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;
  size_t path;
  PathPaintable *path_image;

  gboolean updating;

  GtkGrid *grid;
  GtkLabel *path_cmds;
  GtkEditableLabel *id_label;
  GtkDropDown *origin;
  GtkDropDown *transition_type;
  GtkSpinButton *transition_duration;
  GtkSpinButton *transition_delay;
  GtkDropDown *transition_easing;
  GtkDropDown *animation_type;
  GtkDropDown *animation_direction;
  GtkSpinButton *animation_duration;
  GtkSpinButton *animation_repeat;
  GtkSpinButton *animation_segment;
  GtkCheckButton *infty_check;
  GtkDropDown *animation_easing;
  MiniGraph *mini_graph;
  ColorEditor *stroke_paint;
  RangeEditor *width_range;
  GtkSpinButton *min_width;
  GtkSpinButton *line_width;
  GtkSpinButton *max_width;
  GtkDropDown *line_join;
  GtkDropDown *line_cap;
  ColorEditor *fill_paint;
  GtkDropDown *fill_rule;
  GtkDropDown *attach_to;
  GtkScale *attach_at;
  GtkSizeGroup *sg;
  GtkButton *move_down;
};

enum
{
  PROP_PAINTABLE = 1,
  PROP_PATH,
  PROP_PATH_IMAGE,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ Path-to-SVG */

static gboolean
collect_path (GskPathOperation        op,
              const graphene_point_t *pts,
              size_t                  n_pts,
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

static PathPaintable *
path_editor_get_path_image (PathEditor *self)
{
  if (!self->path_image)
    {
      gboolean do_stroke;
      g_autoptr (GskStroke) stroke = gsk_stroke_new (1);
      unsigned int stroke_symbolic = 0;
      GdkRGBA stroke_color;
      gboolean do_fill;
      unsigned int fill_symbolic = 0;
      GdkRGBA fill_color;
      GskFillRule rule;
      float shape_params[6] = { 0, };

      self->path_image = path_paintable_new ();
      path_paintable_add_path (self->path_image, path_paintable_get_path (self->paintable, self->path), SHAPE_PATH, shape_params);

      do_stroke = path_paintable_get_path_stroke (self->paintable, self->path,
                                                  stroke, &stroke_symbolic, &stroke_color);
      do_fill = path_paintable_get_path_fill (self->paintable, self->path,
                                              &rule, &fill_symbolic, &fill_color);

      path_paintable_set_path_stroke (self->path_image, 0, do_stroke, stroke, stroke_symbolic, &stroke_color);
      path_paintable_set_path_fill (self->path_image, 0, do_fill, rule, fill_symbolic, &fill_color);
      path_paintable_set_size (self->path_image,
                               path_paintable_get_width (self->paintable),
                               path_paintable_get_height (self->paintable));
      path_paintable_set_state (self->path_image, 0);
    }

  return self->path_image;
}

static void
animation_changed (PathEditor *self)
{
  AnimationType type;
  AnimationDirection direction;
  float duration;
  float repeat;
  EasingFunction easing;
  CalcMode mode = CALC_MODE_SPLINE;
  float segment;
  const KeyFrame *frames;
  unsigned int n_frames;

  if (self->updating)
    return;

  type = (AnimationType) gtk_drop_down_get_selected (self->animation_type);
  direction = (AnimationDirection) gtk_drop_down_get_selected (self->animation_direction);
  duration = gtk_spin_button_get_value (self->animation_duration);
  if (gtk_check_button_get_active (self->infty_check))
    repeat = G_MAXFLOAT;
  else
    repeat = gtk_spin_button_get_value (self->animation_repeat);
  segment = gtk_spin_button_get_value (self->animation_segment);
  easing = (EasingFunction) gtk_drop_down_get_selected (self->animation_easing);

  path_paintable_set_path_animation (self->paintable, self->path, type, direction, duration, repeat, easing, segment);

  frames = path_paintable_get_path_animation_frames (self->paintable, self->path);
  n_frames = path_paintable_get_path_animation_n_frames (self->paintable, self->path);
  path_paintable_set_path_animation_timing (self->paintable, self->path, easing, mode, frames, n_frames);

  mini_graph_set_params (self->mini_graph, easing, mode, frames, n_frames);
}

static void
transition_changed (PathEditor *self)
{
  TransitionType type;
  float duration;
  float delay;
  EasingFunction easing;

  if (self->updating)
    return;

  type = (TransitionType) gtk_drop_down_get_selected (self->transition_type);
  duration = gtk_spin_button_get_value (self->transition_duration);
  delay = gtk_spin_button_get_value (self->transition_delay);
  easing = (EasingFunction) gtk_drop_down_get_selected (self->transition_easing);

  path_paintable_set_path_transition (self->paintable, self->path, type, duration, delay, easing);
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
id_changed (PathEditor *self)
{
  const char *id;

  if (self->updating)
    return;

  id = gtk_editable_get_text (GTK_EDITABLE (self->id_label));
  if (!path_paintable_set_path_id (self->paintable, self->path, id))
    gtk_widget_error_bell (GTK_WIDGET (self->id_label));
}

static void
line_width_changed (PathEditor *self)
{
  float lower, upper;
  float min, width, max;

  if (self->updating)
    return;

  range_editor_get_limits (self->width_range, &lower, NULL, &upper);

  width = gtk_spin_button_get_value (self->line_width);
  width = CLAMP (width, lower, upper);

  min = gtk_spin_button_get_value (self->min_width);
  max = gtk_spin_button_get_value (self->max_width);

  min = MIN (min, width);
  max = MAX (max, width);

  range_editor_configure (self->width_range,
                          lower, width, upper, min, max);
}

static void
stroke_changed (PathEditor *self)
{
  gboolean do_stroke;
  float width;
  float min, max;
  GskLineJoin line_join;
  GskLineCap line_cap;
  unsigned int selected;
  unsigned int symbolic;
  const GdkRGBA *color;
  g_autoptr (GskStroke) stroke = NULL;

  if (self->updating)
    return;

  line_join = gtk_drop_down_get_selected (self->line_join);
  line_cap = gtk_drop_down_get_selected (self->line_cap);
  range_editor_get_limits (self->width_range, NULL, &width, NULL);
  range_editor_get_values (self->width_range, &min, &max);

  stroke = gsk_stroke_new (width);
  gsk_stroke_set_line_join (stroke, line_join);
  gsk_stroke_set_line_cap (stroke, line_cap);

  selected = color_editor_get_color_type (self->stroke_paint);
  if (selected == 0)
    {
      do_stroke = FALSE;
      symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
    }
  else if (selected == 6)
    {
      do_stroke = TRUE;
      symbolic = 0xffff;
    }
  else
    {
      do_stroke = TRUE;
      symbolic = selected - 1;
    }

  color = color_editor_get_color (self->stroke_paint);

  path_paintable_set_path_stroke (self->paintable, self->path,
                                  do_stroke, stroke, symbolic, color);
  path_paintable_set_path_stroke_variation (self->paintable, self->path,
                                            min, max);

  gtk_spin_button_set_value (self->min_width, min);
  gtk_spin_button_set_value (self->line_width, width);
  gtk_spin_button_set_value (self->max_width, max);

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

static void
fill_changed (PathEditor *self)
{
  gboolean do_fill;
  unsigned int selected;
  unsigned int symbolic;
  const GdkRGBA *color;
  GskFillRule fill_rule;

  if (self->updating)
    return;

  fill_rule = gtk_drop_down_get_selected (self->fill_rule);

  selected = color_editor_get_color_type (self->fill_paint);
  if (selected == 0)
    {
      do_fill = FALSE;
      symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
    }
  else if (selected == 6)
    {
      do_fill = TRUE;
      symbolic = 0xffff;
    }
  else
    {
      do_fill = TRUE;
      symbolic = selected - 1;
    }

  color = color_editor_get_color (self->fill_paint);

  path_paintable_set_path_fill (self->paintable, self->path,
                                do_fill, fill_rule, symbolic, color);

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

static void
attach_changed (PathEditor *self)
{
  size_t selected;
  float pos;

  if (self->updating)
    return;

  selected = gtk_drop_down_get_selected (self->attach_to);
  pos = gtk_range_get_value (GTK_RANGE (self->attach_at));

  if (selected == 0)
    path_paintable_attach_path (self->paintable, self->path, (size_t) -1, pos);
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

static gboolean
bool_and_and (GObject  *object,
              gboolean  b1,
              gboolean  b2,
              gboolean  b3)
{
  return b1 && b2 && b3;
}

static gboolean
uint_equal (GObject      *object,
            unsigned int  u1,
            unsigned int  u2)
{
  return u1 == u2;
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

      g_clear_object (&self->path_image);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
    }
}

static void
file_launched (GObject      *source,
               GAsyncResult *result,
               gpointer      data)
{
  GtkFileLauncher *launcher = GTK_FILE_LAUNCHER (source);
  g_autoptr (GError) error = NULL;

  if (!gtk_file_launcher_launch_finish (launcher, result, &error))
    {
      g_print ("Failed to launch path editor: %s", error->message);
    }
}

static void
edit_path (PathEditor *self)
{
  GString *str;
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *name = NULL;
  g_autofree char *filename = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GOutputStream) ostream = NULL;
  gssize written;
  g_autoptr (GError) error = NULL;
  GFileMonitor *monitor;
  g_autoptr (GskStroke) stroke = NULL;
  unsigned int symbolic;
  GdkRGBA color;
  const char *linecap[] = { "butt", "round", "square" };
  const char *linejoin[] = { "miter", "round", "bevel" };
  g_autoptr (GskPath) path = NULL;
  g_autoptr(GtkFileLauncher) launcher = NULL;
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

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

  name = g_strdup_printf ("org.gtk.Shaper-path%lu.svg", self->path);
  filename = g_build_filename (g_get_user_cache_dir (), name, NULL);
  file = g_file_new_for_path (filename);
  ostream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));
  if (!ostream)
    {
      show_error (self, "Editing Failed", error->message);
      return;
    }

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

  launcher = gtk_file_launcher_new (file);

  gtk_file_launcher_set_writable (launcher, TRUE);

  gtk_file_launcher_launch (launcher, GTK_WINDOW (root), NULL, file_launched, self);
}

static void
move_path_down (PathEditor *self)
{
  path_paintable_move_path (self->paintable, self->path, self->path + 1);
}

static void
duplicate_path (PathEditor *self)
{
  path_paintable_duplicate_path (self->paintable, self->path);
}

static void
delete_path (PathEditor *self)
{
  path_paintable_delete_path (self->paintable, self->path);
}

static void
repopulate_attach_to (PathEditor *self)
{
  g_autoptr (GtkStringList) model = NULL;

  model = gtk_string_list_new (NULL);
  gtk_string_list_append (model, "None");
  for (size_t i = 0; i < path_paintable_get_n_paths (self->paintable); i++)
    {
      if (i == self->path)
        continue;
      if (path_paintable_get_path_id (self->paintable, i))
        gtk_string_list_take (model, g_strdup (path_paintable_get_path_id (self->paintable, i)));
      else
        gtk_string_list_take (model, g_strdup_printf ("Path %lu", i));
   }
 gtk_drop_down_set_model (self->attach_to, G_LIST_MODEL (model));
}

static void
path_editor_update (PathEditor *self)
{
  if (self->paintable && self->path != (size_t) -1)
    {
      GskPath *path;
      g_autofree char *text = NULL;
      g_autofree char *id = NULL;
      g_autofree char *states = NULL;
      gboolean do_stroke;
      g_autoptr (GskStroke) stroke = gsk_stroke_new (1);
      unsigned int symbolic;
      GdkRGBA color;
      gboolean do_fill;
      GskFillRule fill_rule;
      size_t to;
      float pos;
      float width;
      float min_width, max_width;
      float lower, upper;

      self->updating = TRUE;

      path = path_paintable_get_path (self->paintable, self->path);
      text = gsk_path_to_string (path);
      gtk_label_set_label (GTK_LABEL (self->path_cmds), text);

      gtk_editable_set_text (GTK_EDITABLE (self->id_label), path_paintable_get_path_id (self->paintable, self->path));

      do_stroke = path_paintable_get_path_stroke (self->paintable, self->path,
                                                  stroke, &symbolic, &color);

      gtk_drop_down_set_selected (self->transition_type,
                                  path_paintable_get_path_transition_type (self->paintable, self->path));

      gtk_spin_button_set_value (self->transition_duration,
                                 path_paintable_get_path_transition_duration (self->paintable, self->path));

      gtk_spin_button_set_value (self->transition_delay,
                                 path_paintable_get_path_transition_delay (self->paintable, self->path));

      gtk_drop_down_set_selected (self->transition_easing,
                                  path_paintable_get_path_transition_easing (self->paintable, self->path));

      gtk_range_set_value (GTK_RANGE (self->origin),
                           path_paintable_get_path_origin (self->paintable, self->path));

      gtk_drop_down_set_selected (self->animation_type,
                                  path_paintable_get_path_animation_type (self->paintable, self->path));

      gtk_drop_down_set_selected (self->animation_direction,
                                  path_paintable_get_path_animation_direction (self->paintable, self->path));

      gtk_spin_button_set_value (self->animation_duration,
                                 path_paintable_get_path_animation_duration (self->paintable, self->path));

      if (path_paintable_get_path_animation_repeat (self->paintable, self->path) == G_MAXFLOAT)
        {
          gtk_check_button_set_active (self->infty_check, TRUE);
          gtk_spin_button_set_value (self->animation_repeat, 1);
        }
      else
        {
          gtk_check_button_set_active (self->infty_check, FALSE);
          gtk_spin_button_set_value (self->animation_repeat,
                                     path_paintable_get_path_animation_repeat (self->paintable, self->path));
        }

      gtk_drop_down_set_selected (self->animation_easing,
                                  path_paintable_get_path_animation_easing (self->paintable, self->path));

      mini_graph_set_params (self->mini_graph,
                             path_paintable_get_path_animation_easing (self->paintable, self->path),
                             path_paintable_get_path_animation_mode (self->paintable, self->path),
                             path_paintable_get_path_animation_frames (self->paintable, self->path),
                             path_paintable_get_path_animation_n_frames (self->paintable, self->path));

      gtk_spin_button_set_value (self->animation_segment,
                                 path_paintable_get_path_animation_segment (self->paintable, self->path));

      width = gsk_stroke_get_line_width (stroke);
      path_paintable_get_path_stroke_variation (self->paintable, self->path,
                                                &min_width, &max_width);

      lower = MIN (min_width, 0);
      upper = MAX (max_width, 25);

      if (!do_stroke)
        color_editor_set_color_type (self->stroke_paint, 0);
      else if (symbolic == 0xffff)
        color_editor_set_color_type (self->stroke_paint, 6);
      else
        color_editor_set_color_type (self->stroke_paint, symbolic + 1);

      color_editor_set_color (self->stroke_paint, &color);

      gtk_spin_button_set_value (self->min_width, min_width);
      gtk_spin_button_set_value (self->line_width, width);
      gtk_spin_button_set_value (self->max_width, max_width);

      range_editor_configure (self->width_range,
                              lower, width, upper, min_width, max_width);

      gtk_drop_down_set_selected (self->line_join, (unsigned int) gsk_stroke_get_line_join (stroke));
      gtk_drop_down_set_selected (self->line_cap, (unsigned int) gsk_stroke_get_line_cap (stroke));

      do_fill = path_paintable_get_path_fill (self->paintable, self->path,
                                              &fill_rule, &symbolic, &color);

      if (!do_fill)
        color_editor_set_color_type (self->fill_paint, 0);
      else if (symbolic == 0xffff)
        color_editor_set_color_type (self->fill_paint, 6);
      else
        color_editor_set_color_type (self->fill_paint, symbolic + 1);

      color_editor_set_color (self->fill_paint, &color);

      gtk_drop_down_set_selected (self->fill_rule, (unsigned int) fill_rule);

      repopulate_attach_to (self);
      path_paintable_get_attach_path (self->paintable, self->path, &to, &pos);

      if (to == (size_t) -1)
        gtk_drop_down_set_selected (self->attach_to, 0);
      else if (to < self->path)
        gtk_drop_down_set_selected (self->attach_to, to + 1);
      else
        gtk_drop_down_set_selected (self->attach_to, to);

      gtk_range_set_value (GTK_RANGE (self->attach_at), pos);

      if (self->path + 1 == path_paintable_get_n_paths (self->paintable))
        gtk_widget_set_sensitive (GTK_WIDGET (self->move_down), FALSE);

      self->updating = FALSE;

      g_clear_object (&self->path_image);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
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
                          unsigned int  prop_id,
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
path_editor_get_property (GObject      *object,
                          unsigned int  prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
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

    case PROP_PATH_IMAGE:
      g_value_set_object (value, path_editor_get_path_image (self));
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

  g_clear_object (&self->path_image);
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (path_editor_parent_class)->finalize (object);
}

static void
path_editor_class_init (PathEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  g_type_ensure (RANGE_EDITOR_TYPE);
  g_type_ensure (COLOR_EDITOR_TYPE);
  g_type_ensure (MINI_GRAPH_TYPE);

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

  properties[PROP_PATH_IMAGE] =
    g_param_spec_object ("path-image", NULL, NULL,
                         PATH_PAINTABLE_TYPE,
                         G_PARAM_READABLE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/path-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, PathEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, path_cmds);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, id_label);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, origin);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, transition_type);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, transition_duration);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, transition_delay);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, transition_easing);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, animation_type);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, animation_direction);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, animation_duration);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, animation_segment);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, animation_repeat);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, infty_check);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, animation_easing);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, mini_graph);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, stroke_paint);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, width_range);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, min_width);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, line_width);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, max_width);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, line_join);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, line_cap);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, fill_paint);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, fill_rule);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, attach_to);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, attach_at);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, move_down);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, sg);

  gtk_widget_class_bind_template_callback (widget_class, transition_changed);
  gtk_widget_class_bind_template_callback (widget_class, animation_changed);
  gtk_widget_class_bind_template_callback (widget_class, origin_changed);
  gtk_widget_class_bind_template_callback (widget_class, id_changed);
  gtk_widget_class_bind_template_callback (widget_class, stroke_changed);
  gtk_widget_class_bind_template_callback (widget_class, line_width_changed);
  gtk_widget_class_bind_template_callback (widget_class, fill_changed);
  gtk_widget_class_bind_template_callback (widget_class, attach_changed);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_bool);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_and);
  gtk_widget_class_bind_template_callback (widget_class, uint_equal);
  gtk_widget_class_bind_template_callback (widget_class, edit_path);
  gtk_widget_class_bind_template_callback (widget_class, duplicate_path);
  gtk_widget_class_bind_template_callback (widget_class, move_path_down);
  gtk_widget_class_bind_template_callback (widget_class, delete_path);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* }}} */
/* {{{ Public API */

PathEditor *
path_editor_new (PathPaintable *paintable,
                 size_t         path)
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

  g_clear_object (&self->path_image);

  if (!g_set_object (&self->paintable, paintable))
    return;

  self->path = (size_t) -1;

  path_editor_update (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

PathPaintable *
path_editor_get_paintable (PathEditor *self)
{
  g_return_val_if_fail (PATH_IS_EDITOR (self), NULL);

  return self->paintable;
}

void
path_editor_set_path (PathEditor *self,
                      size_t      path)
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

size_t
path_editor_get_path (PathEditor *self)
{
  g_return_val_if_fail (PATH_IS_EDITOR (self), 0);

  return self->path;
}

void
path_editor_edit_path (PathEditor *self)
{
  g_return_if_fail (PATH_IS_EDITOR (self));

  edit_path (self);
}

/* }}} */

/* vim:set foldmethod=marker: */
