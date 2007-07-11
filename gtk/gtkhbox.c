/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gtkhbox.h"
#include "gtkextendedlayout.h"
#include "gtkintl.h"
#include "gtkalias.h"


enum {
  PROP_0,
  PROP_BASELINE_POLICY
};

#define GTK_HBOX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_HBOX, GtkHBoxPrivate))

typedef struct _GtkHBoxPrivate GtkHBoxPrivate;

struct _GtkHBoxPrivate
{
  GtkBaselinePolicy baseline_policy;
  gint baseline;
};

static void gtk_hbox_set_property  (GObject        *object,
		                    guint           prop_id,
		                    const GValue   *value,
		                    GParamSpec     *pspec);
static void gtk_hbox_get_property  (GObject        *object,
		                    guint           prop_id,
		                    GValue         *value,
		                    GParamSpec     *pspec);

static void gtk_hbox_size_request  (GtkWidget      *widget,
				    GtkRequisition *requisition);
static void gtk_hbox_size_allocate (GtkWidget      *widget,
				    GtkAllocation  *allocation);

static void gtk_hbox_extended_layout_interface_init (GtkExtendedLayoutIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkHBox, gtk_hbox, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EXTENDED_LAYOUT,
						gtk_hbox_extended_layout_interface_init))

static void
gtk_hbox_class_init (GtkHBoxClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->set_property = gtk_hbox_set_property;
  gobject_class->get_property = gtk_hbox_get_property;

  widget_class->size_request = gtk_hbox_size_request;
  widget_class->size_allocate = gtk_hbox_size_allocate;

  g_type_class_add_private (gobject_class, sizeof (GtkHBoxPrivate));
  
  g_object_class_install_property (gobject_class,
				   PROP_BASELINE_POLICY,
				   g_param_spec_enum ("baseline-policy", 
						      P_("Baseline policy"), 
						      P_("Indicates which baseline of children to use for vertical alignment"),
						      GTK_TYPE_BASELINE_POLICY, GTK_BASELINE_NONE,
						      G_PARAM_READWRITE));
}

static void
gtk_hbox_init (GtkHBox *hbox)
{
}

GtkWidget*
gtk_hbox_new (gboolean homogeneous,
	      gint spacing)
{
  GtkHBox *hbox;

  hbox = g_object_new (GTK_TYPE_HBOX, NULL);

  GTK_BOX (hbox)->spacing = spacing;
  GTK_BOX (hbox)->homogeneous = homogeneous ? TRUE : FALSE;

  return GTK_WIDGET (hbox);
}


