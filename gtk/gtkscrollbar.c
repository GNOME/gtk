/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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

#include "gtkscrollbar.h"
#include "gtkrange.h"

#include "gtkadjustment.h"
#include "gtkintl.h"
#include "gtkorientable.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkbox.h"


/**
 * SECTION:gtkscrollbar
 * @Short_description: A Scrollbar
 * @Title: GtkScrollbar
 * @See_also: #GtkAdjustment, #GtkScrolledWindow
 *
 * The #GtkScrollbar widget is a horizontal or vertical scrollbar,
 * depending on the value of the #GtkOrientable:orientation property.
 *
 * Its position and movement are controlled by the adjustment that is passed to
 * or created by gtk_scrollbar_new(). See #GtkAdjustment for more details. The
 * #GtkAdjustment:value field sets the position of the thumb and must be between
 * #GtkAdjustment:lower and #GtkAdjustment:upper - #GtkAdjustment:page-size. The
 * #GtkAdjustment:page-size represents the size of the visible scrollable area.
 * The fields #GtkAdjustment:step-increment and #GtkAdjustment:page-increment
 * fields are added to or subtracted from the #GtkAdjustment:value when the user
 * asks to move by a step (using e.g. the cursor arrow keys or, if present, the
 * stepper buttons) or by a page (using e.g. the Page Down/Up keys).
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * scrollbar
 * ╰── box
 *     ╰── range[.fine-tune]
 *         ╰── trough
 *             ╰── slider
 * ]|
 *
 * GtkScrollbar has a main CSS node with name scrollbar and a subnode for its
 * contents. Both the main node and the box subnode get the .horizontal or .vertical
 * style classes applied, depending on the scrollbar's orientation.
 *
 * The range node gets the style class .fine-tune added when the scrollbar is
 * in 'fine-tuning' mode.
 *
 * If steppers are enabled, they are represented by up to four additional
 * subnodes with name button. These get the style classes .up and .down to
 * indicate in which direction they are moving.
 *
 * Other style classes that may be added to scrollbars inside #GtkScrolledWindow
 * include the positional classes (.left, .right, .top, .bottom) and style
 * classes related to overlay scrolling (.overlay-indicator, .dragging, .hovering).
 */

typedef struct _GtkScrollbarClass   GtkScrollbarClass;

struct _GtkScrollbar
{
  GtkWidget parent_instance;
};

struct _GtkScrollbarClass
{
  GtkWidgetClass parent_class;
};

typedef struct {
  GtkOrientation orientation;
  GtkWidget *box;
  GtkWidget *range;
} GtkScrollbarPrivate;

enum {
  PROP_0,
  PROP_ADJUSTMENT,

  PROP_ORIENTATION,
  LAST_PROP = PROP_ORIENTATION
};

G_DEFINE_TYPE_WITH_CODE (GtkScrollbar, gtk_scrollbar, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkScrollbar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))


static GParamSpec *props[LAST_PROP] = { NULL, };


static void
gtk_scrollbar_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkScrollbar *self = GTK_SCROLLBAR (widget);
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  gtk_widget_measure (priv->box, orientation, for_size,
                      minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_scrollbar_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkScrollbar *self = GTK_SCROLLBAR (widget);
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  gtk_widget_size_allocate (priv->box,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, -1);
}

static void
gtk_scrollbar_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkScrollbar *self = GTK_SCROLLBAR (object);
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  switch (property_id)
   {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, gtk_scrollbar_get_adjustment (self));
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_scrollbar_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkScrollbar *self = GTK_SCROLLBAR (object);
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  switch (property_id)
    {
    case PROP_ADJUSTMENT:
      gtk_scrollbar_set_adjustment (self, g_value_get_object (value));
      break;
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);

        if (orientation != priv->orientation)
          {
            gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->box), orientation);
            gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->range), orientation);
            priv->orientation = orientation;
            _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));

            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify_by_pspec (object, pspec);
          }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_scrollbar_dispose (GObject *object)
{
  GtkScrollbar *self = GTK_SCROLLBAR (object);
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  g_clear_pointer (&priv->box, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_scrollbar_parent_class)->dispose (object);
}

static void
gtk_scrollbar_class_init (GtkScrollbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_scrollbar_get_property;
  object_class->set_property = gtk_scrollbar_set_property;
  object_class->dispose = gtk_scrollbar_dispose;

  widget_class->measure = gtk_scrollbar_measure;
  widget_class->size_allocate = gtk_scrollbar_size_allocate;

  props[PROP_ADJUSTMENT] =
      g_param_spec_object ("adjustment",
                           P_("Adjustment"),
                           P_("The GtkAdjustment that contains the current value of this scrollbar"),
                           GTK_TYPE_ADJUSTMENT,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_SCROLL_BAR);
  gtk_widget_class_set_css_name (widget_class, I_("scrollbar"));
}

static void
gtk_scrollbar_init (GtkScrollbar *self)
{
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  priv->box = gtk_box_new (priv->orientation, 0);
  gtk_widget_set_parent (priv->box, GTK_WIDGET (self));
  priv->range = g_object_new (GTK_TYPE_RANGE, NULL);
  gtk_widget_set_hexpand (priv->range, TRUE);
  gtk_widget_set_vexpand (priv->range, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->box), priv->range);

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));
}

/**
 * gtk_scrollbar_new:
 * @orientation: the scrollbar’s orientation.
 * @adjustment: (allow-none): the #GtkAdjustment to use, or %NULL to create a new adjustment.
 *
 * Creates a new scrollbar with the given orientation.
 *
 * Returns:  the new #GtkScrollbar.
 **/
GtkWidget *
gtk_scrollbar_new (GtkOrientation  orientation,
                   GtkAdjustment  *adjustment)
{
  g_return_val_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment),
                        NULL);

  return g_object_new (GTK_TYPE_SCROLLBAR,
                       "orientation", orientation,
                       "adjustment",  adjustment,
                       NULL);
}

/**
 * gtk_scrollbar_set_adjustment:
 * @self: a #GtkScrollbar
 * @adjustment: (nullable): the adjustment to set
 *
 * Makes the scrollbar use the given adjustment.
 */
void
gtk_scrollbar_set_adjustment (GtkScrollbar  *self,
                              GtkAdjustment *adjustment)
{
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  g_return_if_fail (GTK_IS_SCROLLBAR (self));
  g_return_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment));


  gtk_range_set_adjustment (GTK_RANGE (priv->range), adjustment);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ADJUSTMENT]);
}

/**
 * gtk_scrollbar_get_adjustment:
 * @self: a #GtkScrollbar
 *
 * Returns the scrollbar's adjustment.
 *
 * Returns: (transfer none): the scrollbar's adjustment
 */
GtkAdjustment *
gtk_scrollbar_get_adjustment (GtkScrollbar  *self)
{
  GtkScrollbarPrivate *priv = gtk_scrollbar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_SCROLLBAR (self), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (priv->range));
}
