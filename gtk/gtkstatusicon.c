/* gtkstatusicon-x11.c:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>
#include <string.h>

#include "gtkstatusicon.h"

#include "gtkintl.h"
#include "gtkiconfactory.h"
#include "gtkmarshalers.h"
#include "gtktooltips.h"
#include "gtktrayicon.h"

#include "gtkprivate.h"
#include "gtkwidget.h"

#include "gtkalias.h"

#define BLINK_TIMEOUT 500

enum
{
  PROP_0,
  PROP_PIXBUF,
  PROP_FILE,
  PROP_STOCK,
  PROP_ICON_NAME,
  PROP_STORAGE_TYPE,
  PROP_SIZE,
  PROP_VISIBLE,
  PROP_BLINKING
};

enum 
{
  ACTIVATE_SIGNAL,
  POPUP_MENU_SIGNAL,
  SIZE_CHANGED_SIGNAL,
  LAST_SIGNAL
};

struct _GtkStatusIconPrivate
{
  GtkWidget    *tray_icon;
  GtkWidget    *image;
  gint          size;

  gint          image_width;
  gint          image_height;

  GtkTooltips  *tooltips;

  GtkImageType  storage_type;

  union
    {
      GdkPixbuf *pixbuf;
      gchar     *stock_id;
      gchar     *icon_name;
    } image_data;

  GdkPixbuf    *blank_icon;
  guint         blinking_timeout;

  guint         blinking : 1;
  guint         blink_off : 1;
  guint         visible : 1;
};

static void     gtk_status_icon_finalize         (GObject        *object);
static void     gtk_status_icon_set_property     (GObject        *object,
						  guint           prop_id,
						  const GValue   *value,
						  GParamSpec     *pspec);
static void     gtk_status_icon_get_property     (GObject        *object,
						  guint           prop_id,
						  GValue         *value,
					          GParamSpec     *pspec);

static void     gtk_status_icon_size_allocate    (GtkStatusIcon  *status_icon,
						  GtkAllocation  *allocation);
static gboolean gtk_status_icon_button_press     (GtkStatusIcon  *status_icon,
						  GdkEventButton *event);
static void     gtk_status_icon_disable_blinking (GtkStatusIcon  *status_icon);
static void     gtk_status_icon_reset_image_data (GtkStatusIcon  *status_icon);
					   

static guint status_icon_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkStatusIcon, gtk_status_icon, G_TYPE_OBJECT);

static void
gtk_status_icon_class_init (GtkStatusIconClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *) class;

  gobject_class->finalize     = gtk_status_icon_finalize;
  gobject_class->set_property = gtk_status_icon_set_property;
  gobject_class->get_property = gtk_status_icon_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_PIXBUF,
				   g_param_spec_object ("pixbuf",
							P_("Pixbuf"),
							P_("A GdkPixbuf to display"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_FILE,
				   g_param_spec_string ("file",
							P_("Filename"),
							P_("Filename to load and display"),
							NULL,
							GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
				   PROP_STOCK,
				   g_param_spec_string ("stock",
							P_("Stock ID"),
							P_("Stock ID for a stock image to display"),
							NULL,
							GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        P_("Icon Name"),
                                                        P_("The name of the icon from the icon theme"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
				   PROP_STORAGE_TYPE,
				   g_param_spec_enum ("storage-type",
						      P_("Storage type"),
						      P_("The representation being used for image data"),
						      GTK_TYPE_IMAGE_TYPE,
						      GTK_IMAGE_EMPTY,
						      GTK_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_SIZE,
				   g_param_spec_int ("size",
						     P_("Size"),
						     P_("The size of the icon"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_BLINKING,
				   g_param_spec_boolean ("blinking",
							 P_("Blinking"),
							 P_("Whether or not the status icon is blinking"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
							 P_("Visible"),
							 P_("Whether or not the status icon is visible"),
							 TRUE,
							 GTK_PARAM_READWRITE));


  /**
   * GtkStatusIcon::activate:
   * @status_icon: the object which received the signal
   *
   * Gets emitted when the user activates the status icon. 
   * If and how status icons can activated is platform-dependent.
   *
   * Since: 2.10
   */
  status_icon_signals [ACTIVATE_SIGNAL] =
    g_signal_new ("activate",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkStatusIconClass, activate),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);

  /**
   * GtkStatusIcon::popup-menu:
   * @status_icon: the object which received the signal
   * @button: the button that was pressed, or 0 if the 
   *   signal is not emitted in response to a button press event
   * @activate_time: the timestamp of the event that
   *   triggered the signal emission
   *
   * Gets emitted when the user brings up the context menu
   * of the status icon. Whether status icons can have context 
   * menus and how these are activated is platform-dependent.
   *
   * The @button and @activate_timeout parameters should be 
   * passed as the last to arguments to gtk_menu_popup().
   *
   * Since: 2.10
   */
  status_icon_signals [POPUP_MENU_SIGNAL] =
    g_signal_new ("popup-menu",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkStatusIconClass, popup_menu),
		  NULL,
		  NULL,
		  _gtk_marshal_VOID__UINT_UINT,
		  G_TYPE_NONE,
		  2,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

  /**
   * GtkStatusIcon::size-changed:
   * @status_icon: the object which received the signal
   * @size: the new size
   *
   * Gets emitted when the size available for the image
   * changes, e.g. because the notification area got resized.
   *
   * Since: 2.10
   */
  status_icon_signals [SIZE_CHANGED_SIGNAL] =
    g_signal_new ("size-changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkStatusIconClass, size_changed),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__INT,
		  G_TYPE_NONE,
		  1,
		  G_TYPE_INT);

  g_type_class_add_private (class, sizeof (GtkStatusIconPrivate));
}