static void 
gtk_hbox_set_property (GObject      *object,
		       guint         prop_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
  GtkHBoxPrivate *priv = GTK_HBOX_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_BASELINE_POLICY:
      priv->baseline_policy = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_hbox_get_property (GObject    *object,
		       guint       prop_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
  GtkHBoxPrivate *priv = GTK_HBOX_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_BASELINE_POLICY:
      g_value_set_enum (value, priv->baseline_policy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_hbox_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  GtkBox *box = GTK_BOX (widget);

  gint nvis_children;
  GtkBoxChild *child;
  GList *children;

  requisition->width = 0;
  requisition->height = 0;

  nvis_children = 0;
  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
        nvis_children += 1;
    }

  if (nvis_children > 0)
    {
      GtkHBoxPrivate *priv = GTK_HBOX_GET_PRIVATE (widget);
      gint *baselines = NULL;
      gint i;

      if (priv->baseline_policy != GTK_BASELINE_NONE)
        {
          baselines = g_newa (gint, nvis_children);
          priv->baseline = 0;

          i = 0;
          children = box->children;
          while (children)
            {
              child = children->data;
              children = children->next;

              if (GTK_WIDGET_VISIBLE (child->widget))
                {
                  if (GTK_IS_EXTENDED_LAYOUT (child->widget) &&
                      GTK_EXTENDED_LAYOUT_HAS_BASELINES (child->widget))
                    {
                      GtkExtendedLayout *layout = GTK_EXTENDED_LAYOUT (child->widget);

                      gtk_extended_layout_set_baseline_offset (layout, 0);
                      baselines[i] = gtk_extended_layout_get_single_baseline (layout, priv->baseline_policy);
                      priv->baseline = MAX (priv->baseline, baselines[i]);
                    }
                  else
                    baselines[i] = 0;

                  ++i;
                }
            }
        }

      i = 0;
      children = box->children;
      while (children)
        {
          child = children->data;
          children = children->next;

          if (GTK_WIDGET_VISIBLE (child->widget))
            {
              GtkRequisition child_requisition;
              gint width;

              gtk_widget_size_request (child->widget, &child_requisition);

              if (box->homogeneous)
                {
                  width = child_requisition.width + child->padding * 2;
                  requisition->width = MAX (requisition->width, width);
                }
              else
                {
                  requisition->width += child_requisition.width + child->padding * 2;
                }

              if (baselines)
                {
                  gint padding = MAX (priv->baseline - baselines[i], 0);
                  child_requisition.height += padding;
                }

              requisition->height = MAX (requisition->height, child_requisition.height);

              ++i;
            }
        }

      if (box->homogeneous)
	requisition->width *= nvis_children;

      requisition->width += (nvis_children - 1) * box->spacing;
    }

  requisition->width += GTK_CONTAINER (box)->border_width * 2;
  requisition->height += GTK_CONTAINER (box)->border_width * 2;
}

static void
gtk_hbox_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkBox *box;

  gint nvis_children;
  gint nexpand_children;
  GtkBoxChild *child;
  GList *children;

  box = GTK_BOX (widget);
  widget->allocation = *allocation;

  nvis_children = 0;
  nexpand_children = 0;
  children = box->children;

  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  nvis_children += 1;
	  if (child->expand)
	    nexpand_children += 1;
	}
    }

  if (nvis_children > 0)
    {
      GtkHBoxPrivate *priv = GTK_HBOX_GET_PRIVATE (widget);

      GtkPackType packing;

      GtkTextDirection direction;
      gint border_width;

      GtkAllocation child_allocation;

      gint *natural_requisitions;
      gint *minimum_requisitions;

      gint natural_width;
      gint minimum_width;

      gint available, natural, extra;
      gint i;

      direction = gtk_widget_get_direction (widget);
      border_width = GTK_CONTAINER (box)->border_width;

      natural_width = 0;
      natural_requisitions = g_newa (gint, nvis_children);
      minimum_requisitions = g_newa (gint, nvis_children);

      children = box->children;
      i = 0;

      while (children)
        {
          child = children->data;
          children = children->next;

          if (GTK_WIDGET_VISIBLE (child->widget))
            {
              GtkRequisition child_requisition;

              gtk_widget_size_request (child->widget, &child_requisition);
              minimum_requisitions[i] = child_requisition.width;

              if (GTK_IS_EXTENDED_LAYOUT (child->widget) &&
                  GTK_EXTENDED_LAYOUT_HAS_NATURAL_SIZE (child->widget))
                {
                  gtk_extended_layout_get_natural_size (
                    GTK_EXTENDED_LAYOUT (child->widget), 
                    &child_requisition);
                  natural_requisitions[i] =
                    child_requisition.width - 
                    minimum_requisitions[i];
                }
              else
                natural_requisitions[i] = 0;

              natural_width += natural_requisitions[i++];
            }
        }

      minimum_width = widget->requisition.width - border_width * 2 - 
                      (nvis_children - 1) * box->spacing;

      if (box->homogeneous)
	{
	  available = (allocation->width - border_width * 2 -
		       (nvis_children - 1) * box->spacing);
	  extra = available / nvis_children;
          natural = 0;
	}
      else if (nexpand_children > 0)
	{
	  available = (gint)allocation->width - widget->requisition.width;
          natural = MAX (0, MIN (available, natural_width));
          available -= natural;

	  extra = MAX (0, available / nexpand_children);
	}
      else
	{
	  available = 0;
          natural = 0;
	  extra = 0;
	}

      child_allocation.y = allocation->y + border_width;
      child_allocation.height = MAX (1, (gint) allocation->height - (gint) border_width * 2);

      for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
        {
          gint x;

          if (GTK_PACK_START == packing)
            x = allocation->x + border_width;
          else
            x = allocation->x + allocation->width - border_width;

          i = 0;
          children = box->children;
          while (children)
            {
              child = children->data;
              children = children->next;

              if (GTK_WIDGET_VISIBLE (child->widget))
                {
                  if ((child->pack == packing))
                    {
                      gint child_width;

                      if (box->homogeneous)
                        {
                          if (nvis_children == 1)
                            child_width = available;
                          else
                            child_width = extra;

                          nvis_children -= 1;
                          available -= extra;
                        }
                      else
                        {
                          child_width = minimum_requisitions[i] + child->padding * 2;

                          if (child->expand)
                            {
                              if (nexpand_children == 1)
                                child_width += available;
                              else
                                child_width += extra;

                              nexpand_children -= 1;
                              available -= extra;
                            }
                        }

		      if (natural_width > 0)
                        child_width += natural * natural_requisitions[i] / natural_width;

                      if (child->fill)
                        {
                          child_allocation.width = MAX (1, (gint) child_width - (gint) child->padding * 2);
                          child_allocation.x = x + child->padding;
                        }
                      else
                        {
                          child_allocation.width = minimum_requisitions[i];
                          child_allocation.x = x + (child_width - child_allocation.width) / 2;
                        }

                      if (GTK_PACK_END == packing)
                        child_allocation.x -= child_width;

                      if (GTK_TEXT_DIR_RTL == direction)
                        child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

                      gtk_widget_size_allocate (child->widget, &child_allocation);

                      if (GTK_PACK_START == packing)
                        x += child_width + box->spacing;
                      else
                        x -= child_width + box->spacing;
                    } /* packing */

                  ++i;
                } /* visible */
            } /* while children */
        } /* for packing */

      if (GTK_BASELINE_NONE != priv->baseline_policy)
        {
          i = 0;
          children = box->children;
          while (children)
            {
              child = children->data;
              children = children->next;

              if (GTK_WIDGET_VISIBLE (child->widget))
                {
                  if (GTK_IS_EXTENDED_LAYOUT (child->widget) &&
                      GTK_EXTENDED_LAYOUT_HAS_BASELINES (child->widget))
                    {
                      GtkExtendedLayout *layout = GTK_EXTENDED_LAYOUT (child->widget);
                      gint baseline, dy;

                      gtk_extended_layout_set_baseline_offset (layout, 0);
                      baseline = gtk_extended_layout_get_single_baseline (layout, priv->baseline_policy);
                      dy = priv->baseline - baseline;

                      gtk_extended_layout_set_baseline_offset (layout, dy);
                    }

                  ++i;
                }
            }
        } /* baseline_policy */
    } /* nvis_children */
}

