/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkcursor.h"
#include "gdkcursorprivate.h"
#include "gdktexture.h"
#include "gdkdisplay.h"
#include <glib/gi18n-lib.h>

#include <math.h>
#include <errno.h>

#include <graphene.h>

/**
 * GdkCursor:
 *
 * `GdkCursor` is used to create and destroy cursors.
 *
 * Cursors are immutable objects, so once you created them, there is no way
 * to modify them later. You should create a new cursor when you want to change
 * something about it.
 *
 * Cursors by themselves are not very interesting: they must be bound to a
 * window for users to see them. This is done with [method@Gdk.Surface.set_cursor]
 * or [method@Gdk.Surface.set_device_cursor]. Applications will typically
 * use higher-level GTK functions such as [gtk_widget_set_cursor()](../gtk4/method.Widget.set_cursor.html)
 * instead.
 *
 * Cursors are not bound to a given [class@Gdk.Display], so they can be shared.
 * However, the appearance of cursors may vary when used on different
 * platforms.
 *
 * ## Named and texture cursors
 *
 * There are multiple ways to create cursors. The platform's own cursors
 * can be created with [ctor@Gdk.Cursor.new_from_name]. That function lists
 * the commonly available names that are shared with the CSS specification.
 * Other names may be available, depending on the platform in use. On some
 * platforms, what images are used for named cursors may be influenced by
 * the cursor theme.
 *
 * Another option to create a cursor is to use [ctor@Gdk.Cursor.new_from_texture]
 * and provide an image to use for the cursor.
 *
 * To ease work with unsupported cursors, a fallback cursor can be provided.
 * If a [class@Gdk.Surface] cannot use a cursor because of the reasons mentioned
 * above, it will try the fallback cursor. Fallback cursors can themselves have
 * fallback cursors again, so it is possible to provide a chain of progressively
 * easier to support cursors. If none of the provided cursors can be supported,
 * the default cursor will be the ultimate fallback.
 */

enum {
  PROP_0,
  PROP_FALLBACK,
  PROP_HOTSPOT_X,
  PROP_HOTSPOT_Y,
  PROP_NAME,
  PROP_TEXTURE,
  PROP_PAINTABLE,
};

static guint invalidate_signal;

G_DEFINE_TYPE (GdkCursor, gdk_cursor, G_TYPE_OBJECT)

static void
gdk_cursor_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkCursor *cursor = GDK_CURSOR (object);

  switch (prop_id)
    {
    case PROP_FALLBACK:
      g_value_set_object (value, cursor->fallback);
      break;
    case PROP_HOTSPOT_X:
      g_value_set_int (value, cursor->hotspot_x);
      break;
    case PROP_HOTSPOT_Y:
      g_value_set_int (value, cursor->hotspot_y);
      break;
    case PROP_NAME:
      g_value_set_string (value, cursor->name);
      break;
    case PROP_TEXTURE:
      g_value_set_object (value, cursor->texture);
      break;
    case PROP_PAINTABLE:
      g_value_set_object (value, cursor->paintable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_cursor_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkCursor *cursor = GDK_CURSOR (object);

  switch (prop_id)
    {
    case PROP_FALLBACK:
      cursor->fallback = g_value_dup_object (value);
      break;
    case PROP_HOTSPOT_X:
      cursor->hotspot_x = g_value_get_int (value);
      break;
    case PROP_HOTSPOT_Y:
      cursor->hotspot_y = g_value_get_int (value);
      break;
    case PROP_NAME:
      cursor->name = g_value_dup_string (value);
      break;
    case PROP_TEXTURE:
      cursor->texture = g_value_dup_object (value);
      break;
    case PROP_PAINTABLE:
      cursor->paintable = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_cursor_finalize (GObject *object)
{
  GdkCursor *cursor = GDK_CURSOR (object);

  g_free (cursor->name);
  g_clear_object (&cursor->texture);
  g_clear_object (&cursor->fallback);
  g_clear_object (&cursor->paintable);

  if (cursor->destroy)
    cursor->destroy (cursor->data);

  G_OBJECT_CLASS (gdk_cursor_parent_class)->finalize (object);
}

static void
gdk_cursor_class_init (GdkCursorClass *cursor_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cursor_class);

  object_class->get_property = gdk_cursor_get_property;
  object_class->set_property = gdk_cursor_set_property;
  object_class->finalize = gdk_cursor_finalize;

  /**
   * GdkCursor:fallback: (attributes org.gtk.Property.get=gdk_cursor_get_fallback)
   *
   * Cursor to fall back to if this cursor cannot be displayed.
   */
  g_object_class_install_property (object_class,
                                   PROP_FALLBACK,
                                   g_param_spec_object ("fallback", NULL, NULL,
                                                        GDK_TYPE_CURSOR,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GdkCursor:hotspot-x: (attributes org.gtk.Property.get=gdk_cursor_get_hotspot_x)
   *
   * X position of the cursor hotspot in the cursor image.
   */
  g_object_class_install_property (object_class,
                                   PROP_HOTSPOT_X,
                                   g_param_spec_int ("hotspot-x", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));

  /**
   * GdkCursor:hotspot-y: (attributes org.gtk.Property.get=gdk_cursor_get_hotspot_y)
   *
   * Y position of the cursor hotspot in the cursor image.
   */
  g_object_class_install_property (object_class,
                                   PROP_HOTSPOT_Y,
                                   g_param_spec_int ("hotspot-y", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));

  /**
   * GdkCursor:name: (attributes org.gtk.Property.get=gdk_cursor_get_name)
   *
   * Name of this this cursor.
   *
   * The name will be %NULL if the cursor was created from a texture.
   */
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GdkCursor:texture:
   *
   * The texture displayed by this cursor.
   *
   * The texture will be %NULL if the cursor was created from a name.
   */
  g_object_class_install_property (object_class,
                                   PROP_TEXTURE,
                                   g_param_spec_object ("texture", NULL, NULL,
                                                        GDK_TYPE_TEXTURE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GdkCursor:paintable:
   *
   * The paintable displayed by this cursor.
   *
   * The intrinsic size of the paintable determines
   * the size of the cursor.
   *
   * If the GDK backend has the capability, non-static paintables
   * may create animated cursors.
   *
   * The paintable will be %NULL if the cursor was created
   * from a name or texture.
   *
   * Since: 4.16
   */
  g_object_class_install_property (object_class,
                                   PROP_PAINTABLE,
                                   g_param_spec_object ("paintable", NULL, NULL,
                                                        GDK_TYPE_PAINTABLE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  invalidate_signal = g_signal_new ("invalidate",
                                    GDK_TYPE_CURSOR,
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL,
                                    NULL,
                                    G_TYPE_NONE, 0);
}

static void
gdk_cursor_init (GdkCursor *cursor)
{
}

guint
gdk_cursor_hash (gconstpointer pointer)
{
  const GdkCursor *cursor = pointer;
  guint hash;

  if (cursor->fallback)
    hash = gdk_cursor_hash (cursor->fallback) << 16;
  else
    hash = 0;

  if (cursor->name)
    hash ^= g_str_hash (cursor->name);
  else if (cursor->texture)
    hash ^= g_direct_hash (cursor->texture);
  else if (cursor->paintable)
    hash ^= g_direct_hash (cursor->paintable);

  hash ^= (cursor->hotspot_x << 8) | cursor->hotspot_y;

  return hash;
}

gboolean
gdk_cursor_equal (gconstpointer a,
                  gconstpointer b)
{
  const GdkCursor *ca = a;
  const GdkCursor *cb = b;

  if ((ca->fallback != NULL) != (cb->fallback != NULL))
    return FALSE;
  if (ca->fallback != NULL && !gdk_cursor_equal (ca->fallback, cb->fallback))
    return FALSE;

  if (g_strcmp0 (ca->name, cb->name) != 0)
    return FALSE;

  if (ca->texture != cb->texture)
    return FALSE;

  if (ca->paintable != cb->paintable)
    return FALSE;

  if (ca->hotspot_x != cb->hotspot_x ||
      ca->hotspot_y != cb->hotspot_y)
    return FALSE;

  return TRUE;
}

/**
 * gdk_cursor_new_from_name:
 * @name: the name of the cursor
 * @fallback: (nullable): %NULL or the `GdkCursor` to fall back to when
 *   this one cannot be supported
 *
 * Creates a new cursor by looking up @name in the current cursor
 * theme.
 *
 * A recommended set of cursor names that will work across different
 * platforms can be found in the CSS specification:
 *
 * | | | | |
 * | --- | --- | ---- | --- |
 * | "none" | ![](default_cursor.png) "default" | ![](help_cursor.png) "help" | ![](pointer_cursor.png) "pointer" |
 * | ![](context_menu_cursor.png) "context-menu" | ![](progress_cursor.png) "progress" | ![](wait_cursor.png) "wait" | ![](cell_cursor.png) "cell" |
 * | ![](crosshair_cursor.png) "crosshair" | ![](text_cursor.png) "text" | ![](vertical_text_cursor.png) "vertical-text" | ![](alias_cursor.png) "alias" |
 * | ![](copy_cursor.png) "copy" | ![](no_drop_cursor.png) "no-drop" | ![](move_cursor.png) "move" | ![](not_allowed_cursor.png) "not-allowed" |
 * | ![](grab_cursor.png) "grab" | ![](grabbing_cursor.png) "grabbing" | ![](all_scroll_cursor.png) "all-scroll" | ![](col_resize_cursor.png) "col-resize" |
 * | ![](row_resize_cursor.png) "row-resize" | ![](n_resize_cursor.png) "n-resize" | ![](e_resize_cursor.png) "e-resize" | ![](s_resize_cursor.png) "s-resize" |
 * | ![](w_resize_cursor.png) "w-resize" | ![](ne_resize_cursor.png) "ne-resize" | ![](nw_resize_cursor.png) "nw-resize" | ![](sw_resize_cursor.png) "sw-resize" |
 * | ![](se_resize_cursor.png) "se-resize" | ![](ew_resize_cursor.png) "ew-resize" | ![](ns_resize_cursor.png) "ns-resize" | ![](nesw_resize_cursor.png) "nesw-resize" |
 * | ![](nwse_resize_cursor.png) "nwse-resize" | ![](zoom_in_cursor.png) "zoom-in" | ![](zoom_out_cursor.png) "zoom-out" | |
 *
 * Returns: (nullable): a new `GdkCursor`, or %NULL if there is no
 *   cursor with the given name
 */
GdkCursor *
gdk_cursor_new_from_name (const char *name,
                          GdkCursor  *fallback)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (fallback == NULL || GDK_IS_CURSOR (fallback), NULL);

  return g_object_new (GDK_TYPE_CURSOR,
                       "name", name,
                       "fallback", fallback,
                       NULL);
}

/**
 * gdk_cursor_new_from_texture:
 * @texture: the texture providing the pixel data
 * @hotspot_x: the horizontal offset of the “hotspot” of the cursor
 * @hotspot_y: the vertical offset of the “hotspot” of the cursor
 * @fallback: (nullable): the `GdkCursor` to fall back to when
 *   this one cannot be supported
 *
 * Creates a new cursor from a `GdkTexture`.
 *
 * Returns: a new `GdkCursor`
 */
GdkCursor *
gdk_cursor_new_from_texture (GdkTexture *texture,
                             int         hotspot_x,
                             int         hotspot_y,
                             GdkCursor  *fallback)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (0 <= hotspot_x && hotspot_x < gdk_texture_get_width (texture), NULL);
  g_return_val_if_fail (0 <= hotspot_y && hotspot_y < gdk_texture_get_height (texture), NULL);
  g_return_val_if_fail (fallback == NULL || GDK_IS_CURSOR (fallback), NULL);

  return g_object_new (GDK_TYPE_CURSOR,
                       "texture", texture,
                       "hotspot-x", hotspot_x,
                       "hotspot-y", hotspot_y,
                       "fallback", fallback,
                       NULL);
}

/**
 * gdk_cursor_new_from_paintable:
 * @paintable: the paintable providing the pixel data
 * @hotspot_x: the horizontal offset of the “hotspot” of the cursor
 * @hotspot_y: the vertical offset of the “hotspot” of the cursor
 * @fallback: (nullable): the `GdkCursor` to fall back to when
 *   this one cannot be supported
 *
 * Creates a new cursor from a `GdkPaintable`.
 *
 * The intrinsic size of the paintable determines
 * the size of the cursor.
 *
 * If the GDK backend has the capability, non-static paintables
 * may create animated cursors.
 *
 * Returns: a new `GdkCursor`
 *
 * Since: 4.16
 */
GdkCursor *
gdk_cursor_new_from_paintable (GdkPaintable *paintable,
                               int           hotspot_x,
                               int           hotspot_y,
                               GdkCursor    *fallback)
{
  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), NULL);
  g_return_val_if_fail (0 <= hotspot_x && hotspot_x < gdk_paintable_get_intrinsic_width (paintable), NULL);
  g_return_val_if_fail (0 <= hotspot_y && hotspot_y < gdk_paintable_get_intrinsic_height (paintable), NULL);
  g_return_val_if_fail (fallback == NULL || GDK_IS_CURSOR (fallback), NULL);

  return g_object_new (GDK_TYPE_CURSOR,
                       "paintable", paintable,
                       "hotspot-x", hotspot_x,
                       "hotspot-y", hotspot_y,
                       "fallback", fallback,
                       NULL);
}

/**
 * gdk_cursor_get_fallback: (attributes org.gtk.Method.get_property=fallback)
 * @cursor: a `GdkCursor`
 *
 * Returns the fallback for this @cursor.
 *
 * The fallback will be used if this cursor is not available on a given
 * `GdkDisplay`. For named cursors, this can happen when using nonstandard
 * names or when using an incomplete cursor theme. For textured cursors,
 * this can happen when the texture is too large or when the `GdkDisplay`
 * it is used on does not support textured cursors.
 *
 * Returns: (transfer none) (nullable): the fallback of the cursor or %NULL
 *   to use the default cursor as fallback
 */
GdkCursor *
gdk_cursor_get_fallback (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return cursor->fallback;
}

/**
 * gdk_cursor_get_name: (attributes org.gtk.Method.get_property=name)
 * @cursor: a `GdkCursor`
 *
 * Returns the name of the cursor.
 *
 * If the cursor is not a named cursor, %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): the name of the cursor or %NULL
 *   if it is not a named cursor
 */
const char *
gdk_cursor_get_name (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return cursor->name;
}

/**
 * gdk_cursor_get_texture:
 * @cursor: a `GdkCursor`
 *
 * Returns the texture for the cursor.
 *
 * If the cursor is a named cursor, %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): the texture for cursor or %NULL
 *   if it is a named cursor
 */
GdkTexture *
gdk_cursor_get_texture (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return cursor->texture;
}

/**
 * gdk_cursor_get_paintable:
 * @cursor: a `GdkCursor`
 *
 * Returns the paintable for the cursor.
 *
 * If the cursor is a named or texture cursor, %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): the paintable for @cursor
 *   or %NULL if it is a named or texture cursor
 *
 * Since: 4.16
 */
GdkPaintable *
gdk_cursor_get_paintable (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return cursor->paintable;
}

/**
 * gdk_cursor_get_hotspot_x: (attributes org.gtk.Method.get_property=hotspot-x)
 * @cursor: a `GdkCursor`
 *
 * Returns the horizontal offset of the hotspot.
 *
 * The hotspot indicates the pixel that will be directly above the cursor.
 *
 * Note that named cursors may have a nonzero hotspot, but this function
 * will only return the hotspot position for cursors created with
 * [ctor@Gdk.Cursor.new_from_texture].
 *
 * Returns: the horizontal offset of the hotspot or 0 for named cursors
 */
int
gdk_cursor_get_hotspot_x (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), 0);

  return cursor->hotspot_x;
}

/**
 * gdk_cursor_get_hotspot_y: (attributes org.gtk.Method.get_property=hotspot-y)
 * @cursor: a `GdkCursor`
 *
 * Returns the vertical offset of the hotspot.
 *
 * The hotspot indicates the pixel that will be directly above the cursor.
 *
 * Note that named cursors may have a nonzero hotspot, but this function
 * will only return the hotspot position for cursors created with
 * [ctor@Gdk.Cursor.new_from_texture].
 *
 * Returns: the vertical offset of the hotspot or 0 for named cursors
 */
int
gdk_cursor_get_hotspot_y (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), 0);

  return cursor->hotspot_y;
}

