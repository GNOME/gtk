/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GTK Calendar Widget
 * Copyright (C) 1998 Cesar Miquel, Shawn T. Amundson and Mattias Grönlund
 * 
 * lib_date routines
 * Copyright (c) 1995, 1996, 1997, 1998 by Steffen Beyer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "gtkcalendar.h"
#include "gdk/gdkkeysyms.h"

/***************************************************************************/
/* The following date routines are taken from the lib_date package.  Keep
 * them seperate in case we want to update them if a newer lib_date comes
 * out with fixes.  */

typedef	 unsigned   int	    N_int;
typedef	 unsigned   long    N_long;
typedef	 signed	    long    Z_long;
typedef enum { false = FALSE , true = TRUE } boolean;

#define and	    &&	    /* logical (boolean) operators: lower case */
#define or	    ||

static const N_int month_length[2][13] =
{
  { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const N_int days_in_months[2][14] =
{
  { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
  { 0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

static Z_long  calc_days(N_int year, N_int mm, N_int dd);
static N_int   day_of_week(N_int year, N_int mm, N_int dd);
static Z_long  dates_difference(N_int year1, N_int mm1, N_int dd1,
				N_int year2, N_int mm2, N_int dd2);
static N_int   weeks_in_year(N_int year);

static boolean 
leap(N_int year)
{
  return((((year % 4) == 0) and ((year % 100) != 0)) or ((year % 400) == 0));
}

static N_int 
day_of_week(N_int year, N_int mm, N_int dd)
{
  Z_long  days;
  
  days = calc_days(year, mm, dd);
  if (days > 0L)
    {
      days--;
      days %= 7L;
      days++;
    }
  return( (N_int) days );
}

static N_int weeks_in_year(N_int year)
{
  return(52 + ((day_of_week(year,1,1)==4) or (day_of_week(year,12,31)==4)));
}

static boolean 
check_date(N_int year, N_int mm, N_int dd)
{
  if (year < 1) return(false);
  if ((mm < 1) or (mm > 12)) return(false);
  if ((dd < 1) or (dd > month_length[leap(year)][mm])) return(false);
  return(true);
}

static N_int 
week_number(N_int year, N_int mm, N_int dd)
{
  N_int first;
  
  first = day_of_week(year,1,1) - 1;
  return( (N_int) ( (dates_difference(year,1,1, year,mm,dd) + first) / 7L ) +
	  (first < 4) );
}

static Z_long 
year_to_days(N_int year)
{
  return( year * 365L + (year / 4) - (year / 100) + (year / 400) );
}


static Z_long 
calc_days(N_int year, N_int mm, N_int dd)
{
  boolean lp;
  
  if (year < 1) return(0L);
  if ((mm < 1) or (mm > 12)) return(0L);
  if ((dd < 1) or (dd > month_length[(lp = leap(year))][mm])) return(0L);
  return( year_to_days(--year) + days_in_months[lp][mm] + dd );
}

#ifndef G_DISABLE_CHECKS

static boolean 
week_of_year(N_int *week, N_int *year, N_int mm, N_int dd)
{
  if (check_date(*year,mm,dd))
    {
      *week = week_number(*year,mm,dd);
      if (*week == 0) 
	*week = weeks_in_year(--(*year));
      else if (*week > weeks_in_year(*year))
	{
	  *week = 1;
	  (*year)++;
	}
      return(true);
    }
  return(false);
}

#endif /* !G_DISABLE_CHECKS */

static Z_long 
dates_difference(N_int year1, N_int mm1, N_int dd1,
		 N_int year2, N_int mm2, N_int dd2)
{
  return( calc_days(year2, mm2, dd2) - calc_days(year1, mm1, dd1) );
}

/** END OF lib_date routines ************************************************/

#define CALENDAR_MARGIN		 0
#define CALENDAR_YSEP		 4
#define CALENDAR_XSEP		 4
#define INNER_BORDER		 4

#define DAY_XPAD		 2
#define DAY_YPAD		 2
#define DAY_XSEP		 0 /* not really good for small calendar */
#define DAY_YSEP		 0 /* not really good for small calendar */

/* Color usage */
#define HEADER_FG_COLOR(widget)		 (& (widget)->style->fg[GTK_WIDGET_STATE (widget)])
#define HEADER_BG_COLOR(widget)		 (& (widget)->style->bg[GTK_WIDGET_STATE (widget)])
#define DAY_NAME_COLOR(widget)		 (& (widget)->style->bg[GTK_STATE_SELECTED])
#define NORMAL_DAY_COLOR(widget)	 (& (widget)->style->fg[GTK_WIDGET_STATE (widget)])
#define SELECTION_FOCUS_COLOR(widget)	 (& (widget)->style->fg[GTK_WIDGET_STATE (widget)])
#define SELECTION_NO_FOCUS_COLOR(widget) (& (widget)->style->mid[GTK_WIDGET_STATE (widget)])
#define PREV_MONTH_COLOR(widget)	 (& (widget)->style->mid[GTK_WIDGET_STATE (widget)])
#define NEXT_MONTH_COLOR(widget)	 (& (widget)->style->mid[GTK_WIDGET_STATE (widget)])
#define MARKED_COLOR(widget)		 (& (widget)->style->fg[GTK_WIDGET_STATE (widget)])
#define FOREGROUND_COLOR(widget)	 (& (widget)->style->fg[GTK_WIDGET_STATE (widget)])
#define BACKGROUND_COLOR(widget)	 (& (widget)->style->base[GTK_WIDGET_STATE (widget)])
#define HIGHLIGHT_BACK_COLOR(widget)	 (& (widget)->style->mid[GTK_WIDGET_STATE (widget)])

#define HEADER_FONT(widget) ((widget)->style->font)
#define LABEL_FONT(widget)   ((widget)->style->font)
#define DAY_FONT(widget)     ((widget)->style->font)

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

static gint gtk_calendar_signals[LAST_SIGNAL] = { 0 };

static GtkWidgetClass *parent_class = NULL;

typedef struct _GtkCalendarPrivateData GtkCalendarPrivateData;
struct _GtkCalendarPrivateData
{
  GdkWindow *header_win;
  GdkWindow *day_name_win;
  GdkWindow *main_win;
  GdkWindow *week_win;
  GdkWindow *arrow_win[4];

  guint header_h;
  guint day_name_h;
  guint main_h;

  guint	     arrow_state[4];
  guint	     arrow_width;
  guint	     max_month_width;
  guint	     max_year_width;
  
  guint day_width;
  guint week_width;

  guint min_day_width;
  guint max_day_char_width;
  guint max_day_char_ascent;
  guint max_day_char_descent;
  guint max_label_char_ascent;
  guint max_label_char_descent;
  guint max_week_char_width;
  
  guint freeze_count;

  /* flags */
  guint dirty_header : 1;
  guint dirty_day_names : 1;
  guint dirty_main : 1;
  guint dirty_week : 1;
};

#define GTK_CALENDAR_PRIVATE_DATA(widget)  (((GtkCalendarPrivateData*)(GTK_CALENDAR (widget)->private_data)))

typedef void (*GtkCalendarSignalDate) (GtkObject *object, guint arg1, guint arg2, guint arg3, gpointer data);

static void gtk_calendar_class_init	(GtkCalendarClass *class);
static void gtk_calendar_init		(GtkCalendar *calendar);
static void gtk_calendar_destroy	(GtkObject *calendar);
static void gtk_calendar_realize	(GtkWidget *widget);
static void gtk_calendar_unrealize	(GtkWidget *widget);
static void gtk_calendar_draw_focus	(GtkWidget *widget);
static void gtk_calendar_size_request	(GtkWidget *widget,
					 GtkRequisition *requisition);
static void gtk_calendar_size_allocate	(GtkWidget *widget,
					 GtkAllocation *allocation);
static gint gtk_calendar_expose		(GtkWidget *widget,
					 GdkEventExpose *event);
static gint gtk_calendar_button_press	(GtkWidget *widget,
					 GdkEventButton *event);
static void gtk_calendar_main_button	(GtkWidget *widget,
					 GdkEventButton *event);
static gint gtk_calendar_motion_notify	(GtkWidget *widget,
					 GdkEventMotion *event);
static gint gtk_calendar_enter_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static gint gtk_calendar_leave_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static gint gtk_calendar_key_press	(GtkWidget	   *widget,
					 GdkEventKey	   *event);
static gint gtk_calendar_focus_in	(GtkWidget *widget,
					 GdkEventFocus *event);
static gint gtk_calendar_focus_out	(GtkWidget *widget,
					 GdkEventFocus *event);
static void gtk_calendar_state_changed	(GtkWidget *widget,
					 GtkStateType previous_state);
static void gtk_calendar_style_set	(GtkWidget *widget,
					 GtkStyle  *previous_style);
static void gtk_calendar_paint_header	    (GtkWidget *widget);
static void gtk_calendar_paint_day_names    (GtkWidget *widget);
static void gtk_calendar_paint_week_numbers (GtkWidget *widget);
static void gtk_calendar_paint_main	    (GtkWidget *widget);


static void gtk_calendar_paint		(GtkWidget    *widget,
					 GdkRectangle *area);
static void gtk_calendar_paint_arrow	(GtkWidget    *widget,
					 guint	       arrow);
static void gtk_calendar_paint_day_num	(GtkWidget    *widget,
					 gint	       day);
static void gtk_calendar_paint_day	(GtkWidget    *widget,
					 gint	       row,
					 gint	       col);
static void gtk_calendar_draw		(GtkWidget    *widget,
					 GdkRectangle *area);
static void gtk_calendar_compute_days	(GtkCalendar  *calendar);
static gint left_x_for_column		(GtkCalendar  *calendar,
					 gint	       column);
static gint top_y_for_row		(GtkCalendar  *calendar,
					 gint	       row);

static char    *default_abbreviated_dayname[7];
static char    *default_monthname[12];

GtkType
gtk_calendar_get_type (void)
{
  static GtkType calendar_type = 0;
  
  if (!calendar_type)
    {
      static const GtkTypeInfo calendar_info =
      {
	"GtkCalendar",
	sizeof (GtkCalendar),
	sizeof (GtkCalendarClass),
	(GtkClassInitFunc) gtk_calendar_class_init,
	(GtkObjectInitFunc) gtk_calendar_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };
      
      calendar_type = gtk_type_unique (GTK_TYPE_WIDGET, &calendar_info);
    }
  
  return calendar_type;
}

static void
gtk_calendar_class_init (GtkCalendarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_WIDGET);
  
  widget_class->realize = gtk_calendar_realize;
  widget_class->unrealize = gtk_calendar_unrealize;
  widget_class->expose_event = gtk_calendar_expose;
  widget_class->draw = gtk_calendar_draw;
  widget_class->draw_focus = gtk_calendar_draw_focus;
  widget_class->size_request = gtk_calendar_size_request;
  widget_class->size_allocate = gtk_calendar_size_allocate;
  widget_class->button_press_event = gtk_calendar_button_press;
  widget_class->motion_notify_event = gtk_calendar_motion_notify;
  widget_class->enter_notify_event = gtk_calendar_enter_notify;
  widget_class->leave_notify_event = gtk_calendar_leave_notify;
  widget_class->key_press_event = gtk_calendar_key_press;
  widget_class->focus_in_event = gtk_calendar_focus_in;
  widget_class->focus_out_event = gtk_calendar_focus_out;
  widget_class->style_set = gtk_calendar_style_set;
  widget_class->state_changed = gtk_calendar_state_changed;
  object_class->destroy = gtk_calendar_destroy;
  
  gtk_calendar_signals[MONTH_CHANGED_SIGNAL] =
    gtk_signal_new ("month_changed",
		    GTK_RUN_FIRST, object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCalendarClass, month_changed),
		    gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);
  gtk_calendar_signals[DAY_SELECTED_SIGNAL] =
    gtk_signal_new ("day_selected",
		    GTK_RUN_FIRST, object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCalendarClass, day_selected),
		    gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);
  gtk_calendar_signals[DAY_SELECTED_DOUBLE_CLICK_SIGNAL] =
    gtk_signal_new ("day_selected_double_click",
		    GTK_RUN_FIRST, object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCalendarClass, day_selected_double_click),
		    gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);
  gtk_calendar_signals[PREV_MONTH_SIGNAL] =
    gtk_signal_new ("prev_month",
		    GTK_RUN_FIRST, object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCalendarClass, prev_month),
		    gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);
  gtk_calendar_signals[NEXT_MONTH_SIGNAL] =
    gtk_signal_new ("next_month",
		    GTK_RUN_FIRST, object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCalendarClass, next_month),
		    gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);
  gtk_calendar_signals[PREV_YEAR_SIGNAL] =
    gtk_signal_new ("prev_year",
		    GTK_RUN_FIRST, object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCalendarClass, prev_year),
		    gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);
  gtk_calendar_signals[NEXT_YEAR_SIGNAL] =
    gtk_signal_new ("next_year",
		    GTK_RUN_FIRST, object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCalendarClass, next_year),
		    gtk_signal_default_marshaller, GTK_TYPE_NONE, 0);
  
  gtk_object_class_add_signals (object_class, gtk_calendar_signals, LAST_SIGNAL);
  
  class->month_changed = NULL;
  class->day_selected = NULL;
  class->day_selected_double_click = NULL;
  class->prev_month = NULL;
  class->next_month = NULL;
  class->prev_year = NULL;
  class->next_year = NULL;
}

