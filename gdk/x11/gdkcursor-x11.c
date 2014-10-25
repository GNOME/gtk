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

/* needs to be first because any header might include gdk-pixbuf.h otherwise */
#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkcursor.h"
#include "gdkcursorprivate.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#include <string.h>
#include <errno.h>
#include <math.h>

struct _GdkX11Cursor
{
  GdkCursor cursor;

  Cursor xcursor;
  gchar *name;
  guint serial;
};

struct _GdkX11CursorClass
{
  GdkCursorClass cursor_class;
};

static guint theme_serial = 0;

/* cursor_cache holds a cache of non-pixmap cursors to avoid expensive 
 * libXcursor searches, cursors are added to it but only removed when
 * their display is closed. We make the assumption that since there are 
 * a small number of display’s and a small number of cursor’s that this 
 * list will stay small enough not to be a problem.
 */
static GSList* cursor_cache = NULL;

struct cursor_cache_key
{
  GdkDisplay* display;
  GdkCursorType type;
  const char* name;
};

/* Caller should check if there is already a match first.
 * Cursor MUST be either a typed cursor or a pixmap with 
 * a non-NULL name.
 */
static void
add_to_cache (GdkX11Cursor* cursor)
{
  cursor_cache = g_slist_prepend (cursor_cache, cursor);

  /* Take a ref so that if the caller frees it we still have it */
  g_object_ref (cursor);
}

/* Returns 0 on a match
 */
static gint
cache_compare_func (gconstpointer listelem, 
                    gconstpointer target)
{
  GdkX11Cursor* cursor = (GdkX11Cursor*)listelem;
  struct cursor_cache_key* key = (struct cursor_cache_key*)target;

  if ((cursor->cursor.type != key->type) ||
      (gdk_cursor_get_display (GDK_CURSOR (cursor)) != key->display))
    return 1; /* No match */
  
  /* Elements marked as pixmap must be named cursors 
   * (since we don't store normal pixmap cursors 
   */
  if (key->type == GDK_CURSOR_IS_PIXMAP)
    return strcmp (key->name, cursor->name);

  return 0; /* Match */
}

/* Returns the cursor if there is a match, NULL if not
 * For named cursors type shall be GDK_CURSOR_IS_PIXMAP
 * For unnamed, typed cursors, name shall be NULL
 */
static GdkX11Cursor*
find_in_cache (GdkDisplay    *display, 
               GdkCursorType  type,
               const char    *name)
{
  GSList* res;
  struct cursor_cache_key key;

  key.display = display;
  key.type = type;
  key.name = name;

  res = g_slist_find_custom (cursor_cache, &key, cache_compare_func);

  if (res)
    return (GdkX11Cursor *) res->data;

  return NULL;
}

/* Called by gdk_x11_display_finalize to flush any cached cursors
 * for a dead display.
 */
void
_gdk_x11_cursor_display_finalize (GdkDisplay *display)
{
  GSList* item;
  GSList** itemp; /* Pointer to the thing to fix when we delete an item */
  item = cursor_cache;
  itemp = &cursor_cache;
  while (item)
    {
      GdkX11Cursor* cursor = (GdkX11Cursor*)(item->data);
      if (gdk_cursor_get_display (GDK_CURSOR (cursor)) == display)
        {
          GSList* olditem;
          g_object_unref ((GdkCursor*) cursor);
          /* Remove this item from the list */
          *(itemp) = item->next;
          olditem = item;
          item = g_slist_next (item);
          g_slist_free_1 (olditem);
        } 
      else 
        {
          itemp = &(item->next);
          item = g_slist_next (item);
        }
    }
}

/*** GdkX11Cursor ***/

G_DEFINE_TYPE (GdkX11Cursor, gdk_x11_cursor, GDK_TYPE_CURSOR)

static cairo_surface_t *gdk_x11_cursor_get_surface (GdkCursor *cursor,
						    gdouble   *x_hot,
						    gdouble   *y_hot);

static void
gdk_x11_cursor_finalize (GObject *object)
{
  GdkX11Cursor *private = GDK_X11_CURSOR (object);
  GdkDisplay *display;

  display = gdk_cursor_get_display (GDK_CURSOR (object));
  if (private->xcursor && !gdk_display_is_closed (display))
    XFreeCursor (GDK_DISPLAY_XDISPLAY (display), private->xcursor);

  g_free (private->name);

  G_OBJECT_CLASS (gdk_x11_cursor_parent_class)->finalize (object);
}

