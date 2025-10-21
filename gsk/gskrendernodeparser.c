/*
 * Copyright © 2019 Benjamin Otte
 *                  Timm Bäder
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 *          Timm Bäder <mail@baedert.org>
 */

#include "config.h"

#include "gskrendernodeparserprivate.h"

#include "gskpath.h"
#include "gskpathbuilder.h"
#include "gskroundedrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskstroke.h"
#include "gsktransformprivate.h"
#include "gskenumtypes.h"
#include "gskcomponenttransferprivate.h"
#include "gskprivate.h"

#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gdk/gdkdmabufprivate.h"
#include "gdk/gdkdmabuffourccprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkdmabuftexturebuilderprivate.h"
#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkmemorytextureprivate.h"
#include "gdk/gdkrgbaprivate.h"
#include "gdk/gdktextureprivate.h"
#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcssdataurlprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtk/css/gtkcssserializerprivate.h"

#ifdef GDK_WINDOWING_WIN32
#include "gdk/win32/gdkd3d12texturebuilder.h"
#include "gdk/win32/gdkd3d12textureprivate.h"
#include "gdk/win32/gdkd3d12utilsprivate.h"
#endif

#ifdef CAIRO_HAS_SCRIPT_SURFACE
#include <cairo-script.h>
#endif
#ifdef HAVE_CAIRO_SCRIPT_INTERPRETER
#include <cairo-script-interpreter.h>
#endif

#include <cairo-gobject.h>
#include <pango/pangocairo.h>

#ifdef HAVE_PANGOFT
#include <pango/pangofc-fontmap.h>
#endif

#ifdef HAVE_PANGOWIN32
#undef STRICT
#include <pango/pangowin32.h>
#endif

#include <hb-subset.h>

#include <glib/gstdio.h>

typedef struct _Context Context;

struct _Context
{
  GHashTable *named_nodes;
  GHashTable *named_textures;
  GHashTable *named_color_states;
  PangoFontMap *fontmap;
};

typedef struct _Declaration Declaration;

struct _Declaration
{
  const char *name;
  gboolean (* parse_func) (GtkCssParser *parser, Context *context, gpointer result);
  void (* clear_func) (gpointer data);
  gpointer result;
};

static void
context_init (Context *context)
{
  memset (context, 0, sizeof (Context));
}

static void
context_finish (Context *context)
{
  g_clear_pointer (&context->named_nodes, g_hash_table_unref);
  g_clear_pointer (&context->named_textures, g_hash_table_unref);
  g_clear_pointer (&context->named_color_states, g_hash_table_unref);
  g_clear_object (&context->fontmap);
}

static guint
parse_declarations (GtkCssParser      *parser,
                    Context           *context,
                    const Declaration *declarations,
                    guint              n_declarations)
{
  guint parsed = 0;
  guint i;

  g_assert (n_declarations < 8 * sizeof (guint));

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);

      for (i = 0; i < n_declarations; i++)
        {
          if (gtk_css_parser_try_ident (parser, declarations[i].name))
            {
              if (parsed & (1 << i))
                {
                  gtk_css_parser_warn_syntax (parser, "Variable \"%s\" defined multiple times", declarations[i].name);
                  /* Unset, just to be sure */
                  parsed &= ~(1 << i);
                  if (declarations[i].clear_func)
                    declarations[i].clear_func (declarations[i].result);
                }

              if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
                {
                  gtk_css_parser_error_syntax (parser, "Expected ':' after variable declaration");
                }
              else
                {
                  if (!declarations[i].parse_func (parser, context, declarations[i].result))
                    {
                      /* nothing to do */
                    }
                  else if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
                    {
                      gtk_css_parser_error_syntax (parser, "Expected ';' at end of statement");
                      if (declarations[i].clear_func)
                        declarations[i].clear_func (declarations[i].result);
                    }
                  else
                    {
                      parsed |= (1 << i);
                    }
                }
              break;
            }
        }
      if (i == n_declarations)
        {
          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
            gtk_css_parser_error_syntax (parser, "No variable named \"%s\"",
                                         gtk_css_token_get_string (gtk_css_parser_get_token (parser)));
          else
            gtk_css_parser_error_syntax (parser, "Expected a variable name");
        }

      gtk_css_parser_end_block (parser);
    }

  return parsed;
}

static gboolean
parse_unsigned (GtkCssParser *parser,
                Context      *context,
                gpointer      out)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      double d;

      if (!gtk_css_parser_consume_number (parser, &d))
        return FALSE;

      *(unsigned *) out = d;
      return TRUE;
    }

  gtk_css_parser_error_value (parser, "Not an allowed value here");
  return FALSE;
}

static gboolean
parse_size (GtkCssParser *parser,
            Context      *context,
            gpointer      out)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      double d;

      if (!gtk_css_parser_consume_number (parser, &d))
        return FALSE;

      *(gsize *) out = d;
      return TRUE;
    }

  gtk_css_parser_error_value (parser, "Not an allowed value here");
  return FALSE;
}

static gboolean
parse_boolean (GtkCssParser *parser,
               Context      *context,
               gpointer      boolean)
{
  if (gtk_css_parser_try_ident (parser, "true"))
    {
      *(gboolean *) boolean = TRUE;
      return TRUE;
    }
  else if (gtk_css_parser_try_ident (parser, "false"))
    {
      *(gboolean *) boolean = FALSE;
      return TRUE;
    }

  gtk_css_parser_error_syntax (parser, "Boolean value must be \"true\" or \"false\"");
  return FALSE;
}

static gboolean
parse_enum (GtkCssParser *parser,
            GType         type,
            gpointer      out_value)
{
  GEnumClass *class;
  GEnumValue *v;
  char *enum_name;

  enum_name = gtk_css_parser_consume_ident (parser);
  if (enum_name == NULL)
    return FALSE;

  class = g_type_class_ref (type);

  v = g_enum_get_value_by_nick (class, enum_name);
  if (v == NULL)
    {
      gtk_css_parser_error_value (parser, "Unknown value \"%s\" for enum \"%s\"",
                                  enum_name, g_type_name (type));
      g_free (enum_name);
      g_type_class_unref (class);
      return FALSE;
    }

  *(int*)out_value = v->value;

  g_free (enum_name);
  g_type_class_unref (class);

  return TRUE;
}

static gboolean
parse_rect (GtkCssParser    *parser,
            Context         *context,
            gpointer         out_rect)
{
  double numbers[4];

  if (!gtk_css_parser_consume_number (parser, &numbers[0]) ||
      !gtk_css_parser_consume_number (parser, &numbers[1]) ||
      !gtk_css_parser_consume_number (parser, &numbers[2]) ||
      !gtk_css_parser_consume_number (parser, &numbers[3]))
    return FALSE;

  graphene_rect_init (out_rect, numbers[0], numbers[1], numbers[2], numbers[3]);

  return TRUE;
}

static gboolean
parse_vec4 (GtkCssParser    *parser,
            Context         *context,
            gpointer         out_vec4)
{
  double numbers[4];

  if (!gtk_css_parser_consume_number (parser, &numbers[0]) ||
      !gtk_css_parser_consume_number (parser, &numbers[1]) ||
      !gtk_css_parser_consume_number (parser, &numbers[2]) ||
      !gtk_css_parser_consume_number (parser, &numbers[3]))
    return FALSE;

  graphene_vec4_init (out_vec4, numbers[0], numbers[1], numbers[2], numbers[3]);

  return TRUE;
}

static GBytes *
consume_bytes (GtkCssParser *parser)
{
  GtkCssLocation start_location;
  GError *error = NULL;
  char *url, *scheme;
  GBytes *bytes;

  start_location = *gtk_css_parser_get_start_location (parser);
  url = gtk_css_parser_consume_url (parser);
  if (url == NULL)
    return NULL;

  scheme = g_uri_parse_scheme (url);
  if (scheme && g_ascii_strcasecmp (scheme, "data") == 0)
    {
      bytes = gtk_css_data_url_parse (url, NULL, &error);
    }
  else
    {
      GFile *file;

      file = gtk_css_parser_resolve_url (parser, url);
      if (file)
        {
          bytes = g_file_load_bytes (file, NULL, NULL, &error);
          g_object_unref (file);
        }
      else
        {
          g_set_error (&error,
                       GTK_CSS_PARSER_ERROR,
                       GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                       "Failed to resolve URL");
          bytes = NULL;
        }
    }

  g_free (scheme);
  g_free (url);

  if (bytes == NULL)
    {
      gtk_css_parser_emit_error (parser,
                                 &start_location,
                                 gtk_css_parser_get_end_location (parser),
                                 error);
      g_clear_error (&error);
    }

  return bytes;
}

static void
clear_bytes (gpointer inout_bytes)
{
  g_clear_pointer ((GBytes **) inout_bytes, g_bytes_unref);
}

static gboolean
parse_bytes (GtkCssParser *parser,
             Context      *context,
             gpointer      out_data)
{
  GBytes **out_bytes = out_data;
  GBytes *bytes;

  bytes = consume_bytes (parser);
  if (bytes == NULL)
    return FALSE;

  *out_bytes = bytes;

  return TRUE;
}

static gboolean
parse_compressed_bytes (GtkCssParser *parser,
                        Context      *context,
                        gpointer      out_data)
{
  GZlibDecompressor *decompressor;
  GError *error = NULL;
  GBytes *decompressed_bytes, *data_bytes;

  if (!parse_bytes (parser, context, &data_bytes))
    return FALSE;

  decompressor = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
  decompressed_bytes = g_converter_convert_bytes (G_CONVERTER (decompressor), data_bytes, &error);
  g_object_unref (decompressor);
  g_bytes_unref (data_bytes);
  if (decompressed_bytes == NULL)
    {
      gtk_css_parser_emit_error (parser,
                                 gtk_css_parser_get_start_location (parser),
                                 gtk_css_parser_get_end_location (parser),
                                 error);
      g_clear_error (&error);
      return FALSE;
    }
  
  *(GBytes **) out_data = decompressed_bytes;

  return TRUE;
}

static gboolean
parse_texture_data (GtkCssParser *parser,
                    Context      *context,
                    gpointer      out_data)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
    {
      char *s;
      gsize i, j, len;
      guchar *data;

      s = gtk_css_parser_consume_string (parser);
      if (s == NULL)
        return FALSE;

      len = strlen (s);

      data = g_malloc (len);
      j = 0;
      for (i = 0; i < len; i++)
        {
          int v1, v2;

          if (g_ascii_isspace (s[i]))
            continue;

          v1 = g_ascii_xdigit_value (s[i]);
          if (v1 < 0)
            {
              gtk_css_parser_error_syntax (parser, "Invalid hex character at position %zu", i);
              continue;
            }
          i++;
          v2 = g_ascii_xdigit_value (s[i]);
          if (v2 < 0)
            {
              gtk_css_parser_error_syntax (parser, "Invalid hex character at position %zu", i);
              continue;
            }

          data[j++] = v1 * 16 + v2;
        }

      *(GBytes **) out_data = g_bytes_new_take (data, j);
      return TRUE;
    }

  return parse_compressed_bytes (parser, context, out_data);
}

static gboolean
parse_color_state (GtkCssParser *parser,
                   Context      *context,
                   gpointer      color_state)
{
  GdkColorState *cs = NULL;

  if (gtk_css_parser_try_ident (parser, "srgb"))
    cs = gdk_color_state_get_srgb ();
  else if (gtk_css_parser_try_ident (parser, "srgb-linear"))
    cs = gdk_color_state_get_srgb_linear ();
  else if (gtk_css_parser_try_ident (parser, "rec2100-pq"))
    cs = gdk_color_state_get_rec2100_pq ();
  else if (gtk_css_parser_try_ident (parser, "rec2100-linear"))
    cs = gdk_color_state_get_rec2100_linear ();
  else if (gtk_css_parser_try_ident (parser, "oklab"))
    cs = gdk_color_state_get_oklab ();
  else if (gtk_css_parser_try_ident (parser, "oklch"))
    cs = gdk_color_state_get_oklch ();
  else if (gtk_css_token_is (gtk_css_parser_get_token (parser), GTK_CSS_TOKEN_STRING))
    {
      char *name = gtk_css_parser_consume_string (parser);

      if (context->named_color_states)
        cs = g_hash_table_lookup (context->named_color_states, name);

      if (!cs)
        {
          gtk_css_parser_error_value (parser, "No color state named \"%s\"", name);
          g_free (name);
          return FALSE;
        }

      g_free (name);
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Expected a valid color state");
      return FALSE;
    }

  *(GdkColorState **) color_state = gdk_color_state_ref (cs);
  return TRUE;
}

static void
clear_color_state (gpointer inout_color_state)
{
  GdkColorState **cs = inout_color_state;

  if (*cs)
    {
      gdk_color_state_unref (*cs);
      *cs = NULL;
    }
}

static gboolean
parse_dmabuf_fourcc (GtkCssParser *parser,
                     Context      *context,
                     gpointer      data)
{
  guint32 fourcc;

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
    {
      char *fourcc_str = gtk_css_parser_consume_string (parser);

      if (strlen (fourcc_str) != 4)
        {
          gtk_css_parser_error_value (parser, "fourccs must be 4 characters long");
          return FALSE;
        }

      fourcc = (fourcc_str[0] <<  0) |
               (fourcc_str[1] <<  8) |
               (fourcc_str[2] << 16) |
               (fourcc_str[3] << 24);
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      double tmp;
      if (!gtk_css_parser_consume_number (parser, &tmp))
        return FALSE;

      fourcc = tmp;
    }
  else
    {
      gtk_css_parser_error_value (parser, "fourccs must be specified as strings or integers");
      return FALSE;
    }

  *(guint32 *) data = fourcc;
  return TRUE;
}

static gboolean
parse_4_unsigned (GtkCssParser *parser,
                  Context      *context,
                  gpointer      data)
{
  unsigned *u = data;

  if (!parse_unsigned (parser, context, &u[0]))
    return FALSE;
  if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA))
    return TRUE;
  if (!parse_unsigned (parser, context, &u[1]))
    return FALSE;
  if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA))
    return TRUE;
  if (!parse_unsigned (parser, context, &u[2]))
    return FALSE;
  if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA))
    return TRUE;
  if (!parse_unsigned (parser, context, &u[3]))
    return FALSE;

  return TRUE;
}

#ifdef HAVE_DMABUF
static void
destroy_fd (gpointer data)
{
  close (GPOINTER_TO_INT (data));
}
#endif

static GdkTexture *
parse_dmabuf_texture (GtkCssParser *parser,
                      Context      *context)
{
  GBytes *bytes = NULL;
  GdkTexture *texture;
  GError *error = NULL;
  GtkCssLocation start_location;
  guint width = 0;
  guint height = 0;
  GdkDmabuf dmabuf = { 0, };
  gboolean premultiplied = FALSE;
  GdkColorState *color_state = NULL;
  unsigned offsets[4] = { 0, }, strides[4] = { 0, };
  const Declaration declarations[] = {
    { "data", parse_texture_data, clear_bytes, &bytes },
    { "width", parse_unsigned, NULL, &width },
    { "height", parse_unsigned , NULL, &height },
    { "fourcc", parse_dmabuf_fourcc, NULL, &dmabuf.fourcc },
    { "premultiplied", parse_boolean, NULL, &premultiplied },
    { "offset", parse_4_unsigned, NULL, offsets },
    { "stride", parse_4_unsigned, NULL, strides },
    { "color-state", parse_color_state, clear_color_state, &color_state }
  };
  guint parse_result;
#ifdef HAVE_DMABUF
  int dmabuf_fd;
#endif

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (parser, "Expected '{' for \"dmabuf\"");
      return NULL;
    }

  start_location = *gtk_css_parser_get_start_location (parser);
  gtk_css_parser_end_block_prelude (parser);

  parse_result = parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  /* deduce number of planes from given amount of strides */
  while (dmabuf.n_planes < GDK_DMABUF_MAX_PLANES && strides[dmabuf.n_planes] > 0)
    {
      dmabuf.planes[dmabuf.n_planes].offset = offsets[dmabuf.n_planes];
      dmabuf.planes[dmabuf.n_planes].stride = strides[dmabuf.n_planes];
      dmabuf.n_planes++;
    }

  if (dmabuf.fourcc == 0)
    {
      g_set_error (&error,
                   GTK_CSS_PARSER_ERROR,
                   GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                   "Cannot create a dmabuf texture without fourcc");
    }
  else if (dmabuf.n_planes == 0)
    {
      g_set_error (&error,
                   GTK_CSS_PARSER_ERROR,
                   GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                   "No stride specified");
    }
  else if (bytes == NULL && !(parse_result & (1 << 0)))
    {
      g_set_error (&error,
                   GTK_CSS_PARSER_ERROR,
                   GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                   "Cannot create a dmabuf texture without data");
    }
  else if (width == 0 && !(parse_result & (1 << 1)))
    {
      g_set_error (&error,
                   GTK_CSS_PARSER_ERROR,
                   GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                   "No width specified");
    }
  else if (height == 0 && !(parse_result & (1 << 2)))
    {
      g_set_error (&error,
                   GTK_CSS_PARSER_ERROR,
                   GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                   "No height specified");
    }
  if (error)
    {
      gtk_css_parser_emit_error (parser,
                                 &start_location,
                                 gtk_css_parser_get_end_location (parser),
                                 error);
      g_clear_pointer (&bytes, g_bytes_unref);
      g_clear_pointer (&color_state, gdk_color_state_unref);
      g_clear_error (&error);
      return NULL;
    }

#ifdef HAVE_DMABUF
  dmabuf_fd = gdk_dmabuf_new_for_bytes (bytes, &error);
  if (dmabuf_fd >= 0)
    {
      GdkDmabufTextureBuilder *builder = gdk_dmabuf_texture_builder_new ();
      gsize i;

      for (i = 0; i < dmabuf.n_planes; i++)
        dmabuf.planes[i].fd = dmabuf_fd;

      gdk_dmabuf_texture_builder_set_dmabuf (builder, &dmabuf);
      gdk_dmabuf_texture_builder_set_premultiplied (builder, premultiplied);
      gdk_dmabuf_texture_builder_set_width (builder, width);
      gdk_dmabuf_texture_builder_set_height (builder, height);
      gdk_dmabuf_texture_builder_set_color_state (builder, color_state);
      texture = gdk_dmabuf_texture_builder_build (builder, destroy_fd, GINT_TO_POINTER (dmabuf_fd), &error);
      if (texture == NULL)
        close (dmabuf_fd);

      g_object_unref (builder);
    }
  else
    texture = NULL;
  if (error)
    {
      gtk_css_parser_emit_error (parser,
                                 &start_location,
                                 gtk_css_parser_get_end_location (parser),
                                 error);
      g_clear_error (&error);
    }
#else
  gtk_css_parser_warn (parser,
                       GTK_CSS_PARSER_WARNING_UNIMPLEMENTED,
                       &start_location,
                       gtk_css_parser_get_end_location (parser),
                       "No dmabuf support available. Using fallback.");
  texture = NULL;
#endif

  if (texture == NULL)
    {
      GdkMemoryLayout layout;

      if (gdk_memory_layout_init_from_dmabuf (&layout, &dmabuf, premultiplied, width, height) && 
          g_bytes_get_size (bytes) >= layout.size)
        {
          if (color_state == NULL)
            {
              if (gdk_memory_format_get_dmabuf_yuv_fourcc (layout.format) == dmabuf.fourcc)
                color_state = gdk_color_state_ref (gdk_color_state_yuv ());
              else
                color_state = gdk_color_state_ref (gdk_color_state_get_srgb ());
            }
          texture = gdk_memory_texture_new_from_layout (bytes, &layout, color_state, NULL, NULL);
        }
      else
        {
          gtk_css_parser_error (parser,
                                GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                                &start_location,
                                gtk_css_parser_get_end_location (parser),
                                "Cannot create fallback texture for this fourcc");
        }
    }

  g_bytes_unref (bytes);
  g_clear_pointer (&color_state, gdk_color_state_unref);

  return texture;
}

static gboolean
parse_memory_format (GtkCssParser *parser,
                     Context      *context,
                     gpointer      out)
{
  gsize i;

  for (i = 0; i < GDK_MEMORY_N_FORMATS; i++)
    {
      if (gtk_css_parser_try_ident (parser, gdk_memory_format_get_name (i)))
        {
          *(GdkMemoryFormat *) out = i;
          return TRUE;
        }
    }

  return parse_enum (parser, GDK_TYPE_MEMORY_FORMAT, out);
}