static void
gtk_calendar_init (GtkCalendar *calendar)
{
  time_t secs;
  struct tm *tm;
  gint i;
  char buffer[255];
  time_t tmp_time;
  GtkWidget *widget;
  GtkCalendarPrivateData *private_data;
  
  widget = GTK_WIDGET (calendar);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
  
  calendar->private_data = (gpointer) malloc (sizeof (GtkCalendarPrivateData));
  private_data = GTK_CALENDAR_PRIVATE_DATA (calendar);

  if (!default_abbreviated_dayname[0])
    for (i=0; i<7; i++)
      {
	tmp_time= (i+3)*86400;
	strftime ( buffer, sizeof (buffer), "%a", gmtime (&tmp_time));
	default_abbreviated_dayname[i] = g_strdup (buffer);
      }
  
  if (!default_monthname[0])
    for (i=0; i<12; i++)
      {
	tmp_time=i*2764800;
	strftime ( buffer, sizeof (buffer), "%B", gmtime (&tmp_time));
	default_monthname[i] = g_strdup (buffer);
      }
  
  /* Set defaults */
  secs = time (NULL);
  tm = localtime (&secs);
  calendar->month = tm->tm_mon;
  calendar->year  = 1900 + tm->tm_year;
  
  for (i=0;i<31;i++)
    calendar->marked_date[i] = FALSE;
  calendar->num_marked_dates = 0;
  calendar->selected_day = 1;
  
  calendar->display_flags = ( GTK_CALENDAR_SHOW_HEADING | 
			      GTK_CALENDAR_SHOW_DAY_NAMES );
  
  calendar->highlight_row = -1;
  calendar->highlight_col = -1;
  
  calendar->focus_row = -1;
  calendar->focus_col = -1;
  calendar->xor_gc = NULL;

  private_data->max_year_width = 0;
  private_data->max_month_width = 0;
  private_data->max_day_char_width = 0;
  private_data->max_week_char_width = 0;

  private_data->max_day_char_ascent = 0;
  private_data->max_day_char_descent = 0;
  private_data->max_label_char_ascent = 0;
  private_data->max_label_char_descent = 0;

  private_data->arrow_width = 10;

  private_data->freeze_count = 0;
  
  private_data->dirty_header = 0;
  private_data->dirty_day_names = 0;
  private_data->dirty_week = 0;
  private_data->dirty_main = 0;
}

GtkWidget*
gtk_calendar_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_CALENDAR));
}

/* column_from_x: returns the column 0-6 that the
 * x pixel of the xwindow is in */
static gint
column_from_x (GtkCalendar *calendar,
	       gint	    event_x)
{
  gint c, column;
  gint x_left, x_right;
  
  column = -1;
  
  for (c = 0; c < 7; c++)
    {
      x_left = left_x_for_column (calendar, c);
      x_right = x_left + GTK_CALENDAR_PRIVATE_DATA (calendar)->day_width;
      
      if (event_x >= x_left && event_x < x_right)
	{
	  column = c;
	  break;
	}
    }
  
  return column;
}

static gint
row_height (GtkCalendar *calendar)
{
  return (GTK_CALENDAR_PRIVATE_DATA (calendar)->main_h - CALENDAR_MARGIN
	  - ((calendar->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
	     ? CALENDAR_YSEP : CALENDAR_MARGIN)) / 6;
}


/* row_from_y: returns the row 0-5 that the
 * y pixel of the xwindow is in */
static gint
row_from_y (GtkCalendar *calendar,
	    gint	 event_y)
{
  gint r, row;
  gint height;
  gint y_top, y_bottom;
  
  height = row_height (calendar);
  row = -1;
  
  for (r = 0; r < 6; r++)
    {
      y_top = top_y_for_row (calendar, r);
      y_bottom = y_top + height;
      
      if (event_y >= y_top && event_y < y_bottom)
	{
	  row = r;
	  break;
	}
    }
  
  return row;
}

/* left_x_for_column: returns the x coordinate
 * for the left of the column */
static gint
left_x_for_column (GtkCalendar *calendar,
		   gint		column)
{
  gint width;
  gint x_left;
  
  width = GTK_CALENDAR_PRIVATE_DATA (calendar)->day_width;
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    x_left =  DAY_XSEP + (width + DAY_XSEP) * column;
  else
    x_left = CALENDAR_MARGIN + (width + DAY_XSEP) * column;
  
  return x_left;
}

/* top_y_for_row: returns the y coordinate
 * for the top of the row */
static gint
top_y_for_row (GtkCalendar *calendar,
	       gint	    row)
{
  
  return (GTK_CALENDAR_PRIVATE_DATA (calendar)->main_h 
	  - (CALENDAR_MARGIN + (6 - row)
	     * row_height (calendar)));
}

/* This function should be done by the toolkit, but we don't like the
 * GTK arrows because they don't look good on this widget */
static void
draw_arrow_right (GdkWindow *window,
		  GdkGC	    *gc,
		  gint	     x,
		  gint	     y,
		  gint	     size)
{
  gint i;
  
  for (i = 0; i <= size / 2; i++)
    {
      gdk_draw_line (window, gc,
		     x + i,
		     y + i,
		     x + i,
		     y + size - i);
    }
}

/* This function should be done by the toolkit, but we don't like the
 * GTK arrows because they don't look good on this widget */
static void
draw_arrow_left (GdkWindow *window,
		 GdkGC	   *gc,
		 gint	    x,
		 gint	    y,
		 gint	    size)
{
  gint i;
  
  for (i = 0; i <= size / 2; i++)
    {
      gdk_draw_line (window, gc,
		     x + size/2 - i,
		     y + i,
		     x + size/2 - i,
		     y + size - i);
    }
}

static void
gtk_calendar_set_month_prev (GtkCalendar *calendar)
{
  gint month_len;
  
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    return;
  
  if (calendar->month == 0)
    {
      calendar->month = 11;
      calendar->year--;
    } 
  else 
    calendar->month--;
  
  month_len = month_length[leap (calendar->year)][calendar->month + 1];
  
  gtk_calendar_freeze (calendar);
  gtk_calendar_compute_days (calendar);
  
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[PREV_MONTH_SIGNAL]);
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[MONTH_CHANGED_SIGNAL]);
  
  if (month_len < calendar->selected_day)
    {
      calendar->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    {
      if (calendar->selected_day < 0)
	calendar->selected_day = calendar->selected_day + 1 + month_length[leap (calendar->year)][calendar->month + 1];
      gtk_calendar_select_day (calendar, calendar->selected_day);
    }
  
  gtk_calendar_paint (GTK_WIDGET (calendar), NULL);
  gtk_calendar_thaw (calendar);
}


static void
gtk_calendar_set_month_next (GtkCalendar *calendar)
{
  gint month_len;
  
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_WIDGET (calendar));
  
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    return;
  
  
  if (calendar->month == 11)
    {
      calendar->month = 0;
      calendar->year++;
    } 
  else 
    calendar->month++;
  
  gtk_calendar_freeze (calendar);
  gtk_calendar_compute_days (calendar);
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[NEXT_MONTH_SIGNAL]);
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[MONTH_CHANGED_SIGNAL]);
  
  month_len = month_length[leap (calendar->year)][calendar->month + 1];
  
  if (month_len < calendar->selected_day)
    {
      calendar->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, calendar->selected_day);
  
  gtk_calendar_paint (GTK_WIDGET(calendar), NULL);
  gtk_calendar_thaw (calendar);
}

static void
gtk_calendar_set_year_prev (GtkCalendar *calendar)
{
  gint month_len;
  
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_WIDGET (calendar));
  
  calendar->year--;
  gtk_calendar_freeze (calendar);
  gtk_calendar_compute_days (calendar);
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[PREV_YEAR_SIGNAL]);
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[MONTH_CHANGED_SIGNAL]);
  
  month_len = month_length[leap (calendar->year)][calendar->month + 1];
  
  if (month_len < calendar->selected_day)
    {
      calendar->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, calendar->selected_day);
  
  gtk_calendar_paint (GTK_WIDGET (calendar), NULL);
  gtk_calendar_thaw (calendar);
}

static void
gtk_calendar_set_year_next (GtkCalendar *calendar)
{
  gint month_len;
  GtkWidget *widget;
  
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_WIDGET (calendar));
  
  widget = GTK_WIDGET (calendar);
  
  gtk_calendar_freeze (calendar);
  
  calendar->year++;
  gtk_calendar_compute_days (calendar);
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[NEXT_YEAR_SIGNAL]);
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[MONTH_CHANGED_SIGNAL]);
  
  month_len = month_length[leap (calendar->year)][calendar->month + 1];
  
  if (month_len < calendar->selected_day)
    {
      calendar->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, calendar->selected_day);
  
  gtk_calendar_paint (GTK_WIDGET (calendar), NULL);
  gtk_calendar_thaw (calendar);
}

static void
gtk_calendar_main_button (GtkWidget	 *widget,
			  GdkEventButton *event)
{
  GtkCalendar *calendar;
  gint x, y;
  gint row, col;
  gint day_month;
  gint old_focus_row, old_focus_col;
  
  calendar = GTK_CALENDAR (widget);
  
  x = (gint) (event->x);
  y = (gint) (event->y);
  
  row = row_from_y (calendar, y);
  col = column_from_x (calendar, x);

  /* If row or column isn't found, just return. */
  if (row == -1 || col == -1)
    return;
  
  day_month = calendar->day_month[row][col];
  
  if (day_month == MONTH_CURRENT)
    {
      if (event->type == GDK_2BUTTON_PRESS)
	gtk_signal_emit (GTK_OBJECT (calendar),
			 gtk_calendar_signals[DAY_SELECTED_DOUBLE_CLICK_SIGNAL]);
      else
	{
	  if (!GTK_WIDGET_HAS_FOCUS (widget))
	    gtk_widget_grab_focus (widget);
	  old_focus_row = calendar->focus_row;
	  old_focus_col = calendar->focus_col;
	  calendar->focus_row = row;
	  calendar->focus_col = col;
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_select_day (calendar, calendar->day[row][col]);
	}
    }
  else if (day_month == MONTH_PREV)
    gtk_calendar_set_month_prev (calendar);
  else if (day_month == MONTH_NEXT)
    gtk_calendar_set_month_next (calendar);
}

static void
gtk_calendar_realize_arrows (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint i;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /* Arrow windows ------------------------------------- */
  if (! (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
      && (calendar->display_flags & GTK_CALENDAR_SHOW_HEADING))
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = (gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK
			       | GDK_BUTTON_PRESS_MASK	| GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.y = 3;
      attributes.width = private_data->arrow_width;
      attributes.height = private_data->header_h - 7;
      for (i = 0; i < 4; i++)
	{
	  switch (i)
	    {
	    case ARROW_MONTH_LEFT:
	      attributes.x = 3;
	      break;
	    case ARROW_MONTH_RIGHT:
	      attributes.x = (private_data->arrow_width 
			      + private_data->max_month_width);
	      break;
	    case ARROW_YEAR_LEFT:
	      attributes.x = (widget->allocation.width - 4
			      - (3 + 2*private_data->arrow_width 
				 + private_data->max_year_width));
	      break;
	    case ARROW_YEAR_RIGHT:
	      attributes.x = (widget->allocation.width - 4 
			      - 3 - private_data->arrow_width);
	      break;
	    }
	  private_data->arrow_win[i] = gdk_window_new (private_data->header_win,
						       &attributes, 
						       attributes_mask);
	  private_data->arrow_state[i] = GTK_STATE_NORMAL;
	  gdk_window_set_background (private_data->arrow_win[i],
				     HEADER_BG_COLOR (GTK_WIDGET (calendar)));
	  gdk_window_show (private_data->arrow_win[i]);
	  gdk_window_set_user_data (private_data->arrow_win[i], widget);
	}
    }
  else
    {
      for (i = 0; i < 4; i++)
	private_data->arrow_win[i] = NULL;
    }
}

static void
gtk_calendar_realize_header (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /* Header window ------------------------------------- */
  if (calendar->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.x = 2;
      attributes.y = 2;
      attributes.width = widget->allocation.width - 4;
      attributes.height = private_data->header_h;
      private_data->header_win = gdk_window_new (widget->window,
					     &attributes, attributes_mask);
      
      gdk_window_set_background (private_data->header_win,
				 HEADER_BG_COLOR (GTK_WIDGET (calendar)));
      gdk_window_show (private_data->header_win);
      gdk_window_set_user_data (private_data->header_win, widget);
      
    }
  else
    {
      private_data->header_win = NULL;
    }
  gtk_calendar_realize_arrows (widget);
}

static void
gtk_calendar_realize_day_names (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /* Day names	window --------------------------------- */
  if ( calendar->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.x = (widget->style->klass->xthickness + INNER_BORDER);
      attributes.y = private_data->header_h + (widget->style->klass->ythickness 
					   + INNER_BORDER);
      attributes.width = (widget->allocation.width 
			  - (widget->style->klass->xthickness + INNER_BORDER) 
			  * 2);
      attributes.height = private_data->day_name_h;
      private_data->day_name_win = gdk_window_new (widget->window,
						   &attributes, 
						   attributes_mask);
      gdk_window_set_background (private_data->day_name_win, 
				 BACKGROUND_COLOR ( GTK_WIDGET ( calendar)));
      gdk_window_show (private_data->day_name_win);
      gdk_window_set_user_data (private_data->day_name_win, widget);
    }
  else
    {
      private_data->day_name_win = NULL;
    }
}

static void
gtk_calendar_realize_week_numbers (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /* Week number window -------------------------------- */
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.x = + (widget->style->klass->xthickness + INNER_BORDER);
      attributes.y = (private_data->header_h + private_data->day_name_h 
		      + (widget->style->klass->ythickness + INNER_BORDER));
      attributes.width = private_data->week_width;
      attributes.height = private_data->main_h;
      private_data->week_win = gdk_window_new (widget->window,
					       &attributes, attributes_mask);
      gdk_window_set_background (private_data->week_win,  
				 BACKGROUND_COLOR (GTK_WIDGET (calendar)));
      gdk_window_show (private_data->week_win);
      gdk_window_set_user_data (private_data->week_win, widget);
    } 
  else
    {
      private_data->week_win = NULL;
    }
}

