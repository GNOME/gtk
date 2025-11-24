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

#include "shape-editor.h"
#include "path-paintable.h"
#include "color-editor.h"
#include "mini-graph.h"


struct _ShapeEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;
  size_t path;
  PathPaintable *path_image;

  gboolean updating;
  gboolean deleted;
  ShapeAttr externally_editing;

  GtkGrid *grid;
  GtkDropDown *shape_dropdown;
  GtkStack *path_cmds_stack;
  GtkLabel *path_cmds;
  GtkEntry *path_cmds_entry;
  GtkBox *polyline_box;
  GtkEntry *line_x1;
  GtkEntry *line_y1;
  GtkEntry *line_x2;
  GtkEntry *line_y2;
  GtkEntry *circle_cx;
  GtkEntry *circle_cy;
  GtkEntry *circle_r;
  GtkEntry *ellipse_cx;
  GtkEntry *ellipse_cy;
  GtkEntry *ellipse_rx;
  GtkEntry *ellipse_ry;
  GtkEntry *rect_x;
  GtkEntry *rect_y;
  GtkEntry *rect_width;
  GtkEntry *rect_height;
  GtkEntry *rect_rx;
  GtkEntry *rect_ry;
  GtkEditableLabel *id_label;
  GtkDropDown *origin;
  GtkDropDown *transition_type;
  GtkSpinButton *transition_duration;
  GtkSpinButton *transition_delay;
  GtkDropDown *transition_easing;
  GtkDropDown *animation_direction;
  GtkSpinButton *animation_duration;
  GtkSpinButton *animation_repeat;
  GtkSpinButton *animation_segment;
  GtkCheckButton *infty_check;
  GtkDropDown *animation_easing;
  MiniGraph *mini_graph;
  ColorEditor *stroke_paint;
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
  GtkDropDown *paint_order;
  GtkScale *opacity;
  GtkScale *miter_limit;
  GtkStack *clip_path_cmds_stack;
  GtkLabel *clip_path_cmds;
  GtkEntry *clip_path_cmds_entry;
  GtkEntry *transform;
  GtkEntry *filter;
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

enum
{
  LINE,
  RECTANGLE,
  CIRCLE,
  ELLIPSE,
  POLYLINE,
  POLYGON,
  PATH,
};

static void
shape_changed (ShapeEditor *self)
{
  int res = 0;
  double params[6];

  switch (gtk_drop_down_get_selected (self->shape_dropdown))
    {
    case LINE:
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->line_x1)), "%lf", &params[0]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->line_y1)), "%lf", &params[1]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->line_x2)), "%lf", &params[2]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->line_y2)), "%lf", &params[3]);
      if (res == 4)
        path_paintable_set_shape (self->paintable, self->path,
                                  SHAPE_LINE, params, 4);
      break;
    case CIRCLE:
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->circle_cx)), "%lf", &params[0]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->circle_cy)), "%lf", &params[1]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->circle_r)), "%lf", &params[2]);
      if (res == 3)
        path_paintable_set_shape (self->paintable, self->path,
                                  SHAPE_CIRCLE, params, 3);
      break;
    case ELLIPSE:
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->ellipse_cx)), "%lf", &params[0]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->ellipse_cy)), "%lf", &params[1]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->ellipse_rx)), "%lf", &params[2]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->ellipse_ry)), "%lf", &params[3]);
      if (res == 4)
        path_paintable_set_shape (self->paintable, self->path,
                                  SHAPE_ELLIPSE, params, 4);
      break;
    case RECTANGLE:
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->rect_x)), "%lf", &params[0]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->rect_y)), "%lf", &params[1]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->rect_width)), "%lf", &params[2]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->rect_height)), "%lf", &params[3]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->rect_rx)), "%lf", &params[4]);
      res += sscanf (gtk_editable_get_text (GTK_EDITABLE (self->rect_ry)), "%lf", &params[5]);
      if (res == 6)
        path_paintable_set_shape (self->paintable, self->path,
                                  SHAPE_RECT, params, 6);
      break;
    case POLYLINE:
    case POLYGON:
      {
        unsigned int n_rows = 0;
        double *parms;
        unsigned int i;

        for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->polyline_box)); child; child = gtk_widget_get_next_sibling (child))
          n_rows++;

        parms = g_newa (double , 2 * n_rows);

        i = 0;
        for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->polyline_box)); child; child = gtk_widget_get_next_sibling (child))
          {
            GtkWidget *widget = gtk_widget_get_first_child (child);
            res += sscanf (gtk_editable_get_text (GTK_EDITABLE (widget)), "%lf", &parms[i++]);
            widget = gtk_widget_get_next_sibling (widget);
            res += sscanf (gtk_editable_get_text (GTK_EDITABLE (widget)), "%lf", &parms[i++]);
          }

        if (res == 2 * n_rows)
          {
            if (gtk_drop_down_get_selected (self->shape_dropdown) == POLYLINE)
              path_paintable_set_shape (self->paintable, self->path,
                                        SHAPE_POLY_LINE, parms, 2 * n_rows);
            else
              path_paintable_set_shape (self->paintable, self->path,
                                        SHAPE_POLYGON, parms, 2 * n_rows);
          }
      }
      break;
    case PATH:
      break;
    default:
      g_assert_not_reached ();
    }

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

