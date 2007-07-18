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
#include "gtkvbox.h"
#include "gtkextendedlayout.h"
#include "gtkintl.h"
#include "gtkalias.h"


static void gtk_vbox_size_request  (GtkWidget      *widget,
				    GtkRequisition *requisition);
static void gtk_vbox_size_allocate (GtkWidget      *widget,
				    GtkAllocation  *allocation);

static void gtk_vbox_extended_layout_interface_init (GtkExtendedLayoutIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkVBox, gtk_vbox, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EXTENDED_LAYOUT,
						gtk_vbox_extended_layout_interface_init))

static void
gtk_vbox_class_init (GtkVBoxClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->size_request = gtk_vbox_size_request;
  widget_class->size_allocate = gtk_vbox_size_allocate;
}

static void
gtk_vbox_init (GtkVBox *vbox)
{
}

GtkWidget*
gtk_vbox_new (gboolean homogeneous,
	      gint spacing)
{
  GtkVBox *vbox;

  vbox = g_object_new (GTK_TYPE_VBOX, NULL);

  GTK_BOX (vbox)->spacing = spacing;
  GTK_BOX (vbox)->homogeneous = homogeneous ? TRUE : FALSE;

  return GTK_WIDGET (vbox);
}


static void
gtk_vbox_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  GtkBox *box;
  GtkBoxChild *child;
  GtkRequisition child_requisition;
  GList *children;
  gint nvis_children;
  gint height;

  box = GTK_BOX (widget);
  requisition->width = 0;
  requisition->height = 0;
  nvis_children = 0;

  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  gtk_widget_size_request (child->widget, &child_requisition);

	  if (box->homogeneous)
	    {
	      height = child_requisition.height + child->padding * 2;
	      requisition->height = MAX (requisition->height, height);
	    }
	  else
	    {
	      requisition->height += child_requisition.height + child->padding * 2;
	    }

	  requisition->width = MAX (requisition->width, child_requisition.width);

	  nvis_children += 1;
	}
    }

  if (nvis_children > 0)
    {
      if (box->homogeneous)
	requisition->height *= nvis_children;
      requisition->height += (nvis_children - 1) * box->spacing;
    }

  requisition->width += GTK_CONTAINER (box)->border_width * 2;
  requisition->height += GTK_CONTAINER (box)->border_width * 2;
}

static void
gtk_vbox_size_allocate (GtkWidget     *widget,
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
      GtkPackType packing;
      gint border_width;

      GtkAllocation child_allocation;
      gint *natural_requisitions;
      gint *minimum_requisitions;

      gint available, natural, extra;
      gint natural_height;
      gint i;

      border_width = GTK_CONTAINER (box)->border_width;

      natural_height = 0;
      natural_requisitions = g_newa (gint, nvis_children);
      minimum_requisitions = g_newa (gint, nvis_children);

      i = 0;
      children = box->children;

      while (children)
        {
          child = children->data;
          children = children->next;

          if (GTK_WIDGET_VISIBLE (child->widget))
            {
              GtkRequisition child_requisition;

              gtk_widget_size_request (child->widget, &child_requisition);
              minimum_requisitions[i] = child_requisition.height;

              if (GTK_IS_EXTENDED_LAYOUT (child->widget) &&
                  GTK_EXTENDED_LAYOUT_HAS_NATURAL_SIZE (child->widget))
                {
                  gtk_extended_layout_get_natural_size (
                    GTK_EXTENDED_LAYOUT (child->widget), 
                    &child_requisition);
                  natural_requisitions[i] =
                    child_requisition.height - 
                    minimum_requisitions[i];
                }
              else
                natural_requisitions[i] = 0;

              natural_height += natural_requisitions[i++];
            }
        }

      if (box->homogeneous)
	{
	  available = (allocation->height - border_width * 2 -
		      (nvis_children - 1) * box->spacing);
	  extra = available / nvis_children;
          natural = 0;
	}
      else if (nexpand_children > 0)
	{
	  available = (gint)allocation->height - widget->requisition.height;
          natural = MAX (0, MIN (available, natural_height));
          available -= natural;

	  extra = MAX (0, available / nexpand_children);
	}
      else
	{
	  available = 0;
          natural = 0;
	  extra = 0;
	}

      child_allocation.x = allocation->x + border_width;
      child_allocation.width = MAX (1, (gint) allocation->width - (gint) border_width * 2);

      for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
        {
          gint y;

          if (GTK_PACK_START == packing)
            y = allocation->y + border_width;
          else
            y = allocation->y + allocation->height - border_width;

          i = 0;
          children = box->children;
          while (children)
            {
              child = children->data;
              children = children->next;

              if (GTK_WIDGET_VISIBLE (child->widget))
                {
                  if (child->pack == packing)
                    {
                      gint child_height;

                      if (box->homogeneous)
                        {
                          if (nvis_children == 1)
                            child_height = available;
                          else
                            child_height = extra;

                          nvis_children -= 1;
                          available -= extra;
                        }
                      else
                        {
                          child_height = minimum_requisitions[i] + child->padding * 2;

                          if (child->expand)
                            {
                              if (nexpand_children == 1)
                                child_height += available;
                              else
                                child_height += extra;

                              nexpand_children -= 1;
                              available -= extra;
                            }
                        }

		      if (natural_height > 0)
                        child_height += natural * natural_requisitions[i] / natural_height;

                      if (child->fill)
                        {
                          child_allocation.height = MAX (1, child_height - (gint)child->padding * 2);
                          child_allocation.y = y + child->padding;
                        }
                      else
                        {
                          child_allocation.height = minimum_requisitions[i];
                          child_allocation.y = y + (child_height - child_allocation.height) / 2;
                        }

                      if (GTK_PACK_END == packing)
                        child_allocation.y -= child_height;

                      gtk_widget_size_allocate (child->widget, &child_allocation);

                      if (GTK_PACK_START == packing)
	                y += child_height + box->spacing;
                      else
                        y -= child_height + box->spacing;
                    } /* packing */

                  ++i;
                } /* visible */
            } /* while children */
        } /* for packing */
    } /* nvis_children */
}

static GtkExtendedLayoutFeatures
gtk_vbox_extended_layout_get_features (GtkExtendedLayout *layout)
{
  return GTK_EXTENDED_LAYOUT_NATURAL_SIZE;
}

static void
gtk_vbox_extended_layout_get_natural_size (GtkExtendedLayout *layout,
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

          requisition->width += MAX (child_requisition.width, requisition->width);
          requisition->height = child_requisition.height;
	}
    }
}

static void
gtk_vbox_extended_layout_interface_init (GtkExtendedLayoutIface *iface)
{
  iface->get_features = gtk_vbox_extended_layout_get_features;
  iface->get_natural_size = gtk_vbox_extended_layout_get_natural_size;
}

#define __GTK_VBOX_C__
#include "gtkaliasdef.c"
