/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
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

#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtkvbox.h"
#include "gtktoolbar.h"


#define DEFAULT_SPACE_SIZE 5


enum {
	ORIENTATION_CHANGED,
	STYLE_CHANGED,
	LAST_SIGNAL
};


typedef struct {
	GtkWidget *button;
	GtkWidget *icon;
	GtkWidget *label;
} Child;


typedef void (*GtkToolbarSignal1) (GtkObject *object,
				   gint       arg1,
				   gpointer   data);

static void gtk_toolbar_marshal_signal_1 (GtkObject     *object,
					  GtkSignalFunc  func,
					  gpointer       func_data,
					  GtkArg        *args);


static void gtk_toolbar_class_init               (GtkToolbarClass *class);
static void gtk_toolbar_init                     (GtkToolbar      *toolbar);
static void gtk_toolbar_destroy                  (GtkObject       *object);
static void gtk_toolbar_map                      (GtkWidget       *widget);
static void gtk_toolbar_unmap                    (GtkWidget       *widget);
static void gtk_toolbar_draw                     (GtkWidget       *widget,
				                  GdkRectangle    *area);
static void gtk_toolbar_size_request             (GtkWidget       *widget,
				                  GtkRequisition  *requisition);
static void gtk_toolbar_size_allocate            (GtkWidget       *widget,
				                  GtkAllocation   *allocation);
static void gtk_toolbar_add                      (GtkContainer    *container,
				                  GtkWidget       *widget);
static void gtk_toolbar_foreach                  (GtkContainer    *container,
				                  GtkCallback      callback,
				                  gpointer         callback_data);
static void gtk_real_toolbar_orientation_changed (GtkToolbar      *toolbar,
						  GtkOrientation   orientation);
static void gtk_real_toolbar_style_changed       (GtkToolbar      *toolbar,
						  GtkToolbarStyle  style);


static GtkContainerClass *parent_class;

static gint toolbar_signals[LAST_SIGNAL] = { 0 };


guint
gtk_toolbar_get_type(void)
{
	static guint toolbar_type = 0;

	if (!toolbar_type) {
		GtkTypeInfo toolbar_info = {
			"GtkToolbar",
			sizeof(GtkToolbar),
			sizeof(GtkToolbarClass),
			(GtkClassInitFunc) gtk_toolbar_class_init,
			(GtkObjectInitFunc) gtk_toolbar_init,
			(GtkArgFunc) NULL
		};

		toolbar_type = gtk_type_unique(gtk_container_get_type(), &toolbar_info);
	}

	return toolbar_type;
}

static void
gtk_toolbar_class_init(GtkToolbarClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	container_class = (GtkContainerClass *) class;

	parent_class = gtk_type_class(gtk_container_get_type());

	toolbar_signals[ORIENTATION_CHANGED] =
		gtk_signal_new("orientation_changed",
			       GTK_RUN_FIRST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(GtkToolbarClass, orientation_changed),
			       gtk_toolbar_marshal_signal_1,
			       GTK_TYPE_NONE, 1,
			       GTK_TYPE_INT);
	toolbar_signals[STYLE_CHANGED] =
		gtk_signal_new("style_changed",
			       GTK_RUN_FIRST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(GtkToolbarClass, style_changed),
			       gtk_toolbar_marshal_signal_1,
			       GTK_TYPE_NONE, 1,
			       GTK_TYPE_INT);

	gtk_object_class_add_signals(object_class, toolbar_signals, LAST_SIGNAL);

	object_class->destroy = gtk_toolbar_destroy;

	widget_class->map = gtk_toolbar_map;
	widget_class->unmap = gtk_toolbar_unmap;
	widget_class->draw = gtk_toolbar_draw;
	widget_class->size_request = gtk_toolbar_size_request;
	widget_class->size_allocate = gtk_toolbar_size_allocate;

	container_class->add = gtk_toolbar_add;
	container_class->foreach = gtk_toolbar_foreach;

	class->orientation_changed = gtk_real_toolbar_orientation_changed;
	class->style_changed = gtk_real_toolbar_style_changed;
}

static void
gtk_toolbar_init(GtkToolbar *toolbar)
{
	GTK_WIDGET_SET_FLAGS(toolbar, GTK_NO_WINDOW);

	toolbar->num_children = 0;
	toolbar->children     = NULL;
	toolbar->orientation  = GTK_ORIENTATION_HORIZONTAL;
	toolbar->style        = GTK_TOOLBAR_ICONS;
	toolbar->space_size   = DEFAULT_SPACE_SIZE;
	toolbar->tooltips     = gtk_tooltips_new();
}

