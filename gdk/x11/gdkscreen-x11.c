 /*
 * gdkscreen-x11.c
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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

#include "config.h"

#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"
#include "xsettings-client.h"

#include <glib.h>

#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>

#ifdef HAVE_SOLARIS_XINERAMA
#include <X11/extensions/xinerama.h>
#endif
#ifdef HAVE_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

static void         gdk_x11_screen_dispose     (GObject		  *object);
static void         gdk_x11_screen_finalize    (GObject		  *object);
static void	    init_randr_support	       (GdkScreen	  *screen);
static void	    deinit_multihead           (GdkScreen         *screen);

enum
{
  WINDOW_MANAGER_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkX11Screen, gdk_x11_screen, GDK_TYPE_SCREEN)

typedef struct _NetWmSupportedAtoms NetWmSupportedAtoms;

struct _NetWmSupportedAtoms
{
  Atom *atoms;
  gulong n_atoms;
};

struct _GdkX11Monitor
{
  GdkRectangle  geometry;
  XID		output;
  int		width_mm;
  int		height_mm;
  char *	output_name;
  char *	manufacturer;
};


static void
gdk_x11_screen_init (GdkX11Screen *screen)
{
}

static GdkDisplay *
gdk_x11_screen_get_display (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->display;
}

static gint
gdk_x11_screen_get_width (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->width / GDK_X11_SCREEN (screen)->window_scale;
}

static gint
gdk_x11_screen_get_height (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->height / GDK_X11_SCREEN (screen)->window_scale;
}

static gint
gdk_x11_screen_get_width_mm (GdkScreen *screen)
{
  return WidthMMOfScreen (GDK_X11_SCREEN (screen)->xscreen);
}

static gint
gdk_x11_screen_get_height_mm (GdkScreen *screen)
{
  return HeightMMOfScreen (GDK_X11_SCREEN (screen)->xscreen);
}

static gint
gdk_x11_screen_get_number (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->screen_num;
}

static GdkWindow *
gdk_x11_screen_get_root_window (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->root_window;
}

static void
gdk_x11_screen_dispose (GObject *object)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (object);
  int i;

  for (i = 0; i < 32; ++i)
    {
      if (x11_screen->subwindow_gcs[i])
        {
          XFreeGC (x11_screen->xdisplay, x11_screen->subwindow_gcs[i]);
          x11_screen->subwindow_gcs[i] = 0;
        }
    }

  _gdk_x11_xsettings_finish (x11_screen);

  if (x11_screen->root_window)
    _gdk_window_destroy (x11_screen->root_window, TRUE);

  for (i = 0; i < x11_screen->nvisuals; i++)
    g_object_run_dispose (G_OBJECT (x11_screen->visuals[i]));

  G_OBJECT_CLASS (gdk_x11_screen_parent_class)->dispose (object);

  x11_screen->xdisplay = NULL;
  x11_screen->xscreen = NULL;
  x11_screen->screen_num = -1;
  x11_screen->xroot_window = None;
  x11_screen->wmspec_check_window = None;
}

static void
gdk_x11_screen_finalize (GObject *object)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (object);
  gint          i;

  if (x11_screen->root_window)
    g_object_unref (x11_screen->root_window);

  /* Visual Part */
  for (i = 0; i < x11_screen->nvisuals; i++)
    g_object_unref (x11_screen->visuals[i]);
  g_free (x11_screen->visuals);
  g_hash_table_destroy (x11_screen->visual_hash);

  g_free (x11_screen->window_manager_name);

  deinit_multihead (GDK_SCREEN (object));
  
  G_OBJECT_CLASS (gdk_x11_screen_parent_class)->finalize (object);
}

static gint
gdk_x11_screen_get_n_monitors (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->n_monitors;
}

static gint
gdk_x11_screen_get_primary_monitor (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->primary_monitor;
}

static gint
gdk_x11_screen_get_monitor_width_mm (GdkScreen *screen,
                                     gint       monitor_num)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  return x11_screen->monitors[monitor_num].width_mm;
}

static gint
gdk_x11_screen_get_monitor_height_mm (GdkScreen *screen,
				      gint       monitor_num)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  return x11_screen->monitors[monitor_num].height_mm;
}

static gchar *
gdk_x11_screen_get_monitor_plug_name (GdkScreen *screen,
				      gint       monitor_num)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  return g_strdup (x11_screen->monitors[monitor_num].output_name);
}

/**
 * gdk_x11_screen_get_monitor_output:
 * @screen: (type GdkX11Screen): a #GdkScreen
 * @monitor_num: number of the monitor, between 0 and gdk_screen_get_n_monitors (screen)
 *
 * Gets the XID of the specified output/monitor.
 * If the X server does not support version 1.2 of the RANDR
 * extension, 0 is returned.
 *
 * Returns: the XID of the monitor
 *
 * Since: 2.14
 */
XID
gdk_x11_screen_get_monitor_output (GdkScreen *screen,
                                   gint       monitor_num)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  g_return_val_if_fail (GDK_IS_SCREEN (screen), None);
  g_return_val_if_fail (monitor_num >= 0, None);
  g_return_val_if_fail (monitor_num < x11_screen->n_monitors, None);

  return x11_screen->monitors[monitor_num].output;
}

static void
gdk_x11_screen_get_monitor_geometry (GdkScreen    *screen,
				     gint          monitor_num,
				     GdkRectangle *dest)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  if (dest)
    {
      *dest = x11_screen->monitors[monitor_num].geometry;
      dest->x /= x11_screen->window_scale;
      dest->y /= x11_screen->window_scale;
      dest->width /= x11_screen->window_scale;
      dest->height /= x11_screen->window_scale;
    }
}

static int
get_current_desktop (GdkScreen *screen)
{
  Display *display;
  Window win;
  Atom current_desktop, type;
  int format;
  unsigned long n_items, bytes_after;
  unsigned char *data_return = NULL;
  int workspace = 0;

  if (!gdk_x11_screen_supports_net_wm_hint (screen,
                                            gdk_atom_intern_static_string ("_NET_CURRENT_DESKTOP")))
    return workspace;

  display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));
  win = XRootWindow (display, GDK_SCREEN_XNUMBER (screen));

  current_desktop = XInternAtom (display, "_NET_CURRENT_DESKTOP", True);

  XGetWindowProperty (display,
                      win,
                      current_desktop,
                      0, G_MAXLONG,
                      False, XA_CARDINAL,
                      &type, &format, &n_items, &bytes_after,
                      &data_return);

  if (type == XA_CARDINAL && format == 32 && n_items > 0)
    workspace = ((long *) data_return)[0];

  if (data_return)
    XFree (data_return);

  return workspace;
}

