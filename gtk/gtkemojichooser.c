/* gtkemojichooser.c: An Emoji chooser widget
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

#include "gtkemojichooser.h"

#include "gtkadjustmentprivate.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkcssprovider.h"
#include "gtkentry.h"
#include "gtkflowbox.h"
#include "gtkstack.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkgesturelongpress.h"
#include "gtkgesturemultipress.h"
#include "gtkpopover.h"
#include "gtkscrolledwindow.h"
#include "gtkeventbox.h"
#include "gtkintl.h"
#include "gtkprivate.h"

#define BOX_SPACE 6

typedef struct {
  GtkWidget *box;
  GtkWidget *heading;
  GtkWidget *button;
  int group;
  gunichar label;
  gboolean empty;
} EmojiSection;

struct _GtkEmojiChooser
{
  GtkPopover parent_instance;

  GtkWidget *search_entry;
  GtkWidget *stack;
  GtkWidget *scrolled_window;

  int emoji_max_width;

  EmojiSection recent;
  EmojiSection people;
  EmojiSection body;
  EmojiSection nature;
  EmojiSection food;
  EmojiSection travel;
  EmojiSection activities;
  EmojiSection objects;
  EmojiSection symbols;
  EmojiSection flags;

  GtkGesture *recent_long_press;
  GtkGesture *recent_multi_press;
  GtkGesture *people_long_press;
  GtkGesture *people_multi_press;
  GtkGesture *body_long_press;
  GtkGesture *body_multi_press;

  GVariant *data;
  GtkWidget *box;
  GVariantIter *iter;
  guint populate_idle;

  GSettings *settings;
};

struct _GtkEmojiChooserClass {
  GtkPopoverClass parent_class;
};

enum {
  EMOJI_PICKED,
  LAST_SIGNAL
};

static int signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkEmojiChooser, gtk_emoji_chooser, GTK_TYPE_POPOVER)

static void
gtk_emoji_chooser_finalize (GObject *object)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (object);

  if (chooser->populate_idle)
    g_source_remove (chooser->populate_idle);

  g_variant_unref (chooser->data);
  g_object_unref (chooser->settings);

  g_clear_object (&chooser->recent_long_press);
  g_clear_object (&chooser->recent_multi_press);
  g_clear_object (&chooser->people_long_press);
  g_clear_object (&chooser->people_multi_press);
  g_clear_object (&chooser->body_long_press);
  g_clear_object (&chooser->body_multi_press);

  G_OBJECT_CLASS (gtk_emoji_chooser_parent_class)->finalize (object);
}

static void
scroll_to_section (GtkButton *button,
                   gpointer   data)
{
  EmojiSection *section = data;
  GtkEmojiChooser *chooser;
  GtkAdjustment *adj;
  GtkAllocation alloc = { 0, 0, 0, 0 };

  chooser = GTK_EMOJI_CHOOSER (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_EMOJI_CHOOSER));

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));

  if (section->heading)
    gtk_widget_get_allocation (section->heading, &alloc);

  gtk_adjustment_animate_to_value (adj, alloc.y - BOX_SPACE);
}

static void
add_emoji (GtkWidget    *box,
           gboolean      prepend,
           GVariant     *item,
           gunichar      modifier,
           GtkEmojiChooser *chooser);

#define MAX_RECENT (7*3)

static void
populate_recent_section (GtkEmojiChooser *chooser)
{
  GVariant *variant;
  GVariant *item;
  GVariantIter iter;
  gboolean empty = TRUE;

  variant = g_settings_get_value (chooser->settings, "recent-emoji");
  g_variant_iter_init (&iter, variant);
  while ((item = g_variant_iter_next_value (&iter)))
    {
      GVariant *emoji_data;
      gunichar modifier;

      emoji_data = g_variant_get_child_value (item, 0);
      g_variant_get_child (item, 1, "u", &modifier);
      add_emoji (chooser->recent.box, FALSE, emoji_data, modifier, chooser);
      g_variant_unref (emoji_data);
      g_variant_unref (item);
      empty = FALSE;
    }

  gtk_widget_set_visible (chooser->recent.box, !empty);
  gtk_widget_set_sensitive (chooser->recent.button, !empty);

  g_variant_unref (variant);
}

static GVariant *
get_recent_emoji_data (GtkWidget *widget)
{
  GVariant *emoji_data = g_object_get_data (G_OBJECT (widget), "emoji-data");
  GVariantIter *codes_iter;
  GVariantIter *keywords_iter;
  GVariantBuilder codes_builder;
  const char *name;
  const char *shortname;
  guint code;
  guint group;

  g_assert (emoji_data);

  if (g_variant_is_of_type (emoji_data, G_VARIANT_TYPE ("(auss)")))
    return emoji_data;

  g_variant_get (emoji_data, "(au&sasu)", &codes_iter, &name, &keywords_iter, &group);

  g_variant_builder_init (&codes_builder, G_VARIANT_TYPE ("au"));
  while (g_variant_iter_loop (codes_iter, "u", &code))
    g_variant_builder_add (&codes_builder, "u", code);

  g_variant_iter_free (codes_iter);
  g_variant_iter_free (keywords_iter);

  shortname = "";

  return g_variant_new ("(auss)", &codes_builder, name, shortname);
}

static void
add_recent_item (GtkEmojiChooser *chooser,
                 GVariant        *item,
                 gunichar         modifier)
{
  GList *children, *l;
  int i;
  GVariantBuilder builder;

  g_variant_ref (item);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a((auss)u)"));
  g_variant_builder_add (&builder, "(@(auss)u)", item, modifier);

  children = gtk_container_get_children (GTK_CONTAINER (chooser->recent.box));
  for (l = children, i = 1; l; l = l->next, i++)
    {
      GVariant *item2 = get_recent_emoji_data (GTK_WIDGET (l->data));
      gunichar modifier2 = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (l->data), "modifier"));

      if (modifier == modifier2 && g_variant_equal (item, item2))
        {
          gtk_widget_destroy (GTK_WIDGET (l->data));
          i--;
          continue;
        }
      if (i >= MAX_RECENT)
        {
          gtk_widget_destroy (GTK_WIDGET (l->data));
          continue;
        }

      g_variant_builder_add (&builder, "(@(auss)u)", item2, modifier2);
    }
  g_list_free (children);

  add_emoji (chooser->recent.box, TRUE, item, modifier, chooser);

  /* Enable recent */
  gtk_widget_show (chooser->recent.box);
  gtk_widget_set_sensitive (chooser->recent.button, TRUE);

  g_settings_set_value (chooser->settings, "recent-emoji", g_variant_builder_end (&builder));

  g_variant_unref (item);
}