GtkWidget *
gtk_toolbar_new(GtkOrientation  orientation,
		GtkToolbarStyle style)
{
	GtkToolbar *toolbar;

	toolbar = gtk_type_new(gtk_toolbar_get_type());

	toolbar->orientation = orientation;
	toolbar->style = style;

	return GTK_WIDGET(toolbar);
}


static void
gtk_toolbar_destroy(GtkObject *object)
{
	GtkToolbar *toolbar;
	GList      *children;
	Child      *child;

	g_return_if_fail(object != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(object));

	toolbar = GTK_TOOLBAR(object);

	gtk_tooltips_unref(toolbar->tooltips); /* XXX: do I have to unref the tooltips to destroy them? */

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		/* NULL child means it is a space in the toolbar, rather than a button */
		if (child) {
			child->button->parent = NULL;
			gtk_object_unref(GTK_OBJECT(child->button));
			gtk_widget_destroy(child->button);
			g_free(child);
		}
	}

	g_list_free(toolbar->children);

	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		(* GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}

static void
gtk_toolbar_map(GtkWidget *widget)
{
	GtkToolbar *toolbar;
	GList      *children;
	Child      *child;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(widget));

	toolbar = GTK_TOOLBAR(widget);
	GTK_WIDGET_SET_FLAGS(toolbar, GTK_MAPPED);

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		if (child && GTK_WIDGET_VISIBLE(child->button) && !GTK_WIDGET_MAPPED(child->button))
			gtk_widget_map(child->button);
	}
}

static void
gtk_toolbar_unmap(GtkWidget *widget)
{
	GtkToolbar *toolbar;
	GList      *children;
	Child      *child;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(widget));
	
	toolbar = GTK_TOOLBAR(widget);
	GTK_WIDGET_UNSET_FLAGS(toolbar, GTK_MAPPED);

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		if (child && GTK_WIDGET_VISIBLE(child->button) && GTK_WIDGET_MAPPED(child->button))
			gtk_widget_unmap(child->button);
	}
}

static void
gtk_toolbar_draw(GtkWidget    *widget,
		 GdkRectangle *area)
{
	GtkToolbar   *toolbar;
	GList        *children;
	Child        *child;
	GdkRectangle  child_area;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(widget));

	if (GTK_WIDGET_DRAWABLE(widget)) {
		toolbar = GTK_TOOLBAR(widget);

		for (children = toolbar->children; children; children = children->next) {
			child = children->data;

			if (child && gtk_widget_intersect(child->button, area, &child_area))
				gtk_widget_draw(child->button, &child_area);
		}
	}
}

static void
gtk_toolbar_size_request(GtkWidget      *widget,
			 GtkRequisition *requisition)
{
	GtkToolbar     *toolbar;
	GList          *children;
	Child          *child;
	gint           	nchildren;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(widget));
	g_return_if_fail(requisition != NULL);

	toolbar = GTK_TOOLBAR(widget);

	requisition->width = GTK_CONTAINER(toolbar)->border_width * 2;
	requisition->height = GTK_CONTAINER(toolbar)->border_width * 2;
	nchildren = 0;
	toolbar->child_maxw = 0;
	toolbar->child_maxh = 0;

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		if (child) {
			nchildren++;

			gtk_widget_size_request(child->button, &child->button->requisition);

			toolbar->child_maxw = MAX(toolbar->child_maxw, child->button->requisition.width);
			toolbar->child_maxh = MAX(toolbar->child_maxh, child->button->requisition.height);
		} else
			/* NULL child means it is a space in the toolbar, rather than a button */
			if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
				requisition->width += toolbar->space_size;
			else
				requisition->height += toolbar->space_size;
	}

	if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL) {
		requisition->width += nchildren * toolbar->child_maxw;
		requisition->height += toolbar->child_maxh;
	} else {
		requisition->width += toolbar->child_maxw;
		requisition->height += nchildren * toolbar->child_maxh;
	}
}

