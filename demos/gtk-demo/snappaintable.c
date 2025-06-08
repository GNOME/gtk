#include "snappaintable.h"

#include <gtk/gtk.h>

struct _SnapPaintable
{
  GObject parent_instance;
  GFile *file;
  GBytes *bytes;
  gsize width;
  gsize height;
  gsize stride;
  GskRectSnap snap;
  int zoom;
  gboolean use_tiles;
  gsize rows, cols;
  GdkTexture **tiles;
};

static void
ensure_tiles (SnapPaintable *self)
{
  gsize x, y;
  const guchar *data;
  gsize size;

  if (self->rows > 0 && self->cols > 0)
    return;

  self->cols = ceil (self->width / 64.0);
  self->rows = ceil (self->height / 64.0);

  self->tiles = g_new0 (GdkTexture *, self->rows * self->cols);

  data = g_bytes_get_data (self->bytes, &size);

  for (y = 0; y < self->height; y += 64)
    {
      for (x = 0; x < self->width; x += 64)
        {
          gsize w, h;
          gsize offset;
          GBytes *b;

          w = MIN (64, self->width - x);
          h = MIN (64, self->height - y);

          offset = y * self->stride + x * 4;

          b = g_bytes_new (data + offset, size); /* FIXME */

          self->tiles[(y / 64) * self->cols + (x / 64)] =
              gdk_memory_texture_new (w, h, GDK_MEMORY_DEFAULT, b, self->stride);
          g_bytes_unref (b);
        }
    }
}

static void
clear_tiles (SnapPaintable *self)
{
  for (gsize i = 0; i < self->rows * self->cols; i++)
    g_object_unref (self->tiles[i]);
  g_free (self->tiles);
  self->tiles = NULL;
  self->rows = 0;
  self->cols = 0;
}

 /* {{{ GdkPaintable implementation */

static float
get_zoom (SnapPaintable *self)
{
  return powf (1.2, self->zoom);
}

static void
snap_paintable_snapshot (GdkPaintable *paintable,
                         GdkSnapshot  *snapshot,
                         double        width,
                         double        height)
{
  SnapPaintable *self = SNAP_PAINTABLE (paintable);
  const guchar *data;
  const guchar *row;

  if (!self->bytes)
    return;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_set_snap (snapshot, self->snap);
  gtk_snapshot_scale (snapshot, get_zoom (self), get_zoom (self));

  data = g_bytes_get_data (self->bytes, NULL);

  if (self->use_tiles)
    {
      for (gsize j = 0; j < self->rows; j++)
        {
          for (gsize i = 0; i < self->cols; i++)
            {
              GdkTexture *texture = self->tiles[j * self->cols + i];

              gtk_snapshot_append_texture (snapshot,
                                           texture,
                                           &GRAPHENE_RECT_INIT (0, 0,
                                                                gdk_texture_get_width (texture),
                                                                gdk_texture_get_height (texture)));
              gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (64.0, 0.0));
            }
          gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-64.0 * self->cols, 64.0));
        }
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0.0, -64.0 * self->rows));
    }
  else /* pixels */
    {
      for (gsize j = 0; j < self->height; j++)
        {
          row = data + j * self->stride;

          for (gsize i = 0; i < self->width; i++)
            {
              float b = row[4*i] / 255.0;
              float g = row[4*i + 1] / 255.0;
              float r = row[4*i + 2] / 255.0;
              float a = row[4*i + 3] / 255.0;

              gtk_snapshot_append_color (snapshot,
                                         &(GdkRGBA) { r, g, b, a },
                                         &GRAPHENE_RECT_INIT (i, j, 1, 1));
            }
        }
    }

  gtk_snapshot_restore (snapshot);
}

static int
snap_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  SnapPaintable *self = SNAP_PAINTABLE (paintable);

  return ceil (get_zoom (self) * self->width);
}

static int
snap_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  SnapPaintable *self = SNAP_PAINTABLE (paintable);

  return ceil (get_zoom (self) * self->height);
}

static void
snap_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = snap_paintable_snapshot;
  iface->get_intrinsic_width = snap_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = snap_paintable_get_intrinsic_height;
}

 /* }}} */
/* {{{ GObject boilerplate */

struct _SnapPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FILE = 1,
  PROP_SNAP,
  PROP_ZOOM,
  PROP_TILES,
  NUM_PROPERTIES
};

G_DEFINE_TYPE_WITH_CODE (SnapPaintable, snap_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                snap_paintable_init_interface))

static void
snap_paintable_init (SnapPaintable *self)
{
}

