/*
 * Copyright (c) 2014 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>

#include "actions.h"
#include "action-editor.h"
#include "action-holder.h"

#include "gtkapplication.h"
#include "gtkapplicationwindow.h"
#include "gtkactiontreeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkpopover.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtklistbox.h"
#include "gtksizegroup.h"
#include "gtkboxlayout.h"


struct _GtkInspectorActions
{
  GtkWidget parent;

  GtkWidget *swin;
  GtkWidget *list;
  GtkWidget *button;

  GObject *object;

  GPtrArray *subscriptions;

  GListStore *actions;
  GtkSortListModel *sorted;
  GtkColumnViewColumn *name;
};

typedef struct _GtkInspectorActionsClass
{
  GtkWidgetClass parent;
} GtkInspectorActionsClass;

enum {
  PROP_0,
  PROP_BUTTON,
  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (GtkInspectorActions, gtk_inspector_actions, GTK_TYPE_WIDGET)

static void
gtk_inspector_actions_init (GtkInspectorActions *sl)
{
 GtkBoxLayout *layout;

  gtk_widget_init_template (GTK_WIDGET (sl));

  layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (sl)));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_VERTICAL);
}

static void
action_added (GObject             *owner,
              const char          *action_name,
              GtkInspectorActions *sl)
{
  ActionHolder *holder = action_holder_new (owner, action_name);
  g_list_store_append (sl->actions, holder);
  g_object_unref (holder);
}

static void
setup_name_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_add_css_class (label, "cell");
  gtk_list_item_set_child (list_item, label);
}

static void
bind_name_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  gpointer item;
  GtkWidget *label;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  gtk_label_set_label (GTK_LABEL (label), action_holder_get_name (ACTION_HOLDER (item)));
}

static void
setup_enabled_cb (GtkSignalListItemFactory *factory,
                  GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.5);
  gtk_widget_add_css_class (label, "cell");
  gtk_list_item_set_child (list_item, label);
}

static void
update_enabled (ActionHolder *holder,
                GtkLabel     *label)
{
  GObject *owner = action_holder_get_owner (holder);
  const char *name = action_holder_get_name (holder);
  gboolean enabled = FALSE;

  if (GTK_IS_WIDGET (owner))
    {
      GtkActionKey *key = gtk_action_key_new (name);
      GtkActionResolution resolution = GTK_ACTION_RESOLUTION_INIT;
      GtkActionNode *node = _gtk_widget_get_action_node (GTK_WIDGET (owner), FALSE);

      if (node == NULL || key == NULL ||
          !gtk_action_resolution_init (&resolution, node, key) ||
          !gtk_action_resolution_query (&resolution, &enabled, NULL, NULL, NULL, NULL))
        enabled = FALSE;

      gtk_action_resolution_clear (&resolution);
      g_clear_pointer (&key, gtk_action_key_unref);
    }
  else if (G_IS_ACTION_GROUP (owner))
    enabled = g_action_group_get_action_enabled (G_ACTION_GROUP (owner), name);

  gtk_label_set_label (label, enabled ? "+" : "-");
}

static void
bind_enabled_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  gpointer item = gtk_list_item_get_item (list_item);
  GtkWidget *label = gtk_list_item_get_child (list_item);

  g_signal_connect (item, "changed", G_CALLBACK (update_enabled), label);

  update_enabled (ACTION_HOLDER (item), GTK_LABEL (label));
}

static void
unbind_enabled_cb (GtkSignalListItemFactory *factory,
                   GtkListItem              *list_item)
{
  gpointer item = gtk_list_item_get_item (list_item);
  GtkWidget *label = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (item, update_enabled, label);
}

static void
setup_parameter_cb (GtkSignalListItemFactory *factory,
                    GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.5);
  gtk_widget_add_css_class (label, "cell");
  gtk_list_item_set_child (list_item, label);
}

static void
bind_parameter_cb (GtkSignalListItemFactory *factory,
                   GtkListItem              *list_item)
{
  gpointer item;
  GtkWidget *label;
  GObject *owner;
  const char *name;
  char *parameter = NULL;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  owner = action_holder_get_owner (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));
  if (GTK_IS_WIDGET (owner))
    {
      GtkActionKey *key = gtk_action_key_new (name);
      GtkActionResolution resolution = GTK_ACTION_RESOLUTION_INIT;
      GtkActionNode *node = _gtk_widget_get_action_node (GTK_WIDGET (owner), FALSE);
      const GVariantType *parameter_type = NULL;

      if (node == NULL || key == NULL ||
          !gtk_action_resolution_init (&resolution, node, key) ||
          !gtk_action_resolution_query (&resolution, NULL, &parameter_type,
                                        NULL, NULL, NULL))
        parameter = g_strdup ("(Unknown)");
      else if (parameter_type != NULL)
        parameter = g_variant_type_dup_string (parameter_type);

      gtk_action_resolution_clear (&resolution);
      g_clear_pointer (&key, gtk_action_key_unref);
    }
  else if (G_IS_ACTION_GROUP (owner))
    {
      const GVariantType *parameter_type;

      parameter_type = g_action_group_get_action_parameter_type (G_ACTION_GROUP (owner), name);
      if (parameter_type != NULL)
        parameter = g_variant_type_dup_string (parameter_type);
    }
  else
    parameter = g_strdup ("(Unknown)");

  gtk_label_set_label (GTK_LABEL (label), parameter != NULL ? parameter : "");

  g_clear_pointer (&parameter, g_free);
}

static void
setup_state_cb (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_widget_set_margin_start (label, 5);
  gtk_widget_set_margin_end (label, 5);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_add_css_class (label, "cell");
  gtk_list_item_set_child (list_item, label);
}

static void
update_state (ActionHolder *h,
              GtkLabel     *label)
{
  GObject *owner = action_holder_get_owner (h);
  const char *name = action_holder_get_name (h);
  GVariant *state;

  if (GTK_IS_WIDGET (owner))
    {
      GtkActionKey *key = gtk_action_key_new (name);
      GtkActionResolution resolution = GTK_ACTION_RESOLUTION_INIT;
      GtkActionNode *node = _gtk_widget_get_action_node (GTK_WIDGET (owner), FALSE);

      if (node == NULL || key == NULL ||
          !gtk_action_resolution_init (&resolution, node, key) ||
          !gtk_action_resolution_query (&resolution, NULL, NULL, NULL, NULL, &state))
        state = NULL;

      gtk_action_resolution_clear (&resolution);
      g_clear_pointer (&key, gtk_action_key_unref);
    }
  else if (G_IS_ACTION_GROUP (owner))
    state = g_action_group_get_action_state (G_ACTION_GROUP (owner), name);
  else
    state = NULL;

  if (state)
    {
      char *state_string;

      state_string = g_variant_print (state, FALSE);
      gtk_label_set_label (label, state_string);
      g_free (state_string);
      g_variant_unref (state);
    }
  else
    gtk_label_set_label (label, "");
}

static void
bind_state_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  gpointer item = gtk_list_item_get_item (list_item);
  GtkWidget *label = gtk_list_item_get_child (list_item);

  g_signal_connect (item, "changed", G_CALLBACK (update_state), label);

  update_state (ACTION_HOLDER (item), GTK_LABEL (label));
}

static void
unbind_state_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  gpointer item = gtk_list_item_get_item (list_item);
  GtkWidget *label = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (item, update_state, label);
}

static void
setup_changes_cb (GtkSignalListItemFactory *factory,
                  GtkListItem              *list_item)
{
  GtkWidget *editor;

  editor = gtk_inspector_action_editor_new ();
  gtk_widget_add_css_class (editor, "cell");
  gtk_list_item_set_child (list_item, editor);
}

static void
update_changes (ActionHolder             *h,
                GtkInspectorActionEditor *editor)
{
  gtk_inspector_action_editor_update (editor);
}

static void
bind_changes_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  gpointer item = gtk_list_item_get_item (list_item);
  GtkWidget *editor = gtk_list_item_get_child (list_item);
  GObject *owner = action_holder_get_owner (ACTION_HOLDER (item));
  const char *name = action_holder_get_name (ACTION_HOLDER (item));

  gtk_inspector_action_editor_set (GTK_INSPECTOR_ACTION_EDITOR (editor), owner, name);

  g_signal_connect (item, "changed", G_CALLBACK (update_changes), editor);
}

static void
unbind_changes_cb (GtkSignalListItemFactory *factory,
                   GtkListItem              *list_item)
{
  gpointer item = gtk_list_item_get_item (list_item);
  GtkWidget *editor = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (item, update_changes, editor);
}

static void
add_group (GtkInspectorActions *sl,
           GActionGroup        *group)
{
  int i;
  char **names;

  names = g_action_group_list_actions (group);
  for (i = 0; names[i]; i++)
    action_added (G_OBJECT (group), names[i], sl);
  g_strfreev (names);
}

static void
add_node (GtkInspectorActions *sl,
          GtkWidget           *widget,
          GtkActionNode       *node)
{
  int i;
  char **names;

  names = gtk_action_node_list_actions (node, FALSE);
  for (i = 0; names[i]; i++)
    action_added (G_OBJECT (widget), names[i], sl);
  g_strfreev (names);
}

static gboolean
reload (GtkInspectorActions *sl)
{
  gboolean loaded = FALSE;

  g_object_unref (sl->actions);
  sl->actions = g_list_store_new (ACTION_TYPE_HOLDER);

  if (GTK_IS_WIDGET (sl->object))
    {
      GtkActionNode *node;

      node = _gtk_widget_get_action_node (GTK_WIDGET (sl->object), TRUE);
      if (node != NULL)
        {
          add_node (sl, GTK_WIDGET (sl->object), node);
          loaded = TRUE;
        }
    }
  else if (GTK_IS_APPLICATION (sl->object))
    {
      add_group (sl, G_ACTION_GROUP (sl->object));
      loaded = TRUE;
    }

  gtk_sort_list_model_set_model (sl->sorted, G_LIST_MODEL (sl->actions));

  return loaded;
}

static void
refresh_all (GtkInspectorActions *sl)
{
  reload (sl);
}

static void
action_changed (GtkInspectorActions *sl,
                const char          *name)
{
  unsigned int n_actions;

  n_actions = g_list_model_get_n_items (G_LIST_MODEL (sl->actions));
  for (unsigned int i = 0; i < n_actions; i++)
    {
      ActionHolder *h = ACTION_HOLDER (g_list_model_get_item (G_LIST_MODEL (sl->actions), i));

      if (g_str_equal (action_holder_get_name (h), name))
        {
          action_holder_changed (h);
          g_object_unref (h);
          break;
        }

      g_object_unref (h);
    }
}

static void
action_enabled_changed (GActionGroup        *group,
                        const char          *action_name,
                        gboolean             enabled,
                        GtkInspectorActions *sl)
{
  action_changed (sl, action_name);
}

static void
action_state_changed (GActionGroup        *group,
                      const char          *action_name,
                      GVariant            *state,
                      GtkInspectorActions *sl)
{
  action_changed (sl, action_name);
}

static void
subscription_changed (GtkActionSubscription   *subscription,
                      GtkActionChange           changed,
                      const GtkActionSnapshot *snapshot,
                      gpointer                 user_data)
{
  GtkInspectorActions *sl = user_data;
  GtkActionKey *key = gtk_action_subscription_get_key (subscription);

  action_changed (sl, gtk_action_key_get_full_name (key));
}

static void
gtk_inspector_actions_connect (GtkInspectorActions *sl)
{
  if (GTK_IS_WIDGET (sl->object))
    {
      GtkActionNode *node;

      node = _gtk_widget_get_action_node (GTK_WIDGET (sl->object), TRUE);

      if (node != NULL)
        {
          int i;
          char **names;

          names = gtk_action_node_list_actions (node, FALSE);
          for (i = 0; names[i]; i++)
            {
              GtkActionKey *key = NULL;
              GtkActionSubscription *subscription;

              key = gtk_action_key_new (names[i]);
              if (key == NULL)
                continue;

              subscription = gtk_action_node_subscribe (node,
                                                        key,
                                                        NULL,
                                                        (GTK_ACTION_INTEREST_ENABLED |
                                                         GTK_ACTION_INTEREST_RAW_STATE),
                                                        subscription_changed,
                                                        sl,
                                                        NULL);
              if (subscription != NULL)
                g_ptr_array_add (sl->subscriptions, subscription);

              g_clear_pointer (&key, gtk_action_key_unref);
            }
          g_strfreev (names);
        }
    }
  else if (G_IS_ACTION_GROUP (sl->object))
    {
      g_signal_connect (sl->object, "action-enabled-changed",
                        G_CALLBACK (action_enabled_changed), sl);
      g_signal_connect (sl->object, "action-state-changed",
                        G_CALLBACK (action_state_changed), sl);
    }
}

static void
gtk_inspector_actions_disconnect (GtkInspectorActions *sl)
{
  if (GTK_IS_WIDGET (sl->object))
    {
      for (guint i = 0; i < sl->subscriptions->len; i++)
        gtk_action_subscription_cancel (g_ptr_array_index (sl->subscriptions, i));
      g_ptr_array_set_size (sl->subscriptions, 0);
    }
  else if (G_IS_ACTION_GROUP (sl->object))
    {
      g_signal_handlers_disconnect_by_func (sl->object, action_enabled_changed, sl);
      g_signal_handlers_disconnect_by_func (sl->object, action_state_changed, sl);
    }
}

void
gtk_inspector_actions_set_object (GtkInspectorActions *sl,
                                  GObject             *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  gboolean loaded;

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));
  gtk_stack_page_set_visible (page, FALSE);

  if (sl->object)
    gtk_inspector_actions_disconnect (sl);

  g_set_object (&sl->object, object);

  gtk_column_view_sort_by_column (GTK_COLUMN_VIEW (sl->list), sl->name, GTK_SORT_ASCENDING);
  loaded = reload (sl);
  gtk_stack_page_set_visible (page, loaded);

  if (sl->object)
    gtk_inspector_actions_connect (sl);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorActions *sl = GTK_INSPECTOR_ACTIONS (object);

  switch (param_id)
    {
    case PROP_BUTTON:
      g_value_set_object (value, sl->button);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorActions *sl = GTK_INSPECTOR_ACTIONS (object);

  switch (param_id)
    {
    case PROP_BUTTON:
      sl->button = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static char *
holder_name (gpointer item)
{
  return g_strdup (action_holder_get_name (ACTION_HOLDER (item)));
}

static void
constructed (GObject *object)
{
  GtkInspectorActions *sl = GTK_INSPECTOR_ACTIONS (object);
  GtkSorter *sorter;
  GListModel *model;

  G_OBJECT_CLASS (gtk_inspector_actions_parent_class)->constructed (object);

  g_signal_connect_swapped (sl->button, "clicked",
                            G_CALLBACK (refresh_all), sl);

  sorter = GTK_SORTER (gtk_string_sorter_new (gtk_cclosure_expression_new (G_TYPE_STRING,
                                                               NULL,
                                                               0, NULL,
                                                               (GCallback)holder_name,
                                                               NULL, NULL)));
  gtk_column_view_column_set_sorter (sl->name, sorter);
  g_object_unref (sorter);

  sl->actions = g_list_store_new (ACTION_TYPE_HOLDER);
  sl->subscriptions = g_ptr_array_new ();
  sl->sorted = gtk_sort_list_model_new (g_object_ref (G_LIST_MODEL (sl->actions)),
                                        g_object_ref (gtk_column_view_get_sorter (GTK_COLUMN_VIEW (sl->list))));
  model = G_LIST_MODEL (gtk_no_selection_new (g_object_ref (G_LIST_MODEL (sl->sorted))));
  gtk_column_view_set_model (GTK_COLUMN_VIEW (sl->list), GTK_SELECTION_MODEL (model));
  g_object_unref (model);
}

static void
dispose (GObject *object)
{
  GtkInspectorActions *sl = GTK_INSPECTOR_ACTIONS (object);

  if (sl->object)
    gtk_inspector_actions_disconnect (sl);

  g_clear_object (&sl->sorted);
  g_clear_object (&sl->actions);
  g_clear_object (&sl->object);
  g_clear_pointer (&sl->subscriptions, g_ptr_array_unref);

  gtk_widget_dispose_template (GTK_WIDGET (sl), GTK_TYPE_INSPECTOR_ACTIONS);

  G_OBJECT_CLASS (gtk_inspector_actions_parent_class)->dispose (object);
}

static void
gtk_inspector_actions_class_init (GtkInspectorActionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;

  props[PROP_BUTTON] = g_param_spec_object ("button", NULL, NULL,
                                            GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, N_PROPS, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/actions.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorActions, swin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorActions, list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorActions, name);
  gtk_widget_class_bind_template_callback (widget_class, setup_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_enabled_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_enabled_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_enabled_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_parameter_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_parameter_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_state_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_state_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_state_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_changes_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_changes_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_changes_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

// vim: set et sw=2 ts=2:
