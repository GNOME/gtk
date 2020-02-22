/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GTK Calendar Widget
 * Copyright (C) 1998 Cesar Miquel, Shawn T. Amundson and Mattias Groenlund
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * SECTION:gtkcalendar
 * @Short_description: Displays a calendar and allows the user to select a date
 * @Title: GtkCalendar
 *
 * #GtkCalendar is a widget that displays a Gregorian calendar, one month
 * at a time. It can be created with gtk_calendar_new().
 *
 * The date that is currently displayed can be altered with
 * gtk_calendar_select_day().
 *
 * To place a visual marker on a particular day, use gtk_calendar_mark_day()
 * and to remove the marker, gtk_calendar_unmark_day(). Alternative, all
 * marks can be cleared with gtk_calendar_clear_marks().
 *
 * The selected date can be retrieved from a #GtkCalendar using
 * gtk_calendar_get_date().
 *
 * Users should be aware that, although the Gregorian calendar is the
 * legal calendar in most countries, it was adopted progressively
 * between 1582 and 1929. Display before these dates is likely to be
 * historically incorrect.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * calendar.view
 * ├── header
 * │   ├── button
 * │   ├── stack.month
 * │   ├── button
 * │   ├── button
 * │   ├── label.year
 * │   ╰── button
 * ╰── grid
 *     ╰── label[.day-name][.week-number][.day-number][.other-month][.today]
 * ]|
 *
 * GtkCalendar has a main node with name calendar. It contains a subnode called header
 * containing the widgets for switching between years and months.
 *
 * The grid subnode contains all day labels, including week numbers on the left (marked
 * with the .week-number css class) and day names on top (marked with the .day-name
 * css class).
 *
 * Day labels that belong to the previous or next month get the .other-month style class.
 * The label of the current day get the .today style class.
 *
 * Marked day labels get the :selected state assigned.
 */

#include "config.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
#include <langinfo.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <glib.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include "gtkcalendar.h"
#include "gtkdragdest.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtktooltip.h"
#include "gtkprivate.h"
#include "gtkrendericonprivate.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkgestureclick.h"
#include "gtkgesturedrag.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkdragsource.h"
#include "gtknative.h"
#include "gtkicontheme.h"
#include "gtkdragicon.h"
#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkboxlayout.h"
#include "gtkorientable.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkgrid.h"

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

enum {
  MONTH_PREV,
  MONTH_CURRENT,
  MONTH_NEXT
};

enum {
  DAY_SELECTED_SIGNAL,
  PREV_MONTH_SIGNAL,
  NEXT_MONTH_SIGNAL,
  PREV_YEAR_SIGNAL,
  NEXT_YEAR_SIGNAL,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_YEAR,
  PROP_MONTH,
  PROP_DAY,
  PROP_SHOW_HEADING,
  PROP_SHOW_DAY_NAMES,
  PROP_SHOW_WEEK_NUMBERS,
};

static guint gtk_calendar_signals[LAST_SIGNAL] = { 0 };

typedef struct _GtkCalendarClass   GtkCalendarClass;
typedef struct _GtkCalendarPrivate GtkCalendarPrivate;

struct _GtkCalendar
{
  GtkWidget widget;
};

struct _GtkCalendarClass
{
  GtkWidgetClass parent_class;

  void (* day_selected)                 (GtkCalendar *calendar);
  void (* prev_month)                   (GtkCalendar *calendar);
  void (* next_month)                   (GtkCalendar *calendar);
  void (* prev_year)                    (GtkCalendar *calendar);
  void (* next_year)                    (GtkCalendar *calendar);
};

struct _GtkCalendarPrivate
{
  guint show_week_numbers: 1;
  guint show_heading: 1;
  guint show_day_names: 1;

  GtkWidget *header_box;
  GtkWidget *year_label;
  GtkWidget *month_name_stack;
  GtkWidget *arrow_widgets[4];

  GtkWidget *grid;
  GtkWidget *day_name_labels[7];
  GtkWidget *week_number_labels[6];
  GtkWidget *day_number_labels[6][7];

  GDateTime *date;

  gint  day_month[6][7];
  gint  day[6][7];

  gint  num_marked_dates;
  gint  marked_date[31];

  gint  focus_row;
  gint  focus_col;

  guint year_before : 1;
  guint in_drag : 1;

  gint week_start;

  gint drag_start_x;
  gint drag_start_y;
};

static void gtk_calendar_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec);
static void gtk_calendar_get_property (GObject      *object,
                                       guint         prop_id,
                                       GValue       *value,
                                       GParamSpec   *pspec);

static void     gtk_calendar_button_press   (GtkGestureClick *gesture,
                                             int              n_press,
                                             double           x,
                                             double           y,
                                             gpointer         user_data);
static void     gtk_calendar_button_release (GtkGestureClick *gesture,
                                             int              n_press,
                                             double           x,
                                             double           y,
                                             gpointer         user_data);
static void     gtk_calendar_drag_begin     (GtkGestureDrag   *gesture,
                                             double            x,
                                             double            y,
                                             gpointer          data);
static void     gtk_calendar_drag_update    (GtkGestureDrag   *gesture,
                                             double            x,
                                             double            y,
                                             gpointer          data);
static gboolean gtk_calendar_key_controller_key_pressed (GtkEventControllerKey *controller,
                                                         guint                  keyval,
                                                         guint                  keycode,
                                                         GdkModifierType        state,
                                                         GtkWidget             *widget);
static void     gtk_calendar_focus_controller_focus     (GtkEventController    *controller,
                                                         GtkWidget             *widget);
static void     gtk_calendar_state_flags_changed  (GtkWidget     *widget,
                                                   GtkStateFlags  previous_state);
static gboolean gtk_calendar_drag_accept        (GtkDropTarget    *dest,
                                                 GdkDrop          *drop,
                                                 GtkCalendar      *calendar);
static void     gtk_calendar_drag_leave         (GtkDropTarget    *dest,
                                                 GdkDrop          *drop,
                                                 GtkCalendar      *calendar);
static gboolean gtk_calendar_drag_drop          (GtkDropTarget    *dest,
                                                 GdkDrop          *drop,
                                                 int               x,
                                                 int               y,
                                                 GtkCalendar      *calendar);


static void calendar_invalidate_day     (GtkCalendar *widget,
                                         gint       row,
                                         gint       col);
static void calendar_invalidate_day_num (GtkCalendar *widget,
                                         gint       day);

