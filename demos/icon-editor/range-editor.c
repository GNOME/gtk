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

#include "range-editor.h"

/* {{{ Gizmo */

#define GIZMO_TYPE (gizmo_get_type ())
G_DECLARE_FINAL_TYPE (Gizmo, gizmo, GIZ, MO, GtkWidget)

typedef void    (* GizmoMeasureFunc)   (Gizmo          *gizmo,
                                        GtkOrientation  orientation,
                                        int             for_size,
                                        int            *minimum,
                                        int            *natural,
                                        int            *minimum_baseline,
                                        int            *natural_baseline);
typedef void    (* GizmoAllocateFunc)  (Gizmo          *gizmo,
                                        int             width,
                                        int             height,
                                        int             baseline);
typedef void    (* GizmoSnapshotFunc)  (Gizmo          *gizmo,
                                        GtkSnapshot    *snapshot);

struct _Gizmo
{
  GtkWidget parent_instance;

  GizmoMeasureFunc   measure_func;
  GizmoAllocateFunc  allocate_func;
  GizmoSnapshotFunc  snapshot_func;
};

struct _GizmoClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (Gizmo, gizmo, GTK_TYPE_WIDGET);

static void
gizmo_measure (GtkWidget      *widget,
               GtkOrientation  orientation,
               int             for_size,
               int            *minimum,
               int            *natural,
               int            *minimum_baseline,
               int            *natural_baseline)
{
  Gizmo *self = GIZ_MO (widget);

  if (self->measure_func)
    self->measure_func (self, orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
gizmo_size_allocate (GtkWidget *widget,
                     int        width,
                     int        height,
                     int        baseline)
{
  Gizmo *self = GIZ_MO (widget);

  if (self->allocate_func)
    self->allocate_func (self, width, height, baseline);
}

static void
gizmo_snapshot (GtkWidget   *widget,
                GtkSnapshot *snapshot)
{
  Gizmo *self = GIZ_MO (widget);

  if (self->snapshot_func)
    self->snapshot_func (self, snapshot);
  else
    GTK_WIDGET_CLASS (gizmo_parent_class)->snapshot (widget, snapshot);
}

static void
gizmo_finalize (GObject *object)
{
  Gizmo *self = GIZ_MO (object);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gizmo_parent_class)->finalize (object);
}

static void
gizmo_class_init (GizmoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gizmo_finalize;

  widget_class->measure = gizmo_measure;
  widget_class->size_allocate = gizmo_size_allocate;
  widget_class->snapshot = gizmo_snapshot;
}

static void
gizmo_init (Gizmo *self)
{
}

static GtkWidget *
gizmo_new (const char        *css_name,
           GizmoMeasureFunc   measure_func,
           GizmoAllocateFunc  allocate_func,
           GizmoSnapshotFunc  snapshot_func)
{
  Gizmo *gizmo;

  gizmo = g_object_new (GIZMO_TYPE,
                        "css-name", css_name,
                        "accessible-role", GTK_ACCESSIBLE_ROLE_NONE,
                        NULL);

  gizmo->measure_func  = measure_func;
  gizmo->allocate_func = allocate_func;
  gizmo->snapshot_func = snapshot_func;

  return GTK_WIDGET (gizmo);
}

/* }}} */

struct _RangeEditor
{
  GtkWidget parent_instance;

  float lower;
  float middle;
  float upper;
  float value1;
  float value2;

  GtkWidget *trough;
  GtkWidget *highlight;
  GtkWidget *mark1;
  GtkWidget *mark2;
  GtkWidget *indicator1;
  GtkWidget *indicator2;
  GtkWidget *slider1;
  GtkWidget *slider2;

  GtkGesture *drag_gesture;
  GtkWidget *mouse_location;
};

enum
{
  PROP_LOWER = 1,
  PROP_MIDDLE,
  PROP_UPPER,
  PROP_VALUE1,
  PROP_VALUE2,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

struct _RangeEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (RangeEditor, range_editor, GTK_TYPE_WIDGET)

/* {{{ Setters */

static void
range_editor_set_values (RangeEditor *self,
                         float        value1,
                         float        value2)
{
  g_return_if_fail (self->lower <= value1);
  g_return_if_fail (value1 <= self->middle);
  g_return_if_fail (self->middle <= value2);
  g_return_if_fail (value2 <= self->upper);

  if (self->value1 == value1 && self->value2 == value2)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (self->value1 != value1)
    {
      self->value1 = value1;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE1]);
    }

  if (self->value2 != value2)
    {
      self->value2 = value2;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE2]);
    }

  g_object_thaw_notify (G_OBJECT (self));
  gtk_widget_queue_resize (self->highlight);
  gtk_widget_queue_resize (self->slider1);
  gtk_widget_queue_resize (self->slider2);
}

