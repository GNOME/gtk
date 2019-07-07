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
 * The month and year currently displayed can be altered with
 * gtk_calendar_select_month(). The exact day can be selected from the
 * displayed month using gtk_calendar_select_day().
 *
 * To place a visual marker on a particular day, use gtk_calendar_mark_day()
 * and to remove the marker, gtk_calendar_unmark_day(). Alternative, all
 * marks can be cleared with gtk_calendar_clear_marks().
 *
 * The way in which the calendar itself is displayed can be altered using
 * gtk_calendar_set_display_options().
 *
 * The selected date can be retrieved from a #GtkCalendar using
 * gtk_calendar_get_date().
 *
 * Users should be aware that, although the Gregorian calendar is the
 * legal calendar in most countries, it was adopted progressively
 * between 1582 and 1929. Display before these dates is likely to be
 * historically incorrect.
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
#include "gtkdnd.h"
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

#define TIMEOUT_INITIAL  500
#define TIMEOUT_REPEAT    50

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

/* Spacing around day/week headers and main area, inside those windows */
#define CALENDAR_MARGIN          0

#define DAY_XSEP                 0 /* not really good for small calendar */
#define DAY_YSEP                 0 /* not really good for small calendar */

#define SCROLL_DELAY_FACTOR      5

enum {
  ARROW_YEAR_LEFT,
  ARROW_YEAR_RIGHT,
  ARROW_MONTH_LEFT,
  ARROW_MONTH_RIGHT
};

enum {
  MONTH_PREV,
  MONTH_CURRENT,
  MONTH_NEXT
};

enum {
  MONTH_CHANGED_SIGNAL,
  DAY_SELECTED_SIGNAL,
  DAY_SELECTED_DOUBLE_CLICK_SIGNAL,
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
  PROP_NO_MONTH_CHANGE,
  PROP_SHOW_WEEK_NUMBERS,
  PROP_SHOW_DETAILS,
  PROP_DETAIL_WIDTH_CHARS,
  PROP_DETAIL_HEIGHT_ROWS
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

  void (* month_changed)                (GtkCalendar *calendar);
  void (* day_selected)                 (GtkCalendar *calendar);
  void (* day_selected_double_click)    (GtkCalendar *calendar);
  void (* prev_month)                   (GtkCalendar *calendar);
  void (* next_month)                   (GtkCalendar *calendar);
  void (* prev_year)                    (GtkCalendar *calendar);
  void (* next_year)                    (GtkCalendar *calendar);
};

struct _GtkCalendarPrivate
{
  GtkCalendarDisplayOptions display_flags;

  gint  month;
  gint  year;
  gint  selected_day;

  gint  day_month[6][7];
  gint  day[6][7];

  gint  num_marked_dates;
  gint  marked_date[31];

  gint  focus_row;
  gint  focus_col;

  guint header_h;
  guint day_name_h;
  guint main_h;

  guint arrow_prelight : 4;
  guint arrow_width;
  guint max_month_width;
  guint max_year_width;

  guint day_width;
  guint week_width;

  guint min_day_width;
  guint max_day_char_width;
  guint max_day_char_ascent;
  guint max_day_char_descent;
  guint max_label_char_ascent;
  guint max_label_char_descent;
  guint max_week_char_width;

  /* flags */
  guint year_before : 1;

  guint need_timer  : 1;

  guint in_drag : 1;
  guint drag_highlight : 1;

  guint32 timer;
  gint click_child;

  gint week_start;

  gint drag_start_x;
  gint drag_start_y;

  /* Optional callback, used to display extra information for each day. */
  GtkCalendarDetailFunc detail_func;
  gpointer              detail_func_user_data;
  GDestroyNotify        detail_func_destroy;

  /* Size requistion for details provided by the hook. */
  gint detail_height_rows;
  gint detail_width_chars;
  gint detail_overflow[6];
};

static void gtk_calendar_destroy      (GtkWidget    *widget);
static void gtk_calendar_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec);
static void gtk_calendar_get_property (GObject      *object,
                                       guint         prop_id,
                                       GValue       *value,
                                       GParamSpec   *pspec);

static void     gtk_calendar_measure        (GtkWidget        *widget,
                                             GtkOrientation  orientation,
                                             int             for_size,
                                             int            *minimum,
                                             int            *natural,
                                             int            *minimum_baseline,
                                             int            *natural_baseline);
static void     gtk_calendar_size_allocate  (GtkWidget      *widget,
                                             int             width,
                                             int             height,
                                             int             baseline);
static void     gtk_calendar_snapshot       (GtkWidget      *widget,
                                             GtkSnapshot    *snapshot);
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
static void     gtk_calendar_key_controller_focus       (GtkEventControllerKey *controller,
                                                         GdkCrossingMode        mode,
                                                         GdkNotifyType          detail,
                                                         GtkWidget             *widget);
static void     gtk_calendar_grab_notify    (GtkWidget        *widget,
                                             gboolean          was_grabbed);
static void     gtk_calendar_state_flags_changed  (GtkWidget     *widget,
                                                   GtkStateFlags  previous_state);
static gboolean gtk_calendar_query_tooltip  (GtkWidget        *widget,
                                             gint              x,
                                             gint              y,
                                             gboolean          keyboard_mode,
                                             GtkTooltip       *tooltip);

static void     gtk_calendar_drag_data_get      (GtkWidget        *widget,
                                                 GdkDrag          *drag, 
                                                 GtkSelectionData *selection_data);
static void     gtk_calendar_drag_data_received (GtkWidget        *widget,
                                                 GdkDrop          *drop,
                                                 GtkSelectionData *selection_data);
static gboolean gtk_calendar_drag_motion        (GtkWidget        *widget,
                                                 GdkDrop          *drop,
                                                 gint              x,
                                                 gint              y);
static void     gtk_calendar_drag_leave         (GtkWidget        *widget,
                                                 GdkDrop          *drop);
static gboolean gtk_calendar_drag_drop          (GtkWidget        *widget,
                                                 GdkDrop          *drop,
                                                 gint              x,
                                                 gint              y);


static void calendar_start_spinning (GtkCalendar *calendar,
                                     gint         click_child);
static void calendar_stop_spinning  (GtkCalendar *calendar);

static void calendar_invalidate_day     (GtkCalendar *widget,
                                         gint       row,
                                         gint       col);
static void calendar_invalidate_day_num (GtkCalendar *widget,
                                         gint       day);

static void calendar_compute_days      (GtkCalendar *calendar);
static gint calendar_get_xsep          (GtkCalendar *calendar);
static gint calendar_get_ysep          (GtkCalendar *calendar);
static gint calendar_get_inner_border  (GtkCalendar *calendar);

static gboolean gtk_calendar_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                                       gdouble                   dx,
                                                       gdouble                   dy,
                                                       GtkWidget                *widget);

static char    *default_abbreviated_dayname[7];
static char    *default_monthname[12];

G_DEFINE_TYPE_WITH_PRIVATE (GtkCalendar, gtk_calendar, GTK_TYPE_WIDGET)

