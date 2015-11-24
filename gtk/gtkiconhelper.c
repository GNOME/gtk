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
#include "gtkimagedefinitionprivate.h"
#include "gtkrender.h"
#include "gtkstylecontextprivate.h"
#include "deprecated/gtkstock.h"

struct _GtkIconHelperPrivate {
  GtkImageDefinition *def;

  GdkWindow *window;

  GtkIconSize icon_size;
  gint pixel_size;

  guint use_fallback : 1;
  guint force_scale_pixbuf : 1;

  GdkPixbuf *rendered_pixbuf;
  GtkStateFlags last_rendered_state;

  cairo_surface_t *rendered_surface;
  gint rendered_surface_width;
  gint rendered_surface_height;
  GtkStateFlags last_surface_state;
  gint last_surface_scale;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkIconHelper, _gtk_icon_helper, G_TYPE_OBJECT)

static void
gtk_icon_helper_take_definition (GtkIconHelper      *self,
                                 GtkImageDefinition *def)
{
  GtkIconSize icon_size;

  _gtk_icon_helper_clear (self);

  if (def == NULL)
    return;

  gtk_image_definition_unref (self->priv->def);
  self->priv->def = def;

  icon_size = gtk_image_definition_get_icon_size (def);
  if (icon_size != GTK_ICON_SIZE_INVALID)
    self->priv->icon_size = icon_size;
  
  _gtk_icon_helper_invalidate (self);
}

void
_gtk_icon_helper_clear (GtkIconHelper *self)
{
  g_clear_object (&self->priv->rendered_pixbuf);
  g_clear_object (&self->priv->window);
  g_clear_pointer (&self->priv->rendered_surface, cairo_surface_destroy);

  gtk_image_definition_unref (self->priv->def);
  self->priv->def = gtk_image_definition_new_empty ();

  self->priv->icon_size = GTK_ICON_SIZE_INVALID;
  self->priv->last_rendered_state = GTK_STATE_FLAG_NORMAL;
  self->priv->last_surface_state = GTK_STATE_FLAG_NORMAL;
  self->priv->last_surface_scale = 0;
}

void
_gtk_icon_helper_invalidate (GtkIconHelper *self)
{
  g_clear_object (&self->priv->rendered_pixbuf);
  if (self->priv->rendered_surface != NULL)
    {
      cairo_surface_destroy (self->priv->rendered_surface);
      self->priv->rendered_surface = NULL;
    }
}

void
 _gtk_icon_helper_set_window (GtkIconHelper *self,
			      GdkWindow *window)
{
  if (window)
    g_object_ref (window);
  g_clear_object (&self->priv->window);
  self->priv->window = window;

}

static void
gtk_icon_helper_finalize (GObject *object)
{
  GtkIconHelper *self = GTK_ICON_HELPER (object);

  _gtk_icon_helper_clear (self);
  gtk_image_definition_unref (self->priv->def);
  
  G_OBJECT_CLASS (_gtk_icon_helper_parent_class)->finalize (object);
}

static void
_gtk_icon_helper_class_init (GtkIconHelperClass *klass)
{
  GObjectClass *oclass;

  oclass = G_OBJECT_CLASS (klass);
  oclass->finalize = gtk_icon_helper_finalize;
}

static void
_gtk_icon_helper_init (GtkIconHelper *self)
{
  self->priv = _gtk_icon_helper_get_instance_private (self);

  self->priv->def = gtk_image_definition_new_empty ();

  self->priv->icon_size = GTK_ICON_SIZE_INVALID;
  self->priv->pixel_size = -1;
  self->priv->last_rendered_state = GTK_STATE_FLAG_NORMAL;
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
          g_warning ("Invalid icon size %d\n", self->priv->icon_size);
          width = height = 24;
        }
    }

  *width_out = width;
  *height_out = height;
}

