/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * 
 * GtkPacker Widget 
 * Copyright (C) 1998 Shawn T. Amundson, James S. Mitchell, Michael L. Staiger
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
 * This file contains modified code derived from Tk 8.0.  Below is the header of
 * the relevant file.  The file 'license.terms' is included inline below.
 * 
 * tkPack.c --
 *
 *      This file contains code to implement the "packer"
 *      geometry manager for Tk.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkPack.c 1.64 96/05/03 10:51:52
 *
 * The file license.terms is below.  NOTE: THE FOLLOWING APPLIES ONLY TO
 * PORTIONS DERIVED FROM TK 8.0.  THE LICENSE FOR THIS FILE IS LGPL, AS
 * STATED ABOVE AND ALLOWED BELOW.  
-- BEGIN license.terms --
This software is copyrighted by the Regents of the University of
California, Sun Microsystems, Inc., and other parties.  The following
terms apply to all files associated with the software unless explicitly
disclaimed in individual files.

The authors hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal 
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license.
-- END license.terms --
 *
 */


#include "gtkpacker.h"


static void gtk_packer_class_init    (GtkPackerClass   *klass);
static void gtk_packer_init          (GtkPacker        *packer);
static void gtk_packer_map           (GtkWidget        *widget);
static void gtk_packer_unmap         (GtkWidget        *widget);
static void gtk_packer_draw          (GtkWidget        *widget,
                                      GdkRectangle     *area);
static gint gtk_packer_expose        (GtkWidget        *widget,
                                      GdkEventExpose   *event);
static void gtk_packer_size_request  (GtkWidget      *widget,
                                      GtkRequisition *requisition);
static void gtk_packer_size_allocate (GtkWidget      *widget,
                                      GtkAllocation  *allocation);
static void gtk_packer_container_add (GtkContainer   *container,
                                      GtkWidget      *child);
static void gtk_packer_remove        (GtkContainer   *container,
                                      GtkWidget      *widget);
static void gtk_packer_foreach       (GtkContainer   *container,
                                      GtkCallback     callback,
                                      gpointer        callback_data);
static GtkType gtk_packer_child_type (GtkContainer   *container);


static GtkPackerClass *parent_class;

GtkType
gtk_packer_get_type (void)
{
  static GtkType packer_type = 0;

  if (!packer_type)
    {
      GtkTypeInfo packer_info =
      {
        "GtkPacker",
        sizeof (GtkPacker),
        sizeof (GtkPackerClass),
        (GtkClassInitFunc) gtk_packer_class_init,
        (GtkObjectInitFunc) gtk_packer_init,
        (GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL
      };

      packer_type = gtk_type_unique (gtk_container_get_type (), &packer_info);
    }
  
  return packer_type;
}

static void
gtk_packer_class_init (GtkPackerClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  parent_class = gtk_type_class (gtk_container_get_type ());
  
  widget_class->map = gtk_packer_map;
  widget_class->unmap = gtk_packer_unmap;
  widget_class->draw = gtk_packer_draw;
  widget_class->expose_event = gtk_packer_expose;
  
  widget_class->size_request = gtk_packer_size_request;
  widget_class->size_allocate = gtk_packer_size_allocate;
  
  container_class->add = gtk_packer_container_add;
  container_class->remove = gtk_packer_remove;
  container_class->foreach = gtk_packer_foreach;
  container_class->child_type = gtk_packer_child_type;
}

static GtkType
gtk_packer_child_type (GtkContainer   *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_packer_init (GtkPacker *packer)
{
  GTK_WIDGET_SET_FLAGS (packer, GTK_NO_WINDOW | GTK_BASIC);
  
  packer->children = NULL;
  packer->spacing = 0;
}

void
gtk_packer_set_spacing (GtkPacker *packer,
			guint      spacing)
{
  g_return_if_fail (packer != NULL);
  g_return_if_fail (GTK_IS_PACKER (packer));
  
  if (spacing != packer->spacing) 
    {
      packer->spacing = spacing;
      gtk_widget_queue_resize(GTK_WIDGET(packer));
    }
};

GtkWidget*
gtk_packer_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_packer_get_type ()));
}

static void
redo_defaults_children (GtkPacker *packer)
{
  GList *list;
  GtkPackerChild *child;
  
  list = g_list_first(packer->children);
  while (list != NULL) 
    {
      child = list->data;
      
      if (child->use_default) 
	{
	  child->border_width = packer->default_border_width;
	  child->pad_x = packer->default_pad_x;
	  child->pad_y = packer->default_pad_y;
	  child->i_pad_x = packer->default_i_pad_x;
	  child->i_pad_y = packer->default_i_pad_y;
	  gtk_widget_queue_resize(GTK_WIDGET(packer));
	}
      list = g_list_next(list);
    }
}

void
gtk_packer_set_default_border_width (GtkPacker *packer,
				     guint      border)
{
  g_return_if_fail (packer != NULL);
  g_return_if_fail (GTK_IS_PACKER (packer));
  
  if (packer->default_border_width != border) 
    {
      packer->default_border_width = border;;
      redo_defaults_children (packer);
    }
}
void
gtk_packer_set_default_pad(GtkPacker *packer,
			   guint      pad_x,
			   guint      pad_y)
{
  g_return_if_fail (packer != NULL);
  g_return_if_fail (GTK_IS_PACKER (packer));
  
  if (packer->default_pad_x != pad_x ||
      packer->default_pad_y != pad_y) 
    {
      packer->default_pad_x = pad_x;
      packer->default_pad_y = pad_y;
      redo_defaults_children (packer);
    }
}

void
gtk_packer_set_default_ipad(GtkPacker *packer,
			    guint      i_pad_x,
			    guint      i_pad_y)
{
  g_return_if_fail (packer != NULL);
  g_return_if_fail (GTK_IS_PACKER (packer));
  
  if (packer->default_i_pad_x != i_pad_x ||
      packer->default_i_pad_y != i_pad_y) {
    
    packer->default_i_pad_x = i_pad_x;
    packer->default_i_pad_y = i_pad_y;
    redo_defaults_children (packer);
  }
}

static void 
gtk_packer_container_add (GtkContainer *packer, GtkWidget *child)
{
  gtk_packer_add_defaults(GTK_PACKER(packer), child,
			  GTK_SIDE_TOP, GTK_ANCHOR_CENTER, 0);
}

void 
gtk_packer_add_defaults (GtkPacker       *packer,
			 GtkWidget       *child,
                         GtkSideType      side,
                         GtkAnchorType    anchor,
                         GtkPackerOptions options)
{
  GtkPackerChild *pchild;
  
  g_return_if_fail (packer != NULL);
  g_return_if_fail (GTK_IS_PACKER (packer));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  
  pchild = (GtkPackerChild*) g_malloc(sizeof(GtkPackerChild));
  
  pchild->widget = child;
  pchild->side = side;
  pchild->options = options;
  pchild->anchor = anchor;
  
  pchild->use_default = 1;
  
  pchild->border_width = packer->default_border_width;
  pchild->pad_x = packer->default_pad_x;
  pchild->pad_y = packer->default_pad_y;
  pchild->i_pad_x = packer->default_i_pad_x;
  pchild->i_pad_y = packer->default_i_pad_y;
  
  packer->children = g_list_append(packer->children, (gpointer) pchild);
  
  gtk_widget_set_parent (child, GTK_WIDGET (packer));
  
  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (packer))) 
    {
      if (GTK_WIDGET_REALIZED (GTK_WIDGET (packer)) &&
          !GTK_WIDGET_REALIZED (child)) 
        gtk_widget_realize (child);
      
      if (GTK_WIDGET_MAPPED (GTK_WIDGET (packer)) &&
          !GTK_WIDGET_MAPPED (child)) 
        gtk_widget_map (child);
    }

  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (packer))
    gtk_widget_queue_resize (child);
  
}

void 
gtk_packer_add (GtkPacker       *packer,
                GtkWidget       *child,
                GtkSideType      side,
                GtkAnchorType    anchor,
                GtkPackerOptions options,
                guint            border_width,
                guint            pad_x,
                guint            pad_y,
                guint            i_pad_x,
                guint            i_pad_y)
{
  GtkPackerChild *pchild;
  
  g_return_if_fail (packer != NULL);
  g_return_if_fail (GTK_IS_PACKER (packer));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  
  pchild = (GtkPackerChild*) g_malloc(sizeof(GtkPackerChild));
  
  pchild->widget = child;
  pchild->side = side;
  pchild->options = options;
  pchild->anchor = anchor;
  
  pchild->use_default = 0;
  
  pchild->border_width = border_width;
  pchild->pad_x = pad_x;
  pchild->pad_y = pad_y;
  pchild->i_pad_x = i_pad_x;
  pchild->i_pad_y = i_pad_y;
  
  packer->children = g_list_append(packer->children, (gpointer) pchild);
  
  gtk_widget_set_parent (child, GTK_WIDGET (packer));
  
  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (packer))) 
    {
      if (GTK_WIDGET_REALIZED (GTK_WIDGET (packer)) &&
          !GTK_WIDGET_REALIZED (child)) 
        gtk_widget_realize (child);
      
      if (GTK_WIDGET_MAPPED (GTK_WIDGET (packer)) &&
          !GTK_WIDGET_MAPPED (child)) 
        gtk_widget_map (child);
    }
  
  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (packer))
    gtk_widget_queue_resize (child);
  
}

void
gtk_packer_configure (GtkPacker       *packer,
                      GtkWidget       *child,
                      GtkSideType      side,
                      GtkAnchorType    anchor,
                      GtkPackerOptions options,
                      guint            border_width,
                      guint            pad_x,
                      guint            pad_y,
                      guint            i_pad_x,
                      guint            i_pad_y)
{
  GList *list;
  GtkPackerChild *pchild;
  
  g_return_if_fail (packer != NULL);
  g_return_if_fail (GTK_IS_PACKER (packer));
  g_return_if_fail (child != NULL);
  
  list = g_list_first(packer->children);
  while (list != NULL) 
    {
      pchild = (GtkPackerChild*) list->data;
      if (pchild->widget == child) 
	{
	  pchild->side = side;
	  pchild->anchor = anchor;
	  pchild->options = options;
	  
	  pchild->use_default = 0;
	  
	  pchild->border_width = border_width;
	  pchild->pad_x = pad_x;
	  pchild->pad_y = pad_y;
	  pchild->i_pad_x = i_pad_x;
	  pchild->i_pad_y = i_pad_y;
	  
	  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (packer))
	    gtk_widget_queue_resize (child);
	  return;
        }
      list = g_list_next(list);
    }

  g_warning ("gtk_packer_configure(): couldn't find child `%s' amongst the packer's children", gtk_type_name (GTK_OBJECT_TYPE (child)));

}

static void 
gtk_packer_remove (GtkContainer *container, GtkWidget *widget) 
{
  GtkPacker *packer;
  GtkPackerChild *child;
  GList *children;
  gint visible;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (widget != NULL);
  
  packer = GTK_PACKER (container);
  
  children = g_list_first(packer->children);
  while (children) 
    {
      child = children->data;
      
      if (child->widget == widget) 
	{
	  visible = GTK_WIDGET_VISIBLE (widget);
	  gtk_widget_unparent (widget);
	  
	  packer->children = g_list_remove_link (packer->children, children);
	  g_list_free (children);
	  g_free (child);
	  
	  if (visible && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));
	  
	  break;
        }
      
      children = g_list_next(children);
    }
}