static void
get_work_area (GdkScreen    *screen,
               GdkRectangle *area)
{
  GdkX11Screen   *x11_screen = GDK_X11_SCREEN (screen);
  Atom            workarea;
  Atom            type;
  Window          win;
  int             format;
  gulong          num;
  gulong          leftovers;
  gulong          max_len = 4 * 32;
  guchar         *ret_workarea = NULL;
  long           *workareas;
  int             result;
  int             disp_screen;
  int             desktop;
  Display        *display;

  display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));
  disp_screen = GDK_SCREEN_XNUMBER (screen);
  workarea = XInternAtom (display, "_NET_WORKAREA", True);

  /* Defaults in case of error */
  area->x = 0;
  area->y = 0;
  area->width = gdk_screen_get_width (screen) / x11_screen->window_scale;
  area->height = gdk_screen_get_height (screen) / x11_screen->window_scale;

  if (!gdk_x11_screen_supports_net_wm_hint (screen,
                                            gdk_atom_intern_static_string ("_NET_WORKAREA")))
    return;

  if (workarea == None)
    return;

  win = XRootWindow (display, disp_screen);
  result = XGetWindowProperty (display,
                               win,
                               workarea,
                               0,
                               max_len,
                               False,
                               AnyPropertyType,
                               &type,
                               &format,
                               &num,
                               &leftovers,
                               &ret_workarea);
  if (result != Success ||
      type == None ||
      format == 0 ||
      leftovers ||
      num % 4 != 0)
    goto out;

  desktop = get_current_desktop (screen);
  if (desktop + 1 > num / 4) /* fvwm gets this wrong */
    goto out;

  workareas = (long *) ret_workarea;
  area->x = workareas[desktop * 4];
  area->y = workareas[desktop * 4 + 1];
  area->width = workareas[desktop * 4 + 2];
  area->height = workareas[desktop * 4 + 3];

  area->x /= x11_screen->window_scale;
  area->y /= x11_screen->window_scale;
  area->width /= x11_screen->window_scale;
  area->height /= x11_screen->window_scale;

out:
  if (ret_workarea)
    XFree (ret_workarea);
}

static void
gdk_x11_screen_get_monitor_workarea (GdkScreen    *screen,
                                     gint          monitor_num,
                                     GdkRectangle *dest)
{
  GdkRectangle workarea;

  gdk_x11_screen_get_monitor_geometry (screen, monitor_num, dest);

  /* The EWMH constrains workarea to be a rectangle, so it
   * can't adequately deal with L-shaped monitor arrangements.
   * As a workaround, we ignore the workarea for anything
   * but the primary monitor. Since that is where the 'desktop
   * chrome' usually lives, this works ok in practice.
   */
  if (monitor_num == GDK_X11_SCREEN (screen)->primary_monitor)
    {
      get_work_area (screen, &workarea);
      if (gdk_rectangle_intersect (dest, &workarea, &workarea))
        *dest = workarea;
    }
}

static gint
gdk_x11_screen_get_monitor_scale_factor (GdkScreen *screen,
					 gint       monitor_num)
{
  GdkX11Screen *screen_x11 = GDK_X11_SCREEN (screen);

  return screen_x11->window_scale;
}

static GdkVisual *
gdk_x11_screen_get_rgba_visual (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  return x11_screen->rgba_visual;
}

/**
 * gdk_x11_screen_get_xscreen:
 * @screen: (type GdkX11Screen): a #GdkScreen
 *
 * Returns the screen of a #GdkScreen.
 *
 * Returns: (transfer none): an Xlib Screen*
 *
 * Since: 2.2
 */
Screen *
gdk_x11_screen_get_xscreen (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->xscreen;
}

/**
 * gdk_x11_screen_get_screen_number:
 * @screen: (type GdkX11Screen): a #GdkScreen
 *
 * Returns the index of a #GdkScreen.
 *
 * Returns: the position of @screen among the screens
 *     of its display
 *
 * Since: 2.2
 */
int
gdk_x11_screen_get_screen_number (GdkScreen *screen)
{
  return GDK_X11_SCREEN (screen)->screen_num;
}

static Atom
get_cm_atom (GdkX11Screen *x11_screen)
{
  return _gdk_x11_get_xatom_for_display_printf (x11_screen->display, "_NET_WM_CM_S%d", x11_screen->screen_num);
}

static gboolean
check_is_composited (GdkDisplay *display,
		     GdkX11Screen *x11_screen)
{
  Window xwindow;
  
  xwindow = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display), get_cm_atom (x11_screen));

  return xwindow != None;
}

static void
init_monitor_geometry (GdkX11Monitor *monitor,
		       int x, int y, int width, int height)
{
  monitor->geometry.x = x;
  monitor->geometry.y = y;
  monitor->geometry.width = width;
  monitor->geometry.height = height;

  monitor->output = None;
  monitor->width_mm = -1;
  monitor->height_mm = -1;
  monitor->output_name = NULL;
  monitor->manufacturer = NULL;
}

static gboolean
init_fake_xinerama (GdkScreen *screen)
{
#ifdef G_ENABLE_DEBUG
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  XSetWindowAttributes atts;
  Window win;
  gint w, h;

  if (!(_gdk_debug_flags & GDK_DEBUG_XINERAMA))
    return FALSE;
  
  /* Fake Xinerama mode by splitting the screen into 4 monitors.
   * Also draw a little cross to make the monitor boundaries visible.
   */
  w = WidthOfScreen (x11_screen->xscreen);
  h = HeightOfScreen (x11_screen->xscreen);

  x11_screen->n_monitors = 4;
  x11_screen->monitors = g_new0 (GdkX11Monitor, 4);
  init_monitor_geometry (&x11_screen->monitors[0], 0, 0, w / 2, h / 2);
  init_monitor_geometry (&x11_screen->monitors[1], w / 2, 0, w / 2, h / 2);
  init_monitor_geometry (&x11_screen->monitors[2], 0, h / 2, w / 2, h / 2);
  init_monitor_geometry (&x11_screen->monitors[3], w / 2, h / 2, w / 2, h / 2);
  
  atts.override_redirect = 1;
  atts.background_pixel = WhitePixel(GDK_SCREEN_XDISPLAY (screen), 
				     x11_screen->screen_num);
  win = XCreateWindow(GDK_SCREEN_XDISPLAY (screen), 
		      x11_screen->xroot_window, 0, h / 2, w, 1, 0, 
		      DefaultDepth(GDK_SCREEN_XDISPLAY (screen), 
				   x11_screen->screen_num),
		      InputOutput, 
		      DefaultVisual(GDK_SCREEN_XDISPLAY (screen), 
				    x11_screen->screen_num),
		      CWOverrideRedirect|CWBackPixel, 
		      &atts);
  XMapRaised(GDK_SCREEN_XDISPLAY (screen), win); 
  win = XCreateWindow(GDK_SCREEN_XDISPLAY (screen), 
		      x11_screen->xroot_window, w/2 , 0, 1, h, 0, 
		      DefaultDepth(GDK_SCREEN_XDISPLAY (screen), 
				   x11_screen->screen_num),
		      InputOutput, 
		      DefaultVisual(GDK_SCREEN_XDISPLAY (screen), 
				    x11_screen->screen_num),
		      CWOverrideRedirect|CWBackPixel, 
		      &atts);
  XMapRaised(GDK_SCREEN_XDISPLAY (screen), win);
  return TRUE;
#endif
  
  return FALSE;
}

