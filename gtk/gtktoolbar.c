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

#include "gtktoolbar.h"


#define DEFAULT_SPACE_SIZE 5


static void gtk_toolbar_class_init    (GtkToolbarClass *class);
static void gtk_toolbar_init          (GtkToolbar      *toolbar);
static void gtk_toolbar_destroy       (GtkObject       *object);
static void gtk_toolbar_map           (GtkWidget       *widget);
static void gtk_toolbar_unmap         (GtkWidget       *widget);
static void gtk_toolbar_draw          (GtkWidget       *widget,
				       GdkRectangle    *area);
static void gtk_toolbar_size_request  (GtkWidget       *widget,
				       GtkRequisition  *requisition);
static void gtk_toolbar_size_allocate (GtkWidget       *widget,
				       GtkAllocation   *allocation);
static void gtk_toolbar_add           (GtkContainer    *container,
				       GtkWidget       *widget);

static GtkContainerClass *parent_class;


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

	object_class->destroy = gtk_toolbar_destroy;

	widget_class->map = gtk_toolbar_map;
	widget_class->unmap = gtk_toolbar_unmap;
	widget_class->draw = gtk_toolbar_draw;
	widget_class->size_request = gtk_toolbar_size_request;
	widget_class->size_allocate = gtk_toolbar_size_allocate;

	container_class->add = gtk_toolbar_add;
}

static void
gtk_toolbar_init(GtkToolbar *toolbar)
{
	GTK_WIDGET_SET_FLAGS(toolbar, GTK_NO_WINDOW);

	toolbar->children    = NULL;
	toolbar->orientation = GTK_ORIENTATION_HORIZONTAL;
	toolbar->style       = GTK_TOOLBAR_ICONS;
	toolbar->space_size  = DEFAULT_SPACE_SIZE;
}

GtkWidget *
gtk_toolbar_new(GtkOrientation  orientation,
		GtkToolbarStyle style)
{
	GtkToolbar *toolbar;

	toolbar = gtk_type_new(gtk_toolbar_get_type());

	toolbar->orientation = orientation;
	toolbar->style = style;
}


static void
gtk_toolbar_destroy(GtkObject *object)
{
	GtkToolbar *toolbar;
	GList      *children;
	GtkWidget  *child;

	g_return_if_fail(object != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(object));

	toolbar = GTK_TOOLBAR(object);

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		/* NULL child means it is a space in the toolbar, rather than a button */
		if (child) {
			child->parent = NULL;
			gtk_object_unref(GTK_OBJECT(child));
			gtk_widget_destroy(child);
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
	GtkWidget  *child;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(widget));

	toolbar = GTK_TOOLBAR(widget);
	GTK_WIDGET_SET_FLAGS(toolbar, GTK_MAPPED);

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		if (child && GTK_WIDGET_VISIBLE(child) && !GTK_WIDGET_MAPPED(child))
			gtk_widget_map(child);
	}
}

static void
gtk_toolbar_unmap(GtkWidget *widget)
{
	GtkToolbar *toolbar;
	GList      *children;
	GtkWidget  *child;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(widget));
	
	toolbar = GTK_TOOLBAR(widget);
	GTK_WIDGET_UNSET_FLAGS(toolbar, GTK_MAPPED);

	for (children = toolbar->children; children; children = children->next) {
		child = children->data;

		if (child && GTK_WIDGET_VISIBLE(child) && GTK_WIDGET_MAPPED(child))
			gtk_widget_unmap(child);
	}
}

static void
gtk_toolbar_draw(GtkWidget    *widget,
		 GdkRectangle *area)
{
	GtkToolbar   *toolbar;
	GList        *children;
	GtkWidget    *child;
	GdkRectangle  child_area;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_TOOLBAR(widget));

	if (GTK_WIDGET_DRAWABLE(widget)) {
		toolbar = GTK_TOOLBAR(widget);

		for (children = toolbar->children; children; children = children->next) {
			child = children->data;

			if (child && gtk_widget_intersect(child, area, &child_area))
				gtk_widget_draw(child, &child_area);
		}
	}
}

static void
gtk_toolbar_size_request(GtkWidget      *widget,
			 GtkRequisition *requisition)
{
	GtkToolbar     *toolbar;
	GList          *children;
	GtkWidget      *child;
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

			gtk_widget_size_request(child, &child->requisition);

			toolbar->child_maxw = MAX(toolbar->child_maxw, child->requisition->width);
			toolbar->child_maxh = MAX(toolbar->child_maxh, child->requisition->height);
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
	GtkWidget      *child;
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
			gtk_widget_size_allocate(child, &alloc);

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
