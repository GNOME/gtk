/*
 * Copyright © 2018 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkloaderprivate.h"

#include "gtksnapshot.h"

struct _GtkLoader
{
  GObject parent_instance;

  GdkTexture *texture;
};

struct _GtkLoaderClass
{
  GObjectClass parent_class;
};

static void
gtk_loader_paintable_snapshot (GdkPaintable *paintable,
                               GdkSnapshot  *snapshot,
                               double        width,
                               double        height)
{
  GtkLoader *self = GTK_LOADER (paintable);

  if (self->texture)
    gdk_paintable_snapshot (GDK_PAINTABLE (self->texture), snapshot, width, height);
}

static GdkPaintable *
gtk_loader_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkLoader *self = GTK_LOADER (paintable);

  if (self->texture)
    return gdk_paintable_get_current_image (GDK_PAINTABLE (self->texture));

  // FIXME: return a loading image
  return NULL;
}

static int
gtk_loader_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkLoader *self = GTK_LOADER (paintable);

  if (self->texture)
    return gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (self->texture));

  return 16;
}

static int
gtk_loader_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkLoader *self = GTK_LOADER (paintable);

  if (self->texture)
    return gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (self->texture));

  return 16;
}

static double
gtk_loader_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkLoader *self = GTK_LOADER (paintable);

  if (self->texture)
    return gdk_paintable_get_intrinsic_aspect_ratio (GDK_PAINTABLE (self->texture));

  return 0;
};

static void
gtk_loader_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_loader_paintable_snapshot;
  iface->get_current_image = gtk_loader_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_loader_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_loader_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_loader_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_EXTENDED (GtkLoader, gtk_loader, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_loader_paintable_init))

static void
gtk_loader_dispose (GObject *object)
{
  GtkLoader *self = GTK_LOADER (object);

  g_clear_object (&self->texture);

  G_OBJECT_CLASS (gtk_loader_parent_class)->dispose (object);
}

static void
gtk_loader_class_init (GtkLoaderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_loader_dispose;
}

static void
gtk_loader_init (GtkLoader *self)
{
}

static void
load_texture_in_thread (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  GBytes *bytes = task_data;
  GdkTexture *texture;
  GError *error = NULL;

  texture = gdk_texture_new_from_bytes (bytes, &error);

  if (texture)
    g_task_return_pointer (task, texture, g_object_unref);
  else
    g_task_return_error (task, error);
}

static void
texture_finished (GObject      *source,
                  GAsyncResult *result,
                  gpointer      data)
{
  GtkLoader *self = GTK_LOADER (source);
  GdkTexture *texture;
  GError *error = NULL;

  texture = g_task_propagate_pointer (G_TASK (result), &error);

  if (texture)
    {
      self->texture = g_object_ref (texture);

      gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
    }
}

GdkPaintable *
gtk_loader_new (GBytes *bytes)
{
  GtkLoader *self;
  GTask *task;

  g_return_val_if_fail (bytes != NULL, NULL);

  self = g_object_new (GTK_TYPE_LOADER, NULL);

  task = g_task_new (self, NULL, texture_finished, NULL);
  g_task_set_task_data (task, g_bytes_ref (bytes), (GDestroyNotify)g_bytes_unref);
  g_task_run_in_thread (task, load_texture_in_thread);
  g_object_unref (task);

  return GDK_PAINTABLE (self);
}
