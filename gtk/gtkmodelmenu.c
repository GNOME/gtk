/*
 * Copyright © 2011 Red Hat, Inc.
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 *         Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkmodelmenu.h"

#include "gtkmenu.h"
#include "gtkmenubar.h"
#include "gtkseparatormenuitem.h"
#include "gtkmodelmenuitem.h"
#include "gtkapplicationprivate.h"

typedef struct {
  GActionObservable *actions;
  GMenuModel        *model;
  GtkAccelGroup     *accels;
  GtkMenuShell      *shell;
  guint              update_idle;
  GSList            *connected;
  gboolean           with_separators;
  gint               n_items;
} GtkModelMenuBinding;

static void
gtk_model_menu_binding_items_changed (GMenuModel *model,
                                      gint        position,
                                      gint        removed,
                                      gint        added,
                                      gpointer    user_data);
static void gtk_model_menu_binding_append_model (GtkModelMenuBinding *binding,
                                                 GMenuModel *model,
                                                 gboolean with_separators);

static void
gtk_model_menu_binding_free (gpointer data)
{
  GtkModelMenuBinding *binding = data;

  /* disconnect all existing signal handlers */
  while (binding->connected)
    {
      g_signal_handlers_disconnect_by_func (binding->connected->data, gtk_model_menu_binding_items_changed, binding);
      g_object_unref (binding->connected->data);

      binding->connected = g_slist_delete_link (binding->connected, binding->connected);
    }

  if (binding->actions)
    g_object_unref (binding->actions);
  g_object_unref (binding->model);

  g_slice_free (GtkModelMenuBinding, binding);
}

static void
gtk_model_menu_binding_append_item (GtkModelMenuBinding  *binding,
                                    GMenuModel           *model,
                                    gint                  item_index,
                                    gchar               **heading)
{
  GMenuModel *section;

  if ((section = g_menu_model_get_item_link (model, item_index, "section")))
    {
      g_menu_model_get_item_attribute (model, item_index, "label", "s", heading);
      gtk_model_menu_binding_append_model (binding, section, FALSE);
    }
  else
    {
      GtkMenuItem *item;

      item = gtk_model_menu_item_new (model, item_index, binding->actions, binding->accels);
      gtk_menu_shell_append (binding->shell, GTK_WIDGET (item));
      gtk_widget_show (GTK_WIDGET (item));
      binding->n_items++;
    }
}

static void
gtk_model_menu_binding_append_model (GtkModelMenuBinding *binding,
                                     GMenuModel          *model,
                                     gboolean             with_separators)
{
  gint n, i;

  g_signal_connect (model, "items-changed", G_CALLBACK (gtk_model_menu_binding_items_changed), binding);
  binding->connected = g_slist_prepend (binding->connected, g_object_ref (model));

  /* Deciding if we should show a separator is a bit difficult.  There
   * are two types of separators:
   *
   *  - section headings (when sections have 'label' property)
   *
   *  - normal separators automatically put between sections
   *
   * The easiest way to think about it is that a section usually has a
   * separator (or heading) immediately before it.
   *
   * There are three exceptions to this general rule:
   *
   *  - empty sections don't get separators or headings
   *
   *  - sections only get separators and headings at the toplevel of a
   *    menu (ie: no separators on nested sections or in menubars)
   *
   *  - the first section in the menu doesn't get a normal separator,
   *    but it can get a header (if it's not empty)
   *
   * Unfortunately, we cannot simply check the size of the section in
   * order to determine if we should place a header: the section may
   * contain other sections that are themselves empty.  Instead, we need
   * to append the section, and check if we ended up with any actual
   * content.  If we did, then we need to insert before that content.
   * We use 'our_position' to keep track of this.
   */

  n = g_menu_model_get_n_items (model);

  for (i = 0; i < n; i++)
    {
      gint our_position = binding->n_items;
      gchar *heading = NULL;

      gtk_model_menu_binding_append_item (binding, model, i, &heading);

      if (with_separators && our_position < binding->n_items)
        {
          GtkWidget *separator = NULL;

          if (heading)
            {
              separator = gtk_menu_item_new_with_label (heading);
              gtk_widget_set_sensitive (separator, FALSE);
            }
          else if (our_position > 0)
            separator = gtk_separator_menu_item_new ();

          if (separator)
            {
              gtk_menu_shell_insert (binding->shell, separator, our_position);
              gtk_widget_show (separator);
              binding->n_items++;
            }
        }

      g_free (heading);
    }
}

static void
gtk_model_menu_binding_populate (GtkModelMenuBinding *binding)
{
  GList *children;

  /* remove current children */
  children = gtk_container_get_children (GTK_CONTAINER (binding->shell));
  while (children)
    {
      gtk_container_remove (GTK_CONTAINER (binding->shell), children->data);
      children = g_list_delete_link (children, children);
    }

  binding->n_items = 0;

  /* add new items from the model */
  gtk_model_menu_binding_append_model (binding, binding->model, binding->with_separators);
}

static gboolean
gtk_model_menu_binding_handle_changes (gpointer user_data)
{
  GtkModelMenuBinding *binding = user_data;

  /* disconnect all existing signal handlers */
  while (binding->connected)
    {
      g_signal_handlers_disconnect_by_func (binding->connected->data, gtk_model_menu_binding_items_changed, binding);
      g_object_unref (binding->connected->data);

      binding->connected = g_slist_delete_link (binding->connected, binding->connected);
    }

  gtk_model_menu_binding_populate (binding);

  binding->update_idle = 0;

  g_object_unref (binding->shell);

  return G_SOURCE_REMOVE;
}

