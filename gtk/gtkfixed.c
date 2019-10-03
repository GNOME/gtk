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
 * such as Hebrew or Arabic. That is: normally GTK will order containers
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
 */

#include "config.h"

#include "gtkfixed.h"

#include "gtkcontainerprivate.h"
#include "gtkfixedlayout.h"
#include "gtkintl.h"
#include "gtkprivate.h"
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
} GtkFixedPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkFixed, gtk_fixed, GTK_TYPE_CONTAINER)

static void
gtk_fixed_dispose (GObject *object)
{
  GtkWidget *child;

  child = gtk_widget_get_first_child (GTK_WIDGET (object));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      gtk_widget_unparent (child);

      child = next;
    }

  G_OBJECT_CLASS (gtk_fixed_parent_class)->dispose (object);
}

static void
gtk_fixed_class_init (GtkFixedClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_fixed_dispose;

  container_class->add = gtk_fixed_add;
  container_class->remove = gtk_fixed_remove;
  container_class->forall = gtk_fixed_forall;
  container_class->child_type = gtk_fixed_child_type;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_FIXED_LAYOUT);
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

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);

  priv->layout = gtk_widget_get_layout_manager (GTK_WIDGET (self)); 
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
 * Adds a widget to a #GtkFixed container and assigns a translation
 * transformation to the given @x and @y coordinates to it.
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
  gtk_fixed_layout_child_set_transform (child_info, transform);
  gsk_transform_unref (transform);
}

/**
 * gtk_fixed_get_child_position:
 * @fixed: a #GtkFixed
 * @widget: a child of @fixed
 * @x: (out): the horizontal position of the @widget
 * @y: (out): the vertical position of the @widget
 *
 * Retrieves the translation transformation of the given child #GtkWidget
 * in the given #GtkFixed container.
 *
 * See also: gtk_fixed_get_child_transform().
 */
void
gtk_fixed_get_child_position (GtkFixed  *fixed,
                              GtkWidget *widget,
                              int       *x,
                              int       *y)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (fixed);
  GtkFixedLayoutChild *child_info;
  float pos_x = 0.f, pos_y = 0.f;
  GskTransform *transform;

  g_return_if_fail (GTK_IS_FIXED (fixed));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (fixed));

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  transform = gtk_fixed_layout_child_get_transform (child_info);
  gsk_transform_to_translate (transform, &pos_x, &pos_y);

  *x = floorf (pos_x);
  *y = floorf (pos_y);
}

/**
 * gtk_fixed_set_child_transform:
 * @fixed: a #GtkFixed
 * @widget: a #GtkWidget, child of @fixed
 * @transform: (nullable): the transformation assigned to @widget or %NULL
 *   to reset @widget's transform
 *
 * Sets the transformation for @widget.
 *
 * This is a convenience function that retrieves the #GtkFixedLayoutChild
 * instance associated to @widget and calls gtk_fixed_layout_child_set_transform().
 */
void
gtk_fixed_set_child_transform (GtkFixed     *fixed,
                               GtkWidget    *widget,
                               GskTransform *transform)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (fixed);
  GtkFixedLayoutChild *child_info;

  g_return_if_fail (GTK_IS_FIXED (fixed));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (fixed));

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  gtk_fixed_layout_child_set_transform (child_info, transform);
}

/**
 * gtk_fixed_get_child_transform:
 * @fixed: a #GtkFixed
 * @widget: a #GtkWidget, child of @fixed
 *
 * Retrieves the transformation for @widget set using
 * gtk_fixed_set_child_transform().
 *
 * Returns: (transfer none) (nullable): a #GskTransform or %NULL
 *   in case no transform has been set on @widget
 */
GskTransform *
gtk_fixed_get_child_transform (GtkFixed  *fixed,
                               GtkWidget *widget)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (fixed);
  GtkFixedLayoutChild *child_info;

  g_return_val_if_fail (GTK_IS_FIXED (fixed), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (fixed), NULL);

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  return gtk_fixed_layout_child_get_transform (child_info);
}

/**
 * gtk_fixed_move:
 * @fixed: a #GtkFixed.
 * @widget: the child widget.
 * @x: the horizontal position to move the widget to.
 * @y: the vertical position to move the widget to.
 *
 * Sets a translation transformation to the given @x and @y coordinates to
 * the child @widget of the given #GtkFixed container.
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
  gtk_fixed_layout_child_set_transform (child_info, transform);
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