static void
gtk_status_icon_init (GtkStatusIcon *status_icon)
{
  status_icon->priv = G_TYPE_INSTANCE_GET_PRIVATE (status_icon, GTK_TYPE_STATUS_ICON,
						   GtkStatusIconPrivate);
  
  status_icon->priv->storage_type = GTK_IMAGE_EMPTY;
  status_icon->priv->size       = 0;
  status_icon->priv->image_width = 0;
  status_icon->priv->image_height = 0;
  status_icon->priv->visible    = TRUE;

  status_icon->priv->tray_icon = GTK_WIDGET (_gtk_tray_icon_new (NULL));

  gtk_widget_add_events (GTK_WIDGET (status_icon->priv->tray_icon),
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  g_signal_connect_swapped (status_icon->priv->tray_icon, "button-press-event",
			    G_CALLBACK (gtk_status_icon_button_press), status_icon);

  status_icon->priv->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (status_icon->priv->tray_icon),
		     status_icon->priv->image);

  g_signal_connect_swapped (status_icon->priv->image, "size-allocate",
			    G_CALLBACK (gtk_status_icon_size_allocate), status_icon);

  gtk_widget_show (status_icon->priv->image);
  gtk_widget_show (status_icon->priv->tray_icon);

  status_icon->priv->tooltips = gtk_tooltips_new ();
  g_object_ref (status_icon->priv->tooltips);
  gtk_object_sink (GTK_OBJECT (status_icon->priv->tooltips));
}

static void
gtk_status_icon_finalize (GObject *object)
{
  GtkStatusIcon *status_icon = GTK_STATUS_ICON (object);

  gtk_status_icon_disable_blinking (status_icon);
  
  gtk_status_icon_reset_image_data (status_icon);

  if (status_icon->priv->blank_icon)
    g_object_unref (status_icon->priv->blank_icon);
  status_icon->priv->blank_icon = NULL;

  if (status_icon->priv->tooltips)
    g_object_unref (status_icon->priv->tooltips);
  status_icon->priv->tooltips = NULL;

  gtk_widget_destroy (status_icon->priv->tray_icon);

  G_OBJECT_CLASS (gtk_status_icon_parent_class)->finalize (object);
}

