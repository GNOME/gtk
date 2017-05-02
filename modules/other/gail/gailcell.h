/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GAIL_CELL_H__
#define __GAIL_CELL_H__

#include <atk/atk.h>

G_BEGIN_DECLS

#define GAIL_TYPE_CELL                           (gail_cell_get_type ())
#define GAIL_CELL(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CELL, GailCell))
#define GAIL_CELL_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_CELL, GailCellClass))
#define GAIL_IS_CELL(obj)                        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CELL))
#define GAIL_IS_CELL_CLASS(klass)                (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_CELL))
#define GAIL_CELL_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_CELL, GailCellClass))

typedef struct _GailCell                  GailCell;
typedef struct _GailCellClass             GailCellClass;
typedef struct _ActionInfo ActionInfo;
typedef void (*ACTION_FUNC) (GailCell *cell);
  
struct _GailCell
{
  AtkObject parent;

  GtkWidget    *widget;
  /*
   * This cached value is used only by atk_object_get_index_in_parent()
   * which updates the value when it is stale.
   */
  gint         index;
  AtkStateSet *state_set;
  GList       *action_list;
  void (*refresh_index) (GailCell *cell);
  gint         action_idle_handler;
  ACTION_FUNC  action_func;
};

GType gail_cell_get_type (void);

struct _GailCellClass
{
  AtkObjectClass parent_class;
};

struct _ActionInfo {
  gchar *name;
  gchar *description;
  gchar *keybinding;
  ACTION_FUNC do_action_func;
};

void      gail_cell_initialise           (GailCell        *cell,
                                          GtkWidget       *widget, 
                                          AtkObject       *parent,
			                  gint            index);

gboolean gail_cell_add_state             (GailCell        *cell,
                                          AtkStateType    state_type,
                                          gboolean        emit_signal);

gboolean gail_cell_remove_state          (GailCell        *cell,
                                          AtkStateType    state_type,
                                          gboolean        emit_signal);

#ifndef GTK_DISABLE_DEPRECATED
void     gail_cell_type_add_action_interface 
                                         (GType           type);
#endif

gboolean gail_cell_add_action            (GailCell        *cell,
		                          const gchar     *action_name,
		                          const gchar     *action_description,
		                          const gchar     *action_keybinding,
		                          ACTION_FUNC     action_func);

gboolean gail_cell_remove_action         (GailCell        *cell,
                                          gint            action_id);

gboolean gail_cell_remove_action_by_name (GailCell        *cell,
                                          const gchar     *action_name);

G_END_DECLS

#endif /* __GAIL_CELL_H__ */
