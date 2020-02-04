/* example-start calendar calendar.c */
/*
 * Copyright (C) 1998 Cesar Miquel, Shawn T. Amundson, Mattias Grönlund
 * Copyright (C) 2000 Tony Gale
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

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
  if (g_date_valid_dmy (day, month + 1, year))
    {
      date = g_date_new_dmy (day, month + 1, year);
      g_date_strftime (buffer, buff_len-1, "%x", date);
      g_date_free (date);
    }
  else
    {
      g_snprintf (buffer, buff_len - 1, "%d/%d/%d (invalid)", month + 1, day, year);
    }
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

  if (gtk_calendar_get_day_is_marked (GTK_CALENDAR (data->window), day))
    gtk_calendar_unmark_day (GTK_CALENDAR (data->window), day);
  else
    gtk_calendar_mark_day (GTK_CALENDAR (data->window), day);
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

static GtkWidget*
create_frame (const char *caption,
              GtkWidget  *child,
              GtkAlign    halign,
              GtkAlign    valign)
{
  GtkWidget *frame = gtk_frame_new ("");
  GtkWidget *label = gtk_frame_get_label_widget (GTK_FRAME (frame));

  g_object_set (child,
                "margin-top", 6,
                "margin-bottom", 0,
                "margin-start", 18,
                "margin-end", 0,
                "halign", halign,
                "valign", valign,
                NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_label_set_markup (GTK_LABEL (label), caption);

  gtk_container_add (GTK_CONTAINER (frame), child);

  return frame;
}

static void
create_calendar(void)
{
  static CalendarData calendar_data;

  GtkWidget *window, *hpaned, *vbox, *rpane, *hbox;
  GtkWidget *calendar, *toggle, *button;
  GtkWidget *frame, *label, *bbox;
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

  for (i = 0; i < G_N_ELEMENTS (calendar_data.settings); i++)
    calendar_data.settings[i] = 0;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_hide_on_close (GTK_WINDOW (window), TRUE);
  gtk_window_set_title (GTK_WINDOW (window), "GtkCalendar Example");
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

  /* Calendar widget */

  calendar = gtk_calendar_new ();
  calendar_data.calendar_widget = calendar;
  frame = create_frame ("<b>Calendar</b>", calendar, GTK_ALIGN_CENTER, GTK_ALIGN_CENTER);
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

  rpane = gtk_box_new (GTK_ORIENTATION_VERTICAL, DEF_PAD_SMALL);
  gtk_paned_pack2 (GTK_PANED (hpaned), rpane, FALSE, FALSE);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, DEF_PAD_SMALL);
  frame = create_frame ("<b>Options</b>", vbox, GTK_ALIGN_FILL, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (rpane), frame);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, DEF_PAD_SMALL);
  gtk_widget_set_halign (hbox, GTK_ALIGN_START);
  gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  /* Build the Right frame with the flags in */

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (rpane), vbox);

  for (i = 0; i < G_N_ELEMENTS (calendar_data.settings); i++)
    {
      toggle = gtk_check_button_new_with_mnemonic(flags[i].label);
      gtk_container_add (GTK_CONTAINER (vbox), toggle);
      calendar_data.flag_checkboxes[i] = toggle;

      g_signal_connect (toggle, "toggled",
                        G_CALLBACK (calendar_toggle_flag),
                        &calendar_data);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), flags[i].init);
    }

  /*
   *  Build the Signal-event part.
   */

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, DEF_PAD_SMALL);
  gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
  frame = create_frame ("<b>Signal Events</b>", vbox, GTK_ALIGN_FILL, GTK_ALIGN_CENTER);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  label = gtk_label_new ("Signal:");
  gtk_container_add (GTK_CONTAINER (hbox), label);
  calendar_data.last_sig = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (hbox), calendar_data.last_sig);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  label = gtk_label_new ("Previous signal:");
  gtk_container_add (GTK_CONTAINER (hbox), label);
  calendar_data.prev_sig = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (hbox), calendar_data.prev_sig);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  label = gtk_label_new ("Second previous signal:");
  gtk_container_add (GTK_CONTAINER (hbox), label);
  calendar_data.prev2_sig = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (hbox), calendar_data.prev2_sig);

  /*
   *  Glue everything together
   */

  bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (bbox, GTK_ALIGN_END);

  button = gtk_button_new_with_label ("Close");
  g_signal_connect (button, "clicked", G_CALLBACK (gtk_main_quit), NULL);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, DEF_PAD_SMALL);

  gtk_container_add (GTK_CONTAINER (vbox), hpaned);
  gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_container_add (GTK_CONTAINER (vbox), bbox);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  gtk_window_set_default_widget (GTK_WINDOW (window), button);

  gtk_window_set_default_size (GTK_WINDOW (window), 600, 0);
  g_signal_connect (window, "close-request", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);
}


int main(int   argc,
         char *argv[] )
{
  gtk_init ();

  if (g_getenv ("GTK_RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  create_calendar();

  gtk_main();

  return(0);
}
/* example-end */
