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

#include "gtkcssgadgetprivate.h"

#include <math.h>

#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsswidgetnodeprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"

/*
 * Gadgets are 'next-generation widgets' - they combine a CSS node
 * for style matching with geometry management and drawing. Each gadget
 * corresponds to 'CSS box'. Compared to traditional widgets, they are more
 * like building blocks - a typical GTK+ widget will have multiple gadgets,
 * for example a check button has its main gadget, and sub-gadgets for
 * the checkmark and the text.
 *
 * Gadgets are not themselves hierarchically organized, but it is common
 * to have a 'main' gadget, which gets used by the widgets size_allocate,
 * get_preferred_width, etc. and draw callbacks, and which in turn calls out
 * to the sub-gadgets. This call tree might extend further if there are
 * sub-sub-gadgets that a allocated relative to sub-gadgets. In typical
 * situations, the callback chain will reflect the tree structure of the
 * gadgets CSS nodes.
 *
 * Geometry management - Gadgets implement much of the CSS box model for you:
 * margins, border, padding, shadows, min-width/height are all applied automatically.
 *
 * Drawing - Gadgets implement standardized CSS drawing for you: background,
 * shadows and border are drawn before any custom drawing, and the focus outline
 * is (optionally) drawn afterwards.
 *
 * Invalidation - Gadgets sit 'between' widgets and CSS nodes, and connect
 * to the nodes ::style-changed signal and trigger appropriate invalidations
 * on the widget side.
 */

typedef struct _GtkCssGadgetPrivate GtkCssGadgetPrivate;
struct _GtkCssGadgetPrivate {
  GtkCssNode    *node;
  GtkWidget     *owner;
  GtkAllocation  allocated_size;
  gint           allocated_baseline;
};

enum {
  PROP_0,
  PROP_NODE,
  PROP_OWNER,
  /* add more */
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkCssGadget, gtk_css_gadget, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (GtkCssGadget))

static void
gtk_css_gadget_real_get_preferred_size (GtkCssGadget   *gadget,
                                        GtkOrientation  orientation,
                                        gint            for_size,
                                        gint           *minimum,
                                        gint           *natural,
                                        gint           *minimum_baseline,
                                        gint           *natural_baseline)
{
  *minimum = 0;
  *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = 0;
  if (natural_baseline)
    *natural_baseline = 0;
}

static void
gtk_css_gadget_real_allocate (GtkCssGadget        *gadget,
                              const GtkAllocation *allocation,
                              int                  baseline,
                              GtkAllocation       *out_clip)
{
  *out_clip = *allocation;
}

static gboolean
gtk_css_gadget_real_draw (GtkCssGadget *gadget,
                          cairo_t      *cr,
                          int           x,
                          int           y,
                          int           width,
                          int           height)
{
  return FALSE;
}

static void
gtk_css_gadget_real_style_changed (GtkCssGadget      *gadget,
                                   GtkCssStyleChange *change)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);

  if (priv->owner)
    {
      if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SIZE))
        gtk_widget_queue_resize (priv->owner);
      else if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_CLIP))
        gtk_widget_queue_allocate (priv->owner);
      else if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_REDRAW))
        gtk_widget_queue_draw (priv->owner);
    }
}

static void
gtk_css_gadget_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (GTK_CSS_GADGET (object));

  switch (property_id)
  {
    case PROP_NODE:
      g_value_set_object (value, priv->node);
      break;

    case PROP_OWNER:
      g_value_set_object (value, priv->owner);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gtk_css_gadget_node_style_changed_cb (GtkCssNode        *node,
                                      GtkCssStyleChange *change,
                                      GtkCssGadget      *gadget)
{
  GtkCssGadgetClass *klass = GTK_CSS_GADGET_GET_CLASS (gadget);

  klass->style_changed (gadget, change);
}

static gboolean
gtk_css_gadget_should_connect_style_changed (GtkCssNode *node)
{
  /* Delegate to WidgetClass->style_changed */
  if (GTK_IS_CSS_WIDGET_NODE (node))
    return FALSE;

  return TRUE;
}

static void
gtk_css_gadget_unset_node (GtkCssGadget *gadget)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);

  if (priv->node)
    {
      if (gtk_css_gadget_should_connect_style_changed (priv->node))
        {
        if (g_signal_handlers_disconnect_by_func (priv->node, gtk_css_gadget_node_style_changed_cb, gadget) != 1)
          {
            g_assert_not_reached ();
          }
        }
      g_object_unref (priv->node);
      priv->node = NULL;
    }
}

