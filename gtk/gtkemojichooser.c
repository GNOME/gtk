/* gtkemojichooser.c: An Emoji chooser widget
 * Copyright 2017, Red Hat, Inc.
 * Copyright 2026, Christian Hergert
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
#include "gtkemojidataprivate.h"
#include "gtkemojiitemprivate.h"

#include "gtkadjustment.h"
#include "gtkbitset.h"
#include "gtkbutton.h"
#include "gtkcssstylechangeprivate.h"
#include "gtkcustomfilter.h"
#include "gtkentry.h"
#include "gtkfilterlistmodel.h"
#include "gtkflattenlistmodel.h"
#include "gtkgestureclick.h"
#include "gtkgesturelongpress.h"
#include "gtkgridview.h"
#include "gtkinscription.h"
#include "gtkinscriptionprivate.h"
#include "gtklabel.h"
#include "gtklistbaseprivate.h"
#include "gtklistheader.h"
#include "gtklistitembaseprivate.h"
#include "gtklistitem.h"
#include "gtknoselection.h"
#include "gtkpopover.h"
#include "gtkscrolledwindow.h"
#include "gtksearchentryprivate.h"
#include "gtksignallistitemfactory.h"
#include "gtkslicelistmodel.h"
#include "gtkshortcut.h"
#include "gtkshortcutcontroller.h"
#include "gtkshortcuttrigger.h"
#include "gtkstack.h"
#include "gtktext.h"
#include "gtkwidgetprivate.h"

static void grid_schedule_font_invalidation (GtkEmojiChooser *chooser);
static void show_variations                 (GtkEmojiChooser *chooser,
                                             GtkListItem     *list_item);

/**
 * GtkEmojiChooser:
 *
 * Used by text widgets to let users insert Emoji characters.
 *
 * <picture>
 *   <source srcset="emojichooser-dark.png" media="(prefers-color-scheme: dark)">
 *   <img alt="An example GtkEmojiChooser" src="emojichooser.png">
 * </picture>
 *
 * `GtkEmojiChooser` emits the [signal@Gtk.EmojiChooser::emoji-picked]
 * signal when an Emoji is selected.
 *
 * # Shortcuts and Gestures
 *
 * `GtkEmojiChooser` supports the following keyboard shortcuts:
 *
 * - <kbd>Ctrl</kbd>+<kbd>N</kbd> scrolls to the next section.
 * - <kbd>Ctrl</kbd>+<kbd>P</kbd> scrolls to the previous section.
 * - <kbd>Enter</kbd> to select the first emoji result.
 *
 * # Actions
 *
 * `GtkEmojiChooser` defines a set of built-in actions:
 *
 * - `scroll.section` scrolls to the next or previous section.
 *
 * # CSS nodes
 *
 * ```
 * popover
 * ├── box.emoji-searchbar
 * │   ╰── entry.search
 * ╰── box.emoji-toolbar
 *     ├── button.image-button.emoji-section
 *     ├── ...
 *     ╰── button.image-button.emoji-section
 * ```
 *
 * Every `GtkEmojiChooser` consists of a main node called popover.
 * The contents of the popover are largely implementation defined
 * and supposed to inherit general styles.
 * The top searchbar used to search emoji and gets the .emoji-searchbar
 * style class itself.
 * The bottom toolbar used to switch between different emoji categories
 * consists of buttons with the .emoji-section style class and gets the
 * .emoji-toolbar style class itself.
 */

typedef struct
{
  GtkWidget  *button;
  GListModel *model;
  int         group;
} EmojiSection;

struct _GtkEmojiChooser
{
  GtkPopover            parent_instance;

  GtkWidget            *search_entry;
  GtkWidget            *stack;
  GtkWidget            *grid_scroller;
  GtkWidget            *grid_view;

  GtkEmojiDatabase     *database;
  GtkFilterListModel   *page;
  GtkCustomFilter      *search_filter;
  GtkCustomFilter      *support_filter;
  char                **search_tokens;
  GtkFlattenListModel  *browse;
  GListStore           *recent_items;
  GtkListItemFactory   *header_factory;
  GtkListItemFactory   *variation_factory;
  EmojiSection          sections[10];
  guint                 selected_section;
  guint                 browse_anchor;
  double                browse_align;
  GtkEmojiItem         *browse_item;

  GHashTable           *bound_items; /* Borrowed GtkListItem */
  GtkBitset            *tested_emoji;
  GtkBitset            *unsupported_emoji;
  GHashTable           *unsupported_standalone; /* UTF-8 text includes the modifier. */
  GSettings            *settings;
  int                   emoji_max_width;
  guint                 rejected_idle;
  guint                 font_idle;

  GtkWidget            *variation_popover;
  GtkListItem          *variation_anchor;

  gboolean              disposing;
  gboolean              searching;
  gboolean              support_filter_active;
  gboolean              rejected_pending;
};

struct _GtkEmojiChooserClass
{
  GtkPopoverClass parent_class;
};

enum
{
  EMOJI_PICKED,
  LAST_SIGNAL
};

static int signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkEmojiChooser, gtk_emoji_chooser, GTK_TYPE_POPOVER)

static void grid_activated (GtkGridView     *grid,
                            guint            position,
                            GtkEmojiChooser *chooser);
static void search_changed (GtkEntry        *entry,
                            gpointer         data);

static GListModel *
current_model (GtkEmojiChooser *chooser)
{
  return chooser->searching ? G_LIST_MODEL (chooser->page) : G_LIST_MODEL (chooser->browse);
}