static GdkPixbuf *
ensure_stated_pixbuf_from_pixbuf (GtkIconHelper   *self,
				  GtkStyleContext *context,
				  GdkPixbuf       *pixbuf)
{
  GdkPixbuf *rendered;
  GtkIconSource *source;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  /* FIXME: use gtk_icon_info_load_icon? */

  source = gtk_icon_source_new ();
  gtk_icon_source_set_pixbuf (source, pixbuf);
  /* The size here is arbitrary; since size isn't
   * wildcarded in the source, it isn't supposed to be
   * scaled by the engine function
   */
  gtk_icon_source_set_size (source,
			    GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_icon_source_set_size_wildcarded (source, FALSE);

  rendered = gtk_render_icon_pixbuf (context, source, (GtkIconSize) -1);
  gtk_icon_source_free (source);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return rendered;
}

static GdkPixbuf *
ensure_stated_icon_from_info (GtkIconHelper *self,
                              GtkStyleContext *context,
			      GtkIconInfo *info)
{
  GdkPixbuf *destination = NULL;
  gboolean symbolic;

  symbolic = FALSE;

  if (info)
    destination =
      gtk_icon_info_load_symbolic_for_context (info,
					       context,
					       &symbolic,
					       NULL);

  if (destination == NULL)
    {
      GtkIconTheme *icon_theme;
      int width;

      icon_theme = gtk_icon_theme_get_for_screen (gtk_style_context_get_screen (context));
      gtk_icon_size_lookup (self->priv->icon_size, &width, NULL);
      destination = gtk_icon_theme_load_icon (icon_theme,
                                              "image-missing",
                                              width,
                                              GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                              NULL);
    }
  else if (!symbolic)
    {
      GdkPixbuf *rendered;

      rendered = ensure_stated_pixbuf_from_pixbuf (self, context, destination);
      g_object_unref (destination);
      destination = rendered;
    }

  return destination;
}

static gboolean
check_invalidate_pixbuf (GtkIconHelper *self,
                         GtkStyleContext *context)
{
  GtkStateFlags state;

  state = gtk_style_context_get_state (context);

  if ((self->priv->rendered_pixbuf != NULL) &&
      (self->priv->last_rendered_state == state))
    return FALSE;

  self->priv->last_rendered_state = state;
  g_clear_object (&self->priv->rendered_pixbuf);

  return TRUE;
}

static GtkIconLookupFlags
get_icon_lookup_flags (GtkIconHelper *self, GtkStyleContext *context)
{
  GtkIconLookupFlags flags;
  GtkCssIconStyle icon_style;
  GtkStateFlags state;

  state = gtk_style_context_get_state (context);
  flags = GTK_ICON_LOOKUP_USE_BUILTIN;

  if (self->priv->pixel_size != -1 || self->priv->force_scale_pixbuf)
    flags |= GTK_ICON_LOOKUP_FORCE_SIZE;

  icon_style = _gtk_css_icon_style_value_get (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ICON_STYLE));

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

  if (state & GTK_STATE_FLAG_DIR_LTR)
    flags |= GTK_ICON_LOOKUP_DIR_LTR;
  else if (state & GTK_STATE_FLAG_DIR_RTL)
    flags |= GTK_ICON_LOOKUP_DIR_RTL;

  return flags;
}

static void
ensure_pixbuf_for_gicon (GtkIconHelper   *self,
                         GtkStyleContext *context,
                         GIcon           *gicon)
{
  GtkIconTheme *icon_theme;
  gint width, height;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;

  if (!check_invalidate_pixbuf (self, context))
    return;

  icon_theme = gtk_icon_theme_get_for_screen (gtk_style_context_get_screen (context));
  flags = get_icon_lookup_flags (self, context);

  ensure_icon_size (self, &width, &height);

  info = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                         gicon,
                                         MIN (width, height), flags);

  self->priv->rendered_pixbuf = ensure_stated_icon_from_info (self, context, info);

  if (info)
    g_object_unref (info);
}

static void
ensure_pixbuf_for_icon_set (GtkIconHelper *self,
                            GtkStyleContext *context,
                            GtkIconSet *icon_set)
{
  if (!check_invalidate_pixbuf (self, context))
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  self->priv->rendered_pixbuf = 
    gtk_icon_set_render_icon_pixbuf (icon_set, context, self->priv->icon_size);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
get_surface_size (GtkIconHelper   *self,
		  cairo_surface_t *surface,
		  int *width,
		  int *height)
{
  double x_scale, y_scale;

  if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE)
    {
      x_scale = y_scale = 1;

      cairo_surface_get_device_scale (surface, &x_scale, &y_scale);

      /* Assume any set scaling is icon scale */
      *width =
	ceil (cairo_image_surface_get_width (surface) / x_scale);
      *height =
	ceil (cairo_image_surface_get_height (surface) / y_scale);
    }
  else
    ensure_icon_size (self, width, height);
}

