/* GTK+ - accessibility implementations
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include "config.h"

#define GDK_COMPILATION
#include "gdk/gdkeventsprivate.h"

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gtkpango.h"
#include "gtktextaccessible.h"
#include "gtktextprivate.h"
#include "gtkcomboboxaccessible.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

struct _GtkTextAccessiblePrivate
{
  gint cursor_position;
  gint selection_bound;
};

/* Callbacks */

static void     insert_text_cb             (GtkEditable        *editable,
                                            gchar              *new_text,
                                            gint                new_text_length,
                                            gint               *position);
static void     delete_text_cb             (GtkEditable        *editable,
                                            gint                start,
                                            gint                end);

static gboolean check_for_selection_change (GtkTextAccessible *entry,
                                            GtkText           *gtk_text);


static void atk_editable_text_interface_init (AtkEditableTextIface *iface);
static void atk_text_interface_init          (AtkTextIface         *iface);
static void atk_action_interface_init        (AtkActionIface       *iface);


G_DEFINE_TYPE_WITH_CODE (GtkTextAccessible, gtk_text_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkTextAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_EDITABLE_TEXT, atk_editable_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))


static AtkStateSet *
gtk_text_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  gboolean value;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  state_set = ATK_OBJECT_CLASS (gtk_text_accessible_parent_class)->ref_state_set (accessible);

  g_object_get (G_OBJECT (widget), "editable", &value, NULL);
  if (value)
    atk_state_set_add_state (state_set, ATK_STATE_EDITABLE);
  atk_state_set_add_state (state_set, ATK_STATE_SINGLE_LINE);

  return state_set;
}

static AtkAttributeSet *
gtk_text_accessible_get_attributes (AtkObject *accessible)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;
  AtkAttribute *placeholder_text;
  const gchar *text;

  attributes = ATK_OBJECT_CLASS (gtk_text_accessible_parent_class)->get_attributes (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return attributes;

  text = gtk_text_get_placeholder_text (GTK_TEXT (widget));
  if (text == NULL)
    return attributes;

  placeholder_text = g_malloc (sizeof (AtkAttribute));
  placeholder_text->name = g_strdup ("placeholder-text");
  placeholder_text->value = g_strdup (text);

  attributes = g_slist_append (attributes, placeholder_text);

  return attributes;
}

static void
gtk_text_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  GtkText *entry;
  GtkTextAccessible *gtk_text_accessible;
  gint start_pos, end_pos;

  ATK_OBJECT_CLASS (gtk_text_accessible_parent_class)->initialize (obj, data);

  gtk_text_accessible = GTK_TEXT_ACCESSIBLE (obj);

  entry = GTK_TEXT (data);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_pos, &end_pos);
  gtk_text_accessible->priv->cursor_position = end_pos;
  gtk_text_accessible->priv->selection_bound = start_pos;

  /* Set up signal callbacks */
  g_signal_connect_after (entry, "insert-text", G_CALLBACK (insert_text_cb), NULL);
  g_signal_connect (entry, "delete-text", G_CALLBACK (delete_text_cb), NULL);

  if (gtk_text_get_visibility (entry))
    obj->role = ATK_ROLE_TEXT;
  else
    obj->role = ATK_ROLE_PASSWORD_TEXT;
}

