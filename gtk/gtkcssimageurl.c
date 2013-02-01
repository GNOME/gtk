/*
 * Copyright © 2011 Red Hat Inc.
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
 */

#include "config.h"

#include <string.h>

#include "gtkcssimageurlprivate.h"
#include "gtkcssimagesurfaceprivate.h"

G_DEFINE_TYPE (GtkCssImageUrl, _gtk_css_image_url, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_url_get_width (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return _gtk_css_image_get_width (url->loaded_image);
}

static int
gtk_css_image_url_get_height (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return _gtk_css_image_get_height (url->loaded_image);
}

static double
gtk_css_image_url_get_aspect_ratio (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return _gtk_css_image_get_aspect_ratio (url->loaded_image);
}

static void
gtk_css_image_url_draw (GtkCssImage        *image,
                        cairo_t            *cr,
                        double              width,
                        double              height)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  _gtk_css_image_draw (url->loaded_image, cr, width, height);
}

static GtkCssImage *
gtk_css_image_url_compute (GtkCssImage             *image,
                           guint                    property_id,
                           GtkStyleProviderPrivate *provider,
                           GtkCssComputedValues    *values,
                           GtkCssComputedValues    *parent_values,
                           GtkCssDependencies      *dependencies)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return g_object_ref (url->loaded_image);
}

static gboolean
gtk_css_image_url_parse (GtkCssImage  *image,
                         GtkCssParser *parser)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);
  GdkPixbuf *pixbuf;
  GError *error = NULL;
  GFileInputStream *input;

  url->file = _gtk_css_parser_read_url (parser);
  if (url->file == NULL)
    return FALSE;

  /* We special case resources here so we can use
     gdk_pixbuf_new_from_resource, which in turn has some special casing
     for GdkPixdata files to avoid duplicating the memory for the pixbufs */
  if (g_file_has_uri_scheme (url->file, "resource"))
    {
      char *uri = g_file_get_uri (url->file);
      char *resource_path = g_uri_unescape_string (uri + strlen ("resource://"), NULL);

      pixbuf = gdk_pixbuf_new_from_resource (resource_path, &error);
      g_free (resource_path);
      g_free (uri);
    }
  else
    {
      input = g_file_read (url->file, NULL, &error);
      if (input == NULL)
	{
	  _gtk_css_parser_take_error (parser, error);
	  return FALSE;
	}

      pixbuf = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (input), NULL, &error);
      g_object_unref (input);
    }

  if (pixbuf == NULL)
    {
      _gtk_css_parser_take_error (parser, error);
      return FALSE;
    }

  url->loaded_image = _gtk_css_image_surface_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return TRUE;
}

static void
gtk_css_image_url_print (GtkCssImage *image,
                         GString     *string)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  _gtk_css_image_print (url->loaded_image, string);
}

static void
gtk_css_image_url_dispose (GObject *object)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (object);

  g_clear_object (&url->file);
  g_clear_object (&url->loaded_image);

  G_OBJECT_CLASS (_gtk_css_image_url_parent_class)->dispose (object);
}

static void
_gtk_css_image_url_class_init (GtkCssImageUrlClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_url_get_width;
  image_class->get_height = gtk_css_image_url_get_height;
  image_class->get_aspect_ratio = gtk_css_image_url_get_aspect_ratio;
  image_class->compute = gtk_css_image_url_compute;
  image_class->draw = gtk_css_image_url_draw;
  image_class->parse = gtk_css_image_url_parse;
  image_class->print = gtk_css_image_url_print;

  object_class->dispose = gtk_css_image_url_dispose;
}

static void
_gtk_css_image_url_init (GtkCssImageUrl *image_url)
{
}

