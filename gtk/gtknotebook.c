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
#include "gtknotebook.h"
#include "gtksignal.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtklabel.h"
#include <gdk/gdkkeysyms.h>
#include <stdio.h>


#define TAB_OVERLAP    2
#define TAB_CURVATURE  1
#define ARROW_SIZE     11
#define ARROW_SPACING  3
#define NOTEBOOK_INIT_SCROLL_DELAY (200)
#define NOTEBOOK_SCROLL_DELAY      (100)


enum {
  SWITCH_PAGE,
  LAST_SIGNAL
};

enum {
  STEP_PREV,
  STEP_NEXT
};

typedef void (*GtkNotebookSignal) (GtkObject       *object,
				   GtkNotebookPage *arg1,
				   gint             arg2,
				   gpointer         data);

static void gtk_notebook_class_init          (GtkNotebookClass *klass);
static void gtk_notebook_init                (GtkNotebook      *notebook);
static void gtk_notebook_destroy             (GtkObject        *object);
static void gtk_notebook_map                 (GtkWidget        *widget);
static void gtk_notebook_unmap               (GtkWidget        *widget);
static void gtk_notebook_realize             (GtkWidget        *widget);
static void gtk_notebook_panel_realize       (GtkNotebook      *notebook);
static void gtk_notebook_size_request        (GtkWidget        *widget,
					      GtkRequisition   *requisition);
static void gtk_notebook_size_allocate       (GtkWidget        *widget,
					      GtkAllocation    *allocation);
static void gtk_notebook_paint               (GtkWidget        *widget,
					      GdkRectangle     *area);
static void gtk_notebook_draw                (GtkWidget        *widget,
					      GdkRectangle     *area);
static gint gtk_notebook_expose              (GtkWidget        *widget,
					      GdkEventExpose   *event);
static gint gtk_notebook_button_press        (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint gtk_notebook_button_release      (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint gtk_notebook_enter_notify        (GtkWidget        *widget,
					      GdkEventCrossing *event);
static gint gtk_notebook_leave_notify        (GtkWidget        *widget,
					      GdkEventCrossing *event);
static gint gtk_notebook_motion_notify       (GtkWidget        *widget,
					      GdkEventMotion   *event);
static gint gtk_notebook_key_press           (GtkWidget        *widget,
					      GdkEventKey      *event);
static void gtk_notebook_add                 (GtkContainer     *container,
					      GtkWidget        *widget);
static void gtk_notebook_remove              (GtkContainer     *container,
					      GtkWidget        *widget);
static void gtk_notebook_real_remove         (GtkNotebook      *notebook,
					      GList            *list,
					      gint              page_num);
static void gtk_notebook_foreach             (GtkContainer     *container,
					      GtkCallback       callback,
					      gpointer          callback_data);
static void gtk_notebook_switch_page         (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      gint              page_num);
static void gtk_notebook_draw_tab            (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      GdkRectangle     *area);
static void gtk_notebook_set_focus_child     (GtkContainer     *container,
					      GtkWidget        *child);
static gint gtk_notebook_focus_in            (GtkWidget        *widget,
					      GdkEventFocus    *event);
static gint gtk_notebook_focus_out           (GtkWidget        *widget,
					      GdkEventFocus    *event);
static void gtk_notebook_draw_focus          (GtkWidget        *widget);
static void gtk_notebook_focus_changed       (GtkNotebook      *notebook, 
					      GtkNotebookPage  *old_page);
static void gtk_notebook_pages_allocate      (GtkNotebook      *notebook,
					      GtkAllocation    *allocation);
static void gtk_notebook_page_allocate       (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      GtkAllocation    *allocation);
static void gtk_notebook_draw_arrow          (GtkNotebook      *notebook, 
					      guint             arrow);
static gint gtk_notebook_timer               (GtkNotebook      *notebook);
static gint gtk_notebook_focus               (GtkContainer     *container,
					      GtkDirectionType  direction);
static gint gtk_notebook_page_select         (GtkNotebook      *notebook);
static void gtk_notebook_calc_tabs           (GtkNotebook      *notebook, 
			                      GList            *start, 
					      GList           **end,
					      gint             *tab_space,
					      guint             direction);
static void gtk_notebook_expose_tabs         (GtkNotebook      *notebook);
static void gtk_notebook_switch_focus_tab    (GtkNotebook      *notebook, 
                                              GList            *new_child);
static void gtk_real_notebook_switch_page    (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      gint              page_num);
static void gtk_notebook_marshal_signal      (GtkObject        *object,
					      GtkSignalFunc     func,
					      gpointer          func_data,
					      GtkArg           *args);
static void gtk_notebook_menu_switch_page    (GtkWidget        *widget,
					      GtkNotebookPage  *page);
static void gtk_notebook_update_labels       (GtkNotebook      *notebook,
					      GList            *list,
					      gint              page_num);
static void gtk_notebook_menu_detacher       (GtkWidget        *widget,
					      GtkMenu          *menu);
static void gtk_notebook_menu_label_unparent (GtkWidget        *widget, 
					      gpointer          data);
static void gtk_notebook_menu_item_create    (GtkNotebook      *notebook, 
					      GtkNotebookPage  *page,
					      gint              position);
static GtkType gtk_notebook_child_type       (GtkContainer     *container);


static GtkContainerClass *parent_class = NULL;
static guint notebook_signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_notebook_get_type (void)
{
  static GtkType notebook_type = 0;

  if (!notebook_type)
    {
      GtkTypeInfo notebook_info =
      {
	"GtkNotebook",
	sizeof (GtkNotebook),
	sizeof (GtkNotebookClass),
	(GtkClassInitFunc) gtk_notebook_class_init,
	(GtkObjectInitFunc) gtk_notebook_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
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

  notebook_signals[SWITCH_PAGE] =
    gtk_signal_new ("switch_page",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkNotebookClass, switch_page),
                    gtk_notebook_marshal_signal,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, notebook_signals, LAST_SIGNAL);

  object_class->destroy = gtk_notebook_destroy;

  widget_class->map = gtk_notebook_map;
  widget_class->unmap = gtk_notebook_unmap;
  widget_class->realize = gtk_notebook_realize;
  widget_class->size_request = gtk_notebook_size_request;
  widget_class->size_allocate = gtk_notebook_size_allocate;
  widget_class->draw = gtk_notebook_draw;
  widget_class->expose_event = gtk_notebook_expose;
  widget_class->button_press_event = gtk_notebook_button_press;
  widget_class->button_release_event = gtk_notebook_button_release;
  widget_class->enter_notify_event = gtk_notebook_enter_notify;
  widget_class->leave_notify_event = gtk_notebook_leave_notify;
  widget_class->motion_notify_event = gtk_notebook_motion_notify;
  widget_class->key_press_event = gtk_notebook_key_press;
  widget_class->focus_in_event = gtk_notebook_focus_in;
  widget_class->focus_out_event = gtk_notebook_focus_out;
  widget_class->draw_focus = gtk_notebook_draw_focus;

  container_class->add = gtk_notebook_add;
  container_class->remove = gtk_notebook_remove;
  container_class->foreach = gtk_notebook_foreach;
  container_class->focus = gtk_notebook_focus;
  container_class->set_focus_child = gtk_notebook_set_focus_child;
  container_class->child_type = gtk_notebook_child_type;

  class->switch_page = gtk_real_notebook_switch_page;
}

static GtkType
gtk_notebook_child_type (GtkContainer     *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_notebook_init (GtkNotebook *notebook)
{
  GTK_WIDGET_SET_FLAGS (notebook, GTK_CAN_FOCUS);

  notebook->cur_page = NULL;
  notebook->children = NULL;
  notebook->first_tab = NULL;
  notebook->focus_tab = NULL;
  notebook->panel = NULL;
  notebook->menu = NULL;

  notebook->tab_border = 3;
  notebook->show_tabs = TRUE;
  notebook->show_border = TRUE;
  notebook->tab_pos = GTK_POS_TOP;
  notebook->scrollable = FALSE;
  notebook->in_child = 0;
  notebook->click_child = 0;
  notebook->button = 0;
  notebook->need_timer = 0;
  notebook->child_has_focus = FALSE;
}

GtkWidget*
gtk_notebook_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_notebook_get_type ()));
}

static void
gtk_notebook_destroy (GtkObject *object)
{
  GtkNotebook *notebook;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (object));

  notebook = GTK_NOTEBOOK (object);

  if (notebook->menu)
    gtk_notebook_popup_disable (notebook);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

void
gtk_notebook_append_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, -1);
}

void
gtk_notebook_append_page_menu (GtkNotebook *notebook,
			       GtkWidget   *child,
			       GtkWidget   *tab_label,
			       GtkWidget   *menu_label)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, -1);
}

void
gtk_notebook_prepend_page (GtkNotebook *notebook,
			   GtkWidget   *child,
			   GtkWidget   *tab_label)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, 0);
}