/* Hack. We don't include gsk/gsk.h here to avoid a build order problem
 * with the generated header gskenumtypes.h, so we need to hack around
 * a bit to access the gsk and gtk api we need.
 */

typedef struct _GskRenderer GskRenderer;
typedef struct _GskRenderNode GskRenderNode;
typedef struct _GdkSnapshot GtkSnapshot;

extern GskRenderer *   gsk_cairo_renderer_new           (void);
extern gboolean        gsk_renderer_realize_for_display (GskRenderer            *renderer,
                                                         GdkDisplay             *display,
                                                         GError                **error);
extern void            gsk_renderer_unrealize           (GskRenderer            *renderer);
extern GdkTexture *    gsk_renderer_render_texture      (GskRenderer            *renderer,
                                                         GskRenderNode          *root,
                                                         const graphene_rect_t  *viewport);

extern void            gsk_render_node_get_bounds       (GskRenderNode          *node,
                                                         graphene_rect_t        *bounds);

extern void            gsk_render_node_unref            (GskRenderNode          *node);

extern GtkSnapshot *   gtk_snapshot_new                 (void);
extern GskRenderNode * gtk_snapshot_free_to_node        (GtkSnapshot            *snapshot);

static GdkTexture *
paintable_to_texture (GdkPaintable *paintable,
                      double        scale)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GskRenderer *renderer;
  int width, height;
  GdkTexture *texture;
  graphene_rect_t bounds;

  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width * scale, height * scale);
  node = gtk_snapshot_free_to_node (snapshot);

  /* FIXME: use a proper renderer, and cache it */
  renderer = gsk_cairo_renderer_new ();
  gsk_renderer_realize_for_display (renderer, gdk_display_get_default (), NULL);

  gsk_render_node_get_bounds (node, &bounds);
  texture = gsk_renderer_render_texture (renderer, node, &bounds);

  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);

  gsk_render_node_unref (node);

  return texture;
}

