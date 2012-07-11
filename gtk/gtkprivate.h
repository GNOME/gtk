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

#include "gtkcsstypesprivate.h"
#include "gtktexthandleprivate.h"

G_BEGIN_DECLS

#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

const gchar * _gtk_get_datadir            (void);
const gchar * _gtk_get_libdir             (void);
const gchar * _gtk_get_sysconfdir         (void);
const gchar * _gtk_get_localedir          (void);
const gchar * _gtk_get_data_prefix        (void);

gboolean      _gtk_fnmatch                (const char *pattern,
                                           const char *string,
                                           gboolean    no_leading_period);

gchar       * _gtk_get_lc_ctype           (void);

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

gboolean        _gtk_propagate_captured_event  (GtkWidget       *widget,
                                                GdkEvent        *event,
                                                GtkWidget       *topmost);

G_END_DECLS

#endif /* __GTK_PRIVATE_H__ */