void
gtk_notebook_prepend_page_menu (GtkNotebook *notebook,
				GtkWidget   *child,
				GtkWidget   *tab_label,
				GtkWidget   *menu_label)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, 0);
}

void
gtk_notebook_insert_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label,
			  gint         position)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, position);
}

void
gtk_notebook_insert_page_menu (GtkNotebook *notebook,
			       GtkWidget   *child,
			       GtkWidget   *tab_label,
			       GtkWidget   *menu_label,
			       gint         position)
{
  GtkNotebookPage *page;
  gint nchildren;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  page = g_new (GtkNotebookPage, 1);
  page->child = child;
  page->requisition.width = 0;
  page->requisition.height = 0;
  page->allocation.x = 0;
  page->allocation.y = 0;
  page->allocation.width = 0;
  page->allocation.height = 0;
  page->default_menu = FALSE;
  page->default_tab = FALSE;

  nchildren = g_list_length (notebook->children);
  if ((position < 0) || (position > nchildren))
    position = nchildren;

  notebook->children = g_list_insert (notebook->children, page, position);

  if (!tab_label)
    {
      page->default_tab = TRUE;
      if (notebook->show_tabs)
	tab_label = gtk_label_new ("");
    }
  page->tab_label = tab_label;
  page->menu_label = menu_label;

  if (!menu_label)
    page->default_menu = TRUE;
  else  
    {
      gtk_widget_ref (page->menu_label);
      gtk_object_sink (GTK_OBJECT(page->menu_label));
    }

  if (notebook->menu)
    gtk_notebook_menu_item_create (notebook, page, position);

  gtk_notebook_update_labels 
    (notebook, g_list_nth (notebook->children, position), position + 1);

  if (!notebook->first_tab)
    notebook->first_tab = notebook->children;

  gtk_widget_set_parent (child, GTK_WIDGET (notebook));
  if (tab_label)
    {
      gtk_widget_set_parent (tab_label, GTK_WIDGET (notebook));
      gtk_widget_show (tab_label);
    }

  if (!notebook->cur_page)
    {
      gtk_notebook_switch_page (notebook, page, 0);
      notebook->focus_tab = NULL;
    }

  if (GTK_WIDGET_VISIBLE (notebook))
    {
      if (GTK_WIDGET_REALIZED (notebook) &&
	  !GTK_WIDGET_REALIZED (child))
	gtk_widget_realize (child);
	
      if (GTK_WIDGET_MAPPED (notebook) &&
	  !GTK_WIDGET_MAPPED (child) && notebook->cur_page == page)
	gtk_widget_map (child);

      if (tab_label)
	{
	  if (GTK_WIDGET_REALIZED (notebook) &&
	      !GTK_WIDGET_REALIZED (tab_label))
	    gtk_widget_realize (tab_label);
      
	  if (GTK_WIDGET_MAPPED (notebook) &&
	      !GTK_WIDGET_MAPPED (tab_label))
	    gtk_widget_map (tab_label);
	}
    }

  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (notebook))
    gtk_widget_queue_resize (child);
}

void
gtk_notebook_remove_page (GtkNotebook *notebook,
			  gint         page_num)
{
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if ((list = g_list_nth (notebook->children, page_num)))
    gtk_notebook_real_remove (notebook, list, page_num);
}

static void
gtk_notebook_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (container));
  g_return_if_fail (widget != NULL);

  gtk_notebook_insert_page_menu (GTK_NOTEBOOK (container), widget, 
				 NULL, NULL, -1);
}

static void
gtk_notebook_remove (GtkContainer *container,
		     GtkWidget    *widget)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  gint page_num;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (container));
  g_return_if_fail (widget != NULL);

  notebook = GTK_NOTEBOOK (container);

  children = notebook->children;
  page_num = 0;
  while (children)
    {
      page = children->data;
      if (page->child == widget)
	{
	  gtk_notebook_real_remove (notebook, children, page_num);
	  break;
	}
      page_num++;
      children = children->next;
    }
}

static void
gtk_notebook_real_remove (GtkNotebook *notebook,
			  GList        *list,
			  gint          page_num)
{
  GtkNotebookPage *page;
  GList * next_list;
  gint need_resize = FALSE;
      
  if (list->prev)
    {
      next_list = list->prev;
      page_num--;
    }
  else if (list->next)
    {
      next_list = list->next;
      page_num++;
    }
  else 
    next_list = NULL;
  
  if (notebook->cur_page == list->data)
    { 
      notebook->cur_page = NULL;
      if (next_list)
	{
	  page = next_list->data;
	  gtk_notebook_switch_page (notebook, page, page_num);
	}
    }
  
  if (list == notebook->first_tab)
    notebook->first_tab = next_list;
  if (list == notebook->focus_tab)
    notebook->focus_tab = next_list;
  
  page = list->data;
  
  if ((GTK_WIDGET_VISIBLE (page->child) || 
       (page->tab_label && GTK_WIDGET_VISIBLE (page->tab_label))) 
       && GTK_WIDGET_VISIBLE (notebook))
    need_resize = TRUE;

  gtk_widget_unparent (page->child);

  if (page->tab_label)
    gtk_widget_unparent (page->tab_label);

  if (notebook->menu)
    {
      gtk_container_remove (GTK_CONTAINER (notebook->menu), 
			    page->menu_label->parent);
      gtk_widget_queue_resize (notebook->menu);
    }
  if (!page->default_menu)
    gtk_widget_unref (page->menu_label);
  
  gtk_notebook_update_labels (notebook, list->next, page_num + 1);
  
  notebook->children = g_list_remove_link (notebook->children, list);
  g_list_free (list);
  g_free (page);

  if (need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

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
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if ((list = g_list_nth (notebook->children, page_num)))
      gtk_notebook_switch_page (notebook, 
				((GtkNotebookPage *)(list->data)), page_num);
}

void
gtk_notebook_next_page (GtkNotebook *notebook)
{
  GtkNotebookPage *page;
  GList *children;
  gint num;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  children = notebook->children;
  num = 0;
  while (children)
    {
      if (notebook->cur_page == children->data)
	break;
      children = children->next;
      num++;
    }

  if (!children)
    return;

  if (children->next)
    {
      children = children->next;
      num++;
    }
  else
    {
      children = notebook->children;
      num = 0;
    }

  page = children->data;
  gtk_notebook_switch_page (notebook, page, num);
}

void
gtk_notebook_prev_page (GtkNotebook *notebook)
{
  GtkNotebookPage *page;
  GList *children;
  gint num;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  children = notebook->children;
  num = 0;
  while (children)
    {
      if (notebook->cur_page == children->data)
	break;
      children = children->next;
      num++;
    }

  if (!children)
    return;

  if (children->prev)
    {
      children = children->prev;
      num--;
    }
  else
    {
      while (children->next)
	{
	  children = children->next;
	  num++;
	}
    }

  page = children->data;
  gtk_notebook_switch_page (notebook, page, num);
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
gtk_notebook_expose_tabs (GtkNotebook *notebook)
{
  GtkWidget *widget;
  GtkNotebookPage *page;
  GdkEventExpose event;
  gint border;

  widget = GTK_WIDGET (notebook);
  border = GTK_CONTAINER (notebook)->border_width;

  page = notebook->first_tab->data;

  event.type = GDK_EXPOSE;
  event.window = widget->window;
  event.count = 0;
  event.area.x = border;
  event.area.y = border;

  switch (notebook->tab_pos)
    {
    case GTK_POS_BOTTOM:
      event.area.y = widget->allocation.height - border 
	- page->allocation.height - widget->style->klass->ythickness;
      if (notebook->first_tab->data != notebook->cur_page)
	event.area.y -= widget->style->klass->ythickness;
    case GTK_POS_TOP:
      event.area.width = widget->allocation.width - 2 * border;
      event.area.height = page->allocation.height 
	+ widget->style->klass->ythickness;
      if (notebook->first_tab->data != notebook->cur_page)
	event.area.height += widget->style->klass->ythickness;
      break;
    case GTK_POS_RIGHT:
      event.area.x = widget->allocation.width - border 
	- page->allocation.width - widget->style->klass->xthickness;
      if (notebook->first_tab->data != notebook->cur_page)
	event.area.x -= widget->style->klass->xthickness;
    case GTK_POS_LEFT:
      event.area.width = page->allocation.width 
	+ widget->style->klass->xthickness;
      event.area.height = widget->allocation.height - 2 * border;
      if (notebook->first_tab->data != notebook->cur_page)
	event.area.width += widget->style->klass->xthickness;
      break;
    }	     
  gtk_widget_event (widget, (GdkEvent *) &event);
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
	{
	  gtk_widget_queue_resize (GTK_WIDGET (notebook));
	  if (notebook->panel)
	    gdk_window_clear (notebook->panel);
	}
    }
}

void
gtk_notebook_set_show_tabs (GtkNotebook *notebook,
			    gint         show_tabs)
{
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->show_tabs == show_tabs)
    return;

  notebook->show_tabs = show_tabs;
  children = notebook->children;

  if (!show_tabs)
    {
      GTK_WIDGET_UNSET_FLAGS (notebook, GTK_CAN_FOCUS);
      
      while (children)
	{
	  page = children->data;
	  children = children->next;
	  if (page->default_tab)
	    {
	      gtk_widget_destroy (page->tab_label);
	      page->tab_label = NULL;
	    }
	  else
	    gtk_widget_hide (page->tab_label);
	}
      
      if (notebook->panel)
	gdk_window_hide (notebook->panel);
    }
  else
    {
      gchar string[32];
      gint i = 1;
      
      GTK_WIDGET_SET_FLAGS (notebook, GTK_CAN_FOCUS);
      
      while (children)
	{
	  page = children->data;
	  children = children->next;
	  if (page->default_tab)
	    {
	      sprintf (string, "Page %d", i);
	      page->tab_label = gtk_label_new (string);
	      gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
	    }
	  gtk_widget_show (page->tab_label);
	  i++;
	}
    }
  gtk_widget_queue_resize (GTK_WIDGET (notebook));
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

void
gtk_notebook_set_scrollable (GtkNotebook     *notebook,
			     gint             scrollable)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (scrollable != notebook->scrollable)
    {
      if ( (notebook->scrollable = (scrollable != 0)) ) 
	gtk_notebook_panel_realize (notebook);
      else if (notebook->panel)
	{
	  gdk_window_destroy (notebook->panel);
	  notebook->panel = NULL;
	}
      gtk_widget_queue_resize (GTK_WIDGET(notebook));
    }	  
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

  if (notebook->scrollable)
      gtk_notebook_pages_allocate (notebook, &(widget->allocation));
  else
    {
      children = notebook->children;

      while (children)
	{
	  page = children->data;
	  children = children->next;

	  if (page->tab_label && 
	      GTK_WIDGET_VISIBLE (page->child) && 
	      !GTK_WIDGET_MAPPED (page->tab_label))
	    gtk_widget_map (page->tab_label);
	}
    }
}

static void
gtk_notebook_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  gdk_window_hide (widget->window);
  if (GTK_NOTEBOOK (widget)->panel)
    gdk_window_hide (GTK_NOTEBOOK (widget)->panel);
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
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
    | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, notebook);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  if (notebook->scrollable)
    gtk_notebook_panel_realize (notebook);
}

static void
gtk_notebook_panel_realize (GtkNotebook *notebook)
{
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  
  widget = GTK_WIDGET (notebook);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK 
    | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
    | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;
  
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  
  attributes.width = 2 * ARROW_SIZE + ARROW_SPACING;
  attributes.height = ARROW_SIZE;

  attributes.x = widget->allocation.width - attributes.width - 
    GTK_CONTAINER (notebook)->border_width;
  attributes.y = widget->allocation.height - ARROW_SIZE -
    GTK_CONTAINER (notebook)->border_width;
  if (notebook->tab_pos == GTK_POS_TOP)
    attributes.y = GTK_CONTAINER (notebook)->border_width;
  else if (notebook->tab_pos == GTK_POS_LEFT)
    attributes.x = widget->allocation.x 
      + GTK_CONTAINER (notebook)->border_width;

  
  notebook->panel = gdk_window_new (widget->window, &attributes, 
				    attributes_mask);
  gtk_style_set_background (widget->style, notebook->panel, 
			    GTK_STATE_NORMAL);
  gdk_window_set_user_data (notebook->panel, widget);
}

static void
gtk_notebook_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;

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

  if (notebook->show_border || notebook->show_tabs)
    {
      widget->requisition.width += widget->style->klass->xthickness * 2;
      widget->requisition.height += widget->style->klass->ythickness * 2;

      if (notebook->show_tabs)
	{
	  gint tab_width = 0;
	  gint tab_height = 0;
	  gint tab_max = 0;
	  
	  children = notebook->children;
	  while (children)
	    {
	      page = children->data;
	      children = children->next;
	      
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  gtk_widget_size_request (page->tab_label, 
					   &page->tab_label->requisition);
		  
		  page->requisition.width = 
		    (page->tab_label->requisition.width +
		     (widget->style->klass->xthickness + notebook->tab_border)
		     * 2);
		  page->requisition.height = 
		    (page->tab_label->requisition.height +
		     (widget->style->klass->ythickness + notebook->tab_border)
		     * 2);
		  
		  switch (notebook->tab_pos)
		    {
		    case GTK_POS_TOP:
		    case GTK_POS_BOTTOM:
		      page->requisition.width -= TAB_OVERLAP;

		      tab_width += page->requisition.width;
		      tab_height = MAX (tab_height, page->requisition.height);
		      tab_max = MAX (tab_max, page->requisition.width);
		      break;
		    case GTK_POS_LEFT:
		    case GTK_POS_RIGHT:
		      page->requisition.height -= TAB_OVERLAP;

		      tab_width = MAX (tab_width, page->requisition.width);
		      tab_height += page->requisition.height;
		      tab_max = MAX (tab_max, page->requisition.height);
		      break;
		    }
		}
	    }

	  children = notebook->children;

	  if (children && children->next && notebook->scrollable) 
	    {
	      if ((notebook->tab_pos == GTK_POS_TOP) ||
		  (notebook->tab_pos == GTK_POS_BOTTOM))
		{
		  if (widget->requisition.width < tab_width)
		    {
		      tab_width = tab_max + 2 * (ARROW_SIZE + ARROW_SPACING);
		      tab_height = MAX (tab_height, ARROW_SIZE);
		    }
		}
	      else
		{
		  if (widget->requisition.height < tab_height)
		    {
		      tab_height = tab_max + ARROW_SIZE + ARROW_SPACING;
		      tab_width = MAX (tab_width, 
				       ARROW_SPACING + 2 * ARROW_SIZE);
		    }
		}
	    }

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
	      tab_width += widget->style->klass->xthickness;
	      widget->requisition.width = MAX (widget->requisition.width, 
					       tab_width);
	      widget->requisition.height += tab_height;
	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      tab_height += widget->style->klass->ythickness;
	      widget->requisition.width += tab_width;
	      widget->requisition.height = MAX (widget->requisition.height, 
						tab_height);
	      break;
	    }
	}
    }
  widget->requisition.width += GTK_CONTAINER (widget)->border_width * 2;
  widget->requisition.height += GTK_CONTAINER (widget)->border_width * 2;
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
      child_allocation.width = MAX (1, allocation->width - child_allocation.x * 2);
      child_allocation.height = MAX (1, allocation->height - child_allocation.y * 2);

      if (notebook->show_tabs || notebook->show_border)
	{
	  child_allocation.x += widget->style->klass->xthickness;
	  child_allocation.y += widget->style->klass->ythickness;
	  child_allocation.width = MAX (1, 
	      child_allocation.width - widget->style->klass->xthickness * 2);
	  child_allocation.height = MAX (1, 
              child_allocation.height - widget->style->klass->ythickness * 2);

	  if (notebook->show_tabs && notebook->children)
	    {
	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		  child_allocation.y += notebook->cur_page->requisition.height;
		case GTK_POS_BOTTOM:
		  child_allocation.height = MAX (1, 
		    child_allocation.height - notebook->cur_page->requisition.height);
		  break;
		case GTK_POS_LEFT:
		  child_allocation.x += notebook->cur_page->requisition.width;
		case GTK_POS_RIGHT:
		  child_allocation.width = MAX (1, 
		    child_allocation.width - notebook->cur_page->requisition.width);
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
  gint showarrow;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      notebook = GTK_NOTEBOOK (widget);

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

	      if (!GTK_WIDGET_MAPPED (notebook->cur_page->tab_label))
		{
		  GtkNotebookPage *page;

		  page = notebook->first_tab->data;

		  switch (notebook->tab_pos)
		    {
		    case GTK_POS_TOP:
		      y += page->allocation.height +
			widget->style->klass->ythickness;
		    case GTK_POS_BOTTOM:
		      height -= page->allocation.height +
			widget->style->klass->ythickness;
		      break;
		    case GTK_POS_LEFT:
		      x += page->allocation.width +
			widget->style->klass->xthickness;
		    case GTK_POS_RIGHT:
		      width -= page->allocation.width +
			widget->style->klass->xthickness;
		    break;
		    }
		  gtk_draw_shadow (widget->style, widget->window,
				   GTK_STATE_NORMAL, GTK_SHADOW_OUT,
				   x, y, width, height);
		}
	      else
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
				     widget->style->klass->xthickness);
		      points[5].y = y;

		      if (points[5].x == (x + width))
			points[5].x -= 1;
		      break;
		    case GTK_POS_BOTTOM:
		      points[0].x = (notebook->cur_page->allocation.x +
				     notebook->cur_page->allocation.width -
				     widget->style->klass->xthickness);
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
				     widget->style->klass->ythickness);
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
				     widget->style->klass->ythickness);

		      if (points[5].y == (y + height))
			points[5].y -= 1;
		      break;
		    }
		  
		  gtk_draw_polygon (widget->style, widget->window,
				    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
				    points, 6, FALSE);
		}
	      children = g_list_last (notebook->children);
	      showarrow = FALSE;

	      while (children)
		{
		  page = children->data;
		  children = children->prev;

		  if (!GTK_WIDGET_MAPPED (page->tab_label))
		    showarrow = TRUE;
		  else if (notebook->cur_page != page)
		    gtk_notebook_draw_tab (notebook, page, area);
		}

	      if (showarrow && notebook->scrollable && notebook->show_tabs) 
		{
		  gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
		  gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
		}
	      if (notebook->cur_page && 
		  GTK_WIDGET_MAPPED(((GtkNotebookPage *)
				     (notebook->cur_page))->tab_label))
		gtk_notebook_draw_tab (notebook, notebook->cur_page, area);
	    }
	  else if (notebook->show_border)
	    {
	      gtk_draw_shadow (widget->style, widget->window,
			       GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			       x, y, width, height);
	    }
	}
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
      gtk_widget_draw_focus (widget);

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
      gtk_widget_draw_focus (widget);

      child_event = *event;
      if (notebook->cur_page && 
	  GTK_WIDGET_NO_WINDOW (notebook->cur_page->child) &&
	  gtk_widget_intersect (notebook->cur_page->child, &event->area, 
				&child_event.area))
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
  gint num;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  notebook = GTK_NOTEBOOK (widget);

  if (event->type != GDK_BUTTON_PRESS || !notebook->children 
      || notebook->button)
    return FALSE;

  if (event->window == notebook->panel)
    {
      if (!GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);

      gtk_grab_add (widget);
      notebook->button = event->button;
      
      if (event->x <= ARROW_SIZE + ARROW_SPACING / 2)
	{
	  notebook->click_child = GTK_ARROW_LEFT;
	  if (event->button == 1)
	    {
	      if (!notebook->focus_tab || notebook->focus_tab->prev)
		gtk_container_focus (GTK_CONTAINER (notebook), GTK_DIR_LEFT);

	      if (!notebook->timer)
		{
		  notebook->timer = gtk_timeout_add 
		    (NOTEBOOK_INIT_SCROLL_DELAY, 
		     (GtkFunction) gtk_notebook_timer, (gpointer) notebook);
		  notebook->need_timer = TRUE;
		}
	    }
	  else if (event->button == 2)
	    gtk_notebook_page_select (GTK_NOTEBOOK (widget));
	  else if (event->button == 3)
	    gtk_notebook_switch_focus_tab (notebook, notebook->children);
	  gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
	}
      else
	{
	  notebook->click_child = GTK_ARROW_RIGHT;
	  if (event->button == 1)
	    {
	      if (!notebook->focus_tab || notebook->focus_tab->next)
		gtk_container_focus (GTK_CONTAINER (notebook), GTK_DIR_RIGHT);
	      if (!notebook->timer)
		{
		  notebook->timer = gtk_timeout_add 
		    (NOTEBOOK_INIT_SCROLL_DELAY, 
		     (GtkFunction) gtk_notebook_timer, (gpointer) notebook);
		  notebook->need_timer = TRUE;
		}
	    }      
	  else if (event->button == 2)
	    gtk_notebook_page_select (GTK_NOTEBOOK (widget));
	  else if (event->button == 3)
	    gtk_notebook_switch_focus_tab (notebook, 
					   g_list_last (notebook->children));
	  gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
	}
    }
  else if (event->window == widget->window)
    {
      if (event->button == 3 && notebook->menu)
	{
	  gtk_menu_popup (GTK_MENU (notebook->menu), NULL, NULL, 
			  NULL, NULL, 3, event->time);
	  return FALSE;
	}

      num = 0;
      children = notebook->children;
      while (children)
	{
	  page = children->data;
	  
	  if (GTK_WIDGET_VISIBLE (page->child) &&
	      page->tab_label && GTK_WIDGET_MAPPED (page->tab_label) &&
	      (event->x >= page->allocation.x) &&
	      (event->y >= page->allocation.y) &&
	      (event->x <= (page->allocation.x + page->allocation.width)) &&
	      (event->y <= (page->allocation.y + page->allocation.height)))
	    {
	      if (page == notebook->cur_page && notebook->focus_tab &&
		  notebook->focus_tab != children &&
		  GTK_WIDGET_HAS_FOCUS (notebook))
		{
		  GtkNotebookPage *old_page;

		  notebook->child_has_focus = FALSE;
		  old_page = (GtkNotebookPage *)
			      (notebook->focus_tab->data);
		  notebook->focus_tab = children;
		  gtk_notebook_focus_changed (notebook, old_page);
		}
	      else
		{
		  notebook->focus_tab = children;
		  gtk_notebook_switch_page (notebook, page, num);
		  gtk_widget_grab_focus (widget);
		}
	      break;
	    }
	  children = children->next;
	  num++;
	}
      if (!children && !GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);
    }
  return FALSE;
}

static gint
gtk_notebook_button_release (GtkWidget      *widget,
			     GdkEventButton *event)
{
  GtkNotebook *notebook;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  notebook = GTK_NOTEBOOK (widget);

  if (event->button == notebook->button)
    {
      guint click_child;

      if (notebook->timer)
	{
	  gtk_timeout_remove (notebook->timer);
	  notebook->timer = 0;
	  notebook->need_timer = FALSE;
	}
      gtk_grab_remove (widget);
      click_child = notebook->click_child;
      notebook->click_child = 0;
      notebook->button = 0;
      gtk_notebook_draw_arrow (notebook, click_child);
      
    }
  return FALSE;
}

static gint
gtk_notebook_enter_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  GtkNotebook *notebook;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  notebook = GTK_NOTEBOOK (widget);

  if (event->window == notebook->panel)
    {
      gint x;
      gint y;

      gdk_window_get_pointer (notebook->panel, &x, &y, NULL);

      if (x <= ARROW_SIZE + ARROW_SPACING / 2)
	{
	  notebook->in_child = GTK_ARROW_LEFT;

	  if (notebook->click_child == 0) 
	    gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
	}
      else 
	{
	  notebook->in_child = GTK_ARROW_RIGHT;

	  if (notebook->click_child == 0) 
	    gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
	}
    }

  return FALSE;
}

static gint
gtk_notebook_leave_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  GtkNotebook *notebook;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  notebook = GTK_NOTEBOOK (widget);

  if (event->window == notebook->panel && !notebook->click_child)
    {
      if (notebook->in_child == GTK_ARROW_LEFT)
	{
          notebook->in_child = 0;
	  gtk_notebook_draw_arrow (notebook,GTK_ARROW_LEFT);
	}
      else
	{
          notebook->in_child = 0;
	  gtk_notebook_draw_arrow (notebook,GTK_ARROW_RIGHT);
	}
    }
  return FALSE;
}

static gint
gtk_notebook_motion_notify (GtkWidget      *widget,
			    GdkEventMotion *event)
{
  GtkNotebook *notebook;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  notebook = GTK_NOTEBOOK (widget);
  
  if (notebook->button)
    return FALSE;

  if (event->window == notebook->panel)
    {
      gint x;
      
      x = event->x;
      if (event->is_hint)
        gdk_window_get_pointer (notebook->panel, &x, NULL, NULL);

      if (x <= ARROW_SIZE + ARROW_SPACING / 2 && 
          notebook->in_child == GTK_ARROW_RIGHT)
        {
          notebook->in_child = GTK_ARROW_LEFT;
          gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
          gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
        }
      else if (x > ARROW_SIZE + ARROW_SPACING / 2 && 
	       notebook->in_child == GTK_ARROW_LEFT)
        {
          notebook->in_child = GTK_ARROW_RIGHT;
          gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
          gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
        }
      return FALSE;
    }
  return FALSE;
}

static gint
gtk_notebook_timer (GtkNotebook *notebook)
{
  g_return_val_if_fail (notebook != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  if (notebook->timer)
    {
      if (notebook->click_child == GTK_ARROW_LEFT)
	{
	  if (!notebook->focus_tab || notebook->focus_tab->prev)
	    gtk_container_focus (GTK_CONTAINER (notebook), GTK_DIR_LEFT);
	}
      else if (notebook->click_child == GTK_ARROW_RIGHT)
	{
	  if (!notebook->focus_tab || notebook->focus_tab->next)
	    gtk_container_focus (GTK_CONTAINER (notebook), GTK_DIR_RIGHT);
	}
      
      if (notebook->need_timer) 
	{
	  notebook->need_timer = FALSE;
	  notebook->timer = gtk_timeout_add 
	    (NOTEBOOK_SCROLL_DELAY, (GtkFunction) gtk_notebook_timer, 
	     (gpointer) notebook);
	  return FALSE;
	}
      return TRUE;
    }
  return FALSE;
}

static void
gtk_notebook_draw_arrow (GtkNotebook *notebook, guint arrow)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GtkWidget *widget;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  
  widget = GTK_WIDGET(notebook);

  if (GTK_WIDGET_DRAWABLE (notebook))
    {
      if (notebook->in_child == arrow)
        {
          if (notebook->click_child == arrow)
            state_type = GTK_STATE_ACTIVE;
          else
            state_type = GTK_STATE_PRELIGHT;
        }
      else
        state_type = GTK_STATE_NORMAL;

      if (notebook->click_child == arrow)
        shadow_type = GTK_SHADOW_IN;
      else
        shadow_type = GTK_SHADOW_OUT;

      if (arrow == GTK_ARROW_LEFT)
	{
	  if (notebook->tab_pos == GTK_POS_LEFT ||
	      notebook->tab_pos == GTK_POS_RIGHT)
	    arrow = GTK_ARROW_UP;
	  gtk_draw_arrow (widget->style, notebook->panel, state_type, 
			  shadow_type, arrow, TRUE, 
                          0, 0, ARROW_SIZE, ARROW_SIZE);
	}
      else
	{
	  if (notebook->tab_pos == GTK_POS_LEFT ||
	      notebook->tab_pos == GTK_POS_RIGHT)
	    arrow = GTK_ARROW_DOWN;
	  gtk_draw_arrow (widget->style, notebook->panel, state_type, 
			  shadow_type, arrow, TRUE, ARROW_SIZE + ARROW_SPACING,
                          0, ARROW_SIZE, ARROW_SIZE);
	}
    }
}

static void
gtk_real_notebook_switch_page (GtkNotebook     *notebook,
			       GtkNotebookPage *page,
			       gint             page_num)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (page != NULL);

  if (notebook->cur_page == page)
    return;

  if (notebook->cur_page && GTK_WIDGET_MAPPED (notebook->cur_page->child))
    gtk_widget_unmap (notebook->cur_page->child);
  
  notebook->cur_page = page;

  if (!notebook->focus_tab ||
      notebook->focus_tab->data != (gpointer) notebook->cur_page)
    notebook->focus_tab = 
      g_list_find (notebook->children, notebook->cur_page);

  gtk_notebook_pages_allocate (notebook, &GTK_WIDGET (notebook)->allocation);

  if (GTK_WIDGET_MAPPED (notebook))
    {
      if (GTK_WIDGET_REALIZED (notebook->cur_page->child))
	gtk_widget_map (notebook->cur_page->child);
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

static void
gtk_notebook_draw_tab (GtkNotebook     *notebook,
		       GtkNotebookPage *page,
		       GdkRectangle    *area)
{
  GdkRectangle child_area;
  GdkRectangle page_area;
  GtkStateType state_type;
  GdkPoint points[6];
  gint n = 0;
 
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (page != NULL);
  g_return_if_fail (area != NULL);

  if (!GTK_WIDGET_MAPPED (page->tab_label))
    return;

  page_area.x = page->allocation.x;
  page_area.y = page->allocation.y;
  page_area.width = page->allocation.width;
  page_area.height = page->allocation.height;

  if (gdk_rectangle_intersect (&page_area, area, &child_area))
    {
      GtkWidget *widget;

      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  if (child_area.x + child_area.width > 
	      page->allocation.x + page->allocation.width - TAB_OVERLAP) 
	    {
	      points[0].x = page->allocation.x + page->allocation.width - 1;
	      points[0].y = page->allocation.y + page->allocation.height - 1;

	      points[1].x = page->allocation.x + page->allocation.width - 1;
	      points[1].y = page->allocation.y + TAB_CURVATURE;

	      points[2].x = page->allocation.x + page->allocation.width 
		- TAB_CURVATURE - 1;
	      points[2].y = page->allocation.y;
	      n = 3;
	    }
	  else 
	    {
	      points[0].x = page->allocation.x + page->allocation.width 
		- TAB_OVERLAP - 1;
	      points[0].y = page->allocation.y;
	      n = 1;
	    }
	  
	  if ( (child_area.x < page->allocation.x + TAB_OVERLAP) &&
	       (page == notebook->cur_page || 
		page == (GtkNotebookPage *)(notebook->children->data) ||
		(notebook->scrollable && 
		 page == (GtkNotebookPage *)(notebook->first_tab->data))) ) 
	    {
	      points[n].x = page->allocation.x + TAB_CURVATURE;
	      points[n++].y = page->allocation.y;
	    
	      points[n].x = page->allocation.x;
	      points[n++].y = page->allocation.y + TAB_CURVATURE;

	      points[n].x = page->allocation.x;
	      points[n++].y = page->allocation.y + page->allocation.height - 1;
	    }
	  else 
	    {
	      points[n].x = page->allocation.x + TAB_OVERLAP;
	      points[n++].y = page->allocation.y;
	    }
	  break;
	case GTK_POS_BOTTOM:
	  if ( (child_area.x < page->allocation.x + TAB_OVERLAP) &&
	       (page == notebook->cur_page || 
		page == (GtkNotebookPage *)(notebook->children->data) ||
		(notebook->scrollable && 
		 page == (GtkNotebookPage *)(notebook->first_tab->data))) ) 
	    {
	      points[0].x = page->allocation.x;
	      points[0].y = page->allocation.y;

	      points[1].x = page->allocation.x;
	      points[1].y = page->allocation.y + page->allocation.height 
		- TAB_CURVATURE - 1;

	      points[2].x = page->allocation.x + TAB_CURVATURE;
	      points[2].y = page->allocation.y + page->allocation.height - 1;
	      n = 3;
	    }
	  else 
	    {
	      points[0].x = page->allocation.x + TAB_OVERLAP;
	      points[0].y = page->allocation.y + page->allocation.height - 1;
	      n = 1;
	    }

	  if (child_area.x + child_area.width > 
	      page->allocation.x + page->allocation.width - TAB_OVERLAP)
	    {
	      points[n].x = page->allocation.x + page->allocation.width 
		- TAB_CURVATURE - 1;
	      points[n++].y = page->allocation.y + page->allocation.height - 1;

	      points[n].x = page->allocation.x + page->allocation.width - 1;
	      points[n++].y = page->allocation.y + page->allocation.height 
		- TAB_CURVATURE - 1;
	    
	      points[n].x = page->allocation.x + page->allocation.width - 1;
	      points[n++].y = page->allocation.y;
	    }
	  else 
	    {
	      points[n].x = page->allocation.x + page->allocation.width 
		- TAB_OVERLAP - 1;
	      points[n++].y = page->allocation.y + page->allocation.height - 1;
	    }
	  break;
	case GTK_POS_LEFT:
	  if ( (child_area.y < page->allocation.y + TAB_OVERLAP) &&
	       (page == notebook->cur_page || 
		page == (GtkNotebookPage *)(notebook->children->data) ||
		(notebook->scrollable && 
		 page == (GtkNotebookPage *)(notebook->first_tab->data))) )
	    {
	      points[0].x = page->allocation.x + page->allocation.width - 1;
	      points[0].y = page->allocation.y;
	      
	      points[1].x = page->allocation.x + TAB_CURVATURE;
	      points[1].y = page->allocation.y;

	      points[2].x = page->allocation.x;
	      points[2].y = page->allocation.y + TAB_CURVATURE;
	      n = 3;
	    }
	  else 
	    {
	      points[0].x = page->allocation.x;
	      points[0].y = page->allocation.y + TAB_OVERLAP;
	      n = 1;
	    }

	  if (child_area.y + child_area.height > 
	      page->allocation.y + page->allocation.height - TAB_OVERLAP) 
	    {
	      points[n].x = page->allocation.x;
	      points[n++].y = page->allocation.y + page->allocation.height 
		- TAB_CURVATURE - 1;

	      points[n].x = page->allocation.x + TAB_CURVATURE;
	      points[n++].y = page->allocation.y + page->allocation.height - 1;

	      points[n].x = page->allocation.x + page->allocation.width - 1;
	      points[n++].y = page->allocation.y + page->allocation.height - 1;
	    }
	  else 
	    {
	      points[n].x = page->allocation.x;
	      points[n++].y = page->allocation.y + page->allocation.height 
		- TAB_OVERLAP - 1;
	    }
	  break;
	case GTK_POS_RIGHT:
	  if (child_area.y + child_area.height > 
	      page->allocation.y + page->allocation.height - TAB_OVERLAP) 
	    {
	      points[0].x = page->allocation.x;
	      points[0].y = page->allocation.y + page->allocation.height - 1;

	      points[1].x = page->allocation.x + page->allocation.width 
		- TAB_CURVATURE - 1;
	      points[1].y = page->allocation.y + page->allocation.height - 1;
	      
	      points[2].x = page->allocation.x + page->allocation.width - 1;
	      points[2].y = page->allocation.y + page->allocation.height 
		- TAB_CURVATURE - 1;
	      n = 3;
	    }
	  else 
	    {
	      points[0].x = page->allocation.x + page->allocation.width - 1;
	      points[0].y = page->allocation.y + page->allocation.height 
		- TAB_OVERLAP - 1;
	      n = 1;
	    }

	  if ( (child_area.y < page->allocation.y + TAB_OVERLAP) && 
	       (page == notebook->cur_page || 
		page == (GtkNotebookPage *)(notebook->children->data) ||
		(notebook->scrollable && 
		 page == (GtkNotebookPage *)(notebook->first_tab->data))) ) 
	    {
	      points[n].x = page->allocation.x + page->allocation.width - 1;
	      points[n++].y = page->allocation.y + TAB_CURVATURE;

	      points[n].x = page->allocation.x + page->allocation.width 
		- TAB_CURVATURE - 1;
	      points[n++].y = page->allocation.y;

	      points[n].x = page->allocation.x;
	      points[n++].y = page->allocation.y;
	    }
	  else 
	    {
	      points[n].x = page->allocation.x + page->allocation.width - 1;
	      points[n++].y = page->allocation.y + TAB_OVERLAP;
	    }
	  break;
	}

      widget = GTK_WIDGET(notebook);

      if (notebook->cur_page == page)
	{
	  state_type = GTK_STATE_NORMAL;
	}
      else 
	{
	  state_type = GTK_STATE_ACTIVE;
	  gdk_draw_rectangle (widget->window, widget->style->bg_gc[state_type],
			      TRUE, child_area.x, child_area.y,
			      child_area.width, child_area.height);
	}
      
      gtk_draw_polygon (widget->style, widget->window, state_type, 
			GTK_SHADOW_OUT,	points, n, FALSE);

      if (gtk_widget_intersect (page->tab_label, area, &child_area))
	gtk_widget_draw (page->tab_label, &child_area);
    }
}

static void
gtk_notebook_set_focus_child (GtkContainer *container,
			      GtkWidget    *child)
{
  GtkNotebook *notebook;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (container));
  
  if (child)
    {
      g_return_if_fail (GTK_IS_WIDGET (child));

      notebook = GTK_NOTEBOOK (container);

      notebook->child_has_focus = TRUE;
      if (!notebook->focus_tab)
	{
	  GList *children;
	  GtkNotebookPage *page;

	  children = notebook->children;
	  while (children)
	    {
	      page = children->data;
	      if (page->child == child || page->tab_label == child)
		notebook->focus_tab = children;
	      children = children->next;
	    }
	}
    }
  parent_class->set_focus_child (container, child);
}

static gint
gtk_notebook_focus_in (GtkWidget     *widget,
		       GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_NOTEBOOK (widget)->child_has_focus = FALSE;
  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return FALSE;
}

static gint
gtk_notebook_focus_out (GtkWidget     *widget,
			GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return FALSE;
}

static void
gtk_notebook_draw_focus (GtkWidget *widget)
{
  GtkNotebook *notebook;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  notebook = GTK_NOTEBOOK (widget);

  if (GTK_WIDGET_DRAWABLE (widget) && notebook->show_tabs &&
      notebook->focus_tab)
    {
      GtkNotebookPage *page;
      GdkGC *gc;

      page = notebook->focus_tab->data;

      if (GTK_WIDGET_HAS_FOCUS (widget))
        gc = widget->style->black_gc;
      else if (page == notebook->cur_page)
        gc = widget->style->bg_gc[GTK_STATE_NORMAL];
      else
        gc = widget->style->bg_gc[GTK_STATE_ACTIVE];

      gdk_draw_rectangle (widget->window, 
			  gc, FALSE, 
			  page->tab_label->allocation.x - 1, 
			  page->tab_label->allocation.y - 1,
			  page->tab_label->allocation.width + 1, 
			  page->tab_label->allocation.height + 1);
    }
}

static void
gtk_notebook_focus_changed (GtkNotebook *notebook, GtkNotebookPage *old_page)
{
  GtkWidget *widget;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  widget = GTK_WIDGET (notebook);

  if (GTK_WIDGET_DRAWABLE (widget) && notebook->show_tabs)
    {
      GdkGC *gc;

      if (notebook->focus_tab)
	{
	  GtkNotebookPage *page;

	  page = notebook->focus_tab->data;

	  if (GTK_WIDGET_HAS_FOCUS (widget))
	    gc = widget->style->black_gc;
	  else if (page == notebook->cur_page)
	    gc = widget->style->bg_gc[GTK_STATE_NORMAL];
	  else
	    gc = widget->style->bg_gc[GTK_STATE_ACTIVE];
	  
	  gdk_draw_rectangle (widget->window, 
			      gc, FALSE, 
			      page->tab_label->allocation.x - 1, 
			      page->tab_label->allocation.y - 1,
			      page->tab_label->allocation.width + 1, 
			      page->tab_label->allocation.height + 1);
	}

      if (old_page)
	{
	  if (old_page == notebook->cur_page)
	    gc = widget->style->bg_gc[GTK_STATE_NORMAL];
	  else
	    gc = widget->style->bg_gc[GTK_STATE_ACTIVE];
         
	  gdk_draw_rectangle (widget->window, 
			      gc, FALSE, 
			      old_page->tab_label->allocation.x - 1, 
			      old_page->tab_label->allocation.y - 1,
			      old_page->tab_label->allocation.width + 1, 
			      old_page->tab_label->allocation.height + 1);
	}
    }
}

static void 
gtk_notebook_calc_tabs (GtkNotebook  *notebook, 
			GList        *start, 
                        GList       **end,
			gint         *tab_space,
                        guint         direction)
{
  GtkNotebookPage *page = NULL;
  GList *children;

  children = start;
  switch (notebook->tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      while (children)
	{
	  page = children->data;
	  *tab_space -= page->requisition.width;
	  if (*tab_space < 0 || children == *end)
	    {
	      if (*tab_space < 0) 
		{
		  *tab_space = - (*tab_space + page->requisition.width);
		  *end = children;
		}
	      break;
	    }
	  if (direction == STEP_NEXT)
	    children = children->next;
	  else
	    children = children->prev;
	}
      break;
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      while (children)
	{
	  page = children->data;
	  *tab_space -= page->requisition.height;
	  if (*tab_space < 0 || children == *end)
	    {
	      if (*tab_space < 0)
		{
		  *tab_space = - (*tab_space + page->requisition.height);
		  *end = children;
		}
	      break;
	    }
	  if (direction == STEP_NEXT)
	    children = children->next;
	  else
	    children = children->prev;
	}
      break;
    }
}

static void
gtk_notebook_pages_allocate (GtkNotebook   *notebook,
			     GtkAllocation *allocation)
{
  GtkWidget    *widget;
  GtkContainer *container;
  GtkNotebookPage *page = NULL;
  GtkAllocation child_allocation;
  GList *children;
  GList *last_child = NULL;
  gint showarrow = FALSE;
  gint tab_space = 0; 
  gint x = 0;
  gint y = 0;
  gint i;
  gint n = 1;
  gint old_fill = 0;
  gint new_fill = 0;
  
  if (!notebook->show_tabs || !notebook->children)
    return;
  
  widget = GTK_WIDGET (notebook);
  container = GTK_CONTAINER (notebook);
  
  child_allocation.x = container->border_width;
  child_allocation.y = container->border_width;
  
  switch (notebook->tab_pos)
    {
    case GTK_POS_BOTTOM:
      child_allocation.y = (allocation->height -
			    notebook->cur_page->requisition.height -
			    container->border_width);
    case GTK_POS_TOP:
      child_allocation.height = notebook->cur_page->requisition.height;
      break;
    case GTK_POS_RIGHT:
      child_allocation.x = (allocation->width -
			    notebook->cur_page->requisition.width -
			    container->border_width);
    case GTK_POS_LEFT:
      child_allocation.width = notebook->cur_page->requisition.width;
      break;
    }
  
  if (notebook->scrollable) 
    {
      GList *focus_tab;
      
      children = notebook->children;
      
      if (notebook->focus_tab)
	focus_tab = notebook->focus_tab;
      else if (notebook->first_tab)
	focus_tab = notebook->first_tab;
      else
	focus_tab = notebook->children;

      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;
	      tab_space += page->requisition.width;
	    }
	  if (tab_space > allocation->width - 2 * container->border_width - TAB_OVERLAP) 
	    {
	      showarrow = TRUE;
	      page = focus_tab->data; 
	      
	      tab_space = (allocation->width - TAB_OVERLAP - page->requisition.width -
			   2 * (container->border_width + ARROW_SPACING + ARROW_SIZE));
	      x = allocation->width - 2 * ARROW_SIZE - ARROW_SPACING - container->border_width;
	      
	      page = notebook->children->data;
	      if (notebook->tab_pos == GTK_POS_TOP)
		y = container->border_width + (page->requisition.height - ARROW_SIZE) / 2;
	      else
		y = (allocation->height - container->border_width - 
		     ARROW_SIZE - (page->requisition.height - ARROW_SIZE) / 2);
	    }
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;
	      tab_space += page->requisition.height;
	    }
	  if (tab_space > (allocation->height - 2 * container->border_width - TAB_OVERLAP))
	    {
	      showarrow = TRUE;
	      page = focus_tab->data; 
	      tab_space = (allocation->height -
			   ARROW_SIZE - ARROW_SPACING - TAB_OVERLAP -
			   2 * container->border_width - page->requisition.height);
	      y = allocation->height - container->border_width - ARROW_SIZE;  
	      
	      page = notebook->children->data;
	      if (notebook->tab_pos == GTK_POS_LEFT)
		x = (container->border_width +
		     (page->requisition.width - (2 * ARROW_SIZE - ARROW_SPACING)) / 2); 
	      else
		x = (allocation->width - container->border_width -
		     (2 * ARROW_SIZE -  ARROW_SPACING) -
		     (page->requisition.width - (2 * ARROW_SIZE - ARROW_SPACING)) / 2);
	    }
	  break;
	}
      if (showarrow) /* first_tab <- focus_tab */
	{ 
	  children = focus_tab->prev;
	  while (children)
	    {
	      if (notebook->first_tab == children)
		break;
	      children = children->prev;
	    }
	  
	  if (!children)
	    notebook->first_tab = focus_tab;
	  else
	    gtk_notebook_calc_tabs (notebook, focus_tab->prev, 
				    &(notebook->first_tab), &tab_space,
				    STEP_PREV);
	  if (tab_space <= 0)
	    {
	      notebook->first_tab = notebook->first_tab->next;
	      if (!notebook->first_tab)
		notebook->first_tab = focus_tab;
	      last_child = focus_tab->next; 
	    }
	  else /* focus_tab -> end */   
	    {
	      if (!notebook->first_tab)
		notebook->first_tab = notebook->children;
	      
	      children = NULL;
	      gtk_notebook_calc_tabs (notebook, focus_tab->next, 
				      &children, &tab_space, STEP_NEXT);
	      
	      if (tab_space <= 0) 
		last_child = children;
	      else /* start <- first_tab */
		{
		  last_child = NULL;
		  children = NULL;
		  gtk_notebook_calc_tabs (notebook,notebook->first_tab->prev,
					  &children, &tab_space, STEP_PREV);
		  if (children)
		    notebook->first_tab = children->next;
		  else
		    notebook->first_tab = notebook->children;
		}
	    }
	  
	  if (GTK_WIDGET_REALIZED (notebook))
	    {
	      gdk_window_move (notebook->panel, x, y);
	      gdk_window_show (notebook->panel);
	    }
	  
	  if (tab_space < 0) 
	    {
	      tab_space = -tab_space;
	      n = 0;
	      children = notebook->first_tab;
	      while (children != last_child)
		{
		  children = children->next;
		  n++;
		}
	    }
	  else 
	    tab_space = 0;
	  
	  children = notebook->children;
	  while (children != notebook->first_tab)
	    {
	      page = children->data;
	      children = children->next;
	      
	      if (page->tab_label && GTK_WIDGET_MAPPED (page->tab_label))
		gtk_widget_unmap (page->tab_label);
	      
	    }
	  children = last_child;
	  while (children)
	    {
	      page = children->data;
	      children = children->next;
	      
	      if (page->tab_label && GTK_WIDGET_MAPPED (page->tab_label))
		gtk_widget_unmap (page->tab_label);
	    }
	}
      else /* !showarrow */
	{
	  notebook->first_tab = notebook->children;
	  tab_space = 0;
	  if (GTK_WIDGET_REALIZED (notebook))
	    gdk_window_hide (notebook->panel);
	}
      children = notebook->first_tab;
    }
  else
    children = notebook->children;
  
  i = 1;
  while (children != last_child)
    {
      page = children->data;
      children = children->next;
      
      if (GTK_WIDGET_VISIBLE (page->child))
	{
	  new_fill = (tab_space * i++) / n;
	  switch (notebook->tab_pos)
	    {
	    case GTK_POS_TOP:
	    case GTK_POS_BOTTOM:
	      child_allocation.width = page->requisition.width + TAB_OVERLAP + new_fill - old_fill;
	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      child_allocation.height = page->requisition.height + TAB_OVERLAP + new_fill - old_fill;
	      break;
	    }
	  old_fill = new_fill;
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
	  
	  if (GTK_WIDGET_REALIZED (notebook) &&
	      page->tab_label && !GTK_WIDGET_MAPPED (page->tab_label))
	    gtk_widget_map (page->tab_label);
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
      child_allocation.x = xthickness + notebook->tab_border;
      child_allocation.y = ythickness + notebook->tab_border + page->allocation.y;
      child_allocation.width = page->allocation.width - child_allocation.x * 2;
      child_allocation.height = page->allocation.height - ythickness - 2 * notebook->tab_border;
      child_allocation.x += page->allocation.x;
      break;
    case GTK_POS_BOTTOM:
      child_allocation.x = xthickness + notebook->tab_border;
      child_allocation.width = page->allocation.width - child_allocation.x * 2;
      child_allocation.height = page->allocation.height - ythickness - 2 * notebook->tab_border;
      child_allocation.x += page->allocation.x;
      child_allocation.y = page->allocation.y + notebook->tab_border;
      break;
    case GTK_POS_LEFT:
      child_allocation.x = xthickness + notebook->tab_border + page->allocation.x;
      child_allocation.y = ythickness + notebook->tab_border;
      child_allocation.width = page->allocation.width - xthickness - 2 * notebook->tab_border;
      child_allocation.height = page->allocation.height - child_allocation.y * 2;
      child_allocation.y += page->allocation.y;
      break;
    case GTK_POS_RIGHT:
      child_allocation.y = ythickness + notebook->tab_border;
      child_allocation.width = page->allocation.width - xthickness - 2 * notebook->tab_border;
      child_allocation.height = page->allocation.height - child_allocation.y * 2;
      child_allocation.x = page->allocation.x + notebook->tab_border;
      child_allocation.y += page->allocation.y;
      break;
    }

  if (page->tab_label)
    gtk_widget_size_allocate (page->tab_label, &child_allocation);
}

static void
gtk_notebook_menu_switch_page (GtkWidget *widget,
			       GtkNotebookPage *page)
{
  GtkNotebook *notebook;
  GList *children;
  gint page_num;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (page != NULL);

  notebook = GTK_NOTEBOOK (gtk_menu_get_attach_widget 
			   (GTK_MENU (widget->parent)));

  if (notebook->cur_page == page)
    return;

  page_num = 0;
  children = notebook->children;
  while (children && children->data != page)
    {
      children = children->next;
      page_num++;
    }

  gtk_signal_emit (GTK_OBJECT (notebook), 
		   notebook_signals[SWITCH_PAGE], 
		   page,
		   page_num);
}

static void
gtk_notebook_switch_page (GtkNotebook     *notebook,
			  GtkNotebookPage *page,
			  gint             page_num)
{ 
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (page != NULL);
 
  if (notebook->cur_page == page)
    return;

  gtk_signal_emit (GTK_OBJECT (notebook), 
		   notebook_signals[SWITCH_PAGE], 
		   page,
		   page_num);
}

static void
gtk_notebook_marshal_signal (GtkObject      *object,
			     GtkSignalFunc   func,
			     gpointer        func_data,
			     GtkArg         *args)
{
  GtkNotebookSignal rfunc;

  rfunc = (GtkNotebookSignal) func;

  (* rfunc) (object, GTK_VALUE_POINTER (args[0]), GTK_VALUE_INT (args[1]),
	     func_data);
}

static gint
gtk_notebook_focus (GtkContainer     *container,
		    GtkDirectionType  direction)
{
  GtkNotebook *notebook;
  GtkWidget *focus_child;
  GtkNotebookPage *page = NULL;
  GtkNotebookPage *old_page = NULL;
  gint return_val;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (container), FALSE);

  notebook = GTK_NOTEBOOK (container);

  if (!GTK_WIDGET_SENSITIVE (container) || !notebook->children)
    return FALSE;

  focus_child = container->focus_child;
  gtk_container_set_focus_child (container, NULL);

  if (!notebook->show_tabs)
    {
      if (GTK_WIDGET_VISIBLE (notebook->cur_page->child))
	{
	  if (GTK_IS_CONTAINER (notebook->cur_page->child))
	    {
	      if (gtk_container_focus 
		  (GTK_CONTAINER (notebook->cur_page->child), direction))
		return TRUE;
	    }
	  else if (GTK_WIDGET_CAN_FOCUS (notebook->cur_page->child))
	    {
	      if (!focus_child)
		{
		  gtk_widget_grab_focus (notebook->cur_page->child);
		  return TRUE;
		}
	    }
	}
      return FALSE;
    }

  if (notebook->focus_tab)
    old_page = notebook->focus_tab->data;

  return_val = FALSE;

  if (focus_child && old_page && focus_child == old_page->child &&
      notebook->child_has_focus)
    {
      if (GTK_WIDGET_VISIBLE (old_page->child))
	{
	  if (GTK_IS_CONTAINER (old_page->child) &&
	      !GTK_WIDGET_HAS_FOCUS (old_page->child))
	    {
	      if (gtk_container_focus (GTK_CONTAINER (old_page->child),
				       direction))
		return TRUE;
	    }
	  gtk_widget_grab_focus (GTK_WIDGET(notebook));
	  return TRUE;
	}
      return FALSE;
    }
  
  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_RIGHT:
    case GTK_DIR_DOWN:
      if (!notebook->focus_tab)
	notebook->focus_tab = notebook->children;
      else
	notebook->focus_tab = notebook->focus_tab->next;
      
      if (!notebook->focus_tab)
	{
	  gtk_notebook_focus_changed (notebook, old_page);
	  return FALSE;
	}

      page = notebook->focus_tab->data;
      return_val = TRUE;
      break;
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_LEFT:
    case GTK_DIR_UP:
      if (!notebook->focus_tab)
	notebook->focus_tab = g_list_last (notebook->children);
      else
	notebook->focus_tab = notebook->focus_tab->prev;
      
      if (!notebook->focus_tab)
	{
	  gtk_notebook_focus_changed (notebook, old_page);
	  return FALSE;
	}
      
      page = notebook->focus_tab->data;
      return_val = TRUE;
      break;
    }

  if (return_val)
    {
      if (!GTK_WIDGET_HAS_FOCUS (container) )
	gtk_widget_grab_focus (GTK_WIDGET (container));

      if (GTK_WIDGET_MAPPED (page->tab_label))
	gtk_notebook_focus_changed (notebook, old_page);
      else
	{
	  gtk_notebook_pages_allocate (notebook, 
				       &(GTK_WIDGET (notebook)->allocation));
	  gtk_notebook_expose_tabs (notebook);
	}
    }

  return return_val;
}

static gint
gtk_notebook_page_select (GtkNotebook *notebook)
{
  g_return_val_if_fail (notebook != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  if (notebook->focus_tab)
    {
      GtkNotebookPage *page;
      GList *children;
      gint num;

      page = notebook->focus_tab->data;

      children = notebook->children;
      num = 0;
      while (children != notebook->focus_tab)
	{
	  children = children->next;
	  num++;
	}

      gtk_notebook_switch_page (notebook, page, num);

     if (GTK_WIDGET_VISIBLE (page->child))
       {
	 if (GTK_IS_CONTAINER (page->child))
	   {
	     if (gtk_container_focus (GTK_CONTAINER (page->child), 
				      GTK_DIR_TAB_FORWARD))
	       return TRUE;
	   }
	 else if (GTK_WIDGET_CAN_FOCUS (page->child))
	   {
	     gtk_widget_grab_focus (page->child);
	     return TRUE;
	   }
       }
    }
  return FALSE;
}

static void
gtk_notebook_switch_focus_tab (GtkNotebook *notebook, 
			       GList       *new_child)
{
  GtkNotebookPage *old_page = NULL;
  GtkNotebookPage *page;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->focus_tab != new_child)
    {
      if (notebook->focus_tab)
	old_page = notebook->focus_tab->data;

      notebook->focus_tab = new_child;
      page = notebook->focus_tab->data;
      if (GTK_WIDGET_MAPPED (page->tab_label))
	gtk_notebook_focus_changed (notebook, old_page);
      else
	{
	  gtk_notebook_pages_allocate (notebook, 
				       &(GTK_WIDGET (notebook)->allocation));
	  gtk_notebook_expose_tabs (notebook);
	}
    }
}

static gint
gtk_notebook_key_press (GtkWidget   *widget,
			GdkEventKey *event)
{
  GtkNotebook *notebook;
  GtkDirectionType direction = 0;
  gint return_val;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  notebook = GTK_NOTEBOOK (widget);
  return_val = TRUE;

  if (!notebook->children || !notebook->show_tabs)
    return FALSE;

  switch (event->keyval)
    {
    case GDK_Up:
      direction = GTK_DIR_UP;
      break;
    case GDK_Left:
      direction = GTK_DIR_LEFT;
      break;
    case GDK_Down:
      direction = GTK_DIR_DOWN;
      break;
    case GDK_Right:
      direction = GTK_DIR_RIGHT;
      break;
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
      if (event->state & GDK_SHIFT_MASK)
	direction = GTK_DIR_TAB_BACKWARD;
      else
	direction = GTK_DIR_TAB_FORWARD;
      break;
    case GDK_Home:
      gtk_notebook_switch_focus_tab (notebook, notebook->children);
      return TRUE;
    case GDK_End:
      gtk_notebook_switch_focus_tab (notebook, 
				     g_list_last (notebook->children));
      return TRUE;
    case GDK_Return:
    case GDK_space:
      gtk_notebook_page_select (GTK_NOTEBOOK (widget));
      return TRUE;
    default:
      return_val = FALSE;
    }
  if (return_val)
    return gtk_container_focus (GTK_CONTAINER (widget), direction);
  return return_val;
}

void
gtk_notebook_set_tab_border (GtkNotebook *notebook,
			     gint         tab_border)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->tab_border != tab_border && tab_border > 0)
    {
      notebook->tab_border = tab_border;

      if (GTK_WIDGET_VISIBLE (notebook) && notebook->show_tabs)
	gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }
}

static void
gtk_notebook_update_labels (GtkNotebook *notebook,
			    GList       *list,
			    gint        page_num)
{
  GtkNotebookPage *page;
  gchar string[32];

  while (list)
    {
      page = list->data;
      list = list->next;
      sprintf (string, "Page %d", page_num);
      if (notebook->show_tabs && page->default_tab)
	gtk_label_set (GTK_LABEL (page->tab_label), string);
      if (notebook->menu && page->default_menu)
	{
	  if (GTK_IS_LABEL (page->tab_label))
	    gtk_label_set (GTK_LABEL (page->menu_label), GTK_LABEL (page->tab_label)->label);
	  else
	    gtk_label_set (GTK_LABEL (page->menu_label), string);
	}
      page_num++;
    }  
}

static void
gtk_notebook_menu_item_create (GtkNotebook     *notebook, 
			       GtkNotebookPage *page,
			       gint             position)
{	       
  GtkWidget *menu_item;

  if (page->default_menu)
    {
      if (page->tab_label && GTK_IS_LABEL (page->tab_label))
	page->menu_label = gtk_label_new (GTK_LABEL (page->tab_label)->label);
      else
	page->menu_label = gtk_label_new ("");
      gtk_misc_set_alignment (GTK_MISC (page->menu_label), 0.0, 0.5);
    }
  gtk_widget_show (page->menu_label);
  menu_item = gtk_menu_item_new ();
  gtk_widget_freeze_accelerators (menu_item);
  gtk_container_add (GTK_CONTAINER (menu_item), page->menu_label);
  gtk_menu_insert (GTK_MENU (notebook->menu), menu_item, position);
  gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
		      GTK_SIGNAL_FUNC (gtk_notebook_menu_switch_page), 
		      page);
  gtk_widget_show (menu_item);
}

void
gtk_notebook_popup_enable (GtkNotebook *notebook)
{
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  
  if (notebook->menu)
    return;

  notebook->menu = gtk_menu_new ();
  
  children = notebook->children;
  while (children)
    {
      page = children->data;
      children = children->next;
      gtk_notebook_menu_item_create (notebook, page, -1);
    }
  gtk_notebook_update_labels (notebook, notebook->children,1);

  gtk_menu_attach_to_widget (GTK_MENU (notebook->menu), GTK_WIDGET (notebook),
			     gtk_notebook_menu_detacher);
}

static void
gtk_notebook_menu_label_unparent (GtkWidget *widget, 
				  gpointer  data)
{
  gtk_widget_unparent (GTK_BIN(widget)->child);
  GTK_BIN(widget)->child = NULL;
}

void       
gtk_notebook_popup_disable  (GtkNotebook *notebook)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (!notebook->menu)
    return;

  gtk_container_foreach (GTK_CONTAINER (notebook->menu),
			 (GtkCallback) gtk_notebook_menu_label_unparent, NULL);
  gtk_widget_destroy (notebook->menu);
}

static void
gtk_notebook_menu_detacher (GtkWidget *widget,
			    GtkMenu   *menu)
{
  GtkNotebook *notebook;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  notebook = GTK_NOTEBOOK (widget);
  g_return_if_fail (notebook->menu == (GtkWidget*) menu);

  notebook->menu = NULL;
}