static void
delete_row (GtkWidget   *button,
            ShapeEditor *self)
{
  GtkWidget *row = gtk_widget_get_parent (button);
  gtk_box_remove (GTK_BOX (gtk_widget_get_parent (row)), row);
  shape_changed (self);
}

static void
add_row (ShapeEditor *self)
{
  GtkBox *box;
  GtkEntry *entry;
  GtkButton *button;

  box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_widget_add_css_class (GTK_WIDGET (box), "linked");
  entry = GTK_ENTRY (gtk_entry_new ());
  gtk_editable_set_text (GTK_EDITABLE (entry), "0");
  g_signal_connect_swapped (entry, "activate", G_CALLBACK (shape_changed), self);
  gtk_box_append (box, GTK_WIDGET (entry));
  entry = GTK_ENTRY (gtk_entry_new ());
  gtk_editable_set_text (GTK_EDITABLE (entry), "0");
  g_signal_connect_swapped (entry, "activate", G_CALLBACK (shape_changed), self);
  gtk_box_append (box, GTK_WIDGET (entry));
  button = GTK_BUTTON (gtk_button_new_from_icon_name ("user-trash-symbolic"));
  g_signal_connect (button, "clicked", G_CALLBACK (delete_row), self);
  gtk_box_append (box, GTK_WIDGET (button));

  gtk_box_append (self->polyline_box, GTK_WIDGET (box));
  shape_changed (self);
}

static void
shape_editor_update_path (ShapeEditor *self,
                          GskPath     *path)
{
  g_autofree char *text = NULL;

  path_paintable_set_path (self->paintable, self->path, path);

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);

  text = gsk_path_to_string (path);
  gtk_label_set_label (self->path_cmds, text);
}

static void
path_cmds_clicked (ShapeEditor *self)
{
  gtk_stack_set_visible_child_name (self->path_cmds_stack, "entry");

  gtk_entry_grab_focus_without_selecting (self->path_cmds_entry);
}

static gboolean
path_cmds_key (ShapeEditor     *self,
               unsigned int     keyval,
               unsigned int     keycode,
               GdkModifierType  state)
{
  if (keyval == GDK_KEY_Escape)
    {
      GskPath *path;
      g_autofree char *text = NULL;

      path = path_paintable_get_path (self->paintable, self->path);
      text = gsk_path_to_string (path);
      gtk_editable_set_text (GTK_EDITABLE (self->path_cmds_entry), text);

      gtk_widget_remove_css_class (GTK_WIDGET (self->path_cmds_entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->path_cmds_entry), GTK_ACCESSIBLE_STATE_INVALID);
      gtk_stack_set_visible_child_name (self->path_cmds_stack, "label");
      return TRUE;
    }

  return FALSE;
}

