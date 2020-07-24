/* gtkpasswordentryaccessible.c: A GtkWidgetAccessible for GtkPasswordEntry
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

#include "config.h"

#include "gtkpasswordentryaccessibleprivate.h"

#include "gtkeditable.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkpango.h"
#include "gtkpasswordentryprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtktextprivate.h"

struct _GtkPasswordEntryAccessible
{
  GtkWidgetAccessible parent_instance;

  int cursor_position;
  int selection_bound;
};

static void atk_editable_text_interface_init (AtkEditableTextIface *iface);
static void atk_text_interface_init (AtkTextIface *iface);
static void atk_action_interface_init (AtkActionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPasswordEntryAccessible, gtk_password_entry_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_EDITABLE_TEXT, atk_editable_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static inline GtkText *
get_text_widget (GtkAccessible *accessible)
{
  GtkWidget *widget = gtk_accessible_get_widget (accessible);

  if (widget == NULL)
    return NULL;

  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);

  return gtk_password_entry_get_text_widget (entry);
}

static gunichar
gtk_password_entry_accessible_get_character_at_offset (AtkText *atk_text,
                                                       int      offset)
{
  GtkText *text;
  char *contents, *index;
  gunichar result;

  result = '\0';

  text = get_text_widget (GTK_ACCESSIBLE (atk_text));
  if (text == NULL)
    return 0;

  if (!gtk_text_get_visibility (text))
    return result;

  contents = gtk_text_get_display_text (text, 0, -1);
  if (offset < g_utf8_strlen (contents, -1))
    {
      index = g_utf8_offset_to_pointer (contents, offset);
      result = g_utf8_get_char (index);
      g_free (contents);
    }

  return result;
}

static int
gtk_password_entry_accessible_get_caret_offset (AtkText *atk_text)
{
  GtkWidget *widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));

  if (widget == NULL)
    return -1;

  int cursor_position = 0;
  gboolean result = gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), NULL, &cursor_position);
  if (!result)
    return -1;

  return cursor_position;
}

static gboolean
gtk_password_entry_accessible_set_caret_offset (AtkText *atk_text,
                                                int      offset)
{
  GtkWidget *widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return FALSE;

  gtk_editable_set_position (GTK_EDITABLE (widget), offset);

  return TRUE;
}

static int
gtk_password_entry_accessible_get_character_count (AtkText *atk_text)
{
  GtkText *text = get_text_widget (GTK_ACCESSIBLE (atk_text));
  if (text == NULL)
    return 0;

  char *display_text = gtk_text_get_display_text (text, 0, -1);

  int char_count = 0;
  if (display_text)
    {
      char_count = g_utf8_strlen (display_text, -1);
      g_free (display_text);
    }

  return char_count;
}

static int
gtk_password_entry_accessible_get_offset_at_point (AtkText      *atk_text,
                                                   int           x,
                                                   int           y,
                                                   AtkCoordType  coords)
{
  GtkText *text = get_text_widget (GTK_ACCESSIBLE (atk_text));
  int index, x_layout, y_layout;
  int x_local, y_local;
  glong offset;

  if (text == NULL)
    return 1;

  gtk_text_get_layout_offsets (text, &x_layout, &y_layout);

  x_local = x - x_layout;
  y_local = y - y_layout;

  if (!pango_layout_xy_to_index (gtk_text_get_layout (text),
                                 x_local * PANGO_SCALE,
                                 y_local * PANGO_SCALE,
                                 &index, NULL))
    {
      if (x_local < 0 || y_local < 0)
        index = 0;
      else
        index = -1;
    }

  offset = -1;
  if (index != -1)
    {
      char *entry_text = gtk_text_get_display_text (text, 0, -1);
      offset = g_utf8_pointer_to_offset (entry_text, entry_text + index);
      g_free (entry_text);
    }

  return offset;
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_character_at_offset = gtk_password_entry_accessible_get_character_at_offset;
  iface->get_caret_offset = gtk_password_entry_accessible_get_caret_offset;
  iface->set_caret_offset = gtk_password_entry_accessible_set_caret_offset;
  iface->get_character_count = gtk_password_entry_accessible_get_character_count;
  iface->get_offset_at_point = gtk_password_entry_accessible_get_offset_at_point;
}

static void
gtk_password_entry_accessible_set_text_contents (AtkEditableText *text,
                                                 const char      *string)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  if (!gtk_editable_get_editable (GTK_EDITABLE (widget)))
    return;

  gtk_editable_set_text (GTK_EDITABLE (widget), string);
}

static void
gtk_password_entry_accessible_insert_text (AtkEditableText *text,
                                           const char      *string,
                                           int              length,
                                           int             *position)
{
  GtkWidget *widget;
  GtkEditable *editable;
  int pos = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  if (position != NULL)
    pos = *position;

  gtk_editable_insert_text (editable, string, length, &pos);
  gtk_editable_set_position (editable, pos);

  if (position != NULL)
    *position = pos;
}

static void
gtk_password_entry_accessible_delete_text (AtkEditableText *text,
                                           int              start_pos,
                                           int              end_pos)
{
  GtkWidget *widget;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  gtk_editable_delete_text (editable, start_pos, end_pos);
}

typedef struct
{
  GtkEditable *entry;
  int position;
} PasteData;

static void
paste_received_cb (GObject      *clipboard,
                   GAsyncResult *result,
                   gpointer      data)
{
  PasteData *paste = data;
  char *text;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), result, NULL);
  if (text != NULL)
    gtk_editable_insert_text (paste->entry,
                              text, -1,
                              &(paste->position));

  g_object_unref (paste->entry);
  g_free (paste);
  g_free (text);
}

static void
gtk_password_entry_accessible_paste_text (AtkEditableText *text,
                                          int              position)
{
  GtkWidget *widget;
  GtkEditable *editable;
  PasteData *paste;
  GdkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  paste = g_new0 (PasteData, 1);
  paste->entry = GTK_EDITABLE (widget);
  paste->position = position;

  g_object_ref (paste->entry);
  clipboard = gtk_widget_get_clipboard (widget);
  gdk_clipboard_read_text_async (clipboard, NULL, paste_received_cb, paste);
}

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
  iface->set_text_contents = gtk_password_entry_accessible_set_text_contents;
  iface->insert_text = gtk_password_entry_accessible_insert_text;
  iface->copy_text = NULL;
  iface->cut_text = NULL;
  iface->delete_text = gtk_password_entry_accessible_delete_text;
  iface->paste_text = gtk_password_entry_accessible_paste_text;
  iface->set_run_attributes = NULL;
}

static gboolean
gtk_password_entry_accessible_do_action (AtkAction *action,
                                         int        i)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  if (i == 0)
    {
      gtk_widget_activate (widget);
      return TRUE;
    }

  if (i == 1)
    {
      GtkText *text = get_text_widget (GTK_ACCESSIBLE (action));

      gboolean visibility = gtk_text_get_visibility (text);
      gtk_text_set_visibility (text, !visibility);

      return TRUE;
    }

  return FALSE;
}

static int
gtk_password_entry_accessible_get_n_actions (AtkAction *action)
{
  GtkAccessible *accessible = GTK_ACCESSIBLE (action);

  GtkWidget *widget = gtk_accessible_get_widget (accessible);
  if (widget == NULL)
    return 0;

  int n_actions = 1;
  if (gtk_password_entry_get_show_peek_icon (GTK_PASSWORD_ENTRY (widget)))
    n_actions += 1;

  return n_actions;
}

static const char *
gtk_password_entry_accessible_get_keybinding (AtkAction *action,
                                              int        i)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkRelationSet *set;
  AtkRelation *relation;
  GPtrArray *target;
  gpointer target_object;
  guint key_val;

  if (i != 0)
    return NULL;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return NULL;

  set = atk_object_ref_relation_set (ATK_OBJECT (action));
  if (!set)
    return NULL;

  label = NULL;
  relation = atk_relation_set_get_relation_by_type (set, ATK_RELATION_LABELLED_BY);
  if (relation)
    {
      target = atk_relation_get_target (relation);

      target_object = g_ptr_array_index (target, 0);
      label = gtk_accessible_get_widget (GTK_ACCESSIBLE (target_object));
    }

  g_object_unref (set);

  if (GTK_IS_LABEL (label))
    {
      key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
      if (key_val != GDK_KEY_VoidSymbol)
        return gtk_accelerator_name (key_val, GDK_ALT_MASK);
    }

  return NULL;
}

static const char *
gtk_password_entry_accessible_action_get_name (AtkAction *action,
                                               int        i)
{
  switch (i)
    {
    case 0:
      return "activate";

    case 1:
      return "peek";

    default:
      break;
    }

  return NULL;
}

static const char *
gtk_password_entry_accessible_action_get_localized_name (AtkAction *action,
                                                         int        i)
{
  if (i == 0)
    return C_("Action name", "Activate");
  if (i == 1)
    return C_("Action name", "Peek");

  return NULL;
}

static const char *
gtk_password_entry_accessible_action_get_description (AtkAction *action,
                                                      int        i)
{
  if (i == 0)
    return C_("Action description", "Activates the entry");

  if (i == 1)
    return C_("Action description", "Reveals the contents the entry");

  return NULL;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_password_entry_accessible_do_action;
  iface->get_n_actions = gtk_password_entry_accessible_get_n_actions;
  iface->get_keybinding = gtk_password_entry_accessible_get_keybinding;
  iface->get_name = gtk_password_entry_accessible_action_get_name;
  iface->get_localized_name = gtk_password_entry_accessible_action_get_localized_name;
  iface->get_description = gtk_password_entry_accessible_action_get_description;
}

static AtkAttributeSet *
gtk_password_entry_accessible_get_attributes (AtkObject *accessible)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;
  AtkAttribute *placeholder_text;
  char *text = NULL;

  attributes = ATK_OBJECT_CLASS (gtk_password_entry_accessible_parent_class)->get_attributes (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return attributes;

  g_object_get (widget, "placeholder-text", &text, NULL);
  if (text == NULL)
    return attributes;

  placeholder_text = g_malloc (sizeof (AtkAttribute));
  placeholder_text->name = g_strdup ("placeholder-text");
  placeholder_text->value = text;

  attributes = g_slist_append (attributes, placeholder_text);

  return attributes;
}

static gboolean
check_for_selection_change (GtkPasswordEntryAccessible *self,
                            GtkEditable                *editable)
{
  gboolean ret_val = FALSE;
  int start, end;

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    {
      if (end != self->cursor_position ||
          start != self->selection_bound)
        /*
         * This check is here as this function can be called
         * for notification of selection_bound and current_pos.
         * The values of current_pos and selection_bound may be the same
         * for both notifications and we only want to generate one
         * text_selection_changed signal.
         */
        ret_val = TRUE;
    }
  else
    {
      /* We had a selection */
      ret_val = (self->cursor_position != self->selection_bound);
    }

  self->cursor_position = end;
  self->selection_bound = start;

  return ret_val;
}

