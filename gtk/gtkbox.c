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

#include "gtkbox.h"
#include "gtkintl.h"

enum {
  PROP_0,
  PROP_SPACING,
  PROP_HOMOGENEOUS
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_EXPAND,
  CHILD_PROP_FILL,
  CHILD_PROP_PADDING,
  CHILD_PROP_PACK_TYPE,
  CHILD_PROP_POSITION
};

static void gtk_box_class_init (GtkBoxClass    *klass);
static void gtk_box_init       (GtkBox         *box);
static void gtk_box_set_property (GObject         *object,
				  guint            prop_id,
				  const GValue    *value,
				  GParamSpec      *pspec);
static void gtk_box_get_property (GObject         *object,
				  guint            prop_id,
				  GValue          *value,
				  GParamSpec      *pspec);
static void gtk_box_add        (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_box_remove     (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_box_forall     (GtkContainer   *container,
				gboolean	include_internals,
			        GtkCallback     callback,
			        gpointer        callback_data);
static void gtk_box_set_child_property (GtkContainer    *container,
					GtkWidget       *child,
					guint            property_id,
					const GValue    *value,
					GParamSpec      *pspec);
static void gtk_box_get_child_property (GtkContainer    *container,
					GtkWidget       *child,
					guint            property_id,
					GValue          *value,
					GParamSpec      *pspec);
static GtkType gtk_box_child_type (GtkContainer   *container);
     

static GtkContainerClass *parent_class = NULL;


GtkType
gtk_box_get_type (void)
{
  static GtkType box_type = 0;

  if (!box_type)
    {
      static const GtkTypeInfo box_info =
      {
	"GtkBox",
	sizeof (GtkBox),
	sizeof (GtkBoxClass),
	(GtkClassInitFunc) gtk_box_class_init,
	(GtkObjectInitFunc) gtk_box_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      box_type = gtk_type_unique (GTK_TYPE_CONTAINER, &box_info);
    }

  return box_type;
}

static void
gtk_box_class_init (GtkBoxClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->set_property = gtk_box_set_property;
  gobject_class->get_property = gtk_box_get_property;
   
  container_class->add = gtk_box_add;
  container_class->remove = gtk_box_remove;
  container_class->forall = gtk_box_forall;
  container_class->child_type = gtk_box_child_type;
  container_class->set_child_property = gtk_box_set_child_property;
  container_class->get_child_property = gtk_box_get_child_property;

  g_object_class_install_property (gobject_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
                                                     _("Spacing"),
                                                     _("The amount of space between children."),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_HOMOGENEOUS,
                                   g_param_spec_boolean ("homogeneous",
							 _("Homogeneous"),
							 _("Whether the children should all be the same size."),
							 FALSE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_EXPAND,
					      g_param_spec_boolean ("expand", NULL, NULL,
								    TRUE,
								    G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_FILL,
					      g_param_spec_boolean ("fill", NULL, NULL,
								    TRUE,
								    G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PADDING,
					      g_param_spec_uint ("padding", NULL, NULL,
								 0, G_MAXINT, 0,
								 G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PACK_TYPE,
					      g_param_spec_enum ("pack_type", NULL, NULL,
								 GTK_TYPE_PACK_TYPE, GTK_PACK_START,
								 G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_POSITION,
					      g_param_spec_int ("position", NULL, NULL,
								-1, G_MAXINT, 0,
								G_PARAM_READWRITE));
}

static void
gtk_box_init (GtkBox *box)
{
  GTK_WIDGET_SET_FLAGS (box, GTK_NO_WINDOW);

  box->children = NULL;
  box->spacing = 0;
  box->homogeneous = FALSE;
}

static void 
gtk_box_set_property (GObject         *object,
		      guint            prop_id,
		      const GValue    *value,
		      GParamSpec      *pspec)
{
  GtkBox *box;

  box = GTK_BOX (object);

  switch (prop_id)
    {
    case PROP_SPACING:
      gtk_box_set_spacing (box, g_value_get_int (value));
      break;
    case PROP_HOMOGENEOUS:
      gtk_box_set_homogeneous (box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void gtk_box_get_property (GObject         *object,
				  guint            prop_id,
				  GValue          *value,
				  GParamSpec      *pspec)
{
  GtkBox *box;

  box = GTK_BOX (object);

  switch (prop_id)
    {
    case PROP_SPACING:
      g_value_set_int (value, box->spacing);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, box->homogeneous);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkType
gtk_box_child_type	(GtkContainer   *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_box_set_child_property (GtkContainer    *container,
			    GtkWidget       *child,
			    guint            property_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  gboolean expand = 0;
  gboolean fill = 0;
  guint padding = 0;
  GtkPackType pack_type = 0;

  if (property_id != CHILD_PROP_POSITION)
    gtk_box_query_child_packing (GTK_BOX (container),
				 child,
				 &expand,
				 &fill,
				 &padding,
				 &pack_type);
  switch (property_id)
    {
    case CHILD_PROP_EXPAND:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 g_value_get_boolean (value),
				 fill,
				 padding,
				 pack_type);
      break;
    case CHILD_PROP_FILL:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 expand,
				 g_value_get_boolean (value),
				 padding,
				 pack_type);
      break;
    case CHILD_PROP_PADDING:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 expand,
				 fill,
				 g_value_get_uint (value),
				 pack_type);
      break;
    case CHILD_PROP_PACK_TYPE:
      gtk_box_set_child_packing (GTK_BOX (container),
				 child,
				 expand,
				 fill,
				 padding,
				 g_value_get_enum (value));
      break;
    case CHILD_PROP_POSITION:
      gtk_box_reorder_child (GTK_BOX (container),
			     child,
			     g_value_get_int (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_box_get_child_property (GtkContainer *container,
			    GtkWidget    *child,
			    guint         property_id,
			    GValue       *value,
			    GParamSpec   *pspec)
{
  gboolean expand = 0;
  gboolean fill = 0;
  guint padding = 0;
  GtkPackType pack_type = 0;
  GList *list;

  if (property_id != CHILD_PROP_POSITION)
    gtk_box_query_child_packing (GTK_BOX (container),
				 child,
				 &expand,
				 &fill,
				 &padding,
				 &pack_type);
  switch (property_id)
    {
      guint i;
    case CHILD_PROP_EXPAND:
      g_value_set_boolean (value, expand);
      break;
    case CHILD_PROP_FILL:
      g_value_set_boolean (value, fill);
      break;
    case CHILD_PROP_PADDING:
      g_value_set_uint (value, padding);
      break;
    case CHILD_PROP_PACK_TYPE:
      g_value_set_enum (value, pack_type);
      break;
    case CHILD_PROP_POSITION:
      i = 0;
      for (list = GTK_BOX (container)->children; list; list = list->next)
	{
	  GtkBoxChild *child_entry;

	  child_entry = list->data;
	  if (child_entry->widget == child)
	    break;
	  i++;
	}
      g_value_set_int (value, list ? i : -1);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

void
gtk_box_pack_start (GtkBox    *box,
		    GtkWidget *child,
		    gboolean   expand,
		    gboolean   fill,
		    guint      padding)
{
  GtkBoxChild *child_info;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);
  g_return_if_fail (child->parent == NULL);

  child_info = g_new (GtkBoxChild, 1);
  child_info->widget = child;
  child_info->padding = padding;
  child_info->expand = expand ? TRUE : FALSE;
  child_info->fill = fill ? TRUE : FALSE;
  child_info->pack = GTK_PACK_START;
  child_info->is_secondary = FALSE;

  box->children = g_list_append (box->children, child_info);

  gtk_widget_freeze_child_notify (child);

  gtk_widget_set_parent (child, GTK_WIDGET (box));
  
  gtk_widget_child_notify (child, "expand");
  gtk_widget_child_notify (child, "fill");
  gtk_widget_child_notify (child, "padding");
  gtk_widget_child_notify (child, "pack_type");
  gtk_widget_child_notify (child, "position");
  gtk_widget_thaw_child_notify (child);
}

void
gtk_box_pack_end (GtkBox    *box,
		  GtkWidget *child,
		  gboolean   expand,
		  gboolean   fill,
		  guint      padding)
{
  GtkBoxChild *child_info;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);
  g_return_if_fail (child->parent == NULL);

  child_info = g_new (GtkBoxChild, 1);
  child_info->widget = child;
  child_info->padding = padding;
  child_info->expand = expand ? TRUE : FALSE;
  child_info->fill = fill ? TRUE : FALSE;
  child_info->pack = GTK_PACK_END;
  child_info->is_secondary = FALSE;

  box->children = g_list_append (box->children, child_info);

  gtk_widget_freeze_child_notify (child);

  gtk_widget_set_parent (child, GTK_WIDGET (box));

  gtk_widget_child_notify (child, "expand");
  gtk_widget_child_notify (child, "fill");
  gtk_widget_child_notify (child, "padding");
  gtk_widget_child_notify (child, "pack_type");
  gtk_widget_child_notify (child, "position");
  gtk_widget_thaw_child_notify (child);
}

void
gtk_box_pack_start_defaults (GtkBox    *box,
			     GtkWidget *child)
{
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  gtk_box_pack_start (box, child, TRUE, TRUE, 0);
}

void
gtk_box_pack_end_defaults (GtkBox    *box,
			   GtkWidget *child)
{
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  gtk_box_pack_end (box, child, TRUE, TRUE, 0);
}

void
gtk_box_set_homogeneous (GtkBox  *box,
			 gboolean homogeneous)
{
  g_return_if_fail (GTK_IS_BOX (box));

  if ((homogeneous ? TRUE : FALSE) != box->homogeneous)
    {
      box->homogeneous = homogeneous ? TRUE : FALSE;
      g_object_notify (G_OBJECT (box), "homogeneous");
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

/**
 * gtk_box_get_homogeneous:
 * @box: a #GtkBox
 *
 * Returns whether the box is homogeneous (all children are the
 * same size). See gtk_box_set_homogeneous ().
 *
 * Return value: %TRUE if the box is homogeneous.
 **/
gboolean
gtk_box_get_homogeneous (GtkBox *box)
{
  g_return_val_if_fail (GTK_IS_BOX (box), FALSE);

  return box->homogeneous;
}

void
gtk_box_set_spacing (GtkBox *box,
		     gint    spacing)
{
  g_return_if_fail (GTK_IS_BOX (box));

  if (spacing != box->spacing)
    {
      box->spacing = spacing;
      g_object_notify (G_OBJECT (box), "spacing");
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

/**
 * gtk_box_get_spacing:
 * @box: a #GtkBox
 * 
 * Gets the value set by gtk_box_set_spacing().
 * 
 * Return value: spacing between children
 **/
gint
gtk_box_get_spacing (GtkBox *box)
{
  g_return_val_if_fail (GTK_IS_BOX (box), 0);

  return box->spacing;
}

void
gtk_box_reorder_child (GtkBox    *box,
		       GtkWidget *child,
		       gint       position)
{
  GList *list;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  list = box->children;
  while (list)
    {
      GtkBoxChild *child_info;

      child_info = list->data;
      if (child_info->widget == child)
	break;

      list = list->next;
    }

  if (list && box->children->next)
    {
      GList *tmp_list;

      if (list->next)
	list->next->prev = list->prev;
      if (list->prev)
	list->prev->next = list->next;
      else
	box->children = list->next;

      tmp_list = box->children;
      while (position && tmp_list->next)
	{
	  position--;
	  tmp_list = tmp_list->next;
	}

      if (position)
	{
	  tmp_list->next = list;
	  list->prev = tmp_list;
	  list->next = NULL;
	}
      else
	{
	  if (tmp_list->prev)
	    tmp_list->prev->next = list;
	  else
	    box->children = list;
	  list->prev = tmp_list->prev;
	  tmp_list->prev = list;
	  list->next = tmp_list;
	}

      gtk_widget_child_notify (child, "position");
      if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (box))
	gtk_widget_queue_resize (child);
    }
}

void
gtk_box_query_child_packing (GtkBox             *box,
			     GtkWidget          *child,
			     gboolean           *expand,
			     gboolean           *fill,
			     guint              *padding,
			     GtkPackType        *pack_type)
{
  GList *list;
  GtkBoxChild *child_info = NULL;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  list = box->children;
  while (list)
    {
      child_info = list->data;
      if (child_info->widget == child)
	break;

      list = list->next;
    }

  if (list)
    {
      if (expand)
	*expand = child_info->expand;
      if (fill)
	*fill = child_info->fill;
      if (padding)
	*padding = child_info->padding;
      if (pack_type)
	*pack_type = child_info->pack;
    }
}

void
gtk_box_set_child_packing (GtkBox               *box,
			   GtkWidget            *child,
			   gboolean              expand,
			   gboolean              fill,
			   guint                 padding,
			   GtkPackType           pack_type)
{
  GList *list;
  GtkBoxChild *child_info = NULL;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  list = box->children;
  while (list)
    {
      child_info = list->data;
      if (child_info->widget == child)
	break;

      list = list->next;
    }

  gtk_widget_freeze_child_notify (child);
  if (list)
    {
      child_info->expand = expand != FALSE;
      gtk_widget_child_notify (child, "expand");
      child_info->fill = fill != FALSE;
      gtk_widget_child_notify (child, "fill");
      child_info->padding = padding;
      gtk_widget_child_notify (child, "padding");
      if (pack_type == GTK_PACK_END)
	child_info->pack = GTK_PACK_END;
      else
	child_info->pack = GTK_PACK_START;
      gtk_widget_child_notify (child, "pack_type");

      if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (box))
	gtk_widget_queue_resize (child);
    }
  gtk_widget_thaw_child_notify (child);
}

static void
gtk_box_add (GtkContainer *container,
	     GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_BOX (container));
  g_return_if_fail (widget != NULL);

  gtk_box_pack_start_defaults (GTK_BOX (container), widget);
}

static void
gtk_box_remove (GtkContainer *container,
		GtkWidget    *widget)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;

  g_return_if_fail (GTK_IS_BOX (container));
  g_return_if_fail (widget != NULL);

  box = GTK_BOX (container);

  children = box->children;
  while (children)
    {
      child = children->data;

      if (child->widget == widget)
	{
	  gboolean was_visible;

	  was_visible = GTK_WIDGET_VISIBLE (widget);
	  gtk_widget_unparent (widget);

	  box->children = g_list_remove_link (box->children, children);
	  g_list_free (children);
	  g_free (child);

	  /* queue resize regardless of GTK_WIDGET_VISIBLE (container),
	   * since that's what is needed by toplevels.
	   */
	  if (was_visible)
	    gtk_widget_queue_resize (GTK_WIDGET (container));

	  break;
	}

      children = children->next;
    }
}

static void
gtk_box_forall (GtkContainer *container,
		gboolean      include_internals,
		GtkCallback   callback,
		gpointer      callback_data)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;

  g_return_if_fail (GTK_IS_BOX (container));
  g_return_if_fail (callback != NULL);

  box = GTK_BOX (container);

  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (child->pack == GTK_PACK_START)
	(* callback) (child->widget, callback_data);
    }

  children = g_list_last (box->children);
  while (children)
    {
      child = children->data;
      children = children->prev;

      if (child->pack == GTK_PACK_END)
	(* callback) (child->widget, callback_data);
    }
}