static void
gtk_calendar_class_init (GtkCalendarClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*)  class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_calendar_set_property;
  gobject_class->get_property = gtk_calendar_get_property;

  widget_class->destroy = gtk_calendar_destroy;
  widget_class->snapshot = gtk_calendar_snapshot;
  widget_class->measure = gtk_calendar_measure;
  widget_class->size_allocate = gtk_calendar_size_allocate;
  widget_class->state_flags_changed = gtk_calendar_state_flags_changed;
  widget_class->grab_notify = gtk_calendar_grab_notify;
  widget_class->query_tooltip = gtk_calendar_query_tooltip;

  widget_class->drag_data_get = gtk_calendar_drag_data_get;
  widget_class->drag_motion = gtk_calendar_drag_motion;
  widget_class->drag_leave = gtk_calendar_drag_leave;
  widget_class->drag_drop = gtk_calendar_drag_drop;
  widget_class->drag_data_received = gtk_calendar_drag_data_received;

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
                                                     0, G_MAXINT >> 9, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

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
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

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
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

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
 * GtkCalendar:no-month-change:
 *
 * Determines whether the selected month can be changed.
 */
  g_object_class_install_property (gobject_class,
                                   PROP_NO_MONTH_CHANGE,
                                   g_param_spec_boolean ("no-month-change",
                                                         P_("No Month Change"),
                                                         P_("If TRUE, the selected month cannot be changed"),
                                                         FALSE,
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
 * GtkCalendar:detail-width-chars:
 *
 * Width of a detail cell, in characters.
 * A value of 0 allows any width. See gtk_calendar_set_detail_func().
 */
  g_object_class_install_property (gobject_class,
                                   PROP_DETAIL_WIDTH_CHARS,
                                   g_param_spec_int ("detail-width-chars",
                                                     P_("Details Width"),
                                                     P_("Details width in characters"),
                                                     0, 127, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

/**
 * GtkCalendar:detail-height-rows:
 *
 * Height of a detail cell, in rows.
 * A value of 0 allows any width. See gtk_calendar_set_detail_func().
 */
  g_object_class_install_property (gobject_class,
                                   PROP_DETAIL_HEIGHT_ROWS,
                                   g_param_spec_int ("detail-height-rows",
                                                     P_("Details Height"),
                                                     P_("Details height in rows"),
                                                     0, 127, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

/**
 * GtkCalendar:show-details:
 *
 * Determines whether details are shown directly in the widget, or if they are
 * available only as tooltip. When this property is set days with details are
 * marked.
 */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_DETAILS,
                                   g_param_spec_boolean ("show-details",
                                                         P_("Show Details"),
                                                         P_("If TRUE, details are shown"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkCalendar::month-changed:
   * @calendar: the object which received the signal.
   *
   * Emitted when the user clicks a button to change the selected month on a
   * calendar.
   */
  gtk_calendar_signals[MONTH_CHANGED_SIGNAL] =
    g_signal_new (I_("month-changed"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, month_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

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
   * GtkCalendar::day-selected-double-click:
   * @calendar: the object which received the signal.
   *
   * Emitted when the user double-clicks a day.
   */
  gtk_calendar_signals[DAY_SELECTED_DOUBLE_CLICK_SIGNAL] =
    g_signal_new (I_("day-selected-double-click"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, day_selected_double_click),
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
  gtk_widget_class_set_css_name (widget_class, I_("calendar"));
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static void
gtk_calendar_init (GtkCalendar *calendar)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkEventController *controller;
  GtkGesture *gesture;
  time_t secs;
  struct tm *tm;
  gint i;
#ifdef G_OS_WIN32
  wchar_t wbuffer[100];
#else
  static const char *month_format = NULL;
  char buffer[255];
  time_t tmp_time;
#endif
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gchar *year_before;
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  union { unsigned int word; char *string; } langinfo;
  gint week_1stday = 0;
  gint first_weekday = 1;
  guint week_origin;
#else
  gchar *week_start;
#endif

  gtk_widget_set_can_focus (widget, TRUE);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (calendar)),
                               GTK_STYLE_CLASS_VIEW);

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
  g_signal_connect (controller, "focus-in",
                    G_CALLBACK (gtk_calendar_key_controller_focus),
                    calendar);
  g_signal_connect (controller, "focus-out",
                    G_CALLBACK (gtk_calendar_key_controller_focus),
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

  /* Set defaults */
  secs = time (NULL);
  tm = localtime (&secs);
  priv->month = tm->tm_mon;
  priv->year  = 1900 + tm->tm_year;

  for (i=0;i<31;i++)
    priv->marked_date[i] = FALSE;
  priv->num_marked_dates = 0;
  priv->selected_day = tm->tm_mday;

  priv->display_flags = (GTK_CALENDAR_SHOW_HEADING |
                             GTK_CALENDAR_SHOW_DAY_NAMES |
                             GTK_CALENDAR_SHOW_DETAILS);

  priv->focus_row = -1;
  priv->focus_col = -1;

  priv->max_year_width = 0;
  priv->max_month_width = 0;
  priv->max_day_char_width = 0;
  priv->max_week_char_width = 0;

  priv->max_day_char_ascent = 0;
  priv->max_day_char_descent = 0;
  priv->max_label_char_ascent = 0;
  priv->max_label_char_descent = 0;

  priv->arrow_width = 10;

  priv->need_timer = 0;
  priv->timer = 0;
  priv->click_child = -1;

  priv->in_drag = 0;
  priv->drag_highlight = 0;

  gtk_drag_dest_set (widget, 0, NULL, GDK_ACTION_COPY);
  gtk_drag_dest_add_text_targets (widget);

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

  calendar_compute_days (calendar);
}

#pragma GCC diagnostic pop

/****************************************
 *          Utility Functions           *
 ****************************************/

static void
calendar_queue_refresh (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  if (!(priv->detail_func) ||
      !(priv->display_flags & GTK_CALENDAR_SHOW_DETAILS) ||
       (priv->detail_width_chars && priv->detail_height_rows))
    gtk_widget_queue_draw (GTK_WIDGET (calendar));
  else
    gtk_widget_queue_resize (GTK_WIDGET (calendar));
}

static void
calendar_set_month_next (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint month_len;

  if (priv->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    return;

  if (priv->month == 11)
    {
      priv->month = 0;
      priv->year++;
    }
  else
    priv->month++;

  calendar_compute_days (calendar);
  g_signal_emit (calendar,
                 gtk_calendar_signals[NEXT_MONTH_SIGNAL],
                 0);
  g_signal_emit (calendar,
                 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
                 0);

  month_len = month_length[leap (priv->year)][priv->month + 1];

  if (month_len < priv->selected_day)
    {
      priv->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, priv->selected_day);

  calendar_queue_refresh (calendar);
}

static void
calendar_set_year_prev (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint month_len;

  priv->year--;
  calendar_compute_days (calendar);
  g_signal_emit (calendar,
                 gtk_calendar_signals[PREV_YEAR_SIGNAL],
                 0);
  g_signal_emit (calendar,
                 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
                 0);

  month_len = month_length[leap (priv->year)][priv->month + 1];

  if (month_len < priv->selected_day)
    {
      priv->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, priv->selected_day);

  calendar_queue_refresh (calendar);
}

static void
calendar_set_year_next (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint month_len;

  priv->year++;
  calendar_compute_days (calendar);
  g_signal_emit (calendar,
                 gtk_calendar_signals[NEXT_YEAR_SIGNAL],
                 0);
  g_signal_emit (calendar,
                 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
                 0);

  month_len = month_length[leap (priv->year)][priv->month + 1];

  if (month_len < priv->selected_day)
    {
      priv->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, priv->selected_day);

  calendar_queue_refresh (calendar);
}

static void
calendar_compute_days (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint month;
  gint year;
  gint ndays_in_month;
  gint ndays_in_prev_month;
  gint first_day;
  gint row;
  gint col;
  gint day;

  year = priv->year;
  month = priv->month + 1;

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
                               guint        day)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint old_focus_row = priv->focus_row;
  gint old_focus_col = priv->focus_col;
  gint row;
  gint col;

  for (row = 0; row < 6; row ++)
    for (col = 0; col < 7; col++)
      {
        if (priv->day_month[row][col] == MONTH_CURRENT
            && priv->day[row][col] == day)
          {
            priv->focus_row = row;
            priv->focus_col = col;
          }
      }

  if (old_focus_row != -1 && old_focus_col != -1)
    calendar_invalidate_day (calendar, old_focus_row, old_focus_col);

  gtk_calendar_select_day (calendar, day);
}


/****************************************
 *     Layout computation utilities     *
 ****************************************/

static gint
calendar_row_height (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  return (priv->main_h - CALENDAR_MARGIN
          - ((priv->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
             ? calendar_get_ysep (calendar) : CALENDAR_MARGIN)) / 6;
}

static void
get_component_paddings (GtkCalendar *calendar,
                        GtkBorder   *day_padding,
                        GtkBorder   *day_name_padding,
                        GtkBorder   *week_padding)
{
  GtkStyleContext * context;
  GtkWidget *widget;

  widget = GTK_WIDGET (calendar);
  context = gtk_widget_get_style_context (widget);

  if (day_padding)
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, I_("day-number"));
      gtk_style_context_get_padding (context, day_padding);
      gtk_style_context_restore (context);
    }

  if (day_name_padding)
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, I_("day-name"));
      gtk_style_context_get_padding (context, day_name_padding);
      gtk_style_context_restore (context);
    }

  if (week_padding)
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, I_("week-number"));
      gtk_style_context_get_padding (context, week_padding);
      gtk_style_context_restore (context);
    }
}

/* calendar_left_x_for_column: returns the x coordinate
 * for the left of the column */
static gint
calendar_left_x_for_column (GtkCalendar *calendar,
                            gint         column)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint width;
  gint x_left;
  gint week_width;
  gint calendar_xsep = calendar_get_xsep (calendar);
  gint inner_border = calendar_get_inner_border (calendar);

  get_component_paddings (calendar, NULL, NULL, NULL);

  week_width = priv->week_width + inner_border;

  if (gtk_widget_get_direction (GTK_WIDGET (calendar)) == GTK_TEXT_DIR_RTL)
    {
      column = 6 - column;
      week_width = 0;
    }

  width = priv->day_width;
  if (priv->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    x_left = week_width + calendar_xsep + (width + DAY_XSEP) * column;
  else
    x_left = week_width + CALENDAR_MARGIN + (width + DAY_XSEP) * column;

  return x_left;
}

/* column_from_x: returns the column 0-6 that the
 * x pixel of the xwindow is in */
static gint
calendar_column_from_x (GtkCalendar *calendar,
                        gint         event_x)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint c, column;
  gint x_left, x_right;

  column = -1;

  for (c = 0; c < 7; c++)
    {
      x_left = calendar_left_x_for_column (calendar, c);
      x_right = x_left + priv->day_width;

      if (event_x >= x_left && event_x < x_right)
        {
          column = c;
          break;
        }
    }

  return column;
}

/* calendar_top_y_for_row: returns the y coordinate
 * for the top of the row */
static gint
calendar_top_y_for_row (GtkCalendar *calendar,
                        gint         row)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint inner_border = calendar_get_inner_border (calendar);

  return priv->header_h + priv->day_name_h + inner_border
         + row * calendar_row_height (calendar);
}

/* row_from_y: returns the row 0-5 that the
 * y pixel of the xwindow is in */
static gint
calendar_row_from_y (GtkCalendar *calendar,
                     gint         event_y)
{
  gint r, row;
  gint height;
  gint y_top, y_bottom;

  height = calendar_row_height (calendar);
  row = -1;

  for (r = 0; r < 6; r++)
    {
      y_top = calendar_top_y_for_row (calendar, r);
      y_bottom = y_top + height;

      if (event_y >= y_top && event_y < y_bottom)
        {
          row = r;
          break;
        }
    }

  return row;
}

static void
calendar_arrow_rectangle (GtkCalendar  *calendar,
                          guint         arrow,
                          GdkRectangle *rect)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkWidget *widget = GTK_WIDGET (calendar);
  int width;
  gboolean year_left;

  width = gtk_widget_get_width (widget);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    year_left = priv->year_before;
  else
    year_left = !priv->year_before;

  rect->y = 3;
  rect->width = priv->arrow_width;
  rect->height = priv->header_h - 7;

  switch (arrow)
    {
    case ARROW_MONTH_LEFT:
      if (year_left)
        rect->x = (width - (3 + 2 * priv->arrow_width + priv->max_month_width));
      else
        rect->x = 3;
      break;
    case ARROW_MONTH_RIGHT:
      if (year_left)
        rect->x = width - 3 - priv->arrow_width;
      else
        rect->x = priv->arrow_width + priv->max_month_width;
      break;
    case ARROW_YEAR_LEFT:
      if (year_left)
        rect->x = 3;
      else
        rect->x = width - (3 + 2 * priv->arrow_width + priv->max_year_width);
      break;
    case ARROW_YEAR_RIGHT:
      if (year_left)
        rect->x = (priv->arrow_width + priv->max_year_width);
      else
        rect->x = width - 3 - priv->arrow_width;
      break;

    default:
      g_assert_not_reached();
    }
}

static void
calendar_day_rectangle (GtkCalendar  *calendar,
                        gint          row,
                        gint          col,
                        GdkRectangle *rect)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  rect->x = calendar_left_x_for_column (calendar, col);
  rect->y = calendar_top_y_for_row (calendar, row);
  rect->height = calendar_row_height (calendar);
  rect->width = priv->day_width;
}

