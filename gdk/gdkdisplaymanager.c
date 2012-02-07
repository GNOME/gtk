/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "gdkconfig.h"
#include "gdkdisplaymanagerprivate.h"
#include "gdkinternals.h"
#include "gdkkeysprivate.h"
#include "gdkmarshalers.h"
#include "gdkintl.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
/* We immediately include gdkquartzdisplaymanager.h here instead of
 * gdkquartz.h so that we do not have to enable -xobjective-c for the
 * "generic" GDK source code.
 */
#include "quartz/gdkquartzdisplaymanager.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadwaydisplaymanager.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

/**
 * SECTION:gdkdisplaymanager
 * @Short_description: Maintains a list of all open GdkDisplays
 * @Title: GdkDisplayManager
 *
 * The purpose of the #GdkDisplayManager singleton object is to offer
 * notification when displays appear or disappear or the default display
 * changes.
 *
 * You can use gdk_display_manager_get() to obtain the GdkDisplayManager
 * singleton, but that should be rarely necessary. Typically, initializing
 * GTK+ opens a display that you can work with without ever accessing the
 * GdkDisplayManager.
 *
 * The GDK library can be built with support for multiple backends.
 * The GdkDisplayManager object determines which backend is used
 * at runtime.
 *
 * When writing backend-specific code that is supposed to work with
 * multiple GDK backends, you have to consider both compile time and
 * runtime. At compile time, use the #GDK_WINDOWING_X11, #GDK_WINDOWING_WIN32
 * macros, etc. to find out which backends are present in the GDK library
 * you are building your application against. At runtime, use type-check
 * macros like GDK_IS_X11_DISPLAY() to find out which backend is in use:
 *
 * <example id="backend-specific">
 * <title>Backend-specific code</title>
 * <programlisting>
 * #ifdef GDK_WINDOWING_X11
 *   if (GDK_IS_X11_DISPLAY (display))
 *     {
 *       /&ast; make X11-specific calls here &ast;/
 *     }
 *   else
 * #endif
 * #ifdef GDK_WINDOWING_QUARTZ
 *   if (GDK_IS_QUARTZ_DISPLAY (display))
 *     {
 *       /&ast; make Quartz-specific calls here &ast;/
*     }
 *   else
 * #endif
 *   g_error ("Unsupported GDK backend");
 * </programlisting>
 * </example>
 */


enum {
  PROP_0,
  PROP_DEFAULT_DISPLAY
};

enum {
  DISPLAY_OPENED,
  LAST_SIGNAL
};

static void gdk_display_manager_class_init   (GdkDisplayManagerClass *klass);
static void gdk_display_manager_set_property (GObject                *object,
                                              guint                   prop_id,
                                              const GValue           *value,
                                              GParamSpec             *pspec);
static void gdk_display_manager_get_property (GObject                *object,
                                              guint                   prop_id,
                                              GValue                 *value,
                                              GParamSpec             *pspec);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkDisplayManager, gdk_display_manager, G_TYPE_OBJECT)

