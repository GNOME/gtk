/* gtkemojicompletion.c: An Emoji picker widget
 * Copyright 2017, Red Hat, Inc.
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

#include "gtkemojicompletion.h"

#include "gtkentryprivate.h"
#include "gtkbox.h"
#include "gtkcssprovider.h"
#include "gtklistbox.h"
#include "gtklabel.h"
#include "gtkpopover.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkgesturelongpress.h"
#include "gtkflowbox.h"

struct _GtkEmojiCompletion
{
  GtkPopover parent_instance;

  GtkEntry *entry;
  char *text;
  guint length;
  guint offset;
  gulong changed_id;
  guint n_matches;

  GtkWidget *list;
  GtkWidget *active;

  GVariant *data;

  GtkGesture *long_press;
};

struct _GtkEmojiCompletionClass {
  GtkPopoverClass parent_class;
};

static void connect_signals    (GtkEmojiCompletion *completion,
                                GtkEntry           *entry);
static void disconnect_signals (GtkEmojiCompletion *completion);
static int populate_completion (GtkEmojiCompletion *completion,
                                const char          *text,
                                guint                offset);

#define MAX_ROWS 5

G_DEFINE_TYPE (GtkEmojiCompletion, gtk_emoji_completion, GTK_TYPE_POPOVER)

static void
gtk_emoji_completion_finalize (GObject *object)
{
  GtkEmojiCompletion *completion = GTK_EMOJI_COMPLETION (object);

  disconnect_signals (completion);

  g_free (completion->text);
  g_variant_unref (completion->data);

  g_clear_object (&completion->long_press);

  G_OBJECT_CLASS (gtk_emoji_completion_parent_class)->finalize (object);
}

static void
update_completion (GtkEmojiCompletion *completion)
{
  const char *text;
  guint length;
  guint n_matches;

  n_matches = 0;

  text = gtk_entry_get_text (GTK_ENTRY (completion->entry));
  length = strlen (text);

  if (length > 0)
    {
      gboolean found_candidate = FALSE;
      const char *p;

      p = text + length;
      do
        {
next:
          p = g_utf8_prev_char (p);
          if (*p == ':')
            {
              if (p + 1 == text + length)
                goto next;

              if (p == text || !g_unichar_isalnum (g_utf8_get_char (p - 1)))
                {
                  found_candidate = TRUE;
                }

              break;
            }
        }
      while (g_unichar_isalnum (g_utf8_get_char (p)) || *p == '_');

      if (found_candidate)
        n_matches = populate_completion (completion, p, 0);
    }

  if (n_matches > 0)
    gtk_popover_popup (GTK_POPOVER (completion));
  else
    gtk_popover_popdown (GTK_POPOVER (completion));
}

static void
entry_changed (GtkEntry *entry, GtkEmojiCompletion *completion)
{
  update_completion (completion);
}

static void
emoji_activated (GtkListBox    *list,
                 GtkListBoxRow *row,
                 gpointer       data)
{
  GtkEmojiCompletion *completion = data;
  const char *emoji;
  guint length;

  gtk_popover_popdown (GTK_POPOVER (completion));

  emoji = (const char *)g_object_get_data (G_OBJECT (row), "text");

  g_signal_handler_block (completion->entry, completion->changed_id);

  length = g_utf8_strlen (gtk_entry_get_text (completion->entry), -1);
  gtk_entry_set_positions (completion->entry, length - completion->length, length);
  gtk_entry_enter_text (completion->entry, emoji);

  g_signal_handler_unblock (completion->entry, completion->changed_id);
}

static void
move_active_row (GtkEmojiCompletion *completion,
                 int                 direction)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (completion->list);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_widget_unset_state_flags (child, GTK_STATE_FLAG_PRELIGHT);
    }

  if (completion->active != NULL)
    {
      if (direction == 1)
        completion->active = gtk_widget_get_next_sibling (completion->active);
      else
        completion->active = gtk_widget_get_prev_sibling (completion->active);
    }

  if (completion->active == NULL)
    {
      if (direction == 1)
        completion->active = gtk_widget_get_first_child (completion->list);
      else
        completion->active = gtk_widget_get_last_child (completion->list);
    }

  if (completion->active != NULL)
    gtk_widget_set_state_flags (completion->active, GTK_STATE_FLAG_PRELIGHT, FALSE);
}

static void
activate_active_row (GtkEmojiCompletion *completion)
{
  if (completion->active != NULL)
    emoji_activated (GTK_LIST_BOX (completion->list),
                     GTK_LIST_BOX_ROW (completion->active),
                     completion);
}

static gboolean
entry_key_press (GtkEntry           *entry,
                 GdkEventKey        *event,
                 GtkEmojiCompletion *completion)
{
  guint keyval;

  if (!gtk_widget_get_visible (GTK_WIDGET (completion)))
    return FALSE;

  gdk_event_get_keyval ((GdkEvent*)event, &keyval);

  if (keyval == GDK_KEY_Escape)
    {
      gtk_popover_popdown (GTK_POPOVER (completion));
      return TRUE;
    }

  if (keyval == GDK_KEY_Tab)
    {
      guint offset = completion->offset + MAX_ROWS;
      if (offset >= completion->n_matches)
        offset = 0;
      populate_completion (completion, completion->text, offset);
      return TRUE;
    }

  if (keyval == GDK_KEY_Up)
    {
      move_active_row (completion, -1);
      return TRUE;
    }

  if (keyval == GDK_KEY_Down)
    {
      move_active_row (completion, 1);
      return TRUE;
    }

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter)
    {
      activate_active_row (completion);
      return TRUE;
    }

  return FALSE;
}

static gboolean
entry_focus_out (GtkWidget *entry,
                 GdkEventFocus *event,
                 GtkEmojiCompletion *completion)
{
  gtk_popover_popdown (GTK_POPOVER (completion));
  return FALSE;
}

static void
connect_signals (GtkEmojiCompletion *completion,
                 GtkEntry           *entry)
{
  completion->entry = entry;

  completion->changed_id = g_signal_connect (entry, "changed", G_CALLBACK (entry_changed), completion);
  g_signal_connect (entry, "key-press-event", G_CALLBACK (entry_key_press), completion);
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (entry_focus_out), completion);
}

static void
disconnect_signals (GtkEmojiCompletion *completion)
{
  g_signal_handlers_disconnect_by_func (completion->entry, entry_changed, completion);
  g_signal_handlers_disconnect_by_func (completion->entry, entry_key_press, completion);
  g_signal_handlers_disconnect_by_func (completion->entry, entry_focus_out, completion);

  completion->entry = NULL;
}

static void
add_emoji (GtkWidget *list,
           GVariant  *item)
{
  GtkWidget *child;
  GtkWidget *label;
  GtkWidget *box;
  PangoAttrList *attrs;
  GVariant *codes;
  char text[64];
  char *p = text;
  int i;
  const char *shortname;

  codes = g_variant_get_child_value (item, 0);
  for (i = 0; i < g_variant_n_children (codes); i++)
    {
      gunichar code;

      g_variant_get_child (codes, i, "u", &code);
      if (code != 0)
        p += g_unichar_to_utf8 (code, p);
    }
  p[0] = 0;

  label = gtk_label_new (text);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  child = gtk_list_box_row_new ();
  gtk_widget_set_focus_on_click (child, FALSE);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (child), box);
  gtk_box_pack_start (GTK_BOX (box), label);

  g_variant_get_child (item, 2, "&s", &shortname);
  label = gtk_label_new (shortname);
  gtk_box_pack_start (GTK_BOX (box), label);

  g_object_set_data_full (G_OBJECT (child), "text", g_strdup (text), g_free);
  g_object_set_data_full (G_OBJECT (child), "emoji-data",
                          g_variant_ref (item), (GDestroyNotify)g_variant_unref);
  gtk_style_context_add_class (gtk_widget_get_style_context (child), "emoji-completion-row");

  gtk_list_box_insert (GTK_LIST_BOX (list), child, -1);
}

static int
populate_completion (GtkEmojiCompletion *completion,
                     const char          *text,
                     guint                offset)
{
  GList *children, *l;
  int n_matches;
  int n_added;
  GVariantIter iter;
  GVariant *item;

  text = g_strdup (text);
  g_free (completion->text);
  completion->text = g_strdup (text);
  completion->length = g_utf8_strlen (text, -1);
  completion->offset = offset;

  children = gtk_container_get_children (GTK_CONTAINER (completion->list));
  for (l = children; l; l = l->next)
    gtk_widget_destroy (GTK_WIDGET (l->data));
  g_list_free (children);

  completion->active = NULL;

  n_matches = 0;
  n_added = 0;
  g_variant_iter_init (&iter, completion->data);
  while ((item = g_variant_iter_next_value (&iter)))
    {
      const char *shortname;

      g_variant_get_child (item, 2, "&s", &shortname);
      if (g_str_has_prefix (shortname, text))
        {
          n_matches++;

          if (n_matches > offset && n_added < MAX_ROWS)
            {
              add_emoji (completion->list, item);
              n_added++;
            }
        }
    }

  completion->n_matches = n_matches;

  if (n_added > 0)
    {
      completion->active = gtk_widget_get_first_child (completion->list);
      gtk_widget_set_state_flags (completion->active, GTK_STATE_FLAG_PRELIGHT, FALSE);
    }

  return n_added;
}

static void
add_emoji_variation (GtkWidget *box,
                     GVariant  *item,
                     gunichar   modifier)
{
  GtkWidget *child;
  GtkWidget *label;
  PangoAttrList *attrs;
  GVariant *codes;
  char text[64];
  char *p = text;
  int i;

  codes = g_variant_get_child_value (item, 0);
  for (i = 0; i < g_variant_n_children (codes); i++)
    {
      gunichar code;

      g_variant_get_child (codes, i, "u", &code);
      if (code == 0)
        code = modifier;
      if (code != 0)
        p += g_unichar_to_utf8 (code, p);
    }
  g_variant_unref (codes);
  p[0] = 0;

  label = gtk_label_new (text);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  child = gtk_flow_box_child_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (child), "emoji");
  g_object_set_data_full (G_OBJECT (child), "text", g_strdup (text), g_free);
  g_object_set_data_full (G_OBJECT (child), "emoji-data",
                          g_variant_ref (item),
                          (GDestroyNotify)g_variant_unref);
  if (modifier != 0)
    g_object_set_data (G_OBJECT (child), "modifier", GUINT_TO_POINTER (modifier));

  gtk_container_add (GTK_CONTAINER (child), label);
  gtk_flow_box_insert (GTK_FLOW_BOX (box), child, -1);
}

static void
long_pressed_cb (GtkGesture *gesture,
                 double      x,
                 double      y,
                 gpointer    data)
{
  GtkEmojiCompletion *completion = data;
  GtkWidget *row;
  GVariant *emoji_data;
  gboolean has_variations;
  GtkWidget *popover;
  gunichar modifier;
  GVariant *codes;
  int i;
  GtkWidget *box;
  GtkWidget *view;

  row = GTK_WIDGET (gtk_list_box_get_row_at_y (GTK_LIST_BOX (completion->list), y));
  if (!row)
    return;

  emoji_data = (GVariant*) g_object_get_data (G_OBJECT (row), "emoji-data");
  if (!emoji_data)
    return;

  has_variations = FALSE;
  codes = g_variant_get_child_value (emoji_data, 0);
  for (i = 0; i < g_variant_n_children (codes); i++)
    {
      gunichar code;
      g_variant_get_child (codes, i, "u", &code);
      if (code == 0)
        {
          has_variations = TRUE;
          break;
        }
    }
  g_variant_unref (codes);
  if (!has_variations)
    return;

  popover = gtk_popover_new (row);
  gtk_popover_set_modal (GTK_POPOVER (popover), FALSE);
  view = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (view), "view");
  box = gtk_flow_box_new ();
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (box), TRUE);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (box), 6);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (box), 6);
  gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (box), TRUE);
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (box), GTK_SELECTION_NONE);
  gtk_container_add (GTK_CONTAINER (popover), view);
  gtk_container_add (GTK_CONTAINER (view), box);

  g_signal_connect (box, "child-activated", G_CALLBACK (emoji_activated), completion);

  add_emoji_variation (box, emoji_data, 0);
  for (modifier = 0x1f3fb; modifier <= 0x1f3ff; modifier++)
    add_emoji_variation (box, emoji_data, modifier);

  gtk_popover_popup (GTK_POPOVER (popover));
}

static void
gtk_emoji_completion_init (GtkEmojiCompletion *completion)
{
  g_autoptr(GBytes) bytes = NULL;

  gtk_widget_init_template (GTK_WIDGET (completion));

  bytes = g_resources_lookup_data ("/org/gtk/libgtk/emoji/emoji.data", 0, NULL);
  completion->data = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(auss)"), bytes, TRUE));

  completion->long_press = gtk_gesture_long_press_new (completion->list);
  g_signal_connect (completion->long_press, "pressed", G_CALLBACK (long_pressed_cb), completion);
}

static void
gtk_emoji_completion_class_init (GtkEmojiCompletionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_emoji_completion_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkemojicompletion.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiCompletion, list);

  gtk_widget_class_bind_template_callback (widget_class, emoji_activated);
}

GtkWidget *
gtk_emoji_completion_new (GtkEntry *entry)
{
  GtkEmojiCompletion *completion;

  completion = GTK_EMOJI_COMPLETION (g_object_new (GTK_TYPE_EMOJI_COMPLETION,
                                                   "relative-to", entry,
                                                   NULL));

  connect_signals (completion, entry);

  return GTK_WIDGET (completion);
}