static void
calendar_set_month_prev (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint month_len;

  if (priv->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    return;

  if (priv->month == 0)
    {
      priv->month = 11;
      priv->year--;
    }
  else
    priv->month--;

  month_len = month_length[leap (priv->year)][priv->month + 1];

  calendar_compute_days (calendar);

  g_signal_emit (calendar,
                 gtk_calendar_signals[PREV_MONTH_SIGNAL],
                 0);
  g_signal_emit (calendar,
                 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
                 0);

  if (month_len < priv->selected_day)
    {
      priv->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    {
      if (priv->selected_day < 0)
        priv->selected_day = priv->selected_day + 1 + month_length[leap (priv->year)][priv->month + 1];
      gtk_calendar_select_day (calendar, priv->selected_day);
    }

  calendar_queue_refresh (calendar);
}


/****************************************
 *           Basic object methods       *
 ****************************************/

static void
gtk_calendar_destroy (GtkWidget *widget)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  calendar_stop_spinning (calendar);

  /* Call the destroy function for the extra display callback: */
  if (priv->detail_func_destroy && priv->detail_func_user_data)
    {
      priv->detail_func_destroy (priv->detail_func_user_data);
      priv->detail_func_user_data = NULL;
      priv->detail_func_destroy = NULL;
    }

  GTK_WIDGET_CLASS (gtk_calendar_parent_class)->destroy (widget);
}

static gboolean
calendar_set_display_option (GtkCalendar              *calendar,
                             GtkCalendarDisplayOptions flag,
                             gboolean                  setting)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkCalendarDisplayOptions flags;
  gboolean old_setting;

  old_setting = (priv->display_flags & flag) != 0;
  if (old_setting == setting)
    return FALSE;

  if (setting)
    flags = priv->display_flags | flag;
  else
    flags = priv->display_flags & ~flag;

  gtk_calendar_set_display_options (calendar, flags);

  return TRUE;
}

static gboolean
calendar_get_display_option (GtkCalendar              *calendar,
                             GtkCalendarDisplayOptions flag)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  return (priv->display_flags & flag) != 0;
}

static void
gtk_calendar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkCalendar *calendar = GTK_CALENDAR (object);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  switch (prop_id)
    {
    case PROP_YEAR:
      gtk_calendar_select_month (calendar,
                                 priv->month,
                                 g_value_get_int (value));
      break;
    case PROP_MONTH:
      gtk_calendar_select_month (calendar,
                                 g_value_get_int (value),
                                 priv->year);
      break;
    case PROP_DAY:
      gtk_calendar_select_day (calendar,
                               g_value_get_int (value));
      break;
    case PROP_SHOW_HEADING:
      if (calendar_set_display_option (calendar,
                                       GTK_CALENDAR_SHOW_HEADING,
                                       g_value_get_boolean (value)))
        g_object_notify (object, "show-heading");
      break;
    case PROP_SHOW_DAY_NAMES:
      if (calendar_set_display_option (calendar,
                                       GTK_CALENDAR_SHOW_DAY_NAMES,
                                       g_value_get_boolean (value)))
        g_object_notify (object, "show-day-names");
      break;
    case PROP_NO_MONTH_CHANGE:
      if (calendar_set_display_option (calendar,
                                       GTK_CALENDAR_NO_MONTH_CHANGE,
                                       g_value_get_boolean (value)))
        g_object_notify (object, "no-month-change");
      break;
    case PROP_SHOW_WEEK_NUMBERS:
      if (calendar_set_display_option (calendar,
                                       GTK_CALENDAR_SHOW_WEEK_NUMBERS,
                                       g_value_get_boolean (value)))
        g_object_notify (object, "show-week-numbers");
      break;
    case PROP_SHOW_DETAILS:
      if (calendar_set_display_option (calendar,
                                       GTK_CALENDAR_SHOW_DETAILS,
                                       g_value_get_boolean (value)))
        g_object_notify (object, "show-details");
      break;
    case PROP_DETAIL_WIDTH_CHARS:
      gtk_calendar_set_detail_width_chars (calendar,
                                           g_value_get_int (value));
      break;
    case PROP_DETAIL_HEIGHT_ROWS:
      gtk_calendar_set_detail_height_rows (calendar,
                                           g_value_get_int (value));
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
      g_value_set_int (value, priv->year);
      break;
    case PROP_MONTH:
      g_value_set_int (value, priv->month);
      break;
    case PROP_DAY:
      g_value_set_int (value, priv->selected_day);
      break;
    case PROP_SHOW_HEADING:
      g_value_set_boolean (value, calendar_get_display_option (calendar,
                                                               GTK_CALENDAR_SHOW_HEADING));
      break;
    case PROP_SHOW_DAY_NAMES:
      g_value_set_boolean (value, calendar_get_display_option (calendar,
                                                               GTK_CALENDAR_SHOW_DAY_NAMES));
      break;
    case PROP_NO_MONTH_CHANGE:
      g_value_set_boolean (value, calendar_get_display_option (calendar,
                                                               GTK_CALENDAR_NO_MONTH_CHANGE));
      break;
    case PROP_SHOW_WEEK_NUMBERS:
      g_value_set_boolean (value, calendar_get_display_option (calendar,
                                                               GTK_CALENDAR_SHOW_WEEK_NUMBERS));
      break;
    case PROP_SHOW_DETAILS:
      g_value_set_boolean (value, calendar_get_display_option (calendar,
                                                               GTK_CALENDAR_SHOW_DETAILS));
      break;
    case PROP_DETAIL_WIDTH_CHARS:
      g_value_set_int (value, priv->detail_width_chars);
      break;
    case PROP_DETAIL_HEIGHT_ROWS:
      g_value_set_int (value, priv->detail_height_rows);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gint
calendar_get_inner_border (GtkCalendar *calendar)
{
  return 4;
}

static gint
calendar_get_xsep (GtkCalendar *calendar)
{
  return 4;
}

static gint
calendar_get_ysep (GtkCalendar *calendar)
{

  return 4;
}

static gchar*
gtk_calendar_get_detail (GtkCalendar *calendar,
                         gint         row,
                         gint         column)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint year, month;

  if (priv->detail_func == NULL)
    return NULL;

  year = priv->year;
  month = priv->month + priv->day_month[row][column] - MONTH_CURRENT;

  if (month < 0)
    {
      month += 12;
      year -= 1;
    }
  else if (month > 11)
    {
      month -= 12;
      year += 1;
    }

  return priv->detail_func (calendar,
                            year, month,
                            priv->day[row][column],
                            priv->detail_func_user_data);
}

