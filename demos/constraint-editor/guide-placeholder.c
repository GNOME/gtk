/*
 * Copyright © 2019 Red Hat, Inc.
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
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "guide-placeholder.h"

struct _GuidePlaceholder {
  GtkWidget parent_instance;

  GtkWidget *label;

  GtkConstraintGuide *guide;
};

enum {
  PROP_GUIDE = 1,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];


G_DEFINE_TYPE (GuidePlaceholder, guide_placeholder, GTK_TYPE_WIDGET);

static void
guide_placeholder_measure (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           int             for_size,
                           int            *minimum,
                           int            *natural,
                           int            *minimum_baseline,
                           int            *natural_baseline)
{
  GuidePlaceholder *self = GUIDE_PLACEHOLDER (widget);
  int min_width, min_height;
  int nat_width, nat_height;

  gtk_constraint_guide_get_min_size (self->guide, &min_width, &min_height);
  gtk_constraint_guide_get_nat_size (self->guide, &nat_width, &nat_height);

  gtk_widget_measure (self->label, orientation, for_size, minimum, natural, NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = min_width;
      *natural = nat_width;
    }
  else
    {
      *minimum = min_height;
      *natural = nat_height;
    }
}

static void
guide_placeholder_size_allocate (GtkWidget *widget,
                                 int        width,
                                 int        height,
                                 int        baseline)
{
  GuidePlaceholder *self = GUIDE_PLACEHOLDER (widget);

  gtk_widget_allocate (self->label, width, height, baseline, NULL);
}

static void
guide_changed (GObject *obj, GParamSpec *pspec, GuidePlaceholder *self)
{
  gtk_label_set_label (GTK_LABEL (self->label), gtk_constraint_guide_get_name (self->guide));
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
guide_placeholder_dispose (GObject *object)
{
  GuidePlaceholder *self = GUIDE_PLACEHOLDER (object);

  g_signal_handlers_disconnect_by_func (self->guide, guide_changed, self);
  g_object_unref (self->guide);

  gtk_widget_unparent (self->label);

  G_OBJECT_CLASS (guide_placeholder_parent_class)->dispose (object);
}

static void
guide_placeholder_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GuidePlaceholder *self = GUIDE_PLACEHOLDER (object);

  switch (prop_id)
    {
    case PROP_GUIDE:
      self->guide = g_value_dup_object (value);
      g_signal_connect (self->guide, "notify", G_CALLBACK (guide_changed), self);
      guide_changed ((GObject *)self->guide, NULL, self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
guide_placeholder_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GuidePlaceholder *self = GUIDE_PLACEHOLDER (object);

  switch (prop_id)
    {
    case PROP_GUIDE:
      g_value_set_object (value, self->guide);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
guide_placeholder_class_init (GuidePlaceholderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = guide_placeholder_dispose;
  object_class->set_property = guide_placeholder_set_property;
  object_class->get_property = guide_placeholder_get_property;

  widget_class->measure = guide_placeholder_measure;
  widget_class->size_allocate = guide_placeholder_size_allocate;

  props[PROP_GUIDE] =
    g_param_spec_object ("guide", "guide", "guide",
                         GTK_TYPE_CONSTRAINT_GUIDE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, "guide");
}

static void
guide_placeholder_init (GuidePlaceholder *self)
{
  self->label = gtk_label_new ("");
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));
}

GtkWidget *
guide_placeholder_new (GtkConstraintGuide *guide)
{
  return g_object_new (guide_placeholder_get_type (),
                       "guide", guide,
                       NULL);
}