static void
update_sections (GtkEmojiChooser *chooser)
{
  for (guint i = 0; i < G_N_ELEMENTS (chooser->sections); i++)
    gtk_widget_set_sensitive (chooser->sections[i].button,
                              g_list_model_get_n_items (chooser->sections[i].model) > 0);

  gtk_stack_set_visible_child_name (GTK_STACK (chooser->stack),
                                    g_list_model_get_n_items (current_model (chooser)) ||
                                    (chooser->searching &&
                                     gtk_filter_list_model_get_pending (chooser->page))
                                      ? "grid" : "empty");
}

static void
model_items_changed (GListModel      *model,
                     guint            position,
                     guint            removed,
                     guint            added,
                     GtkEmojiChooser *chooser)
{
  update_sections (chooser);
}

static void
search_pending_changed (GtkFilterListModel *model,
                        GParamSpec         *pspec,
                        GtkEmojiChooser    *chooser)
{
  update_sections (chooser);
}

static gboolean
emoji_is_supported (gpointer item,
                    gpointer data)
{
  GtkEmojiChooser *chooser = data;
  guint id = gtk_emoji_item_get_id (item);
  char *text;
  gboolean supported;

  if (id != G_MAXUINT)
    return !gtk_bitset_contains (chooser->unsupported_emoji, id);

  text = gtk_emoji_item_dup_text (item);
  supported = !g_hash_table_contains (chooser->unsupported_standalone, text);
  g_free (text);

  return supported;
}

static gboolean
search_matches (gpointer item,
                gpointer data)
{
  GtkEmojiChooser *chooser = data;
  GVariant *record;
  guint group;
  gboolean matches;

  if (!emoji_is_supported (item, chooser))
    return FALSE;

  record = gtk_emoji_item_dup_record (item);
  g_variant_get_child (record, 5, "u", &group);
  matches = group != 2 &&
            gtk_emoji_data_matches (record, (const char **) chooser->search_tokens);
  g_variant_unref (record);

  return matches;
}

static gboolean
grid_flush_rejected (gpointer data)
{
  GtkEmojiChooser *chooser = data;

  chooser->rejected_idle = 0;
  chooser->rejected_pending = FALSE;

  if (!chooser->support_filter_active)
    {
      chooser->support_filter_active = TRUE;
      gtk_custom_filter_set_filter_func (chooser->support_filter,
                                         emoji_is_supported,
                                         chooser,
                                         NULL);
    }
  else
    {
      gtk_filter_changed (GTK_FILTER (chooser->support_filter), GTK_FILTER_CHANGE_MORE_STRICT);
    }

  if (chooser->searching)
    gtk_filter_changed (GTK_FILTER (chooser->search_filter), GTK_FILTER_CHANGE_MORE_STRICT);

  update_sections (chooser);

  return G_SOURCE_REMOVE;
}

static void
gtk_emoji_chooser_finalize (GObject *object)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (object);

  g_clear_object (&chooser->settings);
  g_clear_object (&chooser->page);
  g_clear_object (&chooser->search_filter);
  g_clear_object (&chooser->support_filter);
  g_clear_pointer (&chooser->search_tokens, g_strfreev);
  g_clear_object (&chooser->browse_item);
  g_clear_object (&chooser->browse);
  g_clear_object (&chooser->recent_items);

  for (guint i = 0; i < G_N_ELEMENTS (chooser->sections); i++)
    g_clear_object (&chooser->sections[i].model);

  g_clear_object (&chooser->database);
  g_clear_pointer (&chooser->bound_items, g_hash_table_unref);
  g_clear_pointer (&chooser->tested_emoji, gtk_bitset_unref);
  g_clear_pointer (&chooser->unsupported_emoji, gtk_bitset_unref);
  g_clear_pointer (&chooser->unsupported_standalone, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_emoji_chooser_parent_class)->finalize (object);
}

static void
clear_variation_popover (GtkEmojiChooser *chooser)
{
  if (chooser->variation_popover != NULL)
    {
      gtk_widget_unparent (chooser->variation_popover);
      g_clear_object (&chooser->variation_popover);
    }

  chooser->variation_anchor = NULL;
}

static void
gtk_emoji_chooser_dispose (GObject *object)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (object);

  chooser->disposing = TRUE;

  g_clear_handle_id (&chooser->rejected_idle, g_source_remove);
  g_clear_handle_id (&chooser->font_idle, g_source_remove);

  clear_variation_popover (chooser);

  if (chooser->grid_view != NULL)
    {
      GtkSelectionModel *model = gtk_grid_view_get_model (GTK_GRID_VIEW (chooser->grid_view));

      gtk_no_selection_set_model (GTK_NO_SELECTION (model), NULL);
    }

  for (guint i = 0; i < G_N_ELEMENTS (chooser->sections); i++)
    {
      if (chooser->sections[i].model != NULL)
        g_signal_handlers_disconnect_by_data (chooser->sections[i].model, chooser);
    }

  if (chooser->page != NULL)
    g_signal_handlers_disconnect_by_data (chooser->page, chooser);

  if (chooser->search_filter != NULL)
    gtk_custom_filter_set_filter_func (chooser->search_filter, NULL, NULL, NULL);

  if (chooser->support_filter != NULL)
    gtk_custom_filter_set_filter_func (chooser->support_filter, NULL, NULL, NULL);

  gtk_widget_dispose_template (GTK_WIDGET (object), GTK_TYPE_EMOJI_CHOOSER);

  G_OBJECT_CLASS (gtk_emoji_chooser_parent_class)->dispose (object);
}

