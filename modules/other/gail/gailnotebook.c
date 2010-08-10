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

#include <string.h>
#include <gtk/gtk.h>
#include "gailnotebook.h"
#include "gailnotebookpage.h"
#include "gail-private-macros.h"

static void         gail_notebook_class_init          (GailNotebookClass *klass);
static void         gail_notebook_init                (GailNotebook      *notebook);
static void         gail_notebook_finalize            (GObject           *object);
static void         gail_notebook_real_initialize     (AtkObject         *obj,
                                                       gpointer          data);

static void         gail_notebook_real_notify_gtk     (GObject           *obj,
                                                       GParamSpec        *pspec);

static AtkObject*   gail_notebook_ref_child           (AtkObject      *obj,
                                                       gint           i);
static gint         gail_notebook_real_remove_gtk     (GtkContainer   *container,
                                                       GtkWidget      *widget,
                                                       gpointer       data);    
static void         atk_selection_interface_init      (AtkSelectionIface *iface);
static gboolean     gail_notebook_add_selection       (AtkSelection   *selection,
                                                       gint           i);
static AtkObject*   gail_notebook_ref_selection       (AtkSelection   *selection,
                                                       gint           i);
static gint         gail_notebook_get_selection_count (AtkSelection   *selection);
static gboolean     gail_notebook_is_child_selected   (AtkSelection   *selection,
                                                       gint           i);
static AtkObject*   find_child_in_list                (GList          *list,
                                                       gint           index);
static void         check_cache                       (GailNotebook   *gail_notebook,
                                                       GtkNotebook    *notebook);
static void         reset_cache                       (GailNotebook   *gail_notebook,
                                                       gint           index);
static void         create_notebook_page_accessible   (GailNotebook   *gail_notebook,
                                                       GtkNotebook    *notebook,
                                                       gint           index,
                                                       gboolean       insert_before,
                                                       GList          *list);
static void         gail_notebook_child_parent_set    (GtkWidget      *widget,
                                                       GtkWidget      *old_parent,
                                                       gpointer       data);
static gboolean     gail_notebook_focus_cb            (GtkWidget      *widget,
                                                       GtkDirectionType type);
static gboolean     gail_notebook_check_focus_tab     (gpointer       data);
static void         gail_notebook_destroyed           (gpointer       data);


G_DEFINE_TYPE_WITH_CODE (GailNotebook, gail_notebook, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gail_notebook_class_init (GailNotebookClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;
  GailContainerClass *container_class;

  widget_class = (GailWidgetClass*)klass;
  container_class = (GailContainerClass*)klass;

  gobject_class->finalize = gail_notebook_finalize;

  widget_class->notify_gtk = gail_notebook_real_notify_gtk;

  class->ref_child = gail_notebook_ref_child;
  class->initialize = gail_notebook_real_initialize;
  /*
   * We do not provide an implementation of get_n_children
   * as the implementation in GailContainer returns the correct
   * number of children.
   */
  container_class->remove_gtk = gail_notebook_real_remove_gtk;
}

static void
gail_notebook_init (GailNotebook      *notebook)
{
  notebook->page_cache = NULL;
  notebook->selected_page = -1;
  notebook->focus_tab_page = -1;
  notebook->remove_index = -1;
  notebook->idle_focus_id = 0;
}

static AtkObject*
gail_notebook_ref_child (AtkObject      *obj,
                         gint           i)
{
  AtkObject *accessible = NULL;
  GailNotebook *gail_notebook;
  GtkNotebook *gtk_notebook;
  GtkWidget *widget;
 
  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  gail_notebook = GAIL_NOTEBOOK (obj);
  
  gtk_notebook = GTK_NOTEBOOK (widget);
  
  if (gail_notebook->page_count < g_list_length (gtk_notebook->children))
    check_cache (gail_notebook, gtk_notebook);

  accessible = find_child_in_list (gail_notebook->page_cache, i);

  if (accessible != NULL)
    g_object_ref (accessible);

  return accessible;
}

static void
gail_notebook_page_added (GtkNotebook *gtk_notebook,
                          GtkWidget   *child,
                          guint        page_num,
                          gpointer     data)
{
  AtkObject *atk_obj;
  GailNotebook *notebook;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (gtk_notebook));
  notebook = GAIL_NOTEBOOK (atk_obj);
  create_notebook_page_accessible (notebook, gtk_notebook, page_num, FALSE, NULL);
  notebook->page_count++;
}

