/* GDK - The GIMP Drawing Kit
 * gdkdisplaymanager-wayland.c
 *
 * Copyright 2010 Red Hat, Inc.
 *
 * Author: Matthias clasen
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

#include "gdkdisplaymanagerprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkprivate-wayland.h"
#include "gdkwayland.h"
#include "gdkinternals.h"

#include <xkbcommon/xkbcommon.h>

struct _GdkWaylandDisplayManager
{
  GdkDisplayManager parent;

  GdkDisplay *default_display;
  GSList *displays;

  GHashTable *name_to_atoms;
  guint next_atom;
};

struct _GdkWaylandDisplayManagerClass
{
  GdkDisplayManagerClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandDisplayManager, gdk_wayland_display_manager, GDK_TYPE_DISPLAY_MANAGER)

static void
gdk_wayland_display_manager_finalize (GObject *object)
{
  g_error ("A GdkWaylandDisplayManager object was finalized. This should not happen");
  G_OBJECT_CLASS (gdk_wayland_display_manager_parent_class)->finalize (object);
}

static GdkDisplay *
gdk_wayland_display_manager_open_display (GdkDisplayManager *manager,
					  const gchar       *name)
{
  return _gdk_wayland_display_open (name);
}

static GSList *
gdk_wayland_display_manager_list_displays (GdkDisplayManager *manager)
{
  return g_slist_copy (GDK_WAYLAND_DISPLAY_MANAGER (manager)->displays);
}

static void
gdk_wayland_display_manager_set_default_display (GdkDisplayManager *manager,
						 GdkDisplay        *display)
{
  GDK_WAYLAND_DISPLAY_MANAGER (manager)->default_display = display;

  _gdk_wayland_display_make_default (display);
}

static GdkDisplay *
gdk_wayland_display_manager_get_default_display (GdkDisplayManager *manager)
{
  return GDK_WAYLAND_DISPLAY_MANAGER (manager)->default_display;
}

static GdkAtom
gdk_wayland_display_manager_atom_intern (GdkDisplayManager *manager_in,
					 const gchar       *atom_name,
					 gboolean           dup)
{
  GdkWaylandDisplayManager *manager = GDK_WAYLAND_DISPLAY_MANAGER (manager_in);
  GdkAtom atom;
  gpointer data;
  const gchar *atom_name_intern;

  atom_name_intern = g_intern_string (atom_name);
  data = g_hash_table_lookup (manager->name_to_atoms, atom_name_intern);

  if (data)
    {
      atom = GDK_POINTER_TO_ATOM (data);
      return atom;
    }

  atom = _GDK_MAKE_ATOM (manager->next_atom);

  g_hash_table_insert (manager->name_to_atoms,
                       (gchar *)atom_name_intern,
                       GDK_ATOM_TO_POINTER (atom));
  manager->next_atom++;

  return atom;
}

static gchar *
gdk_wayland_display_manager_get_atom_name (GdkDisplayManager *manager_in,
					   GdkAtom            atom)
{
  GdkWaylandDisplayManager *manager = GDK_WAYLAND_DISPLAY_MANAGER (manager_in);
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, manager->name_to_atoms);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (GDK_POINTER_TO_ATOM (value) == atom)
        return g_strdup (key);
    }

  return NULL;
}

static guint
gdk_wayland_display_manager_lookup_keyval (GdkDisplayManager *manager,
					   const gchar       *keyval_name)
{
  g_return_val_if_fail (keyval_name != NULL, 0);

  return xkb_keysym_from_name (keyval_name, 0);
}

static gchar *
gdk_wayland_display_manager_get_keyval_name (GdkDisplayManager *manager,
					     guint              keyval)
{
  static char buf[128];

  switch (keyval)
    {
    case GDK_KEY_Page_Up:
      return "Page_Up";
    case GDK_KEY_Page_Down:
      return "Page_Down";
    case GDK_KEY_KP_Page_Up:
      return "KP_Page_Up";
    case GDK_KEY_KP_Page_Down:
      return "KP_Page_Down";
    }

  xkb_keysym_get_name(keyval, buf, sizeof (buf));

  return buf;
}

static void
gdk_wayland_display_manager_class_init (GdkWaylandDisplayManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayManagerClass *manager_class = GDK_DISPLAY_MANAGER_CLASS (class);

  object_class->finalize = gdk_wayland_display_manager_finalize;

  manager_class->open_display = gdk_wayland_display_manager_open_display;
  manager_class->list_displays = gdk_wayland_display_manager_list_displays;
  manager_class->set_default_display = gdk_wayland_display_manager_set_default_display;
  manager_class->get_default_display = gdk_wayland_display_manager_get_default_display;
  manager_class->atom_intern = gdk_wayland_display_manager_atom_intern;
  manager_class->get_atom_name = gdk_wayland_display_manager_get_atom_name;
  manager_class->lookup_keyval = gdk_wayland_display_manager_lookup_keyval;
  manager_class->get_keyval_name = gdk_wayland_display_manager_get_keyval_name;
}

static struct {
  const gchar *name;
  guint atom_id;
} predefined_atoms[] = {
      { "NONE", 0 },
      { "PRIMARY", 1 },
      { "SECONDARY", 2 },
      { "ATOM", 4 },
      { "BITMAP", 5 },
      { "COLORMAP", 7 },
      { "DRAWABLE", 17 },
      { "INTEGER", 19 },
      { "PIXMAP", 20 },
      { "STRING", 31 },
      { "WINDOW", 33 },
      { "CLIPBOARD", 69 },
};

static void
gdk_wayland_display_manager_init (GdkWaylandDisplayManager *manager)
{
  gint i;

  manager->name_to_atoms = g_hash_table_new (NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (predefined_atoms); i++)
    {
      GdkAtom atom;
      const gchar *atom_name = predefined_atoms[i].name;

      atom = _GDK_MAKE_ATOM (predefined_atoms[i].atom_id);
      g_hash_table_insert (manager->name_to_atoms,
                           (gchar *)g_intern_static_string (atom_name),
                           GDK_ATOM_TO_POINTER (atom));
    }

  manager->next_atom =
    predefined_atoms[G_N_ELEMENTS (predefined_atoms) - 1].atom_id + 1;
}

void
_gdk_wayland_display_manager_add_display (GdkDisplayManager *manager,
					  GdkDisplay        *display)
{
  GdkWaylandDisplayManager *manager_wayland = GDK_WAYLAND_DISPLAY_MANAGER (manager);

  if (manager_wayland->displays == NULL)
    gdk_display_manager_set_default_display (manager, display);

  manager_wayland->displays = g_slist_prepend (manager_wayland->displays, display);
}

void
_gdk_wayland_display_manager_remove_display (GdkDisplayManager *manager,
					     GdkDisplay        *display)
{
  GdkWaylandDisplayManager *manager_wayland = GDK_WAYLAND_DISPLAY_MANAGER (manager);

  manager_wayland->displays = g_slist_remove (manager_wayland->displays, display);

  if (manager_wayland->default_display == display)
    {
      if (manager_wayland->displays)
        gdk_display_manager_set_default_display (manager, manager_wayland->displays->data);
      else
        gdk_display_manager_set_default_display (manager, NULL);
    }
}