static void
range_editor_set_limits (RangeEditor *self,
                         float        lower,
                         float        middle,
                         float        upper)
{
  g_return_if_fail (lower <= middle);
  g_return_if_fail (middle <= upper);

  if (self->lower == lower && self->middle == middle && self->upper == upper)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (self->lower != lower)
    {
      self->lower = lower;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOWER]);
    }

  if (self->middle != middle)
    {
      self->middle = middle;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIDDLE]);
    }

  if (self->upper != upper)
    {
      self->upper = upper;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPPER]);
    }

  range_editor_set_values (self,
                           CLAMP (self->value1, self->lower, self->middle),
                           CLAMP (self->value2, self->middle, self->upper));

  g_object_thaw_notify (G_OBJECT (self));
  gtk_widget_queue_resize (self->highlight);
  gtk_widget_queue_resize (self->mark1);
  gtk_widget_queue_resize (self->mark2);
}

/* }}} */
/* {{{ Utilities */

static void
measure_widget (GtkWidget      *widget,
                GtkOrientation  orientation,
                int            *min,
                int            *nat)
{
  gtk_widget_measure (widget, orientation, -1, min, nat, NULL, NULL);
}

/* }}} */
/* {{{ Trough */

static void
measure_trough (Gizmo          *gizmo,
                GtkOrientation  orientation,
                int             for_size,
                int            *minimum,
                int            *natural,
                int            *minimum_baseline,
                int            *natural_baseline)
{
  RangeEditor *self = RANGE_EDITOR (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  int min, nat;

  measure_widget (self->highlight, orientation, minimum, natural);
  measure_widget (self->slider1, orientation, &min, &nat);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum += min;
      *natural += nat;
    }
  else
    {
      *minimum = MAX (*minimum, min);
      *natural = MAX (*natural, nat);
    }

  measure_widget (self->slider2, orientation, &min, &nat);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum += min;
      *natural += nat;
    }
  else
    {
      *minimum = MAX (*minimum, min);
      *natural = MAX (*natural, nat);
    }
}