static void
on_cursor_position_changed (GObject    *gobject,
                            GParamSpec *pspec,
                            gpointer    data)
{
  GtkPasswordEntryAccessible *self = data;
  GtkEditable *editable = GTK_EDITABLE (gobject);

  if (check_for_selection_change (self, editable))
    g_signal_emit_by_name (self, "text-selection-changed");

  // The entry cursor position has moved so generate the signal
  g_signal_emit_by_name (self, "text-caret-moved",
                         gtk_editable_get_position (editable));
}

static void
on_selection_bound_changed (GObject     *gobject,
                            GParamSpec *pspec,
                            gpointer    data)
{
  GtkPasswordEntryAccessible *self = data;
  GtkEditable *editable = GTK_EDITABLE (gobject);

  if (check_for_selection_change (self, editable))
    g_signal_emit_by_name (self, "text-selection-changed");
}

static void
insert_text_cb (GtkEditable                *editable,
                char                       *new_text,
                int                         new_text_length,
                int                        *position,
                GtkPasswordEntryAccessible *self)
{
  int length;

  if (new_text_length == 0)
    return;

  length = g_utf8_strlen (new_text, new_text_length);

  g_signal_emit_by_name (self,
                         "text-changed::insert",
                         *position - length,
                          length);
}

static void
delete_text_cb (GtkEditable                *editable,
                int                         start,
                int                         end,
                GtkPasswordEntryAccessible *self)
{
  GtkText *text;

  text = get_text_widget (GTK_ACCESSIBLE (self));
  if (text == NULL)
    return;

  if (end < 0)
    {
      char *contents;

      contents = gtk_text_get_display_text (text, 0, -1);
      end = g_utf8_strlen (contents, -1);

      g_free (contents);
    }

  if (end == start)
    return;

  g_signal_emit_by_name (self,
                         "text-changed::delete",
                         start,
                         end - start);
}

