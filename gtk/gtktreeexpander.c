/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtktreeexpander.h"

#include "gtkboxlayout.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkintl.h"
#include "gtktreelistmodel.h"

/**
 * SECTION:gtktreeexpander
 * @title: GtkTreeExpander
 * @short_description: An indenting expander button for use in a tree list
 * @see_also: #GtkTreeListModel
 *
 * GtkTreeExpander is a widget that provides an expander for a list.
 *
 * It is typically placed as a bottommost child into a #GtkListView to allow
 * users to expand and collapse children in a list with a #GtkTreeListModel.
 * It will provide the common UI elements, gestures and keybindings for this
 * purpose.
 *
 * On top of this, the "listitem.expand", "listitem.collapse" and
 * "listitem.toggle-expand" actions are provided to allow adding custom UI
 * for managing expanded state.
 *
 * The #GtkTreeListModel must be set to not be passthrough. Then it will provide
 * #GtkTreeListRow items which can be set via gtk_tree_expander_set_list_row()
 * on the expander. The expander will then watch that row item automatically.  
 * gtk_tree_expander_set_child() sets the widget that displays the actual row
 * contents.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * treeexpander
 * ├── [indent]*
 * ├── [expander]
 * ╰── <child>
 * ]|
 *
 * GtkTreeExpander has zero or one CSS nodes with the name "expander" that should
 * display the expander icon. The node will be `:checked` when it is expanded.
 * If the node is not expandable, an "indent" node will be displayed instead.
 *
 * For every level of depth, another "indent" node is prepended.
 */

struct _GtkTreeExpander
{
  GtkWidget parent_instance;

  GtkTreeListRow *list_row;
  GtkWidget *child;

  GtkWidget *expander;
  guint notify_handler;
};

enum
{
  PROP_0,
  PROP_CHILD,
  PROP_ITEM,
  PROP_LIST_ROW,

  N_PROPS
};

G_DEFINE_TYPE (GtkTreeExpander, gtk_tree_expander, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_tree_expander_update_for_list_row (GtkTreeExpander *self)
{
  if (self->list_row == NULL)
    {
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
           child != self->child;
           child = gtk_widget_get_first_child (GTK_WIDGET (self)))
        {
          gtk_widget_unparent (child);
        }
      self->expander = NULL;
    }
  else
    {
      GtkWidget *child;
      guint i, depth;

      depth = gtk_tree_list_row_get_depth (self->list_row);
      if (gtk_tree_list_row_is_expandable (self->list_row))
        {
          if (self->expander == NULL)
            {
              self->expander = gtk_builtin_icon_new ("expander");
              gtk_widget_insert_before (self->expander,
                                        GTK_WIDGET (self),
                                        self->child);
            }
          if (gtk_tree_list_row_get_expanded (self->list_row))
            gtk_widget_set_state_flags (self->expander, GTK_STATE_FLAG_CHECKED, FALSE);
          else
            gtk_widget_unset_state_flags (self->expander, GTK_STATE_FLAG_CHECKED);
          child = gtk_widget_get_prev_sibling (self->expander);
        }
      else
        {
          g_clear_pointer (&self->expander, gtk_widget_unparent);
          depth++;
          if (self->child)
            child = gtk_widget_get_prev_sibling (self->child);
          else
            child = gtk_widget_get_last_child (GTK_WIDGET (self));
        }

      for (i = 0; i < depth; i++)
        {
          if (child)
            child = gtk_widget_get_prev_sibling (child);
          else
            gtk_widget_insert_after (gtk_builtin_icon_new ("indent"), GTK_WIDGET (self), NULL);
        }

      while (child)
        {
          GtkWidget *prev = gtk_widget_get_prev_sibling (child);
          gtk_widget_unparent (child);
          child = prev;
        }
    }
}

static void
gtk_tree_expander_list_row_notify_cb (GtkTreeListRow  *list_row,
                                      GParamSpec      *pspec,
                                      GtkTreeExpander *self)
{
  if (pspec->name == g_intern_static_string ("expanded"))
    {
      if (self->expander)
        {
          if (gtk_tree_list_row_get_expanded (list_row))
            gtk_widget_set_state_flags (self->expander, GTK_STATE_FLAG_CHECKED, FALSE);
          else
            gtk_widget_unset_state_flags (self->expander, GTK_STATE_FLAG_CHECKED);
        }
    }
  else if (pspec->name == g_intern_static_string ("item"))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
    }
  else
    {
      /* can this happen other than when destroying the row? */
      gtk_tree_expander_update_for_list_row (self);
    }
}

static void
gtk_tree_expander_clear_list_row (GtkTreeExpander *self)
{
  if (self->list_row == NULL)
    return;

  g_signal_handler_disconnect (self->list_row, self->notify_handler);
  self->notify_handler = 0;
  g_clear_object (&self->list_row);
}

static void
gtk_tree_expander_dispose (GObject *object)
{
  GtkTreeExpander *self = GTK_TREE_EXPANDER (object);

  gtk_tree_expander_clear_list_row (self);
  gtk_tree_expander_update_for_list_row (self);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  g_assert (self->expander == NULL);

  G_OBJECT_CLASS (gtk_tree_expander_parent_class)->dispose (object);
}

