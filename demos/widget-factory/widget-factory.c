/* widget-factory: a collection of widgets, for easy theme testing
 *
 * Copyright (C) 2011 Canonical Ltd
 *
 * This  library is free  software; you can  redistribute it and/or
 * modify it  under  the terms  of the  GNU Lesser  General  Public
 * License  as published  by the Free  Software  Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed  in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY; without even  the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static void
change_theme_state (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
  GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (G_OBJECT (settings),
                "gtk-application-prefer-dark-theme",
                g_variant_get_boolean (state),
                NULL);

  g_simple_action_set_state (action, state);
}

static GtkWidget *page_stack;

static void
change_transition_state (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
  GtkStackTransitionType transition;

  if (g_variant_get_boolean (state))
    transition = GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT;
  else
    transition = GTK_STACK_TRANSITION_TYPE_NONE;

  gtk_stack_set_transition_type (GTK_STACK (page_stack), transition);

  g_simple_action_set_state (action, state);
}

static gboolean
get_idle (gpointer data)
{
  GtkWidget *window = data;
  GtkApplication *app = gtk_window_get_application (GTK_WINDOW (window));

  gtk_widget_set_sensitive (window, TRUE);
  gdk_surface_set_cursor (gtk_native_get_surface (GTK_NATIVE (window)), NULL);
  g_application_unmark_busy (G_APPLICATION (app));

  return G_SOURCE_REMOVE;
}

static void
get_busy (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  GtkWidget *window = user_data;
  GdkCursor *cursor;
  GtkApplication *app = gtk_window_get_application (GTK_WINDOW (window));

  g_application_mark_busy (G_APPLICATION (app));
  cursor = gdk_cursor_new_from_name ("wait", NULL);
  gdk_surface_set_cursor (gtk_native_get_surface (GTK_NATIVE (window)), cursor);
  g_object_unref (cursor);
  g_timeout_add (5000, get_idle, window);

  gtk_widget_set_sensitive (window, FALSE);
}

static gint current_page = 0;
static gboolean
on_page (gint i)
{
  return current_page == i;
}

static void
activate_search (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkWidget *searchbar;

  if (!on_page (2))
    return;

  searchbar = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "searchbar"));
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (searchbar), TRUE);
}

static void
activate_delete (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkWidget *infobar;

  g_print ("Activate action delete\n");

  if (!on_page (2))
    return;

  infobar = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "infobar"));
  gtk_widget_show (infobar);
}

static void populate_flowbox (GtkWidget *flowbox);

static void
activate_background (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkWidget *dialog;
  GtkWidget *flowbox;

  if (!on_page (2))
    return;

  dialog = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "selection_dialog"));
  flowbox = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "selection_flowbox"));

  gtk_widget_show (dialog);
  populate_flowbox (flowbox);
}

static void
activate_open (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkWidget *button;

  if (!on_page (3))
    return;

  button = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "open_menubutton"));
  g_signal_emit_by_name (button, "clicked");
}

static void
activate_record (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkWidget *button;

  if (!on_page (3))
    return;

  button = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "record_button"));
  g_signal_emit_by_name (button, "clicked");
}

static void
activate_lock (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkWidget *button;

  if (!on_page (3))
    return;

  button = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "lockbutton"));
  g_signal_emit_by_name (button, "clicked");
}

static void
activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkApplication *app = user_data;
  const gchar *authors[] = {
    "Andrea Cimitan",
    "Cosimo Cecchi",
    NULL
  };
  gchar *version;
  GString *s;

  s = g_string_new ("");

  g_string_append (s, "System libraries\n");
  g_string_append_printf (s, "\tGLib\t%d.%d.%d\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  g_string_append_printf (s, "\tGTK\t%d.%d.%d\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version ());
  g_string_append_printf (s, "\nA link can apppear here: <http://www.gtk.org>");

  version = g_strdup_printf ("%s\nRunning against GTK %d.%d.%d",
                             PACKAGE_VERSION,
                             gtk_get_major_version (),
                             gtk_get_minor_version (),
                             gtk_get_micro_version ());

  gtk_show_about_dialog (GTK_WINDOW (gtk_application_get_active_window (app)),
                         "program-name", "GTK Widget Factory",
                         "version", version,
                         "copyright", "© 1997—2019 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to demonstrate GTK themes and widgets",
                         "authors", authors,
                         "logo-icon-name", "org.gtk.WidgetFactory4",
                         "title", "About GTK Widget Factory",
                         "system-information", s->str,
                         NULL);

  g_string_free (s, TRUE);
  g_free (version);
}

static void
activate_quit (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkApplication *app = user_data;
  GtkWidget *win;
  GList *list, *next;

  list = gtk_application_get_windows (app);
  while (list)
    {
      win = list->data;
      next = list->next;

      gtk_widget_destroy (GTK_WIDGET (win));

      list = next;
    }
}

static void
activate_inspector (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
spin_value_changed (GtkAdjustment *adjustment, GtkWidget *label)
{
  GtkWidget *w;
  gint v;
  gchar *text;

  v = (int)gtk_adjustment_get_value (adjustment);

  if ((v % 3) == 0)
    {
      text = g_strdup_printf ("%d is a multiple of 3", v);
      gtk_label_set_label (GTK_LABEL (label), text);
      g_free (text);
    }

  w = gtk_widget_get_ancestor (label, GTK_TYPE_REVEALER);
  gtk_revealer_set_reveal_child (GTK_REVEALER (w), (v % 3) == 0);
}

static void
dismiss (GtkWidget *button)
{
  GtkWidget *w;

  w = gtk_widget_get_ancestor (button, GTK_TYPE_REVEALER);
  gtk_revealer_set_reveal_child (GTK_REVEALER (w), FALSE);
}

static void
spin_value_reset (GtkWidget *button, GtkAdjustment *adjustment)
{
  gtk_adjustment_set_value (adjustment, 50.0);
  dismiss (button);
}

static gint pulse_time = 250;
static gint pulse_entry_mode = 0;

static void
remove_pulse (gpointer pulse_id)
{
  g_source_remove (GPOINTER_TO_UINT (pulse_id));
}

static gboolean
pulse_it (GtkWidget *widget)
{
  guint pulse_id;

  if (GTK_IS_ENTRY (widget))
    gtk_entry_progress_pulse (GTK_ENTRY (widget));
  else
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (widget));

  pulse_id = g_timeout_add (pulse_time, (GSourceFunc)pulse_it, widget);
  g_object_set_data_full (G_OBJECT (widget), "pulse_id", GUINT_TO_POINTER (pulse_id), remove_pulse);

  return G_SOURCE_REMOVE;
}

static void
update_pulse_time (GtkAdjustment *adjustment, GtkWidget *widget)
{
  gdouble value;
  guint pulse_id;

  value = gtk_adjustment_get_value (adjustment);

  pulse_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "pulse_id"));

  /* vary between 50 and 450 */
  pulse_time = 50 + 4 * value;

  if (value == 100)
    {
      g_object_set_data (G_OBJECT (widget), "pulse_id", NULL);
    }
  else if (value < 100)
    {
      if (pulse_id == 0 && (GTK_IS_PROGRESS_BAR (widget) || pulse_entry_mode % 3 == 2))
        {
          pulse_id = g_timeout_add (pulse_time, (GSourceFunc)pulse_it, widget);
          g_object_set_data_full (G_OBJECT (widget), "pulse_id", GUINT_TO_POINTER (pulse_id), remove_pulse);
        }
    }
}

static void
on_entry_icon_release (GtkEntry            *entry,
                       GtkEntryIconPosition icon_pos,
                       gpointer             user_data)
{
  if (icon_pos != GTK_ENTRY_ICON_SECONDARY)
    return;

  pulse_entry_mode++;

  if (pulse_entry_mode % 3 == 0)
    {
      g_object_set_data (G_OBJECT (entry), "pulse_id", NULL);
      gtk_entry_set_progress_fraction (entry, 0);
    }
  else if (pulse_entry_mode % 3 == 1)
    gtk_entry_set_progress_fraction (entry, 0.25);
  else if (pulse_entry_mode % 3 == 2)
    {
      if (pulse_time - 50 < 400)
        {
          gtk_entry_set_progress_pulse_step (entry, 0.1);
          pulse_it (GTK_WIDGET (entry));
        }
    }
}