static void 
gtk_packer_map (GtkWidget *widget)
{
  GtkPacker *packer;
  GtkPackerChild *child;
  GList *children;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PACKER (widget));
  
  packer = GTK_PACKER (widget);
  GTK_WIDGET_SET_FLAGS (packer, GTK_MAPPED);
  
  children = g_list_first(packer->children);
  while (children != NULL) 
    {
      child = children->data;
      children = g_list_next(children);
      
      if (GTK_WIDGET_VISIBLE (child->widget) &&
	  !GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_map (child->widget);
    }
}

static void 
gtk_packer_unmap (GtkWidget *widget)
{
  GtkPacker *packer;
  GtkPackerChild *child;
  GList *children;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PACKER (widget));
  
  packer = GTK_PACKER (widget);
  GTK_WIDGET_UNSET_FLAGS (packer, GTK_MAPPED);
  
  children = g_list_first(packer->children);
  while (children) 
    {
      child = children->data;
      children = g_list_next(children);
      
      if (GTK_WIDGET_VISIBLE (child->widget) &&
	  GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_unmap (child->widget);
    }
}

static void 
gtk_packer_draw (GtkWidget *widget, GdkRectangle     *area)
{
  GtkPacker *packer;
  GtkPackerChild *child;
  GdkRectangle child_area;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PACKER (widget));
 
  if (GTK_WIDGET_DRAWABLE (widget)) 
    {
      packer = GTK_PACKER (widget);
      
      children = g_list_first(packer->children);
      while (children != NULL) 
	{
	  child = children->data;
	  children = g_list_next(children);
	  
	  if (gtk_widget_intersect (child->widget, area, &child_area))
	    gtk_widget_draw (child->widget, &child_area);
        }
    }
  
}

static gint 
gtk_packer_expose (GtkWidget *widget, GdkEventExpose *event)
{
  GtkPacker *packer;
  GtkPackerChild *child;
  GdkEventExpose child_event;
  GList *children;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PACKER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (GTK_WIDGET_DRAWABLE (widget)) 
    {
      packer = GTK_PACKER (widget);
      
      child_event = *event;
      
      children = g_list_first(packer->children);
      while (children) 
	{
	  child = children->data;
	  children = g_list_next(children);
	  
	  if (GTK_WIDGET_NO_WINDOW (child->widget) &&
	      gtk_widget_intersect (child->widget, &event->area, &child_event.area))
	    gtk_widget_event (child->widget, (GdkEvent*) &child_event);
	}
    }   
  
  return FALSE;
}

static void 
gtk_packer_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  GtkPacker *packer;
  GtkPackerChild *child;
  GList *children;
  gint nvis_vert_children;
  gint nvis_horz_children;
  gint width, height;
  gint maxWidth, maxHeight;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PACKER (widget));
  g_return_if_fail (requisition != NULL);
  
  packer = GTK_PACKER (widget);
  requisition->width = 0;
  requisition->height = 0;
  nvis_vert_children = 0;
  nvis_horz_children = 0;
  
  width = height = maxWidth = maxHeight = 0;
  
  children = g_list_first(packer->children);
  while (children != NULL) 
    {
      child = children->data;
      
      if (GTK_WIDGET_VISIBLE (child->widget)) 
	{
	  gtk_widget_size_request (child->widget, &child->widget->requisition);
	  
	  if((child->side == GTK_SIDE_TOP) || (child->side == GTK_SIDE_BOTTOM))
	    {
	      maxWidth = MAX (maxWidth,
			      (child->widget->requisition.width +
			       2 * child->border_width +
			       child->pad_x + child->i_pad_x +
			       width));
	      height += (child->widget->requisition.height +
			 2 * child->border_width +
			 child->pad_y + child->i_pad_y);
            } 
	  else 
	    {
	      maxHeight = MAX (maxHeight,
			       (child->widget->requisition.height +
				2 * child->border_width +
				child->pad_y + child->i_pad_y +
				height));
	      width += (child->widget->requisition.width +
			2 * child->border_width +
			child->pad_x + child->i_pad_x);
            }
        }

      children = g_list_next(children);
    }
  requisition->width = MAX (maxWidth, width);
  requisition->height = MAX (maxHeight, height);
}