static gboolean gtk_calendar_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                                       gdouble                   dx,
                                                       gdouble                   dy,
                                                       GtkWidget                *widget);

static void     calendar_set_month_prev (GtkCalendar *calendar);
static void     calendar_set_month_next (GtkCalendar *calendar);
static void     calendar_set_year_prev  (GtkCalendar *calendar);
static void     calendar_set_year_next  (GtkCalendar *calendar);


static char    *default_abbreviated_dayname[7];
static char    *default_monthname[12];

G_DEFINE_TYPE_WITH_PRIVATE (GtkCalendar, gtk_calendar, GTK_TYPE_WIDGET)

static void
gtk_calendar_dispose (GObject *object)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (GTK_CALENDAR (object));

  g_clear_pointer (&priv->date, g_date_time_unref);
  g_clear_pointer (&priv->header_box, gtk_widget_unparent);
  g_clear_pointer (&priv->grid, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_calendar_parent_class)->dispose (object);
}

static void
gtk_calendar_class_init (GtkCalendarClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*)  class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->dispose = gtk_calendar_dispose;
  gobject_class->set_property = gtk_calendar_set_property;
  gobject_class->get_property = gtk_calendar_get_property;

  widget_class->state_flags_changed = gtk_calendar_state_flags_changed;

  /**
   * GtkCalendar:year:
   *
   * The selected year.
   * This property gets initially set to the current year.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_YEAR,
                                   g_param_spec_int ("year",
                                                     P_("Year"),
                                                     P_("The selected year"),
                                                     1, 9999, 1,
                                                     G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar:month:
   *
   * The selected month (as a number between 0 and 11).
   * This property gets initially set to the current month.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MONTH,
                                   g_param_spec_int ("month",
                                                     P_("Month"),
                                                     P_("The selected month (as a number between 0 and 11)"),
                                                     0, 11, 0,
                                                     G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar:day:
   *
   * The selected day (as a number between 1 and 31, or 0
   * to unselect the currently selected day).
   * This property gets initially set to the current day.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DAY,
                                   g_param_spec_int ("day",
                                                     P_("Day"),
                                                     P_("The selected day (as a number between 1 and 31, or 0 to unselect the currently selected day)"),
                                                     0, 31, 0,
                                                     G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar:show-heading:
   *
   * Determines whether a heading is displayed.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_HEADING,
                                   g_param_spec_boolean ("show-heading",
                                                         P_("Show Heading"),
                                                         P_("If TRUE, a heading is displayed"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar:show-day-names:
   *
   * Determines whether day names are displayed.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_DAY_NAMES,
                                   g_param_spec_boolean ("show-day-names",
                                                         P_("Show Day Names"),
                                                         P_("If TRUE, day names are displayed"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkCalendar:show-week-numbers:
   *
   * Determines whether week numbers are displayed.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_WEEK_NUMBERS,
                                   g_param_spec_boolean ("show-week-numbers",
                                                         P_("Show Week Numbers"),
                                                         P_("If TRUE, week numbers are displayed"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar::day-selected:
   * @calendar: the object which received the signal.
   *
   * Emitted when the user selects a day.
   */
  gtk_calendar_signals[DAY_SELECTED_SIGNAL] =
    g_signal_new (I_("day-selected"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, day_selected),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCalendar::prev-month:
   * @calendar: the object which received the signal.
   *
   * Emitted when the user switched to the previous month.
   */
  gtk_calendar_signals[PREV_MONTH_SIGNAL] =
    g_signal_new (I_("prev-month"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, prev_month),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCalendar::next-month:
   * @calendar: the object which received the signal.
   *
   * Emitted when the user switched to the next month.
   */
  gtk_calendar_signals[NEXT_MONTH_SIGNAL] =
    g_signal_new (I_("next-month"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, next_month),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCalendar::prev-year:
   * @calendar: the object which received the signal.
   *
   * Emitted when user switched to the previous year.
   */
  gtk_calendar_signals[PREV_YEAR_SIGNAL] =
    g_signal_new (I_("prev-year"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, prev_year),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCalendar::next-year:
   * @calendar: the object which received the signal.
   *
   * Emitted when user switched to the next year.
   */
  gtk_calendar_signals[NEXT_YEAR_SIGNAL] =
    g_signal_new (I_("next-year"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, next_year),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_CALENDAR);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("calendar"));
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static void
gtk_calendar_init (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkEventController *controller;
  GtkGesture *gesture;
  gint i;
#ifdef G_OS_WIN32
  wchar_t wbuffer[100];
#else
  static const char *month_format = NULL;
  char buffer[255];
  time_t tmp_time;
#endif
  gchar *year_before;
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  union { unsigned int word; char *string; } langinfo;
  gint week_1stday = 0;
  gint first_weekday = 1;
  guint week_origin;
#else
  gchar *week_start;
#endif
  GtkDropTarget *dest;
  int min_year_width;
  GDateTime *now;

  gtk_widget_set_can_focus (widget, TRUE);

  gtk_widget_add_css_class (GTK_WIDGET (calendar), GTK_STYLE_CLASS_VIEW);

  priv->header_box = g_object_new (GTK_TYPE_BOX,
                                   "css-name", "header",
                                   NULL);
  priv->year_label = gtk_label_new ("");
  gtk_widget_add_css_class (priv->year_label, "year");
  priv->month_name_stack = gtk_stack_new ();
  gtk_widget_add_css_class (priv->month_name_stack, "month");
  priv->arrow_widgets[0] = gtk_button_new_from_icon_name ("pan-start-symbolic");
  g_signal_connect_swapped (priv->arrow_widgets[0], "clicked", G_CALLBACK (calendar_set_month_prev), calendar);
  priv->arrow_widgets[1] = gtk_button_new_from_icon_name ("pan-end-symbolic");
  g_signal_connect_swapped (priv->arrow_widgets[1], "clicked", G_CALLBACK (calendar_set_month_next), calendar);
  gtk_widget_set_hexpand (priv->arrow_widgets[1], TRUE);
  gtk_widget_set_halign (priv->arrow_widgets[1], GTK_ALIGN_START);
  priv->arrow_widgets[2] = gtk_button_new_from_icon_name ("pan-start-symbolic");
  g_signal_connect_swapped (priv->arrow_widgets[2], "clicked", G_CALLBACK (calendar_set_year_prev), calendar);
  priv->arrow_widgets[3] = gtk_button_new_from_icon_name ("pan-end-symbolic");
  g_signal_connect_swapped (priv->arrow_widgets[3], "clicked", G_CALLBACK (calendar_set_year_next), calendar);

  gtk_container_add (GTK_CONTAINER (priv->header_box), priv->arrow_widgets[0]);
  gtk_container_add (GTK_CONTAINER (priv->header_box), priv->month_name_stack);
  gtk_container_add (GTK_CONTAINER (priv->header_box), priv->arrow_widgets[1]);
  gtk_container_add (GTK_CONTAINER (priv->header_box), priv->arrow_widgets[2]);
  gtk_container_add (GTK_CONTAINER (priv->header_box), priv->year_label);
  gtk_container_add (GTK_CONTAINER (priv->header_box), priv->arrow_widgets[3]);

  gtk_widget_set_parent (priv->header_box, GTK_WIDGET (calendar));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (gtk_calendar_button_press), calendar);
  g_signal_connect (gesture, "released", G_CALLBACK (gtk_calendar_button_release), calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_drag_new ();
  g_signal_connect (gesture, "drag-begin", G_CALLBACK (gtk_calendar_drag_begin), calendar);
  g_signal_connect (gesture, "drag-update", G_CALLBACK (gtk_calendar_drag_update), calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), GTK_EVENT_CONTROLLER (gesture));

  controller =
    gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                                     GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (gtk_calendar_scroll_controller_scroll),
                    calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), controller);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_calendar_key_controller_key_pressed),
                    calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), controller);
  controller = gtk_event_controller_focus_new ();
  g_signal_connect (controller, "enter",
                    G_CALLBACK (gtk_calendar_focus_controller_focus),
                    calendar);
  g_signal_connect (controller, "leave",
                    G_CALLBACK (gtk_calendar_focus_controller_focus),
                    calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), controller);

  if (!default_abbreviated_dayname[0])
    for (i=0; i<7; i++)
      {
#ifndef G_OS_WIN32
        tmp_time= (i+3)*86400;
        strftime (buffer, sizeof (buffer), "%a", gmtime (&tmp_time));
        default_abbreviated_dayname[i] = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
#else
        if (!GetLocaleInfoW (GetThreadLocale (), LOCALE_SABBREVDAYNAME1 + (i+6)%7,
                             wbuffer, G_N_ELEMENTS (wbuffer)))
          default_abbreviated_dayname[i] = g_strdup_printf ("(%d)", i);
        else
          default_abbreviated_dayname[i] = g_utf16_to_utf8 (wbuffer, -1, NULL, NULL, NULL);
#endif
      }

  if (!default_monthname[0])
    for (i=0; i<12; i++)
      {
#ifndef G_OS_WIN32
        tmp_time=i*2764800;
        if (G_UNLIKELY (month_format == NULL))
          {
            buffer[0] = '\0';
            month_format = "%OB";
            strftime (buffer, sizeof (buffer), month_format, gmtime (&tmp_time));
            /* "%OB" is not supported in Linux with glibc < 2.27  */
            if (!strcmp (buffer, "%OB") || !strcmp (buffer, "OB") || !strcmp (buffer, ""))
              {
                month_format = "%B";
                strftime (buffer, sizeof (buffer), month_format, gmtime (&tmp_time));
              }
          }
        else
          strftime (buffer, sizeof (buffer), month_format, gmtime (&tmp_time));

        default_monthname[i] = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
#else
        if (!GetLocaleInfoW (GetThreadLocale (), LOCALE_SMONTHNAME1 + i,
                             wbuffer, G_N_ELEMENTS (wbuffer)))
          default_monthname[i] = g_strdup_printf ("(%d)", i);
        else
          default_monthname[i] = g_utf16_to_utf8 (wbuffer, -1, NULL, NULL, NULL);
#endif
      }

  for (i = 0; i < 12; i ++)
    {
      GtkWidget *month_label = gtk_label_new (default_monthname[i]);

      gtk_stack_add_named (GTK_STACK (priv->month_name_stack), month_label, default_monthname[i]);
    }

  priv->grid = gtk_grid_new ();
  gtk_grid_set_row_homogeneous (GTK_GRID (priv->grid), TRUE);
  gtk_grid_set_column_homogeneous (GTK_GRID (priv->grid), TRUE);
  /* Day name labels */
  for (i = 0; i < 7; i ++)
    {
      GtkWidget *label = gtk_label_new (default_abbreviated_dayname[i]);

      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_set_vexpand (label, TRUE);
      gtk_widget_add_css_class (label, "day-name");
      gtk_grid_attach (GTK_GRID (priv->grid), label, 1 + i, 0, 1, 1);

      priv->day_name_labels[i] = label;
    }

  /* Week number labels */
  for (i = 0; i < 6; i ++)
    {
      GtkWidget *label = gtk_label_new ("");

      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_set_vexpand (label, TRUE);
      gtk_widget_add_css_class (label, "week-number");
      gtk_grid_attach (GTK_GRID (priv->grid), label, 0, 1 + i, 1, 1);

      priv->week_number_labels[i] = label;
      gtk_widget_hide (label);
    }

  {
    int x, y;

    for (y = 0; y < 6; y ++)
      for (x = 0; x < 7; x ++)
        {
          GtkWidget *label = gtk_label_new ("");

          gtk_widget_set_hexpand (label, TRUE);
          gtk_widget_set_vexpand (label, TRUE);
          gtk_widget_add_css_class (label, "day-number");
          gtk_grid_attach (GTK_GRID (priv->grid), label, 1 + x, 1 + y, 1, 1);

          priv->day_number_labels[y][x] = label;
        }
  }

  gtk_widget_set_hexpand (priv->grid, TRUE);
  gtk_widget_set_vexpand (priv->grid, TRUE);
  gtk_widget_set_parent (priv->grid, GTK_WIDGET (calendar));

  for (i=0;i<31;i++)
    priv->marked_date[i] = FALSE;
  priv->num_marked_dates = 0;

  priv->show_heading = TRUE;
  priv->show_day_names = TRUE;

  priv->focus_row = -1;
  priv->focus_col = -1;

  priv->in_drag = 0;

  dest = gtk_drop_target_new (gdk_content_formats_new_for_gtype (G_TYPE_STRING),
                              GDK_ACTION_COPY);

  g_signal_connect (dest, "accept", G_CALLBACK (gtk_calendar_drag_accept), calendar);
  g_signal_connect (dest, "drag-leave", G_CALLBACK (gtk_calendar_drag_leave), calendar);
  g_signal_connect (dest, "drag-drop", G_CALLBACK (gtk_calendar_drag_drop), calendar);

  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (dest));

  priv->year_before = 0;

  /* Translate to calendar:YM if you want years to be displayed
   * before months; otherwise translate to calendar:MY.
   * Do *not* translate it to anything else, if it
   * it isn't calendar:YM or calendar:MY it will not work.
   *
   * Note that the ordering described here is logical order, which is
   * further influenced by BIDI ordering. Thus, if you have a default
   * text direction of RTL and specify "calendar:YM", then the year
   * will appear to the right of the month.
   */
  year_before = _("calendar:MY");
  if (strcmp (year_before, "calendar:YM") == 0)
    priv->year_before = 1;
  else if (strcmp (year_before, "calendar:MY") != 0)
    g_warning ("Whoever translated calendar:MY did so wrongly.");

