/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_TOOLTIPS_H__
#define __GTK_TOOLTIPS_H__

#include <gdk/gdk.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct
{
  GtkWidget *widget;
  gchar *tips_text;
  GdkFont *font;
  gint width;
  GList *row;
  gint old_event_mask;
} GtkTooltipsData;

typedef struct
{
  GtkWidget *tip_window;
  GtkTooltipsData *active_widget;
  GList *widget_list;

  GdkGC *gc;
  GdkColor *foreground;
  GdkColor *background;

  gint numwidgets;
  gint enabled;
  gint inside;
  gint delay;
  gint timer_tag;
  gint timer_active;

  gint ref_count;
  gint pending_destroy;
} GtkTooltips;


GtkTooltips* gtk_tooltips_new        (void);

void         gtk_tooltips_destroy    (GtkTooltips *tooltips);
GtkTooltips* gtk_tooltips_ref        (GtkTooltips *tooltips);
void         gtk_tooltips_unref      (GtkTooltips *tooltips);

void         gtk_tooltips_enable     (GtkTooltips *tooltips);

void         gtk_tooltips_disable    (GtkTooltips *tooltips);

void         gtk_tooltips_set_delay  (GtkTooltips *tooltips,
                                      gint         delay);

void         gtk_tooltips_set_tips   (GtkTooltips *tooltips,
                                      GtkWidget   *widget,
                                      const gchar *tips_text);

void         gtk_tooltips_set_colors (GtkTooltips *tooltips,
                                      GdkColor    *background,
                                      GdkColor    *foreground);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TOOLTIPS_H__ */