static void
path_cmds_activated (ShapeEditor *self)
{
  const char *text;
  g_autoptr (GskPath) path = NULL;

  text = gtk_editable_get_text (GTK_EDITABLE (self->path_cmds_entry));
  path = gsk_path_parse (text);

  if (!path)
    {
      gtk_widget_error_bell (GTK_WIDGET (self->path_cmds_entry));
      gtk_widget_add_css_class (GTK_WIDGET (self->path_cmds_entry), "error");
      gtk_accessible_update_state (GTK_ACCESSIBLE (self->path_cmds_entry),
                                   GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
      return;
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->path_cmds_entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->path_cmds_entry), GTK_ACCESSIBLE_STATE_INVALID);
      gtk_stack_set_visible_child_name (self->path_cmds_stack, "label");
    }

  shape_editor_update_path (self, path);
}

static void
shape_editor_update_clip_path (ShapeEditor *self,
                               GskPath     *path)
{
  if (gsk_path_is_empty (path))
    {
      gtk_label_set_label (GTK_LABEL (self->clip_path_cmds), "—");
      path_paintable_set_clip_path (self->paintable, self->path, NULL);
    }
  else
    {
      g_autofree char *text = gsk_path_to_string (path);
      gtk_label_set_label (GTK_LABEL (self->clip_path_cmds), text);
      path_paintable_set_clip_path (self->paintable, self->path, path);
    }
}

static void
clip_path_cmds_clicked (ShapeEditor *self)
{
  gtk_stack_set_visible_child_name (self->clip_path_cmds_stack, "entry");

  gtk_entry_grab_focus_without_selecting (self->clip_path_cmds_entry);
}

static gboolean
clip_path_cmds_key (ShapeEditor     *self,
                    unsigned int     keyval,
                    unsigned int     keycode,
                    GdkModifierType  state)
{
  if (keyval == GDK_KEY_Escape)
    {
      GskPath *path;
      g_autofree char *text = NULL;

      path = path_paintable_get_clip_path (self->paintable, self->path);
      text = gsk_path_to_string (path);
      gtk_editable_set_text (GTK_EDITABLE (self->clip_path_cmds_entry), text);

      gtk_widget_remove_css_class (GTK_WIDGET (self->clip_path_cmds_entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->clip_path_cmds_entry), GTK_ACCESSIBLE_STATE_INVALID);
      gtk_stack_set_visible_child_name (self->clip_path_cmds_stack, "label");
      return TRUE;
    }

  return FALSE;
}

static void
clip_path_cmds_activated (ShapeEditor *self)
{
  const char *text;
  g_autoptr (GskPath) path = NULL;

  text = gtk_editable_get_text (GTK_EDITABLE (self->clip_path_cmds_entry));
  path = gsk_path_parse (text);

  if (!path)
    {
      gtk_widget_error_bell (GTK_WIDGET (self->clip_path_cmds_entry));
      gtk_widget_add_css_class (GTK_WIDGET (self->clip_path_cmds_entry), "error");
      gtk_accessible_update_state (GTK_ACCESSIBLE (self->clip_path_cmds_entry),
                                   GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
      return;
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->clip_path_cmds_entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->clip_path_cmds_entry), GTK_ACCESSIBLE_STATE_INVALID);
      gtk_stack_set_visible_child_name (self->clip_path_cmds_stack, "label");
    }

  shape_editor_update_clip_path (self, path);
}

static void
transform_changed (ShapeEditor *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->transform));

  if (path_paintable_set_transform (self->paintable, self->path, text))
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->transform), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->transform), GTK_ACCESSIBLE_STATE_INVALID);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (self->transform));
      gtk_widget_add_css_class (GTK_WIDGET (self->transform), "error");
      gtk_accessible_update_state (GTK_ACCESSIBLE (self->transform),
                                   GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
    }
}

static void
filter_changed (ShapeEditor *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->filter));

  if (path_paintable_set_filter (self->paintable, self->path, text))
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->filter), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->filter), GTK_ACCESSIBLE_STATE_INVALID);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (self->filter));
      gtk_widget_add_css_class (GTK_WIDGET (self->filter), "error");
      gtk_accessible_update_state (GTK_ACCESSIBLE (self->filter),
                                   GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
    }
}