static void
ensure_pixbuf_from_surface (GtkIconHelper   *self,
			    GtkStyleContext *context,
                            cairo_surface_t *orig_surface)
{
  cairo_surface_t *surface;
  gint width, height;
  cairo_t *cr;


  if (!check_invalidate_pixbuf (self, context))
    return;

  if (self->priv->rendered_pixbuf)
    return;

  get_surface_size (self, orig_surface, &width, &height);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					width, height);

  cr = cairo_create (surface);
  cairo_set_source_surface (cr, orig_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  self->priv->rendered_pixbuf =
    gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);

  cairo_surface_destroy (surface);
}

static void
ensure_pixbuf_at_size (GtkIconHelper   *self,
                       GtkStyleContext *context,
                       GdkPixbuf       *orig_pixbuf,
                       int              orig_scale)
{
  gint width, height;
  GdkPixbuf *stated;

  if (!check_invalidate_pixbuf (self, context))
    return;

  if (self->priv->rendered_pixbuf)
    return;

  if (self->priv->force_scale_pixbuf &&
      (self->priv->pixel_size != -1 ||
       self->priv->icon_size != GTK_ICON_SIZE_INVALID))
    {
      ensure_icon_size (self, &width, &height);

      if (orig_scale > 1 ||
	  /* These should divide the orig_pixbuf size by scale, but need not 
	     due to the above scale > 1 check */
	  width < gdk_pixbuf_get_width (orig_pixbuf) ||
          height < gdk_pixbuf_get_height (orig_pixbuf))
	{
	  width = MIN (width, gdk_pixbuf_get_width (orig_pixbuf) / orig_scale);
	  height = MIN (height, gdk_pixbuf_get_height (orig_pixbuf) / orig_scale);
	  self->priv->rendered_pixbuf =
	    gdk_pixbuf_scale_simple (orig_pixbuf,
				     width, height,
				     GDK_INTERP_BILINEAR);
	}
    }
  else if (orig_scale > 1)
    {
	  width = gdk_pixbuf_get_width (orig_pixbuf) / orig_scale;
	  height =gdk_pixbuf_get_height (orig_pixbuf) / orig_scale;
	  self->priv->rendered_pixbuf =
	    gdk_pixbuf_scale_simple (orig_pixbuf,
				     width, height,
				     GDK_INTERP_BILINEAR);
    }

  if (!self->priv->rendered_pixbuf)
    self->priv->rendered_pixbuf = g_object_ref (orig_pixbuf);

  stated = ensure_stated_pixbuf_from_pixbuf (self, context, self->priv->rendered_pixbuf);
  g_object_unref (self->priv->rendered_pixbuf);
  self->priv->rendered_pixbuf = stated;
}

