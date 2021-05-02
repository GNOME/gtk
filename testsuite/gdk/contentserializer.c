#include <stdlib.h>

#include <gtk/gtk.h>

typedef struct {
  GOutputStream *ostream;
  GInputStream *istream;
  const char *mime_type;
  GValue value;
  gboolean done;
} TestData;

static void
deserialize_done (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GError *error = NULL;
  gboolean res;
  TestData *data = user_data;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_VALUE_TYPE (&data->value));

  res = gdk_content_deserialize_finish (result, &value, &error);
  g_assert_true (res);
  g_assert_no_error (error);

  if (G_VALUE_TYPE (&data->value) == G_TYPE_STRING)
    g_assert_cmpstr (g_value_get_string (&data->value), ==, g_value_get_string (&value));
  else if (G_VALUE_TYPE (&data->value) == GDK_TYPE_RGBA)
    {
      GdkRGBA *c1 = g_value_get_boxed (&data->value);
      GdkRGBA *c2 = g_value_get_boxed (&value);

      g_assert_true (gdk_rgba_equal (c1, c2));
    }
  else if (G_VALUE_TYPE (&data->value) == G_TYPE_FILE)
    {
      GFile *f1 = g_value_get_object (&data->value);
      GFile *f2 = g_value_get_object (&value);

      g_assert_true (g_file_equal (f1, f2));
    }
  else if (G_VALUE_TYPE (&data->value) == GDK_TYPE_FILE_LIST)
    {
      GSList *s1 = g_value_get_boxed (&data->value);
      GSList *s2 = g_value_get_boxed (&value);
      GSList *l1, *l2;

      g_assert_cmpint (g_slist_length (s1), ==, g_slist_length (s2));
      for (l1 = s1, l2 = s2; l1 != NULL; l1 = l1->next, l2 = l2->next)
        {
          GFile *f1 = l1->data;
          GFile *f2 = l2->data;
          g_assert_true (g_file_equal (f1, f2));
        }
    }
  else
    g_assert_not_reached ();

  g_value_unset (&value);

  data->done = TRUE;
  g_main_context_wakeup (NULL);
}

static void
serialize_done (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  GError *error = NULL;
  gboolean res;
  TestData *data = user_data;
  gpointer serialized_data;
  gsize serialized_len;

  res = gdk_content_serialize_finish (result, &error);
  g_assert_true (res);
  g_assert_no_error (error);

  serialized_data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (data->ostream));
  serialized_len = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (data->ostream));
  data->istream = g_memory_input_stream_new_from_data (serialized_data, serialized_len, NULL);

  gdk_content_deserialize_async (data->istream,
                                 data->mime_type,
                                 G_VALUE_TYPE (&data->value),
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 deserialize_done,
                                 data);
}

static void
test_content_roundtrip (const GValue *value,
                        const char   *mime_type)
{
  TestData data = { 0, };

  data.ostream = g_memory_output_stream_new_resizable ();
  data.mime_type = g_strdup (mime_type);
  g_value_init (&data.value, G_VALUE_TYPE (value));
  g_value_copy (value, &data.value);
  data.done = FALSE;

  gdk_content_serialize_async (data.ostream,
                               data.mime_type,
                               &data.value,
                               G_PRIORITY_DEFAULT,
                               NULL,
                               serialize_done,
                               &data);

  while (!data.done)
    g_main_context_iteration (NULL, TRUE);

  g_object_unref (data.ostream);
  g_object_unref (data.istream);
  g_value_unset (&data.value);
}

static void
test_content_text_plain_utf8 (void)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "ABCDEF12345");
  test_content_roundtrip (&value, "text/plain;charset=utf-8");
  g_value_unset (&value);
}

static void
test_content_text_plain (void)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "ABCDEF12345");
  test_content_roundtrip (&value, "text/plain");
  g_value_unset (&value);
}

static void
test_content_color (void)
{
  GdkRGBA color;
  GValue value = G_VALUE_INIT;

  gdk_rgba_parse (&color, "magenta");
  g_value_init (&value, GDK_TYPE_RGBA);
  g_value_set_boxed (&value, &color);
  test_content_roundtrip (&value, "application/x-color");
  g_value_unset (&value);
}

static void
test_content_file (void)
{
  GFile *file;
  GValue value = G_VALUE_INIT;

  file = g_file_new_for_path ("/etc/passwd");
  g_value_init (&value, G_TYPE_FILE);
  g_value_set_object (&value, file);
  test_content_roundtrip (&value, "text/uri-list");
  g_value_unset (&value);
  g_object_unref (file);
}

static void
test_content_files (void)
{
  GSList *files;
  GValue value = G_VALUE_INIT;

  files = NULL;
  files = g_slist_append (files, g_file_new_for_path ("/etc/passwd"));
  files = g_slist_append (files, g_file_new_for_path ("/boot/ostree"));

  g_value_init (&value, GDK_TYPE_FILE_LIST);
  g_value_set_boxed (&value, files);
  test_content_roundtrip (&value, "text/uri-list");
  g_value_unset (&value);
  g_slist_free_full (files, g_object_unref);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/content/text_plain_utf8", test_content_text_plain_utf8);
  g_test_add_func ("/content/text_plain", test_content_text_plain);
  g_test_add_func ("/content/color", test_content_color);
  g_test_add_func ("/content/file", test_content_file);
  g_test_add_func ("/content/files", test_content_files);

  return g_test_run ();
}
