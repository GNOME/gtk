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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/vt.h>
#include <sys/kd.h>

#include "gdk.h"

#include "gdkprivate-fb.h"
#include "gdkinternals.h"

/* Private variable declarations
 */
static int gdk_initialized = 0;			    /* 1 if the library is initialized,
						     * 0 otherwise.
						     */

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"misc",	    GDK_DEBUG_MISC},
  {"events",	    GDK_DEBUG_EVENTS},
};

static const int gdk_ndebug_keys = sizeof(gdk_debug_keys)/sizeof(GDebugKey);

#endif /* G_ENABLE_DEBUG */

GdkArgDesc _gdk_windowing_args[] = {
  { NULL }
};

static const GScannerConfig     fb_modes_scanner_config =
{
  (
   " \t\n"
   )                    /* cset_skip_characters */,
  (
   G_CSET_a_2_z
   G_CSET_A_2_Z
   )                    /* cset_identifier_first */,
  (
   G_CSET_a_2_z
   "_-0123456789"
   G_CSET_A_2_Z
   )                    /* cset_identifier_nth */,
  ( "#\n" )             /* cpair_comment_single */,
  
  FALSE                  /* case_sensitive */,
  
  FALSE                 /* skip_comment_multi */,
  TRUE                  /* skip_comment_single */,
  FALSE                 /* scan_comment_multi */,
  TRUE                  /* scan_identifier */,
  TRUE                  /* scan_identifier_1char */,
  FALSE                 /* scan_identifier_NULL */,
  TRUE                  /* scan_symbols */,
  FALSE                 /* scan_binary */,
  FALSE                 /* scan_octal */,
  FALSE                  /* scan_float */,
  FALSE                  /* scan_hex */,
  FALSE                 /* scan_hex_dollar */,
  FALSE                 /* scan_string_sq */,
  TRUE                  /* scan_string_dq */,
  TRUE                  /* numbers_2_int */,
  FALSE                 /* int_2_float */,
  FALSE                 /* identifier_2_string */,
  TRUE                  /* char_2_token */,
  FALSE                 /* symbol_2_token */,
  FALSE                 /* scope_0_fallback */,
};

enum {
  FB_MODE,
  FB_ENDMODE,
  FB_GEOMETRY,
  FB_TIMINGS,
  FB_LACED,
  FB_HSYNC,
  FB_VSYNC,
  FB_CSYNC,
  FB_EXTSYNC,
  FB_DOUBLE
};

char *fb_modes_keywords[] =
{
  "mode",
  "endmode",
  "geometry",
  "timings",
  "laced",
  "hsync",
  "vsync",
  "csync",
  "extsync",
  "double"
};

