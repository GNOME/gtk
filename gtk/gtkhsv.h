/* HSV color selector for GTK+
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Simon Budig <Simon.Budig@unix-ag.org> (original code)
 *          Federico Mena-Quintero <federico@gimp.org> (cleanup for GTK+)
 *          Jonathan Blandford <jrb@redhat.com> (cleanup for GTK+)
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_HSV_H__
#define __GTK_HSV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtkwidget.h>



#define GTK_TYPE_HSV            (gtk_hsv_get_type ())
#define GTK_HSV(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_HSV, GtkHSV))
#define GTK_HSV_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_HSV, GtkHSV))
#define GTK_IS_HSV(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_HSV))
#define GTK_IS_HSV_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_HSV))


typedef struct _GtkHSV GtkHSV;
typedef struct _GtkHSVClass GtkHSVClass;

struct _GtkHSV {
	GtkWidget widget;

	/* Private data */
	gpointer priv;
};

struct _GtkHSVClass {
	GtkWidgetClass parent_class;

	/* Notification signals */

	void (* changed) (GtkHSV *hsv);
};


GtkType gtk_hsv_get_type (void);

GtkWidget *gtk_hsv_new (void);

void gtk_hsv_set_color (GtkHSV *hsv, double h, double s, double v);
void gtk_hsv_get_color (GtkHSV *hsv, double *h, double *s, double *v);

void gtk_hsv_set_metrics (GtkHSV *hsv, int size, int ring_width);
void gtk_hsv_get_metrics (GtkHSV *hsv, int *size, int *ring_width);

gboolean gtk_hsv_is_adjusting (GtkHSV *hsv);

void gtk_hsv_to_rgb (double h, double s, double v, double *r, double *g, double *b);
void gtk_rgb_to_hsv (double r, double g, double b, double *h, double *s, double *v);



#ifdef __cplusplus
}
#endif

#endif
