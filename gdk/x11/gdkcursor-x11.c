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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkx.h"

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


static guint theme_serial = 0;

/* cursor_cache holds a cache of non-pixmap cursors to avoid expensive 
 * libXcursor searches, cursors are added to it but only removed when
 * their display is closed. We make the assumption that since there are 
 * a small number of display's and a small number of cursor's that this 
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
add_to_cache (GdkCursorPrivate* cursor)
{
  cursor_cache = g_slist_prepend (cursor_cache, cursor);

  /* Take a ref so that if the caller frees it we still have it */
  gdk_cursor_ref ((GdkCursor*) cursor);
}

/* Returns 0 on a match
 */
static gint
cache_compare_func (gconstpointer listelem, 
                    gconstpointer target)
{
  GdkCursorPrivate* cursor = (GdkCursorPrivate*)listelem;
  struct cursor_cache_key* key = (struct cursor_cache_key*)target;

  if ((cursor->cursor.type != key->type) ||
      (cursor->display != key->display))
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
static GdkCursorPrivate*
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
    return (GdkCursorPrivate *) res->data;

  return NULL;
}

/* Called by gdk_display_x11_finalize to flush any cached cursors
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
      GdkCursorPrivate* cursor = (GdkCursorPrivate*)(item->data);
      if (cursor->display == display)
        {
          GSList* olditem;
          gdk_cursor_unref ((GdkCursor*) cursor);
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
  GdkCursorPrivate *private;
  GdkCursor *cursor;
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
          gdk_cursor_ref ((GdkCursor*) private);

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

  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  cursor = (GdkCursor *) private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;
  
  if (xcursor != None)
    add_to_cache (private);

  return cursor;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);
  g_return_if_fail (cursor->ref_count == 0);

  private = (GdkCursorPrivate *) cursor;
  if (private->xcursor && !gdk_display_is_closed (private->display))
    XFreeCursor (GDK_DISPLAY_XDISPLAY (private->display), private->xcursor);

  g_free (private->name);
  g_free (private);
}

/**
 * gdk_x11_cursor_get_xdisplay:
 * @cursor: a #GdkCursor.
 * 
 * Returns the display of a #GdkCursor.
 * 
 * Return value: an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_cursor_get_xdisplay (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return GDK_DISPLAY_XDISPLAY(((GdkCursorPrivate *)cursor)->display);
}

/**
 * gdk_x11_cursor_get_xcursor:
 * @cursor: a #GdkCursor.
 * 
 * Returns the X cursor belonging to a #GdkCursor.
 * 
 * Return value: an Xlib <type>Cursor</type>.
 **/
Cursor
gdk_x11_cursor_get_xcursor (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, None);

  return ((GdkCursorPrivate *)cursor)->xcursor;
}

/** 
 * gdk_cursor_get_display:
 * @cursor: a #GdkCursor.
 *
 * Returns the display on which the #GdkCursor is defined.
 *
 * Returns: (transfer none): the #GdkDisplay associated to @cursor
 *
 * Since: 2.2
 */

GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return ((GdkCursorPrivate *)cursor)->display;
}

#if defined(HAVE_XCURSOR) && defined(HAVE_XFIXES) && XFIXES_MAJOR >= 2

/**
 * gdk_cursor_get_image:
 * @cursor: a #GdkCursor
 *
 * Returns a #GdkPixbuf with the image used to display the cursor.
 *
 * Note that depending on the capabilities of the windowing system and 
 * on the cursor, GDK may not be able to obtain the image data. In this 
 * case, %NULL is returned.
 *
 * Returns: (transfer full): a #GdkPixbuf representing @cursor, or %NULL
 *
 * Since: 2.8
 */
GdkPixbuf*  
gdk_cursor_get_image (GdkCursor *cursor)
{
  Display *xdisplay;
  GdkCursorPrivate *private;
  XcursorImages *images = NULL;
  XcursorImage *image;
  gint size;
  gchar buf[32];
  guchar *data, *p, tmp;
  GdkPixbuf *pixbuf;
  gchar *theme;
  
  g_return_val_if_fail (cursor != NULL, NULL);

  private = (GdkCursorPrivate *) cursor;
    
  xdisplay = GDK_DISPLAY_XDISPLAY (private->display);

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

  data = g_malloc (4 * image->width * image->height);
  memcpy (data, image->pixels, 4 * image->width * image->height);

  for (p = data; p < data + (4 * image->width * image->height); p += 4)
    {
      tmp = p[0];
      p[0] = p[2];
      p[2] = tmp;
    }

  pixbuf = gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB, TRUE,
                                     8, image->width, image->height,
                                     4 * image->width,
                                     (GdkPixbufDestroyNotify)g_free, NULL);

  if (private->name)
    gdk_pixbuf_set_option (pixbuf, "name", private->name);
  g_snprintf (buf, 32, "%d", image->xhot);
  gdk_pixbuf_set_option (pixbuf, "x_hot", buf);
  g_snprintf (buf, 32, "%d", image->yhot);
  gdk_pixbuf_set_option (pixbuf, "y_hot", buf);

  XcursorImagesDestroy (images);

  return pixbuf;
}