static gboolean
gtk_calendar_query_tooltip (GtkWidget  *widget,
                            gint        x,
                            gint        y,
                            gboolean    keyboard_mode,
                            GtkTooltip *tooltip)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gchar *detail = NULL;
  GdkRectangle day_rect;
  gint row, col;

  col = calendar_column_from_x (calendar, x);
  row = calendar_row_from_y (calendar, y);

  if (col != -1 && row != -1 &&
      (0 != (priv->detail_overflow[row] & (1 << col)) ||
      0 == (priv->display_flags & GTK_CALENDAR_SHOW_DETAILS)))
    {
      detail = gtk_calendar_get_detail (calendar, row, col);
      calendar_day_rectangle (calendar, row, col, &day_rect);
    }

  if (detail)
    {
      gtk_tooltip_set_tip_area (tooltip, &day_rect);
      gtk_tooltip_set_markup (tooltip, detail);

      g_free (detail);

      return TRUE;
    }

  if (GTK_WIDGET_CLASS (gtk_calendar_parent_class)->query_tooltip)
    return GTK_WIDGET_CLASS (gtk_calendar_parent_class)->query_tooltip (widget, x, y, keyboard_mode, tooltip);

  return FALSE;
}

/****************************************
 *       Size Request and Allocate      *
 ****************************************/

