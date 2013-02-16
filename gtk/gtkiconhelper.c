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

G_DEFINE_TYPE (GtkIconHelper, _gtk_icon_helper, G_TYPE_OBJECT)

struct _GtkIconHelperPrivate {
  GtkImageType storage_type;

  GdkPixbuf *orig_pixbuf;
  GdkPixbufAnimation *animation;
  GIcon *gicon;
  GtkIconSet *icon_set;
  gchar *icon_name;
  gchar *stock_id;

  GtkIconSize icon_size;
  gint pixel_size;

  guint use_fallback : 1;
  guint force_scale_pixbuf : 1;

  GdkPixbuf *rendered_pixbuf;
  GtkStateFlags last_rendered_state;
};

void
_gtk_icon_helper_clear (GtkIconHelper *self)
{
  g_clear_object (&self->priv->gicon);
  g_clear_object (&self->priv->orig_pixbuf);
  g_clear_object (&self->priv->animation);
  g_clear_object (&self->priv->rendered_pixbuf);

  if (self->priv->icon_set != NULL)
    {
      gtk_icon_set_unref (self->priv->icon_set);
      self->priv->icon_set = NULL;
    }

  g_free (self->priv->icon_name);
  self->priv->icon_name = NULL;

  g_free (self->priv->stock_id);
  self->priv->stock_id = NULL;

  self->priv->storage_type = GTK_IMAGE_EMPTY;
  self->priv->icon_size = GTK_ICON_SIZE_INVALID;
  self->priv->last_rendered_state = GTK_STATE_FLAG_NORMAL;
}

void
_gtk_icon_helper_invalidate (GtkIconHelper *self)
{
  g_clear_object (&self->priv->rendered_pixbuf);
}

static void
gtk_icon_helper_finalize (GObject *object)
{
  GtkIconHelper *self = GTK_ICON_HELPER (object);

  _gtk_icon_helper_clear (self);
  
  G_OBJECT_CLASS (_gtk_icon_helper_parent_class)->finalize (object);
}

static void
_gtk_icon_helper_class_init (GtkIconHelperClass *klass)
{
  GObjectClass *oclass;

  oclass = G_OBJECT_CLASS (klass);
  oclass->finalize = gtk_icon_helper_finalize;

  g_type_class_add_private (klass, sizeof (GtkIconHelperPrivate));
}

static void
_gtk_icon_helper_init (GtkIconHelper *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_ICON_HELPER, GtkIconHelperPrivate);

  self->priv->storage_type = GTK_IMAGE_EMPTY;
  self->priv->icon_size = GTK_ICON_SIZE_INVALID;
  self->priv->pixel_size = -1;
  self->priv->last_rendered_state = GTK_STATE_FLAG_NORMAL;
}

