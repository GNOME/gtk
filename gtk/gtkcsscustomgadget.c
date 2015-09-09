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

#include "gtkcsscustomgadgetprivate.h"

#include "gtkcssnodeprivate.h"

typedef struct _GtkCssCustomGadgetPrivate GtkCssCustomGadgetPrivate;
struct _GtkCssCustomGadgetPrivate {
  GtkCssPreferredSizeFunc          preferred_size_func;
  GtkCssAllocateFunc               allocate_func;
  GtkCssDrawFunc                   draw_func;
  gpointer                         data;
  GDestroyNotify                   destroy_func;
};

G_DEFINE_TYPE_WITH_CODE (GtkCssCustomGadget, gtk_css_custom_gadget, GTK_TYPE_CSS_GADGET,
                         G_ADD_PRIVATE (GtkCssCustomGadget))

static void
gtk_css_custom_gadget_get_preferred_size (GtkCssGadget   *gadget,
                                          GtkOrientation  orientation,
                                          gint            for_size,
                                          gint           *minimum,
                                          gint           *natural,
                                          gint           *minimum_baseline,
                                          gint           *natural_baseline)
{
  GtkCssCustomGadgetPrivate *priv = gtk_css_custom_gadget_get_instance_private (GTK_CSS_CUSTOM_GADGET (gadget));

  if (priv->preferred_size_func)
    return priv->preferred_size_func (gadget, orientation, for_size, 
                                      minimum, natural,
                                      minimum_baseline, natural_baseline,
                                      priv->data);
  else
    return GTK_CSS_GADGET_CLASS (gtk_css_custom_gadget_parent_class)->get_preferred_size (gadget, orientation, for_size, 
                                                                                          minimum, natural,
                                                                                          minimum_baseline, natural_baseline);
}

static void
gtk_css_custom_gadget_allocate (GtkCssGadget        *gadget,
                                const GtkAllocation *allocation,
                                int                  baseline,
                                GtkAllocation       *out_clip)
{
  GtkCssCustomGadgetPrivate *priv = gtk_css_custom_gadget_get_instance_private (GTK_CSS_CUSTOM_GADGET (gadget));

  if (priv->allocate_func)
    return priv->allocate_func (gadget, allocation, baseline, out_clip, priv->data);
  else
    return GTK_CSS_GADGET_CLASS (gtk_css_custom_gadget_parent_class)->allocate (gadget, allocation, baseline, out_clip);
}

static gboolean
gtk_css_custom_gadget_draw (GtkCssGadget *gadget,
                            cairo_t      *cr,
                            int           x,
                            int           y,
                            int           width,
                            int           height)
{
  GtkCssCustomGadgetPrivate *priv = gtk_css_custom_gadget_get_instance_private (GTK_CSS_CUSTOM_GADGET (gadget));

  if (priv->draw_func)
    return priv->draw_func (gadget, cr, x, y, width, height, priv->data);
  else
    return GTK_CSS_GADGET_CLASS (gtk_css_custom_gadget_parent_class)->draw (gadget, cr, x, y, width, height);
}

static void
gtk_css_custom_gadget_finalize (GObject *object)
{
  GtkCssCustomGadgetPrivate *priv = gtk_css_custom_gadget_get_instance_private (GTK_CSS_CUSTOM_GADGET (object));

  if (priv->destroy_func)
    priv->destroy_func (priv->data);

  G_OBJECT_CLASS (gtk_css_custom_gadget_parent_class)->finalize (object);
}

static void
gtk_css_custom_gadget_class_init (GtkCssCustomGadgetClass *klass)
{
  GtkCssGadgetClass *gadget_class = GTK_CSS_GADGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_custom_gadget_finalize;

  gadget_class->get_preferred_size = gtk_css_custom_gadget_get_preferred_size;
  gadget_class->allocate = gtk_css_custom_gadget_allocate;
  gadget_class->draw = gtk_css_custom_gadget_draw;
}

static void
gtk_css_custom_gadget_init (GtkCssCustomGadget *custom_gadget)
{

}

GtkCssGadget *
gtk_css_custom_gadget_new_for_node (GtkCssNode                 *node,
                                    GtkWidget                  *owner,
                                    GtkCssPreferredSizeFunc     preferred_size_func,
                                    GtkCssAllocateFunc          allocate_func,
                                    GtkCssDrawFunc              draw_func,
                                    gpointer                    data,
                                    GDestroyNotify              destroy_func)
{
  GtkCssCustomGadgetPrivate *priv;
  GtkCssGadget *result;

  result = g_object_new (GTK_TYPE_CSS_CUSTOM_GADGET,
                         "node", node,
                         "owner", owner,
                         NULL);

  priv = gtk_css_custom_gadget_get_instance_private (GTK_CSS_CUSTOM_GADGET (result));

  priv->preferred_size_func = preferred_size_func;
  priv->allocate_func = allocate_func;
  priv->draw_func = draw_func;
  priv->data = data;
  priv->destroy_func = destroy_func;

  return result;
}

GtkCssGadget *
gtk_css_custom_gadget_new (const char                 *name,
                           GtkWidget                  *owner,
                           GtkCssGadget               *parent,
                           GtkCssGadget               *next_sibling,
                           GtkCssPreferredSizeFunc     preferred_size_func,
                           GtkCssAllocateFunc          allocate_func,
                           GtkCssDrawFunc              draw_func,
                           gpointer                    data,
                           GDestroyNotify              destroy_func)
{
  GtkCssNode *node;
  GtkCssGadget *result;

  node = gtk_css_node_new ();
  gtk_css_node_set_name (node, g_intern_string (name));
  if (parent)
    gtk_css_node_insert_before (gtk_css_gadget_get_node (parent),
                                node,
                                next_sibling ? gtk_css_gadget_get_node (next_sibling) : NULL);

  result = gtk_css_custom_gadget_new_for_node (node,
                                               owner,
                                               preferred_size_func,
                                               allocate_func,
                                               draw_func,
                                               data,
                                               destroy_func);

  g_object_unref (node);

  return result;
}
