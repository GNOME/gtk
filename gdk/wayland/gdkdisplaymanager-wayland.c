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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkdisplaymanagerprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkprivate-wayland.h"

#include "gdkinternals.h"

#include <X11/extensions/XKBcommon.h>

typedef struct _GdkWaylandDisplayManager GdkWaylandDisplayManager;
typedef struct _GdkWaylandDisplayManagerClass GdkWaylandDisplayManagerClass;

#define GDK_TYPE_WAYLAND_DISPLAY_MANAGER              (gdk_wayland_display_manager_get_type())
#define GDK_WAYLAND_DISPLAY_MANAGER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_DISPLAY_MANAGER, GdkWaylandDisplayManager))
#define GDK_WAYLAND_DISPLAY_MANAGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_DISPLAY_MANAGER, GdkWaylandDisplayManagerClass))
#define GDK_IS_WAYLAND_DISPLAY_MANAGER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_DISPLAY_MANAGER))
#define GDK_IS_WAYLAND_DISPLAY_MANAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_DISPLAY_MANAGER))
#define GDK_WAYLAND_DISPLAY_MANAGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_DISPLAY_MANAGER, GdkWaylandDisplayManagerClass))

struct _GdkWaylandDisplayManager
{
  GdkDisplayManager parent;

  GdkDisplay *default_display;
  GSList *displays;
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
gdk_wayland_display_manager_atom_intern (GdkDisplayManager *manager,
					 const gchar       *atom_name,
					 gboolean           dup)
{
  return 0;
}

static gchar *
gdk_wayland_display_manager_get_atom_name (GdkDisplayManager *manager,
					   GdkAtom            atom)
{
  return 0;
}

static guint
gdk_wayland_display_manager_lookup_keyval (GdkDisplayManager *manager,
					   const gchar       *keyval_name)
{
  g_return_val_if_fail (keyval_name != NULL, 0);

  return xkb_string_to_keysym(keyval_name);
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

  xkb_keysym_to_string(keyval, buf, sizeof buf);

  return buf;
}

static void
gdk_wayland_display_manager_keyval_convert_case (GdkDisplayManager *manager,
						 guint              symbol,
						 guint             *lower,
						 guint             *upper)
{
  guint xlower = symbol;
  guint xupper = symbol;

  /* Check for directly encoded 24-bit UCS characters: */
  if ((symbol & 0xff000000) == 0x01000000)
    {
      if (lower)
	*lower = gdk_unicode_to_keyval (g_unichar_tolower (symbol & 0x00ffffff));
      if (upper)
	*upper = gdk_unicode_to_keyval (g_unichar_toupper (symbol & 0x00ffffff));
      return;
    }

  switch (symbol >> 8)
    {
    case 0: /* Latin 1 */
      if ((symbol >= GDK_KEY_A) && (symbol <= GDK_KEY_Z))
	xlower += (GDK_KEY_a - GDK_KEY_A);
      else if ((symbol >= GDK_KEY_a) && (symbol <= GDK_KEY_z))
	xupper -= (GDK_KEY_a - GDK_KEY_A);
      else if ((symbol >= GDK_KEY_Agrave) && (symbol <= GDK_KEY_Odiaeresis))
	xlower += (GDK_KEY_agrave - GDK_KEY_Agrave);
      else if ((symbol >= GDK_KEY_agrave) && (symbol <= GDK_KEY_odiaeresis))
	xupper -= (GDK_KEY_agrave - GDK_KEY_Agrave);
      else if ((symbol >= GDK_KEY_Ooblique) && (symbol <= GDK_KEY_Thorn))
	xlower += (GDK_KEY_oslash - GDK_KEY_Ooblique);
      else if ((symbol >= GDK_KEY_oslash) && (symbol <= GDK_KEY_thorn))
	xupper -= (GDK_KEY_oslash - GDK_KEY_Ooblique);
      break;

    case 1: /* Latin 2 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol == GDK_KEY_Aogonek)
	xlower = GDK_KEY_aogonek;
      else if (symbol >= GDK_KEY_Lstroke && symbol <= GDK_KEY_Sacute)
	xlower += (GDK_KEY_lstroke - GDK_KEY_Lstroke);
      else if (symbol >= GDK_KEY_Scaron && symbol <= GDK_KEY_Zacute)
	xlower += (GDK_KEY_scaron - GDK_KEY_Scaron);
      else if (symbol >= GDK_KEY_Zcaron && symbol <= GDK_KEY_Zabovedot)
	xlower += (GDK_KEY_zcaron - GDK_KEY_Zcaron);
      else if (symbol == GDK_KEY_aogonek)
	xupper = GDK_KEY_Aogonek;
      else if (symbol >= GDK_KEY_lstroke && symbol <= GDK_KEY_sacute)
	xupper -= (GDK_KEY_lstroke - GDK_KEY_Lstroke);
      else if (symbol >= GDK_KEY_scaron && symbol <= GDK_KEY_zacute)
	xupper -= (GDK_KEY_scaron - GDK_KEY_Scaron);
      else if (symbol >= GDK_KEY_zcaron && symbol <= GDK_KEY_zabovedot)
	xupper -= (GDK_KEY_zcaron - GDK_KEY_Zcaron);
      else if (symbol >= GDK_KEY_Racute && symbol <= GDK_KEY_Tcedilla)
	xlower += (GDK_KEY_racute - GDK_KEY_Racute);
      else if (symbol >= GDK_KEY_racute && symbol <= GDK_KEY_tcedilla)
	xupper -= (GDK_KEY_racute - GDK_KEY_Racute);
      break;

    case 2: /* Latin 3 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Hstroke && symbol <= GDK_KEY_Hcircumflex)
	xlower += (GDK_KEY_hstroke - GDK_KEY_Hstroke);
      else if (symbol >= GDK_KEY_Gbreve && symbol <= GDK_KEY_Jcircumflex)
	xlower += (GDK_KEY_gbreve - GDK_KEY_Gbreve);
      else if (symbol >= GDK_KEY_hstroke && symbol <= GDK_KEY_hcircumflex)
	xupper -= (GDK_KEY_hstroke - GDK_KEY_Hstroke);
      else if (symbol >= GDK_KEY_gbreve && symbol <= GDK_KEY_jcircumflex)
	xupper -= (GDK_KEY_gbreve - GDK_KEY_Gbreve);
      else if (symbol >= GDK_KEY_Cabovedot && symbol <= GDK_KEY_Scircumflex)
	xlower += (GDK_KEY_cabovedot - GDK_KEY_Cabovedot);
      else if (symbol >= GDK_KEY_cabovedot && symbol <= GDK_KEY_scircumflex)
	xupper -= (GDK_KEY_cabovedot - GDK_KEY_Cabovedot);
      break;

    case 3: /* Latin 4 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Rcedilla && symbol <= GDK_KEY_Tslash)
	xlower += (GDK_KEY_rcedilla - GDK_KEY_Rcedilla);
      else if (symbol >= GDK_KEY_rcedilla && symbol <= GDK_KEY_tslash)
	xupper -= (GDK_KEY_rcedilla - GDK_KEY_Rcedilla);
      else if (symbol == GDK_KEY_ENG)
	xlower = GDK_KEY_eng;
      else if (symbol == GDK_KEY_eng)
	xupper = GDK_KEY_ENG;
      else if (symbol >= GDK_KEY_Amacron && symbol <= GDK_KEY_Umacron)
	xlower += (GDK_KEY_amacron - GDK_KEY_Amacron);
      else if (symbol >= GDK_KEY_amacron && symbol <= GDK_KEY_umacron)
	xupper -= (GDK_KEY_amacron - GDK_KEY_Amacron);
      break;

    case 6: /* Cyrillic */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Serbian_DJE && symbol <= GDK_KEY_Serbian_DZE)
	xlower -= (GDK_KEY_Serbian_DJE - GDK_KEY_Serbian_dje);
      else if (symbol >= GDK_KEY_Serbian_dje && symbol <= GDK_KEY_Serbian_dze)
	xupper += (GDK_KEY_Serbian_DJE - GDK_KEY_Serbian_dje);
      else if (symbol >= GDK_KEY_Cyrillic_YU && symbol <= GDK_KEY_Cyrillic_HARDSIGN)
	xlower -= (GDK_KEY_Cyrillic_YU - GDK_KEY_Cyrillic_yu);
      else if (symbol >= GDK_KEY_Cyrillic_yu && symbol <= GDK_KEY_Cyrillic_hardsign)
	xupper += (GDK_KEY_Cyrillic_YU - GDK_KEY_Cyrillic_yu);
      break;

    case 7: /* Greek */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Greek_ALPHAaccent && symbol <= GDK_KEY_Greek_OMEGAaccent)
	xlower += (GDK_KEY_Greek_alphaaccent - GDK_KEY_Greek_ALPHAaccent);
      else if (symbol >= GDK_KEY_Greek_alphaaccent && symbol <= GDK_KEY_Greek_omegaaccent &&
	       symbol != GDK_KEY_Greek_iotaaccentdieresis &&
	       symbol != GDK_KEY_Greek_upsilonaccentdieresis)
	xupper -= (GDK_KEY_Greek_alphaaccent - GDK_KEY_Greek_ALPHAaccent);
      else if (symbol >= GDK_KEY_Greek_ALPHA && symbol <= GDK_KEY_Greek_OMEGA)
	xlower += (GDK_KEY_Greek_alpha - GDK_KEY_Greek_ALPHA);
      else if (symbol >= GDK_KEY_Greek_alpha && symbol <= GDK_KEY_Greek_omega &&
	       symbol != GDK_KEY_Greek_finalsmallsigma)
	xupper -= (GDK_KEY_Greek_alpha - GDK_KEY_Greek_ALPHA);
      break;
    }

  if (lower)
    *lower = xlower;
  if (upper)
    *upper = xupper;
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
  manager_class->keyval_convert_case = gdk_wayland_display_manager_keyval_convert_case;
}

static void
gdk_wayland_display_manager_init (GdkWaylandDisplayManager *manager)
{
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
