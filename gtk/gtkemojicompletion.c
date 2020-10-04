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
#include "gtkstack.h"

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
  GtkWidget *active_variation;

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
      while (g_unichar_isalnum (g_utf8_get_char (p)) || *p == '_' || *p == ' ');

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
emoji_activated (GtkWidget          *row,
                 GtkEmojiCompletion *completion)
{
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
row_activated (GtkListBox    *list,
               GtkListBoxRow *row,
               gpointer       data)
{
  GtkEmojiCompletion *completion = data;

  emoji_activated (GTK_WIDGET (row), completion);
}

static void
child_activated (GtkFlowBox      *box,
                 GtkFlowBoxChild *child,
                 gpointer         data)
{
  GtkEmojiCompletion *completion = data;

  emoji_activated (GTK_WIDGET (child), completion);
}

static void
move_active_row (GtkEmojiCompletion *completion,
                 int                 direction)
{
  GtkWidget *child;
  GtkWidget *base;
  GList *children, *l, *active, *last;

  active = NULL;
  last = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (completion->list));
  for (l = children; l; l = l->next)
    {
      child = l->data;

      if (completion->active == child)
        active = l;

      if (l->next == NULL)
        last = l;

      gtk_widget_unset_state_flags (child, GTK_STATE_FLAG_PRELIGHT);
      base = GTK_WIDGET (g_object_get_data (G_OBJECT (child), "base"));
      gtk_widget_unset_state_flags (base, GTK_STATE_FLAG_PRELIGHT);
    }

  if (completion->active != NULL)
    {
      if (direction == 1)
        completion->active = (active && active->next) ? active->next->data : NULL;
      else
        completion->active = (active && active->prev) ? active->prev->data : NULL;
    }

  if (completion->active == NULL)
    {
      if (direction == 1)
        completion->active = children->data;
      else
        completion->active = last->data;
    }

  if (completion->active != NULL)
    gtk_widget_set_state_flags (completion->active, GTK_STATE_FLAG_PRELIGHT, FALSE);

  if (completion->active_variation)
    {
      gtk_widget_unset_state_flags (completion->active_variation, GTK_STATE_FLAG_PRELIGHT);
      completion->active_variation = NULL;
    }

  g_list_free (children);
}

static void
activate_active_row (GtkEmojiCompletion *completion)
{
  if (GTK_IS_FLOW_BOX_CHILD (completion->active_variation))
    emoji_activated (completion->active_variation, completion);
  else if (completion->active != NULL)
    emoji_activated (completion->active, completion);
}

static void
show_variations (GtkEmojiCompletion *completion,
                 GtkWidget          *row,
                 gboolean            visible)
{
  GtkWidget *stack;
  GtkWidget *box;
  gboolean is_visible;

  if (!row)
    return;

  stack = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "stack"));
  box = gtk_stack_get_child_by_name (GTK_STACK (stack), "variations");
  if (!box)
    return;

  is_visible = gtk_stack_get_visible_child (GTK_STACK (stack)) == box;
  if (is_visible == visible)
    return;

  if (visible)
    gtk_widget_unset_state_flags (row, GTK_STATE_FLAG_PRELIGHT);
  else
    gtk_widget_set_state_flags (row, GTK_STATE_FLAG_PRELIGHT, FALSE);

  gtk_stack_set_visible_child_name (GTK_STACK (stack), visible ? "variations" : "text");
  if (completion->active_variation)
    {
      gtk_widget_unset_state_flags (completion->active_variation, GTK_STATE_FLAG_PRELIGHT);
      completion->active_variation = NULL;
    }
}

