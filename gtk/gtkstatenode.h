/*
 * Copyright © Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtkrestorable.h>
#include <gtk/gtkexpression.h>

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

G_BEGIN_DECLS

#define GTK_TYPE_STATE_NODE (gtk_state_node_get_type ())

GDK_AVAILABLE_IN_4_24
G_DECLARE_FINAL_TYPE (GtkStateNode,
                      gtk_state_node,
                      GTK, STATE_NODE,
                      GObject)

GDK_AVAILABLE_IN_4_24
GtkStateNode *gtk_state_node_new (void);

/**
 * GtkStateNodeBindingFlags:
 * @GTK_STATE_NODE_BINDING_FLAGS_NONE: No flags
 *
 * Flags to use when creating `GtkStateNode` bindings
 *
 * Since: 4.24
 */
/**
 * GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY:
 *
 * Only collect state from this binding, and never restore state from it.
 *
 * This is primarily useful in cases where apps need to "tag" their state with
 * a recognizable name. For instance: an app may have multiple different types
 * of windows, and may want to insert some static identifier into the save
 * state so that it can know which window type to construct at restore time.
 *
 * More generally, there may be situations where apps will want to manually
 * deal with state that has been collected into the state node for them. This
 * flag faciliates that.
 *
 * Bindings to a `GtkConstantExpression` will automatically imply this flag.
 * This flag cannot be combined with `GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ONLY`.
 */
/**
 * GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ONLY:
 *
 * Only restore state into this binding, and never collect state from it.
 *
 * This is a useful way for apps to deal with changes to the shape of their
 * state tree after updates. Apps should be able to load old state saves after
 * an update, and restore-only bindings allow the app to load state from the
 * previous format but then stop collecting state in that format going forwards.
 *
 * This flag cannot be combined with `GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY`.
 */
/**
 * GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ALWAYS:
 *
 * Restore from this binding even when the restore reason is only
 * `GTK_RESTORE_REASON_LAUNCH`. By default, bindings are not restored for
 * normal launches of the app.
 *
 * This flag cannot be combined with `GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY`.
 */
typedef enum {
  GTK_STATE_NODE_BINDING_FLAGS_NONE = 0,
  GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY = 1 << 0,
  GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ONLY = 1 << 1,
  GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ALWAYS = 1 << 2,
} GtkStateNodeBindingFlags;

GDK_AVAILABLE_IN_4_24
void gtk_state_node_bind_expression (GtkStateNode             *self,
                                     const char               *key,
                                     GtkExpression            *source,
                                     GtkStateNodeBindingFlags  flags);

GDK_AVAILABLE_IN_4_24
void gtk_state_node_bind (GtkStateNode             *self,
                          const char               *key,
                          GtkRestorable            *source,
                          GtkStateNodeBindingFlags  flags);

GDK_AVAILABLE_IN_4_24
void gtk_state_node_bind_property (GtkStateNode *self,
                                   const char *key,
                                   GObject *object,
                                   const char *name,
                                   GtkStateNodeBindingFlags flags);

GDK_AVAILABLE_IN_4_24
void gtk_state_node_bind_constant (GtkStateNode             *self,
                                   const char               *key,
                                   GtkStateNodeBindingFlags  flags,
                                   GType                     type,
                                   ...);

GDK_AVAILABLE_IN_4_24
gboolean gtk_state_node_unbind (GtkStateNode *self,
                                const char   *key);

G_END_DECLS
