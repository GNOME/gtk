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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkadjustment.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtkroot.h"

#include "a11y/gtkcontaineraccessibleprivate.h"

#include <gobject/gobjectnotifyqueue.c>
#include <gobject/gvaluecollector.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/**
 * SECTION:gtkcontainer
 * @Short_description: Base class for widgets which contain other widgets
 * @Title: GtkContainer
 *
 * A GTK user interface is constructed by nesting widgets inside widgets.
 * Container widgets are the inner nodes in the resulting tree of widgets:
 * they contain other widgets. So, for example, you might have a #GtkWindow
 * containing a #GtkFrame containing a #GtkLabel. If you wanted an image instead
 * of a textual label inside the frame, you might replace the #GtkLabel widget
 * with a #GtkImage widget.
 *
 * There are two major kinds of container widgets in GTK. Both are subclasses
 * of the abstract GtkContainer base class.
 *
 * The first type of container widget has a single child widget and derives
 * from #GtkBin. These containers are decorators, which
 * add some kind of functionality to the child. For example, a #GtkButton makes
 * its child into a clickable button; a #GtkFrame draws a frame around its child
 * and a #GtkWindow places its child widget inside a top-level window.
 *
 * The second type of container can have more than one child; its purpose is to
 * manage layout. This means that these containers assign
 * sizes and positions to their children. For example, a horizontal #GtkBox arranges its
 * children in a horizontal row, and a #GtkGrid arranges the widgets it contains
 * in a two-dimensional grid.
 *
 * For implementations of #GtkContainer the virtual method #GtkContainerClass.forall()
 * is always required, since it's used for drawing and other internal operations
 * on the children.
 * If the #GtkContainer implementation expect to have non internal children
 * it's needed to implement both #GtkContainerClass.add() and #GtkContainerClass.remove().
 * If the GtkContainer implementation has internal children, they should be added
 * with gtk_widget_set_parent() on init() and removed with gtk_widget_unparent()
 * in the #GtkWidgetClass.destroy() implementation.
 *
 * See more about implementing custom widgets at https://wiki.gnome.org/HowDoI/CustomWidgets
 */

enum {
  ADD,
  REMOVE,
  LAST_SIGNAL
};

/* --- prototypes --- */
static void     gtk_container_dispose              (GObject *object);
static void     gtk_container_add_unimplemented    (GtkContainer      *container,
                                                    GtkWidget         *widget);
static void     gtk_container_remove_unimplemented (GtkContainer      *container,
                                                    GtkWidget         *widget);
static void     gtk_container_compute_expand       (GtkWidget         *widget,
                                                    gboolean          *hexpand_p,
                                                    gboolean          *vexpand_p);
static void     gtk_container_children_callback    (GtkWidget         *widget,
                                                    gpointer           client_data);
static GtkSizeRequestMode gtk_container_get_request_mode (GtkWidget   *widget);

/* GtkBuildable */
static void gtk_container_buildable_init           (GtkBuildableIface *iface);
static GtkBuildableIface    *parent_buildable_iface;

static guint                 container_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkContainer, gtk_container, GTK_TYPE_WIDGET,
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                         gtk_container_buildable_init))

static void
gtk_container_class_init (GtkContainerClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->dispose = gtk_container_dispose;

  widget_class->compute_expand = gtk_container_compute_expand;
  widget_class->get_request_mode = gtk_container_get_request_mode;

  class->add = gtk_container_add_unimplemented;
  class->remove = gtk_container_remove_unimplemented;
  class->forall = NULL;
  class->child_type = NULL;

  container_signals[ADD] =
    g_signal_new (I_("add"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkContainerClass, add),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);
  container_signals[REMOVE] =
    g_signal_new (I_("remove"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkContainerClass, remove),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_CONTAINER_ACCESSIBLE);
}

static void
gtk_container_buildable_add_child (GtkBuildable  *buildable,
                                   GtkBuilder    *builder,
                                   GObject       *child,
                                   const gchar   *type)
{
  if (GTK_IS_WIDGET (child) &&
      _gtk_widget_get_parent (GTK_WIDGET (child)) == NULL)
    {
      if (type)
        GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
      else
        gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
    }
  else
    {
      parent_buildable_iface->add_child (buildable, builder, child, type);
    }
}

static void
gtk_container_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_container_buildable_add_child;
}

/**
 * gtk_container_child_type:
 * @container: a #GtkContainer
 *
 * Returns the type of the children supported by the container.
 *
 * Note that this may return %G_TYPE_NONE to indicate that no more
 * children can be added, e.g. for a #GtkPaned which already has two
 * children.
 *
 * Returns: a #GType
 **/
GType
gtk_container_child_type (GtkContainer *container)
{
  g_return_val_if_fail (GTK_IS_CONTAINER (container), 0);

  if (GTK_CONTAINER_GET_CLASS (container)->child_type)
    return GTK_CONTAINER_GET_CLASS (container)->child_type (container);
  else
    return G_TYPE_NONE;
}

static void
gtk_container_add_unimplemented (GtkContainer *container,
                                 GtkWidget    *widget)
{
  g_warning ("GtkContainerClass::add not implemented for '%s'", G_OBJECT_TYPE_NAME (container));
}

static void
gtk_container_remove_unimplemented (GtkContainer *container,
                                    GtkWidget    *widget)
{
  g_warning ("GtkContainerClass::remove not implemented for '%s'", G_OBJECT_TYPE_NAME (container));
}

static void
gtk_container_init (GtkContainer *container)
{
}

static void
gtk_container_remove_cb (GtkWidget    *child,
                         GtkContainer *container)
{
  gtk_container_remove (container, child);
}

