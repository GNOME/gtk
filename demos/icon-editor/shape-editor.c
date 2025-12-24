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
#include "alpha-editor.h"
#include "path-paintable.h"
#include "color-editor.h"
#include "mini-graph.h"
#include "path-editor.h"
#include "transform-editor.h"


struct _ShapeEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;
  Shape *shape;
  GdkPaintable *path_image;

  gboolean updating;
  gboolean deleted;
  ShapeAttr externally_editing;

  GtkGrid *grid;
  GtkDropDown *shape_dropdown;
  PathEditor *path_editor;
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
  AlphaEditor *opacity;
  GtkScale *miter_limit;
  PathEditor *clip_path_editor;
  GtkEntry *transform;
  GtkBox *transform_box;
  GtkEntry *filter;
  GtkBox *children;
};

enum
{
  PROP_PATH_IMAGE = 1,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

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
  GROUP,
};

static gboolean
get_number_from_entry (GtkEntry *entry,
                       double   *value)
{
  if (!sscanf (gtk_editable_get_text (GTK_EDITABLE (entry)), "%lf", value))
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      gtk_widget_add_css_class (GTK_WIDGET (entry), "error");
      gtk_accessible_update_state (GTK_ACCESSIBLE (entry),
                                   GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
      return FALSE;
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_STATE_INVALID);
      return TRUE;
    }
}

static void
shape_changed (ShapeEditor *self)
{
  int res = 0;

  switch (gtk_drop_down_get_selected (self->shape_dropdown))
    {
    case LINE:
      {
        double x1, y1, x2, y2;

        if (!get_number_from_entry (self->line_x1, &x1) ||
            !get_number_from_entry (self->line_y1, &y1) ||
            !get_number_from_entry (self->line_x2, &x2) ||
            !get_number_from_entry (self->line_y2, &y2))
          return;

        self->shape->type = SHAPE_LINE;
        svg_shape_attr_set (self->shape, SHAPE_ATTR_X1, svg_number_new (x1));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_Y1, svg_number_new (y1));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_X2, svg_number_new (x2));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_Y2, svg_number_new (y2));
        path_paintable_changed (self->paintable);
      }
      break;
    case CIRCLE:
      {
        double cx, cy, r;

        if (!get_number_from_entry (self->circle_cx, &cx) ||
            !get_number_from_entry (self->circle_cy, &cy) ||
            !get_number_from_entry (self->circle_r, &r))
          return;

        self->shape->type = SHAPE_CIRCLE;
        svg_shape_attr_set (self->shape, SHAPE_ATTR_CX, svg_number_new (cx));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_CY, svg_number_new (cy));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_R, svg_number_new (r));
        path_paintable_changed (self->paintable);
      }
      break;
    case ELLIPSE:
      {
        double cx, cy, rx, ry;

        if (!get_number_from_entry (self->ellipse_cx, &cx) ||
            !get_number_from_entry (self->ellipse_cy, &cy) ||
            !get_number_from_entry (self->ellipse_rx, &rx) ||
            !get_number_from_entry (self->ellipse_ry, &ry))
          return;

        self->shape->type = SHAPE_ELLIPSE;
        svg_shape_attr_set (self->shape, SHAPE_ATTR_CX, svg_number_new (cx));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_CY, svg_number_new (cy));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_RX, svg_number_new (rx));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_RY, svg_number_new (ry));
        path_paintable_changed (self->paintable);
      }
      break;
    case RECTANGLE:
      {
        double x, y, width, height, rx, ry;

        if (!get_number_from_entry (self->rect_x, &x) ||
            !get_number_from_entry (self->rect_y, &y) ||
            !get_number_from_entry (self->rect_width, &width) ||
            !get_number_from_entry (self->rect_height, &height) ||
            !get_number_from_entry (self->rect_rx, &rx) ||
            !get_number_from_entry (self->rect_ry, &ry))
          return;

        self->shape->type = SHAPE_RECT;
        svg_shape_attr_set (self->shape, SHAPE_ATTR_X, svg_number_new (x));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_Y, svg_number_new (y));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_WIDTH, svg_number_new (width));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_HEIGHT, svg_number_new (height));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_RX, svg_number_new (rx));
        svg_shape_attr_set (self->shape, SHAPE_ATTR_RY, svg_number_new (ry));
        path_paintable_changed (self->paintable);
      }
      break;
    case POLYLINE:
    case POLYGON:
      {
        unsigned int n_rows = 0;
        double *parms;
        unsigned int i;

        for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->polyline_box)); child; child = gtk_widget_get_next_sibling (child))
          n_rows++;

        parms = g_newa (double, 2 * n_rows);

        i = 0;
        for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->polyline_box)); child; child = gtk_widget_get_next_sibling (child))
          {
            GtkWidget *widget = gtk_widget_get_first_child (child);
            if (!get_number_from_entry (GTK_ENTRY (widget), &parms[i++]))
              return;

            widget = gtk_widget_get_next_sibling (widget);
            if (!get_number_from_entry (GTK_ENTRY (widget), &parms[i++]))
              return;
          }

        if (res != 2 * n_rows)
          return;

        if (gtk_drop_down_get_selected (self->shape_dropdown) == POLYLINE)
          self->shape->type = SHAPE_POLYLINE;
        else
          self->shape->type = SHAPE_POLYGON;

        svg_shape_attr_set (self->shape, SHAPE_ATTR_POINTS, svg_numbers_new (parms, 2 * n_rows));
        path_paintable_changed (self->paintable);
      }
      break;
    case PATH:
    case GROUP:
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
  g_autofree char *s = NULL;

  self->shape->type = SHAPE_PATH;
  s = gsk_path_to_string (path);
  svg_shape_attr_set (self->shape, SHAPE_ATTR_PATH, svg_path_new (s));
  path_paintable_changed (self->paintable);

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

