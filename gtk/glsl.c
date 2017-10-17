/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>

#ifdef G_IO_WIN32
#include <gio/gwin32outputstream.h>
#include <windows.h>
#else
#include <gio/gunixoutputstream.h>
#include <unistd.h>
#endif

static gboolean
compile (GskSlCompiler *compiler,
         GOutputStream *output,
         const char    *filename)
{
  GBytes *bytes;
  GskSlProgram *program;
  GError *error = NULL;
  GFile *file;

  file = g_file_new_for_commandline_arg (filename);

  program = gsk_sl_compiler_compile_file (compiler,
                                          GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (compiler), "shader-stage")),
                                          file);
  g_object_unref (file);
  if (program == NULL)
    return FALSE;

  bytes = gsk_sl_program_to_spirv (program);
  if (!g_output_stream_write_all (output, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL, NULL, &error))
    {
      g_print (error->message);
      g_error_free (error);
      g_bytes_unref (bytes);
      g_object_unref (program);
      return FALSE;
    }

  g_bytes_unref (bytes);
  g_object_unref (program);

  return TRUE;
}

static gboolean
dump (GskSlCompiler *compiler,
      GOutputStream *output,
      const char    *filename)
{
  GString *string;
  GskSlProgram *program;
  GError *error = NULL;
  GFile *file;

  file = g_file_new_for_commandline_arg (filename);

  program = gsk_sl_compiler_compile_file (compiler,
                                          GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (compiler), "shader-stage")),
                                          file);
  g_object_unref (file);
  if (program == NULL)
    return FALSE;

  string = g_string_new (NULL);
  gsk_sl_program_print (program, string);
  if (!g_output_stream_write_all (output, string->str, string->len, NULL, NULL, &error))
    {
      g_print (error->message);
      g_error_free (error);
      g_string_free (string, TRUE);
      g_object_unref (program);
      return FALSE;
    }

  g_string_free (string, TRUE);
  g_object_unref (program);

  return TRUE;
}

static void
usage (GOptionContext *ctx)
{
  char *help = g_option_context_get_help (ctx, TRUE, NULL);

  g_print ("%s", help);
  g_free (help);
  exit (EXIT_FAILURE);
}

static gboolean
define (const gchar *option_name,
        const gchar *value,
        gpointer data,
        GError **error)
{
  GskSlCompiler *compiler = data;
  char **tokens;
  gboolean result;

  tokens = g_strsplit (value, "=", 2);

  result = gsk_sl_compiler_add_define (compiler,
                                       tokens[0],
                                       tokens[1],
                                       error);

  g_strfreev (tokens);

  return result;
}

static gboolean
undefine (const gchar *option_name,
          const gchar *value,
          gpointer data,
          GError **error)
{
  GskSlCompiler *compiler = data;

  gsk_sl_compiler_remove_define (compiler, value);

  return TRUE;
}

static gboolean
stage (const gchar *option_name,
       const gchar *value,
       gpointer data,
       GError **error)
{
  static const struct {
    const char *name;
    GskSlShaderStage stage;
  } stage_names[] = {
    { "f", GSK_SL_SHADER_FRAGMENT },
    { "frag", GSK_SL_SHADER_FRAGMENT },
    { "fragment", GSK_SL_SHADER_FRAGMENT },
    { "v", GSK_SL_SHADER_VERTEX },
    { "vert", GSK_SL_SHADER_VERTEX },
    { "vertex", GSK_SL_SHADER_VERTEX }
  };
  GskSlCompiler *compiler = data;
  GString *str;
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (stage_names); i++)
    {
      if (g_ascii_strcasecmp (stage_names[i].name, value) == 0)
        {
          g_object_set_data (G_OBJECT (compiler), "shader-stage", GUINT_TO_POINTER (stage_names[i].stage));
          return TRUE;
        }
    }

  str = g_string_new ("");
  for (i = 0; i < G_N_ELEMENTS (stage_names); i++)
    {
      if (i > 0)
        g_string_append (str, ", ");
      g_string_append (str, stage_names[i].name);
    }
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
               "Unknown value given for shader stage. Valid options are: %s",
               str->str);
  g_string_free (str, TRUE);
  return FALSE;
}

int
main (int argc, char *argv[])
{
  GOptionContext *ctx;
  char **filenames = NULL;
  char *output_file = NULL;
  gboolean print = FALSE;
  const GOptionEntry entries[] = {
    { "define", 'D', 0, G_OPTION_ARG_CALLBACK, define, "Add a preprocssor definition", "NAME[=VALUE]" },
    { "undef", 'U', 0, G_OPTION_ARG_CALLBACK, undefine, "Cancel previous preprocessor definition", "NAME" },
    { "print", 'p', 0, G_OPTION_ARG_NONE, &print, "Print instead of compiling", NULL },
    { "stage", 's', 0, G_OPTION_ARG_CALLBACK, stage, "Set the shader stage", "STAGE" },
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_file, "Output filename", "FILE" },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, "List of input files", "FILE [FILE...]" },
    { NULL, }
  };
  GskSlCompiler *compiler;
  GOptionGroup *group;
  GError *error = NULL;
  GOutputStream *output;
  gboolean success = TRUE;
  guint i;

  g_set_prgname ("gtk-glsl");

  gtk_init ();

  compiler = gsk_sl_compiler_new ();
  g_object_set_data (G_OBJECT (compiler), "shader-stage", GUINT_TO_POINTER (GSK_SL_SHADER_FRAGMENT));
  ctx = g_option_context_new (NULL);
  group = g_option_group_new (NULL, NULL, NULL, g_object_ref (compiler), g_object_unref);
  g_option_group_add_entries (group, entries);
  g_option_context_set_main_group (ctx, group);

  if (!g_option_context_parse (ctx, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (filenames == NULL)
    usage (ctx);

  g_option_context_free (ctx);

  if (output_file == NULL)
    {
#ifdef G_IO_WIN32
      HANDLE handle;
      handle = GetStdHandle (STD_OUTPUT_HANDLE);
      if (handle == NULL)
        {
          g_printerr ("No standard output. Use -o/--ouput to write to file\n");
          exit (1);
        }
      output = g_win32_output_stream_new (handle, FALSE);
#else
      output = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
#endif
    }
  else
    {
      GFile *file = g_file_new_for_path (output_file);
      output = G_OUTPUT_STREAM (g_file_replace (file,
                                                NULL, FALSE,
                                                G_FILE_CREATE_REPLACE_DESTINATION,
                                                NULL,
                                                &error));
      g_object_unref (file);
      if (output == NULL)
        {
          g_printerr ("Error creating output file: %s\n", error->message);
          g_error_free (error);
          exit (1);
        }
    }

  for (i = 0; success && filenames[i] != NULL; i++)
    {
      if (print)
        success = dump (compiler, output, filenames[i]);
      else
        success = compile (compiler, output, filenames[i]);
    }

 if (!g_output_stream_close (output, NULL, &error))
  {
    g_printerr ("%s\n", error->message);
    g_error_free (error);
    success = FALSE;
  }

  g_object_unref (output);
  g_object_unref (compiler);
  g_strfreev (filenames);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
