/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplicationprivate.h"
#include "gtkmodelmenu-quartz.h"
#import <Cocoa/Cocoa.h>

typedef struct
{
  guint cookie;
  GtkApplicationInhibitFlags flags;
  char *reason;
  GtkWindow *window;
} GtkApplicationQuartzInhibitor;

static void
gtk_application_quartz_inhibitor_free (GtkApplicationQuartzInhibitor *inhibitor)
{
  g_free (inhibitor->reason);
  g_clear_object (&inhibitor->window);
  g_slice_free (GtkApplicationQuartzInhibitor, inhibitor);
}

typedef GtkApplicationImplClass GtkApplicationImplQuartzClass;

typedef struct
{
  GtkApplicationImpl impl;

  GSList *inhibitors;
  gint quit_inhibit;
  guint next_cookie;
  NSObject *delegate;
} GtkApplicationImplQuartz;

G_DEFINE_TYPE (GtkApplicationImplQuartz, gtk_application_impl_quartz, GTK_TYPE_APPLICATION_IMPL)

@interface GtkApplicationQuartzDelegate : NSObject
{
  GtkApplicationImplQuartz *quartz;
}

- (id)initWithImpl:(GtkApplicationImplQuartz*)impl;
- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication *)sender;
@end

@implementation GtkApplicationQuartzDelegate
-(id)initWithImpl:(GtkApplicationImplQuartz*)impl
{
  [super init];
  quartz = impl;
  return self;
}

-(NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication *)sender
{
  /* We have no way to give our message other than to pop up a dialog
   * ourselves, which we should not do since the OS will already show
   * one when we return NSTerminateNow.
   *
   * Just let the OS show the generic message...
   */
  return quartz->quit_inhibit == 0 ? NSTerminateNow : NSTerminateCancel;
}
@end

static void
gtk_application_impl_quartz_menu_changed (GtkApplicationImplQuartz *quartz)
{
  GMenu *combined;

  combined = g_menu_new ();
  g_menu_append_submenu (combined, "Application", gtk_application_get_app_menu (quartz->impl.application));
  g_menu_append_section (combined, NULL, gtk_application_get_menubar (quartz->impl.application));

  gtk_quartz_set_main_menu (G_MENU_MODEL (combined), quartz->impl.application);

  g_object_unref (combined);
}

static void
gtk_application_impl_quartz_startup (GtkApplicationImpl *impl,
                                     gboolean            register_session)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  if (register_session)
    {
      quartz->delegate = [[GtkApplicationQuartzDelegate alloc] initWithImpl:quartz];
      [NSApp setDelegate: quartz->delegate];
    }

  gtk_application_impl_quartz_menu_changed (quartz);

  [NSApp finishLaunching];
}

static void
gtk_application_impl_quartz_shutdown (GtkApplicationImpl *impl)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  gtk_quartz_clear_main_menu ();

  if (quartz->delegate)
    {
      [quartz->delegate release];
      quartz->delegate = NULL;
    }

  g_slist_free_full (quartz->inhibitors, (GDestroyNotify) gtk_application_quartz_inhibitor_free);
  quartz->inhibitors = NULL;
}

static void
gtk_application_impl_quartz_set_app_menu (GtkApplicationImpl *impl,
                                          GMenuModel         *app_menu)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  gtk_application_impl_quartz_menu_changed (quartz);
}

static void
gtk_application_impl_quartz_set_menubar (GtkApplicationImpl *impl,
                                         GMenuModel         *menubar)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  gtk_application_impl_quartz_menu_changed (quartz);
}

static guint
gtk_application_impl_quartz_inhibit (GtkApplicationImpl         *impl,
                                     GtkWindow                  *window,
                                     GtkApplicationInhibitFlags  flags,
                                     const gchar                *reason)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;
  GtkApplicationQuartzInhibitor *inhibitor;

  inhibitor = g_slice_new (GtkApplicationQuartzInhibitor);
  inhibitor->cookie = ++quartz->next_cookie;
  inhibitor->flags = flags;
  inhibitor->reason = g_strdup (reason);
  inhibitor->window = window ? g_object_ref (window) : NULL;

  quartz->inhibitors = g_slist_prepend (quartz->inhibitors, inhibitor);

  if (flags & GTK_APPLICATION_INHIBIT_LOGOUT)
    quartz->quit_inhibit++;

  return inhibitor->cookie;
}

static void
gtk_application_impl_quartz_uninhibit (GtkApplicationImpl *impl,
                                       guint               cookie)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;
  GSList *iter;

  for (iter = quartz->inhibitors; iter; iter = iter->next)
    {
      GtkApplicationQuartzInhibitor *inhibitor = iter->data;

      if (inhibitor->cookie == cookie)
        {
          if (inhibitor->flags & GTK_APPLICATION_INHIBIT_LOGOUT)
            quartz->quit_inhibit--;
          gtk_application_quartz_inhibitor_free (inhibitor);
          quartz->inhibitors = g_slist_delete_link (quartz->inhibitors, iter);
          return;
        }
    }

  g_warning ("Invalid inhibitor cookie");
}

static gboolean
gtk_application_impl_quartz_is_inhibited (GtkApplicationImpl         *impl,
                                          GtkApplicationInhibitFlags  flags)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  if (flags & GTK_APPLICATION_INHIBIT_LOGOUT)
    return quartz->quit_inhibit > 0;

  return FALSE;
}

static void
gtk_application_impl_quartz_init (GtkApplicationImplQuartz *quartz)
{
}

static void
gtk_application_impl_quartz_finalize (GObject *object)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) object;

  g_slist_free_full (quartz->inhibitors, (GDestroyNotify) gtk_application_quartz_inhibitor_free);

  G_OBJECT_CLASS (gtk_application_impl_quartz_parent_class)->finalize (object);
}

static void
gtk_application_impl_quartz_class_init (GtkApplicationImplClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  class->startup = gtk_application_impl_quartz_startup;
  class->shutdown = gtk_application_impl_quartz_shutdown;
  class->set_app_menu = gtk_application_impl_quartz_set_app_menu;
  class->set_menubar = gtk_application_impl_quartz_set_menubar;
  class->inhibit = gtk_application_impl_quartz_inhibit;
  class->uninhibit = gtk_application_impl_quartz_uninhibit;
  class->is_inhibited = gtk_application_impl_quartz_is_inhibited;

  gobject_class->finalize = gtk_application_impl_quartz_finalize;
}
