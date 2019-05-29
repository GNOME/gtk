/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_WINDOW_H__
#define __GTK_WINDOW_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkapplication.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GTK_TYPE_WINDOW			(gtk_window_get_type ())
#define GTK_WINDOW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_WINDOW, GtkWindow))
#define GTK_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_WINDOW, GtkWindowClass))
#define GTK_IS_WINDOW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_WINDOW))
#define GTK_IS_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WINDOW))
#define GTK_WINDOW_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WINDOW, GtkWindowClass))

typedef struct _GtkWindowClass        GtkWindowClass;
typedef struct _GtkWindowGeometryInfo GtkWindowGeometryInfo;
typedef struct _GtkWindowGroup        GtkWindowGroup;
typedef struct _GtkWindowGroupClass   GtkWindowGroupClass;
typedef struct _GtkWindowGroupPrivate GtkWindowGroupPrivate;

struct _GtkWindow
{
  GtkBin parent_instance;
};

/**
 * GtkWindowClass:
 * @parent_class: The parent class.
 * @activate_focus: Activates the current focused widget within the window.
 * @activate_default: Activates the default widget for the window.
 * @keys_changed: Signal gets emitted when the set of accelerators or
 *   mnemonics that are associated with window changes.
 * @enable_debugging: Class handler for the #GtkWindow::enable-debugging
 *   keybinding signal.
 * @close_request: Class handler for the #GtkWindow::close-request signal.
 */
struct _GtkWindowClass
{
  GtkBinClass parent_class;

  /*< public >*/

  /* G_SIGNAL_ACTION signals for keybindings */

  void     (* activate_focus)   (GtkWindow *window);
  void     (* activate_default) (GtkWindow *window);
  void	   (* keys_changed)     (GtkWindow *window);
  gboolean (* enable_debugging) (GtkWindow *window,
                                 gboolean   toggle);
  gboolean (* close_request)    (GtkWindow *window);

  /*< private >*/
  gpointer padding[8];
};

/**
 * GtkWindowType:
 * @GTK_WINDOW_TOPLEVEL: A regular window, such as a dialog.
 * @GTK_WINDOW_POPUP: A special window such as a tooltip.
 *
 * A #GtkWindow can be one of these types. Most things you’d consider a
 * “window” should have type #GTK_WINDOW_TOPLEVEL; windows with this type
 * are managed by the window manager and have a frame by default (call
 * gtk_window_set_decorated() to toggle the frame).  Windows with type
 * #GTK_WINDOW_POPUP are ignored by the window manager; window manager
 * keybindings won’t work on them, the window manager won’t decorate the
 * window with a frame, many GTK+ features that rely on the window
 * manager will not work (e.g. maximization/minimization).
 * #GTK_WINDOW_POPUP is used to implement
 * widgets such as #GtkMenu or tooltips that you normally don’t think of
 * as windows per se. Nearly all windows should be #GTK_WINDOW_TOPLEVEL.
 * In particular, do not use #GTK_WINDOW_POPUP just to turn off
 * the window borders; use gtk_window_set_decorated() for that.
 */
typedef enum
{
  GTK_WINDOW_TOPLEVEL,
  GTK_WINDOW_POPUP
} GtkWindowType;

