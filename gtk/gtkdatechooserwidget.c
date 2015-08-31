/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
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

#include "gtkdatechooserwidget.h"
#include "gtkdatechooserdayprivate.h"
#include "gtkstack.h"
#include "gtklabel.h"
#include "gtkgrid.h"
#include "gtkselection.h"
#include "gtkdnd.h"

#include "gtkintl.h"
#include "gtkprivate.h"

#include <stdlib.h>
#include <langinfo.h>


enum {
  MONTH_CHANGED,
  DAY_SELECTED,
  LAST_SIGNAL
};

static guint calendar_signals[LAST_SIGNAL] = { 0, };

enum {
  PROP_YEAR = 1,
  PROP_MONTH,
  PROP_DAY,
  PROP_SHOW_HEADING,
  PROP_SHOW_DAY_NAMES,
  PROP_SHOW_WEEK_NUMBERS,
  PROP_NO_MONTH_CHANGE,
  NUM_PROPERTIES
};

static GParamSpec *calendar_properties[NUM_PROPERTIES] = { NULL, };

struct _GtkDateChooserWidget
{
  GtkBin parent;

  GtkWidget *month_down_button;
  GtkWidget *month_stack;
  GtkWidget *month_up_button;
  GtkWidget *year_down_button;
  GtkWidget *year_stack;
  GtkWidget *odd_year_label;
  GtkWidget *even_year_label;
  GtkWidget *year_up_button;
  GtkWidget *grid;

  guint      click_id;
  GtkWidget *active_button;

  guint      year_id;
  GtkWidget *active_year_button;

  GtkWidget *day_grid;
  GtkWidget *corner;
  GtkWidget *cols[7];
  GtkWidget *rows[6];
  GtkWidget *days[6][7];

  guint month;
  guint year;
  guint day;

  gint week_start;

  gboolean show_heading;
  gboolean show_day_names;
  gboolean show_week_numbers;
  gboolean no_month_change;
};

struct _GtkDateChooserWidgetClass
{
  GtkBinClass parent_class;
};


G_DEFINE_TYPE (GtkDateChooserWidget, gtk_date_chooser_widget, GTK_TYPE_BIN)

static const guint month_length[2][13] =
{
  { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static gboolean
leap (guint year)
{
  return ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0));
}

static guint
day_of_week (guint year, guint mm, guint dd)
{
  GDateTime *dt;
  guint days;

  dt = g_date_time_new_local (year, mm, dd, 1, 1, 1);
  if (dt == NULL)
    return 0;

  days = g_date_time_get_day_of_week (dt);
  g_date_time_unref (dt);

  return days;
}

static guint
week_of_year (guint year, guint mm, guint dd)
{
  GDateTime *dt;
  guint week;

  dt = g_date_time_new_local (year, mm, dd, 1, 1, 1);
  if (dt == NULL)
    return 1;

  week = g_date_time_get_week_of_year (dt);
  g_date_time_unref (dt);

  return week;
}

