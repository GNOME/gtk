#include "gdk.h"
#include "gdkprivate-nanox.h"

GdkColorContext *
gdk_color_context_new (GdkVisual   *visual,
		       GdkColormap *colormap)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

GdkColorContext *
gdk_color_context_new_mono (GdkVisual   *visual,
			    GdkColormap *colormap)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

void
gdk_color_context_free (GdkColorContext *cc)
{
		g_message("unimplemented %s", __FUNCTION__);
}


gulong
gdk_color_context_get_pixel (GdkColorContext *cc,
			     gushort          red,
			     gushort          green,
			     gushort          blue,
			     gint            *failed)
{
  return RGB2PIXEL(red, green, blue);
}

void
gdk_color_context_get_pixels (GdkColorContext *cc,
			      gushort         *reds,
			      gushort         *greens,
			      gushort         *blues,
			      gint             ncolors,
			      gulong          *colors,
			      gint            *nallocated)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_color_context_get_pixels_incremental (GdkColorContext *cc,
					  gushort         *reds,
					  gushort         *greens,
					  gushort         *blues,
					  gint             ncolors,
					  gint            *used,
					  gulong          *colors,
					  gint            *nallocated)
{
		g_message("unimplemented %s", __FUNCTION__);
}

gint
gdk_color_context_query_color (GdkColorContext *cc,
			       GdkColor        *color)
{
  return gdk_color_context_query_colors (cc, color, 1);
}

gint
gdk_color_context_query_colors (GdkColorContext *cc,
				GdkColor        *colors,
				gint             num_colors)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}

gint
gdk_color_context_add_palette (GdkColorContext *cc,
			       GdkColor        *palette,
			       gint             num_palette)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}

void
gdk_color_context_init_dither (GdkColorContext *cc)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_color_context_free_dither (GdkColorContext *cc)
{
		g_message("unimplemented %s", __FUNCTION__);
}

gulong
gdk_color_context_get_pixel_from_palette (GdkColorContext *cc,
					  gushort         *red,
					  gushort         *green,
					  gushort         *blue,
					  gint            *failed)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}

guchar
gdk_color_context_get_index_from_palette (GdkColorContext *cc,
					  gint            *red,
					  gint            *green,
					  gint            *blue,
					  gint            *failed)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}