static void
allocate_trough (Gizmo *gizmo,
                 int    width,
                 int    height,
                 int    baseline)
{
  RangeEditor *self = RANGE_EDITOR (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  GtkAllocation alloc;
  float x1, x2;
  int slider_width, slider_height;

  x1 = width * (self->value1 - self->lower) / (self->upper - self->lower);
  x2 = width * (self->value2 - self->lower) / (self->upper - self->lower);

  alloc.x = x1;
  alloc.y = 0;
  alloc.width = x2 - x1;
  alloc.height = height;

  gtk_widget_size_allocate (self->highlight, &alloc, -1);

  measure_widget (self->slider1, GTK_ORIENTATION_HORIZONTAL, &slider_width, NULL);
  measure_widget (self->slider1, GTK_ORIENTATION_VERTICAL, &slider_height, NULL);

  alloc.x = x1 - slider_width;
  alloc.y = (height - slider_height) / 2;
  alloc.width = slider_width;
  alloc.height = slider_height;

  gtk_widget_size_allocate (self->slider1, &alloc, -1);

  measure_widget (self->slider2, GTK_ORIENTATION_HORIZONTAL, &slider_width, NULL);
  measure_widget (self->slider2, GTK_ORIENTATION_VERTICAL, &slider_height, NULL);

  alloc.x = x2;
  alloc.y = (height - slider_height) / 2;
  alloc.width = slider_width;
  alloc.height = slider_height;

  gtk_widget_size_allocate (self->slider2, &alloc, -1);
}

static void
render_trough (Gizmo       *gizmo,
               GtkSnapshot *snapshot)
{
  RangeEditor *self = RANGE_EDITOR (gtk_widget_get_parent (GTK_WIDGET (gizmo)));

  gtk_widget_snapshot_child (self->trough, self->highlight, snapshot);
  gtk_widget_snapshot_child (self->trough, self->slider1, snapshot);
  gtk_widget_snapshot_child (self->trough, self->slider2, snapshot);
}

/* }}} */
/* {{{ Marks */

static void
measure_mark (Gizmo          *gizmo,
              GtkOrientation  orientation,
              int             for_size,
              int            *minimum,
              int            *natural,
              int            *minimum_baseline,
              int            *natural_baseline)
{
  GtkWidget *indicator;

  indicator = gtk_widget_get_first_child (GTK_WIDGET (gizmo));

  measure_widget (indicator, orientation, minimum, natural);
}

static void
allocate_mark (Gizmo *gizmo,
               int    width,
               int    height,
               int    baseline)
{
  GtkWidget *indicator;
  int min_width, min_height;
  GtkAllocation alloc;

  indicator = gtk_widget_get_first_child (GTK_WIDGET (gizmo));

  measure_widget (indicator, GTK_ORIENTATION_HORIZONTAL, &min_width, NULL);
  measure_widget (indicator, GTK_ORIENTATION_VERTICAL, &min_height, NULL);

  alloc.x = (width - min_width) / 2;
  alloc.y = (height - min_height) / 2;;
  alloc.width = min_width;
  alloc.height = min_height;

  gtk_widget_size_allocate (indicator, &alloc, -1);
}

static void
render_mark (Gizmo       *gizmo,
             GtkSnapshot *snapshot)
{
  GtkWidget *indicator;

  indicator = gtk_widget_get_first_child (GTK_WIDGET (gizmo));

  gtk_widget_snapshot_child (GTK_WIDGET (gizmo), indicator, snapshot);
}

/* }}} */
/* {{{ Input */

static void
drag_gesture_begin (GtkGestureDrag *gesture,
                    double          offset_x,
                    double          offset_y,
                    RangeEditor    *self)
{
  self->mouse_location = gtk_widget_pick (GTK_WIDGET (self), offset_x, offset_y, 0);

  if (self->mouse_location == self->indicator1)
    self->mouse_location = self->mark1;
  else if (self->mouse_location == self->indicator2)
    self->mouse_location = self->mark2;

  if (self->mouse_location == self->slider1 ||
      self->mouse_location == self->slider2 ||
      self->mouse_location == self->mark1 ||
      self->mouse_location == self->mark2)
    gtk_gesture_set_state (self->drag_gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
drag_gesture_end (GtkGestureDrag *gesture,
                  double          offset_x,
                  double          offset_y,
                  RangeEditor    *self)
{
  self->mouse_location = NULL;
}

static void
drag_gesture_update (GtkGestureDrag *gesture,
                     double          offset_x,
                     double          offset_y,
                     RangeEditor    *self)
{
  double start_x, start_y;

  if (self->mouse_location == self->slider1 ||
      self->mouse_location == self->slider2 ||
      self->mouse_location == self->mark1 ||
      self->mouse_location == self->mark2)
    {
      int mouse_x;
      float v;
      graphene_rect_t bounds;

      gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);
      mouse_x = start_x + offset_x;

      if (!gtk_widget_compute_bounds (self->trough, GTK_WIDGET (self), &bounds))
        g_assert_not_reached ();

      v = self->lower + (mouse_x - bounds.origin.x) / bounds.size.width * (self->upper - self->lower);

      if (self->mouse_location == self->slider1)
        {
          v = CLAMP (v, self->lower, self->middle);
          range_editor_set_values (self, v, self->value2);
        }
      else if (self->mouse_location == self->slider2)
        {
          v = CLAMP (v, self->middle, self->upper);
          range_editor_set_values (self, self->value1, v);
        }
      else if (self->mouse_location == self->mark1 ||
               self->mouse_location == self->mark2)
        {
          v = CLAMP (v, self->lower, self->upper);
          range_editor_set_limits (self, self->lower, v, self->upper);
        }
    }
}

/* }}} */
/* {{{ GtkWidget implementation */

static void
range_editor_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  RangeEditor *self = RANGE_EDITOR (widget);
  int min, nat;

  measure_widget (self->trough, orientation, minimum, natural);
  measure_widget (self->mark1, orientation, &min, &nat);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *minimum += min;
      *natural += nat;
    }

  measure_widget (self->mark2, orientation, &min, &nat);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *minimum += min;
      *natural += nat;
    }
}

