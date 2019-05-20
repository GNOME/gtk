#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdkx.h>
#include <cairo-xlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/extensions/shape.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#include "widgets.h"
#include "shadow.h"

#define MAXIMUM_WM_REPARENTING_DEPTH 4
#ifndef _
#define _(x) (x)
#endif

static void queue_show (void);

static Window
find_toplevel_window (Window xid)
{
  Window root, parent, *children;
  guint nchildren;

  do
    {
      if (XQueryTree (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xid, &root,
		      &parent, &children, &nchildren) == 0)
	{
	  g_warning ("Couldn't find window manager window");
	  return 0;
	}

      if (root == parent)
	return xid;

      xid = parent;
    }
  while (TRUE);
}

static GdkPixbuf *
add_border_to_shot (GdkPixbuf *pixbuf)
{
  GdkPixbuf *retval;
  GdkColorspace colorspace;
  int bits;

  colorspace = gdk_pixbuf_get_colorspace (pixbuf);
  bits = gdk_pixbuf_get_bits_per_sample (pixbuf);
  retval = gdk_pixbuf_new (colorspace, TRUE, bits,
			   gdk_pixbuf_get_width (pixbuf) + 2,
			   gdk_pixbuf_get_height (pixbuf) + 2);

  /* Fill with solid black */
  gdk_pixbuf_fill (retval, 0xFF);
  gdk_pixbuf_copy_area (pixbuf,
			0, 0,
			gdk_pixbuf_get_width (pixbuf),
			gdk_pixbuf_get_height (pixbuf),
			retval, 1, 1);

  return retval;
}

static GdkPixbuf *
remove_shaped_area (GdkPixbuf *pixbuf,
		    Window     window)
{
  GdkPixbuf *retval;
  XRectangle *rectangles;
  int rectangle_count, rectangle_order;
  int i;
  GdkColorspace colorspace;
  int bits;

  colorspace = gdk_pixbuf_get_colorspace (pixbuf);
  bits = gdk_pixbuf_get_bits_per_sample (pixbuf);
  retval = gdk_pixbuf_new (colorspace, TRUE, bits,
			   gdk_pixbuf_get_width (pixbuf),
			   gdk_pixbuf_get_height (pixbuf));
  
  gdk_pixbuf_fill (retval, 0);
  rectangles = XShapeGetRectangles (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), window,
				    ShapeBounding, &rectangle_count, &rectangle_order);

  for (i = 0; i < rectangle_count; i++)
    {
      int y, x;

      for (y = rectangles[i].y; y < rectangles[i].y + rectangles[i].height; y++)
	{
	  guchar *src_pixels, *dest_pixels;

	  src_pixels = gdk_pixbuf_get_pixels (pixbuf) +
	    y * gdk_pixbuf_get_rowstride (pixbuf) +
	    rectangles[i].x * (gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3);
	  dest_pixels = gdk_pixbuf_get_pixels (retval) +
	    y * gdk_pixbuf_get_rowstride (retval) +
	    rectangles[i].x * 4;

	  for (x = rectangles[i].x; x < rectangles[i].x + rectangles[i].width; x++)
	    {
	      *dest_pixels++ = *src_pixels ++;
	      *dest_pixels++ = *src_pixels ++;
	      *dest_pixels++ = *src_pixels ++;
	      *dest_pixels++ = 255;

	      if (gdk_pixbuf_get_has_alpha (pixbuf))
		src_pixels++;
	    }
	}
    }

  return retval;
}

typedef enum {
  DECOR_NONE,
  DECOR_FRAME,
  DECOR_WINDOW_FRAME
} DecorationType;

