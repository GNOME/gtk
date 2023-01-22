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
#include "deprecated/gtkiconfactoryprivate.h"
#include "deprecated/gtkstock.h"

struct _GtkIconHelperPrivate {
  GtkImageDefinition *def;

  GtkIconSize icon_size;
  gint pixel_size;

  guint use_fallback : 1;
  guint force_scale_pixbuf : 1;
  guint rendered_surface_is_symbolic : 1;

  cairo_surface_t *rendered_surface;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkIconHelper, gtk_icon_helper, GTK_TYPE_CSS_GADGET)

static void
gtk_icon_helper_invalidate (GtkIconHelper *self)
{
  if (self->priv->rendered_surface != NULL)
    {
      cairo_surface_destroy (self->priv->rendered_surface);
      self->priv->rendered_surface = NULL;
      self->priv->rendered_surface_is_symbolic = FALSE;
    }

  if (!GTK_IS_CSS_TRANSIENT_NODE (gtk_css_gadget_get_node (GTK_CSS_GADGET (self))))
    gtk_widget_queue_resize (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self)));
}

void
gtk_icon_helper_invalidate_for_change (GtkIconHelper     *self,
                                       GtkCssStyleChange *change)
{
  GtkIconHelperPrivate *priv = self->priv;

  if (change == NULL ||
      ((gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SYMBOLIC_ICON) &&
        priv->rendered_surface_is_symbolic) ||
       (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON) &&
        !priv->rendered_surface_is_symbolic)))
    {
      gtk_icon_helper_invalidate (self);
    }
}

static void
gtk_icon_helper_take_definition (GtkIconHelper      *self,
                                 GtkImageDefinition *def)
{
  _gtk_icon_helper_clear (self);

  if (def == NULL)
    return;

  gtk_image_definition_unref (self->priv->def);
  self->priv->def = def;

  gtk_icon_helper_invalidate (self);
}

void
_gtk_icon_helper_clear (GtkIconHelper *self)
{
  g_clear_pointer (&self->priv->rendered_surface, cairo_surface_destroy);

  gtk_image_definition_unref (self->priv->def);
  self->priv->def = gtk_image_definition_new_empty ();

  self->priv->icon_size = GTK_ICON_SIZE_INVALID;

  gtk_icon_helper_invalidate (self);
}

static void
gtk_icon_helper_get_preferred_size (GtkCssGadget   *gadget,
                                    GtkOrientation  orientation,
                                    gint            for_size,
                                    gint           *minimum,
                                    gint           *natural,
                                    gint           *minimum_baseline,
                                    gint           *natural_baseline)
{
  GtkIconHelper *self = GTK_ICON_HELPER (gadget);
  int icon_width, icon_height;

  _gtk_icon_helper_get_size (self, &icon_width, &icon_height);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = icon_width;
  else
    *minimum = *natural = icon_height;
}

static void
gtk_icon_helper_allocate (GtkCssGadget        *gadget,
                          const GtkAllocation *allocation,
                          int                  baseline,
                          GtkAllocation       *out_clip)
{
  GTK_CSS_GADGET_CLASS (gtk_icon_helper_parent_class)->allocate (gadget, allocation, baseline, out_clip);
}

static gboolean
gtk_icon_helper_draw (GtkCssGadget *gadget,
                      cairo_t      *cr,
                      int           x,
                      int           y,
                      int           width,
                      int           height)
{
  GtkIconHelper *self = GTK_ICON_HELPER (gadget);
  int icon_width, icon_height;

  _gtk_icon_helper_get_size (self, &icon_width, &icon_height);
  _gtk_icon_helper_draw (self,
                         cr,
                         x + (width - icon_width) / 2,
                         y + (height - icon_height) / 2);

  return FALSE;
}

static void
gtk_icon_helper_style_changed (GtkCssGadget      *gadget,
                               GtkCssStyleChange *change)
{
  gtk_icon_helper_invalidate_for_change (GTK_ICON_HELPER (gadget), change);

  if (!GTK_IS_CSS_TRANSIENT_NODE (gtk_css_gadget_get_node (gadget)))
    GTK_CSS_GADGET_CLASS (gtk_icon_helper_parent_class)->style_changed (gadget, change);
}

