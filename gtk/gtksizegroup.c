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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <string.h>

#include "gtkbuildable.h"
#include "gtkcontainer.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtksizegroup-private.h"
#include "gtksizerequestcacheprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcontainerprivate.h"


/**
 * SECTION:gtksizegroup
 * @Short_description: Grouping widgets so they request the same size
 * @Title: GtkSizeGroup
 *
 * #GtkSizeGroup provides a mechanism for grouping a number of widgets
 * together so they all request the same amount of space.  This is
 * typically useful when you want a column of widgets to have the same
 * size, but you can't use a #GtkGrid widget.
 *
 * In detail, the size requested for each widget in a #GtkSizeGroup is
 * the maximum of the sizes that would have been requested for each
 * widget in the size group if they were not in the size group. The mode
 * of the size group (see gtk_size_group_set_mode()) determines whether
 * this applies to the horizontal size, the vertical size, or both sizes.
 *
 * Note that size groups only affect the amount of space requested, not
 * the size that the widgets finally receive. If you want the widgets in
 * a #GtkSizeGroup to actually be the same size, you need to pack them in
 * such a way that they get the size they request and not more. For
 * example, if you are packing your widgets into a table, you would not
 * include the %GTK_FILL flag.
 *
 * #GtkSizeGroup objects are referenced by each widget in the size group,
 * so once you have added all widgets to a #GtkSizeGroup, you can drop
 * the initial reference to the size group with g_object_unref(). If the
 * widgets in the size group are subsequently destroyed, then they will
 * be removed from the size group and drop their references on the size
 * group; when all widgets have been removed, the size group will be
 * freed.
 *
 * Widgets can be part of multiple size groups; GTK+ will compute the
 * horizontal size of a widget from the horizontal requisition of all
 * widgets that can be reached from the widget by a chain of size groups
 * of type %GTK_SIZE_GROUP_HORIZONTAL or %GTK_SIZE_GROUP_BOTH, and the
 * vertical size from the vertical requisition of all widgets that can be
 * reached from the widget by a chain of size groups of type
 * %GTK_SIZE_GROUP_VERTICAL or %GTK_SIZE_GROUP_BOTH.
 *
 * Note that only non-contextual sizes of every widget are ever consulted
 * by size groups (since size groups have no knowledge of what size a widget
 * will be allocated in one dimension, it cannot derive how much height
 * a widget will receive for a given width). When grouping widgets that
 * trade height for width in mode %GTK_SIZE_GROUP_VERTICAL or %GTK_SIZE_GROUP_BOTH:
 * the height for the minimum width will be the requested height for all
 * widgets in the group. The same is of course true when horizontally grouping
 * width for height widgets.
 *
 * Widgets that trade height-for-width should set a reasonably large minimum width
 * by way of #GtkLabel:width-chars for instance. Widgets with static sizes as well
 * as widgets that grow (such as ellipsizing text) need no such considerations.
 *
 * <refsect2 id="GtkSizeGroup-BUILDER-UI">
 * <title>GtkSizeGroup as GtkBuildable</title>
 * <para>
 * Size groups can be specified in a UI definition by placing an
 * &lt;object&gt; element with <literal>class="GtkSizeGroup"</literal>
 * somewhere in the UI definition. The widgets that belong to the
 * size group are specified by a &lt;widgets&gt; element that may
 * contain multiple &lt;widget&gt; elements, one for each member
 * of the size group. The name attribute gives the id of the widget.
 *
 * <example>
 * <title>A UI definition fragment with GtkSizeGroup</title>
 * <programlisting><![CDATA[
 * <object class="GtkSizeGroup">
 *   <property name="mode">GTK_SIZE_GROUP_HORIZONTAL</property>
 *   <widgets>
 *     <widget name="radio1"/>
 *     <widget name="radio2"/>
 *   </widgets>
 * </object>
 * ]]></programlisting>
 * </example>
 * </para>
 * </refsect2>
 */


struct _GtkSizeGroupPrivate
{
  GSList         *widgets;

  guint8          mode;

  guint           ignore_hidden : 1;
};

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

G_STATIC_ASSERT (GTK_SIZE_GROUP_HORIZONTAL == (1 << GTK_ORIENTATION_HORIZONTAL));
G_STATIC_ASSERT (GTK_SIZE_GROUP_VERTICAL == (1 << GTK_ORIENTATION_VERTICAL));
G_STATIC_ASSERT (GTK_SIZE_GROUP_BOTH == (GTK_SIZE_GROUP_HORIZONTAL | GTK_SIZE_GROUP_VERTICAL));

G_DEFINE_TYPE_WITH_CODE (GtkSizeGroup, gtk_size_group, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkSizeGroup)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_size_group_buildable_init))

