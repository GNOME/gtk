/* gtktrayicon.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2005 Hans Breuer  <hans@breuer.org>
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
 * This is an implementation of the gtktrayicon interface *not*
 * following the freedesktop.org "system tray" spec,
 * http://www.freedesktop.org/wiki/Standards/systemtray-spec
 */

#include <config.h>
#include <string.h>
#include <libintl.h>

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktrayicon.h"

#include "gtkimage.h"
#include "gtkiconfactory.h"

#include "win32/gdkwin32.h"

#include "gtkalias.h"

#define WIN32_MEAN_AND_LEAN
#include <windows.h>

#define WM_GTK_TRAY_NOTIFICATION (WM_USER+1)

struct _GtkTrayIconPrivate
{
  NOTIFYICONDATA nid;
};

static void gtk_tray_icon_add (GtkContainer   *container,
			       GtkWidget      *widget);
static void gtk_tray_icon_size_request (GtkWidget        *widget,
					GtkRequisition   *requisition);
static void gtk_tray_icon_size_allocate (GtkWidget          *widget,
				         GtkAllocation      *allocation);
static void gtk_tray_icon_finalize (GObject *object);


G_DEFINE_TYPE (GtkTrayIcon, gtk_tray_icon, GTK_TYPE_PLUG);

static void
gtk_tray_icon_class_init (GtkTrayIconClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  gobject_class->finalize     = gtk_tray_icon_finalize;

  widget_class->size_request = gtk_tray_icon_size_request;
  //widget_class->size_allocate = gtk_tray_icon_size_allocate;

  container_class->add = gtk_tray_icon_add;

  g_type_class_add_private (class, sizeof (GtkTrayIconPrivate));
}

static LRESULT CALLBACK
_win32_on_tray_change (HWND   hwnd,
                       UINT   message,
                       WPARAM wparam,
                       LPARAM lparam)
{
  if (WM_GTK_TRAY_NOTIFICATION == message)
    {
      GdkEventButton e = {0, };
      GtkTrayIcon *tray_icon = GTK_TRAY_ICON (wparam);
      switch (lparam)
	{
	case WM_MOUSEMOVE :
	  g_print ("GtkTrayIcon::WM_MOUSEMOVE\n");
	  break;
	case WM_RBUTTONDBLCLK :
	case WM_LBUTTONDBLCLK :
	  g_print ("GtkTrayIcon::WM_LBUTTONDBLCLK\n");
	  e.type = GDK_2BUTTON_PRESS;
	  e.button = (WM_LBUTTONDBLCLK == lparam) ? 1 : 3;
	  break;
	case WM_LBUTTONUP :
	case WM_RBUTTONUP :
	  /* deliberately ignored */
	  break;
	case WM_LBUTTONDOWN :
	case WM_RBUTTONDOWN :
	  g_print ("GtkTrayIcon::WM_RBUTTONUP\n");
	  e.type = GDK_BUTTON_PRESS;
	  e.button = (WM_LBUTTONDOWN == lparam) ? 1 : 3;
	  break;
	default :
	  g_print ("GtkTrayIcon::%d\n", lparam);
	  break;
	}
	g_signal_emit_by_name (tray_icon, "button-press-event", &e);
	return 0;
    }
  else
    {
      return DefWindowProc (hwnd, message, wparam, lparam);
    }
}

HWND
_gdk_win32_create_tray_observer (void)
{
  WNDCLASS    wclass;
  static HWND hwnd = NULL;
  ATOM        klass;
  HINSTANCE   hmodule = GetModuleHandle (NULL);

  if (hwnd)
    return hwnd;

  memset (&wclass, 0, sizeof(WNDCLASS));
  wclass.lpszClassName = "GtkTrayNotification";
  wclass.lpfnWndProc   = _win32_on_tray_change;
  wclass.hInstance     = hmodule;

  klass = RegisterClass (&wclass);
  if (!klass)
    return NULL;

  hwnd = CreateWindow (MAKEINTRESOURCE(klass),
                       NULL, WS_POPUP,
                       0, 0, 16, 16, NULL, NULL,
                       hmodule, NULL);
  if (!hwnd)
    {
      UnregisterClass (MAKEINTRESOURCE(klass), hmodule);
      return NULL;
    }
  return hwnd;
}