static void
gtk_text_accessible_notify_gtk (GObject    *obj,
                                 GParamSpec *pspec)
{
  GtkWidget *widget;
  AtkObject* atk_obj;
  GtkText* gtk_text;
  GtkTextAccessible* entry;

  widget = GTK_WIDGET (obj);
  atk_obj = gtk_widget_get_accessible (widget);
  gtk_text = GTK_TEXT (widget);
  entry = GTK_TEXT_ACCESSIBLE (atk_obj);

  if (g_strcmp0 (pspec->name, "cursor-position") == 0)
    {
      if (check_for_selection_change (entry, gtk_text))
        g_signal_emit_by_name (atk_obj, "text-selection-changed");
      /*
       * The entry cursor position has moved so generate the signal.
       */
      g_signal_emit_by_name (atk_obj, "text-caret-moved",
                             entry->priv->cursor_position);
    }
  else if (g_strcmp0 (pspec->name, "selection-bound") == 0)
    {
      if (check_for_selection_change (entry, gtk_text))
        g_signal_emit_by_name (atk_obj, "text-selection-changed");
    }
  else if (g_strcmp0 (pspec->name, "editable") == 0)
    {
      gboolean value;

      g_object_get (obj, "editable", &value, NULL);
      atk_object_notify_state_change (atk_obj, ATK_STATE_EDITABLE, value);
    }
  else if (g_strcmp0 (pspec->name, "visibility") == 0)
    {
      gboolean visibility;
      AtkRole new_role;

      visibility = gtk_text_get_visibility (gtk_text);
      new_role = visibility ? ATK_ROLE_TEXT : ATK_ROLE_PASSWORD_TEXT;
      atk_object_set_role (atk_obj, new_role);
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_text_accessible_parent_class)->notify_gtk (obj, pspec);
}

static gint
gtk_text_accessible_get_index_in_parent (AtkObject *accessible)
{
  /*
   * If the parent widget is a combo box then the index is 1
   * otherwise do the normal thing.
   */
  if (accessible->accessible_parent)
    if (GTK_IS_COMBO_BOX_ACCESSIBLE (accessible->accessible_parent))
      return 1;

  return ATK_OBJECT_CLASS (gtk_text_accessible_parent_class)->get_index_in_parent (accessible);
}

static void
gtk_text_accessible_class_init (GtkTextAccessibleClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  class->ref_state_set = gtk_text_accessible_ref_state_set;
  class->get_index_in_parent = gtk_text_accessible_get_index_in_parent;
  class->initialize = gtk_text_accessible_initialize;
  class->get_attributes = gtk_text_accessible_get_attributes;

  widget_class->notify_gtk = gtk_text_accessible_notify_gtk;
}

static void
gtk_text_accessible_init (GtkTextAccessible *entry)
{
  entry->priv = gtk_text_accessible_get_instance_private (entry);
  entry->priv->cursor_position = 0;
  entry->priv->selection_bound = 0;
}

static gchar *
gtk_text_accessible_get_text (AtkText *atk_text,
                               gint     start_pos,
                               gint     end_pos)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return NULL;

  return gtk_text_get_display_text (GTK_TEXT (widget), start_pos, end_pos);
}

static gchar *
gtk_text_accessible_get_text_before_offset (AtkText         *text,
                                             gint             offset,
                                             AtkTextBoundary  boundary_type,
                                             gint            *start_offset,
                                             gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_before (gtk_text_get_layout (GTK_TEXT (widget)),
                                     boundary_type, offset,
                                     start_offset, end_offset);
}

static gchar *
gtk_text_accessible_get_text_at_offset (AtkText         *text,
                                         gint             offset,
                                         AtkTextBoundary  boundary_type,
                                         gint            *start_offset,
                                         gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_at (gtk_text_get_layout (GTK_TEXT (widget)),
                                 boundary_type, offset,
                                 start_offset, end_offset);
}

static gchar *
gtk_text_accessible_get_text_after_offset (AtkText         *text,
                                            gint             offset,
                                            AtkTextBoundary  boundary_type,
                                            gint            *start_offset,
                                            gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_after (gtk_text_get_layout (GTK_TEXT (widget)),
                                    boundary_type, offset,
                                    start_offset, end_offset);
}

static gint
gtk_text_accessible_get_character_count (AtkText *atk_text)
{
  GtkWidget *widget;
  gchar *text;
  glong char_count;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return 0;

  text = gtk_text_get_display_text (GTK_TEXT (widget), 0, -1);

  char_count = 0;
  if (text)
    {
      char_count = g_utf8_strlen (text, -1);
      g_free (text);
    }

  return char_count;
}

static gint
gtk_text_accessible_get_caret_offset (AtkText *text)
{
  GtkWidget *widget;
  gboolean result;
  int cursor_position;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return -1;

  result = gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), NULL, &cursor_position);
  if (!result)
    return -1;

  return cursor_position;
}