static void
gail_notebook_real_initialize (AtkObject *obj,
                               gpointer  data)
{
  GailNotebook *notebook;
  GtkNotebook *gtk_notebook;
  gint i;

  ATK_OBJECT_CLASS (gail_notebook_parent_class)->initialize (obj, data);

  notebook = GAIL_NOTEBOOK (obj);
  gtk_notebook = GTK_NOTEBOOK (data);
  for (i = 0; i < g_list_length (gtk_notebook->children); i++)
    {
      create_notebook_page_accessible (notebook, gtk_notebook, i, FALSE, NULL);
    }
  notebook->page_count = i;
  notebook->selected_page = gtk_notebook_get_current_page (gtk_notebook);
  if (gtk_notebook->focus_tab && gtk_notebook->focus_tab->data)
    {
      notebook->focus_tab_page = g_list_index (gtk_notebook->children, gtk_notebook->focus_tab->data);
    }
  g_signal_connect (gtk_notebook,
                    "focus",
                    G_CALLBACK (gail_notebook_focus_cb),
                    NULL);
  g_signal_connect (gtk_notebook,
                    "page-added",
                    G_CALLBACK (gail_notebook_page_added),
                    NULL);
  g_object_weak_ref (G_OBJECT(gtk_notebook),
                     (GWeakNotify) gail_notebook_destroyed,
                     obj);                     

  obj->role = ATK_ROLE_PAGE_TAB_LIST;
}

static void
gail_notebook_real_notify_gtk (GObject           *obj,
                               GParamSpec        *pspec)
{
  GtkWidget *widget;
  AtkObject* atk_obj;

  widget = GTK_WIDGET (obj);
  atk_obj = gtk_widget_get_accessible (widget);

  if (strcmp (pspec->name, "page") == 0)
    {
      gint page_num, old_page_num;
      gint focus_page_num = 0;
      gint old_focus_page_num;
      GailNotebook *gail_notebook;
      GtkNotebook *gtk_notebook;
     
      gail_notebook = GAIL_NOTEBOOK (atk_obj);
      gtk_notebook = GTK_NOTEBOOK (widget);
     
      if (gail_notebook->page_count < g_list_length (gtk_notebook->children))
       check_cache (gail_notebook, gtk_notebook);
      /*
       * Notify SELECTED state change for old and new page
       */
      old_page_num = gail_notebook->selected_page;
      page_num = gtk_notebook_get_current_page (gtk_notebook);
      gail_notebook->selected_page = page_num;
      old_focus_page_num = gail_notebook->focus_tab_page;
      if (gtk_notebook->focus_tab && gtk_notebook->focus_tab->data)
        {
          focus_page_num = g_list_index (gtk_notebook->children, gtk_notebook->focus_tab->data);
          gail_notebook->focus_tab_page = focus_page_num;
        }
    
      if (page_num != old_page_num)
        {
          AtkObject *obj;

          if (old_page_num != -1)
            {
              obj = gail_notebook_ref_child (atk_obj, old_page_num);
              if (obj)
                {
                  atk_object_notify_state_change (obj,
                                                  ATK_STATE_SELECTED,
                                                  FALSE);
                  g_object_unref (obj);
                }
            }
          obj = gail_notebook_ref_child (atk_obj, page_num);
          if (obj)
            {
              atk_object_notify_state_change (obj,
                                              ATK_STATE_SELECTED,
                                              TRUE);
              g_object_unref (obj);
              /*
               * The page which is being displayed has changed but there is
               * no need to tell the focus tracker as the focus page will also 
               * change or a widget in the page will receive focus if the
               * Notebook does not have tabs.
               */
            }
          g_signal_emit_by_name (atk_obj, "selection_changed");
          g_signal_emit_by_name (atk_obj, "visible_data_changed");
        }
      if (gtk_notebook_get_show_tabs (gtk_notebook) &&
         (focus_page_num != old_focus_page_num))
        {
          if (gail_notebook->idle_focus_id)
            g_source_remove (gail_notebook->idle_focus_id);
          gail_notebook->idle_focus_id = gdk_threads_add_idle (gail_notebook_check_focus_tab, atk_obj);
        }
    }
  else
    GAIL_WIDGET_CLASS (gail_notebook_parent_class)->notify_gtk (obj, pspec);
}

