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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_PRIVATE_H__
#define __GTK_PRIVATE_H__

#include <glib.h>

#include "gtksettings.h"

G_BEGIN_DECLS

#ifdef G_OS_WIN32

const gchar *_gtk_get_datadir ();
const gchar *_gtk_get_libdir ();
const gchar *_gtk_get_sysconfdir ();
const gchar *_gtk_get_localedir ();
const gchar *_gtk_get_data_prefix ();

#undef GTK_DATADIR
#define GTK_DATADIR _gtk_get_datadir ()
#undef GTK_LIBDIR
#define GTK_LIBDIR _gtk_get_libdir ()
#undef GTK_LOCALEDIR
#define GTK_LOCALEDIR _gtk_get_localedir ()
#undef GTK_SYSCONFDIR
#define GTK_SYSCONFDIR _gtk_get_sysconfdir ()
#undef GTK_DATA_PREFIX
#define GTK_DATA_PREFIX _gtk_get_data_prefix ()

#endif /* G_OS_WIN32 */

#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

/* Many keyboard shortcuts for Mac are the same as for X
 * except they use Command key instead of Control (e.g. Cut,
 * Copy, Paste). This symbol is for those simple cases. */
#ifndef GDK_WINDOWING_QUARTZ
#define GTK_DEFAULT_ACCEL_MOD_MASK GDK_CONTROL_MASK
#else
#define GTK_DEFAULT_ACCEL_MOD_MASK GDK_META_MASK
#endif

gboolean _gtk_fnmatch      (const char *pattern,
                            const char *string,
                            gboolean    no_leading_period);

gchar   *_gtk_get_lc_ctype (void);

gchar * _gtk_find_module              (const gchar  *name,
                                       const gchar  *type);
gchar **_gtk_get_module_path          (const gchar  *type);

void    _gtk_modules_init             (gint          *argc,
                                       gchar       ***argv,
                                       const gchar   *gtk_modules_args);
void    _gtk_modules_settings_changed (GtkSettings   *settings,
                                       const gchar   *modules);

G_END_DECLS

#endif /* __GTK_PRIVATE_H__ */
