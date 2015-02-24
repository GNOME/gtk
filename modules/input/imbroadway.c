/*
 * gtkimmodulebroadway
 * Copyright (C) 2013 Alexander Larsson
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * $Id:$
 */

#include "config.h"
#include <string.h>

#include <gtk/gtk.h>
#include "gtk/gtkintl.h"
#include "gtk/gtkimmodule.h"

#include "gdk/broadway/gdkbroadway.h"

#define GTK_IM_CONTEXT_TYPE_BROADWAY (type_broadway)
#define GTK_IM_CONTEXT_BROADWAY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_IM_CONTEXT_TYPE_BROADWAY, GtkIMContextBroadway))
#define GTK_IM_CONTEXT_BROADWAY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_IM_CONTEXT_TYPE_BROADWAY, GtkIMContextBroadwayClass))

typedef struct _GtkIMContextBroadway
{
  GtkIMContextSimple parent;
  GdkWindow *client_window;
} GtkIMContextBroadway;

typedef struct _GtkIMContextBroadwayClass
{
  GtkIMContextSimpleClass parent_class;
} GtkIMContextBroadwayClass;

GType type_broadway = 0;
static GObjectClass *parent_class;

static const GtkIMContextInfo imbroadway_info =
{
  "broadway",      /* ID */
  NC_("input method menu", "Broadway"),      /* Human readable name */
  GETTEXT_PACKAGE, /* Translation domain */
  GTK_LOCALEDIR,   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  "",              /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] =
{
  &imbroadway_info,
};

#ifndef INCLUDE_IM_broadway
#define MODULE_ENTRY(type,function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_broadway_ ## function
#endif

static void
broadway_set_client_window (GtkIMContext *context, GdkWindow *window)
{
  GtkIMContextBroadway *bw = GTK_IM_CONTEXT_BROADWAY (context);

  bw->client_window = window;
}

static void
broadway_focus_in (GtkIMContext *context)
{
  GtkIMContextBroadway *bw = GTK_IM_CONTEXT_BROADWAY (context);
  GdkDisplay *display;

  if (bw->client_window)
    {
      display = gdk_window_get_display (bw->client_window);
      gdk_broadway_display_show_keyboard (GDK_BROADWAY_DISPLAY (display));
    }
}

static void
broadway_focus_out (GtkIMContext *context)
{
  GtkIMContextBroadway *bw = GTK_IM_CONTEXT_BROADWAY (context);
  GdkDisplay *display;

  if (bw->client_window)
    {
      display = gdk_window_get_display (bw->client_window);
      gdk_broadway_display_hide_keyboard (GDK_BROADWAY_DISPLAY (display));
    }
}

static void
gtk_im_context_broadway_class_init (GtkIMContextClass *klass)
{
  parent_class = g_type_class_peek_parent (klass);

  klass->focus_in = broadway_focus_in;
  klass->focus_out = broadway_focus_out;
  klass->set_client_window = broadway_set_client_window;
}

static void
gtk_im_context_broadway_init (GtkIMContext *im_context)
{
}

static void
gtk_im_context_broadway_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkIMContextBroadwayClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_im_context_broadway_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkIMContextBroadway),
    0,
    (GInstanceInitFunc) gtk_im_context_broadway_init,
  };

  type_broadway =
    g_type_module_register_type (module,
                                 GTK_TYPE_IM_CONTEXT_SIMPLE,
                                 "GtkIMContextBroadway",
                                 &object_info, 0);
}

MODULE_ENTRY (void, init) (GTypeModule * module)
{
  gtk_im_context_broadway_register_type (module);
}

MODULE_ENTRY (void, exit) (void)
{
}

MODULE_ENTRY (void, list) (const GtkIMContextInfo *** contexts, int *n_contexts)
{
  *contexts = info_list;
  *n_contexts = G_N_ELEMENTS (info_list);
}

MODULE_ENTRY (GtkIMContext *, create) (const gchar * context_id)
{
  if (!strcmp (context_id, "broadway"))
    return g_object_new (type_broadway, NULL);
  else
    return NULL;
}
