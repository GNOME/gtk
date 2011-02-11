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

#include "config.h"
#include "gtkcontainer.h"
#include "gtkmisc.h"
#include "gtkintl.h"
#include "gtkprivate.h"


struct _GtkMiscPrivate
{
  gfloat        xalign;
  gfloat        yalign;

  guint16       xpad;
  guint16       ypad;
};

enum {
  PROP_0,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XPAD,
  PROP_YPAD
};

static void gtk_misc_realize      (GtkWidget    *widget);
static void gtk_misc_set_property (GObject         *object,
				   guint            prop_id,
				   const GValue    *value,
				   GParamSpec      *pspec);
static void gtk_misc_get_property (GObject         *object,
				   guint            prop_id,
				   GValue          *value,
				   GParamSpec      *pspec);


G_DEFINE_ABSTRACT_TYPE (GtkMisc, gtk_misc, GTK_TYPE_WIDGET)

static void
gtk_misc_class_init (GtkMiscClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_misc_set_property;
  gobject_class->get_property = gtk_misc_get_property;
  
  widget_class->realize = gtk_misc_realize;

  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float ("xalign",
						       P_("X align"),
						       P_("The horizontal alignment, from 0 (left) to 1 (right). Reversed for RTL layouts."),
						       0.0,
						       1.0,
						       0.5,
						       GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float ("yalign",
						       P_("Y align"),
						       P_("The vertical alignment, from 0 (top) to 1 (bottom)"),
						       0.0,
						       1.0,
						       0.5,
						       GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_XPAD,
                                   g_param_spec_int ("xpad",
						     P_("X pad"),
						     P_("The amount of space to add on the left and right of the widget, in pixels"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_YPAD,
                                   g_param_spec_int ("ypad",
						     P_("Y pad"),
						     P_("The amount of space to add on the top and bottom of the widget, in pixels"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE));

  g_type_class_add_private (class, sizeof (GtkMiscPrivate));
}

static void
gtk_misc_init (GtkMisc *misc)
{
  GtkMiscPrivate *priv;

  misc->priv = G_TYPE_INSTANCE_GET_PRIVATE (misc,
                                            GTK_TYPE_MISC,
                                            GtkMiscPrivate);
  priv = misc->priv;

  priv->xalign = 0.5;
  priv->yalign = 0.5;
  priv->xpad = 0;
  priv->ypad = 0;
}

static void
gtk_misc_set_property (GObject      *object,
		       guint         prop_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
  GtkMisc *misc = GTK_MISC (object);
  GtkMiscPrivate *priv = misc->priv;

  switch (prop_id)
    {
    case PROP_XALIGN:
      gtk_misc_set_alignment (misc, g_value_get_float (value), priv->yalign);
      break;
    case PROP_YALIGN:
      gtk_misc_set_alignment (misc, priv->xalign, g_value_get_float (value));
      break;
    case PROP_XPAD:
      gtk_misc_set_padding (misc, g_value_get_int (value), priv->ypad);
      break;
    case PROP_YPAD:
      gtk_misc_set_padding (misc, priv->xpad, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_misc_get_property (GObject      *object,
		       guint         prop_id,
		       GValue       *value,
		       GParamSpec   *pspec)
{
  GtkMisc *misc = GTK_MISC (object);
  GtkMiscPrivate *priv = misc->priv;

  switch (prop_id)
    {
    case PROP_XALIGN:
      g_value_set_float (value, priv->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, priv->yalign);
      break;
    case PROP_XPAD:
      g_value_set_int (value, priv->xpad);
      break;
    case PROP_YPAD:
      g_value_set_int (value, priv->ypad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_misc_set_alignment (GtkMisc *misc,
			gfloat   xalign,
			gfloat   yalign)
{
  GtkMiscPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MISC (misc));

  priv = misc->priv;

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (yalign < 0.0)
    yalign = 0.0;
  else if (yalign > 1.0)
    yalign = 1.0;

  if ((xalign != priv->xalign) || (yalign != priv->yalign))
    {
      g_object_freeze_notify (G_OBJECT (misc));
      if (xalign != priv->xalign)
	g_object_notify (G_OBJECT (misc), "xalign");

      if (yalign != priv->yalign)
	g_object_notify (G_OBJECT (misc), "yalign");

      priv->xalign = xalign;
      priv->yalign = yalign;
      
      /* clear the area that was allocated before the change
       */
      widget = GTK_WIDGET (misc);
      if (gtk_widget_is_drawable (widget))
        gtk_widget_queue_draw (widget);

      g_object_thaw_notify (G_OBJECT (misc));
    }
}

/**
 * gtk_misc_get_alignment:
 * @misc: a #GtkMisc
 * @xalign: (out) (allow-none): location to store X alignment of @misc, or %NULL
 * @yalign: (out) (allow-none): location to store Y alignment of @misc, or %NULL
 *
 * Gets the X and Y alignment of the widget within its allocation. 
 * See gtk_misc_set_alignment().
 **/
void
gtk_misc_get_alignment (GtkMisc *misc,
		        gfloat  *xalign,
			gfloat  *yalign)
{
  GtkMiscPrivate *priv;

  g_return_if_fail (GTK_IS_MISC (misc));

  priv = misc->priv;

  if (xalign)
    *xalign = priv->xalign;
  if (yalign)
    *yalign = priv->yalign;
}

void
gtk_misc_set_padding (GtkMisc *misc,
		      gint     xpad,
		      gint     ypad)
{
  GtkMiscPrivate *priv;

  g_return_if_fail (GTK_IS_MISC (misc));

  priv = misc->priv;

  if (xpad < 0)
    xpad = 0;
  if (ypad < 0)
    ypad = 0;

  if ((xpad != priv->xpad) || (ypad != priv->ypad))
    {
      g_object_freeze_notify (G_OBJECT (misc));
      if (xpad != priv->xpad)
	g_object_notify (G_OBJECT (misc), "xpad");

      if (ypad != priv->ypad)
	g_object_notify (G_OBJECT (misc), "ypad");

      priv->xpad = xpad;
      priv->ypad = ypad;

      if (gtk_widget_is_drawable (GTK_WIDGET (misc)))
	gtk_widget_queue_resize (GTK_WIDGET (misc));

      g_object_thaw_notify (G_OBJECT (misc));
    }
}

/**
 * gtk_misc_get_padding:
 * @misc: a #GtkMisc
 * @xpad: (out) (allow-none): location to store padding in the X
 *        direction, or %NULL
 * @ypad: (out) (allow-none): location to store padding in the Y
 *        direction, or %NULL
 *
 * Gets the padding in the X and Y directions of the widget. 
 * See gtk_misc_set_padding().
 **/
void
gtk_misc_get_padding (GtkMisc *misc,
		      gint    *xpad,
		      gint    *ypad)
{
  GtkMiscPrivate *priv;

  g_return_if_fail (GTK_IS_MISC (misc));

  priv = misc->priv;

  if (xpad)
    *xpad = priv->xpad;
  if (ypad)
    *ypad = priv->ypad;
}

static void
gtk_misc_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  if (!gtk_widget_get_has_window (widget))
    {
      window = gtk_widget_get_parent_window (widget);
      gtk_widget_set_window (widget, window);
      g_object_ref (window);
    }
  else
    {
      gtk_widget_get_allocation (widget, &allocation);

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = allocation.x;
      attributes.y = allocation.y;
      attributes.width = allocation.width;
      attributes.height = allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

      window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
      gtk_widget_set_window (widget, window);
      gdk_window_set_user_data (window, widget);
      gdk_window_set_background_pattern (window, NULL);
    }
}
