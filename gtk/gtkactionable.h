/*
 * Copyright © 2012 Canonical Limited
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __GTK_ACTIONABLE_H__
#define __GTK_ACTIONABLE_H__

#include <glib-object.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACTIONABLE (gtk_actionable_get_type ())
GDK_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GtkActionable, gtk_actionable, GTK, ACTIONABLE, GtkWidget)

struct _GtkActionableInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/

  const gchar * (* get_action_name)             (GtkActionable *actionable);
  void          (* set_action_name)             (GtkActionable *actionable,
                                                 const gchar   *action_name);
  GVariant *    (* get_action_target_value)     (GtkActionable *actionable);
  void          (* set_action_target_value)     (GtkActionable *actionable,
                                                 GVariant      *target_value);
};

GDK_AVAILABLE_IN_ALL
const gchar *           gtk_actionable_get_action_name                  (GtkActionable *actionable);
GDK_AVAILABLE_IN_ALL
void                    gtk_actionable_set_action_name                  (GtkActionable *actionable,
                                                                         const gchar   *action_name);

GDK_AVAILABLE_IN_ALL
GVariant *              gtk_actionable_get_action_target_value          (GtkActionable *actionable);
GDK_AVAILABLE_IN_ALL
void                    gtk_actionable_set_action_target_value          (GtkActionable *actionable,
                                                                         GVariant      *target_value);

GDK_AVAILABLE_IN_ALL
void                    gtk_actionable_set_action_target                (GtkActionable *actionable,
                                                                         const gchar   *format_string,
                                                                         ...);

GDK_AVAILABLE_IN_ALL
void                    gtk_actionable_set_detailed_action_name         (GtkActionable *actionable,
                                                                         const gchar   *detailed_action_name);

G_END_DECLS

#endif /* __GTK_ACTIONABLE_H__ */
