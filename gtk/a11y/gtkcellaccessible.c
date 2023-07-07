/* GTK+ - accessibility implementations
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gtkcontainercellaccessible.h"
#include "gtkcellaccessibleprivate.h"
#include "gtkcellaccessibleparent.h"

struct _GtkCellAccessiblePrivate
{
  AtkObject *parent;
};

static const struct {
  AtkState atk_state;
  GtkCellRendererState renderer_state;
  gboolean invert;
} state_map[] = {
  { ATK_STATE_SENSITIVE, GTK_CELL_RENDERER_INSENSITIVE, TRUE },
  { ATK_STATE_ENABLED,   GTK_CELL_RENDERER_INSENSITIVE, TRUE },
  { ATK_STATE_SELECTED,  GTK_CELL_RENDERER_SELECTED,    FALSE },
  /* XXX: why do we map ACTIVE here? */
  { ATK_STATE_ACTIVE,    GTK_CELL_RENDERER_FOCUSED,     FALSE },
  { ATK_STATE_FOCUSED,   GTK_CELL_RENDERER_FOCUSED,     FALSE },
  { ATK_STATE_EXPANDABLE,GTK_CELL_RENDERER_EXPANDABLE,  FALSE },
  { ATK_STATE_EXPANDED,  GTK_CELL_RENDERER_EXPANDED,    FALSE },
};

static GtkCellRendererState gtk_cell_accessible_get_state (GtkCellAccessible *cell);
static void atk_action_interface_init    (AtkActionIface    *iface);
static void atk_component_interface_init (AtkComponentIface *iface);
static void atk_table_cell_interface_init    (AtkTableCellIface    *iface);

G_DEFINE_TYPE_WITH_CODE (GtkCellAccessible, gtk_cell_accessible, GTK_TYPE_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkCellAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TABLE_CELL, atk_table_cell_interface_init))

static gint
gtk_cell_accessible_get_index_in_parent (AtkObject *obj)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (obj);

  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (cell->priv->parent))
    return g_list_index (gtk_container_cell_accessible_get_children (GTK_CONTAINER_CELL_ACCESSIBLE (cell->priv->parent)), obj);

  parent = gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)));
  if (parent == NULL)
    return -1;

  return gtk_cell_accessible_parent_get_child_index (GTK_CELL_ACCESSIBLE_PARENT (cell->priv->parent), cell);
}

static AtkRelationSet *
gtk_cell_accessible_ref_relation_set (AtkObject *object)
{
  GtkCellAccessible *cell;
  AtkRelationSet *relationset;
  AtkObject *parent;

  relationset = ATK_OBJECT_CLASS (gtk_cell_accessible_parent_class)->ref_relation_set (object);
  if (relationset == NULL)
    relationset = atk_relation_set_new ();

  cell = GTK_CELL_ACCESSIBLE (object);
  parent = gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)));

  gtk_cell_accessible_parent_update_relationset (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                                 cell,
                                                 relationset);

  return relationset;
}

static AtkStateSet *
gtk_cell_accessible_ref_state_set (AtkObject *accessible)
{
  GtkCellAccessible *cell_accessible;
  AtkStateSet *state_set;
  GtkCellRendererState flags;
  guint i;

  cell_accessible = GTK_CELL_ACCESSIBLE (accessible);

  state_set = atk_state_set_new ();

  if (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell_accessible)) == NULL)
    {
      atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
      return state_set;
    }

  flags = gtk_cell_accessible_get_state (cell_accessible);

  atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);
  atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);
  atk_state_set_add_state (state_set, ATK_STATE_TRANSIENT);
  atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);

  for (i = 0; i < G_N_ELEMENTS (state_map); i++)
    {
      if (flags & state_map[i].renderer_state)
        {
          if (!state_map[i].invert)
            atk_state_set_add_state (state_set, state_map[i].atk_state);
        }
      else
        {
          if (state_map[i].invert)
            atk_state_set_add_state (state_set, state_map[i].atk_state);
        }
    }

  if (gtk_widget_get_mapped (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell_accessible))))
    atk_state_set_add_state (state_set, ATK_STATE_SHOWING);

  return state_set;
}

static AtkObject *
gtk_cell_accessible_get_parent (AtkObject *object)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (object);

  return cell->priv->parent;
}

static void
gtk_cell_accessible_class_init (GtkCellAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_index_in_parent = gtk_cell_accessible_get_index_in_parent;
  class->ref_state_set = gtk_cell_accessible_ref_state_set;
  class->ref_relation_set = gtk_cell_accessible_ref_relation_set;
  class->get_parent = gtk_cell_accessible_get_parent;
}

static void
gtk_cell_accessible_init (GtkCellAccessible *cell)
{
  cell->priv = gtk_cell_accessible_get_instance_private (cell);
}