static void
free_monitors (GdkX11Monitor *monitors,
               gint           n_monitors)
{
  int i;

  for (i = 0; i < n_monitors; ++i)
    {
      g_free (monitors[i].output_name);
      g_free (monitors[i].manufacturer);
    }

  g_free (monitors);
}

#ifdef HAVE_RANDR
static int
monitor_compare_function (GdkX11Monitor *monitor1,
                          GdkX11Monitor *monitor2)
{
  /* Sort the leftmost/topmost monitors first.
   * For "cloned" monitors, sort the bigger ones first
   * (giving preference to taller monitors over wider
   * monitors)
   */

  if (monitor1->geometry.x != monitor2->geometry.x)
    return monitor1->geometry.x - monitor2->geometry.x;

  if (monitor1->geometry.y != monitor2->geometry.y)
    return monitor1->geometry.y - monitor2->geometry.y;

  if (monitor1->geometry.height != monitor2->geometry.height)
    return - (monitor1->geometry.height - monitor2->geometry.height);

  if (monitor1->geometry.width != monitor2->geometry.width)
    return - (monitor1->geometry.width - monitor2->geometry.width);

  return 0;
}
#endif

static gboolean
init_randr13 (GdkScreen *screen)
{
#ifdef HAVE_RANDR
  GdkDisplay *display = gdk_screen_get_display (screen);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  Display *dpy = GDK_SCREEN_XDISPLAY (screen);
  XRRScreenResources *resources;
  RROutput primary_output;
  RROutput first_output = None;
  int i;
  GArray *monitors;
  gboolean randr12_compat = FALSE;

  if (!display_x11->have_randr13)
      return FALSE;

  resources = XRRGetScreenResourcesCurrent (x11_screen->xdisplay,
				            x11_screen->xroot_window);
  if (!resources)
    return FALSE;

  monitors = g_array_sized_new (FALSE, TRUE, sizeof (GdkX11Monitor),
                                resources->noutput);

  for (i = 0; i < resources->noutput; ++i)
    {
      XRROutputInfo *output =
	XRRGetOutputInfo (dpy, resources, resources->outputs[i]);

      /* Non RandR1.2 X driver have output name "default" */
      randr12_compat |= !g_strcmp0 (output->name, "default");

      if (output->connection == RR_Disconnected)
        {
          XRRFreeOutputInfo (output);
          continue;
        }

      if (output->crtc)
	{
	  GdkX11Monitor monitor;
	  XRRCrtcInfo *crtc = XRRGetCrtcInfo (dpy, resources, output->crtc);

	  monitor.geometry.x = crtc->x;
	  monitor.geometry.y = crtc->y;
	  monitor.geometry.width = crtc->width;
	  monitor.geometry.height = crtc->height;

	  monitor.output = resources->outputs[i];
	  monitor.width_mm = output->mm_width;
	  monitor.height_mm = output->mm_height;
	  monitor.output_name = g_strdup (output->name);
	  /* FIXME: need EDID parser */
	  monitor.manufacturer = NULL;

	  g_array_append_val (monitors, monitor);

          XRRFreeCrtcInfo (crtc);
	}

      XRRFreeOutputInfo (output);
    }

  if (resources->noutput > 0)
    first_output = resources->outputs[0];

  XRRFreeScreenResources (resources);

  /* non RandR 1.2 X driver doesn't return any usable multihead data */
  if (randr12_compat)
    {
      guint n_monitors = monitors->len;

      free_monitors ((GdkX11Monitor *)g_array_free (monitors, FALSE),
		     n_monitors);

      return FALSE;
    }

  g_array_sort (monitors,
                (GCompareFunc) monitor_compare_function);
  x11_screen->n_monitors = monitors->len;
  x11_screen->monitors = (GdkX11Monitor *)g_array_free (monitors, FALSE);

  x11_screen->primary_monitor = 0;

  primary_output = XRRGetOutputPrimary (x11_screen->xdisplay,
                                        x11_screen->xroot_window);

  for (i = 0; i < x11_screen->n_monitors; ++i)
    {
      if (x11_screen->monitors[i].output == primary_output)
	{
	  x11_screen->primary_monitor = i;
	  break;
	}

      /* No RandR1.3+ available or no primary set, fall back to prefer LVDS as primary if present */
      if (primary_output == None &&
          g_ascii_strncasecmp (x11_screen->monitors[i].output_name, "LVDS", 4) == 0)
	{
	  x11_screen->primary_monitor = i;
	  break;
	}

      /* No primary specified and no LVDS found */
      if (x11_screen->monitors[i].output == first_output)
	x11_screen->primary_monitor = i;
    }

  return x11_screen->n_monitors > 0;
#endif

  return FALSE;
}

static gboolean
init_solaris_xinerama (GdkScreen *screen)
{
#ifdef HAVE_SOLARIS_XINERAMA
  Display *dpy = GDK_SCREEN_XDISPLAY (screen);
  int screen_no = gdk_screen_get_number (screen);
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  XRectangle monitors[MAXFRAMEBUFFERS];
  unsigned char hints[16];
  gint result;
  int n_monitors;
  int i;
  
  if (!XineramaGetState (dpy, screen_no))
    return FALSE;

  result = XineramaGetInfo (dpy, screen_no, monitors, hints, &n_monitors);

  /* Yes I know it should be Success but the current implementation 
   * returns the num of monitor
   */
  if (result == 0)
    {
      return FALSE;
    }

  x11_screen->monitors = g_new0 (GdkX11Monitor, n_monitors);
  x11_screen->n_monitors = n_monitors;

  for (i = 0; i < n_monitors; i++)
    {
      init_monitor_geometry (&x11_screen->monitors[i],
			     monitors[i].x, monitors[i].y,
			     monitors[i].width, monitors[i].height);
    }

  x11_screen->primary_monitor = 0;

  return TRUE;
#endif /* HAVE_SOLARIS_XINERAMA */

  return FALSE;
}