static void
gtk_css_gadget_set_node (GtkCssGadget *gadget,
                         GtkCssNode   *node)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);

  if (node != NULL)
    priv->node = g_object_ref (node);
  else
    priv->node = gtk_css_node_new ();

  if (gtk_css_gadget_should_connect_style_changed (priv->node))
    {
      g_signal_connect_after (priv->node,
                              "style-changed",
                              G_CALLBACK (gtk_css_gadget_node_style_changed_cb),
                              gadget);
    }
}

static void
gtk_css_gadget_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkCssGadget *gadget = GTK_CSS_GADGET (object);
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);

  switch (property_id)
  {
    case PROP_NODE:
      gtk_css_gadget_set_node (gadget, g_value_get_object (value));
      break;

    case PROP_OWNER:
      priv->owner = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
gtk_css_gadget_finalize (GObject *object)
{
  GtkCssGadget *gadget = GTK_CSS_GADGET (object);

  gtk_css_gadget_unset_node (gadget);

  G_OBJECT_CLASS (gtk_css_gadget_parent_class)->finalize (object);
}

static void
gtk_css_gadget_class_init (GtkCssGadgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_css_gadget_get_property;
  object_class->set_property = gtk_css_gadget_set_property;
  object_class->finalize = gtk_css_gadget_finalize;

  klass->get_preferred_size = gtk_css_gadget_real_get_preferred_size;
  klass->allocate = gtk_css_gadget_real_allocate;
  klass->draw = gtk_css_gadget_real_draw;
  klass->style_changed = gtk_css_gadget_real_style_changed;

  properties[PROP_NODE] = g_param_spec_object ("node", "Node",
                                               "CSS node",
                                               GTK_TYPE_CSS_NODE,
                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                               G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_OWNER] = g_param_spec_object ("owner", "Owner",
                                                "Widget that created and owns this gadget",
                                                GTK_TYPE_WIDGET,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);


  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_css_gadget_init (GtkCssGadget *gadget)
{

}

/**
 * gtk_css_gadget_get_node:
 * @gadget: a #GtkCssGadget
 *
 * Get the CSS node for this gadget.
 *
  * Returns: (transfer none):  the CSS node
 */
GtkCssNode *
gtk_css_gadget_get_node (GtkCssGadget *gadget)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);

  return priv->node;
}

/**
 * gtk_css_gadget_get_style:
 * @gadget: a #GtkCssGadget
 *
 * Get the CSS style for this gadget.
 *
 * Returns: (transfer none):  the CSS style
 */
GtkCssStyle *
gtk_css_gadget_get_style (GtkCssGadget *gadget)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);

  return gtk_css_node_get_style (priv->node);
}

/**
 * gtk_css_gadget_get_owner:
 * @gadget: a #GtkCssGadget
 *
 * Get the widget to which this gadget belongs.
 *
 * Returns: (transfer none):  the widget to which @gadget belongs
 */
GtkWidget *
gtk_css_gadget_get_owner (GtkCssGadget *gadget)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);

  return priv->owner;
}

/**
 * gtk_css_gadget_add_class:
 * @gadget: a #GtkCssGadget
 * @name: class name to use in CSS matching
 *
 * Adds a style class to the gadgets CSS node.
 */
void
gtk_css_gadget_add_class (GtkCssGadget *gadget,
                          const char   *name)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);
  GQuark quark;

  quark = g_quark_from_string (name);

  gtk_css_node_add_class (priv->node, quark);
}

/**
 * gtk_css_gadget_remove_class:
 * @gadget: a #GtkCssGadget
 * @name: class name
 *
 * Removes a style class from the gadgets CSS node.
 */
void
gtk_css_gadget_remove_class (GtkCssGadget *gadget,
                             const char   *name)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);
  GQuark quark;

  quark = g_quark_try_string (name);
  if (quark == 0)
    return;

  gtk_css_node_remove_class (priv->node, quark);
}

