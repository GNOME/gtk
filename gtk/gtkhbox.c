/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkhbox.h"


static void gtk_hbox_class_init    (GtkHBoxClass   *klass);
static void gtk_hbox_init          (GtkHBox        *box);
static void gtk_hbox_size_request  (GtkWidget      *widget,
				    GtkRequisition *requisition);
static void gtk_hbox_size_allocate (GtkWidget      *widget,
				    GtkAllocation  *allocation);


GtkType
gtk_hbox_get_type (void)
{
  static GtkType hbox_type = 0;

  if (!hbox_type)
    {
      static const GtkTypeInfo hbox_info =
      {
	"GtkHBox",
	sizeof (GtkHBox),
	sizeof (GtkHBoxClass),
	(GtkClassInitFunc) gtk_hbox_class_init,
	(GtkObjectInitFunc) gtk_hbox_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      hbox_type = gtk_type_unique (GTK_TYPE_BOX, &hbox_info);
    }

  return hbox_type;
}

static void
gtk_hbox_class_init (GtkHBoxClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->size_request = gtk_hbox_size_request;
  widget_class->size_allocate = gtk_hbox_size_allocate;
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

  hbox = gtk_type_new (gtk_hbox_get_type ());

  GTK_BOX (hbox)->spacing = spacing;
  GTK_BOX (hbox)->homogeneous = homogeneous ? TRUE : FALSE;

  return GTK_WIDGET (hbox);
}


static void
gtk_hbox_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;
  gint nvis_children;
  gint width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HBOX (widget));
  g_return_if_fail (requisition != NULL);

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
	  GtkRequisition child_requisition;

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

	  requisition->height = MAX (requisition->height, child_requisition.height);

	  nvis_children += 1;
	}
    }

  if (nvis_children > 0)
    {
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
  GtkBoxChild *child;
  GList *children;
  GtkAllocation child_allocation;
  gint nvis_children;
  gint nexpand_children;
  gint child_width;
  gint width;
  gint extra;
  gint x;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HBOX (widget));
  g_return_if_fail (allocation != NULL);

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

              gtk_widget_size_allocate (child->widget, &child_allocation);

              x -= (child_width + box->spacing);
	    }
	}
    }
}