#define EPSILON (1e-10)

static gboolean
on_scale_button_query_tooltip (GtkWidget  *button,
                               gint        x,
                               gint        y,
                               gboolean    keyboard_mode,
                               GtkTooltip *tooltip,
                               gpointer    user_data)
{
  GtkScaleButton *scale_button = GTK_SCALE_BUTTON (button);
  GtkAdjustment *adjustment;
  gdouble val;
  gchar *str;
  AtkImage *image;

  image = ATK_IMAGE (gtk_widget_get_accessible (button));

  adjustment = gtk_scale_button_get_adjustment (scale_button);
  val = gtk_scale_button_get_value (scale_button);

  if (val < (gtk_adjustment_get_lower (adjustment) + EPSILON))
    {
      str = g_strdup (_("Muted"));
    }
  else if (val >= (gtk_adjustment_get_upper (adjustment) - EPSILON))
    {
      str = g_strdup (_("Full Volume"));
    }
  else
    {
      gint percent;

      percent = (gint) (100. * val / (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment)) + .5);

      str = g_strdup_printf (C_("volume percentage", "%d %%"), percent);
    }

  gtk_tooltip_set_text (tooltip, str);
  atk_image_set_image_description (image, str);
  g_free (str);

  return TRUE;
}

static void
on_scale_button_value_changed (GtkScaleButton *button,
                               gdouble         value,
                               gpointer        user_data)
{
  gtk_widget_trigger_tooltip_query (GTK_WIDGET (button));
}

static void
on_record_button_toggled (GtkToggleButton *button,
                          gpointer         user_data)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  if (gtk_toggle_button_get_active (button))
    gtk_style_context_remove_class (context, "destructive-action");
  else
    gtk_style_context_add_class (context, "destructive-action");
}

static void
on_page_combo_changed (GtkComboBox *combo,
                       gpointer     user_data)
{
  GtkWidget *from;
  GtkWidget *to;
  GtkWidget *print;

  from = GTK_WIDGET (g_object_get_data (G_OBJECT (combo), "range_from_spin"));
  to = GTK_WIDGET (g_object_get_data (G_OBJECT (combo), "range_to_spin"));
  print = GTK_WIDGET (g_object_get_data (G_OBJECT (combo), "print_button"));

  switch (gtk_combo_box_get_active (combo))
    {
    case 0: /* Range */
      gtk_widget_set_sensitive (from, TRUE);
      gtk_widget_set_sensitive (to, TRUE);
      gtk_widget_set_sensitive (print, TRUE);
      break;
    case 1: /* All */
      gtk_widget_set_sensitive (from, FALSE);
      gtk_widget_set_sensitive (to, FALSE);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (from), 1);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (to), 99);
      gtk_widget_set_sensitive (print, TRUE);
      break;
    case 2: /* Current */
      gtk_widget_set_sensitive (from, FALSE);
      gtk_widget_set_sensitive (to, FALSE);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (from), 7);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (to), 7);
      gtk_widget_set_sensitive (print, TRUE);
      break;
    case 4:
      gtk_widget_set_sensitive (from, FALSE);
      gtk_widget_set_sensitive (to, FALSE);
      gtk_widget_set_sensitive (print, FALSE);
      break;
    default:;
    }
}

static void
on_range_from_changed (GtkSpinButton *from)
{
  GtkSpinButton *to;
  gint v1, v2;

  to = GTK_SPIN_BUTTON (g_object_get_data (G_OBJECT (from), "range_to_spin"));

  v1 = gtk_spin_button_get_value_as_int (from);
  v2 = gtk_spin_button_get_value_as_int (to);

  if (v1 > v2)
    gtk_spin_button_set_value (to, v1);
}

static void
on_range_to_changed (GtkSpinButton *to)
{
  GtkSpinButton *from;
  gint v1, v2;

  from = GTK_SPIN_BUTTON (g_object_get_data (G_OBJECT (to), "range_from_spin"));

  v1 = gtk_spin_button_get_value_as_int (from);
  v2 = gtk_spin_button_get_value_as_int (to);

  if (v1 > v2)
    gtk_spin_button_set_value (from, v2);
}

static void
info_bar_response (GtkWidget *infobar, gint response_id)
{
  if (response_id == GTK_RESPONSE_CLOSE)
    gtk_widget_hide (infobar);
}

static void
show_dialog (GtkWidget *button, GtkWidget *dialog)
{
  gtk_widget_show (dialog);
}

static void
close_dialog (GtkWidget *dialog)
{
  gtk_widget_hide (dialog);
}

static void
set_needs_attention (GtkWidget *page, gboolean needs_attention)
{
  GtkWidget *stack;

  stack = gtk_widget_get_parent (page);
  g_object_set (gtk_stack_get_page (GTK_STACK (stack), page),
                           "needs-attention", needs_attention,
                           NULL);
}

static gboolean
demand_attention (gpointer stack)
{
  GtkWidget *page;

  page = gtk_stack_get_child_by_name (GTK_STACK (stack), "page3");
  set_needs_attention (page, TRUE);

  return G_SOURCE_REMOVE;
}

static void
action_dialog_button_clicked (GtkButton *button, GtkWidget *page)
{
  g_timeout_add (1000, demand_attention, page);
}

static void
page_changed_cb (GtkWidget *stack, GParamSpec *pspec, gpointer data)
{
  const gchar *name;
  GtkWidget *window;
  GtkWidget *page;

  if (gtk_widget_in_destruction (stack))
    return;

  name = gtk_stack_get_visible_child_name (GTK_STACK (stack));

  window = gtk_widget_get_ancestor (stack, GTK_TYPE_APPLICATION_WINDOW);
  g_object_set (gtk_application_window_get_help_overlay (GTK_APPLICATION_WINDOW (window)),
                "view-name", name,
                NULL);

  if (g_str_equal (name, "page1"))
    current_page = 1;
  else if (g_str_equal (name, "page2"))
    current_page = 2;
  if (g_str_equal (name, "page3"))
    {
      current_page = 3;
      page = gtk_stack_get_visible_child (GTK_STACK (stack));
      set_needs_attention (GTK_WIDGET (page), FALSE);
    }
}

static void
populate_model (GtkTreeStore *store)
{
  GtkTreeIter iter, parent0, parent1, parent2, parent3;

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter,
                      0, "Charlemagne",
                      1, "742",
                      2, "814",
                      -1);
  parent0 = iter;
  gtk_tree_store_append (store, &iter, &parent0);
  gtk_tree_store_set (store, &iter,
                      0, "Pepin the Short",
                      1, "714",
                      2, "768",
                      -1);
  parent1 = iter;
  gtk_tree_store_append (store, &iter, &parent1);
  gtk_tree_store_set (store, &iter,
                      0, "Charles Martel",
                      1, "688",
                      2, "741",
                      -1);
  parent2 = iter;
  gtk_tree_store_append (store, &iter, &parent2);
  gtk_tree_store_set (store, &iter,
                      0, "Pepin of Herstal",
                      1, "635",
                      2, "714",
                      -1);
  parent3 = iter;
  gtk_tree_store_append (store, &iter, &parent3);
  gtk_tree_store_set (store, &iter,
                      0, "Ansegisel",
                      1, "602 or 610",
                      2, "murdered before 679",
                      -1);
  gtk_tree_store_append (store, &iter, &parent3);
  gtk_tree_store_set (store, &iter,
                      0, "Begga",
                      1, "615",
                      2, "693",
                      -1);
  gtk_tree_store_append (store, &iter, &parent2);
  gtk_tree_store_set (store, &iter,
                      0, "Alpaida",
                      -1);
  gtk_tree_store_append (store, &iter, &parent1);
  gtk_tree_store_set (store, &iter,
                      0, "Rotrude",
                      -1);
  parent2 = iter;
  gtk_tree_store_append (store, &iter, &parent2);
  gtk_tree_store_set (store, &iter,
                      0, "Liévin de Trèves",
                      -1);
  parent3 = iter;
  gtk_tree_store_append (store, &iter, &parent3);
  gtk_tree_store_set (store, &iter,
                      0, "Guérin",
                      -1);
  gtk_tree_store_append (store, &iter, &parent3);
  gtk_tree_store_set (store, &iter,
                      0, "Gunza",
                      -1);
  gtk_tree_store_append (store, &iter, &parent2);
  gtk_tree_store_set (store, &iter,
                      0, "Willigarde de Bavière",
                      -1);
  gtk_tree_store_append (store, &iter, &parent0);
  gtk_tree_store_set (store, &iter,
                      0, "Bertrada of Laon",
                      1, "710",
                      2, "783",
                      -1);
  parent1 = iter;
  gtk_tree_store_append (store, &iter, &parent1);
  gtk_tree_store_set (store, &iter,
                      0, "Caribert of Laon",
                      2, "before 762",
                      -1);
  parent2 = iter;
  gtk_tree_store_append (store, &iter, &parent2);
  gtk_tree_store_set (store, &iter,
                      0, "Unknown",
                      -1);
  gtk_tree_store_append (store, &iter, &parent2);
  gtk_tree_store_set (store, &iter,
                      0, "Bertrada of Prüm",
                      1, "ca. 670",
                      2, "after 721",
                      -1);
  gtk_tree_store_append (store, &iter, &parent1);
  gtk_tree_store_set (store, &iter,
                      0, "Gisele of Aquitaine",
                      -1);
  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 3, TRUE, -1);
  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter,
                      0, "Attila the Hun",
                      1, "ca. 390",
                      2, "453",
                      -1);
}