static gboolean
init_xfree_xinerama (GdkScreen *screen)
{
#ifdef HAVE_XFREE_XINERAMA
  Display *dpy = GDK_SCREEN_XDISPLAY (screen);
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  XineramaScreenInfo *monitors;
  int i, n_monitors;
  
  if (!XineramaIsActive (dpy))
    return FALSE;

  monitors = XineramaQueryScreens (dpy, &n_monitors);
  
  if (n_monitors <= 0 || monitors == NULL)
    {
      /* If Xinerama doesn't think we have any monitors, try acting as
       * though we had no Xinerama. If the "no monitors" condition
       * is because XRandR 1.2 is currently switching between CRTCs,
       * we'll be notified again when we have our monitor back,
       * and can go back into Xinerama-ish mode at that point.
       */
      if (monitors)
	XFree (monitors);
      
      return FALSE;
    }

  x11_screen->n_monitors = n_monitors;
  x11_screen->monitors = g_new0 (GdkX11Monitor, n_monitors);
  
  for (i = 0; i < n_monitors; ++i)
    {
      init_monitor_geometry (&x11_screen->monitors[i],
			     monitors[i].x_org, monitors[i].y_org,
			     monitors[i].width, monitors[i].height);
    }
  
  XFree (monitors);
  
  x11_screen->primary_monitor = 0;

  return TRUE;
#endif /* HAVE_XFREE_XINERAMA */
  
  return FALSE;
}

static gboolean
init_solaris_xinerama_indices (GdkX11Screen *x11_screen)
{
#ifdef HAVE_SOLARIS_XINERAMA
  XRectangle    x_monitors[MAXFRAMEBUFFERS];
  unsigned char hints[16];
  gint          result;
  gint          monitor_num;
  gint          x_n_monitors;
  gint          i;

  if (!XineramaGetState (x11_screen->xdisplay, x11_screen->screen_num))
    return FALSE;

  result = XineramaGetInfo (x11_screen->xdisplay, x11_screen->screen_num,
                            x_monitors, hints, &x_n_monitors);

  if (result == 0)
    return FALSE;


  for (monitor_num = 0; monitor_num < x11_screen->n_monitors; ++monitor_num)
    {
      for (i = 0; i < x_n_monitors; ++i)
        {
          if (x11_screen->monitors[monitor_num].geometry.x == x_monitors[i].x &&
	      x11_screen->monitors[monitor_num].geometry.y == x_monitors[i].y &&
	      x11_screen->monitors[monitor_num].geometry.width == x_monitors[i].width &&
	      x11_screen->monitors[monitor_num].geometry.height == x_monitors[i].height)
	    {
	      g_hash_table_insert (x11_screen->xinerama_matches,
				   GINT_TO_POINTER (monitor_num),
				   GINT_TO_POINTER (i));
	    }
        }
    }
  return TRUE;
#endif /* HAVE_SOLARIS_XINERAMA */

  return FALSE;
}

static gboolean
init_xfree_xinerama_indices (GdkX11Screen *x11_screen)
{
#ifdef HAVE_XFREE_XINERAMA
  XineramaScreenInfo *x_monitors;
  gint                monitor_num;
  gint                x_n_monitors;
  gint                i;

  if (!XineramaIsActive (x11_screen->xdisplay))
    return FALSE;

  x_monitors = XineramaQueryScreens (x11_screen->xdisplay, &x_n_monitors);
  if (x_n_monitors <= 0 || x_monitors == NULL)
    {
      if (x_monitors)
	XFree (x_monitors);

      return FALSE;
    }

  for (monitor_num = 0; monitor_num < x11_screen->n_monitors; ++monitor_num)
    {
      for (i = 0; i < x_n_monitors; ++i)
        {
          if (x11_screen->monitors[monitor_num].geometry.x == x_monitors[i].x_org &&
	      x11_screen->monitors[monitor_num].geometry.y == x_monitors[i].y_org &&
	      x11_screen->monitors[monitor_num].geometry.width == x_monitors[i].width &&
	      x11_screen->monitors[monitor_num].geometry.height == x_monitors[i].height)
	    {
	      g_hash_table_insert (x11_screen->xinerama_matches,
				   GINT_TO_POINTER (monitor_num),
				   GINT_TO_POINTER (i));
	    }
        }
    }
  XFree (x_monitors);
  return TRUE;
#endif /* HAVE_XFREE_XINERAMA */

  return FALSE;
}

static void
init_xinerama_indices (GdkX11Screen *x11_screen)
{
  int opcode, firstevent, firsterror;

  x11_screen->xinerama_matches = g_hash_table_new (g_direct_hash, g_direct_equal);
  if (XQueryExtension (x11_screen->xdisplay, "XINERAMA",
		       &opcode, &firstevent, &firsterror))
    {
      x11_screen->xinerama_matches = g_hash_table_new (g_direct_hash, g_direct_equal);

      /* Solaris Xinerama first, then XFree/Xorg Xinerama
       * to match the order in init_multihead()
       */
      if (init_solaris_xinerama_indices (x11_screen) == FALSE)
        init_xfree_xinerama_indices (x11_screen);
    }
}

gint
_gdk_x11_screen_get_xinerama_index (GdkScreen *screen,
				    gint       monitor_num)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  gpointer val;

  g_return_val_if_fail (monitor_num < x11_screen->n_monitors, -1);

  if (x11_screen->xinerama_matches == NULL)
    init_xinerama_indices (x11_screen);

  if (g_hash_table_lookup_extended (x11_screen->xinerama_matches, GINT_TO_POINTER (monitor_num), NULL, &val))
    return (GPOINTER_TO_INT(val));

  return -1;
}

void
_gdk_x11_screen_get_edge_monitors (GdkScreen *screen,
                                   gint      *top,
                                   gint      *bottom,
                                   gint      *left,
                                   gint      *right)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  gint          top_most_pos = x11_screen->height;
  gint          left_most_pos = x11_screen->width;
  gint          bottom_most_pos = 0;
  gint          right_most_pos = 0;
  gint          monitor_num;

  for (monitor_num = 0; monitor_num < x11_screen->n_monitors; monitor_num++)
    {
      gint monitor_x = x11_screen->monitors[monitor_num].geometry.x;
      gint monitor_y = x11_screen->monitors[monitor_num].geometry.y;
      gint monitor_max_x = monitor_x + x11_screen->monitors[monitor_num].geometry.width;
      gint monitor_max_y = monitor_y + x11_screen->monitors[monitor_num].geometry.height;

      if (left && left_most_pos > monitor_x)
	{
	  left_most_pos = monitor_x;
	  *left = monitor_num;
	}
      if (right && right_most_pos < monitor_max_x)
	{
	  right_most_pos = monitor_max_x;
	  *right = monitor_num;
	}
      if (top && top_most_pos > monitor_y)
	{
	  top_most_pos = monitor_y;
	  *top = monitor_num;
	}
      if (bottom && bottom_most_pos < monitor_max_y)
	{
	  bottom_most_pos = monitor_max_y;
	  *bottom = monitor_num;
	}
    }
}

static void
deinit_multihead (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  free_monitors (x11_screen->monitors, x11_screen->n_monitors);
  g_clear_pointer (&x11_screen->xinerama_matches, g_hash_table_destroy);

  x11_screen->n_monitors = 0;
  x11_screen->monitors = NULL;
}

static gboolean
compare_monitor (GdkX11Monitor *m1,
                 GdkX11Monitor *m2)
{
  if (m1->geometry.x != m2->geometry.x ||
      m1->geometry.y != m2->geometry.y ||
      m1->geometry.width != m2->geometry.width ||
      m1->geometry.height != m2->geometry.height)
    return FALSE;

  if (m1->width_mm != m2->width_mm ||
      m1->height_mm != m2->height_mm)
    return FALSE;

  if (g_strcmp0 (m1->output_name, m2->output_name) != 0)
    return FALSE;

  if (g_strcmp0 (m1->manufacturer, m2->manufacturer) != 0)
    return FALSE;

  return TRUE;
}

static gboolean
compare_monitors (GdkX11Monitor *monitors1, gint n_monitors1,
                  GdkX11Monitor *monitors2, gint n_monitors2)
{
  gint i;

  if (n_monitors1 != n_monitors2)
    return FALSE;

  for (i = 0; i < n_monitors1; i++)
    {
      if (!compare_monitor (monitors1 + i, monitors2 + i))
        return FALSE;
    }

  return TRUE;
}

static void
init_multihead (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  int opcode, firstevent, firsterror;

  /* There are four different implementations of multihead support: 
   *
   *  1. Fake Xinerama for debugging purposes
   *  2. RandR 1.2
   *  3. Solaris Xinerama
   *  4. XFree86/Xorg Xinerama
   *
   * We use them in that order.
   */
  if (init_fake_xinerama (screen))
    return;

  if (init_randr13 (screen))
    return;

  if (XQueryExtension (GDK_SCREEN_XDISPLAY (screen), "XINERAMA",
		       &opcode, &firstevent, &firsterror))
    {
      if (init_solaris_xinerama (screen))
	return;
      
      if (init_xfree_xinerama (screen))
	return;
    }

  /* No multihead support of any kind for this screen */
  x11_screen->n_monitors = 1;
  x11_screen->monitors = g_new0 (GdkX11Monitor, 1);
  x11_screen->primary_monitor = 0;

  init_monitor_geometry (x11_screen->monitors, 0, 0,
			 WidthOfScreen (x11_screen->xscreen),
			 HeightOfScreen (x11_screen->xscreen));
}

static void
update_bounding_box (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  gint i, x1, y1, x2, y2;

  x1 = y1 = G_MAXINT;
  x2 = y2 = G_MININT;

  for (i = 0; i < x11_screen->n_monitors; i++)
    {
      GdkX11Monitor *monitor;

      monitor = &x11_screen->monitors[i];
      x1 = MIN (x1, monitor->geometry.x);
      y1 = MIN (y1, monitor->geometry.y);
      x2 = MAX (x2, monitor->geometry.x + monitor->geometry.width);
      y2 = MAX (y2, monitor->geometry.y + monitor->geometry.height);
    }

  x11_screen->width = x2 - x1;
  x11_screen->height = y2 - y1;
}

GdkScreen *
_gdk_x11_screen_new (GdkDisplay *display,
		     gint	 screen_number) 
{
  GdkScreen *screen;
  GdkX11Screen *x11_screen;
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  const char *scale_str;

  screen = g_object_new (GDK_TYPE_X11_SCREEN, NULL);

  x11_screen = GDK_X11_SCREEN (screen);
  x11_screen->display = display;
  x11_screen->xdisplay = display_x11->xdisplay;
  x11_screen->xscreen = ScreenOfDisplay (display_x11->xdisplay, screen_number);
  x11_screen->screen_num = screen_number;
  x11_screen->xroot_window = RootWindow (display_x11->xdisplay,screen_number);
  x11_screen->wmspec_check_window = None;
  /* we want this to be always non-null */
  x11_screen->window_manager_name = g_strdup ("unknown");

#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  scale_str = g_getenv ("GDK_SCALE");
#else
  scale_str = "1";
#endif
  if (scale_str)
    {
      x11_screen->fixed_window_scale = TRUE;
      x11_screen->window_scale = atol (scale_str);
      if (x11_screen->window_scale == 0)
        x11_screen->window_scale = 1;
    }
  else
    x11_screen->window_scale = 1;

  init_multihead (screen);
  init_randr_support (screen);
  
  _gdk_x11_screen_init_visuals (screen);
  _gdk_x11_screen_init_root_window (screen);
  update_bounding_box (screen);

  return screen;
}

void
_gdk_x11_screen_set_window_scale (GdkX11Screen *x11_screen,
				  gint          scale)
{
  GList *toplevels, *l;
  GdkWindow *root;

  if (x11_screen->window_scale == scale)
    return;

  x11_screen->window_scale = scale;

  root = x11_screen->root_window;
  GDK_WINDOW_IMPL_X11 (root->impl)->window_scale = scale;

  toplevels = gdk_screen_get_toplevel_windows (GDK_SCREEN (x11_screen));

  for (l = toplevels; l != NULL; l = l->next)
    {
      GdkWindow *window = l->data;

      _gdk_x11_window_set_window_scale (window, scale);
    }

  g_signal_emit_by_name (GDK_SCREEN (x11_screen), "monitors-changed");
}

/*
 * It is important that we first request the selection
 * notification, and then setup the initial state of
 * is_composited to avoid a race condition here.
 */
void
_gdk_x11_screen_setup (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  gdk_display_request_selection_notification (x11_screen->display,
					      gdk_x11_xatom_to_atom_for_display (x11_screen->display, get_cm_atom (x11_screen)));
  x11_screen->is_composited = check_is_composited (x11_screen->display, x11_screen);
}