static void
gtk_icon_helper_constructed (GObject *object)
{
  GtkIconHelper *self = GTK_ICON_HELPER (object);
  GtkWidget *widget;

  widget = gtk_css_gadget_get_owner (GTK_CSS_GADGET (self));

  g_signal_connect_swapped (widget, "direction-changed", G_CALLBACK (gtk_icon_helper_invalidate), self);
  g_signal_connect_swapped (widget, "notify::scale-factor", G_CALLBACK (gtk_icon_helper_invalidate), self);

  G_OBJECT_CLASS (gtk_icon_helper_parent_class)->constructed (object);
}

static void
gtk_icon_helper_finalize (GObject *object)
{
  GtkIconHelper *self = GTK_ICON_HELPER (object);
  GtkWidget *widget;

  widget = gtk_css_gadget_get_owner (GTK_CSS_GADGET (self));
  g_signal_handlers_disconnect_by_func (widget, G_CALLBACK (gtk_icon_helper_invalidate), self);

  _gtk_icon_helper_clear (self);
  gtk_image_definition_unref (self->priv->def);
  
  G_OBJECT_CLASS (gtk_icon_helper_parent_class)->finalize (object);
}

static void
gtk_icon_helper_class_init (GtkIconHelperClass *klass)
{
  GtkCssGadgetClass *gadget_class = GTK_CSS_GADGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  gadget_class->get_preferred_size = gtk_icon_helper_get_preferred_size;
  gadget_class->allocate = gtk_icon_helper_allocate;
  gadget_class->draw = gtk_icon_helper_draw;
  gadget_class->style_changed = gtk_icon_helper_style_changed;

  object_class->constructed = gtk_icon_helper_constructed;
  object_class->finalize = gtk_icon_helper_finalize;
}

static void
gtk_icon_helper_init (GtkIconHelper *self)
{
  self->priv = gtk_icon_helper_get_instance_private (self);

  self->priv->def = gtk_image_definition_new_empty ();

  self->priv->icon_size = GTK_ICON_SIZE_INVALID;
  self->priv->pixel_size = -1;
  self->priv->rendered_surface_is_symbolic = FALSE;
}

static void
ensure_icon_size (GtkIconHelper *self,
		  gint *width_out,
		  gint *height_out)
{
  gint width, height;

  if (self->priv->pixel_size != -1)
    {
      width = height = self->priv->pixel_size;
    }
  else if (!gtk_icon_size_lookup (self->priv->icon_size, &width, &height))
    {
      if (self->priv->icon_size == GTK_ICON_SIZE_INVALID)
        {
          width = height = 0;
        }
      else
        {
          g_warning ("Invalid icon size %d", self->priv->icon_size);
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

  if (self->priv->pixel_size != -1 || self->priv->force_scale_pixbuf)
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

  if (self->priv->force_scale_pixbuf &&
      (self->priv->pixel_size != -1 ||
       self->priv->icon_size != GTK_ICON_SIZE_INVALID))
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
  GtkCssIconEffect icon_effect;

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

  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, gtk_widget_get_window (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self))));
  icon_effect = _gtk_css_icon_effect_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_EFFECT));
  gtk_css_icon_effect_apply (icon_effect, surface);
  g_object_unref (pixbuf);

  return surface;
}

