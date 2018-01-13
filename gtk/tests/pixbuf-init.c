#include "config.h"
#include <glib.h>
#include <glib/gstdio.h>

static gboolean
file_exists (const char *filename)
{
  GStatBuf statbuf;

  return g_stat (filename, &statbuf) == 0;
}

void
pixbuf_init (void)
{
  if (file_exists ("../../gdk-pixbuf/libpixbufloader-pnm.la"))
    g_setenv ("GDK_PIXBUF_MODULE_FILE", "../../gdk-pixbuf/gdk-pixbuf.loaders", TRUE);
}
