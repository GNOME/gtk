/* example-start calendar calendar.c */
/*
 * Copyright (C) 1998 Cesar Miquel, Shawn T. Amundson, Mattias Gr�nlund
 * Copyright (C) 2000 Tony Gale
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#define DEF_PAD 12
#define DEF_PAD_SMALL 6

#define TM_YEAR_BASE 1900

typedef struct _CalendarData
{
  GtkWidget *calendar_widget;
  GtkWidget *flag_checkboxes[6];
  gboolean  settings[6];
  GtkWidget *font_dialog;
  GtkWidget *window;
  GtkWidget *prev2_sig;
  GtkWidget *prev_sig;
  GtkWidget *last_sig;
  GtkWidget *month;

  GHashTable    *details_table;
  GtkTextBuffer *details_buffer;
  guint          details_changed;
} CalendarData;

enum
{
  calendar_show_header,
  calendar_show_days,
  calendar_month_change, 
  calendar_show_week,
  calendar_monday_first
};

/*
 * GtkCalendar
 */

static void
calendar_date_to_string (CalendarData *data,
			      char         *buffer,
			      gint          buff_len)
{
  GDate *date;
  guint year, month, day;

  gtk_calendar_get_date (GTK_CALENDAR(data->window),
			 &year, &month, &day);
  date = g_date_new_dmy (day, month + 1, year);
  g_date_strftime (buffer, buff_len-1, "%x", date);

  g_date_free (date);
}

static void
calendar_set_detail (CalendarData *data,
                     guint         year,
                     guint         month,
                     guint         day,
                     gchar        *detail)
{
  gchar *key = g_strdup_printf ("%04d-%02d-%02d", year, month + 1, day);
  g_hash_table_replace (data->details_table, key, detail);
}

static gchar*
calendar_get_detail (CalendarData *data,
                     guint         year,
                     guint         month,
                     guint         day)
{
  const gchar *detail;
  gchar *key;

  key = g_strdup_printf ("%04d-%02d-%02d", year, month + 1, day);
  detail = g_hash_table_lookup (data->details_table, key);
  g_free (key);

  return (detail ? g_strdup (detail) : NULL);
}

static void
calendar_update_details (CalendarData *data)
{
  guint year, month, day;
  gchar *detail;

  gtk_calendar_get_date (GTK_CALENDAR (data->calendar_widget), &year, &month, &day);
  detail = calendar_get_detail (data, year, month, day);

  g_signal_handler_block (data->details_buffer, data->details_changed);
  gtk_text_buffer_set_text (data->details_buffer, detail ? detail : "", -1);
  g_signal_handler_unblock (data->details_buffer, data->details_changed);

  g_free (detail);
}

static void
calendar_set_signal_strings (char         *sig_str,
				  CalendarData *data)
{
  const gchar *prev_sig;

  prev_sig = gtk_label_get_text (GTK_LABEL (data->prev_sig));
  gtk_label_set_text (GTK_LABEL (data->prev2_sig), prev_sig);

  prev_sig = gtk_label_get_text (GTK_LABEL (data->last_sig));
  gtk_label_set_text (GTK_LABEL (data->prev_sig), prev_sig);
  gtk_label_set_text (GTK_LABEL (data->last_sig), sig_str);
}

static void
calendar_month_changed (GtkWidget    *widget,
                             CalendarData *data)
{
  char buffer[256] = "month_changed: ";

  calendar_date_to_string (data, buffer+15, 256-15);
  calendar_set_signal_strings (buffer, data);
}

static void
calendar_day_selected (GtkWidget    *widget,
                            CalendarData *data)
{
  char buffer[256] = "day_selected: ";

  calendar_date_to_string (data, buffer+14, 256-14);
  calendar_set_signal_strings (buffer, data);

  calendar_update_details (data);
}

static void
calendar_day_selected_double_click (GtkWidget    *widget,
                                         CalendarData *data)
{
  char buffer[256] = "day_selected_double_click: ";
  guint day;

  calendar_date_to_string (data, buffer+27, 256-27);
  calendar_set_signal_strings (buffer, data);

  gtk_calendar_get_date (GTK_CALENDAR (data->window),
			 NULL, NULL, &day);

  if (GTK_CALENDAR (data->window)->marked_date[day-1] == 0) {
    gtk_calendar_mark_day (GTK_CALENDAR (data->window), day);
  } else { 
    gtk_calendar_unmark_day (GTK_CALENDAR (data->window), day);
  }
}

static void
calendar_prev_month (GtkWidget    *widget,
                          CalendarData *data)
{
  char buffer[256] = "prev_month: ";

  calendar_date_to_string (data, buffer+12, 256-12);
  calendar_set_signal_strings (buffer, data);
}

