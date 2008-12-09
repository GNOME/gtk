/* GAIL - The GNOME Accessibility Enabling Library
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>
#include "gaillist.h"
#include "gailcombo.h"

static void         gail_list_class_init            (GailListClass  *klass); 

static void         gail_list_init                  (GailList       *list);

static void         gail_list_initialize            (AtkObject      *accessible,
                                                     gpointer        data);

static gint         gail_list_get_index_in_parent   (AtkObject      *accessible);

static void         atk_selection_interface_init    (AtkSelectionIface *iface);
static gboolean     gail_list_add_selection         (AtkSelection   *selection,
                                                     gint           i);
static gboolean     gail_list_clear_selection       (AtkSelection   *selection);
static AtkObject*   gail_list_ref_selection         (AtkSelection   *selection,
                                                     gint           i);
static gint         gail_list_get_selection_count   (AtkSelection   *selection);
static gboolean     gail_list_is_child_selected     (AtkSelection   *selection,
                                                     gint           i);
static gboolean     gail_list_remove_selection      (AtkSelection   *selection,
                                                     gint           i);


G_DEFINE_TYPE_WITH_CODE (GailList, gail_list, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gail_list_class_init (GailListClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gail_list_initialize;
  class->get_index_in_parent = gail_list_get_index_in_parent;
}

static void
gail_list_init (GailList *list)
{
}

static void
gail_list_initialize (AtkObject *accessible,
                      gpointer  data)
{
  ATK_OBJECT_CLASS (gail_list_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_LIST;
}

static gint
gail_list_get_index_in_parent (AtkObject *accessible)
{
  /*
   * If the parent widget is a combo box then the index is 0
   * otherwise do the normal thing.
   */
  if (accessible->accessible_parent)
  {
    if (GAIL_IS_COMBO (accessible->accessible_parent))
      return 0;
  }
  return ATK_OBJECT_CLASS (gail_list_parent_class)->get_index_in_parent (accessible);
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gail_list_add_selection;
  iface->clear_selection = gail_list_clear_selection;
  iface->ref_selection = gail_list_ref_selection;
  iface->get_selection_count = gail_list_get_selection_count;
  iface->is_child_selected = gail_list_is_child_selected;
  iface->remove_selection = gail_list_remove_selection;
  /*
   * select_all_selection does not make sense for a combo box
   * so no implementation is provided.
   */
}

static gboolean
gail_list_add_selection (AtkSelection   *selection,
                         gint           i)
{
  GtkList *list;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  list = GTK_LIST (widget);

  gtk_list_select_item (list, i);
  return TRUE;
}

static gboolean 
gail_list_clear_selection (AtkSelection *selection)
{
  GtkList *list;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  list = GTK_LIST (widget);

  gtk_list_unselect_all (list);
  return TRUE;
}

static AtkObject*
gail_list_ref_selection (AtkSelection *selection,
                         gint         i)
{
  GtkList *list;
  GList *g_list;
  GtkWidget *item;
  AtkObject *obj;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  list = GTK_LIST (widget);

  /*
   * A combo box can have only one selection.
   */
  if (i != 0)
    return NULL;

  g_list = list->selection;

  if (g_list == NULL)
    return NULL;

  item = GTK_WIDGET (g_list->data);

  obj = gtk_widget_get_accessible (item);
  g_object_ref (obj);
  return obj;
}

static gint
gail_list_get_selection_count (AtkSelection *selection)
{
  GtkList *list;
  GList *g_list;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return 0;

  list = GTK_LIST (widget);

  g_list = list->selection;

  return g_list_length (g_list);;
}

static gboolean
gail_list_is_child_selected (AtkSelection *selection,
                             gint         i)
{
  GtkList *list;
  GList *g_list;
  GtkWidget *item;
  gint j;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  list = GTK_LIST (widget);

  g_list = list->selection;

  if (g_list == NULL)
    return FALSE;

  item = GTK_WIDGET (g_list->data);

  j = g_list_index (list->children, item);

  return (j == i);
}

static gboolean
gail_list_remove_selection (AtkSelection   *selection,
                             gint           i)
{
  if (atk_selection_is_child_selected (selection, i))
    atk_selection_clear_selection (selection);

  return TRUE;
}
