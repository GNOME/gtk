/* GTK - The GIMP Toolkit
 * gtksizegroup.c: 
 * Copyright (C) 2001 Red Hat Software
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

#include "config.h"
#include <string.h>
#include "gtkcontainer.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtksizegroup.h"
#include "gtkbuildable.h"
#include "gtkextendedlayout.h"
#include "gtkalias.h"

enum {
  PROP_0,
  PROP_MODE,
  PROP_IGNORE_HIDDEN
};

static void gtk_size_group_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec);
static void gtk_size_group_get_property (GObject      *object,
					 guint         prop_id,
					 GValue       *value,
					 GParamSpec   *pspec);

static void add_group_to_closure  (GtkSizeGroup      *group,
				   GtkSizeGroupMode   mode,
				   GSList           **groups,
				   GSList           **widgets);
static void add_widget_to_closure (GtkWidget         *widget,
				   GtkSizeGroupMode   mode,
				   GSList           **groups,
				   GSList           **widgets);

/* GtkBuildable */
static void gtk_size_group_buildable_init (GtkBuildableIface *iface);
static gboolean gtk_size_group_buildable_custom_tag_start (GtkBuildable  *buildable,
							   GtkBuilder    *builder,
							   GObject       *child,
							   const gchar   *tagname,
							   GMarkupParser *parser,
							   gpointer      *data);
static void gtk_size_group_buildable_custom_finished (GtkBuildable  *buildable,
						      GtkBuilder    *builder,
						      GObject       *child,
						      const gchar   *tagname,
						      gpointer       user_data);

static GQuark size_groups_quark;
static const gchar size_groups_tag[] = "gtk-size-groups";

static GQuark visited_quark;
static const gchar visited_tag[] = "gtk-size-group-visited";

static GQuark bumping_quark;
static const gchar bumping_tag[] = "gtk-size-group-bumping";

static GSList *
get_size_groups (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget), size_groups_quark);
}

static void
set_size_groups (GtkWidget *widget,
		 GSList    *groups)
{
  g_object_set_qdata (G_OBJECT (widget), size_groups_quark, groups);
}

static void
mark_visited (gpointer object)
{
  g_object_set_qdata (object, visited_quark, "visited");
}

static void
mark_unvisited (gpointer object)
{
  g_object_set_qdata (object, visited_quark, NULL);
}

static gboolean
is_visited (gpointer object)
{
  return g_object_get_qdata (object, visited_quark) != NULL;
}

static void
mark_bumping (gpointer object, gboolean bumping)
{
  g_object_set_qdata (object, bumping_quark, bumping ? "bumping" : NULL);
}

static gboolean
is_bumping (gpointer object)
{
  return g_object_get_qdata (object, bumping_quark) != NULL;
}

static void
add_group_to_closure (GtkSizeGroup    *group,
		      GtkSizeGroupMode mode,
		      GSList         **groups,
		      GSList         **widgets)
{
  GSList *tmp_widgets;
  
  *groups = g_slist_prepend (*groups, group);
  mark_visited (group);

  tmp_widgets = group->widgets;
  while (tmp_widgets)
    {
      GtkWidget *tmp_widget = tmp_widgets->data;
      
      if (!is_visited (tmp_widget))
	add_widget_to_closure (tmp_widget, mode, groups, widgets);
      
      tmp_widgets = tmp_widgets->next;
    }
}

static void
add_widget_to_closure (GtkWidget       *widget,
		       GtkSizeGroupMode mode,
		       GSList         **groups,
		       GSList         **widgets)
{
  GSList *tmp_groups;

  *widgets = g_slist_prepend (*widgets, widget);
  mark_visited (widget);

  tmp_groups = get_size_groups (widget);
  while (tmp_groups)
    {
      GtkSizeGroup *tmp_group = tmp_groups->data;
      
      if ((tmp_group->mode == GTK_SIZE_GROUP_BOTH || tmp_group->mode == mode) &&
	  !is_visited (tmp_group))
	add_group_to_closure (tmp_group, mode, groups, widgets);

      tmp_groups = tmp_groups->next;
    }
}