static void
range_editor_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  RangeEditor *self = RANGE_EDITOR (widget);
  GtkAllocation alloc;
  int min_width, min_height;
  float x;
  int slider1_width, slider2_width;

  measure_widget (self->slider1, GTK_ORIENTATION_HORIZONTAL, &slider1_width, NULL);
  measure_widget (self->slider2, GTK_ORIENTATION_HORIZONTAL, &slider2_width, NULL);
  measure_widget (self->trough, GTK_ORIENTATION_VERTICAL, &min_height, NULL);

  alloc.x = slider1_width;
  alloc.y = (height - min_height) / 2;
  alloc.width = width - slider1_width - slider2_width;
  alloc.height = min_height;

  gtk_widget_size_allocate (self->trough, &alloc, -1);

  x = alloc.width * (self->middle - self->lower) / (self->upper - self->lower);

  measure_widget (self->mark1, GTK_ORIENTATION_HORIZONTAL, &min_width, NULL);
  measure_widget (self->mark1, GTK_ORIENTATION_VERTICAL, &min_height, NULL);

  alloc.x = slider1_width + x - min_width / 2;
  alloc.y = 0;
  alloc.width = min_width;
  alloc.height = min_height;

  gtk_widget_size_allocate (self->mark1, &alloc, -1);

  measure_widget (self->mark2, GTK_ORIENTATION_HORIZONTAL, &min_width, NULL);
  measure_widget (self->mark2, GTK_ORIENTATION_VERTICAL, &min_height, NULL);

  alloc.x = slider1_width + x - min_width / 2;
  alloc.y = height - min_height;
  alloc.width = min_width;
  alloc.height = min_height;

  gtk_widget_size_allocate (self->mark2, &alloc, -1);
}

/* }}} */
/* {{{ GObject boilerplate */

static void
range_editor_init (RangeEditor *self)
{
  self->lower = 0;
  self->middle = 0;
  self->upper = 0;
  self->value1 = 0;
  self->value2 = 0;

  self->trough = gizmo_new ("trough",
                            measure_trough,
                            allocate_trough,
                            render_trough);
  gtk_widget_set_parent (self->trough, GTK_WIDGET (self));

  self->highlight = gizmo_new ("highlight", NULL, NULL, NULL);
  gtk_widget_set_parent (self->highlight, self->trough);

  self->slider1 = gizmo_new ("slider", NULL, NULL, NULL);
  gtk_widget_set_parent (self->slider1, self->trough);
  gtk_widget_add_css_class (self->slider1, "left");

  self->slider2 = gizmo_new ("slider", NULL, NULL, NULL);
  gtk_widget_set_parent (self->slider2, self->trough);
  gtk_widget_add_css_class (self->slider2, "right");

  self->mark1 = gizmo_new ("mark",
                           measure_mark,
                           allocate_mark,
                           render_mark);

  gtk_widget_set_parent (self->mark1, GTK_WIDGET (self));
  gtk_widget_add_css_class (self->mark1, "top");

  self->indicator1 = gizmo_new ("indicator", NULL, NULL, NULL);
  gtk_widget_set_parent (self->indicator1, self->mark1);

  self->mark2 = gizmo_new ("mark",
                           measure_mark,
                           allocate_mark,
                           render_mark);
  gtk_widget_set_parent (self->mark2, GTK_WIDGET (self));
  gtk_widget_add_css_class (self->mark2, "bottom");

  self->indicator2 = gizmo_new ("indicator", NULL, NULL, NULL);
  gtk_widget_set_parent (self->indicator2, self->mark2);

  self->drag_gesture = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->drag_gesture), 0);
  g_signal_connect (self->drag_gesture, "drag-begin",
                    G_CALLBACK (drag_gesture_begin), self);
  g_signal_connect (self->drag_gesture, "drag-update",
                    G_CALLBACK (drag_gesture_update), self);
  g_signal_connect (self->drag_gesture, "drag-end",
                    G_CALLBACK (drag_gesture_end), self);

  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->drag_gesture));
}