static gboolean
gtk_text_accessible_set_caret_offset (AtkText *text,
                                       gint     offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  gtk_editable_set_position (GTK_EDITABLE (widget), offset);

  return TRUE;
}

static AtkAttributeSet *
add_text_attribute (AtkAttributeSet  *attributes,
                    AtkTextAttribute  attr,
                    gint              i)
{
  AtkAttribute *at;

  at = g_new (AtkAttribute, 1);
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = g_strdup (atk_text_attribute_get_value (attr, i));

  return g_slist_prepend (attributes, at);
}

static AtkAttributeSet *
gtk_text_accessible_get_run_attributes (AtkText *text,
                                         gint     offset,
                                         gint    *start_offset,
                                         gint    *end_offset)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  attributes = NULL;
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_DIRECTION,
                                   gtk_widget_get_direction (widget));
  attributes = _gtk_pango_get_run_attributes (attributes,
                                              gtk_text_get_layout (GTK_TEXT (widget)),
                                              offset,
                                              start_offset,
                                              end_offset);

  return attributes;
}

static AtkAttributeSet *
gtk_text_accessible_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  attributes = NULL;
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_DIRECTION,
                                   gtk_widget_get_direction (widget));
  attributes = _gtk_pango_get_default_attributes (attributes,
                                                  gtk_text_get_layout (GTK_TEXT (widget)));
  attributes = _gtk_style_context_get_attributes (attributes,
                                                  gtk_widget_get_style_context (widget));

  return attributes;
}

static void
gtk_text_accessible_get_character_extents (AtkText      *text,
                                            gint          offset,
                                            gint         *x,
                                            gint         *y,
                                            gint         *width,
                                            gint         *height,
                                            AtkCoordType  coords)
{
  GtkWidget *widget;
  GtkText *entry;
  PangoRectangle char_rect;
  gchar *entry_text;
  gint index, x_layout, y_layout;
  GtkAllocation allocation;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  entry = GTK_TEXT (widget);

  gtk_text_get_layout_offsets (entry, &x_layout, &y_layout);
  entry_text = gtk_text_get_display_text (entry, 0, -1);
  index = g_utf8_offset_to_pointer (entry_text, offset) - entry_text;
  g_free (entry_text);

  pango_layout_index_to_pos (gtk_text_get_layout (entry), index, &char_rect);
  pango_extents_to_pixels (&char_rect, NULL);

  gtk_widget_get_allocation (widget, &allocation);

  *x = allocation.x + x_layout + char_rect.x;
  *y = allocation.y + y_layout + char_rect.y;
  *width = char_rect.width;
  *height = char_rect.height;
}

static gint
gtk_text_accessible_get_offset_at_point (AtkText      *atk_text,
                                          gint          x,
                                          gint          y,
                                          AtkCoordType  coords)
{
  GtkWidget *widget;
  GtkText *entry;
  gchar *text;
  gint index, x_layout, y_layout;
  gint x_local, y_local;
  glong offset;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return -1;

  entry = GTK_TEXT (widget);

  gtk_text_get_layout_offsets (entry, &x_layout, &y_layout);

  x_local = x - x_layout;
  y_local = y - y_layout;

  if (!pango_layout_xy_to_index (gtk_text_get_layout (entry),
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
      text = gtk_text_get_display_text (entry, 0, -1);
      offset = g_utf8_pointer_to_offset (text, text + index);
      g_free (text);
    }

  return offset;
}

static gint
gtk_text_accessible_get_n_selections (AtkText *text)
{
  GtkWidget *widget;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return 0;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
    return 1;

  return 0;
}

static gchar *
gtk_text_accessible_get_selection (AtkText *text,
                                    gint     selection_num,
                                    gint    *start_pos,
                                    gint    *end_pos)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  if (selection_num != 0)
     return NULL;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), start_pos, end_pos))
    return gtk_editable_get_chars (GTK_EDITABLE (widget), *start_pos, *end_pos);

  return NULL;
}