static void
gtk_calendar_size_request (GtkWidget      *widget,
                           GtkRequisition *requisition)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkBorder day_padding, day_name_padding, week_padding;
  PangoLayout *layout;
  PangoRectangle logical_rect;

  gint height;
  gint i, r, c;
  gint calendar_margin = CALENDAR_MARGIN;
  gint header_width, main_width;
  gint max_header_height = 0;
  gint max_detail_height;
  gint inner_border = calendar_get_inner_border (calendar);
  gint calendar_ysep = calendar_get_ysep (calendar);
  gint calendar_xsep = calendar_get_xsep (calendar);

  gboolean show_week_numbers = (priv->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS);

  layout = gtk_widget_create_pango_layout (widget, NULL);

  /*
   * Calculate the requisition  width for the widget.
   */

  /* Header width */

  if (priv->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      priv->max_month_width = 0;
      for (i = 0; i < 12; i++)
        {
          pango_layout_set_text (layout, default_monthname[i], -1);
          pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
          priv->max_month_width = MAX (priv->max_month_width,
                                               logical_rect.width + 8);
          max_header_height = MAX (max_header_height, logical_rect.height);
        }

      priv->max_year_width = 0;
      /* Translators:  This is a text measurement template.
       * Translate it to the widest year text
       *
       * If you don't understand this, leave it as "2000"
       */
      pango_layout_set_text (layout, C_("year measurement template", "2000"), -1);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      priv->max_year_width = MAX (priv->max_year_width,
                                  logical_rect.width + 8);
      max_header_height = MAX (max_header_height, logical_rect.height);
    }
  else
    {
      priv->max_month_width = 0;
      priv->max_year_width = 0;
    }

  if (priv->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    header_width = (priv->max_month_width
                    + priv->max_year_width
                    + 3 * 3);
  else
    header_width = (priv->max_month_width
                    + priv->max_year_width
                    + 4 * priv->arrow_width + 3 * 3);

  /* Mainwindow labels width */

  priv->max_day_char_width = 0;
  priv->max_day_char_ascent = 0;
  priv->max_day_char_descent = 0;
  priv->min_day_width = 0;

  for (i = 0; i < 9; i++)
    {
      gchar buffer[32];
      g_snprintf (buffer, sizeof (buffer), C_("calendar:day:digits", "%d"), i * 11);
      pango_layout_set_text (layout, buffer, -1);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      priv->min_day_width = MAX (priv->min_day_width,
                                         logical_rect.width);

      priv->max_day_char_ascent = MAX (priv->max_day_char_ascent,
                                               PANGO_ASCENT (logical_rect));
      priv->max_day_char_descent = MAX (priv->max_day_char_descent,
                                                PANGO_DESCENT (logical_rect));
    }

  priv->max_label_char_ascent = 0;
  priv->max_label_char_descent = 0;
  if (priv->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    for (i = 0; i < 7; i++)
      {
        pango_layout_set_text (layout, default_abbreviated_dayname[i], -1);
        pango_layout_line_get_pixel_extents (pango_layout_get_lines_readonly (layout)->data, NULL, &logical_rect);

        priv->min_day_width = MAX (priv->min_day_width, logical_rect.width);
        priv->max_label_char_ascent = MAX (priv->max_label_char_ascent,
                                                   PANGO_ASCENT (logical_rect));
        priv->max_label_char_descent = MAX (priv->max_label_char_descent,
                                                    PANGO_DESCENT (logical_rect));
      }

  priv->max_week_char_width = 0;
  if (show_week_numbers)
    for (i = 0; i < 9; i++)
      {
        gchar buffer[32];
        g_snprintf (buffer, sizeof (buffer), C_("calendar:week:digits", "%d"), i * 11);
        pango_layout_set_text (layout, buffer, -1);
        pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
        priv->max_week_char_width = MAX (priv->max_week_char_width,
                                           logical_rect.width / 2);
      }

  /* Calculate detail extents. Do this as late as possible since
   * pango_layout_set_markup is called which alters font settings. */
  max_detail_height = 0;

  if (priv->detail_func && (priv->display_flags & GTK_CALENDAR_SHOW_DETAILS))
    {
      gchar *markup, *tail;

      if (priv->detail_width_chars || priv->detail_height_rows)
        {
          gint rows = MAX (1, priv->detail_height_rows) - 1;
          gsize len = priv->detail_width_chars + rows + 16;

          markup = tail = g_alloca (len);

          memcpy (tail,     "<small>", 7);
          tail += 7;

          memset (tail, 'm', priv->detail_width_chars);
          tail += priv->detail_width_chars;

          memset (tail, '\n', rows);
          tail += rows;

          memcpy (tail,     "</small>", 9);
          tail += 9;

          g_assert (len == (tail - markup));

          pango_layout_set_markup (layout, markup, -1);
          pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

          if (priv->detail_width_chars)
            priv->min_day_width = MAX (priv->min_day_width, logical_rect.width);
          if (priv->detail_height_rows)
            max_detail_height = MAX (max_detail_height, logical_rect.height);
        }

      if (!priv->detail_width_chars || !priv->detail_height_rows)
        for (r = 0; r < 6; r++)
          for (c = 0; c < 7; c++)
            {
              gchar *detail = gtk_calendar_get_detail (calendar, r, c);

              if (detail)
                {
                  markup = g_strconcat ("<small>", detail, "</small>", NULL);
                  pango_layout_set_markup (layout, markup, -1);

                  if (priv->detail_width_chars)
                    {
                      pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
                      pango_layout_set_width (layout, PANGO_SCALE * priv->min_day_width);
                    }

                  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

                  if (!priv->detail_width_chars)
                    priv->min_day_width = MAX (priv->min_day_width, logical_rect.width);
                  if (!priv->detail_height_rows)
                    max_detail_height = MAX (max_detail_height, logical_rect.height);

                  g_free (markup);
                  g_free (detail);
                }
            }
    }

  get_component_paddings (calendar, &day_padding, &day_name_padding, &week_padding);

  priv->min_day_width += day_padding.left + day_padding.right;
  if (show_week_numbers)
    priv->max_week_char_width += week_padding.left + week_padding.right;

  /* We add one to max_day_char_width to be able to make the marked day "bold" */
  priv->max_day_char_width = priv->min_day_width / 2 + 1;

  main_width = (7 * (priv->min_day_width) + (DAY_XSEP * 6) + CALENDAR_MARGIN * 2
                + (show_week_numbers
                   ? priv->max_week_char_width * 2 + calendar_xsep * 2
                   : 0));

  requisition->width = MAX (header_width, main_width + inner_border * 2);

  /*
   * Calculate the requisition height for the widget.
   */

  if (priv->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      priv->header_h = (max_header_height + calendar_ysep * 2);
    }
  else
    {
      priv->header_h = 0;
    }

  if (priv->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    {
      priv->day_name_h = (priv->max_label_char_ascent
                          + priv->max_label_char_descent
                          + day_name_padding.top + day_name_padding.bottom
                          + calendar_margin);
      calendar_margin = calendar_ysep;
    }
  else
    {
      priv->day_name_h = 0;
    }

  priv->main_h = (CALENDAR_MARGIN + calendar_margin
                          + 6 * (priv->max_day_char_ascent
                                 + priv->max_day_char_descent
                                 + max_detail_height
                                 + day_padding.top + day_padding.bottom)
                          + DAY_YSEP * 5);

  height = priv->header_h + priv->day_name_h + priv->main_h;

  requisition->height = height + (inner_border * 2);

  g_object_unref (layout);
}

static void
gtk_calendar_measure (GtkWidget        *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkRequisition requisition;

  gtk_calendar_size_request (widget, &requisition);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = requisition.width;
  else /* VERTICAL */
    *minimum = *natural = requisition.height;
}

static void
gtk_calendar_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint inner_border = calendar_get_inner_border (calendar);
  gint calendar_xsep = calendar_get_xsep (calendar);

  if (priv->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    {
      priv->day_width = (priv->min_day_width
                         * ((width - (inner_border * 2)
                             - (CALENDAR_MARGIN * 2) -  (DAY_XSEP * 6) - calendar_xsep * 2))
                         / (7 * priv->min_day_width + priv->max_week_char_width * 2));
      priv->week_width = ((width - (inner_border * 2)
                           - (CALENDAR_MARGIN * 2) - (DAY_XSEP * 6) - calendar_xsep * 2 )
                          - priv->day_width * 7 + CALENDAR_MARGIN + calendar_xsep);
    }
  else
    {
      priv->day_width = (width
                         - (inner_border * 2)
                         - (CALENDAR_MARGIN * 2)
                         - (DAY_XSEP * 6))/7;
      priv->week_width = 0;
    }
}


/****************************************
 *              Repainting              *
 ****************************************/

static void
calendar_snapshot_header (GtkCalendar *calendar,
                          GtkSnapshot *snapshot)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkStyleContext *context;
  GtkStateFlags state;
  char buffer[255];
  gint x, y;
  gint header_width;
  gint max_month_width;
  gint max_year_width;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gboolean year_left;
  time_t tmp_time;
  struct tm *tm;
  gchar *str;

  context = gtk_widget_get_style_context (widget);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    year_left = priv->year_before;
  else
    year_left = !priv->year_before;

  header_width = gtk_widget_get_width (widget);

  max_month_width = priv->max_month_width;
  max_year_width = priv->max_year_width;

  state = gtk_style_context_get_state (context);
  state &= ~GTK_STATE_FLAG_DROP_ACTIVE;

  gtk_style_context_save (context);

  gtk_style_context_set_state (context, state);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_HEADER);

  gtk_snapshot_render_background (snapshot, context, 0, 0, header_width, priv->header_h);
  gtk_snapshot_render_frame (snapshot, context, 0, 0, header_width, priv->header_h);

  tmp_time = 1;  /* Jan 1 1970, 00:00:01 UTC */
  tm = gmtime (&tmp_time);
  tm->tm_year = priv->year - 1900;

  /* Translators: This dictates how the year is displayed in
   * gtkcalendar widget.  See strftime() manual for the format.
   * Use only ASCII in the translation.
   *
   * Also look for the msgid "2000".
   * Translate that entry to a year with the widest output of this
   * msgid.
   *
   * "%Y" is appropriate for most locales.
   */
  strftime (buffer, sizeof (buffer), C_("calendar year format", "%Y"), tm);
  str = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
  layout = gtk_widget_create_pango_layout (widget, str);
  g_free (str);

  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

  /* Draw title */
  y = (priv->header_h - logical_rect.height) / 2;

  /* Draw year and its arrows */

  if (priv->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    if (year_left)
      x = 3 + (max_year_width - logical_rect.width)/2;
    else
      x = header_width - (3 + max_year_width
                          - (max_year_width - logical_rect.width)/2);
  else
    if (year_left)
      x = 3 + priv->arrow_width + (max_year_width - logical_rect.width)/2;
    else
      x = header_width - (3 + priv->arrow_width + max_year_width
                          - (max_year_width - logical_rect.width)/2);

  gtk_snapshot_render_layout (snapshot, context, x, y, layout);

  /* Draw month */
  g_snprintf (buffer, sizeof (buffer), "%s", default_monthname[priv->month]);
  pango_layout_set_text (layout, buffer, -1);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

  if (priv->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    if (year_left)
      x = header_width - (3 + max_month_width
                          - (max_month_width - logical_rect.width)/2);
    else
      x = 3 + (max_month_width - logical_rect.width) / 2;
  else
    if (year_left)
      x = header_width - (3 + priv->arrow_width + max_month_width
                          - (max_month_width - logical_rect.width)/2);
    else
      x = 3 + priv->arrow_width + (max_month_width - logical_rect.width)/2;

  gtk_snapshot_render_layout (snapshot, context, x, y, layout);
  g_object_unref (layout);

  gtk_style_context_restore (context);
}

static void
calendar_snapshot_day_names (GtkCalendar *calendar,
                             GtkSnapshot *snapshot)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder day_name_padding;
  char buffer[255];
  int day,i;
  int day_width, cal_width;
  int day_wid_sep;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint calendar_ysep = calendar_get_ysep (calendar);
  gint calendar_xsep = calendar_get_xsep (calendar);
  gint inner_border = calendar_get_inner_border (calendar);

  get_component_paddings (calendar, NULL, &day_name_padding, NULL);
  context = gtk_widget_get_style_context (widget);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (inner_border, priv->header_h + inner_border));

  day_width = priv->day_width;
  cal_width = gtk_widget_get_width (widget) - (inner_border * 2);
  day_wid_sep = day_width + DAY_XSEP;

  /*
   * Draw rectangles as inverted background for the labels.
   */

  state = gtk_style_context_get_state (context);
  state &= ~GTK_STATE_FLAG_DROP_ACTIVE;

  gtk_style_context_save (context);

  gtk_style_context_set_state (context, state);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_HIGHLIGHT);

  gtk_snapshot_render_background (snapshot, context,
                                  CALENDAR_MARGIN, CALENDAR_MARGIN,
                                  cal_width - CALENDAR_MARGIN * 2,
                                  priv->day_name_h - CALENDAR_MARGIN);

  if (priv->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    gtk_snapshot_render_background (snapshot, context,
                                    CALENDAR_MARGIN,
                                    priv->day_name_h - calendar_ysep,
                                    priv->week_width - calendar_ysep - CALENDAR_MARGIN,
                                    calendar_ysep);

  /*
   * Write the labels
   */
  layout = gtk_widget_create_pango_layout (widget, NULL);

  for (i = 0; i < 7; i++)
    {
      if (gtk_widget_get_direction (GTK_WIDGET (calendar)) == GTK_TEXT_DIR_RTL)
        day = 6 - i;
      else
        day = i;
      day = (day + priv->week_start) % 7;
      g_snprintf (buffer, sizeof (buffer), "%s", default_abbreviated_dayname[day]);

      pango_layout_set_text (layout, buffer, -1);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      gtk_snapshot_render_layout (snapshot, context,
                                  (CALENDAR_MARGIN +
                                   + (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR ?
                                      (priv->week_width + (priv->week_width ? calendar_xsep : 0))
                                      : 0)
                                   + day_wid_sep * i
                                   + (day_width - logical_rect.width)/2),
                                  CALENDAR_MARGIN + day_name_padding.top + logical_rect.y,
                                  layout);
    }

  g_object_unref (layout);

  gtk_style_context_restore (context);
  gtk_snapshot_restore (snapshot);
}