static void
gtk_status_icon_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkStatusIcon *status_icon = GTK_STATUS_ICON (object);

  switch (prop_id)
    {
    case PROP_PIXBUF:
      gtk_status_icon_set_from_pixbuf (status_icon, g_value_get_object (value));
      break;
    case PROP_FILE:
      gtk_status_icon_set_from_file (status_icon, g_value_get_string (value));
      break;
    case PROP_STOCK:
      gtk_status_icon_set_from_stock (status_icon, g_value_get_string (value));
      break;
    case PROP_ICON_NAME:
      gtk_status_icon_set_from_icon_name (status_icon, g_value_get_string (value));
      break;
    case PROP_BLINKING:
      gtk_status_icon_set_blinking (status_icon, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE:
      gtk_status_icon_set_visible (status_icon, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_status_icon_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkStatusIcon *status_icon = GTK_STATUS_ICON (object);

  switch (prop_id)
    {
    case PROP_PIXBUF:
      g_value_set_object (value, gtk_status_icon_get_pixbuf (status_icon));
      break;
    case PROP_STOCK:
      g_value_set_string (value, gtk_status_icon_get_stock (status_icon));
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, gtk_status_icon_get_icon_name (status_icon));
      break;
    case PROP_STORAGE_TYPE:
      g_value_set_enum (value, gtk_status_icon_get_storage_type (status_icon));
      break;
    case PROP_SIZE:
      g_value_set_int (value, gtk_status_icon_get_size (status_icon));
      break;
    case PROP_BLINKING:
      g_value_set_boolean (value, gtk_status_icon_get_blinking (status_icon));
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, gtk_status_icon_get_visible (status_icon));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_status_icon_new:
 * 
 * Creates an empty status icon object.
 * 
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new (void)
{
  return g_object_new (GTK_TYPE_STATUS_ICON, NULL);
}

/**
 * gtk_status_icon_new_from_pixbuf:
 * @pixbuf: a #GdkPixbuf
 * 
 * Creates a status icon displaying @pixbuf. 
 *
 * The image will be scaled down to fit in the available 
 * space in the notification area, if necessary.
 * 
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "pixbuf", pixbuf,
		       NULL);
}

/**
 * gtk_status_icon_new_from_file:
 * @filename: a filename
 * 
 * Creates a status icon displaying the file @filename. 
 *
 * The image will be scaled down to fit in the available 
 * space in the notification area, if necessary.
 * 
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new_from_file (const gchar *filename)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "file", filename,
		       NULL);
}

/**
 * gtk_status_icon_new_from_stock:
 * @stock_id: a stock icon id
 * 
 * Creates a status icon displaying a stock icon. Sample stock icon
 * names are #GTK_STOCK_OPEN, #GTK_STOCK_EXIT. You can register your 
 * own stock icon names, see gtk_icon_factory_add_default() and 
 * gtk_icon_factory_add(). 
 *
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new_from_stock (const gchar *stock_id)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "stock", stock_id,
		       NULL);
}

/**
 * gtk_status_icon_new_from_icon_name:
 * @icon_name: an icon name
 * 
 * Creates a status icon displaying an icon from the current icon theme.
 * If the current icon theme is changed, the icon will be updated 
 * appropriately.
 * 
 * Return value: a new #GtkStatusIcon
 *
 * Since: 2.10
 **/
GtkStatusIcon *
gtk_status_icon_new_from_icon_name (const gchar *icon_name)
{
  return g_object_new (GTK_TYPE_STATUS_ICON,
		       "icon-name", icon_name,
		       NULL);
}

static void
emit_activate_signal (GtkStatusIcon *status_icon)
{
  g_signal_emit (status_icon,
		 status_icon_signals [ACTIVATE_SIGNAL], 0);
}

static void
emit_popup_menu_signal (GtkStatusIcon *status_icon,
			guint          button,
			guint32        activate_time)
{
  g_signal_emit (status_icon,
		 status_icon_signals [POPUP_MENU_SIGNAL], 0,
		 button,
		 activate_time);
}

static gboolean
emit_size_changed_signal (GtkStatusIcon *status_icon,
			  gint           size)
{
  gboolean handled = FALSE;
  
  g_signal_emit (status_icon,
		 status_icon_signals [SIZE_CHANGED_SIGNAL], 0,
		 size,
		 &handled);

  return handled;
}

static GdkPixbuf *
gtk_status_icon_blank_icon (GtkStatusIcon *status_icon)
{
  if (status_icon->priv->blank_icon)
    {
      gint width, height;

      width  = gdk_pixbuf_get_width (status_icon->priv->blank_icon);
      height = gdk_pixbuf_get_height (status_icon->priv->blank_icon);


      if (width == status_icon->priv->image_width && 
	  height == status_icon->priv->image_height)
	{
	  return status_icon->priv->blank_icon;
	}
      else
	{
	  g_object_unref (status_icon->priv->blank_icon);
	  status_icon->priv->blank_icon = NULL;
	}
    }

  status_icon->priv->blank_icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
						  status_icon->priv->image_width, 
						  status_icon->priv->image_height);
  if (status_icon->priv->blank_icon)
    gdk_pixbuf_fill (status_icon->priv->blank_icon, 0);

  return status_icon->priv->blank_icon;
}

static GtkIconSize
find_icon_size (GtkWidget *widget, 
		gint       pixel_size)
{
  GdkScreen *screen;
  GtkSettings *settings;
  GtkIconSize s, size;
  gint w, h, d, dist;

  screen = gtk_widget_get_screen (widget);

  if (!screen)
    return GTK_ICON_SIZE_MENU;

  settings = gtk_settings_get_for_screen (screen);
  
  dist = G_MAXINT;
  size = GTK_ICON_SIZE_MENU;

  for (s = GTK_ICON_SIZE_MENU; s < GTK_ICON_SIZE_DIALOG; s++)
    {
      if (gtk_icon_size_lookup_for_settings (settings, s, &w, &h) &&
	  w <= pixel_size && h <= pixel_size)
	{
	  d = MAX (pixel_size - w, pixel_size - h);
	  if (d < dist)
	    {
	      dist = d;
	      size = s;
	    }
	}
    }
  
  return size;
}

static void
gtk_status_icon_update_image (GtkStatusIcon *status_icon)
{
  if (status_icon->priv->blink_off)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (status_icon->priv->image),
				 gtk_status_icon_blank_icon (status_icon));
      return;
    }

  switch (status_icon->priv->storage_type)
    {
    case GTK_IMAGE_PIXBUF:
      {
	GdkPixbuf *pixbuf;

	pixbuf = status_icon->priv->image_data.pixbuf;

	if (pixbuf)
	  {
	    GdkPixbuf *scaled;
	    gint size;
	    gint width;
	    gint height;

	    size = status_icon->priv->size;

	    width  = gdk_pixbuf_get_width  (pixbuf);
	    height = gdk_pixbuf_get_height (pixbuf);

	    if (width > size || height > size)
	      {
		scaled = gdk_pixbuf_scale_simple (pixbuf,
						  MIN (size, width),
						  MIN (size, height),
						  GDK_INTERP_BILINEAR);
	      }
	    else
	      {
		scaled = g_object_ref (pixbuf);
	      }

	    gtk_image_set_from_pixbuf (GTK_IMAGE (status_icon->priv->image), scaled);

	    g_object_unref (scaled);
	  }
	else
	  {
	    gtk_image_set_from_pixbuf (GTK_IMAGE (status_icon->priv->image), NULL);
	  }
      }
      break;

    case GTK_IMAGE_STOCK:
      {
	GtkIconSize size = find_icon_size (status_icon->priv->image, status_icon->priv->size);
	gtk_image_set_from_stock (GTK_IMAGE (status_icon->priv->image),
				  status_icon->priv->image_data.stock_id,
				  size);
      }
      break;
      
    case GTK_IMAGE_ICON_NAME:
      {
	GtkIconSize size = find_icon_size (status_icon->priv->image, status_icon->priv->size);
	gtk_image_set_from_icon_name (GTK_IMAGE (status_icon->priv->image),
				      status_icon->priv->image_data.icon_name,
				      size);
      }
      break;
      
    case GTK_IMAGE_EMPTY:
      gtk_image_set_from_pixbuf (GTK_IMAGE (status_icon->priv->image), NULL);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_status_icon_size_allocate (GtkStatusIcon *status_icon,
			       GtkAllocation *allocation)
{
  GtkOrientation orientation;
  gint size;

  orientation = _gtk_tray_icon_get_orientation (GTK_TRAY_ICON (status_icon->priv->tray_icon));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = allocation->height;
  else
    size = allocation->width;

  status_icon->priv->image_width = allocation->width - GTK_MISC (status_icon->priv->image)->xpad * 2;
  status_icon->priv->image_height = allocation->height - GTK_MISC (status_icon->priv->image)->ypad * 2;

  if (status_icon->priv->size != size)
    {
      status_icon->priv->size = size;

      g_object_notify (G_OBJECT (status_icon), "size");

      if (!emit_size_changed_signal (status_icon, size))
	{
	  gtk_status_icon_update_image (status_icon);
	}
    }
}

static gboolean
gtk_status_icon_button_press (GtkStatusIcon  *status_icon,
			      GdkEventButton *event)
{
  if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
    {
      emit_activate_signal (status_icon);
      return TRUE;
    }
  else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      emit_popup_menu_signal (status_icon, event->button, event->time);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_status_icon_reset_image_data (GtkStatusIcon *status_icon)
{
  status_icon->priv->storage_type = GTK_IMAGE_EMPTY;
  g_object_notify (G_OBJECT (status_icon), "storage-type");

  switch (status_icon->priv->storage_type)
  {
    case GTK_IMAGE_PIXBUF:
      if (status_icon->priv->image_data.pixbuf)
	g_object_unref (status_icon->priv->image_data.pixbuf);
      status_icon->priv->image_data.pixbuf = NULL;
      g_object_notify (G_OBJECT (status_icon), "pixbuf");
      break;

    case GTK_IMAGE_STOCK:
      g_free (status_icon->priv->image_data.stock_id);
      status_icon->priv->image_data.stock_id = NULL;

      g_object_notify (G_OBJECT (status_icon), "stock");
      break;
      
    case GTK_IMAGE_ICON_NAME:
      g_free (status_icon->priv->image_data.icon_name);
      status_icon->priv->image_data.icon_name = NULL;

      g_object_notify (G_OBJECT (status_icon), "icon-name");
      break;
      
    case GTK_IMAGE_EMPTY:
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gtk_status_icon_set_image (GtkStatusIcon *status_icon,
    			   GtkImageType   storage_type,
			   gpointer       data)
{
  g_object_freeze_notify (G_OBJECT (status_icon));

  gtk_status_icon_reset_image_data (status_icon);

  status_icon->priv->storage_type = storage_type;
  g_object_notify (G_OBJECT (status_icon), "storage-type");

  switch (storage_type) 
    {
    case GTK_IMAGE_PIXBUF:
      status_icon->priv->image_data.pixbuf = (GdkPixbuf *)data;
      g_object_notify (G_OBJECT (status_icon), "pixbuf");
      break;
    case GTK_IMAGE_STOCK:
      status_icon->priv->image_data.stock_id = g_strdup ((const gchar *)data);
      g_object_notify (G_OBJECT (status_icon), "stock");
      break;
    case GTK_IMAGE_ICON_NAME:
      status_icon->priv->image_data.icon_name = g_strdup ((const gchar *)data);
      g_object_notify (G_OBJECT (status_icon), "icon-name");
      break;
    default:
      g_warning ("Image type %d not handled by GtkStatusIcon", storage_type);
    }

  g_object_thaw_notify (G_OBJECT (status_icon));

  gtk_status_icon_update_image (status_icon);
}

/**
 * gtk_status_icon_set_from_pixbuf:
 * @status_icon: a #GtkStatusIcon
 * @pixbuf: a #GdkPixbuf or %NULL
 * 
 * Makes @status_icon display @pixbuf. 
 * See gtk_status_icon_new_from_pixbuf() for details.
 *
 * Since: 2.10
 **/
void
gtk_status_icon_set_from_pixbuf (GtkStatusIcon *status_icon,
				 GdkPixbuf     *pixbuf)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  if (pixbuf)
    g_object_ref (pixbuf);

  gtk_status_icon_set_image (status_icon, GTK_IMAGE_PIXBUF,
      			     (gpointer) pixbuf);
}
v
/**
 * gtk_status_icon_set_from_file:
 * @status_icon: a #GtkStatusIcon
 * @filename: a filename
 * 
 * Makes @status_icon display the file @filename.
 * See gtk_status_icon_new_from_file() for details.
 *
 * Since: 2.10 
 **/
void
gtk_status_icon_set_from_file (GtkStatusIcon *status_icon,
 			       const gchar   *filename)
{
  GdkPixbuf *pixbuf;
  
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (filename != NULL);
  
  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
  
  gtk_status_icon_set_from_pixbuf (status_icon, pixbuf);
  
  if (pixbuf)
    g_object_unref (pixbuf);
}

/**
 * gtk_status_icon_set_from_stock:
 * @status_icon: a #GtkStatusIcon
 * @stock_id: a stock icon id
 * 
 * Makes @status_icon display the stock icon with the id @stock_id.
 * See gtk_status_icon_new_from_stock() for details.
 *
 * Since: 2.10 
 **/
void
gtk_status_icon_set_from_stock (GtkStatusIcon *status_icon,
				const gchar   *stock_id)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (stock_id != NULL);

  gtk_status_icon_set_image (status_icon, GTK_IMAGE_STOCK,
      			     (gpointer) stock_id);
}

/**
 * gtk_status_icon_set_from_icon_name:
 * @status_icon: a #GtkStatusIcon
 * @icon_name: an icon name
 * 
 * Makes @status_icon display the icon named @icon_name from the 
 * current icon theme.
 * See gtk_status_icon_new_from_icon_name() for details.
 *
 * Since: 2.10 
 **/
void
gtk_status_icon_set_from_icon_name (GtkStatusIcon *status_icon,
				    const gchar   *icon_name)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));
  g_return_if_fail (icon_name != NULL);

  gtk_status_icon_set_image (status_icon, GTK_IMAGE_ICON_NAME,
      			     (gpointer) icon_name);
}

