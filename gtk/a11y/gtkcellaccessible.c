/* GAIL - The GNOME Accessibility Implementation Library
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

#include <gtk/gtk.h>
#include "gtkcontainercellaccessible.h"
#include "gtkcellaccessible.h"
#include "gtkcellaccessibleparent.h"

typedef struct _ActionInfo ActionInfo;
struct _ActionInfo {
  gchar *name;
  gchar *description;
  gchar *keybinding;
  void (*do_action_func) (GtkCellAccessible *cell);
};


static void atk_action_interface_init    (AtkActionIface    *iface);
static void atk_component_interface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkCellAccessible, _gtk_cell_accessible, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init))

static void
destroy_action_info (gpointer action_info)
{
  ActionInfo *info = (ActionInfo *)action_info;

  g_free (info->name);
  g_free (info->description);
  g_free (info->keybinding);
  g_free (info);
}

static void
gtk_cell_accessible_object_finalize (GObject *obj)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (obj);
  AtkRelationSet *relation_set;
  AtkRelation *relation;
  GPtrArray *target;
  gpointer target_object;
  gint i;

  if (cell->state_set)
    g_object_unref (cell->state_set);

  if (cell->action_list)
    g_list_free_full (cell->action_list, destroy_action_info);

  relation_set = atk_object_ref_relation_set (ATK_OBJECT (obj));
  if (ATK_IS_RELATION_SET (relation_set))
    {
      relation = atk_relation_set_get_relation_by_type (relation_set,
                                                        ATK_RELATION_NODE_CHILD_OF);
      if (relation)
        {
          target = atk_relation_get_target (relation);
          for (i = 0; i < target->len; i++)
            {
              target_object = g_ptr_array_index (target, i);
              if (GTK_IS_CELL_ACCESSIBLE (target_object))
                g_object_unref (target_object);
            }
        }
      g_object_unref (relation_set);
    }
  G_OBJECT_CLASS (_gtk_cell_accessible_parent_class)->finalize (obj);
}

static gint
gtk_cell_accessible_get_index_in_parent (AtkObject *obj)
{
  GtkCellAccessible *cell;

  cell = GTK_CELL_ACCESSIBLE (obj);
  if (atk_state_set_contains_state (cell->state_set, ATK_STATE_STALE) &&
      cell->refresh_index != NULL)
    {
      cell->refresh_index (cell);
      atk_state_set_remove_state (cell->state_set, ATK_STATE_STALE);
    }

  return cell->index;
}

static AtkStateSet *
gtk_cell_accessible_ref_state_set (AtkObject *obj)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (obj);

  g_object_ref (cell->state_set);

  return cell->state_set;
}

static void
_gtk_cell_accessible_class_init (GtkCellAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = gtk_cell_accessible_object_finalize;

  class->get_index_in_parent = gtk_cell_accessible_get_index_in_parent;
  class->ref_state_set = gtk_cell_accessible_ref_state_set;
}

static void
_gtk_cell_accessible_init (GtkCellAccessible *cell)
{
  cell->widget = NULL;
  cell->action_list = NULL;
  cell->index = 0;
  cell->refresh_index = NULL;
  cell->state_set = atk_state_set_new ();
  atk_state_set_add_state (cell->state_set, ATK_STATE_TRANSIENT);
  atk_state_set_add_state (cell->state_set, ATK_STATE_ENABLED);
  atk_state_set_add_state (cell->state_set, ATK_STATE_SENSITIVE);
  atk_state_set_add_state (cell->state_set, ATK_STATE_SELECTABLE);
}

static void
widget_destroyed (GtkWidget         *widget,
                  GtkCellAccessible *cell)
{
  cell->widget = NULL;
}

void
_gtk_cell_accessible_initialise (GtkCellAccessible *cell,
                                 GtkWidget         *widget,
                                 AtkObject         *parent,
                                 gint               index)
{
  cell->widget = widget;
  atk_object_set_parent (ATK_OBJECT (cell), parent);
  cell->index = index;

  g_signal_connect_object (G_OBJECT (widget), "destroy",
                           G_CALLBACK (widget_destroyed), cell, 0);
}

gboolean
_gtk_cell_accessible_add_state (GtkCellAccessible *cell,
                                AtkStateType       state_type,
                                gboolean           emit_signal)
{
  gboolean rc;
  AtkObject *parent;

  if (atk_state_set_contains_state (cell->state_set, state_type))
    return FALSE;

  rc = atk_state_set_add_state (cell->state_set, state_type);

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
  parent = atk_object_get_parent (ATK_OBJECT (cell));
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (parent), state_type, emit_signal);

  return rc;
}

gboolean
_gtk_cell_accessible_remove_state (GtkCellAccessible *cell,
                                   AtkStateType       state_type,
                                   gboolean           emit_signal)
{
  gboolean rc;
  AtkObject *parent;

  if (!atk_state_set_contains_state (cell->state_set, state_type))
    return FALSE;

  parent = atk_object_get_parent (ATK_OBJECT (cell));

  rc = atk_state_set_remove_state (cell->state_set, state_type);

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
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (parent), state_type, emit_signal);

  return rc;
}

gboolean
_gtk_cell_accessible_add_action (GtkCellAccessible *cell,
                                 const gchar       *name,
                                 const gchar       *description,
                                 const gchar       *keybinding,
                                 void (*func) (GtkCellAccessible *))
{
  ActionInfo *info;

  info = g_new (ActionInfo, 1);
  info->name = g_strdup (name);
  info->description = g_strdup (description);
  info->keybinding = g_strdup (keybinding);
  info->do_action_func = func;

  cell->action_list = g_list_append (cell->action_list, info);

  return TRUE;
}

gboolean
_gtk_cell_accessible_remove_action (GtkCellAccessible *cell,
                                    gint               index)
{
  GList *l;

  l = g_list_nth (cell->action_list, index);
  if (l == NULL)
    return FALSE;

  destroy_action_info (l->data);
  cell->action_list = g_list_remove_link (cell->action_list, l);

  return TRUE;
}


gboolean
_gtk_cell_accessible_remove_action_by_name (GtkCellAccessible *cell,
                                            const gchar       *name)
{
  GList *l;

  for (l = cell->action_list; l; l = l->next)
    {
      ActionInfo *info = l->data;

      if (g_strcmp0 (info->name, name) == 0)
        break;
    }

  if (l == NULL)
    return FALSE;

  destroy_action_info (l->data);
  cell->action_list = g_list_remove_link (cell->action_list, l);

  return TRUE;
}

static ActionInfo *
get_action_info (GtkCellAccessible *cell,
                 gint               index)
{
  GList *l;

  l = g_list_nth (cell->action_list, index);
  if (l == NULL)
    return NULL;

  return (ActionInfo *) (l->data);
}

static gint
gtk_cell_accessible_action_get_n_actions (AtkAction *action)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE(action);
  if (cell->action_list != NULL)
    return g_list_length (cell->action_list);
  else
    return 0;
}

static const gchar *
gtk_cell_accessible_action_get_name (AtkAction *action,
                                     gint       index)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (action);
  ActionInfo *info;

  info = get_action_info (cell, index);
  if (info == NULL)
    return NULL;

  return info->name;
}

static const gchar *
gtk_cell_accessible_action_get_description (AtkAction *action,
                                            gint       index)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (action);
  ActionInfo *info;

  info = get_action_info (cell, index);
  if (info == NULL)
    return NULL;

  return info->description;
}

static const gchar *
gtk_cell_accessible_action_get_keybinding (AtkAction *action,
                                           gint       index)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (action);
  ActionInfo *info;

  info = get_action_info (cell, index);
  if (info == NULL)
    return NULL;

  return info->keybinding;
}

static gboolean
gtk_cell_accessible_action_do_action (AtkAction *action,
                                      gint       index)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (action);
  ActionInfo *info;

  info = get_action_info (cell, index);
  if (info == NULL)
    return FALSE;

  if (info->do_action_func == NULL)
    return FALSE;

  info->do_action_func (cell);

  return TRUE;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->get_n_actions = gtk_cell_accessible_action_get_n_actions;
  iface->do_action = gtk_cell_accessible_action_do_action;
  iface->get_name = gtk_cell_accessible_action_get_name;
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
  parent = gtk_widget_get_accessible (cell->widget);

  _gtk_cell_accessible_parent_get_cell_extents (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                                cell,
                                                x, y, width, height, coord_type);
}

static gboolean
gtk_cell_accessible_grab_focus (AtkComponent *component)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (component);
  parent = gtk_widget_get_accessible (cell->widget);

  return _gtk_cell_accessible_parent_grab_focus (GTK_CELL_ACCESSIBLE_PARENT (parent), cell);
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  iface->get_extents = gtk_cell_accessible_get_extents;
  iface->grab_focus = gtk_cell_accessible_grab_focus;
}