static void
gtk_password_entry_accessible_initialize (AtkObject *atk_object,
                                          gpointer   data)
{
  GtkPasswordEntryAccessible *self = GTK_PASSWORD_ENTRY_ACCESSIBLE (atk_object);
  GtkEditable *editable = data;
  GtkWidget *widget = data;
  int start_pos, end_pos;

  gtk_editable_get_selection_bounds (editable, &start_pos, &end_pos);
  self->cursor_position = end_pos;
  self->selection_bound = start_pos;

  /* Set up signal callbacks */
  g_signal_connect_after (widget, "insert-text", G_CALLBACK (insert_text_cb), self);
  g_signal_connect (widget, "delete-text", G_CALLBACK (delete_text_cb), self);
  g_signal_connect (widget, "notify::cursor-position",
                    G_CALLBACK (on_cursor_position_changed), self);
  g_signal_connect (widget, "notify::selection-bound",
                    G_CALLBACK (on_selection_bound_changed), self);
}

static void
gtk_password_entry_accessible_class_init (GtkPasswordEntryAccessibleClass *klass)
{
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

  atk_object_class->initialize = gtk_password_entry_accessible_initialize;
  atk_object_class->get_attributes = gtk_password_entry_accessible_get_attributes;
}

static void
gtk_password_entry_accessible_init (GtkPasswordEntryAccessible *self)
{
  AtkObject *atk_obj = ATK_OBJECT (self);

  atk_obj->role = ATK_ROLE_PASSWORD_TEXT;
  atk_object_set_name (atk_obj, _("Password"));
}

void
gtk_password_entry_accessible_update_visibility (GtkPasswordEntryAccessible *self)
{
  GtkText *text = get_text_widget (GTK_ACCESSIBLE (self));

  if (text == NULL)
    return;

  gboolean visibility = gtk_text_get_visibility (text);
  AtkRole role = visibility ? ATK_ROLE_TEXT : ATK_ROLE_PASSWORD_TEXT;
  atk_object_set_role (ATK_OBJECT (self), role);
}
