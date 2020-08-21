#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sysprof.h>
#include <gio/gio.h>

typedef struct {
  const char *mark;
  const char *detail;
  gboolean do_start;
  gint64 start_time;
  gint64 value;
} Data;

static bool
callback (const SysprofCaptureFrame *frame,
          gpointer                   user_data)
{
  Data *data = user_data;

  if (frame->type == SYSPROF_CAPTURE_FRAME_MARK)
    {
      SysprofCaptureMark *mark = (SysprofCaptureMark *)frame;
      if (strcmp (mark->group, "gtk") == 0 &&
          strcmp (mark->name, data->mark) == 0 &&
          (data->detail == NULL || strcmp (mark->message, data->detail) == 0))
        {
          if (data->do_start)
            data->value = frame->time - data->start_time;
          else
            data->value = mark->duration;
          return FALSE;
        }
    }

  return TRUE;
}

#define MILLISECONDS(v) ((v) / (1000.0 * G_TIME_SPAN_MILLISECOND))

static int opt_rep = 10;
static char *opt_mark;
static char *opt_detail;
static char *opt_name;
static char *opt_output;
static gboolean opt_start_time;
static GMainLoop *main_loop;
static GError *failure;

static GOptionEntry options[] = {
  { "mark", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &opt_mark, "Name of the mark", "NAME" },
  { "detail", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &opt_detail, "Detail of the mark", "DETAIL" },
  { "start", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &opt_start_time, "Measure the start time", NULL },
  { "runs", '0', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &opt_rep, "Number of runs", "COUNT" },
  { "name", '0', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &opt_name, "Name of this test", "NAME" },
  { "output", '0', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &opt_output, "Directory to save syscap files", "DIRECTORY" },
  { NULL, }
};

static gboolean
start_in_main (gpointer data)
{
  SysprofProfiler *profiler = data;
  sysprof_profiler_start (profiler);
  return G_SOURCE_REMOVE;
}

static void
on_failure_cb (SysprofProfiler *profiler,
               const GError    *error)
{
  g_propagate_error (&failure, g_error_copy (error));
  g_main_loop_quit (main_loop);
}

static void
on_stopped_cb (SysprofProfiler *profiler)
{
  g_clear_error (&failure);
  g_main_loop_quit (main_loop);
}

int
main (int argc, char *argv[])
{
  GOptionContext *context;
  GError *error = NULL;
  Data data;
  SysprofCaptureFrameType type;
  gint64 *values;
  gint64 min, max, total;
  int count;
  char *output_dir = NULL;
  char **spawn_env;
  char *workdir;
  int i, j;

  context = g_option_context_new ("COMMANDLINE");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    g_error ("Parsing options: %s", error->message);

  if (argc < 2)
    {
      g_print ("Usage: %s [OPTIONS] COMMANDLINE\n", argv[0]);
      exit (1);
    }

  if (opt_rep < 1)
    g_error ("COUNT must be a positive number");

  main_loop = g_main_loop_new (NULL, FALSE);
  workdir = g_get_current_dir ();

  spawn_env = g_get_environ ();
  spawn_env = g_environ_setenv (spawn_env, "GTK_DEBUG_AUTO_QUIT", "1", TRUE);

  if (opt_output)
    {
      GError *err = NULL;
      GFile *file;
      const char *subdir;

      file = g_file_new_for_commandline_arg (opt_output);

      subdir = g_getenv ("TEST_OUTPUT_SUBDIR");
      if (subdir)
        {
          GFile *child = g_file_get_child (file, subdir);
          g_object_unref (file);
          file = child;
        }

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
      SysprofProfiler *profiler;
      int fd;
      char *name;
      SysprofCaptureWriter *writer;
      SysprofCaptureReader *reader;
      SysprofCaptureCursor *cursor;
      SysprofCaptureCondition *condition;
      char **child_argv;

      fd = g_file_open_tmp ("gtk.XXXXXX.syscap", &name, &error);
      if (error)
        g_error ("Create syscap file: %s", error->message);

      child_argv = g_new (char *, argc);
      for (j = 0; j + 1 < argc; j++)
        child_argv[j] = argv[j + 1];
      child_argv[argc - 1] = NULL;

      writer = sysprof_capture_writer_new_from_fd (fd, 0);
      if (!writer)
        g_error ("Failed to create capture writer: %s", g_strerror (errno));

      profiler = sysprof_local_profiler_new ();
      sysprof_profiler_set_whole_system (profiler, FALSE);
      sysprof_profiler_set_spawn (profiler, TRUE);
      sysprof_profiler_set_spawn_argv (profiler, (const char * const *)child_argv);
      sysprof_profiler_set_spawn_cwd (profiler, workdir);
      sysprof_profiler_set_spawn_env (profiler, (const char * const *)spawn_env);
      sysprof_profiler_set_writer (profiler, writer);

      sysprof_capture_writer_unref (writer);
      g_free (child_argv);

      g_signal_connect_swapped (profiler, "failed", G_CALLBACK (on_failure_cb), NULL);
      g_signal_connect_swapped (profiler, "stopped", G_CALLBACK (on_stopped_cb), NULL);

      g_idle_add (start_in_main, profiler);
      g_main_loop_run (main_loop);

      if (failure)
        g_error ("Run child: %s", failure->message);

      reader = sysprof_capture_writer_create_reader (writer);
      if (!reader)
        g_error ("Opening syscap file: %s", g_strerror (errno));

      sysprof_capture_writer_unref (writer);

      data.mark = opt_mark ? opt_mark : "css validation";
      data.detail = opt_detail ? opt_detail : NULL;
      data.do_start = opt_start_time;
      data.start_time = sysprof_capture_reader_get_start_time (reader);
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

  g_free (workdir);

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
