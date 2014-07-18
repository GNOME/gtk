/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkorientableprivate.h"
#include "gtkseparator.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkseparator
 * @Short_description: A separator widget
 * @Title: GtkSeparator
 *
 * GtkSeparator is a horizontal or vertical separator widget, depending on the 
 * value of the #GtkOrientable:orientation property, used to group the widgets
 * within a window. It displays a line with a shadow to make it appear sunken
 * into the interface.
 */


struct _GtkSeparatorPrivate
{
  GtkOrientation orientation;
};


enum {
  PROP_0,
  PROP_ORIENTATION
};


G_DEFINE_TYPE_WITH_CODE (GtkSeparator, gtk_separator, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkSeparator)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))


static void
gtk_separator_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkSeparator *separator = GTK_SEPARATOR (object);
  GtkSeparatorPrivate *private = separator->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      if (private->orientation != g_value_get_enum (value))
        {
          private->orientation = g_value_get_enum (value);
          _gtk_orientable_set_style_classes (GTK_ORIENTABLE (object));
          gtk_widget_queue_resize (GTK_WIDGET (object));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkSeparator *separator = GTK_SEPARATOR (object);
  GtkSeparatorPrivate *private = separator->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, private->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_get_preferred_size (GtkWidget      *widget,
                                  GtkOrientation  orientation,
                                  gint           *minimum,
                                  gint           *natural)
{
  GtkSeparator *separator = GTK_SEPARATOR (widget);
  GtkSeparatorPrivate *private = separator->priv;
  gboolean wide_sep;
  gint     sep_width;
  gint     sep_height;

  gtk_widget_style_get (widget,
                        "wide-separators",  &wide_sep,
                        "separator-width",  &sep_width,
                        "separator-height", &sep_height,
                        NULL);

  if (orientation == private->orientation)
    {
      *minimum = *natural = 1;
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *minimum = *natural = wide_sep ? sep_height : 1;
    }
  else
    {
      *minimum = *natural = wide_sep ? sep_width : 1;
    }
}

static void
gtk_separator_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  gtk_separator_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum, natural);
}

static void
gtk_separator_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  gtk_separator_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, minimum, natural);
}

static gboolean
gtk_separator_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkSeparator *separator = GTK_SEPARATOR (widget);
  GtkSeparatorPrivate *private = separator->priv;
  GtkStyleContext *context;
  gboolean wide_separators;
  gint separator_width;
  gint separator_height;
  int width, height;

  gtk_widget_style_get (widget,
                        "wide-separators",  &wide_separators,
                        "separator-width",  &separator_width,
                        "separator-height", &separator_height,
                        NULL);

  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (wide_separators)
        gtk_render_frame (context, cr,
                          0, (height - separator_height) / 2,
                          width, separator_height);
      else
        gtk_render_line (context, cr,
                         0, height / 2,
                         width - 1, height / 2);
    }
  else
    {
      if (wide_separators)
        gtk_render_frame (context, cr,
                          (width - separator_width) / 2, 0,
                          separator_width, height);
      else
        gtk_render_line (context, cr,
                         width / 2, 0,
                         width / 2, height - 1);
    }

  return FALSE;
}

static void
gtk_separator_init (GtkSeparator *separator)
{
  GtkStyleContext *context;

  separator->priv = gtk_separator_get_instance_private (separator);
  separator->priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  gtk_widget_set_has_window (GTK_WIDGET (separator), FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (separator));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);
  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (separator));
}

static void
gtk_separator_class_init (GtkSeparatorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_separator_set_property;
  object_class->get_property = gtk_separator_get_property;

  widget_class->get_preferred_width = gtk_separator_get_preferred_width;
  widget_class->get_preferred_height = gtk_separator_get_preferred_height;

  widget_class->draw = gtk_separator_draw;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_SEPARATOR);

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");
}

/**
 * gtk_separator_new:
 * @orientation: the separator’s orientation.
 *
 * Creates a new #GtkSeparator with the given orientation.
 *
 * Returns: a new #GtkSeparator.
 *
 * Since: 3.0
 */
GtkWidget *
gtk_separator_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_SEPARATOR,
                       "orientation", orientation,
                       NULL);
}
