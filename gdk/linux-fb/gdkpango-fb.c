#include <config.h>
#include <glib.h>
#include "gdkprivate-fb.h"
#include "gdkpango.h"

#include <pango/pangoft2.h>

PangoContext *
gdk_pango_context_get_for_screen (GdkScreen *screen)
{
  return pango_ft2_get_context (75.0, 75.0);
}