static void
calendar_snapshot_week_numbers (GtkCalendar *calendar,
                                GtkSnapshot *snapshot)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder week_padding;
  gint row, x_loc, y_loc;
  gint day_height;
  char buffer[32];
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint calendar_xsep = calendar_get_xsep (calendar);
  gint inner_border = calendar_get_inner_border (calendar);
  gint x, y;

  get_component_paddings (calendar, NULL, NULL, &week_padding);
  context = gtk_widget_get_style_context (widget);

  y = priv->header_h + priv->day_name_h + inner_border;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    x = inner_border;
  else
    x = gtk_widget_get_width (widget) - priv->week_width - inner_border;

  state = gtk_style_context_get_state (context);
  state &= ~GTK_STATE_FLAG_DROP_ACTIVE;

  gtk_style_context_save (context);

  gtk_style_context_set_state (context, state);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_HIGHLIGHT);

  if (priv->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    gtk_snapshot_render_background (snapshot, context,
                                    x + CALENDAR_MARGIN, y,
                                    priv->week_width - CALENDAR_MARGIN,
                                    priv->main_h - CALENDAR_MARGIN);
  else
    gtk_snapshot_render_background (snapshot, context,
                                    x + CALENDAR_MARGIN,
                                    y + CALENDAR_MARGIN,
                                    priv->week_width - CALENDAR_MARGIN,
                                    priv->main_h - 2 * CALENDAR_MARGIN);

  /*
   * Write the labels
   */

  layout = gtk_widget_create_pango_layout (widget, NULL);
  day_height = calendar_row_height (calendar);

  for (row = 0; row < 6; row++)
    {
      gint year, month, week;

      year = priv->year;
      month = priv->month + priv->day_month[row][6] - MONTH_CURRENT;

      if (month < 0)
        {
          month += 12;
          year -= 1;
        }
      else if (month > 11)
        {
          month -= 12;
          year += 1;
        }

      month += 1;

      week = week_of_year (year, month, priv->day[row][6]);

      /* Translators: this defines whether the week numbers should use
       * localized digits or the ones used in English (0123...).
       *
       * Translate to "%Id" if you want to use localized digits, or
       * translate to "%d" otherwise.
       *
       * Note that translating this doesn't guarantee that you get localized
       * digits. That needs support from your system and locale definition
       * too.
       */
      g_snprintf (buffer, sizeof (buffer), C_("calendar:week:digits", "%d"), week);
      pango_layout_set_text (layout, buffer, -1);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      y_loc = calendar_top_y_for_row (calendar, row) + (day_height - logical_rect.height) / 2;

      x_loc = x + (priv->week_width
                   - logical_rect.width
                   - calendar_xsep - week_padding.right);

      gtk_snapshot_render_layout (snapshot, context, x_loc, y_loc, layout);
    }

  g_object_unref (layout);

  gtk_style_context_restore (context);
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

static gboolean
is_color_attribute (PangoAttribute *attribute,
                    gpointer        data)
{
  return (attribute->klass->type == PANGO_ATTR_FOREGROUND ||
          attribute->klass->type == PANGO_ATTR_BACKGROUND);
}

static void
calendar_snapshot_day (GtkCalendar *calendar,
                       GtkSnapshot *snapshot,
                       gint         row,
                       gint         col)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkStyleContext *context;
  GtkStateFlags state = 0;
  gchar *detail;
  gchar buffer[32];
  gint day;
  gint x_loc, y_loc;
  GdkRectangle day_rect;

  PangoLayout *layout;
  PangoRectangle logical_rect;
  gboolean overflow = FALSE;
  gboolean show_details;

  g_return_if_fail (row < 6);
  g_return_if_fail (col < 7);

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  day = priv->day[row][col];
  show_details = (priv->display_flags & GTK_CALENDAR_SHOW_DETAILS);

  calendar_day_rectangle (calendar, row, col, &day_rect);

  gtk_style_context_save (context);

  state &= ~(GTK_STATE_FLAG_INCONSISTENT | GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_DROP_ACTIVE);

  if (priv->day_month[row][col] == MONTH_PREV ||
      priv->day_month[row][col] == MONTH_NEXT)
    state |= GTK_STATE_FLAG_INCONSISTENT;
  else
    {
      if (priv->marked_date[day-1])
        state |= GTK_STATE_FLAG_ACTIVE;

      if (priv->selected_day == day)
        {
          state |= GTK_STATE_FLAG_SELECTED;

          gtk_style_context_set_state (context, state);
          gtk_snapshot_render_background (snapshot, context,
                                          day_rect.x, day_rect.y,
                                          day_rect.width, day_rect.height);
        }
    }

  gtk_style_context_set_state (context, state);

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

  /* Get extra information to show, if any: */

  detail = gtk_calendar_get_detail (calendar, row, col);

  layout = gtk_widget_create_pango_layout (widget, buffer);
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

  x_loc = day_rect.x + (day_rect.width - logical_rect.width) / 2;
  y_loc = day_rect.y;

  gtk_snapshot_render_layout (snapshot, context, x_loc, y_loc, layout);

  if (priv->day_month[row][col] == MONTH_CURRENT &&
     (priv->marked_date[day-1] || (detail && !show_details)))
    gtk_snapshot_render_layout (snapshot, context, x_loc - 1, y_loc, layout);

  y_loc += priv->max_day_char_descent;

  if (priv->detail_func && show_details)
    {
      GdkRGBA color;

      gtk_style_context_get_color (context, &color);

      gtk_snapshot_append_color (snapshot,
                                 &color,
                                 &GRAPHENE_RECT_INIT (
                                     day_rect.x + 2, y_loc,
                                     day_rect.width - 2, 1
                                 ));

      y_loc += 2;
    }

  if (detail && show_details)
    {
      gchar *markup = g_strconcat ("<small>", detail, "</small>", NULL);
      pango_layout_set_markup (layout, markup, -1);
      g_free (markup);

      if (day == priv->selected_day)
        {
          /* Stripping colors as they conflict with selection marking. */

          PangoAttrList *attrs = pango_layout_get_attributes (layout);
          PangoAttrList *colors = NULL;

          if (attrs)
            colors = pango_attr_list_filter (attrs, is_color_attribute, NULL);
          if (colors)
            pango_attr_list_unref (colors);
        }

      pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
      pango_layout_set_width (layout, PANGO_SCALE * day_rect.width);

      if (priv->detail_height_rows)
        {
          gint dy = day_rect.height - (y_loc - day_rect.y);
          pango_layout_set_height (layout, PANGO_SCALE * dy);
          pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
        }

      gtk_snapshot_render_layout (snapshot, context, day_rect.x, y_loc, layout);
    }

  if (gtk_widget_has_visible_focus (widget) &&
      priv->focus_row == row && priv->focus_col == col)
    gtk_snapshot_render_focus (snapshot, context,
                               day_rect.x, day_rect.y,
                               day_rect.width, day_rect.height);

  if (overflow)
    priv->detail_overflow[row] |= (1 << col);
  else
    priv->detail_overflow[row] &= ~(1 << col);

  gtk_style_context_restore (context);
  g_object_unref (layout);
  g_free (detail);
}

static void
calendar_snapshot_main (GtkCalendar *calendar,
                        GtkSnapshot *snapshot)
{
  gint row, col;

  for (col = 0; col < 7; col++)
    for (row = 0; row < 6; row++)
      calendar_snapshot_day (calendar, snapshot, row, col);
}

static void
calendar_snapshot_arrow (GtkCalendar *calendar,
                         GtkSnapshot *snapshot,
                         guint        arrow)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GtkCssImageBuiltinType image_type;
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRectangle rect;

  calendar_arrow_rectangle (calendar, arrow, &rect);

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  if (priv->arrow_prelight & (1 << arrow))
    state |= GTK_STATE_FLAG_PRELIGHT;
  else
    state &= ~(GTK_STATE_FLAG_PRELIGHT);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);

  gtk_snapshot_render_background (snapshot, context,
                                  rect.x, rect.y,
                                  rect.width, rect.height);

  if (arrow == ARROW_MONTH_LEFT || arrow == ARROW_YEAR_LEFT)
    image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT;
  else
    image_type = GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT(
                          rect.x + (rect.width - 8) / 2,
                          rect.y + (rect.height - 8) / 2));
  gtk_css_style_snapshot_icon (gtk_style_context_lookup_style (context),
                               snapshot,
                               8, 8,
                               image_type);
  gtk_snapshot_restore (snapshot);

  gtk_style_context_restore (context);
}