static void
path_changed (ShapeEditor *self)
{
  GskPath *path = path_editor_get_path (self->path_editor);
  shape_editor_update_path (self, path);
}

static void
shape_editor_update_clip_path (ShapeEditor *self,
                               GskPath     *path)
{
  if (gsk_path_is_empty (path))
    {
      svg_shape_attr_set (self->shape, SHAPE_ATTR_CLIP_PATH, svg_clip_new_none ());
    }
  else
    {
      char *s = gsk_path_to_string (path);
      svg_shape_attr_set (self->shape, SHAPE_ATTR_CLIP_PATH, svg_clip_new_path (s));
      g_free (s);
    }
  path_paintable_changed (self->paintable);
}

static void
clip_path_changed (ShapeEditor *self)
{
  GskPath *path = path_editor_get_path (self->clip_path_editor);
  shape_editor_update_clip_path (self, path);
}

static void
set_transform (ShapeEditor *self,
               SvgValue    *tf)
{
  svg_shape_attr_set (self->shape, SHAPE_ATTR_TRANSFORM, tf);
  path_paintable_changed (self->paintable);
  gtk_widget_remove_css_class (GTK_WIDGET (self->transform), "error");
  gtk_accessible_reset_state (GTK_ACCESSIBLE (self->transform), GTK_ACCESSIBLE_STATE_INVALID);
}

static void
primitive_transform_changed (ShapeEditor *self)
{
  GString *s = g_string_new ("");
  SvgValue *value;

  for (GtkWidget *row = gtk_widget_get_first_child (GTK_WIDGET (self->transform_box));
       row != NULL;
       row = gtk_widget_get_next_sibling (row))
    {
      TransformEditor *e = TRANSFORM_EDITOR (gtk_widget_get_first_child (row));
      SvgValue *tf = transform_editor_get_transform (e);
      double params[6] = { 0, };
      TransformType type = svg_transform_get_primitive (tf, 0, params);
      char buffer[128];
      const char *names[] = { "none", "translate", "scale", "rotate", "skew_x", "skew_y", "matrix" };

      if (type == TRANSFORM_NONE)
        continue;

      if (s->len > 0)
        g_string_append_c (s, ' ');

      g_string_append (s, names[type]);
      g_string_append_c (s, '(');

      if (type == TRANSFORM_SKEW_X || type == TRANSFORM_SKEW_Y)
        {
          g_string_append (s, g_ascii_formatd (buffer, sizeof (buffer), "%g", params[0]));
        }
      else if (type == TRANSFORM_ROTATE)
        {
          g_string_append (s, g_ascii_formatd (buffer, sizeof (buffer), "%g", params[0]));
          g_string_append (s, ", ");
          g_string_append (s, g_ascii_formatd (buffer, sizeof (buffer), "%g", params[1]));
          g_string_append (s, ", ");
          g_string_append (s, g_ascii_formatd (buffer, sizeof (buffer), "%g", params[2]));
        }
      else if (type == TRANSFORM_TRANSLATE || type == TRANSFORM_SCALE)
        {
          g_string_append (s, g_ascii_formatd (buffer, sizeof (buffer), "%g", params[0]));
          g_string_append (s, ", ");
          g_string_append (s, g_ascii_formatd (buffer, sizeof (buffer), "%g", params[1]));
        }
      else if (type == TRANSFORM_MATRIX)
        {
          g_string_append (s, g_ascii_formatd (buffer, sizeof (buffer), "%g", params[0]));
          for (unsigned int i = 1; i < 6; i++)
            {
              g_string_append (s, ", ");
              g_string_append (s, g_ascii_formatd (buffer, sizeof (buffer), "%g", params[i]));
            }
        }
      else
        g_assert_not_reached ();

      g_string_append_c (s, ')');
    }

  gtk_editable_set_text (GTK_EDITABLE (self->transform), s->str);

  if (s->len > 0)
    value = svg_transform_parse (s->str);
  else
    value = svg_transform_parse ("none");

  set_transform (self, value);

  g_string_free (s, TRUE);
}