GdkPixbuf *
_gtk_icon_helper_ensure_pixbuf (GtkIconHelper *self,
                                GtkStyleContext *context)
{
  GdkPixbuf *pixbuf = NULL;
  GtkIconSet *icon_set;
  GIcon *gicon;

  switch (gtk_image_definition_get_storage_type (self->priv->def))
    {
    case GTK_IMAGE_SURFACE:
      ensure_pixbuf_from_surface (self, context, gtk_image_definition_get_surface (self->priv->def));
      break;

    case GTK_IMAGE_PIXBUF:
      ensure_pixbuf_at_size (self, context, 
                             gtk_image_definition_get_pixbuf (self->priv->def),
                             gtk_image_definition_get_scale (self->priv->def));
      break;

    case GTK_IMAGE_STOCK:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      icon_set = gtk_icon_factory_lookup_default (gtk_image_definition_get_stock (self->priv->def));
      if (icon_set != NULL)
	ensure_pixbuf_for_icon_set (self, context, icon_set);
      else
	pixbuf = NULL;
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;

    case GTK_IMAGE_ICON_SET:
      icon_set = gtk_image_definition_get_icon_set (self->priv->def);
      ensure_pixbuf_for_icon_set (self, context, icon_set);
      break;

    case GTK_IMAGE_ICON_NAME:
      if (self->priv->use_fallback)
        gicon = g_themed_icon_new_with_default_fallbacks (gtk_image_definition_get_icon_name (self->priv->def));
      else
        gicon = g_themed_icon_new (gtk_image_definition_get_icon_name (self->priv->def));
      ensure_pixbuf_for_gicon (self, context, gicon);
      g_object_unref (gicon);
      break;

    case GTK_IMAGE_GICON:
      ensure_pixbuf_for_gicon (self, context, gtk_image_definition_get_gicon (self->priv->def));
      break;

    case GTK_IMAGE_ANIMATION:
    case GTK_IMAGE_EMPTY:
    default:
      pixbuf = NULL;
      break;
    }

  if (pixbuf == NULL &&
      self->priv->rendered_pixbuf != NULL)
    pixbuf = g_object_ref (self->priv->rendered_pixbuf);

  return pixbuf;
}

static gint
get_scale_factor (GtkIconHelper *self,
		  GtkStyleContext *context)
{
  GdkScreen *screen;

  if (self->priv->window)
    return gdk_window_get_scale_factor (self->priv->window);

  screen = gtk_style_context_get_screen (context);

  /* else fall back to something that is more likely to be right than
   * just returning 1:
   */
  return gdk_screen_get_monitor_scale_factor (screen, 0);
}

static gboolean
check_invalidate_surface (GtkIconHelper *self,
			  GtkStyleContext *context)
{
  GtkStateFlags state;
  int scale;

  state = gtk_style_context_get_state (context);
  scale = get_scale_factor (self, context);

  if ((self->priv->rendered_surface != NULL) &&
      (self->priv->last_surface_state == state) &&
      (self->priv->last_surface_scale == scale))
    return FALSE;

  self->priv->last_surface_state = state;
  self->priv->last_surface_scale = scale;

  if (self->priv->rendered_surface)
    cairo_surface_destroy (self->priv->rendered_surface);
  self->priv->rendered_surface = NULL;

  return TRUE;
}

static void
ensure_surface_from_surface (GtkIconHelper   *self,
			     GtkStyleContext *context,
                             cairo_surface_t *orig_surface)
{
  if (!check_invalidate_surface (self, context))
    return;

  if (self->priv->rendered_surface)
    return;

  self->priv->rendered_surface =
    cairo_surface_reference (orig_surface);

  get_surface_size (self, orig_surface,
		    &self->priv->rendered_surface_width,
		    &self->priv->rendered_surface_height);
}

static gboolean
get_pixbuf_size (GtkIconHelper   *self,
                 GtkStyleContext *context,
                 GdkPixbuf       *orig_pixbuf,
                 gint             orig_scale,
                 gint *width_out,
                 gint *height_out,
                 gint *scale_out)
{
  gboolean scale_pixmap;
  gint width, height;
  int scale;

  scale = get_scale_factor (self, context);
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

static void
ensure_surface_from_pixbuf (GtkIconHelper   *self,
			    GtkStyleContext *context,
                            GdkPixbuf       *orig_pixbuf,
                            gint             orig_scale)
{
  gint width, height;
  GdkPixbuf *pixbuf, *stated;
  int scale;

  if (!check_invalidate_surface (self, context))
    return;

  if (self->priv->rendered_surface)
    return;

  if (get_pixbuf_size (self,
                       context,
                       orig_pixbuf,
                       orig_scale,
                       &width, &height, &scale))
    pixbuf = gdk_pixbuf_scale_simple (orig_pixbuf,
                                      width, height,
                                      GDK_INTERP_BILINEAR);
  else
    pixbuf = g_object_ref (orig_pixbuf);

  stated = ensure_stated_pixbuf_from_pixbuf (self, context, pixbuf);
  g_object_unref (pixbuf);
  pixbuf = stated;

  self->priv->rendered_surface_width = (width + scale - 1) / scale;
  self->priv->rendered_surface_height = (height + scale - 1) / scale;

  self->priv->rendered_surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, self->priv->window);
  g_object_unref (pixbuf);
}

