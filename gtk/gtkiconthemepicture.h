/* GTK - The GIMP Drawing Kit
 * Copyright (C) 2010 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_ICON_THEME_PICTURE_H__
#define __GTK_ICON_THEME_PICTURE_H__

#include <gdk/gdk.h>
#include <gtk/gtkicontheme.h>


G_BEGIN_DECLS

#define GTK_TYPE_ICON_THEME_PICTURE              (gtk_icon_theme_picture_get_type ())
#define GTK_ICON_THEME_PICTURE(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_ICON_THEME_PICTURE, GtkIconThemePicture))
#define GTK_ICON_THEME_PICTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ICON_THEME_PICTURE, GtkIconThemePictureClass))
#define GTK_IS_ICON_THEME_PICTURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_ICON_THEME_PICTURE))
#define GTK_IS_ICON_THEME_PICTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ICON_THEME_PICTURE))
#define GTK_ICON_THEME_PICTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ICON_THEME_PICTURE, GtkIconThemePictureClass))

typedef struct _GtkIconThemePicture        GtkIconThemePicture;
typedef struct _GtkIconThemePicturePrivate GtkIconThemePicturePrivate;
typedef struct _GtkIconThemePictureClass   GtkIconThemePictureClass;

struct _GtkIconThemePicture {
  GdkPicture                  parent_picture;

  GtkIconThemePicturePrivate *priv;
};

struct _GtkIconThemePictureClass {
  GdkPictureClass             parent_class;

  GtkIconInfo *               (* lookup)                (GtkIconThemePicture * picture,
                                                         GtkIconTheme *        theme,
                                                         int                   size,
                                                         GtkIconLookupFlags    flags);
};

GType           gtk_icon_theme_picture_get_type         (void) G_GNUC_CONST;

GtkIconSize     gtk_icon_theme_picture_get_size         (GtkIconThemePicture * picture);
void            gtk_icon_theme_picture_set_size         (GtkIconThemePicture * picture,
                                                         GtkIconSize           size);
int             gtk_icon_theme_picture_get_pixel_size   (GtkIconThemePicture * picture);
void            gtk_icon_theme_picture_set_pixel_size   (GtkIconThemePicture * picture,
                                                         int                   pixel_size);
GtkIconTheme *  gtk_icon_theme_picture_get_icon_theme   (GtkIconThemePicture * picture);
void            gtk_icon_theme_picture_set_icon_theme   (GtkIconThemePicture * picture,
                                                         GtkIconTheme *        theme);
gboolean        gtk_icon_theme_picture_get_use_fallback (GtkIconThemePicture * picture);
void            gtk_icon_theme_picture_set_use_fallback (GtkIconThemePicture * picture,
                                                         gboolean              use_fallback);

/* for subclasses only */
void            gtk_icon_theme_picture_update           (GtkIconThemePicture * picture);

G_END_DECLS

#endif /* __GTK_ICON_THEME_PICTURE_H__ */