static void
real_queue_resize (GtkWidget *widget)
{
  GTK_PRIVATE_SET_FLAG (widget, GTK_ALLOC_NEEDED);
  GTK_PRIVATE_SET_FLAG (widget, GTK_REQUEST_NEEDED);
  GTK_PRIVATE_SET_FLAG (widget, GTK_WIDTH_REQUEST_NEEDED);
  GTK_PRIVATE_SET_FLAG (widget, GTK_HEIGHT_REQUEST_NEEDED);
  
  if (widget->parent)
    _gtk_container_queue_resize (GTK_CONTAINER (widget->parent));
  else if (gtk_widget_is_toplevel (widget) && GTK_IS_CONTAINER (widget))
    _gtk_container_queue_resize (GTK_CONTAINER (widget));
}

static void
reset_group_sizes (GSList *groups)
{
  GSList *tmp_list = groups;
  while (tmp_list)
    {
      GtkSizeGroup *tmp_group = tmp_list->data;

      tmp_group->have_width = FALSE;
      tmp_group->have_height = FALSE;
      
      tmp_list = tmp_list->next;
    }
}

static void
queue_resize_on_widget (GtkWidget *widget,
			gboolean   check_siblings)
{
  GtkWidget *parent = widget;
  GSList *tmp_list;

  while (parent)
    {
      GSList *widget_groups;
      GSList *groups;
      GSList *widgets;
      
      if (widget == parent && !check_siblings)
	{
	  real_queue_resize (widget);
	  parent = parent->parent;
	  continue;
	}
      
      widget_groups = get_size_groups (parent);
      if (!widget_groups)
	{
	  if (widget == parent)
	    real_queue_resize (widget);

	  parent = parent->parent;
	  continue;
	}

      groups = NULL;
      widgets = NULL;
	  
      add_widget_to_closure (parent, GTK_SIZE_GROUP_HORIZONTAL, &groups, &widgets);
      g_slist_foreach (widgets, (GFunc)mark_unvisited, NULL);
      g_slist_foreach (groups, (GFunc)mark_unvisited, NULL);

      reset_group_sizes (groups);
	      
      tmp_list = widgets;
      while (tmp_list)
	{
	  if (tmp_list->data == parent)
	    {
	      if (widget == parent)
		real_queue_resize (parent);
	    }
	  else if (tmp_list->data == widget)
            {
              g_warning ("A container and its child are part of this SizeGroup");
            }
	  else
	    queue_resize_on_widget (tmp_list->data, FALSE);

	  tmp_list = tmp_list->next;
	}
      
      g_slist_free (widgets);
      g_slist_free (groups);
	      
      groups = NULL;
      widgets = NULL;
	      
      add_widget_to_closure (parent, GTK_SIZE_GROUP_VERTICAL, &groups, &widgets);
      g_slist_foreach (widgets, (GFunc)mark_unvisited, NULL);
      g_slist_foreach (groups, (GFunc)mark_unvisited, NULL);

      reset_group_sizes (groups);
	      
      tmp_list = widgets;
      while (tmp_list)
	{
	  if (tmp_list->data == parent)
	    {
	      if (widget == parent)
		real_queue_resize (parent);
	    }
	  else if (tmp_list->data == widget)
            {
              g_warning ("A container and its child are part of this SizeGroup");
            }
	  else
	    queue_resize_on_widget (tmp_list->data, FALSE);

	  tmp_list = tmp_list->next;
	}
      
      g_slist_free (widgets);
      g_slist_free (groups);
      
      parent = parent->parent;
    }
}

static void
queue_resize_on_group (GtkSizeGroup *size_group)
{
  if (size_group->widgets)
    queue_resize_on_widget (size_group->widgets->data, TRUE);
}

static void
initialize_size_group_quarks (void)
{
  if (!size_groups_quark)
    {
      size_groups_quark = g_quark_from_static_string (size_groups_tag);
      visited_quark = g_quark_from_static_string (visited_tag);
      bumping_quark = g_quark_from_static_string (bumping_tag);
    }
}