static void
remove_primitive_transform (GtkButton   *button,
                            ShapeEditor *self)
{
  GtkWidget *row = gtk_widget_get_parent (GTK_WIDGET (button));
  gtk_box_remove (GTK_BOX (gtk_widget_get_parent (row)), row);
  primitive_transform_changed (self);
}

static void
add_transform_editor (ShapeEditor *self,
                      SvgValue    *value)
{
  TransformEditor *e;
  GtkBox *box;
  GtkButton *button;

  box = GTK_BOX (gtk_box_new (0, GTK_ORIENTATION_HORIZONTAL));
  gtk_box_append (self->transform_box, GTK_WIDGET (box));
  gtk_widget_set_hexpand (GTK_WIDGET (box), FALSE);

  e = transform_editor_new ();
  transform_editor_set_transform (e, value);
  g_signal_connect_swapped (e, "notify::transform",
                            G_CALLBACK (primitive_transform_changed), self);
  gtk_box_append (box, GTK_WIDGET (e));

  button = GTK_BUTTON (gtk_button_new_from_icon_name ("user-trash-symbolic"));
  gtk_button_set_has_frame (button, FALSE);
  gtk_widget_add_css_class (GTK_WIDGET (button), "circular");
  gtk_widget_set_hexpand (GTK_WIDGET (button), TRUE);
  gtk_widget_set_halign (GTK_WIDGET (button), GTK_ALIGN_END);
  gtk_widget_set_valign (GTK_WIDGET (button), GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), "Remove Transform");
  g_signal_connect (button, "clicked",
                    G_CALLBACK (remove_primitive_transform), self);

  gtk_box_append (box, GTK_WIDGET (button));
}

static void
populate_transform (ShapeEditor *self,
                    SvgValue    *tf)
{
  unsigned int n;

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->transform_box));
       child != NULL;
       child = gtk_widget_get_first_child (GTK_WIDGET (self->transform_box)))
    {
      gtk_box_remove (self->transform_box, child);
    }

  if (!tf)
    return;

  n = svg_transform_get_n_transforms (tf);

  for (unsigned int i = 0; i < n; i++)
    {
      SvgValue *value = svg_transform_get_transform (tf, i);
      add_transform_editor (self, value);
      svg_value_unref (value);
    }
}