static cairo_surface_t *
ensure_surface_for_icon_set (GtkIconHelper    *self,
                             GtkCssStyle      *style,
                             GtkTextDirection  direction,
                             gint              scale,
			     GtkIconSet       *icon_set)
{
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf;

  pixbuf = gtk_icon_set_render_icon_pixbuf_for_scale (icon_set,
                                                      style,
                                                      direction,
                                                      self->priv->icon_size,
                                                      scale);
  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf,
                                                  scale,
                                                  gtk_widget_get_window (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self))));
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
  GtkIconHelperPrivate *priv = self->priv;
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
      destination = gtk_icon_theme_load_icon_for_scale (icon_theme,
                                                        "image-missing",
                                                        MIN (width, height),
                                                        scale,
                                                        flags | GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                                        &error);
      /* We include this image as resource, so we always have it available or
       * the icontheme code is broken */
      g_assert_no_error (error);
      g_assert (destination);
      symbolic = FALSE;
    }

  surface = gdk_cairo_surface_create_from_pixbuf (destination, scale, gtk_widget_get_window (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self))));

  if (!symbolic)
    {
      GtkCssIconEffect icon_effect;

      icon_effect = _gtk_css_icon_effect_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_EFFECT));
      gtk_css_icon_effect_apply (icon_effect, surface);
    }
  else
    {
      priv->rendered_surface_is_symbolic = TRUE;
    }

  g_object_unref (destination);

  return surface;
}

cairo_surface_t *
gtk_icon_helper_load_surface (GtkIconHelper   *self,
                              int              scale)
{
  cairo_surface_t *surface;
  GtkIconSet *icon_set;
  GIcon *gicon;

  switch (gtk_image_definition_get_storage_type (self->priv->def))
    {
    case GTK_IMAGE_SURFACE:
      surface = ensure_surface_from_surface (self, gtk_image_definition_get_surface (self->priv->def));
      break;

    case GTK_IMAGE_PIXBUF:
      surface = ensure_surface_from_pixbuf (self,
                                            gtk_css_node_get_style (gtk_css_gadget_get_node (GTK_CSS_GADGET (self))),
                                            scale,
                                            gtk_image_definition_get_pixbuf (self->priv->def),
                                            gtk_image_definition_get_scale (self->priv->def));
      break;

    case GTK_IMAGE_STOCK:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      icon_set = gtk_icon_factory_lookup_default (gtk_image_definition_get_stock (self->priv->def));
G_GNUC_END_IGNORE_DEPRECATIONS;
      if (icon_set != NULL)
	surface = ensure_surface_for_icon_set (self,
                                               gtk_css_node_get_style (gtk_css_gadget_get_node (GTK_CSS_GADGET (self))),
                                               gtk_widget_get_direction (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self))), 
                                               scale, icon_set);
      else
	surface = NULL;
      break;

    case GTK_IMAGE_ICON_SET:
      icon_set = gtk_image_definition_get_icon_set (self->priv->def);
      surface = ensure_surface_for_icon_set (self,
                                             gtk_css_node_get_style (gtk_css_gadget_get_node (GTK_CSS_GADGET (self))),
                                             gtk_widget_get_direction (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self))), 
                                             scale, icon_set);
      break;

    case GTK_IMAGE_ICON_NAME:
      if (self->priv->use_fallback)
        gicon = g_themed_icon_new_with_default_fallbacks (gtk_image_definition_get_icon_name (self->priv->def));
      else
        gicon = g_themed_icon_new (gtk_image_definition_get_icon_name (self->priv->def));
      surface = ensure_surface_for_gicon (self,
                                          gtk_css_node_get_style (gtk_css_gadget_get_node (GTK_CSS_GADGET (self))),
                                          gtk_widget_get_direction (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self))), 
                                          scale, 
                                          gicon);
      g_object_unref (gicon);
      break;

    case GTK_IMAGE_GICON:
      surface = ensure_surface_for_gicon (self, 
                                          gtk_css_node_get_style (gtk_css_gadget_get_node (GTK_CSS_GADGET (self))),
                                          gtk_widget_get_direction (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self))), 
                                          scale,
                                          gtk_image_definition_get_gicon (self->priv->def));
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

  if (self->priv->rendered_surface)
    return;

  scale = gtk_widget_get_scale_factor (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self)));

  self->priv->rendered_surface = gtk_icon_helper_load_surface (self, scale);
}

