/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkiconhelperprivate.h"

#include <math.h>

#include "gtkcssenumvalueprivate.h"
#include "gtkcssiconthemevalueprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransientnodeprivate.h"
#include "gtkiconthemeprivate.h"
#include "gtkrendericonprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"

void
gtk_icon_helper_invalidate (GtkIconHelper *self)
{
  g_clear_object (&self->texture);

  if (self->rendered_surface != NULL)
    {
      cairo_surface_destroy (self->rendered_surface);
      self->rendered_surface = NULL;
      self->rendered_surface_is_symbolic = FALSE;
    }

  if (!GTK_IS_CSS_TRANSIENT_NODE (self->node))
    gtk_widget_queue_resize (self->owner);
}

void
gtk_icon_helper_invalidate_for_change (GtkIconHelper     *self,
                                       GtkCssStyleChange *change)
{
  if (change == NULL ||
      ((gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SYMBOLIC_ICON) &&
        self->rendered_surface_is_symbolic) ||
       (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON) &&
        !self->rendered_surface_is_symbolic)))
    {
      /* Avoid the queue_resize in gtk_icon_helper_invalidate */
      g_clear_object (&self->texture);

      if (self->rendered_surface != NULL)
        {
          cairo_surface_destroy (self->rendered_surface);
          self->rendered_surface = NULL;
          self->rendered_surface_is_symbolic = FALSE;
        }

      if (change == NULL ||
          (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON_SIZE) &&
          !GTK_IS_CSS_TRANSIENT_NODE (self->node)))
        gtk_widget_queue_resize (self->owner);
    }
}

static void
gtk_icon_helper_take_definition (GtkIconHelper      *self,
                                 GtkImageDefinition *def)
{
  _gtk_icon_helper_clear (self);

  if (def == NULL)
    return;

  gtk_image_definition_unref (self->def);
  self->def = def;

  gtk_icon_helper_invalidate (self);
}

void
_gtk_icon_helper_clear (GtkIconHelper *self)
{
  g_clear_object (&self->texture);
  g_clear_pointer (&self->rendered_surface, cairo_surface_destroy);

  if (gtk_image_definition_get_storage_type (self->def) != GTK_IMAGE_EMPTY)
    {
      gtk_image_definition_unref (self->def);
      self->def = gtk_image_definition_new_empty ();
      gtk_icon_helper_invalidate (self);
    }
  self->icon_size = GTK_ICON_SIZE_INVALID;
}

void
gtk_icon_helper_destroy (GtkIconHelper *self)
{
  _gtk_icon_helper_clear (self);
  g_signal_handlers_disconnect_by_func (self->owner, G_CALLBACK (gtk_icon_helper_invalidate), self);
  gtk_image_definition_unref (self->def);
}

void
gtk_icon_helper_init (GtkIconHelper *self,
                      GtkCssNode    *css_node,
                      GtkWidget     *owner)
{
  memset (self, 0, sizeof (GtkIconHelper));
  self->def = gtk_image_definition_new_empty ();

  self->icon_size = GTK_ICON_SIZE_INVALID;
  self->pixel_size = -1;
  self->rendered_surface_is_symbolic = FALSE;

  self->node = css_node;
  self->owner = owner;
  g_signal_connect_swapped (owner, "direction-changed", G_CALLBACK (gtk_icon_helper_invalidate), self);
  g_signal_connect_swapped (owner, "notify::scale-factor", G_CALLBACK (gtk_icon_helper_invalidate), self);
}

static void
ensure_icon_size (GtkIconHelper *self,
		  gint *width_out,
		  gint *height_out)
{
  gint width, height;

  if (self->pixel_size != -1)
    {
      width = height = self->pixel_size;
    }
  else if (!gtk_icon_size_lookup (self->icon_size, &width, &height))
    {
      if (self->icon_size == GTK_ICON_SIZE_INVALID)
        {
          width = height = 0;
        }
      else
        {
          g_warning ("Invalid icon size %d", self->icon_size);
          width = height = 24;
        }
    }

  *width_out = width;
  *height_out = height;
}