static int
fb_modes_parse_mode (GScanner *scanner,
		     struct fb_var_screeninfo *modeinfo,
		     char *specified_modename)
{
  guint token;
  int keyword;
  int i;
  char *modename;
  int geometry[5];
  int timings[7];
  int vsync=0, hsync=0, csync=0, extsync=0, doublescan=0, laced=0;
  int found_geometry = 0;
  int found_timings = 0;
    
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_SYMBOL)
    return -1;
  
  keyword = GPOINTER_TO_INT (scanner->value.v_symbol);
  if (keyword != FB_MODE)
    return -1;

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return -1;

  modename = g_strdup (scanner->value.v_string);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_SYMBOL)
    {
      g_free (modename);
      return -1; /* Not a valid keyword */
    }
  keyword = GPOINTER_TO_INT (scanner->value.v_symbol);
  while ( keyword != FB_ENDMODE )
    {

      switch (GPOINTER_TO_INT (scanner->value.v_symbol))
	{
	case FB_GEOMETRY:
	  for (i=0;i<5;i++) {
	    token = g_scanner_get_next_token (scanner);
	    if (token != G_TOKEN_INT)
	      {
		g_free (modename);
		return -1; /* need a integer */
	      }
	    geometry[i] = scanner->value.v_int;
	  }
	  found_geometry = TRUE;
	  break;
	case FB_TIMINGS:
	  for (i=0;i<7;i++) {
	    token = g_scanner_get_next_token (scanner);
	    if (token != G_TOKEN_INT)
	      {
		g_free (modename);
		return -1; /* need a integer */
	      }
	    timings[i] = scanner->value.v_int;
	  }
	  found_timings = TRUE;
	  break;
	case FB_LACED:
	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_IDENTIFIER)
	      {
		g_free (modename);
		return -1;
	      }
	  if (g_strcasecmp (scanner->value.v_identifier, "true")==0)
	    laced = 1;
	  else if (g_strcasecmp (scanner->value.v_identifier, "false")==0)
	    laced = 0;
	  else
	    {
	      g_free (modename);
	      return -1;
	    }
	  break;
	case FB_EXTSYNC:
	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_IDENTIFIER)
	      {
		g_free (modename);
		return -1;
	      }
	  if (g_strcasecmp (scanner->value.v_identifier, "true")==0)
	    extsync = 1;
	  else if (g_strcasecmp (scanner->value.v_identifier, "false")==0)
	    extsync = 0;
	  else
	    {
	      g_free (modename);
	      return -1;
	    }
	  break;
	case FB_DOUBLE:
	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_IDENTIFIER)
	      {
		g_free (modename);
		return -1;
	      }
	  if (g_strcasecmp (scanner->value.v_identifier, "true")==0)
	    doublescan = 1;
	  else if (g_strcasecmp (scanner->value.v_identifier, "false")==0)
	    doublescan = 0;
	  else
	    {
	      g_free (modename);
	      return -1;
	    }
	  break;
	case FB_VSYNC:
	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_IDENTIFIER)
	      {
		g_free (modename);
		return -1;
	      }
	  if (g_strcasecmp (scanner->value.v_identifier, "high")==0)
	    vsync = 1;
	  else if (g_strcasecmp (scanner->value.v_identifier, "low")==0)
	    vsync = 0;
	  else
	    {
	      g_free (modename);
	      return -1;
	    }
	  break;
	case FB_HSYNC:
	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_IDENTIFIER)
	    {
	      g_free (modename);
	      return -1;
	    }
	  if (g_strcasecmp (scanner->value.v_identifier, "high")==0)
	    hsync = 1;
	  else if (g_strcasecmp (scanner->value.v_identifier, "low")==0)
	    hsync = 0;
	  else
	    {
	      g_free (modename);
	      return -1;
	    }
	  break;
	case FB_CSYNC:
	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_IDENTIFIER)
	    {
	      g_free (modename);
	      return -1;
	    }
	  if (g_strcasecmp (scanner->value.v_identifier, "high")==0)
	    csync = 1;
	  else if (g_strcasecmp (scanner->value.v_identifier, "low")==0)
	    csync = 0;
	  else
	    {
	      g_free (modename);
	      return -1;
	    }
	  break;
	}
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_SYMBOL)
	{
	  g_free (modename);
	  return -1; /* Not a valid keyword */
	}
      keyword = GPOINTER_TO_INT (scanner->value.v_symbol);
    }

  if (strcmp (modename, specified_modename)== 0) {
    if (!found_geometry)
      g_warning ("Geometry not specified");

    if (found_geometry)
      {
	modeinfo->xres = geometry[0];
	modeinfo->yres = geometry[1];
	modeinfo->xres_virtual = geometry[2];
	modeinfo->yres_virtual = geometry[3];
	modeinfo->bits_per_pixel = geometry[4];
      }
    
    if (!found_timings)
      g_warning ("Timing not specified");
    
    if (found_timings)
      {
	modeinfo->pixclock = timings[0];
	modeinfo->left_margin = timings[1];
	modeinfo->right_margin = timings[2];
	modeinfo->upper_margin = timings[3];
	modeinfo->lower_margin = timings[4];
	modeinfo->hsync_len = timings[5];
	modeinfo->vsync_len = timings[6];
	
	modeinfo->vmode = 0;
	if (laced)
	  modeinfo->vmode |= FB_VMODE_INTERLACED;
	if (doublescan)
	  modeinfo->vmode |= FB_VMODE_DOUBLE;
	  
	modeinfo->sync = 0;
	if (hsync)
	  modeinfo->sync |= FB_SYNC_HOR_HIGH_ACT;
	if (vsync)
	  modeinfo->sync |= FB_SYNC_VERT_HIGH_ACT;
      }

    g_free (modename);
    return 1;
  }
  
  g_free (modename);

  return 0;
}

