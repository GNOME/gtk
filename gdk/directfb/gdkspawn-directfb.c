/*
 * Copyright (C) 2003 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Mark McLoughlin <mark@skynet.ie>
 */

#include "config.h"
#include <glib.h>
#include "gdk.h"
#include "gdkspawn.h"
#include "gdkprivate.h"
#include "gdkalias.h"


gboolean
gdk_spawn_on_screen (GdkScreen             *screen,
		     const gchar           *working_directory,
		     gchar                **argv,
		     gchar                **envp,
		     GSpawnFlags            flags,
		     GSpawnChildSetupFunc   child_setup,
		     gpointer               user_data,
		     gint                  *child_pid,
		     GError               **error)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  return g_spawn_async (working_directory,
			argv,
			envp,
			flags,
			child_setup,
			user_data,
			child_pid,
			error);
}

gboolean
gdk_spawn_on_screen_with_pipes (GdkScreen            *screen,
				const gchar          *working_directory,
				gchar               **argv,
				gchar               **envp,
				GSpawnFlags           flags,
				GSpawnChildSetupFunc  child_setup,
				gpointer              user_data,
				gint                 *child_pid,
				gint                 *standard_input,
				gint                 *standard_output,
				gint                 *standard_error,
				GError              **error)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  return g_spawn_async_with_pipes (working_directory,
				   argv,
				   envp,
				   flags,
				   child_setup,
				   user_data,
				   child_pid,
				   standard_input,
				   standard_output,
				   standard_error,
				   error);
}

gboolean
gdk_spawn_command_line_on_screen (GdkScreen    *screen,
				  const gchar  *command_line,
				  GError      **error)
{
  gchar    **argv = NULL;
  gboolean   retval;

  g_return_val_if_fail (command_line != NULL, FALSE);

  if (!g_shell_parse_argv (command_line,
			   NULL, &argv,
			   error))
    return FALSE;

  retval = gdk_spawn_on_screen (screen,
				NULL, argv, NULL,
				G_SPAWN_SEARCH_PATH,
				NULL, NULL, NULL,
				error);
  g_strfreev (argv);

  return retval;
}

#define __GDK_SPAWN_X11_C__
#include "gdkaliasdef.c"
