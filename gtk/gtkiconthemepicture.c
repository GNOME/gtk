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

#include "config.h"

#include "gtkiconthemepicture.h"

#include <cairo-gobject.h>

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkstyledpicture.h"

struct _GtkIconThemePicturePrivate {
  GdkPicture *picture;
  GtkIconTheme *theme;
  GtkIconSize size;
  int pixel_size;
  gboolean use_fallback;
};

enum {
  PROP_0,
  PROP_ICON_THEME,
  PROP_SIZE,
  PROP_PIXEL_SIZE
};

static int
gtk_icon_theme_picture_lookup_size (GtkSettings *settings,
                                    GtkIconSize  icon_size)
{
  int width, height;

  if (gtk_icon_size_lookup_for_settings (settings,
					 icon_size,
					 &width, &height))
    return MIN (width, height);

  if (icon_size != -1)
    {
      g_warning ("Invalid icon size %d\n", icon_size);
      return 24;
    }

  return 48;
}

static GdkPixbuf *
gtk_icon_theme_picture_get_pixbuf_for_widget (GtkIconThemePicture *picture,
                                              GtkWidget *          widget)
{
  GtkIconThemePicturePrivate *priv = picture->priv;
  GtkIconTheme *icon_theme;
  GdkScreen *screen;
  GtkSettings *settings;
  gint size;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;
  GdkPixbuf *pixbuf;

  screen = widget ? gtk_widget_get_screen (widget) : gdk_screen_get_default ();
  icon_theme = priv->theme ? priv->theme : gtk_icon_theme_get_for_screen (screen);
  settings = gtk_settings_get_for_screen (screen);

  flags = GTK_ICON_LOOKUP_USE_BUILTIN;
  if (priv->use_fallback)
    flags |= GTK_ICON_LOOKUP_GENERIC_FALLBACK;

  if (priv->pixel_size != -1)
    {
      size = priv->pixel_size;
      flags |= GTK_ICON_LOOKUP_FORCE_SIZE;
    }
  else
    size = gtk_icon_theme_picture_lookup_size (settings, priv->size);

  info = GTK_ICON_THEME_PICTURE_GET_CLASS (picture)->lookup (picture,
                                                             icon_theme,
                                                             size,
                                                             flags);

  if (info)
    {
      if (widget)
        {
          GtkStyleContext *context;

          context = gtk_widget_get_style_context (widget);
          gtk_style_context_save (context);
          gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));
          pixbuf = gtk_icon_info_load_symbolic_for_context (info,
                                                            context,
                                                            NULL, NULL);
          gtk_style_context_restore (context);
        }
      else
        {
          static const GdkRGBA foreground =    { 0.0,  0.0,  0.0,  1.0 };
          static const GdkRGBA success_color = { 0.3,  0.6,  0.02, 1.0 };
          static const GdkRGBA warning_color = { 0.96, 0.47, 0.24, 1.0 };
          static const GdkRGBA error_color =   { 0.8,  0.0,  0.0,  1.0 };

          pixbuf = gtk_icon_info_load_symbolic (info,
                                                &foreground,
                                                &success_color,
                                                &warning_color,
                                                &error_color,
                                                NULL, NULL);
        }

      gtk_icon_info_free (info);
    }
  else
    pixbuf = NULL;

  if (pixbuf == NULL && widget != NULL)
    {
      pixbuf = gtk_widget_render_icon_pixbuf (widget,
                                              GTK_STOCK_MISSING_IMAGE,
                                              priv->size);
    }

  return pixbuf;
}

void
gtk_icon_theme_picture_update (GtkIconThemePicture *picture)
{
  GtkIconThemePicturePrivate *priv = picture->priv;
  GdkPixbuf *pixbuf;
  
  pixbuf = gtk_icon_theme_picture_get_pixbuf_for_widget (picture, NULL);
  gdk_pixbuf_picture_set_pixbuf (GDK_PIXBUF_PICTURE (priv->picture), pixbuf);
  if (pixbuf)
    g_object_unref (pixbuf);
}

static GdkPicture *
gtk_icon_theme_picture_update_styled (GtkStyledPicture *styled,
                                      GdkPicture *pixbuf_picture)
{
  GdkPicture *icon = gtk_picture_get_unstyled (GDK_PICTURE (styled));
  GtkWidget *widget = gtk_styled_picture_get_widget (styled);
  GdkPixbuf *pixbuf;

  pixbuf = gtk_icon_theme_picture_get_pixbuf_for_widget (GTK_ICON_THEME_PICTURE (icon),
                                                         widget);
  gdk_pixbuf_picture_set_pixbuf (GDK_PIXBUF_PICTURE (pixbuf_picture), pixbuf);
  if (pixbuf)
    g_object_unref (pixbuf);

  g_object_ref (pixbuf_picture);
  return pixbuf_picture;
}