static void
gtk_toolbar_size_allocate(GtkWidget     *widget,
			  GtkAllocation *allocation)
{
	GtkToolbar     *toolbar;
	GList          *children;
	Child          *child;
	GtkAllocation   alloc;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(widget));
	g_return_if_fail(allocation != NULL);

	toolbar = GTK_TOOLBAR(widget);
	widget->allocation = *allocation;

	alloc.x      = allocation->x + GTK_CONTAINER(toolbar)->border_width;
	alloc.y      = allocation->y + GTK_CONTAINER(toolbar)->border_width;
	alloc.width  = toolbar->child_maxw;
	alloc.height = toolbar->child_maxh;

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		if (child) {
			gtk_widget_size_allocate(child->button, &alloc);

			if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
				alloc.x += toolbar->child_maxw;
			else
				alloc.y += toolbar->child_maxh;
		} else
			/* NULL child means it is a space in the toolbar, rather than a button */
			if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
				alloc.x += toolbar->space_size;
			else
				alloc.y += toolbar->space_size;
	}
}

static void
gtk_toolbar_add(GtkContainer *container,
		GtkWidget    *widget)
{
	g_warning("gtk_toolbar_add: use gtk_toolbar_add_item() instead!");
}

static void
gtk_toolbar_foreach(GtkContainer    *container,
		    GtkCallback      callback,
		    gpointer         callback_data)
{
	GtkToolbar *toolbar;
	GList      *children;
	Child      *child;

	g_return_if_fail(container != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(container));
	g_return_if_fail(callback != NULL);

	toolbar = GTK_TOOLBAR(container);

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		if (child)
			(*callback) (child->button, callback_data);
	}
}

void
gtk_toolbar_append_item(GtkToolbar      *toolbar,
			const char      *text,
			const char      *tooltip_text,
			GtkPixmap       *icon,
			GtkSignalFunc    callback,
			gpointer         user_data)
{
	gtk_toolbar_insert_item(toolbar, text, tooltip_text, icon,
				callback, user_data, toolbar->num_children);
}

void
gtk_toolbar_prepend_item(GtkToolbar      *toolbar,
			 const char      *text,
			 const char      *tooltip_text,
			 GtkPixmap       *icon,
			 GtkSignalFunc    callback,
			 gpointer         user_data)
{
	gtk_toolbar_insert_item(toolbar, text, tooltip_text, icon,
				callback, user_data, 0);
}

void
gtk_toolbar_insert_item(GtkToolbar      *toolbar,
			const char      *text,
			const char      *tooltip_text,
			GtkPixmap       *icon,
			GtkSignalFunc    callback,
			gpointer         user_data,
			gint             position)
{
	Child     *child;
	GtkWidget *vbox;

	g_return_if_fail(toolbar != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(toolbar));

	child = g_new(Child, 1);

	child->button = gtk_button_new();

	if (callback)
		gtk_signal_connect(GTK_OBJECT(child->button), "clicked",
				   callback, user_data);

	if (tooltip_text)
		gtk_tooltips_set_tips(toolbar->tooltips, child->button, tooltip_text);

	if (text)
		child->label = gtk_label_new(text);
	else
		child->label = NULL;
	
	child->icon = GTK_WIDGET(icon);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(child->button), vbox);
	gtk_widget_show(vbox);

	if (child->icon)
		gtk_box_pack_start(GTK_BOX(vbox), child->icon, FALSE, FALSE, 0);

	if (child->label)
		gtk_box_pack_start(GTK_BOX(vbox), child->label, FALSE, FALSE, 0);

	switch (toolbar->style) {
		case GTK_TOOLBAR_ICONS:
			if (child->icon)
				gtk_widget_show(child->icon);
			break;

		case GTK_TOOLBAR_TEXT:
			if (child->label)
				gtk_widget_show(child->label);
			break;

		case GTK_TOOLBAR_BOTH:
			if (child->icon)
				gtk_widget_show(child->icon);

			if (child->label)
				gtk_widget_show(child->label);

			break;

		default:
			g_assert_not_reached();
	}

	gtk_widget_show(child->button);

	toolbar->children = g_list_insert(toolbar->children, child, position);
	toolbar->num_children++;

	gtk_widget_set_parent(child->button, GTK_WIDGET(toolbar));

	if (GTK_WIDGET_VISIBLE(toolbar)) {
		if (GTK_WIDGET_REALIZED(toolbar)
		    && !GTK_WIDGET_REALIZED(child->button))
			gtk_widget_realize(child->button);

		if (GTK_WIDGET_MAPPED(toolbar)
		    && !GTK_WIDGET_MAPPED(child->button))
			gtk_widget_map(child->button);
	}

	if (GTK_WIDGET_VISIBLE(child->button) && GTK_WIDGET_VISIBLE(toolbar))
		gtk_widget_queue_resize(child->button);
}