static void
gtk_container_dispose (GObject *object)
{
  GtkContainer *container = GTK_CONTAINER (object);

  gtk_container_foreach (container, (GtkCallback) gtk_container_remove_cb, container);

  G_OBJECT_CLASS (gtk_container_parent_class)->dispose (object);
}

/**
 * gtk_container_add:
 * @container: a #GtkContainer
 * @widget: a widget to be placed inside @container
 *
 * Adds @widget to @container. Typically used for simple containers
 * such as #GtkWindow, #GtkFrame, or #GtkButton; for more complicated
 * layout containers such #GtkGrid, this function will
 * pick default packing parameters that may not be correct.  So
 * consider functions such as gtk_grid_attach() as an alternative
 * to gtk_container_add() in those cases. A widget may be added to
 * only one container at a time; you can’t place the same widget
 * inside two different containers.
 *
 * Note that some containers, such as #GtkScrolledWindow or #GtkListBox,
 * may add intermediate children between the added widget and the
 * container.
 */
void
gtk_container_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  parent = _gtk_widget_get_parent (widget);

  if (parent != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a container of "
                 "type %s, but the widget is already inside a container of type %s, "
                 "please remove the widget from its existing container first." ,
                 G_OBJECT_TYPE_NAME (widget),
                 G_OBJECT_TYPE_NAME (container),
                 G_OBJECT_TYPE_NAME (parent));
      return;
    }

  g_signal_emit (container, container_signals[ADD], 0, widget);

  _gtk_container_accessible_add (GTK_WIDGET (container), widget);
}

/**
 * gtk_container_remove:
 * @container: a #GtkContainer
 * @widget: a current child of @container
 *
 * Removes @widget from @container. @widget must be inside @container.
 * Note that @container will own a reference to @widget, and that this
 * may be the last reference held; so removing a widget from its
 * container can destroy that widget. If you want to use @widget
 * again, you need to add a reference to it before removing it from
 * a container, using g_object_ref(). If you don’t want to use @widget
 * again it’s usually more efficient to simply destroy it directly
 * using gtk_widget_destroy() since this will remove it from the
 * container and help break any circular reference count cycles.
 **/
void
gtk_container_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_ref (container);
  g_object_ref (widget);

  g_signal_emit (container, container_signals[REMOVE], 0, widget);

  _gtk_container_accessible_remove (GTK_WIDGET (container), widget);

  g_object_unref (widget);
  g_object_unref (container);
}

static GtkSizeRequestMode 
gtk_container_get_request_mode (GtkWidget *widget)
{
  GtkWidget *w;
  int wfh = 0, hfw = 0;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      GtkSizeRequestMode mode = gtk_widget_get_request_mode (w);

      switch (mode)
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw ++;
          break;
        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh ++;
          break;
        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }

  if (hfw == 0 && wfh == 0)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return wfh > hfw ?
        GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT :
        GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

/**
 * gtk_container_forall: (virtual forall)
 * @container: a #GtkContainer
 * @callback: (scope call): a callback
 * @callback_data: (closure): callback user data
 *
 * Invokes @callback on each direct child of @container, including
 * children that are considered “internal” (implementation details
 * of the container). “Internal” children generally weren’t added
 * by the user of the container, but were added by the container
 * implementation itself.
 *
 * Most applications should use gtk_container_foreach(), rather
 * than gtk_container_forall().
 **/
void
gtk_container_forall (GtkContainer *container,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  gtk_widget_forall (GTK_WIDGET (container), callback, callback_data);
}

/**
 * gtk_container_foreach:
 * @container: a #GtkContainer
 * @callback: (scope call):  a callback
 * @callback_data: callback user data
 *
 * Invokes @callback on each non-internal child of @container.
 * See gtk_container_forall() for details on what constitutes
 * an “internal” child. For all practical purposes, this function
 * should iterate over precisely those child widgets that were
 * added to the container by the application with explicit add()
 * calls.
 *
 * It is permissible to remove the child from the @callback handler.
 *
 * Most applications should use gtk_container_foreach(),
 * rather than gtk_container_forall().
 **/
void
gtk_container_foreach (GtkContainer *container,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  if (GTK_CONTAINER_GET_CLASS (container)->forall)
    GTK_CONTAINER_GET_CLASS (container)->forall (container, callback, callback_data);
}

/**
 * gtk_container_get_children:
 * @container: a #GtkContainer
 *
 * Returns the container’s non-internal children. See
 * gtk_container_forall() for details on what constitutes an "internal" child.
 *
 * Returns: (element-type GtkWidget) (transfer container): a newly-allocated list of the container’s non-internal children.
 **/
GList*
gtk_container_get_children (GtkContainer *container)
{
  GList *children = NULL;

  gtk_container_foreach (container,
                         gtk_container_children_callback,
                         &children);

  return g_list_reverse (children);
}

static void
gtk_container_compute_expand (GtkWidget *widget,
                              gboolean  *hexpand_p,
                              gboolean  *vexpand_p)
{
  GtkWidget *w;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      /* note that we don't get_expand on the child if we already know we
       * have to expand, so we only recurse into children until we find
       * one that expands and then we basically don't do any more
       * work. This means that we can leave some children in a
       * need_compute_expand state, which is fine, as long as GtkWidget
       * doesn't rely on an invariant that "if a child has
       * need_compute_expand, its parents also do"
       *
       * gtk_widget_compute_expand() always returns FALSE if the
       * child is !visible so that's taken care of.
       */
      hexpand = hexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_HORIZONTAL);
      vexpand = vexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_VERTICAL);
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static void
gtk_container_children_callback (GtkWidget *widget,
                                 gpointer   client_data)
{
  GList **children;

  children = (GList**) client_data;
  *children = g_list_prepend (*children, widget);
}