static void
transform_changed (ShapeEditor *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->transform));
  SvgValue *tf;

  if (text && *text)
    tf = svg_transform_parse (text);
  else
    tf = svg_transform_parse ("none");

  if (tf)
    {
      populate_transform (self, tf);
      set_transform (self, tf);
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
  SvgValue *value;

  if (text && *text)
    value = svg_filter_parse (text);
  else
    value = svg_filter_parse ("none");

  if (value)
    {
      svg_shape_attr_set (self->shape, SHAPE_ATTR_FILTER, value);
      path_paintable_changed (self->paintable);
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

static void
add_primitive_transform (ShapeEditor *self)
{
  g_print ("add primitive\n");
  SvgValue *value = svg_transform_new_none ();
  add_transform_editor (self, value);
  svg_value_unref (value);
}

static GdkPaintable *
shape_editor_get_path_image (ShapeEditor *self)
{
  if (!self->path_image)
    {
      GtkSvg *svg = gtk_svg_new ();
      g_autoptr (GBytes) bytes = NULL;

      svg->width = path_paintable_get_width (self->paintable);
      svg->height = path_paintable_get_height (self->paintable);

      if (self->shape->type != SHAPE_GROUP)
        g_ptr_array_add (svg->content->shapes, shape_duplicate (self->shape));

      bytes = gtk_svg_serialize (svg);
      g_object_unref (svg);
      svg = gtk_svg_new_from_bytes (bytes);
      gtk_svg_play (svg);
      self->path_image = GDK_PAINTABLE (svg);
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

  if (self->shape->gpa.animation == direction &&
      self->shape->gpa.animation_duration == duration * G_TIME_SPAN_MILLISECOND &&
      self->shape->gpa.animation_repeat == repeat &&
      self->shape->gpa.animation_easing == easing &&
      self->shape->gpa.animation_segment == segment)
    return;

  self->shape->gpa.animation = direction;
  self->shape->gpa.animation_duration = duration * G_TIME_SPAN_MILLISECOND;
  self->shape->gpa.animation_repeat = repeat;
  self->shape->gpa.animation_easing = easing;
  self->shape->gpa.animation_segment = segment;

  path_paintable_changed (self->paintable);

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

  if (self->shape->gpa.transition == type &&
      self->shape->gpa.transition_duration == duration * G_TIME_SPAN_MILLISECOND &&
      self->shape->gpa.transition_delay == delay * G_TIME_SPAN_MILLISECOND &&
      self->shape->gpa.transition_easing == easing)
    return;

  self->shape->gpa.transition = type;
  self->shape->gpa.transition_duration = duration * G_TIME_SPAN_MILLISECOND;
  self->shape->gpa.transition_delay = delay * G_TIME_SPAN_MILLISECOND;
  self->shape->gpa.transition_easing = easing;

  path_paintable_changed (self->paintable);
}

static void
origin_changed (ShapeEditor *self)
{
  double origin;

  if (self->updating)
    return;

  origin = gtk_range_get_value (GTK_RANGE (self->origin));
  if (self->shape->gpa.origin == origin)
    return;

  self->shape->gpa.origin = origin;
  path_paintable_changed (self->paintable);
}

static void
id_changed (ShapeEditor *self)
{
  const char *id;

  if (self->updating)
    return;

  id = gtk_editable_get_text (GTK_EDITABLE (self->id_label));
  if (g_set_str (&self->shape->id, id))
    path_paintable_changed (self->paintable);
  else
    gtk_widget_error_bell (GTK_WIDGET (self->id_label));
}

static void
paint_order_changed (ShapeEditor *self)
{
  unsigned int value = gtk_drop_down_get_selected (self->paint_order);

  if (self->updating)
    return;

  svg_shape_attr_set (self->shape, SHAPE_ATTR_PAINT_ORDER, svg_paint_order_new (value));
  path_paintable_changed (self->paintable);
}

static void
opacity_changed (ShapeEditor *self)
{
  double value = alpha_editor_get_alpha (self->opacity);

  if (self->updating)
    return;

  svg_shape_attr_set (self->shape, SHAPE_ATTR_OPACITY, svg_number_new (value));
  path_paintable_changed (self->paintable);
}

static void
stroke_changed (ShapeEditor *self)
{
  gboolean do_stroke;
  double width, stroke_width;
  double min, max, stroke_min, stroke_max;
  GskLineJoin line_join, linejoin;
  GskLineCap line_cap, linecap;
  double miter_limit, miterlimit;
  unsigned int selected;
  unsigned int symbolic, stroke_symbolic;
  const GdkRGBA *color;
  GdkRGBA stroke_color;
  const graphene_rect_t *viewport;
  PaintKind kind;

  if (self->updating)
    return;

  viewport = path_paintable_get_viewport (self->paintable);

  line_join = gtk_drop_down_get_selected (self->line_join);
  line_cap = gtk_drop_down_get_selected (self->line_cap);
  miter_limit = gtk_range_get_value (GTK_RANGE (self->miter_limit));

  width = gtk_spin_button_get_value (self->line_width);
  min = gtk_spin_button_get_value (self->min_width);
  max = gtk_spin_button_get_value (self->max_width);

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

  kind = svg_shape_attr_get_paint (self->shape, SHAPE_ATTR_STROKE, &stroke_symbolic, &stroke_color);
  stroke_width = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_STROKE_WIDTH, viewport);
  stroke_min = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_STROKE_MINWIDTH, viewport);
  stroke_max = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_STROKE_MAXWIDTH, viewport);
  linecap = svg_shape_attr_get_enum (self->shape, SHAPE_ATTR_STROKE_LINEJOIN);
  linejoin = svg_shape_attr_get_enum (self->shape, SHAPE_ATTR_STROKE_LINEJOIN);
  miterlimit = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_STROKE_MITERLIMIT, viewport);

  if (do_stroke == (kind != PAINT_NONE) &&
      width == stroke_width &&
      min == stroke_min &&
      max == stroke_max &&
      linecap == line_cap &&
      linejoin == line_join &&
      miterlimit == miter_limit &&
      stroke_symbolic == symbolic &&
      ((symbolic != 0xffff && stroke_color.alpha == color->alpha) ||
       gdk_rgba_equal (&stroke_color, color)))
    return;

  if (!do_stroke)
    svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE, svg_paint_new_none ());
  else if (symbolic != 0xffff)
    {
      svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE, svg_paint_new_symbolic (symbolic));
      svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE_OPACITY, svg_number_new (color->alpha));
    }
  else
    svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE, svg_paint_new_rgba (color));

  svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE_WIDTH, svg_number_new (width));
  svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE_MINWIDTH, svg_number_new (min));
  svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE_MAXWIDTH, svg_number_new (max));
  svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE_LINECAP, svg_linecap_new (line_cap));
  svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE_LINEJOIN, svg_linejoin_new (line_join));
  svg_shape_attr_set (self->shape, SHAPE_ATTR_STROKE_MITERLIMIT, svg_number_new (miter_limit));
  path_paintable_changed (self->paintable);

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

