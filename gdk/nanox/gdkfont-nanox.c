#include "gdk.h"
#include "gdkprivate-nanox.h"

static GR_GC_ID  gc_for_width = 0;

#define ENSURE_GC if (!gc_for_width) gc_for_width = GrNewGC();

GdkFont*
gdk_font_load (const gchar *font_name)
{
  GdkFont *font;
  GdkFontPrivateX *private;
  GR_FONTID xfont;

  g_return_val_if_fail (font_name != NULL, NULL);

  xfont = GrCreateFont("System", 0, 0);
  if (xfont == 0)
    return NULL;

    {
      private = g_new (GdkFontPrivateX, 1);
      private->xfont = xfont;
      private->base.ref_count = 1;
 
      font = (GdkFont*) private;
      font->type = GDK_FONT_FONT;
      font->ascent =  8;
      font->descent = 4;

    }

  return font;
}

GdkFont*
gdk_fontset_load (const gchar *fontset_name)
{
  return gdk_font_load(fontset_name);
}

void
_gdk_font_destroy (GdkFont *font)
{
  GrUnloadFont(GDK_FONT_XFONT(font));
}

gint
_gdk_font_strlen (GdkFont     *font,
		  const gchar *str)
{
  return strlen(str);
}

gint
gdk_font_id (const GdkFont *font)
{
  return GDK_FONT_XFONT(font);
}

gboolean
gdk_font_equal (const GdkFont *fonta,
                const GdkFont *fontb)
{
  return GDK_FONT_XFONT(fonta) == GDK_FONT_XFONT(fontb);
}

gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
  gint width, height, base;
  ENSURE_GC;
  GrSetGCFont(gc_for_width, GDK_FONT_XFONT(font));
  GrGetGCTextSize(gc_for_width, text, text_length, TF_UTF8, &width, &height, &base);
  return width;
}

gint
gdk_text_width_wc (GdkFont	  *font,
		   const GdkWChar *text,
		   gint		   text_length)
{
  gint width, height, base;
  ENSURE_GC;
  GrSetGCFont(gc_for_width, GDK_FONT_XFONT(font));
  GrGetGCTextSize(gc_for_width, text, text_length, TF_UC32, &width, &height, &base);
  return width;
}


void
gdk_text_extents (GdkFont     *font,
                  const gchar *text,
                  gint         text_length,
		  gint        *lbearing,
		  gint        *rbearing,
		  gint        *width,
		  gint        *ascent,
		  gint        *descent)
{
  gint mwidth, height, base;
  ENSURE_GC;
  GrSetGCFont(gc_for_width, GDK_FONT_XFONT(font));
  GrGetGCTextSize(gc_for_width, text, text_length, TF_UTF8, &mwidth, &height, &base);
  if (width)
	*width = mwidth;
  if (lbearing)
	*lbearing = 0;
  if (rbearing)
	*rbearing = 0;
  if (ascent)
	*ascent = base;
  if (descent)
	*descent = height - base;

}

void
gdk_text_extents_wc (GdkFont        *font,
		     const GdkWChar *text,
		     gint            text_length,
		     gint           *lbearing,
		     gint           *rbearing,
		     gint           *width,
		     gint           *ascent,
		     gint           *descent)
{
  gint mwidth, height, base;
  ENSURE_GC;
  GrSetGCFont(gc_for_width, GDK_FONT_XFONT(font));
  GrGetGCTextSize(gc_for_width, text, text_length, TF_UC32, &mwidth, &height, &base);
  if (width)
	*width = mwidth;
  if (lbearing)
	*lbearing = 0;
  if (rbearing)
	*rbearing = 0;
  if (ascent)
	*ascent = base;
  if (descent)
	*descent = height - base;
}