/**
 * gtk_status_icon_get_storage_type:
 * @status_icon: 
 * 
 * Gets the type of representation being used by the #GtkStatusIcon
 * to store image data. If the #GtkStatusIcon has no image data,
 * the return value will be %GTK_IMAGE_EMPTY. 
 * 
 * Return value: the image representation being used
 *
 * Since: 2.10
 **/
GtkImageType
gtk_status_icon_get_storage_type (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), GTK_IMAGE_EMPTY);

  return status_icon->priv->storage_type;
}
/**
 * gtk_status_icon_get_pixbuf:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the #GdkPixbuf being displayed by the #GtkStatusIcon.
 * The storage type of the status icon must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_PIXBUF (see gtk_status_icon_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned pixbuf.
 * 
 * Return value: the displayed pixbuf, or %NULL if the image is empty.
 *
 * Since: 2.10
 **/
GdkPixbuf *
gtk_status_icon_get_pixbuf (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);
  g_return_val_if_fail (status_icon->priv->storage_type == GTK_IMAGE_PIXBUF ||
			status_icon->priv->storage_type == GTK_IMAGE_EMPTY, NULL);
                                                                                                             
  if (status_icon->priv->storage_type == GTK_IMAGE_EMPTY)
    status_icon->priv->image_data.pixbuf = NULL;
                                                                                                             
  return status_icon->priv->image_data.pixbuf;
}