static int
gdk_fb_setup_mode_from_name (struct fb_var_screeninfo *modeinfo,
			     char *modename)
{
  GScanner *scanner;
  char *filename;
  gint result;
  int fd, i;
  int retval;

  retval = 0;
  
  filename = "/etc/fb.modes";
  
  fd = open (filename, O_RDONLY);
  if (fd < 0)
    {
      g_warning ("Cannot read %s", filename);
      return retval;
    }
  
  scanner = g_scanner_new ((GScannerConfig *) &fb_modes_scanner_config);
  scanner->input_name = filename;

  for (i=0;i<sizeof(fb_modes_keywords)/sizeof(fb_modes_keywords[0]);i++)
    g_scanner_add_symbol (scanner, fb_modes_keywords[i], GINT_TO_POINTER (i));

  g_scanner_input_file (scanner, fd);
  
  while (1) {
    if (g_scanner_peek_next_token (scanner) == G_TOKEN_EOF) {
      break;
    } 
    result = fb_modes_parse_mode (scanner, modeinfo, modename);
      
    if (result < 0) {
      g_warning ("parse error in %s at line %d", filename, scanner->line);
      break;
    }
    if (result > 0)
      {
	retval = 1;
	break;
      }
  }
  
  g_scanner_destroy (scanner);
  
  close (fd);
  
  return retval;
}
  

static int
gdk_fb_set_mode (GdkFBDisplay *display)
{
  char *env, *end;
  int depth, height, width;
  gboolean changed;
  
  if (ioctl (display->fb_fd, FBIOGET_VSCREENINFO, &display->modeinfo) < 0)
    return -1;

  display->orig_modeinfo = display->modeinfo;

  changed = FALSE;
  
  env = getenv ("GDK_DISPLAY_MODE");
  if (env)
    {
      if (gdk_fb_setup_mode_from_name (&display->modeinfo, env))
	changed = TRUE;
      else
	g_warning ("Couldn't find mode named '%s'", env);
    }

  env = getenv ("GDK_DISPLAY_DEPTH");
  if (env)
    {
      depth = strtol (env, &end, 10);
      if (env != end)
	{
	  changed = TRUE;
	  display->modeinfo.bits_per_pixel = depth;
	}
    }
  
  env = getenv ("GDK_DISPLAY_WIDTH");
  if (env)
    {
      width = strtol (env, &end, 10);
      if (env != end)
	{
	  changed = TRUE;
	  display->modeinfo.xres = width;
	  display->modeinfo.xres_virtual = width;
	}
    }
    
  env = getenv ("GDK_DISPLAY_HEIGHT");
  if (env)
    {
      height = strtol (env, &end, 10);
      if (env != end)
	{
	  changed = TRUE;
	  display->modeinfo.yres = height;
	  display->modeinfo.yres_virtual = height;
	}
    }

  if (changed &&
      (ioctl (display->fb_fd, FBIOPUT_VSCREENINFO, &display->modeinfo) < 0))
    {
      g_warning ("Couldn't set specified mode");
      return -1;
    }
  
  if (ioctl (display->fb_fd, FBIOGET_FSCREENINFO, &display->sinfo) < 0)
    {
      g_warning ("Error getting fixed screen info");
      return -1;
    }
  return 0;
}

