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

#include "gtkiconpicture.h"

#include <cairo-gobject.h>

#include "gtkintl.h"
#include "gtkprivate.h"

struct _GtkIconPicturePrivate {
  GdkPicture *picture;
  char *name;
  GtkIconSize size;

  GtkWidget *widget;
  GdkPicture *unstyled;
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_SIZE
};

static void
gtk_icon_picture_update_picture (GtkIconPicture *picture)
{
  GtkIconPicturePrivate *priv = picture->priv;
  GtkIconSet *icon_set;
  GtkStyleContext *style;
  GdkPixbuf *pixbuf;
  const char *name;

  if (priv->widget)
    style = g_object_ref (gtk_widget_get_style_context (priv->widget));
  else
    {
      GtkWidgetPath *path = gtk_widget_path_new ();
      style = gtk_style_context_new ();
      gtk_style_context_set_path (style, path);
      gtk_widget_path_free (path);
    }

  name = priv->name;
  if (name == NULL)
    name = GTK_STOCK_MISSING_IMAGE;

  icon_set = gtk_style_context_lookup_icon_set (style, name);
  if (icon_set == NULL)
    {
      icon_set = gtk_style_context_lookup_icon_set (style, GTK_STOCK_MISSING_IMAGE);
      g_assert (icon_set);
    }

  pixbuf = gtk_icon_set_render_icon_pixbuf (icon_set, style, priv->size);
  gdk_pixbuf_picture_set_pixbuf (GDK_PIXBUF_PICTURE (priv->picture), pixbuf);

  g_object_unref (pixbuf);
  g_object_unref (style);
}

static void
gtk_icon_picture_attached_notify (GdkPicture *original,
                                  GParamSpec *pspec,
                                  GdkPicture *attached)
{
  GValue value = { 0, };

  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_object_get_property (G_OBJECT (original), pspec->name, &value);
  g_object_set_property (G_OBJECT (attached), pspec->name, &value);

  g_value_unset (&value);
}

static GdkPicture *
gtk_icon_picture_attach (GdkPicture *picture,
                         GtkWidget  *widget)
{
  GtkIconPicture *icon = GTK_ICON_PICTURE (picture);
  GtkIconPicture *attached;

  attached = GTK_ICON_PICTURE (gtk_icon_picture_new (icon->priv->name,
                                                     icon->priv->size));

  attached->priv->widget = widget;
  g_object_add_weak_pointer (G_OBJECT (widget), (void **) &attached->priv->widget);
  g_signal_connect_swapped (widget, 
                            "style-updated",
                            G_CALLBACK (gtk_icon_picture_update_picture),
                            attached);
  g_signal_connect_swapped (widget, 
                            "state-flags-changed",
                            G_CALLBACK (gtk_icon_picture_update_picture),
                            attached);
  g_signal_connect_swapped (widget, 
                            "direction-changed",
                            G_CALLBACK (gtk_icon_picture_update_picture),
                            attached);
  g_signal_connect_swapped (widget, 
                            "notify::sensitive",
                            G_CALLBACK (gtk_icon_picture_update_picture),
                            attached);
  attached->priv->unstyled = g_object_ref (picture);
  g_signal_connect (picture, 
                    "notify",
                    G_CALLBACK (gtk_icon_picture_attached_notify),
                    attached);

  gtk_icon_picture_update_picture (attached);

  return GDK_PICTURE (attached);
}

static GdkPicture *
gtk_icon_picture_get_unstyled (GdkPicture *picture)
{
  GtkIconPicture *icon = GTK_ICON_PICTURE (picture);

  if (icon->priv->unstyled)
    return icon->priv->unstyled;

  return picture;
}

static void
gtk_icon_picture_stylable_picture_init (GtkStylablePictureInterface *iface)
{
  iface->attach = gtk_icon_picture_attach;
  iface->get_unstyled = gtk_icon_picture_get_unstyled;
}

G_DEFINE_TYPE_WITH_CODE (GtkIconPicture, gtk_icon_picture, GDK_TYPE_PICTURE,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLABLE_PICTURE,
						gtk_icon_picture_stylable_picture_init))

/**
 * SECTION:gtkiconpicture
 * @Short_description: Pictures for icons
 * @Title: GtkIconPicture
 * @See_also: #GtkIconFactory
 *
 * A #GtkIconPicture is an implementation of #GdkPicture for named icons.
 */