static gint
get_number (GtkCssStyle *style,
            guint        property)
{
  double d = _gtk_css_number_value_get (gtk_css_style_get_value (style, property), 100);

  if (d < 1)
    return ceil (d);
  else
    return floor (d);
}

static void
get_box_margin (GtkCssStyle *style,
                GtkBorder   *margin)
{
  margin->top = get_number (style, GTK_CSS_PROPERTY_MARGIN_TOP);
  margin->left = get_number (style, GTK_CSS_PROPERTY_MARGIN_LEFT);
  margin->bottom = get_number (style, GTK_CSS_PROPERTY_MARGIN_BOTTOM);
  margin->right = get_number (style, GTK_CSS_PROPERTY_MARGIN_RIGHT);
}

static void
get_box_border (GtkCssStyle *style,
                GtkBorder   *border)
{
  border->top = get_number (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH);
  border->left = get_number (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH);
  border->bottom = get_number (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH);
  border->right = get_number (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH);
}

static void
get_box_padding (GtkCssStyle *style,
                 GtkBorder   *border)
{
  border->top = get_number (style, GTK_CSS_PROPERTY_PADDING_TOP);
  border->left = get_number (style, GTK_CSS_PROPERTY_PADDING_LEFT);
  border->bottom = get_number (style, GTK_CSS_PROPERTY_PADDING_BOTTOM);
  border->right = get_number (style, GTK_CSS_PROPERTY_PADDING_RIGHT);
}

/**
 * gtk_css_gadget_get_preferred_size:
 * @gadget: the #GtkCssGadget whose size is requested
 * @orientation: whether a width (ie horizontal) or height (ie vertical) size is requested
 * @for_size: the available size in the opposite direction, or -1
 * @minimum: (nullable): return location for the minimum size
 * @natural: (nullable): return location for the natural size
 * @minimum_baseline: (nullable): return location for the baseline at minimum size
 * @natural_baseline: (nullable): return location for the baseline at natural size
 *
 * Gets the gadgets minimum and natural size (and, optionally, baseline)
 * in the given orientation for the specified size in the opposite direction.
 *
 * The returned values include CSS padding, border and margin in addition to the
 * gadgets content size, and respect the CSS min-with or min-height properties.
 *
 * The @for_size is assumed to include CSS padding, border and margins as well.
 */
void
gtk_css_gadget_get_preferred_size (GtkCssGadget   *gadget,
                                   GtkOrientation  orientation,
                                   gint            for_size,
                                   gint           *minimum,
                                   gint           *natural,
                                   gint           *minimum_baseline,
                                   gint           *natural_baseline)
{
  GtkCssStyle *style;
  GtkBorder margin, border, padding;
  int min_size, extra_size, extra_opposite, extra_baseline;
  int unused_minimum, unused_natural;

  if (minimum == NULL)
    minimum = &unused_minimum;
  if (natural == NULL)
    natural = &unused_natural;

  style = gtk_css_gadget_get_style (gadget);
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      extra_size = margin.left + margin.right + border.left + border.right + padding.left + padding.right;
      extra_opposite = margin.top + margin.bottom + border.top + border.bottom + padding.top + padding.bottom;
      extra_baseline = margin.left + border.left + padding.left;
      min_size = get_number (style, GTK_CSS_PROPERTY_MIN_WIDTH);
    }
  else
    {
      extra_size = margin.top + margin.bottom + border.top + border.bottom + padding.top + padding.bottom;
      extra_opposite = margin.left + margin.right + border.left + border.right + padding.left + padding.right;
      extra_baseline = margin.top + border.top + padding.top;
      min_size = get_number (style, GTK_CSS_PROPERTY_MIN_HEIGHT);
    }

  if (for_size > -1)
    for_size -= extra_opposite;

  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;

  GTK_CSS_GADGET_GET_CLASS (gadget)->get_preferred_size (gadget,
                                                         orientation,
                                                         for_size,
                                                         minimum, natural,
                                                         minimum_baseline, natural_baseline);

  g_warn_if_fail (*minimum <= *natural);

  *minimum = MAX (min_size, *minimum);
  *natural = MAX (min_size, *natural);

  *minimum += extra_size;
  *natural += extra_size;

  if (minimum_baseline && *minimum_baseline > -1)
    *minimum_baseline += extra_baseline;
  if (natural_baseline && *natural_baseline > -1)
    *natural_baseline += extra_baseline;
}

