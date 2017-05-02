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

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>
#include "gailcombo.h"

static void         gail_combo_class_init              (GailComboClass *klass);
static void         gail_combo_init                    (GailCombo      *combo);
static void         gail_combo_real_initialize         (AtkObject      *obj,
                                                        gpointer       data);

static void         gail_combo_selection_changed_gtk   (GtkWidget      *widget,
                                                        gpointer       data);

static gint         gail_combo_get_n_children          (AtkObject      *obj);
static AtkObject*   gail_combo_ref_child               (AtkObject      *obj,
                                                        gint           i);
static void         gail_combo_finalize                (GObject        *object);
static void         atk_action_interface_init          (AtkActionIface *iface);

static gboolean     gail_combo_do_action               (AtkAction      *action,
                                                        gint           i);
static gboolean     idle_do_action                     (gpointer       data);
static gint         gail_combo_get_n_actions           (AtkAction      *action)
;
static const gchar* gail_combo_get_description         (AtkAction      *action,
                                                        gint           i);
static const gchar* gail_combo_get_name                (AtkAction      *action,
                                                        gint           i);
static gboolean              gail_combo_set_description(AtkAction      *action,
                                                        gint           i,
                                                        const gchar    *desc);

static void         atk_selection_interface_init       (AtkSelectionIface *iface);
static gboolean     gail_combo_add_selection           (AtkSelection   *selection,
                                                        gint           i);
static gboolean     gail_combo_clear_selection         (AtkSelection   *selection);
static AtkObject*   gail_combo_ref_selection           (AtkSelection   *selection,
                                                        gint           i);
static gint         gail_combo_get_selection_count     (AtkSelection   *selection);
static gboolean     gail_combo_is_child_selected       (AtkSelection   *selection,
                                                        gint           i);
static gboolean     gail_combo_remove_selection        (AtkSelection   *selection,
                                                        gint           i);

static gint         _gail_combo_button_release         (gpointer       data);
static gint         _gail_combo_popup_release          (gpointer       data);


