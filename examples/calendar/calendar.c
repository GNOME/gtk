/* example-start calendar calendar.c */
/*
 * Copyright (C) 1998 Cesar Miquel, Shawn T. Amundson, Mattias Grönlund
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

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DEF_PAD 10
#define DEF_PAD_SMALL 5

#define TM_YEAR_BASE 1900

typedef struct _CalendarData {
  GtkWidget *flag_checkboxes[5];
  gboolean  settings[5];
  gchar     *font;
  GtkWidget *font_dialog;
  GtkWidget *window;
  GtkWidget *prev2_sig;
  GtkWidget *prev_sig;
  GtkWidget *last_sig;
  GtkWidget *month;
} CalendarData;

enum {
  calendar_show_header,
  calendar_show_days,
  calendar_month_change, 
  calendar_show_week,
  calendar_monday_first
};

/*
 * GtkCalendar
 */

void calendar_date_to_string( CalendarData *data,
			      char         *buffer,
			      gint          buff_len )
{
  struct tm tm;
  time_t time;

  memset (&tm, 0, sizeof (tm));
  gtk_calendar_get_date (GTK_CALENDAR(data->window),
			 &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
  tm.tm_year -= TM_YEAR_BASE;
  time = mktime(&tm);
  strftime (buffer, buff_len-1, "%x", gmtime(&time));
}

void calendar_set_signal_strings( char         *sig_str,
				  CalendarData *data)
{
  gchar *prev_sig;

  gtk_label_get (GTK_LABEL (data->prev_sig), &prev_sig);
  gtk_label_set (GTK_LABEL (data->prev2_sig), prev_sig);

  gtk_label_get (GTK_LABEL (data->last_sig), &prev_sig);
  gtk_label_set (GTK_LABEL (data->prev_sig), prev_sig);
  gtk_label_set (GTK_LABEL (data->last_sig), sig_str);
}

void calendar_month_changed( GtkWidget    *widget,
                             CalendarData *data )
{
  char buffer[256] = "month_changed: ";

  calendar_date_to_string (data, buffer+15, 256-15);
  calendar_set_signal_strings (buffer, data);
}

void calendar_day_selected( GtkWidget    *widget,
                            CalendarData *data )
{
  char buffer[256] = "day_selected: ";

  calendar_date_to_string (data, buffer+14, 256-14);
  calendar_set_signal_strings (buffer, data);
}

void calendar_day_selected_double_click( GtkWidget    *widget,
                                         CalendarData *data )
{
  struct tm tm;
  char buffer[256] = "day_selected_double_click: ";

  calendar_date_to_string (data, buffer+27, 256-27);
  calendar_set_signal_strings (buffer, data);

  memset (&tm, 0, sizeof (tm));
  gtk_calendar_get_date (GTK_CALENDAR(data->window),
			 &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
  tm.tm_year -= TM_YEAR_BASE;

  if(GTK_CALENDAR(data->window)->marked_date[tm.tm_mday-1] == 0) {
    gtk_calendar_mark_day(GTK_CALENDAR(data->window),tm.tm_mday);
  } else { 
    gtk_calendar_unmark_day(GTK_CALENDAR(data->window),tm.tm_mday);
  }
}

void calendar_prev_month( GtkWidget    *widget,
                            CalendarData *data )
{
  char buffer[256] = "prev_month: ";

  calendar_date_to_string (data, buffer+12, 256-12);
  calendar_set_signal_strings (buffer, data);
}

void calendar_next_month( GtkWidget    *widget,
                            CalendarData *data )
{
  char buffer[256] = "next_month: ";

  calendar_date_to_string (data, buffer+12, 256-12);
  calendar_set_signal_strings (buffer, data);
}

void calendar_prev_year( GtkWidget    *widget,
                            CalendarData *data )
{
  char buffer[256] = "prev_year: ";

  calendar_date_to_string (data, buffer+11, 256-11);
  calendar_set_signal_strings (buffer, data);
}

void calendar_next_year( GtkWidget    *widget,
                            CalendarData *data )
{
  char buffer[256] = "next_year: ";

  calendar_date_to_string (data, buffer+11, 256-11);
  calendar_set_signal_strings (buffer, data);
}


void calendar_set_flags( CalendarData *calendar )
{
  gint i;
  gint options=0;
  for (i=0;i<5;i++) 
    if (calendar->settings[i])
      {
	options=options + (1<<i);
      }
  if (calendar->window)
    gtk_calendar_display_options (GTK_CALENDAR (calendar->window), options);
}

void calendar_toggle_flag( GtkWidget    *toggle,
                           CalendarData *calendar )
{
  gint i;
  gint j;
  j=0;
  for (i=0; i<5; i++)
    if (calendar->flag_checkboxes[i] == toggle)
      j = i;

  calendar->settings[j]=!calendar->settings[j];
  calendar_set_flags(calendar);
  
}

void calendar_font_selection_ok( GtkWidget    *button,
                                 CalendarData *calendar )
{
  GtkStyle *style;
  GdkFont  *font;

  calendar->font = gtk_font_selection_dialog_get_font_name(
			GTK_FONT_SELECTION_DIALOG (calendar->font_dialog));
  if (calendar->window)
    {
      font = gtk_font_selection_dialog_get_font(GTK_FONT_SELECTION_DIALOG(calendar->font_dialog));
      if (font) 
	{
	  style = gtk_style_copy (gtk_widget_get_style (calendar->window));
	  gdk_font_unref (style->font);
	  style->font = font;
	  gdk_font_ref (style->font);
	  gtk_widget_set_style (calendar->window, style);
	}
    }
}

void calendar_select_font( GtkWidget    *button,
                           CalendarData *calendar )
{
  GtkWidget *window;

  if (!calendar->font_dialog) {
    window = gtk_font_selection_dialog_new ("Font Selection Dialog");
    g_return_if_fail(GTK_IS_FONT_SELECTION_DIALOG(window));
    calendar->font_dialog = window;
    
    gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);
    
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			&calendar->font_dialog);
    
    gtk_signal_connect (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG (window)->ok_button),
			"clicked", GTK_SIGNAL_FUNC(calendar_font_selection_ok),
			calendar);
    gtk_signal_connect_object (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG (window)->cancel_button),
			       "clicked",
			       GTK_SIGNAL_FUNC (gtk_widget_destroy), 
			       GTK_OBJECT (calendar->font_dialog));
  }
  window=calendar->font_dialog;
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

}

