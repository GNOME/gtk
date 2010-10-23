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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gtkscrollable
 * @Short_Description: An interface for scrollable widgets
 * @Title: GtkScrollable
 *
 * #GtkScrollable is interface that is implemented by widgets with native
 * scrolling ability.
 *
 * To implement this interface, all one needs to do is to override
 * #GtkScrollable:hadjustment and #GtkScrollable:vadjustment properties.
 *
 * <refsect2>
 * <title>Creating a scrollable widget</title>
 * <para>
 * There are some common things all scrollable widgets will need to do.
 *
 * <orderedlist>
 *   <listitem>
 *     <para>
 *       When parent sets adjustments, widget needs to populate adjustment's
 *       #GtkAdjustment:lower, #GtkAdjustment:upper,
 *       #GtkAdjustment:step-increment, #GtkAdjustment:page-increment and
 *       #GtkAdjustment:page-size properties and connect to
 *       #GtkAdjustment::value-changed signal.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       When parent allocates space to child, scrollable widget needs to update
 *       properties listed under 1 with new values.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       When any of the adjustments emits #GtkAdjustment::value-changed signal,
 *       scrollable widget needs to scroll it's contents.
 *     </para>
 *   </listitem>
 * </orderedlist>
 * </para>
 * </refsect2>
 */

#include "config.h"

#include "gtkscrollable.h"
#include "gtkprivate.h"
#include "gtkintl.h"

G_DEFINE_INTERFACE (GtkScrollable, gtk_scrollable, G_TYPE_OBJECT)

static void
gtk_scrollable_default_init (GtkScrollableInterface *iface)
{
  GParamSpec *pspec;

  /**
   * GtkScrollable:hadjustment:
   *
   * Horizontal #GtkAdjustment of scrollable widget. This adjustment is
   * shared between scrollable widget and it's parent.
   *
   * Since: 3.0
   */
  pspec = g_param_spec_object ("hadjustment",
                               P_("Horizontal adjustment"),
                               P_("Horizontal adjustment that is shared "
                                  "between scrollable widget and it's "
                                  "controller"),
                               GTK_TYPE_ADJUSTMENT,
                               GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_interface_install_property (iface, pspec);

  /**
   * GtkScrollable:vadjustment:
   *
   * Verical #GtkAdjustment of scrollable widget. This adjustment is shared
   * between scrollable widget and it's parent.
   *
   * Since: 3.0
   */
  pspec = g_param_spec_object ("vadjustment",
                               P_("Vertical adjustment"),
                               P_("Vertical adjustment that is shared "
                                  "between scrollable widget and it's "
                                  "controller"),
                               GTK_TYPE_ADJUSTMENT,
                               GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_interface_install_property (iface, pspec);
}

/**
 * gtk_scrollable_get_hadjustment:
 * @scrollable: a #GtkScrollable
 *
 * Retrieves the #GtkAdjustment, used for horizontal scrolling.
 *
 * Return value: (transfer none): horizontal #GtkAdjustment.
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
 * Retrieves the #GtkAdjustment, used for vertical scrolling.
 *
 * Return value: (transfer none): vertical #GtkAdjustment.
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
