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
#include "gtkgesturelongpress.h"
#include "gtkpopover.h"
#include "gtkscrolledwindow.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtksearchentryprivate.h"
#include "gtkstylecontext.h"
#include "gtktext.h"

#define BOX_SPACE 6

typedef struct {
  GtkWidget *box;
  GtkWidget *heading;
  GtkWidget *button;
  const char *first;
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
  gboolean empty = FALSE;

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
      GVariant *item2 = g_object_get_data (G_OBJECT (l->data), "emoji-data");
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

static void
emoji_activated (GtkFlowBox      *box,
                 GtkFlowBoxChild *child,
                 gpointer         data)
{
  GtkEmojiChooser *chooser = data;
  char *text;
  GtkWidget *label;
  GVariant *item;
  gunichar modifier;

  gtk_popover_popdown (GTK_POPOVER (chooser));

  label = gtk_bin_get_child (GTK_BIN (child));
  text = g_strdup (gtk_label_get_label (GTK_LABEL (label)));

  item = (GVariant*) g_object_get_data (G_OBJECT (child), "emoji-data");
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
  g_object_set (box, "accept-unpaired-release", TRUE, NULL);
  gtk_container_add (GTK_CONTAINER (popover), view);
  gtk_container_add (GTK_CONTAINER (view), box);

  g_signal_connect (box, "child-activated", G_CALLBACK (emoji_activated), parent_popover);

  add_emoji (box, FALSE, emoji_data, 0, chooser);
  for (modifier = 0x1f3fb; modifier <= 0x1f3ff; modifier++)
    add_emoji (box, FALSE, emoji_data, modifier, chooser);

  gtk_popover_popup (GTK_POPOVER (popover));
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

  if (chooser)
    g_signal_connect (child, "popup-menu", G_CALLBACK (popup_menu), chooser);

  gtk_container_add (GTK_CONTAINER (child), label);
  gtk_flow_box_insert (GTK_FLOW_BOX (box), child, prepend ? 0 : -1);
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
      GBytes *bytes = g_resources_lookup_data ("/org/gtk/libgtk/emoji/emoji.data", 0, NULL);
      chooser->data = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(auss)"), bytes, TRUE));
      g_bytes_unref (bytes);
    }

  if (!chooser->iter)
    {
      chooser->iter = g_variant_iter_new (chooser->data);
      chooser->box = chooser->people.box;
    }

  while ((item = g_variant_iter_next_value (chooser->iter)))
    {
      const char *name;

      g_variant_get_child (item, 1, "&s", &name);

      if (strcmp (name, chooser->body.first) == 0)
        chooser->box = chooser->body.box;
      else if (strcmp (name, chooser->nature.first) == 0)
        chooser->box = chooser->nature.box;
      else if (strcmp (name, chooser->food.first) == 0)
        chooser->box = chooser->food.box;
      else if (strcmp (name, chooser->travel.first) == 0)
        chooser->box = chooser->travel.box;
      else if (strcmp (name, chooser->activities.first) == 0)
        chooser->box = chooser->activities.box;
      else if (strcmp (name, chooser->objects.first) == 0)
        chooser->box = chooser->objects.box;
      else if (strcmp (name, chooser->symbols.first) == 0)
        chooser->box = chooser->symbols.box;
      else if (strcmp (name, chooser->flags.first) == 0)
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
filter_func (GtkFlowBoxChild *child,
             gpointer         data)
{
  EmojiSection *section = data;
  GtkEmojiChooser *chooser;
  GVariant *emoji_data;
  const char *text;
  const char *name;
  gboolean res;

  res = TRUE;

  chooser = GTK_EMOJI_CHOOSER (gtk_widget_get_ancestor (GTK_WIDGET (child), GTK_TYPE_EMOJI_CHOOSER));
  text = gtk_editable_get_text (GTK_EDITABLE (chooser->search_entry));
  emoji_data = (GVariant *) g_object_get_data (G_OBJECT (child), "emoji-data");

  if (text[0] == 0)
    goto out;

  if (!emoji_data)
    goto out;

  g_variant_get_child (emoji_data, 1, "&s", &name);
  res = g_str_match_string (text, name, TRUE);

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
stop_search (GtkEntry *entry,
             gpointer  data)
{
  gtk_popover_popdown (GTK_POPOVER (data));
}

static void
setup_section (GtkEmojiChooser *chooser,
               EmojiSection   *section,
               const char     *first,
               const char     *icon)
{
  GtkAdjustment *adj;

  section->first = first;

  gtk_button_set_icon_name (GTK_BUTTON (section->button), icon);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));

  gtk_container_set_focus_vadjustment (GTK_CONTAINER (section->box), adj);
  gtk_flow_box_set_filter_func (GTK_FLOW_BOX (section->box), filter_func, section, NULL);
  g_signal_connect (section->button, "clicked", G_CALLBACK (scroll_to_section), section);
}

static void
gtk_emoji_chooser_init (GtkEmojiChooser *chooser)
{
  GtkAdjustment *adj;
  GtkText *text;

  chooser->settings = g_settings_new ("org.gtk.gtk4.Settings.EmojiChooser");

  gtk_widget_init_template (GTK_WIDGET (chooser));

  text = gtk_search_entry_get_text_widget (GTK_SEARCH_ENTRY (chooser->search_entry));
  gtk_text_set_input_hints (text, GTK_INPUT_HINT_NO_EMOJI);

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

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));
  g_signal_connect (adj, "value-changed", G_CALLBACK (adj_value_changed), chooser);

  setup_section (chooser, &chooser->recent, NULL, "emoji-recent-symbolic");
  setup_section (chooser, &chooser->people, "grinning face", "emoji-people-symbolic");
  setup_section (chooser, &chooser->body, "selfie", "emoji-body-symbolic");
  setup_section (chooser, &chooser->nature, "monkey face", "emoji-nature-symbolic");
  setup_section (chooser, &chooser->food, "grapes", "emoji-food-symbolic");
  setup_section (chooser, &chooser->travel, "globe showing Europe-Africa", "emoji-travel-symbolic");
  setup_section (chooser, &chooser->activities, "jack-o-lantern", "emoji-activities-symbolic");
  setup_section (chooser, &chooser->objects, "muted speaker", "emoji-objects-symbolic");
  setup_section (chooser, &chooser->symbols, "ATM sign", "emoji-symbols-symbolic");
  setup_section (chooser, &chooser->flags, "chequered flag", "emoji-flags-symbolic");

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

  gtk_editable_set_text (GTK_EDITABLE (chooser->search_entry), "");
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
  gtk_widget_class_bind_template_callback (widget_class, stop_search);
  gtk_widget_class_bind_template_callback (widget_class, pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, long_pressed_cb);
}

GtkWidget *
gtk_emoji_chooser_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_EMOJI_CHOOSER, NULL));
}