/**
 * gtk_status_icon_get_stock:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the id of the stock icon being displayed by the #GtkStatusIcon.
 * The storage type of the status icon must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_STOCK (see gtk_status_icon_get_storage_type()).
 * The returned string is owned by the #GtkStatusIcon and should not
 * be freed or modified.
 * 
 * Return value: stock id of the displayed stock icon,
 *   or %NULL if the image is empty.
 *
 * Since: 2.10
 **/
G_CONST_RETURN gchar *
gtk_status_icon_get_stock (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);
  g_return_val_if_fail (status_icon->priv->storage_type == GTK_IMAGE_STOCK ||
			status_icon->priv->storage_type == GTK_IMAGE_EMPTY, NULL);
  
  if (status_icon->priv->storage_type == GTK_IMAGE_EMPTY)
    status_icon->priv->image_data.stock_id = NULL;

  return status_icon->priv->image_data.stock_id;
}

/**
 * gtk_status_icon_get_icon_name:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the name of the icon being displayed by the #GtkStatusIcon.
 * The storage type of the status icon must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ICON_NAME (see gtk_status_icon_get_storage_type()).
 * The returned string is owned by the #GtkStatusIcon and should not
 * be freed or modified.
 * 
 * Return value: name of the displayed icon, or %NULL if the image is empty.
 *
 * Since: 2.10
 **/
