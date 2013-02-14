/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#ifndef __GTK_COLOR_SCALE_H__
#define __GTK_COLOR_SCALE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkscale.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_SCALE            (gtk_color_scale_get_type ())
#define GTK_COLOR_SCALE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COLOR_SCALE, GtkColorScale))
#define GTK_COLOR_SCALE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_COLOR_SCALE, GtkColorScaleClass))
#define GTK_IS_COLOR_SCALE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_SCALE))
#define GTK_IS_COLOR_SCALE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_COLOR_SCALE))
#define GTK_COLOR_SCALE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_COLOR_SCALE, GtkColorScaleClass))


typedef struct _GtkColorScale         GtkColorScale;
typedef struct _GtkColorScaleClass    GtkColorScaleClass;
typedef struct _GtkColorScalePrivate  GtkColorScalePrivate;

struct _GtkColorScale
{
  GtkScale parent_instance;

  GtkColorScalePrivate *priv;
};

struct _GtkColorScaleClass
{
  GtkScaleClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

typedef enum
{
  GTK_COLOR_SCALE_HUE,
  GTK_COLOR_SCALE_ALPHA
} GtkColorScaleType;

G_GNUC_INTERNAL
GType       gtk_color_scale_get_type (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkWidget * gtk_color_scale_new      (GtkAdjustment     *adjustment,
                                      GtkColorScaleType  type);

G_GNUC_INTERNAL
void        gtk_color_scale_set_rgba (GtkColorScale     *scale,
                                      const GdkRGBA     *color);

G_END_DECLS

#endif /* __GTK_COLOR_SCALE_H__ */