static void
gtk_size_group_class_init (GtkSizeGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_size_group_set_property;
  gobject_class->get_property = gtk_size_group_get_property;
  
  g_object_class_install_property (gobject_class,
				   PROP_MODE,
				   g_param_spec_enum ("mode",
						      P_("Mode"),
						      P_("The directions in which the size group affects the requested sizes"
							" of its component widgets"),
						      GTK_TYPE_SIZE_GROUP_MODE,
						      GTK_SIZE_GROUP_HORIZONTAL,						      GTK_PARAM_READWRITE));

  /**
   * GtkSizeGroup:ignore-hidden:
   *
   * If %TRUE, unmapped widgets are ignored when determining 
   * the size of the group.
   *
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
				   PROP_IGNORE_HIDDEN,
				   g_param_spec_boolean ("ignore-hidden",
							 P_("Ignore hidden"),
							 P_("If TRUE, unmapped widgets are ignored "
							    "when determining the size of the group"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  
  initialize_size_group_quarks ();
}

static void
gtk_size_group_init (GtkSizeGroup *size_group)
{
  size_group->widgets = NULL;
  size_group->mode = GTK_SIZE_GROUP_HORIZONTAL;
  size_group->have_width = 0;
  size_group->have_height = 0;
  size_group->ignore_hidden = 0;
}

static void
gtk_size_group_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_size_group_buildable_custom_tag_start;
  iface->custom_finished = gtk_size_group_buildable_custom_finished;
}

G_DEFINE_TYPE_WITH_CODE (GtkSizeGroup, gtk_size_group, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_size_group_buildable_init))

static void
gtk_size_group_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  GtkSizeGroup *size_group = GTK_SIZE_GROUP (object);

  switch (prop_id)
    {
    case PROP_MODE:
      gtk_size_group_set_mode (size_group, g_value_get_enum (value));
      break;
    case PROP_IGNORE_HIDDEN:
      gtk_size_group_set_ignore_hidden (size_group, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_size_group_get_property (GObject      *object,
			     guint         prop_id,
			     GValue       *value,
			     GParamSpec   *pspec)
{
  GtkSizeGroup *size_group = GTK_SIZE_GROUP (object);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, size_group->mode);
      break;
    case PROP_IGNORE_HIDDEN:
      g_value_set_boolean (value, size_group->ignore_hidden);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_size_group_new:
 * @mode: the mode for the new size group.
 * 
 * Create a new #GtkSizeGroup.
 
 * Return value: a newly created #GtkSizeGroup
 **/
GtkSizeGroup *
gtk_size_group_new (GtkSizeGroupMode mode)
{
  GtkSizeGroup *size_group = g_object_new (GTK_TYPE_SIZE_GROUP, NULL);

  size_group->mode = mode;

  return size_group;
}

/**
 * gtk_size_group_set_mode:
 * @size_group: a #GtkSizeGroup
 * @mode: the mode to set for the size group.
 * 
 * Sets the #GtkSizeGroupMode of the size group. The mode of the size
 * group determines whether the widgets in the size group should
 * all have the same horizontal requisition (%GTK_SIZE_GROUP_MODE_HORIZONTAL)
 * all have the same vertical requisition (%GTK_SIZE_GROUP_MODE_VERTICAL),
 * or should all have the same requisition in both directions
 * (%GTK_SIZE_GROUP_MODE_BOTH).
 **/
void
gtk_size_group_set_mode (GtkSizeGroup     *size_group,
			 GtkSizeGroupMode  mode)
{
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));

  if (size_group->mode != mode)
    {
      if (size_group->mode != GTK_SIZE_GROUP_NONE)
	queue_resize_on_group (size_group);
      size_group->mode = mode;
      if (size_group->mode != GTK_SIZE_GROUP_NONE)
	queue_resize_on_group (size_group);

      g_object_notify (G_OBJECT (size_group), "mode");
    }
}

