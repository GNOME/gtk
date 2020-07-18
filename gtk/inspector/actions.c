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
#include "gtktreeview.h"
#include "gtkliststore.h"
#include "gtkwidgetprivate.h"
#include "gtkactionmuxerprivate.h"
#include "gtkpopover.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtklistbox.h"
#include "gtkstylecontext.h"
#include "gtksizegroup.h"
#include "gtkboxlayout.h"


struct _GtkInspectorActions
{
  GtkWidget parent;

  GtkWidget *list;
  GtkWidget *button;

  GActionGroup *group;
  GListModel *actions;
  GtkColumnViewColumn *name;
};

typedef struct _GtkInspectorActionsClass
{
  GtkWidgetClass parent;
} GtkInspectorActionsClass;

enum {
  PROP_0,
  PROP_BUTTON
};

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
action_added_cb (GActionGroup        *group,
                 const gchar         *action_name,
                 GtkInspectorActions *sl)
{
  ActionHolder *holder = action_holder_new (group, action_name);
  g_list_store_append (G_LIST_STORE (sl->actions), holder);
  g_object_unref (holder);
}


static void
setup_name_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
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
  gtk_list_item_set_child (list_item, label);
}

static void
bind_enabled_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  gpointer item;
  GtkWidget *label;
  GActionGroup *group;
  const char *name;
  gboolean enabled;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  group = action_holder_get_group (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));
  enabled = g_action_group_get_action_enabled (group, name);

  gtk_label_set_label (GTK_LABEL (label), enabled ? "+" : "-");
}

static void
setup_parameter_cb (GtkSignalListItemFactory *factory,
                    GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.5);
  gtk_list_item_set_child (list_item, label);
  gtk_widget_add_css_class (label, "cell");
}

static void
bind_parameter_cb (GtkSignalListItemFactory *factory,
                   GtkListItem              *list_item)
{
  gpointer item;
  GtkWidget *label;
  GActionGroup *group;
  const char *name;
  const char *parameter;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  group = action_holder_get_group (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));
  parameter = (const gchar *)g_action_group_get_action_parameter_type (group, name);

  gtk_label_set_label (GTK_LABEL (label), parameter);
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
  gtk_list_item_set_child (list_item, label);
  gtk_widget_add_css_class (label, "cell");
}

static void
bind_state_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  gpointer item;
  GtkWidget *label;
  GActionGroup *group;
  const char *name;
  GVariant *state;
  char *state_string;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  group = action_holder_get_group (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));
  state = g_action_group_get_action_state (group, name);
  if (state)
    state_string = g_variant_print (state, FALSE);
  else
    state_string = g_strdup ("");

  gtk_label_set_label (GTK_LABEL (label), state_string);

  g_free (state_string);
  if (state)
    g_variant_unref (state);
}

static void
bind_changes_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  gpointer item;
  GActionGroup *group;
  const char *name;
  GtkWidget *editor;

  item = gtk_list_item_get_item (list_item);

  group = action_holder_get_group (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));

  editor = gtk_inspector_action_editor_new (group, name, NULL);
  gtk_widget_add_css_class (editor, "cell");
  gtk_list_item_set_child (list_item, editor);
}

static void
unbind_changes_cb (GtkSignalListItemFactory *factory,
                   GtkListItem              *list_item)
{
  gtk_list_item_set_child (list_item, NULL);
}

static void
action_removed_cb (GActionGroup        *group,
                   const gchar         *action_name,
                   GtkInspectorActions *sl)
{
  int i;

  for (i = 0; i < g_list_model_get_n_items (sl->actions); i++)
    {
      ActionHolder *holder = g_list_model_get_item (sl->actions, i);

      if (group == action_holder_get_group (holder) &&
          strcmp (action_name, action_holder_get_name (holder)) == 0)
        g_list_store_remove (G_LIST_STORE (sl->actions), i);

      g_object_unref (holder);
    }
}

static void
notify_action_changed (GtkInspectorActions *sl,
                       GActionGroup        *group,
                       const char          *action_name)
{
  int i;

  for (i = 0; i < g_list_model_get_n_items (sl->actions); i++)
    {
      ActionHolder *holder = g_list_model_get_item (sl->actions, i);

      if (group == action_holder_get_group (holder) &&
          strcmp (action_name, action_holder_get_name (holder)) == 0)
        g_list_model_items_changed (sl->actions, i, 1, 1);

      g_object_unref (holder);
    }
}