static GdkPicture *
gtk_icon_theme_picture_attach (GdkPicture *picture,
                               GtkWidget  *widget)
{
  GdkPicture *styled;
  GdkPicture *pixbuf_picture;

  styled = gtk_styled_picture_new (picture, widget);
  pixbuf_picture = gdk_pixbuf_picture_new (NULL);
  g_signal_connect (styled, "update", G_CALLBACK (gtk_icon_theme_picture_update_styled), pixbuf_picture);
  gtk_styled_picture_update (GTK_STYLED_PICTURE (styled));
  /* We can get rid of it here. styled will have a reference to it
   * and keep that reference until it goes away. */
  g_object_unref (pixbuf_picture);

  return styled;
}

static void
gtk_icon_theme_picture_stylable_picture_init (GtkStylablePictureInterface *iface)
{
  iface->attach = gtk_icon_theme_picture_attach;
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkIconThemePicture, gtk_icon_theme_picture, GDK_TYPE_PICTURE,
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLABLE_PICTURE,
                                                         gtk_icon_theme_picture_stylable_picture_init))

/**
 * SECTION:gtkiconthemepicture
 * @Short_description: Pictures for a #GtkIconTheme
 * @Title: GtkIconThemePicture
 * @See_also: #GtkIconTheme
 *
 * A #GtkIconThemePicture is an implementation of #GdkPicture to display
 * icons from a #GtkIconTheme.
 */

