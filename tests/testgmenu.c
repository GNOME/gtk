/* testgmenu.c
 * Copyright (C) 2011  Red Hat, Inc.
 * Written by Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

/* TODO
 *
 * - Labeled sections
 *
 * - Focus changes. Verify that stopping subscriptions works.
 *
 * - Other attributes. What about icons ?
 */

/* MenuHolder {{{1 */

typedef struct {
  GMenuModel   *model;
  GActionGroup *group;
  GtkWidget    *menu;
  gboolean      items_changed;
} MenuHolder;

/* Menumodel callbacks {{{2 */

static void
connect_to_items_changed (GMenuModel *model,
                          GCallback   callback,
                          gpointer    data)
{
  gint i;
  GMenuModel *m;
  GMenuLinkIter *iter;

  if (!g_object_get_data (G_OBJECT (model), "handler-connected"))
    {
      g_signal_connect (model, "items-changed", callback, data);
      g_object_set_data (G_OBJECT (model), "handler-connected", GINT_TO_POINTER (1));
    }
  for (i = 0; i < g_menu_model_get_n_items (model); i++)
    {
      iter = g_menu_model_iterate_item_links (model, i);
      while (g_menu_link_iter_next (iter))
        {
          m = g_menu_link_iter_get_value (iter);
          connect_to_items_changed (m, callback, data);
          g_object_unref (m);
        }
      g_object_unref (iter);
    }
}

static void
items_changed (GMenuModel *model,
               gint        position,
               gint        removed,
               gint        added,
               MenuHolder *holder)
{
  g_print ("Received GMenuModel::items-changed\n");
  holder->items_changed = TRUE;
  connect_to_items_changed (model, G_CALLBACK (items_changed), holder);
}

 /* Actiongroup callbacks {{{2 */

typedef struct {
  GActionGroup *group;
  gchar        *name;
  gchar        *target;
  gulong        enabled_changed_id;
  gulong        state_changed_id;
  gulong        activate_handler;
} ActionData;

static void
action_data_free (gpointer data)
{
  ActionData *a = data;

  if (a->enabled_changed_id)
    g_signal_handler_disconnect (a->group, a->enabled_changed_id);

  if (a->state_changed_id)
    g_signal_handler_disconnect (a->group, a->state_changed_id);

  g_object_unref (a->group);
  g_free (a->name);
  g_free (a->target);

  g_free (a);
}

static void
enabled_changed (GActionGroup *group,
                 const gchar  *action_name,
                 gboolean      enabled,
                 GtkWidget    *widget)
{
  gtk_widget_set_sensitive (widget, enabled);
}

static void
toggle_state_changed (GActionGroup     *group,
                      const gchar      *name,
                      GVariant         *state,
                      GtkCheckMenuItem *w)
{
  ActionData *a;

  a = g_object_get_data (G_OBJECT (w), "action");
  g_signal_handler_block (w, a->activate_handler);
  gtk_check_menu_item_set_active (w, g_variant_get_boolean (state));
  g_signal_handler_unblock (w, a->activate_handler);
}

static void
radio_state_changed (GActionGroup     *group,
                     const gchar      *name,
                     GVariant         *state,
                     GtkCheckMenuItem *w)
{
  ActionData *a;
  gboolean b;

  a = g_object_get_data (G_OBJECT (w), "action");
  g_signal_handler_block (w, a->activate_handler);
  b = g_strcmp0 (a->target, g_variant_get_string (state, NULL)) == 0;
  gtk_check_menu_item_set_active (w, b);
  g_signal_handler_unblock (w, a->activate_handler);
}

/* Menuitem callbacks {{{2 */

static void
item_activated (GtkWidget *w,
                gpointer   data)
{
  ActionData *a;
  GVariant *parameter;

  a = g_object_get_data (G_OBJECT (w), "action");
  if (a->target)
    parameter = g_variant_new_string (a->target);
  else
    parameter = NULL;
  g_action_group_activate_action (a->group, a->name, parameter);
}