static void
calendar_next_month (GtkWidget    *widget,
                     CalendarData *data)
{
  char buffer[256] = "next_month: ";

  calendar_date_to_string (data, buffer+12, 256-12);
  calendar_set_signal_strings (buffer, data);
}

static void
calendar_prev_year (GtkWidget    *widget,
                    CalendarData *data)
{
  char buffer[256] = "prev_year: ";

  calendar_date_to_string (data, buffer+11, 256-11);
  calendar_set_signal_strings (buffer, data);
}

static void
calendar_next_year (GtkWidget    *widget,
                    CalendarData *data)
{
  char buffer[256] = "next_year: ";

  calendar_date_to_string (data, buffer+11, 256-11);
  calendar_set_signal_strings (buffer, data);
}


static void
calendar_set_flags (CalendarData *calendar)
{
  gint options = 0, i;

  for (i = 0; i < G_N_ELEMENTS (calendar->settings); i++)
    if (calendar->settings[i])
      options=options + (1 << i);

  if (calendar->window)
    gtk_calendar_set_display_options (GTK_CALENDAR (calendar->window), options);
}

static void
calendar_toggle_flag (GtkWidget    *toggle,
                      CalendarData *calendar)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (calendar->flag_checkboxes); i++)
    if (calendar->flag_checkboxes[i] == toggle)
      calendar->settings[i] = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));

  calendar_set_flags(calendar);
  
}

void calendar_select_font (GtkWidget    *button,
                                 CalendarData *calendar)
{
  const char *font = NULL;
  GtkRcStyle *style;

  if (calendar->window)
    font = gtk_font_button_get_font_name (GTK_FONT_BUTTON (button));

  if (font)
	{
	  style = gtk_rc_style_new ();
	  pango_font_description_free (style->font_desc);
      style->font_desc = pango_font_description_from_string (font);
	  gtk_widget_modify_style (calendar->window, style);
	}
}

static gchar*
calendar_detail_cb (GtkCalendar *calendar,
                    guint        year,
                    guint        month,
                    guint        day,
                    gpointer     data)
{
  return calendar_get_detail (data, year, month, day);
}

static void
calendar_details_changed (GtkTextBuffer *buffer,
                          CalendarData  *data)
{
  GtkTextIter start, end;
  guint year, month, day;
  gchar *detail;

  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  gtk_calendar_get_date (GTK_CALENDAR (data->calendar_widget), &year, &month, &day);
  detail = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  if (!detail[0])
    {
      g_free (detail);
      detail = NULL;
    }

  calendar_set_detail (data, year, month, day, detail);
  gtk_widget_queue_resize (data->calendar_widget);
}

static void
demonstrate_details (CalendarData *data)
{
  static char *rainbow[] = { "#900", "#980", "#390", "#095", "#059", "#309", "#908" };
  GtkCalendar *calendar = GTK_CALENDAR (data->calendar_widget);
  gint row, col;

  for (row = 0; row < 6; ++row)
    for (col = 0; col < 7; ++col)
      {
        gint year, month, day;
        gchar *detail;
    
        year = calendar->year;
        month = calendar->month;
        month += calendar->day_month[row][col];
        day = calendar->day[row][col];
    
        if (month < 1)
          {
            month += 12;
            year -= 1;
          }
        else if (month > 12)
          {
            month -= 12;
            year += 1;
  }

        detail = g_strdup_printf ("<span color='%s'>yadda\n"
                                  "(%04d-%02d-%02d)</span>",
                                  rainbow[(day - 1) % 7],
                                  year, month, day);

        calendar_set_detail (data, year, month - 1, day, detail);
      }

  gtk_widget_queue_resize (data->calendar_widget);
  calendar_update_details (data);
}

static void
reset_details (CalendarData *data)
{
  g_hash_table_remove_all (data->details_table);
  gtk_widget_queue_resize (data->calendar_widget);
  calendar_update_details (data);
}

static void
calendar_toggle_details (GtkWidget    *widget,
                         CalendarData *data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    gtk_calendar_set_detail_func (GTK_CALENDAR (data->calendar_widget),
                                  calendar_detail_cb, data, NULL);
  else
    gtk_calendar_set_detail_func (GTK_CALENDAR (data->calendar_widget),
                                  NULL, NULL, NULL);
}

static GtkWidget*
create_expander (const char *caption,
                 GtkWidget  *child,
                 gdouble     xscale,
                 gdouble     yscale)
{
  GtkWidget *expander = gtk_expander_new ("");
  GtkWidget *label = gtk_expander_get_label_widget (GTK_EXPANDER (expander));
  GtkWidget *align = gtk_alignment_new (0, 0, xscale, yscale);

  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 6, 0, 18, 0);
  gtk_label_set_markup (GTK_LABEL (label), caption);

  gtk_container_add (GTK_CONTAINER (expander), align);
  gtk_container_add (GTK_CONTAINER (align), child);

  return expander;
}