static void
calendar_compute_days (GtkDateChooserWidget *calendar)
{
  guint year, month, day;
  gint ndays_in_month;
  gint ndays_in_prev_month;
  gint first_day;
  gint row;
  gint col;
  GtkDateChooserDay *d;

  year = calendar->year;
  month = calendar->month + 1;

  ndays_in_month = month_length[leap (year)][month];

  first_day = day_of_week (year, month, 1);
  first_day = (first_day + 7 - calendar->week_start) % 7;
  if (first_day == 0)
    first_day = 7;

  /* Compute days of previous month */
  if (month > 1)
    ndays_in_prev_month = month_length[leap (year)][month - 1];
  else
    ndays_in_prev_month = month_length[leap (year - 1)][12];
  day = ndays_in_prev_month - first_day + 1;

  month = (calendar->month + 11) % 12;
  year = calendar->year - (month == 11);

  for (col = 0; col < first_day; col++)
    {
      d = GTK_DATE_CHOOSER_DAY (calendar->days[0][col]);
      gtk_date_chooser_day_set_date (d, year, month, day);
      gtk_date_chooser_day_set_other_month (d, TRUE);
      day++;
    }

  /* Compute days of current month */
  row = first_day / 7;
  col = first_day % 7;

  month = calendar->month;
  year = calendar->year;

  for (day = 1; day <= ndays_in_month; day++)
    {
      d = GTK_DATE_CHOOSER_DAY (calendar->days[row][col]);
      gtk_date_chooser_day_set_date (d, year, month, day);
      gtk_date_chooser_day_set_other_month (d, FALSE);

      col++;
      if (col == 7)
        {
          row++;
          col = 0;
        }
    }

  /* Compute days of next month */

  month = (calendar->month + 1) % 12;
  year = calendar->year + (month == 0);

  day = 1;
  for (; row < 6; row++)
    {
      for (; col < 7; col++)
        {
          d = GTK_DATE_CHOOSER_DAY (calendar->days[row][col]);
          gtk_date_chooser_day_set_date (d, year, month, day);
          gtk_date_chooser_day_set_other_month (d, TRUE);
          day++;
        }
      col = 0;
    }

  /* update week numbers */
  for (row = 0; row < 6; row++)
    {
      gchar *text;
      guint week;

      d = GTK_DATE_CHOOSER_DAY (calendar->days[row][6]);
      gtk_date_chooser_day_get_date (d, &year, &month, &day);

      week = week_of_year (year, month + 1, day);
      text = g_strdup_printf ("%d", week);
      gtk_label_set_label (GTK_LABEL (calendar->rows[row]), text);
      g_free (text);
    }
}

static const gchar *months[] = {
  "jan", "feb", "mar", "apr",
  "may", "jun", "jul", "aug",
  "sep", "oct", "nov", "dec"
};

static void
calendar_update_year_display (GtkDateChooserWidget *calendar)
{
  GtkWidget *label;
  gchar *text;
  GtkWidget *vis;

  vis = gtk_stack_get_visible_child (GTK_STACK (calendar->year_stack));
  if (vis == calendar->odd_year_label)
    label = calendar->even_year_label;
  else
    label = calendar->odd_year_label;

  text = g_strdup_printf ("%d", calendar->year);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);

  gtk_stack_set_visible_child (GTK_STACK (calendar->year_stack), label);
}

static void
calendar_update_month_display (GtkDateChooserWidget   *calendar,
                               GtkStackTransitionType  transition)
{
  gtk_stack_set_visible_child_full (GTK_STACK (calendar->month_stack),
                                    months[calendar->month],
                                    transition);
}

static gchar *
calendar_get_weekday_name (gint i)
{
  time_t time;
  gchar buffer[128];

  time = (i + 3) * 86400;
  strftime (buffer, sizeof (buffer), "%a", gmtime (&time));
  return g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
}

static gchar *
calendar_get_month_name (gint i)
{
  time_t time;
  gchar buffer[128];

  time = i * 2764800;
  strftime (buffer, sizeof (buffer), "%B", gmtime (&time));
  return g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
}

static void
calendar_init_weekday_display (GtkDateChooserWidget *calendar)
{
  gint i;
  gchar *text;

  for (i = 0; i < 7; i++)
    {
      text = calendar_get_weekday_name ((i + calendar->week_start) % 7);
      gtk_label_set_label (GTK_LABEL (calendar->cols[i]), text);
      g_free (text);
    }
}

static void
calendar_init_month_display (GtkDateChooserWidget *calendar)
{
  gint i;
  GtkWidget *label;
  gchar *text;

  for (i = 0; i < 12; i++)
    {
      text = calendar_get_month_name (i);
      label = gtk_label_new (text);
      gtk_widget_show (label);
      gtk_stack_add_named (GTK_STACK (calendar->month_stack), label, months[i]);
      g_free (text);
    }
}

static void
calendar_update_selected_day_display (GtkDateChooserWidget *calendar)
{
  gint row, col;
  GtkDateChooserDay *d;
  guint year, month, day;

  for (row = 0; row < 6; row++)
    for (col = 0; col < 7; col++)
      {
        d = GTK_DATE_CHOOSER_DAY (calendar->days[row][col]);
        gtk_date_chooser_day_get_date (d, &year, &month, &day);
        gtk_date_chooser_day_set_selected (d, day == calendar->day &&
                                              month == calendar->month &&
                                              year == calendar->year);
      }
}