GDK_AVAILABLE_IN_ALL
GType      gtk_window_get_type                 (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_window_new                      (GtkWindowType        type);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_title                (GtkWindow           *window,
						const gchar         *title);
GDK_AVAILABLE_IN_ALL
const gchar * gtk_window_get_title             (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_startup_id           (GtkWindow           *window,
                                                const gchar         *startup_id);
GDK_AVAILABLE_IN_ALL
void       gtk_window_add_accel_group          (GtkWindow           *window,
						GtkAccelGroup	    *accel_group);
GDK_AVAILABLE_IN_ALL
void       gtk_window_remove_accel_group       (GtkWindow           *window,
						GtkAccelGroup	    *accel_group);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_focus                (GtkWindow           *window,
						GtkWidget           *focus);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_window_get_focus                (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_default_widget       (GtkWindow           *window,
						GtkWidget           *default_widget);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_window_get_default_widget       (GtkWindow           *window);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_transient_for        (GtkWindow           *window,
						GtkWindow           *parent);
GDK_AVAILABLE_IN_ALL
GtkWindow *gtk_window_get_transient_for        (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_attached_to          (GtkWindow           *window,
                                                GtkWidget           *attach_widget);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_window_get_attached_to          (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_type_hint            (GtkWindow           *window,
						GdkSurfaceTypeHint    hint);
GDK_AVAILABLE_IN_ALL
GdkSurfaceTypeHint gtk_window_get_type_hint     (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_accept_focus         (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_accept_focus         (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_focus_on_map         (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_focus_on_map         (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_destroy_with_parent  (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_destroy_with_parent  (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_hide_on_close        (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_hide_on_close        (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_mnemonics_visible    (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_mnemonics_visible    (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_focus_visible        (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_focus_visible        (GtkWindow           *window);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_resizable            (GtkWindow           *window,
                                                gboolean             resizable);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_resizable            (GtkWindow           *window);

GDK_AVAILABLE_IN_ALL
void	   gtk_window_set_display              (GtkWindow	    *window,
						GdkDisplay          *display);

GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_is_active                (GtkWindow           *window);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_decorated            (GtkWindow *window,
                                                gboolean   setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_decorated            (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_deletable            (GtkWindow *window,
                                                gboolean   setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_deletable            (GtkWindow *window);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_icon_name                (GtkWindow   *window,
						    const gchar *name);
GDK_AVAILABLE_IN_ALL
const gchar * gtk_window_get_icon_name             (GtkWindow  *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_default_icon_name        (const gchar *name);
GDK_AVAILABLE_IN_ALL
const gchar * gtk_window_get_default_icon_name     (void);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_auto_startup_notification (gboolean setting);

/* If window is set modal, input will be grabbed when show and released when hide */
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_modal      (GtkWindow *window,
				      gboolean   modal);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_modal      (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
GListModel *gtk_window_get_toplevels (void);
GDK_AVAILABLE_IN_ALL
GList*     gtk_window_list_toplevels (void);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_has_user_ref_count (GtkWindow *window,
                                              gboolean   setting);

GDK_AVAILABLE_IN_ALL
void     gtk_window_add_mnemonic          (GtkWindow       *window,
					   guint            keyval,
					   GtkWidget       *target);
GDK_AVAILABLE_IN_ALL
void     gtk_window_remove_mnemonic       (GtkWindow       *window,
					   guint            keyval,
					   GtkWidget       *target);
GDK_AVAILABLE_IN_ALL
gboolean gtk_window_mnemonic_activate     (GtkWindow       *window,
					   guint            keyval,
					   GdkModifierType  modifier);
GDK_AVAILABLE_IN_ALL
void     gtk_window_set_mnemonic_modifier (GtkWindow       *window,
					   GdkModifierType  modifier);
GDK_AVAILABLE_IN_ALL
GdkModifierType gtk_window_get_mnemonic_modifier (GtkWindow *window);

GDK_AVAILABLE_IN_ALL
gboolean gtk_window_activate_key          (GtkWindow        *window,
					   GdkEventKey      *event);
GDK_AVAILABLE_IN_ALL
gboolean gtk_window_propagate_key_event   (GtkWindow        *window,
					   GdkEventKey      *event);

GDK_AVAILABLE_IN_ALL
void     gtk_window_present            (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_present_with_time  (GtkWindow *window,
				        guint32    timestamp);
GDK_AVAILABLE_IN_ALL
void     gtk_window_iconify       (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_deiconify     (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_stick         (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_unstick       (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_maximize      (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_unmaximize    (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_fullscreen    (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_unfullscreen  (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_fullscreen_on_monitor (GtkWindow  *window,
                                           GdkMonitor *monitor);
GDK_AVAILABLE_IN_ALL
void     gtk_window_close         (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_set_keep_above    (GtkWindow *window, gboolean setting);
GDK_AVAILABLE_IN_ALL
void     gtk_window_set_keep_below    (GtkWindow *window, gboolean setting);

GDK_AVAILABLE_IN_ALL
void gtk_window_begin_resize_drag (GtkWindow     *window,
                                   GdkSurfaceEdge  edge,
                                   gint           button,
                                   gint           x,
                                   gint           y,
                                   guint32        timestamp);
GDK_AVAILABLE_IN_ALL
void gtk_window_begin_move_drag   (GtkWindow     *window,
                                   gint           button,
                                   gint           x,
                                   gint           y,
                                   guint32        timestamp);

/* Set initial default size of the window (does not constrain user
 * resize operations)
 */
GDK_AVAILABLE_IN_ALL
void     gtk_window_set_default_size (GtkWindow   *window,
                                      gint         width,
                                      gint         height);
GDK_AVAILABLE_IN_ALL
void     gtk_window_get_default_size (GtkWindow   *window,
                                      gint        *width,
                                      gint        *height);
GDK_AVAILABLE_IN_ALL
void     gtk_window_resize           (GtkWindow   *window,
                                      gint         width,
                                      gint         height);
GDK_AVAILABLE_IN_ALL
void     gtk_window_get_size         (GtkWindow   *window,
                                      gint        *width,
                                      gint        *height);

GDK_AVAILABLE_IN_ALL
GtkWindowGroup *gtk_window_get_group (GtkWindow   *window);
GDK_AVAILABLE_IN_ALL
gboolean gtk_window_has_group        (GtkWindow   *window);

GDK_AVAILABLE_IN_ALL
GtkWindowType gtk_window_get_window_type     (GtkWindow     *window);


GDK_AVAILABLE_IN_ALL
GtkApplication *gtk_window_get_application      (GtkWindow          *window);
GDK_AVAILABLE_IN_ALL
void            gtk_window_set_application      (GtkWindow          *window,
                                                 GtkApplication     *application);

GDK_AVAILABLE_IN_ALL
void     gtk_window_set_titlebar           (GtkWindow    *window,
                                            GtkWidget    *titlebar);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_window_get_titlebar         (GtkWindow    *window);

GDK_AVAILABLE_IN_ALL
gboolean gtk_window_is_maximized           (GtkWindow    *window);

GDK_AVAILABLE_IN_ALL
void     gtk_window_set_interactive_debugging (gboolean enable);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkWindow, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkWindowGroup, g_object_unref)

G_END_DECLS

#endif /* __GTK_WINDOW_H__ */