static GtkIconLookupFlags
get_icon_lookup_flags (GtkIconHelper    *self,
                       GtkCssStyle      *style,
                       GtkTextDirection  dir)
{
  GtkIconLookupFlags flags;
  GtkCssIconStyle icon_style;

  flags = GTK_ICON_LOOKUP_USE_BUILTIN;

  if (self->pixel_size != -1 || self->force_scale_pixbuf)
    flags |= GTK_ICON_LOOKUP_FORCE_SIZE;

  icon_style = _gtk_css_icon_style_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_STYLE));

  switch (icon_style)
    {
    case GTK_CSS_ICON_STYLE_REGULAR:
      flags |= GTK_ICON_LOOKUP_FORCE_REGULAR;
      break;
    case GTK_CSS_ICON_STYLE_SYMBOLIC:
      flags |= GTK_ICON_LOOKUP_FORCE_SYMBOLIC;
      break;
    case GTK_CSS_ICON_STYLE_REQUESTED:
      break;
    default:
      g_assert_not_reached ();
      return 0;
    }

  if (dir == GTK_TEXT_DIR_LTR)
    flags |= GTK_ICON_LOOKUP_DIR_LTR;
  else if (dir == GTK_TEXT_DIR_RTL)
    flags |= GTK_ICON_LOOKUP_DIR_RTL;

  return flags;
}

static void
get_surface_size (GtkIconHelper   *self,
		  cairo_surface_t *surface,
		  int *width,
		  int *height)
{
  GdkRectangle clip;
  cairo_t *cr;

  cr = cairo_create (surface);
  if (gdk_cairo_get_clip_rectangle (cr, &clip))
    {
      if (clip.x != 0 || clip.y != 0)
        {
          g_warning ("origin of surface is %d %d, not supported", clip.x, clip.y);
        }
      *width = clip.width;
      *height = clip.height;
    }
  else
    {
      g_warning ("infinite surface size not supported");
      ensure_icon_size (self, width, height);
    }

  cairo_destroy (cr);
}

static cairo_surface_t *
ensure_surface_from_surface (GtkIconHelper   *self,
                             cairo_surface_t *orig_surface)
{
  return cairo_surface_reference (orig_surface);
}

static gboolean
get_pixbuf_size (GtkIconHelper   *self,
                 gint             scale,
                 GdkPixbuf       *orig_pixbuf,
                 gint             orig_scale,
                 gint *width_out,
                 gint *height_out,
                 gint *scale_out)
{
  gboolean scale_pixmap;
  gint width, height;

  scale_pixmap = FALSE;

  if (self->force_scale_pixbuf &&
      (self->pixel_size != -1 ||
       self->icon_size != GTK_ICON_SIZE_INVALID))
    {
      ensure_icon_size (self, &width, &height);

      if (scale != orig_scale ||
	  width < gdk_pixbuf_get_width (orig_pixbuf) / orig_scale ||
          height < gdk_pixbuf_get_height (orig_pixbuf) / orig_scale)
	{
	  width = MIN (width * scale, gdk_pixbuf_get_width (orig_pixbuf) * scale / orig_scale);
	  height = MIN (height * scale, gdk_pixbuf_get_height (orig_pixbuf) * scale / orig_scale);

          scale_pixmap = TRUE;
	}
      else
	{
	  width = gdk_pixbuf_get_width (orig_pixbuf);
	  height = gdk_pixbuf_get_height (orig_pixbuf);
	  scale = orig_scale;
	}
    }
  else
    {
      width = gdk_pixbuf_get_width (orig_pixbuf);
      height = gdk_pixbuf_get_height (orig_pixbuf);
      scale = orig_scale;
    }

  *width_out = width;
  *height_out = height;
  *scale_out = scale;

  return scale_pixmap;
}

static cairo_surface_t *
ensure_surface_from_pixbuf (GtkIconHelper *self,
                            GtkCssStyle   *style,
                            gint           scale,
                            GdkPixbuf     *orig_pixbuf,
                            gint           orig_scale)
{
  gint width, height;
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf;

  if (get_pixbuf_size (self,
                       scale,
                       orig_pixbuf,
                       orig_scale,
                       &width, &height, &scale))
    pixbuf = gdk_pixbuf_scale_simple (orig_pixbuf,
                                      width, height,
                                      GDK_INTERP_BILINEAR);
  else
    pixbuf = g_object_ref (orig_pixbuf);

  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, _gtk_widget_get_window (self->owner));
  g_object_unref (pixbuf);

  return surface;
}

static cairo_surface_t *
ensure_surface_for_gicon (GtkIconHelper    *self,
                          GtkCssStyle      *style,
                          GtkTextDirection  dir,
                          gint              scale,
                          GIcon            *gicon)
{
  GtkIconTheme *icon_theme;
  gint width, height;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;
  cairo_surface_t *surface;
  GdkPixbuf *destination;
  gboolean symbolic;

  icon_theme = gtk_css_icon_theme_value_get_icon_theme
    (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_THEME));
  flags = get_icon_lookup_flags (self, style, dir);

  ensure_icon_size (self, &width, &height);

  info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme,
                                                   gicon,
                                                   MIN (width, height),
                                                   scale, flags);
  if (info)
    {
      symbolic = gtk_icon_info_is_symbolic (info);

      if (symbolic)
        {
          GdkRGBA fg, success_color, warning_color, error_color;

          gtk_icon_theme_lookup_symbolic_colors (style, &fg, &success_color, &warning_color, &error_color);

          destination = gtk_icon_info_load_symbolic (info,
                                                     &fg, &success_color,
                                                     &warning_color, &error_color,
                                                     NULL,
                                                     NULL);
        }
      else
        {
          destination = gtk_icon_info_load_icon (info, NULL);
        }

      g_object_unref (info);
    }
  else
    {
      destination = NULL;
    }

  if (destination == NULL)
    {
      GError *error = NULL;
      destination = gtk_icon_theme_load_icon (icon_theme,
                                              "image-missing",
                                              width,
                                              flags | GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                              &error);
      /* We include this image as resource, so we always have it available or
       * the icontheme code is broken */
      g_assert_no_error (error);
      g_assert (destination);
      symbolic = FALSE;
    }

  surface = gdk_cairo_surface_create_from_pixbuf (destination, scale, _gtk_widget_get_window (self->owner));

  if (symbolic)
    {
      self->rendered_surface_is_symbolic = TRUE;
    }

  g_object_unref (destination);

  return surface;
}

static cairo_surface_t *
gtk_icon_helper_load_surface (GtkIconHelper   *self,
                              int              scale)
{
  cairo_surface_t *surface;
  GIcon *gicon;

  switch (gtk_image_definition_get_storage_type (self->def))
    {
    case GTK_IMAGE_SURFACE:
      surface = ensure_surface_from_surface (self, gtk_image_definition_get_surface (self->def));
      break;

    case GTK_IMAGE_PIXBUF:
      surface = ensure_surface_from_pixbuf (self,
                                            gtk_css_node_get_style (self->node),
                                            scale,
                                            gtk_image_definition_get_pixbuf (self->def),
                                            gtk_image_definition_get_scale (self->def));
      break;

    case GTK_IMAGE_ICON_NAME:
      if (self->use_fallback)
        gicon = g_themed_icon_new_with_default_fallbacks (gtk_image_definition_get_icon_name (self->def));
      else
        gicon = g_themed_icon_new (gtk_image_definition_get_icon_name (self->def));
      surface = ensure_surface_for_gicon (self,
                                          gtk_css_node_get_style (self->node),
                                          gtk_widget_get_direction (self->owner),
                                          scale, 
                                          gicon);
      g_object_unref (gicon);
      break;

    case GTK_IMAGE_GICON:
      surface = ensure_surface_for_gicon (self, 
                                          gtk_css_node_get_style (self->node),
                                          gtk_widget_get_direction (self->owner),
                                          scale,
                                          gtk_image_definition_get_gicon (self->def));
      break;

    case GTK_IMAGE_ANIMATION:
    case GTK_IMAGE_EMPTY:
    default:
      surface = NULL;
      break;
    }

  return surface;

}

static void
gtk_icon_helper_ensure_surface (GtkIconHelper *self)
{
  int scale;

  if (self->rendered_surface)
    return;

  scale = gtk_widget_get_scale_factor (self->owner);

  self->rendered_surface = gtk_icon_helper_load_surface (self, scale);
}

static GskTexture *
find_cached_texture (GtkIconHelper *self)
{
  GtkIconTheme *icon_theme;
  int width, height;
  GtkIconLookupFlags flags;
  GtkCssStyle *style;
  GtkTextDirection dir;
  int scale;
  GIcon *gicon;
  GtkIconInfo *info;
  GskTexture *texture;

  style = gtk_css_node_get_style (self->node);
  dir = gtk_widget_get_direction (self->owner);
  scale = gtk_widget_get_scale_factor (self->owner);

  icon_theme = gtk_css_icon_theme_value_get_icon_theme (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_THEME));
  flags = get_icon_lookup_flags (self, style, dir);
  ensure_icon_size (self, &width, &height);

  switch (gtk_image_definition_get_storage_type (self->def))
    {
    case GTK_IMAGE_GICON:
      gicon = g_object_ref (gtk_image_definition_get_gicon (self->def));
      break;
    case GTK_IMAGE_ICON_NAME:
      if (self->use_fallback)
        gicon = g_themed_icon_new_with_default_fallbacks (gtk_image_definition_get_icon_name (self->def));
      else
        gicon = g_themed_icon_new (gtk_image_definition_get_icon_name (self->def));
      break;
    case GTK_IMAGE_EMPTY:
    case GTK_IMAGE_PIXBUF:
    case GTK_IMAGE_ANIMATION:
    case GTK_IMAGE_SURFACE:
    default:
      return NULL;
    }

  info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme, gicon, MIN (width, height), scale, flags);
  g_object_unref (gicon);

  if (!info)
    return NULL;

  if (gtk_icon_info_is_symbolic (info))
      self->rendered_surface_is_symbolic = TRUE;

  texture = gtk_icon_info_load_texture (info);

  g_object_unref (info);

  return texture;
}

static void
gtk_icon_helper_ensure_texture (GtkIconHelper *self)
{
  cairo_surface_t *map;
  int width, height, scale;

  if (self->texture)
    return;

  self->texture = find_cached_texture (self);
  if (self->texture)
    return;

  gtk_icon_helper_ensure_surface (self);
  if (self->rendered_surface == NULL)
    return;

  scale = gtk_widget_get_scale_factor (self->owner);
  _gtk_icon_helper_get_size (self, &width, &height);

  if (cairo_image_surface_get_format (self->rendered_surface) != CAIRO_FORMAT_ARGB32)
    {
      cairo_surface_t *argb_surface = cairo_surface_create_similar_image (self->rendered_surface,
                                                                          CAIRO_FORMAT_ARGB32,
                                                                          width, height);
      cairo_t *ct;
      cairo_surface_set_device_scale (argb_surface, scale, scale);

      ct = cairo_create (argb_surface);
      cairo_set_source_surface (ct, self->rendered_surface, 0, 0);
      cairo_paint (ct);
      cairo_destroy (ct);
      cairo_surface_destroy (self->rendered_surface);
      self->rendered_surface = argb_surface;
    }

  map = cairo_surface_map_to_image (self->rendered_surface,
                                    &(GdkRectangle) { 0, 0, width * scale, height * scale});

  self->texture = gsk_texture_new_for_data (cairo_image_surface_get_data (map),
                                                  width * scale,
                                                  height * scale,
                                                  cairo_image_surface_get_stride (map));

  cairo_surface_unmap_image (self->rendered_surface, map);
}