/* GtkMenu construction {{{2 */

static GtkWidget *
create_menuitem_from_model (GMenuModel   *model,
                            gint          item,
                            GActionGroup *group)
{
  GtkWidget *w;
  gchar *label;
  gchar *action;
  gchar *target;
  gchar *s;
  ActionData *a;
  const GVariantType *type;
  GVariant *v;

  label = NULL;
  g_menu_model_get_item_attribute (model, item, G_MENU_ATTRIBUTE_LABEL, "s", &label);

  action = NULL;
  g_menu_model_get_item_attribute (model, item, G_MENU_ATTRIBUTE_ACTION, "s", &action);

  if (action != NULL)
    type = g_action_group_get_action_state_type (group, action);
  else
    type = NULL;

  if (type == NULL)
    w = gtk_menu_item_new_with_mnemonic (label);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    w = gtk_check_menu_item_new_with_label (label);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      w = gtk_check_menu_item_new_with_label (label);
      gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (w), TRUE);
    }
  else
    g_assert_not_reached ();

  if (action != NULL)
    {
      a = g_new0 (ActionData, 1);
      a->group = g_object_ref (group);
      a->name = g_strdup (action);
      g_object_set_data_full (G_OBJECT (w), "action", a, action_data_free);

      if (!g_action_group_get_action_enabled (group, action))
        gtk_widget_set_sensitive (w, FALSE);

      s = g_strconcat ("action-enabled-changed::", action, NULL);
      a->enabled_changed_id = g_signal_connect (group, s,
                                                G_CALLBACK (enabled_changed), w);
      g_free (s);

      a->activate_handler = g_signal_connect (w, "activate", G_CALLBACK (item_activated), NULL);

      if (type == NULL)
        {
          /* all set */
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
        {
          s = g_strconcat ("action-state-changed::", action, NULL);
          a->state_changed_id = g_signal_connect (group, s,
                                                  G_CALLBACK (toggle_state_changed), w);
          g_free (s);
          v = g_action_group_get_action_state (group, action);
          gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w),
                                          g_variant_get_boolean (v));
          g_variant_unref (v);
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
        {
          s = g_strconcat ("action-state-changed::", action, NULL);
          a->state_changed_id = g_signal_connect (group, s,
                                                  G_CALLBACK (radio_state_changed), w);
          g_free (s);
          g_menu_model_get_item_attribute (model, item, G_MENU_ATTRIBUTE_TARGET, "s", &target);
          a->target = g_strdup (target);
          v = g_action_group_get_action_state (group, action);
          gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w),
                                          g_strcmp0 (g_variant_get_string (v, NULL), target) == 0);
          g_variant_unref (v);
          g_free (target);
        }
      else
        g_assert_not_reached ();
    }

  g_free (label);
  g_free (action);

  return w;
}

static GtkWidget *create_menu_from_model (GMenuModel   *model,
                                          GActionGroup *group);

static void
append_items_from_model (GtkWidget    *menu,
                         GMenuModel   *model,
                         GActionGroup *group,
                         gboolean     *need_separator,
                         const gchar  *heading)
{
  gint n;
  gint i;
  GtkWidget *w;
  GtkWidget *menuitem;
  GtkWidget *submenu;
  GMenuModel *m;
  gchar *label;

  n = g_menu_model_get_n_items (model);

  if (*need_separator && n > 0)
    {
      w = gtk_separator_menu_item_new ();
      gtk_widget_show (w);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
      *need_separator = FALSE;
    }

  if (heading != NULL)
    {
      w = gtk_menu_item_new_with_label (heading);
      gtk_widget_show (w);
      gtk_widget_set_sensitive (w, FALSE);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
    }

  for (i = 0; i < n; i++)
    {
      if ((m = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION)))
        {
          label = NULL;
          g_menu_model_get_item_attribute (model, i, G_MENU_ATTRIBUTE_LABEL, "s", &label);
          append_items_from_model (menu, m, group, need_separator, label);
          g_object_unref (m);
          g_free (label);

          if (*need_separator)
            {
              w = gtk_separator_menu_item_new ();
              gtk_widget_show (w);
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
              *need_separator = FALSE;
            }

          continue;
        }

      menuitem = create_menuitem_from_model (model, i, group);

      if ((m = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU)))
        {
          submenu = create_menu_from_model (m, group);
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
          g_object_unref (m);
        }

      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      *need_separator = TRUE;
    }
}

