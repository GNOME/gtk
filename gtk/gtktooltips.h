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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_TOOLTIPS_H__
#define __GTK_TOOLTIPS_H__

#include <gdk/gdk.h>
#include <gtk/gtkdata.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TOOLTIPS(obj)	   GTK_CHECK_CAST (obj, gtk_tooltips_get_type (), GtkTooltips)
#define GTK_TOOLTIPS_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_tooltips_get_type (), GtkTooltipsClass)
#define GTK_IS_TOOLTIPS(obj)	   GTK_CHECK_TYPE (obj, gtk_tooltips_get_type ())

typedef struct _GtkTooltips	 GtkTooltips;
typedef struct _GtkTooltipsClass GtkTooltipsClass;
typedef struct _GtkTooltipsData	 GtkTooltipsData;

struct _GtkTooltipsData
{
  GtkTooltips *tooltips;
  GtkWidget *widget;
  gchar *tip_text;
  gchar *tip_private;
  GdkFont *font;
  gint width;
  GList *row;
};

struct _GtkTooltips
{
  GtkData data;

  GtkWidget *tip_window;
  GtkTooltipsData *active_widget;
  GList *widget_list;

  GdkGC *gc;
  GdkColor *foreground;
  GdkColor *background;

  gint16 delay;
  gint	 enabled : 1;
  gint	 timer_active : 1;
  gint	 timer_tag;
};

struct _GtkTooltipsClass
{
  GtkDataClass parent_class;
};

GtkType		 gtk_tooltips_get_type	 (void);
GtkTooltips*	 gtk_tooltips_new	 (void);

void		 gtk_tooltips_enable	 (GtkTooltips     *tooltips);
void		 gtk_tooltips_disable	 (GtkTooltips     *tooltips);
void		 gtk_tooltips_set_delay	 (GtkTooltips     *tooltips,
					  gint	           delay);
void		 gtk_tooltips_set_tip	 (GtkTooltips     *tooltips,
					  GtkWidget	  *widget,
					  const gchar     *tip_text,
					  const gchar     *tip_private);
void		 gtk_tooltips_set_colors (GtkTooltips     *tooltips,
					  GdkColor	  *background,
					  GdkColor	  *foreground);
GtkTooltipsData* gtk_tooltips_data_get	 (GtkWidget	  *widget);

/* discouraged old function name
 */
#define	gtk_tooltips_set_tips(t,w,x)	gtk_tooltips_set_tip(t,w,x,NULL)



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TOOLTIPS_H__ */
