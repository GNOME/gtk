/* testpaintablecursor.c
 * Copyright (C) 2024 Red Hat, Inc.
 * Author: Matthias Clasen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (MyPaintable, my_paintable, MY, PAINTABLE, GObject)

struct _MyPaintable
{
  GObject parent_instance;
  GdkTexture *texture;
  int width;
  int height;
  double angle;

  guint timeout_id;
};

struct _MyPaintableClass
{
  GObjectClass parent_class;
};


static void
my_paintable_snapshot (GdkPaintable *paintable,
                       GdkSnapshot  *snapshot,
                       double        width,
                       double        height)
{
  MyPaintable *self = MY_PAINTABLE (paintable);

  gtk_snapshot_push_opacity (GTK_SNAPSHOT (snapshot), 0.5 * sin (self->angle) + 0.5);
  gtk_snapshot_append_texture (GTK_SNAPSHOT (snapshot), self->texture,
                               &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_pop (GTK_SNAPSHOT (snapshot));
}

static int
my_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  MyPaintable *self = MY_PAINTABLE (paintable);

  return self->width;
}

static int
my_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  MyPaintable *self = MY_PAINTABLE (paintable);

  return self->height;
}

static void
my_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = my_paintable_snapshot;
  iface->get_intrinsic_width = my_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = my_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE (MyPaintable, my_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                my_paintable_init_interface))

static void
my_paintable_init (MyPaintable *self)
{
}

static void
my_paintable_dispose (GObject *object)
{
  MyPaintable *self = MY_PAINTABLE (object);

  g_clear_object (&self->texture);

  g_source_remove (self->timeout_id);
  self->timeout_id = 0;

  G_OBJECT_CLASS (my_paintable_parent_class)->dispose (object);
}

static void
my_paintable_class_init (MyPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = my_paintable_dispose;
}

static gboolean
rotate_cb (gpointer data)
{
  MyPaintable *self = data;

  self->angle += 20;

  g_signal_emit_by_name (self, "invalidate-contents", 0);

  return G_SOURCE_CONTINUE;
}

static GdkPaintable *
my_paintable_new (GdkTexture *texture, int width, int height)
{
  MyPaintable *self;

  self = g_object_new (my_paintable_get_type (), NULL);
  self->texture = g_object_ref (texture);
  self->width = width;
  self->height = height;

  self->angle = 0;

  self->timeout_id = g_timeout_add (100, rotate_cb, self);

  return GDK_PAINTABLE (self);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *button;
  gboolean done = FALSE;
  GdkTexture *texture;
  GdkCursor *cursor;
  GdkPaintable *paintable;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "hello world");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  button = gtk_button_new ();
  gtk_button_set_label (GTK_BUTTON (button), "hello world");
  gtk_widget_set_margin_top (button, 10);
  gtk_widget_set_margin_bottom (button, 10);
  gtk_widget_set_margin_start (button, 10);
  gtk_widget_set_margin_end (button, 10);

  /* A 96x96 image */
  texture = gdk_texture_new_from_filename ("tests/all-scroll.png", NULL);

#if 0
  cursor = gdk_cursor_new_from_name ("all-scroll", NULL);
#elif 0
  cursor = gdk_cursor_new_from_texture (texture, gdk_texture_ge t_width (texture) / 2, gdk_texture_get_height (texture) / 2, NULL);
#else
  /* We create a cursor of size 32x32 */
  paintable = my_paintable_new (texture, 32, 32);
  cursor = gdk_cursor_new_from_paintable (paintable, 16, 16, NULL);
  g_object_unref (paintable);
#endif

  g_object_unref (texture);

  gtk_widget_set_cursor (button, cursor);
  g_object_unref (cursor);

  gtk_window_set_child (GTK_WINDOW (window), button);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