static guint
section_offset (GtkEmojiChooser *chooser,
                guint            section)
{
  guint position = 0;

  for (guint i = 0; i < section; i++)
    position += g_list_model_get_n_items (chooser->sections[i].model);

  return position;
}

static void
scroll_to_section (GtkEmojiChooser *chooser,
                   guint            index)
{
  EmojiSection *section = &chooser->sections[index];

  gtk_editable_set_text (GTK_EDITABLE (chooser->search_entry), "");

  search_changed (NULL, chooser);

  if (g_list_model_get_n_items (section->model) == 0)
    return;

  chooser->selected_section = index;

  gtk_list_base_set_anchor (GTK_LIST_BASE (chooser->grid_view),
                            section_offset (chooser, index),
                            0,
                            GTK_PACK_START,
                            0.1,
                            GTK_PACK_START);
}

static void
section_clicked (GtkButton       *button,
                 GtkEmojiChooser *chooser)
{
  for (guint i = 0; i < G_N_ELEMENTS (chooser->sections); i++)
    {
      if (chooser->sections[i].button == GTK_WIDGET (button))
        {
          scroll_to_section (chooser, i);
          return;
        }
    }
}

static void
populate_recent_section (GtkEmojiChooser *chooser)
{
  GVariant *saved = g_settings_get_value (chooser->settings, "recently-used-emoji");

  gtk_emoji_recent_load (chooser->recent_items, saved);
  update_sections (chooser);
  g_variant_unref (saved);
}

static void
add_recent_item (GtkEmojiChooser *chooser,
                 GtkEmojiItem    *item)
{
  gtk_emoji_recent_add (chooser->recent_items, item);
  g_settings_set_value (chooser->settings,
                        "recently-used-emoji",
                        gtk_emoji_recent_serialize (chooser->recent_items));
  update_sections (chooser);
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
  device = gdk_seat_get_keyboard (seat);
  state = gdk_device_get_modifier_state (device);

  return (state & GDK_CONTROL_MASK) == 0;
}

static void
search_changed (GtkEntry *entry,
                gpointer  data)
{
  GtkEmojiChooser *chooser = data;
  const char *text = gtk_editable_get_text (GTK_EDITABLE (chooser->search_entry));
  GtkNoSelection *selection;

  gboolean searching = text[0] != 0;

  if (chooser->database == NULL)
    return;

  selection = GTK_NO_SELECTION (gtk_grid_view_get_model (GTK_GRID_VIEW (chooser->grid_view)));

  if (searching)
    {
      if (!chooser->searching)
        {
          GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->grid_scroller));
          GdkRectangle area = { 0 };

          chooser->browse_anchor = gtk_list_base_get_anchor (GTK_LIST_BASE (chooser->grid_view));
          chooser->browse_align = 0;

          g_clear_object (&chooser->browse_item);
          chooser->browse_item = g_list_model_get_item (G_LIST_MODEL (chooser->browse), chooser->browse_anchor);

          if (GTK_LIST_BASE_GET_CLASS (chooser->grid_view)->get_allocation (GTK_LIST_BASE (chooser->grid_view),
                                                                            chooser->browse_anchor,
                                                                            &area))
            chooser->browse_align = (area.y - gtk_adjustment_get_value (adj))
                                  / MAX (1, gtk_widget_get_height (chooser->grid_view));
        }

      g_clear_pointer (&chooser->search_tokens, g_strfreev);
      chooser->search_tokens = g_str_tokenize_and_fold (text, "en", NULL);

      if (!chooser->searching)
        gtk_custom_filter_set_filter_func (chooser->search_filter,
                                           search_matches,
                                           chooser,
                                           NULL);
      else
        gtk_filter_changed (GTK_FILTER (chooser->search_filter), GTK_FILTER_CHANGE_DIFFERENT);
    }
  else if (!chooser->searching)
    {
      return;
    }

  if (!searching && chooser->browse_item != NULL)
    {
      guint id = gtk_emoji_item_get_id (chooser->browse_item);
      guint offset = g_list_model_get_n_items (chooser->sections[0].model);

      if (id != G_MAXUINT)
        {
          GVariant *record = gtk_emoji_item_dup_record (chooser->browse_item);
          guint group;

          g_variant_get_child (record, 5, "u", &group);
          g_variant_unref (record);

          for (guint i = 1; i < G_N_ELEMENTS (chooser->sections); i++)
            {
              guint group_offset;
              guint group_size;
              guint rejected;

              if (chooser->sections[i].group != group)
                {
                  offset += g_list_model_get_n_items (chooser->sections[i].model);
                  continue;
                }

              gtk_emoji_database_get_group_range (chooser->database,
                                                  group,
                                                  &group_offset,
                                                  &group_size);

              if (id >= group_offset &&
                  id < group_offset + group_size &&
                  !gtk_bitset_contains (chooser->unsupported_emoji, id))
                {
                  rejected = id > group_offset
                           ? gtk_bitset_get_size_in_range (chooser->unsupported_emoji, group_offset, id - 1)
                           : 0;
                  chooser->browse_anchor = offset + id - group_offset - rejected;
                }

              break;
            }
        }
      else
        {
          GVariant *record = gtk_emoji_item_dup_record (chooser->browse_item);
          gunichar modifier = gtk_emoji_item_get_modifier (chooser->browse_item);

          for (guint i = 0; i < offset; i++)
            {
              GtkEmojiItem *item = g_list_model_get_item (chooser->sections[0].model, i);
              GVariant *other;
              gboolean matches;

              other = gtk_emoji_item_dup_record (item);
              matches = modifier == gtk_emoji_item_get_modifier (item) &&
                        g_variant_equal (record, other);
              g_variant_unref (other);
              g_object_unref (item);

              if (matches)
                {
                  chooser->browse_anchor = i;
                  break;
                }
            }

          g_variant_unref (record);
        }
    }

  chooser->searching = searching;
  if (gtk_no_selection_get_model (selection) != current_model (chooser))
    {
      gtk_grid_view_set_header_factory (GTK_GRID_VIEW (chooser->grid_view),
                                        searching ? NULL : chooser->header_factory);
      gtk_no_selection_set_model (selection, current_model (chooser));
    }

  if (!searching)
    {
      gtk_custom_filter_set_filter_func (chooser->search_filter, NULL, NULL, NULL);
      g_clear_pointer (&chooser->search_tokens, g_strfreev);
    }

  gtk_list_base_set_anchor (GTK_LIST_BASE (chooser->grid_view),
                            searching ? 0 : chooser->browse_anchor,
                            0,
                            GTK_PACK_START,
                            searching ? 0 : chooser->browse_align,
                            GTK_PACK_START);
  update_sections (chooser);
}