static void
calendar_update_selected_day (GtkDateChooserWidget *calendar)
{
  gint month_len;

  month_len = month_length[leap (calendar->year)][calendar->month + 1];

  if (month_len < calendar->day)
    gtk_date_chooser_widget_select_day (calendar, month_len);
  else
    calendar_update_selected_day_display (calendar);
}

static void
calendar_set_year_prev (GtkDateChooserWidget *calendar)
{
  if (calendar->no_month_change)
    return;

  calendar->year -= 1;
  calendar_update_year_display (calendar);

  calendar_compute_days (calendar);

  g_signal_emit (calendar, calendar_signals[MONTH_CHANGED], 0);

  calendar_update_selected_day (calendar);
}

static void
calendar_set_year_next (GtkDateChooserWidget *calendar)
{
  if (calendar->no_month_change)
    return;

  calendar->year += 1;
  calendar_update_year_display (calendar);

  calendar_compute_days (calendar);

  g_signal_emit (calendar, calendar_signals[MONTH_CHANGED], 0);

  calendar_update_selected_day (calendar);
}

static void
calendar_set_month_prev (GtkDateChooserWidget *calendar)
{
  if (calendar->no_month_change)
    return;

  calendar->month = (calendar->month + 11) % 12;
  calendar_update_month_display (calendar, GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);

  if (calendar->month == 11)
    {
      calendar->year -= 1;
      calendar_update_year_display (calendar);
    }

  calendar_compute_days (calendar);

  g_signal_emit (calendar, calendar_signals[MONTH_CHANGED], 0);

  calendar_update_selected_day (calendar);
}

static void
calendar_set_month_next (GtkDateChooserWidget *calendar)
{
  if (calendar->no_month_change)
    return;

  calendar->month = (calendar->month + 1) % 12;
  calendar_update_month_display (calendar, GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);

  if (calendar->month == 0)
    {
      calendar->year += 1;
      calendar_update_year_display (calendar);
    }

  calendar_compute_days (calendar);

  g_signal_emit (calendar, calendar_signals[MONTH_CHANGED], 0);

  calendar_update_selected_day (calendar);
}

static gint
calendar_get_week_start (void)
{
  union { unsigned int word; char *string; } langinfo;
  gint week_1stday = 0;
  gint first_weekday = 1;
  guint week_origin;

  langinfo.string = nl_langinfo (_NL_TIME_FIRST_WEEKDAY);
  first_weekday = langinfo.string[0];
  langinfo.string = nl_langinfo (_NL_TIME_WEEK_1STDAY);
  week_origin = langinfo.word;
  if (week_origin == 19971130) /* Sunday */
    week_1stday = 0;
  else if (week_origin == 19971201) /* Monday */
    week_1stday = 1;
  else
    g_warning ("Unknown value of _NL_TIME_WEEK_1STDAY.\n");

  return (week_1stday + first_weekday - 1) % 7;
}

static void
day_selected_cb (GtkDateChooserDay    *d,
                 GtkDateChooserWidget *calendar)
{
  guint year, month, day;

  gtk_date_chooser_day_get_date (d, &year, &month, &day);

  if ((month + 1) % 12 == calendar->month)
    calendar_set_month_prev (calendar);
  else if ((calendar->month + 1) % 12 == month)
    calendar_set_month_next (calendar);

  gtk_date_chooser_widget_select_day (calendar, day);
}