static void
add_widget_to_closure (GHashTable     *widgets,
                       GHashTable     *groups,
                       GtkWidget      *widget,
		       GtkOrientation  orientation)
{
  GSList *tmp_groups, *tmp_widgets;
  gboolean hidden;

  if (g_hash_table_lookup (widgets, widget))
    return;

  g_hash_table_add (widgets, widget);
  hidden = !gtk_widget_is_visible (widget);

  for (tmp_groups = _gtk_widget_get_sizegroups (widget); tmp_groups; tmp_groups = tmp_groups->next)
    {
      GtkSizeGroup        *tmp_group = tmp_groups->data;
      GtkSizeGroupPrivate *tmp_priv  = tmp_group->priv;

      if (g_hash_table_lookup (groups, tmp_group))
        continue;

      if (tmp_priv->ignore_hidden && hidden)
        continue;

      if (!(tmp_priv->mode & (1 << orientation)))
        continue;

      g_hash_table_add (groups, tmp_group);

      for (tmp_widgets = tmp_priv->widgets; tmp_widgets; tmp_widgets = tmp_widgets->next)
        add_widget_to_closure (widgets, groups, tmp_widgets->data, orientation);
    }
}

GHashTable *
_gtk_size_group_get_widget_peers (GtkWidget      *for_widget,
                                  GtkOrientation  orientation)
{
  GHashTable *widgets, *groups;

  widgets = g_hash_table_new (g_direct_hash, g_direct_equal);
  groups = g_hash_table_new (g_direct_hash, g_direct_equal);

  add_widget_to_closure (widgets, groups, for_widget, orientation);

  g_hash_table_unref (groups);

  return widgets;
}
                                     
static void
real_queue_resize (GtkWidget          *widget,
		   GtkQueueResizeFlags flags)
{
  GtkWidget *container;

  _gtk_widget_set_alloc_needed (widget, TRUE);
  _gtk_size_request_cache_clear (_gtk_widget_peek_request_cache (widget));

  container = gtk_widget_get_parent (widget);
  if (!container &&
      gtk_widget_is_toplevel (widget) && GTK_IS_CONTAINER (widget))
    container = widget;

  if (container)
    {
      if (flags & GTK_QUEUE_RESIZE_INVALIDATE_ONLY)
	_gtk_container_resize_invalidate (GTK_CONTAINER (container));
      else
	_gtk_container_queue_resize (GTK_CONTAINER (container));
    }
}

static void
queue_resize_on_widget (GtkWidget          *widget,
			gboolean            check_siblings,
			GtkQueueResizeFlags flags)
{
  GtkWidget *parent = widget;

  while (parent)
    {
      GSList *widget_groups;
      GHashTable *widgets;
      GHashTableIter iter;
      gpointer current;
      
      if (widget == parent && !check_siblings)
	{
	  real_queue_resize (widget, flags);
          parent = gtk_widget_get_parent (parent);
	  continue;
	}
      
      widget_groups = _gtk_widget_get_sizegroups (parent);
      if (!widget_groups)
	{
	  if (widget == parent)
	    real_queue_resize (widget, flags);

          parent = gtk_widget_get_parent (parent);
	  continue;
	}

      widgets = _gtk_size_group_get_widget_peers (parent, GTK_ORIENTATION_HORIZONTAL);

      g_hash_table_iter_init (&iter, widgets);
      while (g_hash_table_iter_next (&iter, &current, NULL))
	{
	  if (current == parent)
	    {
	      if (widget == parent)
		real_queue_resize (parent, flags);
	    }
	  else if (current == widget)
            {
              g_warning ("A container and its child are part of this SizeGroup");
            }
	  else
	    queue_resize_on_widget (current, FALSE, flags);
	}
      
      g_hash_table_destroy (widgets);
      
      widgets = _gtk_size_group_get_widget_peers (parent, GTK_ORIENTATION_VERTICAL);

      g_hash_table_iter_init (&iter, widgets);
      while (g_hash_table_iter_next (&iter, &current, NULL))
	{
	  if (current == parent)
	    {
	      if (widget == parent)
		real_queue_resize (parent, flags);
	    }
	  else if (current == widget)
            {
              g_warning ("A container and its child are part of this SizeGroup");
            }
	  else
	    queue_resize_on_widget (current, FALSE, flags);
	}
      
      g_hash_table_destroy (widgets);

      parent = gtk_widget_get_parent (parent);
    }
}

static void
queue_resize_on_group (GtkSizeGroup       *size_group)
{
  GtkSizeGroupPrivate *priv = size_group->priv;

  if (priv->widgets)
    queue_resize_on_widget (priv->widgets->data, TRUE, 0);
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
}