#ifdef G_OS_WIN32
  priv->week_start = 0;
  week_start = NULL;

  if (GetLocaleInfoW (GetThreadLocale (), LOCALE_IFIRSTDAYOFWEEK,
                      wbuffer, G_N_ELEMENTS (wbuffer)))
    week_start = g_utf16_to_utf8 (wbuffer, -1, NULL, NULL, NULL);

  if (week_start != NULL)
    {
      priv->week_start = (week_start[0] - '0' + 1) % 7;
      g_free(week_start);
    }
#else
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
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

  priv->week_start = (week_1stday + first_weekday - 1) % 7;
#else
  /* Translate to calendar:week_start:0 if you want Sunday to be the
   * first day of the week to calendar:week_start:1 if you want Monday
   * to be the first day of the week, and so on.
   */
  week_start = _("calendar:week_start:0");

  if (strncmp (week_start, "calendar:week_start:", 20) == 0)
    priv->week_start = *(week_start + 20) - '0';
  else
    priv->week_start = -1;

  if (priv->week_start < 0 || priv->week_start > 6)
    {
      g_warning ("Whoever translated calendar:week_start:0 did so wrongly.");
      priv->week_start = 0;
    }
#endif
#endif

  gtk_orientable_set_orientation (GTK_ORIENTABLE (gtk_widget_get_layout_manager (GTK_WIDGET (calendar))),
                                  GTK_ORIENTATION_VERTICAL);

  /* Select current day */
  priv->date = g_date_time_new_from_unix_local (0);
  now = g_date_time_new_now_local ();
  gtk_calendar_select_day (calendar, now);
  g_date_time_unref (now);

  /* We just initialized the year label, now add some space to it so
   * changing the year does not increase the calendar width */
  gtk_widget_measure (priv->year_label, GTK_ORIENTATION_HORIZONTAL, -1,
                      &min_year_width, NULL, NULL, NULL);
  gtk_widget_set_size_request (priv->year_label, min_year_width + 10, -1);
}

