#include <glib.h>

#include <sys/stat.h>
#include <stdlib.h>

static gboolean
file_exists (const char *filename)
{
  struct stat statbuf;

  return stat (filename, &statbuf) == 0;
}

void
pixbuf_init ()
{
  if (file_exists ("../gdk-pixbuf/libpixbufloader-pnm.la"))
    putenv ("GDK_PIXBUF_MODULEDIR=../gdk-pixbuf");
}