G_CONST_RETURN gchar *
gtk_status_icon_get_icon_name (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), NULL);
  g_return_val_if_fail (status_icon->priv->storage_type == GTK_IMAGE_ICON_NAME ||
			status_icon->priv->storage_type == GTK_IMAGE_EMPTY, NULL);

  if (status_icon->priv->storage_type == GTK_IMAGE_EMPTY)
    status_icon->priv->image_data.icon_name = NULL;

  return status_icon->priv->image_data.icon_name;
}

/**
 * gtk_status_icon_get_size:
 * @status_icon: a #GtkStatusIcon
 * 
 * Gets the size in pixels that is available for the image. 
 * Stock icons and named icons adapt their size automatically
 * if the size of the notification area changes. For other
 * storage types, the size-changed signal can be used to
 * react to size changes.
 * 
 * Return value: the size that is available for the image
 *
 * Since: 2.10
 **/
gint
gtk_status_icon_get_size (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), 0);

  return status_icon->priv->size;
}

/**
 * gtk_status_icon_set_tooltip:
 * @status_icon: a #GtkStatusIcon
 * @tooltip_text: the tooltip text, or %NULL
 * 
 * Sets the tooltip of the status icon.
 * 
 * Since: 2.10
 **/ 
void
gtk_status_icon_set_tooltip (GtkStatusIcon *status_icon,
			     const gchar   *tooltip_text)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  gtk_tooltips_set_tip (status_icon->priv->tooltips,
			status_icon->priv->tray_icon,
			tooltip_text, NULL);
}