static GtkWidget *
create_menu_from_model (GMenuModel   *model,
                        GActionGroup *group)
{
  GtkWidget *w;
  gboolean need_separator;

  w = gtk_menu_new ();
  need_separator = FALSE;
  append_items_from_model (w, model, group, &need_separator, NULL);

  return w;
}

/* }}}2 */

MenuHolder *
menu_holder_new (GMenuModel   *model,
                 GActionGroup *group)
{
  MenuHolder *holder;

  holder = g_new (MenuHolder, 1);
  holder->model = g_object_ref (model);
  holder->group = g_object_ref (group);
  holder->menu = create_menu_from_model (model, group);
  holder->items_changed = FALSE;

  connect_to_items_changed (model, G_CALLBACK (items_changed), holder);

  return holder;
}

GtkWidget *
menu_holder_get_menu (MenuHolder *holder)
{
  if (holder->items_changed)
    {
      holder->items_changed = FALSE;
      gtk_widget_destroy (holder->menu);
      holder->menu = create_menu_from_model (holder->model, holder->group);
    }

  return holder->menu;
}

/* The example menu {{{1 */

static const gchar menu_markup[] =
  "<interface>\n"
  "<menu id='edit-menu'>\n"
  "  <section>\n"
  "    <item>\n"
  "      <attribute name='action'>undo</attribute>\n"
  "      <attribute name='label' translatable='yes' context='Stock label'>_Undo</attribute>\n"
  "    </item>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Redo</attribute>\n"
  "      <attribute name='action'>redo</attribute>\n"
  "    </item>\n"
  "  </section>\n"
  "  <section/>\n"
  "  <section>\n"
  "    <attribute name='label' translatable='yes'>Copy &amp; Paste</attribute>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Cut</attribute>\n"
  "      <attribute name='action'>cut</attribute>\n"
  "    </item>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Copy</attribute>\n"
  "      <attribute name='action'>copy</attribute>\n"
  "    </item>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Paste</attribute>\n"
  "      <attribute name='action'>paste</attribute>\n"
  "    </item>\n"
  "  </section>\n"
  "  <section>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Bold</attribute>\n"
  "      <attribute name='action'>bold</attribute>\n"
  "    </item>\n"
  "    <submenu>\n"
  "      <attribute name='label' translatable='yes'>Language</attribute>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>Latin</attribute>\n"
  "        <attribute name='action'>lang</attribute>\n"
  "        <attribute name='target'>latin</attribute>\n"
  "      </item>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>Greek</attribute>\n"
  "        <attribute name='action'>lang</attribute>\n"
  "        <attribute name='target'>greek</attribute>\n"
  "      </item>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>Urdu</attribute>\n"
  "        <attribute name='action'>lang</attribute>\n"
  "        <attribute name='target'>urdu</attribute>\n"
  "      </item>\n"
  "    </submenu>\n"
  "  </section>\n"
  "</menu>\n"
  "</interface>\n";

static GMenuModel *
get_model (void)
{
  GError *error = NULL;
  GtkBuilder *builder;
  GMenuModel *menu;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, menu_markup, -1, &error);
  g_assert_no_error (error);

  menu = g_object_ref (gtk_builder_get_object (builder, "edit-menu"));
  g_object_unref (builder);

  return menu;
}

/* The example actions {{{1 */

static void
activate_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  g_print ("Action %s activated\n", g_action_get_name (G_ACTION (action)));
}