static void
gdk_x11_cursor_class_init (GdkX11CursorClass *xcursor_class)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (xcursor_class);
  GObjectClass *object_class = G_OBJECT_CLASS (xcursor_class);

  object_class->finalize = gdk_x11_cursor_finalize;

  cursor_class->get_surface = gdk_x11_cursor_get_surface;
}

static void
gdk_x11_cursor_init (GdkX11Cursor *cursor)
{
}

static Cursor
get_blank_cursor (GdkDisplay *display)
{
  GdkScreen *screen;
  Pixmap pixmap;
  XColor color;
  Cursor cursor;
  cairo_surface_t *surface;
  cairo_t *cr;

  screen = gdk_display_get_default_screen (display);
  surface = _gdk_x11_window_create_bitmap_surface (gdk_screen_get_root_window (screen), 1, 1);
  /* Clear surface */
  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_destroy (cr);
 
  pixmap = cairo_xlib_surface_get_drawable (surface);

  color.pixel = 0; 
  color.red = color.blue = color.green = 0;

  if (gdk_display_is_closed (display))
    cursor = None;
  else
    cursor = XCreatePixmapCursor (GDK_DISPLAY_XDISPLAY (display),
                                  pixmap, pixmap,
                                  &color, &color, 1, 1);
  cairo_surface_destroy (surface);

  return cursor;
}

GdkCursor*
_gdk_x11_display_get_cursor_for_type (GdkDisplay    *display,
                                      GdkCursorType  cursor_type)
{
  GdkX11Cursor *private;
  Cursor xcursor;

  if (gdk_display_is_closed (display))
    {
      xcursor = None;
    }
  else
    {
      private = find_in_cache (display, cursor_type, NULL);

      if (private)
        {
          /* Cache had it, add a ref for this user */
          g_object_ref (private);

          return (GdkCursor*) private;
        }
      else
        {
          if (cursor_type != GDK_BLANK_CURSOR)
            xcursor = XCreateFontCursor (GDK_DISPLAY_XDISPLAY (display),
                                         cursor_type);
          else
            xcursor = get_blank_cursor (display);
       }
    }

  private = g_object_new (GDK_TYPE_X11_CURSOR,
                          "cursor-type", cursor_type,
                          "display", display,
                          NULL);
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  if (xcursor != None)
    add_to_cache (private);

  return GDK_CURSOR (private);
}

/**
 * gdk_x11_cursor_get_xdisplay:
 * @cursor: (type GdkX11Cursor): a #GdkCursor.
 * 
 * Returns the display of a #GdkCursor.
 * 
 * Returns: (transfer none): an Xlib Display*.
 **/
Display *
gdk_x11_cursor_get_xdisplay (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return GDK_DISPLAY_XDISPLAY (gdk_cursor_get_display (cursor));
}

/**
 * gdk_x11_cursor_get_xcursor:
 * @cursor: (type GdkX11Cursor): a #GdkCursor.
 * 
 * Returns the X cursor belonging to a #GdkCursor.
 * 
 * Returns: an Xlib Cursor.
 **/
Cursor
gdk_x11_cursor_get_xcursor (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, None);

  return ((GdkX11Cursor *)cursor)->xcursor;
}

#if defined(HAVE_XCURSOR) && defined(HAVE_XFIXES) && XFIXES_MAJOR >= 2

static cairo_surface_t *  
gdk_x11_cursor_get_surface (GdkCursor *cursor,
			    gdouble   *x_hot,
			    gdouble   *y_hot)
{
  GdkDisplay *display;
  Display *xdisplay;
  GdkX11Cursor *private;
  XcursorImages *images = NULL;
  XcursorImage *image;
  gint size;
  cairo_surface_t *surface;
  gint scale;
  gchar *theme;
  
  private = GDK_X11_CURSOR (cursor);

  display = gdk_cursor_get_display (cursor);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  size = XcursorGetDefaultSize (xdisplay);
  theme = XcursorGetTheme (xdisplay);

  if (cursor->type == GDK_CURSOR_IS_PIXMAP)
    {
      if (private->name)
        images = XcursorLibraryLoadImages (private->name, theme, size);
    }
  else
    images = XcursorShapeLoadImages (cursor->type, theme, size);

  if (!images)
    return NULL;

  image = images->images[0];

  /* Assume the currently set cursor was defined for the screen
     scale */
  scale =
    gdk_screen_get_monitor_scale_factor (gdk_display_get_default_screen (display), 0);

  surface = gdk_window_create_similar_image_surface (NULL,
						     CAIRO_FORMAT_ARGB32,
						     image->width,
						     image->height,
						     scale);

  memcpy (cairo_image_surface_get_data (surface),
	  image->pixels, 4 * image->width * image->height);

  cairo_surface_mark_dirty (surface);

  if (x_hot)
    *x_hot = (double)image->xhot / scale;
  if (y_hot)
    *y_hot = (double)image->yhot / scale;

  XcursorImagesDestroy (images);

  return surface;
}