void
gtk_toolbar_append_space(GtkToolbar *toolbar)
{
	gtk_toolbar_insert_space(toolbar, toolbar->num_children);
}

void
gtk_toolbar_prepend_space(GtkToolbar *toolbar)
{
	gtk_toolbar_insert_space(toolbar, 0);
}

void
gtk_toolbar_insert_space(GtkToolbar *toolbar,
			 gint        position)
{
	g_return_if_fail(toolbar != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(toolbar));

	/* NULL child means it is a space in the toolbar, rather than a button */

	toolbar->children = g_list_insert(toolbar->children, NULL, position);
	toolbar->num_children++;

	if (GTK_WIDGET_VISIBLE(toolbar))
		gtk_widget_queue_resize(GTK_WIDGET(toolbar));
}

void
gtk_toolbar_set_orientation(GtkToolbar     *toolbar,
			    GtkOrientation  orientation)
{
	gtk_signal_emit(GTK_OBJECT(toolbar), toolbar_signals[ORIENTATION_CHANGED], orientation);
}

void
gtk_toolbar_set_style(GtkToolbar      *toolbar,
		      GtkToolbarStyle  style)
{
	gtk_signal_emit(GTK_OBJECT(toolbar), toolbar_signals[STYLE_CHANGED], style);
}

void
gtk_toolbar_set_space_size(GtkToolbar *toolbar,
			   gint        space_size)
{
	g_return_if_fail(toolbar != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(toolbar));

	if (toolbar->space_size != space_size) {
		toolbar->space_size = space_size;
		gtk_widget_queue_resize(GTK_WIDGET(toolbar));
	}
}

void
gtk_toolbar_set_tooltips(GtkToolbar *toolbar,
			 gint        enable)
{
	g_return_if_fail(toolbar != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(toolbar));

	if (enable)
		gtk_tooltips_enable(toolbar->tooltips);
	else
		gtk_tooltips_disable(toolbar->tooltips);
}

static void
gtk_toolbar_marshal_signal_1(GtkObject     *object,
			     GtkSignalFunc  func,
			     gpointer       func_data,
			     GtkArg        *args)
{
	GtkToolbarSignal1 rfunc;

	rfunc = (GtkToolbarSignal1) func;

	(*rfunc) (object, GTK_VALUE_ENUM(args[0]), func_data);
}

static void
gtk_real_toolbar_orientation_changed(GtkToolbar      *toolbar,
				     GtkOrientation   orientation)
{
	g_return_if_fail(toolbar != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(toolbar));

	if (toolbar->orientation != orientation) {
		toolbar->orientation = orientation;
		gtk_widget_queue_resize(GTK_WIDGET(toolbar));
	}
}

static void
gtk_real_toolbar_style_changed(GtkToolbar      *toolbar,
			       GtkToolbarStyle  style)
{
	GList *children;
	Child *child;

	g_return_if_fail(toolbar != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(toolbar));

	if (toolbar->style != style) {
		toolbar->style = style;

		for (children = toolbar->children; children; children = children->next) {
			child = children->data;

			if (child)
				switch (style) {
					case GTK_TOOLBAR_ICONS:
						if (!GTK_WIDGET_VISIBLE(child->icon))
							gtk_widget_show(child->icon);

						if (GTK_WIDGET_VISIBLE(child->label))
							gtk_widget_hide(child->label);

						break;

					case GTK_TOOLBAR_TEXT:
						if (GTK_WIDGET_VISIBLE(child->icon))
							gtk_widget_hide(child->icon);

						if (!GTK_WIDGET_VISIBLE(child->label))
							gtk_widget_show(child->label);

						break;

					case GTK_TOOLBAR_BOTH:
						if (!GTK_WIDGET_VISIBLE(child->icon))
							gtk_widget_show(child->icon);

						if (!GTK_WIDGET_VISIBLE(child->label))
							gtk_widget_show(child->label);

						break;

					default:
						g_assert_not_reached();
				}
		}
		
		gtk_widget_queue_resize(GTK_WIDGET(toolbar));
	}
}
