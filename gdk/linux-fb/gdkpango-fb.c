#include <glib.h>
#include "gdkprivate-fb.h"
#include "gdkpango.h"

#include <pango/pangoft2.h>

PangoContext *
gdk_pango_context_get (void)
{
  return pango_ft2_get_context ();
}