void
_gdk_x11_cursor_update_theme (GdkCursor *cursor)
{
  Display *xdisplay;
  GdkCursorPrivate *private;
  Cursor new_cursor = None;
  GdkDisplayX11 *display_x11;

  private = (GdkCursorPrivate *) cursor;
  xdisplay = GDK_DISPLAY_XDISPLAY (private->display);
  display_x11 = GDK_DISPLAY_X11 (private->display);

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
 * @display: a #GdkDisplay
 * @theme: the name of the cursor theme to use, or %NULL to unset
 *         a previously set value 
 * @size: the cursor size to use, or 0 to keep the previous size
 *
 * Sets the cursor theme from which the images for cursor
 * should be taken. 
 * 
 * If the windowing system supports it, existing cursors created 
 * with gdk_cursor_new(), gdk_cursor_new_for_display() and 
 * gdk_cursor_new_for_name() are updated to reflect the theme 
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
  GdkDisplayX11 *display_x11;
  Display *xdisplay;
  gchar *old_theme;
  gint old_size;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_x11 = GDK_DISPLAY_X11 (display);
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

GdkPixbuf*  
gdk_cursor_get_image (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);
  
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

static XcursorImage*
create_cursor_image (GdkPixbuf *pixbuf,
                     gint       x,
                     gint       y)
{
  guint width, height;
  XcursorImage *xcimage;
  cairo_surface_t *surface;
  cairo_t *cr;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  xcimage = XcursorImageCreate (width, height);

  xcimage->xhot = x;
  xcimage->yhot = y;

  surface = cairo_image_surface_create_for_data ((guchar *) xcimage->pixels,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width,
                                                 height,
                                                 width * 4);

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_destroy (surface);

  return xcimage;
}

GdkCursor *
_gdk_x11_display_get_cursor_for_pixbuf (GdkDisplay *display,
                                        GdkPixbuf  *pixbuf,
                                        gint        x,
                                        gint        y)
{
  XcursorImage *xcimage;
  Cursor xcursor;
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  const char *option;
  char *end;
  gint64 value;

  if (x == -1 && (option = gdk_pixbuf_get_option (pixbuf, "x_hot")))
    {
      errno = 0;
      end = NULL;
      value = g_ascii_strtoll (option, &end, 10);
      if (errno == 0 &&
          end != option &&
          value >= 0 && value < G_MAXINT)
        x = (gint) value;
    }
  if (y == -1 && (option = gdk_pixbuf_get_option (pixbuf, "y_hot")))
    {
      errno = 0;
      end = NULL;
      value = g_ascii_strtoll (option, &end, 10);
      if (errno == 0 &&
          end != option &&
          value >= 0 && value < G_MAXINT)
        y = (gint) value;
    }

  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  if (gdk_display_is_closed (display))
    {
      xcursor = None;
    }
  else
    {
      xcimage = create_cursor_image (pixbuf, x, y);
      xcursor = XcursorImageLoadCursor (GDK_DISPLAY_XDISPLAY (display), xcimage);
      XcursorImageDestroy (xcimage);
    }

  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;

  return cursor;
}

GdkCursor*
_gdk_x11_display_get_cursor_for_name (GdkDisplay  *display,
                                      const gchar *name)
{
  Cursor xcursor;
  Display *xdisplay;
  GdkCursorPrivate *private;
  GdkCursor *cursor;

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
          gdk_cursor_ref ((GdkCursor*) private);

          return (GdkCursor*) private;
        }

      xdisplay = GDK_DISPLAY_XDISPLAY (display);
      xcursor = XcursorLibraryLoadCursor (xdisplay, name);
      if (xcursor == None)
        return NULL;
    }

  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  private->name = g_strdup (name);
  private->serial = theme_serial;

  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  add_to_cache (private);

  return cursor;
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
                            const GdkColor *fg,
                            const GdkColor *bg,
                            gint            x,
                            gint            y)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  Cursor xcursor;
  XColor xfg, xbg;

  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  xfg.pixel = fg->pixel;
  xfg.red = fg->red;
  xfg.blue = fg->blue;
  xfg.green = fg->green;
  xbg.pixel = bg->pixel;
  xbg.red = bg->red;
  xbg.blue = bg->blue;
  xbg.green = bg->green;
  
  if (gdk_display_is_closed (display->closed))
    xcursor = None;
  else
    xcursor = XCreatePixmapCursor (GDK_DISPLAY_XDISPLAY (display),
                                   source_pixmap, mask_pixmap, &xfg, &xbg, x, y);
  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;

  return cursor;
}

GdkCursor *
_gdk_x11_display_get_cursor_for_pixbuf (GdkDisplay *display,
                                        GdkPixbuf  *pixbuf,
                                        gint        x,
                                        gint        y)
{
  GdkCursor *cursor;
  cairo_surface_t *pixmap, *mask;
  guint width, height, n_channels, rowstride, data_stride, i, j;
  guint8 *data, *mask_data, *pixels;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0xffff, 0xffff, 0xffff };
  GdkScreen *screen;
  cairo_surface_t *image;
  cairo_t *cr;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  g_return_val_if_fail (0 <= x && x < width, NULL);
  g_return_val_if_fail (0 <= y && y < height, NULL);

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
_gdk_x11_display_get_default_cursor_size (GdkDisplay *display)
{
  /* no idea, really */
  return 20;
}

#endif


/**
 * gdk_display_get_maximal_cursor_size:
 * @display: a #GdkDisplay
 * @width: (out): the return location for the maximal cursor width
 * @height: (out): the return location for the maximal cursor height
 *
 * Gets the maximal size to use for cursors on @display.
 *
 * Since: 2.4
 */
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