static void
gail_notebook_finalize (GObject            *object)
{
  GailNotebook *notebook = GAIL_NOTEBOOK (object);
  GList *list;

  /*
   * Get rid of the GailNotebookPage objects which we have cached.
   */
  list = notebook->page_cache;
  if (list != NULL)
    {
      while (list)
        {
          g_object_unref (list->data);
          list = list->next;
        }
    }

  g_list_free (notebook->page_cache);

  if (notebook->idle_focus_id)
    g_source_remove (notebook->idle_focus_id);

  G_OBJECT_CLASS (gail_notebook_parent_class)->finalize (object);
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gail_notebook_add_selection;
  iface->ref_selection = gail_notebook_ref_selection;
  iface->get_selection_count = gail_notebook_get_selection_count;
  iface->is_child_selected = gail_notebook_is_child_selected;
  /*
   * The following don't make any sense for GtkNotebook widgets.
   * Unsupported AtkSelection interfaces:
   * clear_selection();
   * remove_selection();
   * select_all_selection();
   */
}

/*
 * GtkNotebook only supports the selection of one page at a time. 
 * Selecting a page unselects any previous selection, so this 
 * changes the current selection instead of adding to it.
 */
static gboolean
gail_notebook_add_selection (AtkSelection *selection,
                             gint         i)
{
  GtkNotebook *notebook;
  GtkWidget *widget;
  
  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;
  
  notebook = GTK_NOTEBOOK (widget);
  gtk_notebook_set_current_page (notebook, i);
  return TRUE;
}

static AtkObject*
gail_notebook_ref_selection (AtkSelection *selection,
                             gint i)
{
  AtkObject *accessible;
  GtkWidget *widget;
  GtkNotebook *notebook;
  gint pagenum;
  
  /*
   * A note book can have only one selection.
   */
  gail_return_val_if_fail (i == 0, NULL);
  g_return_val_if_fail (GAIL_IS_NOTEBOOK (selection), NULL);
  
  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
	return NULL;
  
  notebook = GTK_NOTEBOOK (widget);
  pagenum = gtk_notebook_get_current_page (notebook);
  gail_return_val_if_fail (pagenum != -1, NULL);
  accessible = gail_notebook_ref_child (ATK_OBJECT (selection), pagenum);

  return accessible;
}

/*
 * Always return 1 because there can only be one page
 * selected at any time
 */
static gint
gail_notebook_get_selection_count (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkNotebook *notebook;
  
  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return 0;

  notebook = GTK_NOTEBOOK (widget);
  if (notebook == NULL || gtk_notebook_get_current_page (notebook) == -1)
    return 0;
  else
    return 1;
}

static gboolean
gail_notebook_is_child_selected (AtkSelection *selection,
                                 gint i)
{
  GtkWidget *widget;
  GtkNotebook *notebook;
  gint pagenumber;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* 
     * State is defunct
     */
    return FALSE;

  
  notebook = GTK_NOTEBOOK (widget);
  pagenumber = gtk_notebook_get_current_page(notebook);

  if (pagenumber == i)
    return TRUE;
  else
    return FALSE; 
}

static AtkObject*
find_child_in_list (GList *list,
                    gint  index)
{
  AtkObject *obj = NULL;

  while (list)
    {
      if (GAIL_NOTEBOOK_PAGE (list->data)->index == index)
        {
          obj = ATK_OBJECT (list->data);
          break;
        }
      list = list->next;
    }
  return obj;
}

static void
check_cache (GailNotebook *gail_notebook,
             GtkNotebook  *notebook)
{
  GList *gtk_list;
  GList *gail_list;
  gint i;

  gtk_list = notebook->children;
  gail_list = gail_notebook->page_cache;

  i = 0;
  while (gtk_list)
    {
      if (!gail_list)
        {
          create_notebook_page_accessible (gail_notebook, notebook, i, FALSE, NULL);
        }
      else if (GAIL_NOTEBOOK_PAGE (gail_list->data)->page != gtk_list->data)
        {
          create_notebook_page_accessible (gail_notebook, notebook, i, TRUE, gail_list);
        }
      else
        {
          gail_list = gail_list->next;
        }
      i++;
      gtk_list = gtk_list->next;
    }
  gail_notebook->page_count = i;
}

