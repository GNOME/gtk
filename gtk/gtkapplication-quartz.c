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
#include "gtkbuilder.h"
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

  GtkActionMuxer *muxer;
  GMenu *combined;

  GSList *inhibitors;
  gint quit_inhibit;
  guint next_cookie;
  NSObject *delegate;
} GtkApplicationImplQuartz;

G_DEFINE_TYPE (GtkApplicationImplQuartz, gtk_application_impl_quartz, GTK_TYPE_APPLICATION_IMPL)
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
@interface GtkApplicationQuartzDelegate : NSObject <NSApplicationDelegate>
#else
@interface GtkApplicationQuartzDelegate : NSObject
#endif
{
  GtkApplicationImplQuartz *quartz;
}

- (id)initWithImpl:(GtkApplicationImplQuartz*)impl;
- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication *)sender;
- (void)application:(NSApplication *)theApplication openFiles:(NSArray *)filenames;
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app;
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
  const gchar *quit_action_name = "quit";
  GActionGroup *action_group = G_ACTION_GROUP (quartz->impl.application);

  if (quartz->quit_inhibit != 0)
    return NSTerminateCancel;

  if (g_action_group_has_action (action_group, quit_action_name))
    {
      g_action_group_activate_action (action_group, quit_action_name, NULL);
      return NSTerminateCancel;
    }

  return NSTerminateNow;
}

-(void)application:(NSApplication *)theApplication openFiles:(NSArray *)filenames
{
  GFile **files;
  gint i;
  GApplicationFlags flags;

  flags = g_application_get_flags (G_APPLICATION (quartz->impl.application));

  if (~flags & G_APPLICATION_HANDLES_OPEN)
    {
      [theApplication replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
      return;
    }

  files = g_new (GFile *, [filenames count]);

  for (i = 0; i < [filenames count]; i++)
    files[i] = g_file_new_for_path ([(NSString *)[filenames objectAtIndex:i] UTF8String]);

  g_application_open (G_APPLICATION (quartz->impl.application), files, [filenames count], "");

  for (i = 0; i < [filenames count]; i++)
    g_object_unref (files[i]);

  g_free (files);

  [theApplication replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

-(BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app
{
  return YES;
}
@end

/* these exist only for accel handling */
static void
gtk_application_impl_quartz_hide (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
  [NSApp hide:NSApp];
}

static void
gtk_application_impl_quartz_hide_others (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       user_data)
{
  [NSApp hideOtherApplications:NSApp];
}

static void
gtk_application_impl_quartz_show_all (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
  [NSApp unhideAllApplications:NSApp];
}

static GActionEntry gtk_application_impl_quartz_actions[] = {
  { "hide",             gtk_application_impl_quartz_hide        },
  { "hide-others",      gtk_application_impl_quartz_hide_others },
  { "show-all",         gtk_application_impl_quartz_show_all    }
};

static void
gtk_application_impl_quartz_startup (GtkApplicationImpl *impl,
                                     gboolean            register_session)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;
  GSimpleActionGroup *gtkinternal;
  GMenuModel *app_menu;
  const gchar *pref_accel[] = {"<Primary>comma", NULL};
  const gchar *hide_others_accel[] = {"<Primary><Alt>h", NULL};
  const gchar *hide_accel[] = {"<Primary>h", NULL};
  const gchar *quit_accel[] = {"<Primary>q", NULL};

  if (register_session)
    {
      quartz->delegate = [[GtkApplicationQuartzDelegate alloc] initWithImpl:quartz];
      [NSApp setDelegate: (id)(quartz->delegate)];
    }

  quartz->muxer = gtk_action_muxer_new ();
  gtk_action_muxer_set_parent (quartz->muxer, gtk_application_get_action_muxer (impl->application));

  /* Add the default accels */
  gtk_application_set_accels_for_action (impl->application, "app.preferences", pref_accel);
  gtk_application_set_accels_for_action (impl->application, "gtkinternal.hide-others", hide_others_accel);
  gtk_application_set_accels_for_action (impl->application, "gtkinternal.hide", hide_accel);
  gtk_application_set_accels_for_action (impl->application, "app.quit", quit_accel);

  /* and put code behind the 'special' accels */
  gtkinternal = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (gtkinternal), gtk_application_impl_quartz_actions,
                                   G_N_ELEMENTS (gtk_application_impl_quartz_actions), quartz);
  gtk_application_insert_action_group (impl->application, "gtkinternal", G_ACTION_GROUP (gtkinternal));
  g_object_unref (gtkinternal);

  /* now setup the menu */
  app_menu = gtk_application_get_app_menu (impl->application);
  if (app_menu == NULL)
    {
      GtkBuilder *builder;

      /* If the user didn't fill in their own menu yet, add ours.
       *
       * The fact that we do this here ensures that we will always have the
       * app menu at index 0 in 'combined'.
       */
      builder = gtk_builder_new_from_resource ("/org/gtk/libgtk/ui/gtkapplication-quartz.ui");
      gtk_application_set_app_menu (impl->application, G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu")));
      g_object_unref (builder);
    }
  else
    gtk_application_impl_set_app_menu (impl, app_menu);

  /* This may or may not add an item to 'combined' */
  gtk_application_impl_set_menubar (impl, gtk_application_get_menubar (impl->application));

  /* OK.  Now put it in the menu. */
  gtk_application_impl_quartz_setup_menu (G_MENU_MODEL (quartz->combined), quartz->muxer);

  [NSApp finishLaunching];
}

static void
gtk_application_impl_quartz_shutdown (GtkApplicationImpl *impl)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  /* destroy our custom menubar */
  [NSApp setMainMenu:[[[NSMenu alloc] init] autorelease]];

  if (quartz->delegate)
    {
      [quartz->delegate release];
      quartz->delegate = NULL;
    }

  g_slist_free_full (quartz->inhibitors, (GDestroyNotify) gtk_application_quartz_inhibitor_free);
  quartz->inhibitors = NULL;
}

static void
gtk_application_impl_quartz_active_window_changed (GtkApplicationImpl *impl,
                                                   GtkWindow          *window)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  gtk_action_muxer_remove (quartz->muxer, "win");

  if (G_IS_ACTION_GROUP (window))
    gtk_action_muxer_insert (quartz->muxer, "win", G_ACTION_GROUP (window));
}

static void
gtk_application_impl_quartz_set_app_menu (GtkApplicationImpl *impl,
                                          GMenuModel         *app_menu)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  /* If there are any items at all, then the first one is the app menu */
  if (g_menu_model_get_n_items (G_MENU_MODEL (quartz->combined)))
    g_menu_remove (quartz->combined, 0);

  if (app_menu)
    g_menu_prepend_submenu (quartz->combined, "Application", app_menu);
  else
    {
      GMenu *empty;

      /* We must preserve the rule that index 0 is the app menu */
      empty = g_menu_new ();
      g_menu_prepend_submenu (quartz->combined, "Application", G_MENU_MODEL (empty));
      g_object_unref (empty);
    }
}

