/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
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

#ifndef __GTK_ICON_HELPER_H__
#define __GTK_ICON_HELPER_H__

#include "gtk/gtkimage.h"
#include "gtk/gtktypes.h"

#include "gtkcssnodeprivate.h"
#include "gtkimagedefinitionprivate.h"

G_BEGIN_DECLS

typedef struct _GtkIconHelper GtkIconHelper;

struct _GtkIconHelper
{
  GObject parent_instance;

  GtkImageDefinition *def;

  GtkIconSize icon_size;
  gint pixel_size;

  guint use_fallback : 1;
  guint force_scale_pixbuf : 1;
  guint rendered_surface_is_symbolic : 1;

  GtkWidget *owner;
  GtkCssNode *node;
  cairo_surface_t *rendered_surface;
  GdkTexture *texture;
};

void gtk_icon_helper_init (GtkIconHelper *self,
                           GtkCssNode    *css_node,
                           GtkWidget     *owner);

void gtk_icon_helper_destroy (GtkIconHelper *self);

void _gtk_icon_helper_clear (GtkIconHelper *self);

gboolean _gtk_icon_helper_get_is_empty (GtkIconHelper *self);

void _gtk_icon_helper_set_definition (GtkIconHelper *self,
                                      GtkImageDefinition *def);
void _gtk_icon_helper_set_gicon (GtkIconHelper *self,
                                 GIcon *gicon,
                                 GtkIconSize icon_size);

void _gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                     const gchar *icon_name,
                                     GtkIconSize icon_size);
void _gtk_icon_helper_set_surface (GtkIconHelper *self,
				   cairo_surface_t *surface);
void _gtk_icon_helper_set_texture (GtkIconHelper *self,
				   GdkTexture *texture);

gboolean _gtk_icon_helper_set_icon_size    (GtkIconHelper *self,
                                            GtkIconSize    icon_size);
gboolean _gtk_icon_helper_set_pixel_size   (GtkIconHelper *self,
                                            gint           pixel_size);
gboolean _gtk_icon_helper_set_use_fallback (GtkIconHelper *self,
                                            gboolean       use_fallback);

GtkImageType _gtk_icon_helper_get_storage_type (GtkIconHelper *self);
GtkIconSize _gtk_icon_helper_get_icon_size (GtkIconHelper *self);
gint _gtk_icon_helper_get_pixel_size (GtkIconHelper *self);
gboolean _gtk_icon_helper_get_use_fallback (GtkIconHelper *self);

GIcon *_gtk_icon_helper_peek_gicon (GtkIconHelper *self);
cairo_surface_t *_gtk_icon_helper_peek_surface (GtkIconHelper *self);
GdkTexture *_gtk_icon_helper_peek_texture (GtkIconHelper *self);

GtkImageDefinition *gtk_icon_helper_get_definition (GtkIconHelper *self);
const gchar *_gtk_icon_helper_get_icon_name (GtkIconHelper *self);

void _gtk_icon_helper_get_size (GtkIconHelper *self,
                                gint *width_out,
                                gint *height_out);

void gtk_icon_helper_snapshot (GtkIconHelper *self,
                               GtkSnapshot *snapshot);

gboolean _gtk_icon_helper_get_force_scale_pixbuf (GtkIconHelper *self);
void     _gtk_icon_helper_set_force_scale_pixbuf (GtkIconHelper *self,
                                                  gboolean       force_scale);

void      gtk_icon_helper_invalidate (GtkIconHelper *self);
void      gtk_icon_helper_invalidate_for_change (GtkIconHelper     *self,
                                                 GtkCssStyleChange *change);

G_END_DECLS

#endif /* __GTK_ICON_HELPER_H__ */