static gboolean
row_separator_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  gboolean is_sep;

  gtk_tree_model_get (model, iter, 3, &is_sep, -1);

  return is_sep;
}

static void
update_title_header (GtkListBoxRow *row,
                     GtkListBoxRow *before,
                     gpointer       data)
{
  GtkWidget *header;
  gchar *title;

  header = gtk_list_box_row_get_header (row);
  title = (gchar *)g_object_get_data (G_OBJECT (row), "title");
  if (!header && title)
    {
      title = g_strdup_printf ("<b>%s</b>", title);

      header = gtk_label_new (title);
      gtk_label_set_use_markup (GTK_LABEL (header), TRUE);
      gtk_widget_set_halign (header, GTK_ALIGN_START);
      gtk_widget_set_margin_top (header, 12);
      gtk_widget_set_margin_start (header, 6);
      gtk_widget_set_margin_end (header, 6);
      gtk_widget_set_margin_bottom (header, 6);
      gtk_widget_show (header);

      gtk_list_box_row_set_header (row, header);

      g_free (title);
    }
}

static void
overshot (GtkScrolledWindow *sw, GtkPositionType pos, GtkWidget *widget)
{
  GtkWidget *box, *row, *label, *swatch;
  GdkRGBA rgba;
  const gchar *color;
  gchar *text;
  GtkWidget *silver;
  GtkWidget *gold;

  silver = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "Silver"));
  gold = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "Gold"));

  if (pos == GTK_POS_TOP)
    {
      if (silver)
        {
          gtk_container_remove (GTK_CONTAINER (widget), silver);
          g_object_set_data (G_OBJECT (widget), "Silver", NULL);
        }
      if (gold)
        {
          gtk_container_remove (GTK_CONTAINER (widget), gold);
          g_object_set_data (G_OBJECT (widget), "Gold", NULL);
        }

      return;
    }


  if (gold)
    return;
  else if (silver)
    color = "Gold";
  else
    color = "Silver";

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  text = g_strconcat ("<b>", color, "</b>", NULL);
  label = gtk_label_new (text);
  g_free (text);
  g_object_set (label,
                "use-markup", TRUE,
                "halign", GTK_ALIGN_START,
                "valign", GTK_ALIGN_CENTER,
                "hexpand", TRUE,
                "margin", 6,
                "xalign", 0.0,
                NULL);
  gtk_container_add (GTK_CONTAINER (row), label);
  gdk_rgba_parse (&rgba, color);
  swatch = g_object_new (g_type_from_name ("GtkColorSwatch"),
                         "rgba", &rgba,
                         "selectable", FALSE,
                         "halign", GTK_ALIGN_END,
                         "valign", GTK_ALIGN_CENTER,
                         "margin", 6,
                         "height-request", 24,
                         NULL);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), swatch);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_list_box_insert (GTK_LIST_BOX (widget), row, -1);
  row = gtk_widget_get_parent (row);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  g_object_set_data (G_OBJECT (widget), color, row);
  g_object_set_data (G_OBJECT (row), "color", (gpointer)color);
}

static void
rgba_changed (GtkColorChooser *chooser, GParamSpec *pspec, GtkListBox *box)
{
  gtk_list_box_select_row (box, NULL);
}

static void
set_color (GtkListBox *box, GtkListBoxRow *row, GtkColorChooser *chooser)
{
  const char *color;
  GdkRGBA rgba;

  if (!row)
    return;

  color = (const char *)g_object_get_data (G_OBJECT (row), "color");

  if (!color)
    return;

  if (gdk_rgba_parse (&rgba, color))
    {
      g_signal_handlers_block_by_func (chooser, rgba_changed, box);
      gtk_color_chooser_set_rgba (chooser, &rgba);
      g_signal_handlers_unblock_by_func (chooser, rgba_changed, box);
    }
}

static void
populate_colors (GtkWidget *widget, GtkWidget *chooser)
{
  struct { const gchar *name; const gchar *color; const gchar *title; } colors[] = {
    { "2.5", "#C8828C", "Red" },
    { "5", "#C98286", NULL },
    { "7.5", "#C9827F", NULL },
    { "10", "#C98376", NULL },
    { "2.5", "#C8856D", "Red/Yellow" },
    { "5", "#C58764", NULL },
    { "7.5", "#C1895E", NULL },
    { "10", "#BB8C56", NULL },
    { "2.5", "#B58F4F", "Yellow" },
    { "5", "#AD924B", NULL },
    { "7.5", "#A79548", NULL },
    { "10", "#A09749", NULL },
    { "2.5", "#979A4E", "Yellow/Green" },
    { "5", "#8D9C55", NULL },
    { "7.5", "#7F9F62", NULL },
    { "10", "#73A06E", NULL },
    { "2.5", "#65A27C", "Green" },
    { "5", "#5CA386", NULL },
    { "7.5", "#57A38D", NULL },
    { "10", "#52A394", NULL },
    { "2.5", "#4EA39A", "Green/Blue" },
    { "5", "#49A3A2", NULL },
    { "7.5", "#46A2AA", NULL },
    { "10", "#46A1B1", NULL },
    { "2.5", "#49A0B8", "Blue" },
    { "5", "#529EBD", NULL },
    { "7.5", "#5D9CC1", NULL },
    { "10", "#689AC3", NULL },
    { "2.5", "#7597C5", "Blue/Purple" },
    { "5", "#8095C6", NULL },
    { "7.5", "#8D91C6", NULL },
    { "10", "#988EC4", NULL },
    { "2.5", "#A08CC1", "Purple" },
    { "5", "#A88ABD", NULL },
    { "7.5", "#B187B6", NULL },
    { "10", "#B786B0", NULL },
    { "2.5", "#BC84A9", "Purple/Red" },
    { "5", "#C183A0", NULL },
    { "7.5", "#C48299", NULL },
    { "10", "#C68292", NULL }
  };
  gint i;
  GtkWidget *row, *box, *label, *swatch;
  GtkWidget *sw;
  GdkRGBA rgba;

  gtk_list_box_set_header_func (GTK_LIST_BOX (widget), update_title_header, NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (colors); i++)
    {
      row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
      label = gtk_label_new (colors[i].name);
      g_object_set (label,
                    "halign", GTK_ALIGN_START,
                    "valign", GTK_ALIGN_CENTER,
                    "margin", 6,
                    "hexpand", TRUE,
                    "xalign", 0.0,
                    NULL);
      gtk_container_add (GTK_CONTAINER (row), label);
      gdk_rgba_parse (&rgba, colors[i].color);
      swatch = g_object_new (g_type_from_name ("GtkColorSwatch"),
                             "rgba", &rgba,
                             "selectable", FALSE,
                             "halign", GTK_ALIGN_END,
                             "valign", GTK_ALIGN_CENTER,
                             "margin", 6,
                             "height-request", 24,
                             NULL);
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (box), swatch);
      gtk_container_add (GTK_CONTAINER (row), box);
      gtk_list_box_insert (GTK_LIST_BOX (widget), row, -1);
      row = gtk_widget_get_parent (row);
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
      g_object_set_data (G_OBJECT (row), "color", (gpointer)colors[i].color);
      if (colors[i].title)
        g_object_set_data (G_OBJECT (row), "title", (gpointer)colors[i].title);
    }

  g_signal_connect (widget, "row-selected", G_CALLBACK (set_color), chooser);

  gtk_list_box_invalidate_headers (GTK_LIST_BOX (widget));

  sw = gtk_widget_get_ancestor (widget, GTK_TYPE_SCROLLED_WINDOW);
  g_signal_connect (sw, "edge-overshot", G_CALLBACK (overshot), widget);
}