/**
 * gtk_hbox_get_baseline_policy:
 * @hbox: a #GtkHBox
 *
 * Returns which baseline of children is used 
 * to vertically align them.
 *
 * Return value: the baseline alignment policy of the @hbox.
 **/
GtkBaselinePolicy
gtk_hbox_get_baseline_policy (GtkHBox *hbox)
{
  g_return_val_if_fail (GTK_IS_HBOX (hbox), GTK_BASELINE_NONE);
  return GTK_HBOX_GET_PRIVATE (hbox)->baseline_policy;
}

/**
 * gtk_hbox_set_baseline_policy:
 * @box: a #GtkBox
 * @policy: the baseline alignment policy
 *
 * Sets the #GtkHBox:baseline-policy property of @vbox, 
 * which is the policy to vertically align children.
 */
void
gtk_hbox_set_baseline_policy (GtkHBox           *hbox,
                              GtkBaselinePolicy  policy)
{
  g_return_if_fail (GTK_IS_HBOX (hbox));
  g_object_set (hbox, "baseline-policy", policy, NULL);
}

static GtkExtendedLayoutFeatures
gtk_hbox_extended_layout_get_features (GtkExtendedLayout *layout)
{
  GtkExtendedLayoutFeatures features;
  GtkHBoxPrivate *priv;

  features = GTK_EXTENDED_LAYOUT_NATURAL_SIZE;
  priv = GTK_HBOX_GET_PRIVATE (layout);

  if (GTK_BASELINE_NONE != priv->baseline_policy)
    features |= GTK_EXTENDED_LAYOUT_BASELINES;
  
  return features;
}

static void
gtk_hbox_extended_layout_get_natural_size (GtkExtendedLayout *layout,
                                           GtkRequisition    *requisition)
{
  GtkBox *box = GTK_BOX (layout);

  GtkRequisition child_requisition;
  GtkBoxChild *child;
  GList *children;

  requisition->width = GTK_CONTAINER (box)->border_width * 2;
  requisition->height = GTK_CONTAINER (box)->border_width * 2;

  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
          if (GTK_IS_EXTENDED_LAYOUT (child->widget) &&
              GTK_EXTENDED_LAYOUT_HAS_NATURAL_SIZE (child->widget))
            gtk_extended_layout_get_natural_size (GTK_EXTENDED_LAYOUT (child->widget),
                                                  &child_requisition);
          else
            gtk_widget_size_request (child->widget, &child_requisition);

          requisition->width += child_requisition.width;
          requisition->height = MAX (child_requisition.height, requisition->height);
	}
    }
}

static gint
gtk_hbox_extended_layout_get_baselines (GtkExtendedLayout  *layout,
                                        gint              **baselines)
{
  GtkHBoxPrivate *priv;
  GtkBox *box;

  gint hbox_baseline = 0;
  gint child_baseline;

  GtkBoxChild *child;
  GList *children;

  priv = GTK_HBOX_GET_PRIVATE (layout);

  g_return_val_if_fail (GTK_BASELINE_NONE != priv->baseline_policy, -1);

  box = GTK_BOX (layout);
  children = box->children;

  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget) &&
          GTK_IS_EXTENDED_LAYOUT (child->widget) &&
          GTK_EXTENDED_LAYOUT_HAS_BASELINES (child->widget))
        {
          child_baseline = gtk_extended_layout_get_single_baseline (
            GTK_EXTENDED_LAYOUT (child->widget), priv->baseline_policy);
          hbox_baseline = MAX (hbox_baseline, child_baseline);
        }
    }

  *baselines = g_new (gint, 1);
  *baselines[0] = hbox_baseline;

  return 1;
}

static void
gtk_hbox_extended_layout_interface_init (GtkExtendedLayoutIface *iface)
{
  iface->get_features = gtk_hbox_extended_layout_get_features;
  iface->get_natural_size = gtk_hbox_extended_layout_get_natural_size;
  iface->get_baselines = gtk_hbox_extended_layout_get_baselines;
}

#define __GTK_HBOX_C__
#include "gtkaliasdef.c"