static void
activate_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GVariant *old_state, *new_state;

  old_state = g_action_get_state (G_ACTION (action));
  new_state = g_variant_new_boolean (!g_variant_get_boolean (old_state));

  g_print ("Toggle action %s activated, state changes from %d to %d\n",
           g_action_get_name (G_ACTION (action)),
           g_variant_get_boolean (old_state),
           g_variant_get_boolean (new_state));

  g_simple_action_set_state (action, new_state);
  g_variant_unref (old_state);
}

static void
activate_radio (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GVariant *old_state, *new_state;

  old_state = g_action_get_state (G_ACTION (action));
  new_state = g_variant_new_string (g_variant_get_string (parameter, NULL));

  g_print ("Radio action %s activated, state changes from %s to %s\n",
           g_action_get_name (G_ACTION (action)),
           g_variant_get_string (old_state, NULL),
           g_variant_get_string (new_state, NULL));

  g_simple_action_set_state (action, new_state);
  g_variant_unref (old_state);
}

static GActionEntry actions[] = {
  { "undo",  activate_action, NULL, NULL,      NULL },
  { "redo",  activate_action, NULL, NULL,      NULL },
  { "cut",   activate_action, NULL, NULL,      NULL },
  { "copy",  activate_action, NULL, NULL,      NULL },
  { "paste", activate_action, NULL, NULL,      NULL },
  { "bold",  activate_toggle, NULL, "true",    NULL },
  { "lang",  activate_radio,  "s",  "'latin'", NULL },
};

static GActionGroup *
get_group (void)
{
  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), NULL);

  return G_ACTION_GROUP (group);
}
 
/* The action treeview {{{1 */

static void
enabled_cell_func (GtkTreeViewColumn *column,
                   GtkCellRenderer   *cell,
                   GtkTreeModel      *model,
                   GtkTreeIter       *iter,
                   gpointer           data)
{
  GActionGroup *group = data;
  gchar *name;
  gboolean enabled;

  gtk_tree_model_get (model, iter, 0, &name, -1);
  enabled = g_action_group_get_action_enabled (group, name);
  g_free (name);

  gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell), enabled);
}

static void
state_cell_func (GtkTreeViewColumn *column,
                 GtkCellRenderer   *cell,
                 GtkTreeModel      *model,
                 GtkTreeIter       *iter,
                 gpointer           data)
{
  GActionGroup *group = data;
  gchar *name;
  GVariant *state;

  gtk_tree_model_get (model, iter, 0, &name, -1);
  state = g_action_group_get_action_state (group, name);
  g_free (name);

  gtk_cell_renderer_set_visible (cell, FALSE);
  g_object_set (cell, "mode", GTK_CELL_RENDERER_MODE_INERT, NULL);

  if (state == NULL)
    return;

  if (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN) &&
      GTK_IS_CELL_RENDERER_TOGGLE (cell))
    {
      gtk_cell_renderer_set_visible (cell, TRUE);
      g_object_set (cell, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
      gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell),
                                           g_variant_get_boolean (state));
    }
  else if (g_variant_is_of_type (state, G_VARIANT_TYPE_STRING) &&
           GTK_IS_CELL_RENDERER_COMBO (cell))
    {
      gtk_cell_renderer_set_visible (cell, TRUE);
      g_object_set (cell, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
      g_object_set (cell, "text", g_variant_get_string (state, NULL), NULL);
    }

  g_variant_unref (state);
}

static void
enabled_cell_toggled (GtkCellRendererToggle *cell,
                      const gchar           *path_str,
                      GtkTreeModel          *model)
{
  GActionGroup *group;
  GAction *action;
  gchar *name;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean enabled;

  group = g_object_get_data (G_OBJECT (model), "group");
  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &name, -1);

  enabled = g_action_group_get_action_enabled (group, name);
  action = g_action_map_lookup_action (G_ACTION_MAP (group), name);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !enabled);

  gtk_tree_model_row_changed (model, path, &iter);

  g_free (name);
  gtk_tree_path_free (path);
}

