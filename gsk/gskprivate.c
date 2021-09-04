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

GskTextRenderFlags
gsk_text_render_flags_from_cairo (const cairo_font_options_t *options)
{
  GskTextRenderFlags flags = GSK_TEXT_RENDER_NONE;

  if (cairo_font_options_get_antialias (options) != CAIRO_ANTIALIAS_NONE)
    flags |= GSK_TEXT_RENDER_ANTIALIAS;

  if (cairo_font_options_get_hint_metrics (options) == CAIRO_HINT_METRICS_ON)
    flags |= GSK_TEXT_RENDER_HINT_METRICS;

  switch (cairo_font_options_get_hint_style (options))
    {
    case CAIRO_HINT_STYLE_DEFAULT:
    case CAIRO_HINT_STYLE_NONE:
      break;
    case CAIRO_HINT_STYLE_SLIGHT:
      flags |= GSK_TEXT_RENDER_HINT_OUTLINES_SLIGHT;
      break;
    case CAIRO_HINT_STYLE_MEDIUM:
      flags |= GSK_TEXT_RENDER_HINT_OUTLINES_MEDIUM;
      break;
    case CAIRO_HINT_STYLE_FULL:
      flags |= GSK_TEXT_RENDER_HINT_OUTLINES_FULL;
      break;
    default:
      g_assert_not_reached ();
    }

  return flags;
}

void
gsk_text_render_flags_to_cairo (GskTextRenderFlags    flags,
                                cairo_font_options_t *options)
{
  cairo_font_options_set_hint_metrics (options, flags & GSK_TEXT_RENDER_HINT_METRICS
                                                ? CAIRO_HINT_METRICS_ON
                                                : CAIRO_HINT_METRICS_OFF);
  cairo_font_options_set_antialias (options, flags & GSK_TEXT_RENDER_ANTIALIAS
                                             ? CAIRO_ANTIALIAS_GRAY
                                             : CAIRO_ANTIALIAS_NONE);
  switch (flags & ~(GSK_TEXT_RENDER_ANTIALIAS | GSK_TEXT_RENDER_HINT_METRICS))
    {
    case GSK_TEXT_RENDER_NONE:
      cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
      break;
    case GSK_TEXT_RENDER_HINT_OUTLINES_SLIGHT:
      cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_SLIGHT);
      break;
    case GSK_TEXT_RENDER_HINT_OUTLINES_MEDIUM:
      cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_MEDIUM);
      break;
    case GSK_TEXT_RENDER_HINT_OUTLINES_FULL:
      cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_FULL);
      break;
    default:
      g_assert_not_reached ();
    }
}