static void
stop_search (GtkEntry *entry,
             gpointer  data)
{
  gtk_popover_popdown (GTK_POPOVER (data));
}

static void
activate_search (GtkEmojiChooser *chooser,
                 GtkEntry        *entry,
                 gpointer         data)
{
  if (g_list_model_get_n_items (current_model (chooser)) > 0)
    grid_activated (GTK_GRID_VIEW (chooser->grid_view), 0, chooser);
}

static void
grid_popup (GtkGesture  *gesture,
            double       x,
            double       y,
            GtkListItem *list_item)
{
  GtkWidget *child = gtk_list_item_get_child (list_item);
  GtkWidget *chooser = gtk_widget_get_ancestor (child, GTK_TYPE_EMOJI_CHOOSER);

  if (chooser != NULL)
    show_variations (GTK_EMOJI_CHOOSER (chooser), list_item);
}

static void
grid_pressed (GtkGestureClick *gesture,
              int              n_press,
              double           x,
              double           y,
              GtkListItem     *list_item)
{
  GtkWidget *child = gtk_list_item_get_child (list_item);
  GtkWidget *chooser = gtk_widget_get_ancestor (child, GTK_TYPE_EMOJI_CHOOSER);

  if (chooser != NULL)
    show_variations (GTK_EMOJI_CHOOSER (chooser), list_item);
}

static void
grid_popup_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GtkListItem *list_item = user_data;
  GtkWidget *cell = gtk_list_item_get_child (list_item);
  GtkWidget *chooser = gtk_widget_get_ancestor (cell, GTK_TYPE_EMOJI_CHOOSER);

  if (chooser != NULL)
    show_variations (GTK_EMOJI_CHOOSER (chooser), list_item);
}

static GtkWidget *
create_grid_cell (void)
{
  PangoAttrList *attrs = pango_attr_list_new ();
  GtkWidget *label = gtk_inscription_new (NULL);

  gtk_widget_set_size_request (label, 44, 44);
  gtk_widget_add_css_class (label, "emoji");
  gtk_inscription_set_min_chars (GTK_INSCRIPTION (label), 0);
  gtk_inscription_set_nat_chars (GTK_INSCRIPTION (label), 0);
  gtk_inscription_set_min_lines (GTK_INSCRIPTION (label), 0);
  gtk_inscription_set_nat_lines (GTK_INSCRIPTION (label), 0);
  gtk_inscription_set_xalign (GTK_INSCRIPTION (label), 0.5);
  gtk_inscription_set_yalign (GTK_INSCRIPTION (label), 0.5);
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  gtk_inscription_set_attributes (GTK_INSCRIPTION (label), attrs);
  pango_attr_list_unref (attrs);

  return label;
}

static void
grid_setup (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item,
            GtkEmojiChooser          *chooser)
{
  GSimpleActionGroup *actions = g_simple_action_group_new ();
  GSimpleAction *popup = g_simple_action_new ("popup", NULL);
  GtkEventController *shortcuts = gtk_shortcut_controller_new ();
  GtkWidget *label = create_grid_cell ();
  GtkGesture *long_press = gtk_gesture_long_press_new ();
  GtkGesture *right_click = gtk_gesture_click_new ();

  g_signal_connect_object (popup,
                           "activate",
                           G_CALLBACK (grid_popup_action),
                           list_item,
                           0);
  g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (popup));
  gtk_widget_insert_action_group (label, "menu", G_ACTION_GROUP (actions));
  g_object_unref (popup);
  g_object_unref (actions);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (shortcuts),
                                        gtk_shortcut_new (gtk_shortcut_trigger_create_for_menu (),
                                                          gtk_named_action_new ("menu.popup")));
  gtk_widget_add_controller (label, shortcuts);

  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (right_click), 3);
  g_signal_connect_object (long_press,
                           "pressed",
                           G_CALLBACK (grid_popup),
                           list_item,
                           0);
  g_signal_connect_object (right_click,
                           "pressed",
                           G_CALLBACK (grid_pressed),
                           list_item,
                           0);
  gtk_widget_add_controller (label, GTK_EVENT_CONTROLLER (long_press));
  gtk_widget_add_controller (label, GTK_EVENT_CONTROLLER (right_click));
  gtk_list_item_set_child (list_item, label);
}

