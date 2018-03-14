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
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransientnodeprivate.h"
#include "gtkiconthemeprivate.h"
#include "gtkrendericonprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"

struct _GtkIconHelper
{
  GObject parent_instance;

  GtkImageDefinition *def;

  gint pixel_size;

  guint use_fallback : 1;
  guint force_scale_pixbuf : 1;
  guint texture_is_symbolic : 1;

  GtkWidget *owner;
  GtkCssNode *node;
  GdkPaintable *paintable;
  int texture_scale;
};

G_DEFINE_TYPE (GtkIconHelper, gtk_icon_helper, G_TYPE_OBJECT)

void
gtk_icon_helper_invalidate (GtkIconHelper *self)
{
  g_clear_object (&self->paintable);
  self->texture_scale = 1;
  self->texture_is_symbolic = FALSE;

  if (!GTK_IS_CSS_TRANSIENT_NODE (self->node))
    gtk_widget_queue_resize (self->owner);
}

void
gtk_icon_helper_invalidate_for_change (GtkIconHelper     *self,
                                       GtkCssStyleChange *change)
{
  if (change == NULL ||
      ((gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SYMBOLIC_ICON) &&
        self->texture_is_symbolic) ||
       (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON) &&
        !self->texture_is_symbolic)))
    {
      /* Avoid the queue_resize in gtk_icon_helper_invalidate */
      g_clear_object (&self->paintable);
      self->texture_scale = 1;
      self->texture_is_symbolic = FALSE;

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
  g_clear_object (&self->paintable);
  self->texture_scale = 1;
  self->texture_is_symbolic = FALSE;

  if (gtk_image_definition_get_storage_type (self->def) != GTK_IMAGE_EMPTY)
    {
      gtk_image_definition_unref (self->def);
      self->def = gtk_image_definition_new_empty ();
      gtk_icon_helper_invalidate (self);
    }
}

static void
gtk_icon_helper_finalize (GObject *object)
{
  GtkIconHelper *self = GTK_ICON_HELPER (object);

  _gtk_icon_helper_clear (self);
  g_signal_handlers_disconnect_by_func (self->owner, G_CALLBACK (gtk_icon_helper_invalidate), self);
  gtk_image_definition_unref (self->def);

  G_OBJECT_CLASS (gtk_icon_helper_parent_class)->finalize (object);
}

void
gtk_icon_helper_class_init (GtkIconHelperClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_icon_helper_finalize;
}

void
gtk_icon_helper_init (GtkIconHelper *self)
{
  self->def = gtk_image_definition_new_empty ();
}

GtkIconHelper *
gtk_icon_helper_new (GtkCssNode *css_node,
                     GtkWidget  *owner)
{
  GtkIconHelper *self;
  
  self = g_object_new (GTK_TYPE_ICON_HELPER, NULL);

  self->pixel_size = -1;
  self->texture_is_symbolic = FALSE;

  self->node = css_node;
  self->owner = owner;
  g_signal_connect_swapped (owner, "direction-changed", G_CALLBACK (gtk_icon_helper_invalidate), self);
  g_signal_connect_swapped (owner, "notify::scale-factor", G_CALLBACK (gtk_icon_helper_invalidate), self);

  return self;
}

static int
get_default_size (GtkIconHelper *self)
{
  GtkCssStyle *style;

  if (self->pixel_size != -1)
    return self->pixel_size;

  style = gtk_css_node_get_style (self->node);
  return _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SIZE), 100);
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
get_surface_size (cairo_surface_t *surface,
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
      *width = 0;
      *height = 0;
    }

  cairo_destroy (cr);
}

