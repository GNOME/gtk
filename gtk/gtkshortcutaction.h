/*
 * Copyright © 2018 Benjamin Otte
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

#ifndef __GTK_SHORTCUT_ACTION_H__
#define __GTK_SHORTCUT_ACTION_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_SHORTCUT_ACTION (gtk_shortcut_action_get_type ())

#define GTK_IS_SHORTCUT_ACTION(obj) ((obj) != NULL)

/**
 * GtkShortcutFunc:
 * @widget: The widget passed to the activation
 * @args: The arguments passed to the activation
 * @user_data: The user data provided when activating the action
 *
 * Prototype for shortcuts based on user callbacks.
 */
typedef gboolean (* GtkShortcutFunc) (GtkWidget *widget,
                                      GVariant  *args,
                                      gpointer   user_data);

/**
 * GtkShortcutActionFlags:
 * @GTK_SHORTCUT_ACTION_EXCLUSIVE: The action is the only
 *     action that can be activated. If this flag is not set,
 *     a future activation may select a different action.
 *
 * List of flags that can be passed to action activation.
 * More flags may be added in the future.
 **/
typedef enum {
  GTK_SHORTCUT_ACTION_EXCLUSIVE = 1 << 0
} GtkShortcutActionFlags;

/**
 * GtkShortcutActionType:
 * @GTK_SHORTCUT_ACTION_NOTHING: Don't ever activate
 * @GTK_SHORTCUT_ACTION_CALLBACK: Call a custom user-provided callback
 * @GTK_SHORTCUT_ACTION_ACTIVATE: Call gtk_widget_activate() on the widget
 * @GTK_SHORTCUT_ACTION_MNEMONIC: Call gtk_widget_mnemonic_activate()
 *     on the widget
 * @GTK_SHORTCUT_ACTION_SIGNAL: Emit the given action signal on the widget
 * @GTK_SHORTCUT_ACTION_ACTION: Call the provided action on the widget
 * @GTK_SHORTCUT_ACTION_GACTION: Activate a GAction
 *
 * The type of a action determines what the action does when activated.
 **/
typedef enum {
 GTK_SHORTCUT_ACTION_NOTHING,
 GTK_SHORTCUT_ACTION_CALLBACK,
 GTK_SHORTCUT_ACTION_ACTIVATE,
 GTK_SHORTCUT_ACTION_MNEMONIC,
 GTK_SHORTCUT_ACTION_SIGNAL,
 GTK_SHORTCUT_ACTION_ACTION,
 GTK_SHORTCUT_ACTION_GACTION
} GtkShortcutActionType;

GDK_AVAILABLE_IN_ALL
GType                   gtk_shortcut_action_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkShortcutAction *     gtk_shortcut_action_ref                 (GtkShortcutAction      *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_shortcut_action_unref               (GtkShortcutAction      *self);

GDK_AVAILABLE_IN_ALL
GtkShortcutActionType   gtk_shortcut_action_get_action_type     (GtkShortcutAction      *self);

GDK_AVAILABLE_IN_ALL
char *                  gtk_shortcut_action_to_string           (GtkShortcutAction      *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_shortcut_action_print               (GtkShortcutAction      *self,
                                                                 GString                *string);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_shortcut_action_activate            (GtkShortcutAction      *self,
                                                                 GtkShortcutActionFlags  flags,
                                                                 GtkWidget              *widget,
                                                                 GVariant               *args);

GDK_AVAILABLE_IN_ALL
GtkShortcutAction *     gtk_nothing_action_new                  (void);

GDK_AVAILABLE_IN_ALL
GtkShortcutAction *     gtk_callback_action_new                 (GtkShortcutFunc         callback,
                                                                 gpointer                data,
                                                                 GDestroyNotify          destroy);

GDK_AVAILABLE_IN_ALL
GtkShortcutAction *     gtk_mnemonic_action_new                 (void);
GDK_AVAILABLE_IN_ALL
GtkShortcutAction *     gtk_activate_action_new                 (void);

GDK_AVAILABLE_IN_ALL
GtkShortcutAction *     gtk_signal_action_new                   (const char             *signal_name);
GDK_AVAILABLE_IN_ALL
const char *            gtk_signal_action_get_signal_name       (GtkShortcutAction      *action);

GDK_AVAILABLE_IN_ALL
GtkShortcutAction *     gtk_action_action_new                   (const char             *name);
GDK_AVAILABLE_IN_ALL
const char *            gtk_action_action_get_name              (GtkShortcutAction      *action);

GDK_AVAILABLE_IN_ALL
GtkShortcutAction *     gtk_gaction_action_new                  (GAction                *action);
GDK_AVAILABLE_IN_ALL
GAction *               gtk_gaction_action_get_gaction          (GtkShortcutAction      *action);

G_END_DECLS

#endif /* __GTK_SHORTCUT_ACTION_H__ */
