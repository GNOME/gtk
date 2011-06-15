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

#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gailcontainercell.h"
#include "gailcell.h"
#include "gailcellparent.h"

static void	    gail_cell_class_init          (GailCellClass       *klass);
static void         gail_cell_destroyed           (GtkWidget           *widget,
                                                   GailCell            *cell);

static void         gail_cell_init                (GailCell            *cell);
static void         gail_cell_object_finalize     (GObject             *cell);
static AtkStateSet* gail_cell_ref_state_set       (AtkObject           *obj);
static gint         gail_cell_get_index_in_parent (AtkObject           *obj);

/* AtkAction */

static void         atk_action_interface_init 
                                                  (AtkActionIface      *iface);
static ActionInfo * _gail_cell_get_action_info    (GailCell            *cell,
			                           gint                index);
static void         _gail_cell_destroy_action_info 
                                                  (gpointer            data,
				                   gpointer            user_data);

static gint         gail_cell_action_get_n_actions 
                                                  (AtkAction           *action);
static const gchar *
                    gail_cell_action_get_name     (AtkAction           *action,
			                           gint                index);
static const gchar *
                    gail_cell_action_get_description 
                                                  (AtkAction           *action,
				                   gint                index);
static gboolean     gail_cell_action_set_description 
                                                  (AtkAction           *action,
				                   gint                index,
				                   const gchar         *desc);
static const gchar *
                    gail_cell_action_get_keybinding 
                                                  (AtkAction           *action,
				                   gint                index);
static gboolean     gail_cell_action_do_action    (AtkAction           *action,
			                           gint                index);
static gboolean     idle_do_action                (gpointer            data);

static void         atk_component_interface_init  (AtkComponentIface   *iface);
static void         gail_cell_get_extents         (AtkComponent        *component,
                                                   gint                *x,
                                                   gint                *y,
                                                   gint                *width,
                                                   gint                *height,
                                                   AtkCoordType        coord_type);
static gboolean     gail_cell_grab_focus         (AtkComponent        *component);

G_DEFINE_TYPE_WITH_CODE (GailCell, gail_cell, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init))

static void	 
gail_cell_class_init (GailCellClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = gail_cell_object_finalize;

  class->get_index_in_parent = gail_cell_get_index_in_parent;
  class->ref_state_set = gail_cell_ref_state_set;
}

void 
gail_cell_initialise (GailCell  *cell,
                      GtkWidget *widget,
                      AtkObject *parent,
                      gint      index)
{
  g_return_if_fail (GAIL_IS_CELL (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  cell->widget = widget;
  atk_object_set_parent (ATK_OBJECT (cell), parent);
  cell->index = index;

  g_signal_connect_object (G_OBJECT (widget),
                           "destroy",
                           G_CALLBACK (gail_cell_destroyed ),
                           cell, 0);
}

static void
gail_cell_destroyed (GtkWidget       *widget,
                     GailCell        *cell)
{
  /*
   * This is the signal handler for the "destroy" signal for the 
   * GtkWidget. We set the  pointer location to NULL;
   */
  cell->widget = NULL;
}

static void
gail_cell_init (GailCell *cell)
{
  cell->state_set = atk_state_set_new ();
  cell->widget = NULL;
  cell->action_list = NULL;
  cell->index = 0;
  atk_state_set_add_state (cell->state_set, ATK_STATE_TRANSIENT);
  atk_state_set_add_state (cell->state_set, ATK_STATE_ENABLED);
  atk_state_set_add_state (cell->state_set, ATK_STATE_SENSITIVE);
  atk_state_set_add_state (cell->state_set, ATK_STATE_SELECTABLE);
  cell->refresh_index = NULL;
}

static void
gail_cell_object_finalize (GObject *obj)
{
  GailCell *cell = GAIL_CELL (obj);
  AtkRelationSet *relation_set;
  AtkRelation *relation;
  GPtrArray *target;
  gpointer target_object;
  gint i;

  if (cell->state_set)
    g_object_unref (cell->state_set);
  if (cell->action_list)
    {
      g_list_foreach (cell->action_list, _gail_cell_destroy_action_info, NULL);
      g_list_free (cell->action_list);
    }
  if (cell->action_idle_handler)
    {
      g_source_remove (cell->action_idle_handler);
      cell->action_idle_handler = 0;
    }
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
              if (GAIL_IS_CELL (target_object))
                {
                  g_object_unref (target_object);
                }
            }
        }
      g_object_unref (relation_set);
    }
  G_OBJECT_CLASS (gail_cell_parent_class)->finalize (obj);
}

static AtkStateSet *
gail_cell_ref_state_set (AtkObject *obj)
{
  GailCell *cell = GAIL_CELL (obj);
  g_assert (cell->state_set);

  g_object_ref(cell->state_set);
  return cell->state_set;
}

