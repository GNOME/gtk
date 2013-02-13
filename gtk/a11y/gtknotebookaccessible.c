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
#include "gtknotebookaccessible.h"
#include "gtknotebookpageaccessible.h"

struct _GtkNotebookAccessiblePrivate
{
  /*
   * page_cache maintains a list of pre-ref'd Notebook Pages.
   * This cache is queried by gtk_notebook_accessible_ref_child().
   * If the page is found in the list then a new page does not
   * need to be created
   */
  GHashTable * pages;
  gint         selected_page;
};

static void atk_selection_interface_init (AtkSelectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkNotebookAccessible, gtk_notebook_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkNotebookAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
create_notebook_page_accessible (GtkNotebookAccessible *accessible,
                                 GtkNotebook           *notebook,
                                 GtkWidget             *child,
                                 gint                   page_num)
{
  AtkObject *obj;

  obj = gtk_notebook_page_accessible_new (accessible, child);
  g_hash_table_insert (accessible->priv->pages, child, obj);
  atk_object_set_parent (obj, ATK_OBJECT (accessible));
  g_signal_emit_by_name (accessible, "children-changed::add", page_num, obj, NULL);
}

static void
page_added_cb (GtkNotebook *notebook,
               GtkWidget   *child,
               guint        page_num,
               gpointer     data)
{
  AtkObject *atk_obj;
  GtkNotebookAccessible *accessible;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (notebook));
  accessible = GTK_NOTEBOOK_ACCESSIBLE (atk_obj);
  create_notebook_page_accessible (accessible, notebook, child, page_num);
}

static void
page_removed_cb (GtkNotebook *notebook,
                 GtkWidget   *widget,
                 guint        page_num,
                 gpointer     data)
{
  GtkNotebookAccessible *accessible;
  AtkObject *obj;

  accessible = GTK_NOTEBOOK_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (notebook)));

  obj = g_hash_table_lookup (accessible->priv->pages, widget);
  g_return_if_fail (obj);
  g_signal_emit_by_name (accessible, "children-changed::remove",
                         page_num, obj, NULL);
  gtk_notebook_page_accessible_invalidate (GTK_NOTEBOOK_PAGE_ACCESSIBLE (obj));
  g_hash_table_remove (accessible->priv->pages, widget);
}


static void
gtk_notebook_accessible_initialize (AtkObject *obj,
                                    gpointer   data)
{
  GtkNotebookAccessible *accessible;
  GtkNotebook *notebook;
  gint i;

  ATK_OBJECT_CLASS (gtk_notebook_accessible_parent_class)->initialize (obj, data);

  accessible = GTK_NOTEBOOK_ACCESSIBLE (obj);
  notebook = GTK_NOTEBOOK (data);
  for (i = 0; i < gtk_notebook_get_n_pages (notebook); i++)
    {
      create_notebook_page_accessible (accessible,
                                       notebook,
                                       gtk_notebook_get_nth_page (notebook, i),
                                       i);
    }
  accessible->priv->selected_page = gtk_notebook_get_current_page (notebook);

  g_signal_connect (notebook, "page-added",
                    G_CALLBACK (page_added_cb), NULL);
  g_signal_connect (notebook, "page-removed",
                    G_CALLBACK (page_removed_cb), NULL);

  obj->role = ATK_ROLE_PAGE_TAB_LIST;
}

static void
gtk_notebook_accessible_finalize (GObject *object)
{
  GtkNotebookAccessible *accessible = GTK_NOTEBOOK_ACCESSIBLE (object);

  g_hash_table_destroy (accessible->priv->pages);

  G_OBJECT_CLASS (gtk_notebook_accessible_parent_class)->finalize (object);
}

static AtkObject *
gtk_notebook_accessible_ref_child (AtkObject *obj,
                                   gint       i)
{
  AtkObject *child;
  GtkNotebookAccessible *accessible;
  GtkNotebook *notebook;
  GtkWidget *widget;
 
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  accessible = GTK_NOTEBOOK_ACCESSIBLE (obj);
  notebook = GTK_NOTEBOOK (widget);

  child = g_hash_table_lookup (accessible->priv->pages,
                               gtk_notebook_get_nth_page (notebook, i));
  /* can return NULL when i >= n_children */

  if (child)
    g_object_ref (child);

  return child;
}