static void
fill_changed (ShapeEditor *self)
{
  gboolean do_fill;
  unsigned int selected;
  unsigned int symbolic, fill_symbolic;
  const GdkRGBA *color;
  GdkRGBA fill_color;
  GskFillRule fill_rule, rule;
  PaintKind kind;

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

  kind = svg_shape_attr_get_paint (self->shape, SHAPE_ATTR_FILL, &fill_symbolic, &fill_color);
  rule = svg_shape_attr_get_enum (self->shape, SHAPE_ATTR_FILL_RULE);

  if (do_fill == (kind != PAINT_NONE) &&
      fill_rule == rule &&
      fill_symbolic == symbolic &&
      ((symbolic != 0xffff && fill_color.alpha == color->alpha) ||
       gdk_rgba_equal (&fill_color, color)))
    return;

  svg_shape_attr_set (self->shape, SHAPE_ATTR_FILL_RULE, svg_fill_rule_new (fill_rule));
  if (!do_fill)
    svg_shape_attr_set (self->shape, SHAPE_ATTR_FILL, svg_paint_new_none ());
  else if (symbolic != 0xffff)
    {
      svg_shape_attr_set (self->shape, SHAPE_ATTR_FILL, svg_paint_new_symbolic (symbolic));
      svg_shape_attr_set (self->shape, SHAPE_ATTR_FILL_OPACITY, svg_number_new (color->alpha));
    }
  else
    svg_shape_attr_set (self->shape, SHAPE_ATTR_FILL, svg_paint_new_rgba (color));

  path_paintable_changed (self->paintable);

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

static void
attach_changed (ShapeEditor *self)
{
  const char *id;
  size_t selected;
  double pos;

  if (self->updating)
    return;

  selected = gtk_drop_down_get_selected (self->attach_to);
  pos = gtk_range_get_value (GTK_RANGE (self->attach_at));

  if (selected == 0)
    {
      g_clear_pointer (&self->shape->gpa.attach.ref, g_free);
      self->shape->gpa.attach.shape = NULL;
      self->shape->gpa.attach.pos = pos;
    }
  else
    {
      id = gtk_string_object_get_string (GTK_STRING_OBJECT (gtk_drop_down_get_selected_item (self->attach_to)));
      g_set_str (&self->shape->gpa.attach.ref, id);
      self->shape->gpa.attach.shape = path_paintable_get_shape_by_id (self->paintable, id);
      self->shape->gpa.attach.pos = pos;
    }

  path_paintable_changed (self->paintable);
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
bool_and_bool_and_uint_unequal (GObject  *object,
                                gboolean  b1,
                                gboolean  b2,
                                guint     u1,
                                guint     u2)
{
  return b1 && b2 && (u1 != u2);
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
bool_and_bool_and_uint_one_of_three (GObject  *object,
                                     gboolean  b1,
                                     gboolean  b2,
                                     guint     u1,
                                     guint     u2,
                                     guint     u3,
                                     guint     u4)
{
  return b1 && b2 && (u1 == u2 || u1 == u3 || u1 == u4);
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
move_path_down (ShapeEditor *self)
{
  Shape *parent = self->shape->parent;
  unsigned int idx;

  g_ptr_array_find (parent->shapes, self->shape, &idx);
  g_ptr_array_steal_index (parent->shapes, idx);
  g_ptr_array_insert (parent->shapes, idx + 1, self->shape);
  path_paintable_changed (self->paintable);
  path_paintable_paths_changed (self->paintable);
}

static void
duplicate_path (ShapeEditor *self)
{
  g_ptr_array_add (self->shape->parent->shapes, shape_duplicate (self->shape));
  path_paintable_changed (self->paintable);
  path_paintable_paths_changed (self->paintable);
}

static void
delete_path (ShapeEditor *self)
{
  self->deleted = TRUE;
  svg_shape_delete (self->shape);
  path_paintable_changed (self->paintable);
  path_paintable_paths_changed (self->paintable);
}

static void
repopulate_attach_to_with_shape (ShapeEditor   *self,
                                 Shape         *shape,
                                 GtkStringList *model)
{
  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      Shape *sh = g_ptr_array_index (shape->shapes, i);

      if (sh->type == SHAPE_GROUP)
        {
          repopulate_attach_to_with_shape (self, sh, model);
          continue;
        }
      else if (shape_is_graphical (sh) && sh != self->shape)
        {
          if (sh->id)
            gtk_string_list_take (model, g_strdup (sh->id));
        }
    }
}

static void
repopulate_attach_to (ShapeEditor *self)
{
  g_autoptr (GtkStringList) model = NULL;

  model = gtk_string_list_new (NULL);
  gtk_string_list_append (model, "None");
  repopulate_attach_to_with_shape (self, path_paintable_get_content (self->paintable), model);
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
append_shape_editor (ShapeEditor *self,
                     Shape       *shape)
{
  ShapeEditor *pe;

  pe = shape_editor_new (self->paintable, shape);
  gtk_box_append (self->children, GTK_WIDGET (pe));
  gtk_box_append (self->children, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}

static void
populate_children (ShapeEditor *self)
{
  for (unsigned int i = 0; i < self->shape->shapes->len; i++)
    {
      Shape *shape = g_ptr_array_index (self->shape->shapes, i);

      if (!shape_is_graphical (shape) &&
          shape->type != SHAPE_GROUP)
        continue;

      append_shape_editor (self, shape);
    }
}

static void
shape_editor_update (ShapeEditor *self)
{
  if (self->shape)
    {
      GskPath *path;
      g_autofree char *text = NULL;
      g_autofree char *id = NULL;
      g_autofree char *states = NULL;
      g_autoptr (GskStroke) stroke = gsk_stroke_new (1);
      unsigned int symbolic;
      GdkRGBA color;
      double line_width;
      double min_width, max_width;
      char buffer[128];
      const graphene_rect_t *viewport;
      PaintKind kind;
      unsigned int idx;

      viewport = path_paintable_get_viewport (self->paintable);

      self->updating = TRUE;

      if (self->shape->type != SHAPE_GROUP)
        {
          path = svg_shape_get_path (self->shape, viewport);
          path_editor_set_path (self->path_editor, path);
          g_object_set (self->path_editor,
                        "width", path_paintable_get_width (self->paintable),
                        "height", path_paintable_get_height (self->paintable),
                        NULL);
        }

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

      switch ((unsigned int) self->shape->type)
        {
        case SHAPE_LINE:
          {
            double x1, y1, x2, y2;

            x1 = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_X1, viewport);
            y1 = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_Y1, viewport);
            x2 = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_X2, viewport);
            y2 = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_Y2, viewport);

            gtk_editable_set_text (GTK_EDITABLE (self->line_x1), g_ascii_formatd (buffer, sizeof (buffer), "%g", x1));
            gtk_editable_set_text (GTK_EDITABLE (self->line_y1), g_ascii_formatd (buffer, sizeof (buffer), "%g", y1));
            gtk_editable_set_text (GTK_EDITABLE (self->line_x2), g_ascii_formatd (buffer, sizeof (buffer), "%g", x2));
            gtk_editable_set_text (GTK_EDITABLE (self->line_y2), g_ascii_formatd (buffer, sizeof (buffer), "%g", y2));
            gtk_drop_down_set_selected (self->shape_dropdown, LINE);
          }
          break;

        case SHAPE_CIRCLE:
          {
            double cx, cy, r;

            cx = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_CX, viewport);
            cy = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_CY, viewport);
            r = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_R, viewport);

            gtk_editable_set_text (GTK_EDITABLE (self->circle_cx), g_ascii_formatd (buffer, sizeof (buffer), "%g", cx));
            gtk_editable_set_text (GTK_EDITABLE (self->circle_cy), g_ascii_formatd (buffer, sizeof (buffer), "%g", cy));
            gtk_editable_set_text (GTK_EDITABLE (self->circle_r), g_ascii_formatd (buffer, sizeof (buffer), "%g", r));
            gtk_drop_down_set_selected (self->shape_dropdown, CIRCLE);
          }
          break;

        case SHAPE_ELLIPSE:
          {
            double cx, cy, rx, ry;

            cx = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_CX, viewport);
            cy = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_CY, viewport);
            rx = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_RX, viewport);
            ry = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_RY, viewport);

            gtk_editable_set_text (GTK_EDITABLE (self->ellipse_cx), g_ascii_formatd (buffer, sizeof (buffer), "%g", cx));
            gtk_editable_set_text (GTK_EDITABLE (self->ellipse_cy), g_ascii_formatd (buffer, sizeof (buffer), "%g", cy));
            gtk_editable_set_text (GTK_EDITABLE (self->ellipse_rx), g_ascii_formatd (buffer, sizeof (buffer), "%g", rx));
            gtk_editable_set_text (GTK_EDITABLE (self->ellipse_ry), g_ascii_formatd (buffer, sizeof (buffer), "%g", ry));
            gtk_drop_down_set_selected (self->shape_dropdown, ELLIPSE);
          }
          break;

        case SHAPE_RECT:
          {
            double x, y, width, height, rx, ry;

            x = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_X, viewport);
            y = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_Y, viewport);
            width = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_WIDTH, viewport);
            height = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_HEIGHT, viewport);
            rx = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_RX, viewport);
            ry = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_RY, viewport);

            gtk_editable_set_text (GTK_EDITABLE (self->rect_x), g_ascii_formatd (buffer, sizeof (buffer), "%g", x));
            gtk_editable_set_text (GTK_EDITABLE (self->rect_y), g_ascii_formatd (buffer, sizeof (buffer), "%g", y));
            gtk_editable_set_text (GTK_EDITABLE (self->rect_width), g_ascii_formatd (buffer, sizeof (buffer), "%g", width));
            gtk_editable_set_text (GTK_EDITABLE (self->rect_height), g_ascii_formatd (buffer, sizeof (buffer), "%g", height));
            gtk_editable_set_text (GTK_EDITABLE (self->rect_rx), g_ascii_formatd (buffer, sizeof (buffer), "%g", rx));
            gtk_editable_set_text (GTK_EDITABLE (self->rect_ry), g_ascii_formatd (buffer, sizeof (buffer), "%g", ry));
            gtk_drop_down_set_selected (self->shape_dropdown, RECTANGLE);
          }
          break;

        case SHAPE_PATH:
          gtk_drop_down_set_selected (self->shape_dropdown, PATH);
          break;

        case SHAPE_POLYLINE:
        case SHAPE_POLYGON:
          // FIXME
          break;
        case SHAPE_GROUP:
          gtk_drop_down_set_selected (self->shape_dropdown, GROUP);
          populate_children (self);
          break;
        default:
          g_assert_not_reached ();
        }

      gtk_editable_set_text (GTK_EDITABLE (self->id_label), self->shape->id ? self->shape->id : "");

      gtk_drop_down_set_selected (self->transition_type, self->shape->gpa.transition);

      gtk_spin_button_set_value (self->transition_duration,
                                 self->shape->gpa.transition_duration / (double) G_TIME_SPAN_MILLISECOND);

      gtk_spin_button_set_value (self->transition_delay,
                                 self->shape->gpa.transition_delay / (double) G_TIME_SPAN_MILLISECOND);

      gtk_drop_down_set_selected (self->transition_easing, self->shape->gpa.transition_easing);

      gtk_range_set_value (GTK_RANGE (self->origin), self->shape->gpa.origin);

      gtk_drop_down_set_selected (self->animation_direction, self->shape->gpa.animation);

      gtk_spin_button_set_value (self->animation_duration,
                                 self->shape->gpa.animation_duration / (double) G_TIME_SPAN_MILLISECOND);

      if (self->shape->gpa.animation_repeat == REPEAT_FOREVER)
        {
          gtk_check_button_set_active (self->infty_check, TRUE);
          gtk_spin_button_set_value (self->animation_repeat, 1);
        }
      else
        {
          gtk_check_button_set_active (self->infty_check, FALSE);
          gtk_spin_button_set_value (self->animation_repeat, self->shape->gpa.animation_repeat);
        }

      gtk_drop_down_set_selected (self->animation_easing, self->shape->gpa.animation_easing);

      mini_graph_set_easing (self->mini_graph, self->shape->gpa.animation_easing);

      gtk_spin_button_set_value (self->animation_segment, self->shape->gpa.animation_segment);

      kind = svg_shape_attr_get_paint (self->shape, SHAPE_ATTR_STROKE, &symbolic, &color);

      if (kind == PAINT_NONE)
        color_editor_set_color_type (self->stroke_paint, 0);
      else if (symbolic == 0xffff)
        color_editor_set_color_type (self->stroke_paint, 6);
      else
        color_editor_set_color_type (self->stroke_paint, symbolic + 1);

      color_editor_set_color (self->stroke_paint, &color);

      line_width = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_STROKE_WIDTH, viewport);
      min_width = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_STROKE_MINWIDTH, viewport);
      max_width = svg_shape_attr_get_number (self->shape, SHAPE_ATTR_STROKE_MAXWIDTH, viewport);

      gtk_spin_button_set_value (self->min_width, min_width);
      gtk_spin_button_set_value (self->line_width, line_width);
      gtk_spin_button_set_value (self->max_width, max_width);

      gtk_drop_down_set_selected (self->line_join, svg_shape_attr_get_enum (self->shape, SHAPE_ATTR_STROKE_LINEJOIN));
      gtk_drop_down_set_selected (self->line_cap, svg_shape_attr_get_enum (self->shape, SHAPE_ATTR_STROKE_LINECAP));
      gtk_range_set_value (GTK_RANGE (self->miter_limit), svg_shape_attr_get_number (self->shape, SHAPE_ATTR_STROKE_MITERLIMIT, viewport));

      kind = svg_shape_attr_get_paint (self->shape, SHAPE_ATTR_FILL, &symbolic, &color);

      if (kind == PAINT_NONE)
        color_editor_set_color_type (self->fill_paint, 0);
      else if (symbolic == 0xffff)
        color_editor_set_color_type (self->fill_paint, 6);
      else
        color_editor_set_color_type (self->fill_paint, symbolic + 1);

      color_editor_set_color (self->fill_paint, &color);

      gtk_drop_down_set_selected (self->fill_rule, svg_shape_attr_get_enum (self->shape, SHAPE_ATTR_FILL_RULE));

      repopulate_attach_to (self);

