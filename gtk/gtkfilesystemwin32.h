/* GTK - The GIMP Toolkit
 * gtkfilesystemunix.h: Default implementation of GtkFileSystem for UNIX-like systems
 * Copyright (C) 2003, Red Hat, Inc.
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

#ifndef __GTK_FILE_SYSTEM_WIN32_H__
#define __GTK_FILE_SYSTEM_WIN32_H__

#include <glib-object.h>
#include "gtkfilesystem.h"

G_BEGIN_DECLS

#define GTK_TYPE_FILE_SYSTEM_WIN32             (gtk_file_system_win32_get_type ())
#define GTK_FILE_SYSTEM_WIN32(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_SYSTEM_WIN32, GtkFileSystemWin32))
#define GTK_IS_FILE_SYSTEM_WIN32(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_SYSTEM_WIN32))

typedef struct _GtkFileSystemWin32      GtkFileSystemWin32;

GtkFileSystem *gtk_file_system_win32_new      (void);
GType          gtk_file_system_win32_get_type (void) G_GNUC_CONST;
     
gchar         *_gtk_file_system_win32_get_desktop (void);

G_END_DECLS

#endif /* __GTK_FILE_SYSTEM_WIN32_H__ */