static GdkFBDisplay *
gdk_fb_display_new ()
{
  GdkFBDisplay *display;
  gchar *fb_filename;
  struct vt_stat vs;
  int vt, n;
  gchar *s, *send;
  char buf[32];

  display = g_new0 (GdkFBDisplay, 1);

  display->console_fd = open ("/dev/console", O_RDWR);
  if (display->console_fd < 0)
    {
      g_warning ("Can't open /dev/console");
      g_free (display);
      return NULL;
    }
  
  ioctl (display->console_fd, VT_GETSTATE, &vs);
  display->start_vt = vs.v_active;

  vt = display->start_vt;
  s = getenv("GDK_VT");
  if (s)
    {
      if (g_strcasecmp ("new", s)==0)
	{
	  n = ioctl (display->console_fd, VT_OPENQRY, &vt);
	  if (n < 0 || vt == -1)
	    g_error("Cannot allocate new VT");
	}
      else
	{
	  vt = strtol (s, &send, 10);
	  if (s==send)
	    {
	      g_warning ("Cannot parse GDK_VT");
	      vt = display->start_vt;
	    }
	}
      
    }

  display->vt = vt;
  
  /* Switch to the new VT */
  if (vt != display->start_vt)
    {
      ioctl (display->console_fd, VT_ACTIVATE, vt);
      ioctl (display->console_fd, VT_WAITACTIVE, vt);
    }
  
  /* Open the tty */
  g_snprintf (buf, sizeof(buf), "/dev/tty%d", vt);
  display->tty_fd = open (buf, O_RDWR|O_NONBLOCK);
  if (display->tty_fd < 0)
    {
      g_warning ("Can't open %s", buf);
      close (display->console_fd);
      g_free (display);
      return NULL;
    }

  /* Set controlling tty */
  ioctl (0, TIOCNOTTY, 0);
  ioctl (display->tty_fd, TIOCSCTTY, 0);
  
  fb_filename = gdk_get_display ();
  display->fb_fd = open (fb_filename, O_RDWR);
  if (display->fb_fd < 0)
    {
      g_warning ("Can't open %s", fb_filename);
      g_free (fb_filename);
      close (display->tty_fd);
      close (display->console_fd);
      g_free (display);
      return NULL;
    }
  g_free (fb_filename);

  if (gdk_fb_set_mode (display) < 0)
    {
      close (display->fb_fd);
      close (display->tty_fd);
      close (display->console_fd);
      g_free (display);
      return NULL;
    }

  /* Disable normal text on the console */
  ioctl (display->fb_fd, KDSETMODE, KD_GRAPHICS);

  ioctl (display->fb_fd, FBIOBLANK, 0);

  /* We used to use sinfo.smem_len, but that seemed to be broken in many cases */
  display->fb_mmap = mmap (NULL,
			   display->modeinfo.yres * display->sinfo.line_length,
			   PROT_READ|PROT_WRITE,
			   MAP_SHARED,
			   display->fb_fd,
			   0);
  g_assert (display->fb_mmap != MAP_FAILED);

  if (display->sinfo.visual == FB_VISUAL_TRUECOLOR)
    {
      display->red_byte = display->modeinfo.red.offset >> 3;
      display->green_byte = display->modeinfo.green.offset >> 3;
      display->blue_byte = display->modeinfo.blue.offset >> 3;
    }

#ifdef ENABLE_SHADOW_FB
  if (_gdk_fb_screen_angle % 2 == 0)
    {
      display->fb_width = display->modeinfo.xres;
      display->fb_height = display->modeinfo.yres;
    } 
  else
    {
      display->fb_width = display->modeinfo.yres;
      display->fb_height = display->modeinfo.xres;
    }
  display->fb_stride =
    display->fb_width * (display->modeinfo.bits_per_pixel / 8);
  display->fb_mem = g_malloc(display->fb_height * display->fb_stride);
#else
  display->fb_mem = display->fb_mmap;
  display->fb_width = display->modeinfo.xres;
  display->fb_height = display->modeinfo.yres;
  display->fb_stride = display->sinfo.line_length;
#endif

  return display;
}