static void
state_cell_toggled (GtkCellRendererToggle *cell,
                    const gchar           *path_str,
                    GtkTreeModel          *model)
{
  GActionGroup *group;
  GAction *action;
  gchar *name;
  GtkTreePath *path;
  GtkTreeIter iter;
  GVariant *state;

  group = g_object_get_data (G_OBJECT (model), "group");
  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &name, -1);

  state = g_action_group_get_action_state (group, name);
  action = g_action_map_lookup_action (G_ACTION_MAP (group), name);
  if (state && g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    {
      gboolean b;

      b = g_variant_get_boolean (state);
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (!b));
    }
  else
    {
      /* nothing to do */
    }

  gtk_tree_model_row_changed (model, path, &iter);

  g_free (name);
  gtk_tree_path_free (path);
  if (state)
    g_variant_unref (state);
}

static void
state_cell_edited (GtkCellRendererCombo  *cell,
                   const gchar           *path_str,
                   const gchar           *new_text,
                   GtkTreeModel          *model)
{
  GActionGroup *group;
  GAction *action;
  gchar *name;
  GtkTreePath *path;
  GtkTreeIter iter;

  group = g_object_get_data (G_OBJECT (model), "group");
  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &name, -1);
  action = g_action_map_lookup_action (G_ACTION_MAP (group), name);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (new_text));

  gtk_tree_model_row_changed (model, path, &iter);

  g_free (name);
  gtk_tree_path_free (path);
}

static GtkWidget *
create_action_treeview (GActionGroup *group)
{
  GtkWidget *tv;
  GtkListStore *store;
  GtkListStore *values;
  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  gchar **actions;
  gint i;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  actions = g_action_group_list_actions (group);
  for (i = 0; actions[i]; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, actions[i], -1);
    }
  g_strfreev (actions);
  g_object_set_data (G_OBJECT (store), "group", group);

  tv = gtk_tree_view_new ();

  g_signal_connect_swapped (group, "action-enabled-changed",
                            G_CALLBACK (gtk_widget_queue_draw), tv);
  g_signal_connect_swapped (group, "action-state-changed",
                            G_CALLBACK (gtk_widget_queue_draw), tv);

  gtk_tree_view_set_model (GTK_TREE_VIEW (tv), GTK_TREE_MODEL (store));

  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Action", cell,
                                                     "text", 0,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Enabled");
  cell = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, cell, enabled_cell_func, group, NULL);
  g_signal_connect (cell, "toggled", G_CALLBACK (enabled_cell_toggled), store);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "State");
  cell = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, cell, state_cell_func, group, NULL);
  g_signal_connect (cell, "toggled", G_CALLBACK (state_cell_toggled), store);
  cell = gtk_cell_renderer_combo_new ();
  values = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_list_store_append (values, &iter);
  gtk_list_store_set (values, &iter, 0, "latin", -1);
  gtk_list_store_append (values, &iter);
  gtk_list_store_set (values, &iter, 0, "greek", -1);
  gtk_list_store_append (values, &iter);
  gtk_list_store_set (values, &iter, 0, "urdu", -1);
  gtk_list_store_append (values, &iter);
  gtk_list_store_set (values, &iter, 0, "sumerian", -1);
  g_object_set (cell,
                "has-entry", FALSE,
                "model", values,
                "text-column", 0,
                "editable", TRUE,
                NULL);
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, cell, state_cell_func, group, NULL);
  g_signal_connect (cell, "edited", G_CALLBACK (state_cell_edited), store);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

  return tv;
}

/* Dynamic menu changes {{{1 */