static void
gtk_model_menu_binding_items_changed (GMenuModel *model,
                                      gint        position,
                                      gint        removed,
                                      gint        added,
                                      gpointer    user_data)
{
  GtkModelMenuBinding *binding = user_data;

  if (binding->update_idle == 0)
    {
      binding->update_idle = gdk_threads_add_idle (gtk_model_menu_binding_handle_changes, user_data);
      g_object_ref (binding->shell);
    }
}

static void
gtk_model_menu_bind (GtkMenuShell      *shell,
                     GMenuModel        *model,
                     gboolean           with_separators)
{
  GtkModelMenuBinding *binding;

  binding = g_slice_new (GtkModelMenuBinding);
  binding->model = g_object_ref (model);
  binding->actions = NULL;
  binding->accels = NULL;
  binding->shell = shell;
  binding->update_idle = 0;
  binding->connected = NULL;
  binding->with_separators = with_separators;

  g_object_set_data_full (G_OBJECT (shell), "gtk-model-menu-binding", binding, gtk_model_menu_binding_free);
}


static void
gtk_model_menu_populate (GtkMenuShell      *shell,
                         GActionObservable *actions,
                         GtkAccelGroup     *accels)
{
  GtkModelMenuBinding *binding;

  binding = (GtkModelMenuBinding*) g_object_get_data (G_OBJECT (shell), "gtk-model-menu-binding");

  binding->actions = g_object_ref (actions);
  binding->accels = accels;

  gtk_model_menu_binding_populate (binding);
}

GtkWidget *
gtk_model_menu_create_menu (GMenuModel        *model,
                            GActionObservable *actions,
                            GtkAccelGroup     *accels)
{
  GtkWidget *menu;

  menu = gtk_menu_new ();

  gtk_model_menu_bind (GTK_MENU_SHELL (menu), model, TRUE);
  gtk_model_menu_populate (GTK_MENU_SHELL (menu), actions, accels);

  return menu;
}

static void
notify_attach (GtkMenu    *menu,
               GParamSpec *pspec,
               gpointer    data)
{
  GtkWidget *widget;
  GtkWidget *toplevel;
  GActionObservable *actions;
  GtkAccelGroup *accels;

  widget = gtk_menu_get_attach_widget (menu);
  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_IS_APPLICATION_WINDOW (toplevel))
    {
      actions = gtk_application_window_get_observable (GTK_APPLICATION_WINDOW (toplevel));
      accels = gtk_application_window_get_accel_group (GTK_APPLICATION_WINDOW (toplevel));

      gtk_model_menu_populate (GTK_MENU_SHELL (menu), actions, accels);
    }
}

/**
 * gtk_menu_new_from_model:
 * @model: a #GMenuModel
 *
 * Creates a #GtkMenu and populates it with menu items and
 * submenus according to @model.
 *
 * The created menu items are connected to actions found in the
 * #GtkApplicationWindow to which the menu belongs - typically
 * by means of being attached to a widget (see gtk_menu_attach_to_widget())
 * that is contained within the #GtkApplicationWindows widget hierarchy.
 *
 * Returns: a new #GtkMenu
 *
 * Since: 3.4
 */
GtkWidget *
gtk_menu_new_from_model (GMenuModel *model)
{
  GtkWidget *menu;

  menu = gtk_menu_new ();
  gtk_model_menu_bind (GTK_MENU_SHELL (menu), model, TRUE);
  g_signal_connect (menu, "notify::attach-widget",
                    G_CALLBACK (notify_attach), NULL);

  return menu;
}

GtkWidget *
gtk_model_menu_create_menu_bar (GMenuModel        *model,
                                GActionObservable *actions,
                                GtkAccelGroup     *accels)
{
  GtkWidget *menubar;

  menubar = gtk_menu_bar_new ();

  gtk_model_menu_bind (GTK_MENU_SHELL (menubar), model, FALSE);
  gtk_model_menu_populate (GTK_MENU_SHELL (menubar), actions, accels);

  return menubar;
}

static void
hierarchy_changed (GtkMenuShell *shell,
                   GObject      *previous_toplevel,
                   gpointer      data)
{
  GtkWidget *toplevel;
  GActionObservable *actions;
  GtkAccelGroup *accels;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  if (GTK_IS_APPLICATION_WINDOW (toplevel))
    {
      actions = gtk_application_window_get_observable (GTK_APPLICATION_WINDOW (toplevel));
      accels = gtk_application_window_get_accel_group (GTK_APPLICATION_WINDOW (toplevel));

      gtk_model_menu_populate (shell, actions, accels);
    }
}

/**
 * gtk_menu_bar_new_from_model:
 * @model: a #GMenuModel
 *
 * Creates a new #GtkMenuBar and populates it with menu items
 * and submenus according to @model.
 *
 * The created menu items are connected to actions found in the
 * #GtkApplicationWindow to which the menu bar belongs - typically
 * by means of being contained within the #GtkApplicationWindows
 * widget hierarchy.
 *
 * Returns: a new #GtkMenuBar
 *
 * Since: 3.4
 */
GtkWidget *
gtk_menu_bar_new_from_model (GMenuModel *model)
{
  GtkWidget *menubar;

  menubar = gtk_menu_bar_new ();

  gtk_model_menu_bind (GTK_MENU_SHELL (menubar), model, FALSE);

  g_signal_connect (menubar, "hierarchy-changed",
                    G_CALLBACK (hierarchy_changed), NULL);

  return menubar;
}