GdkTexture *
gdk_cursor_create_texture (GdkCursor *cursor,
                           double     scale)
{
  if (cursor->texture)
    return g_object_ref (cursor->texture);
  else if (cursor->paintable)
    return paintable_to_texture (cursor->paintable, scale);
  else
    return NULL;
}

/**
 * GdkCursorGetTestureCallback:
 * @cursor: the `GdkCursor`
 * @cursor_size: the nominal cursor size, in application pixels
 * @scale: the device scale
 * @width: (out): return location for the actual cursor width,
 *   in application pixels
 * @height: (out): return location for the actual cursor height,
 *   in application pixels
 * @hotspot_x: (out): return location for the hotspot X position,
 *   in application pixels
 * @hotspot_y: (out): return location for the hotspot Y position,
 *   in application pixels
 * @data: User data for the callback
 *
 * The type of callback used by a dynamic `GdkCursor` to generate
 * a texture for the cursor image at the given @cursor_size
 * and @scale.
 *
 * The actual cursor size in application pixels may be different
 * from @cursor_size x @cursor_size, and will be returned in
 * @width, @height.
 *
 * The returned texture should have a size that corresponds to
 * the actual cursor size, in device pixels.
 *
 * This function may fail and return `NULL`, in which case
 * the backend should use the fallback cursor.
 *
 * Returns: (nullable) (transfer full): the cursor image, or
 *   `NULL` if none could be produced.
 */

