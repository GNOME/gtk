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
#include "gtknotebookpageaccessible.h"


struct _GtkNotebookPageAccessiblePrivate
{
  GtkAccessible *notebook;
  GtkWidget *child;
};

static void atk_component_interface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkNotebookPageAccessible, gtk_notebook_page_accessible, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init))


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
get_label_from_notebook_page (GtkNotebookPageAccessible *page)
{
  GtkWidget *child;
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (gtk_accessible_get_widget (page->priv->notebook));
  if (!notebook)
    return NULL;

  if (!gtk_notebook_get_show_tabs (notebook))
    return NULL;

  child = gtk_notebook_get_tab_label (notebook, page->priv->child);

  if (GTK_IS_LABEL (child))
    return child;

  if (GTK_IS_CONTAINER (child))
    child = find_label_child (GTK_CONTAINER (child));

  return child;
}

static const gchar *
gtk_notebook_page_accessible_get_name (AtkObject *accessible)
{
  GtkWidget *label;

  if (accessible->name != NULL)
    return accessible->name;

  label = get_label_from_notebook_page (GTK_NOTEBOOK_PAGE_ACCESSIBLE (accessible));
  if (GTK_IS_LABEL (label))
    return gtk_label_get_text (GTK_LABEL (label));

  return NULL;
}

static AtkObject *
gtk_notebook_page_accessible_get_parent (AtkObject *accessible)
{
  GtkNotebookPageAccessible *page;

  page = GTK_NOTEBOOK_PAGE_ACCESSIBLE (accessible);

  return ATK_OBJECT (page->priv->notebook);
}

static gint
gtk_notebook_page_accessible_get_n_children (AtkObject *accessible)
{
  return 1;
}

static AtkObject *
gtk_notebook_page_accessible_ref_child (AtkObject *accessible,
                                        gint       i)
{
  AtkObject *child_obj;
  GtkNotebookPageAccessible *page = NULL;

  if (i != 0)
    return NULL;

  page = GTK_NOTEBOOK_PAGE_ACCESSIBLE (accessible);
  if (!page->priv->child)
    return NULL;

  child_obj = gtk_widget_get_accessible (page->priv->child);
  g_object_ref (child_obj);

  return child_obj;
}

static AtkStateSet *
gtk_notebook_page_accessible_ref_state_set (AtkObject *accessible)
{
  GtkNotebookPageAccessible *page = GTK_NOTEBOOK_PAGE_ACCESSIBLE (accessible);
  AtkStateSet *state_set, *label_state_set, *merged_state_set;
  AtkObject *atk_label;
  GtkWidget *label;
  AtkObject *selected;

  state_set = ATK_OBJECT_CLASS (gtk_notebook_page_accessible_parent_class)->ref_state_set (accessible);

  atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);

  selected = atk_selection_ref_selection (ATK_SELECTION (page->priv->notebook), 0);
  if (selected == accessible)
    atk_state_set_add_state (state_set, ATK_STATE_SELECTED);
  g_object_unref (selected);

  label = get_label_from_notebook_page (GTK_NOTEBOOK_PAGE_ACCESSIBLE (accessible));
  if (label)
    {
      atk_label = gtk_widget_get_accessible (label);
      label_state_set = atk_object_ref_state_set (atk_label);
      merged_state_set = atk_state_set_or_sets (state_set, label_state_set);
      g_object_unref (label_state_set);
      g_object_unref (state_set);
    }
  else
    {
      AtkObject *child;

      child = atk_object_ref_accessible_child (accessible, 0);
      if (!child)
        return state_set;

      merged_state_set = state_set;
      state_set = atk_object_ref_state_set (child);
      if (atk_state_set_contains_state (state_set, ATK_STATE_VISIBLE))
        {
          atk_state_set_add_state (merged_state_set, ATK_STATE_VISIBLE);
          if (atk_state_set_contains_state (state_set, ATK_STATE_ENABLED))
              atk_state_set_add_state (merged_state_set, ATK_STATE_ENABLED);
          if (atk_state_set_contains_state (state_set, ATK_STATE_SHOWING))
              atk_state_set_add_state (merged_state_set, ATK_STATE_SHOWING);

        }
      g_object_unref (state_set);
      g_object_unref (child);
    }
  return merged_state_set;
}

static gint
gtk_notebook_page_accessible_get_index_in_parent (AtkObject *accessible)
{
  GtkNotebookPageAccessible *page;

  page = GTK_NOTEBOOK_PAGE_ACCESSIBLE (accessible);
  if (!page->priv->child)
    return -1;

  return gtk_notebook_page_num (GTK_NOTEBOOK (gtk_accessible_get_widget (page->priv->notebook)),
                                page->priv->child);
}

