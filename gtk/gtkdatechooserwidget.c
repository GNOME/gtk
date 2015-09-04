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
#include "gtkmultichoiceprivate.h"

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
  PROP_DATE = 1,
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

  GtkWidget *month_choice;
  GtkWidget *grid;

  GtkWidget *day_grid;
  GtkWidget *corner;
  GtkWidget *cols[7];
  GtkWidget *rows[6];
  GtkWidget *days[6][7];

  GDateTime *date;

  gint this_year;
  gint week_start;

  gboolean show_heading;
  gboolean show_day_names;
  gboolean show_week_numbers;
  gboolean no_month_change;

  GtkDateChooserDayOptionsCallback day_options_cb;
  gpointer                         day_options_data;
  GDestroyNotify                   day_options_destroy;
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

static void
calendar_compute_days (GtkDateChooserWidget *calendar)
{
  gint year, month, day;
  gint other_year, other_month;
  gint ndays_in_month;
  gint ndays_in_prev_month;
  gint first_day;
  gint row;
  gint col;
  GtkDateChooserDay *d;
  GDateTime *date;

  g_date_time_get_ymd (calendar->date, &year, &month, NULL);

  ndays_in_month = month_length[leap (year)][month];

  date = g_date_time_new_local (year, month, 1, 1, 1, 1);
  first_day = g_date_time_get_day_of_week (date);
  g_date_time_unref (date);

  first_day = (first_day + 7 - calendar->week_start) % 7;
  if (first_day == 0)
    first_day = 7;

  /* Compute days of previous month */
  if (month > 1)
    ndays_in_prev_month = month_length[leap (year)][month - 1];
  else
    ndays_in_prev_month = month_length[leap (year - 1)][12];
  day = ndays_in_prev_month - first_day + 1;

  other_year = year;
  other_month = month - 1;
  if (other_month == 0)
    {
      other_month = 12;
      other_year -= 1;
    }

  for (col = 0; col < first_day; col++)
    {
      d = GTK_DATE_CHOOSER_DAY (calendar->days[0][col]);
      date = g_date_time_new_local (other_year, other_month, day, 1, 1, 1);
      gtk_date_chooser_day_set_date (d, date);
      gtk_date_chooser_day_set_other_month (d, TRUE);
      g_date_time_unref (date);
      day++;
    }

  /* Compute days of current month */
  row = first_day / 7;
  col = first_day % 7;

  for (day = 1; day <= ndays_in_month; day++)
    {
      d = GTK_DATE_CHOOSER_DAY (calendar->days[row][col]);
      date = g_date_time_new_local (year, month, day, 1, 1, 1);
      gtk_date_chooser_day_set_date (d, date);
      gtk_date_chooser_day_set_other_month (d, FALSE);
      g_date_time_unref (date);

      col++;
      if (col == 7)
        {
          row++;
          col = 0;
        }
    }

  /* Compute days of next month */

  other_year = year;
  other_month = month + 1;
  if (other_month == 13)
    {
      other_month = 1;
      other_year += 1;
    }

  day = 1;
  for (; row < 6; row++)
    {
      for (; col < 7; col++)
        {
          d = GTK_DATE_CHOOSER_DAY (calendar->days[row][col]);
          date = g_date_time_new_local (other_year, other_month, day, 1, 1, 1);
          gtk_date_chooser_day_set_date (d, date);
          gtk_date_chooser_day_set_other_month (d, TRUE);
          g_date_time_unref (date);
          day++;
        }
      col = 0;
    }

  /* update week numbers */
  for (row = 0; row < 6; row++)
    {
      gchar *text;

      d = GTK_DATE_CHOOSER_DAY (calendar->days[row][6]);
      date = gtk_date_chooser_day_get_date (d);
      text = g_strdup_printf ("%d", g_date_time_get_week_of_year (date));
      gtk_label_set_label (GTK_LABEL (calendar->rows[row]), text);
      g_free (text);
    }

  gtk_date_chooser_widget_invalidate_day_options (calendar);
}

/* 0 == sunday */
static gchar *
calendar_get_weekday_name (gint i)
{
  GDateTime *date;
  gchar *text;

  date = g_date_time_new_local (2015, 1, 4 + i, 1, 1, 1);
  text = g_date_time_format (date, "%a");
  g_date_time_unref (date);

  return text;
}

