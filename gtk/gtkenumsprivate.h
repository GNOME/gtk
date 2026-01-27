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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**<private>
 * GtkRestoreReason:
 * @GTK_RESTORE_REASON_PRISTINE: Don't restore anything
 * @GTK_RESTORE_REASON_LAUNCH: This is normal launch. Restore as little as is reasonable
 * @GTK_RESTORE_REASON_RECOVER: The application has crashed before. Try to restore the previous state
 * @GTK_RESTORE_REASON_RESTORE: This is a session restore. Restore the previous state as far as possible
 *
 * Enumerates possible reasons for an application to restore saved state.
 *
 * See [signal@Gtk.Application::restore-state].
 *
 * Since: 4.22
 */
typedef enum
{
  GTK_RESTORE_REASON_PRISTINE,
  GTK_RESTORE_REASON_LAUNCH,
  GTK_RESTORE_REASON_RECOVER,
  GTK_RESTORE_REASON_RESTORE,
} GtkRestoreReason;

G_END_DECLS