typedef struct {
  GtkWidget *flowbox;
  gchar *filename;
} BackgroundData;

static void
background_loaded_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      data)
{
  BackgroundData *bd = data;
  GtkWidget *child;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
  if (error)
    {
      g_warning ("Error loading '%s': %s", bd->filename, error->message);
      g_error_free (error);
      return;
    }

  child = gtk_picture_new_for_pixbuf (pixbuf);
  gtk_widget_set_size_request (child, 110, 70);
  gtk_flow_box_insert (GTK_FLOW_BOX (bd->flowbox), child, -1);
  child = gtk_widget_get_parent (child);
  g_object_set_data_full (G_OBJECT (child), "filename", bd->filename, g_free);
  g_free (bd);
}

static void
populate_flowbox (GtkWidget *flowbox)
{
  const gchar *location;
  GDir *dir;
  GError *error = NULL;
  const gchar *name;
  gchar *filename;
  GFile *file;
  GInputStream *stream;
  BackgroundData *bd;
  GdkPixbuf *pixbuf;
  GtkWidget *child;

  if (GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (flowbox), "populated")))
    return;

  g_object_set_data (G_OBJECT (flowbox), "populated", GUINT_TO_POINTER (1));

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 110, 70);
  gdk_pixbuf_fill (pixbuf, 0xffffffff);
  child = gtk_picture_new_for_pixbuf (pixbuf);
  gtk_flow_box_insert (GTK_FLOW_BOX (flowbox), child, -1);

  location = "/usr/share/backgrounds/gnome";
  dir = g_dir_open (location, 0, &error);
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      filename = g_build_filename (location, name, NULL);
      file = g_file_new_for_path (filename);
      stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
      if (error)
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
          g_free (filename); 
        }
      else
        {
          bd = g_new (BackgroundData, 1);
          bd->flowbox = flowbox;
          bd->filename = filename;
          gdk_pixbuf_new_from_stream_at_scale_async (stream, 110, 110, TRUE, NULL, 
                                                     background_loaded_cb, bd);
        }

      g_object_unref (file);
      g_object_unref (stream);
    }

  g_dir_close (dir);
}

static void
row_activated (GtkListBox *box, GtkListBoxRow *row)
{
  GtkWidget *image;
  GtkWidget *dialog;

  image = (GtkWidget *)g_object_get_data (G_OBJECT (row), "image");
  dialog = (GtkWidget *)g_object_get_data (G_OBJECT (row), "dialog");

  if (image)
    {
      if (gtk_widget_get_opacity (image) > 0)
        gtk_widget_set_opacity (image, 0);
      else
        gtk_widget_set_opacity (image, 1);
    }
  else if (dialog)
    {
      gtk_window_present (GTK_WINDOW (dialog));
    }
}

static void
set_accel (GtkApplication *app, GtkWidget *widget)
{
  GtkWidget *accel_label;
  const gchar *action;
  gchar **accels;
  guint key;
  GdkModifierType mods;

  accel_label = gtk_bin_get_child (GTK_BIN (widget));
  g_assert (GTK_IS_ACCEL_LABEL (accel_label));

  action = gtk_actionable_get_action_name (GTK_ACTIONABLE (widget));
  accels = gtk_application_get_accels_for_action (app, action);

  gtk_accelerator_parse (accels[0], &key, &mods);
  gtk_accel_label_set_accel (GTK_ACCEL_LABEL (accel_label), key, mods);

  g_strfreev (accels);
}

typedef struct
{
  GtkTextView tv;
  GdkTexture *texture;
} MyTextView;

typedef GtkTextViewClass MyTextViewClass;

G_DEFINE_TYPE (MyTextView, my_text_view, GTK_TYPE_TEXT_VIEW)

static void
my_text_view_init (MyTextView *tv)
{
}

static void
my_tv_snapshot_layer (GtkTextView      *widget,
                      GtkTextViewLayer  layer,
                      GtkSnapshot      *snapshot)
{
  MyTextView *tv = (MyTextView *)widget;

  if (layer == GTK_TEXT_VIEW_LAYER_BELOW_TEXT && tv->texture)
    {
      gtk_snapshot_push_opacity (snapshot, 0.333);
      gtk_snapshot_append_texture (snapshot,
                                   tv->texture,
                                   &GRAPHENE_RECT_INIT(
                                     0, 0,
                                     gdk_texture_get_width (tv->texture),
                                     gdk_texture_get_height (tv->texture)
                                   ));
      gtk_snapshot_pop (snapshot);
    }
}

static void
my_tv_finalize (GObject *object)
{
  MyTextView *tv = (MyTextView *)object;

  g_clear_object (&tv->texture);

  G_OBJECT_CLASS (my_text_view_parent_class)->finalize (object);
}

static void
my_text_view_class_init (MyTextViewClass *class)
{
  GtkTextViewClass *tv_class = GTK_TEXT_VIEW_CLASS (class);
  GObjectClass *o_class = G_OBJECT_CLASS (class);

  o_class->finalize = my_tv_finalize;
  tv_class->snapshot_layer = my_tv_snapshot_layer;
}

static void
my_text_view_set_background (MyTextView *tv, const gchar *filename)
{
  GError *error = NULL;
  GFile *file;

  g_clear_object (&tv->texture);

  if (filename == NULL)
    return;

  file = g_file_new_for_path (filename);
  tv->texture = gdk_texture_new_from_file (file, &error);
  g_object_unref (file);

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  gtk_widget_queue_draw (GTK_WIDGET (tv));
}

static void
close_selection_dialog (GtkWidget *dialog, gint response, GtkWidget *tv)
{
  GtkWidget *box;
  GtkWidget *child;
  GList *children;
  const gchar *filename;

  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_CANCEL)
    return;

  box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  children = gtk_container_get_children (GTK_CONTAINER (box));
  box = children->data;
  g_list_free (children);
  g_assert (GTK_IS_FLOW_BOX (box));
  children = gtk_flow_box_get_selected_children (GTK_FLOW_BOX (box));

  if (!children)
    return;

  child = children->data;
  filename = (const gchar *)g_object_get_data (G_OBJECT (child), "filename");

  g_list_free (children);

  my_text_view_set_background ((MyTextView *)tv, filename);
}

static void
toggle_selection_mode (GtkSwitch  *sw,
                       GParamSpec *pspec,
                       GtkListBox *listbox)
{
  if (gtk_switch_get_active (sw))
    gtk_list_box_set_selection_mode (listbox, GTK_SELECTION_SINGLE);
  else
    gtk_list_box_set_selection_mode (listbox, GTK_SELECTION_NONE);

  gtk_list_box_set_activate_on_single_click (listbox, !gtk_switch_get_active (sw));
}

