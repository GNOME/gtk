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

#include <gdk/gdk.h>
#include "gtksignal.h"
#include "gtkinvisible.h"
#include "gtkintl.h"
enum {
  PROP_0,

  /* Construct */
  PROP_TYPE,

  /* Normal Props */
  PROP_SCREEN,
  
  LAST_ARG
};

static void gtk_invisible_class_init    (GtkInvisibleClass *klass);
static void gtk_invisible_init          (GtkInvisible      *invisible);
static void gtk_invisible_destroy       (GtkObject         *object);
static void gtk_invisible_realize       (GtkWidget         *widget);
static void gtk_invisible_style_set     (GtkWidget         *widget,
					 GtkStyle          *previous_style);
static void gtk_invisible_show          (GtkWidget         *widget);
static void gtk_invisible_size_allocate (GtkWidget         *widget,
					 GtkAllocation     *allocation);
static void gtk_invisible_set_property  (GObject           *object,
					 guint              prop_id,
					 const GValue      *value,
					 GParamSpec        *pspec);
static void gtk_invisible_get_property  (GObject           *object,
					 guint              prop_id,
					 GValue		   *value,
					 GParamSpec        *pspec);

GtkType
gtk_invisible_get_type (void)
{
  static GtkType invisible_type = 0;

  if (!invisible_type)
    {
      static const GtkTypeInfo invisible_info =
      {
	"GtkInvisible",
	sizeof (GtkInvisible),
	sizeof (GtkInvisibleClass),
	(GtkClassInitFunc) gtk_invisible_class_init,
	(GtkObjectInitFunc) gtk_invisible_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      invisible_type = gtk_type_unique (GTK_TYPE_WIDGET, &invisible_info);
    }

  return invisible_type;
}

static void
gtk_invisible_class_init (GtkInvisibleClass *class)
{
  
  GObjectClass	 *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
  object_class = (GtkObjectClass*) class;
  gobject_class = G_OBJECT_CLASS (class);

  widget_class->realize = gtk_invisible_realize;
  widget_class->style_set = gtk_invisible_style_set;
  widget_class->show = gtk_invisible_show;
  widget_class->size_allocate = gtk_invisible_size_allocate;

  object_class->destroy = gtk_invisible_destroy;
  gobject_class->set_property = gtk_invisible_set_property;
  gobject_class->get_property = gtk_invisible_get_property;

  /* Regular Props */
  g_object_class_install_property (gobject_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
 							_("Screen"),
 							_("The screen where this window will be displayed."),
							GDK_TYPE_SCREEN,
 							G_PARAM_READWRITE));
}

static void
gtk_invisible_init (GtkInvisible *invisible)
{
  GTK_WIDGET_UNSET_FLAGS (invisible, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (invisible, GTK_TOPLEVEL);

  gtk_widget_ref (GTK_WIDGET (invisible));
  gtk_object_sink (GTK_OBJECT (invisible));

  invisible->has_user_ref_count = TRUE;
  invisible->screen = gdk_get_default_screen ();
}

static void
gtk_invisible_destroy (GtkObject *object)
{
  GtkInvisible *invisible = GTK_INVISIBLE (object);
  
  if (invisible->has_user_ref_count)
    {
      invisible->has_user_ref_count = FALSE;
      gtk_widget_unref (GTK_WIDGET (invisible));
    }
}

GtkWidget* 
gtk_invisible_new_for_screen (GdkScreen *screen)
{
  GtkWidget *result;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  
  result = g_object_new (GTK_TYPE_INVISIBLE, NULL);
  GTK_INVISIBLE (result)->screen = screen;

  gtk_widget_realize (result);

  return result;
}

GtkWidget*
gtk_invisible_new (void)
{
  GtkWidget *result = g_object_new (GTK_TYPE_INVISIBLE, NULL);

  gtk_widget_realize (result);

  return result;
}

void
gtk_invisible_set_screen (GtkInvisible *invisible,
			  GdkScreen    *screen)
{
  g_return_if_fail (GTK_IS_INVISIBLE (invisible));
  g_return_if_fail (GDK_IS_SCREEN (screen));
  invisible->screen = screen;
}

GdkScreen *
gtk_invisible_get_screen (GtkInvisible *invisible)
{
  g_return_val_if_fail (GTK_IS_INVISIBLE (invisible), NULL);
  return invisible->screen;
}

static void
gtk_invisible_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (GTK_IS_INVISIBLE (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = gtk_widget_get_events (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  widget->window = gdk_window_new (gtk_widget_get_root_window (widget),
				   &attributes, attributes_mask);
					      
  gdk_window_set_user_data (widget->window, widget);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_invisible_style_set (GtkWidget *widget,
			 GtkStyle  *previous_style)
{
  /* Don't chain up to parent implementation */
}

static void
gtk_invisible_show (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);

  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_map (widget);
}

static void
gtk_invisible_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  widget->allocation = *allocation;
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
    case PROP_SCREEN:
      gtk_invisible_set_screen (invisible,  g_value_get_object (value));
      break;
    default:
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
  
  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, invisible->screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}
