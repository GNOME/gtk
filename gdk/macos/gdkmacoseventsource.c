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

#include <errno.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach.h>
#include <mach/port.h>
#include <mach/port.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include "gdkinternals.h"

#include "gdkmacoseventsource-private.h"
#include "gdkmacosdisplay-private.h"

/* See https://daurnimator.com/post/147024385399/using-your-own-main-loop-on-osx
 * for more information on integrating Cocoa's CFRunLoop using an FD.
 */
extern mach_port_t _dispatch_get_main_queue_port_4CF (void);
extern void        _dispatch_main_queue_callback_4CF (void);

typedef struct _GdkMacosEventSource
{
  GSource source;
  GPollFD pfd;
} GdkMacosEventSource;

static gboolean
gdk_macos_event_source_check (GSource *base)
{
  GdkMacosEventSource *source = (GdkMacosEventSource *)base;
  return (source->pfd.revents & G_IO_IN) != 0;
}

static gboolean
gdk_macos_event_source_dispatch (GSource     *base,
                                 GSourceFunc  callback,
                                 gpointer     data)
{
  GdkMacosEventSource *source = (GdkMacosEventSource *)base;

  g_assert (source != NULL);

  source->pfd.revents = 0;
  _dispatch_main_queue_callback_4CF ();

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs macos_event_source_funcs = {
  .prepare = NULL,
  .check = gdk_macos_event_source_check,
  .dispatch = gdk_macos_event_source_dispatch,
  .finalize = NULL,
};

static int
gdk_macos_event_source_get_fd (void)
{
  int fd = kqueue ();

  if (fd != -1)
    {
      mach_port_t port;
      mach_port_t portset;

      fcntl (fd, F_SETFD, FD_CLOEXEC);
      port = _dispatch_get_main_queue_port_4CF ();

      if (KERN_SUCCESS == mach_port_allocate (mach_task_self (),
                                              MACH_PORT_RIGHT_PORT_SET,
                                              &portset))
        {
          struct kevent64_s event;

          EV_SET64 (&event, portset, EVFILT_MACHPORT, EV_ADD|EV_CLEAR, MACH_RCV_MSG, 0, 0, 0, 0);

          if (kevent64 (fd, &event, 1, NULL, 0, 0, &(struct timespec){0,0}) == 0)
            {
              if (KERN_SUCCESS == mach_port_insert_member (mach_task_self (), port, portset))
                return fd;
            }
        }

      close (fd);
    }

  return -1;
}

GSource *
_gdk_macos_event_source_new (void)
{
  GdkMacosEventSource *macos_source;
  GSource *source;
  int fd;

  if ((fd = gdk_macos_event_source_get_fd ()) == -1)
    return NULL;

  source = g_source_new (&macos_event_source_funcs, sizeof (GdkMacosEventSource));
  g_source_set_name (source, "GDK macOS Event Source");
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);

  macos_source = (GdkMacosEventSource *)source;
  macos_source->pfd.fd = fd;
  macos_source->pfd.events = G_IO_IN;
  macos_source->pfd.revents = 0;
  g_source_add_poll (source, &macos_source->pfd);

  return source;
}