static PathPaintable *
shape_editor_get_path_image (ShapeEditor *self)
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

      self->path_image = path_paintable_new ();
      path_paintable_add_path (self->path_image, path_paintable_get_path (self->paintable, self->path));

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
animation_changed (ShapeEditor *self)
{
  GpaAnimation direction;
  double duration;
  double repeat;
  GpaEasing easing;
  double segment;

  if (self->updating)
    return;

  direction = (GpaAnimation) gtk_drop_down_get_selected (self->animation_direction);
  duration = gtk_spin_button_get_value (self->animation_duration);
  if (gtk_check_button_get_active (self->infty_check))
    repeat = REPEAT_FOREVER;
  else
    repeat = gtk_spin_button_get_value (self->animation_repeat);
  segment = gtk_spin_button_get_value (self->animation_segment);
  easing = (GpaEasing) gtk_drop_down_get_selected (self->animation_easing);

  path_paintable_set_path_animation (self->paintable, self->path, direction, duration, repeat, easing, segment);

  mini_graph_set_easing (self->mini_graph, easing);
}

static void
transition_changed (ShapeEditor *self)
{
  GpaTransition type;
  double duration;
  double delay;
  GpaEasing easing;

  if (self->updating)
    return;

  type = (GpaTransition) gtk_drop_down_get_selected (self->transition_type);
  duration = gtk_spin_button_get_value (self->transition_duration);
  delay = gtk_spin_button_get_value (self->transition_delay);
  easing = (GpaEasing) gtk_drop_down_get_selected (self->transition_easing);

  path_paintable_set_path_transition (self->paintable, self->path, type, duration, delay, easing);
}

static void
origin_changed (ShapeEditor *self)
{
  double origin;

  if (self->updating)
    return;

  origin = gtk_range_get_value (GTK_RANGE (self->origin));
  path_paintable_set_path_origin (self->paintable, self->path, origin);
}

static void
id_changed (ShapeEditor *self)
{
  const char *id;

  if (self->updating)
    return;

  id = gtk_editable_get_text (GTK_EDITABLE (self->id_label));
  if (!path_paintable_set_path_id (self->paintable, self->path, id))
    gtk_widget_error_bell (GTK_WIDGET (self->id_label));
}

static void
paint_order_changed (ShapeEditor *self)
{
  unsigned int value = gtk_drop_down_get_selected (self->paint_order);
  path_paintable_set_paint_order (self->paintable, self->path, value);
}

static void
opacity_changed (ShapeEditor *self)
{
  double value = gtk_range_get_value (GTK_RANGE (self->opacity));
  path_paintable_set_opacity (self->paintable, self->path, value);
}

