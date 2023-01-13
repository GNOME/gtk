#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdktests.h"

static void
test_format (gconstpointer d)
{
  const char *f = d;
  GSList *formats;
  gboolean found;

  found = FALSE;

  formats = gdk_pixbuf_get_formats ();

  for (GSList *l = formats; l && !found; l = l->next)
    {
      GdkPixbufFormat *format = l->data;
      char *name;

      name = gdk_pixbuf_format_get_name (format);

      if (strcmp (name, f) == 0)
        found = TRUE;

      g_free (name);
    }

  g_slist_free (formats);

  g_assert_true (found);
}

void
add_pixbuf_tests (void)
{
  g_test_add_data_func ("/pixbuf/format/png", "png", test_format);
  g_test_add_data_func ("/pixbuf/format/jpeg", "jpeg", test_format);
}
