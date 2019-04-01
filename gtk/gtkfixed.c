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

/**
 * SECTION:gtkfixed
 * @Short_description: A container which allows you to position
 * widgets at fixed coordinates
 * @Title: GtkFixed
 * @See_also: #GtkLayout
 *
 * The #GtkFixed widget is a container which can place child widgets
 * at fixed positions and with fixed sizes, given in pixels. #GtkFixed
 * performs no automatic layout management.
 *
 * For most applications, you should not use this container! It keeps
 * you from having to learn about the other GTK+ containers, but it
 * results in broken applications.  With #GtkFixed, the following
 * things will result in truncated text, overlapping widgets, and
 * other display bugs:
 *
 * - Themes, which may change widget sizes.
 *
 * - Fonts other than the one you used to write the app will of course
 *   change the size of widgets containing text; keep in mind that
 *   users may use a larger font because of difficulty reading the
 *   default, or they may be using a different OS that provides different fonts.
 *
 * - Translation of text into other languages changes its size. Also,
 *   display of non-English text will use a different font in many
 *   cases.
 *
 * In addition, #GtkFixed does not pay attention to text direction and thus may
 * produce unwanted results if your app is run under right-to-left languages
 * such as Hebrew or Arabic. That is: normally GTK+ will order containers
 * appropriately for the text direction, e.g. to put labels to the right of the
 * thing they label when using an RTL language, but it can’t do that with
 * #GtkFixed. So if you need to reorder widgets depending on the text direction,
 * you would need to manually detect it and adjust child positions accordingly.
 *
 * Finally, fixed positioning makes it kind of annoying to add/remove
 * GUI elements, since you have to reposition all the other
 * elements. This is a long-term maintenance problem for your
 * application.
 *
 * If you know none of these things are an issue for your application,
 * and prefer the simplicity of #GtkFixed, by all means use the
 * widget. But you should be aware of the tradeoffs.
 *
 * See also #GtkLayout, which shares the ability to perform fixed positioning
 * of child widgets and additionally adds custom drawing and scrollability.
 */

#include "config.h"

#include "gtkfixed.h"

#include "gtkadjustment.h"
#include "gtkcontainerprivate.h"
#include "gtkfixedlayout.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkscrollable.h"
#include "gtkwidgetprivate.h"

static void gtk_fixed_add           (GtkContainer     *container,
                                     GtkWidget        *widget);
static void gtk_fixed_remove        (GtkContainer     *container,
                                     GtkWidget        *widget);
static void gtk_fixed_forall        (GtkContainer     *container,
                                     GtkCallback       callback,
                                     gpointer          callback_data);
static GType gtk_fixed_child_type   (GtkContainer     *container);

typedef struct {
  GtkLayoutManager *layout;

  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  GtkScrollablePolicy hscroll_policy;
  GtkScrollablePolicy vscroll_policy;
} GtkFixedPrivate;

enum {
   PROP_0,

   /* GtkFixed properties */

   N_PROPERTIES,

   /* GtkScrollable properties */
   PROP_HADJUSTMENT = N_PROPERTIES,
   PROP_VADJUSTMENT,
   PROP_HSCROLL_POLICY,
   PROP_VSCROLL_POLICY
};

G_DEFINE_TYPE_WITH_CODE (GtkFixed, gtk_fixed, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkFixed)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
gtk_fixed_adjustment_changed (GtkFixed *self)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (self);
  double scroll_x = 0;
  double scroll_y = 0;
  GskTransform *transform;

  if (priv->hadjustment)
    scroll_x = gtk_adjustment_get_value (priv->hadjustment) * -1.0;

  if (priv->vadjustment)
    scroll_y = gtk_adjustment_get_value (priv->vadjustment) * -1.0;

  transform = NULL;
  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (scroll_x, scroll_y));
  gtk_fixed_layout_set_child_transform (GTK_FIXED_LAYOUT (priv->layout), transform);
  gsk_transform_unref (transform);
}

