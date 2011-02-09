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

#include "gtkstyledpicture.h"

#include <cairo-gobject.h>

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

struct _GtkStyledPicturePrivate {
  GdkPicture *styled;

  GtkWidget *widget;
  GdkPicture *unstyled;
};

enum {
  UPDATE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static GdkPicture *
gtk_styled_picture_attach (GdkPicture *picture,
                           GtkWidget  *widget)
{
  GtkStyledPicture *styled = GTK_STYLED_PICTURE (picture);
  GtkStyledPicturePrivate *priv = styled->priv;

  g_warning ("styled pictures should not be exposed to the world, did somebody not call gtk_picture_get_unstyled()?");

  return gtk_styled_picture_new (priv->unstyled,
                                 priv->widget);
}

static GdkPicture *
gtk_styled_picture_get_unstyled (GdkPicture *picture)
{
  GtkStyledPicture *styled = GTK_STYLED_PICTURE (picture);

  return styled->priv->unstyled;
}

static void
gtk_styled_picture_stylable_picture_init (GtkStylablePictureInterface *iface)
{
  iface->attach = gtk_styled_picture_attach;
  iface->get_unstyled = gtk_styled_picture_get_unstyled;
}

G_DEFINE_TYPE_WITH_CODE (GtkStyledPicture, gtk_styled_picture, GDK_TYPE_PICTURE,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLABLE_PICTURE,
						gtk_styled_picture_stylable_picture_init))

static void
gdk_styled_picture_resized_callback (GdkPicture *picture,
                                     GtkStyledPicture *styled)
{
  gdk_picture_resized (GDK_PICTURE (styled),
                       gdk_picture_get_width (picture),
                       gdk_picture_get_height (picture));
}

static void
gtk_styled_picture_set_styled (GtkStyledPicture *styled,
                               GdkPicture *      picture)
{
  GtkStyledPicturePrivate *priv = styled->priv;
  
  if (priv->styled == picture)
    return;

  if (picture)
    {
      g_object_ref (picture);
      g_signal_connect_swapped (picture,
                                "changed",
                                G_CALLBACK (gdk_picture_changed_region),
                                styled);
      g_signal_connect (picture,
                        "resized",
                        G_CALLBACK (gdk_styled_picture_resized_callback),
                        styled);
    }

  if (priv->styled)
    {
      g_signal_handlers_disconnect_matched (priv->styled,
                                            G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, styled);
      g_object_unref (priv->styled);
    }

  priv->styled = picture;

  if (priv->styled)
    gdk_picture_resized (GDK_PICTURE (styled),
                         gdk_picture_get_width (priv->styled),
                         gdk_picture_get_height (priv->styled));
  else
    gdk_picture_resized (GDK_PICTURE (styled), 0, 0);
}

static void
gtk_styled_picture_dispose (GObject *object)
{
  GtkStyledPicture *styled = GTK_STYLED_PICTURE (object);
  GtkStyledPicturePrivate *priv = styled->priv;

  gtk_styled_picture_set_styled (styled, NULL);

  if (priv->unstyled)
    {
      g_signal_handlers_disconnect_by_func (priv->unstyled,
                                            gtk_styled_picture_update,
                                            styled);
      g_object_unref (priv->unstyled);
      priv->unstyled = NULL;
    }
  if (priv->widget)
    {
      g_signal_handlers_disconnect_by_func (priv->widget,
                                            gtk_styled_picture_update,
                                            styled);
      g_object_remove_weak_pointer (G_OBJECT (priv->widget), (void **) &priv->widget);
      priv->widget = NULL;
    }

  G_OBJECT_CLASS (gtk_styled_picture_parent_class)->dispose (object);
}

static cairo_surface_t *
gtk_styled_picture_ref_surface (GdkPicture *picture)
{
  GtkStyledPicture *styled = GTK_STYLED_PICTURE (picture);

  return gdk_picture_ref_surface (styled->priv->styled);
}

static void
gtk_styled_picture_draw (GdkPicture *picture,
                       cairo_t    *cr)
{
  GtkStyledPicture *styled = GTK_STYLED_PICTURE (picture);

  gdk_picture_draw (styled->priv->styled, cr);
}

static gboolean
gtk_styled_picture_update_accumulator (GSignalInvocationHint *ihint,
                                       GValue                *return_accu,
                                       const GValue          *handler_return,
                                       gpointer               unused)
{
  gboolean continue_emission;
  GObject *object;

  object = g_value_get_object (handler_return);
  g_value_set_object (return_accu, object);
  continue_emission = !object;

  return continue_emission;
}

static void
gtk_styled_picture_class_init (GtkStyledPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkPictureClass *picture_class = GDK_PICTURE_CLASS (klass);

  object_class->dispose = gtk_styled_picture_dispose;

  picture_class->ref_surface = gtk_styled_picture_ref_surface;
  picture_class->draw = gtk_styled_picture_draw;

  signals[UPDATE] =
    g_signal_new (I_("update"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkStyledPictureClass, update),
                  gtk_styled_picture_update_accumulator, NULL,
                  _gtk_marshal_OBJECT__VOID,
                  GDK_TYPE_PICTURE, 0);

  g_type_class_add_private (klass, sizeof (GtkStyledPicturePrivate));
}

static void
gtk_styled_picture_init (GtkStyledPicture *picture)
{
  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GTK_TYPE_STYLED_PICTURE,
                                               GtkStyledPicturePrivate);
}

/**
 * gtk_styled_picture_new:
 * @unstyled: The unstyled picture
 * @widget: The widet to attach to
 *
 * Creates a new #GtkStyledPicture displaying the styled version of
 * an @unstyled #GdkPicture
 *
 * Returns: a new picture
 **/
GdkPicture *
gtk_styled_picture_new (GdkPicture *unstyled,
                        GtkWidget  *widget)
{
  GtkStyledPicturePrivate *priv;
  GtkStyledPicture *styled;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (GDK_IS_PICTURE (unstyled), NULL);

  styled = g_object_new (GTK_TYPE_STYLED_PICTURE, NULL);

  priv = styled->priv;

  priv->widget = widget;
  g_object_add_weak_pointer (G_OBJECT (widget), (void **) &priv->widget);
  g_signal_connect_swapped (widget, 
                            "style-updated",
                            G_CALLBACK (gtk_styled_picture_update),
                            styled);
  g_signal_connect_swapped (widget, 
                            "state-flags-changed",
                            G_CALLBACK (gtk_styled_picture_update),
                            styled);
  g_signal_connect_swapped (widget, 
                            "direction-changed",
                            G_CALLBACK (gtk_styled_picture_update),
                            styled);

  styled->priv->unstyled = g_object_ref (unstyled);
  g_signal_connect_swapped (unstyled, 
                            "notify",
                            G_CALLBACK (gtk_styled_picture_update),
                            styled);

  gtk_styled_picture_update (styled);

  return GDK_PICTURE (styled);
}

void
gtk_styled_picture_update (GtkStyledPicture *picture)
{
  GdkPicture *new_picture = NULL;

  g_signal_emit (picture, signals[UPDATE], 0, &new_picture);

  gtk_styled_picture_set_styled (picture, new_picture);

  if (new_picture)
    g_object_unref (new_picture);
}

GtkWidget *
gtk_styled_picture_get_widget (GtkStyledPicture *picture)
{
  g_return_val_if_fail (GTK_IS_STYLED_PICTURE (picture), NULL);

  return picture->priv->widget;
}