static void
gtk_notebook_page_accessible_class_init (GtkNotebookPageAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_name = gtk_notebook_page_accessible_get_name;
  class->get_parent = gtk_notebook_page_accessible_get_parent;
  class->get_n_children = gtk_notebook_page_accessible_get_n_children;
  class->ref_child = gtk_notebook_page_accessible_ref_child;
  class->ref_state_set = gtk_notebook_page_accessible_ref_state_set;
  class->get_index_in_parent = gtk_notebook_page_accessible_get_index_in_parent;

  g_type_class_add_private (klass, sizeof (GtkNotebookPageAccessiblePrivate));
}

static void
gtk_notebook_page_accessible_init (GtkNotebookPageAccessible *page)
{
  page->priv = G_TYPE_INSTANCE_GET_PRIVATE (page,
                                            GTK_TYPE_NOTEBOOK_PAGE_ACCESSIBLE,
                                            GtkNotebookPageAccessiblePrivate);
}

static void
notify_tab_label (GObject    *object,
                  GParamSpec *pspec,
                  AtkObject  *atk_obj)
{
  if (atk_obj->name == NULL)
    g_object_notify (G_OBJECT (atk_obj), "accessible-name");
  g_signal_emit_by_name (atk_obj, "visible-data-changed");
}

AtkObject *
gtk_notebook_page_accessible_new (GtkNotebookAccessible *notebook,
                                  GtkWidget             *child)
{
  GObject *object;
  AtkObject *atk_object;
  GtkNotebookPageAccessible *page;

  g_return_val_if_fail (GTK_IS_NOTEBOOK_ACCESSIBLE (notebook), NULL);
  g_return_val_if_fail (GTK_WIDGET (child), NULL);

  object = g_object_new (GTK_TYPE_NOTEBOOK_PAGE_ACCESSIBLE, NULL);

  page = GTK_NOTEBOOK_PAGE_ACCESSIBLE (object);
  page->priv->notebook = GTK_ACCESSIBLE (notebook);
  page->priv->child = child;

  atk_object = ATK_OBJECT (page);
  atk_object->role = ATK_ROLE_PAGE_TAB;
  atk_object->layer = ATK_LAYER_WIDGET;

  atk_object_set_parent (gtk_widget_get_accessible (child), atk_object);

  g_signal_connect (gtk_accessible_get_widget (page->priv->notebook),
                    "child-notify::tab-label",
                    G_CALLBACK (notify_tab_label), page);

  return atk_object;
}

void
gtk_notebook_page_accessible_invalidate (GtkNotebookPageAccessible *page)
{
  AtkObject *obj = ATK_OBJECT (page);
  GtkWidget *notebook;

  notebook = gtk_accessible_get_widget (page->priv->notebook);
  if (notebook)
    g_signal_handlers_disconnect_by_func (notebook, notify_tab_label, page);

  atk_object_notify_state_change (obj, ATK_STATE_DEFUNCT, TRUE);
  atk_object_set_parent (obj, NULL);
  page->priv->notebook = NULL;
  atk_object_set_parent (gtk_widget_get_accessible (page->priv->child), NULL);
  page->priv->child = NULL;
}

static AtkObject*
gtk_notebook_page_accessible_ref_accessible_at_point (AtkComponent *component,
                                                      gint          x,
                                                      gint          y,
                                                      AtkCoordType  coord_type)
{
  /* There is only one child so we return it */
  AtkObject* child;

  child = atk_object_ref_accessible_child (ATK_OBJECT (component), 0);

  return child;
}

static void
gtk_notebook_page_accessible_get_extents (AtkComponent *component,
                                          gint         *x,
                                          gint         *y,
                                          gint         *width,
                                          gint         *height,
                                          AtkCoordType  coord_type)
{
  GtkWidget *label;
  AtkObject *atk_label;

  label = get_label_from_notebook_page (GTK_NOTEBOOK_PAGE_ACCESSIBLE (component));
  if (!label)
    {
      AtkObject *child;

      *width = 0;
      *height = 0;

      child = atk_object_ref_accessible_child (ATK_OBJECT (component), 0);
      if (!child)
        return;

      atk_component_get_position (ATK_COMPONENT (child), x, y, coord_type);
      g_object_unref (child);
    }
  else
    {
      atk_label = gtk_widget_get_accessible (label);
      atk_component_get_extents (ATK_COMPONENT (atk_label),
                                 x, y, width, height, coord_type);
    }
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  /* We use the default implementations for contains, get_position, get_size */
  iface->ref_accessible_at_point = gtk_notebook_page_accessible_ref_accessible_at_point;
  iface->get_extents = gtk_notebook_page_accessible_get_extents;
}