static void
gtk_fixed_set_adjustment_values (GtkFixed       *self,
                                 GtkOrientation  orientation,
                                 GtkAdjustment  *adjustment)
{
  GtkAllocation allocation;
  double old_value;
  double new_value;
  double size;

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

  old_value = gtk_adjustment_get_value (adjustment);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = MAX (allocation.width, gtk_adjustment_get_upper (adjustment));
  else
    size = MAX (allocation.height, gtk_adjustment_get_upper (adjustment));

  g_object_set (adjustment,
                "lower", 0.0,
                "upper", size,
                "page-size", size,
                "step-increment", size * 0.1,
                "page-increment", size * 0.9,
                NULL);

  new_value = MAX (old_value, 0);
  if (!G_APPROX_VALUE (new_value, old_value, 0.001))
    gtk_adjustment_set_value (adjustment, new_value);
}

static void
gtk_fixed_finalize (GObject *gobject)
{
  GtkFixed *self = GTK_FIXED (gobject);
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (self);

  g_clear_object (&priv->hadjustment);
  g_clear_object (&priv->vadjustment);

  G_OBJECT_CLASS (gtk_fixed_parent_class)->finalize (gobject);
}

static void
gtk_fixed_set_hadjustment (GtkFixed      *self,
                           GtkAdjustment *adjustment)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (self);

  if (adjustment && priv->hadjustment == adjustment)
    return;

  if (priv->hadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->hadjustment,
                                            gtk_fixed_adjustment_changed,
                                            self);
      g_object_unref (priv->hadjustment);
    }

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect_swapped (adjustment, "value-changed",
                            G_CALLBACK (gtk_fixed_adjustment_changed),
                            self);
  priv->hadjustment = g_object_ref_sink (adjustment);
  gtk_fixed_set_adjustment_values (self, GTK_ORIENTATION_HORIZONTAL, priv->hadjustment);

  g_object_notify (G_OBJECT (self), "hadjustment");
}

static void
gtk_fixed_set_vadjustment (GtkFixed     *self,
                            GtkAdjustment *adjustment)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (self);

  if (adjustment && priv->vadjustment == adjustment)
    return;

  if (priv->vadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->vadjustment,
                                            gtk_fixed_adjustment_changed,
                                            self);
      g_object_unref (priv->vadjustment);
    }

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect_swapped (adjustment, "value-changed",
                            G_CALLBACK (gtk_fixed_adjustment_changed),
                            self);
  priv->vadjustment = g_object_ref_sink (adjustment);
  gtk_fixed_set_adjustment_values (self, GTK_ORIENTATION_VERTICAL, priv->vadjustment);

  g_object_notify (G_OBJECT (self), "vadjustment");
}

static void
gtk_fixed_get_property (GObject     *gobject,
                        guint        prop_id,
                        GValue      *value,
                        GParamSpec  *pspec)
{
  GtkFixed *self = GTK_FIXED (gobject);
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, priv->hadjustment);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, priv->vadjustment);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, priv->hscroll_policy);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, priv->vscroll_policy);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_fixed_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkFixed *self = GTK_FIXED (gobject);
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_fixed_set_hadjustment (self, g_value_get_object (value));
      break;

    case PROP_VADJUSTMENT:
      gtk_fixed_set_vadjustment (self, g_value_get_object (value));
      break;

    case PROP_HSCROLL_POLICY:
      if (priv->hscroll_policy != g_value_get_enum (value))
        {
          priv->hscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (self));
          g_object_notify_by_pspec (gobject, pspec);
        }
      break;

    case PROP_VSCROLL_POLICY:
      if (priv->vscroll_policy != g_value_get_enum (value))
        {
          priv->vscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (self));
          g_object_notify_by_pspec (gobject, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_fixed_class_init (GtkFixedClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  gobject_class->set_property = gtk_fixed_set_property;
  gobject_class->get_property = gtk_fixed_get_property;
  gobject_class->finalize = gtk_fixed_finalize;

  container_class->add = gtk_fixed_add;
  container_class->remove = gtk_fixed_remove;
  container_class->forall = gtk_fixed_forall;
  container_class->child_type = gtk_fixed_child_type;

  /* Scrollable interface */
  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");
}

static GType
gtk_fixed_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_fixed_init (GtkFixed *self)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (self);

  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);

  priv->layout = gtk_fixed_layout_new ();
  gtk_fixed_layout_set_minimum_size (GTK_FIXED_LAYOUT (priv->layout), -1, -1);
  gtk_widget_set_layout_manager (GTK_WIDGET (self), priv->layout);
}