void
_gtk_icon_helper_get_size (GtkIconHelper *self,
                           gint *width_out,
                           gint *height_out)
{
  gint width, height, scale;

  width = height = 0;

  /* Certain kinds of images are easy to calculate the size for,</cosimoc>3 these
     we do immediately to avoid having to potentially load the image
     data for something that may not yet be visible */
  switch (gtk_image_definition_get_storage_type (self->def))
    {
    case GTK_IMAGE_SURFACE:
      get_surface_size (self,
                        gtk_image_definition_get_surface (self->def),
                        &width,
                        &height);
      break;

    case GTK_IMAGE_PIXBUF:
      get_pixbuf_size (self,
                       gtk_widget_get_scale_factor (self->owner),
                       gtk_image_definition_get_pixbuf (self->def),
                       gtk_image_definition_get_scale (self->def),
                       &width, &height, &scale);
      width = (width + scale - 1) / scale;
      height = (height + scale - 1) / scale;
      break;

    case GTK_IMAGE_ANIMATION:
      {
        GdkPixbufAnimation *animation = gtk_image_definition_get_animation (self->def);
        width = gdk_pixbuf_animation_get_width (animation);
        height = gdk_pixbuf_animation_get_height (animation);
        break;
      }

    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      if (self->pixel_size != -1 || self->force_scale_pixbuf)
        ensure_icon_size (self, &width, &height);

      break;

    case GTK_IMAGE_EMPTY:
    default:
      break;
    }

  /* Otherwise we load the surface to guarantee we get a size */
  if (width == 0)
    {
      gtk_icon_helper_ensure_surface (self);

      if (self->rendered_surface != NULL)
        {
          get_surface_size (self, self->rendered_surface, &width, &height);
        }
      else if (self->icon_size != GTK_ICON_SIZE_INVALID)
        {
          ensure_icon_size (self, &width, &height);
        }
    }

  if (width_out)
    *width_out = width;
  if (height_out)
    *height_out = height;
}

void
_gtk_icon_helper_set_definition (GtkIconHelper *self,
                                 GtkImageDefinition *def)
{
  if (def)
    gtk_icon_helper_take_definition (self, gtk_image_definition_ref (def));
  else
    _gtk_icon_helper_clear (self);
}

void 
_gtk_icon_helper_set_gicon (GtkIconHelper *self,
                            GIcon *gicon,
                            GtkIconSize icon_size)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_gicon (gicon));
  _gtk_icon_helper_set_icon_size (self, icon_size);
}

void 
_gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                const gchar *icon_name,
                                GtkIconSize icon_size)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_icon_name (icon_name));
  _gtk_icon_helper_set_icon_size (self, icon_size);
}

void
_gtk_icon_helper_set_pixbuf (GtkIconHelper *self,
                             GdkPixbuf *pixbuf)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_pixbuf (pixbuf, 1));
}

void
_gtk_icon_helper_set_animation (GtkIconHelper *self,
                                GdkPixbufAnimation *animation)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_animation (animation, 1));
}

void
_gtk_icon_helper_set_surface (GtkIconHelper *self,
			      cairo_surface_t *surface)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_surface (surface));
}

gboolean
_gtk_icon_helper_set_icon_size (GtkIconHelper *self,
                                GtkIconSize    icon_size)
{
  if (self->icon_size != icon_size)
    {
      self->icon_size = icon_size;
      gtk_icon_helper_invalidate (self);
      return TRUE;
    }
  return FALSE;
}

gboolean
_gtk_icon_helper_set_pixel_size (GtkIconHelper *self,
                                 gint           pixel_size)
{
  if (self->pixel_size != pixel_size)
    {
      self->pixel_size = pixel_size;
      gtk_icon_helper_invalidate (self);
      return TRUE;
    }
  return FALSE;
}

gboolean
_gtk_icon_helper_set_use_fallback (GtkIconHelper *self,
                                   gboolean       use_fallback)
{
  if (self->use_fallback != use_fallback)
    {
      self->use_fallback = use_fallback;
      gtk_icon_helper_invalidate (self);
      return TRUE;
    }
  return FALSE;
}

GtkImageType
_gtk_icon_helper_get_storage_type (GtkIconHelper *self)
{
  return gtk_image_definition_get_storage_type (self->def);
}

gboolean
_gtk_icon_helper_get_use_fallback (GtkIconHelper *self)
{
  return self->use_fallback;
}

GtkIconSize
_gtk_icon_helper_get_icon_size (GtkIconHelper *self)
{
  return self->icon_size;
}

gint
_gtk_icon_helper_get_pixel_size (GtkIconHelper *self)
{
  return self->pixel_size;
}

