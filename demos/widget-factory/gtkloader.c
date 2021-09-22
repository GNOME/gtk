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

#include "gtkloaderprivate.h"
#include <gtk/gtk.h>

enum {
  PROP_RESOURCE = 1,
};

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

  return 0;
}

static int
gtk_loader_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkLoader *self = GTK_LOADER (paintable);

  if (self->texture)
    return gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (self->texture));

  return 0;
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

static void gtk_loader_set_resource (GtkLoader *self, const char *resource);

static void
gtk_loader_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkLoader *self = (GtkLoader *)object;

  switch (prop_id)
    {
    case PROP_RESOURCE:
      gtk_loader_set_resource (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_loader_class_init (GtkLoaderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_loader_dispose;
  gobject_class->set_property = gtk_loader_set_property;

  g_object_class_install_property (gobject_class, PROP_RESOURCE,
      g_param_spec_string ("resource", "", "",
                           NULL,
                           G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
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
  const char *resource = task_data;
  GdkTexture *texture;
  GError *error = NULL;

  texture = gdk_texture_new_from_resource (resource);

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

static void
gtk_loader_set_resource (GtkLoader  *self,
                         const char *resource)
{
  GTask *task;

  task = g_task_new (self, NULL, texture_finished, NULL);
  g_task_set_task_data (task, g_strdup (resource), (GDestroyNotify)g_free);
  g_task_run_in_thread (task, load_texture_in_thread);
  g_object_unref (task);
}

GdkPaintable *
gtk_loader_new (void)
{
  return g_object_new (GTK_TYPE_LOADER, NULL);
}
