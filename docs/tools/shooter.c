#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdkx.h>
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

  retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
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

  retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
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

static GdkPixbuf *
take_window_shot (Window   child,
		  gboolean include_decoration)
{
  GdkWindow *window;
  Window xid;
  gint x_orig, y_orig;
  gint x = 0, y = 0;
  gint width, height;

  GdkPixbuf *tmp, *tmp2;
  GdkPixbuf *retval;

  if (include_decoration)
    xid = find_toplevel_window (child);
  else
    xid = child;

  window = gdk_x11_window_foreign_new_for_display (gdk_display_get_default (), xid);

  width = gdk_window_get_width (window);
  height = gdk_window_get_height (window);
  gdk_window_get_origin (window, &x_orig, &y_orig);

  if (x_orig < 0)
    {
      x = - x_orig;
      width = width + x_orig;
      x_orig = 0;
    }

  if (y_orig < 0)
    {
      y = - y_orig;
      height = height + y_orig;
      y_orig = 0;
    }

  if (x_orig + width > gdk_screen_width ())
    width = gdk_screen_width () - x_orig;

  if (y_orig + height > gdk_screen_height ())
    height = gdk_screen_height () - y_orig;

  tmp = gdk_pixbuf_get_from_window (window,
				    x, y, width, height);

  if (include_decoration)
    tmp2 = remove_shaped_area (tmp, xid);
  else
    tmp2 = add_border_to_shot (tmp);

  retval = create_shadowed_pixbuf (tmp2);
  g_object_unref (tmp);
  g_object_unref (tmp2);

  return retval;
}

int main (int argc, char **argv)
{
  GList *toplevels;
  GdkPixbuf *screenshot = NULL;
  GList *node;

  /* If there's no DISPLAY, we silently error out.  We don't want to break
   * headless builds. */
  if (! gtk_init_check (&argc, &argv))
    return 0;

  toplevels = get_all_widgets ();

  for (node = toplevels; node; node = g_list_next (node))
    {
      GtkAllocation allocation;
      GdkWindow *window;
      WidgetInfo *info;
      XID id;
      char *filename;

      info = node->data;

      gtk_widget_show (info->window);

      window = gtk_widget_get_window (info->window);
      gtk_widget_get_allocation (info->window, &allocation);

      gtk_widget_show_now (info->window);
      gtk_widget_queue_draw_area (info->window,
                                  allocation.x, allocation.y,
                                  allocation.width, allocation.height);
      gdk_window_process_updates (window, TRUE);

      while (gtk_events_pending ())
	{
	  gtk_main_iteration ();
	}
      sleep (1);

      while (gtk_events_pending ())
	{
	  gtk_main_iteration ();
	}

      id = gdk_x11_window_get_xid (window);
      screenshot = take_window_shot (id, info->include_decorations);
      filename = g_strdup_printf ("./%s.png", info->name);
      gdk_pixbuf_save (screenshot, filename, "png", NULL, NULL);
      g_free(filename);
      gtk_widget_hide (info->window);
    }

  return 0;
}
