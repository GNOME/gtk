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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_CELL_ACCESSIBLE_H__
#define __GTK_CELL_ACCESSIBLE_H__

#include <atk/atk.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_ACCESSIBLE                           (_gtk_cell_accessible_get_type ())
#define GTK_CELL_ACCESSIBLE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_ACCESSIBLE, GtkCellAccessible))
#define GTK_CELL_ACCESSIBLE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_ACCESSIBLE, GtkCellAccessibleClass))
#define GTK_IS_CELL_ACCESSIBLE(obj)                        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_ACCESSIBLE))
#define GTK_IS_CELL_ACCESSIBLE_CLASS(klass)                (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_ACCESSIBLE))
#define GTK_CELL_ACCESSIBLE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_ACCESSIBLE, GtkCellAccessibleClass))

typedef struct _GtkCellAccessible      GtkCellAccessible;
typedef struct _GtkCellAccessibleClass GtkCellAccessibleClass;

struct _GtkCellAccessible
{
  AtkObject parent;

  GtkWidget    *widget;
  /* This cached value is used only by atk_object_get_index_in_parent()
   * which updates the value when it is stale.
   */
  gint         index;
  AtkStateSet *state_set;
  GList       *action_list;
  void (*refresh_index) (GtkCellAccessible *cell);
};

struct _GtkCellAccessibleClass
{
  AtkObjectClass parent_class;
};

GType    _gtk_cell_accessible_get_type      (void);

void     _gtk_cell_accessible_initialise    (GtkCellAccessible *cell,
                                             GtkWidget         *widget,
                                             AtkObject         *parent,
                                             gint               index);
gboolean _gtk_cell_accessible_add_state     (GtkCellAccessible *cell,
                                             AtkStateType       state_type,
                                             gboolean           emit_signal);
gboolean _gtk_cell_accessible_remove_state  (GtkCellAccessible *cell,
                                             AtkStateType       state_type,
                                             gboolean           emit_signal);
gboolean _gtk_cell_accessible_add_action    (GtkCellAccessible *cell,
                                             const gchar       *name,
                                             const gchar       *description,
                                             const gchar       *keybinding,
                                             void (*func) (GtkCellAccessible *));

gboolean _gtk_cell_accessible_remove_action (GtkCellAccessible *cell,
                                             gint               index);
gboolean _gtk_cell_accessible_remove_action_by_name
                                            (GtkCellAccessible *cell,
                                             const gchar       *name);

G_END_DECLS

#endif /* __GTK_CELL_ACCESSIBLE_H__ */
