/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include <gdk/gdkwindow.h>

#include <windowsx.h>
#include <objbase.h>

#include "gdkwin32.h"
#include "gdkdevice-wintab.h"

static GQuark quark_window_input_info = 0;
static GSList *input_windows = NULL;

typedef struct
{
  gdouble root_x;
  gdouble root_y;
  GHashTable *device_events;
} GdkWindowInputInfo;

static gboolean gdk_device_wintab_get_history (GdkDevice      *device,
                                               GdkWindow      *window,
                                               guint32         start,
                                               guint32         stop,
                                               GdkTimeCoord ***events,
                                               gint           *n_events);
static void gdk_device_wintab_get_state (GdkDevice       *device,
                                         GdkWindow       *window,
                                         gdouble         *axes,
                                         GdkModifierType *mask);
static void gdk_device_wintab_set_window_cursor (GdkDevice *device,
                                                 GdkWindow *window,
                                                 GdkCursor *cursor);
static void gdk_device_wintab_warp (GdkDevice *device,
                                    GdkScreen *screen,
                                    gint       x,
                                    gint       y);
static gboolean gdk_device_wintab_query_state (GdkDevice        *device,
                                               GdkWindow        *window,
                                               GdkWindow       **root_window,
                                               GdkWindow       **child_window,
                                               gint             *root_x,
                                               gint             *root_y,
                                               gint             *win_x,
                                               gint             *win_y,
                                               GdkModifierType  *mask);
static GdkGrabStatus gdk_device_wintab_grab   (GdkDevice     *device,
                                               GdkWindow     *window,
                                               gboolean       owner_events,
                                               GdkEventMask   event_mask,
                                               GdkWindow     *confine_to,
                                               GdkCursor     *cursor,
                                               guint32        time_);
static void          gdk_device_wintab_ungrab (GdkDevice     *device,
                                               guint32        time_);
static GdkWindow * gdk_device_wintab_window_at_position (GdkDevice       *device,
                                                         gint            *win_x,
                                                         gint            *win_y,
                                                         GdkModifierType *mask,
                                                         gboolean         get_toplevel);
static void      gdk_device_wintab_select_window_events (GdkDevice       *device,
                                                         GdkWindow       *window,
                                                         GdkEventMask     event_mask);


G_DEFINE_TYPE (GdkDeviceWintab, gdk_device_wintab, GDK_TYPE_DEVICE)

static void
gdk_device_wintab_class_init (GdkDeviceWintabClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_device_wintab_get_history;
  device_class->get_state = gdk_device_wintab_get_state;
  device_class->set_window_cursor = gdk_device_wintab_set_window_cursor;
  device_class->warp = gdk_device_wintab_warp;
  device_class->query_state = gdk_device_wintab_query_state;
  device_class->grab = gdk_device_wintab_grab;
  device_class->ungrab = gdk_device_wintab_ungrab;
  device_class->window_at_position = gdk_device_wintab_window_at_position;
  device_class->select_window_events = gdk_device_wintab_select_window_events;

  quark_window_input_info = g_quark_from_static_string ("gdk-window-input-info");
}

static void
gdk_device_wintab_init (GdkDeviceWintab *device_wintab)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_wintab);
}

static gboolean
gdk_device_wintab_get_history (GdkDevice      *device,
                               GdkWindow      *window,
                               guint32         start,
                               guint32         stop,
                               GdkTimeCoord ***events,
                               gint           *n_events)
{
  return FALSE;
}

static void
gdk_device_wintab_get_state (GdkDevice       *device,
                             GdkWindow       *window,
                             gdouble         *axes,
                             GdkModifierType *mask)
{
  GdkDeviceWintab *device_wintab;

  device_wintab = GDK_DEVICE_WINTAB (device);

  /* For now just use the last known button and axis state of the device.
   * Since graphical tablets send an insane amount of motion events each
   * second, the info should be fairly up to date */
  if (mask)
    {
      gdk_window_get_pointer (window, NULL, NULL, mask);
      *mask &= 0xFF; /* Mask away core pointer buttons */
      *mask |= ((device_wintab->button_state << 8)
                & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
                   | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
                   | GDK_BUTTON5_MASK));
    }

  if (device_wintab->last_axis_data)
    _gdk_device_wintab_translate_axes (device_wintab, window, axes, NULL, NULL);
}

static void
gdk_device_wintab_set_window_cursor (GdkDevice *device,
                                     GdkWindow *window,
                                     GdkCursor *cursor)
{
}

static void
gdk_device_wintab_warp (GdkDevice *device,
                        GdkScreen *screen,
                        gint       x,
                        gint       y)
{
}

static gboolean
gdk_device_wintab_query_state (GdkDevice        *device,
                               GdkWindow        *window,
                               GdkWindow       **root_window,
                               GdkWindow       **child_window,
                               gint             *root_x,
                               gint             *root_y,
                               gint             *win_x,
                               gint             *win_y,
                               GdkModifierType  *mask)
{
  return FALSE;
}

static GdkGrabStatus
gdk_device_wintab_grab (GdkDevice    *device,
                        GdkWindow    *window,
                        gboolean      owner_events,
                        GdkEventMask  event_mask,
                        GdkWindow    *confine_to,
                        GdkCursor    *cursor,
                        guint32       time_)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_wintab_ungrab (GdkDevice *device,
                          guint32    time_)
{
}