static gint
YExpansion (GList *children, gint cavityHeight)
{
  GList *list;
  GtkPackerChild *child;
  GtkWidget *widget;
  gint numExpand, minExpand, curExpand;
  gint childHeight;
  
  minExpand = cavityHeight;
  numExpand = 0;
  
  list = children;
  while (list != NULL) 
    {
      child = list->data;
      widget = child->widget;
      childHeight = (widget->requisition.height +
		     2 * child->border_width +
		     child->i_pad_y +
		     child->pad_y);
      if ((child->side == GTK_SIDE_LEFT) || (child->side == GTK_SIDE_RIGHT)) 
	{
	  curExpand = (cavityHeight - childHeight)/numExpand;
	  minExpand = MIN(minExpand, curExpand);
        } 
      else 
	{
	  cavityHeight -= childHeight;
	  if (child->options & GTK_PACK_EXPAND)
	    numExpand++;
        }
      list = g_list_next(list);
    } 
  curExpand = cavityHeight/numExpand; 
  if (curExpand < minExpand)
    minExpand = curExpand;
  return (minExpand < 0) ? 0 : minExpand;
}

static gint
XExpansion (GList *children, gint cavityWidth)
{
  GList *list;
  GtkPackerChild *child;
  GtkWidget *widget;
  gint numExpand, minExpand, curExpand;
  gint childWidth;
  
  minExpand = cavityWidth;
  numExpand = 0;
  
  list = children;
  while (list != NULL) 
    {
      child = list->data;
      widget = child->widget;
      childWidth = (widget->requisition.width +
		    2 * child->border_width +
		    child->i_pad_x +
		    child->pad_x);

      if ((child->side == GTK_SIDE_TOP) || (child->side == GTK_SIDE_BOTTOM)) 
	{
	  curExpand = (cavityWidth - childWidth)/numExpand;
	  minExpand = MIN(minExpand, curExpand); 
        } 
      else 
	{
	  cavityWidth -= childWidth;
	  if (child->options & GTK_PACK_EXPAND)
	    numExpand++;
        }
      list = g_list_next(list);
    } 
  curExpand = cavityWidth/numExpand;
  if (curExpand < minExpand)
    minExpand = curExpand;
  return (minExpand < 0) ? 0 : minExpand; 
};