static void
gtk_date_chooser_widget_init (GtkDateChooserWidget *calendar)
{
  gint row, col;
  GDateTime *now;
  GtkWidget *label;

  calendar->show_heading = TRUE;
  calendar->show_day_names = TRUE;
  calendar->show_week_numbers = TRUE;
  calendar->no_month_change = FALSE;

  now = g_date_time_new_now_local ();
  calendar->year = g_date_time_get_year (now);
  calendar->month = g_date_time_get_month (now) - 1;
  calendar->day = g_date_time_get_day_of_month (now);
  g_date_time_unref (now);

  calendar->week_start = calendar_get_week_start ();

  gtk_widget_init_template (GTK_WIDGET (calendar));

  for (col = 0; col < 7; col++)
    {
      calendar->cols[col] = gtk_label_new ("");
      g_object_bind_property (calendar, "show-day-names",
                              calendar->cols[col], "visible",
                              G_BINDING_SYNC_CREATE);
      gtk_style_context_add_class (gtk_widget_get_style_context (calendar->cols[col]), "weekday");
      gtk_grid_attach (GTK_GRID (calendar->grid), calendar->cols[col], col, -1, 1, 1);
    }

  for (row = 0; row < 6; row++)
    {
      calendar->rows[row] = gtk_label_new ("");
      g_object_bind_property (calendar, "show-week-numbers",
                              calendar->rows[row], "visible",
                              G_BINDING_SYNC_CREATE);
      gtk_label_set_xalign (GTK_LABEL (calendar->rows[row]), 1.0);
      gtk_widget_show (calendar->rows[row]);
      gtk_style_context_add_class (gtk_widget_get_style_context (calendar->rows[row]), "weeknum");
      gtk_grid_attach (GTK_GRID (calendar->grid), calendar->rows[row], -1, row, 1, 1);
    }

  /* We are using a stack here to keep the week number column from shrinking
   * when all the weeks are single-digit
   */
  calendar->corner = gtk_stack_new ();
  gtk_grid_attach (GTK_GRID (calendar->grid), calendar->corner, -1, -1, 1, 1);
  label = gtk_label_new ("");
  gtk_widget_show (label);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "weekday");
  gtk_container_add (GTK_CONTAINER (calendar->corner), label);

  label = gtk_label_new ("99");
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "weeknum");
  gtk_container_add (GTK_CONTAINER (calendar->corner), label);

  calendar->day_grid = gtk_grid_new ();
  gtk_widget_show (calendar->day_grid);
  gtk_widget_set_halign (calendar->day_grid, GTK_ALIGN_FILL);
  gtk_widget_set_valign (calendar->day_grid, GTK_ALIGN_FILL);
  gtk_grid_attach (GTK_GRID (calendar->grid), calendar->day_grid, 0, 0, 7, 6);

  for (row = 0; row < 6; row++)
    for (col = 0; col < 7; col++)
      {
        calendar->days[row][col] = gtk_date_chooser_day_new ();
        g_signal_connect (calendar->days[row][col], "selected", G_CALLBACK (day_selected_cb), calendar);
        gtk_widget_show (calendar->days[row][col]);
        gtk_grid_attach (GTK_GRID (calendar->day_grid), calendar->days[row][col], col, row, 1, 1);
      }

  calendar_init_month_display (calendar);
  calendar_init_weekday_display (calendar);

  calendar_compute_days (calendar);
  calendar_update_month_display (calendar, GTK_STACK_TRANSITION_TYPE_NONE);
  calendar_update_year_display (calendar);
  calendar_update_selected_day_display (calendar);

  gtk_drag_dest_set (GTK_WIDGET (calendar), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_text_targets (GTK_WIDGET (calendar));
}

