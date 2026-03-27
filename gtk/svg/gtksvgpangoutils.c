#include "gtksvgpangoutilsprivate.h"
#include "gsk/gskpathbuilder.h"

GskPath *
svg_pango_layout_to_path (PangoLayout *layout)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_path_t *path;
  GskPathBuilder *builder;
  PangoRectangle rect;

  pango_layout_get_pixel_extents (layout, &rect, NULL);
  surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA,
                                            &(cairo_rectangle_t) { rect.x, rect.y, rect.width, rect.height });

  cr = cairo_create (surface);
  pango_cairo_layout_path (cr, layout);
  path = cairo_copy_path (cr);
  builder = gsk_path_builder_new ();
  gsk_path_builder_add_cairo_path (builder, path);
  cairo_path_destroy (path);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return gsk_path_builder_free_to_path (builder);
}
