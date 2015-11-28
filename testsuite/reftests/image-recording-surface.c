/*
 * Copyright (C) 2014 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>

G_MODULE_EXPORT void
image_recording_surface_set (GtkWidget *widget,
                             gpointer   unused)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_rectangle_t rect;

  pixbuf = gdk_pixbuf_new_from_resource ("/org/gtk/libgtk/inspector/logo.png", &error);
  g_assert_no_error (error);
  rect.x = 0;
  rect.y = 0;
  rect.width = gdk_pixbuf_get_width (pixbuf);
  rect.height = gdk_pixbuf_get_height (pixbuf);
  surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, &rect);

  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  gtk_image_set_from_surface (GTK_IMAGE (widget), surface);

  cairo_surface_destroy (surface);
  g_object_unref (pixbuf);
}