static void
gtk_tree_expander_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkTreeExpander *self = GTK_TREE_EXPANDER (object);

  switch (property_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;

    case PROP_ITEM:
      g_value_set_object (value, gtk_tree_expander_get_item (self));
      break;

    case PROP_LIST_ROW:
      g_value_set_object (value, self->list_row);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_tree_expander_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkTreeExpander *self = GTK_TREE_EXPANDER (object);

  switch (property_id)
    {
    case PROP_CHILD:
      gtk_tree_expander_set_child (self, g_value_get_object (value));
      break;

    case PROP_LIST_ROW:
      gtk_tree_expander_set_list_row (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_tree_expander_class_init (GtkTreeExpanderClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_tree_expander_dispose;
  gobject_class->get_property = gtk_tree_expander_get_property;
  gobject_class->set_property = gtk_tree_expander_set_property;

  /**
   * GtkTreeExpander:child:
   *
   * The child widget with the actual contents
   */
  properties[PROP_CHILD] =
    g_param_spec_object ("child",
                         P_("Child"),
                         P_("The child widget with the actual contents"),
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkTreeExpander:item:
   *
   * The item held by this expander's row
   */
  properties[PROP_ITEM] =
      g_param_spec_object ("item",
                           P_("Item"),
                           P_("The item held by this expander's row"),
                           G_TYPE_OBJECT,
                           G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkTreeExpander:list-row:
   *
   * The list row to track for expander state
   */
  properties[PROP_LIST_ROW] =
    g_param_spec_object ("list-row",
                         P_("List row"),
                         P_("The list row to track for expander state"),
                         GTK_TYPE_TREE_LIST_ROW,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("treeexpander"));
}

static void
gtk_tree_expander_init (GtkTreeExpander *self)
{
}

/**
 * gtk_tree_expander_new:
 *
 * Creates a new #GtkTreeExpander
 *
 * Returns: a new #GtkTreeExpander
 **/
GtkWidget *
gtk_tree_expander_new (void)
{
  return g_object_new (GTK_TYPE_TREE_EXPANDER,
                       NULL);
}

/**
 * gtk_tree_expander_get_child
 * @self: a #GtkTreeExpander
 *
 * Gets the child widget displayed by @self.
 *
 * Returns: (nullable) (transfer none): The child displayed by @self
 **/
GtkWidget *
gtk_tree_expander_get_child (GtkTreeExpander *self)
{
  g_return_val_if_fail (GTK_IS_TREE_EXPANDER (self), NULL);

  return self->child;
}

/**
 * gtk_tree_expander_set_child:
 * @self: a #GtkTreeExpander widget
 * @child: (nullable): a #GtkWidget, or %NULL
 *
 * Sets the content widget to display.
 */
void
gtk_tree_expander_set_child (GtkTreeExpander *self,
                             GtkWidget       *child)
{
  g_return_if_fail (GTK_IS_TREE_EXPANDER (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child)
    {
      self->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

/**
 * gtk_tree_expander_get_item
 * @self: a #GtkTreeExpander
 *
 * Forwards the item set on the #GtkTreeListRow that @self is managing.
 *
 * This call is essentially equivalent to calling
 * `gtk_tree_list_row_get_item (gtk_tree_expander_get_list_row (@self))`.
 *
 * Returns: (nullable) (transfer none): The item of the row
 **/
gpointer
gtk_tree_expander_get_item (GtkTreeExpander *self)
{
  g_return_val_if_fail (GTK_IS_TREE_EXPANDER (self), NULL);

  if (self->list_row == NULL)
    return NULL;

  return gtk_tree_list_row_get_item (self->list_row);
}

/**
 * gtk_tree_expander_get_list_row
 * @self: a #GtkTreeExpander
 *
 * Gets the list row managed by @self.
 *
 * Returns: (nullable) (transfer none): The list row displayed by @self
 **/
GtkTreeListRow *
gtk_tree_expander_get_list_row (GtkTreeExpander *self)
{
  g_return_val_if_fail (GTK_IS_TREE_EXPANDER (self), NULL);

  return self->list_row;
}

/**
 * gtk_tree_expander_set_list_row:
 * @self: a #GtkTreeExpander widget
 * @list_row: (nullable): a #GtkTreeListRow, or %NULL
 *
 * Sets the tree list row that this expander should manage.
 */
void
gtk_tree_expander_set_list_row (GtkTreeExpander *self,
                                GtkTreeListRow  *list_row)
{
  g_return_if_fail (GTK_IS_TREE_EXPANDER (self));
  g_return_if_fail (list_row == NULL || GTK_IS_TREE_LIST_ROW (list_row));

  if (self->list_row == list_row)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  gtk_tree_expander_clear_list_row (self);

  if (list_row)
    {
      self->list_row = g_object_ref (list_row);
      self->notify_handler = g_signal_connect (list_row,
                                               "notify",
                                               G_CALLBACK (gtk_tree_expander_list_row_notify_cb),
                                               self);
    }

  gtk_tree_expander_update_for_list_row (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIST_ROW]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);

  g_object_thaw_notify (G_OBJECT (self));
}

