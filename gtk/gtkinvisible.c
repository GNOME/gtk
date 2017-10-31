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
#include <gdk/gdk.h>
#include "gtkinvisible.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkinvisible
 * @Short_description: A widget which is not displayed
 * @Title: GtkInvisible
 *
 * The #GtkInvisible widget is used internally in GTK+, and is probably not
 * very useful for application developers.
 *
 * It is used for reliable pointer grabs and selection handling in the code
 * for drag-and-drop.
 */


struct _GtkInvisiblePrivate
{
  GdkDisplay   *display;
  gboolean      has_user_ref_count;
};

enum {
  PROP_0,
  PROP_DISPLAY,
  LAST_ARG
};

static void gtk_invisible_destroy       (GtkWidget         *widget);
static void gtk_invisible_realize       (GtkWidget         *widget);
static void gtk_invisible_style_updated (GtkWidget         *widget);
static void gtk_invisible_show          (GtkWidget         *widget);
static void gtk_invisible_set_property  (GObject           *object,
					 guint              prop_id,
					 const GValue      *value,
					 GParamSpec        *pspec);
static void gtk_invisible_get_property  (GObject           *object,
					 guint              prop_id,
					 GValue		   *value,
					 GParamSpec        *pspec);
static void gtk_invisible_constructed   (GObject           *object);

G_DEFINE_TYPE_WITH_PRIVATE (GtkInvisible, gtk_invisible, GTK_TYPE_WIDGET)

static void
gtk_invisible_class_init (GtkInvisibleClass *class)
{
  GObjectClass	 *gobject_class;
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
  gobject_class = (GObjectClass*) class;

  widget_class->realize = gtk_invisible_realize;
  widget_class->style_updated = gtk_invisible_style_updated;
  widget_class->show = gtk_invisible_show;
  widget_class->destroy = gtk_invisible_destroy;

  gobject_class->set_property = gtk_invisible_set_property;
  gobject_class->get_property = gtk_invisible_get_property;
  gobject_class->constructed = gtk_invisible_constructed;

  g_object_class_install_property (gobject_class,
				   PROP_DISPLAY,
				   g_param_spec_object ("display",
 							P_("Display"),
 							P_("The display where this window will be displayed"),
							GDK_TYPE_DISPLAY,
 							GTK_PARAM_READWRITE));
}

static void
gtk_invisible_init (GtkInvisible *invisible)
{
  GtkInvisiblePrivate *priv;

  invisible->priv = gtk_invisible_get_instance_private (invisible);
  priv = invisible->priv;

  gtk_widget_set_has_window (GTK_WIDGET (invisible), TRUE);
  _gtk_widget_set_is_toplevel (GTK_WIDGET (invisible), TRUE);

  g_object_ref_sink (invisible);

  priv->has_user_ref_count = TRUE;
  priv->display = gdk_display_get_default ();
}

static void
gtk_invisible_destroy (GtkWidget *widget)
{
  GtkInvisible *invisible = GTK_INVISIBLE (widget);
  GtkInvisiblePrivate *priv = invisible->priv;

  if (priv->has_user_ref_count)
    {
      priv->has_user_ref_count = FALSE;
      g_object_unref (invisible);
    }

  GTK_WIDGET_CLASS (gtk_invisible_parent_class)->destroy (widget);
}

/**
 * gtk_invisible_new_for_display:
 * @display: a #GdkDisplay which identifies on which
 *	     the new #GtkInvisible will be created.
 *
 * Creates a new #GtkInvisible object for a specified display.
 *
 * Returns: a newly created #GtkInvisible object
 *
 * Since: 3.94
 **/
GtkWidget* 
gtk_invisible_new_for_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  return g_object_new (GTK_TYPE_INVISIBLE, "display", display, NULL);
}

/**
 * gtk_invisible_new:
 * 
 * Creates a new #GtkInvisible.
 * 
 * Returns: a new #GtkInvisible.
 **/
GtkWidget*
gtk_invisible_new (void)
{
  return g_object_new (GTK_TYPE_INVISIBLE, NULL);
}

/**
 * gtk_invisible_set_display:
 * @invisible: a #GtkInvisible.
 * @display: a #GdkDisplay.
 *
 * Sets the #GdkDisplay where the #GtkInvisible object will be displayed.
 *
 * Since: 3.94
 **/ 
void
gtk_invisible_set_display (GtkInvisible *invisible,
			   GdkDisplay   *display)
{
  GtkInvisiblePrivate *priv;
  GtkWidget *widget;
  GdkDisplay *previous_display;
  gboolean was_realized;

  g_return_if_fail (GTK_IS_INVISIBLE (invisible));
  g_return_if_fail (GDK_IS_DISPLAY (display));

  priv = invisible->priv;

  if (display == priv->display)
    return;

  widget = GTK_WIDGET (invisible);

  previous_display = priv->display;
  was_realized = gtk_widget_get_realized (widget);

  if (was_realized)
    gtk_widget_unrealize (widget);

  priv->display = display;
  if (display != previous_display)
    _gtk_widget_propagate_display_changed (widget, previous_display);
  g_object_notify (G_OBJECT (invisible), "display");
  
  if (was_realized)
    gtk_widget_realize (widget);
}

/**
 * gtk_invisible_get_display:
 * @invisible: a #GtkInvisible.
 *
 * Returns the #GdkDisplay object associated with @invisible
 *
 * Returns: (transfer none): the associated #GdkDisplay.
 *
 * Since: 3.94
 **/
GdkDisplay *
gtk_invisible_get_display (GtkInvisible *invisible)
{
  g_return_val_if_fail (GTK_IS_INVISIBLE (invisible), NULL);

  return invisible->priv->display;
}

static void
gtk_invisible_realize (GtkWidget *widget)
{
  GdkWindow *window;

  gtk_widget_set_realized (widget, TRUE);

  window = gdk_window_new_temp (gtk_widget_get_display (widget));
  gtk_widget_set_window (widget, window);
  gtk_widget_register_window (widget, window);
}

static void
gtk_invisible_style_updated (GtkWidget *widget)
{
  /* Don't chain up to parent implementation */
}

static void
gtk_invisible_show (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_widget_map (widget);
}

static void 
gtk_invisible_set_property  (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  GtkInvisible *invisible = GTK_INVISIBLE (object);
  
  switch (prop_id)
    {
    case PROP_DISPLAY:
      gtk_invisible_set_display (invisible, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_invisible_get_property  (GObject      *object,
			     guint         prop_id,
			     GValue	  *value,
			     GParamSpec   *pspec)
{
  GtkInvisible *invisible = GTK_INVISIBLE (object);
  GtkInvisiblePrivate *priv = invisible->priv;

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* We use a constructor here so that we can realize the invisible on
 * the correct display after the “display” property has been set
 */
static void
gtk_invisible_constructed (GObject *object)
{
  G_OBJECT_CLASS (gtk_invisible_parent_class)->constructed (object);

  gtk_widget_realize (GTK_WIDGET (object));
}
