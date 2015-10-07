#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>


int
main (int argc, char **argv)
{
  GtkSettings  *settings;
  GParamSpec  **props;
  guint         n_properties;
  int           i;
  int           max_prop_name_length = 0;
  gchar        *pattern = NULL;

  gtk_init (&argc, &argv);

  if (argc > 1)
    pattern = argv[1];

  settings = gtk_settings_get_default ();
  props = g_object_class_list_properties (G_OBJECT_GET_CLASS (settings), &n_properties);

  for (i = 0; i < n_properties; i ++)
    {
      int len = strlen (props[i]->name);

      if (len > max_prop_name_length)
        max_prop_name_length = len;
    }


  for (i = 0; i < n_properties; i ++)
    {
      GValue      value = {0};
      GParamSpec *prop = props[i];
      gchar      *value_str;
      int         spacing = max_prop_name_length - strlen (prop->name);

      if (pattern && !g_strrstr (prop->name, pattern))
        continue;

      g_value_init (&value, prop->value_type);
      g_object_get_property (G_OBJECT (settings), prop->name, &value);

      if (G_VALUE_HOLDS_ENUM (&value))
        {
          GEnumClass *enum_class = G_PARAM_SPEC_ENUM (prop)->enum_class;
          GEnumValue *enum_value = g_enum_get_value (enum_class, g_value_get_enum (&value));

          value_str = g_strdup (enum_value->value_name);
        }
      else
        {
          value_str = g_strdup_value_contents (&value);
        }

      for (; spacing >= 0; spacing --)
        printf (" ");

      printf ("%s: %s\n", prop->name, value_str);

      g_free (value_str);
      g_value_unset (&value);
    }

  g_free (props);

  return 0;
}