static void
gtk_calendar_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  int i;

  calendar_snapshot_main (calendar, snapshot);

  if (priv->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      calendar_snapshot_header (calendar, snapshot);
      for (i = 0; i < 4; i++)
        calendar_snapshot_arrow (calendar, snapshot, i);
    }

  if (priv->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    calendar_snapshot_day_names (calendar, snapshot);

  if (priv->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    calendar_snapshot_week_numbers (calendar, snapshot);
}


/****************************************
 *           Mouse handling             *
 ****************************************/

static void
calendar_arrow_action (GtkCalendar *calendar,
                       guint        arrow)
{
  switch (arrow)
    {
    case ARROW_YEAR_LEFT:
      calendar_set_year_prev (calendar);
      break;
    case ARROW_YEAR_RIGHT:
      calendar_set_year_next (calendar);
      break;
    case ARROW_MONTH_LEFT:
      calendar_set_month_prev (calendar);
      break;
    case ARROW_MONTH_RIGHT:
      calendar_set_month_next (calendar);
      break;
    default:;
      /* do nothing */
    }
}

static gboolean
calendar_timer (gpointer data)
{
  GtkCalendar *calendar = data;
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gboolean retval = FALSE;

  if (priv->timer)
    {
      calendar_arrow_action (calendar, priv->click_child);

      if (priv->need_timer)
        {
          priv->need_timer = FALSE;
          priv->timer = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
                                            TIMEOUT_REPEAT * SCROLL_DELAY_FACTOR,
                                            (GSourceFunc) calendar_timer,
                                            calendar, NULL);
          g_source_set_name_by_id (priv->timer, "[gtk] calendar_timer");
        }
      else
        retval = TRUE;
    }

  return retval;
}

static void
calendar_start_spinning (GtkCalendar *calendar,
                         gint         click_child)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  priv->click_child = click_child;

  if (!priv->timer)
    {
      priv->need_timer = TRUE;
      priv->timer = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
                                        TIMEOUT_INITIAL,
                                        (GSourceFunc) calendar_timer,
                                        calendar, NULL);
      g_source_set_name_by_id (priv->timer, "[gtk] calendar_timer");
    }
}

static void
calendar_stop_spinning (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  if (priv->timer)
    {
      g_source_remove (priv->timer);
      priv->timer = 0;
      priv->need_timer = FALSE;
    }
}

static void
calendar_main_button_press (GtkCalendar *calendar,
                            double       x,
                            double       y,
                            int          n_press,
                            int          button)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint row, col;
  gint day_month;
  gint day;

  row = calendar_row_from_y (calendar, y);
  col = calendar_column_from_x (calendar, x);

  /* If row or column isn't found, just return. */
  if (row == -1 || col == -1)
    return;

  day_month = priv->day_month[row][col];

  if (n_press == 1)
    {
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
  else if (n_press == 2)
    {
      priv->in_drag = 0;
      if (day_month == MONTH_CURRENT)
        g_signal_emit (calendar,
                       gtk_calendar_signals[DAY_SELECTED_DOUBLE_CLICK_SIGNAL],
                       0);
    }
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
  gint arrow = -1;

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (y > priv->header_h)
    calendar_main_button_press (calendar, x, y, n_press, button);

  for (arrow = ARROW_YEAR_LEFT; arrow <= ARROW_MONTH_RIGHT; arrow++)
    {
      GdkRectangle arrow_rect;

      calendar_arrow_rectangle (calendar, arrow, &arrow_rect);

      if (gdk_rectangle_contains_point (&arrow_rect, (int)x, (int)y))
        {
          if (button == GDK_BUTTON_PRIMARY)
            calendar_start_spinning (calendar, arrow);

          calendar_arrow_action (calendar, arrow);

          return;
        }
    }
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
      calendar_stop_spinning (calendar);

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
  GdkDrag *drag;
  GdkContentFormats *targets;

  if (!priv->in_drag)
    return;

  if (!gtk_drag_check_threshold (widget, 0, 0, x, y))
    return;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

  gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));

  targets = gdk_content_formats_new (NULL, 0);
  targets = gtk_content_formats_add_text_targets (targets);
  drag = gtk_drag_begin (widget,
                         gtk_gesture_get_device (GTK_GESTURE (gesture)),
                         targets, GDK_ACTION_COPY,
                         start_x, start_y);

  priv->in_drag = 0;
  gdk_content_formats_unref (targets);

  gtk_drag_set_icon_default (drag);
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
gtk_calendar_key_controller_focus (GtkEventControllerKey *key,
                                   GdkCrossingMode        mode,
                                   GdkNotifyType          detail,
                                   GtkWidget             *widget)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  calendar_queue_refresh (calendar);
  calendar_stop_spinning (calendar);
  priv->in_drag = 0;
}


/****************************************
 *           Misc widget methods        *
 ****************************************/

static void
gtk_calendar_state_flags_changed (GtkWidget     *widget,
                                  GtkStateFlags  previous_state)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  if (!gtk_widget_is_sensitive (widget))
    {
      priv->in_drag = 0;
      calendar_stop_spinning (calendar);
    }
}

static void
gtk_calendar_grab_notify (GtkWidget *widget,
                          gboolean   was_grabbed)
{
  GTK_WIDGET_CLASS (gtk_calendar_parent_class)->grab_notify (widget, was_grabbed);

  if (!was_grabbed)
    calendar_stop_spinning (GTK_CALENDAR (widget));
}


/****************************************
 *          Drag and Drop               *
 ****************************************/

static void
gtk_calendar_drag_data_get (GtkWidget        *widget,
                            GdkDrag          *drag,
                            GtkSelectionData *selection_data)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GDate *date;
  gchar str[128];
  gsize len;

  date = g_date_new_dmy (priv->selected_day, priv->month + 1, priv->year);
  len = g_date_strftime (str, 127, "%x", date);
  gtk_selection_data_set_text (selection_data, str, len);

  g_free (date);
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
gtk_calendar_drag_leave (GtkWidget *widget,
                         GdkDrop   *drop)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  priv->drag_highlight = 0;
  gtk_drag_unhighlight (widget);
}

static gboolean
gtk_calendar_drag_motion (GtkWidget      *widget,
                          GdkDrop        *drop,
                          gint            x,
                          gint            y)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  GdkAtom target;

  if (!priv->drag_highlight)
    {
      priv->drag_highlight = 1;
      gtk_drag_highlight (widget);
    }

  target = gtk_drag_dest_find_target (widget, drop, NULL);
  if (target == NULL || gdk_drop_get_actions (drop) == 0)
    gdk_drop_status (drop, 0);
  else if (get_status_pending (drop) == 0)
    {
      set_status_pending (drop, gdk_drop_get_actions (drop));
      gtk_drag_get_data (widget, drop, target);
    }

  return TRUE;
}

static gboolean
gtk_calendar_drag_drop (GtkWidget      *widget,
                        GdkDrop        *drop,
                        gint            x,
                        gint            y)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (widget, drop, NULL);
  if (target != NULL)
    {
      gtk_drag_get_data (widget, drop, target);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_calendar_drag_data_received (GtkWidget        *widget,
                                 GdkDrop          *drop,
                                 GtkSelectionData *selection_data)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  guint day, month, year;
  gchar *str;
  GDate *date;
  GdkDragAction suggested_action;

  suggested_action = get_status_pending (drop);

  if (suggested_action)
    {
      set_status_pending (drop, 0);

      /* We are getting this data due to a request in drag_motion,
       * rather than due to a request in drag_drop, so we are just
       * supposed to call drag_status, not actually paste in the
       * data.
       */
      str = (gchar*) gtk_selection_data_get_text (selection_data);

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

      return;
    }

  date = g_date_new ();
  str = (gchar*) gtk_selection_data_get_text (selection_data);
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
      return;
    }

  day = g_date_get_day (date);
  month = g_date_get_month (date);
  year = g_date_get_year (date);
  g_date_free (date);

  gdk_drop_finish (drop, suggested_action);


  g_object_freeze_notify (G_OBJECT (calendar));
  if (!(priv->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
      && (priv->display_flags & GTK_CALENDAR_SHOW_HEADING))
    gtk_calendar_select_month (calendar, month - 1, year);
  gtk_calendar_select_day (calendar, day);
  g_object_thaw_notify (G_OBJECT (calendar));
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
 * gtk_calendar_get_display_options:
 * @calendar: a #GtkCalendar
 *
 * Returns the current display options of @calendar.
 *
 * Returns: the display options.
 **/
GtkCalendarDisplayOptions
gtk_calendar_get_display_options (GtkCalendar         *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), 0);

  return priv->display_flags;
}

/**
 * gtk_calendar_set_display_options:
 * @calendar: a #GtkCalendar
 * @flags: the display options to set
 *
 * Sets display options (whether to display the heading and the month
 * headings).
 **/
