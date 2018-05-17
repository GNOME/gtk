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

#include "gdkversionmacros.h"

#include "gdkinternals.h"
#include "gdkintl.h"

#include "gdkresources.h"

#include "gdk-private.h"

#ifndef HAVE_XCONVERTCASE
#include "gdkkeysyms.h"
#endif

#include <string.h>
#include <stdlib.h>


/**
 * SECTION:general
 * @Short_description: Library initialization and miscellaneous functions
 * @Title: General
 *
 * This section describes the GDK initialization functions and miscellaneous
 * utility functions, as well as deprecation facilities.
 *
 * The GDK and GTK+ headers annotate deprecated APIs in a way that produces
 * compiler warnings if these deprecated APIs are used. The warnings
 * can be turned off by defining the macro %GDK_DISABLE_DEPRECATION_WARNINGS
 * before including the glib.h header.
 *
 * GDK and GTK+ also provide support for building applications against
 * defined subsets of deprecated or new APIs. Define the macro
 * %GDK_VERSION_MIN_REQUIRED to specify up to what version
 * you want to receive warnings about deprecated APIs. Define the
 * macro %GDK_VERSION_MAX_ALLOWED to specify the newest version
 * whose API you want to use.
 */

/**
 * GDK_WINDOWING_X11:
 *
 * The #GDK_WINDOWING_X11 macro is defined if the X11 backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the X11 backend.
 */

/**
 * GDK_WINDOWING_WIN32:
 *
 * The #GDK_WINDOWING_WIN32 macro is defined if the Win32 backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Win32 backend.
 */

/**
 * GDK_WINDOWING_QUARTZ:
 *
 * The #GDK_WINDOWING_QUARTZ macro is defined if the Quartz backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Quartz backend.
 */

/**
 * GDK_WINDOWING_WAYLAND:
 *
 * The #GDK_WINDOWING_WAYLAND macro is defined if the Wayland backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Wayland backend.
 */

/**
 * GDK_DISABLE_DEPRECATION_WARNINGS:
 *
 * A macro that should be defined before including the gdk.h header.
 * If it is defined, no compiler warnings will be produced for uses
 * of deprecated GDK APIs.
 */

typedef struct _GdkPredicate  GdkPredicate;

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

typedef struct _GdkThreadsDispatch GdkThreadsDispatch;

struct _GdkThreadsDispatch
{
  GSourceFunc func;
  gpointer data;
  GDestroyNotify destroy;
};


/* Private variable declarations
 */
static int gdk_initialized = 0;                     /* 1 if the library is initialized,
                                                     * 0 otherwise.
                                                     */

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  { "misc",            GDK_DEBUG_MISC },
  { "events",          GDK_DEBUG_EVENTS },
  { "dnd",             GDK_DEBUG_DND },
  { "input",           GDK_DEBUG_INPUT },
  { "eventloop",       GDK_DEBUG_EVENTLOOP },
  { "frames",          GDK_DEBUG_FRAMES },
  { "settings",        GDK_DEBUG_SETTINGS },
  { "opengl",          GDK_DEBUG_OPENGL },
  { "vulkan",          GDK_DEBUG_VULKAN },
  { "selection",       GDK_DEBUG_SELECTION },
  { "clipboard",       GDK_DEBUG_CLIPBOARD },
  { "nograbs",         GDK_DEBUG_NOGRABS },
  { "gl-disable",      GDK_DEBUG_GL_DISABLE },
  { "gl-software",     GDK_DEBUG_GL_SOFTWARE },
  { "gl-texture-rect", GDK_DEBUG_GL_TEXTURE_RECT },
  { "gl-legacy",       GDK_DEBUG_GL_LEGACY },
  { "gl-gles",         GDK_DEBUG_GL_GLES },
  { "vulkan-disable",  GDK_DEBUG_VULKAN_DISABLE },
  { "vulkan-validate", GDK_DEBUG_VULKAN_VALIDATE }
};
#endif

static gpointer
register_resources (gpointer dummy G_GNUC_UNUSED)
{
  _gdk_register_resource ();

  return NULL;
}

static void
gdk_ensure_resources (void)
{
  static GOnce register_resources_once = G_ONCE_INIT;

  g_once (&register_resources_once, register_resources, NULL);
}

void
gdk_pre_parse (void)
{
  gdk_initialized = TRUE;

  gdk_ensure_resources ();

#ifdef G_ENABLE_DEBUG
  {
    gchar *debug_string = getenv("GDK_DEBUG");
    if (debug_string != NULL)
      _gdk_debug_flags = g_parse_debug_string (debug_string,
                                              (GDebugKey *) gdk_debug_keys,
                                              G_N_ELEMENTS (gdk_debug_keys));

    if (g_getenv ("GDK_TRACE"))
      gdk_profiler_start (-1);
  }
#endif  /* G_ENABLE_DEBUG */
}

/*< private >
 * gdk_display_open_default:
 *
 * Opens the default display specified by command line arguments or
 * environment variables, sets it as the default display, and returns
 * it. gdk_parse_args() must have been called first. If the default
 * display has previously been set, simply returns that. An internal
 * function that should not be used by applications.
 *
 * Returns: (nullable) (transfer none): the default display, if it
 *   could be opened, otherwise %NULL.
 */
GdkDisplay *
gdk_display_open_default (void)
{
  GdkDisplay *display;

  g_return_val_if_fail (gdk_initialized, NULL);

  display = gdk_display_get_default ();
  if (display)
    return display;

  display = gdk_display_open (NULL);

  return display;
}

/**
 * SECTION:threads
 * @Short_description: Functions for using GDK in multi-threaded programs
 * @Title: Threads
 *
 * For thread safety, GDK relies on the thread primitives in GLib,
 * and on the thread-safe GLib main loop.
 *
 * GLib is completely thread safe (all global data is automatically
 * locked), but individual data structure instances are not automatically
 * locked for performance reasons. So e.g. you must coordinate
 * accesses to the same #GHashTable from multiple threads.
 *
 * GTK+, however, is not thread safe. You should only use GTK+ and GDK
 * from the thread gtk_init() and gtk_main() were called on.
 * This is usually referred to as the “main thread”.
 *
 * Signals on GTK+ and GDK types, as well as non-signal callbacks, are
 * emitted in the main thread.
 *
 * You can schedule work in the main thread safely from other threads
 * by using g_main_context_invoke(), g_idle_add(), and g_timeout_add():
 *
 * |[<!-- language="C" -->
 * static void
 * worker_thread (void)
 * {
 *   ExpensiveData *expensive_data = do_expensive_computation ();
 *
 *   g_main_context_invoke (NULL, got_value, expensive_data);
 * }
 *
 * static gboolean
 * got_value (gpointer user_data)
 * {
 *   ExpensiveData *expensive_data = user_data;
 *
 *   my_app->expensive_data = expensive_data;
 *   gtk_button_set_sensitive (my_app->button, TRUE);
 *   gtk_button_set_label (my_app->button, expensive_data->result_label);
 *
 *   return G_SOURCE_REMOVE;
 * }
 * ]|
 *
 * For more information on this "worker thread" pattern, you should
 * also look at #GTask, which gives you high-level tools to perform
 * expensive tasks from worker threads, and will handle thread
 * management for you.
 */