static void
toggle_sumerian (GtkToggleButton *button, gpointer data)
{
  GMenuModel *model;
  gboolean adding;
  GMenuModel *m;

  model = g_object_get_data (G_OBJECT (button), "model");

  adding = gtk_toggle_button_get_active (button);

  m = g_menu_model_get_item_link (model, g_menu_model_get_n_items (model) - 1, G_MENU_LINK_SECTION);
  m = g_menu_model_get_item_link (m, g_menu_model_get_n_items (m) - 1, G_MENU_LINK_SUBMENU);
  if (adding)
    g_menu_append (G_MENU (m), "Sumerian", "lang::sumerian");
  else
    g_menu_remove (G_MENU (m), g_menu_model_get_n_items (m) - 1);
}

static void
action_list_add (GtkTreeModel *store,
                 const gchar  *action)
{
  GtkTreeIter iter;

  gtk_list_store_append (GTK_LIST_STORE (store), &iter);
  gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, action, -1);
}

static void
action_list_remove (GtkTreeModel *store,
                    const gchar  *action)
{
  GtkTreeIter iter;
  gchar *text;

  gtk_tree_model_get_iter_first (store, &iter);
  do {
    gtk_tree_model_get (store, &iter, 0, &text, -1);
    if (g_strcmp0 (action, text) == 0)
      {
        g_free (text);
        gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
        break;
      }
    g_free (text);
  } while (gtk_tree_model_iter_next (store, &iter));
}

static void
toggle_italic (GtkToggleButton *button, gpointer data)
{
  GMenuModel *model;
  GActionGroup *group;
  GSimpleAction *action;
  gboolean adding;
  GMenuModel *m;
  GtkTreeView *tv = data;
  GtkTreeModel *store;

  model = g_object_get_data (G_OBJECT (button), "model");
  group = g_object_get_data (G_OBJECT (button), "group");

  store = gtk_tree_view_get_model (tv);

  adding = gtk_toggle_button_get_active (button);

  m = g_menu_model_get_item_link (model, g_menu_model_get_n_items (model) - 1, G_MENU_LINK_SECTION);
  if (adding)
    {
      action = g_simple_action_new_stateful ("italic", NULL, g_variant_new_boolean (FALSE));
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_signal_connect (action, "activate", G_CALLBACK (activate_toggle), NULL);
      g_object_unref (action);
      action_list_add (store, "italic");
      g_menu_insert (G_MENU (m), 1, "Italic", "italic");
    }
  else
    {
      g_action_map_remove_action (G_ACTION_MAP (group), "italic");
      action_list_remove (store, "italic");
      g_menu_remove (G_MENU (m), 1);
    }
}

static void
toggle_speed (GtkToggleButton *button, gpointer data)
{
  GMenuModel *model;
  GActionGroup *group;
  GSimpleAction *action;
  gboolean adding;
  GMenuModel *m;
  GMenu *submenu;
  GtkTreeView *tv = data;
  GtkTreeModel *store;

  model = g_object_get_data (G_OBJECT (button), "model");
  group = g_object_get_data (G_OBJECT (button), "group");

  store = gtk_tree_view_get_model (tv);

  adding = gtk_toggle_button_get_active (button);

  m = g_menu_model_get_item_link (model, 1, G_MENU_LINK_SECTION);
  if (adding)
    {
      action = g_simple_action_new ("faster", NULL);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_signal_connect (action, "activate", G_CALLBACK (activate_action), NULL);
      g_object_unref (action);

      action = g_simple_action_new ("slower", NULL);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_signal_connect (action, "activate", G_CALLBACK (activate_action), NULL);
      g_object_unref (action);

      action_list_add (store, "faster");
      action_list_add (store, "slower");

      submenu = g_menu_new ();
      g_menu_append (submenu, "Faster", "faster");
      g_menu_append (submenu, "Slower", "slower");
      g_menu_append_submenu (G_MENU (m), "Speed", G_MENU_MODEL (submenu));
    }
  else
    {
      g_action_map_remove_action (G_ACTION_MAP (group), "faster");
      g_action_map_remove_action (G_ACTION_MAP (group), "slower");

      action_list_remove (store, "faster");
      action_list_remove (store, "slower");

      g_menu_remove (G_MENU (m), g_menu_model_get_n_items (m) - 1);
    }
}
static GtkWidget *
create_add_remove_buttons (GActionGroup *group,
                           GMenuModel   *model,
                           GtkWidget    *treeview)
{
  GtkWidget *box;
  GtkWidget *button;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  button = gtk_check_button_new_with_label ("Add Italic");
  gtk_container_add (GTK_CONTAINER (box), button);

  g_object_set_data  (G_OBJECT (button), "group", group);
  g_object_set_data  (G_OBJECT (button), "model", model);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (toggle_italic), treeview);

  button = gtk_check_button_new_with_label ("Add Sumerian");
  gtk_container_add (GTK_CONTAINER (box), button);

  g_object_set_data  (G_OBJECT (button), "group", group);
  g_object_set_data  (G_OBJECT (button), "model", model);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (toggle_sumerian), NULL);

  button = gtk_check_button_new_with_label ("Add Speed");
  gtk_container_add (GTK_CONTAINER (box), button);

  g_object_set_data  (G_OBJECT (button), "group", group);
  g_object_set_data  (G_OBJECT (button), "model", model);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (toggle_speed), treeview);
  return box;
}