/**
 * gtk_size_group_get_mode:
 * @size_group: a #GtkSizeGroup
 * 
 * Gets the current mode of the size group. See gtk_size_group_set_mode().
 * 
 * Return value: the current mode of the size group.
 **/
GtkSizeGroupMode
gtk_size_group_get_mode (GtkSizeGroup *size_group)
{
  g_return_val_if_fail (GTK_IS_SIZE_GROUP (size_group), GTK_SIZE_GROUP_BOTH);

  return size_group->mode;
}

/**
 * gtk_size_group_set_ignore_hidden:
 * @size_group: a #GtkSizeGroup
 * @ignore_hidden: whether unmapped widgets should be ignored
 *   when calculating the size
 * 
 * Sets whether unmapped widgets should be ignored when
 * calculating the size.
 *
 * Since: 2.8 
 */
void
gtk_size_group_set_ignore_hidden (GtkSizeGroup *size_group,
				  gboolean      ignore_hidden)
{
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  
  ignore_hidden = ignore_hidden != FALSE;

  if (size_group->ignore_hidden != ignore_hidden)
    {
      size_group->ignore_hidden = ignore_hidden;

      g_object_notify (G_OBJECT (size_group), "ignore-hidden");
    }
}

/**
 * gtk_size_group_get_ignore_hidden:
 * @size_group: a #GtkSizeGroup
 *
 * Returns if invisible widgets are ignored when calculating the size.
 *
 * Returns: %TRUE if invisible widgets are ignored.
 *
 * Since: 2.8
 */
gboolean
gtk_size_group_get_ignore_hidden (GtkSizeGroup *size_group)
{
  g_return_val_if_fail (GTK_IS_SIZE_GROUP (size_group), FALSE);

  return size_group->ignore_hidden;
}

static void
gtk_size_group_widget_destroyed (GtkWidget    *widget,
				 GtkSizeGroup *size_group)
{
  gtk_size_group_remove_widget (size_group, widget);
}

/**
 * gtk_size_group_add_widget:
 * @size_group: a #GtkSizeGroup
 * @widget: the #GtkWidget to add
 * 
 * Adds a widget to a #GtkSizeGroup. In the future, the requisition
 * of the widget will be determined as the maximum of its requisition
 * and the requisition of the other widgets in the size group.
 * Whether this applies horizontally, vertically, or in both directions
 * depends on the mode of the size group. See gtk_size_group_set_mode().
 *
 * When the widget is destroyed or no longer referenced elsewhere, it will 
 * be removed from the size group.
 */
void
gtk_size_group_add_widget (GtkSizeGroup     *size_group,
			   GtkWidget        *widget)
{
  GSList *groups;
  
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  groups = get_size_groups (widget);

  if (!g_slist_find (groups, size_group))
    {
      groups = g_slist_prepend (groups, size_group);
      set_size_groups (widget, groups);

      size_group->widgets = g_slist_prepend (size_group->widgets, widget);

      g_signal_connect (widget, "destroy",
			G_CALLBACK (gtk_size_group_widget_destroyed),
			size_group);

      g_object_ref (size_group);
    }
  
  queue_resize_on_group (size_group);
}

/**
 * gtk_size_group_remove_widget:
 * @size_group: a #GtkSizeGrup
 * @widget: the #GtkWidget to remove
 * 
 * Removes a widget from a #GtkSizeGroup.
 **/
void
gtk_size_group_remove_widget (GtkSizeGroup *size_group,
			      GtkWidget    *widget)
{
  GSList *groups;
  
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (g_slist_find (size_group->widgets, widget));

  g_signal_handlers_disconnect_by_func (widget,
					gtk_size_group_widget_destroyed,
					size_group);
  
  groups = get_size_groups (widget);
  groups = g_slist_remove (groups, size_group);
  set_size_groups (widget, groups);

  size_group->widgets = g_slist_remove (size_group->widgets, widget);
  queue_resize_on_group (size_group);
  gtk_widget_queue_resize (widget);

  g_object_unref (size_group);
}