static void
gdk_display_manager_class_init (GdkDisplayManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_display_manager_set_property;
  object_class->get_property = gdk_display_manager_get_property;

  klass->keyval_convert_case = _gdk_display_manager_real_keyval_convert_case;

  /**
   * GdkDisplayManager::display-opened:
   * @manager: the object on which the signal is emitted
   * @display: the opened display
   *
   * The ::display-opened signal is emitted when a display is opened.
   *
   * Since: 2.2
   */
  signals[DISPLAY_OPENED] =
    g_signal_new (g_intern_static_string ("display-opened"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDisplayManagerClass, display_opened),
                  NULL, NULL,
                  _gdk_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  GDK_TYPE_DISPLAY);

  g_object_class_install_property (object_class,
                                   PROP_DEFAULT_DISPLAY,
                                   g_param_spec_object ("default-display",
                                                        P_("Default Display"),
                                                        P_("The default display for GDK"),
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE|G_PARAM_STATIC_NAME|
                                                        G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

static void
gdk_display_manager_init (GdkDisplayManager *manager)
{
}

static void
gdk_display_manager_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DEFAULT_DISPLAY:
      gdk_display_manager_set_default_display (GDK_DISPLAY_MANAGER (object),
                                               g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_display_manager_get_property (GObject      *object,
                                  guint         prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DEFAULT_DISPLAY:
      g_value_set_object (value,
                          gdk_display_manager_get_default_display (GDK_DISPLAY_MANAGER (object)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_display_manager_get:
 *
 * Gets the singleton #GdkDisplayManager object.
 *
 * When called for the first time, this function consults the
 * <envar>GDK_BACKEND</envar> environment variable to find out which
 * of the supported GDK backends to use (in case GDK has been compiled
 * with multiple backends).
 *
 * Returns: (transfer none): The global #GdkDisplayManager singleton;
 *     gdk_parse_args(), gdk_init(), or gdk_init_check() must have
 *     been called first.
 *
 * Since: 2.2
 **/
GdkDisplayManager*
gdk_display_manager_get (void)
{
  static GdkDisplayManager *manager = NULL;

  if (!manager)
    {
      const gchar *backend;

      backend = g_getenv ("GDK_BACKEND");
#ifdef GDK_WINDOWING_QUARTZ
      if (backend == NULL || strcmp (backend, "quartz") == 0)
        manager = g_object_new (gdk_quartz_display_manager_get_type (), NULL);
      else
#endif
#ifdef GDK_WINDOWING_WIN32
      if (backend == NULL || strcmp (backend, "win32") == 0)
        manager = g_object_new (gdk_win32_display_manager_get_type (), NULL);
      else
#endif
#ifdef GDK_WINDOWING_X11
      if (backend == NULL || strcmp (backend, "x11") == 0)
        manager = g_object_new (gdk_x11_display_manager_get_type (), NULL);
      else
#endif
#ifdef GDK_WINDOWING_WAYLAND
      if (backend == NULL || strcmp (backend, "wayland") == 0)
        manager = g_object_new (gdk_wayland_display_manager_get_type (), NULL);
      else
#endif
#ifdef GDK_WINDOWING_BROADWAY
      if (backend == NULL || strcmp (backend, "broadway") == 0)
        manager = g_object_new (gdk_broadway_display_manager_get_type (), NULL);
      else
#endif
      if (backend != NULL)
        g_error ("Unsupported GDK backend: %s", backend);
      else
        g_error ("No GDK backend found");
    }

  return manager;
}

/**
 * gdk_display_manager_get_default_display:
 * @manager: a #GdkDisplayManager
 *
 * Gets the default #GdkDisplay.
 *
 * Returns: (transfer none): a #GdkDisplay, or %NULL
 *     if there is no default display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_manager_get_default_display (GdkDisplayManager *manager)
{
  return GDK_DISPLAY_MANAGER_GET_CLASS (manager)->get_default_display (manager);
}

/**
 * gdk_display_get_default:
 *
 * Gets the default #GdkDisplay. This is a convenience
 * function for
 * <literal>gdk_display_manager_get_default_display (gdk_display_manager_get ())</literal>.
 *
 * Returns: (transfer none): a #GdkDisplay, or %NULL if there is no default
 *   display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_get_default (void)
{
  return gdk_display_manager_get_default_display (gdk_display_manager_get ());
}

/**
 * gdk_screen_get_default:
 *
 * Gets the default screen for the default display. (See
 * gdk_display_get_default ()).
 *
 * Returns: (transfer none): a #GdkScreen, or %NULL if there is no default display.
 *
 * Since: 2.2
 */
GdkScreen *
gdk_screen_get_default (void)
{
  GdkDisplay *default_display;

  default_display = gdk_display_get_default ();

  if (default_display)
    return gdk_display_get_default_screen (default_display);
  else
    return NULL;
}

/**
 * gdk_display_manager_set_default_display:
 * @manager: a #GdkDisplayManager
 * @display: a #GdkDisplay
 * 
 * Sets @display as the default display.
 *
 * Since: 2.2
 **/
void
gdk_display_manager_set_default_display (GdkDisplayManager *manager,
                                         GdkDisplay        *display)
{
  GDK_DISPLAY_MANAGER_GET_CLASS (manager)->set_default_display (manager, display);

  g_object_notify (G_OBJECT (manager), "default-display");
}

/**
 * gdk_display_manager_list_displays:
 * @manager: a #GdkDisplayManager
 *
 * List all currently open displays.
 *
 * Return value: (transfer container) (element-type GdkDisplay): a newly
 *     allocated #GSList of #GdkDisplay objects. Free with g_slist_free()
 *     when you are done with it.
 *
 * Since: 2.2
 **/
GSList *
gdk_display_manager_list_displays (GdkDisplayManager *manager)
{
  return GDK_DISPLAY_MANAGER_GET_CLASS (manager)->list_displays (manager);
}

/**
 * gdk_display_manager_open_display:
 * @manager: a #GdkDisplayManager
 * @name: the name of the display to open
 *
 * Opens a display.
 *
 * Return value: (transfer none): a #GdkDisplay, or %NULL
 *     if the display could not be opened
 *
 * Since: 3.0
 */
GdkDisplay *
gdk_display_manager_open_display (GdkDisplayManager *manager,
                                  const gchar       *name)
{
  return GDK_DISPLAY_MANAGER_GET_CLASS (manager)->open_display (manager, name);
}

/**
 * gdk_atom_intern:
 * @atom_name: a string.
 * @only_if_exists: if %TRUE, GDK is allowed to not create a new atom, but
 *   just return %GDK_NONE if the requested atom doesn't already
 *   exists. Currently, the flag is ignored, since checking the
 *   existance of an atom is as expensive as creating it.
 *
 * Finds or creates an atom corresponding to a given string.
 *
 * Returns: (transfer none): the atom corresponding to @atom_name.
 */
GdkAtom
gdk_atom_intern (const gchar *atom_name,
                 gboolean     only_if_exists)
{
  GdkDisplayManager *manager = gdk_display_manager_get ();

  return GDK_DISPLAY_MANAGER_GET_CLASS (manager)->atom_intern (manager, atom_name, TRUE);
}

/**
 * gdk_atom_intern_static_string:
 * @atom_name: a static string
 *
 * Finds or creates an atom corresponding to a given string.
 *
 * Note that this function is identical to gdk_atom_intern() except
 * that if a new #GdkAtom is created the string itself is used rather
 * than a copy. This saves memory, but can only be used if the string
 * will <emphasis>always</emphasis> exist. It can be used with statically
 * allocated strings in the main program, but not with statically
 * allocated memory in dynamically loaded modules, if you expect to
 * ever unload the module again (e.g. do not use this function in
 * GTK+ theme engines).
 *
 * Returns: (transfer none): the atom corresponding to @atom_name
 *
 * Since: 2.10
 */
GdkAtom
gdk_atom_intern_static_string (const gchar *atom_name)
{
  GdkDisplayManager *manager = gdk_display_manager_get ();

  return GDK_DISPLAY_MANAGER_GET_CLASS (manager)->atom_intern (manager, atom_name, FALSE);
}

/**
 * gdk_atom_name:
 * @atom: a #GdkAtom.
 *
 * Determines the string corresponding to an atom.
 *
 * Returns: a newly-allocated string containing the string
 *   corresponding to @atom. When you are done with the
 *   return value, you should free it using g_free().
 */
gchar *
gdk_atom_name (GdkAtom atom)
{
  GdkDisplayManager *manager = gdk_display_manager_get ();

  return GDK_DISPLAY_MANAGER_GET_CLASS (manager)->get_atom_name (manager, atom);
}