static gboolean
should_close (GtkEmojiChooser *chooser)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;
  GdkModifierType state;

  display = gtk_widget_get_display (GTK_WIDGET (chooser));
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);
  gdk_device_get_state (device, gtk_widget_get_window (GTK_WIDGET (chooser)),
                        NULL, &state);

  return (state & GDK_CONTROL_MASK) == 0;
}

static void
emoji_activated (GtkFlowBox      *box,
                 GtkFlowBoxChild *child,
                 gpointer         data)
{
  GtkEmojiChooser *chooser = data;
  char *text;
  GtkWidget *ebox;
  GtkWidget *label;
  GVariant *item;
  gunichar modifier;

  if (should_close (chooser))
    gtk_popover_popdown (GTK_POPOVER (chooser));
  else
    {
      GtkWidget *popover;

      popover = gtk_widget_get_ancestor (GTK_WIDGET (box), GTK_TYPE_POPOVER);
      if (popover != GTK_WIDGET (chooser))
        gtk_popover_popdown (GTK_POPOVER (popover));
    }

  ebox = gtk_bin_get_child (GTK_BIN (child));
  label = gtk_bin_get_child (GTK_BIN (ebox));
  text = g_strdup (gtk_label_get_label (GTK_LABEL (label)));

  item = get_recent_emoji_data (GTK_WIDGET (child));
  modifier = (gunichar) GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (child), "modifier"));
  add_recent_item (chooser, item, modifier);

  g_signal_emit (data, signals[EMOJI_PICKED], 0, text);
  g_free (text);
}