static void
gtk_application_impl_quartz_set_menubar (GtkApplicationImpl *impl,
                                         GMenuModel         *menubar)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  /* If we have the menubar, it is a section at index '1' */
  if (g_menu_model_get_n_items (G_MENU_MODEL (quartz->combined)) > 1)
    g_menu_remove (quartz->combined, 1);

  if (menubar)
    g_menu_append_section (quartz->combined, NULL, menubar);
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
  /* This is required so that Cocoa is not going to parse the
     command line arguments by itself and generate OpenFile events.
     We already parse the command line ourselves, so this is needed
     to prevent opening files twice, etc. */
  [[NSUserDefaults standardUserDefaults] setObject:@"NO"
                                            forKey:@"NSTreatUnknownArgumentsAsOpen"];

  quartz->combined = g_menu_new ();
}

static void
gtk_application_impl_quartz_finalize (GObject *object)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) object;

  g_clear_object (&quartz->combined);

  G_OBJECT_CLASS (gtk_application_impl_quartz_parent_class)->finalize (object);
}

static void
gtk_application_impl_quartz_class_init (GtkApplicationImplClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  class->startup = gtk_application_impl_quartz_startup;
  class->shutdown = gtk_application_impl_quartz_shutdown;
  class->active_window_changed = gtk_application_impl_quartz_active_window_changed;
  class->set_app_menu = gtk_application_impl_quartz_set_app_menu;
  class->set_menubar = gtk_application_impl_quartz_set_menubar;
  class->inhibit = gtk_application_impl_quartz_inhibit;
  class->uninhibit = gtk_application_impl_quartz_uninhibit;
  class->is_inhibited = gtk_application_impl_quartz_is_inhibited;

  gobject_class->finalize = gtk_application_impl_quartz_finalize;
}