#pragma GCC diagnostic pop

static void
calendar_queue_refresh (GtkCalendar *calendar)
{
  gtk_widget_queue_resize (GTK_WIDGET (calendar));
}

static void
calendar_set_month_prev (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GDateTime *new_date;

  new_date = g_date_time_add_months (priv->date, -1);

  gtk_calendar_select_day (calendar, new_date);
  g_date_time_unref (new_date);

  g_signal_emit (calendar, gtk_calendar_signals[PREV_MONTH_SIGNAL], 0);
}

static void
calendar_set_month_next (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GDateTime *new_date;

  new_date = g_date_time_add_months (priv->date, 1);

  gtk_calendar_select_day (calendar, new_date);
  g_date_time_unref (new_date);

  g_signal_emit (calendar, gtk_calendar_signals[NEXT_MONTH_SIGNAL], 0);
}

static void
calendar_set_year_prev (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GDateTime *new_date;

  new_date = g_date_time_add_years (priv->date, -1);

  gtk_calendar_select_day (calendar, new_date);
  g_date_time_unref (new_date);

  g_signal_emit (calendar, gtk_calendar_signals[PREV_YEAR_SIGNAL], 0);
}

static void
calendar_set_year_next (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GDateTime *new_date;

  new_date = g_date_time_add_years (priv->date, 1);

  gtk_calendar_select_day (calendar, new_date);
  g_date_time_unref (new_date);

  g_signal_emit (calendar, gtk_calendar_signals[NEXT_YEAR_SIGNAL], 0);
}

static void
calendar_compute_days (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  const int month = g_date_time_get_month (priv->date);
  const int year = g_date_time_get_year (priv->date);
  int ndays_in_month;
  int ndays_in_prev_month;
  int first_day;
  int row;
  int col;
  int day;

  ndays_in_month = month_length[leap (year)][month];

  first_day = day_of_week (year, month, 1);
  first_day = (first_day + 7 - priv->week_start) % 7;
  if (first_day == 0)
    first_day = 7;

  /* Compute days of previous month */
  if (month > 1)
    ndays_in_prev_month = month_length[leap (year)][month - 1];
  else
    ndays_in_prev_month = month_length[leap (year - 1)][12];
  day = ndays_in_prev_month - first_day+ 1;

  for (col = 0; col < first_day; col++)
    {
      priv->day[0][col] = day;
      priv->day_month[0][col] = MONTH_PREV;
      day++;
    }

  /* Compute days of current month */
  row = first_day / 7;
  col = first_day % 7;
  for (day = 1; day <= ndays_in_month; day++)
    {
      priv->day[row][col] = day;
      priv->day_month[row][col] = MONTH_CURRENT;

      col++;
      if (col == 7)
        {
          row++;
          col = 0;
        }
    }

  /* Compute days of next month */
  day = 1;
  for (; row <= 5; row++)
    {
      for (; col <= 6; col++)
        {
          priv->day[row][col] = day;
          priv->day_month[row][col] = MONTH_NEXT;
          day++;
        }
      col = 0;
    }
}

static void
calendar_select_and_focus_day (GtkCalendar *calendar,
                               int          day)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GDateTime *new_date;
  gint row;
  gint col;

  for (row = 0; row < 6; row ++)
    for (col = 0; col < 7; col++)
      {
        if (priv->day_month[row][col] == MONTH_CURRENT &&
            priv->day[row][col] == day)
          {
            priv->focus_row = row;
            priv->focus_col = col;
            break;
          }
      }

  new_date = g_date_time_new_local (g_date_time_get_year (priv->date),
                                    g_date_time_get_month (priv->date),
                                    day,
                                    0, 0, 0);

  gtk_calendar_select_day (calendar, new_date);
  g_date_time_unref (new_date);
}