static void
ensure_surface_for_icon_set (GtkIconHelper *self,
			     GtkStyleContext *context,
			     GtkIconSet *icon_set)
{
  gint scale;

  if (!check_invalidate_surface (self, context))
    return;

  scale = get_scale_factor (self, context);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  self->priv->rendered_surface =
    gtk_icon_set_render_icon_surface (icon_set, context, 
				      self->priv->icon_size,
				      scale, self->priv->window);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (self->priv->rendered_surface)
    get_surface_size (self, self->priv->rendered_surface, 
		      &self->priv->rendered_surface_width, 
		      &self->priv->rendered_surface_height);
}

static void
ensure_stated_surface_from_info (GtkIconHelper *self,
				 GtkStyleContext *context,
				 GtkIconInfo *info,
				 int scale)
{
  GdkPixbuf *destination = NULL;
  cairo_surface_t *surface;
  gboolean symbolic;

  symbolic = FALSE;

  if (info)
    destination =
      gtk_icon_info_load_symbolic_for_context (info,
					       context,
					       &symbolic,
					       NULL);

  if (destination == NULL)
    {
      GtkIconSet *icon_set;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

      icon_set = gtk_icon_factory_lookup_default (GTK_STOCK_MISSING_IMAGE);

      destination =
        gtk_icon_set_render_icon_pixbuf (icon_set, context, self->priv->icon_size);

      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
  else if (!symbolic)
    {
      GdkPixbuf *rendered;

      rendered = ensure_stated_pixbuf_from_pixbuf (self, context, destination);
      g_object_unref (destination);
      destination = rendered;
    }

  surface = NULL;
  if (destination)
    {
      surface = gdk_cairo_surface_create_from_pixbuf (destination, scale, self->priv->window);

      self->priv->rendered_surface_width = 
	(gdk_pixbuf_get_width (destination) + scale - 1) / scale;
      self->priv->rendered_surface_height = 
	(gdk_pixbuf_get_height (destination) + scale - 1) / scale;
      g_object_unref (destination);
    }

  self->priv->rendered_surface = surface;
}

static void
ensure_surface_for_gicon (GtkIconHelper   *self,
                          GtkStyleContext *context,
                          GIcon           *gicon)
{
  GtkIconTheme *icon_theme;
  gint width, height, scale;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;

  if (!check_invalidate_surface (self, context))
    return;

  icon_theme = gtk_icon_theme_get_for_screen (gtk_style_context_get_screen (context));
  flags = get_icon_lookup_flags (self, context);

  ensure_icon_size (self, &width, &height);
  scale = get_scale_factor (self, context);

  info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme,
                                                   gicon,
                                                   MIN (width, height),
                                                   scale, flags);

  ensure_stated_surface_from_info (self, context, info, scale);

  if (info)
    g_object_unref (info);
}

cairo_surface_t *
_gtk_icon_helper_ensure_surface (GtkIconHelper *self,
				 GtkStyleContext *context)
{
  cairo_surface_t *surface = NULL;
  GtkIconSet *icon_set;
  GIcon *gicon;

  switch (gtk_image_definition_get_storage_type (self->priv->def))
    {
    case GTK_IMAGE_SURFACE:
      ensure_surface_from_surface (self, context, gtk_image_definition_get_surface (self->priv->def));
      break;

    case GTK_IMAGE_PIXBUF:
      ensure_surface_from_pixbuf (self, context,
                                  gtk_image_definition_get_pixbuf (self->priv->def),
                                  gtk_image_definition_get_scale (self->priv->def));
      break;

    case GTK_IMAGE_STOCK:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      icon_set = gtk_icon_factory_lookup_default (gtk_image_definition_get_stock (self->priv->def));
      if (icon_set != NULL)
	ensure_surface_for_icon_set (self, context, icon_set);
      else
	surface = NULL;
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;

    case GTK_IMAGE_ICON_SET:
      icon_set = gtk_image_definition_get_icon_set (self->priv->def);
      ensure_surface_for_icon_set (self, context, icon_set);
      break;

    case GTK_IMAGE_ICON_NAME:
      if (self->priv->use_fallback)
        gicon = g_themed_icon_new_with_default_fallbacks (gtk_image_definition_get_icon_name (self->priv->def));
      else
        gicon = g_themed_icon_new (gtk_image_definition_get_icon_name (self->priv->def));
      ensure_surface_for_gicon (self, context, gicon);
      g_object_unref (gicon);
      break;

    case GTK_IMAGE_GICON:
      ensure_surface_for_gicon (self, context, gtk_image_definition_get_gicon (self->priv->def));
      break;

    case GTK_IMAGE_ANIMATION:
    case GTK_IMAGE_EMPTY:
    default:
      surface = NULL;
      break;
    }

  if (surface == NULL &&
      self->priv->rendered_surface != NULL)
    surface = cairo_surface_reference (self->priv->rendered_surface);

  return surface;
}