void
gtk_calendar_set_display_options (GtkCalendar          *calendar,
                                  GtkCalendarDisplayOptions flags)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);
  gint resize = 0;
  GtkCalendarDisplayOptions old_flags;

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  old_flags = priv->display_flags;

  if (gtk_widget_get_realized (widget))
    {
      if ((flags ^ priv->display_flags) & GTK_CALENDAR_NO_MONTH_CHANGE)
        {
          resize ++;
          if (! (flags & GTK_CALENDAR_NO_MONTH_CHANGE)
              && (priv->display_flags & GTK_CALENDAR_SHOW_HEADING))
            {
              priv->display_flags &= ~GTK_CALENDAR_NO_MONTH_CHANGE;
            }
        }

      if ((flags ^ priv->display_flags) & GTK_CALENDAR_SHOW_HEADING)
        {
          resize++;

          if (flags & GTK_CALENDAR_SHOW_HEADING)
            {
              priv->display_flags |= GTK_CALENDAR_SHOW_HEADING;
            }
        }

      if ((flags ^ priv->display_flags) & GTK_CALENDAR_SHOW_DAY_NAMES)
        {
          resize++;

          if (flags & GTK_CALENDAR_SHOW_DAY_NAMES)
            priv->display_flags |= GTK_CALENDAR_SHOW_DAY_NAMES;
        }

      if ((flags ^ priv->display_flags) & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
        {
          resize++;

          if (flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
            priv->display_flags |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
        }

      if ((flags ^ priv->display_flags) & GTK_CALENDAR_SHOW_DETAILS)
        resize++;

      priv->display_flags = flags;
      if (resize)
        gtk_widget_queue_resize (GTK_WIDGET (calendar));
    }
  else
    priv->display_flags = flags;

  g_object_freeze_notify (G_OBJECT (calendar));
  if ((old_flags ^ priv->display_flags) & GTK_CALENDAR_SHOW_HEADING)
    g_object_notify (G_OBJECT (calendar), "show-heading");
  if ((old_flags ^ priv->display_flags) & GTK_CALENDAR_SHOW_DAY_NAMES)
    g_object_notify (G_OBJECT (calendar), "show-day-names");
  if ((old_flags ^ priv->display_flags) & GTK_CALENDAR_NO_MONTH_CHANGE)
    g_object_notify (G_OBJECT (calendar), "no-month-change");
  if ((old_flags ^ priv->display_flags) & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    g_object_notify (G_OBJECT (calendar), "show-week-numbers");
  g_object_thaw_notify (G_OBJECT (calendar));
}

/**
 * gtk_calendar_select_month:
 * @calendar: a #GtkCalendar
 * @month: a month number between 0 and 11.
 * @year: the year the month is in.
 *
 * Shifts the calendar to a different month.
 **/
void
gtk_calendar_select_month (GtkCalendar *calendar,
                           guint        month,
                           guint        year)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  g_return_if_fail (month <= 11);

  g_object_freeze_notify (G_OBJECT (calendar));

  if (priv->month != month)
    {
      priv->month = month;
      g_object_notify (G_OBJECT (calendar), "month");
    }
  if (priv->year != year)
    {
      priv->year  = year;
      g_object_notify (G_OBJECT (calendar), "year");
    }

  calendar_compute_days (calendar);
  calendar_queue_refresh (calendar);

  g_object_thaw_notify (G_OBJECT (calendar));

  g_signal_emit (calendar, gtk_calendar_signals[MONTH_CHANGED_SIGNAL], 0);
}

/**
 * gtk_calendar_select_day:
 * @calendar: a #GtkCalendar.
 * @day: the day number between 1 and 31, or 0 to unselect
 *   the currently selected day.
 *
 * Selects a day from the current month.
 **/
void
gtk_calendar_select_day (GtkCalendar *calendar,
                         guint        day)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  g_return_if_fail (day <= 31);

  if (priv->selected_day != day)
    {
      /* Deselect the old day */
      if (priv->selected_day > 0)
        {
          if (gtk_widget_is_drawable (GTK_WIDGET (calendar)))
            calendar_invalidate_day_num (calendar, priv->selected_day);
          priv->selected_day = 0;
        }

      priv->selected_day = day;

      /* Select the new day */
      if (priv->selected_day > 0)
        {
          if (gtk_widget_is_drawable (GTK_WIDGET (calendar)))
            calendar_invalidate_day_num (calendar, priv->selected_day);
        }

      g_object_notify (G_OBJECT (calendar), "day");
    }

  g_signal_emit (calendar, gtk_calendar_signals[DAY_SELECTED_SIGNAL], 0);
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
 * @calendar: a #GtkCalendar
 * @year: (out) (allow-none): location to store the year as a decimal
 *     number (e.g. 2011), or %NULL
 * @month: (out) (allow-none): location to store the month number
 *     (between 0 and 11), or %NULL
 * @day: (out) (allow-none): location to store the day number (between
 *     1 and 31), or %NULL
 *
 * Obtains the selected date from a #GtkCalendar.
 */
void
gtk_calendar_get_date (GtkCalendar *calendar,
                       guint       *year,
                       guint       *month,
                       guint       *day)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  if (year)
    *year = priv->year;

  if (month)
    *month = priv->month;

  if (day)
    *day = priv->selected_day;
}

/**
 * gtk_calendar_set_detail_func:
 * @calendar: a #GtkCalendar.
 * @func: a function providing details for each day.
 * @data: data to pass to @func invokations.
 * @destroy: a function for releasing @data.
 *
 * Installs a function which provides Pango markup with detail information
 * for each day. Examples for such details are holidays or appointments. That
 * information is shown below each day when #GtkCalendar:show-details is set.
 * A tooltip containing with full detail information is provided, if the entire
 * text should not fit into the details area, or if #GtkCalendar:show-details
 * is not set.
 *
 * The size of the details area can be restricted by setting the
 * #GtkCalendar:detail-width-chars and #GtkCalendar:detail-height-rows
 * properties.
 */
void
gtk_calendar_set_detail_func (GtkCalendar           *calendar,
                              GtkCalendarDetailFunc  func,
                              gpointer               data,
                              GDestroyNotify         destroy)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  if (priv->detail_func_destroy)
    priv->detail_func_destroy (priv->detail_func_user_data);

  priv->detail_func = func;
  priv->detail_func_user_data = data;
  priv->detail_func_destroy = destroy;

  gtk_widget_set_has_tooltip (GTK_WIDGET (calendar),
                              NULL != priv->detail_func);
  gtk_widget_queue_resize (GTK_WIDGET (calendar));
}

/**
 * gtk_calendar_set_detail_width_chars:
 * @calendar: a #GtkCalendar.
 * @chars: detail width in characters.
 *
 * Updates the width of detail cells.
 * See #GtkCalendar:detail-width-chars.
 */
void
gtk_calendar_set_detail_width_chars (GtkCalendar *calendar,
                                     gint         chars)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  if (chars != priv->detail_width_chars)
    {
      priv->detail_width_chars = chars;
      g_object_notify (G_OBJECT (calendar), "detail-width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (calendar));
    }
}

/**
 * gtk_calendar_set_detail_height_rows:
 * @calendar: a #GtkCalendar.
 * @rows: detail height in rows.
 *
 * Updates the height of detail cells.
 * See #GtkCalendar:detail-height-rows.
 */
void
gtk_calendar_set_detail_height_rows (GtkCalendar *calendar,
                                     gint         rows)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  if (rows != priv->detail_height_rows)
    {
      priv->detail_height_rows = rows;
      g_object_notify (G_OBJECT (calendar), "detail-height-rows");
      gtk_widget_queue_resize (GTK_WIDGET (calendar));
    }
}

/**
 * gtk_calendar_get_detail_width_chars:
 * @calendar: a #GtkCalendar.
 *
 * Queries the width of detail cells, in characters.
 * See #GtkCalendar:detail-width-chars.
 *
 * Returns: The width of detail cells, in characters.
 */
gint
gtk_calendar_get_detail_width_chars (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), 0);

  return priv->detail_width_chars;
}

/**
 * gtk_calendar_get_detail_height_rows:
 * @calendar: a #GtkCalendar.
 *
 * Queries the height of detail cells, in rows.
 * See #GtkCalendar:detail-width-chars.
 *
 * Returns: The height of detail cells, in rows.
 */
gint
gtk_calendar_get_detail_height_rows (GtkCalendar *calendar)
{
  GtkCalendarPrivate *priv = gtk_calendar_get_instance_private (calendar);

  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), 0);

  return priv->detail_height_rows;
}