static gboolean
gdk_x11_screen_is_composited (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  return x11_screen->is_composited;
}

static void
init_randr_support (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  /* NB: This is also needed for XSettings, so don't remove. */
  XSelectInput (GDK_SCREEN_XDISPLAY (screen),
                x11_screen->xroot_window,
                StructureNotifyMask);

#ifdef HAVE_RANDR
  if (!GDK_X11_DISPLAY (gdk_screen_get_display (screen))->have_randr12)
    return;

  XRRSelectInput (GDK_SCREEN_XDISPLAY (screen),
                  x11_screen->xroot_window,
                  RRScreenChangeNotifyMask
                  | RRCrtcChangeNotifyMask
                  | RROutputPropertyNotifyMask);
#endif
}

static void
process_monitors_change (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  gint		 n_monitors;
  gint		 primary_monitor;
  GdkX11Monitor	*monitors;
  gboolean changed;

  primary_monitor = x11_screen->primary_monitor;
  n_monitors = x11_screen->n_monitors;
  monitors = x11_screen->monitors;

  x11_screen->n_monitors = 0;
  x11_screen->monitors = NULL;

  init_multihead (screen);

  changed =
    !compare_monitors (monitors, n_monitors,
		       x11_screen->monitors, x11_screen->n_monitors) ||
    x11_screen->primary_monitor != primary_monitor;

  free_monitors (monitors, n_monitors);

  if (changed)
    {
      update_bounding_box (screen);
      g_signal_emit_by_name (screen, "monitors-changed");
    }
}

void
_gdk_x11_screen_size_changed (GdkScreen *screen,
			      XEvent    *event)
{
  gint width, height;
#ifdef HAVE_RANDR
  GdkX11Display *display_x11;
#endif

  width = gdk_screen_get_width (screen);
  height = gdk_screen_get_height (screen);

#ifdef HAVE_RANDR
  display_x11 = GDK_X11_DISPLAY (gdk_screen_get_display (screen));

  if (display_x11->have_randr13 && event->type == ConfigureNotify)
    return;

  XRRUpdateConfiguration (event);
#else
  if (event->type == ConfigureNotify)
    {
      XConfigureEvent *rcevent = (XConfigureEvent *) event;
      Screen	    *xscreen = gdk_x11_screen_get_xscreen (screen);

      xscreen->width   = rcevent->width;
      xscreen->height  = rcevent->height;
    }
  else
    return;
#endif

  process_monitors_change (screen);

  if (width != gdk_screen_get_width (screen) ||
      height != gdk_screen_get_height (screen))
    g_signal_emit_by_name (screen, "size-changed");
}

void
_gdk_x11_screen_window_manager_changed (GdkScreen *screen)
{
  g_signal_emit (screen, signals[WINDOW_MANAGER_CHANGED], 0);
}

void
_gdk_x11_screen_process_owner_change (GdkScreen *screen,
				      XEvent *event)
{
#ifdef HAVE_XFIXES
  XFixesSelectionNotifyEvent *selection_event = (XFixesSelectionNotifyEvent *)event;
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  if (selection_event->selection == get_cm_atom (x11_screen))
    {
      gboolean composited = selection_event->owner != None;

      if (composited != x11_screen->is_composited)
	{
	  x11_screen->is_composited = composited;

	  g_signal_emit_by_name (screen, "composited-changed");
	}
    }
#endif
}

static gchar *
substitute_screen_number (const gchar *display_name,
                          gint         screen_number)
{
  GString *str;
  gchar   *p;

  str = g_string_new (display_name);

  p = strrchr (str->str, '.');
  if (p && p >	strchr (str->str, ':'))
    g_string_truncate (str, p - str->str);

  g_string_append_printf (str, ".%d", screen_number);

  return g_string_free (str, FALSE);
}

static gchar *
gdk_x11_screen_make_display_name (GdkScreen *screen)
{
  const gchar *old_display;

  old_display = gdk_display_get_name (gdk_screen_get_display (screen));

  return substitute_screen_number (old_display,
                                   gdk_screen_get_number (screen));
}

static GdkWindow *
gdk_x11_screen_get_active_window (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  GdkWindow *ret = NULL;
  Atom type_return;
  gint format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;

  if (!gdk_x11_screen_supports_net_wm_hint (screen,
                                            gdk_atom_intern_static_string ("_NET_ACTIVE_WINDOW")))
    return NULL;

  if (XGetWindowProperty (x11_screen->xdisplay, x11_screen->xroot_window,
	                  gdk_x11_get_xatom_by_name_for_display (x11_screen->display,
			                                         "_NET_ACTIVE_WINDOW"),
		          0, 1, False, XA_WINDOW, &type_return,
		          &format_return, &nitems_return,
                          &bytes_after_return, &data)
      == Success)
    {
      if ((type_return == XA_WINDOW) && (format_return == 32) && (data))
        {
          Window window = *(Window *) data;

          if (window != None)
            {
              ret = gdk_x11_window_foreign_new_for_display (x11_screen->display,
                                                            window);
            }
        }
    }

  if (data)
    XFree (data);

  return ret;
}

static GList *
gdk_x11_screen_get_window_stack (GdkScreen *screen)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  GList *ret = NULL;
  Atom type_return;
  gint format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;

  if (!gdk_x11_screen_supports_net_wm_hint (screen,
                                            gdk_atom_intern_static_string ("_NET_CLIENT_LIST_STACKING")))
    return NULL;

  if (XGetWindowProperty (x11_screen->xdisplay, x11_screen->xroot_window,
	                  gdk_x11_get_xatom_by_name_for_display (x11_screen->display,
			                                         "_NET_CLIENT_LIST_STACKING"),
		          0, G_MAXLONG, False, XA_WINDOW, &type_return,
		          &format_return, &nitems_return,
                          &bytes_after_return, &data)
      == Success)
    {
      if ((type_return == XA_WINDOW) && (format_return == 32) &&
          (data) && (nitems_return > 0))
        {
          gulong *stack = (gulong *) data;
          GdkWindow *win;
          int i;

          for (i = 0; i < nitems_return; i++)
            {
              win = gdk_x11_window_foreign_new_for_display (x11_screen->display,
                                                            (Window)stack[i]);

              if (win != NULL)
                ret = g_list_append (ret, win);
            }
        }
    }

  if (data)
    XFree (data);

  return ret;
}