static void
gtk_calendar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkCalendar *calendar = GTK_CALENDAR (object);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GDateTime *date;

  switch (prop_id)
    {
    case PROP_YEAR:
      date = g_date_time_new_local (g_value_get_int (value),
                                    g_date_time_get_month (priv->date),
                                    g_date_time_get_day_of_month (priv->date),
                                    0, 0, 0);
      if (date)
        {
          gtk_calendar_select_day (calendar, date);
          g_date_time_unref (date);
        }
      break;
    case PROP_MONTH:
      date = g_date_time_new_local (g_date_time_get_year (priv->date),
                                    g_value_get_int (value) + 1,
                                    g_date_time_get_day_of_month (priv->date),
                                    0, 0, 0);
      if (date)
        {
          gtk_calendar_select_day (calendar, date);
          g_date_time_unref (date);
        }
      break;
    case PROP_DAY:
      date = g_date_time_new_local (g_date_time_get_year (priv->date),
                                    g_date_time_get_month (priv->date),
                                    g_value_get_int (value) + 1,
                                    0, 0, 0);
      if (date)
        {
          gtk_calendar_select_day (calendar, date);
          g_date_time_unref (date);
        }
      break;
    case PROP_SHOW_HEADING:
      gtk_calendar_set_show_heading (calendar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_DAY_NAMES:
      gtk_calendar_set_show_day_names (calendar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_WEEK_NUMBERS:
      gtk_calendar_set_show_week_numbers (calendar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_calendar_get_property (GObject      *object,
                           guint         prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  GtkCalendar *calendar = GTK_CALENDAR (object);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  switch (prop_id)
    {
    case PROP_YEAR:
      g_value_set_int (value, g_date_time_get_year (priv->date));
      break;
    case PROP_MONTH:
      g_value_set_int (value, g_date_time_get_month (priv->date) - 1);
      break;
    case PROP_DAY:
      g_value_set_int (value, g_date_time_get_day_of_month (priv->date) - 1);
      break;
    case PROP_SHOW_HEADING:
      g_value_set_boolean (value, gtk_calendar_get_show_heading (calendar));
      break;
    case PROP_SHOW_DAY_NAMES:
      g_value_set_boolean (value, gtk_calendar_get_show_day_names (calendar));
      break;
    case PROP_SHOW_WEEK_NUMBERS:
      g_value_set_boolean (value, gtk_calendar_get_show_week_numbers (calendar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
calendar_invalidate_day_num (GtkCalendar *calendar,
                             gint         day)
{
  gtk_widget_queue_draw (GTK_WIDGET (calendar));
}

static void
calendar_invalidate_day (GtkCalendar *calendar,
                         gint         row,
                         gint         col)
{
  gtk_widget_queue_draw (GTK_WIDGET (calendar));
}

static void
gtk_calendar_button_press (GtkGestureClick *gesture,
                           int              n_press,
                           double           x,
                           double           y,
                           gpointer         user_data)
{
  GtkCalendar *calendar = user_data;
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  GtkWidget *label;
  int row = -1, col = -1;
  int ix, iy;
  int day_month;
  int day;

  label = gtk_widget_pick (widget, x, y, GTK_PICK_DEFAULT);
  for (iy = 0; iy < 6; iy ++)
    for (ix = 0; ix < 7; ix ++)
      {
        if (label == priv->day_number_labels[iy][ix])
          {
            row = iy;
            col = ix;
          }
      }

  /* If row or column isn't found, just return. */
  if (row == -1 || col == -1)
    return;

  day_month = priv->day_month[row][col];
  day = priv->day[row][col];

  if (day_month == MONTH_PREV)
    calendar_set_month_prev (calendar);
  else if (day_month == MONTH_NEXT)
    calendar_set_month_next (calendar);

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (button == GDK_BUTTON_PRIMARY)
    {
      priv->in_drag = 1;
      priv->drag_start_x = x;
      priv->drag_start_y = y;
    }

  calendar_select_and_focus_day (calendar, day);
}

static void
gtk_calendar_button_release (GtkGestureClick *gesture,
                             int              n_press,
                             double           x,
                             double           y,
                             gpointer         user_data)
{
  GtkCalendar *calendar = user_data;
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    {
      if (priv->in_drag)
        priv->in_drag = 0;
    }
}

static void
gtk_calendar_drag_begin (GtkGestureDrag *gesture,
                         double          x,
                         double          y,
                         gpointer        data)
{
  GtkCalendar *calendar = data;
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  priv->in_drag = TRUE;
}

static GdkContentProvider *
get_calendar_content (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GDate *date;
  gchar str[128];

  date = g_date_new_dmy (g_date_time_get_day_of_month (priv->date),
                         g_date_time_get_month (priv->date),
                         g_date_time_get_year (priv->date));
  g_date_strftime (str, 127, "%x", date);
  g_free (date);

  return gdk_content_provider_new_typed (G_TYPE_STRING, str);
}

static void
gtk_calendar_drag_update (GtkGestureDrag *gesture,
                          double          x,
                          double          y,
                          gpointer        data)
{
  GtkWidget *widget = data;
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gdouble start_x, start_y;
  GdkContentProvider *content;
  GdkDevice *device;
  GdkDrag *drag;
  GtkIconTheme *theme;
  GtkIconPaintable *icon;
  GdkSurface *surface;

  if (!priv->in_drag)
    return;

  if (!gtk_drag_check_threshold (widget, 0, 0, x, y))
    return;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

  surface = gtk_native_get_surface (gtk_widget_get_native (widget));
  device = gtk_gesture_get_device (GTK_GESTURE (gesture));

  content = get_calendar_content (calendar);

  drag = gdk_drag_begin (surface, device, content, GDK_ACTION_COPY, start_x, start_y);

  theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (widget));
  icon = gtk_icon_theme_lookup_icon (theme,
                                     "text-x-generic",
                                     NULL,
                                     32,
                                     1,
                                     GTK_TEXT_DIR_NONE,
                                     0);
  gtk_drag_icon_set_from_paintable (drag, GDK_PAINTABLE (icon), 0, 0);
  g_clear_object (&icon);

  g_object_unref (content);
  g_object_unref (drag);

  priv->in_drag = 0;
}

static gboolean
gtk_calendar_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                       gdouble                   dx,
                                       gdouble                   dy,
                                       GtkWidget                *widget)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (dy < 0)
    calendar_set_month_prev (calendar);
  else if (dy > 0)
    calendar_set_month_next (calendar);

  return GDK_EVENT_STOP;
}


/****************************************
 *             Key handling              *
 ****************************************/

static void
move_focus (GtkCalendar *calendar,
            gint         direction)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkTextDirection text_dir = gtk_widget_get_direction (GTK_WIDGET (calendar));

  if ((text_dir == GTK_TEXT_DIR_LTR && direction == -1) ||
      (text_dir == GTK_TEXT_DIR_RTL && direction == 1))
    {
      if (priv->focus_col > 0)
          priv->focus_col--;
      else if (priv->focus_row > 0)
        {
          priv->focus_col = 6;
          priv->focus_row--;
        }

      if (priv->focus_col < 0)
        priv->focus_col = 6;
      if (priv->focus_row < 0)
        priv->focus_row = 5;
    }
  else
    {
      if (priv->focus_col < 6)
        priv->focus_col++;
      else if (priv->focus_row < 5)
        {
          priv->focus_col = 0;
          priv->focus_row++;
        }

      if (priv->focus_col < 0)
        priv->focus_col = 0;
      if (priv->focus_row < 0)
        priv->focus_row = 0;
    }
}

static gboolean
gtk_calendar_key_controller_key_pressed (GtkEventControllerKey *controller,
                                         guint                  keyval,
                                         guint                  keycode,
                                         GdkModifierType        state,
                                         GtkWidget             *widget)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint return_val;
  gint old_focus_row;
  gint old_focus_col;
  gint row, col, day;

  return_val = FALSE;

  old_focus_row = priv->focus_row;
  old_focus_col = priv->focus_col;

  switch (keyval)
    {
    case GDK_KEY_KP_Left:
    case GDK_KEY_Left:
      return_val = TRUE;
      if (state & GDK_CONTROL_MASK)
        calendar_set_month_prev (calendar);
      else
        {
          move_focus (calendar, -1);
          calendar_invalidate_day (calendar, old_focus_row, old_focus_col);
          calendar_invalidate_day (calendar, priv->focus_row,
                                   priv->focus_col);
        }
      break;
    case GDK_KEY_KP_Right:
    case GDK_KEY_Right:
      return_val = TRUE;
      if (state & GDK_CONTROL_MASK)
        calendar_set_month_next (calendar);
      else
        {
          move_focus (calendar, 1);
          calendar_invalidate_day (calendar, old_focus_row, old_focus_col);
          calendar_invalidate_day (calendar, priv->focus_row,
                                   priv->focus_col);
        }
      break;
    case GDK_KEY_KP_Up:
    case GDK_KEY_Up:
      return_val = TRUE;
      if (state & GDK_CONTROL_MASK)
        calendar_set_year_prev (calendar);
      else
        {
          if (priv->focus_row > 0)
            priv->focus_row--;
          if (priv->focus_row < 0)
            priv->focus_row = 5;
          if (priv->focus_col < 0)
            priv->focus_col = 6;
          calendar_invalidate_day (calendar, old_focus_row, old_focus_col);
          calendar_invalidate_day (calendar, priv->focus_row,
                                   priv->focus_col);
        }
      break;
    case GDK_KEY_KP_Down:
    case GDK_KEY_Down:
      return_val = TRUE;
      if (state & GDK_CONTROL_MASK)
        calendar_set_year_next (calendar);
      else
        {
          if (priv->focus_row < 5)
            priv->focus_row++;
          if (priv->focus_col < 0)
            priv->focus_col = 0;
          calendar_invalidate_day (calendar, old_focus_row, old_focus_col);
          calendar_invalidate_day (calendar, priv->focus_row,
                                   priv->focus_col);
        }
      break;
    case GDK_KEY_KP_Space:
    case GDK_KEY_space:
      row = priv->focus_row;
      col = priv->focus_col;

      if (row > -1 && col > -1)
        {
          return_val = TRUE;

          day = priv->day[row][col];
          if (priv->day_month[row][col] == MONTH_PREV)
            calendar_set_month_prev (calendar);
          else if (priv->day_month[row][col] == MONTH_NEXT)
            calendar_set_month_next (calendar);

          calendar_select_and_focus_day (calendar, day);
        }
    default:
      break;
    }

  return return_val;
}

static void
gtk_calendar_focus_controller_focus (GtkEventController     *controller,
                                     GtkWidget              *widget)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  calendar_queue_refresh (calendar);
  priv->in_drag = 0;
}

static void
gtk_calendar_state_flags_changed (GtkWidget     *widget,
                                  GtkStateFlags  previous_state)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  if (!gtk_widget_is_sensitive (widget))
    {
      priv->in_drag = 0;
    }
}

/* Get/set whether drag_motion requested the drag data and
 * drag_data_received should thus not actually insert the data,
 * since the data doesn’t result from a drop.
 */
static void
set_status_pending (GdkDrop       *drop,
                    GdkDragAction  suggested_action)
{
  g_object_set_data (G_OBJECT (drop),
                     I_("gtk-calendar-status-pending"),
                     GINT_TO_POINTER (suggested_action));
}

static GdkDragAction
get_status_pending (GdkDrop *drop)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drop),
                                             "gtk-calendar-status-pending"));
}