static gboolean
has_variations (GVariant *emoji_data)
{
  GVariant *codes;
  int i;
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
show_variations (GtkEmojiChooser *chooser,
                 GtkWidget       *child)
{
  GtkWidget *popover;
  GtkWidget *view;
  GtkWidget *box;
  GVariant *emoji_data;
  GtkWidget *parent_popover;
  gunichar modifier;

  if (!child)
    return;

  emoji_data = (GVariant*) g_object_get_data (G_OBJECT (child), "emoji-data");
  if (!emoji_data)
    return;

  if (!has_variations (emoji_data))
    return;

  parent_popover = gtk_widget_get_ancestor (child, GTK_TYPE_POPOVER);
  popover = gtk_popover_new (child);
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

  g_signal_connect (box, "child-activated", G_CALLBACK (emoji_activated), parent_popover);

  add_emoji (box, FALSE, emoji_data, 0, chooser);
  for (modifier = 0x1f3fb; modifier <= 0x1f3ff; modifier++)
    add_emoji (box, FALSE, emoji_data, modifier, chooser);

  gtk_widget_show_all (view);
  gtk_popover_popup (GTK_POPOVER (popover));
}

static void
update_hover (GtkWidget *widget,
              GdkEvent  *event,
              gpointer   data)
{
  if (event->type == GDK_ENTER_NOTIFY)
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
  else
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);
}

static void
long_pressed_cb (GtkGesture *gesture,
                 double      x,
                 double      y,
                 gpointer    data)
{
  GtkEmojiChooser *chooser = data;
  GtkWidget *box;
  GtkWidget *child;

  box = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = GTK_WIDGET (gtk_flow_box_get_child_at_pos (GTK_FLOW_BOX (box), x, y));
  show_variations (chooser, child);
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  GtkEmojiChooser *chooser = data;
  GtkWidget *box;
  GtkWidget *child;

  box = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = GTK_WIDGET (gtk_flow_box_get_child_at_pos (GTK_FLOW_BOX (box), x, y));
  show_variations (chooser, child);
}

static gboolean
popup_menu (GtkWidget *widget,
            gpointer   data)
{
  GtkEmojiChooser *chooser = data;

  show_variations (chooser, widget);
  return TRUE;
}

static void
add_emoji (GtkWidget    *box,
           gboolean      prepend,
           GVariant     *item,
           gunichar      modifier,
           GtkEmojiChooser *chooser)
{
  GtkWidget *child;
  GtkWidget *ebox;
  GtkWidget *label;
  PangoAttrList *attrs;
  GVariant *codes;
  char text[64];
  char *p = text;
  int i;
  PangoLayout *layout;
  PangoRectangle rect;

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
  p += g_unichar_to_utf8 (0xFE0F, p); /* U+FE0F is the Emoji variation selector */
  p[0] = 0;

  label = gtk_label_new (text);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  layout = gtk_label_get_layout (GTK_LABEL (label));
  pango_layout_get_extents (layout, &rect, NULL);

  /* Check for fallback rendering that generates too wide items */
  if (pango_layout_get_unknown_glyphs_count (layout) > 0 ||
      rect.width >= 1.5 * chooser->emoji_max_width)
    {
      gtk_widget_destroy (label);
      return;
    }

  child = gtk_flow_box_child_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (child), "emoji");
  g_object_set_data_full (G_OBJECT (child), "emoji-data",
                          g_variant_ref (item),
                          (GDestroyNotify)g_variant_unref);
  if (modifier != 0)
    g_object_set_data (G_OBJECT (child), "modifier", GUINT_TO_POINTER (modifier));

  ebox = gtk_event_box_new ();
  gtk_widget_add_events (ebox, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect (ebox, "enter-notify-event", G_CALLBACK (update_hover), FALSE);
  g_signal_connect (ebox, "leave-notify-event", G_CALLBACK (update_hover), FALSE);
  gtk_container_add (GTK_CONTAINER (child), ebox);
  gtk_container_add (GTK_CONTAINER (ebox), label);
  gtk_widget_show_all (child);

  if (chooser)
    g_signal_connect (child, "popup-menu", G_CALLBACK (popup_menu), chooser);

  gtk_flow_box_insert (GTK_FLOW_BOX (box), child, prepend ? 0 : -1);
}

