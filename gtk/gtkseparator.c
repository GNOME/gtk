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

#include "gtkseparator.h"

#include "gtkorientableprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkrender.h"
#include "gtkwidgetprivate.h"
#include "gtkcsscustomgadgetprivate.h"

/**
 * SECTION:gtkseparator
 * @Short_description: A separator widget
 * @Title: GtkSeparator
 *
 * GtkSeparator is a horizontal or vertical separator widget, depending on the
 * value of the #GtkOrientable:orientation property, used to group the widgets
 * within a window. It displays a line with a shadow to make it appear sunken
 * into the interface.
 *
 * # CSS nodes
 *
 * GtkSeparator has a single CSS node with name separator. The node
 * gets one of the .horizontal or .vertical style classes.
 */


struct _GtkSeparatorPrivate
{
  GtkOrientation orientation;
  GtkCssGadget *gadget;
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
gtk_separator_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  gtk_css_gadget_get_preferred_size (GTK_SEPARATOR (widget)->priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_separator_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  gtk_css_gadget_get_preferred_size (GTK_SEPARATOR (widget)->priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_separator_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (GTK_SEPARATOR (widget)->priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static gboolean
gtk_separator_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  gtk_css_gadget_draw (GTK_SEPARATOR (widget)->priv->gadget, cr);

  return FALSE;
}

static void
gtk_separator_init (GtkSeparator *separator)
{
  GtkCssNode *widget_node;

  separator->priv = gtk_separator_get_instance_private (separator);
  separator->priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  gtk_widget_set_has_window (GTK_WIDGET (separator), FALSE);

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (separator));

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (separator));
  separator->priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                                GTK_WIDGET (separator),
                                                                NULL, NULL, NULL,
                                                                NULL, NULL);
}

static void
gtk_separator_finalize (GObject *object)
{
  GtkSeparatorPrivate *priv = GTK_SEPARATOR (object)->priv;

  g_clear_object (&priv->gadget);


  G_OBJECT_CLASS (gtk_separator_parent_class)->finalize (object);
}

static void
gtk_separator_class_init (GtkSeparatorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_separator_set_property;
  object_class->get_property = gtk_separator_get_property;
  object_class->finalize = gtk_separator_finalize;

  widget_class->get_preferred_width = gtk_separator_get_preferred_width;
  widget_class->get_preferred_height = gtk_separator_get_preferred_height;
  widget_class->size_allocate = gtk_separator_size_allocate;

  widget_class->draw = gtk_separator_draw;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_SEPARATOR);
  gtk_widget_class_set_css_name (widget_class, "separator");
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