static gboolean
gtk_text_accessible_add_selection (AtkText *text,
                                    gint     start_pos,
                                    gint     end_pos)
{
  GtkText *entry;
  GtkWidget *widget;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  entry = GTK_TEXT (widget);

  if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      gtk_editable_select_region (GTK_EDITABLE (entry), start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_text_accessible_remove_selection (AtkText *text,
                                       gint     selection_num)
{
  GtkWidget *widget;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
    {
      gtk_editable_select_region (GTK_EDITABLE (widget), end, end);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_text_accessible_set_selection (AtkText *text,
                                    gint     selection_num,
                                    gint     start_pos,
                                    gint     end_pos)
{
  GtkWidget *widget;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
    {
      gtk_editable_select_region (GTK_EDITABLE (widget), start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gunichar
gtk_text_accessible_get_character_at_offset (AtkText *atk_text,
                                              gint     offset)
{
  GtkWidget *widget;
  gchar *text;
  gchar *index;
  gunichar result;

  result = '\0';

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return result;

  if (!gtk_text_get_visibility (GTK_TEXT (widget)))
    return result;

  text = gtk_text_get_display_text (GTK_TEXT (widget), 0, -1);
  if (offset < g_utf8_strlen (text, -1))
    {
      index = g_utf8_offset_to_pointer (text, offset);
      result = g_utf8_get_char (index);
      g_free (text);
    }

  return result;
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gtk_text_accessible_get_text;
  iface->get_character_at_offset = gtk_text_accessible_get_character_at_offset;
  iface->get_text_before_offset = gtk_text_accessible_get_text_before_offset;
  iface->get_text_at_offset = gtk_text_accessible_get_text_at_offset;
  iface->get_text_after_offset = gtk_text_accessible_get_text_after_offset;
  iface->get_caret_offset = gtk_text_accessible_get_caret_offset;
  iface->set_caret_offset = gtk_text_accessible_set_caret_offset;
  iface->get_character_count = gtk_text_accessible_get_character_count;
  iface->get_n_selections = gtk_text_accessible_get_n_selections;
  iface->get_selection = gtk_text_accessible_get_selection;
  iface->add_selection = gtk_text_accessible_add_selection;
  iface->remove_selection = gtk_text_accessible_remove_selection;
  iface->set_selection = gtk_text_accessible_set_selection;
  iface->get_run_attributes = gtk_text_accessible_get_run_attributes;
  iface->get_default_attributes = gtk_text_accessible_get_default_attributes;
  iface->get_character_extents = gtk_text_accessible_get_character_extents;
  iface->get_offset_at_point = gtk_text_accessible_get_offset_at_point;
}

static void
gtk_text_accessible_set_text_contents (AtkEditableText *text,
                                        const gchar     *string)
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
gtk_text_accessible_insert_text (AtkEditableText *text,
                                  const gchar     *string,
                                  gint             length,
                                  gint            *position)
{
  GtkWidget *widget;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  gtk_editable_insert_text (editable, string, length, position);
  gtk_editable_set_position (editable, *position);
}

static void
gtk_text_accessible_copy_text (AtkEditableText *text,
                                gint             start_pos,
                                gint             end_pos)
{
  GtkWidget *widget;
  GtkEditable *editable;
  gchar *str;
  GdkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  editable = GTK_EDITABLE (widget);
  str = gtk_editable_get_chars (editable, start_pos, end_pos);
  clipboard = gtk_widget_get_clipboard (widget);
  gdk_clipboard_set_text (clipboard, str);
  g_free (str);
}

static void
gtk_text_accessible_cut_text (AtkEditableText *text,
                               gint             start_pos,
                               gint             end_pos)
{
  GtkWidget *widget;
  GtkEditable *editable;
  gchar *str;
  GdkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  str = gtk_editable_get_chars (editable, start_pos, end_pos);
  clipboard = gtk_widget_get_clipboard (widget);
  gdk_clipboard_set_text (clipboard, str);
  gtk_editable_delete_text (editable, start_pos, end_pos);
}

static void
gtk_text_accessible_delete_text (AtkEditableText *text,
                                  gint             start_pos,
                                  gint             end_pos)
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
  GtkText* entry;
  gint position;
} PasteData;

static void
paste_received_cb (GObject      *clipboard,
                   GAsyncResult *result,
                   gpointer      data)
{
  PasteData *paste = data;
  char *text;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), result, NULL);
  if (text)
    gtk_editable_insert_text (GTK_EDITABLE (paste->entry), text, -1,
                              &paste->position);

  g_object_unref (paste->entry);
  g_free (paste);
  g_free (text);
}

static void
gtk_text_accessible_paste_text (AtkEditableText *text,
                                 gint             position)
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
  paste->entry = GTK_TEXT (widget);
  paste->position = position;

  g_object_ref (paste->entry);
  clipboard = gtk_widget_get_clipboard (widget);
  gdk_clipboard_read_text_async (clipboard, NULL, paste_received_cb, paste);
}

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
  iface->set_text_contents = gtk_text_accessible_set_text_contents;
  iface->insert_text = gtk_text_accessible_insert_text;
  iface->copy_text = gtk_text_accessible_copy_text;
  iface->cut_text = gtk_text_accessible_cut_text;
  iface->delete_text = gtk_text_accessible_delete_text;
  iface->paste_text = gtk_text_accessible_paste_text;
  iface->set_run_attributes = NULL;
}

static void
insert_text_cb (GtkEditable *editable,
                gchar       *new_text,
                gint         new_text_length,
                gint        *position)
{
  GtkTextAccessible *accessible;
  gint length;

  if (new_text_length == 0)
    return;

  accessible = GTK_TEXT_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (editable)));
  length = g_utf8_strlen (new_text, new_text_length);

  g_signal_emit_by_name (accessible,
                         "text-changed::insert",
                         *position - length,
                          length);
}

