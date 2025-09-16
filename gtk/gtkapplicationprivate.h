/*
 * Copyright © 2011, 2013 Canonical Limited
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


#pragma once

#include "gtkapplicationwindow.h"
#include "gtkwindowprivate.h"
#include "gtkapplicationwindow.h"

#include "gtkactionmuxerprivate.h"
#include "gtkapplicationaccelsprivate.h"

G_BEGIN_DECLS

void                    gtk_application_window_set_id                   (GtkApplicationWindow     *window,
                                                                         guint                     id);
GActionGroup *          gtk_application_window_get_action_group         (GtkApplicationWindow     *window);
void                    gtk_application_handle_window_realize           (GtkApplication           *application,
                                                                         GtkWindow                *window);
void                    gtk_application_handle_window_map               (GtkApplication           *application,
                                                                         GtkWindow                *window);
GtkActionMuxer *        gtk_application_get_parent_muxer_for_window     (GtkWindow                *window);

GtkActionMuxer *        gtk_application_get_action_muxer                (GtkApplication           *application);
void                    gtk_application_insert_action_group             (GtkApplication           *application,
                                                                         const char               *name,
                                                                         GActionGroup             *action_group);

GtkApplicationAccels *  gtk_application_get_application_accels          (GtkApplication           *application);

void                    gtk_application_set_screensaver_active          (GtkApplication           *application,
                                                                         gboolean                  active);

gboolean                gtk_application_restore                         (GtkApplication           *application,
                                                                         GtkRestoreReason          reason);

void                    gtk_application_restore_window                  (GtkApplication           *application,
                                                                         GtkRestoreReason          reason,
                                                                         GVariant                 *app_state,
                                                                         GVariant                 *gtk_state);


#define GTK_TYPE_APPLICATION_IMPL                           (gtk_application_impl_get_type ())
#define GTK_APPLICATION_IMPL_CLASS(class)                   (G_TYPE_CHECK_CLASS_CAST ((class),                     \
                                                             GTK_TYPE_APPLICATION_IMPL,                            \
                                                             GtkApplicationImplClass))
#define GTK_APPLICATION_IMPL_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),                     \
                                                             GTK_TYPE_APPLICATION_IMPL,                            \
                                                             GtkApplicationImplClass))

typedef struct
{
  GObject parent_instance;
  GtkApplication *application;
  GdkDisplay *display;
} GtkApplicationImpl;

typedef struct
{
  GObjectClass parent_class;

  void        (* startup)                   (GtkApplicationImpl          *impl,
                                             gboolean                     support_save);
  void        (* shutdown)                  (GtkApplicationImpl          *impl);

  void        (* before_emit)               (GtkApplicationImpl          *impl,
                                             GVariant                    *platform_data);

  void        (* window_added)              (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window,
                                             GVariant                    *state);
  void        (* window_removed)            (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);
  void        (* active_window_changed)     (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);
  void        (* handle_window_realize)     (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);
  void        (* handle_window_map)         (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);

  void        (* set_app_menu)              (GtkApplicationImpl          *impl,
                                             GMenuModel                  *app_menu);
  void        (* set_menubar)               (GtkApplicationImpl          *impl,
                                             GMenuModel                  *menubar);

  guint       (* inhibit)                   (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window,
                                             GtkApplicationInhibitFlags   flags,
                                             const char                  *reason);
  void        (* uninhibit)                 (GtkApplicationImpl          *impl,
                                             guint                        cookie);
  gboolean    (* is_inhibited)              (GtkApplicationImpl          *impl,
                                             GtkApplicationInhibitFlags   flags);

  GtkRestoreReason
               (* get_restore_reason)       (GtkApplicationImpl          *impl);

  void         (* collect_global_state)     (GtkApplicationImpl          *impl,
                                             GVariantBuilder             *state);
  void         (* restore_global_state)     (GtkApplicationImpl          *impl,
                                             GVariant                    *state);
  void         (* collect_window_state)     (GtkApplicationImpl          *impl,
                                             GtkApplicationWindow        *window,
                                             GVariantBuilder             *state);
  void         (* store_state)              (GtkApplicationImpl          *impl,
                                             GVariant                    *state);
  void         (* forget_state)             (GtkApplicationImpl          *impl);
  GVariant *   (* retrieve_state)           (GtkApplicationImpl          *impl);
} GtkApplicationImplClass;

#define GTK_TYPE_APPLICATION_IMPL_DBUS                      (gtk_application_impl_dbus_get_type ())
#define GTK_APPLICATION_IMPL_DBUS_CLASS(class)              (G_TYPE_CHECK_CLASS_CAST ((class),                     \
                                                             GTK_TYPE_APPLICATION_IMPL_DBUS,                       \
                                                             GtkApplicationImplDBusClass))
#define GTK_APPLICATION_IMPL_DBUS_GET_CLASS(obj)            (G_TYPE_INSTANCE_GET_CLASS ((obj),                     \
                                                             GTK_TYPE_APPLICATION_IMPL_DBUS,                       \
                                                             GtkApplicationImplDBusClass))

typedef struct
{
  GtkApplicationImpl impl;

  GDBusConnection *session;
  GCancellable    *cancellable;

  const char      *application_id;
  const char      *unique_name;
  const char      *object_path;

  char            *app_menu_path;
  guint            app_menu_id;

  char            *menubar_path;
  guint            menubar_id;

  char            *instance_id;
  GtkRestoreReason reason;

  /* Portal support */
  GDBusProxy      *inhibit_proxy;
  GSList          *inhibit_handles;
  guint            state_changed_handler;
  char            *session_path;
  guint            session_state;
} GtkApplicationImplDBus;

