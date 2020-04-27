/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <mach/mach.h>
#include <mach/port.h>

#include "gdkinternals.h"

#include "gdkmacoseventsource-private.h"
#include "gdkmacosdisplay-private.h"

/* See https://daurnimator.com/post/147024385399/using-your-own-main-loop-on-osx
 * for more information on integrating Cocoa's CFRunLoop using an FD.
 */
extern void _dispatch_main_queue_callback_4CF (void);

typedef struct _GdkMacosEventSource
{
  GSource          source;
  GPollFD          pfd;
  GdkMacosDisplay *display;
} GdkMacosEventSource;

static gboolean
gdk_macos_event_source_check (GSource *base)
{
  GdkMacosEventSource *source = (GdkMacosEventSource *)base;

  g_assert (source != NULL);
  g_assert (GDK_IS_MACOS_DISPLAY (source->display));

  return (source->pfd.revents & G_IO_IN) != 0;
}

static gboolean
gdk_macos_event_source_dispatch (GSource     *base,
                                 GSourceFunc  callback,
                                 gpointer     data)
{
  GdkMacosEventSource *source = (GdkMacosEventSource *)base;

  g_assert (source != NULL);
  g_assert (GDK_IS_MACOS_DISPLAY (source->display));

  _dispatch_main_queue_callback_4CF ();

  return G_SOURCE_CONTINUE;
}

static void
gdk_macos_event_source_finalize (GSource *base)
{
  GdkMacosEventSource *source = (GdkMacosEventSource *)base;
  source->display = NULL;
}

static GSourceFuncs macos_event_source_funcs = {
  .prepare = NULL,
  .check = gdk_macos_event_source_check,
  .dispatch = gdk_macos_event_source_dispatch,
  .finalize = gdk_macos_event_source_finalize,
};

GSource *
_gdk_macos_event_source_new (GdkMacosDisplay *display)
{
  GdkMacosEventSource *macos_source;
  GSource *source;
  gchar *name;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  source = g_source_new (&macos_event_source_funcs, sizeof (GdkMacosEventSource));

  macos_source = (GdkMacosEventSource *)source;
  macos_source->display = display;
  macos_source->pfd.fd = _gdk_macos_display_get_fd (display);
  macos_source->pfd.events = G_IO_IN;
  g_source_add_poll (source, &macos_source->pfd);

  name = g_strdup_printf ("GDK macOS Event Source (%s)",
                          gdk_display_get_name (GDK_DISPLAY (display)));

  g_source_set_name (source, name);
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  g_free (name);

  return source;
}
