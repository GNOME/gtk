/* GTK - The GIMP Toolkit
 * Copyright (C) 1997 David Mosberger
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
#ifndef __GTK_GAMMA_CURVE_H__
#define __GTK_GAMMA_CURVE_H__


#include <gdk/gdk.h>
#include <gtk/gtkvbox.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_GAMMA_CURVE(obj) \
   GTK_CHECK_CAST (obj, gtk_gamma_curve_get_type (), GtkGammaCurve)
#define GTK_GAMMA_CURVE_CLASS(klass) \
   GTK_CHECK_CLASS_CAST (klass, gtk_gamma_curve_get_type, GtkGammaCurveClass)
#define GTK_IS_GAMMA_CURVE(obj) \
   GTK_CHECK_TYPE (obj, gtk_gamma_curve_get_type ())


typedef struct _GtkGammaCurve		GtkGammaCurve;
typedef struct _GtkGammaCurveClass	GtkGammaCurveClass;


struct _GtkGammaCurve
{
  GtkVBox vbox;

  GtkWidget *table;
  GtkWidget *curve;
  GtkWidget *button[5];	/* spline, linear, free, gamma, reset */

  gfloat gamma;
  GtkWidget *gamma_dialog;
  GtkWidget *gamma_text;
};

struct _GtkGammaCurveClass
{
  GtkVBoxClass parent_class;
};


guint      gtk_gamma_curve_get_type (void);
GtkWidget* gtk_gamma_curve_new      (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_GAMMA_CURVE_H__ */
