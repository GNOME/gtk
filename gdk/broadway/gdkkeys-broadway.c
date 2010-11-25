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

#include "gdkprivate-broadway.h"
#include "gdkinternals.h"
#include "gdkdisplay-broadway.h"
#include "gdkkeysyms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

typedef struct _GdkKeymapBroadway   GdkKeymapBroadway;
typedef struct _GdkKeymapClass GdkKeymapBroadwayClass;

#define GDK_TYPE_KEYMAP_BROADWAY          (gdk_keymap_broadway_get_type ())
#define GDK_KEYMAP_BROADWAY(object)       (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_KEYMAP_BROADWAY, GdkKeymapBroadway))
#define GDK_IS_KEYMAP_BROADWAY(object)    (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_KEYMAP_BROADWAY))

typedef struct _DirectionCacheEntry DirectionCacheEntry;

struct _GdkKeymapBroadway
{
  GdkKeymap     parent_instance;

};

#define KEYMAP_USE_XKB(keymap) GDK_DISPLAY_BROADWAY ((keymap)->display)->use_xkb
#define KEYMAP_XDISPLAY(keymap) GDK_DISPLAY_XDISPLAY ((keymap)->display)

static GType gdk_keymap_broadway_get_type   (void);
static void  gdk_keymap_broadway_class_init (GdkKeymapBroadwayClass *klass);
static void  gdk_keymap_broadway_init       (GdkKeymapBroadway      *keymap);
static void  gdk_keymap_broadway_finalize   (GObject           *object);

static GdkKeymapClass *parent_class = NULL;

static GType
gdk_keymap_broadway_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
	{
	  sizeof (GdkKeymapClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gdk_keymap_broadway_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (GdkKeymapBroadway),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) gdk_keymap_broadway_init,
	};
      
      object_type = g_type_register_static (GDK_TYPE_KEYMAP,
                                            g_intern_static_string ("GdkKeymapBroadway"),
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_keymap_broadway_class_init (GdkKeymapBroadwayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_keymap_broadway_finalize;
}

static void
gdk_keymap_broadway_init (GdkKeymapBroadway *keymap)
{
}

static void
gdk_keymap_broadway_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GdkKeymap*
gdk_keymap_get_for_display (GdkDisplay *display)
{
  GdkDisplayBroadway *display_broadway;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  display_broadway = GDK_DISPLAY_BROADWAY (display);

  if (!display_broadway->keymap)
    display_broadway->keymap = g_object_new (gdk_keymap_broadway_get_type (), NULL);

  display_broadway->keymap->display = display;

  return display_broadway->keymap;
}

PangoDirection
gdk_keymap_get_direction (GdkKeymap *keymap)
{
  return PANGO_DIRECTION_NEUTRAL;
}

gboolean
gdk_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  return FALSE;
}

gboolean
gdk_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

gboolean
gdk_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

gboolean
gdk_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                   guint          keyval,
                                   GdkKeymapKey **keys,
                                   gint          *n_keys)
{
  *n_keys = 0;
  return FALSE;
}

gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    gint          *n_entries)
{
  *n_entries = 0;
  return FALSE;
}

guint
gdk_keymap_lookup_key (GdkKeymap          *keymap,
                       const GdkKeymapKey *key)
{
  return 0;
}


gboolean
gdk_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                     guint            hardware_keycode,
                                     GdkModifierType  state,
                                     gint             group,
                                     guint           *keyval,
                                     gint            *effective_group,
                                     gint            *level,
                                     GdkModifierType *consumed_modifiers)
{
  return FALSE;
}


/* Key handling not part of the keymap */
gchar*
gdk_keyval_name (guint	      keyval)
{
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

  return "TODO";
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  g_return_val_if_fail (keyval_name != NULL, 0);

  return 0;
}

void
gdk_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
				  GdkModifierType *state)
{
}

gboolean
gdk_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
				  GdkModifierType *state)
{
  return FALSE;
}