/**
 * gtk_fixed_new:
 *
 * Creates a new #GtkFixed.
 *
 * Returns: a new #GtkFixed.
 */
GtkWidget*
gtk_fixed_new (void)
{
  return g_object_new (GTK_TYPE_FIXED, NULL);
}

/**
 * gtk_fixed_put:
 * @fixed: a #GtkFixed.
 * @widget: the widget to add.
 * @x: the horizontal position to place the widget at.
 * @y: the vertical position to place the widget at.
 *
 * Adds a widget to a #GtkFixed container at the given position.
 */
void
gtk_fixed_put (GtkFixed  *fixed,
               GtkWidget *widget,
               gint       x,
               gint       y)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (fixed);
  GtkFixedLayoutChild *child_info;
  GskTransform *transform = NULL;

  g_return_if_fail (GTK_IS_FIXED (fixed));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (_gtk_widget_get_parent (widget) == NULL);

  gtk_widget_set_parent (widget, GTK_WIDGET (fixed));

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));

  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (x, y));
  gtk_fixed_layout_child_set_position (child_info, transform);
  gsk_transform_unref (transform);
}

/**
 * gtk_fixed_get_child_position:
 * @fixed: a #GtkFixed
 * @widget: a child of @fixed
 * @x: (out): the horizontal position of the @widget
 * @y: (out): the vertical position of the @widget
 *
 * Retrieves the position of the given child #GtkWidget in the given
 * #GtkFixed container.
 */
void
gtk_fixed_get_child_position (GtkFixed  *fixed,
                              GtkWidget *widget,
                              gint      *x,
                              gint      *y)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (fixed);
  GtkFixedLayoutChild *child_info;
  float pos_x = 0.f, pos_y = 0.f;
  GskTransform *transform;

  g_return_if_fail (GTK_IS_FIXED (fixed));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (fixed));

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  transform = gtk_fixed_layout_child_get_position (child_info);
  gsk_transform_to_translate (transform, &pos_x, &pos_y);

  if (x != NULL)
    *x = floorf (pos_x);
  if (y != NULL)
    *y = floorf (pos_y);
}

/**
 * gtk_fixed_move:
 * @fixed: a #GtkFixed.
 * @widget: the child widget.
 * @x: the horizontal position to move the widget to.
 * @y: the vertical position to move the widget to.
 *
 * Moves a child of a #GtkFixed container to the given position.
 */
void
gtk_fixed_move (GtkFixed  *fixed,
                GtkWidget *widget,
                gint       x,
                gint       y)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (fixed);
  GtkFixedLayoutChild *child_info;
  GskTransform *transform = NULL;

  g_return_if_fail (GTK_IS_FIXED (fixed));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (fixed));

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout,  widget));

  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (x, y));
  gtk_fixed_layout_child_set_position (child_info, transform);
  gsk_transform_unref (transform);
}

static void
gtk_fixed_add (GtkContainer *container,
               GtkWidget    *widget)
{
  gtk_fixed_put (GTK_FIXED (container), widget, 0, 0);
}

static void
gtk_fixed_remove (GtkContainer *container,
                  GtkWidget    *widget)
{
  gtk_widget_unparent (widget);
}

static void
gtk_fixed_forall (GtkContainer *container,
                  GtkCallback   callback,
                  gpointer      callback_data)
{
  GtkWidget *widget = GTK_WIDGET (container);
  GtkWidget *child;

  child = gtk_widget_get_first_child (widget);
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      (* callback) (child, callback_data);

      child = next;
    }
}