static void
gdk_fb_display_destroy (GdkFBDisplay *display)
{
  /* Restore old framebuffer mode */
  ioctl (display->fb_fd, FBIOPUT_VSCREENINFO, &display->orig_modeinfo);
  
  /* Enable normal text on the console */
  ioctl (display->fb_fd, KDSETMODE, KD_TEXT);
  
  munmap (display->fb_mmap, display->modeinfo.yres * display->sinfo.line_length);
  close (display->fb_fd);

  ioctl (display->console_fd, VT_ACTIVATE, display->start_vt);
  ioctl (display->console_fd, VT_WAITACTIVE, display->start_vt);
  if (display->vt != display->start_vt)
    ioctl (display->console_fd, VT_DISALLOCATE, display->vt);
  
  close (display->tty_fd);
  close (display->console_fd);
  g_free (display);
}

gboolean
_gdk_windowing_init_check (int argc, char **argv)
{
  if (gdk_initialized)
    return TRUE;

  /* Create new session and become session leader */
  setsid();

  gdk_display = gdk_fb_display_new ();

  if (!gdk_display)
    return FALSE;

  gdk_shadow_fb_init ();
  
  if (!gdk_fb_keyboard_open ())
    {
      g_warning ("Failed to initialize keyboard");
      gdk_fb_display_destroy (gdk_display);
      gdk_display = NULL;
      return FALSE;
    }
  
  if (!gdk_fb_mouse_open ())
    {
      g_warning ("Failed to initialize mouse");
      gdk_fb_keyboard_close ();
      gdk_fb_display_destroy (gdk_display);
      gdk_display = NULL;
      return FALSE;
    }

  gdk_initialized = TRUE;

  gdk_selection_property = gdk_atom_intern ("GDK_SELECTION", FALSE);
  
  return TRUE;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_grab
 *
 *   Grabs the pointer to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "event_mask" masks only interesting events
 *   "confine_to" limits the cursor movement to the specified window
 *   "cursor" changes the cursor for the duration of the grab
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_pointer_ungrab
 *
 *--------------------------------------------------------------
 */

GdkGrabStatus
gdk_pointer_grab (GdkWindow *	  window,
		  gint		  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
  return gdk_fb_pointer_grab (window,
			      owner_events,
			      event_mask,
			      confine_to,
			      cursor,
			      time, FALSE);
}

static gboolean _gdk_fb_pointer_implicit_grab = FALSE;

GdkGrabStatus
gdk_fb_pointer_grab (GdkWindow *	  window,
		     gint		  owner_events,
		     GdkEventMask	  event_mask,
		     GdkWindow *	  confine_to,
		     GdkCursor *	  cursor,
		     guint32	          time,
		     gboolean             implicit_grab)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);

  if (_gdk_fb_pointer_grab_window)
    {
      if (implicit_grab && !_gdk_fb_pointer_implicit_grab)
	return GDK_GRAB_ALREADY_GRABBED;

      gdk_pointer_ungrab (time);
    }

  _gdk_fb_pointer_implicit_grab = implicit_grab;

  _gdk_fb_pointer_grab_window = gdk_window_ref (window);
  _gdk_fb_pointer_grab_window_events = owner_events ? NULL : _gdk_fb_pointer_grab_window;

  _gdk_fb_pointer_grab_confine = confine_to ? gdk_window_ref (confine_to) : NULL;
  _gdk_fb_pointer_grab_events = event_mask;
  _gdk_fb_pointer_grab_cursor = cursor ? gdk_cursor_ref (cursor) : NULL;

  if (cursor)
    gdk_fb_cursor_reset ();

  gdk_fb_window_send_crossing_events (window,
				      GDK_CROSSING_GRAB);  
  
  return GDK_GRAB_SUCCESS;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_ungrab
 *
 *   Releases any pointer grab
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_pointer_ungrab (guint32 time)
{
  gdk_fb_pointer_ungrab (time, FALSE);
}