static void
ensure_icon_size (GtkIconHelper *self,
                  GtkStyleContext *context,
		  gint *width_out,
		  gint *height_out)
{
  gint width, height;
  GtkSettings *settings;
  GdkScreen *screen;

  screen = gtk_style_context_get_screen (context);
  settings = gtk_settings_get_for_screen (screen);

  if (self->priv->pixel_size != -1)
    {
      width = height = self->priv->pixel_size;
    }
  else if (!gtk_icon_size_lookup_for_settings (settings,
					       self->priv->icon_size,
					       &width, &height))
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
      GtkIconSet *icon_set;
      icon_set = gtk_style_context_lookup_icon_set (context, GTK_STOCK_MISSING_IMAGE);

      destination =
        gtk_icon_set_render_icon_pixbuf (icon_set, context, self->priv->icon_size);
    }
  else if (!symbolic)
    {
      GtkIconSource *source;
      GdkPixbuf *rendered;

      source = gtk_icon_source_new ();
      gtk_icon_source_set_pixbuf (source, destination);
      /* The size here is arbitrary; since size isn't
       * wildcarded in the source, it isn't supposed to be
       * scaled by the engine function
       */
      gtk_icon_source_set_size (source,
				GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_icon_source_set_size_wildcarded (source, FALSE);

      rendered = gtk_render_icon_pixbuf (context, source, (GtkIconSize) -1);
      gtk_icon_source_free (source);

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
get_icon_lookup_flags (GtkIconHelper *self)
{
  GtkIconLookupFlags flags = GTK_ICON_LOOKUP_USE_BUILTIN;

  if (self->priv->use_fallback)
    flags |= GTK_ICON_LOOKUP_GENERIC_FALLBACK;
  if (self->priv->pixel_size != -1)
    flags |= GTK_ICON_LOOKUP_FORCE_SIZE;

  return flags;
}

static void
ensure_pixbuf_for_icon_name_or_gicon (GtkIconHelper *self,
                                      GtkStyleContext *context)
{
  GtkIconTheme *icon_theme;
  gint width, height;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;

  if (!check_invalidate_pixbuf (self, context))
    return;

  icon_theme = gtk_icon_theme_get_default ();
  flags = get_icon_lookup_flags (self);

  ensure_icon_size (self, context, &width, &height);

  if (self->priv->storage_type == GTK_IMAGE_ICON_NAME &&
      self->priv->icon_name != NULL)
    {
      info = gtk_icon_theme_lookup_icon (icon_theme,
                                         self->priv->icon_name,
                                         MIN (width, height), flags);
    }
  else if (self->priv->storage_type == GTK_IMAGE_GICON &&
           self->priv->gicon != NULL)
    {
      info = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                             self->priv->gicon,
                                             MIN (width, height), flags);
    }
  else
    {
      g_assert_not_reached ();
      return;
    }

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

  self->priv->rendered_pixbuf = 
    gtk_icon_set_render_icon_pixbuf (icon_set, context, self->priv->icon_size);
}

static void
ensure_pixbuf_at_size (GtkIconHelper   *self,
                       GtkStyleContext *context)
{
  gint width, height;

  if (!check_invalidate_pixbuf (self, context))
    return;

  if (self->priv->rendered_pixbuf)
    return;

  if (self->priv->pixel_size != -1 ||
      self->priv->icon_size != GTK_ICON_SIZE_INVALID)
    {
      ensure_icon_size (self, context, &width, &height);

      if (width < gdk_pixbuf_get_width (self->priv->orig_pixbuf) ||
          height < gdk_pixbuf_get_height (self->priv->orig_pixbuf))
        self->priv->rendered_pixbuf =
          gdk_pixbuf_scale_simple (self->priv->orig_pixbuf,
                                   width, height,
                                   GDK_INTERP_BILINEAR);
    }

  if (!self->priv->rendered_pixbuf)
    self->priv->rendered_pixbuf = g_object_ref (self->priv->orig_pixbuf);
}

GdkPixbuf *
_gtk_icon_helper_ensure_pixbuf (GtkIconHelper *self,
                                GtkStyleContext *context)
{
  GdkPixbuf *pixbuf = NULL;
  GtkIconSet *icon_set;

  switch (self->priv->storage_type)
    {
    case GTK_IMAGE_PIXBUF:
      if (self->priv->force_scale_pixbuf)
        ensure_pixbuf_at_size (self, context);
      else
        pixbuf = g_object_ref (self->priv->orig_pixbuf);
      break;

    case GTK_IMAGE_STOCK:
      icon_set = gtk_style_context_lookup_icon_set (context, self->priv->stock_id);
      if (icon_set != NULL)
	ensure_pixbuf_for_icon_set (self, context, icon_set);
      else
	pixbuf = NULL;
      break;

    case GTK_IMAGE_ICON_SET:
      icon_set = self->priv->icon_set;
      ensure_pixbuf_for_icon_set (self, context, icon_set);
      break;

    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      ensure_pixbuf_for_icon_name_or_gicon (self, context);
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

void
_gtk_icon_helper_get_size (GtkIconHelper *self,
                           GtkStyleContext *context,
                           gint *width_out,
                           gint *height_out)
{
  GdkPixbuf *pix;
  gint width, height;

  width = height = 0;
  pix = _gtk_icon_helper_ensure_pixbuf (self, context);

  if (pix != NULL)
    {
      width = gdk_pixbuf_get_width (pix);
      height = gdk_pixbuf_get_height (pix);

      g_object_unref (pix);
    }
  else if (self->priv->storage_type == GTK_IMAGE_ANIMATION)
    {
      width = gdk_pixbuf_animation_get_width (self->priv->animation);
      height = gdk_pixbuf_animation_get_height (self->priv->animation);
    }
  else if (self->priv->icon_size != -1)
    {
      ensure_icon_size (self, context, &width, &height);
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
  _gtk_icon_helper_clear (self);

  if (gicon != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_GICON;
      self->priv->gicon = g_object_ref (gicon);
      _gtk_icon_helper_set_icon_size (self, icon_size);
    }
}

void 
_gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                const gchar *icon_name,
                                GtkIconSize icon_size)
{
  _gtk_icon_helper_clear (self);

  if (icon_name != NULL &&
      icon_name[0] != '\0')
    {
      self->priv->storage_type = GTK_IMAGE_ICON_NAME;
      self->priv->icon_name = g_strdup (icon_name);
      _gtk_icon_helper_set_icon_size (self, icon_size);
    }
}

void 
_gtk_icon_helper_set_icon_set (GtkIconHelper *self,
                               GtkIconSet *icon_set,
                               GtkIconSize icon_size)
{
  _gtk_icon_helper_clear (self);

  if (icon_set != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_ICON_SET;
      self->priv->icon_set = gtk_icon_set_ref (icon_set);
      _gtk_icon_helper_set_icon_size (self, icon_size);
    }
}

void 
_gtk_icon_helper_set_pixbuf (GtkIconHelper *self,
                             GdkPixbuf *pixbuf)
{
  _gtk_icon_helper_clear (self);

  if (pixbuf != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_PIXBUF;
      self->priv->orig_pixbuf = g_object_ref (pixbuf);
    }
}

void 
_gtk_icon_helper_set_animation (GtkIconHelper *self,
                                GdkPixbufAnimation *animation)
{
  _gtk_icon_helper_clear (self);

  if (animation != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_ANIMATION;
      self->priv->animation = g_object_ref (animation);
    }
}

void 
_gtk_icon_helper_set_stock_id (GtkIconHelper *self,
                               const gchar *stock_id,
                               GtkIconSize icon_size)
{
  _gtk_icon_helper_clear (self);

  if (stock_id != NULL &&
      stock_id[0] != '\0')
    {
      self->priv->storage_type = GTK_IMAGE_STOCK;
      self->priv->stock_id = g_strdup (stock_id);
      _gtk_icon_helper_set_icon_size (self, icon_size);
    }
}

void 
_gtk_icon_helper_set_icon_size (GtkIconHelper *self,
                                GtkIconSize icon_size)
{
  if (self->priv->icon_size != icon_size)
    {
      self->priv->icon_size = icon_size;
      _gtk_icon_helper_invalidate (self);
    }
}

void 
_gtk_icon_helper_set_pixel_size (GtkIconHelper *self,
                                 gint pixel_size)
{
  if (self->priv->pixel_size != pixel_size)
    {
      self->priv->pixel_size = pixel_size;
      _gtk_icon_helper_invalidate (self);
    }
}

void 
_gtk_icon_helper_set_use_fallback (GtkIconHelper *self,
                                   gboolean use_fallback)
{
  if (self->priv->use_fallback != use_fallback)
    {
      self->priv->use_fallback = use_fallback;
      _gtk_icon_helper_invalidate (self);
    }
}

GtkImageType
_gtk_icon_helper_get_storage_type (GtkIconHelper *self)
{
  return self->priv->storage_type;
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
  return self->priv->orig_pixbuf;
}

GIcon *
_gtk_icon_helper_peek_gicon (GtkIconHelper *self)
{
  return self->priv->gicon;
}

GdkPixbufAnimation *
_gtk_icon_helper_peek_animation (GtkIconHelper *self)
{
  return self->priv->animation;
}

GtkIconSet *
_gtk_icon_helper_peek_icon_set (GtkIconHelper *self)
{
  return self->priv->icon_set;
}

const gchar *
_gtk_icon_helper_get_stock_id (GtkIconHelper *self)
{
  return self->priv->stock_id;
}

const gchar *
_gtk_icon_helper_get_icon_name (GtkIconHelper *self)
{
  return self->priv->icon_name;
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
  GdkPixbuf *pixbuf;

  pixbuf = _gtk_icon_helper_ensure_pixbuf (self, context);

  if (pixbuf != NULL)
    {
      gtk_render_icon (context, cr, pixbuf, x, y);
      g_object_unref (pixbuf);
    }
}

gboolean
_gtk_icon_helper_get_is_empty (GtkIconHelper *self)
{
  return (self->priv->storage_type == GTK_IMAGE_EMPTY);
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