/* main {{{1 */

static void
button_clicked (GtkButton  *button,
                MenuHolder *holder)
{
  GtkWidget *menu;

  menu = menu_holder_get_menu (holder);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, 0);
}

#define BUS_NAME "org.gtk.TestMenus"
#define OBJ_PATH "/org/gtk/TestMenus"

static gboolean
on_delete_event (GtkWidget   *widget,
		 GdkEvent    *event,
		 gpointer     user_data)
{
  gtk_main_quit ();
  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *tv;
  GtkWidget *buttons;
  MenuHolder *holder;
  GMenuModel *model;
  GActionGroup *group;
  GDBusConnection *bus;
  GError *error = NULL;
  gboolean do_export = FALSE;
  gboolean do_import = FALSE;
  GOptionEntry entries[] = {
    { "export", 0, 0, G_OPTION_ARG_NONE, &do_export, "Export actions and menus over D-Bus", NULL },
    { "import", 0, 0, G_OPTION_ARG_NONE, &do_import, "Use exported actions and menus", NULL },
    { NULL, }
  };

  gtk_init_with_args (&argc, &argv, NULL, entries, NULL, NULL);

  if (do_export && do_import)
    {
       g_error ("can't have it both ways\n");
       exit (1);
    }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "delete-event", G_CALLBACK(on_delete_event), NULL);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), box);

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  if (do_import)
    {
      g_print ("Getting menus from the bus...\n");
      model = (GMenuModel*)g_dbus_menu_model_get (bus, BUS_NAME, OBJ_PATH);
      g_print ("Getting actions from the bus...\n");
      group = (GActionGroup*)g_dbus_action_group_get (bus, BUS_NAME, OBJ_PATH);
    }
  else
    {
      group = get_group ();
      model = get_model ();

      tv = create_action_treeview (group);
      gtk_container_add (GTK_CONTAINER (box), tv);
      buttons = create_add_remove_buttons (group, model, tv);
      gtk_container_add (GTK_CONTAINER (box), buttons);
    }

  if (do_export)
    {
      g_print ("Exporting menus on the bus...\n");
      if (!g_dbus_connection_export_menu_model (bus, OBJ_PATH, model, &error))
        {
          g_warning ("Menu export failed: %s", error->message);
          exit (1);
        }
      g_print ("Exporting actions on the bus...\n");
      if (!g_dbus_connection_export_action_group (bus, OBJ_PATH, group, &error))
        {
          g_warning ("Action export failed: %s", error->message);
          exit (1);
        }
      g_bus_own_name_on_connection (bus, BUS_NAME, 0, NULL, NULL, NULL, NULL);
    }
  else
    {
      holder = menu_holder_new (model, group);
      button = gtk_button_new_with_label ("Click here");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (button_clicked), holder);
      gtk_container_add (GTK_CONTAINER (box), button);
    }

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