static void
range_editor_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  RangeEditor *self = RANGE_EDITOR (object);
  float v;

  switch (prop_id)
    {
    case PROP_LOWER:
      v = g_value_get_float (value);
      range_editor_set_limits (self, v, MAX (v, self->middle), MAX (v, self->upper));
      break;

    case PROP_MIDDLE:
      v = g_value_get_float (value);
      range_editor_set_limits (self, MIN (v, self->lower), v, MAX (v, self->upper));
      break;

    case PROP_UPPER:
      v = g_value_get_float (value);
      range_editor_set_limits (self, MIN (v, self->lower), MIN (v, self->middle), v);
      break;

    case PROP_VALUE1:
      v = g_value_get_float (value);
      range_editor_set_values (self, v, MAX (v, self->value2));
      break;

    case PROP_VALUE2:
      v = g_value_get_float (value);
      range_editor_set_values (self, MIN (v, self->value1), v);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
range_editor_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  RangeEditor *self = RANGE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_LOWER:
      g_value_set_float (value, self->lower);
      break;

    case PROP_MIDDLE:
      g_value_set_float (value, self->middle);
      break;

    case PROP_UPPER:
      g_value_set_float (value, self->upper);
      break;

    case PROP_VALUE1:
      g_value_set_float (value, self->value1);
      break;

    case PROP_VALUE2:
      g_value_set_float (value, self->value2);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
range_editor_dispose (GObject *object)
{
  RangeEditor *self = RANGE_EDITOR (object);

  g_clear_pointer (&self->highlight, gtk_widget_unparent);
  g_clear_pointer (&self->slider1, gtk_widget_unparent);
  g_clear_pointer (&self->slider2, gtk_widget_unparent);
  g_clear_pointer (&self->trough, gtk_widget_unparent);
  g_clear_pointer (&self->indicator1, gtk_widget_unparent);
  g_clear_pointer (&self->mark1, gtk_widget_unparent);
  g_clear_pointer (&self->indicator2, gtk_widget_unparent);
  g_clear_pointer (&self->mark2, gtk_widget_unparent);

  G_OBJECT_CLASS (range_editor_parent_class)->dispose (object);
}

static void
range_editor_finalize (GObject *object)
{
//  RangeEditor *self = RANGE_EDITOR (object);

  G_OBJECT_CLASS (range_editor_parent_class)->finalize (object);
}

static void
range_editor_class_init (RangeEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = range_editor_set_property;
  object_class->get_property = range_editor_get_property;
  object_class->dispose = range_editor_dispose;
  object_class->finalize = range_editor_finalize;

  widget_class->measure = range_editor_measure;
  widget_class->size_allocate = range_editor_size_allocate;

  properties[PROP_LOWER] =
    g_param_spec_float ("lower", NULL, NULL,
                        -G_MAXFLOAT, G_MAXFLOAT, 0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_MIDDLE] =
    g_param_spec_float ("middle", NULL, NULL,
                        -G_MAXFLOAT, G_MAXFLOAT, 0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_UPPER] =
    g_param_spec_float ("upper", NULL, NULL,
                        -G_MAXFLOAT, G_MAXFLOAT, 0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_VALUE1] =
    g_param_spec_float ("value1", NULL, NULL,
                        -G_MAXFLOAT, G_MAXFLOAT, 0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_VALUE2] =
    g_param_spec_float ("value2", NULL, NULL,
                        -G_MAXFLOAT, G_MAXFLOAT, 0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_css_name (widget_class, "rangeeditor");
}

/* }}} */
/* {{{ Public API */

RangeEditor *
range_editor_new (void)
{
  return g_object_new (RANGE_EDITOR_TYPE, NULL);
}

void
range_editor_get_limits (RangeEditor *self,
                         float       *lower,
                         float       *middle,
                         float       *upper)
{
  if (lower)
    *lower = self->lower;

  if (middle)
    *middle = self->middle;

  if (upper)
    *upper = self->upper;
}

void
range_editor_get_values (RangeEditor *self,
                         float       *value1,
                         float       *value2)
{
  if (value1)
    *value1 = self->value1;

  if (value2)
    *value2 = self->value2;
}

void
range_editor_configure (RangeEditor *self,
                        float        lower,
                        float        middle,
                        float        upper,
                        float        value1,
                        float        value2)
{
  g_return_if_fail (lower <= value1);
  g_return_if_fail (value1 <= middle);
  g_return_if_fail (middle <= value2);
  g_return_if_fail (value2 <= upper);

  g_object_freeze_notify (G_OBJECT (self));

  if (self->lower != lower)
    {
      self->lower = lower;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOWER]);
    }

  if (self->middle != middle)
    {
      self->middle = middle;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIDDLE]);
    }

  if (self->upper != upper)
    {
      self->upper = upper;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPPER]);
    }

  if (self->value1 != value1)
    {
      self->value1 = value1;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE1]);
    }

  if (self->value2 != value2)
    {
      self->value2 = value2;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE2]);
    }

  g_object_thaw_notify (G_OBJECT (self));

  gtk_widget_queue_resize (self->highlight);
  gtk_widget_queue_resize (self->mark1);
  gtk_widget_queue_resize (self->mark2);
  gtk_widget_queue_resize (self->slider1);
  gtk_widget_queue_resize (self->slider2);
}

/* }}} */

/* vim:set foldmethod=marker: */