void
_gdk_x11_cursor_update_theme (GdkCursor *cursor)
{
  Display *xdisplay;
  GdkX11Cursor *private;
  Cursor new_cursor = None;
  GdkX11Display *display_x11;

  private = (GdkX11Cursor *) cursor;
  display_x11 = GDK_X11_DISPLAY (gdk_cursor_get_display (cursor));
  xdisplay = GDK_DISPLAY_XDISPLAY (display_x11);

  if (!display_x11->have_xfixes)
    return;

  if (private->serial == theme_serial)
    return;

  private->serial = theme_serial;

  if (private->xcursor != None)
    {
      if (cursor->type == GDK_BLANK_CURSOR)
        return;

      if (cursor->type == GDK_CURSOR_IS_PIXMAP)
        {
          if (private->name)
            new_cursor = XcursorLibraryLoadCursor (xdisplay, private->name);
        }
      else 
        new_cursor = XcursorShapeLoadCursor (xdisplay, cursor->type);
      
      if (new_cursor != None)
        {
          XFixesChangeCursor (xdisplay, new_cursor, private->xcursor);
          private->xcursor = new_cursor;
        }
    }
}

static void
update_cursor (gpointer data,
               gpointer user_data)
{
  GdkCursor *cursor;

  cursor = (GdkCursor*)(data);

  if (!cursor)
    return;

  _gdk_x11_cursor_update_theme (cursor);
}

/**
 * gdk_x11_display_set_cursor_theme:
 * @display: (type GdkX11Display): a #GdkDisplay
 * @theme: the name of the cursor theme to use, or %NULL to unset
 *         a previously set value
 * @size: the cursor size to use, or 0 to keep the previous size
 *
 * Sets the cursor theme from which the images for cursor
 * should be taken.
 *
 * If the windowing system supports it, existing cursors created
 * with gdk_cursor_new(), gdk_cursor_new_for_display() and
 * gdk_cursor_new_from_name() are updated to reflect the theme
 * change. Custom cursors constructed with
 * gdk_cursor_new_from_pixbuf() will have to be handled
 * by the application (GTK+ applications can learn about
 * cursor theme changes by listening for change notification
 * for the corresponding #GtkSetting).
 *
 * Since: 2.8
 */
void
gdk_x11_display_set_cursor_theme (GdkDisplay  *display,
                                  const gchar *theme,
                                  const gint   size)
{
  Display *xdisplay;
  gchar *old_theme;
  gint old_size;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  old_theme = XcursorGetTheme (xdisplay);
  old_size = XcursorGetDefaultSize (xdisplay);

  if (old_size == size &&
      (old_theme == theme ||
       (old_theme && theme && strcmp (old_theme, theme) == 0)))
    return;

  theme_serial++;

  XcursorSetTheme (xdisplay, theme);
  if (size > 0)
    XcursorSetDefaultSize (xdisplay, size);

  g_slist_foreach (cursor_cache, update_cursor, NULL);
}

#else

static cairo_surface_t *
gdk_x11_cursor_get_surface (GdkCursor *cursor,
			    gdouble *x_hot,
			    gdouble *y_hot)
{
  return NULL;
}

void
gdk_x11_display_set_cursor_theme (GdkDisplay  *display,
                                  const gchar *theme,
                                  const gint   size)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

void
_gdk_x11_cursor_update_theme (GdkCursor *cursor)
{
  g_return_if_fail (cursor != NULL);
}

#endif

#ifdef HAVE_XCURSOR