gboolean
gail_cell_add_state (GailCell     *cell, 
                     AtkStateType state_type,
                     gboolean     emit_signal)
{
  if (!atk_state_set_contains_state (cell->state_set, state_type))
    {
      gboolean rc;
      AtkObject *parent;
    
      rc = atk_state_set_add_state (cell->state_set, state_type);
      /*
       * The signal should only be generated if the value changed,
       * not when the cell is set up.  So states that are set
       * initially should pass FALSE as the emit_signal argument.
       */

      if (emit_signal)
        {
          atk_object_notify_state_change (ATK_OBJECT (cell), state_type, TRUE);
          /* If state_type is ATK_STATE_VISIBLE, additional notification */
          if (state_type == ATK_STATE_VISIBLE)
            g_signal_emit_by_name (cell, "visible_data_changed");
        }

      /* 
       * If the parent is a flyweight container cell, propagate the state 
       * change to it also 
       */

      parent = atk_object_get_parent (ATK_OBJECT (cell));
      if (GAIL_IS_CONTAINER_CELL (parent))
        gail_cell_add_state (GAIL_CELL (parent), state_type, emit_signal);
      return rc;
    }
  else
    return FALSE;
}

gboolean
gail_cell_remove_state (GailCell     *cell, 
                        AtkStateType state_type,
                        gboolean     emit_signal)
{
  if (atk_state_set_contains_state (cell->state_set, state_type))
    {
      gboolean rc;
      AtkObject *parent;

      parent = atk_object_get_parent (ATK_OBJECT (cell));

      rc = atk_state_set_remove_state (cell->state_set, state_type);
      /*
       * The signal should only be generated if the value changed,
       * not when the cell is set up.  So states that are set
       * initially should pass FALSE as the emit_signal argument.
       */

      if (emit_signal)
        {
          atk_object_notify_state_change (ATK_OBJECT (cell), state_type, FALSE);
          /* If state_type is ATK_STATE_VISIBLE, additional notification */
          if (state_type == ATK_STATE_VISIBLE)
            g_signal_emit_by_name (cell, "visible_data_changed");
        }

      /* 
       * If the parent is a flyweight container cell, propagate the state 
       * change to it also 
       */

      if (GAIL_IS_CONTAINER_CELL (parent))
        gail_cell_remove_state (GAIL_CELL (parent), state_type, emit_signal);
      return rc;
    }
  else
    return FALSE;
}

static gint
gail_cell_get_index_in_parent (AtkObject *obj)
{
  GailCell *cell;

  g_assert (GAIL_IS_CELL (obj));

  cell = GAIL_CELL (obj);
  if (atk_state_set_contains_state (cell->state_set, ATK_STATE_STALE))
    if (cell->refresh_index)
      {
        cell->refresh_index (cell);
        atk_state_set_remove_state (cell->state_set, ATK_STATE_STALE);
      }
  return cell->index;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->get_n_actions = gail_cell_action_get_n_actions;
  iface->do_action = gail_cell_action_do_action;
  iface->get_name = gail_cell_action_get_name;
  iface->get_description = gail_cell_action_get_description;
  iface->set_description = gail_cell_action_set_description;
  iface->get_keybinding = gail_cell_action_get_keybinding;
}

/*
 * Deprecated: 2.22: The action interface is added for all cells now.
 */
void
gail_cell_type_add_action_interface (GType type)
{
}

gboolean
gail_cell_add_action (GailCell    *cell,
		      const gchar *action_name,
		      const gchar *action_description,
		      const gchar *action_keybinding,
		      ACTION_FUNC action_func)
{
  ActionInfo *info;
  g_return_val_if_fail (GAIL_IS_CELL (cell), FALSE);
  info = g_new (ActionInfo, 1);

  if (action_name != NULL)
    info->name = g_strdup (action_name);
  else
    info->name = NULL;
  if (action_description != NULL)
    info->description = g_strdup (action_description);
  else
    info->description = NULL;
  if (action_keybinding != NULL)
    info->keybinding = g_strdup (action_keybinding);
  else
    info->keybinding = NULL;
  info->do_action_func = action_func;

  cell->action_list = g_list_append (cell->action_list, (gpointer) info);
  return TRUE;
}

gboolean
gail_cell_remove_action (GailCell *cell,
			 gint     action_index)
{
  GList *list_node;

  g_return_val_if_fail (GAIL_IS_CELL (cell), FALSE);
  list_node = g_list_nth (cell->action_list, action_index);
  if (!list_node)
    return FALSE;
  _gail_cell_destroy_action_info (list_node->data, NULL);
  cell->action_list = g_list_remove_link (cell->action_list, list_node);
  return TRUE;
}