static void
calendar_set_property (GObject      *obj,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkDateChooserWidget *calendar = GTK_DATE_CHOOSER_WIDGET (obj);

  switch (property_id)
    {
    case PROP_YEAR:
      gtk_date_chooser_widget_set_date (calendar, g_value_get_int (value), calendar->month, calendar->day);
      break;
    case PROP_MONTH:
      gtk_date_chooser_widget_set_date (calendar, calendar->year, g_value_get_int (value), calendar->day);
      break;
    case PROP_DAY:
      gtk_date_chooser_widget_set_date (calendar, calendar->year, calendar->month, g_value_get_int (value));
      break;
    case PROP_SHOW_HEADING:
      gtk_date_chooser_widget_set_show_heading (calendar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_DAY_NAMES:
      gtk_date_chooser_widget_set_show_day_names (calendar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_WEEK_NUMBERS:
      gtk_date_chooser_widget_set_show_week_numbers (calendar, g_value_get_boolean (value));
      break;
    case PROP_NO_MONTH_CHANGE:
      gtk_date_chooser_widget_set_no_month_change (calendar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
calendar_get_property (GObject    *obj,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkDateChooserWidget *calendar = GTK_DATE_CHOOSER_WIDGET (obj);

  switch (property_id)
    {
    case PROP_YEAR:
      g_value_set_int (value, calendar->year);
      break;
    case PROP_MONTH:
      g_value_set_int (value, calendar->month);
      break;
    case PROP_DAY:
      g_value_set_int (value, calendar->day);
      break;
    case PROP_SHOW_HEADING:
      g_value_set_boolean (value, calendar->show_heading);
      break;
    case PROP_SHOW_DAY_NAMES:
      g_value_set_boolean (value, calendar->show_day_names);
      break;
    case PROP_SHOW_WEEK_NUMBERS:
      g_value_set_boolean (value, calendar->show_week_numbers);
      break;
    case PROP_NO_MONTH_CHANGE:
      g_value_set_boolean (value, calendar->no_month_change);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static gboolean
calendar_button_activate (GtkDateChooserWidget *calendar,
                          GtkWidget            *button)
{
  if (button == calendar->month_down_button)
    calendar_set_month_prev (calendar);
  else if (button == calendar->month_up_button)
    calendar_set_month_next (calendar);
  else if (button == calendar->year_down_button)
    calendar_set_year_prev (calendar);
  else if (button == calendar->year_up_button)
    calendar_set_year_next (calendar);
  else
    g_assert_not_reached ();

  return TRUE;
}

static gboolean
calendar_button_timeout (gpointer user_data)
{
  GtkDateChooserWidget *calendar = GTK_DATE_CHOOSER_WIDGET (user_data);
  gboolean res;

  if (calendar->click_id == 0)
    return G_SOURCE_REMOVE;

  res = calendar_button_activate (calendar, calendar->active_button);
  if (!res)
    {
      g_source_remove (calendar->click_id);
      calendar->click_id = 0;
    }

  return res;
}

static gboolean
calendar_button_press_cb (GtkWidget            *widget,
                          GdkEventButton       *button,
                          GtkDateChooserWidget *calendar)
{
  gint double_click_time;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-double-click-time", &double_click_time,
                NULL);

  if (calendar->click_id != 0)
    g_source_remove (calendar->click_id);

  calendar->active_button = widget;

  calendar->click_id = gdk_threads_add_timeout (double_click_time,
                                            calendar_button_timeout,
                                            calendar);
  g_source_set_name_by_id (calendar->click_id, "[gtk+] calendar_button_timeout");
  calendar_button_timeout (calendar);

  return TRUE;
}

static gboolean
calendar_button_release_cb (GtkWidget      *widget,
                            GdkEventButton *event,
                            GtkDateChooserWidget *calendar)
{
  if (calendar->click_id != 0)
    {
      g_source_remove (calendar->click_id);
      calendar->click_id = 0;
    }

  calendar->active_button = NULL;

  return TRUE;
}

static void
calendar_clicked_cb (GtkWidget  *widget,
                     GtkDateChooserWidget *calendar)
{
  if (calendar->click_id != 0)
    return;

  calendar_button_activate (calendar, widget);
}

static void
calendar_dispose (GObject *object)
{
  GtkDateChooserWidget *calendar = GTK_DATE_CHOOSER_WIDGET (object);

  if (calendar->click_id != 0)
    {
      g_source_remove (calendar->click_id);
      calendar->click_id = 0;
    }

  G_OBJECT_CLASS (gtk_date_chooser_widget_parent_class)->dispose (object);
}

static void
calendar_drag_data_received (GtkWidget        *widget,
                             GdkDragContext   *context,
                             gint              x,
                             gint              y,
                             GtkSelectionData *selection_data,
                             guint             info,
                             guint             time)
{
  GtkDateChooserWidget *calendar = GTK_DATE_CHOOSER_WIDGET (widget);
  gchar *text;
  GDate *date;
  guint year, month, day;

  date = g_date_new ();
  text = (gchar *)gtk_selection_data_get_text (selection_data);
  if (text)
    {
      g_date_set_parse (date, text);
      g_free (text);
    }
  if (!g_date_valid (date))
    {
      g_date_free (date);
      gtk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  year = g_date_get_year (date);
  month = g_date_get_month (date) - 1;
  day = g_date_get_day (date);

  g_date_free (date);

  gtk_drag_finish (context, TRUE, FALSE, time);

  if (!calendar->show_heading || calendar->no_month_change)
    {
      year = calendar->year;
      month = calendar->month;
    }
  gtk_date_chooser_widget_set_date (calendar, year, month, day);
}

static void
gtk_date_chooser_widget_class_init (GtkDateChooserWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = calendar_dispose;
  object_class->set_property = calendar_set_property;
  object_class->get_property = calendar_get_property;

  widget_class->drag_data_received = calendar_drag_data_received;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkdatechooserwidget.ui");

  calendar_properties[PROP_YEAR] =
      g_param_spec_int ("year", P_("Year"), P_("The selected year"),
                        0, G_MAXINT >> 9, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  calendar_properties[PROP_MONTH] =
      g_param_spec_int ("month", P_("Month"), P_("The selected month (as a number between 0 and 11)"),
                        0, 11, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  calendar_properties[PROP_DAY] =
      g_param_spec_int ("day", P_("Day"), P_("The selected day (as a number between 1 and 31, or 0 to unselect the currently selected day)"),
                        0, 31, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  calendar_properties[PROP_SHOW_HEADING] =
      g_param_spec_boolean ("show-heading", P_("Show Heading"), P_("If TRUE, a heading is displayed"),
                            TRUE, G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  calendar_properties[PROP_SHOW_DAY_NAMES] =
      g_param_spec_boolean ("show-day-names", P_("Show Day Names"), P_("If TRUE, day names are displayed"),
                            TRUE, G_PARAM_READWRITE);
  calendar_properties[PROP_SHOW_WEEK_NUMBERS] =
      g_param_spec_boolean ("show-week-numbers", P_("Show Week Numbers"), P_("If TRUE, week numbers are displayed"),
                            TRUE, G_PARAM_READWRITE);
  calendar_properties[PROP_NO_MONTH_CHANGE] =
      g_param_spec_boolean ("no-month-change", P_("No Month Change"), P_("If TRUE, the selected month cannot be changed"),
                            FALSE, G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, NUM_PROPERTIES, calendar_properties);

  calendar_signals[MONTH_CHANGED] = g_signal_new (I_("month-changed"),
                                                  G_OBJECT_CLASS_TYPE (object_class),
                                                  G_SIGNAL_RUN_FIRST,
                                                  0,
                                                  NULL, NULL,
                                                  NULL,
                                                  G_TYPE_NONE, 0);
  calendar_signals[DAY_SELECTED] = g_signal_new (I_("day-selected"),
                                                  G_OBJECT_CLASS_TYPE (object_class),
                                                  G_SIGNAL_RUN_FIRST,
                                                  0,
                                                  NULL, NULL,
                                                  NULL,
                                                  G_TYPE_NONE, 0);

  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, month_down_button);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, month_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, month_up_button);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, year_down_button);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, year_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, odd_year_label);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, even_year_label);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, year_up_button);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, grid);

  gtk_widget_class_bind_template_callback (widget_class, calendar_button_press_cb);
  gtk_widget_class_bind_template_callback (widget_class, calendar_button_release_cb);
  gtk_widget_class_bind_template_callback (widget_class, calendar_clicked_cb);
}

GtkWidget *
gtk_date_chooser_widget_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_DATE_CHOOSER_WIDGET, NULL));
}

void
gtk_date_chooser_widget_set_show_heading (GtkDateChooserWidget *calendar,
                                          gboolean              setting)
{
  if (calendar->show_heading == setting)
    return;

  calendar->show_heading = setting;

  g_object_notify_by_pspec (G_OBJECT (calendar),
                            calendar_properties[PROP_SHOW_HEADING]);
}

gboolean
gtk_date_chooser_widget_get_show_heading (GtkDateChooserWidget *calendar)
{
  return calendar->show_heading;
}

void
gtk_date_chooser_widget_set_show_day_names (GtkDateChooserWidget *calendar,
                                            gboolean              setting)
{
  if (calendar->show_day_names == setting)
    return;

  calendar->show_day_names = setting;

  gtk_widget_set_visible (calendar->corner, calendar->show_day_names && calendar->show_week_numbers);

  g_object_notify_by_pspec (G_OBJECT (calendar),
                            calendar_properties[PROP_SHOW_DAY_NAMES]);
}

gboolean
gtk_date_chooser_widget_get_show_day_names (GtkDateChooserWidget *calendar)
{
  return calendar->show_day_names;
}

void
gtk_date_chooser_widget_set_show_week_numbers (GtkDateChooserWidget *calendar,
                                               gboolean              setting)
{
  if (calendar->show_week_numbers == setting)
    return;

  calendar->show_week_numbers = setting;

  gtk_widget_set_visible (calendar->corner, calendar->show_day_names && calendar->show_week_numbers);

  g_object_notify_by_pspec (G_OBJECT (calendar),
                            calendar_properties[PROP_SHOW_WEEK_NUMBERS]);
}

gboolean
gtk_date_chooser_widget_get_show_week_numbers (GtkDateChooserWidget *calendar)
{
  return calendar->show_week_numbers;
}

void
gtk_date_chooser_widget_set_no_month_change (GtkDateChooserWidget *calendar,
                                             gboolean              setting)
{
  if (calendar->no_month_change == setting)
    return;

  calendar->no_month_change = setting;

  g_object_notify_by_pspec (G_OBJECT (calendar),
                            calendar_properties[PROP_NO_MONTH_CHANGE]);
}

gboolean
gtk_date_chooser_widget_get_no_month_change (GtkDateChooserWidget *calendar)
{
  return calendar->no_month_change;
}

void
gtk_date_chooser_widget_set_date (GtkDateChooserWidget *calendar,
                                  guint                 year,
                                  guint                 month,
                                  guint                 day)
{
  gboolean month_changed = FALSE;
  gboolean day_changed = FALSE;
  g_object_freeze_notify (G_OBJECT (calendar));

  if (calendar->year != year)
    {
      month_changed = TRUE;
      calendar->year = year;
      g_object_notify_by_pspec (G_OBJECT (calendar),
                                calendar_properties[PROP_YEAR]);
      calendar_update_year_display (calendar);
    }

  if (calendar->month != month)
    {
      month_changed = TRUE;
      calendar->month = month;
      g_object_notify_by_pspec (G_OBJECT (calendar),
                                calendar_properties[PROP_MONTH]);
      calendar_update_month_display (calendar, GTK_STACK_TRANSITION_TYPE_NONE);
    }

  if (calendar->day != day)
    {
      day_changed = TRUE;
      calendar->day = day;
      g_object_notify_by_pspec (G_OBJECT (calendar),
                                calendar_properties[PROP_DAY]);
    }

  if (month_changed)
    calendar_compute_days (calendar);

  if (month_changed || day_changed)
    {
      calendar_update_selected_day (calendar);
      g_signal_emit (calendar, calendar_signals[DAY_SELECTED], 0);
    }

  g_object_thaw_notify (G_OBJECT (calendar));
}

void
gtk_date_chooser_widget_select_month (GtkDateChooserWidget *calendar,
                                      guint                 year,
                                      guint                 month)
{
  gtk_date_chooser_widget_set_date (calendar, year, month, calendar->day);
}

void
gtk_date_chooser_widget_select_day (GtkDateChooserWidget *calendar,
                                    guint                 day)
{
  if (calendar->day == day)
    return;

  calendar->day = day;

  calendar_update_selected_day_display (calendar);

  g_signal_emit (calendar, calendar_signals[DAY_SELECTED], 0);
  g_object_notify_by_pspec (G_OBJECT (calendar),
                            calendar_properties[PROP_DAY]);
}

void
gtk_date_chooser_widget_get_date (GtkDateChooserWidget *calendar,
                                  guint                *year,
                                  guint                *month,
                                  guint                *day)
{
  *year = calendar->year;
  *month = calendar->month;
  *day = calendar->day;
}