/**
 * gtk_css_gadget_allocate:
 * @gadget: the #GtkCssGadget to allocate
 * @allocation: the allocation
 * @baseline: the baseline for the allocation
 * @out_clip: (out): return location for the gadgets clip region
 *
 * Allocates the gadget.
 *
 * The @allocation is assumed to include CSS padding, border and margin.
 * The gadget content will be allocated a smaller area that excludes these.
 * The @out_clip includes the shadow extents of the gadget in addition to
 * any content clip.
 */
void
gtk_css_gadget_allocate (GtkCssGadget        *gadget,
                         const GtkAllocation *allocation,
                         int                  baseline,
                         GtkAllocation       *out_clip)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);
  GtkAllocation content_allocation;
  GtkAllocation content_clip = { 0, 0, 0, 0 };
  GtkBorder margin, border, padding, shadow, extents;
  GtkCssStyle *style;

  g_return_if_fail (out_clip != NULL);

  priv->allocated_size = *allocation;
  priv->allocated_baseline = baseline;

  style = gtk_css_gadget_get_style (gadget);
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);
  extents.top = margin.top + border.top + padding.top;
  extents.right = margin.right + border.right + padding.right;
  extents.bottom = margin.bottom + border.bottom + padding.bottom;
  extents.left = margin.left + border.left + padding.left;

  content_allocation.x = allocation->x + extents.left;
  content_allocation.y = allocation->y + extents.top;
  content_allocation.width = allocation->width - extents.left - extents.right;
  content_allocation.height = allocation->height - extents.top - extents.bottom;
  if (baseline >= 0)
    baseline -= extents.top;

  if (content_allocation.width < 0)
    {
      g_warning ("Negative content width while allocating gadget (node %s, owner %s)\n",
                 gtk_css_node_get_name (gtk_css_gadget_get_node (gadget)),
                 G_OBJECT_TYPE_NAME (gtk_css_gadget_get_owner (gadget)));
      content_allocation.width = 0;
    }

  if (content_allocation.height < 0)
    {
      g_warning ("Negative content height while allocating gadget (node %s, owner %s)\n",
                 gtk_css_node_get_name (gtk_css_gadget_get_node (gadget)),
                 G_OBJECT_TYPE_NAME (gtk_css_gadget_get_owner (gadget)));
      content_allocation.width = 0;
    }

  GTK_CSS_GADGET_GET_CLASS (gadget)->allocate (gadget, &content_allocation, baseline, &content_clip);

  _gtk_css_shadows_value_get_extents (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BOX_SHADOW), &shadow);

  out_clip->x = allocation->x + margin.left - shadow.left;
  out_clip->y = allocation->y + margin.top - shadow.top;
  out_clip->width = allocation->width - margin.left - margin.right + shadow.left + shadow.right;
  out_clip->height = allocation->height - margin.top - margin.bottom + shadow.top + shadow.bottom;

  if (content_clip.width > 0 && content_clip.height > 0)
    gdk_rectangle_union (&content_clip, out_clip, out_clip);
}

/**
 * gtk_css_gadget_draw:
 * @gadget: The gadget to draw
 * @cr: The cairo context to draw to
 *
 * Will draw the gadget at the position allocated via
 * gtk_css_gadget_allocate(). It is your responsibility to make
 * sure that those 2 coordinate systems match.
 *
 * The drawing virtual function will be passed an untransformed @cr.
 * This is important because functions like
 * gtk_container_propagate_draw() depend on that.
 */