void create_calendar()
{
  GtkWidget *window;
  GtkWidget *vbox, *vbox2, *vbox3;
  GtkWidget *hbox;
  GtkWidget *hbbox;
  GtkWidget *calendar;
  GtkWidget *toggle;
  GtkWidget *button;
  GtkWidget *frame;
  GtkWidget *separator;
  GtkWidget *label;
  GtkWidget *bbox;
  static CalendarData calendar_data;
  gint i;
  
  struct {
    char *label;
  } flags[] =
    {
      { "Show Heading" },
      { "Show Day Names" },
      { "No Month Change" },
      { "Show Week Numbers" },
      { "Week Start Monday" }
    };

  
  calendar_data.window = NULL;
  calendar_data.font = NULL;
  calendar_data.font_dialog = NULL;

  for (i=0; i<5; i++) {
    calendar_data.settings[i]=0;
  }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "GtkCalendar Example");
  gtk_container_border_width (GTK_CONTAINER (window), 5);
  gtk_signal_connect(GTK_OBJECT(window), "destroy",
		     GTK_SIGNAL_FUNC(gtk_main_quit),
		     NULL);
  gtk_signal_connect(GTK_OBJECT(window), "delete-event",
		     GTK_SIGNAL_FUNC(gtk_false),
		     NULL);

  gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, TRUE);

  vbox = gtk_vbox_new(FALSE, DEF_PAD);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /*
   * The top part of the window, Calendar, flags and fontsel.
   */

  hbox = gtk_hbox_new(FALSE, DEF_PAD);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, TRUE, TRUE, DEF_PAD);
  hbbox = gtk_hbutton_box_new();
  gtk_box_pack_start(GTK_BOX(hbox), hbbox, FALSE, FALSE, DEF_PAD);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_SPREAD);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbbox), 5);

  /* Calendar widget */
  frame = gtk_frame_new("Calendar");
  gtk_box_pack_start(GTK_BOX(hbbox), frame, FALSE, TRUE, DEF_PAD);
  calendar=gtk_calendar_new();
  calendar_data.window = calendar;
  calendar_set_flags(&calendar_data);
  gtk_calendar_mark_day ( GTK_CALENDAR(calendar), 19);	
  gtk_container_add( GTK_CONTAINER( frame), calendar);
  gtk_signal_connect (GTK_OBJECT (calendar), "month_changed", 
                      GTK_SIGNAL_FUNC (calendar_month_changed),
		      &calendar_data);
  gtk_signal_connect (GTK_OBJECT (calendar), "day_selected", 
                      GTK_SIGNAL_FUNC (calendar_day_selected),
		      &calendar_data);
  gtk_signal_connect (GTK_OBJECT (calendar), "day_selected_double_click", 
                      GTK_SIGNAL_FUNC (calendar_day_selected_double_click),
		      &calendar_data);
  gtk_signal_connect (GTK_OBJECT (calendar), "prev_month", 
                      GTK_SIGNAL_FUNC (calendar_prev_month),
		      &calendar_data);
  gtk_signal_connect (GTK_OBJECT (calendar), "next_month", 
                      GTK_SIGNAL_FUNC (calendar_next_month),
		      &calendar_data);
  gtk_signal_connect (GTK_OBJECT (calendar), "prev_year", 
                      GTK_SIGNAL_FUNC (calendar_prev_year),
		      &calendar_data);
  gtk_signal_connect (GTK_OBJECT (calendar), "next_year", 
                      GTK_SIGNAL_FUNC (calendar_next_year),
		      &calendar_data);


  separator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, TRUE, 0);

  vbox2 = gtk_vbox_new(FALSE, DEF_PAD);
  gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, DEF_PAD);
  
  /* Build the Right frame with the flags in */ 

  frame = gtk_frame_new("Flags");
  gtk_box_pack_start(GTK_BOX(vbox2), frame, TRUE, TRUE, DEF_PAD);
  vbox3 = gtk_vbox_new(TRUE, DEF_PAD_SMALL);
  gtk_container_add(GTK_CONTAINER(frame), vbox3);

  for (i = 0; i < 5; i++)
    {
      toggle = gtk_check_button_new_with_label(flags[i].label);
      gtk_signal_connect (GTK_OBJECT (toggle),
			    "toggled",
			    GTK_SIGNAL_FUNC(calendar_toggle_flag),
			    &calendar_data);
      gtk_box_pack_start (GTK_BOX (vbox3), toggle, TRUE, TRUE, 0);
      calendar_data.flag_checkboxes[i]=toggle;
    }
  /* Build the right font-button */ 
  button = gtk_button_new_with_label("Font...");
  gtk_signal_connect (GTK_OBJECT (button),
		      "clicked",
		      GTK_SIGNAL_FUNC(calendar_select_font),
		      &calendar_data);
  gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);

  /*
   *  Build the Signal-event part.
   */

  frame = gtk_frame_new("Signal events");
  gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, DEF_PAD);

  vbox2 = gtk_vbox_new(TRUE, DEF_PAD_SMALL);
  gtk_container_add(GTK_CONTAINER(frame), vbox2);
  
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, TRUE, 0);
  label = gtk_label_new ("Signal:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  calendar_data.last_sig = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), calendar_data.last_sig, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, TRUE, 0);
  label = gtk_label_new ("Previous signal:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  calendar_data.prev_sig = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), calendar_data.prev_sig, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, TRUE, 0);
  label = gtk_label_new ("Second previous signal:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  calendar_data.prev2_sig = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), calendar_data.prev2_sig, FALSE, TRUE, 0);

  bbox = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);

  button = gtk_button_new_with_label ("Close");
  gtk_signal_connect (GTK_OBJECT (button), "clicked", 
		      GTK_SIGNAL_FUNC (gtk_main_quit), 
		      NULL);
  gtk_container_add (GTK_CONTAINER (bbox), button);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (button);

  gtk_widget_show_all(window);
}


int main(int   argc,
         char *argv[] )
{
  gtk_set_locale ();
  gtk_init (&argc, &argv);

  create_calendar();

  gtk_main();

  return(0);
}
/* example-end */