/* We connect to GtkEditable::delete-text, since it carries
 * the information we need. But we delay emitting our own
 * text_changed::delete signal until the entry has update
 * all its internal state and emits GtkText::changed.
 */
static void
delete_text_cb (GtkEditable *editable,
                gint         start,
                gint         end)
{
  GtkTextAccessible *accessible;

  accessible = GTK_TEXT_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (editable)));

  if (end < 0)
    {
      gchar *text;

      text = gtk_text_get_display_text (GTK_TEXT (editable), 0, -1);
      end = g_utf8_strlen (text, -1);
      g_free (text);
    }

  if (end == start)
    return;

  g_signal_emit_by_name (accessible,
                         "text-changed::delete",
                         start,
                         end - start);
}

static gboolean
check_for_selection_change (GtkTextAccessible *accessible,
                            GtkText           *entry)
{
  gboolean ret_val = FALSE;
  gint start, end;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      if (end != accessible->priv->cursor_position ||
          start != accessible->priv->selection_bound)
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
      ret_val = (accessible->priv->cursor_position != accessible->priv->selection_bound);
    }

  accessible->priv->cursor_position = end;
  accessible->priv->selection_bound = start;

  return ret_val;
}

static gboolean
gtk_text_accessible_do_action (AtkAction *action,
                                gint       i)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  if (i != 0)
    return FALSE;

  gtk_widget_activate (widget);

  return TRUE;
}

static gint
gtk_text_accessible_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
gtk_text_accessible_get_keybinding (AtkAction *action,
                                     gint       i)
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
        return gtk_accelerator_name (key_val, GDK_MOD1_MASK);
    }

  return NULL;
}

static const gchar*
gtk_text_accessible_action_get_name (AtkAction *action,
                                      gint       i)
{
  if (i == 0)
    return "activate";
  return NULL;
}

static const gchar*
gtk_text_accessible_action_get_localized_name (AtkAction *action,
                                                gint       i)
{
  if (i == 0)
    return C_("Action name", "Activate");
  return NULL;
}

static const gchar*
gtk_text_accessible_action_get_description (AtkAction *action,
                                             gint       i)
{
  if (i == 0)
    return C_("Action description", "Activates the entry");
  return NULL;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_text_accessible_do_action;
  iface->get_n_actions = gtk_text_accessible_get_n_actions;
  iface->get_keybinding = gtk_text_accessible_get_keybinding;
  iface->get_name = gtk_text_accessible_action_get_name;
  iface->get_localized_name = gtk_text_accessible_action_get_localized_name;
  iface->get_description = gtk_text_accessible_action_get_description;
}
