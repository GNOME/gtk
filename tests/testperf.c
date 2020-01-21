#include <sysprof-capture.h>

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

      if (strcmp (mark->name, data->group) == 0)
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

  if (argc != 2)
    {
      g_print ("Usage: testperf SYSCAP\n");
      exit (1);
    }

  reader = sysprof_capture_reader_new (argv[1], &error);
  g_assert_no_error (error);

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

  g_print ("%d marks for '%s', first %g, min %g, max %g, avg %g\n",
           data.count,
           data.group,
           MILLISECONDS (data.first),
           MILLISECONDS (data.min),
           MILLISECONDS (data.max),
           MILLISECONDS (data.total / data.count));
}