GBytes *
get_emoji_data (void)
{
  GBytes *bytes;
  const char *lang;
  char q[10];
  char *path;
  GError *error = NULL;

  lang = pango_language_to_string (gtk_get_default_language ());
  if (strchr (lang, '-'))
    {
      int i;
      for (i = 0; lang[i] != '-' && i < 9; i++)
        q[i] = lang[i];
      q[i] = '\0';
      lang = q;
    }

  path = g_strconcat ("/org/gtk/libgtk/emoji/", lang, ".data", NULL);
  bytes = g_resources_lookup_data (path, 0, &error);
  if (bytes)
    {
      g_debug ("Found emoji data for %s in resource %s", lang, path);
      g_free (path);
      return bytes;
    }

  if (g_error_matches (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND))
    {
      char *filename;
      char *gresource_name;
      GMappedFile *file;

      g_clear_error (&error);

      gresource_name = g_strconcat (lang, ".gresource", NULL);
      filename = g_build_filename (_gtk_get_data_prefix (), "share", "gtk-3.0",
                                   "emoji", gresource_name, NULL);
      g_clear_pointer (&gresource_name, g_free);
      file = g_mapped_file_new (filename, FALSE, NULL);

      if (file)
        {
          GBytes *data;
          GResource *resource;

          data = g_mapped_file_get_bytes (file);
          g_mapped_file_unref (file);

          resource = g_resource_new_from_data (data, NULL);
          g_bytes_unref (data);

          g_debug ("Registering resource for Emoji data for %s from file %s", lang, filename);
          g_resources_register (resource);
          g_resource_unref (resource);

          bytes = g_resources_lookup_data (path, 0, NULL);
          if (bytes)
            {
              g_debug ("Found emoji data for %s in resource %s", lang, path);
              g_free (path);
              g_free (filename);
              return bytes;
            }
        }

      g_free (filename);
    }

  g_clear_error (&error);

  g_free (path);

  return g_resources_lookup_data ("/org/gtk/libgtk/emoji/en.data", 0, NULL);
}

static gboolean
populate_emoji_chooser (gpointer data)
{
  GtkEmojiChooser *chooser = data;
  GVariant *item;
  guint64 start, now;

  start = g_get_monotonic_time ();

  if (!chooser->data)
    {
      GBytes *bytes;

      bytes = get_emoji_data ();

      chooser->data = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(ausasu)"), bytes, TRUE));
      g_bytes_unref (bytes);
    }

  if (!chooser->iter)
    {
      chooser->iter = g_variant_iter_new (chooser->data);
      chooser->box = chooser->people.box;
    }
  while ((item = g_variant_iter_next_value (chooser->iter)))
    {
      guint group;

      g_variant_get_child (item, 3, "u", &group);

      if (group == chooser->people.group)
        chooser->box = chooser->people.box;
      else if (group == chooser->body.group)
        chooser->box = chooser->body.box;
      else if (group == chooser->nature.group)
        chooser->box = chooser->nature.box;
      else if (group == chooser->food.group)
        chooser->box = chooser->food.box;
      else if (group == chooser->travel.group)
        chooser->box = chooser->travel.box;
      else if (group == chooser->activities.group)
        chooser->box = chooser->activities.box;
      else if (group == chooser->objects.group)
        chooser->box = chooser->objects.box;
      else if (group == chooser->symbols.group)
        chooser->box = chooser->symbols.box;
      else if (group == chooser->flags.group)
        chooser->box = chooser->flags.box;

      add_emoji (chooser->box, FALSE, item, 0, chooser);
      g_variant_unref (item);

      now = g_get_monotonic_time ();
      if (now > start + 8000)
        return G_SOURCE_CONTINUE;
    }

  g_variant_iter_free (chooser->iter);
  chooser->iter = NULL;
  chooser->box = NULL;
  chooser->populate_idle = 0;

  return G_SOURCE_REMOVE;
}

