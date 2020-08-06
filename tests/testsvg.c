/* testsvg.c
 * Copyright (C) 2020 Red Hat, Inc.
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
#include <librsvg/rsvg.h>

#define SVG_TYPE_PAINTABLE (svg_paintable_get_type ())

G_DECLARE_FINAL_TYPE (SvgPaintable, svg_paintable, SVG, PAINTABLE, GObject)

struct _SvgPaintable
{
  GObject parent_instance;
  GFile *file;
  RsvgHandle *handle;
};

struct _SvgPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FILE = 1,
  NUM_PROPERTIES
};

static void
svg_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
  SvgPaintable *self = SVG_PAINTABLE (paintable);
  cairo_t *cr;
  GError *error = NULL;

  cr = gtk_snapshot_append_cairo (GTK_SNAPSHOT (snapshot),
                                  &GRAPHENE_RECT_INIT (0, 0, width, height));

  if (!rsvg_handle_render_document (self->handle, cr,
                                    &(RsvgRectangle) {0, 0, width, height},
                                    &error))
    {
      g_error ("%s", error->message);
    }

  cairo_destroy (cr);
}

static void
svg_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = svg_paintable_snapshot;
}

G_DEFINE_TYPE_WITH_CODE (SvgPaintable, svg_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                svg_paintable_init_interface))

static void
svg_paintable_init (SvgPaintable *self)
{
}

static void
svg_paintable_dispose (GObject *object)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  g_clear_object (&self->file);
  g_clear_object (&self->handle);

  G_OBJECT_CLASS (svg_paintable_parent_class)->dispose (object);
}

static void
svg_paintable_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      {
        GFile *file = g_value_get_object (value);
        RsvgHandle *handle = rsvg_handle_new_from_gfile_sync (file,
                                                              RSVG_HANDLE_FLAGS_NONE,
                                                              NULL,
                                                              NULL);
        g_set_object (&self->file, file);
        g_set_object (&self->handle, handle);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
svg_paintable_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
svg_paintable_class_init (SvgPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = svg_paintable_dispose;
  object_class->set_property = svg_paintable_set_property;
  object_class->get_property = svg_paintable_get_property;

  g_object_class_install_property (object_class, PROP_FILE,
                                   g_param_spec_object ("file", "File", "File",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}

static SvgPaintable *
svg_paintable_new (GFile *file)
{
  return g_object_new (SVG_TYPE_PAINTABLE,
                       "file", file,
                       NULL);
}

static void
file_set (GtkFileChooserButton *button,
          GtkWidget            *picture)
{
  GFile *file;
  SvgPaintable *paintable;

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (button));

  paintable = svg_paintable_new (file);
  gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (paintable));

  g_object_unref (paintable);
  g_object_unref (file);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *header;
  GtkWidget *picture;
  GtkFileFilter *filter;
  GtkWidget *button;

  gtk_init ();

  window = gtk_window_new ();
  header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);
  button = gtk_file_chooser_button_new ("Select an SVG file", GTK_FILE_CHOOSER_ACTION_OPEN);
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/svg+xml");
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (button), filter);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

  picture = gtk_picture_new ();
  g_signal_connect (button, "file-set", G_CALLBACK (file_set), picture);

  gtk_window_set_child (GTK_WINDOW (window), picture);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