void
_gtk_icon_helper_get_size (GtkIconHelper *self,
                           gint *width_out,
                           gint *height_out)
{
  gint width, height, scale;

  width = height = 0;

  /* Certain kinds of images are easy to calculate the size for, these
     we do immediately to avoid having to potentially load the image
     data for something that may not yet be visible */
  switch (gtk_image_definition_get_storage_type (self->priv->def))
    {
    case GTK_IMAGE_SURFACE:
      get_surface_size (self,
                        gtk_image_definition_get_surface (self->priv->def),
                        &width,
                        &height);
      break;

    case GTK_IMAGE_PIXBUF:
      get_pixbuf_size (self,
                       gtk_widget_get_scale_factor (gtk_css_gadget_get_owner (GTK_CSS_GADGET (self))),
                       gtk_image_definition_get_pixbuf (self->priv->def),
                       gtk_image_definition_get_scale (self->priv->def),
                       &width, &height, &scale);
      width = (width + scale - 1) / scale;
      height = (height + scale - 1) / scale;
      break;

    case GTK_IMAGE_ANIMATION:
      {
        GdkPixbufAnimation *animation = gtk_image_definition_get_animation (self->priv->def);
        width = gdk_pixbuf_animation_get_width (animation);
        height = gdk_pixbuf_animation_get_height (animation);
        break;
      }

    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      if (self->priv->pixel_size != -1 || self->priv->force_scale_pixbuf)
        ensure_icon_size (self, &width, &height);

      break;

    case GTK_IMAGE_STOCK:
    case GTK_IMAGE_ICON_SET:
    case GTK_IMAGE_EMPTY:
    default:
      break;
    }

  /* Otherwise we load the surface to guarantee we get a size */
  if (width == 0)
    {
      gtk_icon_helper_ensure_surface (self);

      if (self->priv->rendered_surface != NULL)
        {
          get_surface_size (self, self->priv->rendered_surface, &width, &height);
        }
      else if (self->priv->icon_size != GTK_ICON_SIZE_INVALID)
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
_gtk_icon_helper_set_icon_set (GtkIconHelper *self,
                               GtkIconSet *icon_set,
                               GtkIconSize icon_size)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_icon_set (icon_set));
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

void 
_gtk_icon_helper_set_stock_id (GtkIconHelper *self,
                               const gchar *stock_id,
                               GtkIconSize icon_size)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_stock (stock_id));
  _gtk_icon_helper_set_icon_size (self, icon_size);
}

gboolean
_gtk_icon_helper_set_icon_size (GtkIconHelper *self,
                                GtkIconSize    icon_size)
{
  if (self->priv->icon_size != icon_size)
    {
      self->priv->icon_size = icon_size;
      gtk_icon_helper_invalidate (self);
      return TRUE;
    }
  return FALSE;
}

gboolean
_gtk_icon_helper_set_pixel_size (GtkIconHelper *self,
                                 gint           pixel_size)
{
  if (self->priv->pixel_size != pixel_size)
    {
      self->priv->pixel_size = pixel_size;
      gtk_icon_helper_invalidate (self);
      return TRUE;
    }
  return FALSE;
}

gboolean
_gtk_icon_helper_set_use_fallback (GtkIconHelper *self,
                                   gboolean       use_fallback)
{
  if (self->priv->use_fallback != use_fallback)
    {
      self->priv->use_fallback = use_fallback;
      gtk_icon_helper_invalidate (self);
      return TRUE;
    }
  return FALSE;
}

GtkImageType
_gtk_icon_helper_get_storage_type (GtkIconHelper *self)
{
  return gtk_image_definition_get_storage_type (self->priv->def);
}

gboolean
_gtk_icon_helper_get_use_fallback (GtkIconHelper *self)
{
  return self->priv->use_fallback;
}

GtkIconSize
_gtk_icon_helper_get_icon_size (GtkIconHelper *self)
{
  return self->priv->icon_size;
}

gint
_gtk_icon_helper_get_pixel_size (GtkIconHelper *self)
{
  return self->priv->pixel_size;
}

GtkImageDefinition *
gtk_icon_helper_get_definition (GtkIconHelper *self)
{
  return self->priv->def;
}

GdkPixbuf *
_gtk_icon_helper_peek_pixbuf (GtkIconHelper *self)
{
  return gtk_image_definition_get_pixbuf (self->priv->def);
}

GIcon *
_gtk_icon_helper_peek_gicon (GtkIconHelper *self)
{
  return gtk_image_definition_get_gicon (self->priv->def);
}

GdkPixbufAnimation *
_gtk_icon_helper_peek_animation (GtkIconHelper *self)
{
  return gtk_image_definition_get_animation (self->priv->def);
}

GtkIconSet *
_gtk_icon_helper_peek_icon_set (GtkIconHelper *self)
{
  return gtk_image_definition_get_icon_set (self->priv->def);
}

cairo_surface_t *
_gtk_icon_helper_peek_surface (GtkIconHelper *self)
{
  return gtk_image_definition_get_surface (self->priv->def);
}

const gchar *
_gtk_icon_helper_get_stock_id (GtkIconHelper *self)
{
  return gtk_image_definition_get_stock (self->priv->def);
}

const gchar *
_gtk_icon_helper_get_icon_name (GtkIconHelper *self)
{
  return gtk_image_definition_get_icon_name (self->priv->def);
}

GtkIconHelper *
gtk_icon_helper_new (GtkCssNode *node,
                     GtkWidget  *owner)
{
  g_return_val_if_fail (GTK_IS_CSS_NODE (node), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (owner), NULL);

  return g_object_new (GTK_TYPE_ICON_HELPER,
                       "node", node,
                       "owner", owner,
                       NULL);
}

GtkCssGadget *
gtk_icon_helper_new_named (const char *name,
                           GtkWidget  *owner)
{
  GtkIconHelper *result;
  GtkCssNode *node;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (owner), NULL);

  node = gtk_css_node_new ();
  gtk_css_node_set_name (node, g_intern_string (name));

  result = gtk_icon_helper_new (node, owner);

  g_object_unref (node);

  return GTK_CSS_GADGET (result);
}

void
_gtk_icon_helper_draw (GtkIconHelper *self,
                       cairo_t *cr,
                       gdouble x,
                       gdouble y)
{
  GtkCssStyle *style = gtk_css_node_get_style (gtk_css_gadget_get_node (GTK_CSS_GADGET (self)));
  gtk_icon_helper_ensure_surface (self);

  if (self->priv->rendered_surface != NULL)
    {
      gtk_css_style_render_icon_surface (style,
                                         cr,
                                         self->priv->rendered_surface,
                                         x, y);
    }
}

gboolean
_gtk_icon_helper_get_is_empty (GtkIconHelper *self)
{
  return gtk_image_definition_get_storage_type (self->priv->def) == GTK_IMAGE_EMPTY;
}

gboolean
_gtk_icon_helper_get_force_scale_pixbuf (GtkIconHelper *self)
{
  return self->priv->force_scale_pixbuf;
}

void
_gtk_icon_helper_set_force_scale_pixbuf (GtkIconHelper *self,
                                         gboolean       force_scale)
{
  if (self->priv->force_scale_pixbuf != force_scale)
    {
      self->priv->force_scale_pixbuf = force_scale;
      gtk_icon_helper_invalidate (self);
    }
}

void 
_gtk_icon_helper_set_pixbuf_scale (GtkIconHelper *self,
				   int scale)
{
  switch (gtk_image_definition_get_storage_type (self->priv->def))
  {
    case GTK_IMAGE_PIXBUF:
      gtk_icon_helper_take_definition (self,
                                      gtk_image_definition_new_pixbuf (gtk_image_definition_get_pixbuf (self->priv->def),
                                                                       scale));
      break;

    case GTK_IMAGE_ANIMATION:
      gtk_icon_helper_take_definition (self,
                                      gtk_image_definition_new_animation (gtk_image_definition_get_animation (self->priv->def),
                                                                          scale));
      break;

    default:
      break;
  }
}