static void
adj_value_changed (GtkAdjustment *adj,
                   gpointer       data)
{
  GtkEmojiChooser *chooser = data;
  double value = gtk_adjustment_get_value (adj);
  EmojiSection const *sections[] = {
    &chooser->recent,
    &chooser->people,
    &chooser->body,
    &chooser->nature,
    &chooser->food,
    &chooser->travel,
    &chooser->activities,
    &chooser->objects,
    &chooser->symbols,
    &chooser->flags,
  };
  EmojiSection const *select_section = sections[0];
  gsize i;

  /* Figure out which section the current scroll position is within */
  for (i = 0; i < G_N_ELEMENTS (sections); ++i)
    {
      EmojiSection const *section = sections[i];
      GtkAllocation alloc;

      if (!gtk_widget_get_visible (section->box))
        continue;

      if (section->heading)
        gtk_widget_get_allocation (section->heading, &alloc);
      else
        gtk_widget_get_allocation (section->box, &alloc);

      if (value < alloc.y - BOX_SPACE)
        break;

      select_section = section;
    }

  /* Un/Check the section buttons accordingly */
  for (i = 0; i < G_N_ELEMENTS (sections); ++i)
    {
      EmojiSection const *section = sections[i];

      if (section == select_section)
        gtk_widget_set_state_flags (section->button, GTK_STATE_FLAG_CHECKED, FALSE);
      else
        gtk_widget_unset_state_flags (section->button, GTK_STATE_FLAG_CHECKED);
    }
}

static gboolean
match_tokens (const char **term_tokens,
              const char **hit_tokens)
{
  int i, j;
  gboolean matched;

  matched = TRUE;

  for (i = 0; term_tokens[i]; i++)
    {
      for (j = 0; hit_tokens[j]; j++)
        if (g_str_has_prefix (hit_tokens[j], term_tokens[i]))
          goto one_matched;

      matched = FALSE;
      break;

one_matched:
      continue;
    }

  return matched;
}

static gboolean
filter_func (GtkFlowBoxChild *child,
             gpointer         data)
{
  EmojiSection *section = data;
  GtkEmojiChooser *chooser;
  GVariant *emoji_data;
  const char *text;
  const char *name;
  char **term_tokens;
  char **name_tokens;
  gboolean res;

  res = TRUE;

  chooser = GTK_EMOJI_CHOOSER (gtk_widget_get_ancestor (GTK_WIDGET (child), GTK_TYPE_EMOJI_CHOOSER));
  text = gtk_entry_get_text (GTK_ENTRY (chooser->search_entry));
  emoji_data = (GVariant *) g_object_get_data (G_OBJECT (child), "emoji-data");

  if (text[0] == 0)
    goto out;

  if (!emoji_data)
    goto out;

  term_tokens = g_str_tokenize_and_fold (text, "en", NULL);
  
  g_variant_get_child (emoji_data, 1, "&s", &name);
  name_tokens = g_str_tokenize_and_fold (name, "en", NULL);

  res = match_tokens ((const char **)term_tokens, (const char **)name_tokens);

  if (g_variant_is_of_type (emoji_data, G_VARIANT_TYPE ("(ausasu)")))
    {
      const char **keywords;

      g_variant_get_child (emoji_data, 2, "^a&s", &keywords);
      res |= match_tokens ((const char **)term_tokens, keywords);
    }

  g_strfreev (term_tokens);
  g_strfreev (name_tokens);

out:
  if (res)
    section->empty = FALSE;

  return res;
}

static void
invalidate_section (EmojiSection *section)
{
  section->empty = TRUE;
  gtk_flow_box_invalidate_filter (GTK_FLOW_BOX (section->box));
}

