#include <sysprof-capture.h>
#include <gio/gio.h>

typedef struct {
  const char *group;
  int count;
  gint64 total;
  gint64 first;
  gint64 min;
  gint64 max;
} Data;

static gboolean
callback (const SysprofCaptureFrame *frame,
          gpointer              user_data)
{
  Data *data = user_data;

  if (frame->type == SYSPROF_CAPTURE_FRAME_MARK)
    {
      SysprofCaptureMark *mark = (SysprofCaptureMark *)frame;
      gint64 value = mark->duration;
      if (strcmp (mark->group, "gtk") == 0 &&
          strcmp (mark->name, data->group) == 0)
        {
          if (data->count == 0)
            data->first = value;
          data->count++;
          data->total += value;
          if (data->max < value)
            data->max = value;
          if (data->min > value)
            data->min = value;
        }
    }

  return TRUE;
}

#define MILLISECONDS(v) ((v) / (1000.0 * G_TIME_SPAN_MILLISECOND))

int
main (int argc, char *argv[])
{
  SysprofCaptureReader *reader;
  SysprofCaptureCursor *cursor;
  SysprofCaptureCondition *condition;
  GError *error = NULL;
  Data data;
  SysprofCaptureFrameType type;
  int fd;
  gchar *name;
  char fd_str[20];
  GSubprocessLauncher *launcher;
  GSubprocess *subprocess;

  if (argc < 2)
    {
      g_print ("Usage: testperf COMMANDLINE\n");
      exit (1);
    }

  fd = g_file_open_tmp ("gtk.XXXXXX.syscap", &name, &error);
  if (error)
    g_error ("Create syscap file: %s", error->message);

  launcher = g_subprocess_launcher_new (0);
  g_subprocess_launcher_take_fd (launcher, fd, fd);
  g_snprintf (fd_str, sizeof (fd_str), "%d", fd);
  g_subprocess_launcher_setenv (launcher, "GTK_TRACE_FD", fd_str, TRUE);

  subprocess = g_subprocess_launcher_spawnv (launcher, (const char *const *)argv + 1, &error);
  if (error)
    g_error ("Launch child: %s", error->message);

  if (!g_subprocess_wait (subprocess, NULL, &error))
    g_error ("Run child: %s", error->message);

  
  reader = sysprof_capture_reader_new (name, &error);
  if (error)
    g_error ("Opening syscap file: %s", error->message);

  data.group = "style";
  data.count = 0;
  data.total = 0;
  data.min = G_MAXINT64;
  data.max = 0;

  cursor = sysprof_capture_cursor_new (reader);

  type = SYSPROF_CAPTURE_FRAME_MARK;
  condition = sysprof_capture_condition_new_where_type_in (1, &type);
  sysprof_capture_cursor_add_condition (cursor, condition);

  sysprof_capture_cursor_foreach (cursor, callback, &data);

  if (data.count == 0)
    {
      g_print ("No marks for '%s' found.\n", data.group);
      return 0;
    }

  g_print ("%d marks for '%s', first %g, min %g, max %g, avg %g\n",
           data.count,
           data.group,
           MILLISECONDS (data.first),
           MILLISECONDS (data.min),
           MILLISECONDS (data.max),
           MILLISECONDS (data.total / data.count));
}