void
gtk_css_gadget_draw (GtkCssGadget *gadget,
                     cairo_t      *cr)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);
  GtkBorder margin, border, padding;
  gboolean draw_focus;
  GtkCssStyle *style;
  int x, y, width, height;
  int contents_x, contents_y, contents_width, contents_height;

  x =  priv->allocated_size.x;
  y =  priv->allocated_size.y;
  if (priv->owner && !gtk_widget_get_has_window (priv->owner))
    {
      GtkAllocation widget_alloc;
      gtk_widget_get_allocation (priv->owner, &widget_alloc);
      x -= widget_alloc.x;
      y -= widget_alloc.y;
    }
  width = priv->allocated_size.width;
  height = priv->allocated_size.height;

  style = gtk_css_gadget_get_style (gadget);
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);

  gtk_css_style_render_background (style,
                                   cr,
                                   x + margin.left,
                                   y + margin.top,
                                   width - margin.left - margin.right,
                                   height - margin.top - margin.bottom,
                                   gtk_css_node_get_junction_sides (priv->node));
  gtk_css_style_render_border (style,
                               cr,
                               x + margin.left,
                               y + margin.top,
                               width - margin.left - margin.right,
                               height - margin.top - margin.bottom,
                               0,
                               gtk_css_node_get_junction_sides (priv->node));

  contents_x = x + margin.left + border.left + padding.left;
  contents_y = y + margin.top + border.top + padding.top;
  contents_width = width - margin.left - margin.right - border.left - border.right - padding.left - padding.right;
  contents_height = height - margin.top - margin.bottom - border.top - border.bottom - padding.top - padding.bottom;

  draw_focus = GTK_CSS_GADGET_GET_CLASS (gadget)->draw (gadget,
                                                        cr,
                                                        contents_x, contents_y,
                                                        contents_width, contents_height);

  if (draw_focus)
    gtk_css_style_render_outline (style,
                                  cr,
                                  x + margin.left,
                                  y + margin.top,
                                  width - margin.left - margin.right,
                                  height - margin.top - margin.bottom);
}

void
gtk_css_node_style_changed_for_widget (GtkCssNode  *node,
                                       GtkCssStyle *old_style,
                                       GtkCssStyle *new_style,
                                       GtkWidget    *widget)
{
  static GtkBitmask *affects_size = NULL;
  GtkBitmask *changes;
  
  changes = _gtk_bitmask_new ();
  changes = gtk_css_style_add_difference (changes, old_style, new_style);

  if (G_UNLIKELY (affects_size == NULL))
    affects_size = _gtk_css_style_property_get_mask_affecting (GTK_CSS_AFFECTS_SIZE | GTK_CSS_AFFECTS_CLIP);

  if (_gtk_bitmask_intersects (changes, affects_size))
    gtk_widget_queue_resize (widget);
  else
    gtk_widget_queue_draw (widget);

  _gtk_bitmask_free (changes);
}

void
gtk_css_gadget_get_border_allocation (GtkCssGadget  *gadget,
                                      GtkAllocation *allocation,
                                      int           *baseline)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);
  GtkBorder margin;

  g_return_if_fail (GTK_IS_CSS_GADGET (gadget));

  get_box_margin (gtk_css_gadget_get_style (gadget), &margin);

  if (allocation)
    {
      allocation->x = priv->allocated_size.x + margin.left;
      allocation->y = priv->allocated_size.y + margin.top;
      allocation->width = priv->allocated_size.width - margin.left - margin.right;
      allocation->height = priv->allocated_size.height - margin.top - margin.bottom;
    }
  if (baseline)
    {
      if (priv->allocated_baseline >= 0)
        *baseline = priv->allocated_baseline - margin.top;
      else
        *baseline = -1;
    }
}

void
gtk_css_gadget_get_content_allocation (GtkCssGadget  *gadget,
                                       GtkAllocation *allocation,
                                       int           *baseline)
{
  GtkCssGadgetPrivate *priv = gtk_css_gadget_get_instance_private (gadget);
  GtkBorder margin, border, padding, extents;
  GtkCssStyle *style;

  g_return_if_fail (GTK_IS_CSS_GADGET (gadget));

  style = gtk_css_gadget_get_style (gadget);
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);
  extents.top = margin.top + border.top + padding.top;
  extents.right = margin.right + border.right + padding.right;
  extents.bottom = margin.bottom + border.bottom + padding.bottom;
  extents.left = margin.left + border.left + padding.left;

  if (allocation)
    {
      allocation->x = priv->allocated_size.x + extents.left;
      allocation->y = priv->allocated_size.y + extents.top;
      allocation->width = priv->allocated_size.width - extents.left - extents.right;
      allocation->height = priv->allocated_size.height - extents.top - extents.bottom;
    }
  if (baseline)
    {
      if (priv->allocated_baseline >= 0)
        *baseline = priv->allocated_baseline - extents.top;
      else
        *baseline = -1;
    }
}