static void
handle_insert (GtkWidget *button, GtkWidget *textview)
{
  GtkTextBuffer *buffer;
  const gchar *id;
  const gchar *text;

  id = gtk_buildable_get_name (GTK_BUILDABLE (button));

  if (strcmp (id, "toolbutton1") == 0)
    text = "⌘";
  else if (strcmp (id, "toolbutton2") == 0)
    text = "⚽";
  else if (strcmp (id, "toolbutton3") == 0)
    text = "⤢";
  else if (strcmp (id, "toolbutton4") == 0)
    text = "☆";
  else
    text = "";

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  gtk_text_buffer_insert_at_cursor (buffer, text, -1);
}

static void
handle_cutcopypaste (GtkWidget *button, GtkWidget *textview)
{
  GtkTextBuffer *buffer;
  GdkClipboard *clipboard;
  const gchar *id;

  clipboard = gtk_widget_get_clipboard (textview);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  id = gtk_buildable_get_name (GTK_BUILDABLE (button));

  if (strcmp (id, "cutbutton") == 0)
    gtk_text_buffer_cut_clipboard (buffer, clipboard, TRUE);
  else if (strcmp (id, "copybutton") == 0)
    gtk_text_buffer_copy_clipboard (buffer, clipboard);
  else if (strcmp (id, "pastebutton") == 0)
    gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL, TRUE);
  else if (strcmp (id, "deletebutton") == 0)
    gtk_text_buffer_delete_selection (buffer, TRUE, TRUE);
}

static void
clipboard_formats_notify (GdkClipboard *clipboard, GdkEvent *event, GtkWidget *button)
{
  const gchar *id;
  gboolean has_text;

  id = gtk_buildable_get_name (GTK_BUILDABLE (button));
  has_text = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), GTK_TYPE_TEXT_BUFFER);

  if (strcmp (id, "pastebutton") == 0)
    gtk_widget_set_sensitive (button, has_text);
}

static void
textbuffer_notify_selection (GObject *object, GParamSpec *pspec, GtkWidget *button)
{
  const gchar *id;
  gboolean has_selection;

  id = gtk_buildable_get_name (GTK_BUILDABLE (button));
  has_selection = gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (object));

  if (strcmp (id, "cutbutton") == 0 ||
      strcmp (id, "copybutton") == 0 ||
      strcmp (id, "deletebutton") == 0)
    gtk_widget_set_sensitive (button, has_selection);
}

static gboolean
osd_frame_pressed (GtkGestureClick *gesture,
                   int              press,
                   double           x,
                   double           y,
                   gpointer         data)
{
  GtkWidget *frame = data;
  GtkWidget *osd;
  gboolean visible;

  osd = g_object_get_data (G_OBJECT (frame), "osd");
  visible = gtk_widget_get_visible (osd);
  gtk_widget_set_visible (osd, !visible);

  return GDK_EVENT_STOP;
}

static gboolean
page_combo_separator_func (GtkTreeModel *model,
                           GtkTreeIter  *iter,
                           gpointer      data)
{
  gchar *text;
  gboolean res;

  gtk_tree_model_get (model, iter, 0, &text, -1);
  res = g_strcmp0 (text, "-") == 0;
  g_free (text);

  return res;
}

static void
toggle_format (GSimpleAction *action,
               GVariant      *value,
               gpointer       user_data)
{
  GtkTextView *text_view = user_data;
  GtkTextIter start, end;
  const char *name;

  name = g_action_get_name (G_ACTION (action));

  g_simple_action_set_state (action, value);

  gtk_text_buffer_get_selection_bounds (gtk_text_view_get_buffer (text_view), &start, &end);
  if (g_variant_get_boolean (value))
    gtk_text_buffer_apply_tag_by_name (gtk_text_view_get_buffer (text_view), name, &start, &end);
  else
    gtk_text_buffer_remove_tag_by_name (gtk_text_view_get_buffer (text_view), name, &start, &end);
}

static GActionGroup *actions;

static void
text_changed (GtkTextBuffer *buffer)
{
  GAction *bold;
  GAction *italic;
  GAction *underline;
  GtkTextIter iter;
  GtkTextTagTable *tags;
  GtkTextTag *bold_tag, *italic_tag, *underline_tag;
  gboolean all_bold, all_italic, all_underline;
  GtkTextIter start, end;
  gboolean has_selection;

  bold = g_action_map_lookup_action (G_ACTION_MAP (actions), "bold");
  italic = g_action_map_lookup_action (G_ACTION_MAP (actions), "italic");
  underline = g_action_map_lookup_action (G_ACTION_MAP (actions), "underline");

  has_selection = gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (bold), has_selection);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (italic), has_selection);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (underline), has_selection);
  if (!has_selection)
    return;

  tags = gtk_text_buffer_get_tag_table (buffer);
  bold_tag = gtk_text_tag_table_lookup (tags, "bold");
  italic_tag = gtk_text_tag_table_lookup (tags, "italic");
  underline_tag = gtk_text_tag_table_lookup (tags, "underline");
  all_bold = TRUE;
  all_italic = TRUE;
  all_underline = TRUE;
  gtk_text_iter_assign (&iter, &start);
  while (!gtk_text_iter_equal (&iter, &end))
    {
      all_bold &= gtk_text_iter_has_tag (&iter, bold_tag);
      all_italic &= gtk_text_iter_has_tag (&iter, italic_tag);
      all_underline &= gtk_text_iter_has_tag (&iter, underline_tag);
      gtk_text_iter_forward_char (&iter);
    }

  g_simple_action_set_state (G_SIMPLE_ACTION (bold), g_variant_new_boolean (all_bold));
  g_simple_action_set_state (G_SIMPLE_ACTION (italic), g_variant_new_boolean (all_italic));
  g_simple_action_set_state (G_SIMPLE_ACTION (underline), g_variant_new_boolean (all_underline));
}

static void
text_view_add_to_context_menu (GtkTextView *text_view)
{
  GMenu *menu;
  GActionEntry entries[] = {
    { "bold", NULL, NULL, "false", toggle_format },
    { "italic", NULL, NULL, "false", toggle_format },
    { "underline", NULL, NULL, "false", toggle_format },
  };
  GMenuItem *item;
  GAction *action;

  actions = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS (entries), text_view);

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "bold");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "italic");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "underline");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  gtk_widget_insert_action_group (GTK_WIDGET (text_view), "format", G_ACTION_GROUP (actions));

  menu = g_menu_new ();
  item = g_menu_item_new (_("Bold"), "format.bold");
  g_menu_item_set_attribute (item, "touch-icon", "s", "format-text-bold-symbolic");
  g_menu_append_item (G_MENU (menu), item);
  g_object_unref (item);
  item = g_menu_item_new (_("Italics"), "format.italic");
  g_menu_item_set_attribute (item, "touch-icon", "s", "format-text-italic-symbolic");
  g_menu_append_item (G_MENU (menu), item);
  g_object_unref (item);
  item = g_menu_item_new (_("Underline"), "format.underline");
  g_menu_item_set_attribute (item, "touch-icon", "s", "format-text-underline-symbolic");
  g_menu_append_item (G_MENU (menu), item);
  g_object_unref (item);

  gtk_text_view_set_extra_menu (text_view, G_MENU_MODEL (menu));
  g_object_unref (menu);

  g_signal_connect (gtk_text_view_get_buffer (text_view), "changed", G_CALLBACK (text_changed), NULL);
  g_signal_connect (gtk_text_view_get_buffer (text_view), "mark-set", G_CALLBACK (text_changed), NULL);
}

static void
open_popover_text_changed (GtkEntry *entry, GParamSpec *pspec, GtkWidget *button)
{
  const gchar *text;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  gtk_widget_set_sensitive (button, strlen (text) > 0);
}

static gboolean
show_page_again (gpointer data)
{
  gtk_widget_show (GTK_WIDGET (data));
  return G_SOURCE_REMOVE;
}

static void
tab_close_cb (GtkWidget *page)
{
  gtk_widget_hide (page);
  g_timeout_add (2500, show_page_again, page);
}

typedef struct _GTestPermission GTestPermission;
typedef struct _GTestPermissionClass GTestPermissionClass;

struct _GTestPermission
{
  GPermission parent;
};

struct _GTestPermissionClass
{
  GPermissionClass parent_class;
};