static gboolean
gdk_x11_screen_get_setting (GdkScreen   *screen,
			    const gchar *name,
			    GValue      *value)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  const GValue *setting;

  if (x11_screen->xsettings == NULL)
    goto out;
  setting = g_hash_table_lookup (x11_screen->xsettings, name);
  if (setting == NULL)
    goto out;

  if (!g_value_type_transformable (G_VALUE_TYPE (setting), G_VALUE_TYPE (value)))
    {
      g_warning ("Cannot transform xsetting %s of type %s to type %s\n",
		 name,
		 g_type_name (G_VALUE_TYPE (setting)),
		 g_type_name (G_VALUE_TYPE (value)));
      goto out;
    }

  g_value_transform (setting, value);

  return TRUE;

 out:
  return _gdk_x11_get_xft_setting (screen, name, value);
}

static void
cleanup_atoms(gpointer data)
{
  NetWmSupportedAtoms *supported_atoms = data;
  if (supported_atoms->atoms)
      XFree (supported_atoms->atoms);
  g_free (supported_atoms);
}

static Window
get_net_supporting_wm_check (GdkX11Screen *screen,
                             Window        window)
{
  GdkDisplay *display;
  Atom type;
  gint format;
  gulong n_items;
  gulong bytes_after;
  guchar *data;
  Window value;

  display = screen->display;
  type = None;
  data = NULL;
  value = None;

  gdk_x11_display_error_trap_push (display);
  XGetWindowProperty (screen->xdisplay, window,
		      gdk_x11_get_xatom_by_name_for_display (display, "_NET_SUPPORTING_WM_CHECK"),
		      0, G_MAXLONG, False, XA_WINDOW, &type, &format,
		      &n_items, &bytes_after, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (type == XA_WINDOW)
    value = *(Window *)data;

  if (data)
    XFree (data);

  return value;
}

static void
fetch_net_wm_check_window (GdkScreen *screen)
{
  GdkX11Screen *x11_screen;
  GdkDisplay *display;
  Window window;
  GTimeVal tv;
  gint error;

  x11_screen = GDK_X11_SCREEN (screen);
  display = x11_screen->display;

  g_return_if_fail (GDK_X11_DISPLAY (display)->trusted_client);

  if (x11_screen->wmspec_check_window != None)
    return; /* already have it */

  g_get_current_time (&tv);

  if (ABS  (tv.tv_sec - x11_screen->last_wmspec_check_time) < 15)
    return; /* we've checked recently */

  window = get_net_supporting_wm_check (x11_screen, x11_screen->xroot_window);
  if (window == None)
    return;

  if (window != get_net_supporting_wm_check (x11_screen, window))
    return;

  gdk_x11_display_error_trap_push (display);

  /* Find out if this WM goes away, so we can reset everything. */
  XSelectInput (x11_screen->xdisplay, window, StructureNotifyMask);

  error = gdk_x11_display_error_trap_pop (display);
  if (!error)
    {
      /* We check the window property again because after XGetWindowProperty()
       * and before XSelectInput() the window may have been recycled in such a
       * way that XSelectInput() doesn't fail but the window is no longer what
       * we want.
       */
      if (window != get_net_supporting_wm_check (x11_screen, window))
        return;

      x11_screen->wmspec_check_window = window;
      x11_screen->last_wmspec_check_time = tv.tv_sec;
      x11_screen->need_refetch_net_supported = TRUE;
      x11_screen->need_refetch_wm_name = TRUE;

      /* Careful, reentrancy */
      _gdk_x11_screen_window_manager_changed (screen);
    }
}

/**
 * gdk_x11_screen_supports_net_wm_hint:
 * @screen: (type GdkX11Screen): the relevant #GdkScreen.
 * @property: a property atom.
 *
 * This function is specific to the X11 backend of GDK, and indicates
 * whether the window manager supports a certain hint from the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 *
 * When using this function, keep in mind that the window manager
 * can change over time; so you shouldn’t use this function in
 * a way that impacts persistent application state. A common bug
 * is that your application can start up before the window manager
 * does when the user logs in, and before the window manager starts
 * gdk_x11_screen_supports_net_wm_hint() will return %FALSE for every property.
 * You can monitor the window_manager_changed signal on #GdkScreen to detect
 * a window manager change.
 *
 * Returns: %TRUE if the window manager supports @property
 *
 * Since: 2.2
 **/
gboolean
gdk_x11_screen_supports_net_wm_hint (GdkScreen *screen,
				     GdkAtom    property)
{
  gulong i;
  GdkX11Screen *x11_screen;
  NetWmSupportedAtoms *supported_atoms;
  GdkDisplay *display;
  Atom atom;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  x11_screen = GDK_X11_SCREEN (screen);
  display = x11_screen->display;

  if (!G_LIKELY (GDK_X11_DISPLAY (display)->trusted_client))
    return FALSE;

  supported_atoms = g_object_get_data (G_OBJECT (screen), "gdk-net-wm-supported-atoms");
  if (!supported_atoms)
    {
      supported_atoms = g_new0 (NetWmSupportedAtoms, 1);
      g_object_set_data_full (G_OBJECT (screen), "gdk-net-wm-supported-atoms", supported_atoms, cleanup_atoms);
    }

  fetch_net_wm_check_window (screen);

  if (x11_screen->wmspec_check_window == None)
    return FALSE;

  if (x11_screen->need_refetch_net_supported)
    {
      /* WM has changed since we last got the supported list,
       * refetch it.
       */
      Atom type;
      gint format;
      gulong bytes_after;

      x11_screen->need_refetch_net_supported = FALSE;

      if (supported_atoms->atoms)
        XFree (supported_atoms->atoms);

      supported_atoms->atoms = NULL;
      supported_atoms->n_atoms = 0;

      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), x11_screen->xroot_window,
                          gdk_x11_get_xatom_by_name_for_display (display, "_NET_SUPPORTED"),
                          0, G_MAXLONG, False, XA_ATOM, &type, &format,
                          &supported_atoms->n_atoms, &bytes_after,
                          (guchar **)&supported_atoms->atoms);

      if (type != XA_ATOM)
        return FALSE;
    }

  if (supported_atoms->atoms == NULL)
    return FALSE;

  atom = gdk_x11_atom_to_xatom_for_display (display, property);

  for (i = 0; i < supported_atoms->n_atoms; i++)
    {
      if (supported_atoms->atoms[i] == atom)
        return TRUE;
    }

  return FALSE;
}

/**
 * gdk_x11_screen_get_window_manager_name:
 * @screen: (type GdkX11Screen): a #GdkScreen
 *
 * Returns the name of the window manager for @screen.
 *
 * Returns: the name of the window manager screen @screen, or
 * "unknown" if the window manager is unknown. The string is owned by GDK
 * and should not be freed.
 *
 * Since: 2.2
 **/
