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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "gtknotebook.h"


#define CHILD_SPACING  2
#define TAB_OVERLAP    2
#define TAB_CURVATURE  1


static void gtk_notebook_class_init     (GtkNotebookClass *klass);
static void gtk_notebook_init           (GtkNotebook      *notebook);
static void gtk_notebook_destroy        (GtkObject        *object);
static void gtk_notebook_map            (GtkWidget        *widget);
static void gtk_notebook_unmap          (GtkWidget        *widget);
static void gtk_notebook_realize        (GtkWidget        *widget);
static void gtk_notebook_unrealize      (GtkWidget        *widget);
static void gtk_notebook_size_request   (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void gtk_notebook_size_allocate  (GtkWidget        *widget,
					 GtkAllocation    *allocation);
static void gtk_notebook_paint          (GtkWidget        *widget,
					 GdkRectangle     *area);
static void gtk_notebook_draw           (GtkWidget        *widget,
					 GdkRectangle     *area);
static gint gtk_notebook_expose         (GtkWidget        *widget,
					 GdkEventExpose   *event);
static gint gtk_notebook_button_press   (GtkWidget        *widget,
					 GdkEventButton   *event);
static void gtk_notebook_add            (GtkContainer     *container,
					 GtkWidget        *widget);
static void gtk_notebook_remove         (GtkContainer     *container,
					 GtkWidget        *widget);
static void gtk_notebook_foreach        (GtkContainer     *container,
					 GtkCallback       callback,
					 gpointer          callback_data);
static void gtk_notebook_switch_page    (GtkNotebook      *notebook,
					 GtkNotebookPage  *page);
static void gtk_notebook_set_clip_rect  (GtkNotebook      *notebook,
				 	 GtkStateType      state_type,
					 GdkRectangle     *area);
static void gtk_notebook_draw_tab       (GtkNotebook      *notebook,
					 GtkNotebookPage  *page,
					 GdkRectangle     *area);
static void gtk_notebook_pages_allocate (GtkNotebook      *notebook,
					 GtkAllocation    *allocation);
static void gtk_notebook_page_allocate  (GtkNotebook      *notebook,
					 GtkNotebookPage  *page,
					 GtkAllocation    *allocation);


static GtkContainerClass *parent_class = NULL;


guint
gtk_notebook_get_type ()
{
  static guint notebook_type = 0;

  if (!notebook_type)
    {
      GtkTypeInfo notebook_info =
      {
	"GtkNotebook",
	sizeof (GtkNotebook),
	sizeof (GtkNotebookClass),
	(GtkClassInitFunc) gtk_notebook_class_init,
	(GtkObjectInitFunc) gtk_notebook_init,
	(GtkArgFunc) NULL,
      };

      notebook_type = gtk_type_unique (gtk_container_get_type (), &notebook_info);
    }

  return notebook_type;
}

static void
gtk_notebook_class_init (GtkNotebookClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  object_class->destroy = gtk_notebook_destroy;

  widget_class->map = gtk_notebook_map;
  widget_class->unmap = gtk_notebook_unmap;
  widget_class->realize = gtk_notebook_realize;
  widget_class->unrealize = gtk_notebook_unrealize;
  widget_class->size_request = gtk_notebook_size_request;
  widget_class->size_allocate = gtk_notebook_size_allocate;
  widget_class->draw = gtk_notebook_draw;
  widget_class->expose_event = gtk_notebook_expose;
  widget_class->button_press_event = gtk_notebook_button_press;

  container_class->add = gtk_notebook_add;
  container_class->remove = gtk_notebook_remove;
  container_class->foreach = gtk_notebook_foreach;
}

static void
gtk_notebook_init (GtkNotebook *notebook)
{
  notebook->cur_page = NULL;
  notebook->children = NULL;
  notebook->show_tabs = TRUE;
  notebook->show_border = TRUE;
  notebook->tab_pos = GTK_POS_TOP;
}

GtkWidget*
gtk_notebook_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_notebook_get_type ()));
}

void
gtk_notebook_append_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);
  g_return_if_fail (tab_label != NULL);

  gtk_notebook_insert_page (notebook, child, tab_label, -1);
}

