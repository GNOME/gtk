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

typedef struct _GtkWindowPrivate      GtkWindowPrivate;
typedef struct _GtkWindowClass        GtkWindowClass;
typedef struct _GtkWindowGeometryInfo GtkWindowGeometryInfo;
typedef struct _GtkWindowGroup        GtkWindowGroup;
typedef struct _GtkWindowGroupClass   GtkWindowGroupClass;
typedef struct _GtkWindowGroupPrivate GtkWindowGroupPrivate;

struct _GtkWindow
{
  GtkBin bin;

  GtkWindowPrivate *priv;
};

struct _GtkWindowClass
{
  GtkBinClass parent_class;

  void     (* set_focus)   (GtkWindow *window,
                            GtkWidget *focus);

  /* G_SIGNAL_ACTION signals for keybindings */

  void     (* activate_focus)   (GtkWindow *window);
  void     (* activate_default) (GtkWindow *window);
  void	   (* keys_changed)     (GtkWindow *window);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

#define GTK_TYPE_WINDOW_GROUP             (gtk_window_group_get_type ())
#define GTK_WINDOW_GROUP(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_WINDOW_GROUP, GtkWindowGroup))
#define GTK_WINDOW_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_WINDOW_GROUP, GtkWindowGroupClass))
#define GTK_IS_WINDOW_GROUP(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_WINDOW_GROUP))
#define GTK_IS_WINDOW_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WINDOW_GROUP))
#define GTK_WINDOW_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WINDOW_GROUP, GtkWindowGroupClass))

struct _GtkWindowGroup
{
  GObject parent_instance;