static void
update_headings (GtkEmojiChooser *chooser)
{
  gtk_widget_set_visible (chooser->people.heading, !chooser->people.empty);
  gtk_widget_set_visible (chooser->people.box, !chooser->people.empty);
  gtk_widget_set_visible (chooser->body.heading, !chooser->body.empty);
  gtk_widget_set_visible (chooser->body.box, !chooser->body.empty);
  gtk_widget_set_visible (chooser->nature.heading, !chooser->nature.empty);
  gtk_widget_set_visible (chooser->nature.box, !chooser->nature.empty);
  gtk_widget_set_visible (chooser->food.heading, !chooser->food.empty);
  gtk_widget_set_visible (chooser->food.box, !chooser->food.empty);
  gtk_widget_set_visible (chooser->travel.heading, !chooser->travel.empty);
  gtk_widget_set_visible (chooser->travel.box, !chooser->travel.empty);
  gtk_widget_set_visible (chooser->activities.heading, !chooser->activities.empty);
  gtk_widget_set_visible (chooser->activities.box, !chooser->activities.empty);
  gtk_widget_set_visible (chooser->objects.heading, !chooser->objects.empty);
  gtk_widget_set_visible (chooser->objects.box, !chooser->objects.empty);
  gtk_widget_set_visible (chooser->symbols.heading, !chooser->symbols.empty);
  gtk_widget_set_visible (chooser->symbols.box, !chooser->symbols.empty);
  gtk_widget_set_visible (chooser->flags.heading, !chooser->flags.empty);
  gtk_widget_set_visible (chooser->flags.box, !chooser->flags.empty);

  if (chooser->recent.empty && chooser->people.empty &&
      chooser->body.empty && chooser->nature.empty &&
      chooser->food.empty && chooser->travel.empty &&
      chooser->activities.empty && chooser->objects.empty &&
      chooser->symbols.empty && chooser->flags.empty)
    gtk_stack_set_visible_child_name (GTK_STACK (chooser->stack), "empty");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (chooser->stack), "list");
}

static void
search_changed (GtkEntry *entry,
                gpointer  data)
{
  GtkEmojiChooser *chooser = data;

  invalidate_section (&chooser->recent);
  invalidate_section (&chooser->people);
  invalidate_section (&chooser->body);
  invalidate_section (&chooser->nature);
  invalidate_section (&chooser->food);
  invalidate_section (&chooser->travel);
  invalidate_section (&chooser->activities);
  invalidate_section (&chooser->objects);
  invalidate_section (&chooser->symbols);
  invalidate_section (&chooser->flags);

  update_headings (chooser);
}

static void
setup_section (GtkEmojiChooser *chooser,
               EmojiSection    *section,
               int              group,
               const char      *icon)
{
  GtkAdjustment *adj;
  GtkWidget *image;

  section->group = group;

  image = gtk_bin_get_child (GTK_BIN (section->button));
  gtk_image_set_from_icon_name (GTK_IMAGE (image), icon, GTK_ICON_SIZE_BUTTON);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));

  gtk_container_set_focus_vadjustment (GTK_CONTAINER (section->box), adj);
  gtk_flow_box_set_filter_func (GTK_FLOW_BOX (section->box), filter_func, section, NULL);
  g_signal_connect (section->button, "clicked", G_CALLBACK (scroll_to_section), section);
}

