/* gtkscrollable.c
 * Copyright (C) 2008 Tadej Borovšak <tadeboro@gmail.com>
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

/**
 * SECTION:gtkscrollable
 * @Short_Description: An interface for scrollable widgets
 * @Title: GtkScrollable
 *
 * #GtkScrollable is an interface that is implemented by widgets with native
 * scrolling ability.
 *
 * To implement this interface you should override the
 * #GtkScrollable:hadjustment and #GtkScrollable:vadjustment properties.
 *
 * ## Creating a scrollable widget
 *
 * All scrollable widgets should do the following.
 *
 * - When a parent widget sets the scrollable child widget’s adjustments,
 *   the widget should populate the adjustments’
 *   #GtkAdjustment:lower, #GtkAdjustment:upper,
 *   #GtkAdjustment:step-increment, #GtkAdjustment:page-increment and
 *   #GtkAdjustment:page-size properties and connect to the
 *   #GtkAdjustment::value-changed signal.
 *
 * - Because its preferred size is the size for a fully expanded widget,
 *   the scrollable widget must be able to cope with underallocations.
 *   This means that it must accept any value passed to its
 *   #GtkWidgetClass.size_allocate() function.
 *
 * - When the parent allocates space to the scrollable child widget,
 *   the widget should update the adjustments’ properties with new values.
 *
 * - When any of the adjustments emits the #GtkAdjustment::value-changed signal,
 *   the scrollable widget should scroll its contents.
 */

#include "config.h"

#include "gtkscrollable.h"

#include "gtkadjustment.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"

G_DEFINE_INTERFACE (GtkScrollable, gtk_scrollable, G_TYPE_OBJECT)

static void
gtk_scrollable_default_init (GtkScrollableInterface *iface)
{
  GParamSpec *pspec;

  /**
   * GtkScrollable:hadjustment:
   *
   * Horizontal #GtkAdjustment of the scrollable widget. This adjustment is
   * shared between the scrollable widget and its parent.
   *
   * Since: 3.0
   */
  pspec = g_param_spec_object ("hadjustment",
                               P_("Horizontal adjustment"),
                               P_("Horizontal adjustment that is shared "
                                  "between the scrollable widget and its "
                                  "controller"),
                               GTK_TYPE_ADJUSTMENT,
                               GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_interface_install_property (iface, pspec);

  /**
   * GtkScrollable:vadjustment:
   *
   * Verical #GtkAdjustment of the scrollable widget. This adjustment is shared
   * between the scrollable widget and its parent.
   *
   * Since: 3.0
   */
  pspec = g_param_spec_object ("vadjustment",
                               P_("Vertical adjustment"),
                               P_("Vertical adjustment that is shared "
                                  "between the scrollable widget and its "
                                  "controller"),
                               GTK_TYPE_ADJUSTMENT,
                               GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_interface_install_property (iface, pspec);

  /**
   * GtkScrollable:hscroll-policy:
   *
   * Determines whether horizontal scrolling should start once the scrollable
   * widget is allocated less than its minimum width or less than its natural width.
   *
   * Since: 3.0
   */
  pspec = g_param_spec_enum ("hscroll-policy",
			     P_("Horizontal Scrollable Policy"),
			     P_("How the size of the content should be determined"),
			     GTK_TYPE_SCROLLABLE_POLICY,
			     GTK_SCROLL_MINIMUM,
			     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  g_object_interface_install_property (iface, pspec);

  /**
   * GtkScrollable:vscroll-policy:
   *
   * Determines whether vertical scrolling should start once the scrollable
   * widget is allocated less than its minimum height or less than its natural height.
   *
   * Since: 3.0
   */
  pspec = g_param_spec_enum ("vscroll-policy",
			     P_("Vertical Scrollable Policy"),
			     P_("How the size of the content should be determined"),
			     GTK_TYPE_SCROLLABLE_POLICY,
			     GTK_SCROLL_MINIMUM,
			     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  g_object_interface_install_property (iface, pspec);
}

/**
 * gtk_scrollable_get_hadjustment:
 * @scrollable: a #GtkScrollable
 *
 * Retrieves the #GtkAdjustment used for horizontal scrolling.
 *
 * Returns: (transfer none): horizontal #GtkAdjustment.
 *
 * Since: 3.0
 **/
GtkAdjustment *
gtk_scrollable_get_hadjustment (GtkScrollable *scrollable)
{
  GtkAdjustment *adj = NULL;

  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), NULL);

  g_object_get (scrollable, "hadjustment", &adj, NULL);

  /* Horrid hack; g_object_get() returns a new reference but
   * that contradicts the memory management conventions
   * for accessors.
   */
  if (adj)
    g_object_unref (adj);

  return adj;
}

/**
 * gtk_scrollable_set_hadjustment:
 * @scrollable: a #GtkScrollable
 * @hadjustment: (allow-none): a #GtkAdjustment
 *
 * Sets the horizontal adjustment of the #GtkScrollable.
 *
 * Since: 3.0
 **/
void
gtk_scrollable_set_hadjustment (GtkScrollable *scrollable,
                                GtkAdjustment *hadjustment)
{
  g_return_if_fail (GTK_IS_SCROLLABLE (scrollable));
  g_return_if_fail (hadjustment == NULL || GTK_IS_ADJUSTMENT (hadjustment));

  g_object_set (scrollable, "hadjustment", hadjustment, NULL);
}

/**
 * gtk_scrollable_get_vadjustment:
 * @scrollable: a #GtkScrollable
 *
 * Retrieves the #GtkAdjustment used for vertical scrolling.
 *
 * Returns: (transfer none): vertical #GtkAdjustment.
 *
 * Since: 3.0
 **/
GtkAdjustment *
gtk_scrollable_get_vadjustment (GtkScrollable *scrollable)
{
  GtkAdjustment *adj = NULL;

  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), NULL);

  g_object_get (scrollable, "vadjustment", &adj, NULL);

  /* Horrid hack; g_object_get() returns a new reference but
   * that contradicts the memory management conventions
   * for accessors.
   */
  if (adj)
    g_object_unref (adj);

  return adj;
}