static void
gtk_calendar_realize (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkGCValues values;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  gtk_calendar_compute_days (calendar);
  
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask =  (gtk_widget_get_events (widget) 
			    | GDK_EXPOSURE_MASK |GDK_KEY_PRESS_MASK);
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (widget->parent->window,
				   &attributes, attributes_mask);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  /* Header window ------------------------------------- */
  gtk_calendar_realize_header (widget);
  /* Day names	window --------------------------------- */
  gtk_calendar_realize_day_names (widget);
  /* Week number window -------------------------------- */
  gtk_calendar_realize_week_numbers (widget);
  /* Main Window --------------------------------------	 */
  attributes.event_mask =  (gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK
			    | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			    | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    attributes.x = private_data->week_width;
  else
    attributes.x = 0;
  attributes.x += (widget->style->klass->xthickness + INNER_BORDER);
  attributes.y = (private_data->header_h + private_data->day_name_h 
		  + (widget->style->klass->ythickness + INNER_BORDER));
  attributes.width = (widget->allocation.width - attributes.x 
		      - (widget->style->klass->xthickness + INNER_BORDER));
  attributes.height = private_data->main_h;
  private_data->main_win = gdk_window_new (widget->window,
					   &attributes, attributes_mask);
  gdk_window_set_background (private_data->main_win, 
			     BACKGROUND_COLOR ( GTK_WIDGET ( calendar)));
  gdk_window_show (private_data->main_win);
  gdk_window_set_user_data (private_data->main_win, widget);
  gdk_window_set_background (widget->window, BACKGROUND_COLOR (widget));
  gdk_window_show (widget->window);
  gdk_window_set_user_data (widget->window, widget);
  
  /* Set widgets gc */
  calendar->gc = gdk_gc_new (widget->window);

  values.foreground = widget->style->white;
  values.function = GDK_XOR;
  calendar->xor_gc = gdk_gc_new_with_values (widget->window,
					     &values,
					     GDK_GC_FOREGROUND |
					     GDK_GC_FUNCTION);
}

static void
gtk_calendar_unrealize (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint i;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (private_data->header_win)
    {
      for (i = 0; i < 4; i++)
	{
	  if (private_data->arrow_win[i])
	    {
	      gdk_window_set_user_data (private_data->arrow_win[i], NULL);
	      gdk_window_destroy (private_data->arrow_win[i]);
	      private_data->arrow_win[i] = NULL;
	    }
	}
      gdk_window_set_user_data (private_data->header_win, NULL);
      gdk_window_destroy (private_data->header_win);
      private_data->header_win = NULL;
    }
  
  if (private_data->week_win)
    {
      gdk_window_set_user_data (private_data->week_win, NULL);
      gdk_window_destroy (private_data->week_win);
      private_data->week_win = NULL;      
    }
  
  if (private_data->main_win)
    {
      gdk_window_set_user_data (private_data->main_win, NULL);
      gdk_window_destroy (private_data->main_win);
      private_data->main_win = NULL;      
    }
  if (private_data->day_name_win)
    {
      gdk_window_set_user_data (private_data->day_name_win, NULL);
      gdk_window_destroy (private_data->day_name_win);
      private_data->day_name_win = NULL;      
    }
  if (calendar->xor_gc)
    gdk_gc_unref (calendar->xor_gc);
  if (calendar->gc)
    gdk_gc_unref (calendar->gc);
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_calendar_size_request (GtkWidget	  *widget,
			   GtkRequisition *requisition)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;

  gint height;
  gint i;
  gchar buffer[255];
  gint calendar_margin = CALENDAR_MARGIN;
  gint header_width, main_width;
  gint lbearing;
  gint rbearing;
  gint ascent;
  gint descent;
  gint width;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  /*
   * Calculate the requisition	width for the widget.
   */
  
  /* Header width */
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      private_data->max_month_width = 0;
      for (i = 0; i < 12; i++)
	{
	  private_data->max_month_width = MAX (private_data->max_month_width,
					       gdk_string_measure (HEADER_FONT (widget),
								   default_monthname[i]) + 8);
	}
      private_data->max_year_width = 0;
      for (i=0; i<10; i++)
	{
	  sprintf (buffer, "%d%d%d%d", i,i,i,i);
	  private_data->max_year_width = MAX (private_data->max_year_width,
					      gdk_string_measure (HEADER_FONT (widget),
								  buffer) + 8);
	}
    } 
  else 
    {
      private_data->max_month_width = 0;
      private_data->max_year_width = 0;
    }
  
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    header_width = (private_data->max_month_width 
		    + private_data->max_year_width
		    + 3 * 3);
  else
    header_width = (private_data->max_month_width 
		    + private_data->max_year_width
		    + 4 * private_data->arrow_width + 3 * 3);

  /* Mainwindow labels width */
  
  private_data->max_day_char_width = 0;
  for (i = 0; i < 9; i++)
    {
      sprintf (buffer, "%d%d", i, i);
      private_data->min_day_width = MAX (private_data->max_day_char_width,
				     gdk_string_measure (DAY_FONT (widget),
							 buffer));
    }
  /* We add one to max_day_char_width to be able to make the marked day "bold" */
  private_data->max_day_char_width = private_data->min_day_width / 2 +1;
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    for (i = 0; i < 7; i++)
      {
	gdk_text_extents (LABEL_FONT (widget),
			  default_abbreviated_dayname[i],
			  strlen(default_abbreviated_dayname[i]),
			  &lbearing,
			  &rbearing,
			  &width,
			  &ascent,
			  &descent);
	private_data->min_day_width = MAX (private_data->min_day_width, width);
	private_data->max_label_char_ascent = MAX (private_data->max_label_char_ascent, 
					       ascent);
	private_data->max_label_char_descent = MAX (private_data->max_label_char_descent, 
						    descent);
      }
  
  private_data->max_week_char_width = 0;
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    for (i = 0; i < 9; i++)
      {
	sprintf (buffer, "%d%d", i, i);
	private_data->max_week_char_width = MAX (private_data->max_week_char_width,
					     gdk_string_measure (LABEL_FONT (widget), buffer) / 2);
      }
  
  main_width = (7 * (private_data->min_day_width + DAY_XPAD * 2) + (DAY_XSEP * 6) + CALENDAR_MARGIN * 2
		+ (private_data->max_week_char_width
		   ? private_data->max_week_char_width * 2 + DAY_XPAD * 2 + CALENDAR_XSEP * 2
		   : 0));
  
  
  requisition->width = MAX (header_width+4, main_width + (widget->style->klass->xthickness + INNER_BORDER) *2);
  
  /*
   * Calculate the requisition height for the widget.
   */
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      private_data->header_h = (HEADER_FONT (widget)->ascent 
				+ HEADER_FONT (widget)->descent
				+ CALENDAR_YSEP * 2);
    }
  else
    {
      private_data->header_h = 0;
    }
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    {
      private_data->day_name_h = (private_data->max_label_char_ascent
				  + private_data->max_label_char_descent
				  + 2 * DAY_YPAD + calendar_margin);
      calendar_margin = CALENDAR_YSEP;
    } 
  else
    {
      private_data->day_name_h = 0;
    }

  gdk_text_extents (DAY_FONT (widget),
		    "0123456789",
		    10,
		    &lbearing,
		    &rbearing,
		    &width,
		    &private_data->max_day_char_ascent,
		    &private_data->max_day_char_descent);
  
  private_data->main_h = (CALENDAR_MARGIN + calendar_margin
			  + 6 * (private_data->max_day_char_ascent
				 + private_data->max_day_char_descent 
				 + DAY_YPAD * 2)
			  + DAY_YSEP * 5);
  
  /* 
   * If we display weeknumbers we need some extra space 
   */
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    {
      private_data->main_h = MAX (private_data->main_h,
				  (CALENDAR_MARGIN + calendar_margin
				   + 6 * (private_data->max_day_char_ascent 
					  + private_data->max_day_char_descent 
					  + DAY_YPAD * 2)
				   + DAY_YSEP * 5));
    }
  
  height = (private_data->header_h + private_data->day_name_h 
	    + private_data->main_h);
  
  requisition->height = height + (widget->style->klass->ythickness + INNER_BORDER) * 2;
}