static void
reset_cache (GailNotebook *gail_notebook,
             gint         index)
{
  GList *l;

  for (l = gail_notebook->page_cache; l; l = l->next)
    {
      if (GAIL_NOTEBOOK_PAGE (l->data)->index > index)
        GAIL_NOTEBOOK_PAGE (l->data)->index -= 1;
    }
}

static void
create_notebook_page_accessible (GailNotebook *gail_notebook,
                                 GtkNotebook  *notebook,
                                 gint         index,
                                 gboolean     insert_before,
                                 GList        *list)
{
  AtkObject *obj;

  obj = gail_notebook_page_new (notebook, index);
  g_object_ref (obj);
  if (insert_before)
    gail_notebook->page_cache = g_list_insert_before (gail_notebook->page_cache, list, obj);
  else
    gail_notebook->page_cache = g_list_append (gail_notebook->page_cache, obj);
  g_signal_connect (gtk_notebook_get_nth_page (notebook, index), 
                    "parent_set",
                    G_CALLBACK (gail_notebook_child_parent_set),
                    obj);
}

static void
gail_notebook_child_parent_set (GtkWidget *widget,
                                GtkWidget *old_parent,
                                gpointer   data)
{
  GailNotebook *gail_notebook;

  gail_return_if_fail (old_parent != NULL);
  gail_notebook = GAIL_NOTEBOOK (gtk_widget_get_accessible (old_parent));
  gail_notebook->remove_index = GAIL_NOTEBOOK_PAGE (data)->index;
}

static gint
gail_notebook_real_remove_gtk (GtkContainer *container,
                               GtkWidget    *widget,
                               gpointer      data)    
{
  GailNotebook *gail_notebook;
  AtkObject *obj;
  gint index;

  g_return_val_if_fail (container != NULL, 1);
  gail_notebook = GAIL_NOTEBOOK (gtk_widget_get_accessible (GTK_WIDGET (container)));
  index = gail_notebook->remove_index;
  gail_notebook->remove_index = -1;

  obj = find_child_in_list (gail_notebook->page_cache, index);
  g_return_val_if_fail (obj, 1);
  gail_notebook->page_cache = g_list_remove (gail_notebook->page_cache, obj);
  gail_notebook->page_count -= 1;
  reset_cache (gail_notebook, index);
  g_signal_emit_by_name (gail_notebook,
                         "children_changed::remove",
                          GAIL_NOTEBOOK_PAGE (obj)->index, 
                          obj, NULL);
  g_object_unref (obj);
  return 1;
}

static gboolean
gail_notebook_focus_cb (GtkWidget      *widget,
                        GtkDirectionType type)
{
  AtkObject *atk_obj = gtk_widget_get_accessible (widget);
  GailNotebook *gail_notebook = GAIL_NOTEBOOK (atk_obj);

  switch (type)
    {
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      if (gail_notebook->idle_focus_id == 0)
        gail_notebook->idle_focus_id = gdk_threads_add_idle (gail_notebook_check_focus_tab, atk_obj);
      break;
    default:
      break;
    }
  return FALSE;
}

static gboolean
gail_notebook_check_focus_tab (gpointer data)
{
  GtkWidget *widget;
  AtkObject *atk_obj;
  gint focus_page_num, old_focus_page_num;
  GailNotebook *gail_notebook;
  GtkNotebook *gtk_notebook;

  atk_obj = ATK_OBJECT (data);
  gail_notebook = GAIL_NOTEBOOK (atk_obj);
  widget = GTK_ACCESSIBLE (atk_obj)->widget;

  gtk_notebook = GTK_NOTEBOOK (widget);

  gail_notebook->idle_focus_id = 0;

  if (!gtk_notebook->focus_tab)
    return FALSE;

  old_focus_page_num = gail_notebook->focus_tab_page;
  focus_page_num = g_list_index (gtk_notebook->children, gtk_notebook->focus_tab->data);
  gail_notebook->focus_tab_page = focus_page_num;
  if (old_focus_page_num != focus_page_num)
    {
      AtkObject *obj;

      obj = atk_object_ref_accessible_child (atk_obj, focus_page_num);
      atk_focus_tracker_notify (obj);
      g_object_unref (obj);
    }

  return FALSE;
}

static void
gail_notebook_destroyed (gpointer data)
{
  GailNotebook *gail_notebook = GAIL_NOTEBOOK (data);

  if (gail_notebook->idle_focus_id)
    {
      g_source_remove (gail_notebook->idle_focus_id);
      gail_notebook->idle_focus_id = 0;
    }
}
