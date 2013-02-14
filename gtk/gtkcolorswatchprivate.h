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

#ifndef __GTK_COLOR_SWATCH_H__
#define __GTK_COLOR_SWATCH_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkdrawingarea.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_SWATCH                  (gtk_color_swatch_get_type ())
#define GTK_COLOR_SWATCH(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COLOR_SWATCH, GtkColorSwatch))
#define GTK_COLOR_SWATCH_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_COLOR_SWATCH, GtkColorSwatchClass))
#define GTK_IS_COLOR_SWATCH(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_SWATCH))
#define GTK_IS_COLOR_SWATCH_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_COLOR_SWATCH))
#define GTK_COLOR_SWATCH_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_COLOR_SWATCH, GtkColorSwatchClass))


typedef struct _GtkColorSwatch        GtkColorSwatch;
typedef struct _GtkColorSwatchClass   GtkColorSwatchClass;
typedef struct _GtkColorSwatchPrivate GtkColorSwatchPrivate;

struct _GtkColorSwatch
{
  GtkWidget parent;

  /*< private >*/
  GtkColorSwatchPrivate *priv;
};

struct _GtkColorSwatchClass
{
  GtkWidgetClass parent_class;

  void ( * activate)  (GtkColorSwatch *swatch);
  void ( * customize) (GtkColorSwatch *swatch);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


G_GNUC_INTERNAL
GType       gtk_color_swatch_get_type         (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkWidget * gtk_color_swatch_new              (void);

G_GNUC_INTERNAL
void        gtk_color_swatch_set_rgba         (GtkColorSwatch *swatch,
                                               const GdkRGBA  *color);
G_GNUC_INTERNAL
gboolean    gtk_color_swatch_get_rgba         (GtkColorSwatch *swatch,
                                               GdkRGBA        *color);
G_GNUC_INTERNAL
void        gtk_color_swatch_set_hsva         (GtkColorSwatch *swatch,
                                               gdouble         h,
                                               gdouble         s,
                                               gdouble         v,
                                               gdouble         a);
G_GNUC_INTERNAL
void        gtk_color_swatch_set_can_drop     (GtkColorSwatch *swatch,
                                               gboolean        can_drop);
G_GNUC_INTERNAL
void        gtk_color_swatch_set_icon         (GtkColorSwatch *swatch,
                                               const gchar    *icon);
G_GNUC_INTERNAL
void        gtk_color_swatch_set_use_alpha    (GtkColorSwatch *swatch,
                                               gboolean        use_alpha);
G_GNUC_INTERNAL
void        gtk_color_swatch_set_selectable   (GtkColorSwatch *swatch,
                                               gboolean        selectable);
G_GNUC_INTERNAL
gboolean    gtk_color_swatch_get_selectable   (GtkColorSwatch *swatch);

G_END_DECLS

#endif /* __GTK_COLOR_SWATCH_H__ */