static void
gtk_calendar_drag_leave (GtkDropTarget *dest,
                         GdkDrop       *drop,
                         GtkCalendar   *calendar)
{
}

static void
got_text (GObject      *source,
          GAsyncResult *result,
          gpointer      data)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (data);
  GtkCalendar *calendar = GTK_CALENDAR (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest)));
  GdkDrop *drop = GDK_DROP (source);
  gchar *str;
  GDate *date;
  GDateTime *datetime;
  GdkDragAction suggested_action;

  suggested_action = get_status_pending (drop);
  set_status_pending (drop, 0);

  str = gdk_drop_read_text_finish (drop, result, NULL);

  if (suggested_action)
    {
      if (str)
        {
          date = g_date_new ();
          g_date_set_parse (date, str);
          if (!g_date_valid (date))
              suggested_action = 0;
          g_date_free (date);
          g_free (str);
        }
      else
        suggested_action = 0;
      gdk_drop_status (drop, suggested_action);
      if (suggested_action == 0)
        gtk_drop_target_deny_drop (dest, drop);
      return;
    }

  date = g_date_new ();
  if (str)
    {
      g_date_set_parse (date, str);
      g_free (str);
    }

  if (!g_date_valid (date))
    {
      g_warning ("Received invalid date data");
      g_date_free (date);
      gdk_drop_finish (drop, 0);
      gtk_drop_target_deny_drop (dest, drop);
      return;
    }

  datetime = g_date_time_new_local (g_date_get_year (date),
                                    g_date_get_month (date),
                                    g_date_get_day (date),
                                    0, 0, 0);
  g_date_free (date);

  gdk_drop_finish (drop, suggested_action);

  gtk_calendar_select_day (calendar, datetime);
  g_date_time_unref (datetime);
}