  GtkWindowGroupPrivate *priv;
};

struct _GtkWindowGroupClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

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
void       gtk_window_set_wmclass              (GtkWindow           *window,
						const gchar         *wmclass_name,
						const gchar         *wmclass_class);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_role                 (GtkWindow           *window,
                                                const gchar         *role);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_startup_id           (GtkWindow           *window,
                                                const gchar         *startup_id);
GDK_AVAILABLE_IN_ALL
const gchar * gtk_window_get_role              (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_add_accel_group          (GtkWindow           *window,
						GtkAccelGroup	    *accel_group);
GDK_AVAILABLE_IN_ALL
void       gtk_window_remove_accel_group       (GtkWindow           *window,
						GtkAccelGroup	    *accel_group);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_position             (GtkWindow           *window,
						GtkWindowPosition    position);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_activate_focus	       (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_focus                (GtkWindow           *window,
						GtkWidget           *focus);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_window_get_focus                (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_default              (GtkWindow           *window,
						GtkWidget           *default_widget);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_window_get_default_widget       (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_activate_default	       (GtkWindow           *window);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_transient_for        (GtkWindow           *window, 
						GtkWindow           *parent);
GDK_AVAILABLE_IN_ALL
GtkWindow *gtk_window_get_transient_for        (GtkWindow           *window);
GDK_AVAILABLE_IN_3_4
void       gtk_window_set_attached_to          (GtkWindow           *window, 
                                                GtkWidget           *attach_widget);
GDK_AVAILABLE_IN_3_4
GtkWidget *gtk_window_get_attached_to          (GtkWindow           *window);
GDK_DEPRECATED_IN_3_8_FOR(gtk_widget_set_opacity)
void       gtk_window_set_opacity              (GtkWindow           *window, 
						gdouble              opacity);
GDK_DEPRECATED_IN_3_8_FOR(gtk_widget_get_opacity)
gdouble    gtk_window_get_opacity              (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_type_hint            (GtkWindow           *window, 
						GdkWindowTypeHint    hint);
GDK_AVAILABLE_IN_ALL
GdkWindowTypeHint gtk_window_get_type_hint     (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_skip_taskbar_hint    (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_skip_taskbar_hint    (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_skip_pager_hint      (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_skip_pager_hint      (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_urgency_hint         (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_urgency_hint         (GtkWindow           *window);
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
GDK_AVAILABLE_IN_3_4
void       gtk_window_set_hide_titlebar_when_maximized (GtkWindow   *window,
                                                        gboolean     setting);
GDK_AVAILABLE_IN_3_4
gboolean   gtk_window_get_hide_titlebar_when_maximized (GtkWindow   *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_mnemonics_visible    (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_mnemonics_visible    (GtkWindow           *window);
GDK_AVAILABLE_IN_3_2
void       gtk_window_set_focus_visible        (GtkWindow           *window,
                                                gboolean             setting);
GDK_AVAILABLE_IN_3_2
gboolean   gtk_window_get_focus_visible        (GtkWindow           *window);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_resizable            (GtkWindow           *window,
                                                gboolean             resizable);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_resizable            (GtkWindow           *window);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_gravity              (GtkWindow           *window,
                                                GdkGravity           gravity);
GDK_AVAILABLE_IN_ALL
GdkGravity gtk_window_get_gravity              (GtkWindow           *window);


GDK_AVAILABLE_IN_ALL
void       gtk_window_set_geometry_hints       (GtkWindow           *window,
						GtkWidget           *geometry_widget,
						GdkGeometry         *geometry,
						GdkWindowHints       geom_mask);

GDK_AVAILABLE_IN_ALL
void	   gtk_window_set_screen	       (GtkWindow	    *window,
						GdkScreen	    *screen);
GDK_AVAILABLE_IN_ALL
GdkScreen* gtk_window_get_screen	       (GtkWindow	    *window);

GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_is_active                (GtkWindow           *window);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_has_toplevel_focus       (GtkWindow           *window);

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
void       gtk_window_set_icon_list                (GtkWindow  *window,
                                                    GList      *list);
GDK_AVAILABLE_IN_ALL
GList*     gtk_window_get_icon_list                (GtkWindow  *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_icon                     (GtkWindow  *window,
                                                    GdkPixbuf  *icon);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_icon_name                (GtkWindow   *window,
						    const gchar *name);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_set_icon_from_file           (GtkWindow   *window,
						    const gchar *filename,
						    GError     **err);
GDK_AVAILABLE_IN_ALL
GdkPixbuf* gtk_window_get_icon                     (GtkWindow  *window);
GDK_AVAILABLE_IN_ALL
const gchar * gtk_window_get_icon_name             (GtkWindow  *window);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_default_icon_list        (GList      *list);
GDK_AVAILABLE_IN_ALL
GList*     gtk_window_get_default_icon_list        (void);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_default_icon             (GdkPixbuf  *icon);
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_default_icon_name        (const gchar *name);
GDK_AVAILABLE_IN_ALL
const gchar * gtk_window_get_default_icon_name     (void);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_set_default_icon_from_file   (const gchar *filename,
						    GError     **err);

GDK_AVAILABLE_IN_ALL
void       gtk_window_set_auto_startup_notification (gboolean setting);

/* If window is set modal, input will be grabbed when show and released when hide */
GDK_AVAILABLE_IN_ALL
void       gtk_window_set_modal      (GtkWindow *window,
				      gboolean   modal);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_window_get_modal      (GtkWindow *window);
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
GDK_AVAILABLE_IN_3_10
void     gtk_window_close         (GtkWindow *window);
GDK_AVAILABLE_IN_ALL
void     gtk_window_set_keep_above    (GtkWindow *window, gboolean setting);
GDK_AVAILABLE_IN_ALL
void     gtk_window_set_keep_below    (GtkWindow *window, gboolean setting);

GDK_AVAILABLE_IN_ALL
void gtk_window_begin_resize_drag (GtkWindow     *window,
                                   GdkWindowEdge  edge,
                                   gint           button,
                                   gint           root_x,
                                   gint           root_y,
                                   guint32        timestamp);
GDK_AVAILABLE_IN_ALL
void gtk_window_begin_move_drag   (GtkWindow     *window,
                                   gint           button,
                                   gint           root_x,
                                   gint           root_y,
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
void     gtk_window_move             (GtkWindow   *window,
                                      gint         x,
                                      gint         y);
GDK_AVAILABLE_IN_ALL
void     gtk_window_get_position     (GtkWindow   *window,
                                      gint        *root_x,
                                      gint        *root_y);
GDK_AVAILABLE_IN_ALL
gboolean gtk_window_parse_geometry   (GtkWindow   *window,
                                      const gchar *geometry);

GDK_AVAILABLE_IN_ALL
void gtk_window_set_default_geometry (GtkWindow *window,
                                      gint       width,
                                      gint       height);
GDK_AVAILABLE_IN_ALL
void gtk_window_resize_to_geometry   (GtkWindow *window,
                                      gint       width,
                                      gint       height);

GDK_AVAILABLE_IN_ALL
GtkWindowGroup *gtk_window_get_group (GtkWindow   *window);
GDK_AVAILABLE_IN_ALL
gboolean gtk_window_has_group        (GtkWindow   *window);

/* Ignore this unless you are writing a GUI builder */
GDK_DEPRECATED_IN_3_10
void     gtk_window_reshow_with_initial_size (GtkWindow *window);

GDK_AVAILABLE_IN_ALL
GtkWindowType gtk_window_get_window_type     (GtkWindow     *window);

/* Window groups
 */
GDK_AVAILABLE_IN_ALL
GType            gtk_window_group_get_type      (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWindowGroup * gtk_window_group_new           (void);
GDK_AVAILABLE_IN_ALL
void             gtk_window_group_add_window    (GtkWindowGroup     *window_group,
						 GtkWindow          *window);
GDK_AVAILABLE_IN_ALL
void             gtk_window_group_remove_window (GtkWindowGroup     *window_group,
					         GtkWindow          *window);
GDK_AVAILABLE_IN_ALL
GList *          gtk_window_group_list_windows  (GtkWindowGroup     *window_group);

GDK_AVAILABLE_IN_ALL
GtkWidget *      gtk_window_group_get_current_grab (GtkWindowGroup *window_group);
GDK_AVAILABLE_IN_ALL
GtkWidget *      gtk_window_group_get_current_device_grab (GtkWindowGroup *window_group,
                                                           GdkDevice      *device);

GDK_AVAILABLE_IN_ALL
GtkApplication *gtk_window_get_application      (GtkWindow          *window);
GDK_AVAILABLE_IN_ALL
void            gtk_window_set_application      (GtkWindow          *window,
                                                 GtkApplication     *application);


/* Window grips
 */
GDK_AVAILABLE_IN_ALL
void     gtk_window_set_has_resize_grip    (GtkWindow    *window,
                                            gboolean      value);
GDK_AVAILABLE_IN_ALL
gboolean gtk_window_get_has_resize_grip    (GtkWindow    *window);
GDK_AVAILABLE_IN_ALL
gboolean gtk_window_resize_grip_is_visible (GtkWindow    *window);
GDK_AVAILABLE_IN_ALL
gboolean gtk_window_get_resize_grip_area   (GtkWindow    *window,
                                            GdkRectangle *rect);

GDK_AVAILABLE_IN_3_10
void     gtk_window_set_titlebar           (GtkWindow    *window,
                                            GtkWidget    *titlebar);

G_END_DECLS

#endif /* __GTK_WINDOW_H__ */
