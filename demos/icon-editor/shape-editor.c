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
#include "number-editor.h"
#include "path-editor.h"
#include "transform-editor.h"
#include "gtk/svg/gtksvgvalueprivate.h"
#include "gtk/svg/gtksvgnumberprivate.h"
#include "gtk/svg/gtksvgnumbersprivate.h"
#include "gtk/svg/gtksvgenumprivate.h"
#include "gtk/svg/gtksvgtransformprivate.h"
#include "gtk/svg/gtksvgpaintprivate.h"
#include "gtk/svg/gtksvgfilterfunctionsprivate.h"
#include "gtk/svg/gtksvgpathprivate.h"
#include "gtk/svg/gtksvgclipprivate.h"
#include "gtk/svg/gtksvgmaskprivate.h"
#include "gtk/svg/gtksvgelementprivate.h"

struct _ShapeEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;
  SvgElement *shape;
  GdkPaintable *path_image;

  gboolean updating;
  gboolean deleted;
  SvgProperty externally_editing;
  int update_counter;

  GtkGrid *grid;
  GtkDropDown *shape_dropdown;
  PathEditor *path_editor;
  GtkBox *polyline_box;
  NumberEditor *line_x1;
  NumberEditor *line_y1;
  NumberEditor *line_x2;
  NumberEditor *line_y2;
  NumberEditor *circle_cx;
  NumberEditor *circle_cy;
  NumberEditor *circle_r;
  NumberEditor *ellipse_cx;
  NumberEditor *ellipse_cy;
  NumberEditor *ellipse_rx;
  NumberEditor *ellipse_ry;
  NumberEditor *rect_x;
  NumberEditor *rect_y;
  NumberEditor *rect_width;
  NumberEditor *rect_height;
  NumberEditor *rect_rx;
  NumberEditor *rect_ry;
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
  NumberEditor *line_width;
  NumberEditor *min_width;
  NumberEditor *max_width;
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
  GtkDropDown *mask_type;
  GtkDropDown *mask_dropdown;
};

enum
{
  PROP_PATH_IMAGE = 1,
  PROP_UPDATE_COUNTER,
  PROP_PAINTABLE,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ Callbacks */

static void
shape_changed (ShapeEditor *self)
{
  unsigned int old_type, type;

  if (self->updating)
    return;

  old_type = svg_element_get_type (self->shape);
  switch (old_type)
    {
    case SVG_ELEMENT_LINE:
      svg_element_take_base_value (self->shape, SVG_PROPERTY_X1, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_Y1, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_X2, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_Y2, NULL);
      break;
    case SVG_ELEMENT_CIRCLE:
      svg_element_take_base_value (self->shape, SVG_PROPERTY_CX, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_CY, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_R, NULL);
      break;
    case SVG_ELEMENT_ELLIPSE:
      svg_element_take_base_value (self->shape, SVG_PROPERTY_CX, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_CY, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_RX, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_RY, NULL);
      break;
    case SVG_ELEMENT_RECT:
      svg_element_take_base_value (self->shape, SVG_PROPERTY_X, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_Y, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_WIDTH, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_HEIGHT, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_RX, NULL);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_RY, NULL);
      break;
    case SVG_ELEMENT_POLYLINE:
    case SVG_ELEMENT_POLYGON:
      svg_element_take_base_value (self->shape, SVG_PROPERTY_POINTS, NULL);
      break;
    case SVG_ELEMENT_PATH:
      svg_element_take_base_value (self->shape, SVG_PROPERTY_PATH, NULL);
      break;
    default:
      break;
    }

  type = gtk_drop_down_get_selected (self->shape_dropdown);
  svg_element_set_type (self->shape, (SvgElementType) type);
  switch (type)
    {
    case SVG_ELEMENT_LINE:
      {
        double x1, y1, x2, y2;
        SvgUnit ux1, uy1, ux2, uy2;

        number_editor_get (self->line_x1, &x1, &ux1);
        number_editor_get (self->line_y1, &y1, &uy1);
        number_editor_get (self->line_x2, &x2, &ux2);
        number_editor_get (self->line_y2, &y2, &uy2);

        svg_element_take_base_value (self->shape, SVG_PROPERTY_X1, svg_number_new_full (ux1, x1));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_Y1, svg_number_new_full (uy1, y1));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_X2, svg_number_new_full (ux2, x2));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_Y2, svg_number_new_full (uy2, y2));
        path_paintable_changed (self->paintable);
      }
      break;
    case SVG_ELEMENT_CIRCLE:
      {
        double cx, cy, r;
        SvgUnit ux, uy, ur;

        number_editor_get (self->circle_cx, &cx, &ux);
        number_editor_get (self->circle_cy, &cy, &uy);
        number_editor_get (self->circle_r, &r, &ur);

        svg_element_take_base_value (self->shape, SVG_PROPERTY_CX, svg_number_new_full (ux, cx));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_CY, svg_number_new_full (uy, cy));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_R, svg_number_new_full (ur, r));
        path_paintable_changed (self->paintable);
      }
      break;
    case SVG_ELEMENT_ELLIPSE:
      {
        double cx, cy, rx, ry;
        SvgUnit ux, uy, urx, ury;

        number_editor_get (self->ellipse_cx, &cx, &ux);
        number_editor_get (self->ellipse_cy, &cy, &uy);
        number_editor_get (self->ellipse_rx, &rx, &urx);
        number_editor_get (self->ellipse_ry, &ry, &ury);

        svg_element_take_base_value (self->shape, SVG_PROPERTY_CX, svg_number_new_full (ux, cx));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_CY, svg_number_new_full (uy, cy));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_RX, svg_number_new_full (urx, rx));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_RY, svg_number_new_full (ury, ry));
        path_paintable_changed (self->paintable);
      }
      break;
    case SVG_ELEMENT_RECT:
      {
        double x, y, width, height, rx, ry;
        SvgUnit ux, uy, uw, uh, urx, ury;

        number_editor_get (self->rect_x, &x, &ux);
        number_editor_get (self->rect_y, &y, &uy);
        number_editor_get (self->rect_width, &width, &uw);
        number_editor_get (self->rect_height, &height, &uh);
        number_editor_get (self->rect_rx, &rx, &urx);
        number_editor_get (self->rect_ry, &ry, &ury);

        svg_element_take_base_value (self->shape, SVG_PROPERTY_X, svg_number_new_full (ux, x));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_Y, svg_number_new_full (uy, y));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_WIDTH, svg_number_new_full (uw, width));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_HEIGHT, svg_number_new_full (uh, height));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_RX, svg_number_new_full (urx, rx));
        svg_element_take_base_value (self->shape, SVG_PROPERTY_RY, svg_number_new_full (ury, ry));
        path_paintable_changed (self->paintable);
      }
      break;
    case SVG_ELEMENT_POLYLINE:
    case SVG_ELEMENT_POLYGON:
      {
        unsigned int n_rows = 0;
        double *parms;
        SvgUnit *units;
        unsigned int i;

        for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->polyline_box)); child; child = gtk_widget_get_next_sibling (child))
          n_rows++;

        parms = g_newa (double, 2 * n_rows);
        units = g_newa (SvgUnit, 2 * n_rows);

        i = 0;
        for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->polyline_box)); child; child = gtk_widget_get_next_sibling (child))
          {
            GtkWidget *editor;

            editor = gtk_widget_get_first_child (child);
            number_editor_get (NUMBER_EDITOR (editor), &parms[2 * i], &units[2 * i]);
            editor = gtk_widget_get_next_sibling (editor);
            number_editor_get (NUMBER_EDITOR (editor), &parms[2 * i + 1], &units[2 * i + 1]);
            i++;
          }

        svg_element_take_base_value (self->shape, SVG_PROPERTY_POINTS, svg_numbers_new_full (parms, units, 2 * n_rows));
        path_paintable_changed (self->paintable);
      }
      break;
    case SVG_ELEMENT_PATH: // handled in shape_editor_update_path
    case SVG_ELEMENT_IMAGE:
    case SVG_ELEMENT_GROUP:
    case SVG_ELEMENT_CLIP_PATH:
    case SVG_ELEMENT_MASK:
    case SVG_ELEMENT_DEFS:
    case SVG_ELEMENT_USE:
    case SVG_ELEMENT_PATTERN:
    case SVG_ELEMENT_MARKER:
    case SVG_ELEMENT_TEXT:
    case SVG_ELEMENT_TSPAN:
    case SVG_ELEMENT_SVG:
    case SVG_ELEMENT_SYMBOL:
    case SVG_ELEMENT_SWITCH:
    case SVG_ELEMENT_LINK:
    case SVG_ELEMENT_LINEAR_GRADIENT:
    case SVG_ELEMENT_RADIAL_GRADIENT:
    case SVG_ELEMENT_FILTER:
      path_paintable_changed (self->paintable);
      break;
    default:
      g_assert_not_reached ();
    }

  if (type != old_type)
    path_paintable_paths_changed (self->paintable);

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_COUNTER]);
}