static void
gtk_size_group_init (GtkSizeGroup *size_group)
{
  GtkSizeGroupPrivate *priv;

  size_group->priv = gtk_size_group_get_instance_private (size_group);
  priv = size_group->priv;

  priv->widgets = NULL;
  priv->mode = GTK_SIZE_GROUP_HORIZONTAL;
  priv->ignore_hidden = FALSE;
}

static void
gtk_size_group_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_size_group_buildable_custom_tag_start;
  iface->custom_finished = gtk_size_group_buildable_custom_finished;
}

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
  GtkSizeGroupPrivate *priv = size_group->priv;

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, priv->mode);
      break;
    case PROP_IGNORE_HIDDEN:
      g_value_set_boolean (value, priv->ignore_hidden);
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
  GtkSizeGroupPrivate *priv = size_group->priv;

  priv->mode = mode;

  return size_group;
}

/**
 * gtk_size_group_set_mode:
 * @size_group: a #GtkSizeGroup
 * @mode: the mode to set for the size group.
 * 
 * Sets the #GtkSizeGroupMode of the size group. The mode of the size
 * group determines whether the widgets in the size group should
 * all have the same horizontal requisition (%GTK_SIZE_GROUP_HORIZONTAL)
 * all have the same vertical requisition (%GTK_SIZE_GROUP_VERTICAL),
 * or should all have the same requisition in both directions
 * (%GTK_SIZE_GROUP_BOTH).
 **/
void
gtk_size_group_set_mode (GtkSizeGroup     *size_group,
			 GtkSizeGroupMode  mode)
{
  GtkSizeGroupPrivate *priv;

  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));

  priv = size_group->priv;

  if (priv->mode != mode)
    {
      if (priv->mode != GTK_SIZE_GROUP_NONE)
	queue_resize_on_group (size_group);
      priv->mode = mode;
      if (priv->mode != GTK_SIZE_GROUP_NONE)
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

  return size_group->priv->mode;
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
  GtkSizeGroupPrivate *priv;

  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));

  priv = size_group->priv;

  ignore_hidden = ignore_hidden != FALSE;

  if (priv->ignore_hidden != ignore_hidden)
    {
      priv->ignore_hidden = ignore_hidden;

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

  return size_group->priv->ignore_hidden;
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
  GtkSizeGroupPrivate *priv;
  GSList *groups;
  
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = size_group->priv;

  groups = _gtk_widget_get_sizegroups (widget);

  if (!g_slist_find (groups, size_group))
    {
      _gtk_widget_add_sizegroup (widget, size_group);

      priv->widgets = g_slist_prepend (priv->widgets, widget);

      g_signal_connect (widget, "destroy",
			G_CALLBACK (gtk_size_group_widget_destroyed),
			size_group);

      g_object_ref (size_group);
    }
  
  queue_resize_on_group (size_group);
}

/**
 * gtk_size_group_remove_widget:
 * @size_group: a #GtkSizeGroup
 * @widget: the #GtkWidget to remove
 * 
 * Removes a widget from a #GtkSizeGroup.
 **/
void
gtk_size_group_remove_widget (GtkSizeGroup *size_group,
			      GtkWidget    *widget)
{
  GtkSizeGroupPrivate *priv;
  
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = size_group->priv;

  g_return_if_fail (g_slist_find (priv->widgets, widget));

  g_signal_handlers_disconnect_by_func (widget,
					gtk_size_group_widget_destroyed,
					size_group);
  
  _gtk_widget_remove_sizegroup (widget, size_group);

  priv->widgets = g_slist_remove (priv->widgets, widget);
  queue_resize_on_group (size_group);
  gtk_widget_queue_resize (widget);

  g_object_unref (size_group);
}

/**
 * gtk_size_group_get_widgets:
 * @size_group: a #GtkSizeGroup
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
  return size_group->priv->widgets;
}

/**
 * _gtk_size_group_queue_resize:
 * @widget: a #GtkWidget
 * 
 * Queue a resize on a widget, and on all other widgets grouped with this widget.
 **/
void
_gtk_size_group_queue_resize (GtkWidget           *widget,
			      GtkQueueResizeFlags  flags)
{
  queue_resize_on_widget (widget, TRUE, flags);
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
    {
      for (i = 0; names[i]; i++)
        {
          if (strcmp (names[i], "name") == 0)
            data->items = g_slist_prepend (data->items, g_strdup (values[i]));
        }
    }
  else if (strcmp (element_name, "widgets") == 0)
    {
      return;
    }
  else
    {
      g_warning ("Unsupported type tag for GtkSizeGroup: %s\n",
                 element_name);
    }
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
