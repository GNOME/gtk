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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_DATE_CHOOSER_DAY_PRIVATE_H__
#define __GTK_DATE_CHOOSER_DAY_PRIVATE_H__

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GTK_TYPE_DATE_CHOOSER_DAY                  (gtk_date_chooser_day_get_type ())
#define GTK_DATE_CHOOSER_DAY(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_DATE_CHOOSER_DAY, GtkDateChooserDay))
#define GTK_DATE_CHOOSER_DAY_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_DATE_CHOOSER_DAY, GtkDateChooserDayClass))
#define GTK_IS_DATE_CHOOSER_DAY(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_DATE_CHOOSER_DAY))
#define GTK_IS_DATE_CHOOSER_DAY_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_DATE_CHOOSER_DAY))
#define GTK_DATE_CHOOSER_DAY_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_DATE_CHOOSER_DAY, GtkDateChooserDayClass))


typedef struct _GtkDateChooserDay        GtkDateChooserDay;
typedef struct _GtkDateChooserDayClass   GtkDateChooserDayClass;

GType       gtk_date_chooser_day_get_type        (void) G_GNUC_CONST;
GtkWidget * gtk_date_chooser_day_new             (void);
void        gtk_date_chooser_day_set_date        (GtkDateChooserDay *day,
                                                  GDateTime         *date);
GDateTime  *gtk_date_chooser_day_get_date        (GtkDateChooserDay *day);
void        gtk_date_chooser_day_set_other_month (GtkDateChooserDay *day,
                                                  gboolean           other_month);
void        gtk_date_chooser_day_set_selected    (GtkDateChooserDay *day,
                                                  gboolean           selected);

G_END_DECLS

#endif /* __GTK_DATE_CHOOSER_DAY_PRIVATE_H__ */
