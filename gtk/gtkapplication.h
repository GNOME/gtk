/*
 * Copyright © 2010 Codethink Limited
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_APPLICATION_H__
#define __GTK_APPLICATION_H__

#include <gtk/gtkwidget.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_APPLICATION            (gtk_application_get_type ())
#define GTK_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_APPLICATION, GtkApplication))
#define GTK_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_APPLICATION, GtkApplicationClass))
#define GTK_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_APPLICATION))
#define GTK_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_APPLICATION))
#define GTK_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_APPLICATION, GtkApplicationClass))

typedef struct _GtkApplication        GtkApplication;
typedef struct _GtkApplicationClass   GtkApplicationClass;
typedef struct _GtkApplicationPrivate GtkApplicationPrivate;

struct _GtkApplication
{
  GApplication parent;

  /*< private >*/
  GtkApplicationPrivate *priv;
};

struct _GtkApplicationClass
{
  GApplicationClass parent_class;

  void (*window_added)   (GtkApplication *application,
                          GtkWindow      *window);
  void (*window_removed) (GtkApplication *application,
                          GtkWindow      *window);

  void (*quit)           (GtkApplication *application);

  /*< private >*/
  gpointer padding[11];
};

GType            gtk_application_get_type      (void) G_GNUC_CONST;

GtkApplication * gtk_application_new           (const gchar       *application_id,
                                                GApplicationFlags  flags);

void             gtk_application_add_window    (GtkApplication    *application,
                                                GtkWindow         *window);

void             gtk_application_remove_window (GtkApplication    *application,
                                                GtkWindow         *window);
GList *          gtk_application_get_windows   (GtkApplication    *application);

GMenuModel *     gtk_application_get_app_menu  (GtkApplication    *application);
void             gtk_application_set_app_menu  (GtkApplication    *application,
                                                GMenuModel        *app_menu);

GMenuModel *     gtk_application_get_menubar   (GtkApplication    *application);
void             gtk_application_set_menubar   (GtkApplication    *application,
                                                GMenuModel        *menubar);

void             gtk_application_add_accelerator    (GtkApplication  *application,
                                                     const gchar     *accelerator,
                                                     const gchar     *action_name,
                                                     GVariant        *parameter);
void             gtk_application_remove_accelerator (GtkApplication *application,
                                                     const gchar    *action_name,
                                                     GVariant       *parameter);

typedef enum
{
  GTK_APPLICATION_INHIBIT_LOGOUT  = (1 << 0),
  GTK_APPLICATION_INHIBIT_SWITCH  = (1 << 1),
  GTK_APPLICATION_INHIBIT_SUSPEND = (1 << 2),
  GTK_APPLICATION_INHIBIT_IDLE    = (1 << 3)
} GtkApplicationInhibitFlags;

guint            gtk_application_inhibit            (GtkApplication             *application,
                                                     GtkWindow                  *window,
                                                     GtkApplicationInhibitFlags  flags,
                                                     const gchar                *reason);
void             gtk_application_uninhibit          (GtkApplication             *application,
                                                     guint                       cookie);
gboolean         gtk_application_is_inhibited       (GtkApplication             *application,
                                                     GtkApplicationInhibitFlags  flags);

typedef enum {
  GTK_APPLICATION_LOGOUT,
  GTK_APPLICATION_REBOOT,
  GTK_APPLICATION_SHUTDOWN
} GtkApplicationEndSessionStyle;

gboolean         gtk_application_end_session        (GtkApplication                *application,
                                                     GtkApplicationEndSessionStyle  style,
                                                     gboolean                       request_confirmation);

G_END_DECLS

#endif /* __GTK_APPLICATION_H__ */

