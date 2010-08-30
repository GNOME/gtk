/* GAIL - The GNOME Accessibility Enabling Library
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include "gailscrolledwindow.h"


static void         gail_scrolled_window_class_init     (GailScrolledWindowClass  *klass); 
static void         gail_scrolled_window_init           (GailScrolledWindow       *window);
static void         gail_scrolled_window_real_initialize
                                                        (AtkObject     *obj,
                                                         gpointer      data);

static gint         gail_scrolled_window_get_n_children (AtkObject     *object);
static AtkObject*   gail_scrolled_window_ref_child      (AtkObject     *obj,
                                                         gint          child);
static void         gail_scrolled_window_scrollbar_visibility_changed 
                                                        (GObject       *object,
                                                         GParamSpec    *pspec,
                                                         gpointer      user_data);

G_DEFINE_TYPE (GailScrolledWindow, gail_scrolled_window, GAIL_TYPE_CONTAINER)

static void
gail_scrolled_window_class_init (GailScrolledWindowClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  class->get_n_children = gail_scrolled_window_get_n_children;
  class->ref_child = gail_scrolled_window_ref_child;
  class->initialize = gail_scrolled_window_real_initialize;
}

static void
gail_scrolled_window_init (GailScrolledWindow *window)
{
}

static void
gail_scrolled_window_real_initialize (AtkObject *obj,
                                      gpointer  data)
{
  GtkScrolledWindow *window;

  ATK_OBJECT_CLASS (gail_scrolled_window_parent_class)->initialize (obj, data);

  window = GTK_SCROLLED_WINDOW (data);

  g_signal_connect_data (gtk_scrolled_window_get_hscrollbar (window), "notify::visible",
                         G_CALLBACK (gail_scrolled_window_scrollbar_visibility_changed),
                         obj, NULL, FALSE);
  g_signal_connect_data (gtk_scrolled_window_get_vscrollbar (window), "notify::visible",
                         G_CALLBACK (gail_scrolled_window_scrollbar_visibility_changed),
                         obj, NULL, FALSE);

  obj->role = ATK_ROLE_SCROLL_PANE;
}

static gint
gail_scrolled_window_get_n_children (AtkObject *object)
{
  GtkWidget *widget;
  GtkScrolledWindow *gtk_window;
  GList *children;
  gint n_children;
 
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (object));
  if (widget == NULL)
    /* Object is defunct */
    return 0;

  gtk_window = GTK_SCROLLED_WINDOW (widget);
   
  /* Get the number of children returned by the backing GtkScrolledWindow */

  children = gtk_container_get_children (GTK_CONTAINER(gtk_window));
  n_children = g_list_length (children);
  g_list_free (children);
  
  /* Add one to the count for each visible scrollbar */

  if (gtk_scrolled_window_get_hscrollbar (gtk_window))
    n_children++;
  if (gtk_scrolled_window_get_vscrollbar (gtk_window))
    n_children++;
  return n_children;
}

static AtkObject *
gail_scrolled_window_ref_child (AtkObject *obj, 
                                gint      child)
{
  GtkWidget *widget;
  GtkScrolledWindow *gtk_window;
  GtkWidget *hscrollbar, *vscrollbar;
  GList *children, *tmp_list;
  gint n_children;
  AtkObject  *accessible = NULL;

  g_return_val_if_fail (child >= 0, NULL);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    /* Object is defunct */
    return NULL;

  gtk_window = GTK_SCROLLED_WINDOW (widget);
  hscrollbar = gtk_scrolled_window_get_hscrollbar (gtk_window);
  vscrollbar = gtk_scrolled_window_get_vscrollbar (gtk_window);

  children = gtk_container_get_children (GTK_CONTAINER (gtk_window));
  n_children = g_list_length (children);

  if (child == n_children)
    {
      if (gtk_scrolled_window_get_hscrollbar (gtk_window))
        accessible = gtk_widget_get_accessible (hscrollbar);
      else if (gtk_scrolled_window_get_vscrollbar (gtk_window))
        accessible = gtk_widget_get_accessible (vscrollbar);
    }
  else if (child == n_children+1 && 
           gtk_scrolled_window_get_hscrollbar (gtk_window) &&
           gtk_scrolled_window_get_vscrollbar (gtk_window))
    accessible = gtk_widget_get_accessible (vscrollbar);
  else if (child < n_children)
    {
      tmp_list = g_list_nth (children, child);
      if (tmp_list)
	accessible = gtk_widget_get_accessible (
		GTK_WIDGET (tmp_list->data));
    }

  g_list_free (children);
  if (accessible)
    g_object_ref (accessible);
  return accessible; 
}

static void
gail_scrolled_window_scrollbar_visibility_changed (GObject    *object,
                                                   GParamSpec *pspec,
                                                   gpointer   user_data)
{
  if (!strcmp (pspec->name, "visible"))
    {
      gint index;
      gint n_children;
      gboolean child_added = FALSE;
      GList *children;
      AtkObject *child;
      GtkScrolledWindow *gtk_window;
      GtkWidget *hscrollbar, *vscrollbar;
      GailScrolledWindow *gail_window = GAIL_SCROLLED_WINDOW (user_data);
      gchar *signal_name;

      gtk_window = GTK_SCROLLED_WINDOW (gtk_accessible_get_widget (GTK_ACCESSIBLE (user_data)));
      if (gtk_window == NULL)
        return;
      children = gtk_container_get_children (GTK_CONTAINER (gtk_window));
      index = n_children = g_list_length (children);
      g_list_free (children);

      hscrollbar = gtk_scrolled_window_get_hscrollbar (gtk_window);
      vscrollbar = gtk_scrolled_window_get_vscrollbar (gtk_window);

      if ((gpointer) object == (gpointer) (hscrollbar))
        {
          if (gtk_scrolled_window_get_hscrollbar (gtk_window))
            child_added = TRUE;

          child = gtk_widget_get_accessible (hscrollbar);
        }
      else if ((gpointer) object == (gpointer) (vscrollbar))
        {
          if (gtk_scrolled_window_get_vscrollbar (gtk_window))
            child_added = TRUE;

          child = gtk_widget_get_accessible (vscrollbar);
          if (gtk_scrolled_window_get_hscrollbar (gtk_window))
            index = n_children+1;
        }
      else 
        {
          g_assert_not_reached ();
          return;
        }

      if (child_added)
        signal_name = "children_changed::add";
      else
        signal_name = "children_changed::delete";

      g_signal_emit_by_name (gail_window, signal_name, index, child, NULL);
    }
}