/**
 * gdk_cursor_new_dynamic:
 * @callback: the `GdkCursorGetTextureCallback`
 * @data: data to pass to @callback
 * @destroy: destroy notify for @data
 * @fallback: (nullable): the `GdkCursor` to fall back to when
 *   this one cannot be supported
 *
 * Creates a new dynamic cursor object.
 *
 * Dynamic cursors produce textures for the cursor image
 * on demand, when the @callback is called.
 *
 * Returns: (nullable): a new `GdkCursor`
 *
 * Since: 4.16
 */
GdkCursor *
gdk_cursor_new_dynamic (GdkCursorGetTextureCallback  callback,
                        gpointer                     data,
                        GDestroyNotify               destroy,
                        GdkCursor                   *fallback)
{
  GdkCursor *cursor;

  g_return_val_if_fail (callback != NULL, NULL);
  g_return_val_if_fail (fallback == NULL || GDK_IS_CURSOR (fallback), NULL);

  cursor = g_object_new (GDK_TYPE_CURSOR,
                         "fallback", fallback,
                         NULL);

  cursor->callback = callback;
  cursor->data = data;
  cursor->destroy = destroy;

  return cursor;
}

/**
 * gdk_cursor_invalidate:
 * @cursor: a `GdkCursor`
 *
 * Emits the invalidate signal to indicate that the cursor
 * has changed and backends should recreate their cursor
 * image.
 *
 * This function does nothing unless the cursor has been
 * created with [constructor@Gdk.Cursor.new_dynamic].
 *
 * Since: 4.16
 */