static GdkWindow *
gdk_device_wintab_window_at_position (GdkDevice       *device,
                                      gint            *win_x,
                                      gint            *win_y,
                                      GdkModifierType *mask,
                                      gboolean         get_toplevel)
{
  return NULL;
}

static void
input_info_free (GdkWindowInputInfo *info)
{
  g_hash_table_destroy (info->device_events);
  g_free (info);
}

static void
gdk_device_wintab_select_window_events (GdkDevice    *device,
                                        GdkWindow    *window,
                                        GdkEventMask  event_mask)
{
  GdkWindowInputInfo *info;

  info = g_object_get_qdata (G_OBJECT (window),
                             quark_window_input_info);
  if (event_mask)
    {
      if (!info)
        {
          info = g_new0 (GdkWindowInputInfo, 1);
          info->device_events = g_hash_table_new (NULL, NULL);

          g_object_set_qdata_full (G_OBJECT (window),
                                   quark_window_input_info,
                                   info,
                                   (GDestroyNotify) input_info_free);
          input_windows = g_slist_prepend (input_windows, window);
        }

      g_hash_table_insert (info->device_events, device,
                           GUINT_TO_POINTER (event_mask));
    }
  else if (info)
    {
      g_hash_table_remove (info->device_events, device);

      if (g_hash_table_size (info->device_events) == 0)
        {
          g_object_set_qdata (G_OBJECT (window),
                              quark_window_input_info,
                              NULL);
          input_windows = g_slist_remove (input_windows, window);
        }
    }
}

GdkEventMask
_gdk_device_wintab_get_events (GdkDeviceWintab *device,
                               GdkWindow       *window)
{
  GdkWindowInputInfo *info;

  info = g_object_get_qdata (G_OBJECT (window),
                             quark_window_input_info);

  if (!info)
    return 0;

  return GPOINTER_TO_UINT (g_hash_table_lookup (info->device_events, device));
}

gboolean
_gdk_device_wintab_get_window_coords (GdkWindow *window,
                                      gdouble   *root_x,
                                      gdouble   *root_y)
{
  GdkWindowInputInfo *info;

  info = g_object_get_qdata (G_OBJECT (window),
                             quark_window_input_info);

  if (!info)
    return FALSE;

  *root_x = info->root_x;
  *root_y = info->root_y;

  return TRUE;
}

void
_gdk_device_wintab_update_window_coords (GdkWindow *window)
{
  GdkWindowInputInfo *info;
  gint root_x, root_y;

  info = g_object_get_qdata (G_OBJECT (window),
                             quark_window_input_info);

  g_return_if_fail (info != NULL);

  gdk_window_get_origin (window, &root_x, &root_y);
  info->root_x = (gdouble) root_x;
  info->root_y = (gdouble) root_y;
}

void
_gdk_device_wintab_translate_axes (GdkDeviceWintab *device_wintab,
                                   GdkWindow       *window,
                                   gdouble         *axes,
                                   gdouble         *x,
                                   gdouble         *y)
{
  GdkDevice *device;
  GdkWindow *impl_window;
  gdouble root_x, root_y;
  gdouble temp_x, temp_y;
  gint i;

  device = GDK_DEVICE (device_wintab);
  impl_window = _gdk_window_get_impl_window (window);
  temp_x = temp_y = 0;

  if (!_gdk_device_wintab_get_window_coords (impl_window, &root_x, &root_y))
    return;

  for (i = 0; i < gdk_device_get_n_axes (device); i++)
    {
      GdkAxisUse use;

      use = _gdk_device_get_axis_use (device, i);

      switch (use)
        {
        case GDK_AXIS_X:
        case GDK_AXIS_Y:
          if (gdk_device_get_mode (device) == GDK_MODE_WINDOW)
            _gdk_device_translate_window_coord (device, window, i,
                                                device_wintab->last_axis_data[i],
                                                &axes[i]);
          else
            _gdk_device_translate_screen_coord (device, window,
                                                root_x, root_y, i,
                                                device_wintab->last_axis_data[i],
                                                &axes[i]);
          if (use == GDK_AXIS_X)
            temp_x = axes[i];
          else if (use == GDK_AXIS_Y)
            temp_y = axes[i];

          break;
        default:
          _gdk_device_translate_axis (device, i,
                                      device_wintab->last_axis_data[i],
                                      &axes[i]);
          break;
        }
    }

  if (x)
    *x = temp_x;

  if (y)
    *y = temp_y;
}

void
_gdk_input_check_extension_events (GdkDevice *device)
{
  GSList *l;

  if (!GDK_IS_DEVICE_WINTAB (device))
    return;

  for (l = input_windows; l; l = l->next)
    {
      GdkWindow *window_private;
      GdkEventMask event_mask = 0;

      window_private = l->data;

      if (gdk_device_get_mode (device) != GDK_MODE_DISABLED)
        event_mask = window_private->extension_events;

      gdk_window_set_device_events (GDK_WINDOW (window_private),
                                    device, event_mask);
    }
}
