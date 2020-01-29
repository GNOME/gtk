#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sysprof-capture.h>
#include <gio/gio.h>

typedef struct {
  const char *group;
  gint64 value;
} Data;

static gboolean
callback (const SysprofCaptureFrame *frame,
          gpointer              user_data)
{
  Data *data = user_data;

  if (frame->type == SYSPROF_CAPTURE_FRAME_MARK)
    {
      SysprofCaptureMark *mark = (SysprofCaptureMark *)frame;
      if (strcmp (mark->group, "gtk") == 0 &&
          strcmp (mark->name, data->group) == 0)
        {
          data->value = mark->duration;
          return FALSE;
        }
    }

  return TRUE;
}

#define MILLISECONDS(v) ((v) / (1000.0 * G_TIME_SPAN_MILLISECOND))

static int opt_rep = 10;
static char *opt_mark;
static char *opt_name;
static char *opt_output;

static GOptionEntry options[] = {
  { "mark", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &opt_mark, "Name of the mark", "NAME" },
  { "runs", '0', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &opt_rep, "Number of runs", "COUNT" },
  { "name", '0', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &opt_name, "Name of this test", "NAME" },
  { "output", '0', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &opt_output, "Directory to save syscap files", "DIRECTORY" },
  { NULL, }
};

int
main (int argc, char *argv[])
{
  GOptionContext *context;
  GError *error = NULL;
  Data data;
  SysprofCaptureFrameType type;
  char fd_str[20];
  gint64 *values;
  gint64 min, max, total;
  int count;
  char *output_dir = NULL;
  int i;

  context = g_option_context_new ("COMMANDLINE");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    g_error ("Parsing options: %s", error->message);

  if (argc < 2)
    {
      g_print ("Usage: testperf [OPTIONS] COMMANDLINE\n");
      exit (1);
    }

  if (opt_rep < 1)
    g_error ("COUNT must be a positive number");

  if (opt_output)
    {
      GError *err = NULL;
      GFile *file;

      file = g_file_new_for_commandline_arg (opt_output);
      if (!g_file_make_directory_with_parents (file, NULL, &err))
        {
          if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_EXISTS))
            g_error ("%s", err->message);
        }

      output_dir = g_file_get_path (file);
      g_object_unref (file);
    }

  opt_rep++;

  values = g_new (gint64, opt_rep);

  for (i = 0; i < opt_rep; i++)
    {
      GSubprocessLauncher *launcher;
      GSubprocess *subprocess;
      int fd;
      gchar *name;
      SysprofCaptureReader *reader;
      SysprofCaptureCursor *cursor;
      SysprofCaptureCondition *condition;

      fd = g_file_open_tmp ("gtk.XXXXXX.syscap", &name, &error);
      if (error)
        g_error ("Create syscap file: %s", error->message);

      launcher = g_subprocess_launcher_new (0);
      g_subprocess_launcher_take_fd (launcher, fd, fd);
      g_snprintf (fd_str, sizeof (fd_str), "%d", fd);
      g_subprocess_launcher_setenv (launcher, "GTK_TRACE_FD", fd_str, TRUE);
      g_subprocess_launcher_setenv (launcher, "GTK_DEBUG_AUTO_QUIT", "1", TRUE);

      subprocess = g_subprocess_launcher_spawnv (launcher, (const char *const *)argv + 1, &error);
      if (error)
        g_error ("Launch child: %s", error->message);

      if (!g_subprocess_wait (subprocess, NULL, &error))
        g_error ("Run child: %s", error->message);

      if (!g_subprocess_get_successful (subprocess))
        g_error ("Child process failed");
        
      g_object_unref (subprocess);
      g_object_unref (launcher);

      reader = sysprof_capture_reader_new (name, &error);
      if (error)
        g_error ("Opening syscap file: %s", error->message);

      data.group = opt_mark ? opt_mark : "style";
      data.value = 0;

      cursor = sysprof_capture_cursor_new (reader);

      type = SYSPROF_CAPTURE_FRAME_MARK;
      condition = sysprof_capture_condition_new_where_type_in (1, &type);
      sysprof_capture_cursor_add_condition (cursor, condition);

      sysprof_capture_cursor_foreach (cursor, callback, &data);

      values[i] = data.value;

      sysprof_capture_cursor_unref (cursor);
      sysprof_capture_reader_unref (reader);

      if (output_dir)
        {
          GFile *src, *dest;
          char * save_to;

          save_to = g_strdup_printf ("%s/%s.%d.syscap", output_dir, opt_name ? opt_name : "gtk", i);

          src = g_file_new_for_path (name);
          dest = g_file_new_for_path (save_to);
          if (!g_file_copy (src, dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error))
            g_error ("%s", error->message);

          g_free (save_to);
          g_object_unref (src);
          g_object_unref (dest);
        }
      else
        remove (name);

      g_free (name);

      /* A poor mans way to try and isolate the runs from each other */
      g_usleep (300000);
    }

  min = G_MAXINT64;
  max = 0;
  count = 0;
  total = 0;

  /* Ignore the first run, to avoid cache effects */
  for (i = 1; i < opt_rep; i++)
    {
      if (min > values[i])
        min = values[i];
      if (max < values[i])
        max = values[i];
      count++;
      total += values[i];
    }

  g_print ("%d runs, min %g, max %g, avg %g\n",
           count,
           MILLISECONDS (min),
           MILLISECONDS (max),
           MILLISECONDS (total / count));
}
