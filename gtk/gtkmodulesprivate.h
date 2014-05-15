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

#ifndef __GTK_MODULES_PRIVATE_H__
#define __GTK_MODULES_PRIVATE_H__

G_BEGIN_DECLS

#include "gtksettings.h"

gchar  * _gtk_find_module              (const gchar  *name,
                                        const gchar  *type);
gchar ** _gtk_get_module_path          (const gchar  *type);

void     _gtk_modules_init             (gint          *argc,
                                        gchar       ***argv,
                                        const gchar   *gtk_modules_args);
void     _gtk_modules_settings_changed (GtkSettings   *settings,
                                        const gchar   *modules);

gboolean _gtk_module_has_mixed_deps    (GModule       *module);

G_END_DECLS

#endif /* __GTK_MODULES_PRIVATE_H__ */
