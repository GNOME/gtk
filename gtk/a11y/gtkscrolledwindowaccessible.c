/* GTK+ - accessibility implementations
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
#include "gtkscrolledwindowaccessible.h"


G_DEFINE_TYPE (GtkScrolledWindowAccessible, gtk_scrolled_window_accessible, GTK_TYPE_WIDGET_ACCESSIBLE)

static void
visibility_changed (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  if (!g_strcmp0 (pspec->name, "visible"))
    {
      gint index;
      gint n_children;
      gboolean child_added = FALSE;
      AtkObject *child;
      GtkWidget *widget;
      GtkScrolledWindow *scrolled_window;
      GtkWidget *hscrollbar, *vscrollbar;
      GtkAccessible *accessible = GTK_ACCESSIBLE (user_data);

      widget = gtk_accessible_get_widget (user_data);
      if (widget == NULL)
        return;

      scrolled_window = GTK_SCROLLED_WINDOW (widget);
      index = n_children = 1;

      hscrollbar = gtk_scrolled_window_get_hscrollbar (scrolled_window);
      vscrollbar = gtk_scrolled_window_get_vscrollbar (scrolled_window);

      if ((gpointer) object == (gpointer) (hscrollbar))
        {
          if (gtk_scrolled_window_get_hscrollbar (scrolled_window))
            child_added = TRUE;

          child = gtk_widget_get_accessible (hscrollbar);
        }
      else if ((gpointer) object == (gpointer) (vscrollbar))
        {
          if (gtk_scrolled_window_get_vscrollbar (scrolled_window))
            child_added = TRUE;

          child = gtk_widget_get_accessible (vscrollbar);
          if (gtk_scrolled_window_get_hscrollbar (scrolled_window))
            index = n_children + 1;
        }
      else
        {
          g_assert_not_reached ();
          return;
        }

      if (child_added)
        g_signal_emit_by_name (accessible, "children-changed::add", index, child, NULL);
      else
        g_signal_emit_by_name (accessible, "children-changed::remove", index, child, NULL);

    }
}

static void
gtk_scrolled_window_accessible_initialize (AtkObject *obj,
                                           gpointer  data)
{
  GtkScrolledWindow *window;

  ATK_OBJECT_CLASS (gtk_scrolled_window_accessible_parent_class)->initialize (obj, data);

  window = GTK_SCROLLED_WINDOW (data);

  g_signal_connect_object (gtk_scrolled_window_get_hscrollbar (window), "notify::visible",
                           G_CALLBACK (visibility_changed),
                           obj, 0);
  g_signal_connect_object (gtk_scrolled_window_get_vscrollbar (window), "notify::visible",
                           G_CALLBACK (visibility_changed),
                           obj, 0);

  obj->role = ATK_ROLE_SCROLL_PANE;
}

static int
gtk_scrolled_window_accessible_get_n_children (AtkObject *object)
{
  GtkWidget *widget;
  GtkScrolledWindow *scrolled_window;
  gint n_children;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (object));
  if (widget == NULL)
    return 0;

  scrolled_window = GTK_SCROLLED_WINDOW (widget);

  n_children = 0;
  if (gtk_scrolled_window_get_child (scrolled_window))
    n_children++;
  if (gtk_scrolled_window_get_hscrollbar (scrolled_window))
    n_children++;
  if (gtk_scrolled_window_get_vscrollbar (scrolled_window))
    n_children++;

  return n_children;
}

static AtkObject *
gtk_scrolled_window_accessible_ref_child (AtkObject *obj,
                                          gint       child)
{
  GtkWidget *widget;
  GtkScrolledWindow *scrolled_window;
  GtkWidget *hscrollbar, *vscrollbar;
  GtkWidget *c;
  GtkWidget *chosen = NULL;

  g_return_val_if_fail (child >= 0, NULL);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  scrolled_window = GTK_SCROLLED_WINDOW (widget);
  c = gtk_scrolled_window_get_child (scrolled_window);
  hscrollbar = gtk_scrolled_window_get_hscrollbar (scrolled_window);
  vscrollbar = gtk_scrolled_window_get_vscrollbar (scrolled_window);

  if (child == 2)
    chosen = vscrollbar;
  else if (child == 1)
    chosen = c ? hscrollbar : vscrollbar;
  else if (child == 0)
    chosen = c ? c : (hscrollbar ? hscrollbar : vscrollbar);

  if (chosen)
    return g_object_ref (gtk_widget_get_accessible (chosen));

  return NULL;
}

static void
gtk_scrolled_window_accessible_class_init (GtkScrolledWindowAccessibleClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  class->get_n_children = gtk_scrolled_window_accessible_get_n_children;
  class->ref_child = gtk_scrolled_window_accessible_ref_child;
  class->initialize = gtk_scrolled_window_accessible_initialize;
}

static void
gtk_scrolled_window_accessible_init (GtkScrolledWindowAccessible *window)
{
}