static GdkPixbuf *
take_window_shot (Window         child,
                  DecorationType decor)
{
  cairo_surface_t *surface;
  XWindowAttributes attrs;
  Window xid;
  Display *dpy;
  gint x = 0, y = 0;
  gint width, height;

  GdkPixbuf *tmp, *tmp2;
  GdkPixbuf *retval = NULL;

  if (decor == DECOR_WINDOW_FRAME)
    xid = find_toplevel_window (child);
  else
    xid = child;

  dpy = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
  XGetWindowAttributes (dpy, xid, &attrs);

  width = attrs.width;
  height = attrs.height;

  if (attrs.x < 0)
    {
      x = - attrs.x;
      width = width + attrs.x;
    }

  if (attrs.y < 0)
    {
      y = - attrs.y;
      height = height + attrs.y;
    }

  if (attrs.x + x + width > WidthOfScreen (DefaultScreenOfDisplay (dpy)))
    width = WidthOfScreen (DefaultScreenOfDisplay (dpy)) - attrs.x - x;

  if (attrs.y + y + height > HeightOfScreen (DefaultScreenOfDisplay (dpy)))
    height = HeightOfScreen (DefaultScreenOfDisplay (dpy)) - attrs.y - y;

  surface = cairo_xlib_surface_create (dpy,
                                       xid,
                                       attrs.visual,
                                       attrs.width,
                                       attrs.height);
  tmp = gdk_pixbuf_get_from_surface (surface,
                                     x, y,
                                     width, height);
  cairo_surface_destroy (surface);

  if (tmp != NULL)
    {
      if (decor == DECOR_WINDOW_FRAME)
        tmp2 = remove_shaped_area (tmp, xid);
      else if (decor == DECOR_FRAME)
        tmp2 = add_border_to_shot (tmp);
      else
        tmp2 = g_object_ref (tmp);

      g_object_unref (tmp);

      if (tmp2 != NULL)
        {
          retval = create_shadowed_pixbuf (tmp2);
          g_object_unref (tmp2);
        }
    }

  return retval;
}

static GList *toplevels;
static guint shot_id;

static gboolean
window_is_csd (GdkSurface *window)
{
  gboolean set;
  GdkWMDecoration decorations = 0;

  /* FIXME: is this accurate? */
  set = gdk_surface_get_decorations (window, &decorations);
  return (set && (decorations == 0));
}

static gboolean
shoot_one (WidgetInfo *info)
{
  GdkSurface *window;
  XID id;
  GdkPixbuf *screenshot = NULL;
  DecorationType decor = DECOR_FRAME;

  if (g_list_find (toplevels, info) == NULL)
    {
      g_warning ("Widget not found in queue");
      gtk_main_quit ();
    }

  window = gtk_native_get_surface (GTK_NATIVE (info->window));
  id = gdk_x11_surface_get_xid (window);
  if (window_is_csd (window))
    decor = (info->include_decorations) ? DECOR_NONE : DECOR_WINDOW_FRAME;
  screenshot = take_window_shot (id, decor);
  if (screenshot != NULL)
    {
      char *filename;
      filename = g_strdup_printf ("./%s.png", info->name);
      gdk_pixbuf_save (screenshot, filename, "png", NULL, NULL);
      g_free (filename);
      g_object_unref (screenshot);
    }
  else
    {
      g_warning ("unable to save shot of %s", info->name);
    }
  gtk_widget_destroy (info->window);

  shot_id = 0;

  /* remove from the queue and try to load up another */
  toplevels = g_list_remove (toplevels, info);
  if (toplevels == NULL)
    gtk_main_quit ();
  else
    queue_show ();

  return G_SOURCE_REMOVE;
}

static void
on_show (WidgetInfo *info)
{
  if (shot_id != 0)
    return;

  shot_id = g_timeout_add (500, (GSourceFunc) shoot_one, info);
}

static gboolean
show_one (void)
{
  WidgetInfo *info = toplevels->data;

  g_message ("shooting %s", info->name);

  g_signal_connect_swapped (info->window,
                            "show",
                            G_CALLBACK (on_show),
                            info);

  gtk_widget_show (info->window);

  return G_SOURCE_REMOVE;
}

static void
queue_show (void)
{
  g_idle_add ((GSourceFunc) show_one, NULL);
}

int main (int argc, char **argv)
{
  /* If there's no DISPLAY, we silently error out.  We don't want to break
   * headless builds. */
  if (! gtk_init_check ())
    return 0;

  toplevels = get_all_widgets ();

  queue_show ();
  gtk_main ();

  return 0;
}