void
_gtk_cell_accessible_initialize (GtkCellAccessible *cell,
                                 GtkWidget         *widget,
                                 AtkObject         *parent)
{
  gtk_accessible_set_widget (GTK_ACCESSIBLE (cell), widget);
  cell->priv->parent = parent;
}

gboolean
_gtk_cell_accessible_add_state (GtkCellAccessible *cell,
                                AtkStateType       state_type,
                                gboolean           emit_signal)
{
  /* The signal should only be generated if the value changed,
   * not when the cell is set up. So states that are set
   * initially should pass FALSE as the emit_signal argument.
   */
  if (emit_signal)
    {
      atk_object_notify_state_change (ATK_OBJECT (cell), state_type, TRUE);
      /* If state_type is ATK_STATE_VISIBLE, additional notification */
      if (state_type == ATK_STATE_VISIBLE)
        g_signal_emit_by_name (cell, "visible-data-changed");
    }

  /* If the parent is a flyweight container cell, propagate the state
   * change to it also
   */
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (cell->priv->parent))
    _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (cell->priv->parent), state_type, emit_signal);

  return TRUE;
}

gboolean
_gtk_cell_accessible_remove_state (GtkCellAccessible *cell,
                                   AtkStateType       state_type,
                                   gboolean           emit_signal)
{
  /* The signal should only be generated if the value changed,
   * not when the cell is set up.  So states that are set
   * initially should pass FALSE as the emit_signal argument.
   */
  if (emit_signal)
    {
      atk_object_notify_state_change (ATK_OBJECT (cell), state_type, FALSE);
      /* If state_type is ATK_STATE_VISIBLE, additional notification */
      if (state_type == ATK_STATE_VISIBLE)
        g_signal_emit_by_name (cell, "visible-data-changed");
    }

  /* If the parent is a flyweight container cell, propagate the state
   * change to it also
   */
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (cell->priv->parent))
    _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (cell->priv->parent), state_type, emit_signal);

  return TRUE;
}

static gint
gtk_cell_accessible_action_get_n_actions (AtkAction *action)
{
  return 3;
}

static const gchar *
gtk_cell_accessible_action_get_name (AtkAction *action,
                                     gint       index)
{
  switch (index)
    {
    case 0:
      return "expand or contract";
    case 1:
      return "edit";
    case 2:
      return "activate";
    default:
      return NULL;
    }
}

static const gchar *
gtk_cell_accessible_action_get_localized_name (AtkAction *action,
                                               gint       index)
{
  switch (index)
    {
    case 0:
      return C_("Action name", "Expand or contract");
    case 1:
      return C_("Action name", "Edit");
    case 2:
      return C_("Action name", "Activate");
    default:
      return NULL;
    }
}

static const gchar *
gtk_cell_accessible_action_get_description (AtkAction *action,
                                            gint       index)
{
  switch (index)
    {
    case 0:
      return C_("Action description", "Expands or contracts the row in the tree view containing this cell");
    case 1:
      return C_("Action description", "Creates a widget in which the contents of the cell can be edited");
    case 2:
      return C_("Action description", "Activates the cell");
    default:
      return NULL;
    }
}

static const gchar *
gtk_cell_accessible_action_get_keybinding (AtkAction *action,
                                           gint       index)
{
  return NULL;
}

static gboolean
gtk_cell_accessible_action_do_action (AtkAction *action,
                                      gint       index)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (action);
  GtkCellAccessibleParent *parent;

  cell = GTK_CELL_ACCESSIBLE (action);
  if (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)) == NULL)
    return FALSE;

  parent = GTK_CELL_ACCESSIBLE_PARENT (gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell))));

  switch (index)
    {
    case 0:
      gtk_cell_accessible_parent_expand_collapse (parent, cell);
      break;
    case 1:
      gtk_cell_accessible_parent_edit (parent, cell);
      break;
    case 2:
      gtk_cell_accessible_parent_activate (parent, cell);
      break;
    default:
      return FALSE;
    }

  return TRUE;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->get_n_actions = gtk_cell_accessible_action_get_n_actions;
  iface->do_action = gtk_cell_accessible_action_do_action;
  iface->get_name = gtk_cell_accessible_action_get_name;
  iface->get_localized_name = gtk_cell_accessible_action_get_localized_name;
  iface->get_description = gtk_cell_accessible_action_get_description;
  iface->get_keybinding = gtk_cell_accessible_action_get_keybinding;
}

static void
gtk_cell_accessible_get_extents (AtkComponent *component,
                                 gint         *x,
                                 gint         *y,
                                 gint         *width,
                                 gint         *height,
                                 AtkCoordType  coord_type)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (component);
  parent = gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)));

  gtk_cell_accessible_parent_get_cell_extents (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                                cell,
                                                x, y, width, height, coord_type);
}

