#include "gdk.h"
#include "gdkprivate-nanox.h"
#include <stdlib.h>
#include <string.h>

GdkColormap*
gdk_colormap_new (GdkVisual *visual,
		  gboolean   private_cmap)
{
  GdkColormap *colormap;
  GdkColormapPrivateX *private;
  int size;
  int i;

  g_return_val_if_fail (visual != NULL, NULL);

  private = g_new (GdkColormapPrivateX, 1);
  colormap = (GdkColormap*) private;

  private->base.visual = visual;
  private->base.ref_count = 1;

  colormap->size = visual->colormap_size;
  colormap->colors = NULL;

  return colormap;
}

void
_gdk_colormap_real_destroy (GdkColormap *colormap)
{
}


void
gdk_colormap_sync (GdkColormap *colormap,
		   gboolean     force)
{
		g_message("unimplemented %s", __FUNCTION__);
}

GdkColormap*
gdk_colormap_get_system (void)
{
  return gdk_colormap_new(gdk_visual_get_system(), 0);
}


gint
gdk_colormap_get_system_size (void)
{
    GR_PALETTE palette;

    GrGetSystemPalette(&palette);
    return palette.count;
}

void
gdk_colormap_change (GdkColormap *colormap,
		     gint         ncolors)
{
		g_message("unimplemented %s", __FUNCTION__);
}

gboolean
gdk_colors_alloc (GdkColormap   *colormap,
		  gboolean       contiguous,
		  gulong        *planes,
		  gint           nplanes,
		  gulong        *pixels,
		  gint           npixels)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 1;
}

static struct cspec {
		char *name;
		int red, green, blue;
} cnames [] = {
		{"white", 0xffff, 0xffff, 0xffff},
		{"black", 0, 0, 0},
		{"red", 0xffff, 0, 0},
		{"green", 0, 0xffff, 0},
		{"blue", 0, 0, 0xffff},
		{NULL}
};

gboolean
gdk_color_parse (const gchar *spec,
		 GdkColor *color)
{
  int size, csize, i, j, shift;
  double dval;
  gchar *end;
  int scale[] = {0, 4096, 256, 16, 1};
  int add[] = {0, 4095, 255, 15, 1};
  
  g_return_val_if_fail(spec != NULL, 0);
  g_return_val_if_fail(color != NULL, 0);

  g_message("color parsing %s", spec);

  if (*spec == '#') {
	spec++;
	size = strlen(spec);
	csize = size/3;
	shift = 16-csize*4;
	if (size > 12 || size % 3)
	  return 0;
	j = spec[csize];
	spec[csize] = 0;
	color->red = strtol(spec, &end, 16) << shift;
	if (end == spec || *end != '\0')
	  return 0;
	spec[csize] = j;
	spec += csize;
	/* green */
	j = spec[csize];
	spec[csize] = 0;
	color->green = strtol(spec, &end, 16) << shift;
	if (end == spec || *end != '\0')
	  return 0;
	spec[csize] = j;
	spec += csize;
	/* blue */
	color->blue = strtol(spec, &end, 16) << shift;
	if (end == spec || *end != '\0')
	  return 0;
	return 1;
  } else if (!strncmp(spec, "rgb:", 4)) {
	spec += 4;
	color->red = strtol(spec, &end, 16);
	if (end == spec || *end != '/')
	  return 0;
	csize = end-spec;
	color->red *= scale[csize];
	color->red += add[csize];
	spec += csize + 1;
	/* green */
	color->green = strtol(spec, &end, 16);
	if (end == spec || *end != '/')
	  return 0;
	csize = end-spec;
	color->green *= scale[csize];
	color->green += add[csize];
	spec += csize + 1;
	/* blue */
	color->blue = strtol(spec, &end, 16);
	if (end == spec || *end != '\0')
	  return 0;
	csize = end-spec;
	color->blue *= scale[csize];
	color->blue += add[csize];
	return 1;
  } else if (!strncmp(spec, "rgbi:", 5)) {
	spec += 5;
	dval = strtod(spec, &end);
	if (end == spec || *end != '/' || dval > 1.0 || dval < 0)
	  return 0;
	color->red = dval*0xffff;
	spec += end-spec + 1;
	/* green */
	dval = strtod(spec, &end);
	if (end == spec || *end != '/' || dval > 1.0 || dval < 0)
	  return 0;
	color->green = dval*0xffff;
	spec += end-spec + 1;
	/* blue */
	dval = strtod(spec, &end);
	if (end == spec || *end != '0' || dval > 1.0 || dval < 0)
	  return 0;
	color->blue = dval*0xffff;
	return 1;
  } else {
    /* use a cdb database, instead, later */
  	for (i=0; cnames[i].name; ++i) {
			if (strcmp(cnames[i].name, spec))
					continue;
			color->red = cnames[i].red;
			color->green = cnames[i].green;
			color->blue = cnames[i].blue;
			return 1;
	}
	if (spec[0] == 'g' && spec[1] == 'r' && (spec[2] == 'a' || spec[2] == 'e') && spec[3] == 'y') {
		dval = strtol(spec+4, NULL, 10)/100;
		color->red = color->green = color->blue = 255 * dval;
		return 1;
	}
  }
  
  return 0;
}

void
gdk_colors_free (GdkColormap *colormap,
		 gulong      *in_pixels,
		 gint         in_npixels,
		 gulong       planes)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_colormap_free_colors (GdkColormap *colormap,
			  GdkColor    *colors,
			  gint         ncolors)
{
		g_message("unimplemented %s", __FUNCTION__);
}


gint
gdk_colormap_alloc_colors (GdkColormap *colormap,
			   GdkColor    *colors,
			   gint         ncolors,
			   gboolean     writeable,
			   gboolean     best_match,
			   gboolean    *success)
{
  int i;

  for (i=0; i < ncolors;++i)
    colors[i].pixel = RGB2PIXEL(colors[i].red>>8, colors[i].green>>8, colors[i].blue>>8);
  success = 1;
  return 1;
}


gboolean
gdk_color_change (GdkColormap *colormap,
		  GdkColor    *color)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 1;
}