void
gtk_notebook_prepend_page (GtkNotebook *notebook,
			   GtkWidget   *child,
			   GtkWidget   *tab_label)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);
  g_return_if_fail (tab_label != NULL);

  gtk_notebook_insert_page (notebook, child, tab_label, 0);
}

void
gtk_notebook_insert_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label,
			  gint         position)
{
  GtkNotebookPage *page;
  gint nchildren;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);
  g_return_if_fail (tab_label != NULL);

  page = g_new (GtkNotebookPage, 1);
  page->child = child;
  page->tab_label = tab_label;
  page->requisition.width = 0;
  page->requisition.height = 0;
  page->allocation.x = 0;
  page->allocation.y = 0;
  page->allocation.width = 0;
  page->allocation.height = 0;

  nchildren = g_list_length (notebook->children);
  if ((position < 0) || (position > nchildren))
    position = nchildren;

  notebook->children = g_list_insert (notebook->children, page, position);

  if (!notebook->cur_page)
    notebook->cur_page = page;

  gtk_widget_show (tab_label);
  gtk_widget_set_parent (child, GTK_WIDGET (notebook));
  gtk_widget_set_parent (tab_label, GTK_WIDGET (notebook));

  if (GTK_WIDGET_VISIBLE (notebook))
    {
      if (GTK_WIDGET_REALIZED (notebook) &&
	  !GTK_WIDGET_REALIZED (child))
	gtk_widget_realize (child);
	

      if (GTK_WIDGET_MAPPED (notebook) &&
	  !GTK_WIDGET_MAPPED (child) && notebook->cur_page == page)
	gtk_widget_map (child);

      if (GTK_WIDGET_REALIZED (notebook) &&
	  !GTK_WIDGET_REALIZED (tab_label))
	gtk_widget_realize (tab_label);
      
      if (GTK_WIDGET_MAPPED (notebook) &&
	  !GTK_WIDGET_MAPPED (tab_label))
	gtk_widget_map (tab_label);
    }

  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (notebook))
    gtk_widget_queue_resize (child);
}

void
gtk_notebook_remove_page (GtkNotebook *notebook,
			  gint         page_num)
{
  GtkNotebookPage *page;
  GList *tmp_list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  tmp_list = g_list_nth (notebook->children, page_num);
  if (tmp_list)
    {
      page = tmp_list->data;

      if (notebook->cur_page == page)
	gtk_notebook_prev_page (notebook);
      if (notebook->cur_page == page)
	notebook->cur_page = NULL;

      notebook->children = g_list_remove_link (notebook->children, tmp_list);
      g_list_free (tmp_list);
      g_free (page);
    }
}

gint
gtk_notebook_current_page (GtkNotebook *notebook)
{
  GList *children;
  gint cur_page;

  g_return_val_if_fail (notebook != NULL, -1);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  if (notebook->cur_page)
    {
      cur_page = 0;
      children = notebook->children;

      while (children)
	{
	  if (children->data == notebook->cur_page)
	    break;
	  children = children->next;
	  cur_page += 1;
	}

      if (!children)
	cur_page = -1;
    }
  else
    {
      cur_page = -1;
    }

  return cur_page;
}

void
gtk_notebook_set_page (GtkNotebook *notebook,
		       gint         page_num)
{
  GtkNotebookPage *page;
  GList *tmp_list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  tmp_list = g_list_nth (notebook->children, page_num);
  if (tmp_list)
    {
      page = tmp_list->data;
      gtk_notebook_switch_page (notebook, page);
    }
}

void
gtk_notebook_next_page (GtkNotebook *notebook)
{
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  children = notebook->children;
  while (children)
    {
      page = children->data;

      if (notebook->cur_page == page)
	{
	  children = children->next;
	  if (!children)
	    children = notebook->children;
	  page = children->data;

	  gtk_notebook_switch_page (notebook, page);
	}

      children = children->next;
    }
}

void
gtk_notebook_prev_page (GtkNotebook *notebook)
{
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  children = notebook->children;
  while (children)
    {
      page = children->data;

      if (notebook->cur_page == page)
	{
	  children = children->prev;
	  if (!children)
	    children = g_list_last (notebook->children);
	  page = children->data;

	  gtk_notebook_switch_page (notebook, page);
	}

      children = children->next;
    }
}

void
gtk_notebook_set_tab_pos (GtkNotebook     *notebook,
			  GtkPositionType  pos)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->tab_pos != pos)
    {
      notebook->tab_pos = pos;

      if (GTK_WIDGET_VISIBLE (notebook))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }
}

void
gtk_notebook_set_show_tabs (GtkNotebook *notebook,
			    gint         show_tabs)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->show_tabs != show_tabs)
    {
      notebook->show_tabs = show_tabs;

      if (GTK_WIDGET_VISIBLE (notebook))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }
}

void
gtk_notebook_set_show_border (GtkNotebook *notebook,
			      gint         show_border)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->show_border != show_border)
    {
      notebook->show_border = show_border;

      if (GTK_WIDGET_VISIBLE (notebook))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }
}

static void
gtk_notebook_destroy (GtkObject *object)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (object));

  notebook = GTK_NOTEBOOK (object);

  children = notebook->children;
  while (children)
    {
      page = children->data;
      children = children->next;

      page->child->parent = NULL;
      page->tab_label->parent = NULL;

      gtk_object_unref (GTK_OBJECT (page->child));
      gtk_object_unref (GTK_OBJECT (page->tab_label));

      gtk_widget_destroy (page->child);
      gtk_widget_destroy (page->tab_label);

      g_free (page);
    }

  g_list_free (notebook->children);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_notebook_map (GtkWidget *widget)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  gdk_window_show (widget->window);

  notebook = GTK_NOTEBOOK (widget);

  if (notebook->cur_page &&
      GTK_WIDGET_VISIBLE (notebook->cur_page->child) &&
      !GTK_WIDGET_MAPPED (notebook->cur_page->child))
    gtk_widget_map (notebook->cur_page->child);

  children = notebook->children;
  while (children)
    {
      page = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (page->child) &&
	  !GTK_WIDGET_MAPPED (page->tab_label))
	gtk_widget_map (page->tab_label);
    }
}

static void
gtk_notebook_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  gdk_window_hide (widget->window);
}

static void
gtk_notebook_realize (GtkWidget *widget)
{
  GtkNotebook *notebook;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  notebook = GTK_NOTEBOOK (widget);
  GTK_WIDGET_SET_FLAGS (notebook, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, notebook);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_notebook_unrealize (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED | GTK_MAPPED);

  gtk_style_detach (widget->style);
  gdk_window_destroy (widget->window);
  widget->window = NULL;
}

static void
gtk_notebook_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  gint tab_width;
  gint tab_height;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (requisition != NULL);

  notebook = GTK_NOTEBOOK (widget);
  widget->requisition.width = 0;
  widget->requisition.height = 0;

  children = notebook->children;
  while (children)
    {
      page = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (page->child))
	{
	  gtk_widget_size_request (page->child, &page->child->requisition);
	  
	  widget->requisition.width = MAX (widget->requisition.width,
					   page->child->requisition.width);
	  widget->requisition.height = MAX (widget->requisition.height,
					    page->child->requisition.height);
	}
    }

  widget->requisition.width += GTK_CONTAINER (widget)->border_width * 2;
  widget->requisition.height += GTK_CONTAINER (widget)->border_width * 2;

  if (notebook->show_tabs)
    {
      widget->requisition.width += GTK_WIDGET (widget)->style->klass->xthickness * 2;
      widget->requisition.height += GTK_WIDGET (widget)->style->klass->ythickness * 2;

      tab_width = 0;
      tab_height = 0;

      children = notebook->children;
      while (children)
	{
	  page = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (page->child))
	    {
	      gtk_widget_size_request (page->tab_label, &page->tab_label->requisition);

	      page->requisition.width = (page->tab_label->requisition.width +
					 (GTK_WIDGET (widget)->style->klass->xthickness +
					  CHILD_SPACING) * 2);
	      page->requisition.height = (page->tab_label->requisition.height +
					  (GTK_WIDGET (widget)->style->klass->ythickness +
					   CHILD_SPACING) * 2);

	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		  page->requisition.width -= TAB_OVERLAP;
		  page->requisition.height -= GTK_WIDGET (widget)->style->klass->ythickness;

		  tab_width += page->requisition.width;
		  tab_height = MAX (tab_height, page->requisition.height);
		  break;
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		  page->requisition.width -= GTK_WIDGET (widget)->style->klass->xthickness;
		  page->requisition.height -= TAB_OVERLAP;

		  tab_width = MAX (tab_width, page->requisition.width);
		  tab_height += page->requisition.height;
		  break;
		}
	    }
	}

      children = notebook->children;
      while (children)
	{
	  page = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (page->child))
	    {
	      if ((notebook->tab_pos == GTK_POS_TOP) ||
		  (notebook->tab_pos == GTK_POS_BOTTOM))
		page->requisition.height = tab_height;
	      else
		page->requisition.width = tab_width;
	    }
	}

      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  tab_width += GTK_WIDGET (widget)->style->klass->xthickness;
	  widget->requisition.width = MAX (widget->requisition.width, tab_width);
	  widget->requisition.height += tab_height;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  tab_height += GTK_WIDGET (widget)->style->klass->ythickness;
	  widget->requisition.width += tab_width;
	  widget->requisition.height = MAX (widget->requisition.height, tab_height);
	  break;
	}
    }
  else if (notebook->show_border)
    {
      widget->requisition.width += GTK_WIDGET (widget)->style->klass->xthickness * 2;
      widget->requisition.height += GTK_WIDGET (widget)->style->klass->ythickness * 2;
    }
}

static void
gtk_notebook_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GtkAllocation child_allocation;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  notebook = GTK_NOTEBOOK (widget);
  if (notebook->children)
    {
      child_allocation.x = GTK_CONTAINER (widget)->border_width;
      child_allocation.y = GTK_CONTAINER (widget)->border_width;
      child_allocation.width = allocation->width - child_allocation.x * 2;
      child_allocation.height = allocation->height - child_allocation.y * 2;

      if (notebook->show_tabs || notebook->show_border)
	{
	  child_allocation.x += GTK_WIDGET (widget)->style->klass->xthickness;
	  child_allocation.y += GTK_WIDGET (widget)->style->klass->ythickness;
	  child_allocation.width -= GTK_WIDGET (widget)->style->klass->xthickness * 2;
	  child_allocation.height -= GTK_WIDGET (widget)->style->klass->ythickness * 2;

	  if (notebook->show_tabs && notebook->children)
	    {
	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		  child_allocation.y += notebook->cur_page->requisition.height;
		case GTK_POS_BOTTOM:
		  child_allocation.height -= notebook->cur_page->requisition.height;
		  break;
		case GTK_POS_LEFT:
		  child_allocation.x += notebook->cur_page->requisition.width;
		case GTK_POS_RIGHT:
		  child_allocation.width -= notebook->cur_page->requisition.width;
		  break;
		}
	    }
	}

      children = notebook->children;
      while (children)
	{
	  page = children->data;
	  children = children->next;
	  
	  if (GTK_WIDGET_VISIBLE (page->child))
	    gtk_widget_size_allocate (page->child, &child_allocation);
	}

      if (notebook->show_tabs && notebook->children)
	gtk_notebook_pages_allocate (notebook, allocation);
    }
}

static void
gtk_notebook_paint (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  GdkPoint points[6];
  gint width, height;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      notebook = GTK_NOTEBOOK (widget);

      /* Set the clip rectangle here, so we don't overwrite things
       * outside of exposed area when drawing shadows */
      gtk_notebook_set_clip_rect (notebook, GTK_STATE_ACTIVE, area);
      gtk_notebook_set_clip_rect (notebook, GTK_STATE_NORMAL, area);

      gdk_window_clear_area (widget->window,
			     area->x, area->y,
			     area->width, area->height);

      if (notebook->show_tabs || notebook->show_border)
	{
	  x = GTK_CONTAINER (widget)->border_width;
	  y = GTK_CONTAINER (widget)->border_width;
	  width = widget->allocation.width - x * 2;
	  height = widget->allocation.height - y * 2;

	  if (notebook->show_tabs && notebook->children)
	    {
	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		  y += notebook->cur_page->allocation.height;
		case GTK_POS_BOTTOM:
		  height -= notebook->cur_page->allocation.height;
		  break;
		case GTK_POS_LEFT:
		  x += notebook->cur_page->allocation.width;
		case GTK_POS_RIGHT:
		  width -= notebook->cur_page->allocation.width;
		  break;
		}

	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		  points[0].x = notebook->cur_page->allocation.x;
		  points[0].y = y;
		  points[1].x = x;
		  points[1].y = y;
		  points[2].x = x;
		  points[2].y = y + height - 1;
		  points[3].x = x + width - 1;
		  points[3].y = y + height - 1;
		  points[4].x = x + width - 1;
		  points[4].y = y;
		  points[5].x = (notebook->cur_page->allocation.x +
				 notebook->cur_page->allocation.width -
				 GTK_WIDGET (notebook)->style->klass->xthickness);
		  points[5].y = y;

		  if (points[5].x == (x + width))
		    points[5].x -= 1;
		  break;
		case GTK_POS_BOTTOM:
		  points[0].x = (notebook->cur_page->allocation.x +
				 notebook->cur_page->allocation.width -
				 GTK_WIDGET (notebook)->style->klass->xthickness);
		  points[0].y = y + height - 1;
		  points[1].x = x + width - 1;
		  points[1].y = y + height - 1;
		  points[2].x = x + width - 1;
		  points[2].y = y;
		  points[3].x = x;
		  points[3].y = y;
		  points[4].x = x;
		  points[4].y = y + height - 1;
		  points[5].x = notebook->cur_page->allocation.x;
		  points[5].y = y + height - 1;

		  if (points[0].x == (x + width))
		    points[0].x -= 1;
		  break;
		case GTK_POS_LEFT:
		  points[0].x = x;
		  points[0].y = (notebook->cur_page->allocation.y +
				 notebook->cur_page->allocation.height -
				 GTK_WIDGET (notebook)->style->klass->ythickness);
		  points[1].x = x;
		  points[1].y = y + height - 1;
		  points[2].x = x + width - 1;
		  points[2].y = y + height - 1;
		  points[3].x = x + width - 1;
		  points[3].y = y;
		  points[4].x = x;
		  points[4].y = y;
		  points[5].x = x;
		  points[5].y = notebook->cur_page->allocation.y;

		  if (points[0].y == (y + height))
		    points[0].y -= 1;
		  break;
		case GTK_POS_RIGHT:
		  points[0].x = x + width - 1;
		  points[0].y = notebook->cur_page->allocation.y;
		  points[1].x = x + width - 1;
		  points[1].y = y;
		  points[2].x = x;
		  points[2].y = y;
		  points[3].x = x;
		  points[3].y = y + height - 1;
		  points[4].x = x + width - 1;
		  points[4].y = y + height - 1;
		  points[5].x = x + width - 1;
		  points[5].y = (notebook->cur_page->allocation.y +
				 notebook->cur_page->allocation.height -
				 GTK_WIDGET (notebook)->style->klass->ythickness);

		  if (points[5].y == (y + height))
		    points[5].y -= 1;
		  break;
		}

	      gtk_draw_polygon (widget->style, widget->window,
				GTK_STATE_NORMAL, GTK_SHADOW_OUT,
				points, 6, FALSE);

	      children = g_list_last (notebook->children);
	      while (children)
		{
		  page = children->data;
		  children = children->prev;

		  if (notebook->cur_page != page)
		    gtk_notebook_draw_tab (notebook, page, area);
		}

	      if (notebook->cur_page)
		gtk_notebook_draw_tab (notebook, notebook->cur_page, area);
	    }
	  else if (notebook->show_border)
	    {
	      gtk_draw_shadow (widget->style, widget->window,
			       GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			       x, y, width, height);
	    }
	}

      gtk_notebook_set_clip_rect (notebook, GTK_STATE_ACTIVE, NULL);
      gtk_notebook_set_clip_rect (notebook, GTK_STATE_NORMAL, NULL);
    }
}

static void
gtk_notebook_draw (GtkWidget    *widget,
		   GdkRectangle *area)
{
  GtkNotebook *notebook;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      notebook = GTK_NOTEBOOK (widget);

      gtk_notebook_paint (widget, area);

      if (notebook->cur_page &&
	  gtk_widget_intersect (notebook->cur_page->child, area, &child_area))
	gtk_widget_draw (notebook->cur_page->child, &child_area);
    }
}

static gint
gtk_notebook_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  GtkNotebook *notebook;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      notebook = GTK_NOTEBOOK (widget);

      gtk_notebook_paint (widget, &event->area);

      child_event = *event;
      if (notebook->cur_page && GTK_WIDGET_NO_WINDOW (notebook->cur_page->child) &&
	  gtk_widget_intersect (notebook->cur_page->child, &event->area, &child_event.area))
	gtk_widget_event (notebook->cur_page->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

static gint
gtk_notebook_button_press (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if ((event->type != GDK_BUTTON_PRESS) ||
      (event->window != widget->window))
    return FALSE;

  notebook = GTK_NOTEBOOK (widget);

  children = notebook->children;
  while (children)
    {
      page = children->data;

      if (GTK_WIDGET_VISIBLE (page->child) &&
	  (event->x >= page->allocation.x) &&
	  (event->y >= page->allocation.y) &&
	  (event->x <= (page->allocation.x + page->allocation.width)) &&
	  (event->y <= (page->allocation.y + page->allocation.height)))
	{
	  gtk_notebook_switch_page (notebook, page);
	  break;
	}
      children = children->next;
    }

  return FALSE;
}

static void
gtk_notebook_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  g_warning ("gtk_notebook_add: use gtk_notebook_{append,prepend}_page instead\n");
}

static void
gtk_notebook_remove (GtkContainer *container,
		     GtkWidget    *widget)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (container));
  g_return_if_fail (widget != NULL);

  notebook = GTK_NOTEBOOK (container);

  children = notebook->children;
  while (children)
    {
      page = children->data;

      if (page->child == widget)
	{
	  gtk_widget_unparent (page->child);
	  gtk_widget_unparent (page->tab_label);

	  notebook->children = g_list_remove_link (notebook->children, children);
	  g_list_free (children);
	  g_free (page);

	  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));

	  break;
	}

      children = children->next;
    }
}

static void
gtk_notebook_foreach (GtkContainer *container,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (container));
  g_return_if_fail (callback != NULL);

  notebook = GTK_NOTEBOOK (container);

  children = notebook->children;
  while (children)
    {
      page = children->data;
      children = children->next;

      (* callback) (page->child, callback_data);
    }
}

static void
gtk_notebook_switch_page (GtkNotebook     *notebook,
			  GtkNotebookPage *page)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (page != NULL);

  if (notebook->cur_page != page)
    {
      if (notebook->cur_page && GTK_WIDGET_MAPPED (notebook->cur_page->child))
	gtk_widget_unmap (notebook->cur_page->child);

      notebook->cur_page = page;
      gtk_notebook_pages_allocate (notebook, &GTK_WIDGET (notebook)->allocation);

      if (GTK_WIDGET_MAPPED (notebook))
	{
	  if (GTK_WIDGET_REALIZED (notebook->cur_page->child))
	    {
	      gtk_widget_map (notebook->cur_page->child);
	    }
	  else
	    {
	      gtk_widget_map (notebook->cur_page->child);
	      gtk_widget_size_allocate (GTK_WIDGET (notebook), 
					&GTK_WIDGET (notebook)->allocation);
	    }
	}

      if (GTK_WIDGET_DRAWABLE (notebook))
	gtk_widget_queue_draw (GTK_WIDGET (notebook));
    }
}

static void
gtk_notebook_set_clip_rect (GtkNotebook  *notebook,
			    GtkStateType  state_type,
			    GdkRectangle *area)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  
  gdk_gc_set_clip_rectangle (widget->style->bg_gc[state_type],    area);
  gdk_gc_set_clip_rectangle (widget->style->light_gc[state_type], area);
  gdk_gc_set_clip_rectangle (widget->style->dark_gc[state_type],  area);
  gdk_gc_set_clip_rectangle (widget->style->black_gc,             area);
}

static void
gtk_notebook_draw_tab (GtkNotebook     *notebook,
		       GtkNotebookPage *page,
		       GdkRectangle    *area)
{
  GdkRectangle child_area;
  GdkRectangle page_area;
  GtkStateType state_type;
  GdkPoint points[6];

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (page != NULL);
  g_return_if_fail (area != NULL);

  page_area.x = page->allocation.x;
  page_area.y = page->allocation.y;
  page_area.width = page->allocation.width;
  page_area.height = page->allocation.height;

  if (gdk_rectangle_intersect (&page_area, area, &child_area))
    {
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  points[0].x = page->allocation.x + page->allocation.width - 1;
	  points[0].y = page->allocation.y + page->allocation.height - 1;

	  points[1].x = page->allocation.x + page->allocation.width - 1;
	  points[1].y = page->allocation.y + TAB_CURVATURE;

	  points[2].x = page->allocation.x + page->allocation.width - TAB_CURVATURE - 1;
	  points[2].y = page->allocation.y;

	  points[3].x = page->allocation.x + TAB_CURVATURE;
	  points[3].y = page->allocation.y;

	  points[4].x = page->allocation.x;
	  points[4].y = page->allocation.y + TAB_CURVATURE;

	  points[5].x = page->allocation.x;
	  points[5].y = page->allocation.y + page->allocation.height - 1;
	  break;
	case GTK_POS_BOTTOM:
	  points[0].x = page->allocation.x;
	  points[0].y = page->allocation.y;

	  points[1].x = page->allocation.x;
	  points[1].y = page->allocation.y + page->allocation.height - TAB_CURVATURE - 1;

	  points[2].x = page->allocation.x + TAB_CURVATURE;
	  points[2].y = page->allocation.y + page->allocation.height - 1;

	  points[3].x = page->allocation.x + page->allocation.width - TAB_CURVATURE - 1;
	  points[3].y = page->allocation.y + page->allocation.height - 1;

	  points[4].x = page->allocation.x + page->allocation.width - 1;
	  points[4].y = page->allocation.y + page->allocation.height - TAB_CURVATURE - 1;

	  points[5].x = page->allocation.x + page->allocation.width - 1;
	  points[5].y = page->allocation.y;
	  break;
	case GTK_POS_LEFT:
	  points[0].x = page->allocation.x + page->allocation.width - 1;
	  points[0].y = page->allocation.y;

	  points[1].x = page->allocation.x + TAB_CURVATURE;
	  points[1].y = page->allocation.y;

	  points[2].x = page->allocation.x;
	  points[2].y = page->allocation.y + TAB_CURVATURE;

	  points[3].x = page->allocation.x;
	  points[3].y = page->allocation.y + page->allocation.height - TAB_CURVATURE - 1;

	  points[4].x = page->allocation.x + TAB_CURVATURE;
	  points[4].y = page->allocation.y + page->allocation.height - 1;

	  points[5].x = page->allocation.x + page->allocation.width - 1;
	  points[5].y = page->allocation.y + page->allocation.height - 1;
	  break;
	case GTK_POS_RIGHT:
	  points[0].x = page->allocation.x;
	  points[0].y = page->allocation.y + page->allocation.height - 1;

	  points[1].x = page->allocation.x + page->allocation.width - TAB_CURVATURE - 1;
	  points[1].y = page->allocation.y + page->allocation.height - 1;

	  points[2].x = page->allocation.x + page->allocation.width - 1;
	  points[2].y = page->allocation.y + page->allocation.height - TAB_CURVATURE - 1;

	  points[3].x = page->allocation.x + page->allocation.width - 1;
	  points[3].y = page->allocation.y + TAB_CURVATURE;

	  points[4].x = page->allocation.x + page->allocation.width - TAB_CURVATURE - 1;
	  points[4].y = page->allocation.y;

	  points[5].x = page->allocation.x;
	  points[5].y = page->allocation.y;
	  break;
	}

      if (notebook->cur_page == page)
	state_type = GTK_STATE_NORMAL;
      else
	state_type = GTK_STATE_ACTIVE;

      gtk_draw_polygon (GTK_WIDGET (notebook)->style,
			GTK_WIDGET (notebook)->window,
			state_type, GTK_SHADOW_OUT,
			points, 6, (notebook->cur_page != page));

      if (gtk_widget_intersect (page->tab_label, area, &child_area))
	gtk_widget_draw (page->tab_label, &child_area);
    }
}

static void
gtk_notebook_pages_allocate (GtkNotebook   *notebook,
			     GtkAllocation *allocation)
{
  GtkNotebookPage *page;
  GtkAllocation child_allocation;
  GList *children;

  if (notebook->show_tabs && notebook->children)
    {
      child_allocation.x = GTK_CONTAINER (notebook)->border_width;
      child_allocation.y = GTK_CONTAINER (notebook)->border_width;

      switch (notebook->tab_pos)
	{
	case GTK_POS_BOTTOM:
	  child_allocation.y = allocation->height - notebook->cur_page->requisition.height;
	case GTK_POS_TOP:
	  child_allocation.height = notebook->cur_page->requisition.height;
	  break;
	case GTK_POS_RIGHT:
	  child_allocation.x = allocation->width - notebook->cur_page->requisition.width;
	case GTK_POS_LEFT:
	  child_allocation.width = notebook->cur_page->requisition.width;
	  break;
	}

      children = notebook->children;
      while (children)
	{
	  page = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (page->child))
	    {
	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		  child_allocation.width = page->requisition.width + TAB_OVERLAP;
		  break;
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		  child_allocation.height = page->requisition.height + TAB_OVERLAP;
		  break;
		}

	      gtk_notebook_page_allocate (notebook, page, &child_allocation);

	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		  child_allocation.x += child_allocation.width - TAB_OVERLAP;
		  break;
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		  child_allocation.y += child_allocation.height - TAB_OVERLAP;
		  break;
		}
	    }
	}
    }
}

static void
gtk_notebook_page_allocate (GtkNotebook     *notebook,
			    GtkNotebookPage *page,
			    GtkAllocation   *allocation)
{
  GtkAllocation child_allocation;
  gint xthickness, ythickness;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (page != NULL);
  g_return_if_fail (allocation != NULL);

  page->allocation = *allocation;

  xthickness = GTK_WIDGET (notebook)->style->klass->xthickness;
  ythickness = GTK_WIDGET (notebook)->style->klass->ythickness;

  if (notebook->cur_page != page)
    {
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  page->allocation.y += ythickness;
	case GTK_POS_BOTTOM:
	  page->allocation.height -= ythickness;
	  break;
	case GTK_POS_LEFT:
	  page->allocation.x += xthickness;
	case GTK_POS_RIGHT:
	  page->allocation.width -= xthickness;
	  break;
	}
    }

  switch (notebook->tab_pos)
    {
    case GTK_POS_TOP:
      child_allocation.x = xthickness + CHILD_SPACING;
      child_allocation.y = ythickness + CHILD_SPACING;
      child_allocation.width = page->allocation.width - child_allocation.x * 2;
      child_allocation.height = page->allocation.height - child_allocation.y;
      child_allocation.x += page->allocation.x;
      child_allocation.y += page->allocation.y;
      break;
    case GTK_POS_BOTTOM:
      child_allocation.x = xthickness + CHILD_SPACING;
      child_allocation.y = ythickness + CHILD_SPACING;
      child_allocation.width = page->allocation.width - child_allocation.x * 2;
      child_allocation.height = page->allocation.height - child_allocation.y;
      child_allocation.x += page->allocation.x;
      child_allocation.y = page->allocation.y;
      break;
    case GTK_POS_LEFT:
      child_allocation.x = xthickness + CHILD_SPACING;
      child_allocation.y = ythickness + CHILD_SPACING;
      child_allocation.width = page->allocation.width - child_allocation.x;
      child_allocation.height = page->allocation.height - child_allocation.y * 2;
      child_allocation.x += page->allocation.x;
      child_allocation.y += page->allocation.y;
      break;
    case GTK_POS_RIGHT:
      child_allocation.x = xthickness + CHILD_SPACING;
      child_allocation.y = ythickness + CHILD_SPACING;
      child_allocation.width = page->allocation.width - child_allocation.x;
      child_allocation.height = page->allocation.height - child_allocation.y * 2;
      child_allocation.x = page->allocation.x;
      child_allocation.y += page->allocation.y;
      break;
    }

  gtk_widget_size_allocate (page->tab_label, &child_allocation);
}