static void
gtk_icon_picture_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkIconPicture *picture = GTK_ICON_PICTURE (object);
  GtkIconPicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_SIZE:
      g_value_set_int (value, priv->size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_picture_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkIconPicture *picture = GTK_ICON_PICTURE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      gtk_icon_picture_set_name (picture, g_value_get_string (value));
      break;
    case PROP_SIZE:
      gtk_icon_picture_set_size (picture, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_picture_dispose (GObject *object)
{
  GtkIconPicture *icon = GTK_ICON_PICTURE (object);
  GtkIconPicturePrivate *priv = icon->priv;

  if (priv->picture)
    {
      g_signal_handlers_disconnect_by_func (priv->picture,
                                            gtk_icon_picture_attached_notify,
                                            icon);
      g_object_unref (priv->picture);
      priv->picture = NULL;
    }
  if (priv->widget)
    {
      g_signal_handlers_disconnect_by_func (priv->widget,
                                            gtk_icon_picture_update_picture,
                                            icon);
      g_object_remove_weak_pointer (G_OBJECT (priv->widget), (void **) &priv->widget);
      priv->widget = NULL;
    }
  g_free (priv->name);
  priv->name = NULL;

  G_OBJECT_CLASS (gtk_icon_picture_parent_class)->dispose (object);
}

static cairo_surface_t *
gtk_icon_picture_ref_surface (GdkPicture *picture)
{
  GtkIconPicture *icon = GTK_ICON_PICTURE (picture);

  return gdk_picture_ref_surface (icon->priv->picture);
}

static void
gtk_icon_picture_draw (GdkPicture *picture,
                       cairo_t    *cr)
{
  GtkIconPicture *icon = GTK_ICON_PICTURE (picture);

  gdk_picture_draw (icon->priv->picture, cr);
}

static void
gtk_icon_picture_class_init (GtkIconPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkPictureClass *picture_class = GDK_PICTURE_CLASS (klass);

  object_class->get_property = gtk_icon_picture_get_property;
  object_class->set_property = gtk_icon_picture_set_property;
  object_class->dispose = gtk_icon_picture_dispose;

  picture_class->ref_surface = gtk_icon_picture_ref_surface;
  picture_class->draw = gtk_icon_picture_draw;

  /**
   * GtkIconPicture:size:
   *
   * The #GtkIconSize to use to determine the actual image size.
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
   * GtkIconPicture:name:
   *
   * The name of the icon to display.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        P_("Name"),
                                                        P_("The name of the icon from the icon theme"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkIconPicturePrivate));
}

static void
gtk_icon_picture_resized_callback (GdkPicture *    pixbuf,
                                   GtkIconPicture *icon)
{
  gdk_picture_resized (GDK_PICTURE (icon),
                       gdk_picture_get_width (pixbuf),
                       gdk_picture_get_height (pixbuf));
}

static void
gtk_icon_picture_init (GtkIconPicture *picture)
{
  GtkIconPicturePrivate *priv;

  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GTK_TYPE_ICON_PICTURE,
                                               GtkIconPicturePrivate);

  priv = picture->priv;
  priv->size = GTK_ICON_SIZE_BUTTON;
  priv->picture = gdk_pixbuf_picture_new (NULL);
  g_signal_connect_swapped (priv->picture,
                            "changed",
                            G_CALLBACK (gdk_picture_changed_region),
                            picture);
  g_signal_connect (priv->picture,
                    "resized",
                    G_CALLBACK (gtk_icon_picture_resized_callback),
                    picture);

  gtk_icon_picture_update_picture (picture);
}

/**
 * gtk_icon_picture_new:
 * @icon_name: the name of the icon to display
 * @size: The icon size to use or -1 for default
 *
 * Creates a new #GtkIconPicture displaying the given @icon.
 *
 * Returns: a new picture
 **/
GdkPicture *
gtk_icon_picture_new (const char *icon_name,
                      GtkIconSize size)
{
  return g_object_new (GTK_TYPE_ICON_PICTURE,
                       "name", icon_name,
                       "size", size,
                       NULL);
}

GtkIconSize
gtk_icon_picture_get_size (GtkIconPicture *picture)
{
  g_return_val_if_fail (GTK_IS_ICON_PICTURE (picture), -1);

  return picture->priv->size;
}

void
gtk_icon_picture_set_size (GtkIconPicture *picture,
                           GtkIconSize     size)
{
  GtkIconPicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_PICTURE (picture));

  priv = picture->priv;

  if (priv->size == size)
    return;

  priv->size = size;
  
  gtk_icon_picture_update_picture (picture);
  g_object_notify (G_OBJECT (picture), "size");
}

const char *
gtk_icon_picture_get_name (GtkIconPicture * picture)
{
  g_return_val_if_fail (GTK_IS_ICON_PICTURE (picture), NULL);

  return picture->priv->name;
}

void
gtk_icon_picture_set_name (GtkIconPicture * picture,
                           const char *     name)
{
  GtkIconPicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_PICTURE (picture));

  priv = picture->priv;

  g_free (priv->name);
  priv->name = g_strdup (name);

  gtk_icon_picture_update_picture (picture);
  g_object_notify (G_OBJECT (picture), "name");
}