static GtkWidget*
create_frame (const char *caption,
              GtkWidget  *child,
              gdouble     xscale,
              gdouble     yscale)
{
  GtkWidget *frame = gtk_frame_new ("");
  GtkWidget *label = gtk_frame_get_label_widget (GTK_FRAME (frame));
  GtkWidget *align = gtk_alignment_new (0, 0, xscale, yscale);

  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 6, 0, 18, 0);
  gtk_label_set_markup (GTK_LABEL (label), caption);

  gtk_container_add (GTK_CONTAINER (frame), align);
  gtk_container_add (GTK_CONTAINER (align), child);

  return frame;
}

static void
detail_width_changed (GtkSpinButton *button,
                      CalendarData  *data)
{
  gint value = (gint) gtk_spin_button_get_value (button);
  gtk_calendar_set_detail_width_chars (GTK_CALENDAR (data->calendar_widget), value);
}

static void
detail_height_changed (GtkSpinButton *button,
                      CalendarData  *data)
{
  gint value = (gint) gtk_spin_button_get_value (button);
  gtk_calendar_set_detail_height_rows (GTK_CALENDAR (data->calendar_widget), value);
}

static void
create_calendar(void)
{
  static CalendarData calendar_data;

  GtkWidget *window, *hpaned, *vbox, *rpane, *hbox;
  GtkWidget *calendar, *toggle, *scroller, *button;
  GtkWidget *frame, *label, *bbox, *align, *details;

  GtkSizeGroup *size;
  GtkStyle *style;
  gchar *font;
  gint i;
  
  struct {
    gboolean init;
    char *label;
  } flags[] =
    {
      { TRUE,  "Show _Heading" },
      { TRUE,  "Show Day _Names" },
      { FALSE, "No Month _Change" },
      { TRUE,  "Show _Week Numbers" },
      { FALSE, "Week Start _Monday" },
      { TRUE,  "Show De_tails" },
    };

  calendar_data.window = NULL;
  calendar_data.font_dialog = NULL;
  calendar_data.details_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  for (i = 0; i < G_N_ELEMENTS (calendar_data.settings); i++)
    calendar_data.settings[i] = 0;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GtkCalendar Example");
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_main_quit),
		    NULL);
  g_signal_connect (window, "delete-event",
		    G_CALLBACK (gtk_false),
		    NULL);

  hpaned = gtk_hpaned_new ();

  /* Calendar widget */

  calendar = gtk_calendar_new ();
  calendar_data.calendar_widget = calendar;
  frame = create_frame ("<b>Calendar</b>", calendar, 0, 0);
  gtk_paned_pack1 (GTK_PANED (hpaned), frame, TRUE, FALSE);

  calendar_data.window = calendar;
  calendar_set_flags(&calendar_data);
  gtk_calendar_mark_day (GTK_CALENDAR (calendar), 19);	

  g_signal_connect (calendar, "month_changed", 
		    G_CALLBACK (calendar_month_changed),
		    &calendar_data);
  g_signal_connect (calendar, "day_selected", 
		    G_CALLBACK (calendar_day_selected),
		    &calendar_data);
  g_signal_connect (calendar, "day_selected_double_click", 
		    G_CALLBACK (calendar_day_selected_double_click),
		    &calendar_data);
  g_signal_connect (calendar, "prev_month", 
		    G_CALLBACK (calendar_prev_month),
		    &calendar_data);
  g_signal_connect (calendar, "next_month", 
		    G_CALLBACK (calendar_next_month),
		    &calendar_data);
  g_signal_connect (calendar, "prev_year", 
		    G_CALLBACK (calendar_prev_year),
		    &calendar_data);
  g_signal_connect (calendar, "next_year", 
		    G_CALLBACK (calendar_next_year),
		    &calendar_data);

  rpane = gtk_vbox_new (FALSE, DEF_PAD_SMALL);
  gtk_paned_pack2 (GTK_PANED (hpaned), rpane, FALSE, FALSE);

  /* Build the right font-button */

  vbox = gtk_vbox_new(FALSE, DEF_PAD_SMALL);
  frame = create_frame ("<b>Options</b>", vbox, 1, 0);
  gtk_box_pack_start (GTK_BOX (rpane), frame, FALSE, TRUE, 0);
  size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_widget_ensure_style (calendar);
  style = gtk_widget_get_style (calendar);
  font = pango_font_description_to_string (style->font_desc);
  button = gtk_font_button_new_with_font (font);
  g_free (font);

  g_signal_connect (button, "font-set",
                    G_CALLBACK(calendar_select_font),
                    &calendar_data);

  label = gtk_label_new_with_mnemonic ("_Font:");
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_size_group_add_widget (size, label);

  hbox = gtk_hbox_new (FALSE, DEF_PAD_SMALL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

  /* Build the width entry */

  button = gtk_spin_button_new_with_range (0, 127, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (button),
                             gtk_calendar_get_detail_width_chars (GTK_CALENDAR (calendar)));

  g_signal_connect (button, "value-changed",
                    G_CALLBACK (detail_width_changed),
                    &calendar_data);

  label = gtk_label_new_with_mnemonic ("Details W_idth:");
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_size_group_add_widget (size, label);

  hbox = gtk_hbox_new (FALSE, DEF_PAD_SMALL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

  /* Build the height entry */

  button = gtk_spin_button_new_with_range (0, 127, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (button),
                             gtk_calendar_get_detail_height_rows (GTK_CALENDAR (calendar)));

  g_signal_connect (button, "value-changed",
                    G_CALLBACK (detail_height_changed),
                    &calendar_data);

  label = gtk_label_new_with_mnemonic ("Details H_eight:");
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_size_group_add_widget (size, label);

  hbox = gtk_hbox_new (FALSE, DEF_PAD_SMALL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

  /* Build the right details frame */

  vbox = gtk_vbox_new(FALSE, DEF_PAD_SMALL);
  frame = create_frame ("<b>Details</b>", vbox, 1, 1);
  gtk_box_pack_start (GTK_BOX (rpane), frame, FALSE, TRUE, 0);

  details = gtk_text_view_new();
  calendar_data.details_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (details));

  calendar_data.details_changed = g_signal_connect (calendar_data.details_buffer, "changed",
                                                    G_CALLBACK (calendar_details_changed),
                                                    &calendar_data);

  scroller = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroller), details);

  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroller),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gtk_box_pack_start (GTK_BOX (vbox), scroller, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, DEF_PAD_SMALL);
  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), hbox);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, TRUE, 0);

  button = gtk_button_new_with_mnemonic ("Demonstrate _Details");

  g_signal_connect_swapped (button,
                            "clicked",
                            G_CALLBACK (demonstrate_details),
                            &calendar_data);

  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);

  button = gtk_button_new_with_mnemonic ("_Reset Details");

  g_signal_connect_swapped (button,
                            "clicked",
                            G_CALLBACK (reset_details),
                            &calendar_data);

  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);

  toggle = gtk_check_button_new_with_mnemonic ("_Use Details");
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK(calendar_toggle_details),
                    &calendar_data);
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, TRUE, 0);
  
  /* Build the Right frame with the flags in */ 

  vbox = gtk_vbox_new(FALSE, 0);
  frame = create_expander ("<b>Flags</b>", vbox, 1, 0);
  gtk_box_pack_start (GTK_BOX (rpane), frame, TRUE, TRUE, 0);

  for (i = 0; i < G_N_ELEMENTS (calendar_data.settings); i++)
    {
      toggle = gtk_check_button_new_with_mnemonic(flags[i].label);
      gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, TRUE, 0);
      calendar_data.flag_checkboxes[i] = toggle;

      g_signal_connect (toggle, "toggled",
                        G_CALLBACK (calendar_toggle_flag),
			&calendar_data);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), flags[i].init);
    }

  /*
   *  Build the Signal-event part.
   */

  vbox = gtk_vbox_new (TRUE, DEF_PAD_SMALL);
  frame = create_frame ("<b>Signal Events</b>", vbox, 1, 0);
  
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  label = gtk_label_new ("Signal:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  calendar_data.last_sig = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), calendar_data.last_sig, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  label = gtk_label_new ("Previous signal:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  calendar_data.prev_sig = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), calendar_data.prev_sig, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  label = gtk_label_new ("Second previous signal:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  calendar_data.prev2_sig = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), calendar_data.prev2_sig, FALSE, TRUE, 0);

  /*
   *  Glue everything together
   */

  bbox = gtk_hbutton_box_new ();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);

  button = gtk_button_new_with_label ("Close");
  g_signal_connect (button, "clicked", G_CALLBACK (gtk_main_quit), NULL);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  vbox = gtk_vbox_new (FALSE, DEF_PAD_SMALL);

  gtk_box_pack_start (GTK_BOX (vbox), hpaned,                TRUE,  TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), frame,                 FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), bbox,                  FALSE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  gtk_widget_set_can_default (button, TRUE);
  gtk_widget_grab_default (button);

  gtk_window_set_default_size (GTK_WINDOW (window), 600, 0);
  gtk_widget_show_all (window);
}


int main(int   argc,
         char *argv[] )
{
  gtk_init (&argc, &argv);

  create_calendar();

  gtk_main();

  return(0);
}
/* example-end */
