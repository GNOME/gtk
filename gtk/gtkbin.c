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

/**
 * SECTION:gtkbin
 * @Short_description: A container with just one child
 * @Title: GtkBin
 *
 * The #GtkBin widget is a container with just one child.
 * It is not very useful itself, but it is useful for deriving subclasses,
 * since it provides common code needed for handling a single child widget.
 *
 * Many GTK+ widgets are subclasses of #GtkBin, including #GtkWindow,
 * #GtkButton, #GtkFrame or #GtkScrolledWindow.
 */

#include "config.h"
#include "gtkbin.h"
#include "gtksizerequest.h"
#include "gtkintl.h"


struct _GtkBinPrivate
{
  GtkWidget *child;
};

static void gtk_bin_add         (GtkContainer   *container,
			         GtkWidget      *widget);
static void gtk_bin_remove      (GtkContainer   *container,
			         GtkWidget      *widget);
static void gtk_bin_forall      (GtkContainer   *container,
				 GtkCallback     callback,
				 gpointer        callback_data);
static GType gtk_bin_child_type (GtkContainer   *container);

static void               gtk_bin_measure                         (GtkWidget      *widget,
                                                                   GtkOrientation  orientation,
                                                                   int             for_size,
                                                                   int            *minimum,
                                                                   int            *natural,
                                                                   int            *minimum_baseline,
                                                                   int            *natural_baseline);
static void               gtk_bin_size_allocate                   (GtkWidget           *widget,
                                                                   GtkAllocation       *allocation);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkBin, gtk_bin, GTK_TYPE_CONTAINER)

static void
gtk_bin_class_init (GtkBinClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass*) class;
  GtkContainerClass *container_class = (GtkContainerClass*) class;

  widget_class->measure = gtk_bin_measure;
  widget_class->size_allocate = gtk_bin_size_allocate;

  container_class->add = gtk_bin_add;
  container_class->remove = gtk_bin_remove;
  container_class->forall = gtk_bin_forall;
  container_class->child_type = gtk_bin_child_type;
}

static void
gtk_bin_init (GtkBin *bin)
{
  bin->priv = gtk_bin_get_instance_private (bin);

  gtk_widget_set_has_window (GTK_WIDGET (bin), FALSE);
}


static GType
gtk_bin_child_type (GtkContainer *container)
{
  GtkBinPrivate *priv = GTK_BIN (container)->priv;

  if (!priv->child)
    return GTK_TYPE_WIDGET;
  else
    return G_TYPE_NONE;
}

static void
gtk_bin_add (GtkContainer *container,
	     GtkWidget    *child)
{
  GtkBin *bin = GTK_BIN (container);
  GtkBinPrivate *priv = bin->priv;

  if (priv->child != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a %s, "
                 "but as a GtkBin subclass a %s can only contain one widget at a time; "
                 "it already contains a widget of type %s",
                 g_type_name (G_OBJECT_TYPE (child)),
                 g_type_name (G_OBJECT_TYPE (bin)),
                 g_type_name (G_OBJECT_TYPE (bin)),
                 g_type_name (G_OBJECT_TYPE (priv->child)));
      return;
    }

  gtk_widget_set_parent (child, GTK_WIDGET (bin));
  priv->child = child;
}

static void
gtk_bin_remove (GtkContainer *container,
		GtkWidget    *child)
{
  GtkBin *bin = GTK_BIN (container);
  GtkBinPrivate *priv = bin->priv;
  gboolean widget_was_visible;

  g_return_if_fail (priv->child == child);

  widget_was_visible = gtk_widget_get_visible (child);
  
  gtk_widget_unparent (child);
  priv->child = NULL;
  
  /* queue resize regardless of gtk_widget_get_visible (container),
   * since that's what is needed by toplevels, which derive from GtkBin.
   */
  if (widget_was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_bin_forall (GtkContainer *container,
		GtkCallback   callback,
		gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  GtkBinPrivate *priv = bin->priv;

  if (priv->child)
    (* callback) (priv->child, callback_data);
}

static void
gtk_bin_measure (GtkWidget *widget,
                 GtkOrientation  orientation,
                 int             for_size,
                 int            *minimum,
                 int            *natural,
                 int            *minimum_baseline,
                 int            *natural_baseline)
{
  GtkBinPrivate *priv = gtk_bin_get_instance_private (GTK_BIN (widget));

  if (priv->child != NULL && gtk_widget_get_visible (priv->child))
    {
      gtk_widget_measure (priv->child,
                          orientation,
                          for_size,
                          minimum, natural,
                          minimum_baseline, natural_baseline);
    }
  else
    {
      *minimum = 0;
      *natural = 0;
    }
}

static void
gtk_bin_size_allocate (GtkWidget     *widget,
                       GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkBinPrivate *priv = bin->priv;

  gtk_widget_set_allocation (widget, allocation);

  if (priv->child && gtk_widget_get_visible (priv->child))
    {
      gtk_widget_size_allocate (priv->child, allocation);
    }
}

/**
 * gtk_bin_get_child:
 * @bin: a #GtkBin
 * 
 * Gets the child of the #GtkBin, or %NULL if the bin contains
 * no child widget. The returned widget does not have a reference
 * added, so you do not need to unref it.
 *
 * Returns: (transfer none): pointer to child of the #GtkBin
 **/
GtkWidget*
gtk_bin_get_child (GtkBin *bin)
{
  g_return_val_if_fail (GTK_IS_BIN (bin), NULL);

  return bin->priv->child;
}

void
_gtk_bin_set_child (GtkBin    *bin,
                    GtkWidget *widget)
{
  bin->priv->child = widget;
}