static gboolean
gtk_cell_accessible_grab_focus (AtkComponent *component)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (component);
  parent = gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)));

  return gtk_cell_accessible_parent_grab_focus (GTK_CELL_ACCESSIBLE_PARENT (parent), cell);
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  iface->get_extents = gtk_cell_accessible_get_extents;
  iface->grab_focus = gtk_cell_accessible_grab_focus;
}

static int
gtk_cell_accessible_get_column_span (AtkTableCell *table_cell)
{
  return 1;
}

static GPtrArray *
gtk_cell_accessible_get_column_header_cells (AtkTableCell *table_cell)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (table_cell);
  parent = gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)));

  return gtk_cell_accessible_parent_get_column_header_cells (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                                             cell);
}

static gboolean
gtk_cell_accessible_get_position (AtkTableCell *table_cell,
                                  gint         *row,
                                  gint         *column)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (table_cell);
  parent = gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)));

  gtk_cell_accessible_parent_get_cell_position (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                                cell,
                                                row, column);
  return ((row && *row >= 0) || (column && *column >= 0));
}

static int
gtk_cell_accessible_get_row_span (AtkTableCell *table_cell)
{
  return 1;
}

static GPtrArray *
gtk_cell_accessible_get_row_header_cells (AtkTableCell *table_cell)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (table_cell);
  parent = gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)));

  return gtk_cell_accessible_parent_get_row_header_cells (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                                          cell);
}

static AtkObject *
gtk_cell_accessible_get_table (AtkTableCell *table_cell)
{
  AtkObject *obj;

  obj = ATK_OBJECT (table_cell);
  do
    {
      AtkRole role;
      obj = atk_object_get_parent (obj);
      role = atk_object_get_role (obj);
      if (role == ATK_ROLE_TABLE || role == ATK_ROLE_TREE_TABLE)
        break;
    }
  while (obj);
  return obj;
}

static void
atk_table_cell_interface_init (AtkTableCellIface *iface)
{
  iface->get_column_span = gtk_cell_accessible_get_column_span;
  iface->get_column_header_cells = gtk_cell_accessible_get_column_header_cells;
  iface->get_position = gtk_cell_accessible_get_position;
  iface->get_row_span = gtk_cell_accessible_get_row_span;
  iface->get_row_header_cells = gtk_cell_accessible_get_row_header_cells;
  iface->get_table = gtk_cell_accessible_get_table;
}

static GtkCellRendererState
gtk_cell_accessible_get_state (GtkCellAccessible *cell)
{
  AtkObject *parent;

  g_return_val_if_fail (GTK_IS_CELL_ACCESSIBLE (cell), 0);

  parent = gtk_widget_get_accessible (gtk_accessible_get_widget (GTK_ACCESSIBLE (cell)));
  if (parent == NULL)
    return 0;

  return gtk_cell_accessible_parent_get_renderer_state (GTK_CELL_ACCESSIBLE_PARENT (parent), cell);
}

/*
 * gtk_cell_accessible_state_changed:
 * @cell: a #GtkCellAccessible
 * @added: the flags that were added from @cell
 * @removed: the flags that were removed from @cell
 *
 * Notifies @cell of state changes. Multiple states may be added
 * or removed at the same time. A state that is @added may not be
 * @removed at the same time.
 **/
void
_gtk_cell_accessible_state_changed (GtkCellAccessible    *cell,
                                    GtkCellRendererState  added,
                                    GtkCellRendererState  removed)
{
  AtkObject *object;
  guint i;

  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (cell));
  g_return_if_fail ((added & removed) == 0);

  object = ATK_OBJECT (cell);

  for (i = 0; i < G_N_ELEMENTS (state_map); i++)
    {
      if (added & state_map[i].renderer_state)
        atk_object_notify_state_change (object,
                                        state_map[i].atk_state,
                                        !state_map[i].invert);
      if (removed & state_map[i].renderer_state)
        atk_object_notify_state_change (object,
                                        state_map[i].atk_state,
                                        state_map[i].invert);
    }
}

/*
 * gtk_cell_accessible_update_cache:
 * @cell: the cell that is changed
 * @emit_signal: whether or not to notify the ATK bridge
 *
 * Notifies the cell that the values in the data in the row that
 * is used to feed the cell renderer with has changed. The
 * cell_changed function of @cell is called to send update
 * notifications for the properties it takes from its cell
 * renderer. If @emit_signal is TRUE, also notify the ATK bridge
 * of the change. The bridge should be notified when an existing
 * cell changes; not when a newly-created cell is being set up.
 *
 * Note that there is no higher granularity available about which
 * properties changed, so you will need to make do with this
 * function.
 **/
void
_gtk_cell_accessible_update_cache (GtkCellAccessible *cell,
                                   gboolean           emit_signal)
{
  GtkCellAccessibleClass *klass;

  g_return_if_fail (GTK_CELL_ACCESSIBLE (cell));

  klass = GTK_CELL_ACCESSIBLE_GET_CLASS (cell);

  if (klass->update_cache)
    klass->update_cache (cell, emit_signal);
}