static void
polyline_delete_row (GtkWidget   *button,
                     ShapeEditor *self)
{
  GtkWidget *row = gtk_widget_get_parent (button);
  gtk_box_remove (GTK_BOX (gtk_widget_get_parent (row)), row);
  shape_changed (self);
}

static void
polyline_add_row (ShapeEditor *self)
{
  GtkBox *box;
  NumberEditor *entry;
  GtkButton *button;

  box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  entry = number_editor_new ();
  g_signal_connect_swapped (entry, "notify::value", G_CALLBACK (shape_changed), self);
  g_signal_connect_swapped (entry, "notify::unit", G_CALLBACK (shape_changed), self);
  gtk_box_append (box, GTK_WIDGET (entry));
  entry = number_editor_new ();
  g_signal_connect_swapped (entry, "notify::value", G_CALLBACK (shape_changed), self);
  g_signal_connect_swapped (entry, "notify::unit", G_CALLBACK (shape_changed), self);
  gtk_box_append (box, GTK_WIDGET (entry));
  button = GTK_BUTTON (gtk_button_new_from_icon_name ("user-trash-symbolic"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), "Delete Point");
  g_signal_connect (button, "clicked", G_CALLBACK (polyline_delete_row), self);
  gtk_box_append (box, GTK_WIDGET (button));

  gtk_box_append (self->polyline_box, GTK_WIDGET (box));
  shape_changed (self);
}

static void
shape_editor_update_path (ShapeEditor *self,
                          GskPath     *path)
{
  if (self->updating)
    return;

  svg_element_set_type (self->shape, SVG_ELEMENT_PATH);
  svg_element_take_base_value (self->shape, SVG_PROPERTY_PATH, svg_path_new (path));
  path_paintable_changed (self->paintable);

  g_clear_object (&self->path_image);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
}

static void
path_changed (ShapeEditor *self)
{
  if (self->updating)
    return;

  shape_editor_update_path (self, path_editor_get_path (self->path_editor));
}

static void
shape_editor_update_clip_path (ShapeEditor *self,
                               GskPath     *path,
                               const char  *id)
{
  if (self->updating)
    return;

  if ((path == NULL || gsk_path_is_empty (path)) &&
      (id == NULL || *id == '\0'))
    {
      svg_element_take_base_value (self->shape, SVG_PROPERTY_CLIP_PATH, NULL);
    }
  else if (path && !gsk_path_is_empty (path))
    {
      char *s = gsk_path_to_string (path);
      svg_element_take_base_value (self->shape, SVG_PROPERTY_CLIP_PATH, svg_clip_new_path (s, 0xffff));
      g_free (s);
    }
  else
    {
      svg_element_take_base_value (self->shape, SVG_PROPERTY_CLIP_PATH, svg_clip_new_url_take (g_strdup_printf ("#%s", id)));
    }

  path_paintable_changed (self->paintable);
}

static void
clip_path_changed (ShapeEditor *self)
{
  GskPath *path = path_editor_get_path (self->clip_path_editor);
  const char *id = path_editor_get_id (self->clip_path_editor);
  shape_editor_update_clip_path (self, path, id);
}

static void
set_transform (ShapeEditor *self,
               SvgValue    *tf)
{
  svg_element_take_base_value (self->shape, SVG_PROPERTY_TRANSFORM, tf);
  path_paintable_changed (self->paintable);
  gtk_widget_remove_css_class (GTK_WIDGET (self->transform), "error");
  gtk_accessible_reset_state (GTK_ACCESSIBLE (self->transform), GTK_ACCESSIBLE_STATE_INVALID);
}