static void
variation_setup (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item,
                 GtkEmojiChooser          *chooser)
{
  gtk_list_item_set_child (list_item, create_grid_cell ());
}

static gboolean
grid_validate_item (GtkEmojiChooser *chooser,
                    GtkInscription  *inscription,
                    GtkEmojiItem    *item)
{
  PangoLayout *layout;
  PangoRectangle rect;
  guint id;

  id = gtk_emoji_item_get_id (item);
  if (id != G_MAXUINT && gtk_bitset_contains (chooser->tested_emoji, id))
    return !gtk_bitset_contains (chooser->unsupported_emoji, id);

  if (chooser->emoji_max_width == 0)
    {
      PangoLayout *reference = gtk_widget_create_pango_layout (GTK_WIDGET (inscription), "🙂");
      PangoAttrList *reference_attrs = pango_attr_list_new ();
      PangoRectangle reference_rect;

      pango_attr_list_insert (reference_attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
      pango_layout_set_attributes (reference, reference_attrs);
      pango_layout_get_extents (reference, &reference_rect, NULL);
      chooser->emoji_max_width = reference_rect.width;
      pango_attr_list_unref (reference_attrs);
      g_object_unref (reference);
    }

  layout = pango_layout_copy (gtk_inscription_get_layout (inscription));
  pango_layout_set_width (layout, -1);
  pango_layout_set_height (layout, -1);
  pango_layout_get_extents (layout, &rect, NULL);

  if (id != G_MAXUINT)
    gtk_bitset_add (chooser->tested_emoji, id);

  if (pango_layout_get_unknown_glyphs_count (layout) > 0 ||
      (chooser->emoji_max_width > 0 &&
       rect.width >= 1.5 * chooser->emoji_max_width))
    {
      if (id != G_MAXUINT)
        gtk_bitset_add (chooser->unsupported_emoji, id);
      else
        g_hash_table_add (chooser->unsupported_standalone,
                          g_strdup (gtk_inscription_get_text (inscription)));

      chooser->rejected_pending = TRUE;
      if (chooser->rejected_idle == 0)
        chooser->rejected_idle = g_idle_add (grid_flush_rejected, chooser);

      g_object_unref (layout);

      return FALSE;
    }

  g_object_unref (layout);
  return TRUE;
}

static void
grid_viewport_changed (GtkAdjustment   *adjustment,
                       GtkEmojiChooser *chooser)
{
  double top = gtk_adjustment_get_value (adjustment);

  if (!chooser->searching)
    {
      guint selected = 0;
      guint offset = 0;

      for (guint i = 0; i < G_N_ELEMENTS (chooser->sections); i++)
        {
          GdkRectangle area = { 0 };

          guint n = g_list_model_get_n_items (chooser->sections[i].model);

          if (n > 0 &&
              GTK_LIST_BASE_GET_CLASS (chooser->grid_view)->get_allocation (GTK_LIST_BASE (chooser->grid_view), offset, &area) &&
              area.y <= top + 48)
            selected = i;

          offset += n;
        }

      chooser->selected_section = selected;
      for (guint i = 0; i < G_N_ELEMENTS (chooser->sections); i++)
        {
          if (i == selected)
            gtk_widget_set_state_flags (chooser->sections[i].button, GTK_STATE_FLAG_CHECKED, FALSE);
          else
            gtk_widget_unset_state_flags (chooser->sections[i].button, GTK_STATE_FLAG_CHECKED);
        }
    }
}

static void
grid_bind (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item,
           GtkEmojiChooser          *chooser)
{
  GtkEmojiItem *item = GTK_EMOJI_ITEM (gtk_list_item_get_item (list_item));
  GtkInscription *cell = GTK_INSCRIPTION (gtk_list_item_get_child (list_item));
  char *text = gtk_emoji_item_dup_text (item);
  GVariant *record = gtk_emoji_item_dup_record (item);
  const char *name;

  gtk_inscription_set_text (cell, text);
  g_variant_get_child (record, 2, "&s", &name);
  gtk_list_item_set_accessible_label (list_item, name);
  gtk_accessible_update_property (GTK_ACCESSIBLE (cell),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  name,
                                  -1);
  gtk_widget_set_sensitive (GTK_WIDGET (cell), grid_validate_item (chooser, cell, item));
  gtk_list_item_set_activatable (list_item, gtk_widget_get_sensitive (GTK_WIDGET (cell)));
  g_hash_table_add (chooser->bound_items, list_item);
  g_variant_unref (record);
  g_free (text);
}

static void
grid_unbind (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item,
             GtkEmojiChooser          *chooser)
{
  GtkWidget *cell = gtk_list_item_get_child (list_item);

  if (chooser->variation_anchor == list_item)
    clear_variation_popover (chooser);

  g_hash_table_remove (chooser->bound_items, list_item);
  gtk_list_item_set_accessible_label (list_item, NULL);
  gtk_accessible_reset_property (GTK_ACCESSIBLE (cell), GTK_ACCESSIBLE_PROPERTY_LABEL);
  gtk_inscription_set_text (GTK_INSCRIPTION (cell), NULL);
}

static void
grid_activated (GtkGridView     *grid,
                guint            position,
                GtkEmojiChooser *chooser)
{
  GtkEmojiItem *item = NULL;
  char *text = NULL;
  GtkWidget *popover;
  gboolean recent;

  item = g_list_model_get_item (G_LIST_MODEL (gtk_grid_view_get_model (grid)), position);
  if (item == NULL)
    return;

  if (!emoji_is_supported (item, chooser))
    {
      g_object_unref (item);
      return;
    }

  recent = GTK_WIDGET (grid) == chooser->grid_view && !chooser->searching &&
           position < g_list_model_get_n_items (chooser->sections[0].model);
  text = gtk_emoji_item_dup_text (item);
  popover = gtk_widget_get_ancestor (GTK_WIDGET (grid), GTK_TYPE_POPOVER);

  if (popover != NULL && popover != GTK_WIDGET (chooser))
    gtk_popover_popdown (GTK_POPOVER (popover));

  g_object_ref (chooser);

  if (!recent)
    add_recent_item (chooser, item);

  g_signal_emit (chooser, signals[EMOJI_PICKED], 0, text);

  if (gtk_widget_get_parent (GTK_WIDGET (chooser)) != NULL && should_close (chooser))
    gtk_popover_popdown (GTK_POPOVER (chooser));

  g_free (text);
  g_object_unref (item);
  g_object_unref (chooser);
}

static void
show_variations (GtkEmojiChooser *chooser,
                 GtkListItem     *list_item)
{
  GtkEmojiItem *item = GTK_EMOJI_ITEM (gtk_list_item_get_item (list_item));
  GVariant *record = NULL;
  GListStore *items;
  GtkFilterListModel *filter_model;
  GtkNoSelection *selection;
  GtkWidget *child = gtk_list_item_get_child (list_item);
  GtkWidget *grid;

  if (item == NULL || child == NULL)
    return;

  record = gtk_emoji_item_dup_record (item);
  if (!gtk_emoji_data_has_variations (record))
    {
      g_variant_unref (record);
      return;
    }

  clear_variation_popover (chooser);
  chooser->variation_popover = g_object_ref_sink (gtk_popover_new ());
  chooser->variation_anchor = list_item;
  gtk_widget_set_parent (chooser->variation_popover, child);
  items = g_list_store_new (GTK_TYPE_EMOJI_ITEM);

  for (guint i = 0; i < 6; i++)
    {
      GtkEmojiItem *variation = gtk_emoji_item_new (record, i ? 0x1f3fa + i : 0);

      g_list_store_append (items, variation);
      g_object_unref (variation);
    }

  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (items),
                                            g_object_ref (GTK_FILTER (chooser->support_filter)));
  selection = gtk_no_selection_new (G_LIST_MODEL (filter_model));
  grid = gtk_grid_view_new (GTK_SELECTION_MODEL (selection),
                            g_object_ref (chooser->variation_factory));
  gtk_grid_view_set_min_columns (GTK_GRID_VIEW (grid), 6);
  gtk_grid_view_set_max_columns (GTK_GRID_VIEW (grid), 6);
  gtk_grid_view_set_single_click_activate (GTK_GRID_VIEW (grid), TRUE);
  gtk_widget_add_css_class (grid, "view");
  g_signal_connect_object (grid,
                           "activate",
                           G_CALLBACK (grid_activated),
                           chooser,
                           0);
  gtk_popover_set_child (GTK_POPOVER (chooser->variation_popover), grid);
  gtk_popover_popup (GTK_POPOVER (chooser->variation_popover));
  g_variant_unref (record);
}