static void
stroke_changed (ShapeEditor *self)
{
  gboolean do_stroke;
  double width;
  double min, max;
  GskLineJoin line_join;
  GskLineCap line_cap;
  double miter_limit;
  unsigned int selected;
  unsigned int symbolic;
  const GdkRGBA *color;
  g_autoptr (GskStroke) stroke = NULL;

  if (self->updating)
    return;

  line_join = gtk_drop_down_get_selected (self->line_join);
  line_cap = gtk_drop_down_get_selected (self->line_cap);
  miter_limit = gtk_range_get_value (GTK_RANGE (self->miter_limit));

  width = gtk_spin_button_get_value (self->line_width);
  min = gtk_spin_button_get_value (self->min_width);
  max = gtk_spin_button_get_value (self->max_width);

  stroke = gsk_stroke_new (width);
  gsk_stroke_set_line_join (stroke, line_join);
  gsk_stroke_set_line_cap (stroke, line_cap);
  gsk_stroke_set_miter_limit (stroke, miter_limit);

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

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

static void
fill_changed (ShapeEditor *self)
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
attach_changed (ShapeEditor *self)
{
  size_t selected;
  double pos;

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
bool_and_bool_and_uint_equal (GObject  *object,
                              gboolean  b1,
                              gboolean  b2,
                              guint     u1,
                              guint     u2)
{
  return b1 && b2 && (u1 == u2);
}

static gboolean
bool_and_bool_and_uint_one_of_two (GObject  *object,
                                   gboolean  b1,
                                   gboolean  b2,
                                   guint     u1,
                                   guint     u2,
                                   guint     u3)
{
  return b1 && b2 && (u1 == u2 || u1 == u3);
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
show_error (ShapeEditor *self,
            const char  *title,
            const char  *detail)
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
                   ShapeEditor       *self)
{
  if (event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
      g_autoptr (GFileMonitor) the_monitor = monitor;
      g_autoptr (GBytes) bytes = NULL;
      g_autoptr (GError) error = NULL;
      g_autoptr (PathPaintable) paintable = NULL;
      g_autoptr (GskPath) path = NULL;

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

      path = path_paintable_get_path_by_id (paintable, "path0");

      if (self->externally_editing == SHAPE_ATTR_PATH)
        shape_editor_update_path (self, path);
      else if (self->externally_editing == SHAPE_ATTR_CLIP_PATH)
        shape_editor_update_clip_path (self, path);
      else
        g_assert_not_reached ();
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
edit_path_externally (ShapeEditor  *self,
                      GskPath      *path_in,
                      ShapeAttr     attr)
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

  self->externally_editing = attr;

  if (path_in)
    path = path_to_svg_path (path_in);

  stroke = gsk_stroke_new (1);
  path_paintable_get_path_stroke (self->paintable, self->path,
                                  stroke, &symbolic, &color);

  str = g_string_new ("");
  g_string_append_printf (str, "<svg width='%g' height='%g'>\n",
                          path_paintable_get_width (self->paintable),
                          path_paintable_get_height (self->paintable));

  g_string_append (str, "<path id='path0'\n"
                        "      d='");
  if (path)
    gsk_path_print (path, str);

  g_string_append (str, "'\n");

  if (attr == SHAPE_ATTR_PATH)
    g_string_append_printf (str,
                            "      fill='none'\n"
                            "      stroke='black'\n"
                            "      stroke-width='%g'\n"
                            "      stroke-linejoin='%s'\n"
                            "      stroke-linecap='%s'/>\n",
                            gsk_stroke_get_line_width (stroke),
                            linejoin[gsk_stroke_get_line_join (stroke)],
                            linecap[gsk_stroke_get_line_cap (stroke)]);
  else
    g_string_append (str,
                     "      fill='black'\n"
                     "      stroke='none'/>\n");

  g_string_append (str, "</svg>");
  bytes = g_string_free_to_bytes (str);

  name = g_strdup_printf ("org.gtk.Shaper-%s%" G_GSIZE_FORMAT ".svg",
                          attr == SHAPE_ATTR_PATH ? "path" : "clip-path",
                          self->path);

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
edit_path (ShapeEditor *self)
{
  edit_path_externally (self,
                        path_paintable_get_path (self->paintable, self->path),
                        SHAPE_ATTR_PATH);
}

static void
edit_clip_path (ShapeEditor *self)
{
  edit_path_externally (self,
                        path_paintable_get_clip_path (self->paintable, self->path),
                        SHAPE_ATTR_CLIP_PATH);
}

static void
move_path_down (ShapeEditor *self)
{
  path_paintable_move_path (self->paintable, self->path, self->path + 1);
}

static void
duplicate_path (ShapeEditor *self)
{
  path_paintable_duplicate_path (self->paintable, self->path);
}

static void
delete_path (ShapeEditor *self)
{
  self->deleted = TRUE;

  path_paintable_delete_path (self->paintable, self->path);
}

static void
repopulate_attach_to (ShapeEditor *self)
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
        gtk_string_list_take (model, g_strdup_printf ("Path %" G_GSIZE_FORMAT, i));
   }
 gtk_drop_down_set_model (self->attach_to, G_LIST_MODEL (model));
}

static void
paths_changed (ShapeEditor *self)
{
  if (self->deleted)
    return;

  repopulate_attach_to (self);
}

static void
shape_editor_update (ShapeEditor *self)
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
      double pos;
      double width;
      double min_width, max_width;
      double *params;
      size_t n_params;
      char buffer[128];

      self->updating = TRUE;

      path = path_paintable_get_path (self->paintable, self->path);
      text = gsk_path_to_string (path);
      gtk_label_set_label (GTK_LABEL (self->path_cmds), text);
      gtk_editable_set_text (GTK_EDITABLE (self->path_cmds_entry), text);
      g_clear_pointer (&text, g_free);

      gtk_editable_set_text (GTK_EDITABLE (self->line_x1), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->line_y1), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->line_x2), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->line_y2), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->circle_cx), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->circle_cy), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->circle_r), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->ellipse_cx), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->ellipse_cy), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->ellipse_rx), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->ellipse_ry), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->rect_x), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->rect_y), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->rect_width), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->rect_height), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->rect_rx), "0");
      gtk_editable_set_text (GTK_EDITABLE (self->rect_ry), "0");

      n_params = path_paintable_get_n_shape_params (self->paintable, self->path);
      params = g_newa (double, n_params);
      path_paintable_get_shape_params (self->paintable, self->path, params);
      switch ((unsigned int) path_paintable_get_path_shape_type (self->paintable, self->path))
        {
        case SHAPE_LINE:
          gtk_editable_set_text (GTK_EDITABLE (self->line_x1), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[0]));
          gtk_editable_set_text (GTK_EDITABLE (self->line_y1), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[1]));
          gtk_editable_set_text (GTK_EDITABLE (self->line_x2), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[2]));
          gtk_editable_set_text (GTK_EDITABLE (self->line_y2), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[3]));
          gtk_drop_down_set_selected (self->shape_dropdown, LINE);
          break;
        case SHAPE_CIRCLE:
          gtk_editable_set_text (GTK_EDITABLE (self->circle_cx), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[0]));
          gtk_editable_set_text (GTK_EDITABLE (self->circle_cy), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[1]));
          gtk_editable_set_text (GTK_EDITABLE (self->circle_r), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[2]));
          gtk_drop_down_set_selected (self->shape_dropdown, CIRCLE);
          break;
        case SHAPE_ELLIPSE:
          gtk_editable_set_text (GTK_EDITABLE (self->ellipse_cx), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[0]));
          gtk_editable_set_text (GTK_EDITABLE (self->ellipse_cy), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[1]));
          gtk_editable_set_text (GTK_EDITABLE (self->ellipse_rx), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[2]));
          gtk_editable_set_text (GTK_EDITABLE (self->ellipse_ry), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[3]));
          gtk_drop_down_set_selected (self->shape_dropdown, ELLIPSE);
          break;
        case SHAPE_RECT:
          gtk_editable_set_text (GTK_EDITABLE (self->rect_x), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[0]));
          gtk_editable_set_text (GTK_EDITABLE (self->rect_y), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[1]));
          gtk_editable_set_text (GTK_EDITABLE (self->rect_width), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[2]));
          gtk_editable_set_text (GTK_EDITABLE (self->rect_height), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[3]));
          gtk_editable_set_text (GTK_EDITABLE (self->rect_rx), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[4]));
          gtk_editable_set_text (GTK_EDITABLE (self->rect_ry), g_ascii_formatd (buffer, sizeof (buffer), "%g", params[5]));
          gtk_drop_down_set_selected (self->shape_dropdown, RECTANGLE);
          break;
        case SHAPE_PATH:
          gtk_drop_down_set_selected (self->shape_dropdown, PATH);
          break;
        case SHAPE_POLY_LINE:
        case SHAPE_POLYGON:
          // FIXME
          break;
        default:
          g_assert_not_reached ();
        }

      gtk_editable_set_text (GTK_EDITABLE (self->id_label), path_paintable_get_path_id (self->paintable, self->path) ?: "");

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

      gtk_drop_down_set_selected (self->animation_direction,
                                  path_paintable_get_path_animation_direction (self->paintable, self->path));

      gtk_spin_button_set_value (self->animation_duration,
                                 path_paintable_get_path_animation_duration (self->paintable, self->path));

      if (path_paintable_get_path_animation_repeat (self->paintable, self->path) == REPEAT_FOREVER)
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

      mini_graph_set_easing (self->mini_graph,
                             path_paintable_get_path_animation_easing (self->paintable, self->path));

      gtk_spin_button_set_value (self->animation_segment,
                                 path_paintable_get_path_animation_segment (self->paintable, self->path));

      width = gsk_stroke_get_line_width (stroke);
      path_paintable_get_path_stroke_variation (self->paintable, self->path,
                                                &min_width, &max_width);

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

      gtk_drop_down_set_selected (self->line_join, (unsigned int) gsk_stroke_get_line_join (stroke));
      gtk_drop_down_set_selected (self->line_cap, (unsigned int) gsk_stroke_get_line_cap (stroke));
      gtk_range_set_value (GTK_RANGE (self->miter_limit), gsk_stroke_get_miter_limit (stroke));

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

      gtk_drop_down_set_selected (self->paint_order,
                                  path_paintable_get_paint_order (self->paintable, self->path));

      gtk_range_set_value (GTK_RANGE (self->opacity),
                           path_paintable_get_opacity (self->paintable, self->path));

      path = path_paintable_get_clip_path (self->paintable, self->path);
      if (path && !gsk_path_is_empty (path))
        {
          text = gsk_path_to_string (path);
          gtk_label_set_label (GTK_LABEL (self->clip_path_cmds), text);
          gtk_editable_set_text (GTK_EDITABLE (self->clip_path_cmds_entry), text);
          g_clear_pointer (&text, g_free);
        }
      else
        {
          gtk_label_set_label (GTK_LABEL (self->clip_path_cmds), "—");
          gtk_editable_set_text (GTK_EDITABLE (self->clip_path_cmds_entry), "");
        }

      text = path_paintable_get_transform (self->paintable, self->path);
      gtk_editable_set_text (GTK_EDITABLE (self->transform), text);

      g_free (text);
      text = path_paintable_get_filter (self->paintable, self->path);
      gtk_editable_set_text (GTK_EDITABLE (self->filter), text);

      self->updating = FALSE;

      g_clear_object (&self->path_image);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
    }
}