/* 0 = january */
static gchar *
calendar_get_month_name (gint i)
{
  GDateTime *date;
  gchar *text;

  date = g_date_time_new_local (2015, i + 1, 1, 1, 1, 1);
  text = g_date_time_format (date, "%B");
  g_date_time_unref (date);

  return text;
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

static gchar *
format_month (GtkMultiChoice *choice,
              gint            value,
              gpointer        data)
{
  GtkDateChooserWidget *calendar = data;
  gint month;
  gint year;
  gchar *month_name;

  month = value % 12;
  year = value / 12;

  month_name = calendar_get_month_name (month);

  if (year == calendar->this_year)
    return month_name;
  else
    {
      gchar *text;

      text = g_strdup_printf ("%s %d", month_name, year);
      g_free (month_name);

      return text;
    }
}

static void
calendar_init_month_display (GtkDateChooserWidget *calendar)
{
  gint i;
  gchar *month;
  gchar *months[13];

  for (i = 0; i < 12; i++)
    {
      month = calendar_get_month_name (i);
      months[i] = g_strdup_printf ("%s 9999", month);
      g_free (month);
    }
  months[12] = NULL;

  gtk_multi_choice_set_choices (GTK_MULTI_CHOICE (calendar->month_choice),
                                (const gchar **)months);

  for (i = 0; i < 12; i++)
    g_free (months[i]);

  gtk_multi_choice_set_format_callback (GTK_MULTI_CHOICE (calendar->month_choice),
                                        format_month, calendar, NULL);
}

static void
calendar_update_selected_day_display (GtkDateChooserWidget *calendar)
{
  gint row, col;
  GtkDateChooserDay *d;
  GDateTime *date;

  for (row = 0; row < 6; row++)
    for (col = 0; col < 7; col++)
      {
        d = GTK_DATE_CHOOSER_DAY (calendar->days[row][col]);
        date = gtk_date_chooser_day_get_date (d);
        gtk_date_chooser_day_set_selected (d, g_date_time_equal (date, calendar->date));
      }
}

static void
calendar_update_selected_day (GtkDateChooserWidget *calendar)
{
  gint month_len;
  gint year, month, day;
  GDateTime *date;

  g_date_time_get_ymd (calendar->date, &year, &month, &day);

  month_len = month_length[leap (year)][month];

  if (month_len < day)
    {
      date = g_date_time_new_local (year, month, month_len, 1, 1, 1);
      gtk_date_chooser_widget_set_date (calendar, date);
      g_date_time_unref (date);
    }
  else
    calendar_update_selected_day_display (calendar);
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
    g_warning ("Unknown value of _NL_TIME_WEEK_1STDAY.");

  return (week_1stday + first_weekday - 1) % 7;
}

static void
day_selected_cb (GtkDateChooserDay    *d,
                 GtkDateChooserWidget *calendar)
{
  gtk_date_chooser_widget_set_date (calendar, gtk_date_chooser_day_get_date (d));
}

static void
gtk_date_chooser_widget_init (GtkDateChooserWidget *calendar)
{
  gint row, col;
  GtkWidget *label;
  gint year, month;

  calendar->show_heading = TRUE;
  calendar->show_day_names = TRUE;
  calendar->show_week_numbers = TRUE;
  calendar->no_month_change = FALSE;

  calendar->date = g_date_time_new_now_local ();
  g_date_time_get_ymd (calendar->date, &calendar->this_year, NULL, NULL);

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
  g_date_time_get_ymd (calendar->date, &year, &month, NULL);
  gtk_multi_choice_set_value (GTK_MULTI_CHOICE (calendar->month_choice), 12 * year + month - 1);
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
    case PROP_DATE:
      gtk_date_chooser_widget_set_date (calendar, g_value_get_boxed (value));
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
    case PROP_DATE:
      g_value_set_boxed (value, calendar->date);
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

static void
calendar_month_changed (GtkDateChooserWidget *calendar)
{
  gint value;
  gint year, month, day;
  GDateTime *date;

  value = gtk_multi_choice_get_value (GTK_MULTI_CHOICE (calendar->month_choice));
  year = value / 12;
  month = value % 12 + 1;
  g_date_time_get_ymd (calendar->date, NULL, NULL, &day);

  date = g_date_time_new_local (year, month, day, 1, 1, 1);
  gtk_date_chooser_widget_set_date (calendar, date);
  g_date_time_unref (date);
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
  GDate *gdate;
  gint year, month, day;
  GDateTime *date;

  gdate = g_date_new ();
  text = (gchar *)gtk_selection_data_get_text (selection_data);
  if (text)
    {
      g_date_set_parse (gdate, text);
      g_free (text);
    }
  if (!g_date_valid (gdate))
    {
      g_date_free (gdate);
      gtk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  year = g_date_get_year (gdate);
  month = g_date_get_month (gdate);
  day = g_date_get_day (gdate);

  g_date_free (gdate);

  gtk_drag_finish (context, TRUE, FALSE, time);

  if (!calendar->show_heading || calendar->no_month_change)
    g_date_time_get_ymd (calendar->date, &year, &month, NULL);

  date = g_date_time_new_local (year, month, day, 1, 1, 1);
  gtk_date_chooser_widget_set_date (calendar, date);
  g_date_time_unref (date);
}

static void
gtk_date_chooser_widget_class_init (GtkDateChooserWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_ensure (GTK_TYPE_MULTI_CHOICE);

  object_class->set_property = calendar_set_property;
  object_class->get_property = calendar_get_property;

  widget_class->drag_data_received = calendar_drag_data_received;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkdatechooserwidget.ui");

  calendar_properties[PROP_DATE] =
      g_param_spec_boxed ("date", P_("Date"), P_("The selected date"),
                          G_TYPE_DATE_TIME,
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

  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, month_choice);
  gtk_widget_class_bind_template_child (widget_class, GtkDateChooserWidget, grid);

  gtk_widget_class_bind_template_callback (widget_class, calendar_month_changed);
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
                                  GDateTime            *date)
{
  gint y1, m1, d1, y2, m2, d2;

  g_object_freeze_notify (G_OBJECT (calendar));

  g_date_time_get_ymd (calendar->date, &y1, &m1, &d1);
  g_date_time_get_ymd (date, &y2, &m2, &d2);

  g_date_time_unref (calendar->date);
  calendar->date = g_date_time_ref (date);

  if (y1 != y2 || m1 != m2)
    {
      gtk_multi_choice_set_value (GTK_MULTI_CHOICE (calendar->month_choice), 12 * y2 + m2 - 1);
      calendar_compute_days (calendar);
    }

  if (y1 != y2 || m1 != m2 || d1 != d2)
    {
      calendar_update_selected_day (calendar);
      g_signal_emit (calendar, calendar_signals[DAY_SELECTED], 0);
      g_object_notify_by_pspec (G_OBJECT (calendar), calendar_properties[PROP_DATE]);
    }

  g_object_thaw_notify (G_OBJECT (calendar));
}

GDateTime *
gtk_date_chooser_widget_get_date (GtkDateChooserWidget *calendar)
{
  return calendar->date;
}

void
gtk_date_chooser_widget_set_day_options_callback (GtkDateChooserWidget      *calendar,
                                                  GtkDateChooserDayOptionsCallback  callback,
                                                  gpointer                   data,
                                                  GDestroyNotify             destroy)
{
  if (calendar->day_options_destroy)
    calendar->day_options_destroy (calendar->day_options_data);

  calendar->day_options_cb = callback;
  calendar->day_options_data = data;
  calendar->day_options_destroy = destroy;

  gtk_date_chooser_widget_invalidate_day_options (calendar);
}

void
gtk_date_chooser_widget_invalidate_day_options (GtkDateChooserWidget *calendar)
{
  gint row, col;
  GDateTime *date;
  GtkDateChooserDay *d;
  GtkDateChooserDayOptions options;

  for (row = 0; row < 6; row++)
    for (col = 0; col < 7; col++)
      {
        d = GTK_DATE_CHOOSER_DAY (calendar->days[row][col]);
        date = gtk_date_chooser_day_get_date (d);
        if (calendar->day_options_cb)
          options = calendar->day_options_cb (calendar, date, calendar->day_options_data);
        else
          options = GTK_DATE_CHOOSER_DAY_NONE;
        gtk_date_chooser_day_set_options (d, options);
      }
}