static void
gtk_notebook_accessible_notify_gtk (GObject    *obj,
                                    GParamSpec *pspec)
{
  GtkWidget *widget;
  AtkObject* atk_obj;

  widget = GTK_WIDGET (obj);
  atk_obj = gtk_widget_get_accessible (widget);

  if (strcmp (pspec->name, "page") == 0)
    {
      gint page_num, old_page_num;
      GtkNotebookAccessible *accessible;
      GtkNotebook *notebook;

      accessible = GTK_NOTEBOOK_ACCESSIBLE (atk_obj);
      notebook = GTK_NOTEBOOK (widget);

      /* Notify SELECTED state change for old and new page */
      old_page_num = accessible->priv->selected_page;
      page_num = gtk_notebook_get_current_page (notebook);
      accessible->priv->selected_page = page_num;

      if (page_num != old_page_num)
        {
          AtkObject *child;

          if (old_page_num != -1)
            {
              child = gtk_notebook_accessible_ref_child (atk_obj, old_page_num);
              if (child)
                {
                  atk_object_notify_state_change (child, ATK_STATE_SELECTED, FALSE);
                  g_object_unref (child);
                }
            }
          child = gtk_notebook_accessible_ref_child (atk_obj, page_num);
          if (child)
            {
              atk_object_notify_state_change (child, ATK_STATE_SELECTED, TRUE);
              g_object_unref (child);
            }
          g_signal_emit_by_name (atk_obj, "selection-changed");
          g_signal_emit_by_name (atk_obj, "visible-data-changed");
        }
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_notebook_accessible_parent_class)->notify_gtk (obj, pspec);
}

/*
 * GtkNotebook only supports the selection of one page at a time.
 * Selecting a page unselects any previous selection, so this
 * changes the current selection instead of adding to it.
 */
static gboolean
gtk_notebook_accessible_add_selection (AtkSelection *selection,
                                       gint          i)
{
  GtkNotebook *notebook;
  GtkWidget *widget;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  notebook = GTK_NOTEBOOK (widget);
  gtk_notebook_set_current_page (notebook, i);
  return TRUE;
}

static void
gtk_notebook_accessible_class_init (GtkNotebookAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;
  GtkContainerAccessibleClass *container_class = (GtkContainerAccessibleClass*)klass;

  gobject_class->finalize = gtk_notebook_accessible_finalize;

  class->ref_child = gtk_notebook_accessible_ref_child;
  class->initialize = gtk_notebook_accessible_initialize;

  widget_class->notify_gtk = gtk_notebook_accessible_notify_gtk;

  /* we listen to page-added/-removed, so we don't care about these */
  container_class->add_gtk = NULL;
  container_class->remove_gtk = NULL;
}

static void
gtk_notebook_accessible_init (GtkNotebookAccessible *notebook)
{
  notebook->priv = gtk_notebook_accessible_get_instance_private (notebook);
  notebook->priv->pages = g_hash_table_new_full (g_direct_hash,
                                                 g_direct_equal,
                                                 NULL,
                                                 g_object_unref);
  notebook->priv->selected_page = -1;
}

static AtkObject *
gtk_notebook_accessible_ref_selection (AtkSelection *selection,
                                       gint          i)
{
  AtkObject *accessible;
  GtkWidget *widget;
  GtkNotebook *notebook;
  gint pagenum;

  if (i != 0)
    return NULL;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return NULL;

  notebook = GTK_NOTEBOOK (widget);
  pagenum = gtk_notebook_get_current_page (notebook);
  if (pagenum == -1)
    return NULL;
  accessible = gtk_notebook_accessible_ref_child (ATK_OBJECT (selection), pagenum);

  return accessible;
}

/* Always return 1 because there can only be one page
 * selected at any time
 */
static gint
gtk_notebook_accessible_get_selection_count (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkNotebook *notebook;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return 0;

  notebook = GTK_NOTEBOOK (widget);
  if (notebook == NULL || gtk_notebook_get_current_page (notebook) == -1)
    return 0;

  return 1;
}

static gboolean
gtk_notebook_accessible_is_child_selected (AtkSelection *selection,
                                           gint          i)
{
  GtkWidget *widget;
  GtkNotebook *notebook;
  gint pagenumber;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  notebook = GTK_NOTEBOOK (widget);
  pagenumber = gtk_notebook_get_current_page(notebook);

  if (pagenumber == i)
    return TRUE;

  return FALSE;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_notebook_accessible_add_selection;
  iface->ref_selection = gtk_notebook_accessible_ref_selection;
  iface->get_selection_count = gtk_notebook_accessible_get_selection_count;
  iface->is_child_selected = gtk_notebook_accessible_is_child_selected;
}
