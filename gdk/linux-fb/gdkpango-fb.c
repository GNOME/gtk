#include <glib.h>
#include "gdkprivate-fb.h"
#include "gdkpango.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#include <pango/pango.h>
#include <pango/pango-modules.h>

#include <freetype/freetype.h>
#include <freetype/ftglyph.h>

#if !defined(FREETYPE_MAJOR) || FREETYPE_MAJOR != 2
#error "We need Freetype 2.0"
#endif

#define PANGO_RENDER_TYPE_FB "PangoRenderTypeFB"

#define PANGO_SCALE_26_6 (PANGO_SCALE / (1<<6))
#define PANGO_PIXELS_26_6(d)				\
  (((d) >= 0) ?						\
   ((d) + PANGO_SCALE_26_6 / 2) / PANGO_SCALE_26_6 :	\
   ((d) - PANGO_SCALE_26_6 / 2) / PANGO_SCALE_26_6)
#define PANGO_UNITS_26_6(d) (PANGO_SCALE_26_6 * (d))

typedef struct {
  PangoFontMap parent_instance;

  GPtrArray *all_descs;
  GHashTable *all_fonts;

  int n_fonts;

  double resolution;
} PangoFBFontMap;

typedef struct {
  PangoFontMapClass parent_class;
} PangoFBFontMapClass;

typedef struct {
  PangoFontDescription desc;
  FT_Face ftf;
} PangoFBFontListing;

FT_Library gdk_fb_ft_lib = NULL;

#define MAX_ALIASES 3

typedef struct {
  PangoFontDescription alias;
  PangoFontDescription descriptions[MAX_ALIASES];
} PangoFBAlias;