#if 0
      if (self->shape->gpa.attach.shape == NULL)
        gtk_drop_down_set_selected (self->attach_to, 0);
      else if (to < self->path)
        gtk_drop_down_set_selected (self->attach_to, to + 1);
      else
        gtk_drop_down_set_selected (self->attach_to, to);
#endif

      gtk_range_set_value (GTK_RANGE (self->attach_at), self->shape->gpa.attach.pos);

      g_ptr_array_find (self->shape->parent->shapes, self->shape, &idx);
      if (idx + 1 == self->shape->parent->shapes->len)
        gtk_widget_set_sensitive (GTK_WIDGET (self->move_down), FALSE);

      gtk_drop_down_set_selected (self->paint_order,
                                  svg_shape_attr_get_enum (self->shape, SHAPE_ATTR_PAINT_ORDER));

      alpha_editor_set_alpha (self->opacity,
                              svg_shape_attr_get_number (self->shape, SHAPE_ATTR_OPACITY, viewport));

      svg_shape_attr_get_clip (self->shape, SHAPE_ATTR_CLIP_PATH, &path);
      if (path)
        {
          path_editor_set_path (self->clip_path_editor, path);
        }
      else
        {
          path = gsk_path_builder_free_to_path (gsk_path_builder_new ());
          path_editor_set_path (self->clip_path_editor, path);
          gsk_path_unref (path);
        }

      g_object_set (self->clip_path_editor,
                    "width", path_paintable_get_width (self->paintable),
                    "height", path_paintable_get_height (self->paintable),
                    NULL);

      text = svg_shape_attr_get_transform (self->shape, SHAPE_ATTR_TRANSFORM);
      gtk_editable_set_text (GTK_EDITABLE (self->transform), text);

      SvgValue *tf;
      if (text && *text)
        tf = svg_transform_parse (text);
       else
        tf = svg_transform_new_none ();

      populate_transform (self, tf);
      svg_value_unref (tf);

      g_clear_pointer (&text, g_free);

      text = svg_shape_attr_get_filter (self->shape, SHAPE_ATTR_FILTER);
      gtk_editable_set_text (GTK_EDITABLE (self->filter), text);
      g_clear_pointer (&text, g_free);

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
shape_editor_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  switch (prop_id)
    {
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
  g_type_ensure (alpha_editor_get_type ());
  g_type_ensure (MINI_GRAPH_TYPE);
  g_type_ensure (PATH_EDITOR_TYPE);
  g_type_ensure (transform_editor_get_type ());

  object_class->get_property = shape_editor_get_property;
  object_class->dispose = shape_editor_dispose;
  object_class->finalize = shape_editor_finalize;

  properties[PROP_PATH_IMAGE] =
    g_param_spec_object ("path-image", NULL, NULL,
                         GDK_TYPE_PAINTABLE,
                         G_PARAM_READABLE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/shape-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, shape_dropdown);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, path_editor);
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
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, clip_path_editor);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, transform);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, filter);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, children);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, transform_box);

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
  gtk_widget_class_bind_template_callback (widget_class, bool_and_bool_and_uint_unequal);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_bool_and_uint_one_of_two);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_bool_and_uint_one_of_three);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_and);
  gtk_widget_class_bind_template_callback (widget_class, uint_equal);
  gtk_widget_class_bind_template_callback (widget_class, duplicate_path);
  gtk_widget_class_bind_template_callback (widget_class, move_path_down);
  gtk_widget_class_bind_template_callback (widget_class, delete_path);
  gtk_widget_class_bind_template_callback (widget_class, shape_changed);
  gtk_widget_class_bind_template_callback (widget_class, add_row);
  gtk_widget_class_bind_template_callback (widget_class, delete_row);
  gtk_widget_class_bind_template_callback (widget_class, transform_changed);
  gtk_widget_class_bind_template_callback (widget_class, filter_changed);
  gtk_widget_class_bind_template_callback (widget_class, add_primitive_transform);
  gtk_widget_class_bind_template_callback (widget_class, path_changed);
  gtk_widget_class_bind_template_callback (widget_class, clip_path_changed);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* }}} */
/* {{{ Public API */

ShapeEditor *
shape_editor_new (PathPaintable *paintable,
                  Shape         *shape)
{
  ShapeEditor *self = g_object_new (SHAPE_EDITOR_TYPE, NULL);
  self->paintable = g_object_ref (paintable);
  g_signal_connect_swapped (paintable, "paths-changed", G_CALLBACK (paths_changed), self);
  self->shape = shape;
  shape_editor_update (self);
  return self;
}

/* }}} */

/* vim:set foldmethod=marker: */
