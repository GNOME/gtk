/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkBindingSet: Keybinding manager for GObjects.
 * Copyright (C) 1998 Tim Janik
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_BINDINGS_H__
#define __GTK_BINDINGS_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

typedef struct _GtkBindingSet    GtkBindingSet;

GDK_AVAILABLE_IN_ALL
GtkBindingSet *gtk_binding_set_new           (const gchar         *set_name);
GDK_AVAILABLE_IN_ALL
GtkBindingSet *gtk_binding_set_by_class      (gpointer             object_class);
GDK_AVAILABLE_IN_ALL
GtkBindingSet *gtk_binding_set_find          (const gchar         *set_name);

GDK_AVAILABLE_IN_ALL
gboolean       gtk_bindings_activate         (GObject             *object,
                                              guint                keyval,
                                              GdkModifierType      modifiers);
GDK_AVAILABLE_IN_ALL
gboolean       gtk_bindings_activate_event   (GObject             *object,
                                              GdkEventKey         *event);
GDK_AVAILABLE_IN_ALL
gboolean       gtk_binding_set_activate      (GtkBindingSet       *binding_set,
                                              guint                keyval,
                                              GdkModifierType      modifiers,
                                              GObject             *object);

GDK_AVAILABLE_IN_ALL
void           gtk_binding_entry_skip        (GtkBindingSet       *binding_set,
                                              guint                keyval,
                                              GdkModifierType      modifiers);
GDK_AVAILABLE_IN_ALL
void           gtk_binding_entry_add_signal  (GtkBindingSet       *binding_set,
                                              guint                keyval,
                                              GdkModifierType      modifiers,
                                              const gchar         *signal_name,
                                              guint                n_args,
                                              ...);

GDK_AVAILABLE_IN_ALL
GTokenType     gtk_binding_entry_add_signal_from_string
                                             (GtkBindingSet       *binding_set,
                                              const gchar         *signal_desc);

GDK_AVAILABLE_IN_ALL
void           gtk_binding_entry_remove      (GtkBindingSet       *binding_set,
                                              guint                keyval,
                                              GdkModifierType      modifiers);

G_END_DECLS

#endif /* __GTK_BINDINGS_H__ */