static GdkTexture *
parse_memory_texture (GtkCssParser *parser,
                      Context      *context)
{
  GBytes *bytes = NULL;
  GdkTexture *texture;
  GError *error = NULL;
  GtkCssLocation start_location;
  GdkMemoryLayout layout = { 0, };
  GdkColorState *color_state = NULL;
  unsigned offsets[4] = { 0, }, strides[4] = { 0, };
  const Declaration declarations[] = {
    { "data", parse_texture_data, clear_bytes, &bytes },
    { "width", parse_size, NULL, &layout.width },
    { "height", parse_size, NULL, &layout.height },
    { "format", parse_memory_format, NULL, &layout.format },
    { "offset", parse_4_unsigned, NULL, offsets },
    { "stride", parse_4_unsigned, NULL, strides },
    { "color-state", parse_color_state, clear_color_state, &color_state }
  };
  guint parse_result;
  gsize i, n_planes;

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (parser, "Expected '{' for \"memory\"");
      return NULL;
    }

  start_location = *gtk_css_parser_get_start_location (parser);
  gtk_css_parser_end_block_prelude (parser);

  parse_result = parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  n_planes = gdk_memory_format_get_n_planes (layout.format);
  for (i = 0; i < n_planes; i++)
    {
      layout.planes[i].offset = offsets[i];
      layout.planes[i].stride = strides[i];
    }

  if (bytes == NULL)
    {
      if (!(parse_result & (1 << 0)))
        gtk_css_parser_error_value (parser, "Cannot create a memory texture without data");
    }
  else
    layout.size = g_bytes_get_size (bytes);

  if (!gdk_memory_layout_is_valid (&layout, &error))
    {
      gtk_css_parser_emit_error (parser,
                                 &start_location,
                                 gtk_css_parser_get_end_location (parser),
                                 error);
      g_clear_error (&error);
      g_clear_pointer (&bytes, g_bytes_unref);
    }

  if (color_state == NULL)
    color_state = gdk_color_state_ref (gdk_color_state_get_srgb ());
  
  if (bytes)
    {
      texture = gdk_memory_texture_new_from_layout (bytes, &layout, color_state, NULL, NULL);
      g_bytes_unref (bytes);
    }
  else
    texture = NULL;

  gdk_color_state_unref (color_state);

  return texture;
}

static gboolean
parse_dxgi_format (GtkCssParser *parser,
                   Context      *context,
                   gpointer      out)
{
  GdkMemoryFormat format;
  guint value;

  if (!parse_unsigned (parser, context, &value))
    return FALSE;

  /* We can ignore premultiplied here, we just need to update the format later */
  if (!gdk_memory_format_find_by_dxgi_format (value, FALSE, &format))
    {
      gtk_css_parser_error_value (parser, "Unhandled DXGI format");
      return FALSE;
    }

  *((GdkMemoryFormat *) out) = format;
  return TRUE;
}

#ifdef GDK_WINDOWING_WIN32
static void
destroy_d3d12_resource (gpointer data)
{
  ID3D12Resource *resource = data;

  ID3D12Resource_Release (resource);
}
#endif

static GdkTexture *
parse_d3d12_texture (GtkCssParser *parser,
                     Context      *context)
{
  GBytes *bytes = NULL;
  GdkTexture *texture;
  GError *error = NULL;
  GtkCssLocation start_location;
  GdkMemoryFormat format = GDK_MEMORY_N_FORMATS;
  guint width = 0;
  guint height = 0;
  gboolean premultiplied = FALSE;
  GdkColorState *color_state = NULL;
  GdkMemoryLayout layout;
#ifdef GDK_WINDOWING_WIN32
  ID3D12Resource *resource;
  GdkD3D12TextureBuilder *builder;
#endif
  const Declaration declarations[] = {
    { "data", parse_texture_data, clear_bytes, &bytes },
    { "width", parse_unsigned, NULL, &width },
    { "height", parse_unsigned , NULL, &height },
    { "format", parse_dxgi_format, NULL, &format },
    { "premultiplied", parse_boolean, NULL, &premultiplied },
    { "color-state", parse_color_state, clear_color_state, &color_state }
  };
  guint parse_result;

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (parser, "Expected '{' for \"d3d12\"");
      return NULL;
    }

  start_location = *gtk_css_parser_get_start_location (parser);
  gtk_css_parser_end_block_prelude (parser);

  parse_result = parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  if (format == GDK_MEMORY_N_FORMATS)
    {
      if (!(parse_result & (1 << 3)))
        g_set_error (&error,
                    GTK_CSS_PARSER_ERROR,
                    GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                    "Cannot create a dmabuf texture without data");
      g_clear_pointer (&bytes, g_bytes_unref);
    }
  else if (bytes == NULL && !(parse_result & (1 << 0)))
    {
      g_set_error (&error,
                   GTK_CSS_PARSER_ERROR,
                   GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                   "Cannot create a dmabuf texture without data");
    }
  else if (width == 0)
    {
      if (!(parse_result & (1 << 1)))
        g_set_error (&error,
                     GTK_CSS_PARSER_ERROR,
                     GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                     "No width specified");
      g_clear_pointer (&bytes, g_bytes_unref);
    }
  else if (height == 0)
    {
      if (!(parse_result & (1 << 2)))
        g_set_error (&error,
                     GTK_CSS_PARSER_ERROR,
                     GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                     "No height specified");
      g_clear_pointer (&bytes, g_bytes_unref);
    }
  else
    {
      gdk_memory_layout_init (&layout, format, width, height, gdk_memory_format_alignment (format));
      if (layout.size > g_bytes_get_size (bytes))
        {
          g_set_error (&error,
                      GTK_CSS_PARSER_ERROR,
                      GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                      "Not enough data for texture. Expected %zu bytes, got %zu",
                      layout.size, g_bytes_get_size (bytes));
        }
    }
  if (error || bytes == NULL || width == 0 || height == 0)
    {
      if (error)
        {
          gtk_css_parser_emit_error (parser,
                                    &start_location,
                                    gtk_css_parser_get_end_location (parser),
                                    error);
          g_clear_error (&error);
        }
      g_clear_pointer (&bytes, g_bytes_unref);
      g_clear_pointer (&color_state, gdk_color_state_unref);
      return NULL;
    }

  if (premultiplied)
    format = gdk_memory_format_get_premultiplied (format);
  if (color_state == NULL)
    color_state = gdk_color_state_ref (gdk_color_state_get_srgb ());

#ifdef GDK_WINDOWING_WIN32
  resource = gdk_d3d12_resource_new_from_bytes (g_bytes_get_data (bytes, NULL),
                                                &layout,
                                                &error);
  if (resource == NULL)
    {
      gtk_css_parser_emit_error (parser,
                                &start_location,
                                gtk_css_parser_get_end_location (parser),
                                error);
      g_bytes_unref (bytes);
      gdk_color_state_unref (color_state);
      return FALSE;
    }

  builder = gdk_d3d12_texture_builder_new();
  gdk_d3d12_texture_builder_set_resource (builder, resource);
  gdk_d3d12_texture_builder_set_premultiplied (builder, premultiplied);
  gdk_d3d12_texture_builder_set_color_state (builder, color_state);
  texture = gdk_d3d12_texture_builder_build (builder, destroy_d3d12_resource, resource, &error);
  if (texture)
    {
      g_clear_pointer (&bytes, g_bytes_unref);
      g_clear_pointer (&color_state, gdk_color_state_unref);
      return texture;
    }

  gtk_css_parser_emit_error (parser,
                            &start_location,
                            gtk_css_parser_get_end_location (parser),
                            error);
  g_clear_error (&error);
#else
  gtk_css_parser_warn (parser,
                       GTK_CSS_PARSER_WARNING_UNIMPLEMENTED,
                       &start_location,
                       gtk_css_parser_get_end_location (parser),
                       "No D3D12 support available. Using fallback.");
#endif

  texture = gdk_memory_texture_new_from_layout (bytes, &layout, color_state, NULL, NULL);
  g_bytes_unref (bytes);
  gdk_color_state_unref (color_state);
  return texture;
}

static GdkTexture *
parse_default_texture (GtkCssParser *parser,
                       Context      *context)
{
  GdkTexture *texture;
  GError *error = NULL;
  GtkCssLocation start_location;
  GBytes *bytes;

  start_location = *gtk_css_parser_get_start_location (parser);
  bytes = consume_bytes (parser);
  if (bytes == NULL)
    return NULL;

  texture = gdk_texture_new_from_bytes (bytes, &error);
  g_bytes_unref (bytes);
  if (texture == NULL)
    {
      gtk_css_parser_emit_error (parser,
                                 &start_location,
                                 gtk_css_parser_get_end_location (parser),
                                 error);
      g_clear_error (&error);
      return NULL;
    }

  return texture;
}

static gboolean
parse_texture (GtkCssParser *parser,
               Context      *context,
               gpointer      out_data)
{
  GdkTexture *texture;
  char *texture_name;

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
    {
      texture_name = gtk_css_parser_consume_string (parser);

      if (context->named_textures)
        texture = g_hash_table_lookup (context->named_textures, texture_name);
      else
        texture = NULL;

      if (texture)
        {
          *(GdkTexture **) out_data = g_object_ref (texture);
          g_free (texture_name);
          return TRUE;
        }
      else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        {
          gtk_css_parser_error_value (parser, "No texture named \"%s\"", texture_name);
          g_free (texture_name);
          return FALSE;
        }

      if (context->named_textures && g_hash_table_lookup (context->named_textures, texture_name))
        {
          gtk_css_parser_error_value (parser, "A texture named \"%s\" already exists.", texture_name);
          g_clear_pointer (&texture_name, g_free);
        }
    }
  else
    texture_name = NULL;

  if (gtk_css_parser_try_ident (parser, "memory"))
    texture = parse_memory_texture (parser, context);
  else if (gtk_css_parser_try_ident (parser, "dmabuf"))
    texture = parse_dmabuf_texture (parser, context);
  else if (gtk_css_parser_try_ident (parser, "d3d12"))
    texture = parse_d3d12_texture (parser, context);
  else
    texture = parse_default_texture (parser, context);

  if (texture == NULL)
    {
      g_free (texture_name);
      return FALSE;
    }
    
  if (texture_name)
    {
      if (context->named_textures == NULL)
        context->named_textures = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                         g_free, g_object_unref);
      g_hash_table_insert (context->named_textures, texture_name, g_object_ref (texture));
    }

  *(GdkTexture **) out_data = texture;
  return TRUE;
}

static void
clear_texture (gpointer inout_texture)
{
  g_clear_object ((GdkTexture **) inout_texture);
}

typedef struct _CairoHookData CairoHookData;
struct _CairoHookData
{
  GtkCssParser *parser;
  graphene_rect_t node_bounds;
  cairo_surface_t *surface;
};

static cairo_surface_t *
csi_hooks_surface_create (void            *closure,
                          cairo_content_t  content,
                          double           width,
                          double           height,
                          long             uid)
{
  CairoHookData *hook = closure;

  if (hook->surface != NULL)
    {
      return cairo_surface_create_similar (hook->surface,
                                           content,
                                           width,
                                           height);
    }

  if (width != hook->node_bounds.size.width || height != hook->node_bounds.size.height)
    {
      gtk_css_parser_error (hook->parser,
                            GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                            gtk_css_parser_get_block_location (hook->parser),
                            gtk_css_parser_get_start_location (hook->parser),
                            "Node size %gx%g does not match script size %gx%g",
                            hook->node_bounds.size.width, hook->node_bounds.size.height,
                            width, height);
    }

  hook->surface = cairo_recording_surface_create (content,
                                                  &(cairo_rectangle_t) {
                                                      hook->node_bounds.origin.x,
                                                      hook->node_bounds.origin.y,
                                                      hook->node_bounds.size.width,
                                                      hook->node_bounds.size.height
                                                  });

  return cairo_surface_reference (hook->surface);
}

static cairo_surface_t *
interpret_cairo_script (GtkCssParser          *parser,
                        const graphene_rect_t *bounds,
                        GBytes                *script)
{
#ifdef HAVE_CAIRO_SCRIPT_INTERPRETER
  cairo_script_interpreter_t *csi;
  CairoHookData hook = { parser, *bounds, NULL };
  cairo_script_interpreter_hooks_t hooks = {
    .closure = &hook,
    .surface_create = csi_hooks_surface_create,
  };

  csi = cairo_script_interpreter_create ();
  cairo_script_interpreter_install_hooks (csi, &hooks);
  cairo_script_interpreter_feed_string (csi, g_bytes_get_data (script, NULL), g_bytes_get_size (script));
  if (hook.surface == NULL)
    {
      gtk_css_parser_error_value (parser, "Cairo script did not create a surface");
    }
  else if (cairo_surface_status (hook.surface) != CAIRO_STATUS_SUCCESS)
    {
      gtk_css_parser_error_value (parser, "Invalid Cairo script: %s", cairo_status_to_string (cairo_surface_status (hook.surface)));
      cairo_script_interpreter_destroy (csi);
      g_clear_pointer (&hook.surface, cairo_surface_destroy);
    }
  if (cairo_script_interpreter_destroy (csi) != CAIRO_STATUS_SUCCESS)
    {
      gtk_css_parser_error_value (parser, "Invalid Cairo script");
      g_clear_pointer (&hook.surface, cairo_surface_destroy);
    }

  return hook.surface;
#else
  gtk_css_parser_warn (parser,
                       GTK_CSS_PARSER_WARNING_UNIMPLEMENTED,
                       gtk_css_parser_get_block_location (parser),
                       gtk_css_parser_get_start_location (parser),
                       "GTK was compiled without script interpreter support. Using fallback pixel data for Cairo node.");
  return NULL;
#endif
}

static gboolean
parse_rounded_rect (GtkCssParser *parser,
                    Context      *context,
                    gpointer      out_rect)
{
  graphene_rect_t r;
  graphene_size_t corners[4];
  double d;
  guint i;

  if (!parse_rect (parser, context, &r))
    return FALSE;

  if (!gtk_css_parser_try_delim (parser, '/'))
    {
      gsk_rounded_rect_init_from_rect (out_rect, &r, 0);
      return TRUE;
    }

  for (i = 0; i < 4; i++)
    {
      if (!gtk_css_parser_has_number (parser))
        break;
      if (!gtk_css_parser_consume_number (parser, &d))
        return FALSE;
      corners[i].width = d;
    }

  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a number");
      return FALSE;
    }

  /* The magic (i - 1) >> 1 below makes it take the correct value
   * according to spec. Feel free to check the 4 cases
   */
  for (; i < 4; i++)
    corners[i].width = corners[(i - 1) >> 1].width;

  if (gtk_css_parser_try_delim (parser, '/'))
    {
      gtk_css_parser_consume_token (parser);

      for (i = 0; i < 4; i++)
        {
          if (!gtk_css_parser_has_number (parser))
            break;
          if (!gtk_css_parser_consume_number (parser, &d))
            return FALSE;
          corners[i].height = d;
        }

      if (i == 0)
        {
          gtk_css_parser_error_syntax (parser, "Expected a number");
          return FALSE;
        }

      for (; i < 4; i++)
        corners[i].height = corners[(i - 1) >> 1].height;
    }
  else
    {
      for (i = 0; i < 4; i++)
        corners[i].height = corners[i].width;
    }

  gsk_rounded_rect_init (out_rect, &r, &corners[0], &corners[1], &corners[2], &corners[3]);

  return TRUE;
}

static gboolean
parse_double (GtkCssParser *parser,
              Context      *context,
              gpointer      out_double)
{
  return gtk_css_parser_consume_number (parser, out_double);
}

static gboolean
parse_positive_double (GtkCssParser *parser,
                       Context      *context,
                       gpointer      out_double)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_NUMBER)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER))
    {
      gtk_css_parser_error_syntax (parser, "Expected a positive number");
      return FALSE;
    }

  return gtk_css_parser_consume_number (parser, out_double);
}

static gboolean
parse_strictly_positive_double (GtkCssParser *parser,
                                Context      *context,
                                gpointer      out_double)
{
  double tmp;

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_NUMBER)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER))
    {
      gtk_css_parser_error_syntax (parser, "Expected a strictly positive number");
      return FALSE;
    }

  if (!gtk_css_parser_consume_number (parser, &tmp))
    return FALSE;

  if (tmp == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a strictly positive number");
      return FALSE;
    }

  *(double *) out_double = tmp;
  return TRUE;
}

static gboolean
parse_point (GtkCssParser *parser,
             Context      *context,
             gpointer      out_point)
{
  double x, y;

  if (!gtk_css_parser_consume_number (parser, &x) ||
      !gtk_css_parser_consume_number (parser, &y))
    return FALSE;

  graphene_point_init (out_point, x, y);

  return TRUE;
}

static gboolean
parse_transform (GtkCssParser *parser,
                 Context      *context,
                 gpointer      out_transform)
{
  GskTransform *transform;

  if (!gsk_transform_parser_parse (parser, &transform))
    {
      gsk_transform_unref (transform);
      return FALSE;
    }

  *(GskTransform **) out_transform = transform;

  return TRUE;
}

static void
clear_transform (gpointer inout_transform)
{
  g_clear_pointer ((GskTransform **) inout_transform, gsk_transform_unref);
}

static gboolean
parse_string (GtkCssParser *parser,
              Context      *context,
              gpointer      out_string)
{
  const GtkCssToken *token;
  char *s;

  token = gtk_css_parser_get_token (parser);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_STRING))
    {
      gtk_css_parser_error_syntax (parser, "Expected a string");
      return FALSE;
    }

  s = g_strdup (gtk_css_token_get_string (token));
  gtk_css_parser_consume_token (parser);

  g_free (*(char **) out_string);
  *(char **) out_string = s;

  return TRUE;
}

static void
clear_string (gpointer inout_string)
{
  g_clear_pointer ((char **) inout_string, g_free);
}

typedef struct {
  Context *context;
  GdkColor *color;
} ColorArgData;

static guint
parse_color_arg (GtkCssParser *parser,
                 guint         arg,
                 gpointer      data)
{
  ColorArgData *d = data;
  GdkColorState *color_state;
  float values[4], clamped[4];

  if (!parse_color_state (parser, d->context, &color_state))
    return 0;

  for (int i = 0; i < 3; i++)
    {
      double number;

      if (!gtk_css_parser_consume_number_or_percentage (parser, 0, 1, &number))
        return 0;

      values[i] = number;
    }

  if (gtk_css_parser_try_delim (parser, '/'))
    {
      double number;

      if (!gtk_css_parser_consume_number_or_percentage (parser, 0, 1, &number))
        return 0;

      values[3] = number;
    }
  else
    {
      values[3] = 1;
    }

  gdk_color_state_clamp (color_state, values, clamped);
  if (values[0] != clamped[0] ||
      values[1] != clamped[1] ||
      values[2] != clamped[2] ||
      values[3] != clamped[3])
    {
      gtk_css_parser_error (parser,
                            GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                            gtk_css_parser_get_block_location (parser),
                            gtk_css_parser_get_end_location (parser),
                            "Color values out of range for color state");
    }

  gdk_color_init (d->color, color_state, clamped);
  return 1;
}

static gboolean
parse_color (GtkCssParser *parser,
             Context      *context,
             gpointer      color)
{
  GdkRGBA rgba;

  if (gtk_css_parser_has_function (parser, "color"))
    {
      ColorArgData data = { context, color };

      if (!gtk_css_parser_consume_function (parser, 1, 1, parse_color_arg, &data))
        return FALSE;

      return TRUE;
    }
  else if (gdk_rgba_parser_parse (parser, &rgba))
    {
      gdk_color_init_from_rgba ((GdkColor *) color, &rgba);
      return TRUE;
    }

  return FALSE;
}

static gboolean
parse_stops (GtkCssParser *parser,
             Context      *context,
             gpointer      out_stops)
{
  GArray *stops;
  GskGradientStop stop;

  stops = g_array_new (FALSE, FALSE, sizeof (GskGradientStop));

  for (;;)
    {
      double dval;

      if (!gtk_css_parser_consume_number (parser, &dval))
        goto error;

      stop.offset = dval;

      if (gtk_css_parser_has_number (parser))
        {
          if (!gtk_css_parser_consume_number (parser, &dval))
            goto error;

          stop.transition_hint = dval;
        }
      else
        stop.transition_hint = 0.5;

      if (!parse_color (parser, context, &stop.color))
        goto error;

      if (stops->len == 0 && stop.offset < 0)
        gtk_css_parser_error_value (parser, "Color stop offset must be >= 0");
      else if (stops->len > 0 && stop.offset < g_array_index (stops, GskGradientStop, stops->len - 1).offset)
        gtk_css_parser_error_value (parser, "Color stop offset must be >= previous value");
      else if (stop.offset > 1)
        gtk_css_parser_error_value (parser, "Color stop offset must be <= 1");
      else
        g_array_append_val (stops, stop);

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        gtk_css_parser_skip (parser);
      else
        break;
  }

  if (stops->len < 2)
    {
      gtk_css_parser_error_value (parser, "At least 2 color stops need to be specified");
      goto error;
    }

  if (*(GArray **) out_stops)
    g_array_free (*(GArray **) out_stops, TRUE);
  *(GArray **) out_stops = stops;

  return TRUE;

error:
  g_array_free (stops, TRUE);
  return FALSE;
}