typedef struct
{
  GtkApplicationImplClass parent_class;

} GtkApplicationImplDBusClass;

GType                   gtk_application_impl_get_type                   (void);
GType                   gtk_application_impl_dbus_get_type              (void);
GType                   gtk_application_impl_x11_get_type               (void);
GType                   gtk_application_impl_wayland_get_type           (void);
GType                   gtk_application_impl_quartz_get_type            (void);
GType                   gtk_application_impl_android_get_type           (void);

GtkApplicationImpl *    gtk_application_impl_new                        (GtkApplication              *application,
                                                                         GdkDisplay                  *display);
void                    gtk_application_impl_startup                    (GtkApplicationImpl          *impl,
                                                                         gboolean                     support_save);
void                    gtk_application_impl_shutdown                   (GtkApplicationImpl          *impl);
void                    gtk_application_impl_before_emit                (GtkApplicationImpl          *impl,
                                                                         GVariant                    *platform_data);
void                    gtk_application_impl_window_added               (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window,
                                                                         GVariant                    *state);
void                    gtk_application_impl_window_removed             (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
void                    gtk_application_impl_active_window_changed      (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
void                    gtk_application_impl_handle_window_realize      (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
void                    gtk_application_impl_handle_window_map          (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
void                    gtk_application_impl_set_app_menu               (GtkApplicationImpl          *impl,
                                                                         GMenuModel                  *app_menu);
void                    gtk_application_impl_set_menubar                (GtkApplicationImpl          *impl,
                                                                         GMenuModel                  *menubar);
guint                   gtk_application_impl_inhibit                    (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window,
                                                                         GtkApplicationInhibitFlags   flags,
                                                                         const char                  *reason);
void                    gtk_application_impl_uninhibit                  (GtkApplicationImpl          *impl,
                                                                         guint                        cookie);
gboolean                gtk_application_impl_is_inhibited               (GtkApplicationImpl          *impl,
                                                                         GtkApplicationInhibitFlags   flags);

char *                 gtk_application_impl_dbus_get_window_path       (GtkApplicationImplDBus      *dbus,
                                                                         GtkWindow                   *window);

void                    gtk_application_impl_quartz_setup_menu          (GMenuModel                  *model,
                                                                         GtkActionMuxer              *muxer);

GtkRestoreReason        gtk_application_impl_get_restore_reason         (GtkApplicationImpl          *impl);

void                    gtk_application_impl_collect_global_state       (GtkApplicationImpl          *impl,
                                                                         GVariantBuilder             *builder);
void                    gtk_application_impl_restore_global_state       (GtkApplicationImpl          *impl,
                                                                         GVariant                    *state);

void                    gtk_application_impl_collect_window_state       (GtkApplicationImpl          *impl,
                                                                         GtkApplicationWindow        *window,
                                                                         GVariantBuilder             *builder);

void                    gtk_application_impl_store_state                (GtkApplicationImpl          *impl,
                                                                         GVariant                    *state);
void                    gtk_application_impl_forget_state               (GtkApplicationImpl          *impl);
GVariant *              gtk_application_impl_retrieve_state             (GtkApplicationImpl          *impl);

GVariant *              gtk_application_impl_dbus_get_window_state      (GtkApplicationImplDBus *dbus,
                                                                         GtkWindow              *window);

G_END_DECLS

