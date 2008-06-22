/* GAIL - The GNOME Accessibility Implementation Library
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

#include <gtk/gtk.h>
#include "gailcontainer.h"

static void         gail_container_class_init          (GailContainerClass *klass);
static void         gail_container_init                (GailContainer      *container);

static gint         gail_container_get_n_children      (AtkObject          *obj);
static AtkObject*   gail_container_ref_child           (AtkObject          *obj,
                                                        gint               i);
static gint         gail_container_add_gtk             (GtkContainer       *container,
                                                        GtkWidget          *widget,
                                                        gpointer           data);
static gint         gail_container_remove_gtk          (GtkContainer       *container,
                                                        GtkWidget          *widget,
                                                        gpointer           data);
static gint         gail_container_real_add_gtk        (GtkContainer       *container,
                                                        GtkWidget          *widget,
                                                        gpointer           data);
static gint         gail_container_real_remove_gtk     (GtkContainer       *container,
                                                        GtkWidget          *widget,
                                                        gpointer           data);

static void          gail_container_real_initialize    (AtkObject          *obj,
                                                        gpointer           data);

static void          gail_container_finalize           (GObject            *object);

G_DEFINE_TYPE (GailContainer, gail_container, GAIL_TYPE_WIDGET)

static void
gail_container_class_init (GailContainerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  gobject_class->finalize = gail_container_finalize;

  class->get_n_children = gail_container_get_n_children;
  class->ref_child = gail_container_ref_child;
  class->initialize = gail_container_real_initialize;

  klass->add_gtk = gail_container_real_add_gtk;
  klass->remove_gtk = gail_container_real_remove_gtk;
}

static void
gail_container_init (GailContainer      *container)
{
  container->children = NULL;
}

static gint
gail_container_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  GList *children;
  gint count = 0;

  g_return_val_if_fail (GAIL_IS_CONTAINER (obj), count);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    return 0;

  children = gtk_container_get_children (GTK_CONTAINER(widget));
  count = g_list_length (children);
  g_list_free (children);

  return count; 
}

static AtkObject* 
gail_container_ref_child (AtkObject *obj,
                          gint       i)
{
  GList *children, *tmp_list;
  AtkObject  *accessible;
  GtkWidget *widget;

  g_return_val_if_fail (GAIL_IS_CONTAINER (obj), NULL);
  g_return_val_if_fail ((i >= 0), NULL);
  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    return NULL;

  children = gtk_container_get_children (GTK_CONTAINER (widget));
  tmp_list = g_list_nth (children, i);
  if (!tmp_list)
    {
      g_list_free (children);
      return NULL;
    }  
  accessible = gtk_widget_get_accessible (GTK_WIDGET (tmp_list->data));

  g_list_free (children);
  g_object_ref (accessible);
  return accessible; 
}

static gint
gail_container_add_gtk (GtkContainer *container,
                        GtkWidget    *widget,
                        gpointer     data)
{
  GailContainer *gail_container = GAIL_CONTAINER (data);
  GailContainerClass *klass;

  klass = GAIL_CONTAINER_GET_CLASS (gail_container);

  if (klass->add_gtk)
    return klass->add_gtk (container, widget, data);
  else
    return 1;
}
 
static gint
gail_container_remove_gtk (GtkContainer *container,
                           GtkWidget    *widget,
                           gpointer     data)
{
  GailContainer *gail_container = GAIL_CONTAINER (data);
  GailContainerClass *klass;

  klass = GAIL_CONTAINER_GET_CLASS (gail_container);

  if (klass->remove_gtk)
    return klass->remove_gtk (container, widget, data);
  else
    return 1;
}
 
static gint
gail_container_real_add_gtk (GtkContainer *container,
                             GtkWidget    *widget,
                             gpointer     data)
{
  AtkObject* atk_parent = ATK_OBJECT (data);
  AtkObject* atk_child = gtk_widget_get_accessible (widget);
  GailContainer *gail_container = GAIL_CONTAINER (atk_parent);
  gint       index;

  g_object_notify (G_OBJECT (atk_child), "accessible_parent");

  g_list_free (gail_container->children);
  gail_container->children = gtk_container_get_children (container);
  index = g_list_index (gail_container->children, widget);
  g_signal_emit_by_name (atk_parent, "children_changed::add", 
                         index, atk_child, NULL);

  return 1;
}

static gint
gail_container_real_remove_gtk (GtkContainer       *container,
                                GtkWidget          *widget,
                                gpointer           data)
{
  AtkPropertyValues values = { NULL };
  AtkObject* atk_parent;
  AtkObject *atk_child;
  GailContainer *gail_container;
  gint       index;

  atk_parent = ATK_OBJECT (data);
  atk_child = gtk_widget_get_accessible (widget);

  if (atk_child)
    {
      g_value_init (&values.old_value, G_TYPE_POINTER);
      g_value_set_pointer (&values.old_value, atk_parent);
    
      values.property_name = "accessible-parent";

      g_object_ref (atk_child);
      g_signal_emit_by_name (atk_child,
                             "property_change::accessible-parent", &values, NULL);
      g_object_unref (atk_child);
    }
  gail_container = GAIL_CONTAINER (atk_parent);
  index = g_list_index (gail_container->children, widget);
  g_list_free (gail_container->children);
  gail_container->children = gtk_container_get_children (container);
  if (index >= 0 && index <= g_list_length (gail_container->children))
    g_signal_emit_by_name (atk_parent, "children_changed::remove", 
			   index, atk_child, NULL);

  return 1;
}

static void
gail_container_real_initialize (AtkObject *obj,
                                gpointer  data)
{
  GailContainer *container = GAIL_CONTAINER (obj);
  guint handler_id;

  ATK_OBJECT_CLASS (gail_container_parent_class)->initialize (obj, data);

  container->children = gtk_container_get_children (GTK_CONTAINER (data));

  /*
   * We store the handler ids for these signals in case some objects
   * need to remove these handlers.
   */
  handler_id = g_signal_connect (data,
                                 "add",
                                 G_CALLBACK (gail_container_add_gtk),
                                 obj);
  g_object_set_data (G_OBJECT (obj), "gail-add-handler-id", 
                     GUINT_TO_POINTER (handler_id));
  handler_id = g_signal_connect (data,
                                 "remove",
                                 G_CALLBACK (gail_container_remove_gtk),
                                 obj);
  g_object_set_data (G_OBJECT (obj), "gail-remove-handler-id", 
                     GUINT_TO_POINTER (handler_id));

  if (GTK_IS_TOOLBAR (data))
    obj->role = ATK_ROLE_TOOL_BAR;
  else if (GTK_IS_VIEWPORT (data))
    obj->role = ATK_ROLE_VIEWPORT;
  else
    obj->role = ATK_ROLE_PANEL;
}

static void
gail_container_finalize (GObject *object)
{
  GailContainer *container = GAIL_CONTAINER (object);

  g_list_free (container->children);
  G_OBJECT_CLASS (gail_container_parent_class)->finalize (object);
}