static void
get_surface_size (cairo_surface_t *surface,
		  int *width,
		  int *height)
{
  double x_scale, y_scale;

  x_scale = y_scale = 1;

  cairo_surface_get_device_scale (surface, &x_scale, &y_scale);

  /* Assume any set scaling is icon scale */
  *width =
    ceil (cairo_image_surface_get_width (surface) / x_scale);
  *height =
    ceil (cairo_image_surface_get_height (surface) / y_scale);
}

static XcursorImage*
create_cursor_image (cairo_surface_t *source_surface,
                     gint       x,
                     gint       y,
		     gint       scale)
{
  gint width, height;
  XcursorImage *xcimage;
  cairo_surface_t *surface;
  cairo_t *cr;

  get_surface_size (source_surface, &width, &height);

  width *= scale;
  height *= scale;
  
  xcimage = XcursorImageCreate (width, height);

  xcimage->xhot = x * scale;
  xcimage->yhot = y * scale;

  surface = cairo_image_surface_create_for_data ((guchar *) xcimage->pixels,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width,
                                                 height,
                                                 width * 4);

  cairo_surface_set_device_scale (surface, scale, scale);

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface (cr, source_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return xcimage;
}

GdkCursor *
_gdk_x11_display_get_cursor_for_surface (GdkDisplay *display,
					 cairo_surface_t *surface,
					 gdouble     x,
					 gdouble     y)
{
  XcursorImage *xcimage;
  Cursor xcursor;
  GdkX11Cursor *private;
  int target_scale;

  if (gdk_display_is_closed (display))
    {
      xcursor = None;
    }
  else
    {
      target_scale =
	gdk_screen_get_monitor_scale_factor (gdk_display_get_default_screen (display), 0);
      xcimage = create_cursor_image (surface, x, y, target_scale);
      xcursor = XcursorImageLoadCursor (GDK_DISPLAY_XDISPLAY (display), xcimage);
      XcursorImageDestroy (xcimage);
    }

  private = g_object_new (GDK_TYPE_X11_CURSOR, 
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  return GDK_CURSOR (private);
}

GdkCursor*
_gdk_x11_display_get_cursor_for_name (GdkDisplay  *display,
                                      const gchar *name)
{
  Cursor xcursor;
  Display *xdisplay;
  GdkX11Cursor *private;

  if (gdk_display_is_closed (display))
    {
      xcursor = None;
    }
  else
    {
      private = find_in_cache (display, GDK_CURSOR_IS_PIXMAP, name);

      if (private)
        {
          /* Cache had it, add a ref for this user */
          g_object_ref (private);

          return (GdkCursor*) private;
        }

      xdisplay = GDK_DISPLAY_XDISPLAY (display);
      xcursor = XcursorLibraryLoadCursor (xdisplay, name);
      if (xcursor == None)
        return NULL;
    }

  private = g_object_new (GDK_TYPE_X11_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  private->xcursor = xcursor;
  private->name = g_strdup (name);
  private->serial = theme_serial;

  add_to_cache (private);

  return GDK_CURSOR (private);
}

gboolean
_gdk_x11_display_supports_cursor_alpha (GdkDisplay *display)
{
  return XcursorSupportsARGB (GDK_DISPLAY_XDISPLAY (display));
}

gboolean
_gdk_x11_display_supports_cursor_color (GdkDisplay *display)
{
  return XcursorSupportsARGB (GDK_DISPLAY_XDISPLAY (display));
}

void
_gdk_x11_display_get_default_cursor_size (GdkDisplay *display,
                                          guint      *width,
                                          guint      *height)
{
  *width = *height = XcursorGetDefaultSize (GDK_DISPLAY_XDISPLAY (display));
}

#else

static GdkCursor*
gdk_cursor_new_from_pixmap (GdkDisplay     *display,
                            Pixmap          source_pixmap,
                            Pixmap          mask_pixmap,
                            const GdkRGBA  *fg,
                            const GdkRGBA  *bg,
                            gint            x,
                            gint            y)
{
  GdkX11Cursor *private;
  Cursor xcursor;
  XColor xfg, xbg;

  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  xfg.red = fg->red * 65535;
  xfg.blue = fg->blue * 65535;
  xfg.green = fg->green * 65535;

  xbg.red = bg->red * 65535;
  xbg.blue = bg->blue * 65535;
  xbg.green = bg->green * 65535;

  if (gdk_display_is_closed (display))
    xcursor = None;
  else
    xcursor = XCreatePixmapCursor (GDK_DISPLAY_XDISPLAY (display),
                                   source_pixmap, mask_pixmap, &xfg, &xbg, x, y);
  private = g_object_new (GDK_TYPE_X11_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  return GDK_CURSOR (private);
}

GdkCursor *
_gdk_x11_display_get_cursor_for_surface (GdkDisplay *display,
					 cairo_surface_t *surface,
					 gdouble     x,
					 gdouble     y)
{
  GdkCursor *cursor;
  cairo_surface_t *pixmap, *mask;
  guint width, height, n_channels, rowstride, data_stride, i, j;
  guint8 *data, *mask_data, *pixels;
  GdkRGBA fg = { 0, 0, 0, 1 };
  GdkRGBA bg = { 1, 1, 1, 1 };
  GdkScreen *screen;
  cairo_surface_t *image;
  cairo_t *cr;
  GdkPixbuf *pixbuf;

  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);

  g_return_val_if_fail (0 <= x && x < width, NULL);
  g_return_val_if_fail (0 <= y && y < height, NULL);

  /* Note: This does not support scaled surfaced, if you need that you
     want XCursor anyway */
  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
  
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  data_stride = 4 * ((width + 31) / 32);
  data = g_new0 (guint8, data_stride * height);
  mask_data = g_new0 (guint8, data_stride * height);

  for (j = 0; j < height; j++)
    {
      guint8 *src = pixels + j * rowstride;
      guint8 *d = data + data_stride * j;
      guint8 *md = mask_data + data_stride * j;

      for (i = 0; i < width; i++)
        {
          if (src[1] < 0x80)
            *d |= 1 << (i % 8);

          if (n_channels == 3 || src[3] >= 0x80)
            *md |= 1 << (i % 8);

          src += n_channels;
          if (i % 8 == 7)
            {
              d++;
              md++;
            }
        }
    }

  g_object_unref (pixbuf);

  screen = gdk_display_get_default_screen (display);

  pixmap = _gdk_x11_window_create_bitmap_surface (gdk_screen_get_root_window (screen),
                                                  width, height);
  cr = cairo_create (pixmap);
  image = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_A1,
                                               width, height, data_stride);
  cairo_set_source_surface (cr, image, 0, 0);
  cairo_surface_destroy (image);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_destroy (cr);

  mask = _gdk_x11_window_create_bitmap_surface (gdk_screen_get_root_window (screen),
                                                width, height);
  cr = cairo_create (mask);
  image = cairo_image_surface_create_for_data (mask_data, CAIRO_FORMAT_A1,
                                               width, height, data_stride);
  cairo_set_source_surface (cr, image, 0, 0);
  cairo_surface_destroy (image);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_destroy (cr);

  cursor = gdk_cursor_new_from_pixmap (display,
                                       cairo_xlib_surface_get_drawable (pixmap),
                                       cairo_xlib_surface_get_drawable (mask),
                                       &fg, &bg,
                                       x, y);

  cairo_surface_destroy (pixmap);
  cairo_surface_destroy (mask);
  
  g_free (data);
  g_free (mask_data);

  return cursor;
}

GdkCursor*
_gdk_x11_display_get_cursor_for_name (GdkDisplay  *display,
                                      const gchar *name)
{
  return NULL;
}

gboolean
_gdk_x11_display_supports_cursor_alpha (GdkDisplay *display)
{
  return FALSE;
}

gboolean
_gdk_x11_display_supports_cursor_color (GdkDisplay *display)
{
  return FALSE;
}

void
_gdk_x11_display_get_default_cursor_size (GdkDisplay *display,
                                          guint      *width,
                                          guint      *height)
{
  /* no idea, really */
  *width = *height = 20;
  return;
}

#endif

void
_gdk_x11_display_get_maximal_cursor_size (GdkDisplay *display,
                                          guint       *width,
                                          guint       *height)
{
  GdkScreen *screen;
  GdkWindow *window;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  screen = gdk_display_get_default_screen (display);
  window = gdk_screen_get_root_window (screen);
  XQueryBestCursor (GDK_DISPLAY_XDISPLAY (display),
                    GDK_WINDOW_XID (window),
                    128, 128, width, height);
}
