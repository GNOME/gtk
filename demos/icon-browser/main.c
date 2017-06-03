#include <string.h>
#include <gtk/gtk.h>
#include <iconbrowserapp.h>
#include <fuzzy/dzl-fuzzy-index-builder.h>

static void
build_fuzzy_index (void)
{
  DzlFuzzyIndexBuilder *builder;
  GFile *file;
  GKeyFile *kf;
  char *data;
  gsize length;
  char **groups;
  int i;
  GFile *outfile;
  GError *error = NULL;

  builder = dzl_fuzzy_index_builder_new ();
  dzl_fuzzy_index_builder_set_case_sensitive (builder, FALSE);

  file = g_file_new_for_uri ("resource:/org/gtk/iconbrowser/gtk/icon.list");
  g_file_load_contents (file, NULL, &data, &length, NULL, NULL);

  kf = g_key_file_new ();
  g_key_file_load_from_data (kf, data, length, G_KEY_FILE_NONE, NULL);

  groups = g_key_file_get_groups (kf, &length);
  for (i = 0; i < length; i++)
    {
      const char *context;
      char **keys;
      gsize len;
      int j;

      context = groups[i];

      keys = g_key_file_get_keys (kf, context, &len, NULL);
      for (j = 0; j < len; j++)
        {
          const char *key = keys[j];
          char *symbolic;

          if (strcmp (key, "Name") == 0 || strcmp (key, "Description") == 0)
            continue;

          dzl_fuzzy_index_builder_insert (builder, key, g_variant_new_string (key));

          symbolic = g_strconcat (key, "-symbolic", NULL);

          dzl_fuzzy_index_builder_insert (builder, symbolic, g_variant_new_string (symbolic));

          g_free (symbolic);
        }
      g_strfreev (keys);
    }
  g_strfreev (groups);

  outfile = g_file_new_for_path ("icon.index");

  if (!dzl_fuzzy_index_builder_write (builder, outfile, G_PRIORITY_DEFAULT, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
    }
  else
    {
      g_print ("icon.index written\n");
    }

  g_object_unref (builder);
}

int
main (int argc, char *argv[])
{
  if (argc == 2 && strcmp (argv[1], "--generate-index") == 0)
    {
      build_fuzzy_index ();
      return 0;
    }

  return g_application_run (G_APPLICATION (icon_browser_app_new ()), argc, argv);
}