gboolean
gail_cell_remove_action_by_name (GailCell    *cell,
				 const gchar *action_name)
{
  GList *list_node;
  gboolean action_found= FALSE;
  
  g_return_val_if_fail (GAIL_IS_CELL (cell), FALSE);
  for (list_node = cell->action_list; list_node && !action_found; 
                    list_node = list_node->next)
    {
      if (!strcmp (((ActionInfo *)(list_node->data))->name, action_name))
	{
	  action_found = TRUE;
	  break;
	}
    }
  if (!action_found)
    return FALSE;
  _gail_cell_destroy_action_info (list_node->data, NULL);
  cell->action_list = g_list_remove_link (cell->action_list, list_node);
  return TRUE;
}

static ActionInfo *
_gail_cell_get_action_info (GailCell *cell,
			    gint     index)
{
  GList *list_node;

  g_return_val_if_fail (GAIL_IS_CELL (cell), NULL);
  if (cell->action_list == NULL)
    return NULL;
  list_node = g_list_nth (cell->action_list, index);
  if (!list_node)
    return NULL;
  return (ActionInfo *) (list_node->data);
}


static void
_gail_cell_destroy_action_info (gpointer action_info, 
                                gpointer user_data)
{
  ActionInfo *info = (ActionInfo *)action_info;
  g_assert (info != NULL);
  g_free (info->name);
  g_free (info->description);
  g_free (info->keybinding);
  g_free (info);
}
static gint
gail_cell_action_get_n_actions (AtkAction *action)
{
  GailCell *cell = GAIL_CELL(action);
  if (cell->action_list != NULL)
    return g_list_length (cell->action_list);
  else
    return 0;
}

static const gchar *
gail_cell_action_get_name (AtkAction *action,
			   gint      index)
{
  GailCell *cell = GAIL_CELL(action);
  ActionInfo *info = _gail_cell_get_action_info (cell, index);

  if (info == NULL)
    return NULL;
  return info->name;
}

static const gchar *
gail_cell_action_get_description (AtkAction *action,
				  gint      index)
{
  GailCell *cell = GAIL_CELL(action);
  ActionInfo *info = _gail_cell_get_action_info (cell, index);

  if (info == NULL)
    return NULL;
  return info->description;
}

static gboolean
gail_cell_action_set_description (AtkAction   *action,
				  gint        index,
				  const gchar *desc)
{
  GailCell *cell = GAIL_CELL(action);
  ActionInfo *info = _gail_cell_get_action_info (cell, index);

  if (info == NULL)
    return FALSE;
  g_free (info->description);
  info->description = g_strdup (desc);
  return TRUE;
}

static const gchar *
gail_cell_action_get_keybinding (AtkAction *action,
				 gint      index)
{
  GailCell *cell = GAIL_CELL(action);
  ActionInfo *info = _gail_cell_get_action_info (cell, index);
  if (info == NULL)
    return NULL;
  return info->keybinding;
}

static gboolean
gail_cell_action_do_action (AtkAction *action,
			    gint      index)
{
  GailCell *cell = GAIL_CELL(action);
  ActionInfo *info = _gail_cell_get_action_info (cell, index);
  if (info == NULL)
    return FALSE;
  if (info->do_action_func == NULL)
    return FALSE;
  if (cell->action_idle_handler)
    return FALSE;
  cell->action_func = info->do_action_func;
  cell->action_idle_handler = gdk_threads_add_idle (idle_do_action, cell);
  return TRUE;
}

static gboolean
idle_do_action (gpointer data)
{
  GailCell *cell;

  cell = GAIL_CELL (data);
  cell->action_idle_handler = 0;
  cell->action_func (cell);

  return FALSE;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  iface->get_extents = gail_cell_get_extents;
  iface->grab_focus = gail_cell_grab_focus;
}

static void 
gail_cell_get_extents (AtkComponent *component,
                       gint         *x,
                       gint         *y,
                       gint         *width,
                       gint         *height,
                       AtkCoordType coord_type)
{
  GailCell *gailcell;
  AtkObject *cell_parent;

  g_assert (GAIL_IS_CELL (component));

  gailcell = GAIL_CELL (component);

  cell_parent = gtk_widget_get_accessible (gailcell->widget);

  gail_cell_parent_get_cell_extents (GAIL_CELL_PARENT (cell_parent), 
                                   gailcell, x, y, width, height, coord_type);
}

static gboolean 
gail_cell_grab_focus (AtkComponent *component)
{
  GailCell *gailcell;
  AtkObject *cell_parent;

  g_assert (GAIL_IS_CELL (component));

  gailcell = GAIL_CELL (component);

  cell_parent = gtk_widget_get_accessible (gailcell->widget);

  return gail_cell_parent_grab_focus (GAIL_CELL_PARENT (cell_parent), 
                                      gailcell);
}
