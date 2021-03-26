#include <gdk-pixbuf/gdk-pixbuf.h>

int
main (int argc, char *argv[])
{
  GSList *formats;
  gboolean have_png, have_jpeg;

  have_png = FALSE;
  have_jpeg = FALSE;

  formats = gdk_pixbuf_get_formats ();

  for (GSList *l = formats; l; l = l->next)
    {
      GdkPixbufFormat *format = l->data;
      const char *name;

      name = gdk_pixbuf_format_get_name (format);

      if (strcmp (name, "png") == 0)
        have_png = TRUE;
      else if (strcmp (name, "jpeg") == 0)
        have_jpeg = TRUE;
    }

  if (!have_png || !have_jpeg)
    return 1;

  return 0;
}