static void
mask_changed (ShapeEditor *self)
{
  if (self->updating)
    return;

  if (gtk_drop_down_get_selected (self->mask_dropdown) == 0)
    {
      svg_element_take_base_value (self->shape, SVG_PROPERTY_MASK, NULL);
    }
  else
    {
      const char *id;

      id = gtk_string_object_get_string (GTK_STRING_OBJECT (gtk_drop_down_get_selected_item (self->mask_dropdown)));
      svg_element_take_base_value (self->shape, SVG_PROPERTY_MASK, svg_mask_new_url_take (g_strdup_printf ("#%s", id)));
    }

  path_paintable_changed (self->paintable);
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

  n = svg_transform_get_length (tf);

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
    value = svg_filter_functions_parse (text);
  else
    value = svg_filter_functions_parse ("none");

  if (value)
    {
      svg_element_take_base_value (self->shape, SVG_PROPERTY_FILTER, value);
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
  SvgValue *value = svg_transform_new_none ();
  add_transform_editor (self, value);
  svg_value_unref (value);
}

static GdkPaintable *
shape_editor_get_path_image (ShapeEditor *self)
{
  if (!self->path_image)
    self->path_image = shape_get_path_image (self->shape,
                                             path_paintable_get_svg (self->paintable));

  return self->path_image;
}

static void
animation_changed (ShapeEditor *self)
{
  GpaAnimation animation, old_animation;
  double duration;
  int64_t old_duration;
  double repeat, old_repeat;
  GpaEasing easing, old_easing;
  double segment, old_segment;

  if (self->updating)
    return;

  animation = (GpaAnimation) gtk_drop_down_get_selected (self->animation_direction);
  duration = gtk_spin_button_get_value (self->animation_duration);
  if (gtk_check_button_get_active (self->infty_check))
    repeat = REPEAT_FOREVER;
  else
    repeat = gtk_spin_button_get_value (self->animation_repeat);
  segment = gtk_spin_button_get_value (self->animation_segment);
  easing = (GpaEasing) gtk_drop_down_get_selected (self->animation_easing);

  svg_element_get_gpa_animation (self->shape, &old_animation, &old_easing, &old_duration, &old_repeat, &old_segment);
  if (old_animation == animation &&
      old_duration == duration * G_TIME_SPAN_MILLISECOND &&
      old_repeat == repeat &&
      old_easing == easing &&
      old_segment == segment)
    return;

  svg_element_set_gpa_animation (self->shape, animation, easing, duration * G_TIME_SPAN_MILLISECOND, repeat, segment);
  path_paintable_changed (self->paintable);

  mini_graph_set_easing (self->mini_graph, easing);
}

static void
transition_changed (ShapeEditor *self)
{
  GpaTransition transition, old_transition;
  double duration;
  int64_t old_duration;
  double delay;
  int64_t old_delay;
  GpaEasing easing, old_easing;

  if (self->updating)
    return;

  transition = (GpaTransition) gtk_drop_down_get_selected (self->transition_type);
  duration = gtk_spin_button_get_value (self->transition_duration);
  delay = gtk_spin_button_get_value (self->transition_delay);
  easing = (GpaEasing) gtk_drop_down_get_selected (self->transition_easing);

  svg_element_get_gpa_transition (self->shape, &old_transition, &old_easing, &old_duration, &old_delay);
  if (old_transition == transition &&
      old_duration == duration * G_TIME_SPAN_MILLISECOND &&
      old_delay == delay * G_TIME_SPAN_MILLISECOND &&
      old_easing == easing)
    return;

  svg_element_set_gpa_transition (self->shape, transition, easing, duration * G_TIME_SPAN_MILLISECOND, delay * G_TIME_SPAN_MILLISECOND);
  path_paintable_changed (self->paintable);
}

static void
origin_changed (ShapeEditor *self)
{
  double origin;

  if (self->updating)
    return;

  origin = gtk_range_get_value (GTK_RANGE (self->origin));
  if (svg_element_get_gpa_origin (self->shape) == origin)
    return;

  svg_element_set_gpa_origin (self->shape, origin);
  path_paintable_changed (self->paintable);
}

static void
id_changed (ShapeEditor *self)
{
  const char *id;

  if (self->updating)
    return;

  id = gtk_editable_get_text (GTK_EDITABLE (self->id_label));
  svg_element_set_id (self->shape, id);
  path_paintable_changed (self->paintable);
}

static void
paint_order_changed (ShapeEditor *self)
{
  unsigned int value = gtk_drop_down_get_selected (self->paint_order);

  if (self->updating)
    return;

  svg_element_take_base_value (self->shape, SVG_PROPERTY_PAINT_ORDER, svg_paint_order_new (value));
  path_paintable_changed (self->paintable);
}

static void
opacity_changed (ShapeEditor *self)
{
  double value = alpha_editor_get_alpha (self->opacity);

  if (self->updating)
    return;

  svg_element_take_base_value (self->shape, SVG_PROPERTY_OPACITY, svg_number_new (value));
  path_paintable_changed (self->paintable);
}

static void
stroke_changed (ShapeEditor *self)
{
  gboolean do_stroke;
  double width, stroke_width;
  SvgUnit width_unit, stroke_width_unit;
  double min, max, stroke_min, stroke_max;
  SvgUnit min_unit, max_unit, stroke_min_unit, stroke_max_unit;
  GskLineJoin line_join, linejoin;
  GskLineCap line_cap, linecap;
  double miter_limit, miterlimit;
  unsigned int selected;
  unsigned int symbolic, stroke_symbolic;
  const GdkRGBA *color;
  GdkRGBA stroke_color;
  PaintKind kind;
  SvgValue *value;

  if (self->updating)
    return;

  line_join = gtk_drop_down_get_selected (self->line_join);
  line_cap = gtk_drop_down_get_selected (self->line_cap);
  miter_limit = gtk_range_get_value (GTK_RANGE (self->miter_limit));

  number_editor_get (self->line_width, &width, &width_unit);
  number_editor_get (self->min_width, &min, &min_unit);
  number_editor_get (self->max_width, &max, &max_unit);

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

  stroke_symbolic = 0xffff;
  stroke_color = (GdkRGBA) { 0, 0, 0, 1 };

  value = ref_value (self->shape, SVG_PROPERTY_STROKE);
  kind = svg_paint_get_kind (value);

  if (kind == PAINT_SYMBOLIC)
    stroke_symbolic = svg_paint_get_symbolic (value);
  else if (kind == PAINT_COLOR)
    gdk_color_to_float (svg_paint_get_color (value), GDK_COLOR_STATE_SRGB, (float *) &stroke_color);
  svg_value_unref (value);

  value = ref_value (self->shape, SVG_PROPERTY_STROKE_WIDTH);
  stroke_width = svg_number_get (value, 100);
  stroke_width_unit = svg_number_get_unit (value);
  svg_value_unref (value);

  value = ref_value (self->shape, SVG_PROPERTY_STROKE_MINWIDTH);
  stroke_min = svg_number_get (value, 100);
  stroke_min_unit = svg_number_get_unit (value);
  svg_value_unref (value);

  value = ref_value (self->shape, SVG_PROPERTY_STROKE_MAXWIDTH);
  stroke_max = svg_number_get (value, 100);
  stroke_max_unit = svg_number_get_unit (value);
  svg_value_unref (value);

  value = ref_value (self->shape, SVG_PROPERTY_STROKE_LINEJOIN);
  linecap = svg_enum_get (value);
  svg_value_unref (value);
  value = ref_value (self->shape, SVG_PROPERTY_STROKE_LINEJOIN);
  linejoin = svg_enum_get (value);
  svg_value_unref (value);
  value = ref_value (self->shape, SVG_PROPERTY_STROKE_MITERLIMIT);
  miterlimit = svg_number_get (value, 100);
  svg_value_unref (value);

  if (do_stroke == (kind != PAINT_NONE) &&
      width == stroke_width && width_unit == stroke_width_unit &&
      min == stroke_min && min_unit == stroke_min_unit &&
      max == stroke_max && max_unit == stroke_max_unit &&
      linecap == line_cap &&
      linejoin == line_join &&
      miterlimit == miter_limit &&
      stroke_symbolic == symbolic &&
      ((symbolic != 0xffff && stroke_color.alpha == color->alpha) ||
       gdk_rgba_equal (&stroke_color, color)))
    return;

  if (!do_stroke)
    svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE, svg_paint_new_none ());
  else if (symbolic != 0xffff)
    {
      svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE, svg_paint_new_symbolic (symbolic));
      svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE_OPACITY, svg_number_new (color->alpha));
    }
  else
    svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE, svg_paint_new_rgba (color));

  svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE_WIDTH, svg_number_new_full (width_unit, width));
  svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE_MINWIDTH, svg_number_new_full (min_unit, min));
  svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE_MAXWIDTH, svg_number_new_full (max_unit, max));
  svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE_LINECAP, svg_linecap_new (line_cap));
  svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE_LINEJOIN, svg_linejoin_new (line_join));
  svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE_MITERLIMIT, svg_number_new (miter_limit));
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
  SvgValue *value;

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

  fill_symbolic = 0xffff;
  fill_color = (GdkRGBA) { 0, 0, 0, 1 };

  value = ref_value (self->shape, SVG_PROPERTY_FILL);
  kind = svg_paint_get_kind (value);

  if (kind == PAINT_SYMBOLIC)
    fill_symbolic = svg_paint_get_symbolic (value);
  else if (kind == PAINT_COLOR)
    gdk_color_to_float (svg_paint_get_color (value), GDK_COLOR_STATE_SRGB, (float *) &fill_color);
  svg_value_unref (value);

  value = ref_value (self->shape, SVG_PROPERTY_FILL_RULE);
  rule = svg_enum_get (value);
  svg_value_unref (value);

  if (do_fill == (kind != PAINT_NONE) &&
      fill_rule == rule &&
      fill_symbolic == symbolic &&
      ((symbolic != 0xffff && fill_color.alpha == color->alpha) ||
       gdk_rgba_equal (&fill_color, color)))
    return;

  svg_element_take_base_value (self->shape, SVG_PROPERTY_FILL_RULE, svg_fill_rule_new (fill_rule));
  if (!do_fill)
    svg_element_take_base_value (self->shape, SVG_PROPERTY_FILL, svg_paint_new_none ());
  else if (symbolic != 0xffff)
    {
      svg_element_take_base_value (self->shape, SVG_PROPERTY_FILL, svg_paint_new_symbolic (symbolic));
      svg_element_take_base_value (self->shape, SVG_PROPERTY_FILL_OPACITY, svg_number_new (color->alpha));
    }
  else
    svg_element_take_base_value (self->shape, SVG_PROPERTY_FILL, svg_paint_new_rgba (color));

  path_paintable_changed (self->paintable);

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
    svg_element_set_gpa_attachment (self->shape, NULL, pos, NULL);
  else
    {
      const char *id;
      SvgElement *sh;

      id = gtk_string_object_get_string (GTK_STRING_OBJECT (gtk_drop_down_get_selected_item (self->attach_to)));
      sh = path_paintable_get_shape_by_id (self->paintable, id);

      svg_element_set_gpa_attachment (self->shape, id, pos, sh);
    }

  path_paintable_changed (self->paintable);
}