static gboolean
gtk_status_icon_blinker (GtkStatusIcon *status_icon)
{
  status_icon->priv->blink_off = !status_icon->priv->blink_off;

  gtk_status_icon_update_image (status_icon);

  return TRUE;
}

static void
gtk_status_icon_enable_blinking (GtkStatusIcon *status_icon)
{
  if (!status_icon->priv->blinking_timeout)
    {
      gtk_status_icon_blinker (status_icon);

      status_icon->priv->blinking_timeout =
	g_timeout_add (BLINK_TIMEOUT, 
		       (GSourceFunc) gtk_status_icon_blinker, 
		       status_icon);
    }
}

static void
gtk_status_icon_disable_blinking (GtkStatusIcon *status_icon)
{
  if (status_icon->priv->blinking_timeout)
    {
      g_source_remove (status_icon->priv->blinking_timeout);
      status_icon->priv->blinking_timeout = 0;
      status_icon->priv->blink_off = FALSE;

      gtk_status_icon_update_image (status_icon);
    }
}

/**
 * gtk_status_icon_set_visible:
 * @status_icon: a #GtkStatusIcon
 * @visible: %TRUE to show the status icon, %FALSE to hide it
 * 
 * Shows or hides a status icon.
 *
 * Since: 2.10
 **/