static void
header_setup (GtkSignalListItemFactory *factory,
              GtkListHeader            *header,
              GtkEmojiChooser          *chooser)
{
  GtkWidget *label = gtk_label_new (NULL);

  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);
  gtk_list_header_set_child (header, label);
}

static void
header_bind (GtkSignalListItemFactory *factory,
             GtkListHeader            *header,
             GtkEmojiChooser          *chooser)
{
  GListModel *model = gtk_flatten_list_model_get_model_for_item (chooser->browse,
                                                                 gtk_list_header_get_start (header));

  for (guint i = 0; i < G_N_ELEMENTS (chooser->sections); i++)
    if (chooser->sections[i].model == model)
      {
        gtk_label_set_text (GTK_LABEL (gtk_list_header_get_child (header)),
                            gtk_widget_get_tooltip_text (chooser->sections[i].button));
        break;
      }
}

static void
setup_grid (GtkEmojiChooser *chooser)
{
  static const int groups[] = { -1, 0, 1, 3, 4, 5, 6, 7, 8, 9 };
  GListStore *models;
  GListStore *search_models;
  GtkNoSelection *selection;

  chooser->bound_items = g_hash_table_new (g_direct_hash, g_direct_equal);
  chooser->tested_emoji = gtk_bitset_new_empty ();
  chooser->unsupported_emoji = gtk_bitset_new_empty ();
  chooser->unsupported_standalone = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  chooser->database = gtk_emoji_database_new ();
  /* Match-all filters leave database items lazy until a query or rejection. */
  chooser->search_filter = gtk_custom_filter_new (NULL, NULL, NULL);
  chooser->support_filter = gtk_custom_filter_new (NULL, NULL, NULL);
  chooser->recent_items = g_list_store_new (GTK_TYPE_EMOJI_ITEM);
  search_models = g_list_store_new (G_TYPE_LIST_MODEL);
  g_list_store_append (search_models, chooser->recent_items);
  g_list_store_append (search_models, chooser->database);
  chooser->page = gtk_filter_list_model_new (G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (search_models))),
                                             g_object_ref (GTK_FILTER (chooser->search_filter)));
  gtk_filter_list_model_set_incremental (chooser->page, TRUE);
  models = g_list_store_new (G_TYPE_LIST_MODEL);

  for (guint i = 0; i < G_N_ELEMENTS (chooser->sections); i++)
    {
      EmojiSection *section = &chooser->sections[i];

      section->group = groups[i];

      if (i == 0)
        {
          section->model = G_LIST_MODEL (gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (chooser->recent_items)),
                                                                    g_object_ref (GTK_FILTER (chooser->support_filter))));
        }
      else
        {
          guint group_offset;
          guint group_size;
          GtkSliceListModel *slice;

          gtk_emoji_database_get_group_range (chooser->database,
                                              section->group,
                                              &group_offset,
                                              &group_size);
          slice = gtk_slice_list_model_new (g_object_ref (G_LIST_MODEL (chooser->database)),
                                            group_offset,
                                            group_size);
          section->model = G_LIST_MODEL (gtk_filter_list_model_new (
                                          G_LIST_MODEL (slice),
                                                      g_object_ref (GTK_FILTER (chooser->support_filter))));
        }

      g_list_store_append (models, section->model);
      g_signal_connect_object (section->model,
                               "items-changed",
                               G_CALLBACK (model_items_changed),
                               chooser,
                               0);
    }

  /* Each child is one GridView section, including the recent items. */
  chooser->browse = gtk_flatten_list_model_new (G_LIST_MODEL (models));
  g_signal_connect_object (chooser->page,
                           "items-changed",
                           G_CALLBACK (model_items_changed),
                           chooser,
                           0);
  g_signal_connect_object (chooser->page,
                           "notify::pending",
                           G_CALLBACK (search_pending_changed),
                           chooser,
                           0);
  selection = gtk_no_selection_new (g_object_ref (G_LIST_MODEL (chooser->browse)));
  gtk_grid_view_set_model (GTK_GRID_VIEW (chooser->grid_view), GTK_SELECTION_MODEL (selection));
  g_object_unref (selection);
}

