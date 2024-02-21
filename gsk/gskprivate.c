#include "gskresources.h"
#include "gskprivate.h"

#include <cairo.h>
#include <pango/pangocairo.h>
#include <math.h>

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

/*< private >
 * gsk_get_unhinted_font:
 * @font: (transfer full): a `PangoFont`
 *
 * Returns a font that is just like @font, but does not apply
 * hinting to its glyphs or metrics.
 *
 * Returns: (transfer full): an unhinted `PangoFont`
 */
PangoFont *
gsk_get_unhinted_font (PangoFont *font)
{
  PangoFont *font2 = font;
  cairo_scaled_font_t *sc;
  cairo_font_options_t *options;

  sc = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
  options = cairo_font_options_create ();
  cairo_scaled_font_get_font_options (sc, options);
  if (cairo_font_options_get_hint_metrics (options) != CAIRO_HINT_METRICS_OFF ||
      cairo_font_options_get_hint_style (options) != CAIRO_HINT_STYLE_NONE)
    {
      font2 = PANGO_FONT (g_object_get_data (G_OBJECT (font), "gsk-unhinted-font"));
      if (!font2)
        {
          PangoFontMap *fontmap = pango_font_get_font_map (font);
          PangoContext *context;
          PangoFontDescription *desc;

          cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
          cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);

          context = pango_font_map_create_context (fontmap);
          pango_cairo_context_set_font_options (context, options);

          desc = pango_font_describe (font);
          font2 = pango_font_map_load_font (fontmap, context, desc);

          pango_font_description_free (desc);
          g_object_unref (context);

          g_object_set_data_full (G_OBJECT (font), "gsk-unhinted-font",
                                  font2, g_object_unref);
        }
    }

  cairo_font_options_destroy (options);

  return font2;
}

PangoFont *
gsk_get_scaled_font (PangoFont *font,
                     float      scale)
{
  GHashTable *fonts;
  int key;
  PangoFont *font2;
  PangoFontDescription *desc;
  int size;
  PangoFontMap *fontmap;
  PangoContext *context;

  key = (int) CLAMP (roundf (scale * PANGO_SCALE), G_MININT, G_MAXINT);

  if (key == PANGO_SCALE)
    return font;

  fonts = (GHashTable *) g_object_get_data (G_OBJECT (font), "gsk-scaled-fonts");

  if (fonts)
    {
      font2 = g_hash_table_lookup (fonts, GINT_TO_POINTER (key));
      if (font2)
        return font2;
    }
  else
    {
      fonts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
      g_object_set_data_full (G_OBJECT (font), "gsk-scaled-fonts",
                              fonts, (GDestroyNotify) g_hash_table_unref);
    }

 desc = pango_font_describe (font);
 size = pango_font_description_get_size (desc);

 if (pango_font_description_get_size_is_absolute (desc))
   pango_font_description_set_absolute_size (desc, size * scale);
 else
   pango_font_description_set_size (desc, (int) roundf (size * scale));

 fontmap = pango_font_get_font_map (font);
 context = pango_font_map_create_context (fontmap);
 font2 = pango_font_map_load_font (fontmap, context, desc);

 pango_font_description_free (desc);
 g_object_unref (context);

 g_hash_table_insert (fonts, GINT_TO_POINTER (key), font2);

 return font2;
}