static void
gtk_calendar_size_allocate (GtkWidget	  *widget,
			    GtkAllocation *allocation)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  g_return_if_fail (allocation != NULL);
  
  widget->allocation = *allocation;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    {
      private_data->day_width = (private_data->min_day_width
			     * ((allocation->width - (widget->style->klass->xthickness + INNER_BORDER) * 2
				 - (CALENDAR_MARGIN * 2) -  (DAY_XSEP * 7) - CALENDAR_XSEP * 2))
			     / (7 * private_data->min_day_width + private_data->max_week_char_width * 2));
      private_data->week_width = ((allocation->width - (widget->style->klass->xthickness + INNER_BORDER) * 2
			       - (CALENDAR_MARGIN * 2) - (DAY_XSEP * 7) - CALENDAR_XSEP * 2 )
			      - private_data->day_width * 7 + CALENDAR_MARGIN + CALENDAR_XSEP);
    } 
  else 
    {
      private_data->day_width = (allocation->width
			     - (widget->style->klass->xthickness + INNER_BORDER) * 2
			     - (CALENDAR_MARGIN * 2)
			     - (DAY_XSEP * 7))/7;
      private_data->week_width = 0;
    }
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      if (private_data->header_win)
	gdk_window_move_resize (private_data->header_win,
				2, 2,
				allocation->width-4, private_data->header_h);
      if (private_data->arrow_win[ARROW_MONTH_LEFT])
	gdk_window_move_resize (private_data->arrow_win[ARROW_MONTH_LEFT],
				3, 3,
				private_data->arrow_width,
				private_data->header_h - 7);
      if (private_data->arrow_win[ARROW_MONTH_RIGHT])
	gdk_window_move_resize (private_data->arrow_win[ARROW_MONTH_RIGHT],
				(private_data->arrow_width 
				 + private_data->max_month_width), 
				3,
				private_data->arrow_width,
				private_data->header_h - 7);
      if (private_data->arrow_win[ARROW_YEAR_LEFT])
	gdk_window_move_resize (private_data->arrow_win[ARROW_YEAR_LEFT],
				(allocation->width - 4
				 - (3 + 2*private_data->arrow_width 
				    + private_data->max_year_width)),
				3,
				private_data->arrow_width,
				private_data->header_h - 7);
      if (private_data->arrow_win[ARROW_YEAR_RIGHT])
	gdk_window_move_resize (private_data->arrow_win[ARROW_YEAR_RIGHT],
				(allocation->width - 4 
				 - 3 - private_data->arrow_width), 
				3,
				private_data->arrow_width,
				private_data->header_h - 7);
      if (private_data->day_name_win)
	gdk_window_move_resize (private_data->day_name_win,
				widget->style->klass->xthickness + INNER_BORDER,
				private_data->header_h + (widget->style->klass->ythickness + INNER_BORDER),
				allocation->width - (widget->style->klass->xthickness + INNER_BORDER) * 2,
				private_data->day_name_h);
      if (private_data->week_win)
	gdk_window_move_resize (private_data->week_win,
				(widget->style->klass->xthickness + INNER_BORDER),
				private_data->header_h + private_data->day_name_h
				+ (widget->style->klass->ythickness + INNER_BORDER),
				private_data->week_width,
				private_data->main_h);
      gdk_window_move_resize (private_data->main_win,
			      (private_data->week_width ? private_data->week_width + CALENDAR_XSEP :0) 
			      + (widget->style->klass->xthickness + INNER_BORDER),
			      private_data->header_h + private_data->day_name_h
			      + (widget->style->klass->ythickness + INNER_BORDER),
			      allocation->width 
			      - (private_data->week_width ? private_data->week_width + CALENDAR_XSEP :0) 
			      - (widget->style->klass->xthickness + INNER_BORDER) * 2,
			      private_data->main_h);
    }
}

static void
gtk_calendar_draw_focus (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint width, height;
  gint x, y;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));

  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      x = 0;
      y = 0;
      gdk_window_get_size (widget->window, &width, &height);
      gdk_window_clear (widget->window);
      
      gdk_draw_rectangle (widget->window, 
			  widget->style->base_gc[GTK_WIDGET_STATE (widget)],
			  FALSE, x + 2, y + 2, width - 5, height - 5);
      
      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, GTK_SHADOW_IN,
		       x, y, width, height);
      
    }
}

static gint
gtk_calendar_expose (GtkWidget	    *widget,
		     GdkEventExpose *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CALENDAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (event->window == private_data->main_win)
	gtk_calendar_paint_main (widget);
      
      if (event->window == private_data->header_win)
	gtk_calendar_paint_header (widget);
      
      if (event->window == private_data->day_name_win)
	gtk_calendar_paint_day_names (widget);
      
      if (event->window == private_data->week_win)
	gtk_calendar_paint_week_numbers (widget);
      if (event->window == widget->window)
	gtk_widget_draw_focus (widget);
    }
  
  return FALSE;
}

static void
gtk_calendar_draw (GtkWidget	*widget,
		   GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  g_return_if_fail (area != NULL);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_calendar_paint (widget, area);
  
}

static void
gtk_calendar_paint (GtkWidget	 *widget,
		    GdkRectangle *area)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (widget->window != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (private_data->main_win != NULL)
    gtk_calendar_paint_main (widget);
  
  if (private_data->header_win != NULL)
    gtk_calendar_paint_header (widget);
  
  if (private_data->day_name_win != NULL)
    gtk_calendar_paint_day_names (widget);
  
  if (private_data->week_win != NULL)
    gtk_calendar_paint_week_numbers (widget);
  
  gtk_widget_draw_focus (widget);
}

static void
gtk_calendar_paint_header (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GdkGC *gc;
  char buffer[255];
  int y, y_arrow;
  gint header_width, cal_height;
  gint str_width;
  gint max_month_width;
  gint max_year_width;
  GtkCalendarPrivateData *private_data;

  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  if (private_data->freeze_count)
    {
      private_data->dirty_header = 1;
      return;
    }
  private_data->dirty_header = 0;
  gc = calendar->gc;
  
  /* Clear window */
  gdk_window_clear (private_data->header_win);
  
  header_width = widget->allocation.width - 4;
  cal_height = widget->allocation.height;
  
  max_month_width = private_data->max_month_width;
  max_year_width = private_data->max_year_width;
  
  gdk_gc_set_foreground (gc, BACKGROUND_COLOR (GTK_WIDGET (calendar)));
  gtk_draw_shadow (widget->style, private_data->header_win,
		   GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		   0, 0, header_width, private_data->header_h);
  
  
  /* Draw title */
  y = private_data->header_h - (private_data->header_h 
				- HEADER_FONT (widget)->ascent
				+ HEADER_FONT (widget)->descent) / 2;
  y_arrow = (private_data->header_h - 9) / 2;
  
  /* Draw year and its arrows */
  sprintf (buffer, "%d", calendar->year);
  str_width = gdk_string_measure (HEADER_FONT (widget), buffer);
  gdk_gc_set_foreground (gc, HEADER_FG_COLOR (GTK_WIDGET (calendar)));
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    gdk_draw_string (private_data->header_win, HEADER_FONT (widget), gc,
		     header_width - (3 + max_year_width
				     - (max_year_width - str_width)/2),
		     y, buffer);
  else
    gdk_draw_string (private_data->header_win, HEADER_FONT (widget), gc,
		     header_width - (3 + private_data->arrow_width + max_year_width
				     - (max_year_width - str_width)/2),
		     y, buffer);
  
  /* Draw month */
  sprintf (buffer, "%s", default_monthname[calendar->month]);
  str_width = gdk_string_measure (HEADER_FONT (widget), buffer);
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    gdk_draw_string (private_data->header_win, HEADER_FONT (widget), gc,
		     3 + (max_month_width - str_width) / 2,
		     y, buffer);
  else
    gdk_draw_string (private_data->header_win, HEADER_FONT (widget), gc,
		     3 + private_data->arrow_width + (max_month_width - str_width)/2,
		     y, buffer);
  
  y += CALENDAR_YSEP + HEADER_FONT (widget)->descent;
  
  gdk_gc_set_foreground (gc, BACKGROUND_COLOR (GTK_WIDGET (calendar)));
  
  gtk_calendar_paint_arrow (widget, ARROW_MONTH_LEFT);
  gtk_calendar_paint_arrow (widget, ARROW_MONTH_RIGHT);
  gtk_calendar_paint_arrow (widget, ARROW_YEAR_LEFT);
  gtk_calendar_paint_arrow (widget, ARROW_YEAR_RIGHT);
  
}

static void
gtk_calendar_paint_day_names (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GdkGC *gc;
  char buffer[255];
  int day,i;
  int day_width, cal_width;
  gint cal_height;
  int day_wid_sep;
  int str_width;
  GtkCalendarPrivateData *private_data;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  gc = calendar->gc;
  
  /*
   * Handle freeze/thaw functionality
   */
  
  if (private_data->freeze_count)
    {
      private_data->dirty_day_names = 1;
      return;
    }
  private_data->dirty_day_names = 0;
  
  /*
   * Clear the window
   */
  
  gdk_window_clear (private_data->day_name_win);
  
  day_width = private_data->day_width;
  cal_width = widget->allocation.width;
  cal_height = widget->allocation.height;
  day_wid_sep = day_width + DAY_XSEP;
  
  /*
   * Draw rectangles as inverted background for the labels.
   */
  
  gdk_gc_set_foreground (gc, &widget->style->bg[GTK_STATE_SELECTED]);
  gdk_draw_rectangle (private_data->day_name_win, gc, TRUE,
		      CALENDAR_MARGIN, CALENDAR_MARGIN,
		      cal_width-CALENDAR_MARGIN * 2,
		      private_data->day_name_h - CALENDAR_MARGIN);
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    gdk_draw_rectangle (private_data->day_name_win, gc, TRUE,
			CALENDAR_MARGIN,
			private_data->day_name_h - CALENDAR_YSEP,
			private_data->week_width - CALENDAR_YSEP - CALENDAR_MARGIN,
			CALENDAR_YSEP);
  
  /*
   * Write the labels
   */
  gdk_gc_set_foreground (gc, &widget->style->fg[GTK_STATE_SELECTED]);
  for (i = 0; i < 7; i++)
    {
      day=i;
      if (calendar->display_flags & GTK_CALENDAR_WEEK_START_MONDAY)
	day= (day+1)%7;
      sprintf (buffer, "%s", default_abbreviated_dayname[day]);
      str_width = gdk_string_measure (LABEL_FONT (widget), buffer);
      gdk_draw_string (private_data->day_name_win, LABEL_FONT (widget),
		       gc,
		       ((private_data->week_width ? CALENDAR_XSEP : CALENDAR_MARGIN)
			+ day_wid_sep * i
			+ private_data->week_width
			+ (day_width - str_width)/2),
		       CALENDAR_MARGIN + DAY_YPAD
		       + private_data->max_label_char_ascent, buffer);
    }
}

