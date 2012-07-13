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

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_ICON_HELPER _gtk_icon_helper_get_type()

#define GTK_ICON_HELPER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   GTK_TYPE_ICON_HELPER, GtkIconHelper))

#define GTK_ICON_HELPER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   GTK_TYPE_ICON_HELPER, GtkIconHelperClass))

#define GTK_IS_ICON_HELPER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   GTK_TYPE_ICON_HELPER))

#define GTK_IS_ICON_HELPER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   GTK_TYPE_ICON_HELPER))

#define GTK_ICON_HELPER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   GTK_TYPE_ICON_HELPER, GtkIconHelperClass))

typedef struct _GtkIconHelper GtkIconHelper;
typedef struct _GtkIconHelperClass GtkIconHelperClass;
typedef struct _GtkIconHelperPrivate GtkIconHelperPrivate;

struct _GtkIconHelper
{
  GObject parent;

  GtkIconHelperPrivate *priv;
};

struct _GtkIconHelperClass
{
  GObjectClass parent_class;
};

GType _gtk_icon_helper_get_type (void) G_GNUC_CONST;

GtkIconHelper *_gtk_icon_helper_new (void);

void _gtk_icon_helper_clear (GtkIconHelper *self);
void _gtk_icon_helper_invalidate (GtkIconHelper *self);

gboolean _gtk_icon_helper_get_is_empty (GtkIconHelper *self);

void _gtk_icon_helper_set_gicon (GtkIconHelper *self,
                                 GIcon *gicon,
                                 GtkIconSize icon_size);
void _gtk_icon_helper_set_pixbuf (GtkIconHelper *self,
                                  GdkPixbuf *pixbuf);
void _gtk_icon_helper_set_animation (GtkIconHelper *self,
                                     GdkPixbufAnimation *animation);
void _gtk_icon_helper_set_icon_set (GtkIconHelper *self,
                                    GtkIconSet *icon_set,
                                    GtkIconSize icon_size);

void _gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                     const gchar *icon_name,
                                     GtkIconSize icon_size);
void _gtk_icon_helper_set_stock_id (GtkIconHelper *self,
                                    const gchar *stock_id,
                                    GtkIconSize icon_size);

void _gtk_icon_helper_set_icon_size (GtkIconHelper *self,
                                     GtkIconSize icon_size);
void _gtk_icon_helper_set_pixel_size (GtkIconHelper *self,
                                      gint pixel_size);
void _gtk_icon_helper_set_use_fallback (GtkIconHelper *self,
                                        gboolean use_fallback);

GtkImageType _gtk_icon_helper_get_storage_type (GtkIconHelper *self);
GtkIconSize _gtk_icon_helper_get_icon_size (GtkIconHelper *self);
gint _gtk_icon_helper_get_pixel_size (GtkIconHelper *self);
gboolean _gtk_icon_helper_get_use_fallback (GtkIconHelper *self);

GdkPixbuf *_gtk_icon_helper_peek_pixbuf (GtkIconHelper *self);
GIcon *_gtk_icon_helper_peek_gicon (GtkIconHelper *self);
GtkIconSet *_gtk_icon_helper_peek_icon_set (GtkIconHelper *self);
GdkPixbufAnimation *_gtk_icon_helper_peek_animation (GtkIconHelper *self);

const gchar *_gtk_icon_helper_get_stock_id (GtkIconHelper *self);
const gchar *_gtk_icon_helper_get_icon_name (GtkIconHelper *self);

GdkPixbuf *_gtk_icon_helper_ensure_pixbuf (GtkIconHelper *self,
                                           GtkStyleContext *context);
void _gtk_icon_helper_get_size (GtkIconHelper *self,
                                GtkStyleContext *context,
                                gint *width_out,
                                gint *height_out);

void _gtk_icon_helper_draw (GtkIconHelper *self,
                            GtkStyleContext *context,
                            cairo_t *cr,
                            gdouble x,
                            gdouble y);

gboolean _gtk_icon_helper_get_force_scale_pixbuf (GtkIconHelper *self);
void     _gtk_icon_helper_set_force_scale_pixbuf (GtkIconHelper *self,
                                                  gboolean       force_scale);


G_END_DECLS

#endif /* __GTK_ICON_HELPER_H__ */
