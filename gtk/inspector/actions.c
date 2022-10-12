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
#include "gtkwidgetprivate.h"
#include "gtkactionmuxerprivate.h"
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
bind_enabled_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  gpointer item;
  GtkWidget *label;
  GObject *owner;
  const char *name;
  gboolean enabled = FALSE;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  owner = action_holder_get_owner (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));
  if (G_IS_ACTION_GROUP (owner))
    enabled = g_action_group_get_action_enabled (G_ACTION_GROUP (owner), name);
  else if (GTK_IS_ACTION_MUXER (owner))
    gtk_action_muxer_query_action (GTK_ACTION_MUXER (owner), name,
                                   &enabled, NULL, NULL, NULL, NULL);

  gtk_label_set_label (GTK_LABEL (label), enabled ? "+" : "-");
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
  const char *parameter;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  owner = action_holder_get_owner (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));
  if (G_IS_ACTION_GROUP (owner))
    parameter = (const char *)g_action_group_get_action_parameter_type (G_ACTION_GROUP (owner), name);
  else if (GTK_IS_ACTION_MUXER (owner))
    gtk_action_muxer_query_action (GTK_ACTION_MUXER (owner), name,
                                   NULL, (const GVariantType **)&parameter, NULL, NULL, NULL);
  else
    parameter = "(Unknown)";

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
  gtk_widget_add_css_class (label, "cell");
  gtk_list_item_set_child (list_item, label);
}

static void
bind_state_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  gpointer item;
  GtkWidget *label;
  GObject *owner;
  const char *name;
  GVariant *state;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  owner = action_holder_get_owner (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));
  if (G_IS_ACTION_GROUP (owner))
    state = g_action_group_get_action_state (G_ACTION_GROUP (owner), name);
  else if (GTK_IS_ACTION_MUXER (owner))
    gtk_action_muxer_query_action (GTK_ACTION_MUXER (owner), name,
                                   NULL, NULL, NULL, NULL, &state);
  else
    state = NULL;

  if (state)
    {
      char *state_string;

      state_string = g_variant_print (state, FALSE);
      gtk_label_set_label (GTK_LABEL (label), state_string);
      g_free (state_string);
      g_variant_unref (state);
    }
  else
    gtk_label_set_label (GTK_LABEL (label), "");
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
bind_changes_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  gpointer item;
  GObject *owner;
  const char *name;
  GtkWidget *editor;

  item = gtk_list_item_get_item (list_item);
  editor = gtk_list_item_get_child (list_item);

  owner = action_holder_get_owner (ACTION_HOLDER (item));
  name = action_holder_get_name (ACTION_HOLDER (item));

  gtk_inspector_action_editor_set (GTK_INSPECTOR_ACTION_EDITOR (editor),
                                   owner,
                                   name);
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
add_muxer (GtkInspectorActions *sl,
           GtkActionMuxer      *muxer)
{
  int i;
  char **names;

  names = gtk_action_muxer_list_actions (muxer, FALSE);
  for (i = 0; names[i]; i++)
    action_added (G_OBJECT (muxer), names[i], sl);
  g_strfreev (names);
}

static gboolean
reload (GtkInspectorActions *sl)
{
  gboolean loaded = FALSE;

  g_object_unref (sl->actions);
  sl->actions = g_list_store_new (ACTION_TYPE_HOLDER);

  if (GTK_IS_APPLICATION (sl->object))
    {
      add_group (sl, G_ACTION_GROUP (sl->object));
      loaded = TRUE;
    }
  else if (GTK_IS_WIDGET (sl->object))
    {
      GtkActionMuxer *muxer;

      muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (sl->object), FALSE);
      if (muxer)
        {
          add_muxer (sl, muxer);
          loaded = TRUE;
        }
    }

  gtk_sort_list_model_set_model (sl->sorted, G_LIST_MODEL (sl->actions));

  return loaded;
}

static void
refresh_all (GtkInspectorActions *sl)
{
  reload (sl);
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

  g_set_object (&sl->object, object);

  gtk_column_view_sort_by_column (GTK_COLUMN_VIEW (sl->list), sl->name, GTK_SORT_ASCENDING);
  loaded = reload (sl);
  gtk_stack_page_set_visible (page, loaded);
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

  g_clear_object (&sl->sorted);
  g_clear_object (&sl->actions);
  g_clear_object (&sl->object);

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

  g_object_class_install_property (object_class, PROP_BUTTON,
      g_param_spec_object ("button", NULL, NULL,
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/actions.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorActions, swin);
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
  gtk_widget_class_bind_template_callback (widget_class, setup_changes_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_changes_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

// vim: set et sw=2 ts=2:
