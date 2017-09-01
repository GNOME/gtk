#include "gskresources.h"
#include "gskprivate.h"

static gpointer
register_resources (gpointer data)
{
  _gsk_register_resource ();
  return NULL;
}

void
gsk_ensure_resources (void)
{
  static GOnce register_resources_once = G_ONCE_INIT;

  g_once (&register_resources_once, register_resources, NULL);
}

int
pango_glyph_string_num_glyphs (PangoGlyphString *glyphs)
{
  int i, count;

  count = 0;
  for (i = 0; i < glyphs->num_glyphs; i++)
    {
      PangoGlyphInfo *gi = &glyphs->glyphs[i];
      if (gi->glyph != PANGO_GLYPH_EMPTY)
        {
          if (!(gi->glyph & PANGO_GLYPH_UNKNOWN_FLAG))
            count++;
        }
    }

  return count;
}