static void
gtk_calendar_paint_week_numbers (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GdkGC *gc;
  gint row, week = 0, year;
  gint x_loc;
  char buffer[3];
  gint y_baseline, day_height;
  GtkCalendarPrivateData *private_data;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (widget->window != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  gc = calendar->gc;
  
  /*
   * Handle freeze/thaw functionality
   */
  
  if (private_data->freeze_count)
    {
      private_data->dirty_week = 1;
      return;
    }
  private_data->dirty_week = 0;
  
  /*
   * Clear the window
   */
  
  gdk_window_clear (private_data->week_win);
  
  /*
   * Draw a rectangle as inverted background for the labels.
   */
  
  gdk_gc_set_foreground (gc, &widget->style->bg[GTK_STATE_SELECTED]);
  if (private_data->day_name_win)
    gdk_draw_rectangle (private_data->week_win, gc, TRUE,
			CALENDAR_MARGIN,
			0,
			private_data->week_width - CALENDAR_MARGIN - CALENDAR_XSEP,
			private_data->main_h - CALENDAR_MARGIN);
  else
    gdk_draw_rectangle (private_data->week_win, gc, TRUE,
			CALENDAR_MARGIN,
			CALENDAR_MARGIN,
			private_data->week_width - CALENDAR_MARGIN - CALENDAR_XSEP,
			private_data->main_h - 2 * CALENDAR_MARGIN);
  
  /*
   * Write the labels
   */
  
  gdk_gc_set_foreground (gc, &widget->style->fg[GTK_STATE_SELECTED]);
  day_height = row_height (calendar);
  for (row = 0; row < 6; row++)
    {
      year = calendar->year;
      if (calendar->day[row][6] < 15 && row > 3 && calendar->month == 11)
	year++;
      y_baseline = (top_y_for_row (calendar, row)
		    + (day_height + LABEL_FONT (widget)->ascent
		       - LABEL_FONT (widget)->descent)/2);
      g_return_if_fail (week_of_year (&week, &year,		
				      ((calendar->day[row][6] < 15 && row > 3 ? 1 : 0)
				       + calendar->month) % 12 + 1, calendar->day[row][6]));
      x_loc= (private_data->week_width - (private_data->week_width - CALENDAR_XSEP
				      - DAY_XPAD * 2 - CALENDAR_MARGIN ) / 2
	      - private_data->max_week_char_width
	      - CALENDAR_XSEP - DAY_XPAD);
      
      if (week > 9)
	{
	  sprintf (buffer, "%d", week/10);
	  gdk_draw_string (private_data->week_win, LABEL_FONT (widget), gc,
			   x_loc, y_baseline , buffer);
	}
      
      sprintf (buffer, "%d", week%10);
      gdk_draw_string (private_data->week_win, LABEL_FONT (widget), gc,
		       x_loc + private_data->max_week_char_width, y_baseline , buffer);
    }
}

static void
gtk_calendar_paint_day_num (GtkWidget *widget,
			    gint       day)
{
  GtkCalendar *calendar;
  gint r, c, row, col;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  
  row = -1;
  col = -1;
  for (r = 0; r < 6; r++)
    for (c = 0; c < 7; c++)
      if (calendar->day_month[r][c] == MONTH_CURRENT &&
	  calendar->day[r][c] == day)
	{
	  row = r;
	  col = c;
	}
  
  g_return_if_fail (row != -1);
  g_return_if_fail (col != -1);
  
  gtk_calendar_paint_day (widget, row, col);
}

static void
gtk_calendar_paint_day (GtkWidget *widget,
			gint	   row,
			gint	   col)
{
  GtkCalendar *calendar;
  GdkGC *gc;
  gchar buffer[255];
  gint day;
  gint day_height;
  gint x_left;
  gint x_loc;
  gint y_top;
  gint y_baseline;
  gint day_xspace;
  GtkCalendarPrivateData *private_data;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  g_return_if_fail (row < 6);
  g_return_if_fail (col < 7);
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /*
   * Handle freeze/thaw functionality
   */
  
  if (private_data->freeze_count)
    {
      private_data->dirty_main = 1;
      return;
    }
  
  day_height = row_height (calendar);
  
  day_xspace = private_data->day_width - private_data->max_day_char_width*2;
  
  day = calendar->day[row][col];
  
  x_left = left_x_for_column (calendar, col);
  x_loc = x_left + private_data->day_width / 2 + private_data->max_day_char_width;
  
  y_top = top_y_for_row (calendar, row);
  y_baseline = y_top + (day_height + private_data->max_day_char_ascent)/2;
  
  gdk_window_clear_area (private_data->main_win, x_left, y_top,
			 private_data->day_width, day_height);
  
  gc = calendar->gc;
  
  if (calendar->day_month[row][col] == MONTH_PREV)
    {
      gdk_gc_set_foreground (gc, PREV_MONTH_COLOR (GTK_WIDGET (calendar)));
    } 
  else if (calendar->day_month[row][col] == MONTH_NEXT)
    {
      gdk_gc_set_foreground (gc, NEXT_MONTH_COLOR (GTK_WIDGET (calendar)));
    } 
  else 
    {
      /*
      if (calendar->highlight_row == row && calendar->highlight_col == col)
	{
	  gdk_gc_set_foreground (gc, HIGHLIGHT_BACK_COLOR (GTK_WIDGET (calendar)));
	  gdk_draw_rectangle (private_data->main_win, gc, TRUE, x_left, y_top,
			      private_data->day_width, day_height);
	}
      */
      if (calendar->selected_day == day)
	{
	  gdk_gc_set_foreground (gc, & (GTK_WIDGET (calendar)->style->bg[GTK_STATE_SELECTED]));
	  gdk_draw_rectangle (private_data->main_win, gc, TRUE, x_left, y_top,
			      private_data->day_width, day_height);
	}
      
      if (calendar->marked_date[day-1])
	gdk_gc_set_foreground (gc, MARKED_COLOR	 (GTK_WIDGET (calendar)));
      else
	gdk_gc_set_foreground (gc, NORMAL_DAY_COLOR (GTK_WIDGET (calendar)));
  
      if (calendar->selected_day == day)
	gdk_gc_set_foreground (gc, & (GTK_WIDGET (calendar)->style->fg[GTK_STATE_SELECTED]));
      else
	gdk_gc_set_foreground (gc, & (GTK_WIDGET (calendar)->style->fg[GTK_WIDGET_STATE (calendar)]));
    }
    

  sprintf (buffer, "%d", day);
  x_loc -= gdk_string_measure (DAY_FONT (widget), buffer);
  sprintf (buffer, "%d", day);
  gdk_draw_string (private_data->main_win,
		   DAY_FONT (widget), gc,
		   x_loc, y_baseline, buffer);
  if (calendar->marked_date[day-1]
      && calendar->day_month[row][col] == MONTH_CURRENT)
    gdk_draw_string (private_data->main_win,
		     DAY_FONT (widget), gc,
		     x_loc-1, y_baseline, buffer);

  if (GTK_WIDGET_HAS_FOCUS (calendar) 
      && calendar->focus_row == row && calendar->focus_col == col)
    {
      gdk_draw_rectangle (private_data->main_win, calendar->xor_gc, 
			  FALSE, x_left, y_top,
			  private_data->day_width-1, day_height-1);
    }

}


static void
gtk_calendar_paint_main (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint row, col;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (widget->window != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  if (private_data->freeze_count)
    {
      private_data->dirty_main = 1;
      return;
    }
  private_data->dirty_main = 0;
  gdk_window_clear (private_data->main_win);
  
  /* gtk_calendar_compute_days (calendar); */ /* REMOVE later */
  
  for (col = 0; col < 7; col++)
    for (row = 0; row < 6; row++)
      gtk_calendar_paint_day (widget, row, col);
}

static void
gtk_calendar_compute_days (GtkCalendar *calendar)
{
  gint month;
  gint year;
  gint ndays_in_month;
  gint ndays_in_prev_month;
  gint first_day;
  gint row;
  gint col;
  gint day;
  
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  year = calendar->year;
  month = calendar->month + 1;
  
  ndays_in_month = month_length[leap (year)][month];
  
  first_day = day_of_week (year, month, 1);
  
  if (calendar->display_flags & GTK_CALENDAR_WEEK_START_MONDAY)
    first_day--;
  else
    first_day %= 7;
  
  
  /* Compute days of previous month */
  if (month > 1)
    ndays_in_prev_month = month_length[leap (year)][month-1];
  else
    ndays_in_prev_month = month_length[leap (year)][12];
  day = ndays_in_prev_month - first_day + 1;
  
  row = 0;
  if (first_day > 0)
    {
      for (col = 0; col < first_day; col++)
	{
	  calendar->day[row][col] = day;
	  calendar->day_month[row][col] = MONTH_PREV;
	  day++;
	}
    }
  
  /* Compute days of current month */
  col = first_day;
  for (day = 1; day <= ndays_in_month; day++)
    {
      calendar->day[row][col] = day;
      calendar->day_month[row][col] = MONTH_CURRENT;
      
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
	  calendar->day[row][col] = day;
	  calendar->day_month[row][col] = MONTH_NEXT;
	  day++;
	}
      col = 0;
    }
}

/* ----------------------------------------------------------------------
   NAME:	gtk_calendar_display_options
   DESCRIPTION:	Set display options (whether to display the
   heading and the month headings)
   
   flags is can be an XOR of:
   GTK_CALENDAR_SHOW_HEADING
   GTK_CALENDAR_SHOW_DAY_NAMES
   GTK_CALENDAR_NO_MONTH_CHANGE
   GTK_CALENDAR_SHOW_WEEK_NUMBERS
   GTK_CALENDAR_WEEK_START_MONDAY
   ---------------------------------------------------------------------- */

void
gtk_calendar_display_options (GtkCalendar	       *calendar,
			      GtkCalendarDisplayOptions flags)
{
  GtkCalendarPrivateData *private_data;
  gint resize = 0;
  GtkWidget *widget;
  gint i;
  
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  widget = GTK_WIDGET (calendar);
  private_data = GTK_CALENDAR_PRIVATE_DATA (calendar);
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_NO_MONTH_CHANGE)
	{
	  resize ++;
	  if (! (flags & GTK_CALENDAR_NO_MONTH_CHANGE)
	      && (private_data->header_win))
	    {
	      calendar->display_flags &= ~GTK_CALENDAR_NO_MONTH_CHANGE;
	      gtk_calendar_realize_arrows (widget);
	    }
	  else
	    {
	      for (i = 0; i < 4; i++)
		{
		  if (private_data->arrow_win[i])
		    {
		      gdk_window_set_user_data (private_data->arrow_win[i], 
						NULL);
		      gdk_window_destroy (private_data->arrow_win[i]);
		      private_data->arrow_win[i] = NULL;
		    }
		}
	    }
	}
      
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_SHOW_HEADING)
	{
	  resize++;
	  
	  if (flags & GTK_CALENDAR_SHOW_HEADING)
	    {
	      calendar->display_flags |= GTK_CALENDAR_SHOW_HEADING;
	      gtk_calendar_realize_header (widget);
	    }
	  else
	    {
	      for (i = 0; i < 4; i++)
		{
		  if (private_data->arrow_win[i])
		    {
		      gdk_window_set_user_data (private_data->arrow_win[i], 
						NULL);
		      gdk_window_destroy (private_data->arrow_win[i]);
		      private_data->arrow_win[i] = NULL;
		    }
		}
	      gdk_window_set_user_data (private_data->header_win, NULL);
	      gdk_window_destroy (private_data->header_win);
	      private_data->header_win = NULL;
	    }
	}
      
      
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_SHOW_DAY_NAMES)
	{
	  resize++;
	  
	  if (flags & GTK_CALENDAR_SHOW_DAY_NAMES)
	    {
	      calendar->display_flags |= GTK_CALENDAR_SHOW_DAY_NAMES;
	      gtk_calendar_realize_day_names (widget);
	    }
	  else
	    {
	      gdk_window_set_user_data (private_data->day_name_win, NULL);
	      gdk_window_destroy (private_data->day_name_win);
	      private_data->day_name_win = NULL;
	    }
	}
      
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
	{
	  resize++;
	  
	  if (flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
	    {
	      calendar->display_flags |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
	      gtk_calendar_realize_week_numbers (widget);
	    }
	  else
	    {
	      gdk_window_set_user_data (private_data->week_win, NULL);
	      gdk_window_destroy (private_data->week_win);
	      private_data->week_win = NULL;
	    }
	}
      
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_WEEK_START_MONDAY)
	{
	  if (calendar->display_flags & GTK_CALENDAR_WEEK_START_MONDAY)
	    calendar->display_flags &= ~GTK_CALENDAR_WEEK_START_MONDAY;
	  else
	    calendar->display_flags |= GTK_CALENDAR_WEEK_START_MONDAY;
	  
	  gtk_calendar_compute_days (calendar);
	  gtk_calendar_paint_main (GTK_WIDGET (calendar));
	  if (private_data->day_name_win)
	    gtk_calendar_paint_day_names (GTK_WIDGET (calendar));
	}
      
      calendar->display_flags = flags;
      if (resize)
	gtk_widget_queue_resize (GTK_WIDGET (calendar));
      
    } 
  else
    calendar->display_flags = flags;
  
}