static void
clear_stops (gpointer inout_stops)
{
  GArray **stops = (GArray **) inout_stops;

  if (*stops)
    {
      for (int i = 0; i < (*stops)->len; i++)
        {
          GskGradientStop *stop = &g_array_index (*stops, GskGradientStop, i);
          gdk_color_finish (&stop->color);
        }

      g_array_free (*stops, TRUE);
      *stops = NULL;
    }
}

static gboolean
parse_float4 (GtkCssParser *parser,
              Context      *context,
              gpointer      out_floats)
{
  float *floats = (float *) out_floats;
  double d[4];
  int i;

  for (i = 0; i < 4 && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF); i ++)
    {
      if (!gtk_css_parser_consume_number (parser, &d[i]))
        return FALSE;
    }
  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a color");
      return FALSE;
    }
  for (; i < 4; i++)
    {
      d[i] = d[(i - 1) >> 1];
    }

  for (i = 0; i < 4; i++)
    {
      floats[i] = d[i];
    }

  return TRUE;
}

static gboolean
parse_shadows (GtkCssParser *parser,
               Context      *context,
               gpointer      out_shadows)
{
  GArray *shadows = out_shadows;

  do
    {
      GskShadowEntry shadow;
      GdkColor color = GDK_COLOR_SRGB (0, 0, 0, 1);
      double dx = 0, dy = 0, radius = 0;

      if (!parse_color (parser, context, &color))
        gtk_css_parser_error_value (parser, "Expected shadow color");

      if (!gtk_css_parser_consume_number (parser, &dx))
        gtk_css_parser_error_value (parser, "Expected shadow x offset");

      if (!gtk_css_parser_consume_number (parser, &dy))
        gtk_css_parser_error_value (parser, "Expected shadow y offset");

      if (gtk_css_parser_has_number (parser))
        {
          if (!gtk_css_parser_consume_number (parser, &radius))
            gtk_css_parser_error_value (parser, "Expected shadow blur radius");
        }

      gdk_color_init_copy (&shadow.color, &color);
      graphene_point_init (&shadow.offset, dx, dy);
      shadow.radius = radius;

      g_array_append_val (shadows, shadow);

      gdk_color_finish (&color);
    }
  while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  return TRUE;
}

static void
clear_shadows (gpointer inout_shadows)
{
  GArray *shadows = inout_shadows;

  for (gsize i = 0; i < shadows->len; i++)
    {
      GskShadowEntry *shadow = &g_array_index (shadows, GskShadowEntry, i);
      gdk_color_finish (&shadow->color);
    }

  g_array_set_size (shadows, 0);
}

static const struct
{
  GskScalingFilter filter;
  const char *name;
} scaling_filters[] = {
  { GSK_SCALING_FILTER_LINEAR, "linear" },
  { GSK_SCALING_FILTER_NEAREST, "nearest" },
  { GSK_SCALING_FILTER_TRILINEAR, "trilinear" },
};

static gboolean
parse_scaling_filter (GtkCssParser *parser,
                      Context      *context,
                      gpointer      out_filter)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (scaling_filters); i++)
    {
      if (gtk_css_parser_try_ident (parser, scaling_filters[i].name))
        {
          *(GskScalingFilter *) out_filter = scaling_filters[i].filter;
          return TRUE;
        }
    }

  gtk_css_parser_error_syntax (parser, "Not a valid scaling filter.");

  return FALSE;
}

static const struct
{
  GskBlendMode mode;
  const char *name;
} blend_modes[] = {
  { GSK_BLEND_MODE_DEFAULT, "normal" },
  { GSK_BLEND_MODE_MULTIPLY, "multiply" },
  { GSK_BLEND_MODE_SCREEN, "screen" },
  { GSK_BLEND_MODE_OVERLAY, "overlay" },
  { GSK_BLEND_MODE_DARKEN, "darken" },
  { GSK_BLEND_MODE_LIGHTEN, "lighten" },
  { GSK_BLEND_MODE_COLOR_DODGE, "color-dodge" },
  { GSK_BLEND_MODE_COLOR_BURN, "color-burn" },
  { GSK_BLEND_MODE_HARD_LIGHT, "hard-light" },
  { GSK_BLEND_MODE_SOFT_LIGHT, "soft-light" },
  { GSK_BLEND_MODE_DIFFERENCE, "difference" },
  { GSK_BLEND_MODE_EXCLUSION, "exclusion" },
  { GSK_BLEND_MODE_COLOR, "color" },
  { GSK_BLEND_MODE_HUE, "hue" },
  { GSK_BLEND_MODE_SATURATION, "saturation" },
  { GSK_BLEND_MODE_LUMINOSITY, "luminosity" }
};

static const char *
get_blend_mode_name (GskBlendMode mode)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (blend_modes); i++)
    {
      if (blend_modes[i].mode == mode)
        return blend_modes[i].name;
    }

  return NULL;
}

static gboolean
parse_blend_mode (GtkCssParser *parser,
                  Context      *context,
                  gpointer      out_mode)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (blend_modes); i++)
    {
      if (gtk_css_parser_try_ident (parser, blend_modes[i].name))
        {
          *(GskBlendMode *) out_mode = blend_modes[i].mode;
          return TRUE;
        }
    }

  gtk_css_parser_error_syntax (parser, "Not a valid blend mode.");

  return FALSE;
}

static const struct
{
  GskMaskMode mode;
  const char *name;
} mask_modes[] = {
  { GSK_MASK_MODE_ALPHA, "alpha" },
  { GSK_MASK_MODE_INVERTED_ALPHA, "inverted-alpha" },
  { GSK_MASK_MODE_LUMINANCE, "luminance" },
  { GSK_MASK_MODE_INVERTED_LUMINANCE, "inverted-luminance" },
};

static const char *
get_mask_mode_name (GskMaskMode mode)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (mask_modes); i++)
    {
      if (mask_modes[i].mode == mode)
        return mask_modes[i].name;
    }

  return NULL;
}

static gboolean
parse_mask_mode (GtkCssParser *parser,
                 Context      *context,
                 gpointer      out_mode)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (mask_modes); i++)
    {
      if (gtk_css_parser_try_ident (parser, mask_modes[i].name))
        {
          *(GskMaskMode *) out_mode = mask_modes[i].mode;
          return TRUE;
        }
    }

  gtk_css_parser_error_syntax (parser, "Not a valid mask mode.");

  return FALSE;
}

static PangoFont *
font_from_string (PangoFontMap *fontmap,
                  const char   *string,
                  gboolean      allow_fallback)
{
  PangoFontDescription *desc;
  PangoContext *ctx;
  PangoFont *font;

  desc = pango_font_description_from_string (string);
  ctx = pango_font_map_create_context (fontmap);
  font = pango_font_map_load_font (fontmap, ctx, desc);
  g_object_unref (ctx);

  if (font && !allow_fallback)
    {
      PangoFontDescription *desc2;
      const char *family, *family2;

      desc2 = pango_font_describe (font);

      family = pango_font_description_get_family (desc);
      family2 = pango_font_description_get_family (desc2);

      if (g_strcmp0 (family, family2) != 0)
        g_clear_object (&font);

      pango_font_description_free (desc2);
    }

  pango_font_description_free (desc);

  return font;
}

#define MIN_ASCII_GLYPH 32
#define MAX_ASCII_GLYPH 127 /* exclusive */
#define N_ASCII_GLYPHS (MAX_ASCII_GLYPH - MIN_ASCII_GLYPH)

static PangoGlyphString *
create_ascii_glyphs (PangoFont *font)
{
  PangoLanguage *language = pango_language_from_string ("en_US"); /* just pick one */
  PangoCoverage *coverage;
  PangoAnalysis not_a_hack = {
    .shape_engine = NULL, /* unused */
    .lang_engine = NULL, /* unused by pango_shape() */
    .font = font,
    .level = 0,
    .gravity = PANGO_GRAVITY_SOUTH,
    .flags = 0,
    .script = PANGO_SCRIPT_COMMON,
    .language = language,
    .extra_attrs = NULL
  };
  PangoGlyphString *result, *glyph_string;
  guint i;

  result = pango_glyph_string_new ();

  coverage = pango_font_get_coverage (font, language);

  pango_glyph_string_set_size (result, N_ASCII_GLYPHS);
  glyph_string = pango_glyph_string_new ();
  for (i = MIN_ASCII_GLYPH; i < MAX_ASCII_GLYPH; i++)
    {
      const char text[2] = { i, 0 };

      if (!pango_coverage_get (coverage, i))
        {
          result->glyphs[i - MIN_ASCII_GLYPH].glyph = PANGO_GLYPH_INVALID_INPUT;
          continue;
        }

      pango_shape_with_flags (text, 1,
                              text, 1,
                              &not_a_hack,
                              glyph_string,
                              PANGO_SHAPE_NONE);

      if (glyph_string->num_glyphs != 1)
        result->glyphs[i - MIN_ASCII_GLYPH].glyph = PANGO_GLYPH_INVALID_INPUT;
      else
        result->glyphs[i - MIN_ASCII_GLYPH] = glyph_string->glyphs[0];
    }

  g_object_unref (coverage);

  pango_glyph_string_free (glyph_string);

  return result;
}

static void
delete_file (gpointer data)
{
  char *path = data;

  g_remove (path);
  g_free (path);
}

static void
ensure_fontmap (Context *context)
{
  GPtrArray *files;

  if (context->fontmap)
    return;

  context->fontmap = pango_cairo_font_map_new ();

#ifdef HAVE_PANGOFT
  {
    FcConfig *config;

    config = FcConfigCreate ();
    pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (context->fontmap), config);
    FcConfigDestroy (config);
  }
#endif

  files = g_ptr_array_new_with_free_func (delete_file);

  g_object_set_data_full (G_OBJECT (context->fontmap), "font-files",
                          files, (GDestroyNotify) g_ptr_array_unref);
}

static gboolean
add_font_from_file (Context     *context,
                    const char  *path,
                    GError     **error)
{
  ensure_fontmap (context);

  if (pango_font_map_add_font_file (context->fontmap, path, error))
    {
      GPtrArray *files;

      files = (GPtrArray *) g_object_get_data (G_OBJECT (context->fontmap), "font-files");
      g_ptr_array_add (files, g_strdup (path));
      return TRUE;
    }
  else
    {
      g_remove (path);
      return FALSE;
    }
}

static gboolean
add_font_from_bytes (Context  *context,
                     GBytes   *bytes,
                     GError  **error)
{
  GFile *file;
  GIOStream *iostream;
  GOutputStream *ostream;
  gboolean result;

  file = g_file_new_tmp ("gtk4-font-XXXXXX.ttf", (GFileIOStream **) &iostream, error);
  if (!file)
    return FALSE;

  ostream = g_io_stream_get_output_stream (iostream);
  if (g_output_stream_write_bytes (ostream, bytes, NULL, error) == -1)
    {
      g_object_unref (file);
      g_object_unref (iostream);

      return FALSE;
    }

  g_io_stream_close (iostream, NULL, NULL);
  g_object_unref (iostream);

  result = add_font_from_file (context, g_file_peek_path (file), error);

  g_object_unref (file);

  return result;
}

static gboolean
parse_font (GtkCssParser *parser,
            Context      *context,
            gpointer      out_font)
{
  PangoFont *font = NULL;
  char *font_name;

  font_name = gtk_css_parser_consume_string (parser);
  if (font_name == NULL)
    return FALSE;

  if (gtk_css_parser_has_url (parser))
    {
      GBytes *bytes;
      GError *error = NULL;
      GtkCssLocation start_location;

      start_location = *gtk_css_parser_get_start_location (parser);
      bytes = consume_bytes (parser);
      if (bytes != NULL)
        {
          if (add_font_from_bytes (context, bytes, &error))
            {
              font = font_from_string (context->fontmap, font_name, FALSE);
              if (!font)
                {
                  gtk_css_parser_error (parser,
                                        GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                                        &start_location,
                                        gtk_css_parser_get_end_location (parser),
                                        "The given url does not define a font named \"%s\"",
                                        font_name);
                }
            }
          else
            {
              gtk_css_parser_error (parser,
                                    GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                                    &start_location,
                                    gtk_css_parser_get_end_location (parser),
                                    "%s", error->message);
              g_clear_error (&error);
            }

          g_bytes_unref (bytes);
        }
    }
  else
    {
      if (context->fontmap)
        font = font_from_string (context->fontmap, font_name, FALSE);

      if (!font)
        font = font_from_string (pango_cairo_font_map_get_default (), font_name, TRUE);

      if (!font)
        gtk_css_parser_error_value (parser, "The font \"%s\" does not exist", font_name);
    }

  g_free (font_name);

  if (font)
    {
      *((PangoFont**)out_font) = font;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
clear_font (gpointer inout_font)
{
  g_clear_object ((PangoFont **) inout_font);
}

#define GLYPH_NEEDS_WIDTH ((PangoGlyphUnit) -1)

static gboolean
parse_glyphs (GtkCssParser *parser,
              Context      *context,
              gpointer      out_glyphs)
{
  PangoGlyphString *glyph_string;

  glyph_string = pango_glyph_string_new ();

  do
    {
      PangoGlyphInfo gi = { 0, { 0, 0, 0}, { 1 } };
      double d, d2;
      int i;

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
        {
          char *s = gtk_css_parser_consume_string (parser);

          for (i = 0; s[i] != 0; i++)
            {
              if (s[i] < MIN_ASCII_GLYPH || s[i] >= MAX_ASCII_GLYPH)
                {
                  gtk_css_parser_error_value (parser, "Unsupported character %d in string", i);
                }
              gi.glyph = PANGO_GLYPH_INVALID_INPUT - MAX_ASCII_GLYPH + s[i];
              gi.geometry.width = GLYPH_NEEDS_WIDTH;
              pango_glyph_string_set_size (glyph_string, glyph_string->num_glyphs + 1);
              glyph_string->glyphs[glyph_string->num_glyphs - 1] = gi;
            }

          g_free (s);
        }
      else
        {
          if (!gtk_css_parser_consume_integer (parser, &i))
            {
              pango_glyph_string_free (glyph_string);
              return FALSE;
            }
          gi.glyph = i;

          if (gtk_css_parser_has_number (parser))
            {
              gtk_css_parser_consume_number (parser, &d);
              gi.geometry.width = (int) (d * PANGO_SCALE);
            }
          else
            {
              gi.geometry.width = GLYPH_NEEDS_WIDTH;
            }

          if (gtk_css_parser_has_number (parser))
            {
              if (!gtk_css_parser_consume_number (parser, &d) ||
                  !gtk_css_parser_consume_number (parser, &d2))
                {
                  pango_glyph_string_free (glyph_string);
                  return FALSE;
                }
              gi.geometry.x_offset = (int) (d * PANGO_SCALE);
              gi.geometry.y_offset = (int) (d2 * PANGO_SCALE);

              if (gtk_css_parser_try_ident (parser, "same-cluster"))
                gi.attr.is_cluster_start = 0;
              else
                gi.attr.is_cluster_start = 1;

              if (gtk_css_parser_try_ident (parser, "color"))
                gi.attr.is_color = 1;
              else
                gi.attr.is_color = 0;
            }

          pango_glyph_string_set_size (glyph_string, glyph_string->num_glyphs + 1);
          glyph_string->glyphs[glyph_string->num_glyphs - 1] = gi;
        }
    }
  while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  *((PangoGlyphString **)out_glyphs) = glyph_string;

  return TRUE;
}

static void
clear_glyphs (gpointer inout_glyphs)
{
  g_clear_pointer ((PangoGlyphString **) inout_glyphs, pango_glyph_string_free);
}

static gboolean
parse_node (GtkCssParser *parser, Context *context, gpointer out_node);

static void
clear_node (gpointer inout_node)
{
  g_clear_pointer ((GskRenderNode **) inout_node, gsk_render_node_unref);
}

static GskRenderNode *
parse_container_node (GtkCssParser *parser,
                      Context      *context)
{
  GPtrArray *nodes;
  const GtkCssToken *token;
  GskRenderNode *node;

  nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref);

  for (token = gtk_css_parser_get_token (parser);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_parser_get_token (parser))
    {
      node = NULL;
      /* We don't want a semicolon here, but the parse_node function will figure
       * that out itself and return an error if we encounter one.
       */
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);

      if (parse_node (parser, context, &node))
        g_ptr_array_add (nodes, node);

      gtk_css_parser_end_block (parser);
    }

  node = gsk_container_node_new ((GskRenderNode **) nodes->pdata, nodes->len);

  g_ptr_array_unref (nodes);

  return node;
}

static GdkTexture *
create_default_texture (void)
{
  static const guint32 pixels[100] = {
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC };
  GBytes *bytes;
  GdkTexture *texture;

  bytes = g_bytes_new_static ((guchar *) pixels, 400);
  texture = gdk_memory_texture_new (10, 10, GDK_MEMORY_DEFAULT, bytes, 40);
  g_bytes_unref (bytes);

  return texture;
}

static GskRenderNode *
create_default_render_node_with_bounds (const graphene_rect_t *rect)
{
  return gsk_color_node_new (&GDK_RGBA("FF00CC"), rect);
}

static GskRenderNode *
create_default_render_node (void)
{
  return create_default_render_node_with_bounds (&GRAPHENE_RECT_INIT (0, 0, 50, 50));
}

static GskPath *
create_default_path (void)
{
  GskPathBuilder *builder;
  guint i;

  builder = gsk_path_builder_new ();

  gsk_path_builder_move_to (builder, 25, 0);
  for (i = 1; i < 5; i++)
    {
      gsk_path_builder_line_to (builder,
                                sin (i * G_PI * 0.8) * 25 + 25,
                                -cos (i * G_PI * 0.8) * 25 + 25);
    }
  gsk_path_builder_close (builder);

  return gsk_path_builder_free_to_path (builder);
}

static gboolean
parse_cicp_range (GtkCssParser *parser,
                  Context      *context,
                  gpointer      out)
{
  if (!parse_enum (parser, GDK_TYPE_CICP_RANGE, out))
    return FALSE;

  return TRUE;
}

static gboolean
parse_color_state_rule (GtkCssParser *parser,
                        Context      *context)
{
  char *name = NULL;
  GdkColorState *cs = NULL;
  GdkCicp cicp = { 2, 2, 2, GDK_CICP_RANGE_FULL };
  const Declaration declarations[] = {
    { "primaries", parse_unsigned, NULL, &cicp.color_primaries },
    { "transfer", parse_unsigned, NULL, &cicp.transfer_function },
    { "matrix", parse_unsigned, NULL, &cicp.matrix_coefficients },
    { "range", parse_cicp_range, NULL, &cicp.range },
  };
  const char *default_names[] = { "srgb", "srgb-linear", "rec2100-pq", "rec2100-linear", NULL};
  GError *error = NULL;
  GtkCssLocation start;
  GtkCssLocation end;

  /* We only return FALSE when we see the wrong @ keyword, since the caller
   * throws an error in this case.
   */
  if (!gtk_css_parser_try_at_keyword (parser, "cicp"))
    return FALSE;

  name = gtk_css_parser_consume_string (parser);
  if (name == NULL)
    return TRUE;

  if (g_strv_contains (default_names, name) ||
      (context->named_color_states &&
       g_hash_table_contains (context->named_color_states, name)))
    {
      gtk_css_parser_error_value (parser, "A color state named \"%s\" already exists", name);
      g_free (name);
      return TRUE;
    }

  start = *gtk_css_parser_get_block_location (parser);
  end = *gtk_css_parser_get_end_location (parser);

  gtk_css_parser_end_block_prelude (parser);

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  cs = gdk_color_state_new_for_cicp (&cicp, &error);

  if (!cs)
    {
      gtk_css_parser_error (parser,
                            GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                            &start, &end,
                            "Not a valid cicp tuple: %s", error->message);
      g_error_free (error);
      return TRUE;
    }

  if (context->named_color_states == NULL)
    context->named_color_states = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                         g_free, (GDestroyNotify) gdk_color_state_unref);
  g_hash_table_insert (context->named_color_states, name, cs);

  return TRUE;
}

static gboolean
parse_hue_interpolation (GtkCssParser *parser,
                         Context      *context,
                         gpointer      out_value)
{
  GskHueInterpolation interpolation;

  if (gtk_css_parser_try_ident (parser, "shorter"))
    interpolation = GSK_HUE_INTERPOLATION_SHORTER;
  else if (gtk_css_parser_try_ident (parser, "longer"))
    interpolation = GSK_HUE_INTERPOLATION_LONGER;
  else if (gtk_css_parser_try_ident (parser, "increasing"))
    interpolation = GSK_HUE_INTERPOLATION_INCREASING;
  else if (gtk_css_parser_try_ident (parser, "decreasing"))
    interpolation = GSK_HUE_INTERPOLATION_DECREASING;
  else
    {
      gtk_css_parser_error_value (parser, "Unknown value for hue interpolation");
      return FALSE;
    }

  *((GskHueInterpolation *)out_value) = interpolation;
  return TRUE;
}

static gboolean
parse_colors4 (GtkCssParser *parser,
               Context      *context,
               gpointer      out_colors)
{
  GdkColor colors[4];
  int i;

  for (i = 0; i < 4 && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF); i ++)
    {
      if (!parse_color (parser, context, &colors[i]))
        return FALSE;
    }
  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a color");
      return FALSE;
    }
  for (; i < 4; i++)
    {
      colors[i] = colors[(i - 1) >> 1];
    }

  memcpy (out_colors, colors, sizeof (GdkColor) * 4);

  return TRUE;
}

static GskRenderNode *
parse_color_node (GtkCssParser *parser,
                  Context      *context)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GdkColor color = GDK_COLOR_SRGB (1, 0, 0.8, 1);
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "color", parse_color, NULL, &color },
  };
  GskRenderNode *node;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  node = gsk_color_node_new2 (&color, &bounds);

  gdk_color_finish (&color);

  return node;
}

static GskRenderNode *
parse_linear_gradient_node_internal (GtkCssParser *parser,
                                     Context      *context,
                                     gboolean      repeating)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  graphene_point_t start = GRAPHENE_POINT_INIT (0, 0);
  graphene_point_t end = GRAPHENE_POINT_INIT (0, 50);
  GArray *stops = NULL;
  GdkColorState *interpolation = NULL;
  GskHueInterpolation hue_interpolation = GSK_HUE_INTERPOLATION_SHORTER;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "start", parse_point, NULL, &start },
    { "end", parse_point, NULL, &end },
    { "stops", parse_stops, clear_stops, &stops },
    { "interpolation", parse_color_state, &clear_color_state, &interpolation },
    { "hue-interpolation", parse_hue_interpolation, NULL, &hue_interpolation },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (stops == NULL)
    {
      GskGradientStop from = {
        .offset = 0.0,
        .transition_hint = 0.5,
        .color = GDK_COLOR_SRGB (0.667, 1, 0, 1)
      };
      GskGradientStop to = {
        .offset = 1.0,
        .transition_hint = 0.5,
        .color = GDK_COLOR_SRGB (1, 0, 0.8, 1)
      };

      stops = g_array_new (FALSE, FALSE, sizeof (GskGradientStop));
      g_array_append_val (stops, from);
      g_array_append_val (stops, to);
    }

  if (interpolation == NULL)
    interpolation = GDK_COLOR_STATE_SRGB;

  if (repeating)
    result = gsk_repeating_linear_gradient_node_new2 (&bounds,
                                                      &start, &end,
                                                      interpolation,
                                                      hue_interpolation,
                                                      (GskGradientStop *) stops->data,
                                                      stops->len);
  else
    result = gsk_linear_gradient_node_new2 (&bounds,
                                            &start, &end,
                                            interpolation,
                                            hue_interpolation,
                                            (GskGradientStop *) stops->data,
                                            stops->len);

  clear_stops (&stops);
  clear_color_state (&interpolation);

  return result;
}

static GskRenderNode *
parse_linear_gradient_node (GtkCssParser *parser,
                            Context      *context)
{
  return parse_linear_gradient_node_internal (parser, context, FALSE);
}

static GskRenderNode *
parse_repeating_linear_gradient_node (GtkCssParser *parser,
                                      Context      *context)
{
  return parse_linear_gradient_node_internal (parser, context, TRUE);
}

static GskRenderNode *
parse_radial_gradient_node_internal (GtkCssParser *parser,
                                     Context      *context,
                                     gboolean      repeating)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  graphene_point_t center = GRAPHENE_POINT_INIT (25, 25);
  double hradius = 25.0;
  double vradius = 25.0;
  double start = 0;
  double end = 1.0;
  GArray *stops = NULL;
  GdkColorState *interpolation = NULL;
  GskHueInterpolation hue_interpolation = GSK_HUE_INTERPOLATION_SHORTER;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "center", parse_point, NULL, &center },
    { "hradius", parse_strictly_positive_double, NULL, &hradius },
    { "vradius", parse_strictly_positive_double, NULL, &vradius },
    { "start", parse_positive_double, NULL, &start },
    { "end", parse_positive_double, NULL, &end },
    { "stops", parse_stops, clear_stops, &stops },
    { "interpolation", parse_color_state, &clear_color_state, &interpolation },
    { "hue-interpolation", parse_hue_interpolation, NULL, &hue_interpolation },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (stops == NULL)
    {
      GskGradientStop from = {
        .offset = 0.0,
        .transition_hint = 0.5,
        .color = GDK_COLOR_SRGB (0.667, 1, 0, 1)
      };
      GskGradientStop to = {
        .offset = 1.0,
        .transition_hint = 0.5,
        .color = GDK_COLOR_SRGB (1, 0, 0.8, 1)
      };

      stops = g_array_new (FALSE, FALSE, sizeof (GskGradientStop));
      g_array_append_val (stops, from);
      g_array_append_val (stops, to);
    }

  if (interpolation == NULL)
    interpolation = GDK_COLOR_STATE_SRGB;

  if (end <= start)
    {
      gtk_css_parser_error (parser,
                            GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                            gtk_css_parser_get_block_location (parser),
                            gtk_css_parser_get_end_location (parser),
                            "\"start\" must be larger than \"end\"");
      result = NULL;
    }
  else if (repeating)
    result = gsk_repeating_radial_gradient_node_new2 (&bounds, &center,
                                                      hradius, vradius,
                                                      start, end,
                                                      interpolation,
                                                      hue_interpolation,
                                                      (GskGradientStop *) stops->data,
                                                      stops->len);
  else
    result = gsk_radial_gradient_node_new2 (&bounds, &center,
                                            hradius, vradius,
                                            start, end,
                                            interpolation,
                                            hue_interpolation,
                                            (GskGradientStop *) stops->data,
                                            stops->len);

  clear_stops (&stops);
  clear_color_state (&interpolation);

  return result;
}

static GskRenderNode *
parse_radial_gradient_node (GtkCssParser *parser,
                            Context      *context)
{
  return parse_radial_gradient_node_internal (parser, context, FALSE);
}

static GskRenderNode *
parse_repeating_radial_gradient_node (GtkCssParser *parser,
                                      Context      *context)
{
  return parse_radial_gradient_node_internal (parser, context, TRUE);
}

static GskRenderNode *
parse_conic_gradient_node (GtkCssParser *parser,
                           Context      *context)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  graphene_point_t center = GRAPHENE_POINT_INIT (25, 25);
  double rotation = 0.0;
  GArray *stops = NULL;
  GdkColorState *interpolation = NULL;
  GskHueInterpolation hue_interpolation = GSK_HUE_INTERPOLATION_SHORTER;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "center", parse_point, NULL, &center },
    { "rotation", parse_double, NULL, &rotation },
    { "stops", parse_stops, clear_stops, &stops },
    { "interpolation", parse_color_state, &clear_color_state, &interpolation },
    { "hue-interpolation", parse_hue_interpolation, NULL, &hue_interpolation },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (stops == NULL)
    {
      GskGradientStop from = {
        .offset = 0.0,
        .transition_hint = 0.5,
        .color = GDK_COLOR_SRGB (0.667, 1, 0, 1)
      };
      GskGradientStop to = {
        .offset = 1.0,
        .transition_hint = 0.5,
        .color = GDK_COLOR_SRGB (1, 0, 0.8, 1)
      };

      stops = g_array_new (FALSE, FALSE, sizeof (GskGradientStop));
      g_array_append_val (stops, from);
      g_array_append_val (stops, to);
    }

  if (interpolation == NULL)
    interpolation = GDK_COLOR_STATE_SRGB;

  result = gsk_conic_gradient_node_new2 (&bounds,
                                         &center, rotation,
                                         interpolation,
                                         hue_interpolation,
                                         (GskGradientStop *) stops->data,
                                         stops->len);

  clear_stops (&stops);
  clear_color_state (&interpolation);

  return result;
}

static GskRenderNode *
parse_inset_shadow_node (GtkCssParser *parser,
                         Context      *context)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  GdkColor color = GDK_COLOR_SRGB (0, 0, 0, 1);
  double dx = 1, dy = 1, blur = 0, spread = 0;
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, NULL, &outline },
    { "color", parse_color, NULL, &color },
    { "dx", parse_double, NULL, &dx },
    { "dy", parse_double, NULL, &dy },
    { "spread", parse_double, NULL, &spread },
    { "blur", parse_positive_double, NULL, &blur }
  };
  GskRenderNode *node;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  node = gsk_inset_shadow_node_new2 (&outline, &color, &GRAPHENE_POINT_INIT (dx, dy), spread, blur);

  gdk_color_finish (&color);

  return node;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
typedef union {
    gint32 i;
    double v[4];
} UniformValue;

typedef struct {
  GskGLShader *shader;
  GskShaderArgsBuilder *args;
} ShaderInfo;

static void
clear_shader_info (gpointer data)
{
  ShaderInfo *info = data;
  g_clear_object (&info->shader);
  g_clear_pointer (&info->args, gsk_shader_args_builder_unref);
}

static gboolean
parse_shader (GtkCssParser *parser,
              Context      *context,
              gpointer      out_shader_info)
{
  ShaderInfo *shader_info = out_shader_info;
  char *sourcecode = NULL;
  GBytes *bytes;
  GskGLShader *shader;

  if (!parse_string (parser, context, &sourcecode))
    {
      gtk_css_parser_error_value (parser, "Not a string");
      return FALSE;
    }

  bytes = g_bytes_new_take (sourcecode, strlen (sourcecode));
  shader = gsk_gl_shader_new_from_bytes (bytes);
  g_bytes_unref (bytes);

  shader_info->shader = shader;

  return TRUE;
}

static gboolean
parse_uniform_value (GtkCssParser *parser,
                     int           idx,
                     ShaderInfo   *shader_info)
{
  switch (gsk_gl_shader_get_uniform_type (shader_info->shader, idx))
    {
    case GSK_GL_UNIFORM_TYPE_FLOAT:
      {
        double f;
        if (!gtk_css_parser_consume_number (parser, &f))
          return FALSE;
        gsk_shader_args_builder_set_float (shader_info->args, idx, f);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_INT:
      {
        int i;
        if (!gtk_css_parser_consume_integer (parser, &i))
          return FALSE;
        gsk_shader_args_builder_set_int (shader_info->args, idx, i);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_UINT:
      {
        int i;
        if (!gtk_css_parser_consume_integer (parser, &i) || i < 0)
          return FALSE;
        gsk_shader_args_builder_set_uint (shader_info->args, idx, i);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_BOOL:
      {
        int i;
        if (!gtk_css_parser_consume_integer (parser, &i) || (i != 0 && i != 1))
          return FALSE;
        gsk_shader_args_builder_set_bool (shader_info->args, idx, i);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_VEC2:
      {
        double f0, f1;
        graphene_vec2_t v;
        if (!gtk_css_parser_consume_number (parser, &f0) ||
            !gtk_css_parser_consume_number (parser, &f1))
          return FALSE;
        graphene_vec2_init (&v, f0, f1);
        gsk_shader_args_builder_set_vec2 (shader_info->args, idx, &v);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_VEC3:
      {
        double f0, f1, f2;
        graphene_vec3_t v;
        if (!gtk_css_parser_consume_number (parser, &f0) ||
            !gtk_css_parser_consume_number (parser, &f1) ||
            !gtk_css_parser_consume_number (parser, &f2))
          return FALSE;
        graphene_vec3_init (&v, f0, f1, f2);
        gsk_shader_args_builder_set_vec3 (shader_info->args, idx, &v);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_VEC4:
      {
        double f0, f1, f2, f3;
        graphene_vec4_t v;
        if (!gtk_css_parser_consume_number (parser, &f0) ||
            !gtk_css_parser_consume_number (parser, &f1) ||
            !gtk_css_parser_consume_number (parser, &f2) ||
            !gtk_css_parser_consume_number (parser, &f3))
          return FALSE;
        graphene_vec4_init (&v, f0, f1, f2, f3);
        gsk_shader_args_builder_set_vec4 (shader_info->args, idx, &v);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  if (idx < gsk_gl_shader_get_n_uniforms (shader_info->shader))
    {
      if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA))
        return FALSE;
    }

  return TRUE;
}

static gboolean
parse_shader_args (GtkCssParser *parser,
                   Context      *context,
                   gpointer      data)
{
  ShaderInfo *shader_info = data;
  int n_uniforms;
  int i;

  shader_info->args = gsk_shader_args_builder_new (shader_info->shader, NULL);
  n_uniforms = gsk_gl_shader_get_n_uniforms (shader_info->shader);

  for (i = 0; i < n_uniforms; i++)
    {
      if (!parse_uniform_value (parser, i, shader_info))
        return FALSE;
    }

  return TRUE;
}

static const char default_glsl[] =
  "void\n"
  "mainImage(out vec4 fragColor,\n"
  "          in vec2 fragCoord,\n"
  "          in vec2 resolution,\n"
  "          in vec2 uv)\n"
  "{\n"
  "  fragColor = vec4(1.0, 105.0/255.0, 180.0/255.0, 1.0);\n"
  "}";

static GskGLShader *
get_default_glshader (void)
{
  GBytes *bytes;
  GskGLShader *shader;

  bytes = g_bytes_new (default_glsl, strlen (default_glsl) + 1);
  shader = gsk_gl_shader_new_from_bytes (bytes);
  g_bytes_unref (bytes);

  return shader;
}

static GskRenderNode *
parse_glshader_node (GtkCssParser *parser,
                     Context      *context)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GskRenderNode *child[4] = { NULL, };
  ShaderInfo shader_info = {
    NULL,
    NULL,
  };
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "sourcecode", parse_shader, NULL, &shader_info },
    { "args", parse_shader_args, clear_shader_info, &shader_info },
    { "child1", parse_node, clear_node, &child[0] },
    { "child2", parse_node, clear_node, &child[1] },
    { "child3", parse_node, clear_node, &child[2] },
    { "child4", parse_node, clear_node, &child[3] },
  };
  GskGLShader *shader;
  GskShaderArgsBuilder *builder;
  GskRenderNode *node;
  GBytes *args = NULL;
  int len, i;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  for (len = 0; len < 4; len++)
    {
      if (child[len] == NULL)
        break;
    }

  if (shader_info.shader)
    shader = shader_info.shader;
  else
    shader = get_default_glshader ();

  if (shader_info.args)
    builder = shader_info.args;
  else
    builder = gsk_shader_args_builder_new (shader, NULL);

  args = gsk_shader_args_builder_free_to_args (builder);

  node = gsk_gl_shader_node_new (shader, &bounds, args, child, len);

  g_bytes_unref (args);
  g_object_unref (shader);

  for (i = 0; i < 4; i++)
    {
      if (child[i])
        gsk_render_node_unref (child[i]);
    }

  return node;
}
G_GNUC_END_IGNORE_DEPRECATIONS

static GskRenderNode *
parse_mask_node (GtkCssParser *parser,
                 Context      *context)
{
  GskRenderNode *source = NULL;
  GskRenderNode *mask = NULL;
  GskMaskMode mode = GSK_MASK_MODE_ALPHA;
  const Declaration declarations[] = {
    { "mode", parse_mask_mode, NULL, &mode },
    { "source", parse_node, clear_node, &source },
    { "mask", parse_node, clear_node, &mask },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS(declarations));
  if (source == NULL)
    source = create_default_render_node ();
  if (mask == NULL)
    mask = gsk_color_node_new (&GDK_RGBA("AAFF00"), &GRAPHENE_RECT_INIT (0, 0, 50, 50));

  result = gsk_mask_node_new (source, mask, mode);

  gsk_render_node_unref (source);
  gsk_render_node_unref (mask);

  return result;
}

static GskRenderNode *
parse_border_node (GtkCssParser *parser,
                   Context      *context)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  float widths[4] = { 1, 1, 1, 1 };
  GdkColor colors[4] = {
    GDK_COLOR_SRGB (0, 0, 0, 1),
    GDK_COLOR_SRGB (0, 0, 0, 1),
    GDK_COLOR_SRGB (0, 0, 0, 1),
    GDK_COLOR_SRGB (0, 0, 0, 1),
  };
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, NULL, &outline },
    { "widths", parse_float4, NULL, &widths },
    { "colors", parse_colors4, NULL, &colors },
  };

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  return gsk_border_node_new2 (&outline, widths, colors);
}

static GskRenderNode *
parse_texture_node (GtkCssParser *parser,
                    Context      *context)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GdkTexture *texture = NULL;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "texture", parse_texture, clear_texture, &texture }
  };
  GskRenderNode *node;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  if (texture == NULL)
    texture = create_default_texture ();

  node = gsk_texture_node_new (texture, &bounds);
  g_object_unref (texture);

  return node;
}

static GskRenderNode *
parse_texture_scale_node (GtkCssParser *parser,
                          Context      *context)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GdkTexture *texture = NULL;
  GskScalingFilter filter = GSK_SCALING_FILTER_LINEAR;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "texture", parse_texture, clear_texture, &texture },
    { "filter", parse_scaling_filter, NULL, &filter }
  };
  GskRenderNode *node;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  if (texture == NULL)
    texture = create_default_texture ();

  node = gsk_texture_scale_node_new (texture, &bounds, filter);
  g_object_unref (texture);

  return node;
}

static GskRenderNode *
parse_cairo_node (GtkCssParser *parser,
                  Context      *context)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GdkTexture *pixels = NULL;
  cairo_surface_t *surface;
  GBytes *bytes = NULL;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "pixels", parse_texture, clear_texture, &pixels },
    { "script", parse_bytes, clear_bytes, &bytes }
  };
  GskRenderNode *node;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  node = gsk_cairo_node_new (&bounds);

  if (bytes != NULL)
    {
      surface = interpret_cairo_script (parser, &bounds, bytes);
      g_bytes_unref (bytes);
    }
  else
    surface = NULL;

  if (surface != NULL)
    {
      gsk_cairo_node_set_surface (node, surface);
    }
  else if (pixels != NULL)
    {
      cairo_t *cr = gsk_cairo_node_get_draw_context (node);
      surface = gdk_texture_download_surface (pixels, GDK_COLOR_STATE_SRGB);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
    }
  else
    {
      /* do nothing */
    }

  g_clear_object (&pixels);
  g_clear_pointer (&surface, cairo_surface_destroy);

  return node;
}

static GskRenderNode *
parse_outset_shadow_node (GtkCssParser *parser,
                          Context      *context)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  GdkColor color = GDK_COLOR_SRGB (0, 0, 0, 1);
  double dx = 1, dy = 1, blur = 0, spread = 0;
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, NULL, &outline },
    { "color", parse_color, NULL, &color },
    { "dx", parse_double, NULL, &dx },
    { "dy", parse_double, NULL, &dy },
    { "spread", parse_double, NULL, &spread },
    { "blur", parse_positive_double, NULL, &blur }
  };
  GskRenderNode *node;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  node = gsk_outset_shadow_node_new2 (&outline, &color, &GRAPHENE_POINT_INIT (dx, dy), spread, blur);

  gdk_color_finish (&color);

  return node;
}

