/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GTK Calendar Widget
 * Copyright (C) 1998 Cesar Miquel and Shawn T. Amundson
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
#ifndef __GTK_CALENDAR_H__
#define __GTK_CALENDAR_H__

#include <gdk/gdk.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define GTK_TYPE_CALENDAR		   (gtk_calendar_get_type ())
#define GTK_CALENDAR(obj)		   (GTK_CHECK_CAST ((obj), GTK_TYPE_CALENDAR, GtkCalendar))
#define GTK_CALENDAR_CLASS(klass)	   (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CALENDAR, GtkCalendarClass))
#define GTK_IS_CALENDAR(obj)		   (GTK_CHECK_TYPE ((obj), GTK_TYPE_CALENDAR))
#define GTK_IS_CALENDAR_CLASS(klass)	   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CALENDAR))


typedef struct _GtkCalendar	       GtkCalendar;
typedef struct _GtkCalendarClass       GtkCalendarClass;

typedef enum
{
  GTK_CALENDAR_SHOW_HEADING		= 1 << 0,
  GTK_CALENDAR_SHOW_DAY_NAMES		= 1 << 1,
  GTK_CALENDAR_NO_MONTH_CHANGE		= 1 << 2,
  GTK_CALENDAR_SHOW_WEEK_NUMBERS	= 1 << 3,
  GTK_CALENDAR_WEEK_START_MONDAY	= 1 << 4
} GtkCalendarDisplayOptions;

struct _GtkCalendar
{
  /* This widget is derived from GtkWidget */
  GtkWidget widget;
  
  GdkWindow *header_win, *day_name_win, *main_win, *week_win;
  gint header_h, day_name_h, main_h;
  
  GtkStyle  *header_style;
  GtkStyle  *label_style;
  
  gint month;
  gint year;
  gint selected_day;
  
  gint day_month[6][7];
  gint day[6][7];
  
  gint num_marked_dates;
  gint marked_date[31];
  GtkCalendarDisplayOptions  display_flags;
  GdkColor marked_date_color[31];
  
  /* Header Information */
  GdkWindow *arrow_win[4];
  gint	     arrow_state[4];
  gint	     arrow_width;
  gint	     max_month_width;
  gint	     max_year_width;
  
  /* Other info */
  gint calstarty, calnumrows;
  
  /* Style parameters for this widget */
  GdkGC *gc;
  GdkCursor *cross;
  
  gint day_width;
  gint week_width;
  GdkRectangle header_button[4];
  GdkRectangle rect_days[6][7];
  
  gint highlight_row;
  gint highlight_col;
  
  gint min_day_width;
  gint max_day_char_width;
  gint max_day_char_ascent;
  gint max_day_char_descent;
  gint max_label_char_ascent;
  gint max_label_char_descent;
  gint max_week_char_width;
  /* flags */
  unsigned int dirty_header:1;
  unsigned int dirty_day_names:1;
  unsigned int dirty_main:1;
  unsigned int dirty_week:1;
  unsigned int frozen;
  
  gint week_num_w;
  gint font_width_day_name, font_width_day;
  char grow_space [32];
};

struct _GtkCalendarClass
{
  GtkWidgetClass parent_class;
  
  /* Signal handlers */
  void (* month_changed)		(GtkCalendarClass *calendar);
  void (* day_selected)			(GtkCalendarClass *calendar);
  void (* day_selected_double_click)	(GtkCalendarClass *calendar);
  void (* prev_month)			(GtkCalendarClass *calendar);
  void (* next_month)			(GtkCalendarClass *calendar);
  void (* prev_year)			(GtkCalendarClass *calendar);
  void (* next_year)			(GtkCalendarClass *calendar);
  
};


GtkType	   gtk_calendar_get_type	(void);
GtkWidget* gtk_calendar_new		(void);

gint	   gtk_calendar_select_month	(GtkCalendar *calendar, 
					 guint	      month,
					 guint	      year);
void	   gtk_calendar_select_day	(GtkCalendar *calendar,
					 guint	      day);

gint	   gtk_calendar_mark_day	(GtkCalendar *calendar,
					 guint	      day);
gint	   gtk_calendar_unmark_day	(GtkCalendar *calendar,
					 guint	      day);
void	   gtk_calendar_clear_marks	(GtkCalendar *calendar);


void	   gtk_calendar_display_options (GtkCalendar		  *calendar,
					 GtkCalendarDisplayOptions flags);

void	   gtk_calendar_get_date	(GtkCalendar *calendar, 
					 guint	     *year,
					 guint	     *month,
					 guint	     *day);
void	   gtk_calendar_freeze		(GtkCalendar *calendar);
void	   gtk_calendar_thaw		(GtkCalendar *calendar);




#ifdef __cplusplus
#pragma {
}
#endif /* __cplusplus */

#endif /* __GTK_CALENDAR_H__ */