static void
gtk_icon_theme_picture_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkIconThemePicture *picture = GTK_ICON_THEME_PICTURE (object);
  GtkIconThemePicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_ICON_THEME:
      g_value_set_object (value, priv->theme);
      break;
    case PROP_SIZE:
      g_value_set_int (value, priv->size);
      break;
    case PROP_PIXEL_SIZE:
      g_value_set_int (value, priv->pixel_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_theme_picture_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkIconThemePicture *picture = GTK_ICON_THEME_PICTURE (object);

  switch (prop_id)
    {
    case PROP_ICON_THEME:
      gtk_icon_theme_picture_set_icon_theme (picture, g_value_get_object (value));
      break;
    case PROP_SIZE:
      gtk_icon_theme_picture_set_size (picture, g_value_get_int (value));
      break;
    case PROP_PIXEL_SIZE:
      gtk_icon_theme_picture_set_pixel_size (picture, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_theme_picture_dispose (GObject *object)
{
  GtkIconThemePicture *picture = GTK_ICON_THEME_PICTURE (object);

  gtk_icon_theme_picture_set_icon_theme (picture, NULL);

  G_OBJECT_CLASS (gtk_icon_theme_picture_parent_class)->dispose (object);
}

static cairo_surface_t *
gtk_icon_theme_picture_ref_surface (GdkPicture *picture)
{
  GtkIconThemePicture *icon_theme = GTK_ICON_THEME_PICTURE (picture);

  return gdk_picture_ref_surface (icon_theme->priv->picture);
}

static void
gtk_icon_theme_picture_draw (GdkPicture *picture,
                             cairo_t    *cr)
{
  GtkIconThemePicture *icon_theme = GTK_ICON_THEME_PICTURE (picture);

  gdk_picture_draw (icon_theme->priv->picture, cr);
}

static void
gtk_icon_theme_picture_class_init (GtkIconThemePictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkPictureClass *picture_class = GDK_PICTURE_CLASS (klass);

  object_class->get_property = gtk_icon_theme_picture_get_property;
  object_class->set_property = gtk_icon_theme_picture_set_property;
  object_class->dispose = gtk_icon_theme_picture_dispose;

  picture_class->ref_surface = gtk_icon_theme_picture_ref_surface;
  picture_class->draw = gtk_icon_theme_picture_draw;

  /**
   * GtkIconThemePicture:size:
   *
   * The #GtkIconSize to use to determine the actual image size.
   * This will only be used if #GtkIconThemePicture:pixel-size is
   * unset.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     P_("Size"),
                                                     P_("Symbolic size to use"),
                                                     G_MININT, G_MAXINT,
                                                     GTK_ICON_SIZE_BUTTON,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkIconThemePicture:pixel-size:
   *
   * The "pixel-size" property can be used to specify a fixed size
   * overriding the #GtkIconThemePicture:icon-size property.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
				   PROP_PIXEL_SIZE,
				   g_param_spec_int ("pixel-size",
						     P_("Pixel size"),
						     P_("Pixel size to override size"),
						     -1, G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE));

  /**
   * GtkIconThemePicture:icon-theme:
   *
   * The icon theme to use or %NULL to use the default theme.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_ICON_THEME,
                                   g_param_spec_object ("icon-theme",
                                                        P_("Icon theme"),
                                                        P_("Icon theme to use"),
                                                       GTK_TYPE_ICON_THEME,
                                                       GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkIconThemePicturePrivate));
}

static void
gtk_icon_theme_picture_resized_callback (GdkPicture *    pixbuf,
                                   GtkIconThemePicture *icon_theme)
{
  gdk_picture_resized (GDK_PICTURE (icon_theme),
                       gdk_picture_get_width (pixbuf),
                       gdk_picture_get_height (pixbuf));
}

static void
gtk_icon_theme_picture_init (GtkIconThemePicture *picture)
{
  GtkIconThemePicturePrivate *priv;

  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GTK_TYPE_ICON_THEME_PICTURE,
                                               GtkIconThemePicturePrivate);

  priv = picture->priv;
  priv->size = GTK_ICON_SIZE_BUTTON;
  priv->pixel_size = -1;
  priv->picture = gdk_pixbuf_picture_new (NULL);
  g_signal_connect_swapped (priv->picture,
                            "changed",
                            G_CALLBACK (gdk_picture_changed_region),
                            picture);
  g_signal_connect (priv->picture,
                    "resized",
                    G_CALLBACK (gtk_icon_theme_picture_resized_callback),
                    picture);
}

GtkIconSize
gtk_icon_theme_picture_get_size (GtkIconThemePicture *picture)
{
  g_return_val_if_fail (GTK_IS_ICON_THEME_PICTURE (picture), -1);

  return picture->priv->size;
}

void
gtk_icon_theme_picture_set_size (GtkIconThemePicture *picture,
                                 GtkIconSize     size)
{
  GtkIconThemePicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_THEME_PICTURE (picture));

  priv = picture->priv;

  if (priv->size == size)
    return;

  priv->size = size;
  
  gtk_icon_theme_picture_update (picture);
  g_object_notify (G_OBJECT (picture), "size");
}

int
gtk_icon_theme_picture_get_pixel_size (GtkIconThemePicture *picture)
{
  g_return_val_if_fail (GTK_IS_ICON_THEME_PICTURE (picture), -1);

  return picture->priv->pixel_size;
}

void
gtk_icon_theme_picture_set_pixel_size (GtkIconThemePicture *picture,
                                       int                  pixel_size)
{
  GtkIconThemePicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_THEME_PICTURE (picture));

  priv = picture->priv;

  if (priv->pixel_size == pixel_size)
    return;

  priv->pixel_size = pixel_size;
  
  gtk_icon_theme_picture_update (picture);
  g_object_notify (G_OBJECT (picture), "pixel-size");
}

GtkIconTheme *
gtk_icon_theme_picture_get_icon_theme (GtkIconThemePicture * picture)
{
  g_return_val_if_fail (GTK_IS_ICON_THEME_PICTURE (picture), NULL);

  return picture->priv->theme;
}

void
gtk_icon_theme_picture_set_icon_theme (GtkIconThemePicture *picture,
                                       GtkIconTheme *       icon_theme)
{
  GtkIconThemePicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_THEME_PICTURE (picture));
  g_return_if_fail (icon_theme == NULL || GTK_IS_ICON_THEME (icon_theme));

  priv = picture->priv;

  if (icon_theme)
    {
      g_object_ref (icon_theme);
      g_signal_connect_swapped (icon_theme,
                                "changed",
                                G_CALLBACK (gtk_icon_theme_picture_update),
                                picture);
    }

  if (priv->theme)
    {
      g_signal_handlers_disconnect_by_func (priv->theme,
                                            gtk_icon_theme_picture_update,
                                            picture);
      g_object_unref (priv->theme);
    }
  
  priv->theme = icon_theme;

  gtk_icon_theme_picture_update (picture);
  g_object_notify (G_OBJECT (picture), "icon-theme");
}

gboolean
gtk_icon_theme_picture_get_use_fallback (GtkIconThemePicture * picture)
{
  g_return_val_if_fail (GTK_IS_ICON_THEME_PICTURE (picture), FALSE);

  return picture->priv->use_fallback;
}

void
gtk_icon_theme_picture_set_use_fallback (GtkIconThemePicture *picture,
                                         gboolean             use_fallback)
{
  GtkIconThemePicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_THEME_PICTURE (picture));

  priv = picture->priv;

  if (priv->use_fallback == use_fallback)
    return;

  priv->use_fallback = use_fallback;

  gtk_icon_theme_picture_update (picture);
  g_object_notify (G_OBJECT (picture), "icon-theme");
}
