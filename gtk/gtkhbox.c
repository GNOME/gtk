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
  gint effective_baseline;
  gint *baselines;
};

static void gtk_hbox_dispose       (GObject        *object);
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

G_DEFINE_TYPE (GtkHBox, gtk_hbox, GTK_TYPE_BOX)

static void
gtk_hbox_class_init (GtkHBoxClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->dispose = gtk_hbox_dispose;
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
gtk_hbox_dispose (GObject    *object)
{
  GtkHBoxPrivate *priv = GTK_HBOX_GET_PRIVATE (object);

  g_free (priv->baselines);
  priv->baselines = NULL;

  G_OBJECT_CLASS (gtk_hbox_parent_class)->dispose (object);
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
      gint i_child;

      g_free (priv->baselines);
      priv->baselines = g_new0 (gint, nvis_children);

      priv->effective_baseline = 0;
      if (priv->baseline_policy != GTK_BASELINE_NONE)
        {
          i_child = 0;
          children = box->children;
          while (children)
            {
              child = children->data;
              children = children->next;

              if (GTK_WIDGET_VISIBLE (child->widget))
                {
                  if (GTK_IS_EXTENDED_LAYOUT (child->widget))
                    {
                      priv->baselines[i_child] = gtk_extended_layout_get_single_baseline (
                        GTK_EXTENDED_LAYOUT (child->widget), priv->baseline_policy);

                      priv->effective_baseline = MAX (priv->effective_baseline, priv->baselines[i_child]);
                    }

                  ++i_child;
                }
            }
        }

      i_child = 0;
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

              child_requisition.height += MAX (0, (priv->effective_baseline - priv->baselines[i_child]));
              requisition->height = MAX (requisition->height, child_requisition.height);

              ++i_child;
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

  GtkTextDirection direction;

  box = GTK_BOX (widget);
  widget->allocation = *allocation;

  direction = gtk_widget_get_direction (widget);
  
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
      GtkAllocation child_allocation;
      gint child_width;
      gint i_child;

      gint width;
      gint extra;
      gint x;

      if (box->homogeneous)
	{
	  width = (allocation->width -
		   GTK_CONTAINER (box)->border_width * 2 -
		   (nvis_children - 1) * box->spacing);
	  extra = width / nvis_children;
	}
      else if (nexpand_children > 0)
	{
	  width = (gint) allocation->width - (gint) widget->requisition.width;
	  extra = width / nexpand_children;
	}
      else
	{
	  width = 0;
	  extra = 0;
	}

      x = allocation->x + GTK_CONTAINER (box)->border_width;
      child_allocation.y = allocation->y + GTK_CONTAINER (box)->border_width;
      child_allocation.height = MAX (1, (gint) allocation->height - (gint) GTK_CONTAINER (box)->border_width * 2);

      children = box->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if ((child->pack == GTK_PACK_START) && GTK_WIDGET_VISIBLE (child->widget))
	    {
	      if (box->homogeneous)
		{
		  if (nvis_children == 1)
		    child_width = width;
		  else
		    child_width = extra;

		  nvis_children -= 1;
		  width -= extra;
		}
	      else
		{
		  GtkRequisition child_requisition;

		  gtk_widget_get_child_requisition (child->widget, &child_requisition);

		  child_width = child_requisition.width + child->padding * 2;

		  if (child->expand)
		    {
		      if (nexpand_children == 1)
			child_width += width;
		      else
			child_width += extra;

		      nexpand_children -= 1;
		      width -= extra;
		    }
		}

	      if (child->fill)
		{
		  child_allocation.width = MAX (1, (gint) child_width - (gint) child->padding * 2);
		  child_allocation.x = x + child->padding;
		}
	      else
		{
		  GtkRequisition child_requisition;

		  gtk_widget_get_child_requisition (child->widget, &child_requisition);
		  child_allocation.width = child_requisition.width;
		  child_allocation.x = x + (child_width - child_allocation.width) / 2;
		}

	      if (direction == GTK_TEXT_DIR_RTL)
		child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

	      gtk_widget_size_allocate (child->widget, &child_allocation);

	      x += child_width + box->spacing;
	    }
	}

      x = allocation->x + allocation->width - GTK_CONTAINER (box)->border_width;

      children = box->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if ((child->pack == GTK_PACK_END) && GTK_WIDGET_VISIBLE (child->widget))
	    {
	      GtkRequisition child_requisition;
	      gtk_widget_get_child_requisition (child->widget, &child_requisition);

              if (box->homogeneous)
                {
                  if (nvis_children == 1)
                    child_width = width;
                  else
                    child_width = extra;

                  nvis_children -= 1;
                  width -= extra;
                }
              else
                {
		  child_width = child_requisition.width + child->padding * 2;

                  if (child->expand)
                    {
                      if (nexpand_children == 1)
                        child_width += width;
                      else
                        child_width += extra;

                      nexpand_children -= 1;
                      width -= extra;
                    }
                }

              if (child->fill)
                {
                  child_allocation.width = MAX (1, (gint)child_width - (gint)child->padding * 2);
                  child_allocation.x = x + child->padding - child_width;
                }
              else
                {
		  child_allocation.width = child_requisition.width;
                  child_allocation.x = x + (child_width - child_allocation.width) / 2 - child_width;
                }

	      if (direction == GTK_TEXT_DIR_RTL)
		child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

              gtk_widget_size_allocate (child->widget, &child_allocation);

              x -= (child_width + box->spacing);
	    }
	}

      i_child = 0;
      children = box->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (child->widget))
	    {
              gint offset;

              offset = MAX (0, (priv->effective_baseline - priv->baselines[i_child]));

              child->widget->allocation.y += offset;
              child->widget->allocation.height -= offset;

              ++i_child;
            }
        }
    }
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

#define __GTK_HBOX_C__
#include "gtkaliasdef.c"