gint
gtk_calendar_select_month (GtkCalendar *calendar,
			   guint	month,
			   guint	year)
{
  g_return_val_if_fail (calendar != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), FALSE);
  g_return_val_if_fail (month <= 11, FALSE);
  
  calendar->month = month;
  calendar->year  = year;
  
  gtk_calendar_compute_days (calendar);
  
  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
    gtk_calendar_paint (GTK_WIDGET (calendar), NULL);
  
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[MONTH_CHANGED_SIGNAL]);
  return TRUE;
}

void
gtk_calendar_select_day (GtkCalendar *calendar,
			 guint	      day)
{
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  g_return_if_fail (day <= 31);
  
  /* gtk_calendar_compute_days (calendar); */
  
  /* Deselect the old day */
  if (calendar->selected_day > 0)
    {
      gint selected_day;
      
      selected_day = calendar->selected_day;
      calendar->selected_day = 0;
      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
	gtk_calendar_paint_day_num (GTK_WIDGET (calendar), selected_day);
    }
  
  calendar->selected_day = day;
  
  /* Deselect the new day */
  if (day != 0)
    {
      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
	gtk_calendar_paint_day_num (GTK_WIDGET (calendar), day);
    }
  
  gtk_signal_emit (GTK_OBJECT (calendar),
		   gtk_calendar_signals[DAY_SELECTED_SIGNAL]);
}

void
gtk_calendar_clear_marks (GtkCalendar *calendar)
{
  guint day;
  
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  for (day = 0; day < 31; day++)
    {
      calendar->marked_date[day] = FALSE;
    }
 
  calendar->num_marked_dates = 0;
 
  if (GTK_WIDGET_DRAWABLE (calendar))
    {
      gtk_calendar_paint_main (GTK_WIDGET (calendar));
    }
}

gint
gtk_calendar_mark_day (GtkCalendar *calendar,
		       guint	    day)
{
  g_return_val_if_fail (calendar != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), FALSE);
  
  if (day >= 1 && day <= 31 && calendar->marked_date[day-1] == FALSE)
    {
      calendar->marked_date[day - 1] = TRUE;
      calendar->num_marked_dates++;
    }
  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
    {
      gtk_calendar_paint_main (GTK_WIDGET (calendar));
    }
  
  return TRUE;
}

gint
gtk_calendar_unmark_day (GtkCalendar *calendar,
			 guint	      day)
{
  g_return_val_if_fail (calendar != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), FALSE);
  
  if (day >= 1 && day <= 31 && calendar->marked_date[day-1] == TRUE)
    {
      calendar->marked_date[day - 1] = FALSE;
      calendar->num_marked_dates--;
    }
  
  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
    {
      gtk_calendar_paint_main (GTK_WIDGET (calendar));
    }
  
  return TRUE;
}

void
gtk_calendar_get_date (GtkCalendar *calendar,
		       guint	   *year,
		       guint	   *month,
		       guint	   *day)
{
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  if (year)
    *year = calendar->year;
  
  if (month)
    *month = calendar->month;
  
  if (day)
    *day = calendar->selected_day;
}

static gint
gtk_calendar_button_press (GtkWidget	  *widget,
			   GdkEventButton *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint x, y;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CALENDAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  x = (gint) (event->x);
  y = (gint) (event->y);
  
  if (event->window == private_data->main_win)
    gtk_calendar_main_button (widget, event);
  
  if (event->type != GDK_BUTTON_PRESS)
    return FALSE; /* Double-clicks? Triple-clicks? No thanks! */

  if (event->window == private_data->arrow_win[ARROW_MONTH_LEFT])
    gtk_calendar_set_month_prev (calendar);
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_RIGHT])
    gtk_calendar_set_month_next (calendar);
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_LEFT])
    gtk_calendar_set_year_prev (calendar);
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_RIGHT])
    gtk_calendar_set_year_next (calendar);
  
  return FALSE;
}

static gint
gtk_calendar_motion_notify (GtkWidget	   *widget,
			    GdkEventMotion *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint event_x, event_y;
  gint row, col;
  gint old_row, old_col;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  event_x = (gint) (event->x);
  event_y = (gint) (event->y);
  
  if (event->window == private_data->main_win)
    {
      
      row = row_from_y (calendar, event_y);
      col = column_from_x (calendar, event_x);
      
      if (row != calendar->highlight_row || calendar->highlight_col != col)
	{
	  old_row = calendar->highlight_row;
	  old_col = calendar->highlight_col;
	  if (old_row > -1 && old_col > -1)
	    {
	      calendar->highlight_row = -1;
	      calendar->highlight_col = -1;
	      gtk_calendar_paint_day (widget, old_row, old_col);
	    }
	  
	  calendar->highlight_row = row;
	  calendar->highlight_col = col;
	  
	  if (row > -1 && col > -1)
	    gtk_calendar_paint_day (widget, row, col);
	}
    }
  return TRUE;
}

