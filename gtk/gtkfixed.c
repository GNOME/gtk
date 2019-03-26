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
gtk_fixed_class_init (GtkFixedClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  container_class->add = gtk_fixed_add;
  container_class->remove = gtk_fixed_remove;
  container_class->forall = gtk_fixed_forall;
  container_class->child_type = gtk_fixed_child_type;
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

  g_return_if_fail (GTK_IS_FIXED (fixed));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (_gtk_widget_get_parent (widget) == NULL);

  gtk_widget_set_parent (widget, GTK_WIDGET (fixed));

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  gtk_fixed_layout_child_set_position (child_info, &GRAPHENE_POINT_INIT (x, y));
}

void
gtk_fixed_get_position (GtkFixed  *fixed,
                        GtkWidget *widget,
                        gint      *x,
                        gint      *y)
{
  GtkFixedPrivate *priv = gtk_fixed_get_instance_private (fixed);
  GtkFixedLayoutChild *child_info;
  graphene_point_t pos;

  g_return_if_fail (GTK_IS_FIXED (fixed));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (fixed));

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  gtk_fixed_layout_child_get_position (child_info, &pos);

  if (x != NULL)
    *x = floorf (pos.x);
  if (y != NULL)
    *y = floorf (pos.y);
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

  g_return_if_fail (GTK_IS_FIXED (fixed));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (fixed));

  child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout,  widget));
  gtk_fixed_layout_child_set_position (child_info, &GRAPHENE_POINT_INIT (x, y));
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