const char*
gdk_x11_screen_get_window_manager_name (GdkScreen *screen)
{
  GdkX11Screen *x11_screen;
  GdkDisplay *display;

  x11_screen = GDK_X11_SCREEN (screen);
  display = x11_screen->display;

  if (!G_LIKELY (GDK_X11_DISPLAY (display)->trusted_client))
    return x11_screen->window_manager_name;

  fetch_net_wm_check_window (screen);

  if (x11_screen->need_refetch_wm_name)
    {
      /* Get the name of the window manager */
      x11_screen->need_refetch_wm_name = FALSE;

      g_free (x11_screen->window_manager_name);
      x11_screen->window_manager_name = g_strdup ("unknown");

      if (x11_screen->wmspec_check_window != None)
        {
          Atom type;
          gint format;
          gulong n_items;
          gulong bytes_after;
          gchar *name;

          name = NULL;

          gdk_x11_display_error_trap_push (display);

          XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                              x11_screen->wmspec_check_window,
                              gdk_x11_get_xatom_by_name_for_display (display,
                                                                     "_NET_WM_NAME"),
                              0, G_MAXLONG, False,
                              gdk_x11_get_xatom_by_name_for_display (display,
                                                                     "UTF8_STRING"),
                              &type, &format,
                              &n_items, &bytes_after,
                              (guchar **)&name);

          gdk_x11_display_error_trap_pop_ignored (display);

          if (name != NULL)
            {
              g_free (x11_screen->window_manager_name);
              x11_screen->window_manager_name = g_strdup (name);
              XFree (name);
            }
        }
    }

  return GDK_X11_SCREEN (screen)->window_manager_name;
}

static void
gdk_x11_screen_class_init (GdkX11ScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_x11_screen_dispose;
  object_class->finalize = gdk_x11_screen_finalize;

  screen_class->get_display = gdk_x11_screen_get_display;
  screen_class->get_width = gdk_x11_screen_get_width;
  screen_class->get_height = gdk_x11_screen_get_height;
  screen_class->get_width_mm = gdk_x11_screen_get_width_mm;
  screen_class->get_height_mm = gdk_x11_screen_get_height_mm;
  screen_class->get_number = gdk_x11_screen_get_number;
  screen_class->get_root_window = gdk_x11_screen_get_root_window;
  screen_class->get_n_monitors = gdk_x11_screen_get_n_monitors;
  screen_class->get_primary_monitor = gdk_x11_screen_get_primary_monitor;
  screen_class->get_monitor_width_mm = gdk_x11_screen_get_monitor_width_mm;
  screen_class->get_monitor_height_mm = gdk_x11_screen_get_monitor_height_mm;
  screen_class->get_monitor_plug_name = gdk_x11_screen_get_monitor_plug_name;
  screen_class->get_monitor_geometry = gdk_x11_screen_get_monitor_geometry;
  screen_class->get_monitor_workarea = gdk_x11_screen_get_monitor_workarea;
  screen_class->get_monitor_scale_factor = gdk_x11_screen_get_monitor_scale_factor;
  screen_class->get_system_visual = _gdk_x11_screen_get_system_visual;
  screen_class->get_rgba_visual = gdk_x11_screen_get_rgba_visual;
  screen_class->is_composited = gdk_x11_screen_is_composited;
  screen_class->make_display_name = gdk_x11_screen_make_display_name;
  screen_class->get_active_window = gdk_x11_screen_get_active_window;
  screen_class->get_window_stack = gdk_x11_screen_get_window_stack;
  screen_class->get_setting = gdk_x11_screen_get_setting;
  screen_class->visual_get_best_depth = _gdk_x11_screen_visual_get_best_depth;
  screen_class->visual_get_best_type = _gdk_x11_screen_visual_get_best_type;
  screen_class->visual_get_best = _gdk_x11_screen_visual_get_best;
  screen_class->visual_get_best_with_depth = _gdk_x11_screen_visual_get_best_with_depth;
  screen_class->visual_get_best_with_type = _gdk_x11_screen_visual_get_best_with_type;
  screen_class->visual_get_best_with_both = _gdk_x11_screen_visual_get_best_with_both;
  screen_class->query_depths = _gdk_x11_screen_query_depths;
  screen_class->query_visual_types = _gdk_x11_screen_query_visual_types;
  screen_class->list_visuals = _gdk_x11_screen_list_visuals;

  signals[WINDOW_MANAGER_CHANGED] =
    g_signal_new (g_intern_static_string ("window-manager-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkX11ScreenClass, window_manager_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static guint32
get_netwm_cardinal_property (GdkScreen   *screen,
                             const gchar *name)
{
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  GdkAtom atom;
  guint32 prop = 0;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;

  atom = gdk_atom_intern_static_string (name);

  if (!gdk_x11_screen_supports_net_wm_hint (screen, atom))
    return 0;

  XGetWindowProperty (x11_screen->xdisplay,
                      x11_screen->xroot_window,
                      gdk_x11_get_xatom_by_name_for_display (GDK_SCREEN_DISPLAY (screen), name),
                      0, G_MAXLONG,
                      False, XA_CARDINAL, &type, &format, &nitems,
                      &bytes_after, &data);
  if (type == XA_CARDINAL)
    {
      prop = *(gulong *)data;
      XFree (data);
    }

  return prop;
}

/**
 * gdk_x11_screen_get_number_of_desktops:
 * @screen: (type GdkX11Screen): a #GdkScreen
 *
 * Returns the number of workspaces for @screen when running under a
 * window manager that supports multiple workspaces, as described
 * in the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 *
 * Returns: the number of workspaces, or 0 if workspaces are not supported
 *
 * Since: 3.10
 */
guint32
gdk_x11_screen_get_number_of_desktops (GdkScreen *screen)
{
  return get_netwm_cardinal_property (screen, "_NET_NUMBER_OF_DESKTOPS");
}

/**
 * gdk_x11_screen_get_current_desktop:
 * @screen: (type GdkX11Screen): a #GdkScreen
 *
 * Returns the current workspace for @screen when running under a
 * window manager that supports multiple workspaces, as described
 * in the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 *
 * Returns: the current workspace, or 0 if workspaces are not supported
 *
 * Since: 3.10
 */
guint32
gdk_x11_screen_get_current_desktop (GdkScreen *screen)
{
  return get_netwm_cardinal_property (screen, "_NET_CURRENT_DESKTOP");
}