GtkImageDefinition *
gtk_icon_helper_get_definition (GtkIconHelper *self)
{
  return self->def;
}

GdkPixbuf *
_gtk_icon_helper_peek_pixbuf (GtkIconHelper *self)
{
  return gtk_image_definition_get_pixbuf (self->def);
}

GIcon *
_gtk_icon_helper_peek_gicon (GtkIconHelper *self)
{
  return gtk_image_definition_get_gicon (self->def);
}

GdkPixbufAnimation *
_gtk_icon_helper_peek_animation (GtkIconHelper *self)
{
  return gtk_image_definition_get_animation (self->def);
}

cairo_surface_t *
_gtk_icon_helper_peek_surface (GtkIconHelper *self)
{
  return gtk_image_definition_get_surface (self->def);
}

const gchar *
_gtk_icon_helper_get_icon_name (GtkIconHelper *self)
{
  return gtk_image_definition_get_icon_name (self->def);
}

void
gtk_icon_helper_snapshot (GtkIconHelper *self,
                          GtkSnapshot   *snapshot)
{
  GtkCssStyle *style;
  GskTexture *texture;

  style = gtk_css_node_get_style (self->node);

  gtk_icon_helper_ensure_texture (self);
  texture = self->texture;
  if (texture == NULL)
    return;

  if (self->rendered_surface_is_symbolic)
    {
      GdkRGBA fg, sc, wc, ec;
      graphene_matrix_t matrix;
      graphene_vec4_t offset, r0, r1, r2, r3;

      gtk_icon_theme_lookup_symbolic_colors (style, &fg, &sc, &wc, &ec);

      graphene_vec4_init (&r0, sc.red   - fg.red,   wc.red   - fg.red,   ec.red   - fg.red,   0);
      graphene_vec4_init (&r1, sc.green - fg.green, wc.green - fg.green, ec.green - fg.green, 0);
      graphene_vec4_init (&r2, sc.blue  - fg.blue,  wc.blue  - fg.blue,  ec.blue  - fg.blue,  0);
      graphene_vec4_init (&r3, 0, 0, 0, fg.alpha);
      graphene_vec4_init (&offset, fg.red, fg.green, fg.blue, 0);
      graphene_matrix_init_from_vec4 (&matrix, &r0, &r1, &r2, &r3);

      gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset, "Symbolic Icon");
    }

  gtk_css_style_snapshot_icon_texture (style,
                                       snapshot,
                                       texture,
                                       gtk_widget_get_scale_factor (self->owner));

  if (self->rendered_surface_is_symbolic)
    {
      gtk_snapshot_pop (snapshot);
    }
}

gboolean
_gtk_icon_helper_get_is_empty (GtkIconHelper *self)
{
  return gtk_image_definition_get_storage_type (self->def) == GTK_IMAGE_EMPTY;
}

gboolean
_gtk_icon_helper_get_force_scale_pixbuf (GtkIconHelper *self)
{
  return self->force_scale_pixbuf;
}

void
_gtk_icon_helper_set_force_scale_pixbuf (GtkIconHelper *self,
                                         gboolean       force_scale)
{
  if (self->force_scale_pixbuf != force_scale)
    {
      self->force_scale_pixbuf = force_scale;
      gtk_icon_helper_invalidate (self);
    }
}

void 
_gtk_icon_helper_set_pixbuf_scale (GtkIconHelper *self,
				   int scale)
{
  switch (gtk_image_definition_get_storage_type (self->def))
  {
    case GTK_IMAGE_PIXBUF:
      gtk_icon_helper_take_definition (self,
                                      gtk_image_definition_new_pixbuf (gtk_image_definition_get_pixbuf (self->def),
                                                                       scale));
      break;

    case GTK_IMAGE_ANIMATION:
      gtk_icon_helper_take_definition (self,
                                      gtk_image_definition_new_animation (gtk_image_definition_get_animation (self->def),
                                                                          scale));
      break;

    case GTK_IMAGE_EMPTY:
    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
    case GTK_IMAGE_SURFACE:
    default:
      break;
  }
}