static void
mask_type_changed (ShapeEditor *self)
{
  GskMaskMode mode;

  if (self->updating)
    return;

  switch (gtk_drop_down_get_selected (self->mask_type))
    {
    case 0:
      mode = GSK_MASK_MODE_LUMINANCE;
      break;
    case 1:
      mode = GSK_MASK_MODE_ALPHA;
      break;
    default:
      g_assert_not_reached ();
    }

  svg_element_take_base_value (self->shape, SVG_PROPERTY_STROKE_LINECAP, svg_mask_type_new (mode));

  path_paintable_changed (self->paintable);
}

static gboolean
can_edit_shape (SvgElement *shape)
{
  switch (svg_element_get_type (shape))
    {
    case SVG_ELEMENT_LINE:
    case SVG_ELEMENT_POLYLINE:
    case SVG_ELEMENT_POLYGON:
    case SVG_ELEMENT_RECT:
    case SVG_ELEMENT_CIRCLE:
    case SVG_ELEMENT_ELLIPSE:
    case SVG_ELEMENT_PATH:
    case SVG_ELEMENT_GROUP:
    case SVG_ELEMENT_DEFS:
    case SVG_ELEMENT_CLIP_PATH:
    case SVG_ELEMENT_MASK:
      return TRUE;
    case SVG_ELEMENT_USE:
    case SVG_ELEMENT_LINEAR_GRADIENT:
    case SVG_ELEMENT_RADIAL_GRADIENT:
    case SVG_ELEMENT_PATTERN:
    case SVG_ELEMENT_MARKER:
    case SVG_ELEMENT_TEXT:
    case SVG_ELEMENT_TSPAN:
    case SVG_ELEMENT_SVG:
    case SVG_ELEMENT_IMAGE:
    case SVG_ELEMENT_FILTER:
    case SVG_ELEMENT_SYMBOL:
    case SVG_ELEMENT_SWITCH:
    case SVG_ELEMENT_LINK:
    case SVG_ELEMENT_VIEW:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
bb_and_uint_equal (GObject  *object,
                   gboolean  b1,
                   gboolean  b2,
                   guint     u1,
                   guint     u2,
                   int       dummy)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  return b1 && b2 && (u1 == u2) && can_edit_shape (self->shape);
}

static gboolean
bb_and_uint_unequal (GObject  *object,
                     gboolean  b1,
                     gboolean  b2,
                     guint     u1,
                     guint     u2,
                     int       dummy)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  return b1 && b2 && (u1 != u2) && can_edit_shape (self->shape);
}