/**
 * gtk_scrollable_set_vadjustment:
 * @scrollable: a #GtkScrollable
 * @vadjustment: (allow-none): a #GtkAdjustment
 *
 * Sets the vertical adjustment of the #GtkScrollable.
 *
 * Since: 3.0
 **/
void
gtk_scrollable_set_vadjustment (GtkScrollable *scrollable,
                                GtkAdjustment *vadjustment)
{
  g_return_if_fail (GTK_IS_SCROLLABLE (scrollable));
  g_return_if_fail (vadjustment == NULL || GTK_IS_ADJUSTMENT (vadjustment));

  g_object_set (scrollable, "vadjustment", vadjustment, NULL);
}


/**
 * gtk_scrollable_get_hscroll_policy:
 * @scrollable: a #GtkScrollable
 *
 * Gets the horizontal #GtkScrollablePolicy.
 *
 * Returns: The horizontal #GtkScrollablePolicy.
 *
 * Since: 3.0
 **/
GtkScrollablePolicy
gtk_scrollable_get_hscroll_policy (GtkScrollable *scrollable)
{
  GtkScrollablePolicy policy;

  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), GTK_SCROLL_MINIMUM);

  g_object_get (scrollable, "hscroll-policy", &policy, NULL);

  return policy;
}

/**
 * gtk_scrollable_set_hscroll_policy:
 * @scrollable: a #GtkScrollable
 * @policy: the horizontal #GtkScrollablePolicy
 *
 * Sets the #GtkScrollablePolicy to determine whether
 * horizontal scrolling should start below the minimum width or
 * below the natural width.
 *
 * Since: 3.0
 **/
void
gtk_scrollable_set_hscroll_policy (GtkScrollable       *scrollable,
				   GtkScrollablePolicy  policy)
{
  g_return_if_fail (GTK_IS_SCROLLABLE (scrollable));

  g_object_set (scrollable, "hscroll-policy", policy, NULL);
}

/**
 * gtk_scrollable_get_vscroll_policy:
 * @scrollable: a #GtkScrollable
 *
 * Gets the vertical #GtkScrollablePolicy.
 *
 * Returns: The vertical #GtkScrollablePolicy.
 *
 * Since: 3.0
 **/
GtkScrollablePolicy
gtk_scrollable_get_vscroll_policy (GtkScrollable *scrollable)
{
  GtkScrollablePolicy policy;

  g_return_val_if_fail (GTK_IS_SCROLLABLE (scrollable), GTK_SCROLL_MINIMUM);

  g_object_get (scrollable, "vscroll-policy", &policy, NULL);

  return policy;
}

/**
 * gtk_scrollable_set_vscroll_policy:
 * @scrollable: a #GtkScrollable
 * @policy: the vertical #GtkScrollablePolicy
 *
 * Sets the #GtkScrollablePolicy to determine whether
 * vertical scrolling should start below the minimum height or
 * below the natural height.
 *
 * Since: 3.0
 **/
void
gtk_scrollable_set_vscroll_policy (GtkScrollable       *scrollable,
				   GtkScrollablePolicy  policy)
{
  g_return_if_fail (GTK_IS_SCROLLABLE (scrollable));

  g_object_set (scrollable, "vscroll-policy", policy, NULL);
}