static void
gtk_emoji_chooser_init (GtkEmojiChooser *chooser)
{
  GtkText *text;

  chooser->settings = g_settings_new ("org.gtk.gtk4.Settings.EmojiChooser");

  gtk_widget_init_template (GTK_WIDGET (chooser));
  text = gtk_search_entry_get_text_widget (GTK_SEARCH_ENTRY (chooser->search_entry));
  gtk_text_set_input_hints (text, GTK_INPUT_HINT_NO_EMOJI);

  setup_grid (chooser);
  populate_recent_section (chooser);
}

static void
gtk_emoji_chooser_show (GtkWidget *widget)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (widget);

  GTK_WIDGET_CLASS (gtk_emoji_chooser_parent_class)->show (widget);
  scroll_to_section (chooser, g_list_model_get_n_items (chooser->sections[0].model) ? 0 : 1);
}

static void
gtk_emoji_chooser_scroll_section (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *parameter)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (widget);
  int direction = g_variant_get_int32 (parameter) > 0 ? 1 : -1;

  for (int i = (int) chooser->selected_section + direction;
       i >= 0 && i < G_N_ELEMENTS (chooser->sections);
       i += direction)
    {
      if (g_list_model_get_n_items (chooser->sections[i].model) > 0)
        {
          guint offset;

          scroll_to_section (chooser, i);

          offset = section_offset (chooser, i);
          for (guint j = 0; j < g_list_model_get_n_items (chooser->sections[i].model); j++)
            {
              GtkEmojiItem *item = g_list_model_get_item (chooser->sections[i].model, j);
              gboolean supported = emoji_is_supported (item, chooser);
              GtkWidget *focus_child;

              g_object_unref (item);
              if (!supported)
                continue;

              gtk_grid_view_scroll_to (GTK_GRID_VIEW (chooser->grid_view),
                                       offset + j,
                                       GTK_LIST_SCROLL_FOCUS,
                                       NULL);
              gtk_widget_grab_focus (chooser->grid_view);
              focus_child = gtk_widget_get_focus_child (chooser->grid_view);
              if (GTK_IS_LIST_ITEM_BASE (focus_child) &&
                  gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (focus_child)) == offset + j)
                break;
            }

          break;
        }
    }
}

static void
gtk_emoji_chooser_map (GtkWidget *widget)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (widget);

  if (chooser->rejected_pending)
    grid_flush_rejected (chooser);

  gtk_no_selection_set_model (GTK_NO_SELECTION (gtk_grid_view_get_model (GTK_GRID_VIEW (chooser->grid_view))),
                              current_model (chooser));

  GTK_WIDGET_CLASS (gtk_emoji_chooser_parent_class)->map (widget);

  gtk_widget_grab_focus (chooser->search_entry);
}

static void
gtk_emoji_chooser_unmap (GtkWidget *widget)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (widget);

  if (chooser->rejected_idle != 0)
    {
      g_source_remove (chooser->rejected_idle);
      chooser->rejected_idle = 0;
    }

  gtk_no_selection_set_model (GTK_NO_SELECTION (gtk_grid_view_get_model (GTK_GRID_VIEW (chooser->grid_view))),
                              NULL);

  GTK_WIDGET_CLASS (gtk_emoji_chooser_parent_class)->unmap (widget);
}