/**
 * gtk_size_group_get_widgets:
 * @size_group: a #GtkSizeGrup
 * 
 * Returns the list of widgets associated with @size_group.
 *
 * Return value:  (element-type GtkWidget) (transfer none): a #GSList of
 *   widgets. The list is owned by GTK+ and should not be modified.
 *
 * Since: 2.10
 **/
GSList *
gtk_size_group_get_widgets (GtkSizeGroup *size_group)
{
  return size_group->widgets;
}

static gint
get_base_dimension (GtkWidget        *widget,
		    GtkSizeGroupMode  mode)
{
  GtkWidgetAuxInfo *aux_info = _gtk_widget_get_aux_info (widget, FALSE);

  if (mode == GTK_SIZE_GROUP_HORIZONTAL)
    {
      if (aux_info && aux_info->width > 0)
	return aux_info->width;
      else
	{
	  /* XXX Possibly we should be using natural values and not minimums here. */
	  gint width;

	  gtk_extended_layout_get_desired_width (GTK_EXTENDED_LAYOUT (widget), &width, NULL);

	  return width;
	}
    }
  else
    {
      if (aux_info && aux_info->height > 0)
	return aux_info->height;
      else
	{
	  /* XXX Possibly we should be using natural values and not minimums here. */
	  gint height;

	  gtk_extended_layout_get_desired_height (GTK_EXTENDED_LAYOUT (widget), &height, NULL);

	  return height;
	}
    }
}

static gint
compute_dimension (GtkWidget        *widget,
		   GtkSizeGroupMode  mode,
		   gint              widget_requisition)
{
  GSList *widgets = NULL;
  GSList *groups = NULL;
  GSList *tmp_list;
  gint result = 0;

  add_widget_to_closure (widget, mode, &groups, &widgets);

  g_slist_foreach (widgets, (GFunc)mark_unvisited, NULL);
  g_slist_foreach (groups, (GFunc)mark_unvisited, NULL);
  
  g_slist_foreach (widgets, (GFunc)g_object_ref, NULL);
  
  if (!groups)
    {
      result = widget_requisition;
    }
  else
    {
      GtkSizeGroup *group = groups->data;

      if (mode == GTK_SIZE_GROUP_HORIZONTAL && group->have_width)
	result = group->requisition.width;
      else if (mode == GTK_SIZE_GROUP_VERTICAL && group->have_height)
	result = group->requisition.height;
      else
	{
	  tmp_list = widgets;
	  while (tmp_list)
	    {
	      GtkWidget *tmp_widget = tmp_list->data;
	      gint dimension;

	      if (tmp_widget == widget)
		dimension = widget_requisition;
	      else
		dimension = get_base_dimension (tmp_widget, mode);

	      if (gtk_widget_get_mapped (tmp_widget) || !group->ignore_hidden)
		{
		  if (dimension > result)
		    result = dimension;
		}

	      tmp_list = tmp_list->next;
	    }

	  tmp_list = groups;
	  while (tmp_list)
	    {
	      GtkSizeGroup *tmp_group = tmp_list->data;

	      if (mode == GTK_SIZE_GROUP_HORIZONTAL)
		{
		  tmp_group->have_width = TRUE;
		  tmp_group->requisition.width = result;
		}
	      else
		{
		  tmp_group->have_height = TRUE;
		  tmp_group->requisition.height = result;
		}
	      
	      tmp_list = tmp_list->next;
	    }
	}
    }

  g_slist_foreach (widgets, (GFunc)g_object_unref, NULL);

  g_slist_free (widgets);
  g_slist_free (groups);

  return result;
}

/**
 * _gtk_size_group_bump_requisition:
 * @widget: a #GtkWidget
 * @mode: either %GTK_SIZE_GROUP_HORIZONTAL or %GTK_SIZE_GROUP_VERTICAL, depending
 *        on the dimension in which to bump the size.
 *
 * Refreshes the sizegroup while returning the groups requested
 * value in the dimension @mode.
 *
 * This function is used to update sizegroup minimum size information
 * in multiple passes from the new #GtkExtendedLayout manager.
 */