G_DEFINE_TYPE (GTestPermission, g_test_permission, G_TYPE_PERMISSION)

static void
g_test_permission_init (GTestPermission *test)
{
  g_permission_impl_update (G_PERMISSION (test), TRUE, TRUE, TRUE);
}

static gboolean
update_allowed (GPermission *permission,
                gboolean     allowed)
{
  g_permission_impl_update (permission, allowed, TRUE, TRUE);

  return TRUE;
}

static gboolean
acquire (GPermission   *permission,
         GCancellable  *cancellable,
         GError       **error)
{
  return update_allowed (permission, TRUE);
}

static void
acquire_async (GPermission         *permission,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GTask *task;

  task = g_task_new ((GObject*)permission, NULL, callback, user_data);
  g_task_return_boolean (task, update_allowed (permission, TRUE));
  g_object_unref (task);
}

gboolean
acquire_finish (GPermission   *permission,
                GAsyncResult  *res,
                GError       **error)
{
  return g_task_propagate_boolean (G_TASK (res), error);
}

static gboolean
release (GPermission   *permission,
         GCancellable  *cancellable,
         GError       **error)
{
  return update_allowed (permission, FALSE);
}

static void
release_async (GPermission         *permission,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GTask *task;

  task = g_task_new ((GObject*)permission, NULL, callback, user_data);
  g_task_return_boolean (task, update_allowed (permission, FALSE));
  g_object_unref (task);
}

gboolean
release_finish (GPermission   *permission,
                GAsyncResult  *result,
                GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_test_permission_class_init (GTestPermissionClass *class)
{
  GPermissionClass *permission_class = G_PERMISSION_CLASS (class);

  permission_class->acquire = acquire;
  permission_class->acquire_async = acquire_async;
  permission_class->acquire_finish = acquire_finish;

  permission_class->release = release;
  permission_class->release_async = release_async;
  permission_class->release_finish = release_finish;
}

static void
update_buttons (GtkWidget *iv, GtkIconSize size)
{
  GtkWidget *button;

  button = GTK_WIDGET (g_object_get_data (G_OBJECT (iv), "increase_button"));
  gtk_widget_set_sensitive (button, size != GTK_ICON_SIZE_LARGE);
  button = GTK_WIDGET (g_object_get_data (G_OBJECT (iv), "decrease_button"));
  gtk_widget_set_sensitive (button, size != GTK_ICON_SIZE_NORMAL);
  button = GTK_WIDGET (g_object_get_data (G_OBJECT (iv), "reset_button"));
  gtk_widget_set_sensitive (button, size != GTK_ICON_SIZE_INHERIT);
}

static void
increase_icon_size (GtkWidget *iv)
{
  GList *cells;
  GtkCellRendererPixbuf *cell;

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (iv));
  cell = cells->data;
  g_list_free (cells);

  g_object_set (cell, "icon-size", GTK_ICON_SIZE_LARGE, NULL);

  update_buttons (iv, GTK_ICON_SIZE_LARGE);

  gtk_widget_queue_resize (iv);
}

static void
decrease_icon_size (GtkWidget *iv)
{
  GList *cells;
  GtkCellRendererPixbuf *cell;

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (iv));
  cell = cells->data;
  g_list_free (cells);

  g_object_set (cell, "icon-size", GTK_ICON_SIZE_NORMAL, NULL);

  update_buttons (iv, GTK_ICON_SIZE_NORMAL);

  gtk_widget_queue_resize (iv);
}

static void
reset_icon_size (GtkWidget *iv)
{
  GList *cells;
  GtkCellRendererPixbuf *cell;

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (iv));
  cell = cells->data;
  g_list_free (cells);

  g_object_set (cell, "icon-size", GTK_ICON_SIZE_INHERIT, NULL);

  update_buttons (iv, GTK_ICON_SIZE_INHERIT);

  gtk_widget_queue_resize (iv);
}

static gchar *
scale_format_value_blank (GtkScale *scale, gdouble value)
{
  return g_strdup (" ");
}

static gchar *
scale_format_value (GtkScale *scale, gdouble value)
{
  return g_strdup_printf ("%0.*f", 1, value);
}

static void
adjustment3_value_changed (GtkAdjustment *adj, GtkProgressBar *pbar)
{
  double fraction;

  fraction = gtk_adjustment_get_value (adj) / (gtk_adjustment_get_upper (adj) - gtk_adjustment_get_lower (adj));

  gtk_progress_bar_set_fraction (pbar, fraction);
}

static void
clicked_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            GtkPopover *popover)
{
  GdkRectangle rect;

  rect.x = x;
  rect.y = y;
  rect.width = 1;
  rect.height = 1;
  gtk_popover_set_pointing_to (popover, &rect);
  gtk_popover_popup (popover);
}

static void
set_up_context_popover (GtkWidget *widget,
                        GMenuModel *model)
{
  GtkWidget *popover = gtk_popover_menu_new_from_model (widget, model);
  GtkGesture *gesture;

  g_object_set (popover, "has-arrow", FALSE, NULL);
  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (clicked_cb), popover);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));
}