static void
snap_paintable_dispose (GObject *object)
{
  SnapPaintable *self = SNAP_PAINTABLE (object);

  g_clear_object (&self->file);
  g_clear_pointer (&self->bytes, g_bytes_unref);
  clear_tiles (self);

  G_OBJECT_CLASS (snap_paintable_parent_class)->dispose (object);
}

static void
snap_paintable_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  SnapPaintable *self = SNAP_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      snap_paintable_set_file (self, g_value_get_object (value));
      break;

    case PROP_SNAP:
      snap_paintable_set_snap (self, g_value_get_uint (value));
      break;

    case PROP_ZOOM:
      snap_paintable_set_zoom (self, g_value_get_int (value));
      break;

    case PROP_TILES:
      snap_paintable_set_tiles (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
snap_paintable_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  SnapPaintable *self = SNAP_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_SNAP:
      g_value_set_uint (value, self->snap);
      break;

    case PROP_ZOOM:
      g_value_set_int (value, self->snap);
      break;

    case PROP_TILES:
      g_value_set_boolean (value, self->use_tiles);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
snap_paintable_class_init (SnapPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = snap_paintable_dispose;
  object_class->set_property = snap_paintable_set_property;
  object_class->get_property = snap_paintable_get_property;

  g_object_class_install_property (object_class, PROP_FILE,
                                   g_param_spec_object ("file", NULL, NULL,
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_SNAP,
                                   g_param_spec_uint ("snap", NULL, NULL,
                                                      0, G_MAXUINT, GSK_RECT_SNAP_NONE,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_ZOOM,
                                   g_param_spec_int ("zoom", NULL, NULL,
                                                      G_MININT, G_MAXINT, 0,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_TILES,
                                   g_param_spec_boolean ("tiles", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

/* }}} */
/* {{{ Public API */

SnapPaintable *
snap_paintable_new (GFile *file)
{
  return g_object_new (SNAP_TYPE_PAINTABLE,
                       "file", file,
                       NULL);
}

void
snap_paintable_set_file (SnapPaintable *self,
                         GFile         *file)
{
  GdkTexture *texture;

  g_return_if_fail (SNAP_IS_PAINTABLE (self));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  if (!g_set_object (&self->file, file))
    return;

  g_clear_pointer (&self->bytes, g_bytes_unref);
  clear_tiles (self);

  if (file)
    texture = gdk_texture_new_from_file (file, NULL);
  else
    texture = NULL;

  if (texture)
    {
      GdkTextureDownloader *downloader;

      self->width = gdk_texture_get_width (texture);
      self->height = gdk_texture_get_height (texture);

      downloader = gdk_texture_downloader_new (texture);

      self->bytes = gdk_texture_downloader_download_bytes (downloader, &self->stride);

      gdk_texture_downloader_free (downloader);
      g_object_unref (texture);

      if (self->use_tiles)
        ensure_tiles (self);
    }

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify (G_OBJECT (self), "file");
}

GskRectSnap
snap_paintable_get_snap (SnapPaintable *self)
{
  g_return_val_if_fail (SNAP_IS_PAINTABLE (self), GSK_RECT_SNAP_NONE);

  return self->snap;
}

void
snap_paintable_set_snap (SnapPaintable *self,
                         GskRectSnap    snap)
{
  g_return_if_fail (SNAP_IS_PAINTABLE (self));

  if (self->snap == snap)
    return;

  self->snap = snap;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify (G_OBJECT (self), "snap");
}

int
snap_paintable_get_zoom (SnapPaintable *self)
{
  g_return_val_if_fail (SNAP_IS_PAINTABLE (self), 0);

  return self->zoom;
}

void
snap_paintable_set_zoom (SnapPaintable *self,
                         int            zoom)
{
  g_return_if_fail (SNAP_IS_PAINTABLE (self));

  if (self->zoom == zoom)
    return;

  self->zoom = zoom;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify (G_OBJECT (self), "zoom");
}

gboolean
snap_paintable_get_tiles (SnapPaintable *self)
{
  g_return_val_if_fail (SNAP_IS_PAINTABLE (self), FALSE);

  return self->use_tiles;
}

void
snap_paintable_set_tiles (SnapPaintable *self,
                          gboolean       tiles)
{
  g_return_if_fail (SNAP_IS_PAINTABLE (self));

  if (self->use_tiles == tiles)
    return;

  self->use_tiles = tiles;

  if (tiles)
    ensure_tiles (self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify (G_OBJECT (self), "tiles");
}

/* }}} */

/* vim:set foldmethod=marker: */