gint
_gtk_size_group_bump_requisition (GtkWidget        *widget,
				  GtkSizeGroupMode  mode,
				  gint              widget_requisition)
{
  gint result = widget_requisition;

  if (!is_bumping (widget))
    {
      GtkWidgetAuxInfo *aux_info = 
	_gtk_widget_get_aux_info (widget, FALSE);

      /* Avoid recursion here */
      mark_bumping (widget, TRUE);

      if (get_size_groups (widget))
	{
	  if (aux_info)
	    {
	      if (mode == GTK_SIZE_GROUP_HORIZONTAL)
		result = compute_dimension (widget, mode, MAX (aux_info->width, widget_requisition));
	      else 
		result = compute_dimension (widget, mode, MAX (aux_info->height, widget_requisition));
	    }
	  else
	    result = compute_dimension (widget, mode, widget_requisition);
	}
      else if (aux_info)
	{
	  if (mode == GTK_SIZE_GROUP_HORIZONTAL)
	    result = MAX (aux_info->width, widget_requisition);
	  else 
	    result = MAX (aux_info->height, widget_requisition);
	}
      mark_bumping (widget, FALSE);
    }
  return result;
}



/**
 * _gtk_size_group_queue_resize:
 * @widget: a #GtkWidget
 * 
 * Queue a resize on a widget, and on all other widgets grouped with this widget.
 **/
void
_gtk_size_group_queue_resize (GtkWidget *widget)
{
  initialize_size_group_quarks ();

  queue_resize_on_widget (widget, TRUE);
}

typedef struct {
  GObject *object;
  GSList *items;
} GSListSubParserData;

static void
size_group_start_element (GMarkupParseContext *context,
			  const gchar         *element_name,
			  const gchar        **names,
			  const gchar        **values,
			  gpointer            user_data,
			  GError            **error)
{
  guint i;
  GSListSubParserData *data = (GSListSubParserData*)user_data;

  if (strcmp (element_name, "widget") == 0)
    for (i = 0; names[i]; i++)
      if (strcmp (names[i], "name") == 0)
	data->items = g_slist_prepend (data->items, g_strdup (values[i]));
  else if (strcmp (element_name, "widgets") == 0)
    return;
  else
    g_warning ("Unsupported type tag for GtkSizeGroup: %s\n",
	       element_name);

}

static const GMarkupParser size_group_parser =
  {
    size_group_start_element
  };

static gboolean
gtk_size_group_buildable_custom_tag_start (GtkBuildable  *buildable,
					   GtkBuilder    *builder,
					   GObject       *child,
					   const gchar   *tagname,
					   GMarkupParser *parser,
					   gpointer      *data)
{
  GSListSubParserData *parser_data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "widgets") == 0)
    {
      parser_data = g_slice_new0 (GSListSubParserData);
      parser_data->items = NULL;
      parser_data->object = G_OBJECT (buildable);

      *parser = size_group_parser;
      *data = parser_data;
      return TRUE;
    }

  return FALSE;
}

static void
gtk_size_group_buildable_custom_finished (GtkBuildable  *buildable,
					  GtkBuilder    *builder,
					  GObject       *child,
					  const gchar   *tagname,
					  gpointer       user_data)
{
  GSList *l;
  GSListSubParserData *data;
  GObject *object;

  if (strcmp (tagname, "widgets"))
    return;
  
  data = (GSListSubParserData*)user_data;
  data->items = g_slist_reverse (data->items);

  for (l = data->items; l; l = l->next)
    {
      object = gtk_builder_get_object (builder, l->data);
      if (!object)
	{
	  g_warning ("Unknown object %s specified in sizegroup %s",
		     (const gchar*)l->data,
		     gtk_buildable_get_name (GTK_BUILDABLE (data->object)));
	  continue;
	}
      gtk_size_group_add_widget (GTK_SIZE_GROUP (data->object),
				 GTK_WIDGET (object));
      g_free (l->data);
    }
  g_slist_free (data->items);
  g_slice_free (GSListSubParserData, data);
}


#define __GTK_SIZE_GROUP_C__
#include "gtkaliasdef.c"