static void
gtk_tray_icon_init (GtkTrayIcon *icon)
{
  icon->priv = G_TYPE_INSTANCE_GET_PRIVATE (icon, GTK_TYPE_TRAY_ICON,
					    GtkTrayIconPrivate);
  memset (&icon->priv->nid, 0, sizeof (NOTIFYICONDATA));

  icon->priv->nid.hWnd = _gdk_win32_create_tray_observer ();
  icon->priv->nid.uID = GPOINTER_TO_UINT (icon);
  icon->priv->nid.uCallbackMessage = WM_GTK_TRAY_NOTIFICATION;
  icon->priv->nid.uFlags = NIF_ICON|NIF_MESSAGE; //NIF_TIP 
}

static void
gtk_tray_icon_finalize (GObject *object)
{
  GtkTrayIcon *icon = GTK_TRAY_ICON (object);

  Shell_NotifyIcon (NIM_DELETE, &icon->priv->nid);

  G_OBJECT_CLASS (gtk_tray_icon_parent_class)->finalize (object);
}

static void
tray_image_changed (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    data)
{
  GtkImage    *image;
  GtkWidget   *widget;
  GdkPixbuf   *pixbuf = NULL;
  GtkTrayIcon *icon = GTK_TRAY_ICON (data);

  g_return_if_fail (GTK_IS_IMAGE (object));
  g_return_if_fail (GTK_IS_TRAY_ICON (data));

  widget = GTK_WIDGET (object);
  image = GTK_IMAGE (object);

  /*
   * TODO: If nothing changed don't do nothing.
   * we get called three times for one change - for 'size', 
   * 'storage-type', 'pixbuf' - could be cached.
   * But 'visible'(==FALSE) needs to be handled!
   */
  switch (image->storage_type)
  {
  case GTK_IMAGE_PIXBUF :
    pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (object));
    g_object_ref (pixbuf);
    g_print ("GtkTrayIcon::image_changed size=%dx%d\n",
             pixbuf ? gdk_pixbuf_get_width (pixbuf) : 0,
	     pixbuf ? gdk_pixbuf_get_height (pixbuf) : 0);
    break;
  case GTK_IMAGE_EMPTY :
    g_print ("GtkTrayIcon::image_changed EMPTY\n");
    break;
  case GTK_IMAGE_ICON_NAME :
    {
      GtkIconSet *icon_set = NULL;
      const char* name = NULL;
      gtk_image_get_icon_name (image, &name, NULL);
      g_print ("GtkTrayIcon::image_changed '%s'\n", name);

      icon_set = gtk_style_lookup_icon_set (widget->style, name);

      pixbuf = gtk_icon_set_render_icon (icon_set, 
                                         widget->style,
                                         gtk_widget_get_direction (GTK_WIDGET(icon)),
                                         GTK_STATE_NORMAL,
                                         GTK_ICON_SIZE_BUTTON,
                                         widget, NULL); 
    }
    break;
  default :
    g_print ("GtkTrayIcon::image_changed %d\n", image->storage_type);
    break;
  }

  if (pixbuf)
    {
      HICON hIcon = icon->priv->nid.hIcon;

      icon->priv->nid.hIcon = gdk_win32_pixbuf_to_hicon_libgtk_only (pixbuf);

      Shell_NotifyIcon (hIcon ? NIM_MODIFY : NIM_ADD, &icon->priv->nid);
      if (hIcon)
        DestroyIcon (hIcon);
      g_object_unref (pixbuf);
    }
}

static void 
gtk_tray_icon_add (GtkContainer   *container,
		   GtkWidget      *widget)
{
  g_return_if_fail (GTK_IS_IMAGE (widget));

  if (GTK_CONTAINER_CLASS (gtk_tray_icon_parent_class)->add)
    GTK_CONTAINER_CLASS (gtk_tray_icon_parent_class)->add (container, widget);

  g_signal_connect (widget, 
                    "notify",
                    G_CALLBACK (tray_image_changed),
		    container);
}

static void
gtk_tray_icon_size_request (GtkWidget      *widget,
		            GtkRequisition *requisition)
{
  requisition->width = 16;
  requisition->height = 16;
}

static void
gtk_tray_icon_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  widget->allocation = *allocation;
}

GtkTrayIcon *
_gtk_tray_icon_new (const gchar *name)
{
  return g_object_new (GTK_TYPE_TRAY_ICON, 
		       "title", name, 
		       NULL);
}

GtkOrientation 
_gtk_tray_icon_get_orientation (GtkTrayIcon *icon)
{
  g_return_val_if_fail (GTK_IS_TRAY_ICON (icon), GTK_ORIENTATION_HORIZONTAL);
  //FIXME: should we deliver the orientation of the tray ??
  return GTK_ORIENTATION_VERTICAL;
}