static PangoFBAlias alias_table[] =
{
  /* sans: */
  {
    {"Sans", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    {
      {"Arial", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
      {"URW Gothic L", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Sans", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    {
      {"Arial", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
      {"URW Gothic L", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Sans", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    {
      {"Arial", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
      {"URW Gothic L", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Sans", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    {
      {"Arial", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
      {"URW Gothic L", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    }
  },

  /* serif: */
  {
    {"Serif", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    {
      {"Times New Roman", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
      {"URW Bookman L", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Serif", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    {
      {"Times New Roman", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
      {"URW Bookman L", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Serif", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    {
      {"Times New Roman", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
      {"URW Bookman L", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Serif", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    {
      {"Times New Roman", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
      {"URW Bookman L", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    }
  },
  
  /* monospace: */
  {
    {"Monospace", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    {
      {"Courier New", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
      {"Courier", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Monospace", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    {
      {"Courier New", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
      {"Courier", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_NORMAL, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Monospace", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    {
      {"Courier New", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
      {"Courier", PANGO_STYLE_NORMAL, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    }
  },
  {
    {"Monospace", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    {
      {"Courier New", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
      {"Courier", PANGO_STYLE_OBLIQUE, PANGO_VARIANT_NORMAL, PANGO_WEIGHT_BOLD, PANGO_STRETCH_NORMAL},
    }
  },
};

#define ALIAS_TABLE_LEN (sizeof(alias_table)/sizeof(PangoFBAlias))

void
gdk_fb_font_init (void)
{
  FT_Init_FreeType (&gdk_fb_ft_lib);
}

void
gdk_fb_font_fini (void)
{
  FT_Done_FreeType (gdk_fb_ft_lib);
}

void              pango_fb_font_set_size          (PangoFont                    *font);
static void       pango_fb_font_map_init          (PangoFBFontMap               *fontmap);
static PangoFont *pango_fb_font_map_load_font     (PangoFontMap                 *fontmap,
						   const PangoFontDescription   *desc);
static void       pango_fb_font_map_list_fonts    (PangoFontMap                 *fontmap,
						   const gchar                  *family,
						   PangoFontDescription       ***descs,
						   int                          *n_descs);
static void       pango_fb_font_map_list_families (PangoFontMap                 *fontmap,
						   gchar                      ***families,
						   int                          *n_families);


static void
pango_fb_font_map_class_init (PangoFBFontMapClass *class)
{
  class->parent_class.load_font = pango_fb_font_map_load_font;
  class->parent_class.list_fonts = pango_fb_font_map_list_fonts;
  class->parent_class.list_families = pango_fb_font_map_list_families;
}

GType
pango_fb_font_map_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (PangoFBFontMapClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) pango_fb_font_map_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (PangoFBFontMap),
        0,              /* n_preallocs */
        (GInstanceInitFunc) pango_fb_font_map_init,
      };
      
      object_type = g_type_register_static (PANGO_TYPE_FONT_MAP,
                                            "PangoFBFontMap",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

static gboolean
pango_font_description_equal_nosize (const PangoFontDescription *d1,
				     const PangoFontDescription *d2)
{
  return
    (g_strcasecmp (d1->family_name, d2->family_name)==0) &&
    d1->style == d2->style &&
    d1->variant == d2->variant &&
    d1->weight == d2->weight &&
    d1->stretch == d2->stretch;
}


static PangoFont *
pango_fb_lookup_descr (PangoFontMap *fontmap,
		       const PangoFontDescription *desc,
		       const PangoFontDescription *save_as)
{
  PangoFBFontMap *fbfm = (PangoFBFontMap *)fontmap;
  PangoFBFont *retval;
  PangoFBFontListing *fl = NULL;
  int i;

  retval = g_hash_table_lookup (fbfm->all_fonts, desc);

  if (retval)
    {
      g_object_ref (G_OBJECT(retval));
      return (PangoFont *)retval;
    }

  for (i = 0; i < fbfm->all_descs->len; i++)
    {
      fl = g_ptr_array_index (fbfm->all_descs, i);

      if (pango_font_description_equal_nosize(desc, &fl->desc))
	break;
    }
  
  if (i >= fbfm->all_descs->len)
    return NULL;

  retval = (PangoFBFont *)g_object_new (PANGO_TYPE_FB_FONT, NULL);

  retval->desc = *save_as;
  retval->desc.family_name = g_strdup (save_as->family_name);
  retval->ftf = fl->ftf;

  g_hash_table_insert (fbfm->all_fonts, &retval->desc, retval);
  g_object_ref (G_OBJECT (retval)); /* XXX FIXME: We have to keep the font in the cache
				       forever because I'm too clueless to see
				       signals in gobject */

  return (PangoFont *)retval;
  
}

static PangoFont *
pango_fb_font_map_load_font (PangoFontMap *fontmap,
			     const PangoFontDescription *desc)
{
  PangoFont *font;
  const PangoFontDescription *descs;
  const PangoFontDescription *alias;
  int i, j;

  font = pango_fb_lookup_descr (fontmap, desc, desc);

  if (font)
    return font;

  for (i=0;i<ALIAS_TABLE_LEN;i++)
    {
      alias = &alias_table[i].alias;
      
      if (pango_font_description_equal_nosize (desc, alias))
	{
	  descs = &alias_table[i].descriptions[0];
	  j = 0;
	  while ((j < MAX_ALIASES) &&
		 (descs[j].family_name != NULL))
	    {
	      font = pango_fb_lookup_descr (fontmap, &descs[j], desc);
	      if (font)
		return font;
	      j++;
	    }
	  return NULL;
	}
    }
  return NULL;
}

static void
list_fonts (PangoFBFontMap *fm,
	    const char *family,
	    GPtrArray *descs,
	    const char *dir)
{
  DIR *dirh;
  struct dirent *dent;

  dirh = opendir (dir);
  if (!dirh)
    return;

  while ((dent = readdir (dirh)))
    {
      PangoFBFontListing *pfd;
      char *ctmp;
      char buf[1024];
      FT_Face ftf;
      FT_Error ec;
      int i = 0, n = 1;

      ctmp = strrchr (dent->d_name, '.');
      if (!ctmp ||
	  (strcasecmp (ctmp, ".ttf") &&
	   strcasecmp (ctmp, ".pfa") &&
	   strcasecmp (ctmp, ".pfb")))
	continue;

      g_snprintf (buf, sizeof(buf), "%s/%s", dir, dent->d_name);

      while (i < n)
	{
	  ec = FT_New_Face (gdk_fb_ft_lib, buf, i, &ftf);
	  if (ec)
	    break; /* error opening */

	  n = ftf->num_faces;

	  if (!ftf->family_name || !ftf->style_name)
	    {
	      g_warning ("No family/style on %s", buf);
	      FT_Done_Face (ftf);
	      break;
	    }

	  pfd = g_new0 (PangoFBFontListing, 1);
	  /* Now add the item */
	  if (ftf->family_name[0] == '/')
	    pfd->desc.family_name = g_strdup (ftf->family_name+1);
	  else
	    pfd->desc.family_name = g_strdup (ftf->family_name);

	  pfd->desc.style = PANGO_STYLE_NORMAL;
	  pfd->desc.variant = PANGO_VARIANT_NORMAL;
	  pfd->desc.weight = PANGO_WEIGHT_NORMAL;
	  pfd->desc.stretch = PANGO_STRETCH_NORMAL;

	  if (ftf->style_name)
	    {
	      char lcstr[512];
	      strcpy (lcstr, ftf->style_name);
	      g_strdown (lcstr);

	      if (strstr (lcstr, "italic"))
		pfd->desc.style = PANGO_STYLE_ITALIC;
	      else if (strstr (lcstr, "oblique"))
		pfd->desc.style = PANGO_STYLE_OBLIQUE;

	      if (strstr (lcstr, "bold"))
		pfd->desc.weight = PANGO_WEIGHT_BOLD;

	      if (strstr (lcstr, "condensed"))
		pfd->desc.stretch = PANGO_STRETCH_CONDENSED;
	      else if (strstr (lcstr, "expanded"))
		pfd->desc.stretch = PANGO_STRETCH_EXPANDED;
	    }

	  pfd->ftf = ftf;

	  g_ptr_array_add (descs, pfd);

	  i++;
	}
    }

  closedir (dirh);
}

static guint
pango_font_description_hash (gconstpointer a)
{
  const PangoFontDescription *fa = a;

  return g_str_hash (fa->family_name) ^ (fa->style + fa->weight + fa->stretch + fa->variant + fa->size);
}

static void
pango_fb_font_map_init (PangoFBFontMap *fontmap)
{
  static const char *font_dirs[] = {
    "/usr/share/fonts/default/TrueType",
    "/usr/share/fonts/default/Type1",
    "/usr/lib/X11/fonts/TrueType",
    "/usr/lib/X11/fonts/Type1",
    NULL
  };
  int i;

  fontmap->all_fonts = g_hash_table_new (pango_font_description_hash, (GEqualFunc)pango_font_description_equal);
  fontmap->all_descs = g_ptr_array_new ();
  for (i = 0; font_dirs[i]; i++)
    list_fonts (fontmap, NULL, fontmap->all_descs, font_dirs[i]);
}

static void
pango_fb_font_map_list_fonts (PangoFontMap *fontmap,
			      const gchar *family,
			      PangoFontDescription ***descs,
			      int *n_descs)
{
  PangoFBFontMap *fbfm = (PangoFBFontMap *)fontmap;
  int i, n;

  *descs = g_new (PangoFontDescription *, fbfm->all_descs->len + ALIAS_TABLE_LEN);
  *n_descs = fbfm->all_descs->len + ALIAS_TABLE_LEN;

  for (i = n = 0; i < fbfm->all_descs->len; i++)
    {
      PangoFontDescription *pfd = g_ptr_array_index (fbfm->all_descs, i);

      if (strcasecmp (family, pfd->family_name))
	continue;

      (*descs)[n++] = pango_font_description_copy (pfd);
    }
  for (i = 0; i < ALIAS_TABLE_LEN; i++)
    {
      PangoFontDescription *pfd = &alias_table[i].alias;

      if (strcasecmp (family, pfd->family_name))
	continue;

      (*descs)[n++] = pango_font_description_copy (pfd);
    }
  *n_descs = n;
  *descs = g_realloc (*descs, n * sizeof(PangoFontDescription *));
}

struct famlist {
  gchar **families;
  int last_added;
};

static void
add_entry (gpointer key, gpointer value, gpointer data)
{
  struct famlist *fl = data;
  fl->families[fl->last_added++] = g_strdup (key);
}

static void
pango_fb_font_map_list_families (PangoFontMap *fontmap,
				 gchar ***families,
				 int *n_families)
{
  PangoFBFontMap *fbfm = (PangoFBFontMap *)fontmap;
  GHashTable *thash = g_hash_table_new (g_str_hash, g_str_equal);
  struct famlist stickittome;
  gchar *family_name;
  int i;

  /* Use a temporary hashtable to uniqueify the family names. */
  
  for(i = 0; i < fbfm->all_descs->len; i++)
    {
      PangoFBFontListing *fl = g_ptr_array_index (fbfm->all_descs, i);
      family_name = fl->desc.family_name;
      g_hash_table_insert (thash, family_name, family_name);
    }
  
  for(i = 0; i < ALIAS_TABLE_LEN; i++)
    {
      family_name = alias_table[i].alias.family_name;
      g_hash_table_insert (thash, family_name, family_name);
    }
  
  *n_families = g_hash_table_size (thash);
  *families = g_new (gchar *, *n_families);

  stickittome.families = *families;
  stickittome.last_added = 0;
  g_hash_table_foreach (thash, add_entry, &stickittome);
  g_hash_table_destroy (thash);
}

static PangoFontMap *
pango_fb_font_map (void)
{
  return g_object_new (pango_fb_font_map_get_type (), NULL);
}

PangoMap *
pango_fb_get_shaper_map (const char *lang)
{
  static guint engine_type_id = 0;
  static guint render_type_id = 0;
  
  if (engine_type_id == 0)
    {
      engine_type_id = g_quark_try_string (PANGO_ENGINE_TYPE_SHAPE);
      render_type_id = g_quark_try_string (PANGO_RENDER_TYPE_FB);
    }

  if (engine_type_id == 0)
    {
      engine_type_id = g_quark_from_static_string (PANGO_ENGINE_TYPE_SHAPE);
      render_type_id = g_quark_from_static_string (PANGO_RENDER_TYPE_FB);
    }
  
  return pango_find_map (lang, engine_type_id, render_type_id);
}


/*************** Font impl *****************/
#define PANGO_FB_FONT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), PANGO_TYPE_FB_FONT, PangoFBFontClass))
#define PANGO_FB_IS_FONT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), PANGO_TYPE_FB_FONT))
#define PANGO_FB_IS_FONT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), PANGO_TYPE_FB_FONT))
#define PANGO_FB_FONT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), PANGO_TYPE_FB_FONT, PangoFBFontClass))

typedef struct _PangoFBFontClass   PangoFBFontClass;
typedef struct _PangoFBMetricsInfo PangoFBMetricsInfo;
typedef struct _PangoFBContextInfo PangoFBContextInfo;

struct _PangoFBMetricsInfo
{
  const char *lang;
  PangoFontMetrics metrics;
};

struct _PangoFBContextInfo
{
};

struct _PangoFBFontClass
{
  PangoFontClass parent_class;
};

static void                  pango_fb_font_class_init        (PangoFBFontClass *class);
static void                  pango_fb_font_init              (PangoFBFont      *font);
static void                  pango_fb_font_finalize          (GObject          *object);

static PangoFontDescription *pango_fb_font_describe          (PangoFont        *font);
static PangoCoverage *       pango_fb_font_get_coverage      (PangoFont        *font,
							      const char       *lang);
static PangoEngineShape *    pango_fb_font_find_shaper       (PangoFont        *font,
							      const char       *lang,
							      guint32           ch);
static void                  pango_fb_font_get_glyph_extents (PangoFont        *font,
							      PangoGlyph        glyph,
							      PangoRectangle   *ink_rect,
							      PangoRectangle   *logical_rect);
static void                  pango_fb_font_get_metrics       (PangoFont        *font,
							      const gchar      *lang,
							      PangoFontMetrics *metrics);

GType
pango_fb_font_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (PangoFBFontClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) pango_fb_font_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (PangoFBFont),
        0,              /* n_preallocs */
        (GInstanceInitFunc) pango_fb_font_init,
      };
      
      object_type = g_type_register_static (PANGO_TYPE_FONT,
                                            "PangoFBFont",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

static void 
pango_fb_font_init (PangoFBFont *font)
{
  font->desc.size = -1;
  font->glyph_info = g_hash_table_new (NULL, NULL);
}

static gboolean
g_free_2(gpointer key, gpointer value, gpointer data)
{
  PangoFBGlyphInfo *pgi = value;
  g_free (pgi->fbd.drawable_data.mem);
  g_free (value);
  return TRUE;
}

static void
pango_fb_font_clear_extent_cache(PangoFBFont *fbf)
{
  g_hash_table_foreach_remove (fbf->glyph_info, g_free_2, NULL);
}

PangoFBGlyphInfo *
pango_fb_font_get_glyph_info (PangoFont *font, PangoGlyph glyph)
{
  PangoFBGlyphInfo *pgi;
  PangoFBFont *fbf = PANGO_FB_FONT (font);
  FT_Bitmap *renderme;
  FT_GlyphSlot g;
  PangoRectangle *my_logical_rect, *my_ink_rect;
  FT_Face ftf;
  FT_Error ec;
  
  ftf = fbf->ftf;

  pango_fb_font_set_size (font);

  pgi = g_hash_table_lookup (fbf->glyph_info, GUINT_TO_POINTER (glyph));
  if (pgi)
    return pgi;

  pgi = g_new0 (PangoFBGlyphInfo, 1);

  ec = FT_Load_Glyph (ftf, glyph, FT_LOAD_DEFAULT);

  g = ftf->glyph;

  if (g->format != ft_glyph_format_bitmap)
    if ((ec = FT_Render_Glyph(g, ft_render_mode_normal)))
      g_warning ("Glyph render failed: %d", ec);
  
  renderme = &g->bitmap;
  pgi->top = g->bitmap_top;
  pgi->left = g->bitmap_left;

  pgi->fbd.drawable_data.mem = g_memdup (renderme->buffer, renderme->pitch * renderme->rows);
  pgi->fbd.drawable_data.rowstride = renderme->pitch;
  pgi->fbd.drawable_data.width = pgi->fbd.drawable_data.lim_x = renderme->width;
  pgi->fbd.drawable_data.height = pgi->fbd.drawable_data.lim_y = renderme->rows;

  switch (renderme->pixel_mode)
    {
    case ft_pixel_mode_mono:
      pgi->fbd.drawable_data.depth = 1;
      break;
    case ft_pixel_mode_grays:
      pgi->fbd.drawable_data.depth = 78;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  my_ink_rect = &pgi->extents[0];
  my_logical_rect = &pgi->extents[1];

  my_ink_rect->x = PANGO_UNITS_26_6 (g->metrics.horiBearingX);
  my_ink_rect->width = PANGO_UNITS_26_6 (g->metrics.width);
  my_ink_rect->y = -PANGO_UNITS_26_6 (g->metrics.horiBearingY);
  my_ink_rect->height = PANGO_UNITS_26_6 (g->metrics.height);
  
  my_logical_rect->x = 0;
  my_logical_rect->width = PANGO_UNITS_26_6 (g->metrics.horiAdvance);
  my_logical_rect->y = -PANGO_UNITS_26_6 (ftf->size->metrics.ascender + 64);
  my_logical_rect->height = PANGO_UNITS_26_6 (ftf->size->metrics.height + 128);
  
  g_hash_table_insert (fbf->glyph_info, GUINT_TO_POINTER(glyph), pgi);

  return pgi;
}

static void pango_fb_font_finalize (GObject         *object)
{
  PangoFBFont *fbf = PANGO_FB_FONT (object);

  /* XXX FIXME g_hash_table_remove(ourfontmap, &fbf->desc); */
  pango_coverage_unref (fbf->coverage);
  g_free (fbf->desc.family_name);
  pango_fb_font_clear_extent_cache (fbf);
  g_hash_table_destroy (fbf->glyph_info);
}

static void
pango_fb_font_class_init (PangoFBFontClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PangoFontClass *font_class = PANGO_FONT_CLASS (class);
  
  object_class->finalize = pango_fb_font_finalize;
  font_class->describe = pango_fb_font_describe;
  font_class->get_coverage = pango_fb_font_get_coverage;
  font_class->find_shaper = pango_fb_font_find_shaper;
  font_class->get_glyph_extents = pango_fb_font_get_glyph_extents;
  font_class->get_metrics = pango_fb_font_get_metrics;
}

static PangoFontDescription *
pango_fb_font_describe (PangoFont *font)
{
  return pango_font_description_copy (&PANGO_FB_FONT(font)->desc);
}

static void
free_coverages_foreach (gpointer key,
			gpointer value,
			gpointer data)
{
  pango_coverage_unref (value);
}

static PangoCoverage *
pango_fb_font_get_coverage (PangoFont        *font,
			    const char       *lang)
{
  int i, n;
  PangoCoverage *retval;
  PangoMap *shape_map;
  GHashTable *coverage_hash;

  if (PANGO_FB_FONT (font)->coverage)
    return pango_coverage_ref (PANGO_FB_FONT (font)->coverage);

  shape_map = pango_fb_get_shaper_map (lang);

  coverage_hash = g_hash_table_new (g_str_hash, g_str_equal);

  retval = pango_coverage_new ();
  n = MIN( 65536, PANGO_FB_FONT (font)->ftf->num_glyphs);
  for(i = 0; i < n; i++)
    {
      PangoCoverageLevel font_level;
      PangoMapEntry *map_entry;

      map_entry = pango_map_get_entry (shape_map, i);

      if (map_entry->info)
	{
	  PangoCoverage *coverage;
	  coverage = g_hash_table_lookup (coverage_hash, map_entry->info->id);
	  if (!coverage)
	    {
	      PangoEngineShape *engine = (PangoEngineShape *)pango_map_get_engine (shape_map, i);

	      coverage = engine->get_coverage (font, lang);
	      g_hash_table_insert (coverage_hash, map_entry->info->id, coverage);
	    }
	  
	  font_level = pango_coverage_get (coverage, i);
	  if (font_level == PANGO_COVERAGE_EXACT && !map_entry->is_exact)
	    font_level = PANGO_COVERAGE_APPROXIMATE;

	  if (font_level != PANGO_COVERAGE_NONE)
	    pango_coverage_set (retval, i, font_level);
	}
    }

  g_hash_table_foreach (coverage_hash, free_coverages_foreach, NULL);
  g_hash_table_destroy (coverage_hash);

  PANGO_FB_FONT (font)->coverage = pango_coverage_ref (retval);

  return retval;
}

static PangoEngineShape *
pango_fb_font_find_shaper (PangoFont        *font,
			   const char       *lang,
			   guint32           ch)
{
  PangoMap *shape_map = NULL;

  shape_map = pango_fb_get_shaper_map (lang);
  return (PangoEngineShape *)pango_map_get_engine (shape_map, ch);
}

void
pango_fb_font_set_size (PangoFont *font)
{
  PangoFBFont *fbf = (PangoFBFont *)font;
  gint height;

  if (PANGO_FB_FONT (font)->desc.size != GPOINTER_TO_UINT (fbf->ftf->generic.data))
    {
      height = PANGO_FB_FONT (font)->desc.size;
      fbf->ftf->generic.data = GUINT_TO_POINTER (height);
      FT_Set_Char_Size (fbf->ftf, 0, PANGO_PIXELS_26_6 (height), 72, 72);
    }
}

static void
pango_fb_font_get_glyph_extents (PangoFont        *font,
				 PangoGlyph        glyph,
				 PangoRectangle   *ink_rect,
				 PangoRectangle   *logical_rect)
{
  PangoFBFont *fbf;
  PangoRectangle *my_extents;
  PangoFBGlyphInfo *gi;

  fbf = PANGO_FB_FONT (font);

  gi = pango_fb_font_get_glyph_info (font, glyph);
  my_extents = gi->extents;

  if (ink_rect)
    *ink_rect = my_extents[0];

  if (logical_rect)
    *logical_rect = my_extents[1];
}

static void
pango_fb_font_get_metrics (PangoFont        *font,
			   const gchar      *lang,
			   PangoFontMetrics *metrics)
{
  FT_Face ftf;

  ftf = PANGO_FB_FONT (font)->ftf;

  pango_fb_font_set_size (font);

  if (metrics)
    {
      metrics->ascent = PANGO_UNITS_26_6 (ftf->size->metrics.ascender);
      metrics->descent = PANGO_UNITS_26_6 (-ftf->size->metrics.descender);
    }
}

/* The following array is supposed to contain enough text to tickle all necessary fonts for each
 * of the languages in the following. Yes, it's pretty lame. Not all of the languages
 * in the following have sufficient text to excercise all the accents for the language, and
 * there are obviously many more languages to include as well.
 */
#if 0
LangInfo lang_texts[] = {
  { "ar", "Arabic  السلام عليكم" },
  { "cs", "Czech (česky)  Dobrý den" },
  { "da", "Danish (Dansk)  Hej, Goddag" },
  { "el", "Greek (Ελληνικά) Γειά σας" },
  { "en", "English Hello" },
  { "eo", "Esperanto Saluton" },
  { "es", "Spanish (Español) ¡Hola!" },
  { "et", "Estonian  Tere, Tervist" },
  { "fi", "Finnish (Suomi)  Hei" },
  { "fr", "French (Français)" },
  { "de", "German Grüß Gott" },
  { "iw", "Hebrew   שלום" },
  { "il", "Italiano  Ciao, Buon giorno" },
  { "ja", "Japanese (日本語) こんにちは, ｺﾝﾆﾁﾊ" },
  { "ko", "Korean (한글)   안녕하세요, 안녕하십니까" },
  { "mt", "Maltese   Ċaw, Saħħa" },
  { "nl", "Nederlands, Vlaams Hallo, Dag" },
  { "no", "Norwegian (Norsk) Hei, God dag" },
  { "pl", "Polish   Dzień dobry, Hej" },
  { "ru", "Russian (Русский)" },
  { "sk", "Slovak   Dobrý deň" },
  { "sv", "Swedish (Svenska) Hej, Goddag" },
  { "tr", "Turkish (Türkçe) Merhaba" },
  { "zh", "Chinese (中文,普通话,汉语)" }
};
#endif

/******** misc gdk tie-ins **********/
PangoContext *
gdk_pango_context_get (void)
{
  PangoContext *retval;
  static PangoFontMap *font_map = NULL;

  if (!font_map)
    font_map = pango_fb_font_map ();

  retval = pango_context_new ();

  pango_context_add_font_map (retval, font_map);

  return retval;
}