static gboolean
gtk_calendar_drag_accept (GtkDropTarget *dest,
                          GdkDrop       *drop,
                          GtkCalendar   *calendar)
{
  const char *target;

  target = gtk_drop_target_find_mimetype (dest);
  if (!target || gdk_drop_get_actions (drop) == 0)
    {
      gdk_drop_status (drop, 0);
      return FALSE;
    }
  else if (get_status_pending (drop) == 0)
    {
      set_status_pending (drop, gdk_drop_get_actions (drop));
      gdk_drop_read_text_async (drop, NULL, got_text, dest);
    }
  return TRUE;
}

static gboolean
gtk_calendar_drag_drop (GtkDropTarget  *dest,
                        GdkDrop        *drop,
                        int             x,
                        int             y,
                        GtkCalendar    *calendar)
{
  const char *target;

  target = gtk_drop_target_find_mimetype (dest);
  if (target != NULL)
    {
      set_status_pending (drop, 0);
      gdk_drop_read_text_async (drop, NULL, got_text, dest);
      return TRUE;
    }

  return FALSE;
}


/****************************************
 *              Public API              *
 ****************************************/

/**
 * gtk_calendar_new:
 *
 * Creates a new calendar, with the current date being selected.
 *
 * Returns: a newly #GtkCalendar widget
 **/
GtkWidget*
gtk_calendar_new (void)
{
  return g_object_new (GTK_TYPE_CALENDAR, NULL);
}

/**
 * gtk_calendar_select_day:
 * @self: a #GtkCalendar.
 * @date: (transfer none): a #GDateTime representing the day to select
 *
 * Will switch to @date's year and month and select its day.
 **/
void
gtk_calendar_select_day (GtkCalendar *self,
                         GDateTime   *date)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (self);
  GDateTime *today;
  int new_day, new_month, new_year;
  gboolean day_changed, month_changed, year_changed;
  char buffer[255];
  char *str;
  time_t tmp_time;
  struct tm *tm;
  int i;
  int x, y;
  int today_day;

  g_return_if_fail (GTK_IS_CALENDAR (self));
  g_return_if_fail (date != NULL);

  day_changed = g_date_time_get_day_of_month (priv->date) != g_date_time_get_day_of_month (date);
  month_changed = g_date_time_get_month (priv->date) != g_date_time_get_month (date);
  year_changed = g_date_time_get_year (priv->date) != g_date_time_get_year (date);

  if (!day_changed && !month_changed && !year_changed)
    return;

  new_year = g_date_time_get_year (date);
  new_month = g_date_time_get_month (date);
  new_day = g_date_time_get_day_of_month (date);

  g_date_time_unref (priv->date);
  priv->date = g_date_time_ref (date);

  tmp_time = 1;  /* Jan 1 1970, 00:00:01 UTC */
  tm = gmtime (&tmp_time);
  tm->tm_year = new_year - 1900;

  /* Translators: This dictates how the year is displayed in
   * gtkcalendar widget.  See strftime() manual for the format.
   * Use only ASCII in the translation.
   *
   * "%Y" is appropriate for most locales.
   */
  strftime (buffer, sizeof (buffer), C_("calendar year format", "%Y"), tm);
  str = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
  gtk_label_set_label (GTK_LABEL (priv->year_label), str);
  g_free (str);

  /* Update month */

  calendar_compute_days (self);
  gtk_stack_set_visible_child_name (GTK_STACK (priv->month_name_stack),
                                    default_monthname[new_month - 1]);

  today = g_date_time_new_now_local ();

  if (g_date_time_get_year (priv->date) == g_date_time_get_year (today) &&
      g_date_time_get_month (priv->date) == g_date_time_get_month (today))
    today_day = g_date_time_get_day_of_month (today);
  else
    today_day = -1;

  g_date_time_unref (today);

  /* Update day labels */
  for (y = 0; y < 6; y ++)
    for (x = 0; x < 7; x ++)
      {
        const int day = priv->day[y][x];
        GtkWidget *label = priv->day_number_labels[y][x];
        /* Translators: this defines whether the day numbers should use
         * localized digits or the ones used in English (0123...).
         *
         * Translate to "%Id" if you want to use localized digits, or
         * translate to "%d" otherwise.
         *
         * Note that translating this doesn't guarantee that you get localized
         * digits. That needs support from your system and locale definition
         * too.
         */
        g_snprintf (buffer, sizeof (buffer), C_("calendar:day:digits", "%d"), day);

        gtk_label_set_label (GTK_LABEL (label), buffer);

        if (priv->day_month[y][x] == MONTH_PREV ||
            priv->day_month[y][x] == MONTH_NEXT)
          gtk_widget_add_css_class (label, "other-month");
        else
          gtk_widget_remove_css_class (label, "other-month");

        if (priv->marked_date[day-1])
          gtk_widget_set_state_flags (label, GTK_STATE_FLAG_CHECKED, FALSE);
        else
          gtk_widget_unset_state_flags (label, GTK_STATE_FLAG_CHECKED);

        if (new_day == day &&
            priv->day_month[y][x] == MONTH_CURRENT)
          gtk_widget_set_state_flags (label, GTK_STATE_FLAG_SELECTED, FALSE);
        else
          gtk_widget_unset_state_flags (label, GTK_STATE_FLAG_SELECTED);

        if (day == today_day)
          gtk_widget_add_css_class (label, "today");
        else
          gtk_widget_remove_css_class (label, "today");
      }

  /* Update week number labels.
   * We simply get the week number of priv->date and add the others.
   * simple. */
  for (i = 0; i < 6; i ++)
    {
      int year = new_year;
      int month, week;

      month = new_month + priv->day_month[i][6] - MONTH_CURRENT;

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

      week = week_of_year (year, month, priv->day[i][6]);

      /* Translators: this defines whether the week numbers should use
       * localized digits or the ones used in English (0123...).
       *
       * Translate to "%Id" if you want to use localized digits, or
       * translate to "%d" otherwise.
       * Note that translating this doesn't guarantee that you get localized
       * digits. That needs support from your system and locale definition
       * too. */
      g_snprintf (buffer, sizeof (buffer), C_("calendar:week:digits", "%d"), week);

      gtk_label_set_label (GTK_LABEL (priv->week_number_labels[i]), buffer);
    }

  if (day_changed)
    {
      g_object_notify (G_OBJECT (self), "day");
      g_signal_emit (self, gtk_calendar_signals[DAY_SELECTED_SIGNAL], 0);
    }

  if (month_changed)
    g_object_notify (G_OBJECT (self), "month");

  if (year_changed)
    g_object_notify (G_OBJECT (self), "year");

}