static GdkPaintable *
ensure_paintable_from_surface (GtkIconHelper   *self,
                               cairo_surface_t *orig_surface,
                               int             *scale_out)
{
  cairo_surface_t *map;
  int width, height, scale;
  GdkTexture *texture;

  scale = gtk_widget_get_scale_factor (self->owner);
  *scale_out = scale;
  _gtk_icon_helper_get_size (self, &width, &height);

  map = cairo_surface_map_to_image (orig_surface,
                                    &(GdkRectangle) { 0, 0, width * scale, height * scale});

  if (cairo_image_surface_get_format (map) == CAIRO_FORMAT_ARGB32)
    {
      texture = gdk_texture_new_for_data (cairo_image_surface_get_data (map),
                                          width * scale,
                                          height * scale,
                                          cairo_image_surface_get_stride (map));
    }
  else
    {
      cairo_surface_t *argb_surface;
      cairo_t *cr;

      argb_surface = cairo_surface_create_similar_image (orig_surface,
                                                         CAIRO_FORMAT_ARGB32,
                                                         width * scale, height * scale);

      cr = cairo_create (argb_surface);
      cairo_set_source_surface (cr, map, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      texture = gdk_texture_new_for_data (cairo_image_surface_get_data (argb_surface),
                                          width * scale,
                                          height * scale,
                                          cairo_image_surface_get_stride (argb_surface));
      cairo_surface_destroy (argb_surface);
    }

  cairo_surface_unmap_image (orig_surface, map);

  return GDK_PAINTABLE (texture);
}

static GdkPaintable *
ensure_paintable_from_texture (GtkIconHelper *self,
                               GdkTexture    *texture,
                               int           *scale)
{
  *scale = gtk_image_definition_get_storage_type (self->def);

  return g_object_ref (GDK_PAINTABLE (texture));
}

static GdkPaintable *
ensure_paintable_from_paintable (GtkIconHelper *self,
                                 GdkPaintable  *paintable,
                                 int           *scale)
{
  *scale = 1;

  return g_object_ref (paintable);
}

static GdkPaintable *
ensure_paintable_for_gicon (GtkIconHelper    *self,
                            GtkCssStyle      *style,
                            GtkTextDirection  dir,
                            gint              scale,
                            GIcon            *gicon,
                            gboolean         *symbolic)
{
  GtkIconTheme *icon_theme;
  gint width, height;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;
  GdkTexture *texture;

  icon_theme = gtk_css_icon_theme_value_get_icon_theme
    (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_THEME));
  flags = get_icon_lookup_flags (self, style, dir);

  width = height = get_default_size (self);

  info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme,
                                                   gicon,
                                                   MIN (width, height),
                                                   scale, flags);
  if (info == NULL)
    info = gtk_icon_theme_lookup_icon (icon_theme,
                                       "image-missing",
                                       width,
                                       flags | GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_GENERIC_FALLBACK);

  *symbolic = gtk_icon_info_is_symbolic (info);
  texture = gtk_icon_info_load_texture (info);

  return GDK_PAINTABLE (texture);
}

static GdkPaintable *
gtk_icon_helper_load_paintable (GtkIconHelper   *self,
                                int             *out_scale,
                                gboolean        *out_symbolic)
{
  GdkPaintable *paintable;
  GIcon *gicon;
  int scale;
  gboolean symbolic;

  switch (gtk_image_definition_get_storage_type (self->def))
    {
    case GTK_IMAGE_SURFACE:
      paintable = ensure_paintable_from_surface (self, gtk_image_definition_get_surface (self->def), &scale);
      symbolic = FALSE;
      break;

    case GTK_IMAGE_TEXTURE:
      paintable = ensure_paintable_from_texture (self, gtk_image_definition_get_texture (self->def), &scale);
      symbolic = FALSE;
      break;

    case GTK_IMAGE_PAINTABLE:
      paintable = ensure_paintable_from_paintable (self, gtk_image_definition_get_paintable (self->def), &scale);
      symbolic = FALSE;
      break;

    case GTK_IMAGE_ICON_NAME:
      scale = gtk_widget_get_scale_factor (self->owner);
      if (self->use_fallback)
        gicon = g_themed_icon_new_with_default_fallbacks (gtk_image_definition_get_icon_name (self->def));
      else
        gicon = g_themed_icon_new (gtk_image_definition_get_icon_name (self->def));
      paintable = ensure_paintable_for_gicon (self,
                                              gtk_css_node_get_style (self->node),
                                              gtk_widget_get_direction (self->owner),
                                              scale,
                                              gicon,
                                              &symbolic);
      g_object_unref (gicon);
      break;

    case GTK_IMAGE_GICON:
      scale = gtk_widget_get_scale_factor (self->owner);
      paintable = ensure_paintable_for_gicon (self, 
                                              gtk_css_node_get_style (self->node),
                                              gtk_widget_get_direction (self->owner),
                                              scale,
                                              gtk_image_definition_get_gicon (self->def),
                                              &symbolic);
      break;

    case GTK_IMAGE_EMPTY:
    default:
      paintable = NULL;
      scale = 1;
      symbolic = FALSE;
      break;
    }

  *out_scale = scale;
  *out_symbolic = symbolic;

  return paintable;
}