static void
gtk_emoji_chooser_init (GtkEmojiChooser *chooser)
{
  GtkAdjustment *adj;

  chooser->settings = g_settings_new ("org.gtk.Settings.EmojiChooser");

  gtk_widget_init_template (GTK_WIDGET (chooser));

  /* Get a reasonable maximum width for an emoji. We do this to
   * skip overly wide fallback rendering for certain emojis the
   * font does not contain and therefore end up being rendered
   * as multiply glyphs.
   */
  {
    PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (chooser), "🙂");
    PangoAttrList *attrs;
    PangoRectangle rect;

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
    pango_layout_set_attributes (layout, attrs);
    pango_attr_list_unref (attrs);

    pango_layout_get_extents (layout, &rect, NULL);
    chooser->emoji_max_width = rect.width;

    g_object_unref (layout);
  }

  chooser->recent_long_press = gtk_gesture_long_press_new (chooser->recent.box);
  g_signal_connect (chooser->recent_long_press, "pressed", G_CALLBACK (long_pressed_cb), chooser);
  chooser->recent_multi_press = gtk_gesture_multi_press_new (chooser->recent.box);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (chooser->recent_multi_press), GDK_BUTTON_SECONDARY);
  g_signal_connect (chooser->recent_multi_press, "pressed", G_CALLBACK (pressed_cb), chooser);

  chooser->people_long_press = gtk_gesture_long_press_new (chooser->people.box);
  g_signal_connect (chooser->people_long_press, "pressed", G_CALLBACK (long_pressed_cb), chooser);
  chooser->people_multi_press = gtk_gesture_multi_press_new (chooser->people.box);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (chooser->people_multi_press), GDK_BUTTON_SECONDARY);
  g_signal_connect (chooser->people_multi_press, "pressed", G_CALLBACK (pressed_cb), chooser);

  chooser->body_long_press = gtk_gesture_long_press_new (chooser->body.box);
  g_signal_connect (chooser->body_long_press, "pressed", G_CALLBACK (long_pressed_cb), chooser);
  chooser->body_multi_press = gtk_gesture_multi_press_new (chooser->body.box);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (chooser->body_multi_press), GDK_BUTTON_SECONDARY);
  g_signal_connect (chooser->body_multi_press, "pressed", G_CALLBACK (pressed_cb), chooser);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));
  g_signal_connect (adj, "value-changed", G_CALLBACK (adj_value_changed), chooser);

  setup_section (chooser, &chooser->recent, -1, "emoji-recent-symbolic");
  setup_section (chooser, &chooser->people, 0, "emoji-people-symbolic");
  setup_section (chooser, &chooser->body, 1, "emoji-body-symbolic");
  setup_section (chooser, &chooser->nature, 3, "emoji-nature-symbolic");
  setup_section (chooser, &chooser->food, 4, "emoji-food-symbolic");
  setup_section (chooser, &chooser->travel, 5, "emoji-travel-symbolic");
  setup_section (chooser, &chooser->activities, 6, "emoji-activities-symbolic");
  setup_section (chooser, &chooser->objects, 7, "emoji-objects-symbolic");
  setup_section (chooser, &chooser->symbols, 8, "emoji-symbols-symbolic");
  setup_section (chooser, &chooser->flags, 9, "emoji-flags-symbolic");

  populate_recent_section (chooser);

  chooser->populate_idle = g_idle_add (populate_emoji_chooser, chooser);
  g_source_set_name_by_id (chooser->populate_idle, "[gtk] populate_emoji_chooser");
}

static void
gtk_emoji_chooser_show (GtkWidget *widget)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (widget);
  GtkAdjustment *adj;

  GTK_WIDGET_CLASS (gtk_emoji_chooser_parent_class)->show (widget);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));
  gtk_adjustment_set_value (adj, 0);
  adj_value_changed (adj, chooser);

  gtk_entry_set_text (GTK_ENTRY (chooser->search_entry), "");
}

static void
gtk_emoji_chooser_class_init (GtkEmojiChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_emoji_chooser_finalize;
  widget_class->show = gtk_emoji_chooser_show;

  signals[EMOJI_PICKED] = g_signal_new ("emoji-picked",
                                        G_OBJECT_CLASS_TYPE (object_class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 1, G_TYPE_STRING|G_SIGNAL_TYPE_STATIC_SCOPE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkemojichooser.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, scrolled_window);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, recent.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, recent.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, people.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, people.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, people.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, body.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, body.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, body.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, nature.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, nature.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, nature.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, food.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, food.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, food.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, travel.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, travel.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, travel.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, activities.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, activities.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, activities.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, objects.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, objects.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, objects.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, symbols.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, symbols.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, symbols.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, flags.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, flags.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, flags.button);

  gtk_widget_class_bind_template_callback (widget_class, emoji_activated);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
}

GtkWidget *
gtk_emoji_chooser_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_EMOJI_CHOOSER, NULL));
}