static gboolean
bb_and_uint_one_of_two (GObject  *object,
                        gboolean  b1,
                        gboolean  b2,
                        guint     u1,
                        guint     u2,
                        guint     u3,
                        int       dummy)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  return b1 && b2 && (u1 == u2 || u1 == u3) && can_edit_shape (self->shape);
}

static gboolean
bbb_and_uint_unequal (GObject  *object,
                      gboolean  b1,
                      gboolean  b2,
                      gboolean  b3,
                      guint     u1,
                      guint     u2,
                      int       dummy)
{
  return b1 && b2 && b3 && (u1 != u2);
}

static gboolean
bb_and_shape_is_graphical (GObject  *object,
                           gboolean  b1,
                           gboolean  b2,
                           int       dummy)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  return b1 && b2 &&
         can_edit_shape (self->shape) &&
         svg_element_type_is_graphical (svg_element_get_type (self->shape));
}

static gboolean
bb_and_shape_has_children (GObject  *object,
                           gboolean  b1,
                           gboolean  b2,
                           int       dummy)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  return b1 && b2 &&
         can_edit_shape (self->shape) &&
         svg_element_type_is_container (svg_element_get_type (self->shape));
}

static gboolean
bb_and_shape_has_gpa (GObject  *object,
                      gboolean  b1,
                      gboolean  b2,
                      int       dummy)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  return b1 && b2 &&
         can_edit_shape (self->shape) &&
         svg_element_type_is_path (svg_element_get_type (self->shape));
}

static gboolean
bb_and_shape_has_attr (GObject    *object,
                       gboolean    b1,
                       gboolean    b2,
                       const char *name,
                       int       dummy)
{
  ShapeEditor *self = SHAPE_EDITOR (object);
  SvgProperty attr;

  if (strcmp (name, "clip-path") == 0)
    attr = SVG_PROPERTY_CLIP_PATH;
  else if (strcmp (name, "transform") == 0)
    attr = SVG_PROPERTY_TRANSFORM;
  else if (strcmp (name, "filter") == 0)
    attr = SVG_PROPERTY_FILTER;
  else if (strcmp (name, "mask") == 0)
    attr = SVG_PROPERTY_MASK;
  else
    g_assert_not_reached ();

  return b1 && b2 &&
         can_edit_shape (self->shape) &&
         svg_property_applies_to (attr, svg_element_get_type (self->shape));
}

static gboolean
bool_and_no_edit (GObject  *object,
                  gboolean  b1,
                  int       dummy)
{
  ShapeEditor *self = SHAPE_EDITOR (object);

  return b1 && !can_edit_shape (self->shape);
}

static void
move_shape_down (ShapeEditor *self)
{
  SvgElement *parent = svg_element_get_parent (self->shape);

  svg_element_move_child_down (parent, self->shape);
  path_paintable_changed (self->paintable);
  path_paintable_paths_changed (self->paintable);
}

static void
duplicate_shape (ShapeEditor *self)
{
  SvgElement *parent = svg_element_get_parent (self->shape);
  svg_element_add_child (parent, svg_element_duplicate (self->shape, parent));
  path_paintable_changed (self->paintable);
  path_paintable_paths_changed (self->paintable);
}

static void
delete_shape (ShapeEditor *self)
{
  self->deleted = TRUE;
  svg_element_delete (self->shape);
  path_paintable_changed (self->paintable);
  path_paintable_paths_changed (self->paintable);
}

static void
add_shape (ShapeEditor *self)
{
  SvgElement *shape;
  char *id;

  shape = svg_element_new (self->shape, SVG_ELEMENT_PATH);
  svg_element_add_child (self->shape, shape);
  shape_set_default_attrs (shape);
  id = path_paintable_find_unused_id (self->paintable, "path");
  svg_element_set_id (shape, id);
  g_free (id);

  path_paintable_changed (self->paintable);
  path_paintable_paths_changed (self->paintable);
}

typedef struct
{
  GtkStringList *model;
  SvgElement *skip;
} CollectData;

static void
collect_graphical (SvgElement *shape,
                   gpointer    data)
{
  CollectData *d = data;

  if (svg_element_type_is_graphical (svg_element_get_type (shape)) &&
      shape != d->skip &&
      svg_element_get_id (shape) != NULL)
    gtk_string_list_append (d->model, svg_element_get_id (shape));
}

static gboolean
shape_has_ancestor (SvgElement *shape,
                    SvgElement *ancestor)
{
  for (SvgElement *p = svg_element_get_parent (shape); p; p = svg_element_get_parent (p))
    if (p == ancestor)
      return TRUE;

  return FALSE;
}

static void
collect_masks (SvgElement    *shape,
               gpointer  data)
{
  CollectData *d = data;

  if (svg_element_get_type (shape) != SVG_ELEMENT_MASK ||
      svg_element_get_id (shape) == NULL)
    return;

  if (d->skip)
    {
      if (shape == d->skip || shape_has_ancestor (shape, d->skip))
        return;
    }

  gtk_string_list_append (d->model, svg_element_get_id (shape));
}

static void
repopulate_attach_to (ShapeEditor *self)
{
  g_autoptr (GtkStringList) model = NULL;
  CollectData data;

  model = gtk_string_list_new (NULL);
  gtk_string_list_append (model, "None");

  data.model = model;
  data.skip = self->shape;
  svg_element_foreach (path_paintable_get_svg (self->paintable)->content, collect_graphical, &data);
  gtk_drop_down_set_model (self->attach_to, G_LIST_MODEL (model));
}

