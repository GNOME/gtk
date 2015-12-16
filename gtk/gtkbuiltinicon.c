/*
 * Copyright © 2015 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkbuiltiniconprivate.h"

#include "gtkcssnodeprivate.h"
#include "gtkrendericonprivate.h"

typedef struct _GtkBuiltinIconPrivate GtkBuiltinIconPrivate;
struct _GtkBuiltinIconPrivate {
  GtkCssImageBuiltinType        image_type;
  int                           default_size;
};

G_DEFINE_TYPE_WITH_CODE (GtkBuiltinIcon, gtk_builtin_icon, GTK_TYPE_CSS_GADGET,
                         G_ADD_PRIVATE (GtkBuiltinIcon))

static void
gtk_builtin_icon_get_preferred_size (GtkCssGadget   *gadget,
                                     GtkOrientation  orientation,
                                     gint            for_size,
                                     gint           *minimum,
                                     gint           *natural,
                                     gint           *minimum_baseline,
                                     gint           *natural_baseline)
{
  GtkBuiltinIconPrivate *priv = gtk_builtin_icon_get_instance_private (GTK_BUILTIN_ICON (gadget));

  *minimum = priv->default_size;
  *natural = priv->default_size;
}

static void
gtk_builtin_icon_allocate (GtkCssGadget        *gadget,
                           const GtkAllocation *allocation,
                           int                  baseline,
                           GtkAllocation       *out_clip)
{
  GdkRectangle icon_clip;

  GTK_CSS_GADGET_CLASS (gtk_builtin_icon_parent_class)->allocate (gadget, allocation, baseline, out_clip);

  gtk_css_style_render_icon_get_extents (gtk_css_gadget_get_style (gadget),
                                         &icon_clip,
                                         allocation->x, allocation->y,
                                         allocation->width, allocation->height);
  gdk_rectangle_union (out_clip, &icon_clip, out_clip);
}

static gboolean
gtk_builtin_icon_draw (GtkCssGadget *gadget,
                            cairo_t      *cr,
                            int           x,
                            int           y,
                            int           width,
                            int           height)
{
  GtkBuiltinIconPrivate *priv = gtk_builtin_icon_get_instance_private (GTK_BUILTIN_ICON (gadget));

  gtk_css_style_render_icon (gtk_css_gadget_get_style (gadget),
                             cr,
                             x, y,
                             width, height,
                             priv->image_type);

  return FALSE;
}

static void
gtk_builtin_icon_finalize (GObject *object)
{
  //GtkBuiltinIconPrivate *priv = gtk_builtin_icon_get_instance_private (GTK_BUILTIN_ICON (object));

  G_OBJECT_CLASS (gtk_builtin_icon_parent_class)->finalize (object);
}

static void
gtk_builtin_icon_class_init (GtkBuiltinIconClass *klass)
{
  GtkCssGadgetClass *gadget_class = GTK_CSS_GADGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_builtin_icon_finalize;

  gadget_class->get_preferred_size = gtk_builtin_icon_get_preferred_size;
  gadget_class->allocate = gtk_builtin_icon_allocate;
  gadget_class->draw = gtk_builtin_icon_draw;
}

static void
gtk_builtin_icon_init (GtkBuiltinIcon *custom_gadget)
{
}

GtkCssGadget *
gtk_builtin_icon_new_for_node (GtkCssNode *node,
                               GtkWidget  *owner)
{
  return g_object_new (GTK_TYPE_BUILTIN_ICON,
                       "node", node,
                       "owner", owner,
                       NULL);
}

GtkCssGadget *
gtk_builtin_icon_new (const char   *name,
                      GtkWidget    *owner,
                      GtkCssGadget *parent,
                      GtkCssGadget *next_sibling)
{
  GtkCssNode *node;
  GtkCssGadget *result;

  node = gtk_css_node_new ();
  gtk_css_node_set_name (node, g_intern_string (name));
  if (parent)
    gtk_css_node_insert_before (gtk_css_gadget_get_node (parent),
                                node,
                                next_sibling ? gtk_css_gadget_get_node (next_sibling) : NULL);

  result = gtk_builtin_icon_new_for_node (node, owner);

  g_object_unref (node);

  return result;
}

void
gtk_builtin_icon_set_image (GtkBuiltinIcon         *icon,
                            GtkCssImageBuiltinType  image)
{
  GtkBuiltinIconPrivate *priv;
  
  g_return_if_fail (GTK_IS_BUILTIN_ICON (icon));

  priv = gtk_builtin_icon_get_instance_private (icon);

  if (priv->image_type != image)
    {
      priv->image_type = image;
      gtk_widget_queue_draw (gtk_css_gadget_get_owner (GTK_CSS_GADGET (icon)));
    }
}

GtkCssImageBuiltinType
gtk_builtin_icon_get_image (GtkBuiltinIcon *icon)
{
  GtkBuiltinIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUILTIN_ICON (icon), GTK_CSS_IMAGE_BUILTIN_NONE);

  priv = gtk_builtin_icon_get_instance_private (icon);

  return priv->image_type;
}

void
gtk_builtin_icon_set_default_size (GtkBuiltinIcon *icon,
                                   int             default_size)
{
  GtkBuiltinIconPrivate *priv;
  
  g_return_if_fail (GTK_IS_BUILTIN_ICON (icon));

  priv = gtk_builtin_icon_get_instance_private (icon);

  if (priv->default_size != default_size)
    {
      priv->default_size = default_size;
      gtk_widget_queue_resize (gtk_css_gadget_get_owner (GTK_CSS_GADGET (icon)));
    }
}

int
gtk_builtin_icon_get_default_size (GtkBuiltinIcon *icon)
{
  GtkBuiltinIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUILTIN_ICON (icon), GTK_CSS_IMAGE_BUILTIN_NONE);

  priv = gtk_builtin_icon_get_instance_private (icon);

  return priv->default_size;
}