G_DEFINE_TYPE_WITH_CODE (GailCombo, gail_combo, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gail_combo_class_init (GailComboClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  gobject_class->finalize = gail_combo_finalize;

  class->get_n_children = gail_combo_get_n_children;
  class->ref_child = gail_combo_ref_child;
  class->initialize = gail_combo_real_initialize;
}

static void
gail_combo_init (GailCombo      *combo)
{
  combo->press_description = NULL;
  combo->old_selection = NULL;
  combo->deselect_idle_handler = 0;
  combo->select_idle_handler = 0;
}

static void
gail_combo_real_initialize (AtkObject *obj,
                            gpointer  data)
{
  GtkCombo *combo;
  GtkList *list;
  GList *slist; 
  GailCombo *gail_combo;

  ATK_OBJECT_CLASS (gail_combo_parent_class)->initialize (obj, data);

  combo = GTK_COMBO (data);

  list = GTK_LIST (combo->list);
  slist = list->selection;

  gail_combo = GAIL_COMBO (obj);
  if (slist && slist->data)
    {
      gail_combo->old_selection = slist->data;
    }
  g_signal_connect (combo->list,
                    "selection_changed",
                    G_CALLBACK (gail_combo_selection_changed_gtk),
                    data);
  atk_object_set_parent (gtk_widget_get_accessible (combo->entry), obj);
  atk_object_set_parent (gtk_widget_get_accessible (combo->popup), obj);

  obj->role = ATK_ROLE_COMBO_BOX;
}

static gboolean
notify_deselect (gpointer data)
{
  GailCombo *combo;

  combo = GAIL_COMBO (data);

  combo->old_selection = NULL;
  combo->deselect_idle_handler = 0;
  g_signal_emit_by_name (data, "selection_changed");

  return FALSE;
}

static gboolean
notify_select (gpointer data)
{
  GailCombo *combo;

  combo = GAIL_COMBO (data);

  combo->select_idle_handler = 0;
  g_signal_emit_by_name (data, "selection_changed");

  return FALSE;
}

static void
gail_combo_selection_changed_gtk (GtkWidget      *widget,
                                  gpointer       data)
{
  GtkCombo *combo;
  GtkList *list;
  GList *slist; 
  AtkObject *obj;
  GailCombo *gail_combo;

  combo = GTK_COMBO (data);
  list = GTK_LIST (combo->list);
  
  slist = list->selection;

  obj = gtk_widget_get_accessible (GTK_WIDGET (data));
  gail_combo = GAIL_COMBO (obj);
  if (slist && slist->data)
    {
      if (slist->data != gail_combo->old_selection)
        {
          gail_combo->old_selection = slist->data;
          if (gail_combo->select_idle_handler == 0)
            gail_combo->select_idle_handler = gdk_threads_add_idle (notify_select, gail_combo);
        }
      if (gail_combo->deselect_idle_handler)
        {
          g_source_remove (gail_combo->deselect_idle_handler);
          gail_combo->deselect_idle_handler = 0;       
        }
    }
  else
    {
      if (gail_combo->deselect_idle_handler == 0)
        gail_combo->deselect_idle_handler = gdk_threads_add_idle (notify_deselect, gail_combo);
      if (gail_combo->select_idle_handler)
        {
          g_source_remove (gail_combo->select_idle_handler);
          gail_combo->select_idle_handler = 0;       
        }
    }
}

/*
 * The children of a GailCombo are the list of items and the entry field
 */
static gint
gail_combo_get_n_children (AtkObject* obj)
{
  gint n_children = 2;
  GtkWidget *widget;

  g_return_val_if_fail (GAIL_IS_COMBO (obj), 0);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return 0;

  return n_children;
}

static AtkObject*
gail_combo_ref_child (AtkObject *obj,
                      gint      i)
{
  AtkObject *accessible;
  GtkWidget *widget;

  g_return_val_if_fail (GAIL_IS_COMBO (obj), NULL);

  if (i < 0 || i > 1)
    return NULL;

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  if (i == 0)
    accessible = gtk_widget_get_accessible (GTK_COMBO (widget)->popup);
  else 
    accessible = gtk_widget_get_accessible (GTK_COMBO (widget)->entry);

  g_object_ref (accessible);
  return accessible;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gail_combo_do_action;
  iface->get_n_actions = gail_combo_get_n_actions;
  iface->get_description = gail_combo_get_description;
  iface->get_name = gail_combo_get_name;
  iface->set_description = gail_combo_set_description;
}

static gboolean
gail_combo_do_action (AtkAction *action,
                       gint      i)
{
  GailCombo *combo;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (action)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  combo = GAIL_COMBO (action);
  if (i == 0)
    {
      if (combo->action_idle_handler)
        return FALSE;

      combo->action_idle_handler = gdk_threads_add_idle (idle_do_action, combo);
      return TRUE;
    }
  else
    return FALSE;
}

/*
 * This action is the pressing of the button on the combo box.
 * The behavior is different depending on whether the list is being
 * displayed or removed.
 *
 * A button press event is simulated on the appropriate widget and 
 * a button release event is simulated in an idle function.
 */
static gboolean
idle_do_action (gpointer data)
{
  GtkCombo *combo;
  GtkWidget *action_widget;
  GtkWidget *widget;
  GailCombo *gail_combo;
  gboolean do_popup;
  GdkEvent tmp_event;

  gail_combo = GAIL_COMBO (data);
  gail_combo->action_idle_handler = 0;
  widget = GTK_ACCESSIBLE (gail_combo)->widget;
  if (widget == NULL /* State is defunct */ ||
      !gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  combo = GTK_COMBO (widget);

  do_popup = !gtk_widget_get_mapped (combo->popwin);

  tmp_event.button.type = GDK_BUTTON_PRESS; 
  tmp_event.button.window = widget->window;
  tmp_event.button.button = 1; 
  tmp_event.button.send_event = TRUE;
  tmp_event.button.time = GDK_CURRENT_TIME;
  tmp_event.button.axes = NULL;

  if (do_popup)
    {
      /* Pop up list */
      action_widget = combo->button;

      gtk_widget_event (action_widget, &tmp_event);

      /* FIXME !*/
      g_idle_add (_gail_combo_button_release, combo);
    }
    else
    {
      /* Pop down list */
      tmp_event.button.window = combo->list->window;
      gdk_window_set_user_data (combo->list->window, combo->button);
      action_widget = combo->popwin;
    
      gtk_widget_event (action_widget, &tmp_event);
      /* FIXME !*/
      g_idle_add (_gail_combo_popup_release, combo);
    }

  return FALSE;
}

static gint
gail_combo_get_n_actions (AtkAction *action)
{
  /*
   * The default behavior of a combo box is to have one action -
   */
  return 1;
}

static const gchar*
gail_combo_get_description (AtkAction *action,
                           gint      i)
{
  if (i == 0)
    {
      GailCombo *combo;

      combo = GAIL_COMBO (action);
      return combo->press_description;
    }
  else
    return NULL;
}

static const gchar*
gail_combo_get_name (AtkAction *action,
                     gint      i)
{
  if (i == 0)
    return "press";
  else
    return NULL;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gail_combo_add_selection;
  iface->clear_selection = gail_combo_clear_selection;
  iface->ref_selection = gail_combo_ref_selection;
  iface->get_selection_count = gail_combo_get_selection_count;
  iface->is_child_selected = gail_combo_is_child_selected;
  iface->remove_selection = gail_combo_remove_selection;
  /*
   * select_all_selection does not make sense for a combo box
   * so no implementation is provided.
   */
}

static gboolean
gail_combo_add_selection (AtkSelection   *selection,
                          gint           i)
{
  GtkCombo *combo;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  combo = GTK_COMBO (widget);

  gtk_list_select_item (GTK_LIST (combo->list), i);
  return TRUE;
}

static gboolean 
gail_combo_clear_selection (AtkSelection   *selection)
{
  GtkCombo *combo;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  combo = GTK_COMBO (widget);

  gtk_list_unselect_all (GTK_LIST (combo->list));
  return TRUE;
}

static AtkObject*
gail_combo_ref_selection (AtkSelection   *selection,
                          gint           i)
{
  GtkCombo *combo;
  GList * list;
  GtkWidget *item;
  AtkObject *obj;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  combo = GTK_COMBO (widget);

  /*
   * A combo box can have only one selection.
   */
  if (i != 0)
    return NULL;

  list = GTK_LIST (combo->list)->selection;

  if (list == NULL)
    return NULL;

  item = GTK_WIDGET (list->data);

  obj = gtk_widget_get_accessible (item);
  g_object_ref (obj);
  return obj;
}

static gint
gail_combo_get_selection_count (AtkSelection   *selection)
{
  GtkCombo *combo;
  GList * list;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return 0;

  combo = GTK_COMBO (widget);

  /*
   * The number of children currently selected is either 1 or 0 so we
   * do not bother to count the elements of the selected list.
   */
  list = GTK_LIST (combo->list)->selection;

  if (list == NULL)
    return 0;
  else
    return 1;
}

static gboolean
gail_combo_is_child_selected (AtkSelection   *selection,
                              gint           i)
{
  GtkCombo *combo;
  GList * list;
  GtkWidget *item;
  gint j;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  combo = GTK_COMBO (widget);

  list = GTK_LIST (combo->list)->selection;

  if (list == NULL)
    return FALSE;

  item = GTK_WIDGET (list->data);

  j = g_list_index (GTK_LIST (combo->list)->children, item);

  return (j == i);
}

static gboolean
gail_combo_remove_selection (AtkSelection   *selection,
                             gint           i)
{
  if (atk_selection_is_child_selected (selection, i))
    atk_selection_clear_selection (selection);

  return TRUE;
}

static gint 
_gail_combo_popup_release (gpointer data)
{
  GtkCombo *combo;
  GtkWidget *action_widget;
  GdkEvent tmp_event;

  GDK_THREADS_ENTER ();

  combo = GTK_COMBO (data);
  if (combo->current_button == 0)
    {
      GDK_THREADS_LEAVE ();
      return FALSE;
    }

  tmp_event.button.type = GDK_BUTTON_RELEASE; 
  tmp_event.button.button = 1; 
  tmp_event.button.time = GDK_CURRENT_TIME;
  action_widget = combo->button;

  gtk_widget_event (action_widget, &tmp_event);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static gint 
_gail_combo_button_release (gpointer data)
{
  GtkCombo *combo;
  GtkWidget *action_widget;
  GdkEvent tmp_event;

  GDK_THREADS_ENTER ();

  combo = GTK_COMBO (data);
  if (combo->current_button == 0)
    {
      GDK_THREADS_LEAVE ();
      return FALSE;
    }

  tmp_event.button.type = GDK_BUTTON_RELEASE; 
  tmp_event.button.button = 1; 
  tmp_event.button.window = combo->list->window;
  tmp_event.button.time = GDK_CURRENT_TIME;
  gdk_window_set_user_data (combo->list->window, combo->button);
  action_widget = combo->list;

  gtk_widget_event (action_widget, &tmp_event);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static gboolean
gail_combo_set_description (AtkAction      *action,
                            gint           i,
                            const gchar    *desc)
{
  if (i == 0)
    {
      GailCombo *combo;

      combo = GAIL_COMBO (action);
      g_free (combo->press_description);
      combo->press_description = g_strdup (desc);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gail_combo_finalize (GObject            *object)
{
  GailCombo *combo = GAIL_COMBO (object);

  g_free (combo->press_description);
  if (combo->action_idle_handler)
    {
      g_source_remove (combo->action_idle_handler);
      combo->action_idle_handler = 0;
    }
  if (combo->deselect_idle_handler)
    {
      g_source_remove (combo->deselect_idle_handler);
      combo->deselect_idle_handler = 0;       
    }
  if (combo->select_idle_handler)
    {
      g_source_remove (combo->select_idle_handler);
      combo->select_idle_handler = 0;       
    }
  G_OBJECT_CLASS (gail_combo_parent_class)->finalize (object);
}