static void
repopulate_mask (ShapeEditor *self)
{
  g_autoptr (GtkStringList) model  = NULL;
  CollectData data;

  model = gtk_string_list_new (NULL);
  gtk_string_list_append (model, "None");

  data.model = model;
  data.skip = self->shape;
  svg_element_foreach (path_paintable_get_svg (self->paintable)->content, collect_masks, &data);
  gtk_drop_down_set_model (self->mask_dropdown, G_LIST_MODEL (model));
}

static void
paths_changed (ShapeEditor *self)
{
  if (self->deleted)
    return;

  if (svg_element_type_is_graphical (svg_element_get_type (self->shape)))
    repopulate_attach_to (self);

  if (svg_property_applies_to (SVG_PROPERTY_MASK, svg_element_get_type (self->shape)))
    repopulate_mask (self);
}

static void
append_shape_editor (ShapeEditor *self,
                     SvgElement       *shape)
{
  ShapeEditor *pe;

  pe = shape_editor_new (self->paintable, shape);
  gtk_box_append (self->children, GTK_WIDGET (pe));
  gtk_box_append (self->children, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}

static void
populate_children (ShapeEditor *self)
{
  for (unsigned int i = 0; i < svg_element_get_n_children (self->shape); i++)
    {
      SvgElement *shape = svg_element_get_child (self->shape, i);
      append_shape_editor (self, shape);
    }
}

static void
shape_editor_update (ShapeEditor *self)
{
  if (self->shape)
    {
      GtkSvg *svg = path_paintable_get_svg (self->paintable);
      g_autofree char *text = NULL;
      SvgValue *tf;
      unsigned int symbolic;
      GdkRGBA color;
      SvgValue *line_width, *min_width, *max_width;
      graphene_rect_t viewport;
      PaintKind kind;
      unsigned int idx;
      const char *id;
      SvgElementType type;
      SvgValue *value;

      id = svg_element_get_id (self->shape);
      type = svg_element_get_type (self->shape);

      gtk_editable_set_text (GTK_EDITABLE (self->id_label), id ? id : "");

      self->updating = TRUE;

      if (!can_edit_shape (self->shape))
        {
          gtk_drop_down_set_selected (self->shape_dropdown, svg_element_get_type (self->shape));

          self->updating = FALSE;
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_COUNTER]);
          return;
        }

      graphene_rect_init (&viewport, 0, 0, svg->width, svg->height);

      if (svg_element_type_is_graphical (type))
        {
          GskPath *path;

          path = svg_element_get_path (self->shape, &viewport, FALSE);
          path_editor_set_path (self->path_editor, path);
          g_object_set (self->path_editor,
                        "width", svg->width,
                        "height", svg->height,
                        NULL);
        }

      number_editor_set (self->line_x1, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->line_y1, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->line_x2, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->line_y2, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->circle_cx, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->circle_cy, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->circle_r, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->ellipse_cx, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->ellipse_cy, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->ellipse_rx, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->ellipse_ry, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->rect_x, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->rect_y, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->rect_width, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->rect_height, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->rect_rx, 0, SVG_UNIT_NUMBER);
      number_editor_set (self->rect_ry, 0, SVG_UNIT_NUMBER);

      gtk_drop_down_set_selected (self->shape_dropdown, type);
      switch ((unsigned int) type)
        {
        case SVG_ELEMENT_LINE:
          {
            value = ref_value (self->shape, SVG_PROPERTY_X1);
            number_editor_set (self->line_x1, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_Y1);
            number_editor_set (self->line_y1, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_X2);
            number_editor_set (self->line_x2, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_Y2);
            number_editor_set (self->line_y2, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
          }
          break;

        case SVG_ELEMENT_CIRCLE:
          {
            value = ref_value (self->shape, SVG_PROPERTY_CX);
            number_editor_set (self->circle_cx, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_CY);
            number_editor_set (self->circle_cy, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_R);
            number_editor_set (self->circle_r, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
          }
          break;

        case SVG_ELEMENT_ELLIPSE:
          {
            value = ref_value (self->shape, SVG_PROPERTY_CX);
            number_editor_set (self->ellipse_cx, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_CY);
            number_editor_set (self->ellipse_cy, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_RX);
            number_editor_set (self->ellipse_rx, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_RY);
            number_editor_set (self->ellipse_ry, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
          }
          break;

        case SVG_ELEMENT_RECT:
          {
            value = ref_value (self->shape, SVG_PROPERTY_X);
            number_editor_set (self->rect_x, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_Y);
            number_editor_set (self->rect_y, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_WIDTH);
            number_editor_set (self->rect_width, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_HEIGHT);
            number_editor_set (self->rect_height, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_RX);
            number_editor_set (self->rect_rx, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
            value = ref_value (self->shape, SVG_PROPERTY_RY);
            number_editor_set (self->rect_ry, svg_number_get (value, 100), svg_number_get_unit (value));
            svg_value_unref (value);
          }
          break;

        case SVG_ELEMENT_PATH:
        case SVG_ELEMENT_POLYLINE:
        case SVG_ELEMENT_POLYGON:
          break;

        case SVG_ELEMENT_GROUP:
        case SVG_ELEMENT_DEFS:
        case SVG_ELEMENT_CLIP_PATH:
        case SVG_ELEMENT_MASK:
          populate_children (self);
          break;
        default:
          g_assert_not_reached ();
        }

      gtk_editable_set_text (GTK_EDITABLE (self->id_label), id ? id : "");

      if (svg_element_type_is_graphical (type))
        {
          GpaTransition transition;
          GpaEasing easing;
          int64_t duration;
          int64_t delay;
          GpaAnimation animation;
          double repeat;
          double segment;

          svg_element_get_gpa_transition (self->shape, &transition, &easing, &duration, &delay);

          gtk_drop_down_set_selected (self->transition_type, transition);

          gtk_drop_down_set_selected (self->transition_easing, easing);
          gtk_spin_button_set_value (self->transition_duration, duration / (double) G_TIME_SPAN_MILLISECOND);
          gtk_spin_button_set_value (self->transition_delay, delay / (double) G_TIME_SPAN_MILLISECOND);

          gtk_range_set_value (GTK_RANGE (self->origin), svg_element_get_gpa_origin (self->shape));
          svg_element_get_gpa_animation (self->shape, &animation, &easing, &duration, &repeat, &segment);
          gtk_drop_down_set_selected (self->animation_direction, animation);

          gtk_spin_button_set_value (self->animation_duration, duration / (double) G_TIME_SPAN_MILLISECOND);
          if (repeat == REPEAT_FOREVER)
            {
              gtk_check_button_set_active (self->infty_check, TRUE);
              gtk_spin_button_set_value (self->animation_repeat, 1);
            }
          else
            {
              gtk_check_button_set_active (self->infty_check, FALSE);
              gtk_spin_button_set_value (self->animation_repeat, repeat);
            }

          gtk_drop_down_set_selected (self->animation_easing, easing);

          mini_graph_set_easing (self->mini_graph, easing);

          gtk_spin_button_set_value (self->animation_segment, segment);
        }

      symbolic = 0xffff;
      color = (GdkRGBA) { 0, 0, 0, 1 };

      value = ref_value (self->shape, SVG_PROPERTY_STROKE);
      kind = svg_paint_get_kind (value);
      svg_value_unref (value);

      if (kind == PAINT_SYMBOLIC)
        symbolic = svg_paint_get_symbolic (value);
      else if (kind == PAINT_COLOR)
        gdk_color_to_float (svg_paint_get_color (value), GDK_COLOR_STATE_SRGB, (float *) &color);

      if (kind == PAINT_NONE)
        color_editor_set_color_type (self->stroke_paint, 0);
      else if (symbolic == 0xffff)
        color_editor_set_color_type (self->stroke_paint, 6);
      else
        color_editor_set_color_type (self->stroke_paint, symbolic + 1);

      color_editor_set_color (self->stroke_paint, &color);

      line_width = ref_value (self->shape, SVG_PROPERTY_STROKE_WIDTH);
      min_width = ref_value (self->shape, SVG_PROPERTY_STROKE_MINWIDTH);
      max_width = ref_value (self->shape, SVG_PROPERTY_STROKE_MAXWIDTH);

      number_editor_set (self->line_width, svg_number_get (line_width, 100), svg_number_get_unit (line_width));
      number_editor_set (self->min_width, svg_number_get (min_width, 100), svg_number_get_unit (min_width));
      number_editor_set (self->max_width, svg_number_get (max_width, 100), svg_number_get_unit (max_width));
      svg_value_unref (line_width);
      svg_value_unref (min_width);
      svg_value_unref (max_width);

      value = ref_value (self->shape, SVG_PROPERTY_STROKE_LINEJOIN);
      gtk_drop_down_set_selected (self->line_join, svg_enum_get (value));
      svg_value_unref (value);
      value = ref_value (self->shape, SVG_PROPERTY_STROKE_LINECAP);
      gtk_drop_down_set_selected (self->line_cap, svg_enum_get (value));
      svg_value_unref (value);
      value = ref_value (self->shape, SVG_PROPERTY_STROKE_MITERLIMIT);
      gtk_range_set_value (GTK_RANGE (self->miter_limit), svg_number_get (value, 100));
      svg_value_unref (value);

      symbolic = 0xffff;
      color = (GdkRGBA) { 0, 0, 0, 1 };

      value = ref_value (self->shape, SVG_PROPERTY_STROKE);
      kind = svg_paint_get_kind (value);
      svg_value_unref (value);

      if (kind == PAINT_SYMBOLIC)
        symbolic = svg_paint_get_symbolic (value);
      else if (kind == PAINT_COLOR)
        gdk_color_to_float (svg_paint_get_color (value), GDK_COLOR_STATE_SRGB, (float *) &color);

      if (kind == PAINT_NONE)
        color_editor_set_color_type (self->fill_paint, 0);
      else if (symbolic == 0xffff)
        color_editor_set_color_type (self->fill_paint, 6);
      else
        color_editor_set_color_type (self->fill_paint, symbolic + 1);

      color_editor_set_color (self->fill_paint, &color);

      value = ref_value (self->shape, SVG_PROPERTY_FILL_RULE);
      gtk_drop_down_set_selected (self->fill_rule, svg_enum_get (value));
      svg_value_unref (value);

      if (svg_element_type_is_graphical (type))
        {
          double pos;

          repopulate_attach_to (self);

          svg_element_get_gpa_attachment (self->shape, NULL, &pos, NULL);
#if 0
          if (self->shape->gpa.attach.shape == NULL)
            gtk_drop_down_set_selected (self->attach_to, 0);
          else if (to < self->path)
            gtk_drop_down_set_selected (self->attach_to, to + 1);
          else
            gtk_drop_down_set_selected (self->attach_to, to);
#endif

          gtk_range_set_value (GTK_RANGE (self->attach_at), pos);
        }

      for (idx = 0; idx < svg_element_get_n_children (svg_element_get_parent (self->shape)); idx++)
        {
          if (svg_element_get_child (svg_element_get_parent (self->shape), idx) == self->shape)
            break;
        }
      if (idx + 1 == svg_element_get_n_children (svg_element_get_parent (self->shape)))
        gtk_widget_set_sensitive (GTK_WIDGET (self->move_down), FALSE);

      value = ref_value (self->shape, SVG_PROPERTY_PAINT_ORDER);
      gtk_drop_down_set_selected (self->paint_order, svg_enum_get (value));
      svg_value_unref (value);

      value = ref_value (self->shape, SVG_PROPERTY_OPACITY);
      alpha_editor_set_alpha (self->opacity, svg_number_get (value, 1));
      svg_value_unref (value);

      if (svg_property_applies_to (SVG_PROPERTY_CLIP_PATH, type))
        {
          GskPath *path = NULL;
          const char *ref = NULL;

          value = ref_value (self->shape, SVG_PROPERTY_CLIP_PATH);
          if (svg_clip_get_kind (value) == CLIP_PATH)
            {
              path = svg_clip_get_path (value);
              path_editor_set_path (self->clip_path_editor, path);
            }
          else if (svg_clip_get_kind (value) == CLIP_URL)
            {
              ref = svg_clip_get_id (value);
              path_editor_set_id (self->clip_path_editor, ref);
            }
          else
            {
              path = gsk_path_builder_free_to_path (gsk_path_builder_new ());
              path_editor_set_path (self->clip_path_editor, path);
              gsk_path_unref (path);
            }

          g_object_set (self->clip_path_editor,
                        "width", svg->width,
                        "height", svg->height,
                        NULL);
          svg_value_unref (value);
        }

      if (svg_property_applies_to (SVG_PROPERTY_MASK, type))
        {
          SvgValue *initial;
          unsigned int pos = 0;

          repopulate_mask (self);
          value = ref_value (self->shape, SVG_PROPERTY_MASK);
          initial = svg_mask_new_none ();
          if (!svg_value_equal (value, initial))
            {
              GListModel *model;
              const char *ref = NULL;

              model = gtk_drop_down_get_model (self->mask_dropdown);
              ref = svg_mask_get_id (value);
              pos = gtk_string_list_find (GTK_STRING_LIST (model), ref);
              if (pos == G_MAXUINT)
                pos = 0;
            }
          svg_value_unref (initial);
          svg_value_unref (value);

          gtk_drop_down_set_selected (self->mask_dropdown, pos);
        }

      if (svg_property_applies_to (SVG_PROPERTY_TRANSFORM, type))
        {
          value = ref_value (self->shape, SVG_PROPERTY_TRANSFORM);
          text = svg_value_to_string (value);
          svg_value_unref (value);

          if (g_strcmp0 (text, "none") == 0)
            gtk_editable_set_text (GTK_EDITABLE (self->transform), "");
          else
            gtk_editable_set_text (GTK_EDITABLE (self->transform), text);

          tf = svg_transform_parse (text);
          populate_transform (self, tf);
          svg_value_unref (tf);

          g_clear_pointer (&text, g_free);
        }

      if (svg_property_applies_to (SVG_PROPERTY_FILTER, type))
        {
          value = ref_value (self->shape, SVG_PROPERTY_FILTER);
          text = svg_value_to_string (value);
          svg_value_unref (value);

          if (g_strcmp0 (text, "none") == 0)
            gtk_editable_set_text (GTK_EDITABLE (self->filter), "");
          else
            gtk_editable_set_text (GTK_EDITABLE (self->filter), text);

          g_clear_pointer (&text, g_free);
        }

      if (svg_property_applies_to (SVG_PROPERTY_MASK, type))
        {
        }

      if (svg_property_applies_to (SVG_PROPERTY_MASK_TYPE, type))
        {
          value = ref_value (self->shape, SVG_PROPERTY_MASK_TYPE);
          switch (svg_enum_get (value))
            {
            case GSK_MASK_MODE_LUMINANCE:
              gtk_drop_down_set_selected (self->mask_type, 0);
              break;
            case GSK_MASK_MODE_ALPHA:
              gtk_drop_down_set_selected (self->mask_type, 1);
              break;
            default:
              g_assert_not_reached ();
            }
          svg_value_unref (value);
        }

      self->updating = FALSE;
      g_clear_object (&self->path_image);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_IMAGE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_COUNTER]);
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

    case PROP_UPDATE_COUNTER:
      g_value_set_int (value, self->update_counter);
      break;

    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
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
  g_type_ensure (number_editor_get_type ());

  object_class->get_property = shape_editor_get_property;
  object_class->dispose = shape_editor_dispose;
  object_class->finalize = shape_editor_finalize;

  properties[PROP_PATH_IMAGE] =
    g_param_spec_object ("path-image", NULL, NULL,
                         GDK_TYPE_PAINTABLE,
                         G_PARAM_READABLE | G_PARAM_STATIC_NAME);

  properties[PROP_UPDATE_COUNTER] =
    g_param_spec_int ("update-counter", NULL, NULL,
                      0, G_MAXINT, 1,
                      G_PARAM_READABLE | G_PARAM_STATIC_NAME);

  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                        PATH_PAINTABLE_TYPE,
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
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, mask_type);
  gtk_widget_class_bind_template_child (widget_class, ShapeEditor, mask_dropdown);

  gtk_widget_class_bind_template_callback (widget_class, transition_changed);
  gtk_widget_class_bind_template_callback (widget_class, animation_changed);
  gtk_widget_class_bind_template_callback (widget_class, origin_changed);
  gtk_widget_class_bind_template_callback (widget_class, id_changed);
  gtk_widget_class_bind_template_callback (widget_class, paint_order_changed);
  gtk_widget_class_bind_template_callback (widget_class, opacity_changed);
  gtk_widget_class_bind_template_callback (widget_class, stroke_changed);
  gtk_widget_class_bind_template_callback (widget_class, fill_changed);
  gtk_widget_class_bind_template_callback (widget_class, attach_changed);
  gtk_widget_class_bind_template_callback (widget_class, bb_and_uint_equal);
  gtk_widget_class_bind_template_callback (widget_class, bb_and_uint_unequal);
  gtk_widget_class_bind_template_callback (widget_class, bbb_and_uint_unequal);
  gtk_widget_class_bind_template_callback (widget_class, bb_and_uint_one_of_two);
  gtk_widget_class_bind_template_callback (widget_class, bb_and_shape_is_graphical);
  gtk_widget_class_bind_template_callback (widget_class, bb_and_shape_has_children);
  gtk_widget_class_bind_template_callback (widget_class, bb_and_shape_has_gpa);
  gtk_widget_class_bind_template_callback (widget_class, bb_and_shape_has_attr);
  gtk_widget_class_bind_template_callback (widget_class, bool_and_no_edit);
  gtk_widget_class_bind_template_callback (widget_class, duplicate_shape);
  gtk_widget_class_bind_template_callback (widget_class, move_shape_down);
  gtk_widget_class_bind_template_callback (widget_class, delete_shape);
  gtk_widget_class_bind_template_callback (widget_class, add_shape);
  gtk_widget_class_bind_template_callback (widget_class, shape_changed);
  gtk_widget_class_bind_template_callback (widget_class, polyline_add_row);
  gtk_widget_class_bind_template_callback (widget_class, polyline_delete_row);
  gtk_widget_class_bind_template_callback (widget_class, transform_changed);
  gtk_widget_class_bind_template_callback (widget_class, filter_changed);
  gtk_widget_class_bind_template_callback (widget_class, add_primitive_transform);
  gtk_widget_class_bind_template_callback (widget_class, path_changed);
  gtk_widget_class_bind_template_callback (widget_class, clip_path_changed);
  gtk_widget_class_bind_template_callback (widget_class, mask_type_changed);
  gtk_widget_class_bind_template_callback (widget_class, mask_changed);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* }}} */
/* {{{ Public API */

ShapeEditor *
shape_editor_new (PathPaintable *paintable,
                  SvgElement    *shape)
{
  ShapeEditor *self;

  self = g_object_new (SHAPE_EDITOR_TYPE, NULL);
  self->paintable = g_object_ref (paintable);
  g_signal_connect_swapped (paintable, "paths-changed", G_CALLBACK (paths_changed), self);
  self->shape = shape;
  path_editor_set_paintable (self->clip_path_editor, paintable);
  shape_editor_update (self);
  return self;
}

/* }}} */

/* vim:set foldmethod=marker: */
