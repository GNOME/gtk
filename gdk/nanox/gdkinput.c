#include "gdk.h"
#include "gdkprivate-nanox.h"
#include "gdkinputprivate.h"

static const GdkAxisUse gdk_input_core_axes[] = { GDK_AXIS_X, GDK_AXIS_Y };

const GdkDeviceInfo gdk_input_core_info =
{
  GDK_CORE_POINTER,
  "Core Pointer",
  GDK_SOURCE_MOUSE,
  GDK_MODE_SCREEN,
  TRUE,
  2,
  gdk_input_core_axes
};

/* Global variables  */

GdkInputVTable    gdk_input_vtable;
/* information about network port and host for gxid daemon */
gchar            *gdk_input_gxid_host;
gint              gdk_input_gxid_port;
gint              gdk_input_ignore_core;

GList            *gdk_input_devices;
GList            *gdk_input_windows;

GList *
gdk_input_list_devices (void)
{
  return gdk_input_devices;
}

void
gdk_input_set_source (guint32 deviceid, GdkInputSource source)
{
  GdkDevicePrivate *gdkdev = gdk_input_find_device(deviceid);
  g_return_if_fail (gdkdev != NULL);

  gdkdev->info.source = source;
}

gboolean
gdk_input_set_mode (guint32 deviceid, GdkInputMode mode)
{
  if (deviceid == GDK_CORE_POINTER)
    return FALSE;

  if (gdk_input_vtable.set_mode)
    return gdk_input_vtable.set_mode(deviceid,mode);
  else
    return FALSE;
}

void
gdk_input_set_axes (guint32 deviceid, GdkAxisUse *axes)
{
  if (deviceid != GDK_CORE_POINTER && gdk_input_vtable.set_axes)
    gdk_input_vtable.set_axes (deviceid, axes);
}

void gdk_input_set_key (guint32 deviceid,
			guint   index,
			guint   keyval,
			GdkModifierType modifiers)
{
  if (deviceid != GDK_CORE_POINTER && gdk_input_vtable.set_key)
    gdk_input_vtable.set_key (deviceid, index, keyval, modifiers);
}

GdkTimeCoord *
gdk_input_motion_events (GdkWindow *window,
			 guint32 deviceid,
			 guint32 start,
			 guint32 stop,
			 gint *nevents_return)
{
  GdkTimeCoord *coords;
  int i;

  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  
  if (GDK_DRAWABLE_DESTROYED (window))
    return NULL;

  if (deviceid == GDK_CORE_POINTER)
    {
	return NULL;
    }
  else
    {
      if (gdk_input_vtable.motion_events)
	{
	  return gdk_input_vtable.motion_events(window,
						deviceid, start, stop,
						nevents_return);
	}
      else
	{
	  *nevents_return = 0;
	  return NULL;
	}
    }
}

gint
gdk_input_enable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  if (gdk_input_vtable.enable_window)
    return gdk_input_vtable.enable_window (window, gdkdev);
  else
    return TRUE;
}

gint
gdk_input_disable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  if (gdk_input_vtable.disable_window)
    return gdk_input_vtable.disable_window(window,gdkdev);
  else
    return TRUE;
}


GdkInputWindow *
gdk_input_window_find(GdkWindow *window)
{
  GList *tmp_list;

  for (tmp_list=gdk_input_windows; tmp_list; tmp_list=tmp_list->next)
    if (((GdkInputWindow *)(tmp_list->data))->window == window)
      return (GdkInputWindow *)(tmp_list->data);

  return NULL;      /* Not found */
}

/* FIXME: this routine currently needs to be called between creation
   and the corresponding configure event (because it doesn't get the
   root_relative_geometry).  This should work with
   gtk_window_set_extension_events, but will likely fail in other
   cases */

void
gdk_input_set_extension_events (GdkWindow *window, gint mask,
				GdkExtensionMode mode)
{
  GdkWindowPrivate *window_private;
  GList *tmp_list;
  GdkInputWindow *iw;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  window_private = (GdkWindowPrivate*) window;
  if (GDK_DRAWABLE_DESTROYED (window))
    return;

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  if (mask != 0)
    {
      iw = g_new(GdkInputWindow,1);

      iw->window = window;
      iw->mode = mode;

      iw->obscuring = NULL;
      iw->num_obscuring = 0;
      iw->grabbed = FALSE;

      gdk_input_windows = g_list_append(gdk_input_windows,iw);
      window_private->extension_events = mask;

      /* Add enter window events to the event mask */
      /* FIXME, this is not needed for XINPUT_NONE */
      gdk_window_set_events (window,
			     gdk_window_get_events (window) | 
			     GDK_ENTER_NOTIFY_MASK);
    }
  else
    {
      iw = gdk_input_window_find (window);
      if (iw)
	{
	  gdk_input_windows = g_list_remove(gdk_input_windows,iw);
	  g_free(iw);
	}

      window_private->extension_events = 0;
    }

  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)(tmp_list->data);

      if (gdkdev->info.deviceid != GDK_CORE_POINTER)
	{
	  if (mask != 0 && gdkdev->info.mode != GDK_MODE_DISABLED
	      && (gdkdev->info.has_cursor || mode == GDK_EXTENSION_EVENTS_ALL))
	    gdk_input_enable_window(window,gdkdev);
	  else
	    gdk_input_disable_window(window,gdkdev);
	}
    }
}

void
gdk_input_window_destroy (GdkWindow *window)
{
  GdkInputWindow *input_window;

  input_window = gdk_input_window_find (window);
  g_return_if_fail (input_window != NULL);

  gdk_input_windows = g_list_remove (gdk_input_windows,input_window);
  g_free(input_window);
}

void
gdk_input_exit (void)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;

  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev->info.deviceid != GDK_CORE_POINTER)
	{
	  gdk_input_set_mode(gdkdev->info.deviceid,GDK_MODE_DISABLED);

	  g_free(gdkdev->info.name);
#ifndef XINPUT_NONE	  
	  g_free(gdkdev->axes);
#endif	  
	  g_free(gdkdev->info.axes);
	  g_free(gdkdev->info.keys);
	  g_free(gdkdev);
	}
    }

  g_list_free(gdk_input_devices);

  for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      g_free(tmp_list->data);
    }
  g_list_free(gdk_input_windows);
}

GdkDevicePrivate *
gdk_input_find_device(guint32 id)
{
  GList *tmp_list = gdk_input_devices;
  GdkDevicePrivate *gdkdev;
  while (tmp_list)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev->info.deviceid == id)
	return gdkdev;
      tmp_list = tmp_list->next;
    }
  return NULL;
}

void
gdk_input_window_get_pointer (GdkWindow       *window,
			      guint32	  deviceid,
			      gdouble         *x,
			      gdouble         *y,
			      gdouble         *pressure,
			      gdouble         *xtilt,
			      gdouble         *ytilt,
			      GdkModifierType *mask)
{
  if (gdk_input_vtable.get_pointer)
    gdk_input_vtable.get_pointer (window, deviceid, x, y, pressure,
				  xtilt, ytilt, mask);
}