void
_gtk_icon_helper_get_size (GtkIconHelper *self,
                           GtkStyleContext *context,
                           gint *width_out,
                           gint *height_out)
{
  cairo_surface_t *surface;
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
      get_pixbuf_size (self, context,
                       gtk_image_definition_get_pixbuf (self->priv->def),
                       gtk_image_definition_get_scale (self->priv->def),
                       &width, &height, &scale);
      width = (width + scale - 1) / scale;
      height = (height + scale - 1) / scale;
      break;

    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      if (self->priv->pixel_size != -1 || self->priv->force_scale_pixbuf)
        ensure_icon_size (self, &width, &height);

      break;

    case GTK_IMAGE_STOCK:
    case GTK_IMAGE_ICON_SET:
    case GTK_IMAGE_ANIMATION:
    case GTK_IMAGE_EMPTY:
    default:
      break;
    }

  /* Otherwise we load the surface to guarantee we get a size */
  if (width == 0)
    {
      surface = _gtk_icon_helper_ensure_surface (self, context);

      if (surface != NULL)
        {
          width = self->priv->rendered_surface_width;
          height = self->priv->rendered_surface_height;
          cairo_surface_destroy (surface);
        }
      else if (gtk_image_definition_get_storage_type (self->priv->def) == GTK_IMAGE_ANIMATION)
        {
          GdkPixbufAnimation *animation = gtk_image_definition_get_animation (self->priv->def);
          width = gdk_pixbuf_animation_get_width (animation);
          height = gdk_pixbuf_animation_get_height (animation);
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
_gtk_icon_helper_set_gicon (GtkIconHelper *self,
                            GIcon *gicon,
                            GtkIconSize icon_size)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_gicon (gicon, icon_size));
}

void 
_gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                const gchar *icon_name,
                                GtkIconSize icon_size)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_icon_name (icon_name, icon_size));
}

void 
_gtk_icon_helper_set_icon_set (GtkIconHelper *self,
                               GtkIconSet *icon_set,
                               GtkIconSize icon_size)
{
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_icon_set (icon_set, icon_size));
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
  gtk_icon_helper_take_definition (self, gtk_image_definition_new_stock (stock_id, icon_size));
}

gboolean
_gtk_icon_helper_set_icon_size (GtkIconHelper *self,
                                GtkIconSize    icon_size)
{
  if (self->priv->icon_size != icon_size)
    {
      self->priv->icon_size = icon_size;
      _gtk_icon_helper_invalidate (self);
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
      _gtk_icon_helper_invalidate (self);
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
      _gtk_icon_helper_invalidate (self);
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
_gtk_icon_helper_new (void)
{
  return g_object_new (GTK_TYPE_ICON_HELPER, NULL);
}

void
_gtk_icon_helper_draw (GtkIconHelper *self,
                       GtkStyleContext *context,
                       cairo_t *cr,
                       gdouble x,
                       gdouble y)
{
  cairo_surface_t *surface;

  surface = _gtk_icon_helper_ensure_surface (self, context);
  if (surface != NULL)
    {
      gtk_render_icon_surface (context, cr, surface, x, y);
      cairo_surface_destroy (surface);
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
      _gtk_icon_helper_invalidate (self);
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
