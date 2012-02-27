/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <gtk/gtk.h>
#include "gtkscrollbaraccessible.h"


G_DEFINE_TYPE (GtkScrollbarAccessible, _gtk_scrollbar_accessible, GTK_TYPE_RANGE_ACCESSIBLE)

static void
_gtk_scrollbar_accessible_init (GtkScrollbarAccessible *accessible)
{
}

static void
gtk_scrollbar_accessible_initialize (AtkObject *accessible,
                                     gpointer   data)
{
  ATK_OBJECT_CLASS (_gtk_scrollbar_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_SCROLL_BAR;
}

static gint
gtk_scrollbar_accessible_get_index_in_parent (AtkObject *accessible)
{
  GtkWidget *widget;
  GtkWidget *parent;
  GtkWidget *child;
  GtkScrolledWindow *scrolled_window;
  gint id;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return -1;

  parent = gtk_widget_get_parent (widget);
  if (!GTK_IS_SCROLLED_WINDOW (parent))
    return ATK_OBJECT_CLASS (_gtk_scrollbar_accessible_parent_class)->get_index_in_parent (accessible);

  scrolled_window = GTK_SCROLLED_WINDOW (parent);
  id = 0;
  child = gtk_bin_get_child (GTK_BIN (scrolled_window));
  if (child)
    {
      if (widget == child)
        return id;
      id++;
    }

  child = gtk_scrolled_window_get_hscrollbar (scrolled_window);
  if (child)
    {
      if (widget == child)
        return id;
      id++;
    }
  child = gtk_scrolled_window_get_vscrollbar (scrolled_window);
  if (child)
    {
      if (widget == child)
        return id;
    }

  return -1;
}

static void
_gtk_scrollbar_accessible_class_init (GtkScrollbarAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gtk_scrollbar_accessible_initialize;
  class->get_index_in_parent = gtk_scrollbar_accessible_get_index_in_parent;
}