static gint
gtk_calendar_enter_notify (GtkWidget	    *widget,
			   GdkEventCrossing *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_LEFT])
    {
      private_data->arrow_state[ARROW_MONTH_LEFT] = GTK_STATE_PRELIGHT;
      gtk_calendar_paint_arrow (widget, ARROW_MONTH_LEFT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_RIGHT])
    {
      private_data->arrow_state[ARROW_MONTH_RIGHT] = GTK_STATE_PRELIGHT;
      gtk_calendar_paint_arrow (widget, ARROW_MONTH_RIGHT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_LEFT])
    {
      private_data->arrow_state[ARROW_YEAR_LEFT] = GTK_STATE_PRELIGHT;
      gtk_calendar_paint_arrow (widget, ARROW_YEAR_LEFT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_RIGHT])
    {
      private_data->arrow_state[ARROW_YEAR_RIGHT] = GTK_STATE_PRELIGHT;
      gtk_calendar_paint_arrow (widget, ARROW_YEAR_RIGHT);
    }
  
  return TRUE;
}

static gint
gtk_calendar_leave_notify (GtkWidget	    *widget,
			   GdkEventCrossing *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint row;
  gint col;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (event->window == private_data->main_win)
    {
      row = calendar->highlight_row;
      col = calendar->highlight_col;
      calendar->highlight_row = -1;
      calendar->highlight_col = -1;
      if (row > -1 && col > -1)
	gtk_calendar_paint_day (widget, row, col);
    }
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_LEFT])
    {
      private_data->arrow_state[ARROW_MONTH_LEFT] = GTK_STATE_NORMAL;
      gtk_calendar_paint_arrow (widget, ARROW_MONTH_LEFT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_RIGHT])
    {
      private_data->arrow_state[ARROW_MONTH_RIGHT] = GTK_STATE_NORMAL;
      gtk_calendar_paint_arrow (widget, ARROW_MONTH_RIGHT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_LEFT])
    {
      private_data->arrow_state[ARROW_YEAR_LEFT] = GTK_STATE_NORMAL;
      gtk_calendar_paint_arrow (widget, ARROW_YEAR_LEFT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_RIGHT])
    {
      private_data->arrow_state[ARROW_YEAR_RIGHT] = GTK_STATE_NORMAL;
      gtk_calendar_paint_arrow (widget, ARROW_YEAR_RIGHT);
    }
  
  return TRUE;
}

static void
gtk_calendar_paint_arrow (GtkWidget *widget,
			  guint	     arrow)
{
  GtkCalendarPrivateData *private_data;
  GdkWindow *window;
  GdkGC *gc;
  GtkCalendar *calendar;
  gint state;
  gint width, height;
  
  g_return_if_fail (widget != NULL);
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  if (private_data->freeze_count)
    {
      private_data->dirty_header = 1;
      return;
    }
  window = private_data->arrow_win[arrow];
  if (window)
    {
      state = private_data->arrow_state[arrow];
      gc = calendar->gc;
      
      gdk_window_clear (window);
      gdk_window_set_background (window, &(widget)->style->bg[state]);
      gdk_window_get_size (window, &width, &height);
      gdk_window_clear_area (window,
			     0,0,
			     width,height);
      
      gdk_gc_set_foreground (gc, & (widget)->style->fg[state]);
      
      if (arrow == ARROW_MONTH_LEFT || arrow == ARROW_YEAR_LEFT)
	draw_arrow_left (window, gc, width/2 - 3, height/2 - 4, 8);
      else 
	draw_arrow_right (window, gc, width/2 - 2, height/2 - 4, 8);
      return;
    }
}

void
gtk_calendar_freeze (GtkCalendar *calendar)
{
  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  GTK_CALENDAR_PRIVATE_DATA (calendar)->freeze_count++;
}

void
gtk_calendar_thaw (GtkCalendar *calendar)
{
  GtkCalendarPrivateData *private_data;

  g_return_if_fail (calendar != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  private_data = GTK_CALENDAR_PRIVATE_DATA (calendar);
  
  if (private_data->freeze_count)
    if (!(--private_data->freeze_count))
      {
	if (private_data->dirty_header)
	  if (GTK_WIDGET_DRAWABLE (calendar))
	    gtk_calendar_paint_header (GTK_WIDGET (calendar));
	
	if (private_data->dirty_day_names)
	  if (GTK_WIDGET_DRAWABLE (calendar))
	    gtk_calendar_paint_day_names (GTK_WIDGET (calendar));
	
	if (private_data->dirty_week)
	  if (GTK_WIDGET_DRAWABLE (calendar))
	    gtk_calendar_paint_week_numbers (GTK_WIDGET (calendar));
	
	if (private_data->dirty_main)
	  if (GTK_WIDGET_DRAWABLE (calendar))
	    gtk_calendar_paint_main (GTK_WIDGET (calendar));
      }
}

static void
gtk_calendar_set_background (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint i;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      for (i = 0; i < 4; i++)
	{
	  if (private_data->arrow_win[i])
	    gdk_window_set_background (private_data->arrow_win[i], 
				       HEADER_BG_COLOR (widget));
	}
      if (private_data->header_win)
	gdk_window_set_background (private_data->header_win, 
				   HEADER_BG_COLOR (widget));
      if (private_data->day_name_win)
	gdk_window_set_background (private_data->day_name_win, 
				   BACKGROUND_COLOR (widget));
      if (private_data->week_win)
	gdk_window_set_background (private_data->week_win,
				   BACKGROUND_COLOR (widget));
      if (private_data->main_win)
	gdk_window_set_background (private_data->main_win,
				   BACKGROUND_COLOR (widget));
      if (widget->window)
	gdk_window_set_background (widget->window,
				   BACKGROUND_COLOR (widget)); 
    }
}

static void
gtk_calendar_style_set (GtkWidget *widget,
			GtkStyle  *previous_style)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  if (previous_style && GTK_WIDGET_REALIZED (widget))
    gtk_calendar_set_background(widget);
}

static void
gtk_calendar_state_changed (GtkWidget	   *widget,
			    GtkStateType    previous_state)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  gtk_calendar_set_background (widget);
}

static gint
gtk_calendar_focus_in (GtkWidget	 *widget,
		       GdkEventFocus	 *event)
{
  GtkCalendar *calendar;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CALENDAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  calendar = GTK_CALENDAR (widget);
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);
  gtk_calendar_paint_day (widget, calendar->focus_row, calendar->focus_col);
  
  return FALSE;
}

static gint
gtk_calendar_focus_out (GtkWidget	  *widget,
			GdkEventFocus	  *event)
{
  GtkCalendar *calendar;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CALENDAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  calendar = GTK_CALENDAR (widget);
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);
  gtk_calendar_paint_day (widget, calendar->focus_row, calendar->focus_col);
  
  return FALSE;
}

static void
gtk_calendar_destroy (GtkObject *object)
{
  GtkCalendarPrivateData *private_data;
  private_data = GTK_CALENDAR_PRIVATE_DATA (object);
  g_free (private_data);
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static gint
gtk_calendar_key_press (GtkWidget   *widget,
			GdkEventKey *event)
{
  GtkCalendar *calendar;
  gint return_val;
  gint old_focus_row;
  gint old_focus_col;
  gint row, col, day;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CALENDAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  calendar = GTK_CALENDAR (widget);
  return_val = FALSE;
  
  old_focus_row = calendar->focus_row;
  old_focus_col = calendar->focus_col;

  switch (event->keyval)
    {
    case GDK_Left:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_calendar_set_month_prev (calendar);
	}
      else
	{
	  if (calendar->focus_col > 0)
	    {
	      calendar->focus_col--;
	    }
	  else if (calendar->focus_row > 0)
	    {
	      calendar->focus_col = 6;
	      calendar->focus_row--;
	    }
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_paint_day (widget, calendar->focus_row,
				  calendar->focus_col);
	}
      break;
    case GDK_Right:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	{
	gtk_calendar_set_month_next (calendar);
	}
      else
	{
	  if (calendar->focus_col < 6)
	    {
	      calendar->focus_col++;
	    }
	  else if (calendar->focus_row < 5)
	    {
	      calendar->focus_col = 0;
	      calendar->focus_row++;
	    }
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_paint_day (widget, calendar->focus_row,
				  calendar->focus_col);
	}
      break;
    case GDK_Up:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_calendar_set_year_prev (calendar);
	}
      else
	{
	  if (calendar->focus_row > 0)
	    {
	      calendar->focus_row--;
	    }
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_paint_day (widget, calendar->focus_row,
				  calendar->focus_col);
	}
      break;
    case GDK_Down:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_calendar_set_year_next (calendar);
	}
      else
	{
	  if (calendar->focus_row < 5)
	    {
	      calendar->focus_row++;
	    }
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_paint_day (widget, calendar->focus_row,
				  calendar->focus_col);
	}
      break;
    case GDK_space:
      row = calendar->focus_row;
      col = calendar->focus_col;
      day = calendar->day[row][col];
      
      if (row > -1 && col > -1)
	{
	  gtk_calendar_freeze (calendar);	  

	  if (calendar->day_month[row][col] == MONTH_PREV)
	    {
	      gtk_calendar_set_month_prev (calendar);
	    }
	  else if (calendar->day_month[row][col] == MONTH_NEXT)
	    {
	      gtk_calendar_set_month_next (calendar);
	    }

	  gtk_calendar_select_day (calendar, day);
	  
	  for (row = 0; row < 6; row ++)
	    for (col = 0; col < 7; col++)
	      {
		if (calendar->day_month[row][col] == MONTH_CURRENT 
		    && calendar->day[row][col] == day)
		  {
		    calendar->focus_row = row;
		    calendar->focus_col = col;
		  }
	      }
	  gtk_calendar_thaw (calendar);	  
	}
    }	
  
  return return_val;
}
