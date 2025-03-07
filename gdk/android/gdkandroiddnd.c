/*
 * Copyright (c) 2024 Florian "sp1rit" <sp1rit@disroot.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkframeclockidleprivate.h"
#include "gdkdragsurfaceprivate.h"

#include "gdkandroidinit-private.h"
#include "gdkandroiddisplay-private.h"
#include "gdkandroidsurface-private.h"
#include "gdkandroidclipboard-private.h"

#include "gdkandroiddnd-private.h"

typedef struct _GdkAndroidDragSurfaceClass
{
  GdkSurfaceClass parent_class;
} GdkAndroidDragSurfaceClass;

typedef struct _GdkAndroidDragClass
{
  GdkDragClass parent_class;
} GdkAndroidDragClass;

typedef struct _GdkAndroidDropClass
{
  GdkDropClass parent_class;
} GdkAndroidDropClass;


static void gdk_android_drag_surface_iface_init(GdkDragSurfaceInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GdkAndroidDragSurface, gdk_android_drag_surface, GDK_TYPE_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE, gdk_android_drag_surface_iface_init))

static void
gdk_android_drag_surface_constructed (GObject *object)
{
  GdkAndroidDragSurface *self = (GdkAndroidDragSurface*)object;
  GdkSurface *surface = (GdkSurface *) self;

  GdkFrameClock *frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  G_OBJECT_CLASS (gdk_android_drag_surface_parent_class)->constructed (object);
}

static void
gdk_android_drag_surface_get_geometry (GdkSurface *surface,
                                       int        *x,
                                       int        *y,
                                       int        *width,
                                       int        *height)
{
  *x = surface->x;
  *y = surface->y;
  *width = surface->width;
  *height = surface->height;
}

static void
gdk_android_drag_surface_get_root_coords (GdkSurface *surface,
                                          int         x,
                                          int         y,
                                          int        *root_x,
                                          int        *root_y)
{
  *root_x = x - surface->x;
  *root_y = y - surface->y;
}

static void
gdk_android_drag_surface_hide (GdkSurface *surface)
{
  g_debug("Hiding drag surface");
}

static gdouble
gdk_android_drag_surface_get_scale (GdkSurface *surface)
{
  GdkAndroidDragSurface *self = (GdkAndroidDragSurface *)surface;
  return gdk_surface_get_scale (gdk_drag_get_surface ((GDK_DRAG (self->drag))));
}

static gboolean
gdk_android_drag_surface_compute_size (GdkSurface *surface)
{
  GdkAndroidDragSurface *self = (GdkAndroidDragSurface *) surface;
  if (surface->width != self->width || surface->height != self->height)
    {
      surface->width = self->width;
      surface->height = self->height;

      g_debug ("New DragSurface bounds: %dx%d", surface->width, surface->height);
      _gdk_surface_update_size (surface);
      gdk_surface_invalidate_rect (surface, NULL);
    }

  return FALSE;
}

static void
gdk_android_drag_surface_destroy (GdkSurface *surface, gboolean foreign_destroy)
{
  g_debug("Destroyed DragSurface");
}

static void
gdk_android_drag_surface_class_init (GdkAndroidDragSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = gdk_android_drag_surface_constructed;
  surface_class->get_geometry = gdk_android_drag_surface_get_geometry;
  surface_class->get_root_coords = gdk_android_drag_surface_get_root_coords;
  surface_class->hide = gdk_android_drag_surface_hide;
  surface_class->get_scale = gdk_android_drag_surface_get_scale;
  surface_class->compute_size = gdk_android_drag_surface_compute_size;
  surface_class->destroy = gdk_android_drag_surface_destroy;
}

static void
gdk_android_drag_surface_init (GdkAndroidDragSurface *self)
{
  self->width = -1;
  self->height = -1;
  self->hot_x = 0;
  self->hot_y = 0;
}

static gboolean
gdk_android_drag_surface_present (GdkDragSurface *drag_surface,
                                  int width, int height)
{
  GdkAndroidDragSurface *self = (GdkAndroidDragSurface *)drag_surface;
  self->width = width;
  self->height = height;

  g_debug("GdkAndroidDragSurface: presenting drag surface %dx%d", width, height);

  gdk_surface_request_layout ((GdkSurface *)self);
  return TRUE;
}

static void
gdk_android_drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_android_drag_surface_present;
}


G_DEFINE_TYPE (GdkAndroidDrag, gdk_android_drag, GDK_TYPE_DRAG)

static void
gdk_android_drag_finalize (GObject *object)
{
  GdkAndroidDrag *self = (GdkAndroidDrag *)object;
  GdkSurface *surface = (GdkSurface *)self->surface;
  G_OBJECT_CLASS (gdk_android_drag_parent_class)->finalize (object);

  // Destroy the surface after handlers finalize handlers of Object have been
  // called. GtkDragIcon expects the frame clock to still exist while it is
  // beeing destroyed. As self has the DragIcon in data, we must destroy the
  // surface only after the data of self has been destroyed.
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      gdk_surface_set_is_mapped (surface, FALSE);
      _gdk_surface_destroy (surface, TRUE);
    }
  g_object_unref (surface);
}

static void
gdk_android_drag_constructed (GObject *object)
{
  GdkAndroidDrag *self = (GdkAndroidDrag *)object;
  G_OBJECT_CLASS (gdk_android_drag_parent_class)->constructed (object);
  self->surface = g_object_new (GDK_TYPE_ANDROID_DRAG_SURFACE,
                                "display", gdk_drag_get_display ((GdkDrag *)self),
                                NULL);
  self->surface->drag = self;
}

static GdkSurface *
gdk_android_drag_get_drag_surface (GdkDrag *drag)
{
  GdkAndroidDrag *self = (GdkAndroidDrag *)drag;
  return (GdkSurface *)self->surface;
}

static void
gdk_android_drag_set_hotspot (GdkDrag *drag,
                              int      hot_x,
                              int      hot_y)
{
  GdkAndroidDrag *self = (GdkAndroidDrag *)drag;
  self->surface->hot_x = hot_x;
  self->surface->hot_y = hot_y;
  gdk_surface_invalidate_rect ((GdkSurface *)self->surface, NULL);
}

static void
gdk_android_drag_cancel (GdkDrag            *drag,
                         GdkDragCancelReason reason)
{
  JNIEnv *env = gdk_android_get_env();
  GdkAndroidSurface *initiator = (GdkAndroidSurface *)gdk_drag_get_surface (drag);
  (*env)->CallVoidMethod (env, initiator->surface,
                          gdk_android_get_java_cache ()->surface.cancel_dnd);
}

static void
gdk_android_drag_class_init (GdkAndroidDragClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragClass *drag_class = GDK_DRAG_CLASS (klass);

  object_class->finalize = gdk_android_drag_finalize;
  object_class->constructed = gdk_android_drag_constructed;
  drag_class->get_drag_surface = gdk_android_drag_get_drag_surface;
  drag_class->set_hotspot = gdk_android_drag_set_hotspot;
  drag_class->cancel = gdk_android_drag_cancel;
}

static void
gdk_android_drag_init (GdkAndroidDrag *self)
{}


G_DEFINE_TYPE (GdkAndroidDrop, gdk_android_drop, GDK_TYPE_DROP)

static void
gdk_android_drop_finalize (GObject *object)
{
  GdkAndroidDrop *self = (GdkAndroidDrop *)object;
  JNIEnv *env = gdk_android_get_env();
  if (self->drop)
    (*env)->DeleteGlobalRef (env, self->drop);
  G_OBJECT_CLASS (gdk_android_drop_parent_class)->finalize (object);
}

static void
gdk_android_drop_status (GdkDrop *drop,
                         GdkDragAction actions,
                         GdkDragAction preferred)
{
  GdkAndroidDrop *self = (GdkAndroidDrop *)drop;
  self->possible_actions = actions;
}

static void
gdk_android_drop_finish (GdkDrop *drop, GdkDragAction action)
{
  GdkAndroidDrop *self = (GdkAndroidDrop *)drop;
  self->commited_action = action;
  self->drop_finished = TRUE;
}

static void
gdk_android_drop_read_async (GdkDrop            *drop,
                             GdkContentFormats  *formats,
                             int                 io_priority,
                             GCancellable       *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer            user_data)
{
  GdkAndroidDrop *self = (GdkAndroidDrop *)drop;

  GTask *task = g_task_new (drop, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_android_drop_read_async);
  g_task_set_priority (task, io_priority);

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame (env, 1);

  jobject clipdata = (*env)->CallObjectMethod (env, self->drop,
                                               gdk_android_get_java_cache ()->a_drag_event.get_clip_data);
  if (!clipdata)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_PERMISSION_DENIED,
                               "Attempted to access DnD event data before drop");
      goto cleanup;
    }

  gdk_android_clipdata_read_async (task, clipdata, formats);

cleanup:
  (*env)->PopLocalFrame (env, NULL);
  g_clear_object (&task);
}

static GInputStream *
gdk_android_drop_read_finish (GdkDrop      *drop,
                              GAsyncResult *result,
                              const char  **out_mime_type,
                              GError      **error)
{
  return gdk_android_clipdata_read_finish (result, out_mime_type, error);
}

static void
gdk_android_drop_class_init (GdkAndroidDropClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GdkDropClass *drop_class = (GdkDropClass *)klass;

  object_class->finalize = gdk_android_drop_finalize;
  drop_class->status = gdk_android_drop_status;
  drop_class->finish = gdk_android_drop_finish;
  drop_class->read_async = gdk_android_drop_read_async;
  drop_class->read_finish = gdk_android_drop_read_finish;
}

static void
gdk_android_drop_init (GdkAndroidDrop *self)
{
  self->possible_actions = 0;
  self->drop = NULL;
  self->commited_action = 0;
}

static void
gdk_android_dnd_surface_drag_clipdata_cb (GdkContentProvider *provider, GAsyncResult *res, GdkAndroidDrag *drag)
{
  JNIEnv *env = gdk_android_get_env ();
  GError *err = NULL;
  (*env)->PushLocalFrame (env, 3);
  jobject clipdata = gdk_android_clipboard_clipdata_from_provider_finish (provider, res, &err);
  if (!clipdata)
    {
      g_critical ("Failed producing clipdata: %s", err->message);
      g_error_free (err);
      goto exit;
    }

  GdkAndroidSurface *self = (GdkAndroidSurface *)gdk_drag_get_surface ((GdkDrag *)drag);

  jobject empty = (*env)->NewObject(env,
                                    gdk_android_get_java_cache ()->clipboard_empty_drag_shadow.klass,
                                    gdk_android_get_java_cache ()->clipboard_empty_drag_shadow.constructor,
                                    self->surface);
  jobject native_identifier = (*env)->NewObject(env,
                                                gdk_android_get_java_cache ()->clipboard_native_drag_identifier.klass,
                                                gdk_android_get_java_cache ()->clipboard_native_drag_identifier.constructor,
                                                (jlong)GPOINTER_TO_SIZE (drag));
  jint drag_flags = 0;
  if (!(*env)->IsInstanceOf (env, clipdata, gdk_android_get_java_cache ()->clipboard_internal_clipdata.klass))
    drag_flags |= gdk_android_get_java_cache ()->a_view.drag_global;
  (*env)->CallVoidMethod (env, self->surface,
                          gdk_android_get_java_cache ()->surface.start_dnd,
                          clipdata, empty,
                          native_identifier,
                          drag_flags);

  gdk_surface_set_is_mapped ((GdkSurface *)drag->surface, TRUE);

exit:
  (*env)->PopLocalFrame (env, NULL);
  g_object_unref (drag);
}

GdkDrag *
gdk_android_dnd_surface_drag_begin (GdkSurface         *surface,
                                    GdkDevice          *device,
                                    GdkContentProvider *content,
                                    GdkDragAction       actions,
                                    double dx, double dy)
{
  GdkAndroidSurface *self = (GdkAndroidSurface *)surface;
  GdkAndroidDisplay *display = (GdkAndroidDisplay *) gdk_surface_get_display (surface);

  GdkContentFormats *formats = gdk_content_provider_ref_formats (content);

  GdkDragAction action;
  if (actions & GDK_ACTION_MOVE)
    action = GDK_ACTION_MOVE;
  else
    action = GDK_ACTION_COPY;

  GdkAndroidDrag *drag = g_object_new (GDK_TYPE_ANDROID_DRAG,
                                       "device", device,
                                       "content", content,
                                       "formats", formats,
                                       "selected-action", action,
                                       "actions", actions,
                                       "surface", surface,
                                       NULL);
  g_hash_table_insert (display->drags, (gpointer)drag, g_object_ref(drag));

  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame(env, 1);
  jobject context = (*env)->CallObjectMethod (env, self->surface,
                                              gdk_android_get_java_cache ()->a_view.get_context);
  gdk_android_clipboard_clipdata_from_provider_async (content,
                                                      formats,
                                                      context,
                                                      NULL,
                                                      (GAsyncReadyCallback)gdk_android_dnd_surface_drag_clipdata_cb,
                                                      g_object_ref (drag));
  (*env)->PopLocalFrame (env, NULL);
  gdk_content_formats_unref (formats);
  return (GdkDrag *)drag;
}

static GdkDrag*
gdk_android_drag_from_native_identifier (GdkAndroidDisplay *display,
                                         jobject native_identifier,
                                         jlong* identifier)
{
  JNIEnv *env = gdk_android_get_env ();
  jlong native = (*env)->GetLongField (env, native_identifier,
                                       gdk_android_get_java_cache ()->clipboard_native_drag_identifier.native_identifier);
  GdkDrag *drag = g_hash_table_lookup (display->drags, GSIZE_TO_POINTER (native));
  if (!drag)
    return NULL;

  if (identifier)
    *identifier = native;

  return drag;
}
static GdkDrag*
gdk_android_drag_from_drop_event (GdkAndroidDisplay *display,
                                  jobject event,
                                  jlong* identifier)
{
  JNIEnv *env = gdk_android_get_env ();
  (*env)->PushLocalFrame(env, 1);
  jobject native_identifier = (*env)->CallObjectMethod (env, event,
                                                        gdk_android_get_java_cache ()->a_drag_event.get_local_state);
  if (!native_identifier)
    goto failure;
  if (!(*env)->IsInstanceOf (env, native_identifier,
                             gdk_android_get_java_cache ()->clipboard_native_drag_identifier.klass))
    goto failure;
  GdkDrag *drag = gdk_android_drag_from_native_identifier (display,
                                                           native_identifier,
                                                           identifier);
  if (!drag)
    goto failure;

  (*env)->PopLocalFrame (env, NULL);
  return drag;

failure:
  (*env)->PopLocalFrame (env, NULL);
  return NULL;
}

void
gdk_android_dnd_handle_drag_start_fail (GdkAndroidDisplay *display,
                                        jobject native_identifier)
{
  jlong native;
  GdkDrag *drag = gdk_android_drag_from_native_identifier (display,
                                                           native_identifier,
                                                           &native);
  if (drag)
    {
      gdk_drag_drop_done (drag, FALSE);
      g_hash_table_remove (display->drags, GSIZE_TO_POINTER (native));
    }
}

gboolean
gdk_android_dnd_surface_handle_drop_event (GdkAndroidSurface *surface,
                                           jobject event)
{
  GdkAndroidDrop *self = (GdkAndroidDrop *)surface->active_drop;
  GdkDrop *drop = (GdkDrop *)self;

  GdkAndroidDisplay *display = (GdkAndroidDisplay *)gdk_surface_get_display ((GdkSurface *)surface);
  JNIEnv *env = gdk_android_get_env ();

  jint action = (*env)->CallIntMethod (env, event,
                                       gdk_android_get_java_cache ()->a_drag_event.get_action);

  if (action == gdk_android_get_java_cache ()->a_drag_event.action_started)
    {
      if (self)
        g_object_unref (self);

      (*env)->PushLocalFrame (env, 1);
      jobject clipdesc = (*env)->CallObjectMethod (env, event,
                                                   gdk_android_get_java_cache ()->a_drag_event.get_clip_description);
      GdkContentFormats *formats = gdk_android_clipboard_description_to_formats (clipdesc);
      surface->active_drop = g_object_new (GDK_TYPE_ANDROID_DROP,
                                        "actions", GDK_ACTION_COPY | GDK_ACTION_MOVE,
                                        "device", display->seat->logical_pointer,
                                        "drag", gdk_android_drag_from_drop_event (display, event, NULL),
                                        "formats", formats,
                                        "surface", surface,
                                        NULL);
      gdk_content_formats_unref (formats);
      (*env)->PopLocalFrame (env, NULL);
      return TRUE;
    }
  else if (action == gdk_android_get_java_cache ()->a_drag_event.action_entered)
    {
      g_return_val_if_fail (GDK_IS_ANDROID_DROP (self), FALSE);
      gdk_drop_emit_enter_event (drop, TRUE, 0., 0., GDK_CURRENT_TIME);
      return self->possible_actions != 0;
    }
  else if (action == gdk_android_get_java_cache ()->a_drag_event.action_location)
    {
      g_return_val_if_fail (GDK_IS_ANDROID_DROP (self), FALSE);
      jfloat x = (*env)->CallFloatMethod (env, event,
                                          gdk_android_get_java_cache()->a_drag_event.get_x);
      jfloat y = (*env)->CallFloatMethod (env, event,
                                          gdk_android_get_java_cache()->a_drag_event.get_y);
      gdk_drop_emit_motion_event (drop,
                                  TRUE,
                                  x / surface->cfg.scale,
                                  y / surface->cfg.scale,
                                  GDK_CURRENT_TIME);
      return self->possible_actions != 0;
    }
  else if (action == gdk_android_get_java_cache ()->a_drag_event.action_exited)
    {
      g_return_val_if_fail (GDK_IS_ANDROID_DROP (self), FALSE);
      gdk_drop_emit_leave_event (drop, TRUE, GDK_CURRENT_TIME);
      return TRUE;
    }
  else if (action == gdk_android_get_java_cache ()->a_drag_event.action_ended)
    {
      g_clear_object(&surface->active_drop);
      jlong native;
      GdkDrag *drag = gdk_android_drag_from_drop_event (display, event, &native);
      if (drag)
        {
          g_object_ref (drag);
          jboolean successful = (*env)->CallBooleanMethod (env, event,
                                                           gdk_android_get_java_cache()->a_drag_event.get_result);
          if (successful)
            g_signal_emit_by_name (drag, "drop-performed");
          gdk_drag_drop_done (drag, successful);
          g_hash_table_remove (display->drags, GSIZE_TO_POINTER (native));
          g_signal_emit_by_name (drag, "dnd-finished");
          g_object_unref (drag);
        }
      return TRUE;
    }
  else if (action == gdk_android_get_java_cache ()->a_drag_event.action_drop)
    {
      g_return_val_if_fail (GDK_IS_ANDROID_DROP (self), FALSE);

      if (self->drop)
        (*env)->DeleteGlobalRef (env, self->drop);
      self->drop = (*env)->NewGlobalRef (env, event);
      jfloat x = (*env)->CallFloatMethod (env, event,
                                          gdk_android_get_java_cache()->a_drag_event.get_x);
      jfloat y = (*env)->CallFloatMethod (env, event,
                                          gdk_android_get_java_cache()->a_drag_event.get_y);
      if (self->possible_actions != 0)
        {
          self->drop_finished = FALSE;
          gdk_drop_emit_drop_event (drop,
                                    TRUE,
                                    x / surface->cfg.scale,
                                    y / surface->cfg.scale,
                                    GDK_CURRENT_TIME);
          while (!self->drop_finished)
            g_main_context_iteration (NULL, FALSE);
        }
      gdk_drop_emit_leave_event (drop, TRUE, GDK_CURRENT_TIME);

      return self->commited_action != 0;
    }
  return FALSE;
}