/**
 * gtk_calendar_clear_marks:
 * @calendar: a #GtkCalendar
 *
 * Remove all visual markers.
 **/
void
gtk_calendar_clear_marks (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  guint day;

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  for (day = 0; day < 31; day++)
    {
      priv->marked_date[day] = FALSE;
    }

  priv->num_marked_dates = 0;
  calendar_queue_refresh (calendar);
}

/**
 * gtk_calendar_mark_day:
 * @calendar: a #GtkCalendar
 * @day: the day number to mark between 1 and 31.
 *
 * Places a visual marker on a particular day.
 */
void
gtk_calendar_mark_day (GtkCalendar *calendar,
                       guint        day)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  if (day >= 1 && day <= 31 && !priv->marked_date[day-1])
    {
      priv->marked_date[day - 1] = TRUE;
      priv->num_marked_dates++;
      calendar_invalidate_day_num (calendar, day);
    }
}

/**
 * gtk_calendar_get_day_is_marked:
 * @calendar: a #GtkCalendar
 * @day: the day number between 1 and 31.
 *
 * Returns if the @day of the @calendar is already marked.
 *
 * Returns: whether the day is marked.
 */
gboolean
gtk_calendar_get_day_is_marked (GtkCalendar *calendar,
                                guint        day)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), FALSE);

  if (day >= 1 && day <= 31)
    return priv->marked_date[day - 1];

  return FALSE;
}

/**
 * gtk_calendar_unmark_day:
 * @calendar: a #GtkCalendar.
 * @day: the day number to unmark between 1 and 31.
 *
 * Removes the visual marker from a particular day.
 */
void
gtk_calendar_unmark_day (GtkCalendar *calendar,
                         guint        day)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  if (day >= 1 && day <= 31 && priv->marked_date[day-1])
    {
      priv->marked_date[day - 1] = FALSE;
      priv->num_marked_dates--;
      calendar_invalidate_day_num (calendar, day);
    }
}

/**
 * gtk_calendar_get_date:
 * @self: a #GtkCalendar
 *
 * Returns: (transfer full): A #GDateTime representing the shown
 *   year, month and the selected day, in the local time zone.
 */
GDateTime *
gtk_calendar_get_date (GtkCalendar *self)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_CALENDAR (self), NULL);

  return g_date_time_ref (priv->date);
}

/**
 * gtk_calendar_set_show_week_numbers
 * @self: a #GtkCalendar
 * @value: Whether to show week numbers on the left of the days
 *
 * Sets whether week numbers are shown in the calendar.
 */
void
gtk_calendar_set_show_week_numbers (GtkCalendar *self,
                                    gboolean     value)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (self);

  g_return_if_fail (GTK_IS_CALENDAR (self));

  if (value != priv->show_week_numbers)
    {
      int i;

      priv->show_week_numbers = value;

      for (i = 0; i < 6; i ++)
        gtk_widget_set_visible (priv->week_number_labels[i], value);

      g_object_notify (G_OBJECT (self), "show-week-numbers");
    }
}

/**
 * gtk_calendar_get_show_week_numbers:
 * @self: a #GtkCalendar
 *
 * Returns: Whether @self is showing week numbers right now,
 *   i.e. the value of the #GtkCalendar:show-week-numbers property.
 */
gboolean
gtk_calendar_get_show_week_numbers (GtkCalendar *self)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_CALENDAR (self), FALSE);

  return priv->show_week_numbers;
}

/**
 * gtk_calendar_set_show_heading:
 * @self: a #GtkCalendar
 * @value: Whether to show the heading in the calendar,
 *   containing the current year and month as well as
 *   buttons for changing both.
 */
void
gtk_calendar_set_show_heading (GtkCalendar *self,
                               gboolean     value)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (self);

  g_return_if_fail (GTK_IS_CALENDAR (self));

  if (value != priv->show_heading)
    {
      priv->show_heading = value;

      gtk_widget_set_visible (priv->header_box, value);

      g_object_notify (G_OBJECT (self), "show-heading");
    }
}

/**
 * gtk_calendar_get_show_heading:
 * @self: a #GtkCalendar
 *
 * Returns: Whether @self is currently showing the heading,
 *   i.e. the value of the #GtkCalendar:show-heading property.
 */
gboolean
gtk_calendar_get_show_heading (GtkCalendar *self)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_CALENDAR (self), FALSE);

  return priv->show_heading;
}

/**
 * gtk_calendar_set_show_day_names:
 * @self: a #GtkCalendar
 * @value: Whether to show week day names above the
 *   day numbers
 */
void
gtk_calendar_set_show_day_names (GtkCalendar *self,
                                 gboolean     value)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (self);

  g_return_if_fail (GTK_IS_CALENDAR (self));

  if (value != priv->show_day_names)
    {
      int i;

      priv->show_day_names = value;

      for (i = 0; i < 7; i ++)
        gtk_widget_set_visible (priv->day_name_labels[i], value);

      g_object_notify (G_OBJECT (self), "show-day-names");
    }
}

/**
 * gtk_calendar_get_day_names:
 * @self: a #GtkCalendar
 *
 * Returns: Whether @self is currently showing the names
 *   of the week days above the day numbers, i.e. the value
 *   of the #GtkCalendar:show-day-names property.
 */
gboolean
gtk_calendar_get_show_day_names (GtkCalendar *self)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_CALENDAR (self), FALSE);

  return priv->show_day_names;
}