/* }}} */
/* {{{ GObject boilerplate */

struct _ShapeEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (ShapeEditor, shape_editor, GTK_TYPE_WIDGET)

static void
shape_editor_init (ShapeEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
shape_editor_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      shape_editor_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_PATH:
      shape_editor_set_path (self, g_value_get_uint64 (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shape_editor_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_PATH:
      g_value_set_uint64 (value, self->path);
      break;

    case PROP_PATH_IMAGE:
      g_value_set_object (value, shape_editor_get_path_image (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shape_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), SHAPE_EDITOR_TYPE);

  G_OBJECT_CLASS (shape_editor_parent_class)->dispose (object);
}

static void
shape_editor_finalize (GObject *object)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  if (self->paintable)
    g_signal_handlers_disconnect_by_func (self->paintable, paths_changed, self);

  g_clear_object (&self->path_image);
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (shape_editor_parent_class)->finalize (object);
}

static void
shape_editor_class_init (ShapeEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  g_type_ensure (COLOR_EDITOR_TYPE);
  g_type_ensure (MINI_GRAPH_TYPE);

  object_class->set_property = shape_editor_set_property;
  object_class->get_property = shape_editor_get_property;
  object_class->dispose = shape_editor_dispose;
  object_class->finalize = shape_editor_finalize;

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/shape-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, shape_dropdown);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, path_cmds_stack);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, path_cmds);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, path_cmds_entry);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, polyline_box);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, line_x1);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, line_y1);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, line_x2);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, line_y2);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, circle_cx);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, circle_cy);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, circle_r);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, ellipse_cx);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, ellipse_cy);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, ellipse_rx);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, ellipse_ry);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, rect_x);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, rect_y);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, rect_width);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, rect_height);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, rect_rx);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, rect_ry);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, id_label);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, origin);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, transition_type);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, transition_duration);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, transition_delay);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, transition_easing);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, animation_direction);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, animation_duration);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, animation_segment);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, animation_repeat);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, infty_check);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, animation_easing);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, mini_graph);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, stroke_paint);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, min_width);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, line_width);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, max_width);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, line_join);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, line_cap);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, fill_paint);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, fill_rule);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, attach_to);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, attach_at);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, move_down);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, sg);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, paint_order);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, opacity);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, miter_limit);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, clip_path_cmds_stack);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, clip_path_cmds);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, clip_path_cmds_entry);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, transform);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, filter);

  gtk_widget_class_bind_template_callback (widget_class, transition_changed);
  gtk_widget_class_bind_template_callback (widget_class, animation_changed);
  gtk_widget_class_bind_template_callback (widget_class, origin_changed);
  gtk_widget_class_bind_template_callback (widget_class, id_changed);
  gtk_widget_class_bind_template_callback (widget_class, paint_order_changed);
  gtk_widget_class_bind_template_callback (widget_class, opacity_changed);
  gtk_widget_class_bind_template_callback (widget_class, stroke_changed);
  gtk_widget_class_bind_template_callback (widget_class, fill_changed);
  gtk_widget_class_bind_template_callback (widget_class, attach_changed);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_bool);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_bool_and_uint_equal);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_bool_and_uint_one_of_two);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_and);
  gtk_widget_class_bind_template_callback (widget_class, uint_equal);
  gtk_widget_class_bind_template_callback (widget_class, edit_path);
  gtk_widget_class_bind_template_callback (widget_class, duplicate_path);
  gtk_widget_class_bind_template_callback (widget_class, move_path_down);
  gtk_widget_class_bind_template_callback (widget_class, delete_path);
  gtk_widget_class_bind_template_callback (widget_class, path_cmds_clicked);
  gtk_widget_class_bind_template_callback (widget_class, path_cmds_activated);
  gtk_widget_class_bind_template_callback (widget_class, path_cmds_key);
  gtk_widget_class_bind_template_callback (widget_class, shape_changed);
  gtk_widget_class_bind_template_callback (widget_class, add_row);
  gtk_widget_class_bind_template_callback (widget_class, delete_row);
  gtk_widget_class_bind_template_callback (widget_class, edit_clip_path);
  gtk_widget_class_bind_template_callback (widget_class, clip_path_cmds_clicked);
  gtk_widget_class_bind_template_callback (widget_class, clip_path_cmds_activated);
  gtk_widget_class_bind_template_callback (widget_class, clip_path_cmds_key);
  gtk_widget_class_bind_template_callback (widget_class, transform_changed);
  gtk_widget_class_bind_template_callback (widget_class, filter_changed);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* }}} */
