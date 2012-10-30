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

#include <string.h>
#include <gtk/gtk.h>
#include "gtkstatusbaraccessible.h"


G_DEFINE_TYPE (GtkStatusbarAccessible, gtk_statusbar_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE)

static void
text_changed (GtkStatusbar *statusbar,
              guint         context_id,
              const gchar  *text,
              AtkObject    *obj)
{
  if (!obj->name)
    g_object_notify (G_OBJECT (obj), "accessible-name");
  g_signal_emit_by_name (obj, "visible-data-changed");
}

static void
gtk_statusbar_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  GtkWidget *statusbar = data;

  ATK_OBJECT_CLASS (gtk_statusbar_accessible_parent_class)->initialize (obj, data);

  g_signal_connect_after (statusbar, "text-pushed",
                          G_CALLBACK (text_changed), obj);
  g_signal_connect_after (statusbar, "text-popped",
                          G_CALLBACK (text_changed), obj);

  obj->role = ATK_ROLE_STATUSBAR;
}

static GtkWidget *
find_label_child (GtkContainer *container)
{
  GList *children, *tmp_list;
  GtkWidget *child;

  children = gtk_container_get_children (container);

  child = NULL;
  for (tmp_list = children; tmp_list != NULL; tmp_list = tmp_list->next)
    {
      if (GTK_IS_LABEL (tmp_list->data))
        {
          child = GTK_WIDGET (tmp_list->data);
          break;
        }
      else if (GTK_IS_CONTAINER (tmp_list->data))
        {
          child = find_label_child (GTK_CONTAINER (tmp_list->data));
          if (child)
            break;
        }
    }
  g_list_free (children);

  return child;
}

static GtkWidget *
get_label_from_statusbar (GtkStatusbar *statusbar)
{
  GtkWidget *box;

  box = gtk_statusbar_get_message_area (statusbar);

  return find_label_child (GTK_CONTAINER (box));
}

static const gchar *
gtk_statusbar_accessible_get_name (AtkObject *obj)
{
  const gchar *name;
  GtkWidget *widget;
  GtkWidget *label;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  name = ATK_OBJECT_CLASS (gtk_statusbar_accessible_parent_class)->get_name (obj);
  if (name != NULL)
    return name;

  label = get_label_from_statusbar (GTK_STATUSBAR (widget));
  if (GTK_IS_LABEL (label))
    return gtk_label_get_label (GTK_LABEL (label));

  return NULL;
}

static gint
gtk_statusbar_accessible_get_n_children (AtkObject *obj)
{
  return 0;
}

static AtkObject*
gtk_statusbar_accessible_ref_child (AtkObject *obj,
                                    gint       i)
{
  return NULL;
}

static void
gtk_statusbar_accessible_class_init (GtkStatusbarAccessibleClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GtkContainerAccessibleClass *container_class = (GtkContainerAccessibleClass*)klass;

  class->get_name = gtk_statusbar_accessible_get_name;
  class->get_n_children = gtk_statusbar_accessible_get_n_children;
  class->ref_child = gtk_statusbar_accessible_ref_child;
  class->initialize = gtk_statusbar_accessible_initialize;
  /*
   * As we report the statusbar as having no children
   * we are not interested in add and remove signals
   */
  container_class->add_gtk = NULL;
  container_class->remove_gtk = NULL;
}

static void
gtk_statusbar_accessible_init (GtkStatusbarAccessible *bar)
{
}
