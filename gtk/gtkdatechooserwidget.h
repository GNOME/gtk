/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_DATE_CHOOSER_WIDGET_H__
#define __GTK_DATE_CHOOSER_WIDGET_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

#ifndef GDK_AVAILABLE_IN_3_20
#define GDK_AVAILABLE_IN_3_20 _GDK_EXTERN
#endif

G_BEGIN_DECLS

#define GTK_TYPE_DATE_CHOOSER_WIDGET              (gtk_date_chooser_widget_get_type ())
#define GTK_DATE_CHOOSER_WIDGET(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_DATE_CHOOSER_WIDGET, GtkDateChooserWidget))
#define GTK_DATE_CHOOSER_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_DATE_CHOOSER_WIDGET, GtkDateChooserWidgetClass))
#define GTK_IS_DATE_CHOOSER_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_DATE_CHOOSER_WIDGET))
#define GTK_IS_DATE_CHOOSER_WIDGET_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_DATE_CHOOSER_WIDGET))
#define GTK_DATE_CHOOSER_WIDGET_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_DATE_CHOOSER_WIDGET, GtkDateChooserWidgetClass))

typedef struct _GtkDateChooserWidget        GtkDateChooserWidget;
typedef struct _GtkDateChooserWidgetClass   GtkDateChooserWidgetClass;

GDK_AVAILABLE_IN_3_20
GType       gtk_date_chooser_widget_get_type              (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_20
GtkWidget * gtk_date_chooser_widget_new                   (void);

GDK_AVAILABLE_IN_3_20
void        gtk_date_chooser_widget_set_date              (GtkDateChooserWidget *chooser,
                                                           GDateTime            *dt);
GDK_AVAILABLE_IN_3_20
GDateTime * gtk_date_chooser_widget_get_date              (GtkDateChooserWidget *chooser);

typedef enum {
  GTK_DATE_CHOOSER_DAY_NONE    = 0,
  GTK_DATE_CHOOSER_DAY_WEEKEND = 1,
  GTK_DATE_CHOOSER_DAY_HOLIDAY = 2,
  GTK_DATE_CHOOSER_DAY_MARKED  = 4
} GtkDateChooserDayOptions;

typedef GtkDateChooserDayOptions (* GtkDateChooserDayOptionsCallback) (GtkDateChooserWidget *chooser,
                                                                       GDateTime            *date,
                                                                       gpointer              data);
GDK_AVAILABLE_IN_3_20
void        gtk_date_chooser_widget_set_day_options_callback      (GtkDateChooserWidget      *chooser,
                                                                   GtkDateChooserDayOptionsCallback  callback,
                                                                   gpointer                   data,
                                                                   GDestroyNotify             destroy);
GDK_AVAILABLE_IN_3_20
void        gtk_date_chooser_widget_invalidate_day_options (GtkDateChooserWidget *widget);

GDK_AVAILABLE_IN_3_20
void        gtk_date_chooser_widget_set_no_month_change   (GtkDateChooserWidget *chooser,
                                                           gboolean              setting);

GDK_AVAILABLE_IN_3_20
void        gtk_date_chooser_widget_set_show_heading      (GtkDateChooserWidget *chooser,
                                                           gboolean              setting);
GDK_AVAILABLE_IN_3_20
gboolean    gtk_date_chooser_widget_get_show_heading      (GtkDateChooserWidget *chooser);
GDK_AVAILABLE_IN_3_20
void        gtk_date_chooser_widget_set_show_day_names    (GtkDateChooserWidget *chooser,
                                                           gboolean              setting);
GDK_AVAILABLE_IN_3_20
gboolean    gtk_date_chooser_widget_get_show_day_names    (GtkDateChooserWidget *chooser);

GDK_AVAILABLE_IN_3_20
void        gtk_date_chooser_widget_set_show_week_numbers (GtkDateChooserWidget *chooser,
                                                           gboolean              setting);
GDK_AVAILABLE_IN_3_20
gboolean    gtk_date_chooser_widget_get_show_week_numbers (GtkDateChooserWidget *chooser);

G_END_DECLS

#endif /* __GTK_DATE_CHOOSER_WIDGET_H__ */