static void
action_enabled_changed_cb (GActionGroup        *group,
                           const gchar         *action_name,
                           gboolean             enabled,
                           GtkInspectorActions *sl)
{
  notify_action_changed (sl, group, action_name);
}

static void
action_state_changed_cb (GActionGroup        *group,
                         const gchar         *action_name,
                         GVariant            *state,
                         GtkInspectorActions *sl)
{
  notify_action_changed (sl, group, action_name);
}

static void
refresh_all (GtkInspectorActions *sl)
{
  guint n = g_list_model_get_n_items (sl->actions);
  g_list_model_items_changed (sl->actions, 0, n, n);
}

static void
connect_group (GActionGroup *group,
               GtkInspectorActions *sl)
{
  g_signal_connect (group, "action-added", G_CALLBACK (action_added_cb), sl);
  g_signal_connect (group, "action-removed", G_CALLBACK (action_removed_cb), sl);
  g_signal_connect (group, "action-enabled-changed", G_CALLBACK (action_enabled_changed_cb), sl);
  g_signal_connect (group, "action-state-changed", G_CALLBACK (action_state_changed_cb), sl);
}

static void
disconnect_group (GActionGroup *group,
                  GtkInspectorActions *sl)
{
  g_signal_handlers_disconnect_by_func (group, action_added_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_removed_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_enabled_changed_cb, sl);
  g_signal_handlers_disconnect_by_func (group, action_state_changed_cb, sl);
}

static void
add_group (GtkInspectorActions *sl,
           GtkStackPage        *page,
           GActionGroup        *group)
{
  gint i;
  gchar **names;

  g_object_set (page, "visible", TRUE, NULL);

  connect_group (group, sl);

  names = g_action_group_list_actions (group);
  for (i = 0; names[i]; i++)
    action_added_cb (group, names[i], sl);
  g_strfreev (names);

  g_set_object (&sl->group, group);
}

static void
remove_group (GtkInspectorActions *sl,
              GtkStackPage        *page,
              GActionGroup        *group)
{
  disconnect_group (group, sl);

  g_set_object (&sl->group, NULL);
}

void
gtk_inspector_actions_set_object (GtkInspectorActions *sl,
                                  GObject             *object)
{
  GtkWidget *stack;
  GtkStackPage *page;

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));

  g_object_set (page, "visible", FALSE, NULL);

  if (sl->group)
    remove_group (sl, page, sl->group);

  g_list_store_remove_all (G_LIST_STORE (sl->actions));

  if (GTK_IS_APPLICATION (object))
    add_group (sl, page, G_ACTION_GROUP (object));
  else if (GTK_IS_WIDGET (object))
    {
      GtkActionMuxer *muxer;

      muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (object), FALSE);
      if (muxer)
        add_group (sl, page, G_ACTION_GROUP (muxer));
    }

  gtk_column_view_sort_by_column (GTK_COLUMN_VIEW (sl->list), sl->name, GTK_SORT_ASCENDING);
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
  GListModel *sorted;
  GListModel *model;

  g_signal_connect_swapped (sl->button, "clicked",
                            G_CALLBACK (refresh_all), sl);

  sorter = gtk_string_sorter_new (gtk_cclosure_expression_new (G_TYPE_STRING,
                                                               NULL,
                                                               0, NULL,
                                                               (GCallback)holder_name,
                                                               NULL, NULL));
  gtk_column_view_column_set_sorter (sl->name, sorter);
  g_object_unref (sorter);
  
  sl->actions = G_LIST_MODEL (g_list_store_new (ACTION_TYPE_HOLDER));
  sorted = G_LIST_MODEL (gtk_sort_list_model_new (sl->actions,
                                                  gtk_column_view_get_sorter (GTK_COLUMN_VIEW (sl->list))));
  model = G_LIST_MODEL (gtk_no_selection_new (sorted));
  gtk_column_view_set_model (GTK_COLUMN_VIEW (sl->list), model);
  g_object_unref (sorted);
  g_object_unref (model);
}

static void
dispose (GObject *object)
{
  GtkInspectorActions *sl = GTK_INSPECTOR_ACTIONS (object);
  GtkWidget *child;

  g_clear_object (&sl->actions);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (sl))))
    gtk_widget_unparent (child);

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

  g_object_class_install_property (object_class, PROP_BUTTON,
      g_param_spec_object ("button", NULL, NULL,
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/actions.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorActions, list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorActions, name);
  gtk_widget_class_bind_template_callback (widget_class, setup_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_enabled_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_enabled_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_parameter_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_parameter_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_state_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_state_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_changes_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_changes_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

// vim: set et sw=2 ts=2:
