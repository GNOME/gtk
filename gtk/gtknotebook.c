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

#include "gtknotebook.h"
#include "gtksignal.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtklabel.h"
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include "gtkintl.h"


#define TAB_OVERLAP    2
#define TAB_CURVATURE  1
#define ARROW_SIZE     12
#define ARROW_SPACING  0
#define FOCUS_WIDTH    1
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

enum {
  ARG_0,
  ARG_TAB_POS,
  ARG_SHOW_TABS,
  ARG_SHOW_BORDER,
  ARG_SCROLLABLE,
  ARG_TAB_BORDER,
  ARG_TAB_HBORDER,
  ARG_TAB_VBORDER,
  ARG_PAGE,
  ARG_ENABLE_POPUP,
  ARG_HOMOGENEOUS
};

enum {
  CHILD_ARG_0,
  CHILD_ARG_TAB_LABEL,
  CHILD_ARG_MENU_LABEL,
  CHILD_ARG_POSITION,
  CHILD_ARG_TAB_EXPAND,
  CHILD_ARG_TAB_FILL,
  CHILD_ARG_TAB_PACK
};

/** GtkNotebook Methods **/
static void gtk_notebook_class_init          (GtkNotebookClass *klass);
static void gtk_notebook_init                (GtkNotebook      *notebook);

/** GtkObject Methods **/
static void gtk_notebook_destroy             (GtkObject        *object);
static void gtk_notebook_set_arg             (GtkObject        *object,
				              GtkArg           *arg,
				              guint             arg_id);
static void gtk_notebook_get_arg	     (GtkObject        *object,
					      GtkArg           *arg,
					      guint             arg_id);

/** GtkWidget Methods **/
static void gtk_notebook_map                 (GtkWidget        *widget);
static void gtk_notebook_unmap               (GtkWidget        *widget);
static void gtk_notebook_realize             (GtkWidget        *widget);
static void gtk_notebook_unrealize           (GtkWidget        *widget);
static void gtk_notebook_size_request        (GtkWidget        *widget,
					      GtkRequisition   *requisition);
static void gtk_notebook_size_allocate       (GtkWidget        *widget,
					      GtkAllocation    *allocation);
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
static gint gtk_notebook_focus_in            (GtkWidget        *widget,
					      GdkEventFocus    *event);
static gint gtk_notebook_focus_out           (GtkWidget        *widget,
					      GdkEventFocus    *event);
static void gtk_notebook_draw_focus          (GtkWidget        *widget);
static void gtk_notebook_style_set           (GtkWidget        *widget,
					      GtkStyle         *previous_style);

/** GtkContainer Methods **/
static void gtk_notebook_set_child_arg	     (GtkContainer     *container,
					      GtkWidget        *child,
					      GtkArg           *arg,
					      guint             arg_id);
static void gtk_notebook_get_child_arg	     (GtkContainer     *container,
					      GtkWidget        *child,
					      GtkArg           *arg,
					      guint             arg_id);
static void gtk_notebook_add                 (GtkContainer     *container,
					      GtkWidget        *widget);
static void gtk_notebook_remove              (GtkContainer     *container,
					      GtkWidget        *widget);
static gint gtk_notebook_focus               (GtkContainer     *container,
					      GtkDirectionType  direction);
static void gtk_notebook_set_focus_child     (GtkContainer     *container,
					      GtkWidget        *child);
static GtkType gtk_notebook_child_type       (GtkContainer     *container);
static void gtk_notebook_forall              (GtkContainer     *container,
					      gboolean		include_internals,
					      GtkCallback       callback,
					      gpointer          callback_data);

/** GtkNotebook Private Functions **/
static void gtk_notebook_panel_realize       (GtkNotebook      *notebook);
static void gtk_notebook_expose_tabs         (GtkNotebook      *notebook);
static void gtk_notebook_focus_changed       (GtkNotebook      *notebook,
					      GtkNotebookPage  *old_page);
static void gtk_notebook_real_remove         (GtkNotebook      *notebook,
					      GList            *list);
static void gtk_notebook_update_labels       (GtkNotebook      *notebook);
static gint gtk_notebook_timer               (GtkNotebook      *notebook);
static gint gtk_notebook_page_compare        (gconstpointer     a,
					      gconstpointer     b);
static gint  gtk_notebook_real_page_position (GtkNotebook      *notebook,
					      GList            *list);
static GList * gtk_notebook_search_page      (GtkNotebook      *notebook,
					      GList            *list,
					      gint              direction,
					      gboolean          find_visible);

/** GtkNotebook Drawing Functions **/
static void gtk_notebook_paint               (GtkWidget        *widget,
					      GdkRectangle     *area);
static void gtk_notebook_draw_tab            (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      GdkRectangle     *area);
static void gtk_notebook_draw_arrow          (GtkNotebook      *notebook,
					      guint             arrow);
static void gtk_notebook_set_shape           (GtkNotebook      *notebook);

/** GtkNotebook Size Allocate Functions **/
static void gtk_notebook_pages_allocate      (GtkNotebook      *notebook,
					      GtkAllocation    *allocation);
static void gtk_notebook_page_allocate       (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      GtkAllocation    *allocation);
static void gtk_notebook_calc_tabs           (GtkNotebook      *notebook,
			                      GList            *start,
					      GList           **end,
					      gint             *tab_space,
					      guint             direction);

/** GtkNotebook Page Switch Methods **/
static void gtk_notebook_real_switch_page    (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      guint             page_num);

/** GtkNotebook Page Switch Functions **/
static void gtk_notebook_switch_page         (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      gint              page_num);
static gint gtk_notebook_page_select         (GtkNotebook      *notebook);
static void gtk_notebook_switch_focus_tab    (GtkNotebook      *notebook,
                                              GList            *new_child);
static void gtk_notebook_menu_switch_page    (GtkWidget        *widget,
					      GtkNotebookPage  *page);

/** GtkNotebook Menu Functions **/
static void gtk_notebook_menu_item_create    (GtkNotebook      *notebook,
					      GList            *list);
static void gtk_notebook_menu_label_unparent (GtkWidget        *widget,
					      gpointer          data);
static void gtk_notebook_menu_detacher       (GtkWidget        *widget,
					      GtkMenu          *menu);


static GtkContainerClass *parent_class = NULL;
static guint notebook_signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_notebook_get_type (void)
{
  static GtkType notebook_type = 0;

  if (!notebook_type)
    {
      static const GtkTypeInfo notebook_info =
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

  gtk_object_add_arg_type ("GtkNotebook::page", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_PAGE);
  gtk_object_add_arg_type ("GtkNotebook::tab_pos", GTK_TYPE_POSITION_TYPE, GTK_ARG_READWRITE, ARG_TAB_POS);
  gtk_object_add_arg_type ("GtkNotebook::tab_border", GTK_TYPE_UINT, GTK_ARG_WRITABLE, ARG_TAB_BORDER);
  gtk_object_add_arg_type ("GtkNotebook::tab_hborder", GTK_TYPE_UINT, GTK_ARG_READWRITE, ARG_TAB_HBORDER);
  gtk_object_add_arg_type ("GtkNotebook::tab_vborder", GTK_TYPE_UINT, GTK_ARG_READWRITE, ARG_TAB_VBORDER);
  gtk_object_add_arg_type ("GtkNotebook::show_tabs", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_SHOW_TABS);
  gtk_object_add_arg_type ("GtkNotebook::show_border", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_SHOW_BORDER);
  gtk_object_add_arg_type ("GtkNotebook::scrollable", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_SCROLLABLE);
  gtk_object_add_arg_type ("GtkNotebook::enable_popup", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ENABLE_POPUP);
  gtk_object_add_arg_type ("GtkNotebook::homogeneous", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HOMOGENEOUS);

  gtk_container_add_child_arg_type ("GtkNotebook::tab_label", GTK_TYPE_STRING, GTK_ARG_READWRITE, CHILD_ARG_TAB_LABEL);
  gtk_container_add_child_arg_type ("GtkNotebook::menu_label", GTK_TYPE_STRING, GTK_ARG_READWRITE, CHILD_ARG_MENU_LABEL);
  gtk_container_add_child_arg_type ("GtkNotebook::position", GTK_TYPE_INT, GTK_ARG_READWRITE, CHILD_ARG_POSITION);
  gtk_container_add_child_arg_type ("GtkNotebook::tab_fill", GTK_TYPE_BOOL, GTK_ARG_READWRITE, CHILD_ARG_TAB_FILL);
  gtk_container_add_child_arg_type ("GtkNotebook::tab_pack", GTK_TYPE_BOOL, GTK_ARG_READWRITE, CHILD_ARG_TAB_PACK);

  notebook_signals[SWITCH_PAGE] =
    gtk_signal_new ("switch_page",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkNotebookClass, switch_page),
		    gtk_marshal_NONE__POINTER_UINT,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_UINT);

  gtk_object_class_add_signals (object_class, notebook_signals, LAST_SIGNAL);

  object_class->set_arg = gtk_notebook_set_arg;
  object_class->get_arg = gtk_notebook_get_arg;
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
  widget_class->button_release_event = gtk_notebook_button_release;
  widget_class->enter_notify_event = gtk_notebook_enter_notify;
  widget_class->leave_notify_event = gtk_notebook_leave_notify;
  widget_class->motion_notify_event = gtk_notebook_motion_notify;
  widget_class->key_press_event = gtk_notebook_key_press;
  widget_class->focus_in_event = gtk_notebook_focus_in;
  widget_class->focus_out_event = gtk_notebook_focus_out;
  widget_class->draw_focus = gtk_notebook_draw_focus;
  widget_class->style_set = gtk_notebook_style_set;
   
  container_class->add = gtk_notebook_add;
  container_class->remove = gtk_notebook_remove;
  container_class->forall = gtk_notebook_forall;
  container_class->focus = gtk_notebook_focus;
  container_class->set_focus_child = gtk_notebook_set_focus_child;
  container_class->get_child_arg = gtk_notebook_get_child_arg;
  container_class->set_child_arg = gtk_notebook_set_child_arg;
  container_class->child_type = gtk_notebook_child_type;

  class->switch_page = gtk_notebook_real_switch_page;
}

static void
gtk_notebook_init (GtkNotebook *notebook)
{
  GTK_WIDGET_SET_FLAGS (notebook, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS (notebook, GTK_NO_WINDOW);

  notebook->cur_page = NULL;
  notebook->children = NULL;
  notebook->first_tab = NULL;
  notebook->focus_tab = NULL;
  notebook->panel = NULL;
  notebook->menu = NULL;

  notebook->tab_hborder = 2;
  notebook->tab_vborder = 2;

  notebook->show_tabs = TRUE;
  notebook->show_border = TRUE;
  notebook->tab_pos = GTK_POS_TOP;
  notebook->scrollable = FALSE;
  notebook->in_child = 0;
  notebook->click_child = 0;
  notebook->button = 0;
  notebook->need_timer = 0;
  notebook->child_has_focus = FALSE;
  notebook->have_visible_child = FALSE;
}

GtkWidget*
gtk_notebook_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_notebook_get_type ()));
}

/* Private GtkObject Methods :
 * 
 * gtk_notebook_destroy
 * gtk_notebook_set_arg
 * gtk_notebook_get_arg
 */
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

static void
gtk_notebook_set_arg (GtkObject *object,
		      GtkArg    *arg,
		      guint      arg_id)
{
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (object);

  switch (arg_id)
    {
    case ARG_SHOW_TABS:
      gtk_notebook_set_show_tabs (notebook, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_SHOW_BORDER:
      gtk_notebook_set_show_border (notebook, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_SCROLLABLE:
      gtk_notebook_set_scrollable (notebook, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_ENABLE_POPUP:
      if (GTK_VALUE_BOOL (*arg))
	gtk_notebook_popup_enable (notebook);
      else
	gtk_notebook_popup_disable (notebook);
      break;
    case ARG_HOMOGENEOUS:
      gtk_notebook_set_homogeneous_tabs (notebook, GTK_VALUE_BOOL (*arg));
      break;  
    case ARG_PAGE:
      gtk_notebook_set_page (notebook, GTK_VALUE_INT (*arg));
      break;
    case ARG_TAB_POS:
      gtk_notebook_set_tab_pos (notebook, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_TAB_BORDER:
      gtk_notebook_set_tab_border (notebook, GTK_VALUE_UINT (*arg));
      break;
    case ARG_TAB_HBORDER:
      gtk_notebook_set_tab_hborder (notebook, GTK_VALUE_UINT (*arg));
      break;
    case ARG_TAB_VBORDER:
      gtk_notebook_set_tab_vborder (notebook, GTK_VALUE_UINT (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_notebook_get_arg (GtkObject *object,
		      GtkArg    *arg,
		      guint      arg_id)
{
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (object);

  switch (arg_id)
    {
    case ARG_SHOW_TABS:
      GTK_VALUE_BOOL (*arg) = notebook->show_tabs;
      break;
    case ARG_SHOW_BORDER:
      GTK_VALUE_BOOL (*arg) = notebook->show_border;
      break;
    case ARG_SCROLLABLE:
      GTK_VALUE_BOOL (*arg) = notebook->scrollable;
      break;
    case ARG_ENABLE_POPUP:
      GTK_VALUE_BOOL (*arg) = notebook->menu != NULL;
      break;
    case ARG_HOMOGENEOUS:
      GTK_VALUE_BOOL (*arg) = notebook->homogeneous;
      break;
    case ARG_PAGE:
      GTK_VALUE_INT (*arg) = gtk_notebook_get_current_page (notebook);
      break;
    case ARG_TAB_POS:
      GTK_VALUE_ENUM (*arg) = notebook->tab_pos;
      break;
    case ARG_TAB_HBORDER:
      GTK_VALUE_UINT (*arg) = notebook->tab_hborder;
      break;
    case ARG_TAB_VBORDER:
      GTK_VALUE_UINT (*arg) = notebook->tab_vborder;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

/* Private GtkWidget Methods :
 * 
 * gtk_notebook_map
 * gtk_notebook_unmap
 * gtk_notebook_realize
 * gtk_notebook_size_request
 * gtk_notebook_size_allocate
 * gtk_notebook_draw
 * gtk_notebook_expose
 * gtk_notebook_button_press
 * gtk_notebook_button_release
 * gtk_notebook_enter_notify
 * gtk_notebook_leave_notify
 * gtk_notebook_motion_notify
 * gtk_notebook_key_press
 * gtk_notebook_focus_in
 * gtk_notebook_focus_out
 * gtk_notebook_draw_focus
 * gtk_notebook_style_set
 */
static void
gtk_notebook_map (GtkWidget *widget)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

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
	      GTK_WIDGET_VISIBLE (page->tab_label) &&
	      !GTK_WIDGET_MAPPED (page->tab_label))
	    gtk_widget_map (page->tab_label);
	}
    }

  gdk_window_show (widget->window);
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
  attributes.event_mask |= (GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, notebook);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
  if (notebook->scrollable)
    gtk_notebook_panel_realize (notebook);
}

static void
gtk_notebook_unrealize (GtkWidget *widget)
{
  GtkNotebook *notebook;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  notebook = GTK_NOTEBOOK (widget);

  if (notebook->panel)
    {
      gdk_window_set_user_data (notebook->panel, NULL);
      gdk_window_destroy (notebook->panel);
      notebook->panel = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_notebook_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  GtkRequisition child_requisition;
  gboolean switch_page = FALSE;
  gint vis_pages;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (requisition != NULL);

  notebook = GTK_NOTEBOOK (widget);
  widget->requisition.width = 0;
  widget->requisition.height = 0;

  for (children = notebook->children, vis_pages = 0; children;
       children = children->next)
    {
      page = children->data;

      if (GTK_WIDGET_VISIBLE (page->child))
	{
	  vis_pages++;
	  gtk_widget_size_request (page->child, &child_requisition);
	  
	  widget->requisition.width = MAX (widget->requisition.width,
					   child_requisition.width);
	  widget->requisition.height = MAX (widget->requisition.height,
					    child_requisition.height);

	  if (GTK_WIDGET_MAPPED (page->child) && page != notebook->cur_page)
	    gtk_widget_unmap (page->child);
	  if (notebook->menu && page->menu_label->parent &&
	      !GTK_WIDGET_VISIBLE (page->menu_label->parent))
	    gtk_widget_show (page->menu_label->parent);
	}
      else
	{
	  if (page == notebook->cur_page)
	    switch_page = TRUE;
	  if (notebook->menu && page->menu_label->parent &&
	      GTK_WIDGET_VISIBLE (page->menu_label->parent))
	    gtk_widget_hide (page->menu_label->parent);
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
	  gint padding;
	  
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  if (!GTK_WIDGET_VISIBLE (page->tab_label))
		    gtk_widget_show (page->tab_label);

		  gtk_widget_size_request (page->tab_label,
					   &child_requisition);

		  page->requisition.width = 
		    child_requisition.width +
		    2 * widget->style->klass->xthickness;
		  page->requisition.height = 
		    child_requisition.height +
		    2 * widget->style->klass->ythickness;
		  
		  switch (notebook->tab_pos)
		    {
		    case GTK_POS_TOP:
		    case GTK_POS_BOTTOM:
		      page->requisition.height += 2 * (notebook->tab_vborder +
						       FOCUS_WIDTH);
		      tab_height = MAX (tab_height, page->requisition.height);
		      tab_max = MAX (tab_max, page->requisition.width);
		      break;
		    case GTK_POS_LEFT:
		    case GTK_POS_RIGHT:
		      page->requisition.width += 2 * (notebook->tab_hborder +
						      FOCUS_WIDTH);
		      tab_width = MAX (tab_width, page->requisition.width);
		      tab_max = MAX (tab_max, page->requisition.height);
		      break;
		    }
		}
	      else if (GTK_WIDGET_VISIBLE (page->tab_label))
		gtk_widget_hide (page->tab_label);
	    }

	  children = notebook->children;

	  if (vis_pages)
	    {
	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		  if (tab_height == 0)
		    break;

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.width < tab_width)
		    tab_height = MAX (tab_height, ARROW_SIZE);

		  padding = 2 * (TAB_CURVATURE + FOCUS_WIDTH +
				 notebook->tab_hborder) - TAB_OVERLAP;
		  tab_max += padding;
		  while (children)
		    {
		      page = children->data;
		      children = children->next;
		  
		      if (!GTK_WIDGET_VISIBLE (page->child))
			continue;

		      if (notebook->homogeneous)
			page->requisition.width = tab_max;
		      else
			page->requisition.width += padding;

		      tab_width += page->requisition.width;
		      page->requisition.height = tab_height;
		    }

		  if (notebook->scrollable && vis_pages > 1 &&
		      widget->requisition.width < tab_width)
		    tab_width = tab_max + 2 * (ARROW_SIZE + ARROW_SPACING);

                  if (notebook->homogeneous && !notebook->scrollable)
                    widget->requisition.width = MAX (widget->requisition.width,
                                                     vis_pages * tab_max +
                                                     TAB_OVERLAP);
                  else
                    widget->requisition.width = MAX (widget->requisition.width,
                                                     tab_width + TAB_OVERLAP);

		  widget->requisition.height += tab_height;
		  break;
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		  if (tab_width == 0)
		    break;

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.height < tab_height)
		    tab_width = MAX (tab_width, ARROW_SPACING +2 * ARROW_SIZE);

		  padding = 2 * (TAB_CURVATURE + FOCUS_WIDTH +
				 notebook->tab_vborder) - TAB_OVERLAP;
		  tab_max += padding;

		  while (children)
		    {
		      page = children->data;
		      children = children->next;

		      if (!GTK_WIDGET_VISIBLE (page->child))
			continue;

		      page->requisition.width   = tab_width;

		      if (notebook->homogeneous)
			page->requisition.height = tab_max;
		      else
			page->requisition.height += padding;

		      tab_height += page->requisition.height;
		    }

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.height < tab_height)
		    tab_height = tab_max + ARROW_SIZE + ARROW_SPACING;

		  widget->requisition.width += tab_width;

                  if (notebook->homogeneous && !notebook->scrollable)
                    widget->requisition.height =
		      MAX (widget->requisition.height,
			   vis_pages * tab_max + TAB_OVERLAP);
                  else
                    widget->requisition.height =
		      MAX (widget->requisition.height,
			   tab_height + TAB_OVERLAP);

		  if (!notebook->homogeneous || notebook->scrollable)
		    vis_pages = 1;
		  widget->requisition.height = MAX (widget->requisition.height,
						    vis_pages * tab_max +
						    TAB_OVERLAP);
		  break;
		}
	    }
	}
      else
	{
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      
	      if (page->tab_label && GTK_WIDGET_VISIBLE (page->tab_label))
		gtk_widget_hide (page->tab_label);
	    }
	}
    }

  widget->requisition.width += GTK_CONTAINER (widget)->border_width * 2;
  widget->requisition.height += GTK_CONTAINER (widget)->border_width * 2;

  if (switch_page)
    {
      if (vis_pages)
	{
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  gtk_notebook_switch_page (notebook, page, -1);
		  break;
		}
	    }
	}
      else if (GTK_WIDGET_VISIBLE (widget))
	{
	  widget->requisition.width = GTK_CONTAINER (widget)->border_width * 2;
	  widget->requisition.height= GTK_CONTAINER (widget)->border_width * 2;
	}
    }
  if (vis_pages && !notebook->cur_page)
    {
      children = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);
      if (children)
	{
	  notebook->first_tab = children;
	  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (children),-1);
	}
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
      child_allocation.width = MAX (1, (gint)allocation->width - child_allocation.x * 2);
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);

      if (notebook->show_tabs || notebook->show_border)
	{
	  child_allocation.x += widget->style->klass->xthickness;
	  child_allocation.y += widget->style->klass->ythickness;
	  child_allocation.width = MAX (1, (gint)child_allocation.width -
					(gint) widget->style->klass->xthickness * 2);
	  child_allocation.height = MAX (1, (gint)child_allocation.height -
					 (gint) widget->style->klass->ythickness * 2);

	  if (notebook->show_tabs && notebook->children && notebook->cur_page)
	    {
	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		  child_allocation.y += notebook->cur_page->requisition.height;
		case GTK_POS_BOTTOM:
		  child_allocation.height =
		    MAX (1, (gint)child_allocation.height -
			 (gint)notebook->cur_page->requisition.height);
		  break;
		case GTK_POS_LEFT:
		  child_allocation.x += notebook->cur_page->requisition.width;
		case GTK_POS_RIGHT:
		  child_allocation.width =
		    MAX (1, (gint)child_allocation.width -
			 (gint)notebook->cur_page->requisition.width);
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
   gtk_notebook_set_shape (notebook);
}

static void
gtk_notebook_draw (GtkWidget    *widget,
		   GdkRectangle *area)
{
  GtkNotebook *notebook;
  GdkRectangle child_area;
  GdkRectangle draw_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (area != NULL);

  notebook = GTK_NOTEBOOK (widget);

  draw_area = *area;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gboolean have_visible_child;

      have_visible_child = notebook->cur_page && GTK_WIDGET_VISIBLE (notebook->cur_page->child);

      if (have_visible_child != notebook->have_visible_child)
	{
	  notebook->have_visible_child = have_visible_child;
	  draw_area.x = 0;
	  draw_area.y = 0;
	  draw_area.width = widget->allocation.width;
	  draw_area.height = widget->allocation.height;
	}

      gtk_notebook_paint (widget, &draw_area);

      gtk_widget_draw_focus (widget);

      if (notebook->cur_page && GTK_WIDGET_VISIBLE (notebook->cur_page->child) &&
	  gtk_widget_intersect (notebook->cur_page->child, &draw_area, &child_area))
	gtk_widget_draw (notebook->cur_page->child, &child_area);
    }
  else
    notebook->have_visible_child = FALSE;
}

static gint
gtk_notebook_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  GtkNotebook *notebook;
  GdkEventExpose child_event;
  GdkRectangle child_area;
   
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      notebook = GTK_NOTEBOOK (widget);

      gtk_notebook_paint (widget, &event->area);
      if (notebook->show_tabs)
	{
	  if (notebook->cur_page && 
	      gtk_widget_intersect (notebook->cur_page->tab_label, 
				    &event->area, &child_area))
	    gtk_widget_draw_focus (widget);
	}

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

  if (event->type != GDK_BUTTON_PRESS || !notebook->children ||
      notebook->button)
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
	      if (!notebook->focus_tab ||
		  gtk_notebook_search_page (notebook, notebook->focus_tab,
					    STEP_PREV, TRUE))
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
	    gtk_notebook_switch_focus_tab (notebook,
					   gtk_notebook_search_page (notebook,
								     NULL,
								     STEP_NEXT,
								     TRUE));
	  gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
	}
      else
	{
	  notebook->click_child = GTK_ARROW_RIGHT;
	  if (event->button == 1)
	    {
	      if (!notebook->focus_tab ||
		  gtk_notebook_search_page (notebook, notebook->focus_tab,
					    STEP_NEXT, TRUE))
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
	    gtk_notebook_switch_focus_tab
	      (notebook, gtk_notebook_search_page (notebook, NULL,
						   STEP_PREV, TRUE));
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
		  gtk_notebook_switch_focus_tab (notebook, children);
		  gtk_notebook_focus_changed (notebook, old_page);
		}
	      else
		{
		  gtk_notebook_switch_focus_tab (notebook, children);
		  gtk_widget_grab_focus (widget);
		  gtk_notebook_switch_page (notebook, page, num);
		}
	      break;
	    }
	  children = children->next;
	  num++;
	}
      if (!children && !GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);
    }
   gtk_notebook_set_shape (notebook);
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
gtk_notebook_key_press (GtkWidget   *widget,
			GdkEventKey *event)
{
  GtkNotebook *notebook;
  GtkDirectionType direction = 0;
  GList *list;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  notebook = GTK_NOTEBOOK (widget);

  if (!notebook->children || !notebook->show_tabs)
    return FALSE;

  switch (event->keyval)
    {
    case GDK_Up:
      switch (notebook->tab_pos)
	{
	case GTK_POS_BOTTOM:
	  gtk_notebook_page_select (GTK_NOTEBOOK (widget));
	  return TRUE;
	case GTK_POS_TOP:
	  return FALSE;
	default:
	  direction = GTK_DIR_UP;
	  break;
	}
      break;
    case GDK_Left:
      switch (notebook->tab_pos)
	{
	case GTK_POS_RIGHT:
	  gtk_notebook_page_select (GTK_NOTEBOOK (widget));
	  return TRUE;
	case GTK_POS_LEFT:
	  return FALSE;
	default:
	  direction = GTK_DIR_LEFT;
	  break;
	}
      break;
    case GDK_Down:
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  gtk_notebook_page_select (GTK_NOTEBOOK (widget));
	  return TRUE;
	case GTK_POS_BOTTOM:
	  return FALSE;
	default:
	  direction = GTK_DIR_DOWN;
	  break;
	}
      break;
    case GDK_Right:
      switch (notebook->tab_pos)
	{
	case GTK_POS_LEFT:
	  gtk_notebook_page_select (GTK_NOTEBOOK (widget));
	  return TRUE;
	case GTK_POS_RIGHT:
	  return FALSE;
	default:
	  direction = GTK_DIR_RIGHT;
	  break;
	}
      break;
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
      if (event->state & GDK_SHIFT_MASK)
	direction = GTK_DIR_TAB_BACKWARD;
      else
	direction = GTK_DIR_TAB_FORWARD;
      break;
    case GDK_Home:
      list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);
      if (list)
	gtk_notebook_switch_focus_tab (notebook, list);
      return TRUE;
    case GDK_End:
      list = gtk_notebook_search_page (notebook, NULL, STEP_PREV, TRUE);
      if (list)
	gtk_notebook_switch_focus_tab (notebook, list);
      return TRUE;
    case GDK_Return:
    case GDK_space:
      gtk_notebook_page_select (GTK_NOTEBOOK (widget));
      return TRUE;
    default:
      return FALSE;
    }
  return gtk_container_focus (GTK_CONTAINER (widget), direction);
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
      GdkRectangle area;

      page = notebook->focus_tab->data;

      area.x = page->tab_label->allocation.x - 1;
      area.y = page->tab_label->allocation.y - 1;
      area.width = page->tab_label->allocation.width + 2;
      area.height = page->tab_label->allocation.height + 2;

      gtk_notebook_draw_tab (GTK_NOTEBOOK (widget), page, &area);
    }
}

static void
gtk_notebook_style_set (GtkWidget *widget,
			GtkStyle  *previous_style)
{
  if (GTK_WIDGET_REALIZED (widget) &&
      !GTK_WIDGET_NO_WINDOW (widget))
    {
      gtk_style_set_background (widget->style, widget->window, widget->state);
      if (GTK_WIDGET_DRAWABLE (widget))
	gdk_window_clear (widget->window);
    }

  gtk_notebook_set_shape (GTK_NOTEBOOK(widget));
}

/* Private GtkContainer Methods :
 * 
 * gtk_notebook_set_child_arg
 * gtk_notebook_get_child_arg
 * gtk_notebook_add
 * gtk_notebook_remove
 * gtk_notebook_focus
 * gtk_notebook_set_focus_child
 * gtk_notebook_child_type
 * gtk_notebook_forall
 */
static void
gtk_notebook_set_child_arg (GtkContainer     *container,
			    GtkWidget        *child,
			    GtkArg           *arg,
			    guint             arg_id)
{
  gboolean expand;
  gboolean fill;
  GtkPackType pack_type;

  switch (arg_id)
    {
    case CHILD_ARG_TAB_LABEL:
      /* a NULL pointer indicates a default_tab setting, otherwise
       * we need to set the associated label
       */
      gtk_notebook_set_tab_label_text (GTK_NOTEBOOK (container), child,
				       GTK_VALUE_STRING(*arg));
      break;
    case CHILD_ARG_MENU_LABEL:
      gtk_notebook_set_menu_label_text (GTK_NOTEBOOK (container), child,
					GTK_VALUE_STRING (*arg));
      break;
    case CHILD_ARG_POSITION:
      gtk_notebook_reorder_child (GTK_NOTEBOOK (container), child,
				  GTK_VALUE_INT (*arg));
      break;
    case CHILD_ARG_TAB_EXPAND:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  GTK_VALUE_BOOL (*arg),
					  fill, pack_type);
      break;
    case CHILD_ARG_TAB_FILL:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  expand,
					  GTK_VALUE_BOOL (*arg),
					  pack_type);
      break;
    case CHILD_ARG_TAB_PACK:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  expand, fill,
					  GTK_VALUE_BOOL (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_notebook_get_child_arg (GtkContainer     *container,
			    GtkWidget        *child,
			    GtkArg           *arg,
			    guint             arg_id)
{
  GList *list;
  GtkNotebook *notebook;
  GtkWidget *label;
  gboolean expand;
  gboolean fill;
  GtkPackType pack_type;

  notebook = GTK_NOTEBOOK (container);

  if (!(list = g_list_find_custom (notebook->children, child,
				   gtk_notebook_page_compare)))
    {
      arg->type = GTK_TYPE_INVALID;
      return;
    }

  switch (arg_id)
    {
    case CHILD_ARG_TAB_LABEL:
      label = gtk_notebook_get_tab_label (notebook, child);

      if (label && GTK_IS_LABEL (label))
	GTK_VALUE_STRING (*arg) = g_strdup (GTK_LABEL (label)->label);
      else
	GTK_VALUE_STRING (*arg) = NULL;
      break;
    case CHILD_ARG_MENU_LABEL:
      label = gtk_notebook_get_menu_label (notebook, child);

      if (label && GTK_IS_LABEL (label))
	GTK_VALUE_STRING (*arg) = g_strdup (GTK_LABEL (label)->label);
      else
	GTK_VALUE_STRING (*arg) = NULL;
      break;
    case CHILD_ARG_POSITION:
      GTK_VALUE_INT (*arg) = g_list_position (notebook->children, list);
      break;
    case CHILD_ARG_TAB_EXPAND:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      &expand, NULL, NULL);
	GTK_VALUE_BOOL (*arg) = expand;
      break;
    case CHILD_ARG_TAB_FILL:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      NULL, &fill, NULL);
	GTK_VALUE_BOOL (*arg) = fill;
      break;
    case CHILD_ARG_TAB_PACK:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      NULL, NULL, &pack_type);
	GTK_VALUE_BOOL (*arg) = pack_type;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_notebook_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (container));

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
  guint page_num;

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
	  gtk_notebook_real_remove (notebook, children);
	  break;
	}
      page_num++;
      children = children->next;
    }
}

static gint
gtk_notebook_focus (GtkContainer     *container,
		    GtkDirectionType  direction)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page = NULL;
  GtkNotebookPage *old_page = NULL;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (container), FALSE);

  notebook = GTK_NOTEBOOK (container);

  if (!GTK_WIDGET_DRAWABLE (notebook) ||
      !GTK_WIDGET_IS_SENSITIVE (container) ||
      !notebook->children ||
      !notebook->cur_page)
    return FALSE;

  if (!notebook->show_tabs)
    {
      if (GTK_WIDGET_DRAWABLE (notebook->cur_page->child) &&
	  GTK_WIDGET_IS_SENSITIVE (notebook->cur_page->child))
	{
	  if (GTK_IS_CONTAINER (notebook->cur_page->child))
	    {
	      if (gtk_container_focus
		  (GTK_CONTAINER (notebook->cur_page->child), direction))
		return TRUE;
	    }
	  else if (GTK_WIDGET_CAN_FOCUS (notebook->cur_page->child))
	    {
	      if (!container->focus_child)
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

  if (container->focus_child && old_page &&
      container->focus_child == old_page->child && notebook->child_has_focus)
    {
      if (GTK_WIDGET_DRAWABLE (container->focus_child))
	{
	  if (GTK_IS_CONTAINER (container->focus_child) &&
	      !GTK_WIDGET_HAS_FOCUS (container->focus_child))
	    {
	      if (gtk_container_focus (GTK_CONTAINER (container->focus_child),
				       direction))
		return TRUE;
	    }
	  gtk_widget_grab_focus (GTK_WIDGET(notebook));
	  return TRUE;
	}
      notebook->focus_tab = NULL;
      return FALSE;
    }

  if (!GTK_WIDGET_HAS_FOCUS (container))
    notebook->focus_tab = NULL;

  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_RIGHT:
    case GTK_DIR_DOWN:
      gtk_notebook_switch_focus_tab
	(notebook, gtk_notebook_search_page (notebook, notebook->focus_tab,
					     STEP_NEXT, TRUE));
      break;
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_LEFT:
    case GTK_DIR_UP:
      gtk_notebook_switch_focus_tab
	(notebook, gtk_notebook_search_page (notebook, notebook->focus_tab,
					     STEP_PREV, TRUE));
      break;
    }

  if (notebook->focus_tab)
    {
      if (!GTK_WIDGET_HAS_FOCUS (container))
	gtk_widget_grab_focus (GTK_WIDGET (container));

      page = notebook->focus_tab->data;
      if (GTK_WIDGET_MAPPED (page->tab_label))
	gtk_notebook_focus_changed (notebook, old_page);
      else
	{
	  gtk_notebook_pages_allocate (notebook, 
				       &(GTK_WIDGET (notebook)->allocation));
	  gtk_notebook_expose_tabs (notebook);
	}
      return TRUE;
    }

  gtk_notebook_focus_changed (notebook, old_page);
  return FALSE;
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
		gtk_notebook_switch_focus_tab (notebook, children);
	      children = children->next;
	    }
	}
    }
  parent_class->set_focus_child (container, child);
}

static void
gtk_notebook_forall (GtkContainer *container,
		     gboolean      include_internals,
		     GtkCallback   callback,
		     gpointer      callback_data)
{
  GtkNotebook *notebook;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (container));
  g_return_if_fail (callback != NULL);

  notebook = GTK_NOTEBOOK (container);

  children = notebook->children;
  while (children)
    {
      GtkNotebookPage *page;
      
      page = children->data;
      children = children->next;
      (* callback) (page->child, callback_data);
      if (include_internals)
	{
	  if (page->tab_label)
	    (* callback) (page->tab_label, callback_data);
	  if (page->menu_label)
	    (* callback) (page->menu_label, callback_data);
	}
    }
}

static GtkType
gtk_notebook_child_type (GtkContainer     *container)
{
  return GTK_TYPE_WIDGET;
}

/* Private GtkNotebook Functions:
 *
 * gtk_notebook_panel_realize
 * gtk_notebook_expose_tabs
 * gtk_notebook_focus_changed
 * gtk_notebook_real_remove
 * gtk_notebook_update_labels
 * gtk_notebook_timer
 * gtk_notebook_page_compare
 * gtk_notebook_real_page_position
 * gtk_notebook_search_page
 */
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

  attributes.x = (widget->allocation.width - attributes.width - 
		  GTK_CONTAINER (notebook)->border_width);
  attributes.y = (widget->allocation.height - ARROW_SIZE -
		  GTK_CONTAINER (notebook)->border_width);
  if (notebook->tab_pos == GTK_POS_TOP)
    attributes.y = GTK_CONTAINER (notebook)->border_width;
  else if (notebook->tab_pos == GTK_POS_LEFT)
    attributes.x = (widget->allocation.x +
		    GTK_CONTAINER (notebook)->border_width);

  notebook->panel = gdk_window_new (widget->window, &attributes, 
				    attributes_mask);
  gtk_style_set_background (widget->style, notebook->panel, 
			    GTK_STATE_NORMAL);
  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
  gdk_window_set_user_data (notebook->panel, widget);
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

  if (!GTK_WIDGET_MAPPED (notebook) || !notebook->first_tab)
    return;

  page = notebook->first_tab->data;

  event.type = GDK_EXPOSE;
  event.window = widget->window;
  event.count = 0;
  event.area.x = border;
  event.area.y = border;

  switch (notebook->tab_pos)
    {
    case GTK_POS_BOTTOM:
      event.area.y = (widget->allocation.height - border -
		      page->allocation.height -
		      widget->style->klass->ythickness);
      if (page != notebook->cur_page)
	event.area.y -= widget->style->klass->ythickness;
    case GTK_POS_TOP:
      event.area.width = widget->allocation.width - 2 * border;
      event.area.height = (page->allocation.height +
			   widget->style->klass->ythickness);
      if (page != notebook->cur_page)
	event.area.height += widget->style->klass->ythickness;
      break;
    case GTK_POS_RIGHT:
      event.area.x = (widget->allocation.width - border -
		      page->allocation.width -
		      widget->style->klass->xthickness);
      if (page != notebook->cur_page)
	event.area.x -= widget->style->klass->xthickness;
    case GTK_POS_LEFT:
      event.area.width = (page->allocation.width +
			  widget->style->klass->xthickness);
      event.area.height = widget->allocation.height - 2 * border;
      if (page != notebook->cur_page)
	event.area.width += widget->style->klass->xthickness;
      break;
    }	     
  gtk_widget_event (widget, (GdkEvent *) &event);
}

static void
gtk_notebook_focus_changed (GtkNotebook     *notebook,
                            GtkNotebookPage *old_page)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (GTK_WIDGET_DRAWABLE (notebook) && notebook->show_tabs) 
    {
      GdkRectangle area;

      if (notebook->focus_tab)
	{
	  GtkNotebookPage *page;

	  page = notebook->focus_tab->data;

	  area.x = page->tab_label->allocation.x - 1;
	  area.y = page->tab_label->allocation.y - 1;
	  area.width = page->tab_label->allocation.width + 2;
	  area.height = page->tab_label->allocation.height + 2;

	  gtk_notebook_draw_tab (notebook, page, &area);
	}

      if (old_page)
	{
	  area.x = old_page->tab_label->allocation.x - 1;
	  area.y = old_page->tab_label->allocation.y - 1;
	  area.width = old_page->tab_label->allocation.width + 2;
	  area.height = old_page->tab_label->allocation.height + 2;

	  gtk_notebook_draw_tab (notebook, old_page, &area);
	}
    }
}

static gint
gtk_notebook_timer (GtkNotebook *notebook)
{
  gboolean retval = FALSE;

  GDK_THREADS_ENTER ();
  
  if (notebook->timer)
    {
      if (notebook->click_child == GTK_ARROW_LEFT)
	{
	  if (!notebook->focus_tab ||
	      gtk_notebook_search_page (notebook, notebook->focus_tab,
					STEP_PREV, TRUE))
	    gtk_container_focus (GTK_CONTAINER (notebook), GTK_DIR_LEFT);
	}
      else if (notebook->click_child == GTK_ARROW_RIGHT)
	{
	  if (!notebook->focus_tab ||
	      gtk_notebook_search_page (notebook, notebook->focus_tab,
					STEP_NEXT, TRUE))
	    gtk_container_focus (GTK_CONTAINER (notebook), GTK_DIR_RIGHT);
	}
      if (notebook->need_timer) 
	{
	  notebook->need_timer = FALSE;
	  notebook->timer = gtk_timeout_add (NOTEBOOK_SCROLL_DELAY,
					     (GtkFunction) gtk_notebook_timer, 
					     (gpointer) notebook);
	}
      else
	retval = TRUE;
    }

  GDK_THREADS_LEAVE ();

  return retval;
}

static gint
gtk_notebook_page_compare (gconstpointer a,
			   gconstpointer b)
{
  return (((GtkNotebookPage *) a)->child != b);
}

static void
gtk_notebook_real_remove (GtkNotebook *notebook,
			  GList       *list)
{
  GtkNotebookPage *page;
  GList * next_list;
  gint need_resize = FALSE;

  next_list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);
  if (!next_list)
    next_list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);

  if (notebook->cur_page == list->data)
    { 
      notebook->cur_page = NULL;
      if (next_list)
	gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (next_list), -1);
    }

  if (list == notebook->first_tab)
    notebook->first_tab = next_list;
  if (list == notebook->focus_tab)
    gtk_notebook_switch_focus_tab (notebook, next_list);

  page = list->data;

  if (GTK_WIDGET_VISIBLE (page->child) && GTK_WIDGET_VISIBLE (notebook))
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
  
  notebook->children = g_list_remove_link (notebook->children, list);
  g_list_free (list);
  g_free (page);

  gtk_notebook_update_labels (notebook);
  if (need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

static void
gtk_notebook_update_labels (GtkNotebook *notebook)
{
  GtkNotebookPage *page;
  GList *list;
  gchar string[32];
  gint page_num = 1;

  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    {
      page = list->data;
      g_snprintf (string, sizeof(string), _("Page %u"), page_num++);
      if (notebook->show_tabs)
	{
	  if (page->default_tab)
	    {
	      if (!page->tab_label)
		{
		  page->tab_label = gtk_label_new (string);
		  gtk_widget_set_parent (page->tab_label,
					 GTK_WIDGET (notebook));
		}
	      else
		gtk_label_set_text (GTK_LABEL (page->tab_label), string);
	    }

	  if (GTK_WIDGET_VISIBLE (page->child) &&
	      !GTK_WIDGET_VISIBLE (page->tab_label))
	    gtk_widget_show (page->tab_label);
	  else if (!GTK_WIDGET_VISIBLE (page->child) &&
		   GTK_WIDGET_VISIBLE (page->tab_label))
	    gtk_widget_hide (page->tab_label);
	}
      if (notebook->menu && page->default_menu)
	{
	  if (page->tab_label && GTK_IS_LABEL (page->tab_label))
	    gtk_label_set_text (GTK_LABEL (page->menu_label),
			   GTK_LABEL (page->tab_label)->label);
	  else
	    gtk_label_set_text (GTK_LABEL (page->menu_label), string);
	}
    }  
}

static gint
gtk_notebook_real_page_position (GtkNotebook *notebook,
				 GList       *list)
{
  GList *work;
  gint count_start;

  g_return_val_if_fail (notebook != NULL, -1);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (list != NULL, -1);

  for (work = notebook->children, count_start = 0;
       work && work != list; work = work->next)
    if (GTK_NOTEBOOK_PAGE (work)->pack == GTK_PACK_START)
      count_start++;

  if (!work)
    return -1;

  if (GTK_NOTEBOOK_PAGE (list)->pack == GTK_PACK_START)
    return count_start;

  return (count_start + g_list_length (list) - 1);
}

static GList *
gtk_notebook_search_page (GtkNotebook *notebook,
			  GList       *list,
			  gint         direction,
			  gboolean     find_visible)
{
  GtkNotebookPage *page = NULL;
  GList *old_list = NULL;
  gint flag = 0;

  g_return_val_if_fail (notebook != NULL, NULL);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  switch (direction)
    {
    case STEP_PREV:
      flag = GTK_PACK_END;
      break;

    case STEP_NEXT:
      flag = GTK_PACK_START;
      break;
    }

  if (list)
    page = list->data;

  if (!page || page->pack == flag)
    {
      if (list)
	{
	  old_list = list;
	  list = list->next;
	}
      else
	list = notebook->children;

      while (list)
	{
	  page = list->data;
	  if (page->pack == flag &&
	      (!find_visible || GTK_WIDGET_VISIBLE (page->child)))
	    return list;
	  old_list = list;
	  list = list->next;
	}
      list = old_list;
    }
  else
    {
      old_list = list;
      list = list->prev;
    }
  while (list)
    {
      page = list->data;
      if (page->pack != flag &&
	  (!find_visible || GTK_WIDGET_VISIBLE (page->child)))
	return list;
      old_list = list;
      list = list->prev;
    }
  return NULL;
}

/* Private GtkNotebook Drawing Functions:
 *
 * gtk_notebook_paint
 * gtk_notebook_draw_tab
 * gtk_notebook_draw_arrow
 * gtk_notebook_set_shape
 */
static void
gtk_notebook_paint (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  gboolean showarrow;
  gint width, height;
  gint x, y;
  gint gap_x = 0, gap_width = 0;
   
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (area != NULL);

  if (!GTK_WIDGET_DRAWABLE (widget))
    return;

  notebook = GTK_NOTEBOOK (widget);

  if ((!notebook->show_tabs && !notebook->show_border) ||
      !notebook->cur_page || !GTK_WIDGET_VISIBLE (notebook->cur_page->child))
    {
      gdk_window_clear_area (widget->window,
			     area->x, area->y,
			     area->width, area->height);
      return;
    }

  x = GTK_CONTAINER (widget)->border_width;
  y = GTK_CONTAINER (widget)->border_width;
  width = widget->allocation.width - x * 2;
  height = widget->allocation.height - y * 2;

  if (notebook->show_border && (!notebook->show_tabs || !notebook->children))
    {
      gtk_paint_box (widget->style, widget->window,
		     GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		     area, widget, "notebook",
		     x, y, width, height);
      return;
    }


  if (!GTK_WIDGET_MAPPED (notebook->cur_page->tab_label))
    {
      page = notebook->first_tab->data;

      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  y += page->allocation.height + widget->style->klass->ythickness;
	case GTK_POS_BOTTOM:
	  height -= page->allocation.height + widget->style->klass->ythickness;
	  break;
	case GTK_POS_LEFT:
	  x += page->allocation.width + widget->style->klass->xthickness;
	case GTK_POS_RIGHT:
	  width -= page->allocation.width + widget->style->klass->xthickness;
	  break;
	}
      gtk_paint_box (widget->style, widget->window,
		     GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		     area, widget, "notebook",
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
	  gap_x = (notebook->cur_page->allocation.x -
		   GTK_CONTAINER(notebook)->border_width);
	  gap_width = notebook->cur_page->allocation.width;
	  break;
	case GTK_POS_BOTTOM:
	  gap_x = (notebook->cur_page->allocation.x -
		   GTK_CONTAINER(notebook)->border_width);
	  gap_width = notebook->cur_page->allocation.width;
	  break;
	case GTK_POS_LEFT:
	  gap_x = (notebook->cur_page->allocation.y -
		   GTK_CONTAINER(notebook)->border_width);
	  gap_width = notebook->cur_page->allocation.height;
	  break;
	case GTK_POS_RIGHT:
	  gap_x = (notebook->cur_page->allocation.y -
		   GTK_CONTAINER(notebook)->border_width);
	  gap_width = notebook->cur_page->allocation.height;
	  break;
	}
      gtk_paint_box_gap(widget->style, widget->window,
			GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			area, widget, "notebook",
			x, y, width, height,
			notebook->tab_pos, gap_x, gap_width);
    }

  showarrow = FALSE;
  children = gtk_notebook_search_page (notebook, NULL, STEP_PREV, TRUE);
  while (children)
    {
      page = children->data;
      children = gtk_notebook_search_page (notebook, children,
					   STEP_PREV, TRUE);
      if (!GTK_WIDGET_VISIBLE (page->child))
	continue;
      if (!GTK_WIDGET_MAPPED (page->tab_label))
	showarrow = TRUE;
      else if (page != notebook->cur_page)
	gtk_notebook_draw_tab (notebook, page, area);
    }

  if (showarrow && notebook->scrollable) 
    {
      gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
      gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
    }
  gtk_notebook_draw_tab (notebook, notebook->cur_page, area);
}

static void
gtk_notebook_draw_tab (GtkNotebook     *notebook,
		       GtkNotebookPage *page,
		       GdkRectangle    *area)
{
  GdkRectangle child_area;
  GdkRectangle page_area;
  GtkStateType state_type;
  GtkPositionType gap_side;
  
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (page != NULL);
  g_return_if_fail (area != NULL);

  if (!GTK_WIDGET_MAPPED (page->tab_label) ||
      (page->allocation.width == 0) || (page->allocation.height == 0))
    return;

  page_area.x = page->allocation.x;
  page_area.y = page->allocation.y;
  page_area.width = page->allocation.width;
  page_area.height = page->allocation.height;

  if (gdk_rectangle_intersect (&page_area, area, &child_area))
    {
      GtkWidget *widget;

      widget = GTK_WIDGET (notebook);
      gap_side = 0;
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  gap_side = GTK_POS_BOTTOM;
	  break;
	case GTK_POS_BOTTOM:
	  gap_side = GTK_POS_TOP;
	  break;
	case GTK_POS_LEFT:
	  gap_side = GTK_POS_RIGHT;
	  break;
	case GTK_POS_RIGHT:
	  gap_side = GTK_POS_LEFT;
	  break;
	}
      
      if (notebook->cur_page == page)
	state_type = GTK_STATE_NORMAL;
      else 
	state_type = GTK_STATE_ACTIVE;
      gtk_paint_extension(widget->style, widget->window,
			  state_type, GTK_SHADOW_OUT,
			  area, widget, "tab",
			  page_area.x, page_area.y,
			  page_area.width, page_area.height,
			  gap_side);
      if ((GTK_WIDGET_HAS_FOCUS (widget)) &&
	  notebook->focus_tab && (notebook->focus_tab->data == page))
	{
	  gtk_paint_focus (widget->style, widget->window,
			   area, widget, "tab",
			   page->tab_label->allocation.x - 1,
			   page->tab_label->allocation.y - 1,
			   page->tab_label->allocation.width + 1,
			   page->tab_label->allocation.height + 1);
	}
      if (gtk_widget_intersect (page->tab_label, area, &child_area))
	gtk_widget_draw (page->tab_label, &child_area);
    }
}

static void
gtk_notebook_draw_arrow (GtkNotebook *notebook,
			 guint        arrow)
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
	  if (notebook->focus_tab &&
	      !gtk_notebook_search_page (notebook, notebook->focus_tab,
					 STEP_PREV, TRUE))
	    {
	      shadow_type = GTK_SHADOW_ETCHED_IN;
	      state_type = GTK_STATE_NORMAL;
	    }

	  if (notebook->tab_pos == GTK_POS_LEFT ||
	      notebook->tab_pos == GTK_POS_RIGHT)
	    arrow = GTK_ARROW_UP;

	  gdk_window_clear_area (notebook->panel, 0, 0, ARROW_SIZE, ARROW_SIZE);
	  gtk_paint_arrow (widget->style, notebook->panel, state_type, 
			   shadow_type, NULL, GTK_WIDGET(notebook), "notebook",
			   arrow, TRUE, 
			   0, 0, ARROW_SIZE, ARROW_SIZE);
	}
      else
	{
	  if (notebook->focus_tab &&
	      !gtk_notebook_search_page (notebook, notebook->focus_tab,
					 STEP_NEXT, TRUE))
	    {
	      shadow_type = GTK_SHADOW_ETCHED_IN;
	      state_type = GTK_STATE_NORMAL;
	    }

	  if (notebook->tab_pos == GTK_POS_LEFT ||
	      notebook->tab_pos == GTK_POS_RIGHT)
	    arrow = GTK_ARROW_DOWN;

	   gdk_window_clear_area(notebook->panel, ARROW_SIZE + ARROW_SPACING, 
				 0, ARROW_SIZE, ARROW_SIZE);
	   gtk_paint_arrow (widget->style, notebook->panel, state_type, 
			    shadow_type, NULL, GTK_WIDGET(notebook), "notebook",
			    arrow, TRUE, ARROW_SIZE + ARROW_SPACING,
			    0, ARROW_SIZE, ARROW_SIZE);
	}
    }
}

static void
gtk_notebook_set_shape (GtkNotebook *notebook)
{
  GtkWidget       *widget = NULL;
  GdkPixmap       *pm = NULL;
  GdkGC           *pmgc = NULL;
  GdkColor         col;
  gint             x, y, width, height, w, h, depth;
  GtkNotebookPage *page;
  GList           *children;

  if (!GTK_WIDGET(notebook)->window)
    return;

  widget = GTK_WIDGET(notebook);

  w = widget->allocation.width;
  h = widget->allocation.height;

  pm = gdk_pixmap_new (widget->window, w, h, 1);
  pmgc = gdk_gc_new (pm);

  /* clear the shape mask */
  col.pixel = 0;
  gdk_gc_set_foreground(pmgc, &col);
  gdk_draw_rectangle(pm, pmgc, TRUE, 0, 0, w, h);

  col.pixel = 1;
  gdk_gc_set_foreground(pmgc, &col);

  /* draw the shape for the notebook page itself */
  x = GTK_CONTAINER(notebook)->border_width;
  y = GTK_CONTAINER(notebook)->border_width;
  width = widget->allocation.width - x * 2;
  height = widget->allocation.height - y * 2;

  if (notebook->show_tabs && notebook->children)
    {
      if (!(notebook->show_tabs))
	{
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
	}
      else
	{
	  if (notebook->cur_page)
	    page = notebook->cur_page;
	  else
	    page = notebook->children->data;

	  if (!GTK_WIDGET_MAPPED (page->tab_label))
	    {
	      if (notebook->tab_pos == GTK_POS_LEFT)
		{
		  x -= widget->style->klass->xthickness * 2;
		  width += widget->style->klass->xthickness * 2;
		}
	      else if (notebook->tab_pos == GTK_POS_RIGHT)
		width += widget->style->klass->xthickness * 2;
	    }
	  switch (notebook->tab_pos)
	    {
	    case GTK_POS_TOP:
	      y += page->allocation.height;
	    case GTK_POS_BOTTOM:
	      height -= page->allocation.height;
	      break;
	    case GTK_POS_LEFT:
	      x += page->allocation.width;
	    case GTK_POS_RIGHT:
	      width -= page->allocation.width;
	      break;
	    }
	}
    }
  gdk_draw_rectangle(pm, pmgc, TRUE, x, y, width, height);

  /* if theres an area for scrolling arrows draw the shape for them */
  if (notebook->panel && gdk_window_is_visible (notebook->panel))
    {
      gdk_window_get_geometry(notebook->panel, &x, &y, &width, &height, &depth);
      gdk_draw_rectangle(pm, pmgc, TRUE, x, y, width, height);
    }

  /* draw the shapes of all the children */
  if (notebook->show_tabs)
    {
      children = notebook->children;
      while (children)
	{
	  page = children->data;
	  if (GTK_WIDGET_MAPPED (page->tab_label))
	    {
	      x = page->allocation.x;
	      y = page->allocation.y;
	      width = page->allocation.width;
	      height = page->allocation.height;
	      gdk_draw_rectangle(pm, pmgc, TRUE, x, y, width, height);
	    }
	  children = children->next;
	}
    }

  /* set the mask */
  gdk_window_shape_combine_mask(widget->window, pm, 0, 0);
  gdk_pixmap_unref(pm);
  gdk_gc_destroy(pmgc);
}

/* Private GtkNotebook Size Allocate Functions:
 *
 * gtk_notebook_pages_allocate
 * gtk_notebook_page_allocate
 * gtk_notebook_calc_tabs
 */
static void
gtk_notebook_pages_allocate (GtkNotebook   *notebook,
			     GtkAllocation *allocation)
{
  GtkWidget    *widget;
  GtkContainer *container;
  GtkNotebookPage *page = NULL;
  GtkAllocation child_allocation;
  GList *children = NULL;
  GList *last_child = NULL;
  gboolean showarrow = FALSE;
  gint tab_space = 0; 
  gint delta; 
  gint x = 0;
  gint y = 0;
  gint i;
  gint n = 1;
  gint old_fill = 0;
  gint new_fill = 0;

  if (!notebook->show_tabs || !notebook->children || !notebook->cur_page)
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
	focus_tab = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);

      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;

	      if (GTK_WIDGET_VISIBLE (page->child))
		tab_space += page->requisition.width;
	    }
	  if (tab_space >
	      allocation->width - 2 * container->border_width - TAB_OVERLAP) 
	    {
	      showarrow = TRUE;
	      page = focus_tab->data; 

	      tab_space = (allocation->width - TAB_OVERLAP -
			   page->requisition.width -
			   2 * (container->border_width + ARROW_SPACING +
				ARROW_SIZE));
	      x = (allocation->width - 2 * ARROW_SIZE - ARROW_SPACING -
		   container->border_width);

	      page = notebook->children->data;
	      if (notebook->tab_pos == GTK_POS_TOP)
		y = (container->border_width +
		     (page->requisition.height - ARROW_SIZE) / 2);
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

	      if (GTK_WIDGET_VISIBLE (page->child))
		tab_space += page->requisition.height;
	    }
	  if (tab_space >
	      (allocation->height - 2 * container->border_width - TAB_OVERLAP))
	    {
	      showarrow = TRUE;
	      page = focus_tab->data; 
	      tab_space = (allocation->height -
			   ARROW_SIZE - ARROW_SPACING - TAB_OVERLAP -
			   2 * container->border_width -
			   page->requisition.height);
	      y = allocation->height - container->border_width - ARROW_SIZE;  

	      page = notebook->children->data;
	      if (notebook->tab_pos == GTK_POS_LEFT)
		x = (container->border_width +
		     (page->requisition.width -
		      (2 * ARROW_SIZE - ARROW_SPACING)) / 2); 
	      else
		x = (allocation->width - container->border_width -
		     (2 * ARROW_SIZE - ARROW_SPACING) -
		     (page->requisition.width -
		      (2 * ARROW_SIZE - ARROW_SPACING)) / 2);
	    }
	  break;
	}
      if (showarrow) /* first_tab <- focus_tab */
	{ 
	  if (tab_space <= 0)
	    {
	      notebook->first_tab = focus_tab;
	      last_child = gtk_notebook_search_page (notebook, focus_tab,
						     STEP_NEXT, TRUE);
	    }
	  else
	    {
	      children = NULL;
	      if (notebook->first_tab && notebook->first_tab != focus_tab)
		{
		  /* Is first_tab really predecessor of focus_tab  ? */
		  page = notebook->first_tab->data;
		  if (GTK_WIDGET_VISIBLE (page->child))
		    for (children = focus_tab;
			 children && children != notebook->first_tab;
			 children = gtk_notebook_search_page (notebook,
							      children,
							      STEP_PREV,
							      TRUE));
		}
	      if (!children)
		notebook->first_tab = focus_tab;
	      else
		gtk_notebook_calc_tabs (notebook,
					gtk_notebook_search_page (notebook,
								  focus_tab,
								  STEP_PREV,
								  TRUE), 
					&(notebook->first_tab), &tab_space,
					STEP_PREV);

	      if (tab_space <= 0)
		{
		  notebook->first_tab =
		    gtk_notebook_search_page (notebook, notebook->first_tab,
					      STEP_NEXT, TRUE);
		  if (!notebook->first_tab)
		    notebook->first_tab = focus_tab;
		  last_child = gtk_notebook_search_page (notebook, focus_tab,
							 STEP_NEXT, TRUE); 
		}
	      else /* focus_tab -> end */   
		{
		  if (!notebook->first_tab)
		    notebook->first_tab = gtk_notebook_search_page (notebook,
								    NULL,
								    STEP_NEXT,
								    TRUE);
		  children = NULL;
		  gtk_notebook_calc_tabs (notebook,
					  gtk_notebook_search_page (notebook,
								    focus_tab,
								    STEP_NEXT,
								    TRUE),
					  &children, &tab_space, STEP_NEXT);

		  if (tab_space <= 0) 
		    last_child = children;
		  else /* start <- first_tab */
		    {
		      last_child = NULL;
		      children = NULL;
		      gtk_notebook_calc_tabs
			(notebook,
			 gtk_notebook_search_page (notebook,
						   notebook->first_tab,
						   STEP_PREV,
						   TRUE),
			 &children, &tab_space, STEP_PREV);
		      notebook->first_tab = gtk_notebook_search_page(notebook,
								     children,
								     STEP_NEXT,
								     TRUE);
		    }
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
	      for (children = notebook->first_tab;
		   children && children != last_child;
		   children = gtk_notebook_search_page (notebook, children,
							STEP_NEXT, TRUE))
		n++;
	    }
	  else 
	    tab_space = 0;

	  /*unmap all non-visible tabs*/
	  for (children = gtk_notebook_search_page (notebook, NULL,
						    STEP_NEXT, TRUE);
	       children && children != notebook->first_tab;
	       children = gtk_notebook_search_page (notebook, children,
						    STEP_NEXT, TRUE))
	    {
	      page = children->data;
	      if (page->tab_label && GTK_WIDGET_MAPPED (page->tab_label))
		gtk_widget_unmap (page->tab_label);
	    }
	  for (children = last_child; children;
	       children = gtk_notebook_search_page (notebook, children,
						    STEP_NEXT, TRUE))
	    {
	      page = children->data;
	      if (page->tab_label && GTK_WIDGET_MAPPED (page->tab_label))
		gtk_widget_unmap (page->tab_label);
	    }
	}
      else /* !showarrow */
	{
	  notebook->first_tab = gtk_notebook_search_page (notebook, NULL,
							  STEP_NEXT, TRUE);
	  tab_space = 0;
	  if (GTK_WIDGET_REALIZED (notebook))
	    gdk_window_hide (notebook->panel);
	}
    }

  if (!showarrow)
    {
      gint c = 0;

      n = 0;
      children = notebook->children;
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;

	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  c++;
		  tab_space += page->requisition.width;
		  if (page->expand)
		    n++;
		}
	    }
	  tab_space -= allocation->width;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;

	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  c++;
		  tab_space += page->requisition.height;
		  if (page->expand)
		    n++;
		}
	    }
	  tab_space -= allocation->height;
	}
      tab_space += 2 * container->border_width + TAB_OVERLAP;
      tab_space *= -1;
      notebook->first_tab = gtk_notebook_search_page (notebook, NULL,
						      STEP_NEXT, TRUE);
      if (notebook->homogeneous && n)
	n = c;
    }
  
  children = notebook->first_tab;
  i = 1;
  while (children)
    {
      if (children == last_child)
	{
	  gtk_notebook_set_shape (notebook);
	  return;
	  children = NULL;
	  break;
	}

      page = children->data;
      if (!showarrow && page->pack != GTK_PACK_START)
	break;
      children = gtk_notebook_search_page (notebook, children, STEP_NEXT,TRUE);
      
      delta = 0;
      if (n && (showarrow || page->expand || notebook->homogeneous))
	{
	  new_fill = (tab_space * i++) / n;
	  delta = new_fill - old_fill;
	  old_fill = new_fill;
	}
      
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  child_allocation.width = (page->requisition.width +
				    TAB_OVERLAP + delta);
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  child_allocation.height = (page->requisition.height +
				     TAB_OVERLAP + delta);
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

      if (GTK_WIDGET_REALIZED (notebook) &&
	  page->tab_label && !GTK_WIDGET_MAPPED (page->tab_label))
	{
	  if (GTK_WIDGET_VISIBLE (page->tab_label))
	    gtk_widget_map (page->tab_label);
	  else
	    gtk_widget_show (page->tab_label);
	}
    }

  if (children)
    {
      children = notebook->children;
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  child_allocation.x = (allocation->x + allocation->width -
				container->border_width);
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  child_allocation.y = (allocation->y + allocation->height -
				container->border_width);
	  break;
	}

      while (children != last_child)
	{
	  page = children->data;
	  children = children->next;

	  if (page->pack != GTK_PACK_END || !GTK_WIDGET_VISIBLE (page->child))
	    continue;

	  delta = 0;
	  if (n && (page->expand || notebook->homogeneous))
	    {
	      new_fill = (tab_space * i++) / n;
	      delta = new_fill - old_fill;
	      old_fill = new_fill;
	    }

	  switch (notebook->tab_pos)
	    {
	    case GTK_POS_TOP:
	    case GTK_POS_BOTTOM:
	      child_allocation.width = (page->requisition.width +
					TAB_OVERLAP + delta);
	      child_allocation.x -= child_allocation.width;
	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      child_allocation.height = (page->requisition.height +
					 TAB_OVERLAP + delta);
	      child_allocation.y -= child_allocation.height;
	      break;
	    }

	  gtk_notebook_page_allocate (notebook, page, &child_allocation);

	  switch (notebook->tab_pos)
	    {
	    case GTK_POS_TOP:
	    case GTK_POS_BOTTOM:
	      child_allocation.x += TAB_OVERLAP;
	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      child_allocation.y += TAB_OVERLAP;
	      break;
	    }

	  if (GTK_WIDGET_REALIZED (notebook) && page->tab_label &&
	      !GTK_WIDGET_MAPPED (page->tab_label))
	    {
	      if (GTK_WIDGET_VISIBLE (page->tab_label))
		gtk_widget_map (page->tab_label);
	      else
		gtk_widget_show (page->tab_label);
	    }
	}
    }
  gtk_notebook_set_shape (notebook);
}

static void
gtk_notebook_page_allocate (GtkNotebook     *notebook,
			    GtkNotebookPage *page,
			    GtkAllocation   *allocation)
{
  GtkWidget *widget;
  GtkAllocation child_allocation;
  GtkRequisition tab_requisition;
  gint xthickness;
  gint ythickness;
  gint padding;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (page != NULL);
  g_return_if_fail (allocation != NULL);

  widget = GTK_WIDGET (notebook);

  xthickness = widget->style->klass->xthickness;
  ythickness = widget->style->klass->ythickness;

  /* If the size of the notebook tabs change, we need to queue
   * a redraw on the tab area
   */
  if ((allocation->width != page->allocation.width) ||
      (allocation->height != page->allocation.height))
    {
      gint x, y, width, height, border_width;

      border_width = GTK_CONTAINER (notebook)->border_width;

      switch (notebook->tab_pos)
       {
       case GTK_POS_TOP:
         width = widget->allocation.width;
         height = MAX (page->allocation.height, allocation->height) + ythickness;
         x = 0;                              
         y = border_width;
         break;

       case GTK_POS_BOTTOM:
         width = widget->allocation.width + xthickness;
         height = MAX (page->allocation.height, allocation->height) + ythickness;
         x = 0;                              
         y = widget->allocation.height - height - border_width;
         break;

       case GTK_POS_LEFT:
         width = MAX (page->allocation.width, allocation->width) + xthickness;
         height = widget->allocation.height;
         x = border_width;
         y = 0;
         break;

       case GTK_POS_RIGHT:
       default:                /* quiet gcc */
         width = MAX (page->allocation.width, allocation->width) + xthickness;
         height = widget->allocation.height;
         x = widget->allocation.width - width - border_width;
         y = 0;
         break;
       }

      gtk_widget_queue_clear_area (widget, x, y, width, height);
    }

  page->allocation = *allocation;
  gtk_widget_get_child_requisition (page->tab_label, &tab_requisition);

  if (notebook->cur_page != page)
    {
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  page->allocation.y += ythickness;
	case GTK_POS_BOTTOM:
	  if (page->allocation.height > ythickness)
	    page->allocation.height -= ythickness;
	  break;
	case GTK_POS_LEFT:
	  page->allocation.x += xthickness;
	case GTK_POS_RIGHT:
	  if (page->allocation.width > xthickness)
	    page->allocation.width -= xthickness;
	  break;
	}
    }

  switch (notebook->tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      padding = TAB_CURVATURE + FOCUS_WIDTH + notebook->tab_hborder;
      if (page->fill)
	{
	  child_allocation.x = (xthickness + FOCUS_WIDTH +
				notebook->tab_hborder);
	  child_allocation.width = MAX (1, (((gint) page->allocation.width) -
					    2 * child_allocation.x));
	  child_allocation.x += page->allocation.x;
	}
      else
	{
	  child_allocation.x = (page->allocation.x +
				(page->allocation.width -
				 tab_requisition.width) / 2);
	  child_allocation.width = tab_requisition.width;
	}
      child_allocation.y = (notebook->tab_vborder + FOCUS_WIDTH +
			    page->allocation.y);
      if (notebook->tab_pos == GTK_POS_TOP)
	child_allocation.y += ythickness;
      child_allocation.height = MAX (1, (((gint) page->allocation.height) - ythickness -
					 2 * (notebook->tab_vborder + FOCUS_WIDTH)));
      break;
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      padding = TAB_CURVATURE + FOCUS_WIDTH + notebook->tab_vborder;
      if (page->fill)
	{
	  child_allocation.y = ythickness + padding;
	  child_allocation.height = MAX (1, (((gint) page->allocation.height) -
					     2 * child_allocation.y));
	  child_allocation.y += page->allocation.y;
	}
      else
	{
	  child_allocation.y = (page->allocation.y + (page->allocation.height -
						      tab_requisition.height) / 2);
	  child_allocation.height = tab_requisition.height;
	}
      child_allocation.x = page->allocation.x + notebook->tab_hborder + FOCUS_WIDTH;
      if (notebook->tab_pos == GTK_POS_LEFT)
	child_allocation.x += xthickness;
      child_allocation.width = MAX (1, (((gint) page->allocation.width) - xthickness -
					2 * (notebook->tab_hborder + FOCUS_WIDTH)));
      break;
    }

  if (page->tab_label)
    gtk_widget_size_allocate (page->tab_label, &child_allocation);
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
  GList *last_list = NULL;
  gboolean pack;

  if (!start)
    return;

  children = start;
  pack = GTK_NOTEBOOK_PAGE (start)->pack;
  if (pack == GTK_PACK_END)
    direction = (direction == STEP_PREV) ? STEP_NEXT : STEP_PREV;

  while (1)
    {
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  while (children)
	    {
	      page = children->data;
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  if (page->pack == pack)
		    {
		      *tab_space -= page->requisition.width;
		      if (*tab_space < 0 || children == *end)
			{
			  if (*tab_space < 0) 
			    {
			      *tab_space = - (*tab_space +
					      page->requisition.width);
			      *end = children;
			    }
			  return;
			}
		    }
		  last_list = children;
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
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  if (page->pack == pack)
		    {
		      *tab_space -= page->requisition.height;
		      if (*tab_space < 0 || children == *end)
			{
			  if (*tab_space < 0)
			    {
			      *tab_space = - (*tab_space +
					      page->requisition.height);
			      *end = children;
			    }
			  return;
			}
		    }
		  last_list = children;
		}
	      if (direction == STEP_NEXT)
		children = children->next;
	      else
		children = children->prev;
	    }
	  break;
	}
      if (direction == STEP_PREV)
	return;
      pack = (pack == GTK_PACK_END) ? GTK_PACK_START : GTK_PACK_END;
      direction = STEP_PREV;
      children = last_list;
    }
}

/* Private GtkNotebook Page Switch Methods:
 *
 * gtk_notebook_real_switch_page
 */
static void
gtk_notebook_real_switch_page (GtkNotebook     *notebook,
			       GtkNotebookPage *page,
			       guint            page_num)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (page != NULL);

  if (notebook->cur_page == page || !GTK_WIDGET_VISIBLE (page->child))
    return;

  if (notebook->cur_page && GTK_WIDGET_MAPPED (notebook->cur_page->child))
    gtk_widget_unmap (notebook->cur_page->child);
  
  notebook->cur_page = page;

  if (!notebook->focus_tab ||
      notebook->focus_tab->data != (gpointer) notebook->cur_page)
    notebook->focus_tab = 
      g_list_find (notebook->children, notebook->cur_page);

  if (GTK_WIDGET_MAPPED (notebook))
    gtk_widget_map (notebook->cur_page->child);

  gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

/* Private GtkNotebook Page Switch Functions:
 *
 * gtk_notebook_switch_page
 * gtk_notebook_page_select
 * gtk_notebook_switch_focus_tab
 * gtk_notebook_menu_switch_page
 */
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

  if (page_num < 0)
    page_num = g_list_index (notebook->children, page);

  gtk_signal_emit (GTK_OBJECT (notebook), 
		   notebook_signals[SWITCH_PAGE], 
		   page,
		   page_num);
}

static gint
gtk_notebook_page_select (GtkNotebook *notebook)
{
  GtkNotebookPage *page;

  g_return_val_if_fail (notebook != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  if (!notebook->focus_tab)
    return FALSE;

  page = notebook->focus_tab->data;
  gtk_notebook_switch_page (notebook, page, -1);

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
  return FALSE;
}

static void
gtk_notebook_switch_focus_tab (GtkNotebook *notebook, 
			       GList       *new_child)
{
  GList *old_child;
  GtkNotebookPage *old_page = NULL;
  GtkNotebookPage *page;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->focus_tab == new_child)
    return;

  old_child = notebook->focus_tab;
  notebook->focus_tab = new_child;

  if (notebook->scrollable && GTK_WIDGET_DRAWABLE (notebook))
    {
      if ((new_child == NULL) != (old_child == NULL))
	{
	  gdk_window_clear (notebook->panel);
	  gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
	  gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
	}
      else
	{
	  GList *olist;
	  GList *nlist;

	  olist = gtk_notebook_search_page (notebook, old_child,
					    STEP_PREV, TRUE);
	  nlist = gtk_notebook_search_page (notebook, new_child,
					    STEP_PREV, TRUE);

	  if ((olist == NULL) != (nlist == NULL))
	    {
	      gdk_window_clear_area (notebook->panel, 0, 0,
				     ARROW_SIZE, ARROW_SIZE);
	      gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
	    }

	  olist = gtk_notebook_search_page (notebook, old_child,
					    STEP_NEXT, TRUE);
	  nlist = gtk_notebook_search_page (notebook, new_child,
					    STEP_NEXT, TRUE);

	  if ((olist == NULL) != (nlist == NULL))
	    {
	      gdk_window_clear_area (notebook->panel,
				     ARROW_SIZE + ARROW_SPACING, 0,
				     ARROW_SIZE, ARROW_SIZE);
	      gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
	    }
	}
    }

  if (!notebook->show_tabs || !notebook->focus_tab)
    return;

  if (old_child)
    old_page = old_child->data;

  page = notebook->focus_tab->data;
  if (GTK_WIDGET_MAPPED (page->tab_label))
    gtk_notebook_focus_changed (notebook, old_page);
  else
    {
      gtk_notebook_pages_allocate (notebook,
				   &(GTK_WIDGET (notebook)->allocation));
      gtk_notebook_expose_tabs (notebook);
    }

  gtk_notebook_set_shape (notebook);
}

static void
gtk_notebook_menu_switch_page (GtkWidget       *widget,
			       GtkNotebookPage *page)
{
  GtkNotebook *notebook;
  GList *children;
  guint page_num;

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

/* Private GtkNotebook Menu Functions:
 *
 * gtk_notebook_menu_item_create
 * gtk_notebook_menu_label_unparent
 * gtk_notebook_menu_detacher
 */
static void
gtk_notebook_menu_item_create (GtkNotebook *notebook, 
			       GList       *list)
{	
  GtkNotebookPage *page;
  GtkWidget *menu_item;

  page = list->data;
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
  gtk_widget_lock_accelerators (menu_item);
  gtk_container_add (GTK_CONTAINER (menu_item), page->menu_label);
  gtk_menu_insert (GTK_MENU (notebook->menu), menu_item,
		   gtk_notebook_real_page_position (notebook, list));
  gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
		      GTK_SIGNAL_FUNC (gtk_notebook_menu_switch_page), page);
  if (GTK_WIDGET_VISIBLE (page->child))
    gtk_widget_show (menu_item);
}

static void
gtk_notebook_menu_label_unparent (GtkWidget *widget, 
				  gpointer  data)
{
  gtk_widget_unparent (GTK_BIN(widget)->child);
  GTK_BIN(widget)->child = NULL;
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

/* Public GtkNotebook Page Insert/Remove Methods :
 *
 * gtk_notebook_append_page
 * gtk_notebook_append_page_menu
 * gtk_notebook_prepend_page
 * gtk_notebook_prepend_page_menu
 * gtk_notebook_insert_page
 * gtk_notebook_insert_page_menu
 * gtk_notebook_remove_page
 */
void
gtk_notebook_append_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label)
{
  gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, -1);
}

void
gtk_notebook_append_page_menu (GtkNotebook *notebook,
			       GtkWidget   *child,
			       GtkWidget   *tab_label,
			       GtkWidget   *menu_label)
{
  gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, -1);
}

void
gtk_notebook_prepend_page (GtkNotebook *notebook,
			   GtkWidget   *child,
			   GtkWidget   *tab_label)
{
  gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, 0);
}

void
gtk_notebook_prepend_page_menu (GtkNotebook *notebook,
				GtkWidget   *child,
				GtkWidget   *tab_label,
				GtkWidget   *menu_label)
{
  gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, 0);
}

void
gtk_notebook_insert_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label,
			  gint         position)
{
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
  page->expand = FALSE;
  page->fill = TRUE;
  page->pack = GTK_PACK_START;

  if (!menu_label)
    page->default_menu = TRUE;
  else  
    {
      gtk_widget_ref (page->menu_label);
      gtk_object_sink (GTK_OBJECT(page->menu_label));
    }

  if (notebook->menu)
    gtk_notebook_menu_item_create (notebook,
				   g_list_find (notebook->children, page));

  gtk_notebook_update_labels (notebook);

  if (!notebook->first_tab)
    notebook->first_tab = notebook->children;

  gtk_widget_set_parent (child, GTK_WIDGET (notebook));
  if (tab_label)
    gtk_widget_set_parent (tab_label, GTK_WIDGET (notebook));

  if (!notebook->cur_page)
    {
      gtk_notebook_switch_page (notebook, page, 0);
      gtk_notebook_switch_focus_tab (notebook, NULL);
    }

  if (GTK_WIDGET_REALIZED (child->parent))
    gtk_widget_realize (child);

  if (GTK_WIDGET_VISIBLE (notebook))
    {
      if (GTK_WIDGET_VISIBLE (child))
	{
	  if (GTK_WIDGET_MAPPED (notebook) &&
	      !GTK_WIDGET_MAPPED (child) &&
	      notebook->cur_page == page)
	    gtk_widget_map (child);
	  
	  gtk_widget_queue_resize (child);
	}

      if (tab_label)
	{
	  if (notebook->show_tabs && GTK_WIDGET_VISIBLE (child))
	    {
	      if (!GTK_WIDGET_VISIBLE (tab_label))
		gtk_widget_show (tab_label);
	      
	      if (GTK_WIDGET_REALIZED (notebook) &&
		  !GTK_WIDGET_REALIZED (tab_label))
		gtk_widget_realize (tab_label);
	      
	      if (GTK_WIDGET_MAPPED (notebook) &&
		  !GTK_WIDGET_MAPPED (tab_label))
		gtk_widget_map (tab_label);
	    }
	  else if (GTK_WIDGET_VISIBLE (tab_label))
	    gtk_widget_hide (tab_label);
	}
    }
}

void
gtk_notebook_remove_page (GtkNotebook *notebook,
			  gint         page_num)
{
  GList *list;
  
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  
  if (page_num >= 0)
    {
      list = g_list_nth (notebook->children, page_num);
      if (list)
	gtk_notebook_real_remove (notebook, list);
    }
  else
    {
      list = g_list_last (notebook->children);
      if (list)
	gtk_notebook_real_remove (notebook, list);
    }
}

/* Public GtkNotebook Page Switch Methods :
 * gtk_notebook_get_current_page
 * gtk_notebook_page_num
 * gtk_notebook_set_page
 * gtk_notebook_next_page
 * gtk_notebook_prev_page
 */
gint
gtk_notebook_get_current_page (GtkNotebook *notebook)
{
  g_return_val_if_fail (notebook != NULL, -1);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  if (!notebook->cur_page)
    return -1;

  return g_list_index (notebook->children, notebook->cur_page);
}

GtkWidget*
gtk_notebook_get_nth_page (GtkNotebook *notebook,
			   gint         page_num)
{
  GtkNotebookPage *page;

  g_return_val_if_fail (notebook != NULL, NULL);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  page = g_list_nth_data (notebook->children, page_num);

  if (page)
    return page->child;

  return NULL;
}

gint
gtk_notebook_page_num (GtkNotebook      *notebook,
		       GtkWidget        *child)
{
  GList *children;
  gint num;

  g_return_val_if_fail (notebook != NULL, -1);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  num = 0;
  children = notebook->children;
  while (children)
    {
      GtkNotebookPage *page;

      page = children->data;
      if (page->child == child)
	return num;

      children = children->next;
      num++;
    }

  return -1;
}

void
gtk_notebook_set_page (GtkNotebook *notebook,
		       gint         page_num)
{
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (page_num >= 0)
    list = g_list_nth (notebook->children, page_num);
  else
    {
      list = g_list_last (notebook->children);
      page_num = g_list_length (notebook->children) - 1;
    }
  if (list)
    gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list), page_num);
}

void
gtk_notebook_next_page (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  list = g_list_find (notebook->children, notebook->cur_page);
  if (!list)
    return;

  list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
  if (!list)
    return;

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list), -1);
}

void
gtk_notebook_prev_page (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  list = g_list_find (notebook->children, notebook->cur_page);
  if (!list)
    return;

  list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);
  if (!list)
    return;

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list), -1);
}

/* Public GtkNotebook/Tab Style Functions
 *
 * gtk_notebook_set_show_border
 * gtk_notebook_set_show_tabs
 * gtk_notebook_set_tab_pos
 * gtk_notebook_set_homogeneous_tabs
 * gtk_notebook_set_tab_border
 * gtk_notebook_set_tab_hborder
 * gtk_notebook_set_tab_vborder
 * gtk_notebook_set_scrollable
 */
void
gtk_notebook_set_show_border (GtkNotebook *notebook,
			      gboolean     show_border)
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
gtk_notebook_set_show_tabs (GtkNotebook *notebook,
			    gboolean     show_tabs)
{
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  show_tabs = show_tabs != FALSE;

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
      GTK_WIDGET_SET_FLAGS (notebook, GTK_CAN_FOCUS);
      gtk_notebook_update_labels (notebook);
    }
  gtk_widget_queue_resize (GTK_WIDGET (notebook));
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
gtk_notebook_set_homogeneous_tabs (GtkNotebook *notebook,
				   gboolean     homogeneous)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (homogeneous == notebook->homogeneous)
    return;

  notebook->homogeneous = homogeneous;
  gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

void
gtk_notebook_set_tab_border (GtkNotebook *notebook,
			     guint        tab_border)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  notebook->tab_hborder = tab_border;
  notebook->tab_vborder = tab_border;

  if (GTK_WIDGET_VISIBLE (notebook) && notebook->show_tabs)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

void
gtk_notebook_set_tab_hborder (GtkNotebook *notebook,
			      guint        tab_hborder)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->tab_hborder == tab_hborder)
    return;

  notebook->tab_hborder = tab_hborder;

  if (GTK_WIDGET_VISIBLE (notebook) && notebook->show_tabs)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

void
gtk_notebook_set_tab_vborder (GtkNotebook *notebook,
			      guint        tab_vborder)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->tab_vborder == tab_vborder)
    return;

  notebook->tab_vborder = tab_vborder;

  if (GTK_WIDGET_VISIBLE (notebook) && notebook->show_tabs)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

void
gtk_notebook_set_scrollable (GtkNotebook *notebook,
			     gboolean     scrollable)
{
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  scrollable = (scrollable != FALSE);

  if (scrollable != notebook->scrollable)
    {
      notebook->scrollable = scrollable;

      if (GTK_WIDGET_REALIZED (notebook))
	{
	  if (scrollable)
	    {
	      gtk_notebook_panel_realize (notebook);
	    }
	  else if (notebook->panel)
	    {
	      gdk_window_set_user_data (notebook->panel, NULL);
	      gdk_window_destroy (notebook->panel);
	      notebook->panel = NULL;
	    }
	}

      if (GTK_WIDGET_VISIBLE (notebook))
	gtk_widget_queue_resize (GTK_WIDGET(notebook));
    }
}

/* Public GtkNotebook Popup Menu Methods:
 *
 * gtk_notebook_popup_enable
 * gtk_notebook_popup_disable
 */
void
gtk_notebook_popup_enable (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->menu)
    return;

  notebook->menu = gtk_menu_new ();
  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    gtk_notebook_menu_item_create (notebook, list);

  gtk_notebook_update_labels (notebook);
  gtk_menu_attach_to_widget (GTK_MENU (notebook->menu),
			     GTK_WIDGET (notebook),
			     gtk_notebook_menu_detacher);
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

/* Public GtkNotebook Page Properties Functions:
 *
 * gtk_notebook_get_tab_label
 * gtk_notebook_set_tab_label
 * gtk_notebook_set_tab_label_text
 * gtk_notebook_get_menu_label
 * gtk_notebook_set_menu_label
 * gtk_notebook_set_menu_label_text
 * gtk_notebook_set_tab_label_packing
 * gtk_notebook_query_tab_label_packing
 */
GtkWidget *
gtk_notebook_get_tab_label (GtkNotebook *notebook,
			    GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (notebook != NULL, NULL);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (child != NULL, NULL);

  if (!(list = g_list_find_custom (notebook->children, child,
				   gtk_notebook_page_compare)))
    return NULL;

  if (GTK_NOTEBOOK_PAGE (list)->default_tab)
    return NULL;

  return GTK_NOTEBOOK_PAGE (list)->tab_label;
}  

void
gtk_notebook_set_tab_label (GtkNotebook *notebook,
			    GtkWidget   *child,
			    GtkWidget   *tab_label)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);
  
  if (!(list = g_list_find_custom (notebook->children, child,
				   gtk_notebook_page_compare)))
      return;

  /* a NULL pointer indicates a default_tab setting, otherwise
   * we need to set the associated label
   */

  page = list->data;
  if (page->tab_label)
    gtk_widget_unparent (page->tab_label);

  if (tab_label)
    {
      page->default_tab = FALSE;
      page->tab_label = tab_label;
      gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
    }
  else
    {
      page->default_tab = TRUE;
      page->tab_label = NULL;

      if (notebook->show_tabs)
	{
	  gchar string[32];

	  g_snprintf (string, sizeof(string), _("Page %u"), 
		      gtk_notebook_real_page_position (notebook, list));
	  page->tab_label = gtk_label_new (string);
	  gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
	}
    }

  if (notebook->show_tabs && GTK_WIDGET_VISIBLE (child))
    {
      gtk_widget_show (page->tab_label);
      gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }
}

void
gtk_notebook_set_tab_label_text (GtkNotebook *notebook,
				 GtkWidget   *child,
				 const gchar *tab_text)
{
  GtkWidget *tab_label = NULL;

  if (tab_text)
    tab_label = gtk_label_new (tab_text);
  gtk_notebook_set_tab_label (notebook, child, tab_label);
}

GtkWidget*
gtk_notebook_get_menu_label (GtkNotebook *notebook,
			     GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (notebook != NULL, NULL);
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (child != NULL, NULL);

  if (!(list = g_list_find_custom (notebook->children, child,
				   gtk_notebook_page_compare)))
      return NULL;

  if (GTK_NOTEBOOK_PAGE (list)->default_menu)
    return NULL;

  return GTK_NOTEBOOK_PAGE (list)->menu_label;
}  

void
gtk_notebook_set_menu_label (GtkNotebook *notebook,
			     GtkWidget   *child,
			     GtkWidget   *menu_label)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  if (!(list = g_list_find_custom (notebook->children, child,
				   gtk_notebook_page_compare)))
      return;

  page = list->data;
  if (page->menu_label)
    {
      if (notebook->menu)
	{
	  gtk_container_remove (GTK_CONTAINER (notebook->menu), 
				page->menu_label->parent);
	  gtk_widget_queue_resize (notebook->menu);
	}
      if (!page->default_menu)
	gtk_widget_unref (page->menu_label);
    }

  if (menu_label)
    {
      page->menu_label = menu_label;
      gtk_widget_ref (page->menu_label);
      gtk_object_sink (GTK_OBJECT(page->menu_label));
      page->default_menu = FALSE;
    }
  else
    page->default_menu = TRUE;

  if (notebook->menu)
    gtk_notebook_menu_item_create (notebook, list);
}

void
gtk_notebook_set_menu_label_text (GtkNotebook *notebook,
				  GtkWidget   *child,
				  const gchar *menu_text)
{
  GtkWidget *menu_label = NULL;

  if (menu_text)
    menu_label = gtk_label_new (menu_text);
  gtk_notebook_set_menu_label (notebook, child, menu_label);
}

void
gtk_notebook_set_tab_label_packing (GtkNotebook *notebook,
				    GtkWidget   *child,
				    gboolean     expand,
				    gboolean     fill,
				    GtkPackType  pack_type)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  if (!(list = g_list_find_custom (notebook->children, child,
				   gtk_notebook_page_compare)))
      return;

  page = list->data;
  if (page->pack == pack_type && page->expand == expand && page->fill == fill)
    return;

  page->expand = expand;
  page->fill = fill;

  if (page->pack != pack_type)
    {
      page->pack = pack_type;
      if (notebook->menu)
	{
	  GtkWidget *menu_item;

	  menu_item = page->menu_label->parent;
	  gtk_container_remove (GTK_CONTAINER (menu_item), page->menu_label);
	  gtk_container_remove (GTK_CONTAINER (notebook->menu), menu_item);
	  gtk_notebook_menu_item_create (notebook, list);
	  gtk_widget_queue_resize (notebook->menu);
	}
      gtk_notebook_update_labels (notebook);
    }

  if (!notebook->show_tabs)
    return;

  gtk_notebook_pages_allocate (notebook, &(GTK_WIDGET (notebook)->allocation));
  gtk_notebook_expose_tabs (notebook);
}  

void
gtk_notebook_query_tab_label_packing (GtkNotebook *notebook,
				      GtkWidget   *child,
				      gboolean    *expand,
				      gboolean    *fill,
				      GtkPackType *pack_type)
{
  GList *list;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);

  if (!(list = g_list_find_custom (notebook->children, child,
				   gtk_notebook_page_compare)))
      return;

  if (expand)
    *expand = GTK_NOTEBOOK_PAGE (list)->expand;
  if (fill)
    *fill = GTK_NOTEBOOK_PAGE (list)->fill;
  if (pack_type)
    *pack_type = GTK_NOTEBOOK_PAGE (list)->pack;
}

void
gtk_notebook_reorder_child (GtkNotebook *notebook,
			    GtkWidget   *child,
			    gint         position)
{
  GList *list;
  GList *work;
  GtkNotebookPage *page = NULL;
  gint old_pos;

  g_return_if_fail (notebook != NULL);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  for (old_pos = 0, list = notebook->children; list;
       list = list->next, old_pos++)
    {
      page = list->data;
      if (page->child == child)
	break;
    }

  if (!list || old_pos == position)
    return;

  notebook->children = g_list_remove_link (notebook->children, list);
  
  if (position <= 0 || !notebook->children)
    {
      list->next = notebook->children;
      if (list->next)
	list->next->prev = list;
      notebook->children = list;
    }
  else if (position > 0 && (work = g_list_nth (notebook->children, position)))
    {
      list->prev = work->prev;
      if (list->prev)
	list->prev->next = list;
      list->next = work;
      work->prev = list;
    }
  else
    {
      work = g_list_last (notebook->children);
      work->next = list;
      list->prev = work;
    }

  if (notebook->menu)
    {
      GtkWidget *menu_item;

      g_assert(page != NULL);

      menu_item = page->menu_label->parent;
      gtk_container_remove (GTK_CONTAINER (menu_item), page->menu_label);
      gtk_container_remove (GTK_CONTAINER (notebook->menu), menu_item);
      gtk_notebook_menu_item_create (notebook, list);
      gtk_widget_queue_resize (notebook->menu);
    }

  gtk_notebook_update_labels (notebook);

  if (notebook->show_tabs)
    {
      gtk_notebook_pages_allocate (notebook,
				   &(GTK_WIDGET (notebook)->allocation));
      gtk_notebook_expose_tabs (notebook);
    }
}