static gboolean
grid_invalidate_font (gpointer data)
{
  GtkEmojiChooser *chooser = data;
  GHashTableIter iter;
  gpointer key;

  chooser->font_idle = 0;
  chooser->emoji_max_width = 0;
  gtk_bitset_remove_all (chooser->tested_emoji);
  gtk_bitset_remove_all (chooser->unsupported_emoji);
  g_hash_table_remove_all (chooser->unsupported_standalone);

  g_hash_table_iter_init (&iter, chooser->bound_items);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkListItem *list_item = key;
      GtkInscription *cell = GTK_INSCRIPTION (gtk_list_item_get_child (list_item));
      GtkEmojiItem *item = GTK_EMOJI_ITEM (gtk_list_item_get_item (list_item));

      if (item != NULL)
        {
          gboolean supported = grid_validate_item (chooser, cell, item);

          gtk_widget_set_sensitive (GTK_WIDGET (cell), supported);
          gtk_list_item_set_activatable (list_item, supported);
        }
    }

  if (chooser->support_filter_active)
    {
      g_clear_handle_id (&chooser->rejected_idle, g_source_remove);
      chooser->rejected_pending = FALSE;

      if (gtk_bitset_is_empty (chooser->unsupported_emoji) &&
          g_hash_table_size (chooser->unsupported_standalone) == 0)
        {
          chooser->support_filter_active = FALSE;
          gtk_custom_filter_set_filter_func (chooser->support_filter, NULL, NULL, NULL);
        }
      else
        {
          gtk_filter_changed (GTK_FILTER (chooser->support_filter), GTK_FILTER_CHANGE_DIFFERENT);
        }
    }

  if (chooser->searching)
    gtk_filter_changed (GTK_FILTER (chooser->search_filter), GTK_FILTER_CHANGE_DIFFERENT);

  update_sections (chooser);

  return G_SOURCE_REMOVE;
}

static void
grid_schedule_font_invalidation (GtkEmojiChooser *chooser)
{
  if (!chooser->disposing &&
      chooser->bound_items != NULL &&
      chooser->font_idle == 0 &&
      (chooser->emoji_max_width != 0 ||
       !gtk_bitset_is_empty (chooser->tested_emoji)))
    chooser->font_idle = g_idle_add (grid_invalidate_font, chooser);
}

static void
gtk_emoji_chooser_css_changed (GtkWidget         *widget,
                               GtkCssStyleChange *change)
{
  GTK_WIDGET_CLASS (gtk_emoji_chooser_parent_class)->css_changed (widget, change);

  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT | GTK_CSS_AFFECTS_TEXT_ATTRS))
    grid_schedule_font_invalidation (GTK_EMOJI_CHOOSER (widget));
}

static void
gtk_emoji_chooser_class_init (GtkEmojiChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_emoji_chooser_finalize;
  object_class->dispose = gtk_emoji_chooser_dispose;
  widget_class->show = gtk_emoji_chooser_show;
  widget_class->map = gtk_emoji_chooser_map;
  widget_class->unmap = gtk_emoji_chooser_unmap;
  widget_class->css_changed = gtk_emoji_chooser_css_changed;

  /**
   * GtkEmojiChooser::emoji-picked:
   * @chooser: the `GtkEmojiChooser`
   * @text: the Unicode sequence for the picked Emoji, in UTF-8
   *
   * Emitted when the user selects an Emoji.
   */
  signals[EMOJI_PICKED] =
    g_signal_new ("emoji-picked",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkemojichooser.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, grid_scroller);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, grid_view);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, header_factory);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, variation_factory);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[0].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[1].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[2].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[3].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[4].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[5].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[6].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[7].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[8].button);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, sections[9].button);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
  gtk_widget_class_bind_template_callback (widget_class, stop_search);
  gtk_widget_class_bind_template_callback (widget_class, activate_search);
  gtk_widget_class_bind_template_callback (widget_class, section_clicked);
  gtk_widget_class_bind_template_callback (widget_class, grid_setup);
  gtk_widget_class_bind_template_callback (widget_class, variation_setup);
  gtk_widget_class_bind_template_callback (widget_class, grid_bind);
  gtk_widget_class_bind_template_callback (widget_class, grid_unbind);
  gtk_widget_class_bind_template_callback (widget_class, header_setup);
  gtk_widget_class_bind_template_callback (widget_class, header_bind);
  gtk_widget_class_bind_template_callback (widget_class, grid_activated);
  gtk_widget_class_bind_template_callback (widget_class, grid_viewport_changed);

  /**
   * GtkEmojiChooser|scroll.section:
   * @direction: 1 to scroll forward, -1 to scroll back
   *
   * Scrolls to the next or previous section.
   */
  gtk_widget_class_install_action (widget_class,
                                   "scroll.section",
                                   "i",
                                   gtk_emoji_chooser_scroll_section);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_n,
                                       GDK_CONTROL_MASK,
                                       "scroll.section",
                                       "i",
                                       1);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_p,
                                       GDK_CONTROL_MASK,
                                       "scroll.section",
                                       "i",
                                       -1);
}

/**
 * gtk_emoji_chooser_new:
 *
 * Creates a new `GtkEmojiChooser`.
 *
 * Returns: a new `GtkEmojiChooser`
 */
GtkWidget *
gtk_emoji_chooser_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_EMOJI_CHOOSER, NULL));
}