void
gdk_fb_pointer_ungrab (guint32 time, gboolean implicit_grab)
{
  gboolean have_grab_cursor = _gdk_fb_pointer_grab_cursor && 1;
  GdkWindow *mousewin;
  
  if (!_gdk_fb_pointer_grab_window)
    return;

  if (implicit_grab && !_gdk_fb_pointer_implicit_grab)
    return;

  if (_gdk_fb_pointer_grab_confine)
    gdk_window_unref (_gdk_fb_pointer_grab_confine);
  _gdk_fb_pointer_grab_confine = NULL;

  if (_gdk_fb_pointer_grab_cursor)
    gdk_cursor_unref (_gdk_fb_pointer_grab_cursor);
  _gdk_fb_pointer_grab_cursor = NULL;

  if (have_grab_cursor)
    gdk_fb_cursor_reset ();

  mousewin = gdk_window_at_pointer (NULL, NULL);
  gdk_fb_window_send_crossing_events (mousewin,
				      GDK_CROSSING_UNGRAB);  
  
  if (_gdk_fb_pointer_grab_window)
    gdk_window_unref (_gdk_fb_pointer_grab_window);
  _gdk_fb_pointer_grab_window = NULL;
  _gdk_fb_pointer_grab_window_events = NULL;

  _gdk_fb_pointer_implicit_grab = FALSE;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_is_grabbed
 *
 *   Tell wether there is an active x pointer grab in effect
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_pointer_is_grabbed (void)
{
  return _gdk_fb_pointer_grab_window != NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_grab
 *
 *   Grabs the keyboard to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_keyboard_ungrab
 *
 *--------------------------------------------------------------
 */

GdkGrabStatus
gdk_keyboard_grab (GdkWindow *	   window,
		   gint		   owner_events,
		   guint32	   time)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (_gdk_fb_pointer_grab_window)
    gdk_keyboard_ungrab (time);

  if (!owner_events)
    _gdk_fb_keyboard_grab_window = gdk_window_ref (window);
  
  return GDK_GRAB_SUCCESS;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_ungrab
 *
 *   Releases any keyboard grab
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_keyboard_ungrab (guint32 time)
{
  if (_gdk_fb_keyboard_grab_window)
    gdk_window_unref (_gdk_fb_keyboard_grab_window);
  _gdk_fb_keyboard_grab_window = NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width
 *
 *   Return the width of the screen.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_width (void)
{
  return gdk_display->fb_width;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_height (void)
{
  return gdk_display->fb_height;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width_mm
 *
 *   Return the width of the screen in millimeters.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_width_mm (void)
{
  return 0.5 + gdk_screen_width () * (25.4 / 72.);
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen in millimeters.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_height_mm (void)
{
  return 0.5 + gdk_screen_height () * (25.4 / 72.);
}

/*
 *--------------------------------------------------------------
 * gdk_set_sm_client_id
 *
 *   Set the SM_CLIENT_ID property on the WM_CLIENT_LEADER window
 *   so that the window manager can save our state using the
 *   X11R6 ICCCM session management protocol. A NULL value should 
 *   be set following disconnection from the session manager to
 *   remove the SM_CLIENT_ID property.
 *
 * Arguments:
 * 
 *   "sm_client_id" specifies the client id assigned to us by the
 *   session manager or NULL to remove the property.
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_set_sm_client_id (const gchar* sm_client_id)
{
}

void
gdk_key_repeat_disable (void)
{
}

void
gdk_key_repeat_restore (void)
{
}


extern void keyboard_shutdown(void);

void
gdk_windowing_exit (void)
{

  gdk_fb_mouse_close ();
  
  gdk_fb_keyboard_close ();
  
  gdk_fb_display_destroy (gdk_display);
  
  gdk_display = NULL;
}

gchar*
gdk_keyval_name (guint	      keyval)
{
  return NULL;
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  return 0;
}

gchar *
gdk_get_display(void)
{
  gchar *s;

  s = getenv ("GDK_DISPLAY");
  if (s==0)
    s = "/dev/fb0";
  
  return g_strdup (s);
}


void
gdk_beep (void)
{
  static int pitch = 600, duration = 100;
  gulong arg;

  /* Thank you XFree86 */
  arg = ((1193190 / pitch) & 0xffff) |
    (((unsigned long)duration) << 16);

  ioctl (gdk_display->tty_fd, KDMKTONE, arg);
}

/* utils */
GdkEvent *
gdk_event_make (GdkWindow *window,
		GdkEventType type,
		gboolean append_to_queue)
{
  static const guint type_masks[] = {
    GDK_SUBSTRUCTURE_MASK, /* GDK_DELETE		= 0, */
    GDK_STRUCTURE_MASK, /* GDK_DESTROY		= 1, */
    GDK_EXPOSURE_MASK, /* GDK_EXPOSE		= 2, */
    GDK_POINTER_MOTION_MASK, /* GDK_MOTION_NOTIFY	= 3, */
    GDK_BUTTON_PRESS_MASK, /* GDK_BUTTON_PRESS	= 4, */
    GDK_BUTTON_PRESS_MASK, /* GDK_2BUTTON_PRESS	= 5, */
    GDK_BUTTON_PRESS_MASK, /* GDK_3BUTTON_PRESS	= 6, */
    GDK_BUTTON_RELEASE_MASK, /* GDK_BUTTON_RELEASE	= 7, */
    GDK_KEY_PRESS_MASK, /* GDK_KEY_PRESS	= 8, */
    GDK_KEY_RELEASE_MASK, /* GDK_KEY_RELEASE	= 9, */
    GDK_ENTER_NOTIFY_MASK, /* GDK_ENTER_NOTIFY	= 10, */
    GDK_LEAVE_NOTIFY_MASK, /* GDK_LEAVE_NOTIFY	= 11, */
    GDK_FOCUS_CHANGE_MASK, /* GDK_FOCUS_CHANGE	= 12, */
    GDK_STRUCTURE_MASK, /* GDK_CONFIGURE		= 13, */
    GDK_VISIBILITY_NOTIFY_MASK, /* GDK_MAP		= 14, */
    GDK_VISIBILITY_NOTIFY_MASK, /* GDK_UNMAP		= 15, */
    GDK_PROPERTY_CHANGE_MASK, /* GDK_PROPERTY_NOTIFY	= 16, */
    GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_CLEAR	= 17, */
    GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_REQUEST = 18, */
    GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_NOTIFY	= 19, */
    GDK_PROXIMITY_IN_MASK, /* GDK_PROXIMITY_IN	= 20, */
    GDK_PROXIMITY_OUT_MASK, /* GDK_PROXIMITY_OUT	= 21, */
    GDK_ALL_EVENTS_MASK, /* GDK_DRAG_ENTER        = 22, */
    GDK_ALL_EVENTS_MASK, /* GDK_DRAG_LEAVE        = 23, */
    GDK_ALL_EVENTS_MASK, /* GDK_DRAG_MOTION       = 24, */
    GDK_ALL_EVENTS_MASK, /* GDK_DRAG_STATUS       = 25, */
    GDK_ALL_EVENTS_MASK, /* GDK_DROP_START        = 26, */
    GDK_ALL_EVENTS_MASK, /* GDK_DROP_FINISHED     = 27, */
    GDK_ALL_EVENTS_MASK, /* GDK_CLIENT_EVENT	= 28, */
    GDK_VISIBILITY_NOTIFY_MASK, /* GDK_VISIBILITY_NOTIFY = 29, */
    GDK_EXPOSURE_MASK, /* GDK_NO_EXPOSE		= 30, */
    GDK_SCROLL_MASK /* GDK_SCROLL            = 31 */
  };
  guint evmask;

  evmask = GDK_WINDOW_IMPL_FBDATA(window)->event_mask;

  /* Bad hack to make sure that things work semi-properly with owner_events */
  if (_gdk_fb_pointer_grab_window)
    evmask |= _gdk_fb_pointer_grab_events;
  if (_gdk_fb_keyboard_grab_window)
    evmask |= _gdk_fb_keyboard_grab_events;

  if (evmask & GDK_BUTTON_MOTION_MASK)
    {
      evmask |= GDK_BUTTON1_MOTION_MASK|GDK_BUTTON2_MOTION_MASK|GDK_BUTTON3_MOTION_MASK;
    }

  if (evmask & (GDK_BUTTON1_MOTION_MASK|GDK_BUTTON2_MOTION_MASK|GDK_BUTTON3_MOTION_MASK))
    {
      GdkModifierType mask;

      gdk_fb_mouse_get_info (NULL, NULL, &mask);

      if (((mask & GDK_BUTTON1_MASK) && (evmask & GDK_BUTTON1_MOTION_MASK)) ||
	  ((mask & GDK_BUTTON2_MASK) && (evmask & GDK_BUTTON2_MOTION_MASK)) ||
	  ((mask & GDK_BUTTON3_MASK) && (evmask & GDK_BUTTON3_MOTION_MASK)))
	evmask |= GDK_POINTER_MOTION_MASK;
    }

  if (evmask & type_masks[type])
    {
      GdkEvent *event = gdk_event_new ();
      guint32 the_time;

      the_time = gdk_fb_get_time ();

      event->any.type = type;
      event->any.window = gdk_window_ref (window);
      event->any.send_event = FALSE;
      switch (type)
	{
	case GDK_MOTION_NOTIFY:
	  event->motion.time = the_time;
	  event->motion.axes = NULL;
	  break;
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	  event->button.time = the_time;
	  event->button.axes = NULL;
	  break;
	case GDK_KEY_PRESS:
	case GDK_KEY_RELEASE:
	  event->key.time = the_time;
	  break;
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
	  event->crossing.time = the_time;
	  break;

	case GDK_PROPERTY_NOTIFY:
	  event->property.time = the_time;
	  break;

	case GDK_SELECTION_CLEAR:
	case GDK_SELECTION_REQUEST:
	case GDK_SELECTION_NOTIFY:
	  event->selection.time = the_time;
	  break;
	case GDK_PROXIMITY_IN:
	case GDK_PROXIMITY_OUT:
	  event->proximity.time = the_time;
	  break;
	case GDK_DRAG_ENTER:
	case GDK_DRAG_LEAVE:
	case GDK_DRAG_MOTION:
	case GDK_DRAG_STATUS:
	case GDK_DROP_START:
	case GDK_DROP_FINISHED:
	  event->dnd.time = the_time;
	  break;

	case GDK_FOCUS_CHANGE:
	case GDK_CONFIGURE:
	case GDK_MAP:
	case GDK_UNMAP:
	case GDK_CLIENT_EVENT:
	case GDK_VISIBILITY_NOTIFY:
	case GDK_NO_EXPOSE:
	case GDK_SCROLL:
	case GDK_DELETE:
	case GDK_DESTROY:
	case GDK_EXPOSE:
	default:
	  break;
	}

      if (append_to_queue)
	gdk_event_queue_append (event);

      return event;
    }

  return NULL;
}

void
gdk_fb_set_rotation (GdkFBAngle angle)
{
  if (angle == _gdk_fb_screen_angle)
    return;
  
#ifdef ENABLE_SHADOW_FB
  if (gdk_display)
    {
      gdk_shadow_fb_stop_updates ();

      _gdk_fb_screen_angle = angle;

      if (angle % 2 == 0)
	{
	  gdk_display->fb_width = gdk_display->modeinfo.xres;
	  gdk_display->fb_height = gdk_display->modeinfo.yres;
	} 
      else
	{
	  gdk_display->fb_width = gdk_display->modeinfo.yres;
	  gdk_display->fb_height = gdk_display->modeinfo.xres;
	}
      gdk_display->fb_stride =
	gdk_display->fb_width * (gdk_display->modeinfo.bits_per_pixel / 8);
      
      gdk_fb_recompute_all();
      gdk_fb_redraw_all ();
    }
  else
    _gdk_fb_screen_angle = angle;
#else
  g_warning ("Screen rotation without shadow fb not supported.");
#endif
}

