/* GTK - The GIMP Toolkit
 *
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_COLOR_CHOOSER_H__
#define __GTK_COLOR_CHOOSER_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_CHOOSER                  (gtk_color_chooser_get_type ())
#define GTK_COLOR_CHOOSER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COLOR_CHOOSER, GtkColorChooser))
#define GTK_IS_COLOR_CHOOSER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_CHOOSER))
#define GTK_COLOR_CHOOSER_GET_IFACE(inst)       (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_COLOR_CHOOSER, GtkColorChooserInterface))

typedef struct _GtkColorChooser          GtkColorChooser;
typedef struct _GtkColorChooserInterface GtkColorChooserInterface;

struct _GtkColorChooserInterface
{
  GTypeInterface base_interface;

  /* Methods */
  void (* get_color) (GtkColorChooser *chooser,
                      GdkRGBA         *color);
  void (* set_color) (GtkColorChooser *chooser,
                      const GdkRGBA   *color);

  /* Signals */
  void (* color_activated) (GtkColorChooser *chooser,
                            const GdkRGBA   *color);

  /* Padding */
  gpointer padding[12];
};

GType  gtk_color_chooser_get_type  (void) G_GNUC_CONST;

void   gtk_color_chooser_get_color (GtkColorChooser *chooser,
                                    GdkRGBA         *color);
void   gtk_color_chooser_set_color (GtkColorChooser *chooser,
                                    const GdkRGBA   *color);

G_END_DECLS

#endif /* __GTK_COLOR_CHOOSER_H__ */