static void
activate (GApplication *app)
{
  GtkBuilder *builder;
  GtkWindow *window;
  GtkWidget *widget;
  GtkWidget *widget2;
  GtkWidget *widget3;
  GtkWidget *widget4;
  GtkWidget *stack;
  GtkWidget *dialog;
  GtkAdjustment *adj;
  GtkCssProvider *provider;
  GMenuModel *model;
  static GActionEntry win_entries[] = {
    { "dark", NULL, NULL, "false", change_theme_state },
    { "transition", NULL, NULL, "false", change_transition_state },
    { "search", activate_search, NULL, NULL, NULL },
    { "delete", activate_delete, NULL, NULL, NULL },
    { "busy", get_busy, NULL, NULL, NULL },
    { "background", activate_background, NULL, NULL, NULL },
    { "open", activate_open, NULL, NULL, NULL },
    { "record", activate_record, NULL, NULL, NULL },
    { "lock", activate_lock, NULL, NULL, NULL },
  };
  struct {
    const gchar *action_and_target;
    const gchar *accelerators[2];
  } accels[] = {
    { "app.about", { "F1", NULL } },
    { "app.quit", { "<Primary>q", NULL } },
    { "app.open-in", { "<Primary>n", NULL } },
    { "app.cut", { "<Primary>x", NULL } },
    { "app.copy", { "<Primary>c", NULL } },
    { "app.paste", { "<Primary>v", NULL } },
    { "win.dark", { "<Primary>d", NULL } },
    { "win.search", { "<Primary>s", NULL } },
    { "win.delete", { "Delete", NULL } },
    { "win.background", { "<Primary>b", NULL } },
    { "win.open", { "<Primary>o", NULL } },
    { "win.record", { "<Primary>r", NULL } },
    { "win.lock", { "<Primary>l", NULL } },
  };
  gint i;
  GPermission *permission;
  GAction *action;

  g_type_ensure (my_text_view_get_type ());

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gtk/WidgetFactory4/widget-factory.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  builder = gtk_builder_new_from_resource ("/org/gtk/WidgetFactory4/widget-factory.ui");
  gtk_builder_add_callback_symbol (builder, "on_entry_icon_release", (GCallback)on_entry_icon_release);
  gtk_builder_add_callback_symbol (builder, "on_scale_button_value_changed", (GCallback)on_scale_button_value_changed);
  gtk_builder_add_callback_symbol (builder, "on_scale_button_query_tooltip", (GCallback)on_scale_button_query_tooltip);
  gtk_builder_add_callback_symbol (builder, "on_record_button_toggled", (GCallback)on_record_button_toggled);
  gtk_builder_add_callback_symbol (builder, "on_page_combo_changed", (GCallback)on_page_combo_changed);
  gtk_builder_add_callback_symbol (builder, "on_range_from_changed", (GCallback)on_range_from_changed);
  gtk_builder_add_callback_symbol (builder, "on_range_to_changed", (GCallback)on_range_to_changed);
  gtk_builder_add_callback_symbol (builder, "tab_close_cb", (GCallback)tab_close_cb);
  gtk_builder_add_callback_symbol (builder, "increase_icon_size", (GCallback)increase_icon_size);
  gtk_builder_add_callback_symbol (builder, "decrease_icon_size", (GCallback)decrease_icon_size);
  gtk_builder_add_callback_symbol (builder, "reset_icon_size", (GCallback)reset_icon_size);
  gtk_builder_add_callback_symbol (builder, "scale_format_value", (GCallback)scale_format_value);
  gtk_builder_add_callback_symbol (builder, "scale_format_value_blank", (GCallback)scale_format_value_blank);
  gtk_builder_add_callback_symbol (builder, "osd_frame_pressed", (GCallback)osd_frame_pressed);

  gtk_builder_connect_signals (builder, NULL);

  window = (GtkWindow *)gtk_builder_get_object (builder, "window");
  gtk_application_add_window (GTK_APPLICATION (app), window);
  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   win_entries, G_N_ELEMENTS (win_entries),
                                   window);

  for (i = 0; i < G_N_ELEMENTS (accels); i++)
    gtk_application_set_accels_for_action (GTK_APPLICATION (app), accels[i].action_and_target, accels[i].accelerators);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "statusbar");
  gtk_statusbar_push (GTK_STATUSBAR (widget), 0, "All systems are operating normally.");
  action = G_ACTION (g_property_action_new ("statusbar", widget, "visible"));
  g_action_map_add_action (G_ACTION_MAP (window), action);
  g_object_unref (G_OBJECT (action));

  widget = (GtkWidget *)gtk_builder_get_object (builder, "toolbar");
  action = G_ACTION (g_property_action_new ("toolbar", widget, "visible"));
  g_action_map_add_action (G_ACTION_MAP (window), action);
  g_object_unref (G_OBJECT (action));

  adj = (GtkAdjustment *)gtk_builder_get_object (builder, "adjustment1");

  widget = (GtkWidget *)gtk_builder_get_object (builder, "progressbar3");
  g_signal_connect (adj, "value-changed", G_CALLBACK (update_pulse_time), widget);
  update_pulse_time (adj, widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "entry1");
  g_signal_connect (adj, "value-changed", G_CALLBACK (update_pulse_time), widget);
  update_pulse_time (adj, widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "page2reset");
  adj = (GtkAdjustment *) gtk_builder_get_object (builder, "adjustment2");
  g_signal_connect (widget, "clicked", G_CALLBACK (spin_value_reset), adj);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "page2dismiss");
  g_signal_connect (widget, "clicked", G_CALLBACK (dismiss), NULL);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "page2note");
  adj = (GtkAdjustment *) gtk_builder_get_object (builder, "adjustment2");
  g_signal_connect (adj, "value-changed", G_CALLBACK (spin_value_changed), widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "listbox");
  g_signal_connect (widget, "row-activated", G_CALLBACK (row_activated), NULL);

  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "listboxrow1switch");
  g_signal_connect (widget2, "notify::active", G_CALLBACK (toggle_selection_mode), widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "listboxrow3");
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "listboxrow3image");
  g_object_set_data (G_OBJECT (widget), "image", widget2);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "listboxrow4");
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "info_dialog");
  g_object_set_data (G_OBJECT (widget), "dialog", widget2);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "listboxrow5button");
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "action_dialog");
  g_signal_connect_swapped (widget, "clicked", G_CALLBACK (gtk_window_present), widget2);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "toolbar");
  g_object_set_data (G_OBJECT (window), "toolbar", widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "searchbar");
  g_object_set_data (G_OBJECT (window), "searchbar", widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "infobar");
  g_signal_connect (widget, "response", G_CALLBACK (info_bar_response), NULL); 
  g_object_set_data (G_OBJECT (window), "infobar", widget);

  dialog = (GtkWidget *)gtk_builder_get_object (builder, "info_dialog");
  g_signal_connect (dialog, "response", G_CALLBACK (close_dialog), NULL);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "info_dialog_button");
  g_signal_connect (widget, "clicked", G_CALLBACK (show_dialog), dialog);

  dialog = (GtkWidget *)gtk_builder_get_object (builder, "action_dialog");
  g_signal_connect (dialog, "response", G_CALLBACK (close_dialog), NULL);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "action_dialog_button");
  g_signal_connect (widget, "clicked", G_CALLBACK (show_dialog), dialog);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "act_action_dialog");
  stack = (GtkWidget *)gtk_builder_get_object (builder, "toplevel_stack");
  g_signal_connect (widget, "clicked", G_CALLBACK (action_dialog_button_clicked), stack);
  g_signal_connect (stack, "notify::visible-child-name", G_CALLBACK (page_changed_cb), NULL);
  page_changed_cb (stack, NULL, NULL);

  page_stack = stack;

  dialog = (GtkWidget *)gtk_builder_get_object (builder, "preference_dialog");
  g_signal_connect (dialog, "response", G_CALLBACK (close_dialog), NULL);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "preference_dialog_button");
  g_signal_connect (widget, "clicked", G_CALLBACK (show_dialog), dialog);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "circular_button");
  g_signal_connect (widget, "clicked", G_CALLBACK (show_dialog), dialog);

  dialog = (GtkWidget *)gtk_builder_get_object (builder, "selection_dialog");
  g_object_set_data (G_OBJECT (window), "selection_dialog", dialog);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "text3");
  g_signal_connect (dialog, "response", G_CALLBACK (close_selection_dialog), widget);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "selection_dialog_button");
  g_signal_connect (widget, "clicked", G_CALLBACK (show_dialog), dialog);

  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "selection_flowbox");
  g_object_set_data (G_OBJECT (window), "selection_flowbox", widget2);
  g_signal_connect_swapped (widget, "clicked", G_CALLBACK (populate_flowbox), widget2);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "charletree");
  populate_model ((GtkTreeStore *)gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (widget), row_separator_func, NULL, NULL);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (widget));

  widget = GTK_WIDGET (gtk_builder_get_object (builder, "munsell"));
  widget2 = GTK_WIDGET (gtk_builder_get_object (builder, "cchooser"));

  populate_colors (widget, widget2);
  g_signal_connect (widget2, "notify::rgba", G_CALLBACK (rgba_changed), widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "page_combo");
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (widget), page_combo_separator_func, NULL, NULL);
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "range_from_spin");
  widget3 = (GtkWidget *)gtk_builder_get_object (builder, "range_to_spin");
  widget4 = (GtkWidget *)gtk_builder_get_object (builder, "print_button");
  g_object_set_data (G_OBJECT (widget), "range_from_spin", widget2);
  g_object_set_data (G_OBJECT (widget3), "range_from_spin", widget2);
  g_object_set_data (G_OBJECT (widget), "range_to_spin", widget3);
  g_object_set_data (G_OBJECT (widget2), "range_to_spin", widget3);
  g_object_set_data (G_OBJECT (widget), "print_button", widget4);

  set_accel (GTK_APPLICATION (app), GTK_WIDGET (gtk_builder_get_object (builder, "quitmenuitem")));
  set_accel (GTK_APPLICATION (app), GTK_WIDGET (gtk_builder_get_object (builder, "deletemenuitem")));
  set_accel (GTK_APPLICATION (app), GTK_WIDGET (gtk_builder_get_object (builder, "searchmenuitem")));
  set_accel (GTK_APPLICATION (app), GTK_WIDGET (gtk_builder_get_object (builder, "darkmenuitem")));
  set_accel (GTK_APPLICATION (app), GTK_WIDGET (gtk_builder_get_object (builder, "aboutmenuitem")));
  set_accel (GTK_APPLICATION (app), GTK_WIDGET (gtk_builder_get_object (builder, "bgmenuitem")));

  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "tooltextview");

  widget = (GtkWidget *)gtk_builder_get_object (builder, "toolbutton1");
  g_signal_connect (widget, "clicked", G_CALLBACK (handle_insert), widget2);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "toolbutton2");
  g_signal_connect (widget, "clicked", G_CALLBACK (handle_insert), widget2);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "toolbutton3");
  g_signal_connect (widget, "clicked", G_CALLBACK (handle_insert), widget2);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "toolbutton4");
  g_signal_connect (widget, "clicked", G_CALLBACK (handle_insert), widget2);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "cutbutton");
  g_signal_connect (widget, "clicked", G_CALLBACK (handle_cutcopypaste), widget2);
  g_signal_connect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget2)), "notify::has-selection",
                    G_CALLBACK (textbuffer_notify_selection), widget);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "copybutton");
  g_signal_connect (widget, "clicked", G_CALLBACK (handle_cutcopypaste), widget2);
  g_signal_connect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget2)), "notify::has-selection",
                    G_CALLBACK (textbuffer_notify_selection), widget);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "deletebutton");
  g_signal_connect (widget, "clicked", G_CALLBACK (handle_cutcopypaste), widget2);
  g_signal_connect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget2)), "notify::has-selection",
                    G_CALLBACK (textbuffer_notify_selection), widget);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "pastebutton");
  g_signal_connect (widget, "clicked", G_CALLBACK (handle_cutcopypaste), widget2);
  g_signal_connect_object (gtk_widget_get_clipboard (widget2), "notify::formats",
                           G_CALLBACK (clipboard_formats_notify), widget, 0);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "osd_frame");
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "totem_like_osd");
  g_object_set_data (G_OBJECT (widget), "osd", widget2);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "textview1");
  text_view_add_to_context_menu (GTK_TEXT_VIEW (widget));

  widget = (GtkWidget *)gtk_builder_get_object (builder, "open_popover");
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "open_popover_entry");
  widget3 = (GtkWidget *)gtk_builder_get_object (builder, "open_popover_button");
  gtk_popover_set_default_widget (GTK_POPOVER (widget), widget3);
  g_signal_connect (widget2, "notify::text", G_CALLBACK (open_popover_text_changed), widget3);
  g_signal_connect_swapped (widget3, "clicked", G_CALLBACK (gtk_widget_hide), widget);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "open_menubutton");
  g_object_set_data (G_OBJECT (window), "open_menubutton", widget);
  widget = (GtkWidget *)gtk_builder_get_object (builder, "record_button");
  g_object_set_data (G_OBJECT (window), "record_button", widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "lockbox");
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "lockbutton");
  g_object_set_data (G_OBJECT (window), "lockbutton", widget2);
  permission = g_object_new (g_test_permission_get_type (), NULL);
  g_object_bind_property (permission, "allowed",
                          widget, "sensitive",
                          G_BINDING_SYNC_CREATE);
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "open");
  g_object_bind_property (permission, "allowed",
                          action, "enabled",
                          G_BINDING_SYNC_CREATE);
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "record");
  g_object_bind_property (permission, "allowed",
                          action, "enabled",
                          G_BINDING_SYNC_CREATE);
  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (widget2), permission);
  g_object_unref (permission);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "iconview1");
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "increase_button");
  g_object_set_data (G_OBJECT (widget), "increase_button", widget2);
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "decrease_button");
  g_object_set_data (G_OBJECT (widget), "decrease_button", widget2);
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "reset_button");
  g_object_set_data (G_OBJECT (widget), "reset_button", widget2);
  reset_icon_size (widget);

  adj = (GtkAdjustment *)gtk_builder_get_object (builder, "adjustment3");
  widget = (GtkWidget *)gtk_builder_get_object (builder, "progressbar1");
  widget2 = (GtkWidget *)gtk_builder_get_object (builder, "progressbar2");
  g_signal_connect (adj, "value-changed", G_CALLBACK (adjustment3_value_changed), widget);
  g_signal_connect (adj, "value-changed", G_CALLBACK (adjustment3_value_changed), widget2);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "extra_info_entry");
  g_timeout_add (100, (GSourceFunc)pulse_it, widget);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "box_for_context");
  model = (GMenuModel *)gtk_builder_get_object (builder, "new_style_context_menu_model");
  set_up_context_popover (widget, model);

  gtk_widget_show (GTK_WIDGET (window));

  g_object_unref (builder);
}

