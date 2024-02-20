#include "gskresources.h"
#include "gskprivate.h"

#include <cairo.h>
#include <pango/pangocairo.h>

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