void
gdk_cursor_invalidate (GdkCursor *cursor)
{
  if (cursor->callback == NULL)
    return;

  g_signal_emit (cursor, invalidate_signal, 0);
}

/*< private >
 * gdk_cursor_get_dynamic_texture:
 * @cursor: a dynamic `GdkCursor`
 * @cursor_size: the nominal size of the cursor image
 *   to produce, in application pixels
 * @scale: the device scale to use
 * @width: return location for the cursor width,
 *   in application pixels
 * @height : return location for the cursor height,
 *   in application pixels
 * @hotspot_x: return location for the hotspot X position,
 *   in application pixels
 * @hotspot_y: return location for the hotspot X position,
 *   in application pixels
 *
 * Call the callback of a dynamic cursor to generate
 * a texture for the cursor image at the given @cursor_size
 * and @scale.
 *
 * The actual cursor size in application pixels may be different
 * from @cursor_size x @cursor_size, and will be returned in
 * @width, @height.
 *
 * The returned texture should have a size that corresponds to
 * the actual cursor size, in device pixels.
 *
 * This function may fail and return `NULL`, in which case
 * the backend should use the fallback cursor.
 *
 * Returns: (nullable) (transfer full): the cursor image, or
 *   `NULL` if none could be produced.
 */
GdkTexture *
gdk_cursor_get_dynamic_texture (GdkCursor *cursor,
                                int        cursor_size,
                                double     scale,
                                int       *width,
                                int       *height,
                                int       *hotspot_x,
                                int       *hotspot_y)
{
  if (cursor->callback == NULL)
    return NULL;

  return cursor->callback (cursor,
                           cursor_size, scale,
                           width, height,
                           hotspot_x, hotspot_y,
                           cursor->data);
}