static void
print_version (void)
{
  g_print ("gtk4-widget-factory %d.%d.%d\n",
           gtk_get_major_version (),
           gtk_get_minor_version (),
           gtk_get_micro_version ());
}

static int
local_options (GApplication *app,
               GVariantDict *options,
               gpointer      data)
{
  gboolean version = FALSE;

  g_variant_dict_lookup (options, "version", "b", &version);

  if (version)
    {
      print_version ();
      return 0;
    }

  return -1;
}

static void
activate_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  g_print ("Activate action %s\n", g_action_get_name (G_ACTION (action)));
}

static void
select_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  g_print ("Select action %s value %s\n",
           g_action_get_name (G_ACTION (action)),
           g_variant_get_string (parameter, NULL));

  g_simple_action_set_state (action, parameter);
}

static void
toggle_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GVariant *state = g_action_get_state (G_ACTION (action));

  g_print ("Toggle action %s to %s\n",
           g_action_get_name (G_ACTION (action)),
           g_variant_get_boolean (state) ? "false" : "true");

  g_simple_action_set_state (action,
                             g_variant_new_boolean (!g_variant_get_boolean (state)));
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;
  GAction *action;
  static GActionEntry app_entries[] = {
    { "about", activate_about, NULL, NULL, NULL },
    { "quit", activate_quit, NULL, NULL, NULL },
    { "inspector", activate_inspector, NULL, NULL, NULL },
    { "main", NULL, "s", "'steak'", NULL },
    { "wine", NULL, NULL, "false", NULL },
    { "beer", NULL, NULL, "false", NULL },
    { "water", NULL, NULL, "true", NULL },
    { "dessert", NULL, "s", "'bars'", NULL },
    { "pay", NULL, "s", NULL, NULL },
    { "print", activate_action, NULL, NULL, NULL },
    { "share", activate_action, NULL, NULL, NULL },
    { "labels", activate_action, NULL, NULL, NULL },
    { "new", activate_action, NULL, NULL, NULL },
    { "open", activate_action, NULL, NULL, NULL },
    { "open-in", activate_action, NULL, NULL, NULL },
    { "open-tab", activate_action, NULL, NULL, NULL },
    { "open-window", activate_action, NULL, NULL, NULL },
    { "save", activate_action, NULL, NULL, NULL },
    { "save-as", activate_action, NULL, NULL, NULL },
    { "cut", activate_action, NULL, NULL, NULL },
    { "copy", activate_action, NULL, NULL, NULL },
    { "paste", activate_action, NULL, NULL, NULL },
    { "pin", toggle_action, NULL, "true", NULL },
    { "size", select_action, "s", "'medium'", NULL },
    { "berk", toggle_action, NULL, "true", NULL },
    { "broni", toggle_action, NULL, "true", NULL },
    { "drutt", toggle_action, NULL, "true", NULL },
    { "upstairs", toggle_action, NULL, "true", NULL },
    { "option-a", activate_action, NULL, NULL, NULL },
    { "option-b", activate_action, NULL, NULL, NULL },
    { "option-c", activate_action, NULL, NULL, NULL },
    { "option-d", activate_action, NULL, NULL, NULL },
    { "check-on", NULL, NULL, "true", NULL },
    { "check-off", NULL, NULL, "false", NULL },
    { "radio-x", NULL, "s", "'x'", NULL },
    { "check-on-disabled", NULL, NULL, "true", NULL },
    { "check-off-disabled", NULL, NULL, "false", NULL },
    { "radio-x-disabled", NULL, "s", "'x'", NULL },
  };
  gint status;

  app = gtk_application_new ("org.gtk.WidgetFactory4", G_APPLICATION_NON_UNIQUE);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  action = g_action_map_lookup_action (G_ACTION_MAP (app), "wine");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  action = g_action_map_lookup_action (G_ACTION_MAP (app), "check-on-disabled");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  action = g_action_map_lookup_action (G_ACTION_MAP (app), "check-off-disabled");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  action = g_action_map_lookup_action (G_ACTION_MAP (app), "radio-x-disabled");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  g_application_add_main_option (G_APPLICATION (app), "version", 0, 0, G_OPTION_ARG_NONE, "Show program version", NULL);

  g_signal_connect (app, "handle-local-options", G_CALLBACK (local_options), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
