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

/*
 * GtkCssCustomGadget is a subclass that lets widgets customize size
 * requests, size allocation and drawing of gadgets. The gadget is passed
 * to the callbacks as the first argument, and you can use gtk_css_gadget_get_owner()
 * to obtain the widget. Note that the widgets style context is not saved,
 * so if you want to query style properties or call gtk_render functions which
 * take the style context as an argument, you should use
 * gtk_style_context_save_to_node to make the gadget's CSS node take effect.
 *
 * The callbacks are
 *
 * GtkCssPreferredSizeFunc:
 * @gadget: the #GtkCssCustomGadget
 * @orientation: whether a width (ie horizontal) or height (ie vertical) is requested
 * @for_size: the available size in the opposite direction, or -1
 * @minimum: return location for the minimum size
 * @natural: return location for the natural size
 * @minimum_baseline: (nullable): return location for the baseline at minimum size
 * @natural_baseline: (nullable): return location for the baseline at natural size
 * @data: data provided when registering the callback
 *
 * The GtkCssPreferredSizeFunc is called to determine the content size in
 * gtk_css_gadget_get_preferred_size(). @for_size is a content size (ie excluding
 * CSS padding, border and margin) and the returned @minimum, @natural,
 * @minimum_baseline, @natural_baseline should be content sizes excluding CSS
 * padding, border and margin as well.
 *
 * Typically, GtkCssPreferredSizeFunc will query the size of sub-gadgets and
 * child widgets that are placed relative to the gadget and determine its own
 * needed size from the results. If the gadget has no sub-gadgets or child
 * widgets that it needs to place, then a GtkCssPreferredSizeFunc is only
 * needed if you want to enforce a minimum size independent of CSS min-width
 * and min-height (e.g. if size-related style properties need to be supported
 * for compatibility).
 *
 * GtkCssAllocateFunc:
 * @gadget: the #GtkCssCustomGadget
 * @allocation: the allocation
 * @baseline: the baseline
 * @out_clip: (out): return location for the content clip
 * @data: data provided when registering the callback
 *
 * The GtkCssAllocateFunc is called to allocate the gadgets content in
 * gtk_css_gadget_allocate(). @allocation and @baseline are content sizes
 * (ie excluding CSS padding, border and margin).
 *
 * Typically, GtkCssAllocateFunc will allocate sub-gadgets and child widgets
 * that are placed relative to the gadget, and merge their clips into the
 * value returned as @out_clip. For clip handling in the main gadget of
 * containers, gtk_container_get_children_clip() can be useful. Gadgets that
 * don't have sub-gadgets of child widgets don't need a GtkCssAllocateFunc
 * (although it is still required to call gtk_css_gadget_allocate() on them).
 *
 * Note that @out_clip *must* be set to meaningful values. If in doubt,
 * just set it to the allocation.
 *
 * GtkCssDrawFunc:
 * @gadget: the #GtkCssCustomGadget
 * @cr: the cairo context to draw on
 * @x: the x origin of the content area
 * @y: the y origin of the content area
 * @width: the width of the content area
 * @height: the height of the content area
 * @data: data provided when registering the callback
 *
 * The GtkCssDrawFunc is called to draw the gadgets content in
 * gtk_css_gadget_draw(). It gets passed an untransformed cairo context
 * and the coordinates of the area to draw the content in.
 *
 * Typically, GtkCssDrawFunc will draw sub-gadgets and child widgets
 * that are placed relative to the gadget, as well as custom content
 * such as icons, checkmarks, arrows or text.
 */

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
    priv->preferred_size_func (gadget, orientation, for_size,
                               minimum, natural,
                               minimum_baseline, natural_baseline,
                               priv->data);
  else
    GTK_CSS_GADGET_CLASS (gtk_css_custom_gadget_parent_class)->get_preferred_size (gadget, orientation, for_size,
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
    priv->allocate_func (gadget, allocation, baseline, out_clip, priv->data);
  else
    GTK_CSS_GADGET_CLASS (gtk_css_custom_gadget_parent_class)->allocate (gadget, allocation, baseline, out_clip);
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

/**
 * gtk_css_custom_gadget_new_for_node:
 * @node: the #GtkCssNode to use for the gadget
 * @owner: the widget that the gadget belongs to
 * @preferred_size_func: (nullable): the GtkCssPreferredSizeFunc to use
 * @allocate_func: (nullable): the GtkCssAllocateFunc to use
 * @draw_func: (nullable): the GtkCssDrawFunc to use
 * @data: (nullable): user data to pass to the callbacks
 * @destroy_func: (nullable): destroy notify for @data
 *
 * Creates a #GtkCssCustomGadget for an existing CSS node.
 * This function is typically used in the widgets init function
 * to create the main gadget for the widgets main CSS node (which
 * is obtained with gtk_widget_get_css_node()), as well as other
 * permanent sub-gadgets. Sub-gadgets that only exist sometimes
 * (e.g. depending on widget properties) should be created and
 * destroyed as needed. All gadgets should be destroyed in the
 * finalize (or dispose) vfunc.
 *
 * Returns: (transfer full): the new gadget
 */
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

/**
 * gtk_css_custom_gadget_new:
 * @name: the name for the CSS node
 * @owner: the widget that the gadget belongs to
 * @parent: the gadget that has the parent CSS node
 * @next_sibling: the gadget that has the sibling CSS node
 * @preferred_size_func: (nullable): the GtkCssPreferredSizeFunc to use
 * @allocate_func: (nullable): the GtkCssAllocateFunc to use
 * @draw_func: (nullable): the GtkCssDrawFunc to use
 * @data: (nullable): user data to pass to the callbacks
 * @destroy_func: (nullable): destroy notify for @data
 *
 * Creates a #GtkCssCustomGadget with a new CSS node which gets
 * placed below the @parent's and before the @next_sibling's CSS node.
 *
 * Returns: (transfer full): the new gadget
 */
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
