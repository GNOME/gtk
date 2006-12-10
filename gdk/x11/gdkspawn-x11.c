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

#include <config.h>
#include <string.h>

#ifdef HAVE_CRT_EXTERNS_H 
#include <crt_externs.h> /* for _NSGetEnviron */
#endif

#include "gdkspawn.h"

#include <glib.h>
#include <gdk/gdk.h>
#include "gdkalias.h"
  
#ifdef HAVE__NSGETENVIRON
#define environ (*_NSGetEnviron())
#else

/* According to the Single Unix Specification, environ is not in 
 * any system header, although unistd.h often declares it.
 */
extern char **environ;
#endif

/**
 * gdk_make_spawn_environment_for_screen:
 * @screen: A #GdkScreen
 * @envp: program environment to copy, or %NULL to use current environment.
 *
 * Returns a modified copy of the program environment @envp (or the current
 * environment if @envp is %NULL) with <envar>DISPLAY</envar> set such that 
 * a launched application (which calls gdk_display_open()) inheriting this 
 * environment would have @screen as its default screen..
 *
 * Returns: a newly-allocated %NULL-terminated array of strings.
 *          Use g_strfreev() to free it.
 *
 * Since: 2.4
 **/
static gchar **
gdk_spawn_make_environment_for_screen (GdkScreen  *screen,
				       gchar     **envp)
{
  gchar **retval = NULL;
  gchar  *display_name;
  gint    display_index = -1;
  gint    i, env_len;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (envp == NULL)
    envp = environ;

  for (env_len = 0; envp[env_len]; env_len++)
    if (strncmp (envp[env_len], "DISPLAY", strlen ("DISPLAY")) == 0)
      display_index = env_len;

  retval = g_new (char *, env_len + 1);
  retval[env_len] = NULL;

  display_name = gdk_screen_make_display_name (screen);

  for (i = 0; i < env_len; i++)
    if (i == display_index)
      retval[i] = g_strconcat ("DISPLAY=", display_name, NULL);
    else
      retval[i] = g_strdup (envp[i]);

  g_assert (i == env_len);

  g_free (display_name);

  return retval;
}

/**
 * gdk_spawn_on_screen:
 * @screen: a #GdkScreen
 * @working_directory: child's current working directory, or %NULL to 
 *   inherit parent's
 * @argv: child's argument vector
 * @envp: child's environment, or %NULL to inherit parent's
 * @flags: flags from #GSpawnFlags
 * @child_setup: function to run in the child just before exec()
 * @user_data: user data for @child_setup
 * @child_pid: return location for child process ID, or %NULL
 * @error: return location for error
 *
 * Like g_spawn_async(), except the child process is spawned in such
 * an environment that on calling gdk_display_open() it would be
 * returned a #GdkDisplay with @screen as the default screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Return value: %TRUE on success, %FALSE if error is set
 *
 * Since: 2.4
 **/
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
  gchar    **new_envp;
  gboolean   retval;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  new_envp = gdk_spawn_make_environment_for_screen (screen, envp);

  retval = g_spawn_async (working_directory,
			  argv,
			  new_envp,
			  flags,
			  child_setup,
			  user_data,
			  child_pid,
			  error);

  g_strfreev (new_envp);

  return retval;
}

/**
 * gdk_spawn_on_screen_with_pipes:
 * @screen: a #GdkScreen
 * @working_directory: child's current working directory, or %NULL to 
 *   inherit parent's
 * @argv: child's argument vector
 * @envp: child's environment, or %NULL to inherit parent's
 * @flags: flags from #GSpawnFlags
 * @child_setup: function to run in the child just before exec()
 * @user_data: user data for @child_setup
 * @child_pid: return location for child process ID, or %NULL
 * @standard_input: return location for file descriptor to write to 
 *   child's stdin, or %NULL
 * @standard_output: return location for file descriptor to read child's 
 *   stdout, or %NULL
 * @standard_error: return location for file descriptor to read child's 
 *   stderr, or %NULL
 * @error: return location for error
 *
 * Like g_spawn_async_with_pipes(), except the child process is
 * spawned in such an environment that on calling gdk_display_open()
 * it would be returned a #GdkDisplay with @screen as the default
 * screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Return value: %TRUE on success, %FALSE if an error was set
 *
 * Since: 2.4
 **/
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
  gchar    **new_envp;
  gboolean   retval;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  new_envp = gdk_spawn_make_environment_for_screen (screen, envp);

  retval = g_spawn_async_with_pipes (working_directory,
				     argv,
				     new_envp,
				     flags,
				     child_setup,
				     user_data,
				     child_pid,
				     standard_input,
				     standard_output,
				     standard_error,
				     error);

  g_strfreev (new_envp);

  return retval;
}

/**
 * gdk_spawn_command_line_on_screen:
 * @screen: a #GdkScreen
 * @command_line: a command line
 * @error: return location for errors
 *
 * Like g_spawn_command_line_async(), except the child process is
 * spawned in such an environment that on calling gdk_display_open()
 * it would be returned a #GdkDisplay with @screen as the default
 * screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Return value: %TRUE on success, %FALSE if error is set.
 *
 * Since: 2.4
 **/
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