void
gtk_status_icon_set_visible (GtkStatusIcon *status_icon,
			     gboolean       visible)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  visible = visible != FALSE;

  if (status_icon->priv->visible != visible)
    {
      status_icon->priv->visible = visible;

      if (visible)
	gtk_widget_show (status_icon->priv->tray_icon);
      else
	gtk_widget_hide (status_icon->priv->tray_icon);

      g_object_notify (G_OBJECT (status_icon), "visible");
    }
}

/**
 * gtk_status_icon_get_visible:
 * @status_icon: a #GtkStatusIcon
 * 
 * Returns wether the status icon is visible or not. 
 * Note that being visible does not guarantee that 
 * the user can actually see the icon, see also 
 * gtk_status_icon_is_embedded().
 * 
 * Return value: %TRUE if the status icon is visible
 *
 * Since: 2.10
 **/
gboolean
gtk_status_icon_get_visible (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

  return status_icon->priv->visible;
}

/**
 * gtk_status_icon_set_blinking:
 * @status_icon: a #GtkStatusIcon
 * @blinking: %TRUE to turn blinking on, %FALSE to turn it off
 * 
 * Makes the status icon start or stop blinking. 
 * Note that blinking user interface elements may be problematic
 * for some users, and thus may be turned off, in which case
 * this setting has no effect.
 *
 * Since: 2.10
 **/
void
gtk_status_icon_set_blinking (GtkStatusIcon *status_icon,
			      gboolean       blinking)
{
  g_return_if_fail (GTK_IS_STATUS_ICON (status_icon));

  blinking = blinking != FALSE;

  if (status_icon->priv->blinking != blinking)
    {
      status_icon->priv->blinking = blinking;

      if (blinking)
	gtk_status_icon_enable_blinking (status_icon);
      else
	gtk_status_icon_disable_blinking (status_icon);

      g_object_notify (G_OBJECT (status_icon), "blinking");
    }
}

/**
 * gtk_status_icon_get_blinking:
 * @status_icon: a #GtkStatusIcon
 * 
 * Returns whether the icon is blinking, see 
 * gtk_status_icon_set_blinking().
 * 
 * Return value: %TRUE if the icon is blinking
 *
 * Since: 2.10
 **/
gboolean
gtk_status_icon_get_blinking (GtkStatusIcon *status_icon)
{
  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

  return status_icon->priv->blinking;
}

/**
 * gtk_status_icon_is_embedded:
 * @status_icon: a #GtkStatusIcon
 * 
 * Returns whether the status icon is embedded in a notification
 * area. 
 * 
 * Return value: %TRUE if the status icon is embedded in
 *   a notification area.
 *
 * Since: 2.10
 **/
gboolean
gtk_status_icon_is_embedded (GtkStatusIcon *status_icon)
{
  GtkPlug *plug;

  g_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

  plug = GTK_PLUG (status_icon->priv->tray_icon);

  if (plug->socket_window)
    return TRUE;
  else
    return FALSE;
}

#define __GTK_STATUS_ICON_C__
#include "gtkaliasdef.c"