static GskRenderNode *
parse_transform_node (GtkCssParser *parser,
                      Context      *context)
{
  GskRenderNode *child = NULL;
  GskTransform *transform = NULL;
  const Declaration declarations[] = {
    { "transform", parse_transform, clear_transform, &transform },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  /* This is very much cheating, isn't it? */
  if (transform == NULL)
    transform = gsk_transform_new ();

  result = gsk_transform_node_new (child, transform);

  gsk_render_node_unref (child);
  gsk_transform_unref (transform);

  return result;
}

static GskRenderNode *
parse_opacity_node (GtkCssParser *parser,
                    Context      *context)
{
  GskRenderNode *child = NULL;
  double opacity = 0.5;
  const Declaration declarations[] = {
    { "opacity", parse_double, NULL, &opacity },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  result = gsk_opacity_node_new (child, opacity);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_color_matrix_node (GtkCssParser *parser,
                         Context      *context)
{
  GskRenderNode *child = NULL;
  graphene_matrix_t matrix;
  GskTransform *transform = NULL;
  graphene_vec4_t offset;
  const Declaration declarations[] = {
    { "matrix", parse_transform, clear_transform, &transform },
    { "offset", parse_vec4, NULL, &offset },
    { "child", parse_node, clear_node, &child }
  };
  GskRenderNode *result;

  graphene_vec4_init (&offset, 0, 0, 0, 0);

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  gsk_transform_to_matrix (transform, &matrix);

  result = gsk_color_matrix_node_new (child, &matrix, &offset);

  gsk_transform_unref (transform);
  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_cross_fade_node (GtkCssParser *parser,
                       Context      *context)
{
  GskRenderNode *start = NULL;
  GskRenderNode *end = NULL;
  double progress = 0.5;
  const Declaration declarations[] = {
    { "progress", parse_double, NULL, &progress },
    { "start", parse_node, clear_node, &start },
    { "end", parse_node, clear_node, &end },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (start == NULL)
    start = gsk_color_node_new (&GDK_RGBA("AAFF00"), &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  if (end == NULL)
    end = create_default_render_node ();

  result = gsk_cross_fade_node_new (start, end, progress);

  gsk_render_node_unref (start);
  gsk_render_node_unref (end);

  return result;
}

static GskRenderNode *
parse_blend_node (GtkCssParser *parser,
                  Context      *context)
{
  GskRenderNode *bottom = NULL;
  GskRenderNode *top = NULL;
  GskBlendMode mode = GSK_BLEND_MODE_DEFAULT;
  const Declaration declarations[] = {
    { "mode", parse_blend_mode, NULL, &mode },
    { "bottom", parse_node, clear_node, &bottom },
    { "top", parse_node, clear_node, &top },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (bottom == NULL)
    bottom = gsk_color_node_new (&GDK_RGBA("AAFF00"), &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  if (top == NULL)
    top = create_default_render_node ();

  result = gsk_blend_node_new (bottom, top, mode);

  gsk_render_node_unref (bottom);
  gsk_render_node_unref (top);

  return result;
}

static GskRenderNode *
parse_repeat_node (GtkCssParser *parser,
                   Context      *context)
{
  GskRenderNode *child = NULL;
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  graphene_rect_t child_bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  const Declaration declarations[] = {
    { "child", parse_node, clear_node, &child },
    { "bounds", parse_rect, NULL, &bounds },
    { "child-bounds", parse_rect, NULL, &child_bounds },
  };
  GskRenderNode *result;
  guint parse_result;

  parse_result = parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  if (!(parse_result & (1 << 1)))
    gsk_render_node_get_bounds (child, &bounds);
  if (!(parse_result & (1 << 2)))
    gsk_render_node_get_bounds (child, &child_bounds);

  result = gsk_repeat_node_new (&bounds, child, &child_bounds);

  gsk_render_node_unref (child);

  return result;
}

static gboolean
unpack_glyphs (PangoFont        *font,
               PangoGlyphString *glyphs)
{
  PangoGlyphString *ascii = NULL;
  guint i;

  for (i = 0; i < glyphs->num_glyphs; i++)
    {
      PangoGlyphInfo *gi = &glyphs->glyphs[i];

      if (gi->glyph >= PANGO_GLYPH_INVALID_INPUT - MAX_ASCII_GLYPH &&
          gi->glyph < PANGO_GLYPH_INVALID_INPUT)
        {
          PangoGlyph idx = gi->glyph - (PANGO_GLYPH_INVALID_INPUT - MAX_ASCII_GLYPH) - MIN_ASCII_GLYPH;

          if (ascii == NULL)
            {
              ascii = create_ascii_glyphs (font);
              if (ascii == NULL)
                return FALSE;
            }

          if (ascii->glyphs[idx].glyph == PANGO_GLYPH_INVALID_INPUT)
            {
              g_clear_pointer (&ascii, pango_glyph_string_free);
              return FALSE;
            }

          gi->glyph = ascii->glyphs[idx].glyph;
          gi->geometry.width = ascii->glyphs[idx].geometry.width;
        }
      
      if (gi->geometry.width == GLYPH_NEEDS_WIDTH)
        {
          PangoRectangle rect;

          pango_font_get_glyph_extents (font, gi->glyph, NULL, &rect);

          gi->geometry.width = rect.width;
        }
    }

  g_clear_pointer (&ascii, pango_glyph_string_free);

  return TRUE;
}

static gboolean
parse_hint_style (GtkCssParser *parser,
                  Context      *context,
                  gpointer      out)
{
  if (!parse_enum (parser, CAIRO_GOBJECT_TYPE_HINT_STYLE, out))
    return FALSE;

  if (*(cairo_hint_style_t *) out != CAIRO_HINT_STYLE_NONE &&
      *(cairo_hint_style_t *) out != CAIRO_HINT_STYLE_SLIGHT &&
      *(cairo_hint_style_t *) out != CAIRO_HINT_STYLE_FULL)
    {
      gtk_css_parser_error_value (parser, "Unsupported value for enum \"%s\"",
                                  g_type_name (CAIRO_GOBJECT_TYPE_HINT_STYLE));
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_antialias (GtkCssParser *parser,
                 Context      *context,
                 gpointer      out)
{
  if (!parse_enum (parser, CAIRO_GOBJECT_TYPE_ANTIALIAS, out))
    return FALSE;

  if (*(cairo_antialias_t *) out != CAIRO_ANTIALIAS_NONE &&
      *(cairo_antialias_t *) out != CAIRO_ANTIALIAS_GRAY)
    {
      gtk_css_parser_error_value (parser, "Unsupported value for enum \"%s\"",
                                  g_type_name (CAIRO_GOBJECT_TYPE_ANTIALIAS));
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_hint_metrics (GtkCssParser *parser,
                    Context      *context,
                    gpointer      out)
{
  if (!parse_enum (parser, CAIRO_GOBJECT_TYPE_HINT_METRICS, out))
    return FALSE;

  if (*(cairo_hint_metrics_t *) out != CAIRO_HINT_METRICS_OFF &&
      *(cairo_hint_metrics_t *) out != CAIRO_HINT_METRICS_ON)
    {
      gtk_css_parser_error_value (parser, "Unsupported value for enum \"%s\"",
                                  g_type_name (CAIRO_GOBJECT_TYPE_HINT_METRICS));
      return FALSE;
    }

  return TRUE;
}

static GskRenderNode *
parse_text_node (GtkCssParser *parser,
                 Context      *context)
{
  PangoFont *font = NULL;
  graphene_point_t offset = GRAPHENE_POINT_INIT (0, 0);
  GdkColor color = GDK_COLOR_SRGB (0, 0, 0, 1);
  PangoGlyphString *glyphs = NULL;
  cairo_hint_style_t hint_style = CAIRO_HINT_STYLE_SLIGHT;
  cairo_antialias_t antialias = CAIRO_ANTIALIAS_GRAY;
  cairo_hint_metrics_t hint_metrics = CAIRO_HINT_METRICS_OFF;
  PangoFont *hinted;
  const Declaration declarations[] = {
    { "font", parse_font, clear_font, &font },
    { "offset", parse_point, NULL, &offset },
    { "color", parse_color, NULL, &color },
    { "glyphs", parse_glyphs, clear_glyphs, &glyphs },
    { "hint-style", parse_hint_style, NULL, &hint_style },
    { "antialias", parse_antialias, NULL, &antialias },
    { "hint-metrics", parse_hint_metrics, NULL, &hint_metrics },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));

  if (font == NULL)
    {
      font = font_from_string (pango_cairo_font_map_get_default (), "Sans 15px", TRUE);
      g_assert (font);
    }

  hinted = gsk_reload_font (font, 1.0, hint_metrics, hint_style, antialias);
  g_object_unref (font);
  font = hinted;

  if (!glyphs)
    {
      const char *text = "Hello";
      PangoGlyphInfo gi = { 0, { 0, 0, 0}, { 1 } };
      guint i;

      glyphs = pango_glyph_string_new ();
      pango_glyph_string_set_size (glyphs, strlen (text));
      for (i = 0; i < strlen (text); i++)
        {
          gi.glyph = PANGO_GLYPH_INVALID_INPUT - MAX_ASCII_GLYPH + text[i];
          gi.geometry.width = GLYPH_NEEDS_WIDTH;
          glyphs->glyphs[i] = gi;
        }
    }

  if (!unpack_glyphs (font, glyphs))
    {
      gtk_css_parser_error_value (parser, "Given font cannot decode the glyph text");
      result = NULL;
    }
  else
    {
      result = gsk_text_node_new2 (font, glyphs, &color, &offset);
      if (result == NULL)
        {
          gtk_css_parser_error_value (parser, "Glyphs result in empty text");
        }
    }

  g_object_unref (font);
  pango_glyph_string_free (glyphs);

  /* return anything, whatever, just not NULL */
  if (result == NULL)
    result = create_default_render_node ();

  gdk_color_finish (&color);

  return result;
}

static GskRenderNode *
parse_blur_node (GtkCssParser *parser,
                 Context      *context)
{
  GskRenderNode *child = NULL;
  double blur_radius = 1.0;
  const Declaration declarations[] = {
    { "blur", parse_positive_double, NULL, &blur_radius },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  result = gsk_blur_node_new (child, blur_radius);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_clip_node (GtkCssParser *parser,
                 Context      *context)
{
  GskRoundedRect clip = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "clip", parse_rounded_rect, NULL, &clip },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  if (gsk_rounded_rect_is_rectilinear (&clip))
    result = gsk_clip_node_new (child, &clip.bounds);
  else
    result = gsk_rounded_clip_node_new (child, &clip);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_rounded_clip_node (GtkCssParser *parser,
                         Context      *context)
{
  GskRoundedRect clip = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "clip", parse_rounded_rect, NULL, &clip },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  result = gsk_rounded_clip_node_new (child, &clip);

  gsk_render_node_unref (child);

  return result;
}

static gboolean
parse_path (GtkCssParser *parser,
            Context      *context,
            gpointer      out_path)
{
  GskPath *path;
  char *str = NULL;

  if (!parse_string (parser, context, &str))
    return FALSE;

  path = gsk_path_parse (str);
  g_free (str);

  if (path == NULL)
    {
      gtk_css_parser_error_value (parser, "Invalid path");
      return FALSE;
    }

  *((GskPath **) out_path) = path;

  return TRUE;
}

static void
clear_path (gpointer inout_path)
{
  g_clear_pointer ((GskPath **) inout_path, gsk_path_unref);
}

static gboolean
parse_dash (GtkCssParser *parser,
            Context      *context,
            gpointer      out_dash)
{
  GArray *dash;
  double d;

  /* because CSS does this, too */
  if (gtk_css_parser_try_ident (parser, "none"))
    {
      *((GArray **) out_dash) = NULL;
      return TRUE;
    }

  dash = g_array_new (FALSE, FALSE, sizeof (float));
  while (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_NUMBER) ||
         gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      if (!gtk_css_parser_consume_number (parser, &d))
        {
          g_array_free (dash, TRUE);
          return FALSE;
        }

      g_array_append_vals (dash, (float[1]) { d }, 1);
    }

  if (dash->len == 0)
    {
      gtk_css_parser_error_syntax (parser, "Empty dash array");
      g_array_free (dash, TRUE);
      return FALSE;
    }

  *((GArray **) out_dash) = dash;

  return TRUE;
}

static void
clear_dash (gpointer inout_array)
{
  g_clear_pointer ((GArray **) inout_array, g_array_unref);
}

static gboolean
parse_fill_rule (GtkCssParser *parser,
                 Context      *context,
                 gpointer      out_rule)
{
  return parse_enum (parser, GSK_TYPE_FILL_RULE, out_rule);
}

static GskRenderNode *
parse_fill_node (GtkCssParser *parser,
                 Context      *context)
{
  GskRenderNode *child = NULL;
  GskPath *path = NULL;
  int rule = GSK_FILL_RULE_WINDING;
  const Declaration declarations[] = {
    { "child", parse_node, clear_node, &child },
    { "path", parse_path, clear_path, &path },
    { "fill-rule", parse_fill_rule, NULL, &rule },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (path == NULL)
    path = create_default_path ();
  if (child == NULL)
    {
      graphene_rect_t bounds;
      if (gsk_path_get_bounds (path, &bounds))
        child = create_default_render_node_with_bounds (&bounds);
      else
        child = gsk_container_node_new (NULL, 0);
    }

  result = gsk_fill_node_new (child, path, rule);

  gsk_path_unref (path);

  gsk_render_node_unref (child);

  return result;
}

static gboolean
parse_line_cap (GtkCssParser *parser,
                Context      *context,
                gpointer      out)
{
  return parse_enum (parser, GSK_TYPE_LINE_CAP, out);
}

static gboolean
parse_line_join (GtkCssParser *parser,
                 Context      *context,
                 gpointer      out)
{
  return parse_enum (parser, GSK_TYPE_LINE_JOIN, out);
}

static GskRenderNode *
parse_stroke_node (GtkCssParser *parser,
                   Context      *context)
{
  GskRenderNode *child = NULL;
  GskPath *path = NULL;
  double line_width = 1.0;
  int line_cap = GSK_LINE_CAP_BUTT;
  int line_join = GSK_LINE_JOIN_MITER;
  double miter_limit = 4.0;
  GArray *dash = NULL;
  double dash_offset = 0.0;
  GskStroke *stroke;

  const Declaration declarations[] = {
    { "child", parse_node, clear_node, &child },
    { "path", parse_path, clear_path, &path },
    { "line-width", parse_positive_double, NULL, &line_width },
    { "line-cap", parse_line_cap, NULL, &line_cap },
    { "line-join", parse_line_join, NULL, &line_join },
    { "miter-limit", parse_positive_double, NULL, &miter_limit },
    { "dash", parse_dash, clear_dash, &dash },
    { "dash-offset", parse_double, NULL, &dash_offset}
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (path == NULL)
    path = create_default_path ();

  stroke = gsk_stroke_new (line_width);
  gsk_stroke_set_line_cap (stroke, line_cap);
  gsk_stroke_set_line_join (stroke, line_join);
  gsk_stroke_set_miter_limit (stroke, miter_limit);
  if (dash)
    {
      gsk_stroke_set_dash (stroke, (float *) dash->data, dash->len);
      g_array_free (dash, TRUE);
    }
  gsk_stroke_set_dash_offset (stroke, dash_offset);

  if (child == NULL)
    {
      graphene_rect_t bounds;
      gsk_path_get_stroke_bounds (path, stroke, &bounds);
      child = create_default_render_node_with_bounds (&bounds);
    }

  result = gsk_stroke_node_new (child, path, stroke);

  gsk_path_unref (path);
  gsk_stroke_free (stroke);
  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_shadow_node (GtkCssParser *parser,
                   Context      *context)
{
  GskRenderNode *child = NULL;
  GArray *shadows = g_array_new (FALSE, TRUE, sizeof (GskShadowEntry));
  const Declaration declarations[] = {
    { "child", parse_node, clear_node, &child },
    { "shadows", parse_shadows, clear_shadows, shadows },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  if (shadows->len == 0)
    {
      GskShadowEntry default_shadow = { GDK_COLOR_SRGB (0, 0, 0, 1), GRAPHENE_POINT_INIT (1, 1), 0 };
      g_array_append_val (shadows, default_shadow);
    }

  result = gsk_shadow_node_new2 (child, (GskShadowEntry *)shadows->data, shadows->len);

  clear_shadows (shadows);
  g_array_free (shadows, TRUE);
  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_debug_node (GtkCssParser *parser,
                  Context      *context)
{
  char *message = NULL;
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "message", parse_string, clear_string, &message},
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  result = gsk_debug_node_new (child, message);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_subsurface_node (GtkCssParser *parser,
                       Context      *context)
{
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  result = gsk_subsurface_node_new (child, NULL);

  gsk_render_node_unref (child);

  return result;
}

static gboolean
parse_component_transfer (GtkCssParser *parser,
                          Context      *context,
                          gpointer      out_transfer)
{
  return gsk_component_transfer_parser_parse (parser, out_transfer);
}

static void
clear_component_transfer (gpointer inout_transfer)
{
  g_clear_pointer ((GskComponentTransfer **) inout_transfer, gsk_component_transfer_free);
}

static GskRenderNode *
parse_component_transfer_node (GtkCssParser *parser,
                               Context      *context)
{
  GskRenderNode *child = NULL;
  GskComponentTransfer *red = NULL;
  GskComponentTransfer *green = NULL;
  GskComponentTransfer *blue = NULL;
  GskComponentTransfer *alpha = NULL;
  const Declaration declarations[] = {
    { "child", parse_node, clear_node, &child },
    { "red", parse_component_transfer, clear_component_transfer, &red },
    { "green", parse_component_transfer, clear_component_transfer, &green },
    { "blue", parse_component_transfer, clear_component_transfer, &blue },
    { "alpha", parse_component_transfer, clear_component_transfer, &alpha },
  };
  GskRenderNode *result;

  parse_declarations (parser, context, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();
  if (red == NULL)
    red = gsk_component_transfer_new_identity ();
  if (green == NULL)
    green = gsk_component_transfer_new_identity ();
  if (blue == NULL)
    blue = gsk_component_transfer_new_identity ();
  if (alpha == NULL)
    alpha = gsk_component_transfer_new_identity ();

  result = gsk_component_transfer_node_new (child, red, green, blue, alpha);

  gsk_component_transfer_free (red);
  gsk_component_transfer_free (green);
  gsk_component_transfer_free (blue);
  gsk_component_transfer_free (alpha);

  gsk_render_node_unref (child);

  return result;
}

static gboolean
parse_node (GtkCssParser *parser,
            Context      *context,
            gpointer      out_node)
{
  static struct {
    const char *name;
    GskRenderNode * (* func) (GtkCssParser *, Context *);
  } node_parsers[] = {
    { "blend", parse_blend_node },
    { "blur", parse_blur_node },
    { "border", parse_border_node },
    { "cairo", parse_cairo_node },
    { "clip", parse_clip_node },
    { "color", parse_color_node },
    { "color-matrix", parse_color_matrix_node },
    { "container", parse_container_node },
    { "cross-fade", parse_cross_fade_node },
    { "debug", parse_debug_node },
    { "inset-shadow", parse_inset_shadow_node },
    { "linear-gradient", parse_linear_gradient_node },
    { "radial-gradient", parse_radial_gradient_node },
    { "conic-gradient", parse_conic_gradient_node },
    { "opacity", parse_opacity_node },
    { "outset-shadow", parse_outset_shadow_node },
    { "repeat", parse_repeat_node },
    { "repeating-linear-gradient", parse_repeating_linear_gradient_node },
    { "repeating-radial-gradient", parse_repeating_radial_gradient_node },
    { "rounded-clip", parse_rounded_clip_node },
    { "fill", parse_fill_node },
    { "stroke", parse_stroke_node },
    { "shadow", parse_shadow_node },
    { "text", parse_text_node },
    { "texture", parse_texture_node },
    { "texture-scale", parse_texture_scale_node },
    { "transform", parse_transform_node },
    { "glshader", parse_glshader_node },
    { "mask", parse_mask_node },
    { "subsurface", parse_subsurface_node },
    { "component-transfer", parse_component_transfer_node },
  };
  GskRenderNode **node_p = out_node;
  guint i;

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
    {
      GskRenderNode *node;
      char *node_name;

      node_name = gtk_css_parser_consume_string (parser);

      if (context->named_nodes)
        node = g_hash_table_lookup (context->named_nodes, node_name);
      else
        node = NULL;

      if (node)
        {
          *node_p = gsk_render_node_ref (node);
          g_free (node_name);
          return TRUE;
        }
      else
        {
          gtk_css_parser_error_value (parser, "No node named \"%s\"", node_name);
          g_free (node_name);
          return FALSE;
        }
    }

  for (i = 0; i < G_N_ELEMENTS (node_parsers); i++)
    {
      if (gtk_css_parser_try_ident (parser, node_parsers[i].name))
        {
          GskRenderNode *node;
          GtkCssLocation node_name_start_location, node_name_end_location;
          char *node_name;

          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
            {
              node_name_start_location = *gtk_css_parser_get_start_location (parser);
              node_name_end_location = *gtk_css_parser_get_end_location (parser);
              node_name = gtk_css_parser_consume_string (parser);
            }
          else
            node_name = NULL;

          if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
            {
              gtk_css_parser_error_syntax (parser, "Expected '{' after node name");
              return FALSE;
            }

          gtk_css_parser_end_block_prelude (parser);
          node = node_parsers[i].func (parser, context);
          if (node)
            {
              if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
                gtk_css_parser_error_syntax (parser, "Expected '}' at end of node definition");
              g_clear_pointer (node_p, gsk_render_node_unref);

              if (node_name)
                {
                  if (context->named_nodes == NULL)
                    context->named_nodes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                                  g_free, (GDestroyNotify) gsk_render_node_unref);
                  if (g_hash_table_lookup (context->named_nodes, node_name))
                    {
                      gtk_css_parser_error (parser,
                                            GTK_CSS_PARSER_ERROR_FAILED,
                                            &node_name_start_location,
                                            &node_name_end_location,
                                            "A node named \"%s\" already exists.", node_name);
                    }
                  else
                    {
                      g_hash_table_insert (context->named_nodes, g_strdup (node_name), gsk_render_node_ref (node));
                    }
                }

              *node_p = node;
            }

          g_free (node_name);

          return node != NULL;
        }
    }

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    gtk_css_parser_error_value (parser, "\"%s\" is not a valid node name",
                                gtk_css_token_get_string (gtk_css_parser_get_token (parser)));
  else
    gtk_css_parser_error_syntax (parser, "Expected a node name");

  return FALSE;
}

static void
gsk_render_node_parser_error (GtkCssParser         *parser,
                              const GtkCssLocation *start,
                              const GtkCssLocation *end,
                              const GError         *error,
                              gpointer              user_data)
{
  struct {
    GskParseErrorFunc error_func;
    gpointer user_data;
  } *error_func_pair = user_data;

  if (error_func_pair->error_func)
    error_func_pair->error_func ((const GskParseLocation *)start,
                                 (const GskParseLocation *)end,
                                 error,
                                 error_func_pair->user_data);
}

GskRenderNode *
gsk_render_node_deserialize_from_bytes (GBytes            *bytes,
                                        GskParseErrorFunc  error_func,
                                        gpointer           user_data)
{
  GskRenderNode *root = NULL;
  GtkCssParser *parser;
  Context context;
  struct {
    GskParseErrorFunc error_func;
    gpointer user_data;
  } error_func_pair = { error_func, user_data };

  parser = gtk_css_parser_new_for_bytes (bytes, NULL, gsk_render_node_parser_error,
                                         &error_func_pair, NULL);
  context_init (&context);

  while (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_AT_KEYWORD))
    {
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);
      if (!parse_color_state_rule (parser, &context))
        {
          gtk_css_parser_error_syntax (parser, "Unknown @ rule");
        }
      gtk_css_parser_end_block (parser);
    }

  root = parse_container_node (parser, &context);

  if (root && gsk_container_node_get_n_children (root) == 1)
    {
      GskRenderNode *child = gsk_container_node_get_child (root, 0);

      gsk_render_node_ref (child);
      gsk_render_node_unref (root);
      root = child;
    }

  context_finish (&context);
  gtk_css_parser_unref (parser);

  return root;
}



typedef struct
{
  int indentation_level;
  GString *str;
  GHashTable *named_nodes;
  gsize named_node_counter;
  GHashTable *named_textures;
  gsize named_texture_counter;
  GHashTable *named_color_states;
  gsize named_color_state_counter;
  GHashTable *fonts;
} Printer;

static void
printer_init_check_texture (Printer    *printer,
                            GdkTexture *texture)
{
  gpointer name;

  if (!g_hash_table_lookup_extended (printer->named_textures, texture, NULL, &name))
    g_hash_table_insert (printer->named_textures, texture, NULL);
  else if (name == NULL)
    g_hash_table_insert (printer->named_textures, texture, g_strdup (""));
}

typedef struct {
  hb_face_t *face;
  hb_subset_input_t *input;
  gboolean serialized;
} FontInfo;

static void
font_info_free (gpointer data)
{
  FontInfo *info = (FontInfo *) data;

  hb_face_destroy (info->face);
  if (info->input)
    hb_subset_input_destroy (info->input);
  g_free (info);
}

static void
printer_init_collect_font_info (Printer       *printer,
                                GskRenderNode *node)
{
  PangoFont *font;
  FontInfo *info;

  font = gsk_text_node_get_font (node);

  info = (FontInfo *) g_hash_table_lookup (printer->fonts, hb_font_get_face (pango_font_get_hb_font (font)));
  if (!info)
    {
      info = g_new0 (FontInfo, 1);

      info->face = hb_face_reference (hb_font_get_face (pango_font_get_hb_font (font)));
      if (!g_object_get_data (G_OBJECT (pango_font_get_font_map (font)), "font-files"))
        {
          if (g_strcmp0 (g_getenv ("GSK_SUBSET_FONTS"), "1") == 0)
            {
              info->input = hb_subset_input_create_or_fail ();
              hb_subset_input_set_flags (info->input, HB_SUBSET_FLAGS_RETAIN_GIDS);
            }
          else
            info->serialized = TRUE; /* Don't subset (or serialize) system fonts */
        }

      g_hash_table_insert (printer->fonts, info->face, info);
    }

  if (info->input)
    {
      const PangoGlyphInfo *glyphs;
      guint n_glyphs;

      glyphs = gsk_text_node_get_glyphs (node, &n_glyphs);

      for (guint i = 0; i < n_glyphs; i++)
        hb_set_add (hb_subset_input_glyph_set (info->input), glyphs[i].glyph);
    }
}

static void
printer_init_duplicates_for_node (Printer       *printer,
                                  GskRenderNode *node)
{
  gpointer name;

  if (!g_hash_table_lookup_extended (printer->named_nodes, node, NULL, &name))
    g_hash_table_insert (printer->named_nodes, node, NULL);
  else if (name == NULL)
    g_hash_table_insert (printer->named_nodes, node, g_strdup (""));

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_TEXT_NODE:
      printer_init_collect_font_info (printer, node);
      break;

    case GSK_COLOR_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_CAIRO_NODE:
      /* no children */
      break;

    case GSK_TEXTURE_NODE:
      printer_init_check_texture (printer, gsk_texture_node_get_texture (node));
      break;

    case GSK_TEXTURE_SCALE_NODE:
      printer_init_check_texture (printer, gsk_texture_scale_node_get_texture (node));
      break;

    case GSK_TRANSFORM_NODE:
      printer_init_duplicates_for_node (printer, gsk_transform_node_get_child (node));
      break;

    case GSK_OPACITY_NODE:
      printer_init_duplicates_for_node (printer, gsk_opacity_node_get_child (node));
      break;

    case GSK_COLOR_MATRIX_NODE:
      printer_init_duplicates_for_node (printer, gsk_color_matrix_node_get_child (node));
      break;

    case GSK_BLUR_NODE:
      printer_init_duplicates_for_node (printer, gsk_blur_node_get_child (node));
      break;

    case GSK_REPEAT_NODE:
      printer_init_duplicates_for_node (printer, gsk_repeat_node_get_child (node));
      break;

    case GSK_CLIP_NODE:
      printer_init_duplicates_for_node (printer, gsk_clip_node_get_child (node));
      break;

    case GSK_ROUNDED_CLIP_NODE:
      printer_init_duplicates_for_node (printer, gsk_rounded_clip_node_get_child (node));
      break;

    case GSK_SHADOW_NODE:
      printer_init_duplicates_for_node (printer, gsk_shadow_node_get_child (node));
      break;

    case GSK_DEBUG_NODE:
      printer_init_duplicates_for_node (printer, gsk_debug_node_get_child (node));
      break;

    case GSK_FILL_NODE:
      printer_init_duplicates_for_node (printer, gsk_fill_node_get_child (node));
      break;

    case GSK_STROKE_NODE:
      printer_init_duplicates_for_node (printer, gsk_stroke_node_get_child (node));
      break;

    case GSK_BLEND_NODE:
      printer_init_duplicates_for_node (printer, gsk_blend_node_get_bottom_child (node));
      printer_init_duplicates_for_node (printer, gsk_blend_node_get_top_child (node));
      break;

    case GSK_MASK_NODE:
      printer_init_duplicates_for_node (printer, gsk_mask_node_get_source (node));
      printer_init_duplicates_for_node (printer, gsk_mask_node_get_mask (node));
      break;

    case GSK_CROSS_FADE_NODE:
      printer_init_duplicates_for_node (printer, gsk_cross_fade_node_get_start_child (node));
      printer_init_duplicates_for_node (printer, gsk_cross_fade_node_get_end_child (node));
      break;

    case GSK_GL_SHADER_NODE:
      {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        guint i;

        for (i = 0; i < gsk_gl_shader_node_get_n_children (node); i++)
          {
            printer_init_duplicates_for_node (printer, gsk_gl_shader_node_get_child (node, i));
          }
G_GNUC_END_IGNORE_DEPRECATIONS
      }
      break;

    case GSK_CONTAINER_NODE:
      {
        guint i;

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          {
            printer_init_duplicates_for_node (printer, gsk_container_node_get_child (node, i));
          }
      }
      break;

    case GSK_SUBSURFACE_NODE:
      printer_init_duplicates_for_node (printer, gsk_subsurface_node_get_child (node));
      break;

    case GSK_COMPONENT_TRANSFER_NODE:
      printer_init_duplicates_for_node (printer, gsk_component_transfer_node_get_child (node));
      break;

    default:
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      break;

    }
}

static void
printer_init (Printer       *self,
              GskRenderNode *node)
{
  self->indentation_level = 0;
  self->str = g_string_new (NULL);
  self->named_nodes = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  self->named_node_counter = 0;
  self->named_textures = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  self->named_texture_counter = 0;
  self->named_color_states = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  self->named_color_state_counter = 0;
  self->fonts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, font_info_free);

  printer_init_duplicates_for_node (self, node);
}

static void
printer_clear (Printer *self)
{
  g_string_free (self->str, TRUE);
  g_hash_table_unref (self->named_nodes);
  g_hash_table_unref (self->named_textures);
  g_hash_table_unref (self->named_color_states);
  g_hash_table_unref (self->fonts);
}

#define IDENT_LEVEL 2 /* Spaces per level */
static void
_indent (Printer *self)
{
  if (self->indentation_level > 0)
    g_string_append_printf (self->str, "%*s", self->indentation_level * IDENT_LEVEL, " ");
}
#undef IDENT_LEVEL

static void
start_node (Printer    *self,
            const char *node_type,
            const char *node_name)
{
  g_string_append_printf (self->str, "%s ", node_type);
  if (node_name)
    {
      gtk_css_print_string (self->str, node_name, FALSE);
      g_string_append_c (self->str, ' ');
    }
  g_string_append_printf (self->str, "{\n");
  self->indentation_level ++;
}

static void
end_node (Printer *self)
{
  self->indentation_level --;
  _indent (self);
  g_string_append (self->str, "}\n");
}

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%g", d);
  g_string_append (string, buf);
}


static void
append_rect (GString               *str,
             const graphene_rect_t *r)
{
  string_append_double (str, r->origin.x);
  g_string_append_c (str, ' ');
  string_append_double (str, r->origin.y);
  g_string_append_c (str, ' ');
  string_append_double (str, r->size.width);
  g_string_append_c (str, ' ');
  string_append_double (str, r->size.height);
}

static void
append_rounded_rect (GString              *str,
                     const GskRoundedRect *r)
{
  append_rect (str, &r->bounds);

  if (!gsk_rounded_rect_is_rectilinear (r))
    {
      gboolean all_the_same = TRUE;
      gboolean all_square = TRUE;
      float w = r->corner[0].width;
      float h = r->corner[0].height;
      int i;

      for (i = 1; i < 4; i ++)
        {
          if (r->corner[i].width != w ||
              r->corner[i].height != h)
            all_the_same = FALSE;

          if (r->corner[i].width != r->corner[i].height)
            all_square = FALSE;

        }

      g_string_append (str, " / ");

      if (all_the_same)
        {
          string_append_double (str, w);
        }
      else if (all_square)
        {
          string_append_double (str, r->corner[0].width);
          g_string_append_c (str, ' ');
          string_append_double (str, r->corner[1].width);
          g_string_append_c (str, ' ');
          string_append_double (str, r->corner[2].width);
          g_string_append_c (str, ' ');
          string_append_double (str, r->corner[3].width);
        }
      else
        {
          for (i = 0; i < 4; i ++)
            {
              string_append_double (str, r->corner[i].width);
              g_string_append_c (str, ' ');
            }

          g_string_append (str, "/ ");

          for (i = 0; i < 3; i ++)
            {
              string_append_double (str, r->corner[i].height);
              g_string_append_c (str, ' ');
            }

          string_append_double (str, r->corner[3].height);
        }
    }
}

static void
append_point (GString                *str,
              const graphene_point_t *p)
{
  string_append_double (str, p->x);
  g_string_append_c (str, ' ');
  string_append_double (str, p->y);
}

static void
append_vec4 (GString               *str,
             const graphene_vec4_t *v)
{
  string_append_double (str, graphene_vec4_get_x (v));
  g_string_append_c (str, ' ');
  string_append_double (str, graphene_vec4_get_y (v));
  g_string_append_c (str, ' ');
  string_append_double (str, graphene_vec4_get_z (v));
  g_string_append_c (str, ' ');
  string_append_double (str, graphene_vec4_get_w (v));
}

static void
append_float_param (Printer    *p,
                    const char *param_name,
                    float       value,
                    float       default_value)
{
  /* Don't approximate-compare here, better be too verbose */
  if (value == default_value)
    return;

  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  string_append_double (p->str, value);
  g_string_append (p->str, ";\n");
}

static void
append_unsigned_param (Printer    *p,
                       const char *param_name,
                       guint       value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: %u;\n", param_name, value);
}

static void
append_boolean_param (Printer    *p,
                      const char *param_name,
                      gboolean    value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: %s;\n", param_name, value ? "true" : "false");
}

static void
print_color_state (Printer       *p,
                   GdkColorState *color_state)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (color_state) ||
      GDK_IS_BUILTIN_COLOR_STATE (color_state))
    {
      g_string_append (p->str, gdk_color_state_get_name (color_state));
    }
  else
    {
      gpointer name;

      if (!g_hash_table_lookup_extended (p->named_color_states, color_state, NULL, &name))
        {
          name = g_strdup_printf ("cicp%zu", ++p->named_color_state_counter);
          g_hash_table_insert (p->named_color_states, color_state, name);
        }
      g_assert (name != NULL);
      g_string_append_c (p->str, '"');
      g_string_append (p->str, name);
      g_string_append_c (p->str, '"');
    }
}

static void
print_color (Printer        *p,
             const GdkColor *color)
{
  if (gdk_color_state_equal (color->color_state, GDK_COLOR_STATE_SRGB) &&
      round (CLAMP (color->red, 0, 1) * 255) == color->red * 255 &&
      round (CLAMP (color->green, 0, 1) * 255) == color->green * 255 &&
      round (CLAMP (color->blue, 0, 1) * 255) == color->blue * 255)
    {
      gdk_rgba_print ((const GdkRGBA *) color->values, p->str);
    }
  else
    {
      g_string_append (p->str, "color(");
      print_color_state (p, color->color_state);
      g_string_append_c (p->str, ' ');
      string_append_double (p->str, color->r);
      g_string_append_c (p->str, ' ');
      string_append_double (p->str, color->g);
      g_string_append_c (p->str, ' ');
      string_append_double (p->str, color->b);
      if (color->a < 1)
        {
          g_string_append (p->str, " / ");
          string_append_double (p->str, color->a);
        }
      g_string_append_c (p->str, ')');
    }
}

static void
append_color_param (Printer        *p,
                    const char     *param_name,
                    const GdkColor *color)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  print_color (p, color);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_rect_param (Printer               *p,
                   const char            *param_name,
                   const graphene_rect_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_rect (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_rounded_rect_param (Printer              *p,
                           const char           *param_name,
                           const GskRoundedRect *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_rounded_rect (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_point_param (Printer                *p,
                    const char             *param_name,
                    const graphene_point_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_point (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_string_param (Printer    *p,
                     const char *param_name,
                     const char *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  gtk_css_print_string (p->str, value, TRUE);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static const char *
enum_to_nick (GType type,
              int   value)
{
  GEnumClass *class;
  GEnumValue *v;

  class = g_type_class_ref (type);
  v = g_enum_get_value (class, value);
  g_type_class_unref (class);

  return v->value_nick;
}

static void
append_enum_param (Printer    *p,
                   const char *param_name,
                   GType       type,
                   int         value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  g_string_append (p->str, enum_to_nick (type, value));
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_vec4_param (Printer               *p,
                   const char            *param_name,
                   const graphene_vec4_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_vec4 (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_matrix_param (Printer                 *p,
                     const char              *param_name,
                     const graphene_matrix_t *value)
{
  GskTransform *transform = NULL;

  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);

  transform = gsk_transform_matrix (transform, value);
  gsk_transform_print (transform,p->str);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');

  gsk_transform_unref (transform);
}

static void
append_transform_param (Printer      *p,
                        const char   *param_name,
                        GskTransform *transform)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  gsk_transform_print (transform, p->str);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_memory_format_param (Printer         *p,
                            const char      *param_name,
                            GdkMemoryFormat  format)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: %s;\n", param_name, gdk_memory_format_get_name (format));
}

static void render_node_print (Printer       *p,
                               GskRenderNode *node);

static void
append_node_param (Printer       *p,
                   const char    *param_name,
                   GskRenderNode *node)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  render_node_print (p, node);
}

static void
append_stops_param (Printer               *p,
                    const char            *param_name,
                    const GskGradientStop *stops,
                    gsize                  n_stops)
{
  gsize i;

  _indent (p);
  g_string_append (p->str, param_name);
  g_string_append (p->str, ": ");

  for (i = 0; i < n_stops; i ++)
    {
      if (i > 0)
        g_string_append (p->str, ", ");

      string_append_double (p->str, stops[i].offset);

      if (i > 0 && stops[i].transition_hint != 0.5)
        {
          g_string_append_c (p->str, ' ');
          string_append_double (p->str, stops[i].transition_hint);
        }

      g_string_append_c (p->str, ' ');
      print_color (p, &stops[i].color);
    }
  g_string_append (p->str, ";\n");
}

static void
append_color_state_param (Printer       *p,
                          const char    *param_name,
                          GdkColorState *color_state,
                          GdkColorState *default_value)
{
  if (gdk_color_state_equal (color_state, default_value))
    return;

  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  print_color_state (p, color_state);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static cairo_status_t
cairo_write_array (void                *closure,
                   const unsigned char *data,
                   unsigned int         length)
{
  g_byte_array_append (closure, data, length);

  return CAIRO_STATUS_SUCCESS;
}

static void
cairo_destroy_array (gpointer array)
{
  g_byte_array_free (array, TRUE);
}

static void
append_escaping_newlines (GString    *str,
                          const char *string)
{
  gsize len;

  do {
    len = strcspn (string, "\n");
    g_string_append_len (str, string, len);
    string += len;
    if (*string)
      {
        g_string_append (str, "\\\n");
        string++;
      }
  } while (*string);
}

/* like g_base64 encode, but breaks lines
 * in CSS-compatible way
 */
static char *
base64_encode_with_linebreaks (const guchar *data,
                               gsize         len)
{
  gsize max;
  char *out;
  int state = 0, outlen;
  int save = 0;

  g_return_val_if_fail (data != NULL || len == 0, NULL);

  /* We can use a smaller limit here, since we know the saved state is 0,
     +1 is needed for trailing \0, also check for unlikely integer overflow */
  g_return_val_if_fail (len < ((G_MAXSIZE - 1) / 4 - 1) * 3, NULL);

  /* The glib docs say:
   *
   * The output buffer must be large enough to fit all the data that will
   * be written to it. Due to the way base64 encodes you will need
   * at least: (@len / 3 + 1) * 4 + 4 bytes (+ 4 may be needed in case of
   * non-zero state). If you enable line-breaking you will need at least:
   * ((@len / 3 + 1) * 4 + 4) / 76 + 1 bytes of extra space.
   */
  max = (len / 3 + 1) * 4;
  max += ((len / 3 + 1) * 4 + 4) / 76 + 1;
  /* and the null byte */
  max += 1;

  out = g_malloc (max);

  outlen = g_base64_encode_step (data, len, TRUE, out, &state, &save);
  outlen += g_base64_encode_close (TRUE, out + outlen, &state, &save);
  out[outlen] = '\0';

  return out;
}

static void
append_bytes_url (Printer    *p,
                  GBytes     *bytes,
                  const char *mime_type)
{
  char *b64;

  g_string_append_printf (p->str, "url(\"data:%s;base64,\\\n", mime_type ? mime_type : "");
  b64 = base64_encode_with_linebreaks (g_bytes_get_data (bytes, NULL),
                                       g_bytes_get_size (bytes));
  append_escaping_newlines (p->str, b64);
  g_free (b64);
  g_string_append (p->str, "\")");
}

static void
append_bytes_param (Printer    *p,
                    const char *param_name,
                    GBytes     *bytes,
                    const char *mime_type)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_bytes_url (p, bytes, mime_type);
  g_string_append (p->str, ";\n");
}

static void
append_compressed_bytes_param (Printer    *p,
                               const char *param_name,
                               GBytes     *bytes)
{
  GZlibCompressor *compressor;
  GBytes *compressed_bytes;

  compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, 9);
#if GLIB_CHECK_VERSION (2, 85, 0)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_zlib_compressor_set_os (compressor, 3);
G_GNUC_END_IGNORE_DEPRECATIONS
#endif

  compressed_bytes = g_converter_convert_bytes (G_CONVERTER (compressor), bytes, NULL);
  g_assert (compressed_bytes != NULL);

#if !GLIB_CHECK_VERSION (2, 85, 0)
  /* fallback for above */
  ((guchar *) g_bytes_get_data (compressed_bytes, NULL))[9] = 3;
#endif

  append_bytes_param (p, param_name, compressed_bytes, "application/gzip");

  g_object_unref (compressor);
  g_bytes_unref (compressed_bytes);
}

static void
append_memory_texture (Printer    *p,
                       GdkTexture *texture)
{
  const GdkMemoryLayout *layout;
  gsize i;

  layout = gdk_memory_texture_get_layout (GDK_MEMORY_TEXTURE (texture));

  g_string_append_printf (p->str, "memory {\n");
  p->indentation_level ++;

  append_memory_format_param (p, "format", layout->format);
  append_unsigned_param (p, "width", layout->width);
  append_unsigned_param (p, "height", layout->height);

  if (gdk_memory_format_get_n_planes (layout->format) > 0 ||
      layout->planes[0].offset != 0)
    {
      _indent (p);
      g_string_append (p->str, "offset: ");
      for (i = 0; i < gdk_memory_format_get_n_planes (layout->format); i++)
        {
          if (i > 0)
            g_string_append_printf (p->str, ", ");
          g_string_append_printf (p->str, "%zu", layout->planes[i].offset);
        }
      g_string_append (p->str, ";\n");
    }
  _indent (p);
  g_string_append (p->str, "stride: ");
  for (i = 0; i < gdk_memory_format_get_n_planes (layout->format); i++)
    {
      if (i > 0)
        g_string_append_printf (p->str, ", ");
      g_string_append_printf (p->str, "%zu", layout->planes[i].stride);
    }
  g_string_append (p->str, ";\n");

  append_color_state_param (p, "color-state", gdk_texture_get_color_state (texture), GDK_COLOR_STATE_SRGB);
  append_compressed_bytes_param (p, "data", gdk_memory_texture_get_bytes (GDK_MEMORY_TEXTURE (texture)));

  p->indentation_level --;
  _indent (p);
  g_string_append_printf (p->str, "}\n");
}

static void
append_dmabuf_texture (Printer    *p,
                       GdkTexture *texture)
{
  GdkMemoryLayout layout;
  GBytes *bytes;
  gsize i, n_planes;
  char fourcc_str[5];
  GdkColorState *default_color_state;
  const GdkDmabuf *dmabuf;

  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));

  gdk_memory_layout_init (&layout,
                          gdk_texture_get_format (texture),
                          gdk_texture_get_width (texture),
                          gdk_texture_get_height (texture),
                          1);

  bytes = gdk_texture_download_bytes (texture, &layout);

  n_planes = gdk_memory_format_get_n_planes (layout.format);
  default_color_state = GDK_COLOR_STATE_SRGB;

  g_string_append_printf (p->str, "dmabuf {\n");
  p->indentation_level ++;

  if (dmabuf->fourcc == gdk_memory_format_get_dmabuf_rgb_fourcc (layout.format))
    default_color_state = GDK_COLOR_STATE_SRGB;
  else if (dmabuf->fourcc == gdk_memory_format_get_dmabuf_yuv_fourcc (layout.format))
    default_color_state = GDK_COLOR_STATE_YUV;
  else
    {
      g_assert_not_reached ();
    }

  fourcc_str[0] = dmabuf->fourcc >> 0;
  fourcc_str[1] = dmabuf->fourcc >> 8;
  fourcc_str[2] = dmabuf->fourcc >> 16;
  fourcc_str[3] = dmabuf->fourcc >> 24;
  fourcc_str[4] = 0;
  if (g_ascii_isalnum (fourcc_str[0]) &&
      g_ascii_isalnum (fourcc_str[1]) &&
      g_ascii_isalnum (fourcc_str[2]) &&
      g_ascii_isalnum (fourcc_str[3]))
    {
      append_string_param (p, "fourcc", fourcc_str);
    }
  else
    {
      append_unsigned_param (p, "fourcc", dmabuf->fourcc);
    }
  if (dmabuf->modifier != DRM_FORMAT_MOD_LINEAR)
    {
      _indent (p);
      g_string_append_printf (p->str, "/* modifier: %llu (0x%0llX) */\n",
                              (unsigned long long) dmabuf->modifier, (unsigned long long) dmabuf->modifier);
    }
  append_unsigned_param (p, "width", layout.width);
  append_unsigned_param (p, "height", layout.height);
  if (gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_STRAIGHT)
    append_boolean_param (p, "premultiplied", FALSE);

  if (n_planes > 0 || layout.planes[0].offset != 0)
    {
      _indent (p);
      g_string_append (p->str, "offset: ");
      for (i = 0; i < n_planes; i++)
        {
          if (i > 0)
            g_string_append_printf (p->str, ", ");
          g_string_append_printf (p->str, "%zu", layout.planes[i].offset);
        }
      g_string_append (p->str, ";\n");
    }
  for (i = 0; i < n_planes; i++)
    {
      _indent (p);
      g_string_append (p->str, "stride: ");
      for (i = 0; i < n_planes; i++)
        {
          if (i > 0)
            g_string_append_printf (p->str, ", ");
          g_string_append_printf (p->str, "%zu", layout.planes[i].stride);
        }
      g_string_append (p->str, ";\n");
    }

  append_color_state_param (p, "color-state", gdk_texture_get_color_state (texture), default_color_state);
  append_compressed_bytes_param (p, "data", bytes);

  p->indentation_level --;
  _indent (p);
  g_string_append_printf (p->str, "}\n");

  g_bytes_unref (bytes);
}

#ifdef GDK_WINDOWING_WIN32
static void
append_d3d12_texture (Printer    *p,
                      GdkTexture *texture)
{
  GdkMemoryLayout layout;
  GBytes *bytes;
  D3D12_RESOURCE_DESC desc;

  gdk_memory_layout_init (&layout,
                          gdk_texture_get_format (texture),
                          gdk_texture_get_width (texture),
                          gdk_texture_get_height (texture),
                          gdk_memory_format_alignment (gdk_texture_get_format (texture)));

  bytes = gdk_texture_download_bytes (texture, &layout);

  g_string_append_printf (p->str, "d3d12 {\n");
  p->indentation_level ++;

  ID3D12Resource_GetDesc (gdk_d3d12_texture_get_resource (GDK_D3D12_TEXTURE (texture)), &desc);
  append_unsigned_param (p, "format", desc.Format);
  append_unsigned_param (p, "width", layout.width);
  append_unsigned_param (p, "height", layout.height);
  if (gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_STRAIGHT)
    append_boolean_param (p, "premultiplied", FALSE);

  append_color_state_param (p, "color-state", gdk_texture_get_color_state (texture), GDK_COLOR_STATE_SRGB);
  append_compressed_bytes_param (p, "data", bytes);

  p->indentation_level --;
  _indent (p);
  g_string_append_printf (p->str, "}\n");

  g_bytes_unref (bytes);
}
#endif

static void
append_texture_param (Printer    *p,
                      const char *param_name,
                      GdkTexture *texture)
{
  GBytes *bytes;
  const char *texture_name;

  _indent (p);

  g_string_append_printf (p->str, "%s: ", param_name);

  texture_name = g_hash_table_lookup (p->named_textures, texture);
  if (texture_name == NULL)
    {
      /* nothing to do here, texture is unique */
    }
  else if (texture_name[0])
    {
      /* texture has been named already */
      gtk_css_print_string (p->str, texture_name, TRUE);
      g_string_append (p->str, ";\n");
      return;
    }
  else
    {
      /* texture needs a name */
      char *new_name = g_strdup_printf ("texture%zu", ++p->named_texture_counter);
      gtk_css_print_string (p->str, new_name, TRUE);
      g_string_append_c (p->str, ' ');
      g_hash_table_insert (p->named_textures, texture, new_name);
    }

  if (GDK_IS_MEMORY_TEXTURE (texture))
    {
      append_memory_texture (p, texture);
    }
  else if (GDK_IS_DMABUF_TEXTURE (texture))
    {
      append_dmabuf_texture (p, texture);
    }
#ifdef GDK_WINDOWING_WIN32
  else if (GDK_IS_D3D12_TEXTURE (texture))
    {
      append_d3d12_texture (p, texture);
    }
#endif
  else
    {
      switch (gdk_texture_get_depth (texture))
        {
        case GDK_MEMORY_U8:
        case GDK_MEMORY_U8_SRGB:
          bytes = gdk_texture_save_to_png_bytes (texture);
          append_bytes_url (p, bytes, "image/png");
          g_bytes_unref (bytes);
          g_string_append (p->str, ";\n");
          break;

        case GDK_MEMORY_FLOAT16:
        case GDK_MEMORY_FLOAT32:
          bytes = gdk_texture_save_to_tiff_bytes (texture);
          append_bytes_url (p, bytes, "image/tiff");
          g_bytes_unref (bytes);
          g_string_append (p->str, ";\n");
          break;

        case GDK_MEMORY_NONE:
        case GDK_N_DEPTHS:
        default:
          g_assert_not_reached ();
        }
    }
}

static void
gsk_text_node_serialize_font (GskRenderNode *node,
                              Printer       *p)
{
  PangoFont *font = gsk_text_node_get_font (node);
  PangoFontDescription *desc;
  char *s;
  FontInfo *info;
  hb_face_t *face;
  hb_blob_t *blob;
  const char *data;
  guint length;
  GBytes *bytes;

  desc = pango_font_describe_with_absolute_size (font);
  s = pango_font_description_to_string (desc);
  g_string_append_printf (p->str, "\"%s\"", s);
  g_free (s);
  pango_font_description_free (desc);

  info = g_hash_table_lookup (p->fonts, hb_font_get_face (pango_font_get_hb_font (font)));
  if (info->serialized)
    return;

  if (info->input)
    face = hb_subset_or_fail (info->face, info->input);
  else
    face = hb_face_reference (info->face);

  blob = hb_face_reference_blob (face);
  data = hb_blob_get_data (blob, &length);
  bytes = g_bytes_new_static (data, length);

  g_string_append (p->str, " ");
  append_bytes_url (p, bytes, "font/ttf");

  g_bytes_unref (bytes);
  hb_blob_destroy (blob);
  hb_face_destroy (face);

  info->serialized = TRUE;
}

static void
gsk_text_node_serialize_font_options (GskRenderNode *node,
                                      Printer       *p)
{
  PangoFont *font = gsk_text_node_get_font (node);
  cairo_scaled_font_t *sf = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
  cairo_font_options_t *options;
  cairo_hint_style_t hint_style;
  cairo_antialias_t antialias;
  cairo_hint_metrics_t hint_metrics;

  options = cairo_font_options_create ();
  cairo_scaled_font_get_font_options (sf, options);
  hint_style = cairo_font_options_get_hint_style (options);
  antialias = cairo_font_options_get_antialias (options);
  hint_metrics = cairo_font_options_get_hint_metrics (options);
  cairo_font_options_destroy (options);

  /* medium and full are identical in the absence of subpixel modes */
  if (hint_style == CAIRO_HINT_STYLE_MEDIUM)
    hint_style = CAIRO_HINT_STYLE_FULL;

  if (hint_style == CAIRO_HINT_STYLE_NONE ||
      hint_style == CAIRO_HINT_STYLE_FULL)
    append_enum_param (p, "hint-style", CAIRO_GOBJECT_TYPE_HINT_STYLE, hint_style);

  /* CAIRO_ANTIALIAS_NONE is the only value we ever emit here, since gray is the default,
   * and we don't accept any other values.
   */
  if (antialias == CAIRO_ANTIALIAS_NONE)
    append_enum_param (p, "antialias", CAIRO_GOBJECT_TYPE_ANTIALIAS, antialias);

  /* CAIRO_HINT_METRICS_ON is the only value we ever emit here, since off is the default,
   * and we don't accept any other values.
   */
  if (hint_metrics == CAIRO_HINT_METRICS_ON)
    append_enum_param (p, "hint-metrics", CAIRO_GOBJECT_TYPE_HINT_METRICS, CAIRO_HINT_METRICS_ON);
}

void
gsk_text_node_serialize_glyphs (GskRenderNode *node,
                                GString       *p)
{
  const guint n_glyphs = gsk_text_node_get_num_glyphs (node);
  const PangoGlyphInfo *glyphs = gsk_text_node_get_glyphs (node, NULL);
  PangoFont *font = gsk_text_node_get_font (node);
  GString *str;
  guint i, j;
  PangoGlyphString *ascii;

  ascii = create_ascii_glyphs (font);
  str = g_string_new ("");

  for (i = 0; i < n_glyphs; i++)
    {
      if (ascii)
        {
          for (j = 0; j < ascii->num_glyphs; j++)
            {
              if (glyphs[i].glyph == ascii->glyphs[j].glyph &&
                  glyphs[i].geometry.width == ascii->glyphs[j].geometry.width &&
                  glyphs[i].geometry.x_offset == 0 &&
                  glyphs[i].geometry.y_offset == 0 &&
                  glyphs[i].attr.is_cluster_start &&
                  !glyphs[i].attr.is_color)
                {
                  switch (j + MIN_ASCII_GLYPH)
                    {
                      case '\\':
                        g_string_append (str, "\\\\");
                        break;
                      case '"':
                        g_string_append (str, "\\\"");
                        break;
                      default:
                        g_string_append_c (str, j + MIN_ASCII_GLYPH);
                        break;
                    }
                  break;
                }
            }
          if (j != ascii->num_glyphs)
            continue;
        }

      if (str->len)
        {
          g_string_append_printf (p, "\"%s\", ", str->str);
          g_string_set_size (str, 0);
        }

      g_string_append_printf (p, "%u ", glyphs[i].glyph);
      string_append_double (p, (double) glyphs[i].geometry.width / PANGO_SCALE);
      if (!glyphs[i].attr.is_cluster_start ||
          glyphs[i].attr.is_color ||
          glyphs[i].geometry.x_offset != 0 ||
          glyphs[i].geometry.y_offset != 0)
        {
          g_string_append (p, " ");
          string_append_double (p, (double) glyphs[i].geometry.x_offset / PANGO_SCALE);
          g_string_append (p, " ");
          string_append_double (p, (double) glyphs[i].geometry.y_offset / PANGO_SCALE);
          if (!glyphs[i].attr.is_cluster_start)
            g_string_append (p, " same-cluster");
          if (glyphs[i].attr.is_color)
            g_string_append (p, " color");
        }

      if (i + 1 < n_glyphs)
        g_string_append (p, ", ");
    }

  if (str->len)
    g_string_append_printf (p, "\"%s\"", str->str);

  g_string_free (str, TRUE);
  if (ascii)
    pango_glyph_string_free (ascii);
}

static void
append_path_param (Printer    *p,
                   const char *param_name,
                   GskPath    *path)
{
  char *str, *s;

  _indent (p);
  g_string_append (p->str, "path: \"\\\n");
  str = gsk_path_to_string (path);
  /* Put each command on a new line */
  for (s = str; *s; s++)
    {
      if (*s == ' ' &&
          (s[1] == 'M' || s[1] == 'C' || s[1] == 'Z' || s[1] == 'L'))
        *s = '\n';
    }
  append_escaping_newlines (p->str, str);
  g_string_append (p->str, "\";\n");
  g_free (str);
}

static void
append_dash_param (Printer     *p,
                   const char  *param_name,
                   const float *dash,
                   gsize        n_dash)
{
  _indent (p);
  g_string_append (p->str, "dash: ");

  if (n_dash == 0)
    {
      g_string_append (p->str, "none");
    }
  else
    {
      gsize i;

      string_append_double (p->str, dash[0]);
      for (i = 1; i < n_dash; i++)
        {
          g_string_append_c (p->str, ' ');
          string_append_double (p->str, dash[i]);
        }
    }

  g_string_append (p->str, ";\n");
}

static const char *
hue_interpolation_to_string (GskHueInterpolation value)
{
  const char *name[] = { "shorter", "longer", "increasing", "decreasing" };

  return name[value];
}

static void
append_hue_interpolation_param (Printer             *p,
                                const char          *param_name,
                                GskHueInterpolation  value,
                                GskHueInterpolation  default_value)
{
  if (value == default_value)
    return;

  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  g_string_append (p->str, hue_interpolation_to_string (value));
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_component_transfer_param (Printer                    *p,
                                 const char                 *param_name,
                                 const GskComponentTransfer *transfer)
{
  if (transfer->kind == GSK_COMPONENT_TRANSFER_IDENTITY)
    return;

  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  gsk_component_transfer_print (transfer, p->str);
  g_string_append (p->str, ";\n");
}

static void
render_node_print (Printer       *p,
                   GskRenderNode *node)
{
  const char *node_name;

  node_name = g_hash_table_lookup (p->named_nodes, node);
  if (node_name == NULL)
    {
      /* nothing to do here, node is unique */
    }
  else if (node_name[0])
    {
      /* node has been named already */
      gtk_css_print_string (p->str, node_name, TRUE);
      g_string_append (p->str, ";\n");
      return;
    }
  else
    {
      /* node needs a name */
      char *new_name = g_strdup_printf ("node%zu", ++p->named_node_counter);
      g_hash_table_insert (p->named_nodes, node, new_name);
      node_name = new_name;
    }

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      {
        guint i;

        start_node (p, "container", node_name);
        for (i = 0; i < gsk_container_node_get_n_children (node); i ++)
          {
            GskRenderNode *child = gsk_container_node_get_child (node, i);

            /* Only in container nodes do we want nodes to be indented. */
            _indent (p);
            render_node_print (p, child);
          }
        end_node (p);
      }
      break;

    case GSK_COLOR_NODE:
      {
        start_node (p, "color", node_name);
        append_rect_param (p, "bounds", &node->bounds);
        append_color_param (p, "color", gsk_color_node_get_gdk_color (node));
        end_node (p);
      }
      break;

    case GSK_CROSS_FADE_NODE:
      {
        start_node (p, "cross-fade", node_name);

        append_float_param (p, "progress", gsk_cross_fade_node_get_progress (node), 0.5f);
        append_node_param (p, "start", gsk_cross_fade_node_get_start_child (node));
        append_node_param (p, "end", gsk_cross_fade_node_get_end_child (node));

        end_node (p);
      }
      break;

    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
      {
        if (gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE)
          start_node (p, "repeating-linear-gradient", node_name);
        else
          start_node (p, "linear-gradient", node_name);

        append_rect_param (p, "bounds", &node->bounds);
        append_point_param (p, "start", gsk_linear_gradient_node_get_start (node));
        append_point_param (p, "end", gsk_linear_gradient_node_get_end (node));
        append_stops_param (p, "stops", gsk_gradient_node_get_stops (node),
                                        gsk_gradient_node_get_n_stops (node));

        append_color_state_param (p, "interpolation",
                                  gsk_gradient_node_get_interpolation (node),
                                  GDK_COLOR_STATE_SRGB);
        append_hue_interpolation_param (p, "hue-interpolation",
                                        gsk_gradient_node_get_hue_interpolation (node),
                                        GSK_HUE_INTERPOLATION_SHORTER);

        end_node (p);
      }
      break;

    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
      {
        if (gsk_render_node_get_node_type (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE)
          start_node (p, "repeating-radial-gradient", node_name);
        else
          start_node (p, "radial-gradient", node_name);

        append_rect_param (p, "bounds", &node->bounds);
        append_point_param (p, "center", gsk_radial_gradient_node_get_center (node));
        append_float_param (p, "hradius", gsk_radial_gradient_node_get_hradius (node), 0.0f);
        append_float_param (p, "vradius", gsk_radial_gradient_node_get_vradius (node), 0.0f);
        append_float_param (p, "start", gsk_radial_gradient_node_get_start (node), 0.0f);
        append_float_param (p, "end", gsk_radial_gradient_node_get_end (node), 1.0f);

        append_stops_param (p, "stops", gsk_gradient_node_get_stops (node),
                                        gsk_gradient_node_get_n_stops (node));

        append_color_state_param (p, "interpolation",
                                  gsk_gradient_node_get_interpolation (node),
                                  GDK_COLOR_STATE_SRGB);
        append_hue_interpolation_param (p, "hue-interpolation",
                                        gsk_gradient_node_get_hue_interpolation (node),
                                        GSK_HUE_INTERPOLATION_SHORTER);

        end_node (p);
      }
      break;

    case GSK_CONIC_GRADIENT_NODE:
      {
        start_node (p, "conic-gradient", node_name);

        append_rect_param (p, "bounds", &node->bounds);
        append_point_param (p, "center", gsk_conic_gradient_node_get_center (node));
        append_float_param (p, "rotation", gsk_conic_gradient_node_get_rotation (node), 0.0f);

        append_stops_param (p, "stops", gsk_gradient_node_get_stops (node),
                                        gsk_gradient_node_get_n_stops (node));

        append_color_state_param (p, "interpolation",
                                  gsk_gradient_node_get_interpolation (node),
                                  GDK_COLOR_STATE_SRGB);
        append_hue_interpolation_param (p, "hue-interpolation",
                                        gsk_gradient_node_get_hue_interpolation (node),
                                        GSK_HUE_INTERPOLATION_SHORTER);

        end_node (p);
      }
      break;

    case GSK_OPACITY_NODE:
      {
        start_node (p, "opacity", node_name);

        append_float_param (p, "opacity", gsk_opacity_node_get_opacity (node), 0.5f);
        append_node_param (p, "child", gsk_opacity_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_OUTSET_SHADOW_NODE:
      {
        start_node (p, "outset-shadow", node_name);

        append_float_param (p, "blur", gsk_outset_shadow_node_get_blur_radius (node), 0.0f);
        if (!gdk_color_equal (gsk_outset_shadow_node_get_gdk_color (node), &GDK_COLOR_SRGB (0, 0, 0, 1)))
          append_color_param (p, "color", gsk_outset_shadow_node_get_gdk_color (node));
        append_float_param (p, "dx", gsk_outset_shadow_node_get_dx (node), 1.0f);
        append_float_param (p, "dy", gsk_outset_shadow_node_get_dy (node), 1.0f);
        append_rounded_rect_param (p, "outline", gsk_outset_shadow_node_get_outline (node));
        append_float_param (p, "spread", gsk_outset_shadow_node_get_spread (node), 0.0f);

        end_node (p);
      }
      break;

    case GSK_CLIP_NODE:
      {
        start_node (p, "clip", node_name);

        append_rect_param (p, "clip", gsk_clip_node_get_clip (node));
        append_node_param (p, "child", gsk_clip_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        start_node (p, "rounded-clip", node_name);

        append_rounded_rect_param (p, "clip", gsk_rounded_clip_node_get_clip (node));
        append_node_param (p, "child", gsk_rounded_clip_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_FILL_NODE:
      {
        start_node (p, "fill", node_name);

        append_node_param (p, "child", gsk_fill_node_get_child (node));
        append_path_param (p, "path", gsk_fill_node_get_path (node));
        append_enum_param (p, "fill-rule", GSK_TYPE_FILL_RULE, gsk_fill_node_get_fill_rule (node));

        end_node (p);
      }
      break;

    case GSK_STROKE_NODE:
      {
        const GskStroke *stroke;
        const float *dash;
        gsize n_dash;

        start_node (p, "stroke", node_name);

        append_node_param (p, "child", gsk_stroke_node_get_child (node));
        append_path_param (p, "path", gsk_stroke_node_get_path (node));

        stroke = gsk_stroke_node_get_stroke (node);
        append_float_param (p, "line-width", gsk_stroke_get_line_width (stroke), 0.0f);
        append_enum_param (p, "line-cap", GSK_TYPE_LINE_CAP, gsk_stroke_get_line_cap (stroke));
        append_enum_param (p, "line-join", GSK_TYPE_LINE_JOIN, gsk_stroke_get_line_join (stroke));
        append_float_param (p, "miter-limit", gsk_stroke_get_miter_limit (stroke), 4.0f);
        dash = gsk_stroke_get_dash (stroke, &n_dash);
        if (dash)
          append_dash_param (p, "dash", dash, n_dash);
        append_float_param (p, "dash-offset", gsk_stroke_get_dash_offset (stroke), 0.0f);

        end_node (p);
      }
      break;

    case GSK_TRANSFORM_NODE:
      {
        GskTransform *transform = gsk_transform_node_get_transform (node);
        start_node (p, "transform", node_name);

        if (gsk_transform_get_category (transform) != GSK_TRANSFORM_CATEGORY_IDENTITY)
          append_transform_param (p, "transform", transform);
        append_node_param (p, "child", gsk_transform_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_COLOR_MATRIX_NODE:
      {
        start_node (p, "color-matrix", node_name);

        if (!graphene_matrix_is_identity (gsk_color_matrix_node_get_color_matrix (node)))
          append_matrix_param (p, "matrix", gsk_color_matrix_node_get_color_matrix (node));
        if (!graphene_vec4_equal (gsk_color_matrix_node_get_color_offset (node), graphene_vec4_zero ()))
          append_vec4_param (p, "offset", gsk_color_matrix_node_get_color_offset (node));
        append_node_param (p, "child", gsk_color_matrix_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_BORDER_NODE:
      {
        const GdkColor *colors = gsk_border_node_get_gdk_colors (node);
        const float *widths = gsk_border_node_get_widths (node);
        guint i, n;
        start_node (p, "border", node_name);

        if (!gdk_color_equal (&colors[3], &colors[1]))
          n = 4;
        else if (!gdk_color_equal (&colors[2], &colors[0]))
          n = 3;
        else if (!gdk_color_equal (&colors[1], &colors[0]))
          n = 2;
        else if (!gdk_color_equal (&colors[0], (&(GdkColor) { .color_state = GDK_COLOR_STATE_SRGB, .values = { 0, 0, 0, 1 } })))
          n = 1;
        else
          n = 0;

        if (n > 0)
          {
            _indent (p);
            g_string_append (p->str, "colors: ");
            for (i = 0; i < n; i++)
              {
                if (i > 0)
                  g_string_append_c (p->str, ' ');
                print_color (p, &colors[i]);
              }
            g_string_append (p->str, ";\n");
          }

        append_rounded_rect_param (p, "outline", gsk_border_node_get_outline (node));

        if (widths[3] != widths[1])
          n = 4;
        else if (widths[2] != widths[0])
          n = 3;
        else if (widths[1] != widths[0])
          n = 2;
        else if (widths[0] != 1.0)
          n = 1;
        else
          n = 0;

        if (n > 0)
          {
            _indent (p);
            g_string_append (p->str, "widths: ");
            for (i = 0; i < n; i++)
              {
                if (i > 0)
                  g_string_append_c (p->str, ' ');
                string_append_double (p->str, widths[i]);
              }
            g_string_append (p->str, ";\n");
          }

        end_node (p);
      }
      break;

    case GSK_SHADOW_NODE:
      {
        const guint n_shadows = gsk_shadow_node_get_n_shadows (node);
        int i;

        start_node (p, "shadow", node_name);

        _indent (p);
        g_string_append (p->str, "shadows: ");
        for (i = 0; i < n_shadows; i ++)
          {
            const GskShadowEntry *s = gsk_shadow_node_get_shadow_entry (node, i);

            if (i > 0)
              g_string_append (p->str, ", ");

            print_color (p, &s->color);
            g_string_append_c (p->str, ' ');
            string_append_double (p->str, s->offset.x);
            g_string_append_c (p->str, ' ');
            string_append_double (p->str, s->offset.y);
            if (s->radius > 0)
              {
                g_string_append_c (p->str, ' ');
                string_append_double (p->str, s->radius);
              }
          }

        g_string_append_c (p->str, ';');
        g_string_append_c (p->str, '\n');
        append_node_param (p, "child", gsk_shadow_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_INSET_SHADOW_NODE:
      {
        start_node (p, "inset-shadow", node_name);

        append_float_param (p, "blur", gsk_inset_shadow_node_get_blur_radius (node), 0.0f);
        if (!gdk_color_equal (gsk_inset_shadow_node_get_gdk_color (node), &GDK_COLOR_SRGB (0, 0, 0, 1)))
          append_color_param (p, "color", gsk_inset_shadow_node_get_gdk_color (node));
        append_float_param (p, "dx", gsk_inset_shadow_node_get_dx (node), 1.0f);
        append_float_param (p, "dy", gsk_inset_shadow_node_get_dy (node), 1.0f);
        append_rounded_rect_param (p, "outline", gsk_inset_shadow_node_get_outline (node));
        append_float_param (p, "spread", gsk_inset_shadow_node_get_spread (node), 0.0f);

        end_node (p);
      }
      break;

    case GSK_TEXTURE_NODE:
      {
        start_node (p, "texture", node_name);

        append_rect_param (p, "bounds", &node->bounds);
        append_texture_param (p, "texture", gsk_texture_node_get_texture (node));

        end_node (p);
      }
      break;

    case GSK_TEXTURE_SCALE_NODE:
      {
        GskScalingFilter filter = gsk_texture_scale_node_get_filter (node);

        start_node (p, "texture-scale", node_name);
        append_rect_param (p, "bounds", &node->bounds);

        if (filter != GSK_SCALING_FILTER_LINEAR)
          {
            _indent (p);
            for (unsigned int i = 0; i < G_N_ELEMENTS (scaling_filters); i++)
              {
                if (scaling_filters[i].filter == filter)
                  {
                    g_string_append_printf (p->str, "filter: %s;\n", scaling_filters[i].name);
                    break;
                  }
              }
          }

        append_texture_param (p, "texture", gsk_texture_scale_node_get_texture (node));
        end_node (p);
      }
      break;

    case GSK_TEXT_NODE:
      {
        const graphene_point_t *offset = gsk_text_node_get_offset (node);
        const GdkColor *color = gsk_text_node_get_gdk_color (node);

        start_node (p, "text", node_name);

        if (!gdk_color_equal (color, &GDK_COLOR_SRGB (0, 0, 0, 1)))
          append_color_param (p, "color", color);

        _indent (p);
        g_string_append (p->str, "font: ");
        gsk_text_node_serialize_font (node, p);
        g_string_append (p->str, ";\n");

        _indent (p);
        g_string_append (p->str, "glyphs: ");
        gsk_text_node_serialize_glyphs (node, p->str);
        g_string_append (p->str, ";\n");

        if (!graphene_point_equal (offset, graphene_point_zero ()))
          append_point_param (p, "offset", offset);

        gsk_text_node_serialize_font_options (node, p);

        end_node (p);
      }
      break;

    case GSK_DEBUG_NODE:
      {
        const char *message = gsk_debug_node_get_message (node);

        start_node (p, "debug", node_name);

        /* TODO: We potentially need to escape certain characters in the message */
        if (message)
          {
            _indent (p);
            g_string_append_printf (p->str, "message: \"%s\";\n", message);
          }
        append_node_param (p, "child", gsk_debug_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_BLUR_NODE:
      {
        start_node (p, "blur", node_name);

        append_float_param (p, "blur", gsk_blur_node_get_radius (node), 1.0f);
        append_node_param (p, "child", gsk_blur_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_GL_SHADER_NODE:
      {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        GskGLShader *shader = gsk_gl_shader_node_get_shader (node);
        GBytes *args = gsk_gl_shader_node_get_args (node);

        start_node (p, "glshader", node_name);

        append_rect_param (p, "bounds", &node->bounds);

        GBytes *bytes = gsk_gl_shader_get_source (shader);
        /* Ensure we are zero-terminated */
        char *sourcecode = g_strndup (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
        append_string_param (p, "sourcecode", sourcecode);
        g_free (sourcecode);

        if (gsk_gl_shader_get_n_uniforms (shader) > 0)
          {
            GString *data = g_string_new ("");

            for (guint i = 0; i < gsk_gl_shader_get_n_uniforms (shader); i++)
              {
                if (i > 0)
                  g_string_append (data, ", ");

                switch (gsk_gl_shader_get_uniform_type (shader, i))
                  {
                  case GSK_GL_UNIFORM_TYPE_NONE:
                  default:
                    g_assert_not_reached ();
                    break;

                  case GSK_GL_UNIFORM_TYPE_FLOAT:
                    {
                      float value = gsk_gl_shader_get_arg_float (shader, args, i);
                      string_append_double (data, value);
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_INT:
                    {
                      gint32 value = gsk_gl_shader_get_arg_int (shader, args, i);
                      g_string_append_printf (data, "%d", value);
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_UINT:
                    {
                      guint32 value = gsk_gl_shader_get_arg_uint (shader, args, i);
                      g_string_append_printf (data, "%u", value);
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_BOOL:
                    {
                      gboolean value = gsk_gl_shader_get_arg_bool (shader, args, i);
                      g_string_append_printf (data, "%d", value);
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_VEC2:
                    {
                      graphene_vec2_t value;
                      gsk_gl_shader_get_arg_vec2 (shader, args, i,
                                                  &value);
                      string_append_double (data, graphene_vec2_get_x (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec2_get_y (&value));
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_VEC3:
                    {
                      graphene_vec3_t value;
                      gsk_gl_shader_get_arg_vec3 (shader, args, i,
                                                  &value);
                      string_append_double (data, graphene_vec3_get_x (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec3_get_y (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec3_get_z (&value));
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_VEC4:
                    {
                      graphene_vec4_t value;
                      gsk_gl_shader_get_arg_vec4 (shader, args, i,
                                                  &value);
                      string_append_double (data, graphene_vec4_get_x (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec4_get_y (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec4_get_z (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec4_get_w (&value));
                    }
                    break;
                  }
              }
            _indent (p);
            g_string_append_printf (p->str, "args: %s;\n", data->str);
            g_string_free (data, TRUE);
          }

        for (guint i = 0; i < gsk_gl_shader_node_get_n_children (node); i ++)
          {
            GskRenderNode *child = gsk_gl_shader_node_get_child (node, i);
            char *name;

            name = g_strdup_printf ("child%d", i + 1);
            append_node_param (p, name, child);
            g_free (name);
          }

        end_node (p);
G_GNUC_END_IGNORE_DEPRECATIONS
      }
      break;

    case GSK_REPEAT_NODE:
      {
        GskRenderNode *child = gsk_repeat_node_get_child (node);
        const graphene_rect_t *child_bounds = gsk_repeat_node_get_child_bounds (node);

        start_node (p, "repeat", node_name);

        if (!graphene_rect_equal (&node->bounds, &child->bounds))
          append_rect_param (p, "bounds", &node->bounds);
        if (!graphene_rect_equal (child_bounds, &child->bounds))
          append_rect_param (p, "child-bounds", child_bounds);
        append_node_param (p, "child", gsk_repeat_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_BLEND_NODE:
      {
        GskBlendMode mode = gsk_blend_node_get_blend_mode (node);

        start_node (p, "blend", node_name);

        if (mode != GSK_BLEND_MODE_DEFAULT)
          {
            _indent (p);
            g_string_append_printf (p->str, "mode: %s;\n", get_blend_mode_name (mode));
          }
        append_node_param (p, "bottom", gsk_blend_node_get_bottom_child (node));
        append_node_param (p, "top", gsk_blend_node_get_top_child (node));

        end_node (p);
      }
      break;

    case GSK_MASK_NODE:
      {
        GskMaskMode mode = gsk_mask_node_get_mask_mode (node);

        start_node (p, "mask", node_name);

        if (mode != GSK_MASK_MODE_ALPHA)
          {
            _indent (p);
            g_string_append_printf (p->str, "mode: %s;\n", get_mask_mode_name (mode));
          }
        append_node_param (p, "source", gsk_mask_node_get_source (node));
        append_node_param (p, "mask", gsk_mask_node_get_mask (node));

        end_node (p);
      }
      break;

    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      break;

    case GSK_CAIRO_NODE:
      {
        cairo_surface_t *surface = gsk_cairo_node_get_surface (node);
        GByteArray *array;
        GBytes *bytes;

        start_node (p, "cairo", node_name);
        append_rect_param (p, "bounds", &node->bounds);

        if (surface != NULL)
          {
            array = g_byte_array_new ();
#if CAIRO_HAS_PNG_FUNCTIONS
            cairo_surface_write_to_png_stream (surface, cairo_write_array, array);
#endif
            bytes = g_byte_array_free_to_bytes (array);

            append_bytes_param (p, "pixels", bytes, "image/png");

            g_bytes_unref (bytes);

#ifdef CAIRO_HAS_SCRIPT_SURFACE
            if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_RECORDING)
              {
                static const cairo_user_data_key_t cairo_is_stupid_key = { 0, };
                cairo_device_t *script;

                array = g_byte_array_new ();
                script = cairo_script_create_for_stream (cairo_write_array, array);

                if (cairo_script_from_recording_surface (script, surface) == CAIRO_STATUS_SUCCESS)
                  {
                    g_byte_array_ref (array); /* Cairo... see below */
                    bytes = g_byte_array_free_to_bytes (array);
                    append_bytes_param (p, "script", bytes, NULL);
                    g_bytes_unref (bytes);
                  }

                /* because Cairo is stupid and writes to the device after we finished it,
                 * we can't just
                g_byte_array_free (array, TRUE);
                 * but have to
                 */
                g_byte_array_set_size (array, 0);
                cairo_device_set_user_data (script, &cairo_is_stupid_key, array, cairo_destroy_array);
                cairo_device_destroy (script);
              }
#endif
          }

        end_node (p);
      }
      break;

    case GSK_SUBSURFACE_NODE:
      {
        start_node (p, "subsurface", node_name);

        append_node_param (p, "child", gsk_subsurface_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_COMPONENT_TRANSFER_NODE:
      {
        start_node (p, "component-transfer", node_name);

        append_node_param (p, "child", gsk_component_transfer_node_get_child (node));
        append_component_transfer_param (p, "red", gsk_component_transfer_node_get_transfer (node, 0));
        append_component_transfer_param (p, "green", gsk_component_transfer_node_get_transfer (node, 1));
        append_component_transfer_param (p, "blue", gsk_component_transfer_node_get_transfer (node, 2));
        append_component_transfer_param (p, "alpha", gsk_component_transfer_node_get_transfer (node, 3));

        end_node (p);
      }
      break;

    default:
      g_error ("Unhandled node: %s", g_type_name_from_instance ((GTypeInstance *) node));
      break;
    }
}

static void
serialize_color_state (GString       *str,
                       GdkColorState *color_state,
                       const char    *name)
{
  const GdkCicp *cicp = gdk_color_state_get_cicp (color_state);

  g_string_append_printf (str, "@cicp \"%s\" {\n", name);
  g_string_append_printf (str, "  primaries: %u;\n", cicp->color_primaries);
  g_string_append_printf (str, "  transfer: %u;\n", cicp->transfer_function);
  g_string_append_printf (str, "  matrix: %u;\n", cicp->matrix_coefficients);
  if (cicp->range != GDK_CICP_RANGE_FULL)
    g_string_append_printf (str, "  range: %s;\n", enum_to_nick (GDK_TYPE_CICP_RANGE, cicp->range));
  g_string_append (str, "}\n");
}

/**
 * gsk_render_node_serialize:
 * @node: a `GskRenderNode`
 *
 * Serializes the @node for later deserialization via
 * gsk_render_node_deserialize(). No guarantees are made about the format
 * used other than that the same version of GTK will be able to deserialize
 * the result of a call to gsk_render_node_serialize() and
 * gsk_render_node_deserialize() will correctly reject files it cannot open
 * that were created with previous versions of GTK.
 *
 * The intended use of this functions is testing, benchmarking and debugging.
 * The format is not meant as a permanent storage format.
 *
 * Returns: a `GBytes` representing the node.
 **/
GBytes *
gsk_render_node_serialize (GskRenderNode *node)
{
  Printer p;
  GHashTableIter iter;
  GdkColorState *cs;
  const char *name;
  GString *str;

  printer_init (&p, node);

  if (gsk_render_node_get_node_type (node) == GSK_CONTAINER_NODE)
    {
      guint i;

      for (i = 0; i < gsk_container_node_get_n_children (node); i ++)
        {
          GskRenderNode *child = gsk_container_node_get_child (node, i);

          render_node_print (&p, child);
        }
    }
  else
    {
      render_node_print (&p, node);
    }

  str = g_string_new (NULL);

  g_hash_table_iter_init (&iter, p.named_color_states);
  while (g_hash_table_iter_next (&iter, (gpointer *)&cs, (gpointer *)&name))
    serialize_color_state (str, cs, name);

  g_string_append_len (str, p.str->str, p.str->len);

  printer_clear (&p);

  return g_string_free_to_bytes (str);
}