static void 
gtk_packer_size_allocate (GtkWidget *widget, GtkAllocation  *allocation)
{
  GtkPacker *packer;
  GtkAllocation child_allocation;
  GList *list;
  GtkPackerChild *child;
  gint cavityX, cavityY;
  gint cavityWidth, cavityHeight;
  gint width, height, x, y;
  gint frameHeight, frameWidth, frameX, frameY;
  gint borderX, borderY;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PACKER (widget));
  g_return_if_fail (allocation != NULL);

  x = y = 0;

  widget->allocation = *allocation;
  packer = GTK_PACKER(widget);
  
  cavityX = widget->allocation.x;
  cavityY = widget->allocation.y;
  cavityWidth = widget->allocation.width;
  cavityHeight = widget->allocation.height;
  list = g_list_first(packer->children);
  while (list != NULL)
    {
      child = list->data;
      
      if ((child->side == GTK_SIDE_TOP) || (child->side == GTK_SIDE_BOTTOM)) 
	{
	  frameWidth = cavityWidth;
	  frameHeight = (child->widget->requisition.height +
			 2 * child->border_width +
			 child->pad_y +
			 child->i_pad_y);
	  if (child->options & GTK_PACK_EXPAND)
	    frameHeight += YExpansion(list, cavityHeight);
	  cavityHeight -= frameHeight;
	  if (cavityHeight < 0) 
	    {
	      frameHeight += cavityHeight;
	      cavityHeight = 0;
            }
	  frameX = cavityX;
	  if (child->side == GTK_SIDE_TOP) 
	    {
	      frameY = cavityY;
	      cavityY += frameHeight;
            } 
	  else 
	    {
	      frameY = cavityY + cavityHeight;
            }
        } 
      else 
	{
	  frameHeight = cavityHeight;
	  frameWidth = (child->widget->requisition.width +
			2 * child->border_width +
			child->pad_x +
			child->i_pad_x);
	  if (child->options & GTK_PACK_EXPAND)
	    frameWidth += XExpansion(list, cavityWidth);
	  cavityWidth -= frameWidth;
	  if (cavityWidth < 0) {
	    frameWidth += cavityWidth;
	    cavityWidth = 0;
	  }
	  frameY = cavityY;
	  if (child->side == GTK_SIDE_LEFT) 
	    {
	      frameX = cavityX;
	      cavityX += frameWidth;
	    } 
	  else 
	    {
	      frameX = cavityX + cavityWidth;
            }
        }
      
      borderX = child->pad_x + 2 * child->border_width;
      borderY = child->pad_y + 2 * child->border_width;
      
      width = (child->widget->requisition.width +
	       2 * child->border_width +
	       child->i_pad_x);
      if ((child->options & GTK_FILL_X) || (width > (frameWidth - borderX)))
	width = frameWidth - borderX;

      height = (child->widget->requisition.height +
		2 * child->border_width +
		child->i_pad_y);
      if ((child->options & GTK_FILL_Y) || (height > (frameHeight - borderY)))
	height = frameHeight - borderY;
      
      borderX /= 2;
      borderY /= 2;
      switch (child->anchor) 
	{
	case GTK_ANCHOR_N:
	  x = frameX + (frameWidth - width)/2;
	  y = frameY + borderY;
	  break;
	case GTK_ANCHOR_NE:
	  x = frameX + frameWidth - width - borderX;
	  y = frameY + borderY;
	  break;
	case GTK_ANCHOR_E:
	  x = frameX + frameWidth - width - borderX;
	  y = frameY + (frameHeight - height)/2;
	  break;
	case GTK_ANCHOR_SE:
	  x = frameX + frameWidth - width - borderX;
	  y = frameY + frameHeight - height - borderY;
	  break;
	case GTK_ANCHOR_S:
	  x = frameX + (frameWidth - width)/2;
	  y = frameY + frameHeight - height - borderY;
	  break;
	case GTK_ANCHOR_SW:
	  x = frameX + borderX;
	  y = frameY + frameHeight - height - borderY;
	  break;
	case GTK_ANCHOR_W:
	  x = frameX + borderX;
	  y = frameY + (frameHeight - height)/2;
	  break;
	case GTK_ANCHOR_NW:
	  x = frameX + borderX;
	  y = frameY + borderY;
	  break;
	case GTK_ANCHOR_CENTER:
	  x = frameX + (frameWidth - width)/2;
	  y = frameY + (frameHeight - height)/2;
	  break;
	default:
	  g_warning ("gtk_packer_size_allocate(): bad anchor type: %d", child->anchor);
        } 
 
        if (width <= 0 || height <= 0) 
	  {
            gtk_widget_unmap(child->widget);
	  } 
	else 
	  {
	    child_allocation.x = x;
	    child_allocation.y = y;
	    child_allocation.width = width;
	    child_allocation.height = height;
	    gtk_widget_size_allocate (child->widget, &child_allocation);
	    
	    if (GTK_WIDGET_MAPPED (widget) &&
		!(GTK_WIDGET_MAPPED (child->widget)))
	      gtk_widget_map(child->widget); 
	  }
	
        list = g_list_next(list);
    }
}

static void
gtk_packer_foreach (GtkContainer *container,
                    GtkCallback   callback,
                    gpointer      callback_data)
{
  GtkPacker *packer;
  GtkPackerChild *child;
  GList *children;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_PACKER (container));
  g_return_if_fail (callback != NULL);
  
  packer = GTK_PACKER (container);
  
  children = g_list_first(packer->children);
  while (children != NULL) 
    {
      child = children->data;
      children = g_list_next(children);
      
      (* callback) (child->widget, callback_data);
    }
}

