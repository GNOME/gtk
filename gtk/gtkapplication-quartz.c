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
#include "gtkmessagedialog.h"
#include <glib/gi18n-lib.h>
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
} GtkApplicationImplQuartz;

G_DEFINE_TYPE (GtkApplicationImplQuartz, gtk_application_impl_quartz, GTK_TYPE_APPLICATION_IMPL)

/* OS X implementation copied from EggSMClient, but simplified since
 * it doesn't need to interact with the user.
 */

static gboolean
idle_will_quit (gpointer user_data)
{
  GtkApplicationImplQuartz *quartz = user_data;

  if (quartz->quit_inhibit == 0)
    g_application_quit (G_APPLICATION (quartz->impl.application));
  else
    {
      GtkApplicationQuartzInhibitor *inhibitor;
      GSList *iter;
      GtkWidget *dialog;

      for (iter = quartz->inhibitors; iter; iter = iter->next)
       {
         inhibitor = iter->data;
         if (inhibitor->flags & GTK_APPLICATION_INHIBIT_LOGOUT)
           break;
        }
      g_assert (inhibitor != NULL);

      dialog = gtk_message_dialog_new (inhibitor->window,
                                      GTK_DIALOG_MODAL,
                                      GTK_MESSAGE_ERROR,
                                      GTK_BUTTONS_OK,
                                      _("%s cannot quit at this time:\n\n%s"),
                                      g_get_application_name (),
                                      inhibitor->reason);
      g_signal_connect_swapped (dialog,
                                "response",
                                G_CALLBACK (gtk_widget_destroy),
                                dialog);
      gtk_widget_show_all (dialog);
    }

  return G_SOURCE_REMOVE;
}

static pascal OSErr
quit_requested (const AppleEvent *aevt,
                AppleEvent       *reply,
                long              refcon)
{
  GtkApplicationImplQuartz *quartz = GSIZE_TO_POINTER ((gsize)refcon);

  /* Don't emit the "quit" signal immediately, since we're
   * called from a weird point in the guts of gdkeventloop-quartz.c
   */
  g_idle_add_full (G_PRIORITY_DEFAULT, idle_will_quit, quartz, NULL);

  return quartz->quit_inhibit == 0 ? noErr : userCanceledErr;
}

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
    AEInstallEventHandler (kCoreEventClass, kAEQuitApplication,
                           NewAEEventHandlerUPP (quit_requested),
                           (long)GPOINTER_TO_SIZE (quartz), false);

  gtk_application_impl_quartz_menu_changed (quartz);

  [NSApp finishLaunching];
}

static void
gtk_application_impl_quartz_shutdown (GtkApplicationImpl *impl)
{
  GtkApplicationImplQuartz *quartz = (GtkApplicationImplQuartz *) impl;

  gtk_quartz_clear_main_menu ();

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