static gboolean
move_active_variation (GtkEmojiCompletion *completion,
                       int                 direction)
{
  GtkWidget *base;
  GtkWidget *stack;
  GtkWidget *box;
  GtkWidget *next;
  GList *children, *l, *active;

  if (!completion->active)
    return FALSE;

  base = GTK_WIDGET (g_object_get_data (G_OBJECT (completion->active), "base"));
  stack = GTK_WIDGET (g_object_get_data (G_OBJECT (completion->active), "stack"));
  box = gtk_stack_get_child_by_name (GTK_STACK (stack), "variations");

  if (gtk_stack_get_visible_child (GTK_STACK (stack)) != box)
    return FALSE;

  next = NULL;

  active = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (box));
  for (l = children; l; l = l->next)
    {
      if (l->data == completion->active_variation)
        active = l;
    }

  if (!completion->active_variation)
    next = base;
  else if (completion->active_variation == base && direction == 1)
    next = children->data;
  else if (completion->active_variation == children->data && direction == -1)
    next = base;
  else if (direction == 1)
    next = (active && active->next) ? active->next->data : NULL;
  else if (direction == -1)
    next = (active && active->prev) ? active->prev->data : NULL;

  if (next)
    {
      if (completion->active_variation)
        gtk_widget_unset_state_flags (completion->active_variation, GTK_STATE_FLAG_PRELIGHT);
      completion->active_variation = next;
      gtk_widget_set_state_flags (completion->active_variation, GTK_STATE_FLAG_PRELIGHT, FALSE);
    }

  g_list_free (children);

  return next != NULL;
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
      guint offset;
      show_variations (completion, completion->active, FALSE);

      offset = completion->offset + MAX_ROWS;
      if (offset >= completion->n_matches)
        offset = 0;
      populate_completion (completion, completion->text, offset);
      return TRUE;
    }

  if (keyval == GDK_KEY_Up)
    {
      show_variations (completion, completion->active, FALSE);

      move_active_row (completion, -1);
      return TRUE;
    }

  if (keyval == GDK_KEY_Down)
    {
      show_variations (completion, completion->active, FALSE);

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

  if (keyval == GDK_KEY_Right)
    {
      show_variations (completion, completion->active, TRUE);
      move_active_variation (completion, 1);
      return TRUE;
    }

  if (keyval == GDK_KEY_Left)
    {
      if (!move_active_variation (completion, -1))
        show_variations (completion, completion->active, FALSE);
      return TRUE;
    }

  return FALSE;
}

static gboolean
entry_focus_out (GtkWidget *entry,
                 GParamSpec *pspec,
                 GtkEmojiCompletion *completion)
{
  if (!gtk_widget_has_focus (entry))
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
  g_signal_connect (entry, "notify::has-focus", G_CALLBACK (entry_focus_out), completion);
}

static void
disconnect_signals (GtkEmojiCompletion *completion)
{
  g_signal_handlers_disconnect_by_func (completion->entry, entry_changed, completion);
  g_signal_handlers_disconnect_by_func (completion->entry, entry_key_press, completion);
  g_signal_handlers_disconnect_by_func (completion->entry, entry_focus_out, completion);

  completion->entry = NULL;
}

static gboolean
has_variations (GVariant *emoji_data)
{
  GVariant *codes;
  gsize i;
  gboolean has_variations;

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

  return has_variations;
}

static void
get_text (GVariant *emoji_data,
          gunichar  modifier,
          char     *text,
          gsize     length)
{
  GVariant *codes;
  int i;
  char *p;

  p = text;
  codes = g_variant_get_child_value (emoji_data, 0);
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
  p += g_unichar_to_utf8 (0xFE0F, p); /* U+FE0F is the Emoji variation selector */
  p[0] = 0;
}

static void
add_emoji_variation (GtkWidget *box,
                     GVariant  *emoji_data,
                     gunichar   modifier)
{
  GtkWidget *child;
  GtkWidget *label;
  PangoAttrList *attrs;
  char text[64];

  get_text (emoji_data, modifier, text, 64);

  label = gtk_label_new (text);
  gtk_widget_show (label);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  child = gtk_flow_box_child_new ();
  gtk_widget_show (child);
  gtk_style_context_add_class (gtk_widget_get_style_context (child), "emoji");
  g_object_set_data_full (G_OBJECT (child), "text", g_strdup (text), g_free);
  g_object_set_data_full (G_OBJECT (child), "emoji-data",
                          g_variant_ref (emoji_data),
                          (GDestroyNotify)g_variant_unref);
  if (modifier != 0)
    g_object_set_data (G_OBJECT (child), "modifier", GUINT_TO_POINTER (modifier));

  gtk_container_add (GTK_CONTAINER (child), label);
  gtk_flow_box_insert (GTK_FLOW_BOX (box), child, -1);
}