static void
gtk_icon_helper_ensure_paintable (GtkIconHelper *self)
{
  gboolean symbolic;

  if (self->paintable)
    return;

  self->paintable = gtk_icon_helper_load_paintable (self,
                                                    &self->texture_scale,
                                                    &symbolic);
  self->texture_is_symbolic = symbolic;
}

void
gtk_icon_helper_measure (GtkIconHelper *self,
                         GtkOrientation orientation,
                         int            for_size,
                         int           *minimum,
                         int           *natural)
{
  switch (gtk_image_definition_get_storage_type (self->def))
    {
    case GTK_IMAGE_PAINTABLE:
      {
        double min_width, min_height, nat_width, nat_height;
        int default_size = get_default_size (self);

        gdk_paintable_compute_concrete_size (gtk_image_definition_get_paintable (self->def),
                                             0, 0,
                                             default_size, default_size,
                                             &min_width, &min_height);

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            gdk_paintable_compute_concrete_size (gtk_image_definition_get_paintable (self->def),
                                                 0,
                                                 for_size < 0 ? 0 : for_size,
                                                 default_size, default_size,
                                                 &nat_width, &nat_height);
            *minimum = ceil (min_width);
            *natural = ceil (nat_width);
          }
        else
          {
            gdk_paintable_compute_concrete_size (gtk_image_definition_get_paintable (self->def),
                                                 for_size < 0 ? 0 : for_size,
                                                 0,
                                                 default_size, default_size,
                                                 &nat_width, &nat_height);
            *minimum = ceil (min_height);
            *natural = ceil (nat_height);
          }
      }
      break;

    case GTK_IMAGE_TEXTURE:
    case GTK_IMAGE_SURFACE:
    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
    case GTK_IMAGE_EMPTY:
    default:
      {
        int width, height;

        _gtk_icon_helper_get_size (self, &width, &height);
        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          *minimum = *natural = width;
        else
          *minimum = *natural = height;
      }
      break;
    }
}

static void
get_size_for_paintable (GtkIconHelper *self,
                        GdkPaintable  *paintable,
                        int           *width_out,
                        int           *height_out)
{
  int width = gdk_paintable_get_intrinsic_width (paintable);
  int height = gdk_paintable_get_intrinsic_height (paintable);

  if (width == 0)
    {
      if (height != 0)
        {
          double ar = gdk_paintable_get_intrinsic_aspect_ratio (paintable);

          if (ar > 0)
            width = ceil (height * ar);
        }
    }
  else
    {
      if (height == 0)
        {
          double ar = gdk_paintable_get_intrinsic_aspect_ratio (paintable);

          if (ar > 0)
            height = ceil (width / ar);
        }
    }
  
  if (width == 0 || height == 0)
    width = height = get_default_size (self);

  *width_out = width;
  *height_out = height;
}

void
_gtk_icon_helper_get_size (GtkIconHelper *self,
                           gint *width_out,
                           gint *height_out)
{
  gint width, height;

  width = height = 0;

  /* Certain kinds of images are easy to calculate the size for,</cosimoc>3 these
     we do immediately to avoid having to potentially load the image
     data for something that may not yet be visible */
  switch (gtk_image_definition_get_storage_type (self->def))
    {
    case GTK_IMAGE_SURFACE:
      get_surface_size (gtk_image_definition_get_surface (self->def),
                        &width,
                        &height);
      break;

    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      if (self->pixel_size != -1 || self->force_scale_pixbuf)
        width = height = get_default_size (self);
      break;

    case GTK_IMAGE_TEXTURE:
      {
        GdkTexture *texture = gtk_image_definition_get_texture (self->def);
        width = gdk_texture_get_width (texture);
        height = gdk_texture_get_height (texture);
      }
      break;

    case GTK_IMAGE_PAINTABLE:
      {
        GdkPaintable *paintable = gtk_image_definition_get_paintable (self->def);

        get_size_for_paintable (self, paintable, &width, &height);
      }
      break;

    case GTK_IMAGE_EMPTY:
    default:
      break;
    }

  /* Otherwise we load the surface to guarantee we get a size */
  if (width == 0)
    {
      gtk_icon_helper_ensure_paintable (self);

      if (self->paintable != NULL)
        {
          get_size_for_paintable (self, self->paintable, &width, &height);
        }
      else
        {
          width = height = get_default_size (self);
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
                            GIcon         *gicon)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_gicon (gicon));
}

void
_gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                const gchar   *icon_name)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_icon_name (icon_name));
}

void
_gtk_icon_helper_set_surface (GtkIconHelper *self,
			      cairo_surface_t *surface)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_surface (surface));
}

void
_gtk_icon_helper_set_texture (GtkIconHelper *self,
			      GdkTexture *texture)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_texture (texture));
}

void
_gtk_icon_helper_set_paintable (GtkIconHelper *self,
			        GdkPaintable  *paintable)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_paintable (paintable));
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

GIcon *
_gtk_icon_helper_peek_gicon (GtkIconHelper *self)
{
  return gtk_image_definition_get_gicon (self->def);
}

cairo_surface_t *
_gtk_icon_helper_peek_surface (GtkIconHelper *self)
{
  return gtk_image_definition_get_surface (self->def);
}

GdkTexture *
_gtk_icon_helper_peek_texture (GtkIconHelper *self)
{
  return gtk_image_definition_get_texture (self->def);
}

GdkPaintable *
_gtk_icon_helper_peek_paintable (GtkIconHelper *self)
{
  return gtk_image_definition_get_paintable (self->def);
}

const gchar *
_gtk_icon_helper_get_icon_name (GtkIconHelper *self)
{
  return gtk_image_definition_get_icon_name (self->def);
}

void
gtk_icon_helper_snapshot (GtkIconHelper *self,
                          GtkSnapshot   *snapshot,
                          double         width,
                          double         height)
{
  GtkCssStyle *style;

  style = gtk_css_node_get_style (self->node);

  gtk_icon_helper_ensure_paintable (self);
  if (self->paintable == NULL)
    return;

  gtk_css_style_snapshot_icon_paintable (style,
                                         snapshot,
                                         self->paintable,
                                         width, height,
                                         self->texture_is_symbolic);
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
gtk_icon_size_set_style_classes (GtkCssNode  *cssnode,
                                 GtkIconSize  icon_size)
{
  struct {
    GtkIconSize icon_size;
    const char *class_name;
  } class_names[] = {
    { GTK_ICON_SIZE_NORMAL, "normal-icons" },
    { GTK_ICON_SIZE_LARGE, "large-icons" }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (class_names); i++)
    {
      if (icon_size == class_names[i].icon_size)
        gtk_css_node_add_class (cssnode, g_quark_from_static_string (class_names[i].class_name));
      else
        gtk_css_node_remove_class (cssnode, g_quark_from_static_string (class_names[i].class_name));
    }
}