/* {{{ Public API */

ShapeEditor *
shape_editor_new (PathPaintable *paintable,
                  size_t         path)
{
  return g_object_new (SHAPE_EDITOR_TYPE,
                       "paintable", paintable,
                       "path", path,
                       NULL);
}

void
shape_editor_set_paintable (ShapeEditor    *self,
                            PathPaintable *paintable)
{
  g_return_if_fail (SHAPE_IS_EDITOR (self));
  g_return_if_fail (paintable == NULL || PATH_IS_PAINTABLE (paintable));

  g_clear_object (&self->path_image);

  if (self->paintable)
    g_signal_handlers_disconnect_by_func (self->paintable, paths_changed, self);

  if (!g_set_object (&self->paintable, paintable))
    return;

  self->path = (size_t) -1;

  if (self->paintable)
    g_signal_connect_swapped (self->paintable, "paths-changed", G_CALLBACK (paths_changed), self);

  shape_editor_update (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

PathPaintable *
shape_editor_get_paintable (ShapeEditor *self)
{
  g_return_val_if_fail (SHAPE_IS_EDITOR (self), NULL);

  return self->paintable;
}

void
shape_editor_set_path (ShapeEditor *self,
                       size_t      path)
{
  g_return_if_fail (SHAPE_IS_EDITOR (self));
  g_return_if_fail ((self->paintable == NULL && path == 0) ||
                    (self->paintable && path < path_paintable_get_n_paths (self->paintable)));

  if (self->path == path)
    return;

  self->path = path;

  shape_editor_update (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

size_t
shape_editor_get_path (ShapeEditor *self)
{
  g_return_val_if_fail (SHAPE_IS_EDITOR (self), 0);

  return self->path;
}

void
shape_editor_edit_path (ShapeEditor *self)
{
  g_return_if_fail (SHAPE_IS_EDITOR (self));

  edit_path (self);
}

/* }}} */

/* vim:set foldmethod=marker: */
