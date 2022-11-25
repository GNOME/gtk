/* gtkaccessible.h: Accessible interface
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <glib-object.h>
#include <gtk/gtktypes.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACCESSIBLE (gtk_accessible_get_type())

GDK_AVAILABLE_IN_4_10
G_DECLARE_INTERFACE (GtkAccessible, gtk_accessible, GTK, ACCESSIBLE, GObject)

/**
 * GtkAccessiblePlatformState:
 * @GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE: whether the accessible can be focused
 * @GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED: whether the accessible has focus
 * @GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE: whether the accessible is active
 * 
 * The various platform states which can be queried
 * using [method@Gtk.Accessible.get_platform_state].
 *
 * Since: 4.10
 */
typedef enum {
  GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE,
  GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED,
  GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE
} GtkAccessiblePlatformState;

/** < private >
 * GtkAccessiblePlatformChange:
 * @GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSABLE: whether the accessible has changed
 *   its focusable state
 * @GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSED: whether the accessible has changed its
 *   focused state
 * @GTK_ACCESSIBLE_PLATFORM_CHANGE_ACTIVE: whether the accessible has changed its
 *   active state
 *
 * Represents the various platform changes which can occur and are communicated
 * using [method@Gtk.Accessible.platform_changed].
 */
typedef enum {
  GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSABLE = 1 << GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE,
  GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSED   = 1 << GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED,
  GTK_ACCESSIBLE_PLATFORM_CHANGE_ACTIVE    = 1 << GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE,
} GtkAccessiblePlatformChange;

struct _GtkAccessibleInterface
{
  GTypeInterface g_iface;

  /**
   * GtkAccessibleInterface::get_at_context:
   * @self: a `GtkAccessible`
   * 
   * Retrieves the `GtkATContext` for this accessible implementation.
   */
  GtkATContext *        (* get_at_context)      (GtkAccessible *self);

  /**
   * GtkAccessibleInterface::get_platform_state:
   * 
   * @self: A `GtkAccessible`
   * @state: The state to retrieve
   * 
   * Returns whether @self has the given platform state.
   * @returns: %true if @state is active for @self.
   * @returns:
   */
  gboolean              (* get_platform_state)  (GtkAccessible              *self,
                                                 GtkAccessiblePlatformState  state);

  /**
   * GtkAccessibleInterface::get_accessible_parent:
   * @self: a `GtkAccessible`
   * 
   * Returns the parent `GtkAccessible` of @self.
   * A return value of %NULL implies that the accessible represents a top-level
   * accessible object, which currently also implies that the accessible
   * represents something which is in the list of top-level widgets.
   */
  GtkAccessible *       (* get_accessible_parent)  (GtkAccessible *self);
  
  /**
   * GtkaccessibleInterface::get_first_accessible_child:
   * @self: a `GtkAccessible`
   * 
   * Returns the first accessible child of @self, or %NULL if @self has none
   */
  GtkAccessible *       (* get_first_accessible_child)  (GtkAccessible *self);


  /**
   * GtkaccessibleInterface::get_next_accessible_sibling:
   * @self: a `GtkAccessible`
   * 
   * Returns the next accessible sibling of @self, or %NULL if @self has none
   */
  GtkAccessible *       (* get_next_accessible_sibling)  (GtkAccessible *self);

  /**
   * GtkAccessibleInterface::get_bounds:
   * @self: a `GtkAccessible`
   * @x: an addres of the x coordinate
   * @y: an address of the y coordinate
   * @width: address of the width
   * @height: address of the height
   * 
   * Returns the dimensions and position of @self, if this information
   * can be determined.
   * @returns: %true if the values are valid, %false otherwise
   */
  gboolean              (* get_bounds) (GtkAccessible *self, int *x, int *y,
                                        int *width, int *height);
};

GDK_AVAILABLE_IN_ALL
GtkATContext *  gtk_accessible_get_at_context   (GtkAccessible *self);

GDK_AVAILABLE_IN_4_10
gboolean        gtk_accessible_get_platform_state (GtkAccessible              *self,
                                                   GtkAccessiblePlatformState  state);

GDK_AVAILABLE_IN_4_10
GtkAccessible * gtk_accessible_get_accessible_parent(GtkAccessible *self);

GDK_AVAILABLE_IN_4_10
GtkAccessible * gtk_accessible_get_first_accessible_child(GtkAccessible *self);

GDK_AVAILABLE_IN_4_10
GtkAccessible * gtk_accessible_get_next_accessible_sibling(GtkAccessible *self);

GDK_AVAILABLE_IN_4_10
gboolean gtk_accessible_get_bounds (GtkAccessible *self, int *x, int *y, int *width, int *height);

GDK_AVAILABLE_IN_ALL
GtkAccessibleRole       gtk_accessible_get_accessible_role      (GtkAccessible         *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_update_state             (GtkAccessible         *self,
                                                                 GtkAccessibleState     first_state,
                                                                 ...);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_update_property          (GtkAccessible         *self,
                                                                 GtkAccessibleProperty  first_property,
                                                                 ...);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_update_relation          (GtkAccessible         *self,
                                                                 GtkAccessibleRelation  first_relation,
                                                                 ...);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_update_state_value       (GtkAccessible         *self,
                                                                 int                    n_states,
                                                                 GtkAccessibleState     states[],
                                                                 const GValue           values[]);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_update_property_value    (GtkAccessible         *self,
                                                                 int                    n_properties,
                                                                 GtkAccessibleProperty  properties[],
                                                                 const GValue           values[]);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_update_relation_value    (GtkAccessible         *self,
                                                                 int                    n_relations,
                                                                 GtkAccessibleRelation  relations[],
                                                                 const GValue           values[]);

GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_reset_state              (GtkAccessible         *self,
                                                                 GtkAccessibleState     state);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_reset_property           (GtkAccessible         *self,
                                                                 GtkAccessibleProperty  property);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_reset_relation           (GtkAccessible         *self,
                                                                 GtkAccessibleRelation  relation);

GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_state_init_value         (GtkAccessibleState     state,
                                                                 GValue                *value);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_property_init_value      (GtkAccessibleProperty  property,
                                                                 GValue                *value);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_relation_init_value      (GtkAccessibleRelation  relation,
                                                                 GValue                *value);

G_END_DECLS
