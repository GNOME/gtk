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

#include "gtkbin.h"


static void gtk_bin_class_init (GtkBinClass    *klass);
static void gtk_bin_init       (GtkBin         *bin);
static void gtk_bin_add        (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_bin_remove     (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_bin_forall     (GtkContainer   *container,
				gboolean	include_internals,
			        GtkCallback     callback,
			        gpointer        callback_data);
static GtkType gtk_bin_child_type (GtkContainer*container);


static GtkContainerClass *parent_class = NULL;


GtkType
gtk_bin_get_type (void)
{
  static GtkType bin_type = 0;

  if (!bin_type)
    {
      static const GtkTypeInfo bin_info =
      {
	"GtkBin",
	sizeof (GtkBin),
	sizeof (GtkBinClass),
	(GtkClassInitFunc) gtk_bin_class_init,
	(GtkObjectInitFunc) gtk_bin_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      bin_type = gtk_type_unique (GTK_TYPE_CONTAINER, &bin_info);
    }

  return bin_type;
}

static void
gtk_bin_class_init (GtkBinClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  container_class->add = gtk_bin_add;
  container_class->remove = gtk_bin_remove;
  container_class->forall = gtk_bin_forall;
  container_class->child_type = gtk_bin_child_type;
}

static void
gtk_bin_init (GtkBin *bin)
{
  GTK_WIDGET_SET_FLAGS (bin, GTK_NO_WINDOW);

  bin->child = NULL;
}


static GtkType
gtk_bin_child_type (GtkContainer *container)
{
  if (!GTK_BIN (container)->child)
    return GTK_TYPE_WIDGET;
  else
    return GTK_TYPE_NONE;
}

static void
gtk_bin_add (GtkContainer *container,
	     GtkWidget    *child)
{
  GtkBin *bin = GTK_BIN (container);

  g_return_if_fail (GTK_IS_WIDGET (child));

  if (bin->child != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a %s, "
                 "but as a GtkBin subclass a %s can only contain one widget at a time; "
                 "it already contains a widget of type %s",
                 g_type_name (G_OBJECT_TYPE (child)),
                 g_type_name (G_OBJECT_TYPE (bin)),
                 g_type_name (G_OBJECT_TYPE (bin)),
                 g_type_name (G_OBJECT_TYPE (bin->child)));
      return;
    }

  gtk_widget_set_parent (child, GTK_WIDGET (bin));
  bin->child = child;
}

static void
gtk_bin_remove (GtkContainer *container,
		GtkWidget    *child)
{
  GtkBin *bin = GTK_BIN (container);
  gboolean widget_was_visible;

  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (bin->child == child);

  widget_was_visible = GTK_WIDGET_VISIBLE (child);
  
  gtk_widget_unparent (child);
  bin->child = NULL;
  
  /* queue resize regardless of GTK_WIDGET_VISIBLE (container),
   * since that's what is needed by toplevels, which derive from GtkBin.
   */
  if (widget_was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_bin_forall (GtkContainer *container,
		gboolean      include_internals,
		GtkCallback   callback,
		gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);

  g_return_if_fail (callback != NULL);

  if (bin->child)
    (* callback) (bin->child, callback_data);
}

/**
 * gtk_bin_get_child:
 * @bin: a #GtkBin
 * 
 * Gets the child of the #GtkBin, or %NULL if the bin contains
 * no child widget. The returned widget does not have a reference
 * added, so you do not need to unref it.
 * 
 * Return value: pointer to child of the #GtkBin
 **/
GtkWidget*
gtk_bin_get_child (GtkBin *bin)
{
  g_return_val_if_fail (GTK_IS_BIN (bin), NULL);

  return bin->child;
}
