/* GDK - The GIMP Drawing Kit
 * gdkdisplaymanager-win32.c
 *
 * Copyright 2010 Hans Breuer
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

#include "gdkwin32display.h"
#include "gdkwin32displaymanager.h"
#include "gdkprivate-win32.h"

#include "gdkdisplaymanagerprivate.h"
#include "gdkinternals.h"

struct _GdkWin32DisplayManager
{
  GdkDisplayManager parent_instance;
};

struct _GdkWin32DisplayManagerClass
{
  GdkDisplayManagerClass parent_instance;
};

G_DEFINE_TYPE (GdkWin32DisplayManager, gdk_win32_display_manager, GDK_TYPE_DISPLAY_MANAGER)

static GdkDisplay *
gdk_win32_display_manager_open_display (GdkDisplayManager *manager,
                                         const gchar       *name)
{
  return _gdk_win32_display_open (name);
}

static GSList *
gdk_win32_display_manager_list_displays (GdkDisplayManager *manager)
{
  return g_slist_append (NULL, gdk_display_get_default ());
}

static GdkDisplay *
gdk_win32_display_manager_get_default_display (GdkDisplayManager *manager)
{
  return _gdk_win32_display_open (NULL);
}

static void
gdk_win32_display_manager_set_default_display (GdkDisplayManager *manager,
                                               GdkDisplay        *display)
{
  g_assert (gdk_display_get_default () == display);
}

#include "../gdkkeynames.c"

static gchar *
gdk_win32_display_manager_get_keyval_name (GdkDisplayManager *manager,
                                           guint              keyval)
{
  return _gdk_keyval_name (keyval);
}

static guint
gdk_win32_display_manager_lookup_keyval (GdkDisplayManager *manager,
                                         const gchar       *name)
{
  return _gdk_keyval_from_name (name);
}

static void
gdk_win32_display_manager_init (GdkWin32DisplayManager *manager)
{
  static once = TRUE;
  /* relies on displaymanager being a singleton , but our init functions
   * call gtk_diplay_maanger_get() again */
  if (once)
    {
      once = FALSE;
      _gdk_win32_windowing_init ();
    }
}

static void
gdk_win32_display_manager_finalize (GObject *object)
{
  g_error ("A GdkWin32DisplayManager object was finalized. This should not happen");
  G_OBJECT_CLASS (gdk_win32_display_manager_parent_class)->finalize (object);
}

static void
gdk_win32_display_manager_class_init (GdkWin32DisplayManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayManagerClass *manager_class = GDK_DISPLAY_MANAGER_CLASS (class);

  object_class->finalize = gdk_win32_display_manager_finalize;

  manager_class->open_display = gdk_win32_display_manager_open_display;
  manager_class->list_displays = gdk_win32_display_manager_list_displays;
  manager_class->set_default_display = gdk_win32_display_manager_set_default_display;
  manager_class->get_default_display = gdk_win32_display_manager_get_default_display;
  manager_class->atom_intern = _gdk_win32_display_manager_atom_intern;
  manager_class->get_atom_name = _gdk_win32_display_manager_get_atom_name;
  manager_class->lookup_keyval = gdk_win32_display_manager_lookup_keyval;
  manager_class->get_keyval_name = gdk_win32_display_manager_get_keyval_name;
}
