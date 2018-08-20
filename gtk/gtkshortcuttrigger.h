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

#ifndef __GTK_SHORTCUT_TRIGGER_H__
#define __GTK_SHORTCUT_TRIGGER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_SHORTCUT_TRIGGER (gtk_shortcut_trigger_get_type ())

#define GTK_IS_SHORTCUT_TRIGGER(obj) ((obj) != NULL)

/**
 * GtkShortcutTriggerType:
 * @GTK_SHORTCUT_TRIGGER_NEVER: Never ever trigger
 * @GTK_SHORTCUT_TRIGGER_KEYVAL: Trigger if a key event with matching
 *     modifiers and keyval is received.
 * @GTK_SHORTCUT_TRIGGER_MNEMONIC: Trigger if a key event with matching
 *     keyval is received and mnemonics are enabled for this event.
 * @GTK_SHORTCUT_TRIGGER_ALTERNAITVE: Trigger if either if two
 *     alternatives triggers
 *
 * The type of a trigger determines what the trigger triggers on.
 **/
typedef enum {
  GTK_SHORTCUT_TRIGGER_NEVER,
  GTK_SHORTCUT_TRIGGER_KEYVAL,
  GTK_SHORTCUT_TRIGGER_MNEMONIC,
  GTK_SHORTCUT_TRIGGER_ALTERNATIVE
} GtkShortcutTriggerType;

GDK_AVAILABLE_IN_ALL
GType                   gtk_shortcut_trigger_get_type           (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkShortcutTrigger *    gtk_shortcut_trigger_ref                (GtkShortcutTrigger *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_shortcut_trigger_unref              (GtkShortcutTrigger *self);

GDK_AVAILABLE_IN_ALL
GtkShortcutTriggerType  gtk_shortcut_trigger_get_trigger_type   (GtkShortcutTrigger *self);

GDK_AVAILABLE_IN_ALL
char *                  gtk_shortcut_trigger_to_string          (GtkShortcutTrigger *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_shortcut_trigger_print              (GtkShortcutTrigger *self,
                                                                 GString            *string);
GDK_AVAILABLE_IN_ALL
char *                  gtk_shortcut_trigger_to_label           (GtkShortcutTrigger *self,
                                                                 GdkDisplay         *display);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_shortcut_trigger_print_label        (GtkShortcutTrigger *self,
                                                                 GdkDisplay         *display,
                                                                 GString            *string);

GDK_AVAILABLE_IN_ALL
guint                   gtk_shortcut_trigger_hash               (gconstpointer       trigger);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_shortcut_trigger_equal              (gconstpointer       trigger1,
                                                                 gconstpointer       trigger2);
GDK_AVAILABLE_IN_ALL
gint                    gtk_shortcut_trigger_compare            (gconstpointer       trigger1,
                                                                 gconstpointer       trigger2);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_shortcut_trigger_trigger            (GtkShortcutTrigger *self,
                                                                 const GdkEvent     *event,
                                                                 gboolean            enable_mnemonics);

GDK_AVAILABLE_IN_ALL
GtkShortcutTrigger *    gtk_never_trigger_get                   (void);

GDK_AVAILABLE_IN_ALL
GtkShortcutTrigger *    gtk_keyval_trigger_new                  (guint               keyval,
                                                                 GdkModifierType     modifiers);
GDK_AVAILABLE_IN_ALL
GdkModifierType         gtk_keyval_trigger_get_modifiers        (GtkShortcutTrigger *self);
GDK_AVAILABLE_IN_ALL
guint                   gtk_keyval_trigger_get_keyval           (GtkShortcutTrigger *self);

GDK_AVAILABLE_IN_ALL
GtkShortcutTrigger *    gtk_mnemonic_trigger_new                (guint               keyval);
GDK_AVAILABLE_IN_ALL
guint                   gtk_mnemonic_trigger_get_keyval         (GtkShortcutTrigger *self);

GDK_AVAILABLE_IN_ALL
GtkShortcutTrigger *    gtk_alternative_trigger_new             (GtkShortcutTrigger *one,
                                                                 GtkShortcutTrigger *two);
GDK_AVAILABLE_IN_ALL
GtkShortcutTrigger *    gtk_alternative_trigger_get_first       (GtkShortcutTrigger *trigger);
GDK_AVAILABLE_IN_ALL
GtkShortcutTrigger *    gtk_alternative_trigger_get_second      (GtkShortcutTrigger *trigger);

G_END_DECLS

#endif /* __GTK_SHORTCUT_TRIGGER_H__ */