static void
add_emoji (GtkWidget          *list,
           GVariant           *emoji_data,
           GtkEmojiCompletion *completion)
{
  GtkWidget *child;
  GtkWidget *label;
  GtkWidget *box;
  PangoAttrList *attrs;
  char text[64];
  const char *name;
  GtkWidget *stack;
  gunichar modifier;

  get_text (emoji_data, 0, text, 64);

  label = gtk_label_new (text);
  gtk_widget_show (label);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "emoji");

  child = gtk_list_box_row_new ();
  gtk_widget_show (child);
  gtk_widget_set_focus_on_click (child, FALSE);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (child), box);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (child), "base", label);

  stack = gtk_stack_new ();
  gtk_widget_show (stack);
  gtk_stack_set_homogeneous (GTK_STACK (stack), TRUE);
  gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT);
  gtk_box_pack_start (GTK_BOX (box), stack, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (child), "stack", stack);

  g_variant_get_child (emoji_data, 1, "&s", &name);
  label = gtk_label_new (name);
  gtk_widget_show (label);
  gtk_label_set_xalign (GTK_LABEL (label), 0);

  gtk_stack_add_named (GTK_STACK (stack), label, "text");

  if (has_variations (emoji_data))
    {
      box = gtk_flow_box_new ();
      gtk_widget_show (box);
      gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (box), TRUE);
      gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (box), 5);
      gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (box), 5);
      gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (box), TRUE);
      gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (box), GTK_SELECTION_NONE);
      g_signal_connect (box, "child-activated", G_CALLBACK (child_activated), completion);
      for (modifier = 0x1f3fb; modifier <= 0x1f3ff; modifier++)
        add_emoji_variation (box, emoji_data, modifier);

      gtk_stack_add_named (GTK_STACK (stack), box, "variations");
    }

  g_object_set_data_full (G_OBJECT (child), "text", g_strdup (text), g_free);
  g_object_set_data_full (G_OBJECT (child), "emoji-data",
                          g_variant_ref (emoji_data), (GDestroyNotify)g_variant_unref);
  gtk_style_context_add_class (gtk_widget_get_style_context (child), "emoji-completion-row");

  gtk_list_box_insert (GTK_LIST_BOX (list), child, -1);
}

static int
populate_completion (GtkEmojiCompletion *completion,
                     const char          *text,
                     guint                offset)
{
  GList *children, *l;
  guint n_matches;
  guint n_added;
  GVariantIter iter;
  GVariant *item;

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
      const char *name;

      g_variant_get_child (item, 1, "&s", &name);

      if (g_str_has_prefix (name, text + 1))
        {
          n_matches++;

          if (n_matches > offset && n_added < MAX_ROWS)
            {
              add_emoji (completion->list, item, completion);
              n_added++;
            }
        }
    }

  completion->n_matches = n_matches;

  if (n_added > 0)
    {
      GList *children;

      children = gtk_container_get_children (GTK_CONTAINER (completion->list));
      completion->active = children->data;
      g_list_free (children);
      gtk_widget_set_state_flags (completion->active, GTK_STATE_FLAG_PRELIGHT, FALSE);
    }

  return n_added;
}

static void
long_pressed_cb (GtkGesture *gesture,
                 double      x,
                 double      y,
                 gpointer    data)
{
  GtkEmojiCompletion *completion = data;
  GtkWidget *row;

  row = GTK_WIDGET (gtk_list_box_get_row_at_y (GTK_LIST_BOX (completion->list), y));
  if (!row)
    return;

  show_variations (completion, row, TRUE);
}

static void
gtk_emoji_completion_init (GtkEmojiCompletion *completion)
{
  GBytes *bytes = NULL;

  gtk_widget_init_template (GTK_WIDGET (completion));

  bytes = get_emoji_data ();
  completion->data = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(ausasu)"), bytes, TRUE));

  g_bytes_unref (bytes);

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

  gtk_widget_class_bind_template_callback (widget_class, row_activated);
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
