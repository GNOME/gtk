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

#ifndef __GTK_PRIVATE_H__
#define __GTK_PRIVATE_H__

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdk-private.h>

#include "gtkcsstypesprivate.h"
#include "gtktexthandleprivate.h"

G_BEGIN_DECLS

#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

#define OPPOSITE_ORIENTATION(_orientation) (1 - (_orientation))

#ifdef G_DISABLE_CAST_CHECKS
/* This is true for debug no and minimum */
#define gtk_internal_return_if_fail(__expr) G_STMT_START{ (void)0; }G_STMT_END
#define gtk_internal_return_val_if_fail(__expr, __val) G_STMT_START{ (void)0; }G_STMT_END
#else
/* This is true for debug yes */
#define gtk_internal_return_if_fail(__expr) g_return_if_fail(__expr)
#define gtk_internal_return_val_if_fail(__expr, __val) g_return_val_if_fail(__expr, __val)
#endif

const gchar * _gtk_get_datadir            (void);
const gchar * _gtk_get_libdir             (void);
const gchar * _gtk_get_sysconfdir         (void);
const gchar * _gtk_get_localedir          (void);
const gchar * _gtk_get_data_prefix        (void);

gboolean      _gtk_fnmatch                (const char *pattern,
                                           const char *string,
                                           gboolean    no_leading_period);

gchar       * _gtk_get_lc_ctype           (void);

void          _gtk_ensure_resources       (void);

void          gtk_main_sync               (void);

gboolean _gtk_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                           GValue                *return_accu,
                                           const GValue          *handler_return,
                                           gpointer               dummy);

gboolean _gtk_single_string_accumulator   (GSignalInvocationHint *ihint,
                                           GValue                *return_accu,
                                           const GValue          *handler_return,
                                           gpointer               dummy);

GdkModifierType _gtk_replace_virtual_modifiers (GdkKeymap       *keymap,
                                                GdkModifierType  modifiers);
GdkModifierType _gtk_get_primary_accel_mod     (void);

gboolean _gtk_translate_keyboard_accel_state   (GdkKeymap       *keymap,
                                                guint            hardware_keycode,
                                                GdkModifierType  state,
                                                GdkModifierType  accel_mask,
                                                gint             group,
                                                guint           *keyval,
                                                gint            *effective_group,
                                                gint            *level,
                                                GdkModifierType *consumed_modifiers);

void             gtk_propagate_event_internal  (GtkWidget       *widget,
                                                GdkEvent        *event,
                                                GtkWidget       *topmost);

gdouble _gtk_get_slowdown (void);
void    _gtk_set_slowdown (gdouble slowdown_factor);

char *gtk_get_portal_request_path (GDBusConnection  *connection,
                                   char            **token);
char *gtk_get_portal_session_path (GDBusConnection  *connection,
                                   char            **token);

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_REQUEST_INTERFACE "org.freedesktop.portal.Request"
#define PORTAL_SESSION_INTERFACE "org.freedesktop.portal.Session"
#define PORTAL_FILECHOOSER_INTERFACE "org.freedesktop.portal.FileChooser"
#define PORTAL_PRINT_INTERFACE "org.freedesktop.portal.Print"
#define PORTAL_SCREENSHOT_INTERFACE "org.freedesktop.portal.Screenshot"
#define PORTAL_INHIBIT_INTERFACE "org.freedesktop.portal.Inhibit"

#ifdef G_OS_WIN32
void _gtk_load_dll_with_libgtk3_manifest (const char *dllname);
#endif

gboolean        gtk_simulate_touchscreen (void);

void  gtk_set_display_debug_flags (GdkDisplay *display,
                                   guint       flags);
guint gtk_get_display_debug_flags (GdkDisplay *display);
gboolean gtk_get_any_display_debug_flag_set (void);

#ifdef G_ENABLE_DEBUG

#define GTK_DISPLAY_DEBUG_CHECK(display,type) (gtk_get_any_display_debug_flag_set () && G_UNLIKELY (gtk_get_display_debug_flags (display) & GTK_DEBUG_##type))
#define GTK_DISPLAY_NOTE(display,type,action) \
  G_STMT_START { \
  if (GTK_DISPLAY_DEBUG_CHECK (display,type)) { action; }; \
  } G_STMT_END

#else

#define GTK_DISPLAY_DEBUG_CHECK(display,type) 0
#define GTK_DISPLAY_NOTE(display,type,action)

#endif /* G_ENABLE_DEBUG */

void gtk_set_current_theme (const char *name,
                            const char *variant);

G_END_DECLS

#endif /* __GTK_PRIVATE_H__ */
